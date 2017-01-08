#include <string.h>

#include "phase.h"

#define CMDLINEMACRO_TEXT "1"

enum dirc {
   DIRC_NONE,
   DIRC_DEFINE,
   DIRC_INCLUDE,
   DIRC_IFDEF,
   DIRC_IFNDEF,
   DIRC_IF,
   DIRC_ELIF,
   DIRC_ELSE,
   DIRC_ENDIF,
   DIRC_UNDEF,
   DIRC_ERROR,
   DIRC_LINE,
   DIRC_REGION,
   DIRC_ENDREGION,
   DIRC_NULL
};

struct macro_reading {
   struct macro* macro;
};

struct ifdirc {
   struct ifdirc* prev;
   const char* name;
   struct pos pos;
   struct pos else_pos;
   bool effective;
   bool section_found;
   bool else_found;
};

struct endif_search {
   int depth;
   bool done;
   bool execute_else;
};

static enum dirc identify_dirc( struct parse* parse );
static enum dirc identify_named_dirc( const char* name );
static void read_identified_dirc( struct parse* parse, struct pos* pos,
   enum dirc dirc );
static void read_define( struct parse* parse );
static void read_macro_name( struct parse* parse,
   struct macro_reading* reading );
static bool valid_macro_name( const char* name );
static struct macro* alloc_macro( struct parse* parse );
static void read_macro_param_list( struct parse* parse,
   struct macro_reading* reading );
static void read_param_list( struct parse* parse,
   struct macro_reading* reading );
static struct macro_param* alloc_param( struct parse* parse );
static void append_param( struct macro* macro, struct macro_param* param );
static void read_macro_body( struct parse* parse,
   struct macro_reading* reading );
static void read_body( struct parse* parse, struct macro_reading* reading );
static void read_body_item( struct parse* parse,
   struct macro_reading* reading );
static bool valid_macro_param( struct parse* parse, struct macro* macro );
static void append_token( struct macro* macro, struct token* token );
static void finish_macro( struct parse* parse, struct macro_reading* reading );
static bool same_macro( struct macro* a, struct macro* b );
static void free_macro( struct parse* parse, struct macro* macro );
static void append_macro( struct parse* parse, struct macro* macro );
static void read_include( struct parse* parse );
static void read_error( struct parse* parse, struct pos* pos );
static void read_line( struct parse* parse );
static void read_undef( struct parse* parse );
static struct macro* remove_macro( struct parse* parse, const char* name );
static void read_if( struct parse* parse, struct pos* pos );
static void push_ifdirc( struct parse* parse, const char* name,
   struct pos* pos );
static bool pop_ifdirc( struct parse* parse );
static void read_ifdef( struct parse* parse, struct pos* pos );
static void find_elif( struct parse* parse );
static void init_endif_search( struct endif_search* search,
   bool execute_else );
static void find_endif( struct parse* parse, struct endif_search* search );
static void read_search_dirc( struct parse* parse, struct endif_search* search,
   struct pos* pos );
static void read_elif( struct parse* parse, struct endif_search* search,
   struct pos* pos );
static void read_else( struct parse* parse, struct endif_search* search,
   struct pos* pos );
static void read_endif( struct parse* parse, struct endif_search* search,
   struct pos* pos );
static void skip_section( struct parse* parse, struct pos* pos );
static void read_region( struct parse* parse );

bool p_read_dirc( struct parse* parse ) {
   enum dirc dirc = identify_dirc( parse );
   if ( dirc != DIRC_NONE ) {
      struct pos pos = parse->token->pos;
      p_test_preptk( parse, TK_HASH );
      p_read_preptk( parse );
      read_identified_dirc( parse, &pos, dirc );
      return true;
   }
   return false;
}

enum dirc identify_dirc( struct parse* parse ) {
   struct streamtk_iter iter;
   p_init_streamtk_iter( parse, &iter );
   p_next_stream( parse, &iter );
   if ( iter.token->type == TK_HORZSPACE ) {
      p_next_stream( parse, &iter );
   }
   enum dirc dirc = DIRC_NONE;
   if ( iter.token->type == TK_ID ) {
      dirc = identify_named_dirc( iter.token->text );
      // To stay compatible with ACS, only execute the following directives
      // when inside the #if family of directives.
      switch ( dirc ) {
      case DIRC_DEFINE:
      case DIRC_INCLUDE:
         if ( ! parse->ifdirc ) {
            dirc = DIRC_NONE;
         }
         break;
      default:
         break;
      }
   }
   else if ( iter.token->type == TK_NL ) {
      dirc = DIRC_NULL;
   }
   return dirc;
}

