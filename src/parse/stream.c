#include <string.h>

#include "phase.h"

struct macro_expan {
   struct macro* macro;
   //struct macro_arg* args;
   //struct macro_arg* args_left;
   struct token* token;
   struct token* token_arg;
   struct macro_expan* prev;
   struct pos pos;
};

static bool dirc_present( struct parse* parse );
static bool pseudo_dirc( struct parse* parse );
static void read_stream( struct parse* parse );
static void read_active_stream( struct parse* parse );
static bool expand_macro( struct parse* parse, struct macro* macro );
static void read_macro_stream( struct parse* parse );
static struct token* peek( struct parse* parse, int pos );

void p_read_tk( struct parse* parse ) {
   struct token* token = NULL;
   if ( parse->peeked ) {
      // When dequeuing, shift the queue elements. For now, this will suffice.
      // In the future, maybe use a circular buffer.
      int i = 0;
      while ( i < parse->peeked ) {
         parse->queue[ i ] = parse->queue[ i + 1 ];
         ++i;
      }
      token = &parse->queue[ 0 ];
      --parse->peeked;
   }
   else {
      token = &parse->queue[ 0 ];
      p_read_source( parse, token );
   }
   parse->token = token;
   parse->tk = token->type;
   parse->tk_text = token->text;
   parse->tk_pos = token->pos;
   parse->tk_length = token->length;
}

void p_read_token( struct parse* parse ) {
   top:
   read_stream( parse );
   // Read directives.
   if ( parse->line_beginning ) {
      while ( dirc_present( parse ) ) {
         p_read_dirc( parse );
      }
   }
   // Read token from source.
   switch ( parse->token->type ) {
   case TK_NL:
      parse->line_beginning = true;
      if ( ! ( parse->read_flags & READF_NL ) ) {
         goto top;
      }
      break;
   case TK_SPACE:
   case TK_TAB:
      break;
   case TK_END:
      p_confirm_ifdircs_closed( parse );
      break;
   default:
      parse->line_beginning = false;
      break;
   }
}

inline bool dirc_present( struct parse* parse ) {
   // NOTE: Maybe instead of testing for pseudo directives, we should
   // test for real directives.
   return ( parse->tk == TK_HASH && ! pseudo_dirc( parse ) );
}

bool pseudo_dirc( struct parse* parse ) {
   static const char* table[] = {
      "library",
      "libdefine",
      "import",
      "nocompact",
      "encryptstrings",
      "wadauthor",
      "nowadauthor",
      "pragma",
   };
   int flags = parse->read_flags;
   parse->read_flags ^= READF_SPACETAB;
   struct token* token = p_peek_tk( parse );
   parse->read_flags = flags;
   for ( int i = 0; i < ARRAY_SIZE( table ); ++i ) {
      if ( strcmp( table[ i ], token->text ) == 0 ) {
         return true;
      }
   }
   return false;
}

void read_stream( struct parse* parse ) {
   top:
   parse->token = NULL;
   while ( ! parse->token ) {
      read_active_stream( parse );
   }
   if ( parse->token->is_id ) {
      struct macro* macro = p_find_macro( parse, parse->token->text );
      if ( macro ) {
         bool expanded = expand_macro( parse, macro );
         if ( expanded ) {
            goto top;
         }
      }
   }
}

void read_active_stream( struct parse* parse ) {
   if ( parse->macro_expan ) {
      read_macro_stream( parse );
   }
   else {
      p_read_tk( parse );
   }
}

bool expand_macro( struct parse* parse, struct macro* macro ) {
   // The macro should not already be undergoing expansion.
   struct macro_expan* expan = parse->macro_expan;
   while ( expan ) {
      if ( expan->macro == macro ) {
         return false;
      }
      expan = expan->prev;
   }
   // Allocate.
   if ( parse->macro_expan_free ) {
      expan = parse->macro_expan_free;
      parse->macro_expan_free = expan->prev;
   }
   else {
      expan = mem_alloc( sizeof( *expan ) );
   }
   // Initialize.
   expan->macro = macro;
   expan->token = macro->body;
   expan->token_arg = NULL;
   expan->prev = parse->macro_expan;
   expan->pos = parse->token->pos;
   parse->macro_expan = expan;
   return true;
}

void read_macro_stream( struct parse* parse ) {
   if ( parse->macro_expan->token ) {
      struct token* token = parse->macro_expan->token;
      parse->macro_expan->token = token->next;
      parse->token = token;
   }
   else {
      struct macro_expan* expan = parse->macro_expan;
      parse->macro_expan = expan->prev;
      expan->prev = parse->macro_expan_free;
      parse->macro_expan_free = expan;
      parse->token = NULL;
   }
}

enum tk p_peek( struct parse* parse ) {
   return peek( parse, 1 )->type;
}

struct token* p_peek_tk( struct parse* parse ) {
   return peek( parse, 1 );
}

// NOTE: Make sure @pos is not more than ( TK_BUFFER_SIZE - 1 ).
struct token* peek( struct parse* parse, int pos ) {
   int i = 0;
   while ( true ) {
      // Peeked tokens begin at position 1.
      struct token* token = &parse->queue[ i + 1 ];
      if ( i == parse->peeked ) {
         p_read_source( parse, token );
         ++parse->peeked;
      }
      ++i;
      if ( i == pos ) {
         return token;
      }
   }
}

void p_test_tk( struct parse* parse, enum tk expected ) {
   if ( parse->tk != expected ) {
      if ( parse->tk == TK_RESERVED ) {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "`%s` is a reserved identifier that is not currently used",
            parse->tk_text );
      }
      else {
         p_diag( parse, DIAG_POS_ERR | DIAG_SYNTAX, &parse->tk_pos, 
            "unexpected %s", p_get_token_name( parse->tk ) );
         p_diag( parse, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &parse->tk_pos,
            "expecting %s here", p_get_token_name( expected ),
            p_get_token_name( parse->tk ) );
      }
      p_bail( parse );
   }
}