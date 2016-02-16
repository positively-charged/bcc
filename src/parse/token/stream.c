#include <string.h>

#include "phase.h"

enum {
   READTK_NONE = 0x0,
   READTK_ACCEPTHORZSPACE = 0x1,
   READTK_ACCEPTNL = 0x2,
   READTK_EXPANDMACROS = 0x4,
   READTK_EXECUTEDIRCS = 0x8,
};

struct macro_expan {
   struct macro* macro;
   struct macro_arg* args;
   struct token* token;
   struct token* seq_token;
   struct macro_expan* prev;
   struct pos pos;
};

struct macro_arg {
   struct macro_arg* next;
   struct token* sequence;
};

struct arg_reading {
   struct macro_expan* expan;
   struct macro_arg* head;
   struct macro_arg* tail;
   struct token* token_head;
   struct token* token_tail;
   int given;
   int paren_depth;
   bool done;
   bool done_seq;
   bool surpassed_param_count;
};

static void readtk_peeked( struct parse* parse );
static void readtk( struct parse* parse, int options );
static void read_token_raw( struct parse* parse );
static void read_token_seq( struct parse* parse );
static void expand_macro_param( struct parse* parse );
static void read_token_expan( struct parse* parse );
static void readtk_macro( struct parse* parse );
static bool can_concat( struct parse* parse );
static void concat( struct parse* parse );
static void stringize( struct parse* parse );
static bool expand_macro( struct parse* parse );
static bool expand_predef_macro( struct parse* parse );
static void read_macro_arg_list( struct parse* parse,
   struct macro_expan* expan );
static void read_macro_arg( struct parse* parse, struct arg_reading* reading );
static void read_seq_token( struct parse* parse, struct arg_reading* reading );
static void add_seq_token( struct parse* parse, struct arg_reading* reading );
static struct token* peek( struct parse* parse, int pos );

// Reads a token. Is used by parser.
// Needs to:
//  - Skip whitespace, both horizontal space and newlines.
//  - Expand macros.
//  - Execute directives.
void p_read_tk( struct parse* parse ) {
   readtk_peeked( parse );
/*
   // Concatenate adjacent strings.
   if ( parse->token->type == TK_LIT_STRING ) {
      struct str string;
      str_init( &string );
      str_append( &string, parse->token->text );
      while ( p_peek_tk( parse )->type == TK_LIT_STRING ) {
         readtk_peeked( parse );
         str_append( &string, parse->token->text );
printf( "t %d\n", parse->token->type );
         
      }
      if ( string.length ) {
   printf( "string: %s\n", string.value );
      }
   }*/
   struct token* token = parse->token;
   parse->tk = token->type;
   parse->tk_text = token->text;
   parse->tk_pos = token->pos;
   parse->tk_length = token->length;
}

void readtk_peeked( struct parse* parse ) {
   if ( parse->peeked ) {
      // When dequeuing, shift the queue elements. For now, this will suffice.
      // In the future, maybe use a circular buffer.
      int i = 0;
      while ( i < parse->peeked ) {
         parse->queue[ i ] = parse->queue[ i + 1 ];
         ++i;
      }
      parse->token = parse->queue[ 0 ];
      --parse->peeked;
   }
   else {
      readtk( parse, READTK_EXPANDMACROS | READTK_EXECUTEDIRCS );
   }
}

// Reads a token. Is used by preprocessor.
void p_read_preptk( struct parse* parse ) {
   readtk( parse, READTK_ACCEPTNL );
}

void p_read_expanpreptk( struct parse* parse ) {
   readtk( parse, READTK_ACCEPTNL | READTK_EXPANDMACROS );
}

void p_read_eoptiontk( struct parse* parse ) {
   readtk( parse, READTK_ACCEPTHORZSPACE | READTK_ACCEPTNL |
      READTK_EXPANDMACROS | READTK_EXECUTEDIRCS );
}

