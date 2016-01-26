#include <string.h>

#include "phase.h"

struct macro {
   const char* name;
   struct macro* next;
   struct macro_param* param_head;
   struct macro_param* param_tail;
   struct token* body;
   struct token* body_tail;
   struct pos pos;
   bool func_like;
   bool variadic;
};

struct macro_param {
   const char* name;
   struct macro_param* next;
};

struct ifdirc {
   struct ifdirc* prev;
   const char* name;
   struct pos pos;
};

struct elsedirc_search {
   int depth;
   bool done;
};

static void output_source( struct parse* parse, struct str* output );
static void output_token( struct parse* parse, struct str* output );
static void read_token( struct parse* parse );
static bool pseudo_dirc( struct parse* parse );
static void read_dirc( struct parse* parse );
static void read_known_dirc( struct parse* parse, struct pos* pos );
static void read_include( struct parse* parse );
static void read_error( struct parse* parse, struct pos* pos );
static void read_line( struct parse* parse );
static void read_define( struct parse* parse );
static struct macro* alloc_macro( struct parse* parse );
static bool valid_macro_name( const char* name );
static void read_param_list( struct parse* parse, struct macro* macro );
static struct macro_param* alloc_param( struct parse* parse );
static void append_param( struct macro* macro, struct macro_param* param );
static void read_body( struct parse* parse, struct macro* macro );
static void read_body_item( struct parse* parse, struct macro* macro );
static struct token* alloc_token( struct parse* parse );
static void append_token( struct macro* macro, struct token* token );
static struct macro* find_macro( struct parse* parse, const char* name );
static bool same_macro( struct macro* a, struct macro* b );
static void add_macro( struct parse* parse, struct macro* macro );
static struct macro* remove_macro( struct parse* parse, const char* name );
static void free_macro( struct parse* parse, struct macro* macro );
static void read_undef( struct parse* parse );
static bool space_token( struct parse* parse );
static bool dirc_end( struct parse* parse );
static void read_ifdef( struct parse* parse, struct pos* pos );
static struct ifdirc* push_ifdirc( struct parse* parse );
static void find_else( struct parse* parse );
static void read_nested_ifelsedirc( struct parse* parse,
   struct elsedirc_search* search );
static void read_else( struct parse* parse );
static void read_endif( struct parse* parse, struct pos* pos );
static bool pop_ifdirc( struct parse* parse );
static void confirm_ifdircs_closed( struct parse* parse );

void p_preprocess( struct parse* parse ) {
   p_load_main_source( parse );
   parse->read_flags = READF_NL | READF_SPACETAB;
   struct str output;
   str_init( &output );
   output_source( parse, &output );
   if ( output.length > 0 && output.value[ output.length - 1 ] != '\n' ) {
      str_append( &output, NEWLINE_CHAR );
   }
   printf( "%s", output.value );
   str_deinit( &output );
}

void output_source( struct parse* parse, struct str* output ) {
   while ( true ) {
      read_token( parse );
      if ( parse->tk != TK_END ) {
         output_token( parse, output );
      }
      else {
         break;
      }
   }
}

void output_token( struct parse* parse, struct str* output ) {
   switch ( parse->tk ) {
   case TK_NL:
      str_append( output, NEWLINE_CHAR );
      break;
   case TK_SPACE:
   case TK_TAB:
      str_append( output, " " );
      break;
   case TK_LIT_STRING:
      str_append( output, "\"" );
      str_append( output, parse->tk_text );
      str_append( output, "\"" );
      break;
   case TK_LIT_CHAR:
      str_append( output, "'" );
      str_append( output, parse->tk_text );
      str_append( output, "'" );
      break;
   default:
      str_append( output, parse->tk_text );
      break;
   }
}