enum dirc identify_named_dirc( const char* name ) {
   static const struct {
      const char* name;
      int dirc;
   } table[] = {
      { "define", DIRC_DEFINE },
      { "include", DIRC_INCLUDE },
      { "ifdef", DIRC_IFDEF },
      { "ifndef", DIRC_IFNDEF },
      { "if", DIRC_IF },
      { "elif", DIRC_ELIF },
      { "else", DIRC_ELSE },
      { "endif", DIRC_ENDIF },
      { "undef", DIRC_UNDEF },
      { "error", DIRC_ERROR },
      { "line", DIRC_LINE },
      { "region", DIRC_REGION },
      { "endregion", DIRC_ENDREGION },
      { NULL, DIRC_NONE }
   };
   int i = 0;
   while ( table[ i ].name &&
      strcmp( name, table[ i ].name ) != 0 ) {
      ++i;
   }
   return table[ i ].dirc;
}

void read_identified_dirc( struct parse* parse, struct pos* pos,
   enum dirc dirc ) {
   switch ( dirc ) {
   case DIRC_DEFINE:
      read_define( parse );
      break;
   case DIRC_INCLUDE:
      read_include( parse );
      break;
   case DIRC_ERROR:
      read_error( parse, pos );
      break;
   case DIRC_LINE:
      read_line( parse );
      break;
   case DIRC_UNDEF:
      read_undef( parse );
      break;
   case DIRC_IFDEF:
   case DIRC_IFNDEF:
      read_ifdef( parse, pos );
      break;
   case DIRC_IF:
      read_if( parse, pos );
      break;
   case DIRC_ELIF:
   case DIRC_ELSE:
   case DIRC_ENDIF:
      skip_section( parse, pos );
      break;
   case DIRC_REGION:
   case DIRC_ENDREGION:
      read_region( parse );
      break;
   case DIRC_NULL:
      break;
   case DIRC_NONE:
   default:
      UNREACHABLE()
   }
}

void read_define( struct parse* parse ) {
   p_test_preptk( parse, TK_ID );
   p_read_preptk( parse );
   struct macro_reading reading;
   read_macro_name( parse, &reading );
   read_macro_param_list( parse, &reading );
   read_macro_body( parse, &reading );
   finish_macro( parse, &reading );
}

void read_macro_name( struct parse* parse, struct macro_reading* reading ) {
   p_test_preptk( parse, TK_ID );
   if ( ! valid_macro_name( parse->token->text ) ) {
      p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
         "invalid macro name" );
      p_bail( parse );
   }
   struct macro* macro = alloc_macro( parse );
   macro->name = parse->token->text;
   macro->pos = parse->token->pos;
   reading->macro = macro;
   p_read_stream( parse );
}

inline bool valid_macro_name( const char* name ) {
   // At this time, only one name is reserved and cannot be used.
   return ( strcmp( name, "defined" ) != 0 );
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
   macro->body_tail = NULL;
   t_init_pos_id( &macro->pos, ALTERN_FILENAME_COMPILER );
   macro->param_count = 0;
   macro->predef = PREDEFMACRO_NONE;
   macro->func_like = false;
   macro->variadic = false;
   return macro;
}

void read_macro_param_list( struct parse* parse,
   struct macro_reading* reading ) {
   if ( parse->token->type == TK_PAREN_L ) {
      read_param_list( parse, reading );
   }
}

