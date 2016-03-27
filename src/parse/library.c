#include <string.h>

#include "phase.h"
#include "cache/cache.h"

static void read_namespace( struct parse* parse );
static void read_namespace_name_list( struct parse* parse );
static void read_namespace_name( struct parse* parse );
static void read_namespace_member_list( struct parse* parse );
static void read_namespace_member( struct parse* parse );
static struct using_dirc* alloc_using( struct pos* pos );
static void read_using_item_list( struct parse* parse,
   struct using_dirc* dirc );
static void read_using_item( struct parse* parse, struct using_dirc* dirc );
static struct using_item* alloc_using_item( void );
static struct path* alloc_path( struct pos pos );
static void read_dirc( struct parse* parse, struct pos* );
static void read_include( struct parse* parse, struct pos*, bool );
static void read_import( struct parse* parse, struct pos* pos );
static void load_imported_lib( struct parse* parse );
static struct library* get_previously_processed_lib( struct parse* parse,
   struct file_entry* file );
static struct library* find_lib( struct parse* parse,
   struct file_entry* file );
static void append_imported_lib( struct parse* parse, struct library* lib );
static void read_library( struct parse* parse, struct pos* );
static void read_library_name( struct parse* parse, struct pos* pos );
static void read_define( struct parse* parse );

void p_read_lib( struct parse* parse ) {
   while ( true ) {
      if ( p_is_dec( parse ) ) {
         struct dec dec;
         p_init_dec( &dec );
         dec.name_offset = parse->ns->body;
         p_read_dec( parse, &dec );
      }
      else if ( parse->tk == TK_SCRIPT ) {
         p_read_script( parse );
      }
      else if ( parse->tk == TK_NAMESPACE ) {
         read_namespace( parse );
      }
      else if ( parse->tk == TK_USING ) {
         p_read_using( parse, &parse->ns->usings );
      }
      else if ( parse->tk == TK_SEMICOLON ) {
         p_read_tk( parse );
      }
      else if ( parse->tk == TK_HASH ) {
         struct pos pos = parse->tk_pos;
         p_read_tk( parse );
         read_dirc( parse, &pos );
      }
      else if ( parse->tk == TK_SPECIAL ) {
         p_read_special_list( parse );
      }
      else if ( parse->tk == TK_END ) {
         break;
      }
      else {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "unexpected %s", p_get_token_name( parse->tk ) );
         p_bail( parse );
      }
   }
   // Library must have the #library directive.
   if ( parse->task->library->imported &&
      ! parse->task->library->header ) {
      p_diag( parse, DIAG_ERR | DIAG_FILE, &parse->tk_pos,
         "#imported file missing #library directive" );
      p_bail( parse );
   }
}

void read_namespace( struct parse* parse ) {
   struct pos pos = parse->tk_pos;
   p_test_tk( parse, TK_NAMESPACE );
   p_read_tk( parse );
   struct ns* parent = parse->ns;
   read_namespace_name_list( parse );
   if ( parse->tk == TK_BRACE_L ) {
      // A namespace can be opened only once. This behaves like modules in
      // other languages.
      if ( parse->ns->defined ) {
         p_diag( parse, DIAG_POS_ERR, &pos,
            "duplicate namespace" );
         p_diag( parse, DIAG_POS, &parse->ns->object.pos,
            "namespace already defined here" );
         p_bail( parse );
      }
      parse->ns->defined = true;
      parse->ns->object.pos = pos;
      p_read_tk( parse );
      read_namespace_member_list( parse );
      p_test_tk( parse, TK_BRACE_R );
      p_read_tk( parse );
   }
   parse->ns = parent;
}