void read_token( struct parse* parse ) {
   p_read_tk( parse );
   switch ( parse->tk ) {
   case TK_NL:
      parse->line_beginning = true;
      if ( ! ( parse->read_flags & READF_NL ) ) {
         read_token( parse );
      }
      break;
   case TK_SPACE:
   case TK_TAB:
      break;
   case TK_HASH:
      // A directive must appear at the beginning of a line.
      if ( parse->line_beginning ) {
         parse->line_beginning = false;
         // NOTE: Maybe instead of testing for pseudo directives, we should
         // test for real directives. 
         if ( ! pseudo_dirc( parse ) ) {
            read_dirc( parse );
         }
      }
      break;
   case TK_END:
      confirm_ifdircs_closed( parse );
      break;
   default:
      parse->line_beginning = false;
      break;
   }
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

void read_dirc( struct parse* parse ) {
   int flags = parse->read_flags;
   parse->read_flags ^= READF_SPACETAB;
   struct pos pos = parse->tk_pos;
   p_test_tk( parse, TK_HASH );
   p_read_tk( parse );
   if ( parse->tk != TK_NL ) {
      read_known_dirc( parse, &pos );
   }
   parse->line_beginning = true;
   parse->read_flags = flags;
   read_token( parse );
}

void read_known_dirc( struct parse* parse, struct pos* pos ) {
   if ( ! parse->token->is_id ) {
      p_unexpect_diag( parse );
      p_unexpect_last( parse, NULL, TK_ID );
      p_bail( parse );
   }
   if ( strcmp( parse->tk_text, "define" ) == 0 ) {
      read_define( parse );
   }
   else if ( strcmp( parse->tk_text, "include" ) == 0 ) {
      read_include( parse );
   }
   else if ( strcmp( parse->tk_text, "ifdef" ) == 0 ||
      strcmp( parse->tk_text, "ifndef" ) == 0 ) {
      read_ifdef( parse, pos );
   }
   else if ( strcmp( parse->tk_text, "else" ) == 0 ) {
      read_else( parse );
   }
   else if ( strcmp( parse->tk_text, "endif" ) == 0 ) {
      read_endif( parse, pos );
   }
   else if ( strcmp( parse->tk_text, "undef" ) == 0 ) {
      read_undef( parse );
   }
   else if ( strcmp( parse->tk_text, "error" ) == 0 ) {
      read_error( parse, pos );
   }
   else if ( strcmp( parse->tk_text, "line" ) == 0 ) {
      read_line( parse );
   }
   else {
      p_diag( parse, DIAG_POS_ERR, pos,
         "unknown directive" );
      p_bail( parse );
   }
}

void read_include( struct parse* parse ) {
   p_test_tk( parse, TK_ID );
   p_read_tk( parse );
   p_test_tk( parse, TK_LIT_STRING );
   const char* path = parse->tk_text;
   struct pos pos = parse->tk_pos;
   p_read_tk( parse );
   p_test_tk( parse, TK_NL );
   struct request request;
   p_init_request( &request, path );
   p_load_source( parse, &request );
   if ( ! request.source ) {
      if ( request.err_loading ) {
         p_diag( parse, DIAG_POS_ERR, &pos,
            "file already being loaded" );
         p_bail( parse );
      }
      else {
         p_diag( parse, DIAG_POS_ERR, &pos,
            "failed to load file: %s", path );
         p_bail( parse );
      }
   }
}

void read_error( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_ID );
   int flags = parse->read_flags;
   parse->read_flags |= READF_SPACETAB;
   p_read_tk( parse );
   struct str message;
   str_init( &message );
   while ( parse->tk != TK_NL ) {
      bool space_separator = false;
      while (
         parse->tk == TK_SPACE ||
         parse->tk == TK_TAB ) {
         space_separator = true;
         p_read_tk( parse );
      }
      if ( message.length > 0 && space_separator ) {
         str_append( &message, " " );
      }
      output_token( parse, &message );
      p_read_tk( parse );
   }
   p_diag( parse, DIAG_POS_ERR | DIAG_CUSTOM, pos, message.value );
   p_bail( parse );
}

void read_line( struct parse* parse ) {
   p_test_tk( parse, TK_ID );
   p_read_tk( parse );
   p_test_tk( parse, TK_LIT_DECIMAL );
   int line = strtol( parse->tk_text, NULL, 10 );
   if ( line == 0 ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "invalid line-number argument" );
      p_bail( parse );
   }
   parse->source->line = line;
   p_read_tk( parse );
   // Custom filename.
   if ( parse->tk == TK_LIT_STRING ) {
      parse->source->file_entry_id = t_add_altern_filename(
         parse->task, parse->tk_text );
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_NL );
}

