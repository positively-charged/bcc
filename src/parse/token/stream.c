#include <string.h>
#include <time.h>

#include "phase.h"

struct macro_expan {
   struct macro* macro;
   struct macro_expan* prev;
   struct macro_arg* args;
   struct macro_arg* args_tail;
   struct token* input;
   struct token* input_head;
   struct token* input_tail;
   struct token* token;
   struct token* arg_token;
   struct token* arg_token_head;
   struct token* arg_token_tail;
   struct token* output;
   struct token* output_head;
   struct token* output_tail;
   struct pos pos;
   int given;
   int paren_depth;
   bool done;
   bool done_arg;
   bool surpassed_param_count;
};

struct macro_arg {
   struct macro_arg* next;
   struct token* sequence;
   struct token* sequence_tail;
};

static void read_peeked_token( struct parse* parse );
static void read_token( struct parse* parse, struct token* token );
static void free_expan( struct parse* parse, struct macro_expan* expan );
static struct macro_expan* alloc_expan( struct parse* parse );
static void init_macro_expan( struct macro_expan* expan,
   struct macro_expan* parent_expan );
static void fill( struct parse* parse, struct macro_expan* expan );
static void add_input_token( struct parse* parse, struct macro_expan* expan,
   const struct token* token );
static void read_arg_list( struct parse* parse, struct macro_expan* expan );
static void skip_whitespace( struct parse* parse, struct macro_expan* expan );
static void read_arg( struct parse* parse, struct macro_expan* expan );
static struct macro_arg* alloc_arg( struct parse* parse );
static void append_arg( struct macro_expan* expan, struct macro_arg* arg );
static void read_arg_token( struct parse* parse, struct macro_expan* expan );
static void add_arg_token( struct parse* parse, struct macro_expan* expan,
   const struct token* token );
static void perform_expan( struct parse* parse, struct macro_expan* expan );
static void expand_predef_macro( struct parse* parse,
   struct macro_expan* expan );
static void expand_predef_line( struct parse* parse,
   struct macro_expan* expan );
static void expand_predef_file( struct parse* parse,
   struct macro_expan* expan );
static void expand_predef_time( struct parse* parse,
   struct macro_expan* expan );
static void expand_predef_date( struct parse* parse,
   struct macro_expan* expan );
static void expand_macro( struct parse* parse,
   struct macro_expan* expan );
static void expand_id( struct parse* parse, struct macro_expan* expan );
static struct macro_arg* find_arg( struct macro_expan* expan,
   const char* param_name );
static void stringize( struct parse* parse, struct macro_expan* expan );
static bool expand_param( struct parse* parse, struct macro_expan* expan );
static bool expand_nested_macro( struct parse* parse,
   struct macro_expan* expan );
static void output_expan( struct macro_expan* expan,
   struct macro_expan* other_expan );
static void output( struct parse* parse, struct macro_expan* expan,
   struct token* token );
static void concat( struct parse* parse, struct macro_expan* expan,
   struct token* lside );
static struct token* push_token( struct parse* parse );
static void free_token_list( struct parse* parse, struct token* head,
   struct token* tail );

void p_init_stream( struct parse* parse ) {
   parse->tk = TK_END;
   parse->prev_tk = TK_NL;
   parse->tk_text = "";
   parse->tk_length = 0;
   parse->token_free = NULL;
   parse->tkque_free_entry = NULL;
   parse->source_token = &parse->token_source;
   parse->tkque = NULL;
   p_init_token_queue( &parse->parser_tkque, false );
}

void p_read_stream( struct parse* parse ) {
   while ( parse->source_entry->source ) {
      read_peeked_token( parse );
      if ( parse->token->type != TK_END ) {
         break;
      }
      p_pop_source( parse );
   }
   parse->line_beginning = ( parse->prev_tk == TK_NL ) ||
      ( parse->prev_tk == TK_HORZSPACE && parse->line_beginning );
   parse->prev_tk = parse->token->type;
}