void read_namespace_name_list( struct parse* parse ) {
   while ( true ) {
      read_namespace_name( parse );
      if ( parse->tk == TK_DOT ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
}

void read_namespace_name( struct parse* parse ) {
   p_test_tk( parse, TK_ID );
   struct name* name = t_extend_name( parse->ns->body, parse->tk_text );
   if ( ! name->object ) {
      struct ns* ns = t_alloc_ns( parse->task, name );
      ns->parent = parse->ns;
      name->object = &ns->object;
   }
   parse->ns = ( struct ns* ) name->object;
   p_read_tk( parse );
}

void read_namespace_member_list( struct parse* parse ) {
   while ( parse->tk != TK_BRACE_R ) {
      read_namespace_member( parse );
   }
}

void read_namespace_member( struct parse* parse ) {
   if ( p_is_dec( parse ) ) {
      struct dec dec;
      p_init_dec( &dec );
      dec.name_offset = parse->ns->body;
      p_read_dec( parse, &dec );
   }
   else if ( parse->tk == TK_SCRIPT ) {
      if ( ! parse->task->library->imported ) {
         p_read_script( parse );
      }
      else {
         p_skip_block( parse );
      }
   }
   else if ( parse->tk == TK_NAMESPACE ) {
      read_namespace( parse );
   }
   else if ( parse->tk == TK_USING ) {
      p_read_using( parse, &parse->ns->usings );
   }
   else if ( parse->tk == TK_SEMICOLON ) {
      p_read_tk( parse );
   }
   else {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "unexpected %s", p_get_token_name( parse->tk ) );
      p_bail( parse );
   }
}

void p_read_using( struct parse* parse, struct list* output ) {
   struct using_dirc* dirc = alloc_using( &parse->tk_pos );
   p_test_tk( parse, TK_USING );
   p_read_tk( parse );
   dirc->path = p_read_path( parse );
   if ( parse->tk == TK_COLON ) {
      p_read_tk( parse );
      read_using_item_list( parse, dirc );
      dirc->type = USING_SELECTION;
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   list_append( output, dirc );
}

struct using_dirc* alloc_using( struct pos* pos ) {
   struct using_dirc* dirc = mem_alloc( sizeof( *dirc ) );
   dirc->node.type = NODE_USING;
   dirc->path = NULL;
   list_init( &dirc->items );
   dirc->pos = *pos;
   dirc->type = USING_ALL;
   return dirc;
}

void read_using_item_list( struct parse* parse, struct using_dirc* dirc ) {
   while ( true ) {
      read_using_item( parse, dirc );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
}

void read_using_item( struct parse* parse, struct using_dirc* dirc ) {
   p_test_tk( parse, TK_ID );
   struct using_item* item = alloc_using_item();
   item->name = parse->tk_text;
   item->name_pos = parse->tk_pos;
   p_read_tk( parse );
   if ( parse->tk == TK_ASSIGN ) {
      p_read_tk( parse );
      p_test_tk( parse, TK_ID );
      item->alias = item->name;
      item->alias_pos = item->name_pos;
      item->name = parse->tk_text;
      item->name_pos = parse->tk_pos;
      p_read_tk( parse );
   }
   list_append( &dirc->items, item );
}

struct using_item* alloc_using_item( void ) {
   struct using_item* item = mem_alloc( sizeof( *item ) );
   item->name = NULL;
   item->alias = NULL;
   return item;
}

struct path* p_read_path( struct parse* parse ) {
   // Head of path.
   struct path* path = alloc_path( parse->tk_pos );
   if ( parse->tk == TK_UPMOST ) {
      path->upmost = true;
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_ID );
      path->text = parse->tk_text;
      p_read_tk( parse );
   }
   // Tail of path.
   struct path* head = path;
   struct path* tail = head;
   while ( parse->tk == TK_DOT && p_peek( parse ) == TK_ID ) {
      p_read_tk( parse );
      p_test_tk( parse, TK_ID );
      path = alloc_path( parse->tk_pos );
      path->text = parse->tk_text;
      tail->next = path;
      tail = path;
      p_read_tk( parse );
   }
   return head;
}

struct path* alloc_path( struct pos pos ) {
   struct path* path = mem_alloc( sizeof( *path ) );
   path->next = NULL;
   path->text = NULL;
   path->pos = pos;
   path->upmost = false;
   return path;
}

bool p_peek_path( struct parse* parse, struct parsertk_iter* iter ) {
/*
   switch ( iter->token->type ) {
   case TK_UPMOST:
   case TK_ID:
      p_next_tk( parse, iter );
      break;
   default:
      return false;
   }
*/
   while ( iter->token->type == TK_DOT ) {
      p_next_tk( parse, iter );
      if ( iter->token->type != TK_ID ) {
         return false;
      }
      p_next_tk( parse, iter );
   }
   return true;
}

void read_dirc( struct parse* parse, struct pos* pos ) {
   if ( parse->tk == TK_IMPORT ) {
      p_read_tk( parse );
      read_import( parse, pos );
   }
   else if ( strcmp( parse->tk_text, "include" ) == 0 ) {
      p_read_tk( parse );
      read_include( parse, pos, false );
   }
   else if ( strcmp( parse->tk_text, "define" ) == 0 ||
      strcmp( parse->tk_text, "libdefine" ) == 0 ) {
      read_define( parse );
   }
   else if ( strcmp( parse->tk_text, "library" ) == 0 ) {
      p_read_tk( parse );
      read_library( parse, pos );
   }
   else if ( strcmp( parse->tk_text, "encryptstrings" ) == 0 ) {
      parse->task->library->encrypt_str = true;
      p_read_tk( parse );
   }
   else if ( strcmp( parse->tk_text, "nocompact" ) == 0 ) {
      parse->task->library->format = FORMAT_BIG_E;
      p_read_tk( parse );
   }
   else if ( strcmp( parse->tk_text, "mnemonic" ) == 0 ) {
      p_read_tk( parse );
      p_read_mnemonic( parse );
   }
   else if (
      // NOTE: Not sure what these two are.
      strcmp( parse->tk_text, "wadauthor" ) == 0 ||
      strcmp( parse->tk_text, "nowadauthor" ) == 0 ) {
      p_diag( parse, DIAG_POS_ERR, pos, "directive `%s` not supported",
         parse->tk_text );
      p_bail( parse );
   }
   else {
      p_diag( parse, DIAG_POS_ERR, pos,
         "unknown directive '%s'", parse->tk_text );
      p_bail( parse );
   }
}

void read_include( struct parse* parse, struct pos* pos, bool import ) {
   p_test_tk( parse, TK_LIT_STRING );
   p_load_included_source( parse );
   p_read_tk( parse );
}

void read_import( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_LIT_STRING );
   load_imported_lib( parse );
   p_read_tk( parse );
}

#include "semantic/phase.h"

void load_imported_lib( struct parse* parse ) {
   struct file_query query;
   t_init_file_query( &query, parse->source->file, parse->tk_text );
   t_find_file( parse->task, &query );
   if ( ! query.file ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "library not found: %s", parse->tk_text );
      p_bail( parse );
   }
   struct library* lib = get_previously_processed_lib( parse, query.file );
   if ( ! lib ) {
      struct source* source = p_load_included_source( parse );
      source->imported = true;
      struct library* parent_lib = parse->task->library;
      lib = t_add_library( parse->task );
      parse->task->library = lib;
      parse->task->library->imported = true;
      p_read_tk( parse );
      p_read_lib( parse );
      struct semantic semantic;
      s_init( &semantic, parse->task, lib );
      s_test( &semantic );
      parse->task->library = parent_lib;
      lib->file = query.file;
      if ( ! parse->task->options->ignore_cache ) {
         cache_add( parse->cache, lib );
      }
   }
   append_imported_lib( parse, lib );
}