void read_define( struct parse* parse ) {
   p_test_tk( parse, TK_ID );
   p_read_tk( parse );
   struct macro* macro = alloc_macro( parse );
   // Name.
   p_test_tk( parse, TK_ID );
   if ( ! valid_macro_name( parse->tk_text ) ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "invalid macro name" );
      p_bail( parse );
   }
   macro->name = parse->tk_text;
   macro->pos = parse->tk_pos;
   int flags = parse->read_flags;
   parse->read_flags |= READF_SPACETAB;
   p_read_tk( parse );
   bool space_separator = space_token( parse );
   parse->read_flags = flags;
   // Parameter list.
   if ( space_separator ) {
      p_read_tk( parse );
   }
   else {
      if ( parse->tk == TK_PAREN_L ) {
         read_param_list( parse, macro );
      }
      else {
         // For an object-like macro, there needs to be whitespace between the
         // name and the body of the macro.
         if ( ! dirc_end( parse ) ) {
            p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
               "missing whitespace between macro name and macro body" );
            p_bail( parse );
         }
      }
   }
   // Body.
   read_body( parse, macro );
   struct macro* prev_macro = find_macro( parse, macro->name );
   if ( prev_macro ) {
      if ( same_macro( prev_macro, macro ) ) {
         prev_macro->pos = macro->pos;
         free_macro( parse, macro );
      }
      else {
         p_diag( parse, DIAG_POS_ERR, &macro->pos,
            "macro `%s` redefined", macro->name );
         p_diag( parse, DIAG_POS, &prev_macro->pos,
            "macro previously defined here" );
         p_bail( parse );
      }
   }
   else {
      add_macro( parse, macro );
   }
   p_test_tk( parse, TK_NL );
}

struct macro* alloc_macro( struct parse* parse ) {
   struct macro* macro;
   if ( parse->macro_free ) {
      macro = parse->macro_free;
      parse->macro_free = macro->next;
   }
   else {
      macro = mem_alloc( sizeof( *macro ) );
   }
   macro->name = NULL;
   macro->next = NULL;
   macro->param_head = NULL;
   macro->param_tail = NULL;
   macro->body = NULL;
   macro->func_like = false;
   macro->variadic = false;
   return macro;
}

inline bool valid_macro_name( const char* name ) {
   // At this time, only one name is reserved and cannot be used.
   return ( strcmp( name, "defined" ) != 0 );
}

void read_param_list( struct parse* parse, struct macro* macro ) {
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   // Named parameters.
   while ( parse->tk == TK_ID ) {
      // Check for a duplicate parameter.
      struct macro_param* param = macro->param_head;
      while ( param ) {
         if ( strcmp( param->name, parse->tk_text ) == 0 ) {
            p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
               "duplicate macro parameter" );
            p_bail( parse );
         }
         param = param->next;
      }
      param = alloc_param( parse );
      param->name = parse->tk_text;
      append_param( macro, param );
      p_read_tk( parse );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
         if ( parse->tk != TK_ID && parse->tk != TK_ELLIPSIS ) {
            p_unexpect_diag( parse );
            p_unexpect_name( parse, NULL, "macro-parameter name" );
            p_unexpect_last( parse, NULL, TK_ELLIPSIS );
            p_bail( parse );
         }
      }
   }
   // Variadic parameter.
   if ( parse->tk == TK_ELLIPSIS ) {
      struct macro_param* param = alloc_param( parse );
      param->name = "__va_args__";
      append_param( macro, param );
      macro->variadic = true;
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   macro->func_like = true;
}

struct macro_param* alloc_param( struct parse* parse ) {
   struct macro_param* param = parse->macro_param_free;
   if ( param ) {
      parse->macro_param_free = param->next;
   }
   else {
      param = mem_alloc( sizeof( *param ) );
   }
   param->name = NULL;
   param->next = NULL;
   return param;
}