void readtk( struct parse* parse, int options ) {
   top:
   p_read_stream( parse );

   // -----------------------------------------------------------------------
   if ( parse->token->is_id ) {
      if ( options & READTK_EXPANDMACROS ) {
         bool expanded = expand_macro( parse );
         if ( expanded ) {
            goto top;
         }
      }
      goto done;
   }

   // -----------------------------------------------------------------------
   switch ( parse->token->type ) {
   case TK_HASH:
      if ( options & READTK_EXECUTEDIRCS ) {
         if ( parse->line_beginning ) {
            bool read = p_read_dirc( parse );
            if ( read ) {
               goto nl;
            }
         }
      }
      break;
   case TK_HORZSPACE:
      if ( ! ( options & READTK_ACCEPTHORZSPACE ) ) {
         goto top;
      }
      break;
   nl:
   case TK_NL:
      parse->line_beginning = true;
      if ( ! ( options & READTK_ACCEPTNL ) ) {
         goto top;
      }
      break;
   default:
      parse->line_beginning = false;
      break;
   }

   // -----------------------------------------------------------------------
   done:
   // Certain identifiers can appear only in a specific context.
   if ( parse->token->is_id &&
      strcmp( parse->token->text, "__va_args__" ) == 0 &&
      ! parse->prep_context != PREPCONTEXT_DEFINEBODY ) {
      p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
         "`%s` outside function-like macro expansion",
         parse->token->text );
      p_bail( parse );
   }
}

void p_read_stream( struct parse* parse ) {
   if ( parse->token_peeked ) {
      parse->token = parse->token_peeked;
      parse->token_peeked = parse->token->next;
      // if ( parse->token == parse->
   }
   else {
      read_token_raw( parse );
   }
}

// Begins by reading tokens from a macro expansion. When a macro expansion is
// not present, or there are no more tokens available in the macro expansion,
// reads a token from the source file.
void read_token_raw( struct parse* parse ) {
   // Read from a predefined-macro expansion.
   if ( parse->predef_macro_expan != PREDEFMACROEXPAN_NONE ) {
      p_read_sourcepos_token( parse, parse->macro_expan ?
      &parse->macro_expan->pos : &parse->token->pos );
      parse->predef_macro_expan = PREDEFMACROEXPAN_NONE;
      return;
   }
   // Read from a macro expansion.
   parse->token = NULL;
   while ( parse->macro_expan ) {
      read_token_expan( parse );
      if ( parse->token ) {
         return;
      }
   }
   // Read from a source file.
   parse->token = parse->source_token;
   p_read_source( parse, parse->token );
}

// Reads a token from a macro expansion.
void read_token_expan( struct parse* parse ) {
   // Reads a token from a macro-argument token-sequence.
   if ( parse->macro_expan->seq_token ) {
      parse->token = parse->macro_expan->seq_token;
      parse->macro_expan->seq_token = parse->token->next;
   }
   // Reads a token from a macro body.
   else if ( parse->macro_expan->token ) {
      readtk_macro( parse );
      while ( can_concat( parse ) ) {
         concat( parse );
      }
      // Discard placemarker tokens.
      if ( parse->token->type == TK_PLACEMARKER ) {
         parse->token = NULL;
      }
   }
   else {
      struct macro_expan* expan = parse->macro_expan;
      parse->macro_expan = expan->prev;
      expan->prev = parse->macro_expan_free;
      parse->macro_expan_free = expan;
   }
}

void readtk_macro( struct parse* parse ) {
   struct token* token = parse->macro_expan->token;
   parse->macro_expan->token = token->next;
   parse->token = token;
   if ( parse->token->type == TK_STRINGIZE ) {
      stringize( parse );
   }
   else {
      if ( parse->token->is_id ) {
         expand_macro_param( parse );
      }
   }
}

void expand_macro_param( struct parse* parse ) {
   struct macro_arg* arg = parse->macro_expan->args;
   struct macro_param* param = parse->macro_expan->macro->param_head;
   while ( param && strcmp( param->name, parse->token->text ) != 0 ) {
      param = param->next;
      arg = arg->next;
   }
   if ( ! param ) {
      return;
   }
   if ( arg->sequence ) {
      parse->macro_expan->seq_token = arg->sequence;
      parse->macro_expan->token = parse->token->next;
      read_token_expan( parse );
   }
   else {
      static struct token placemarker = { .type = TK_PLACEMARKER };
      parse->token = &placemarker;
   }
}

bool can_concat( struct parse* parse ) {
   struct token* token = parse->macro_expan->token;
   if ( token && token->type == TK_HORZSPACE ) {
      token = token->next;
   }
   return ( token && token->type == TK_PREP_HASHHASH );
}