struct library* get_previously_processed_lib( struct parse* parse,
   struct file_entry* file ) {
   struct library* lib = find_lib( parse, file );
   if ( ! lib && ! parse->task->options->ignore_cache ) {
      lib = cache_get( parse->cache, file );
   }
   return lib;
}

struct library* find_lib( struct parse* parse, struct file_entry* file ) {
   list_iter_t i;
   list_iter_init( &i, &parse->task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib->file == file ) {
         return lib;
      }
      list_next( &i );
   }
   return NULL;
}

void append_imported_lib( struct parse* parse, struct library* lib ) {
   list_iter_t i;
   list_iter_init( &i, &parse->task->library->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* imported_lib = list_data( &i );
      if ( lib == imported_lib ) {
         p_diag( parse, DIAG_WARN | DIAG_POS, &parse->tk_pos,
            "duplicate import of library" );
         return;
      }
      list_next( &i );
   }
   list_append( &parse->task->library->dynamic, lib );
}

void read_library( struct parse* parse, struct pos* pos ) {
   parse->task->library->header = true;
   if ( parse->tk == TK_LIT_STRING ) {
      read_library_name( parse, pos );
   }
   else {
      parse->task->library->compiletime = true;
   }
}

void read_library_name( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_LIT_STRING );
   if ( ! parse->tk_length ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "library name is blank" );
      p_bail( parse );
   }
   int length = parse->tk_length;
   if ( length > MAX_LIB_NAME_LENGTH ) {
      p_diag( parse, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &parse->tk_pos, "library name too long" );
      p_diag( parse, DIAG_FILE, &parse->tk_pos,
         "library name can be up to %d characters long",
         MAX_LIB_NAME_LENGTH );
      length = MAX_LIB_NAME_LENGTH;
   }
   char name[ MAX_LIB_NAME_LENGTH + 1 ];
   memcpy( name, parse->tk_text, length );
   name[ length ] = 0;
   p_read_tk( parse );
   // Different #library directives in the same library must have the same
   // name.
   if ( parse->task->library->name.length &&
      strcmp( parse->task->library->name.value, name ) != 0 ) {
      p_diag( parse, DIAG_POS_ERR, pos, "library has multiple names" );
      p_diag( parse, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &parse->task->library->name_pos, "first name given here" );
      p_bail( parse );
   }
   // Each library must have a unique name.
   list_iter_t i;
   list_iter_init( &i, &parse->task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib != parse->task->library && strcmp( name, lib->name.value ) == 0 ) {
         p_diag( parse, DIAG_POS_ERR, pos,
            "duplicate library name" );
         p_diag( parse, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &lib->name_pos,
            "library name previously found here" );
         p_bail( parse );
      }
      list_next( &i );
   }
   str_copy( &parse->task->library->name, name, length );
   parse->task->library->importable = true;
   parse->task->library->name_pos = *pos;
}

void read_define( struct parse* parse ) {
   bool hidden = false;
   if ( parse->tk_text[ 0 ] == 'd' && parse->task->library->imported ) {
      hidden = true;
   }
   p_read_tk( parse );
   p_test_tk( parse, TK_ID );
   struct constant* constant = mem_alloc( sizeof( *constant ) );
   t_init_object( &constant->object, NODE_CONSTANT );
   constant->object.pos = parse->tk_pos;
   if ( hidden ) {
      constant->name = t_extend_name( parse->task->library->hidden_names,
         parse->tk_text );
   }
   else {
      constant->name = t_extend_name( parse->ns->body, parse->tk_text );
   }
   p_read_tk( parse );
   struct expr_reading value;
   p_init_expr_reading( &value, true, false, false, true );
   p_read_expr( parse, &value );
   constant->value_node = value.output_node;
   constant->value = 0;
   constant->hidden = hidden;
   constant->lib_id = parse->task->library->id;
   p_add_unresolved( parse, &constant->object );
}

void p_add_unresolved( struct parse* parse, struct object* object ) {
   t_append_unresolved_namespace_object( parse->ns, object );
}