void append_param( struct macro* macro, struct macro_param* param ) {
   if ( macro->param_head ) {
      macro->param_tail->next = param;
   }
   else {
      macro->param_head = param;
   }
   macro->param_tail = param;
}

void read_body( struct parse* parse, struct macro* macro ) {
   int flags = parse->read_flags;
   parse->read_flags |= READF_SPACETAB;
   while ( ! dirc_end( parse ) ) {
      read_body_item( parse, macro );
   }
   parse->read_flags = flags;
}

void read_body_item( struct parse* parse, struct macro* macro ) {
   struct token* token = alloc_token( parse );
   *token = *parse->token;
   append_token( macro, token );
   p_read_tk( parse );
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

void append_token( struct macro* macro, struct token* token ) {
   if ( macro->body ) {
      macro->body_tail->next = token;
   }
   else {
      macro->body = token;
   }
   macro->body_tail = token;
}

struct macro* find_macro( struct parse* parse, const char* name ) {
   struct macro* macro = parse->macro_head;
   while ( macro ) {
      int result = strcmp( macro->name, name );
      if ( result == 0 ) {
         break;
      }
      if ( result > 0 ) {
         macro = NULL;
         break;
      }
      macro = macro->next;
   }
   return macro;
}

bool same_macro( struct macro* a, struct macro* b ) {
   // Macros need to be of the same kind.
   if ( a->func_like != b->func_like ) {
      return false;
   }
   // Parameter list.
   if ( a->func_like ) {
      struct macro_param* param_a = a->param_head;
      struct macro_param* param_b = b->param_head;
      while ( param_a && param_b ) {
         if ( strcmp( param_a->name, param_b->name ) != 0 ) {
            return false;
         }
         param_a = param_a->next;
         param_b = param_b->next;
      }
      // Parameter count needs to be the same.
      if ( param_a || param_b ) {
         return false;
      }
   }
   // Body.
   struct token* token_a = a->body;
   struct token* token_b = b->body;
   while ( token_a && token_b ) {
      if ( token_a->type != token_b->type ) {
         return false;
      }
      // Tokens that can have different values need to have the same value.
      if ( token_a->text ) {
         if ( strcmp( token_a->text, token_b->text ) != 0 ) {
            return false;
         }
      }
      token_a = token_a->next;
      token_b = token_b->next;
   }
   // Number of tokens need to be the same.
   if ( token_a || token_b ) {
      return false;
   }
   return true;
}

void add_macro( struct parse* parse, struct macro* macro ) {
   struct macro* prev = NULL;
   struct macro* curr = parse->macro_head;
   while ( curr && strcmp( curr->name, macro->name ) < 0 ) {
      prev = curr;
      curr = curr->next;
   }
   if ( prev ) {
      macro->next = prev->next;
      prev->next = macro;
   }
   else {
      macro->next = parse->macro_head;
      parse->macro_head = macro;
   }
}

void free_macro( struct parse* parse, struct macro* macro ) {
   // Reuse parameters.
   if ( macro->param_head ) {
      macro->param_tail->next = parse->macro_param_free;
      parse->macro_param_free = macro->param_head;
   }
   // Reuse tokens.
   if ( macro->body ) {
      macro->body_tail->next = parse->token_free;
      parse->token_free = macro->body;
   }
   // Reuse macro.
   macro->next = parse->macro_free;
   parse->macro_free = macro;
}

void read_undef( struct parse* parse ) {
   p_test_tk( parse, TK_ID );
   p_read_tk( parse );
   p_test_tk( parse, TK_ID );
   struct macro* macro = remove_macro( parse, parse->tk_text );
   if ( macro ) {
      free_macro( parse, macro );
   }
   p_read_tk( parse );
   p_test_tk( parse, TK_NL );
}

struct macro* remove_macro( struct parse* parse, const char* name ) {
   struct macro* macro_prev = NULL;
   struct macro* macro = parse->macro_head;
   while ( macro ) {
      int result = strcmp( macro->name, name );
      if ( result == 0 ) {
         break;
      }
      else if ( result > 0 ) {
         macro = NULL;
         break;
      }
      else {
         macro_prev = macro;
         macro = macro->next;
      }
   }
   if ( macro ) {
      if ( macro_prev ) {
         macro_prev->next = macro->next;
      }
      else {
         parse->macro_head = macro->next;
      }
   }
   return macro;
}

inline bool space_token( struct parse* parse ) {
   return ( parse->tk == TK_SPACE || parse->tk == TK_TAB );
}

inline bool dirc_end( struct parse* parse ) {
   return ( parse->tk == TK_NL || parse->tk == TK_END );
}

void read_ifdef( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_ID );
   const char* name = parse->tk_text;
   p_read_tk( parse );
   p_test_tk( parse, TK_ID );
   struct macro* macro = find_macro( parse, parse->tk_text );
   p_read_tk( parse );
   p_test_tk( parse, TK_NL );
   struct ifdirc* entry = push_ifdirc( parse );
   entry->name = name;
   entry->pos = *pos;
   bool proceed = ( name[ 2 ] == 'n' ) ?
      macro == NULL : macro != NULL;
   if ( ! proceed ) {
      find_else( parse );
   }
}