// `##` operator.
void concat( struct parse* parse ) {
   struct token* lside = parse->token;
   readtk_macro( parse );
   if ( parse->token->type == TK_HORZSPACE ) {
      readtk_macro( parse );
   }
   struct token* hash = parse->token;
   readtk_macro( parse );
   if ( parse->token->type == TK_HORZSPACE ) {
      readtk_macro( parse );
   }
   // If right token is a placemarker, then return the left token.
   if ( parse->token->type == TK_PLACEMARKER ) {
      parse->token = lside;
   }
   else {
      if ( lside->type != TK_PLACEMARKER ) {
         struct str text;
         str_init( &text );
         str_append( &text, lside->text );
         str_append( &text, parse->token->text );
         //append_token_text( &text, lside );
         //append_token_text( &text, &parse->token );
         enum tk type = p_identify_token_type( text.value );
         if ( type == TK_NONE ) {
            p_diag( parse, DIAG_POS_ERR, &hash->pos,
               "concatenating `%s` and `%s` produces an invalid token",
               lside->text, parse->token->text );
            p_bail( parse );
         }
         parse->token->type = type;
         parse->token->text = text.value;
      }
   }
}

// `#` operator.
void stringize( struct parse* parse ) {
   struct macro_arg* arg = parse->macro_expan->args;
   struct macro_param* param = parse->macro_expan->macro->param_head;
   while ( param && strcmp( param->name, parse->token->text ) != 0 ) {
      param = param->next;
      arg = arg->next;
   }
   struct str text;
   str_init( &text );
   str_copy( &text, "", 0 );
   if ( arg->sequence ) {
      struct token* token = arg->sequence;
      while ( token ) {
         str_append( &text, token->text );
         token = token->next;
      }
   }
   // TODO: Allocate text. Clean up. 
   static struct token token;
   token.type = TK_LIT_STRING;
   token.text = text.value;
   token.length = token.length;
   // token.pos = parse->token->pos;
   token.is_id = false;
   parse->token = &token;
}