void read_param_list( struct parse* parse, struct macro_reading* reading ) {
   p_test_preptk( parse, TK_PAREN_L );
   p_read_preptk( parse );
   // Named parameters.
   bool comma = false;
   while ( parse->token->type == TK_ID ) {
      struct macro_param* param = reading->macro->param_head;
      while ( param ) {
         if ( strcmp( param->name, parse->token->text ) == 0 ) {
            p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
               "duplicate macro parameter" );
            p_bail( parse );
         }
         param = param->next;
      }
      param = alloc_param( parse );
      param->name = parse->token->text;
      append_param( reading->macro, param );
      p_read_preptk( parse );
      comma = ( parse->token->type == TK_COMMA );
      if ( comma ) {
         p_read_preptk( parse );
         if ( ! (
            parse->token->type == TK_ID ||
            parse->token->type == TK_ELLIPSIS ) ) {
            p_unexpect_diag( parse );
            p_unexpect_name( parse, NULL, "macro parameter name" );
            p_unexpect_last( parse, NULL, TK_ELLIPSIS );
            p_bail( parse );
         }
      }
   }
   // Variadic parameter.
   if ( parse->token->type == TK_ELLIPSIS &&
      ( comma || reading->macro->param_count == 0 ) ) {
      struct macro_param* param = alloc_param( parse );
      param->name = "__VA_ARGS__";
      append_param( reading->macro, param );
      reading->macro->variadic = true;
      parse->variadic_macro_context = true;
      p_read_preptk( parse );
   }
   p_test_preptk( parse, TK_PAREN_R );
   p_read_preptk( parse );
   reading->macro->func_like = true;
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
   ++macro->param_count;
}

void read_macro_body( struct parse* parse, struct macro_reading* reading ) {
   if ( parse->token->type != TK_NL ) {
      read_body( parse, reading );
   }
}

void read_body( struct parse* parse, struct macro_reading* reading ) {
   if ( parse->token->type == TK_HORZSPACE ) {
      p_read_preptk( parse );
   }
   else {
      // For an object-like macro, there needs to be whitespace between the
      // name and the body of the macro.
      if ( ! reading->macro->func_like ) {
         p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
            "missing whitespace between macro name and macro body" );
         p_bail( parse );
      }
   }
   while ( parse->token->type != TK_NL ) {
      read_body_item( parse, reading );
   }
   if ( reading->macro->body_tail &&
      reading->macro->body_tail->type == TK_HASHHASH ) {
      p_diag( parse, DIAG_POS_ERR, &reading->macro->body_tail->pos,
         "`##` operator at end of macro body" );
      p_bail( parse );
   }
}

void read_body_item( struct parse* parse, struct macro_reading* reading ) {
   // `#` operator.
   if ( parse->token->type == TK_HASH ) {
      if ( reading->macro->func_like ) {
         p_read_preptk( parse );
         p_test_preptk( parse, TK_ID );
         if ( ! valid_macro_param( parse, reading->macro ) ) {
            p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
               "`%s` not a parameter of `%s` macro",
               parse->token->text, reading->macro->name );
            p_bail( parse );
         }
         parse->token->type = TK_STRINGIZE;
      }
      else {
         parse->token->type = TK_PROCESSEDHASH;
      }
   }
   // `##` operator.
   else if ( parse->token->type == TK_HASHHASH ) {
      if ( ! reading->macro->body ) {
         p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
            "`##` operator at beginning of macro body" );
         p_bail( parse );
      }
   }
   else if ( parse->token->type == TK_HORZSPACE ) {
      struct streamtk_iter iter;
      p_init_streamtk_iter( parse, &iter );
      p_next_stream( parse, &iter );
      if (
         iter.token->type == TK_NL ||
         iter.token->type == TK_HASHHASH || (
         reading->macro->body_tail &&
         reading->macro->body_tail->type == TK_HASHHASH ) ) {
         p_read_stream( parse );
         return;
      }
   }
   struct token* token = p_alloc_token( parse );
   *token = *parse->token;
   if ( token->type == TK_HORZSPACE ) {
      token->length = 1;
   }
   append_token( reading->macro, token );
   if ( strcmp( token->text, reading->macro->name ) == 0 ) {
      struct macro_param* param = reading->macro->param_head;
      while ( param && strcmp( param->name, token->text ) != 0 ) {
         param = param->next;
      }
      if ( ! param ) {
         token->type = TK_MACRONAME;
      }
   }
   p_read_stream( parse );
}