void read_peeked_token( struct parse* parse ) {
   if ( parse->tkque->size > 0 ) {
      parse->token = p_shift_entry( parse, parse->tkque );
   }
   else {
      parse->token = &parse->token_source;
      read_token( parse, parse->token );
   }
}

// Begins by reading tokens from a macro expansion. When a macro expansion is
// not present, or there are no more tokens available in the macro expansion,
// reads a token from the source file.
void read_token( struct parse* parse, struct token* token ) {
   // Read from a macro expansion.
   while ( parse->macro_expan ) {
      if ( parse->macro_expan->output ) {
         token[ 0 ] = parse->macro_expan->output[ 0 ];
         parse->macro_expan->output = token->next;
         if ( token->type == TK_MACRONAME ) {
            token->type = TK_ID;
         }
         return;
      }
      else {
         struct macro_expan* expan = parse->macro_expan;
         parse->macro_expan = expan->prev;
         free_expan( parse, expan );
      }
   }
   // Read from a source file.
   p_read_source( parse, token );
}

bool p_expand_macro( struct parse* parse ) {
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
   // Initialize.
   expan = alloc_expan( parse );
   init_macro_expan( expan, NULL );
   expan->macro = macro;
   expan->prev = parse->macro_expan;
   expan->token = macro->body;
   expan->arg_token = NULL;
   if ( parse->macro_expan ) {
      expan->pos = parse->macro_expan->pos;
   }
   else {
      expan->pos = parse->token->pos;
   }
   if ( macro->func_like ) {
      fill( parse, expan );
      read_arg_list( parse, expan );
   }
   perform_expan( parse, expan );
   parse->macro_expan = expan;
   return true;
}

struct macro_expan* alloc_expan( struct parse* parse ) {
   struct macro_expan* expan;
   if ( parse->macro_expan_free ) {
      expan = parse->macro_expan_free;
      parse->macro_expan_free = expan->prev;
   }
   else {
      expan = mem_alloc( sizeof( *expan ) );
   }
   return expan;
}

void free_expan( struct parse* parse, struct macro_expan* expan ) {
   struct macro_arg* arg = expan->args;
   while ( arg ) {
      struct macro_arg* next = arg->next;
      free_token_list( parse, arg->sequence, arg->sequence_tail );
      arg->next = parse->macro_arg_free;
      parse->macro_arg_free = arg;
      arg = next;
   }
   free_token_list( parse, expan->input_head, expan->input_tail );
   free_token_list( parse, expan->output_head, expan->output_tail );
   expan->prev = parse->macro_expan_free;
   parse->macro_expan_free = expan;
}

void init_macro_expan( struct macro_expan* expan,
   struct macro_expan* parent_expan ) {
   expan->macro = NULL;
   expan->prev = parent_expan;
   expan->args = NULL;
   expan->args_tail = NULL;
   expan->input = NULL;
   expan->input_head = NULL;
   expan->input_tail = NULL;
   expan->token = NULL;
   expan->arg_token = NULL;
   expan->arg_token_head = NULL;
   expan->arg_token_tail = NULL;
   expan->output = NULL;
   expan->output_head = NULL;
   expan->output_tail = NULL;
   expan->given = 0;
   expan->paren_depth = 1;
   expan->done = false;
   expan->done_arg = false;
   expan->surpassed_param_count = false;
   if ( parent_expan ) {
      expan->input = parent_expan->arg_token;
   }
}

void fill( struct parse* parse, struct macro_expan* expan ) {
   int paren_depth = 0;
   bool done = false;
   while ( ! done ) {
      p_read_stream( parse );
      add_input_token( parse, expan, parse->token );
      switch ( parse->token->type ) {
      case TK_PAREN_L:
         ++paren_depth;
         break;
      case TK_PAREN_R:
         --paren_depth;
         done = ( paren_depth == 0 );
         break;
      case TK_END:
         done = true;
         break;
      default:
         break;
      }
   }
   expan->input = expan->input_head;
}

