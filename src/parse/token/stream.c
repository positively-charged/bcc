#include <string.h>

#include "phase.h"

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

static void read_peeked_token( struct parse* parse );
static void read_token( struct parse* parse );
static void read_token_seq( struct parse* parse );
static void expand_macro_param( struct parse* parse );
static void read_token_expan( struct parse* parse );
static void readtk_macro( struct parse* parse );
static bool can_concat( struct parse* parse );
static void concat( struct parse* parse );
static void stringize( struct parse* parse );
static bool expand_predef_macro( struct parse* parse );
static void read_macro_arg_list( struct parse* parse,
   struct macro_expan* expan );
static void read_macro_arg( struct parse* parse, struct arg_reading* reading );
static void read_seq_token( struct parse* parse, struct arg_reading* reading );
static void add_seq_token( struct parse* parse, struct arg_reading* reading );
static struct token* push_token( struct parse* parse );

void p_init_stream( struct parse* parse ) {
   parse->tk = TK_END;
   parse->prev_tk = TK_NL;
   parse->tk_text = "";
   parse->tk_length = 0;
   parse->token_free = NULL;
   parse->tkque_free_entry = NULL;
   parse->source_token = &parse->token_source;
   p_init_token_queue( &parse->tkque, true );
   p_init_token_queue( &parse->parser_tkque, false );
}

void p_read_stream( struct parse* parse ) {
   read_peeked_token( parse );
   parse->line_beginning = ( parse->prev_tk == TK_NL ) ||
      ( parse->prev_tk == TK_HORZSPACE && parse->line_beginning );
   parse->prev_tk = parse->token->type;
}

void read_peeked_token( struct parse* parse ) {
   if ( parse->tkque.size > 0 ) {
      parse->token = p_shift_entry( parse, &parse->tkque );
   }
   else {
      read_token( parse );
   }
}

// Begins by reading tokens from a macro expansion. When a macro expansion is
// not present, or there are no more tokens available in the macro expansion,
// reads a token from the source file.
void read_token( struct parse* parse ) {
   if ( parse->lang == LANG_BCS ) {
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
   }
   // Read from a source file.
   p_read_source( parse, parse->source_token );
   parse->token = parse->source_token;
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

bool p_expand_macro( struct parse* parse ) {
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
      struct streamtk_iter iter;
      p_init_streamtk_iter( parse, &iter );
      p_next_stream( parse, &iter );
      while (
         iter.token->type == TK_HORZSPACE ||
         iter.token->type == TK_NL ) {
         p_next_stream( parse, &iter );
      }
      if ( iter.token->type != TK_PAREN_L ) {
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
         struct streamtk_iter iter;
         p_init_streamtk_iter( parse, &iter );
         p_next_stream( parse, &iter );
         if ( iter.token->type != TK_PAREN_R ) {
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
      if ( ! p_expand_macro( parse ) ) {
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

void p_init_streamtk_iter( struct parse* parse, struct streamtk_iter* iter ) {
   iter->entry = parse->tkque.head;
   iter->token = NULL;
}

void p_next_stream( struct parse* parse, struct streamtk_iter* iter ) {
   if ( iter->entry ) {
      struct queue_entry* entry = iter->entry;
      iter->token = entry->token;
      iter->entry = entry->next;
   }
   else {
      iter->token = push_token( parse );
   }
}

struct token* push_token( struct parse* parse ) {
   struct token* token = parse->token;
   struct token* source_token = parse->source_token;
   struct token peeked_token;
   parse->source_token = &peeked_token;
   read_token( parse );
   struct queue_entry* entry = p_push_entry( parse, &parse->tkque );
   entry->token_allocated = ( parse->token == &peeked_token );
   if ( entry->token_allocated ) {
      struct token* fresh_token = p_alloc_token( parse );
      *fresh_token = peeked_token;
      entry->token = fresh_token;
   }
   else {
      entry->token = parse->token;
   }
   parse->source_token = source_token;
   parse->token = token;
   return entry->token;
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
   token->next = NULL;
   return token;
}

struct token* p_alloc_token( struct parse* parse ) {
   return alloc_token( parse );
}

void p_free_token( struct parse* parse, struct token* token ) {
   token->next = parse->token_free;
   parse->token_free = token;
}

void fill_stream( struct parse* parse, int required_size ) {
   while ( parse->tkque.size < required_size ) {
      
   }
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