bool valid_macro_param( struct parse* parse, struct macro* macro ) {
   struct macro_param* param = macro->param_head;
   while ( param && param->name ) {
      if ( strcmp( parse->token->text, param->name ) == 0 ) {
         return true;
      }
      param = param->next;
   }
   return false;
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

void finish_macro( struct parse* parse, struct macro_reading* reading ) {
   struct macro* prev_macro = p_find_macro( parse, reading->macro->name );
   if ( prev_macro ) {
      if ( ! ( prev_macro->predef == PREDEFMACRO_NONE ) ) {
         p_diag( parse, DIAG_POS_ERR, &reading->macro->pos,
            "redefining predefined macro" );
         p_bail( parse );
      }
      if ( same_macro( prev_macro, reading->macro ) ) {
         prev_macro->pos = reading->macro->pos;
         free_macro( parse, reading->macro );
      }
      else {
         p_diag( parse, DIAG_POS_ERR, &reading->macro->pos,
            "macro `%s` redefined", reading->macro->name );
         p_diag( parse, DIAG_POS, &prev_macro->pos,
            "macro previously defined here" );
         p_bail( parse );
      }
   }
   else {
      append_macro( parse, reading->macro );
   }
   parse->variadic_macro_context = false;
}

struct macro* p_find_macro( struct parse* parse, const char* name ) {
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

void append_macro( struct parse* parse, struct macro* macro ) {
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

void p_clear_macros( struct parse* parse ) {
   struct macro* macro = parse->macro_head;
   while ( macro ) {
      parse->macro_head = macro->next;
      free_macro( parse, macro );
      macro = parse->macro_head;
   }
}

void read_include( struct parse* parse ) {
   p_test_preptk( parse, TK_ID );
   p_read_expanpreptk( parse );
   p_test_preptk( parse, TK_LIT_STRING );
   const char* path = parse->token->text;
   struct pos pos = parse->token->pos;
   p_read_expanpreptk( parse );
   p_test_preptk( parse, TK_NL );
   p_load_included_source( parse, path, &pos );
}

void read_error( struct parse* parse, struct pos* pos ) {
   p_test_preptk( parse, TK_ID );
   p_read_preptk( parse );
   struct str message;
   str_init( &message );
   str_append( &message, "#error " );
   while ( parse->token->type != TK_NL ) {
      str_append( &message, parse->token->text );
      p_read_stream( parse );
   }
   p_diag( parse, DIAG_POS_ERR, pos, message.value );
   p_bail( parse );
}

void read_line( struct parse* parse ) {
   p_test_preptk( parse, TK_ID );
   p_read_preptk( parse );
   p_test_preptk( parse, TK_LIT_DECIMAL );
   int line = strtol( parse->token->text, NULL, 10 );
   if ( line == 0 ) {
      p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
         "invalid line-number argument" );
      p_bail( parse );
   }
   parse->source->line = line;
   p_read_preptk( parse );
   const char* filename = NULL;
   if ( parse->token->type == TK_LIT_STRING ) {
      filename = parse->token->text;
      p_read_preptk( parse );
   }
   p_test_preptk( parse, TK_NL );
   parse->source->line = line;
   if ( filename ) {
      parse->source->file_entry_id =
         t_add_altern_filename( parse->task, filename );
   }
}

void read_undef( struct parse* parse ) {
   p_test_preptk( parse, TK_ID );
   p_read_preptk( parse );
   p_test_preptk( parse, TK_ID );
   if ( ! valid_macro_name( parse->token->text ) ) {
      p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
         "invalid macro name", parse->token->text );
      p_bail( parse );
   }
   struct macro* macro = remove_macro( parse, parse->token->text );
   if ( macro ) {
      if ( ! ( macro->predef == PREDEFMACRO_NONE ) ) {
         p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
            "undefining a predefined macro" );
         p_bail( parse );
      }
      free_macro( parse, macro );
   }
   p_read_preptk( parse );
   p_test_preptk( parse, TK_NL );
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

void read_if( struct parse* parse, struct pos* pos ) {
   p_test_preptk( parse, TK_ID );
   push_ifdirc( parse, parse->token->text, pos );
   int value = p_eval_prep_expr( parse );
   p_test_preptk( parse, TK_NL );
   if ( ! value ) {
      find_elif( parse );
   }
}

void push_ifdirc( struct parse* parse, const char* name, struct pos* pos ) {
   struct ifdirc* entry;
   if ( parse->ifdirc_free ) {
      entry = parse->ifdirc_free;
      parse->ifdirc_free = entry->prev;
   }
   else {
      entry = mem_alloc( sizeof( *entry ) );
   }
   entry->prev = parse->ifdirc;
   entry->name = name;
   entry->pos = *pos;
   entry->effective = false;
   entry->section_found = false;
   entry->else_found = false;
   parse->ifdirc = entry;
}

bool pop_ifdirc( struct parse* parse ) {
   if ( parse->ifdirc ) {
      struct ifdirc* entry = parse->ifdirc;
      parse->ifdirc = entry->prev;
      entry->prev = parse->ifdirc_free;
      parse->ifdirc_free = entry;
      return true;
   }
   return false;
}

// Handles #ifdef/#ifndef.
void read_ifdef( struct parse* parse, struct pos* pos ) {
   p_test_preptk( parse, TK_ID );
   push_ifdirc( parse, parse->token->text, pos );
   p_read_preptk( parse );
   p_test_preptk( parse, TK_ID );
   bool defined = p_is_macro_defined( parse, parse->token->text );
   p_read_preptk( parse );
   p_test_preptk( parse, TK_NL );
   if ( ! (
      ( parse->ifdirc->name[ 2 ] == 'd' && defined ) ||
      ( parse->ifdirc->name[ 2 ] == 'n' && ! defined ) ) ) {
      find_elif( parse );
   }
}

bool p_is_macro_defined( struct parse* parse, const char* name ) {
   return ( p_find_macro( parse, name ) != NULL );
}

void find_elif( struct parse* parse ) {
   struct endif_search search;
   init_endif_search( &search, true );
   find_endif( parse, &search );
}

void init_endif_search( struct endif_search* search, bool execute_else ) {
   search->depth = 1;
   search->done = false;
   search->execute_else = execute_else;
}

void find_endif( struct parse* parse, struct endif_search* search ) {
   while ( ! search->done ) {
      if ( parse->token->type == TK_NL ) {
         p_read_preptk( parse );
         if ( parse->token->type == TK_HASH ) {
            struct pos pos = parse->token->pos;
            p_read_preptk( parse );
            if ( parse->token->type == TK_ID ) {
               read_search_dirc( parse, search, &pos );
            }
         }
      }
      else {
         p_read_preptk( parse );
         if ( parse->token->type == TK_END ) {
            p_confirm_ifdircs_closed( parse );
         }
      }
   }
}

void read_search_dirc( struct parse* parse, struct endif_search* search,
   struct pos* pos ) {
   if ( strcmp( parse->token->text, "ifdef" ) == 0 ||
      strcmp( parse->token->text, "ifndef" ) == 0 ||
      strcmp( parse->token->text, "if" ) == 0 ) {
      push_ifdirc( parse, parse->token->text, pos );
      ++search->depth;
   }
   else if ( strcmp( parse->token->text, "elif" ) == 0 ) {
      if ( search->depth == 1 ) {
         read_elif( parse, search, pos );
      }
   }
   else if ( strcmp( parse->token->text, "else" ) == 0 ) {
      if ( search->depth == 1 ) {
         read_else( parse, search, pos );
      }
   }
   else if ( strcmp( parse->token->text, "endif" ) == 0 ) {
      read_endif( parse, search, pos );
   }
}

void read_elif( struct parse* parse, struct endif_search* search,
   struct pos* pos ) {
   p_test_preptk( parse, TK_ID );
   int value = p_eval_prep_expr( parse );
   p_test_preptk( parse, TK_NL );
   if ( ! parse->ifdirc ) {
      p_diag( parse, DIAG_POS_ERR, pos,
         "#elif outside an if-directive" );
      p_bail( parse );
   }
   if ( parse->ifdirc->else_found ) {
      p_diag( parse, DIAG_POS_ERR, pos,
         "#elif found after #else" );
      p_diag( parse, DIAG_POS, &parse->ifdirc->else_pos,
         "#else found here" );
      p_bail( parse );
   }
   if ( search->execute_else ) {
      search->done = ( value != 0 );
   }
}

void read_else( struct parse* parse, struct endif_search* search,
   struct pos* pos ) {
   p_test_preptk( parse, TK_ID );
   p_read_preptk( parse );
   p_test_preptk( parse, TK_NL );
   if ( ! parse->ifdirc ) {
      p_diag( parse, DIAG_POS_ERR, pos,
         "#else used with no open if-directive" );
      p_bail( parse );
   }
   if ( parse->ifdirc->else_found ) {
      p_diag( parse, DIAG_POS_ERR, pos,
         "duplicate #else" );
      p_diag( parse, DIAG_POS, &parse->ifdirc->else_pos,
         "#else previously found here" );
      p_bail( parse );
   }
   parse->ifdirc->else_pos = *pos;
   parse->ifdirc->else_found = true;
   if ( search->execute_else ) {
      search->done = true;
   }
}

void read_endif( struct parse* parse, struct endif_search* search,
   struct pos* pos ) {
   p_test_preptk( parse, TK_ID );
   p_read_preptk( parse );
   p_test_preptk( parse, TK_NL );
   if ( ! pop_ifdirc( parse ) ) {
      p_diag( parse, DIAG_POS_ERR, pos,
         "#endif used with no open if-directive" );
      p_bail( parse );
   }
   --search->depth;
   if ( search->depth == 0 ) {
      search->done = true;
   }
}

void skip_section( struct parse* parse, struct pos* pos ) {
   struct endif_search search;
   init_endif_search( &search, false );
   read_search_dirc( parse, &search, pos );
   find_endif( parse, &search );
}

void p_confirm_ifdircs_closed( struct parse* parse ) {
   if ( parse->ifdirc ) {
      struct ifdirc* entry = parse->ifdirc;
      while ( entry ) {
         p_diag( parse, DIAG_POS_ERR, &entry->pos,
            "unterminated #%s", entry->name );
         entry = entry->prev;
      }
      p_bail( parse );
   }
}

void p_define_imported_macro( struct parse* parse ) {
   struct macro* macro = alloc_macro( parse );
   macro->name = "__IMPORTED__";
   macro->predef = PREDEFMACRO_IMPORTED;
   append_macro( parse, macro );
}

// The predefined __INCLUDED__ macro is present as long as an #included file is
// being processed. 
void p_define_included_macro( struct parse* parse ) {
   struct macro* macro = p_find_macro( parse, "__INCLUDED__" );
   if ( ! macro ) {
      macro = alloc_macro( parse );
      macro->name = "__INCLUDED__";
      macro->predef = PREDEFMACRO_INCLUDED;
      append_macro( parse, macro );
   }
}

void p_undefine_included_macro( struct parse* parse ) {
   struct macro* macro = remove_macro( parse, "__INCLUDED__" );
   if ( macro ) {
      free_macro( parse, macro );
   }
}

void p_define_predef_macros( struct parse* parse ) {
   // Macro: __LINE__
   struct macro* macro = alloc_macro( parse );
   macro->name = "__LINE__";
   macro->predef = PREDEFMACRO_LINE;
   append_macro( parse, macro );
   // Macro: __FILE__
   macro = alloc_macro( parse );
   macro->name = "__FILE__";
   macro->predef = PREDEFMACRO_FILE;
   append_macro( parse, macro );
   // Macro: __TIME__
   macro = alloc_macro( parse );
   macro->name = "__TIME__";
   macro->predef = PREDEFMACRO_TIME;
   append_macro( parse, macro );
   // Macro: __DATE__
   macro = alloc_macro( parse );
   macro->name = "__DATE__";
   macro->predef = PREDEFMACRO_DATE;
   append_macro( parse, macro );
}

void p_define_cmdline_macros( struct parse* parse ) {
   list_iter_t i;
   list_iter_init( &i, &parse->task->options->defines );
   while ( ! list_end( &i ) ) {
      const char* name = list_data( &i );
      struct macro* macro = p_find_macro( parse, name );
      if ( ! macro ) {
         struct token* token = p_alloc_token( parse );
         p_init_token( token );
         token->type = TK_LIT_DECIMAL;
         token->text = CMDLINEMACRO_TEXT,
         token->length = strlen( CMDLINEMACRO_TEXT );
         macro = alloc_macro( parse );
         macro->name = name;
         macro->pos.id = ALTERN_FILENAME_COMMANDLINE;
         append_token( macro, token );
         append_macro( parse, macro );
      }
      list_next( &i );
   }
}

// Reads #region/#endregion. These directives have no effect.
void read_region( struct parse* parse ) {
   p_test_preptk( parse, TK_ID );
   p_read_preptk( parse );
   p_test_preptk( parse, TK_NL );
}