void add_input_token( struct parse* parse, struct macro_expan* expan,
   const struct token* token ) {
   struct token* input_token = p_alloc_token( parse );
   memcpy( input_token, token, sizeof( *token ) );
   input_token->next = NULL;
   if ( expan->input_head ) {
      expan->input_tail->next = input_token;
   }
   else {
      expan->input_head = input_token;
   }
   expan->input_tail = input_token;
}

void read_arg_list( struct parse* parse, struct macro_expan* expan ) {
   skip_whitespace( parse, expan );
   // Consume `(` token.
   expan->input = expan->input->next;
   skip_whitespace( parse, expan );
   if ( expan->input->type == TK_PAREN_R ) {
      if ( expan->macro->param_count > 0 ) {
         append_arg( expan, alloc_arg( parse ) );
      }
   }
   else {
      expan->done = false;
      while ( ! expan->done ) {
         read_arg( parse, expan );
      }
   }
   // Terminating `)`.
   if ( expan->input->type != TK_PAREN_R ) {
      p_diag( parse, DIAG_POS, &expan->pos,
         "unterminated macro" );
      p_bail( parse );
   }
   if ( expan->given < expan->macro->param_count ) {
      p_diag( parse, DIAG_POS_ERR, &expan->pos,
         "too little arguments in macro expansion" );
      p_bail( parse );
   }
   if ( expan->given > expan->macro->param_count &&
      ! expan->macro->variadic ) {
      p_diag( parse, DIAG_POS_ERR, &expan->pos,
         "too many arguments in macro expansion" );
      p_bail( parse );
   }
}

void skip_whitespace( struct parse* parse, struct macro_expan* expan ) {
   while (
      expan->input->type == TK_HORZSPACE ||
      expan->input->type == TK_NL ) {
      expan->input = expan->input->next;
   }
}

void read_arg( struct parse* parse, struct macro_expan* expan ) {
   // Read sequence of tokens that make up the argument.
   if ( expan->macro->variadic &&
      expan->given == expan->macro->param_count - 1 ) {
      expan->surpassed_param_count = true;
   }
   expan->arg_token_head = NULL;
   expan->arg_token_tail = NULL;
   expan->done_arg = false;
   while ( ! expan->done_arg ) {
      read_arg_token( parse, expan );
   }
   // Add argument.
   struct macro_arg* arg = alloc_arg( parse );
   arg->sequence = expan->arg_token_head;
   arg->sequence_tail = expan->arg_token_tail;
   append_arg( expan, arg );
}

void read_arg_token( struct parse* parse, struct macro_expan* expan ) {
   if ( ! expan->input ) {
      p_diag( parse, DIAG_POS_ERR, &expan->pos,
         "unterminated macro" );
      p_bail( parse );
   }
   switch ( expan->input->type ) {
   case TK_NL:
   case TK_HORZSPACE:
      if ( expan->arg_token_tail &&
         expan->arg_token_tail->type != TK_HORZSPACE &&
         expan->input->next && expan->input->next->type != TK_PAREN_R ) {
         struct token token;
         p_init_token( &token );
         token.type = TK_HORZSPACE;
         token.text = " ";
         token.length = 1;
         add_arg_token( parse, expan, &token );
      }
      expan->input = expan->input->next;
      break;
   case TK_PAREN_L:
      ++expan->paren_depth;
      add_arg_token( parse, expan, expan->input );
      expan->input = expan->input->next;
      break;
   case TK_PAREN_R:
      --expan->paren_depth;
      if ( expan->paren_depth == 0 ) {
         expan->done_arg = true;
         expan->done = true;
      }
      else {
         add_arg_token( parse, expan, expan->input );
         expan->input = expan->input->next;
      }
      break;
   case TK_COMMA:
      if ( expan->paren_depth == 1 &&
         ! expan->surpassed_param_count ) {
         expan->done_arg = true;
      }
      else {
         add_arg_token( parse, expan, expan->input );
      }
      expan->input = expan->input->next;
      break;
   default:
      add_arg_token( parse, expan, expan->input );
      expan->input = expan->input->next;
   }
}