struct ifdirc* push_ifdirc( struct parse* parse ) {
   struct ifdirc* entry = mem_alloc( sizeof( *entry ) );
   entry->name = NULL;
   entry->prev = parse->ifdirc_stack;
   parse->ifdirc_stack = entry;
   return entry;
}

void find_else( struct parse* parse ) {
   struct elsedirc_search search = { 1, false };
   while ( ! search.done ) {
      if ( parse->tk == TK_END ) {
         confirm_ifdircs_closed( parse );
      }
      if ( parse->tk == TK_NL ) {
         p_read_tk( parse );
         if ( parse->tk == TK_HASH ) {
            read_nested_ifelsedirc( parse, &search );
         }
      }
      else {
         p_read_tk( parse );
      }
   }
}

void read_nested_ifelsedirc( struct parse* parse,
   struct elsedirc_search* search ) {
   struct pos pos = parse->tk_pos;
   p_test_tk( parse, TK_HASH );
   p_read_tk( parse );
   if ( ! parse->token->is_id ) {
      p_unexpect_diag( parse );
      p_unexpect_last( parse, NULL, TK_ID );
      p_bail( parse );
   }
   if ( strcmp( parse->tk_text, "ifdef" ) == 0 ||
      strcmp( parse->tk_text, "ifndef" ) == 0 ) {
      struct ifdirc* entry = push_ifdirc( parse );
      entry->name = parse->tk_text;
      entry->pos = pos;
      ++search->depth;
   }
   else if ( strcmp( parse->tk_text, "else" ) == 0 ) {
      if ( search->depth == 1 ) {
         p_read_tk( parse );
         p_test_tk( parse, TK_NL );
         search->done = true;
      }
   }
   else if ( strcmp( parse->tk_text, "endif" ) == 0 ) {
      pop_ifdirc( parse );
      --search->depth;
      if ( search->depth == 0 ) {
         p_read_tk( parse );
         p_test_tk( parse, TK_NL );
         search->done = true;
      }
   }
}

void read_else( struct parse* parse ) {
   p_test_tk( parse, TK_ELSE );
   p_read_tk( parse );
   p_test_tk( parse, TK_NL );
}

void read_endif( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_ID );
   p_read_tk( parse );
   if ( ! pop_ifdirc( parse ) ) {
      p_diag( parse, DIAG_POS_ERR, pos,
         "#endif used with no open if-directive" );
      p_bail( parse );
   }
   p_test_tk( parse, TK_NL );
}

bool pop_ifdirc( struct parse* parse ) {
   if ( parse->ifdirc_stack ) {
      parse->ifdirc_stack = parse->ifdirc_stack->prev;
      return true;
   }
   return false;
}

void confirm_ifdircs_closed( struct parse* parse ) {
   if ( parse->ifdirc_stack ) {
      struct ifdirc* entry = parse->ifdirc_stack;
      while ( entry ) {
         p_diag( parse, DIAG_POS_ERR, &entry->pos,
            "unterminated #%s", entry->name );
         entry = entry->prev;
      }
      p_bail( parse );
   }
}