bool expand_macro( struct parse* parse ) {
   if ( ! parse->token->is_id ) {
      return false;
   }

   bool expanded = expand_predef_macro( parse );
   if ( expanded ) {
      return true;
   }

   struct macro* macro = p_find_macro( parse, parse->token->text );
   if ( ! macro ) {
      return false;
   }
   // The macro should not already be undergoing expansion.
   struct macro_expan* expan = parse->macro_expan;
   while ( expan ) {
      if ( expan->macro == macro ) {
         return false;
      }
      expan = expan->prev;
   }
   // A function-like macro is expanded only when a list of arguments is
   // provided.
   if ( macro->func_like ) {
      struct token* token = p_peek_stream( parse, true );
      if ( token->type != TK_PAREN_L ) {
         return false;
      }
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
   expan->args = NULL;
   expan->token = macro->body;
   expan->seq_token = NULL;
   expan->prev = parse->macro_expan;
   expan->pos = parse->token->pos;
   if ( macro->func_like ) {
      p_read_stream( parse );
      read_macro_arg_list( parse, expan );
   }
   parse->macro_expan = expan;

/*
   printf( "macro: %s\n", macro->name );
   struct macro_arg* arg = expan->args;
   while ( arg ) {
      printf( "arg\n" );
      struct token* token = arg->sequence;
      while( token ) {
         printf( "  %d %s\n", token->type,  token->text );
         token = token->next;
      }
      arg = arg->next;
   }
      printf( "body\n" );
   struct token* token = macro->body;
   while ( token ) {
      printf( "  %d %s\n", token->type, token->text );
      token = token->next;
   }
   printf( "done\n" );
*/
   return true;
}

bool expand_predef_macro( struct parse* parse ) {
   parse->predef_macro_expan = p_identify_predef_macro( parse->token->text );
   return ( parse->predef_macro_expan != PREDEFMACROEXPAN_NONE );
}

void init_arg_reading( struct arg_reading* reading,
   struct macro_expan* expan ) {
   reading->expan = expan;
   reading->head = NULL;
   reading->tail = NULL;
   reading->token_head = NULL;
   reading->token_tail = NULL;
   reading->given = 0;
   reading->paren_depth = 1;
   reading->done = false;
   reading->done_seq = false;
   reading->surpassed_param_count = false;
}

void skip_whitespace( struct parse* parse ) {
   while ( true ) {
      switch ( parse->token->type ) {
      case TK_HORZSPACE:
      case TK_NL:
         p_read_stream( parse );
         break;
      default:
         return;
      }
   }
} 

void read_macro_arg_list( struct parse* parse, struct macro_expan* expan ) {
   skip_whitespace( parse );
   p_test_tk( parse, TK_PAREN_L );
   p_read_stream( parse );
   skip_whitespace( parse );
   struct arg_reading reading;
   init_arg_reading( &reading, expan );
   if ( parse->token->type != TK_PAREN_R ) {
      while ( ! reading.done ) {
         read_macro_arg( parse, &reading );
      }
   }
   p_test_tk( parse, TK_PAREN_R );
   if ( reading.given < expan->macro->param_count ) {
      p_diag( parse, DIAG_POS_ERR, &expan->pos,
         "too little arguments in macro expansion" );
      p_bail( parse );
   }
   if ( reading.given > expan->macro->param_count && ! expan->macro->variadic ) {
      p_diag( parse, DIAG_POS_ERR, &expan->pos,
         "too many arguments in macro expansion" );
      p_bail( parse );
   }
   expan->args = reading.head;
}

void read_macro_arg( struct parse* parse, struct arg_reading* reading ) {
   // Read sequence of tokens that make up the argument.
   if ( reading->given == reading->expan->macro->param_count - 1 ) {
      reading->surpassed_param_count = true;
   }
   reading->token_head = NULL;
   reading->token_tail = NULL;
   reading->done_seq = false;
   while ( ! reading->done_seq ) {
      read_seq_token( parse, reading );
   }
   // Add argument.
   struct macro_arg* arg = mem_alloc( sizeof( *arg ) );
   arg->next = NULL;
   arg->sequence = reading->token_head;
   if ( reading->head ) {
      reading->tail->next = arg;
   }
   else {
      reading->head = arg;
   }
   reading->tail = arg;
   ++reading->given;
}

void read_seq_token( struct parse* parse, struct arg_reading* reading ) {
   p_read_stream( parse );
   switch ( parse->token->type ) {
   case TK_HORZSPACE:
   case TK_NL:
      if ( reading->token_tail && reading->token_tail->type != TK_HORZSPACE ) {
         if ( p_peek_stream( parse, false )->type != TK_PAREN_R ) {
            add_seq_token( parse, reading );
            reading->token_tail->type = TK_HORZSPACE;
            reading->token_tail->text = " ";
            reading->token_tail->length = 1;
         }
      }
      if ( parse->token->type == TK_NL ) {
         if ( ! parse->macro_expan ) {
            ++parse->line;
            parse->column = 0;
         }
      }
      break;
   case TK_PAREN_L:
      ++reading->paren_depth;
      add_seq_token( parse, reading );
      break;
   case TK_PAREN_R:
      --reading->paren_depth;
      if ( reading->paren_depth == 0 ) {
         reading->done_seq = true;
         reading->done = true;
         if ( ! parse->macro_expan && parse->line > 0 ) {
            parse->column = parse->token->pos.column + parse->token->length;
         }
      }
      else {
         add_seq_token( parse, reading );
      }
      break;
   case TK_COMMA:
      if ( reading->paren_depth == 1 && ! reading->surpassed_param_count ) {
         reading->done_seq = true;
      }
      else {
         add_seq_token( parse, reading );
      }
      break;
   case TK_ID:
      if ( ! expand_macro( parse ) ) {
         add_seq_token( parse, reading );
      }
      break;
   case TK_END:
      p_diag( parse, DIAG_POS_ERR, &reading->expan->pos,
         "unterminated macro expansion" );
      p_bail( parse );
      break;
   default:
      add_seq_token( parse, reading );
      break;
   }
}

void add_seq_token( struct parse* parse, struct arg_reading* reading ) {
   struct token* token = mem_alloc( sizeof( *token ) );
   *token = *parse->token;
   token->next = NULL;
   if ( reading->token_head ) {
      reading->token_tail->next = token;
   }
   else {
      reading->token_head = token;
   }
   reading->token_tail = token;
}

struct token* p_peek_stream( struct parse* parse, bool nonwhitespace ) {
   struct stream_iter iter = { NULL, parse->token_peeked };
   p_next_token( parse, &iter );
   if ( nonwhitespace ) {
      while (
         iter.token->type == TK_HORZSPACE ||
         iter.token->type == TK_NL ) {
         p_next_token( parse, &iter );
      }

   }
   return iter.token;
}


// NOTE: Does not initialize fields.
struct token* alloc_token( struct parse* parse ) {
   struct token* token;
   if ( parse->token_free ) {
      token = parse->token_free;
      parse->token_free = token->next;
   }
   else {
    
  token = mem_alloc( sizeof( *token ) );
   }
   return token;
}

void append_stream_token( struct parse* parse, struct token* token ) {
   if ( parse->token_peeked ) {
      parse->token_peeked_tail->next = token;
   }
   else {
      parse->token_peeked = token;
   }
   parse->token_peeked_tail = token;
}

void p_init_stream_iter( struct parse* parse, struct stream_iter* iter ) {
   iter->token = NULL;
   iter->next_token = parse->token_peeked;
} 

void p_next_token( struct parse* parse, struct stream_iter* iter ) {
   if ( iter->next_token ) {
      iter->token = iter->next_token;
      iter->next_token = iter->token->next;
   }
   else {
      struct token* peeked_token = alloc_token( parse );
      struct token* token = parse->token;
      parse->source_token = peeked_token;
      read_token_raw( parse );
      parse->source_token = &parse->queue_source[ 0 ];
      parse->token = token;
      append_stream_token( parse, peeked_token );
      iter->token = peeked_token;
   }
}

struct token* p_peek_stream_2nd( struct parse* parse ) {
   return peek( parse, 2 );
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
      if ( i == parse->peeked ) {
         struct token* current_token = parse->token;
         parse->source_token = &parse->queue_source[ i + 1 ];
         readtk( parse, READTK_EXPANDMACROS | READTK_EXECUTEDIRCS );
         parse->source_token = &parse->queue_source[ 0 ];
         parse->queue[ i + 1 ] = parse->token; 
         parse->token = current_token;
         ++parse->peeked;
      }
      ++i;
      if ( i == pos ) {
         return parse->queue[ i ];
      }
   }
}