void add_arg_token( struct parse* parse, struct macro_expan* expan,
   const struct token* token ) {
   struct token* arg_token = p_alloc_token( parse );
   memcpy( arg_token, token, sizeof( *token ) );
   arg_token->next = NULL;
   if ( expan->arg_token_head ) {
      expan->arg_token_tail->next = arg_token;
   }
   else {
      expan->arg_token_head = arg_token;
   }
   expan->arg_token_tail = arg_token;
}

struct macro_arg* alloc_arg( struct parse* parse ) {
   struct macro_arg* arg;
   if ( parse->macro_arg_free ) {
      arg = parse->macro_arg_free;
      parse->macro_arg_free = arg->next;
   }
   else {
      arg = mem_alloc( sizeof( *arg ) );
   }
   arg->next = NULL;
   arg->sequence = NULL;
   arg->sequence_tail = NULL;
   return arg;
}

void append_arg( struct macro_expan* expan, struct macro_arg* arg ) {
   if ( expan->args ) {
      expan->args_tail->next = arg;
   }
   else {
      expan->args = arg;
   }
   expan->args_tail = arg;
   ++expan->given;
}

void perform_expan( struct parse* parse, struct macro_expan* expan ) {
   if ( expan->macro->predef ) {
      expand_predef_macro( parse, expan );
   }
   else {
      expand_macro( parse, expan );
   }
}

void expand_predef_macro( struct parse* parse, struct macro_expan* expan ) {
   switch ( expan->macro->predef ) {
   case PREDEFMACRO_LINE:
      expand_predef_line( parse, expan );
      break;
   case PREDEFMACRO_FILE:
      expand_predef_file( parse, expan );
      break;
   case PREDEFMACRO_TIME:
      expand_predef_time( parse, expan );
      break;
   case PREDEFMACRO_DATE:
      expand_predef_date( parse, expan );
      break;
   default:
      UNREACHABLE();
   }
}

void expand_predef_line( struct parse* parse, struct macro_expan* expan ) {
   char value[ 11 ];
   const char* file;
   int line;
   int column;
   t_decode_pos( parse->task, &expan->pos, &file, &line, &column );
   int length = snprintf( value, sizeof( value ), "%d", line );
   struct token token;
   p_init_token( &token );
   token.type = TK_LIT_DECIMAL;
   token.text = p_intern_text( parse, value, length );
   token.length = length;
   token.pos = expan->pos;
   output( parse, expan, &token );
}

void expand_predef_file( struct parse* parse, struct macro_expan* expan ) {
   const char* file;
   int line;
   int column;
   t_decode_pos( parse->task, &expan->pos, &file, &line, &column );
   struct token token;
   p_init_token( &token );
   token.type = TK_LIT_STRING;
   token.length = strlen( file );
   token.text = p_intern_text( parse, file, token.length );
   token.pos = expan->pos;
   output( parse, expan, &token );
}

void expand_predef_time( struct parse* parse, struct macro_expan* expan ) {
   char value[ 9 ];
   time_t timestamp;
   time( &timestamp );
   struct tm* info = localtime( &timestamp );
   int length = snprintf( value, sizeof( value ), "%02d:%02d:%02d",
      info->tm_hour, info->tm_min, info->tm_sec );
   struct token token;
   p_init_token( &token );
   token.type = TK_LIT_STRING;
   token.text = p_intern_text( parse, value, length );
   token.length = length;
   token.pos = expan->pos;
   output( parse, expan, &token );
}

void expand_predef_date( struct parse* parse, struct macro_expan* expan ) {
   char value[ 12 ];
   time_t timestamp;
   time( &timestamp );
   struct tm* info = localtime( &timestamp );
   int length = strftime( value, sizeof( value ), "%b %e %Y", info );
   struct token token;
   p_init_token( &token );
   token.type = TK_LIT_STRING;
   token.text = p_intern_text( parse, value, length );
   token.length = length;
   token.pos = expan->pos;
   output( parse, expan, &token );
}

void expand_macro( struct parse* parse, struct macro_expan* expan ) {
   while ( expan->token ) {
      if ( expan->token->type == TK_ID ) {
         expand_id( parse, expan );
      }
      else if ( expan->token->type == TK_STRINGIZE ) {
         stringize( parse, expan );
         expan->token = expan->token->next;
      }
      else {
         output( parse, expan, expan->token );
         expan->token = expan->token->next;
      }
   }
   // Concatenate tokens. Remove placemarker tokens.
   struct token* prev_token = NULL;
   struct token* token = expan->output_head;
   while ( token ) {
      if ( token->next && token->next->type == TK_HASHHASH ) {
         concat( parse, expan, token );
      }
      else if ( token->type == TK_PLACEMARKER ) {
         if ( prev_token ) {
            prev_token->next = token->next;
         }
         else {
            expan->output_head = token->next;
         }
         if ( token == expan->output_tail ) {
            expan->output_tail = prev_token;
         }
         struct token* next = token->next;
         p_free_token( parse, token );
         token = next;
      }
      else {
         prev_token = token;
         token = token->next;
      }
   }
   expan->output = expan->output_head;
}

void expand_id( struct parse* parse, struct macro_expan* expan ) {
   if ( ( expan->token->next &&
      expan->token->next->type == TK_HASHHASH ) || (
      expan->output_tail && expan->output_tail->type == TK_HASHHASH ) ) {
      struct macro_arg* arg = find_arg( expan, expan->token->text );
      if ( arg ) {
         if ( arg->sequence ) {
            struct token* token = arg->sequence;
            while ( token ) {
               output( parse, expan, token );
               token = token->next;
            }
         }
         else {
            struct token token;
            p_init_token( &token );
            token.type = TK_PLACEMARKER;
            output( parse, expan, &token );
         }
         expan->token = expan->token->next;
      }
      else {
         output( parse, expan, expan->token );
         expan->token = expan->token->next;
      }
   }
   else {
      if ( ! expand_param( parse, expan ) ) {
         output( parse, expan, expan->token );
         expan->token = expan->token->next;
      }
   }
}

struct macro_arg* find_arg( struct macro_expan* expan,
   const char* param_name ) {
   struct macro_arg* arg = expan->args;
   struct macro_param* param = expan->macro->param_head;
   while ( param && strcmp( param->name, param_name ) != 0 ) {
      param = param->next;
      arg = arg->next;
   }
   return arg;
}

bool expand_param( struct parse* parse, struct macro_expan* expan ) {
   struct macro_arg* arg = find_arg( expan, expan->token->text );
   if ( ! arg ) {
      return false;
   }
   expan->arg_token = arg->sequence;
   while ( expan->arg_token ) {
      if ( expan->arg_token->type == TK_ID ) {
         if ( ! expand_nested_macro( parse, expan ) ) {
            output( parse, expan, expan->arg_token );
            expan->arg_token = expan->arg_token->next;
         }
      }
      else {
         output( parse, expan, expan->arg_token );
         expan->arg_token = expan->arg_token->next;
      }
   }
   expan->token = expan->token->next;
   return true;
}

bool expand_nested_macro( struct parse* parse, struct macro_expan* expan ) {
   struct macro* macro = p_find_macro( parse, expan->arg_token->text );
   if ( ! macro ) {
      return false;
   }
   // A function-like macro is expanded only when a list of arguments is
   // provided.
   if ( macro->func_like ) {
      struct token* token = expan->arg_token->next;
      if ( ! token ) {
         return false;
      }
      while (
         token->type == TK_HORZSPACE ||
         token->type == TK_NL ) {
         token = token->next;
      }
      if ( token->type != TK_PAREN_L ) {
         return false;
      }
   }
   // Initialize.
   struct macro_expan* nested_expan = alloc_expan( parse );
   init_macro_expan( nested_expan, expan );
   nested_expan->macro = macro;
   nested_expan->token = macro->body;
   nested_expan->pos = expan->arg_token->pos;
   if ( macro->func_like ) {
      nested_expan->input = nested_expan->input->next;
      read_arg_list( parse, nested_expan );
   }
   perform_expan( parse, nested_expan );
   output_expan( expan, nested_expan );
   expan->arg_token = nested_expan->input->next;
   free_expan( parse, nested_expan );
   return true;
}