void p_test_tk( struct parse* parse, enum tk expected ) {
   if ( parse->token->type != expected ) {
      if ( parse->token->type == TK_RESERVED ) {
         p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
            "`%s` is a reserved identifier that is not currently used",
            parse->token->text );
      }
      else {
         p_diag( parse, DIAG_POS_ERR | DIAG_SYNTAX, &parse->token->pos, 
            "unexpected %s", p_get_token_name( parse->token->type ) );
         p_diag( parse, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &parse->token->pos,
            "expecting %s here", p_get_token_name( expected ),
            p_get_token_name( parse->token->type ) );
      }
      p_bail( parse );
   }
}

void p_test_preptk( struct parse* parse, enum tk expected ) {
   if ( expected == TK_ID ) {
      if ( ! parse->token->is_id ) {
         p_test_tk( parse, TK_ID );
      }
   }
   else {
      p_test_tk( parse, expected );
   }
}

void p_test_stream( struct parse* parse, enum tk expected ) {
   p_test_tk( parse, expected );
}

static void macro_trace_diag( struct parse* parse, struct macro_expan* expan );

void p_macro_trace_diag( struct parse* parse ) {
   //macro_trace_diag( parse, parse->macro_expan );
}

void macro_trace_diag( struct parse* parse, struct macro_expan* expan ) {
   if ( expan->prev ) {
      macro_trace_diag( parse, expan->prev );
   }
   t_diag( parse->task, DIAG_POS, &expan->pos,
      "from expansion of macro `%s`", expan->macro->name );
}

void p_skip_block( struct parse* parse ) {
   while ( parse->tk != TK_END && parse->tk != TK_BRACE_L ) {
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_BRACE_L );
   p_read_tk( parse );
   int depth = 0;
   while ( true ) {
      if ( parse->tk == TK_BRACE_L ) {
         ++depth;
         p_read_tk( parse );
      }
      else if ( parse->tk == TK_BRACE_R ) {
         if ( depth ) {
            --depth;
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
      else if ( parse->tk == TK_LIB_END ) {
         break;
      }
      else if ( parse->tk == TK_END ) {
         break;
      }
      else {
         p_read_tk( parse );
      }
   }
   p_test_tk( parse, TK_BRACE_R );
   p_read_tk( parse );
}