void output_expan( struct macro_expan* expan,
   struct macro_expan* other_expan ) {
   if ( expan->output_head ) {
      expan->output_tail->next = other_expan->output;
   }
   else {
      expan->output_head = other_expan->output;
   }
   expan->output_tail = other_expan->output_tail;
   other_expan->output_head = NULL;
   other_expan->output_tail = NULL;
}

void output( struct parse* parse, struct macro_expan* expan,
   struct token* token ) {
   struct token* dup_token = p_alloc_token( parse );
   memcpy( dup_token, token, sizeof( *token ) );
   dup_token->next = NULL;
   if ( expan->output_head ) {
      expan->output_tail->next = dup_token;
   }
   else {
      expan->output_head = dup_token;
   }
   expan->output_tail = dup_token;
}

// `#` operator.
void stringize( struct parse* parse, struct macro_expan* expan ) {
   str_clear( &parse->temp_text );
   struct macro_arg* arg = find_arg( expan, expan->token->text );
   struct token* token = arg->sequence;
   while ( token ) {
      str_append( &parse->temp_text, token->text );
      token = token->next;
   }
   struct token result;
   p_init_token( &result );
   result.type = TK_LIT_STRING;
   result.text = p_intern_text( parse, parse->temp_text.value,
      parse->temp_text.length );
   result.length = parse->temp_text.length;
   result.pos = expan->pos;
   output( parse, expan, &result );
}

// `##` operator.
void concat( struct parse* parse, struct macro_expan* expan,
   struct token* lside ) {
   struct token* rside = lside->next->next;
   if ( lside->type == TK_PLACEMARKER ) {
      struct token* token = lside->next;
      memcpy( lside, rside, sizeof( *lside ) );
      p_free_token( parse, token->next );
      p_free_token( parse, token );
   }
   else if ( rside->type == TK_PLACEMARKER ) {
      struct token* token = lside->next;
      lside->next = rside->next;
      p_free_token( parse, token->next );
      p_free_token( parse, token );
   }
   else {
      struct str text;
      str_init( &text );
      str_append( &text, lside->text );
      str_append( &text, rside->text );
      enum tk type = p_identify_token_type( text.value );
      if ( type == TK_NONE ) {
         p_diag( parse, DIAG_POS_ERR, &expan->pos,
            "concatenating `%s` and `%s` produces an invalid token",
            lside->text, rside->text );
         p_bail( parse );
      }
      struct token* token = lside->next;
      memcpy( lside, rside, sizeof( *lside ) );
      lside->type = type;
      lside->text = text.value;
      p_free_token( parse, token->next );
      p_free_token( parse, token );
   }
}

void p_init_streamtk_iter( struct parse* parse, struct streamtk_iter* iter ) {
   iter->entry = parse->tkque->head;
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
   struct queue_entry* entry = p_push_entry( parse, parse->tkque );
   entry->token = p_alloc_token( parse );
   entry->token_allocated = true;
   read_token( parse, entry->token );
   return entry->token;
}

// NOTE: Does not initialize fields.
struct token* p_alloc_token( struct parse* parse ) {
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

void p_init_token( struct token* token ) {
   token->next = NULL;
   token->modifiable_text = NULL;
   token->text = "";
   t_init_pos_id( &token->pos, ALTERN_FILENAME_COMPILER );
   token->type = TK_END;
   token->length = 0;
}

void p_free_token( struct parse* parse, struct token* token ) {
   token->next = parse->token_free;
   parse->token_free = token;
}

void free_token_list( struct parse* parse, struct token* head,
   struct token* tail ) {
   if ( head ) {
      tail->next = parse->token_free;
      parse->token_free = head;
   }
}