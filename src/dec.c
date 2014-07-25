#include <stdio.h>
#include <string.h>

#include "task.h"

#define MAX_WORLD_LOCATIONS 256
#define MAX_GLOBAL_LOCATIONS 64
#define SCRIPT_MIN_NUM 0
#define SCRIPT_MAX_NUM 999
#define SCRIPT_MAX_PARAMS 3
#define SWEEP_MAX_SIZE 20

struct sweep {
   struct sweep* prev;
   struct name* names[ SWEEP_MAX_SIZE ];
   int size;
};

struct scope {
   struct scope* prev;
   struct sweep* sweep;
   struct region_link* region_link;
   struct import* imports;
};

struct params {
   struct param* node;
   struct pos format_pos;
   int min;
   int max;
   bool script;
   bool format;
};

struct multi_value_read {
   struct multi_value* multi_value;
   struct initial* tail;
};

struct multi_value_test {
   struct multi_value_test* parent;
   struct multi_value* multi_value;
   struct dim* dim;
   struct type* type;
   struct type_member* type_member;
   int count;
   bool undef_err;
   bool undef_erred;
   bool has_string;
};

struct value_list {
   struct value* head;
   struct value* tail;
};

struct value_index_alloc {
   struct value* value;
   int index;
};

enum {
   TYPE_INT,
   TYPE_STR,
   TYPE_BOOL,
   TYPE_VOID
};

struct script_read {
   struct pos pos;
   struct pos param_pos;
   struct list labels;
   struct expr* number;
   struct param* params;
   struct node* body;
   int type;
   int flags;
   int num_param;
};

static struct name* new_name( void );
static void init_type( struct task* );
static struct type* new_type( struct task*, struct name* );
static void init_type_members( struct task* );
static struct type* get_type( struct task*, int );
static struct library* add_library( struct task* );
static struct region* alloc_region( struct task*, struct name*, bool );
static void read_region_body( struct task* );
static void init_params( struct params* );
static void read_params( struct task*, struct params* );
static void read_qual( struct task*, struct dec* );
static void read_storage( struct task*, struct dec* );
static void read_type( struct task*, struct dec* );
static void read_enum( struct task*, struct dec* );
static struct constant* alloc_constant( void );
static void read_struct( struct task*, struct dec* );
static void read_storage_index( struct task*, struct dec* );
static void add_unresolved( struct region*, struct object* );
static void test_script( struct task*, struct script* );
static void read_func( struct task*, struct dec* );
static void read_bfunc( struct task*, struct func* );
static void read_script_number( struct task*, struct script_read* );
static void read_script_param( struct task*, struct script_read* );
static void read_script_type( struct task*, struct script_read* );
static void read_script_flag( struct task*, struct script_read* );
static void read_script_body( struct task*, struct script_read* );
static void read_name( struct task*, struct dec* );
static void read_dim( struct task*, struct dec* );
static void read_init( struct task*, struct dec* );
static void init_initial( struct initial*, bool );
static void read_multi_init( struct task*, struct dec*,
   struct multi_value_read* );
static void add_struct_member( struct task*, struct dec* );
static void add_var( struct task*, struct dec* );
static void bind_region( struct task*, struct region* );
static void bind_object( struct task*, struct name*, struct object* );
static void alias_imported( struct task*, char*, struct pos*, struct object* );
static void test_region( struct task*, int*, bool*, bool* );
static void test_type_member( struct task*, struct type_member*, bool );
static struct type* find_type( struct task*, struct path* );
static struct object* find_type_head( struct task*, struct path* );
static void test_var( struct task*, struct var*, bool );
static void test_init( struct task*, struct var*, bool, bool* );
static void init_multi_value_test( struct multi_value_test*,
   struct multi_value_test*, struct multi_value*, struct dim*, struct type*,
   struct type_member*, bool );
static void test_multi_value( struct task*, struct multi_value_test* );
static void test_multi_value_struct( struct task*, struct multi_value_test* );
static void calc_dim_size( struct dim*, struct type* );
static void test_func( struct task*, struct func*, bool );
static void test_func_body( struct task*, struct func* );
static void calc_type_size( struct type* );
static void calc_map_value_index( struct task* );
static void calc_var_value_index( struct var* );
static void make_value_list( struct value_list*, struct multi_value* );
static void alloc_value_index( struct value_index_alloc*, struct multi_value*,
   struct type*, struct dim* );
static void alloc_value_index_struct( struct value_index_alloc*,
   struct multi_value*, struct type* );
static void count_string_usage( struct task* );
static void count_string_usage_node( struct node* );
static void count_string_usage_initial( struct initial* );
static void calc_map_var_size( struct task* );
static void calc_var_size( struct var* );
static void diag_dup_struct_member( struct task*, struct name*, struct pos* );
static void init_object( struct object*, int );
static void read_dirc( struct task*, struct pos* );
static void read_include( struct task*, struct pos*, bool );
static void read_library( struct task*, struct pos* );
static void read_define( struct task* );

void t_init_fields_dec( struct task* task ) {
   task->depth = 0;
   task->scope = NULL;
   task->free_scope = NULL;
   task->free_sweep = NULL;
   task->root_name = new_name();
   task->anon_name = t_make_name( task, "!anon.", task->root_name );
   list_init( &task->regions );
   struct region* region = alloc_region( task, task->root_name, true );
   task->root_name->object = &region->object;
   task->region = region;
   task->region_upmost = region;
   list_append( &task->regions, region );
   init_type( task );
   init_type_members( task );
   task->in_func = false;
}

struct name* new_name( void ) {
   struct name* name = mem_slot_alloc( sizeof( *name ) );
   name->parent = NULL;
   name->next = NULL;
   name->drop = NULL;
   name->object = NULL;
   name->ch = 0;
   return name;
}

struct name* t_make_name( struct task* task, const char* ch,
   struct name* parent ) {
   struct name* name = parent->drop;
   while ( *ch ) {
      // Find the correct node to enter.
      struct name* prev = NULL;
      while ( name && name->ch < *ch ) {
         prev = name;
         name = name->next;
      }
      // Enter a new node if no node could be found.
      if ( ! name ) {
         name = new_name();
         name->parent = parent;
         name->ch = *ch;
         if ( prev ) {
            prev->next = name;
         }
         else {
            parent->drop = name;
         }
      }
      // Enter a new node if no node with the same character exists in the
      // parent node.
      else if ( name->ch != *ch ) {
         struct name* smaller_name = new_name();
         smaller_name->next = name;
         smaller_name->parent = parent;
         smaller_name->ch = *ch;
         name = smaller_name;
         if ( prev ) {
            prev->next = name;
         }
         else {
            parent->drop = name;
         }
      }
      parent = name;
      name = name->drop;
      ++ch;
   }
   return parent;
}

void t_copy_name( struct name* start, bool full, struct str* str ) {
   int length = 0;
   char term = ':';
   if ( full ) {
      term = 0;
   }
   struct name* name = start;
   while ( name->ch && name->ch != term ) {
      name = name->parent;
      ++length;
   }
   if ( str->buffer_length < length + 1 ) {
      str_grow( str, length + 1 );
   }
   str->length = length;
   str->value[ length ] = 0;
   name = start;
   while ( length ) {
      --length;
      str->value[ length ] = name->ch;
      name = name->parent;
   }
}

int t_full_name_length( struct name* name ) {
   int length = 0;
   while ( name->ch ) {
      name = name->parent;
      ++length;
   }
   return length;
}

void init_type( struct task* task ) {
   struct type* type = new_type( task,
      t_make_name( task, "int", task->region_upmost->body ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_int = type;
   type = new_type( task,
      t_make_name( task, "str", task->region_upmost->body ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   type->is_str = true;
   task->type_str = type;
   type = new_type( task,
      t_make_name( task, "bool", task->region_upmost->body ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_bool = type;
}

struct type* new_type( struct task* task, struct name* name ) {
   struct type* type = mem_alloc( sizeof( *type ) );
   init_object( &type->object, NODE_TYPE );
   type->name = name;
   type->body = t_make_name( task, "::", name );
   type->member = NULL;
   type->member_tail = NULL;
   type->size = 0;
   type->primitive = false;
   type->is_str = false;
   type->anon = false;
   return type;
}

void init_type_members( struct task* task ) {
   static struct {
      const char* name;
      int param;
      int type;
      int value;
      int id;
   } list[] = {
      { "length", 0, TYPE_STR, TYPE_INT, INTERN_FUNC_STR_LENGTH },
      { "at", 1, TYPE_STR, TYPE_INT, INTERN_FUNC_STR_AT },
   };
   for ( size_t i = 0; i < ARRAY_SIZE( list ); ++i ) {
      struct func* func = mem_slot_alloc( sizeof( *func ) );
      init_object( &func->object, NODE_FUNC );
      func->object.resolved = true;
      func->type = FUNC_INTERNAL;
      struct type* type = get_type( task, list[ i ].type );
      struct name* name = t_make_name( task, list[ i ].name, type->body );
      name->object = &func->object;
      func->name = name;
      func->params = NULL;
      func->value = get_type( task, list[ i ].value );
      struct func_intern* impl = mem_alloc( sizeof( *impl ) );
      impl->id = list[ i ].id;
      func->impl = impl;
      func->min_param = list[ i ].param; 
      func->max_param = list[ i ].param;
      func->hidden = false;
   }
}

struct type* get_type( struct task* task, int id ) {
   switch ( id ) {
   case TYPE_INT:
      return task->type_int;
   case TYPE_STR:
      return task->type_str;
   case TYPE_BOOL:
      return task->type_bool;
   default:
      return NULL;
   }
}

void t_make_main_lib( struct task* task ) {
   task->library = add_library( task );
   task->library_main = task->library;
}

struct library* add_library( struct task* task ) {
   struct library* lib = mem_alloc( sizeof( *lib ) );
   str_init( &lib->name );
   str_copy( &lib->name, "", 0 );
   list_init( &lib->vars );
   list_init( &lib->funcs );
   list_init( &lib->scripts );
   list_init( &lib->dynamic );
   lib->file_pos.line = 0;
   lib->file_pos.column = 0;
   lib->file_pos.id = 0;
   lib->id = list_size( &task->libraries );
   lib->format = FORMAT_LITTLE_E;
   lib->importable = false;
   lib->imported = false;
   if ( task->library_main ) {
      struct library* last_lib = list_tail( &task->libraries );
      lib->hidden_names = t_make_name( task, "a.", last_lib->hidden_names );
   }
   else {
      lib->hidden_names = t_make_name( task, "!hidden.", task->root_name );
   }
   lib->encrypt_str = false;
   list_append( &task->libraries, lib );
   return lib;
}

void t_read_lib( struct task* task ) {
   while ( true ) {
      if ( t_is_dec( task ) ) {
         struct dec dec;
         t_init_dec( &dec );
         dec.name_offset = task->region_upmost->body;
         t_read_dec( task, &dec );
      }
      else if ( task->tk == TK_SCRIPT ) {
         if ( task->library->imported ) {
            t_skip_block( task );
         }
         else {
            t_read_script( task );
         }
      }
      else if ( task->tk == TK_REGION ) {
         t_read_region( task );
      }
      else if ( task->tk == TK_IMPORT ) {
         t_read_import( task, NULL );
      }
      else if ( task->tk == TK_SEMICOLON ) {
         t_read_tk( task );
      }
      else if ( task->tk == TK_HASH ) {
         struct pos pos = task->tk_pos;
         t_read_tk( task );
         read_dirc( task, &pos );
      }
      else if ( task->tk == TK_END ) {
         break;
      }
      else {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "unexpected %s", t_get_token_name( task->tk ) );
         t_bail( task );
      }
   }
   // Library must have the #library directive.
   if ( task->library->imported && ! task->library->name.length ) {
      t_diag( task, DIAG_ERR | DIAG_FILE, &task->tk_pos,
         "#imported file missing #library directive" );
      t_bail( task );
   }
}

void t_read_region( struct task* task ) {
   t_test_tk( task, TK_REGION );
   t_read_tk( task );
   struct region* parent = task->region;
   while ( true ) {
      t_test_tk( task, TK_ID );
      struct name* name = t_make_name( task, task->tk_text,
         task->region->body );
      if ( name->object ) {
         // It is assumed the object will always be a region.
         task->region = ( struct region* ) name->object;
      }
      else {
         struct region* region = alloc_region( task, name, false );
         region->object.pos = task->tk_pos;
         region->object.resolved = true;
         name->object = &region->object;
         list_append( &task->regions, region );
         task->region = region;
      }
      t_read_tk( task );
      if ( task->tk == TK_COLON_2 ) {
         t_read_tk( task );
      }
      else {
         break;
      }
   }
   t_test_tk( task, TK_BRACE_L );
   t_read_tk( task );
   read_region_body( task );
   t_test_tk( task, TK_BRACE_R );
   t_read_tk( task );
   task->region = parent;
}

struct region* alloc_region( struct task* task, struct name* name,
   bool upmost ) {
   struct region* region = mem_alloc( sizeof( *region ) );
   init_object( &region->object, NODE_REGION );
   region->name = name;
   if ( upmost ) {
      region->body = name;
      region->body_struct = t_make_name( task, "struct::", name );
   }
   else {
      region->body = t_make_name( task, "::", name );
      region->body_struct = t_make_name( task, "::struct::", name );
   }
   region->link = NULL;
   region->unresolved = NULL;
   region->unresolved_tail = NULL;
   list_init( &region->imports );
   list_init( &region->items );
   return region;
}

void read_region_body( struct task* task ) {
   while ( true ) {
      if ( t_is_dec( task ) ) {
         struct dec dec;
         t_init_dec( &dec );
         dec.name_offset = task->region->body;
         t_read_dec( task, &dec );
      }
      else if ( task->tk == TK_SCRIPT ) {
         if ( ! task->library->imported ) {
            t_read_script( task );
         }
         else {
            t_skip_block( task );
         }
      }
      else if ( task->tk == TK_REGION ) {
         t_read_region( task );
      }
      else if ( task->tk == TK_IMPORT ) {
         t_read_import( task, NULL );
      }
      else if ( task->tk == TK_SEMICOLON ) {
         t_read_tk( task );
      }
      else if ( task->tk == TK_BRACE_R ) {
         break;
      }
      else {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "unexpected %s", t_get_token_name( task->tk ) );
         t_bail( task );
      }
   }
}

void read_dirc( struct task* task, struct pos* pos ) {
   // Directives can only appear in the upmost region.
   if ( task->region != task->region_upmost ) {
      t_diag( task, DIAG_POS_ERR, pos, "directive not in upmost region" );
      t_bail( task );
   }
   if ( task->tk == TK_IMPORT ) {
      t_read_tk( task );
      if ( task->source->imported ) {
         t_test_tk( task, TK_LIT_STRING );
         t_read_tk( task );
      }
      else {
         read_include( task, pos, true );
      }
   }
   else if ( strcmp( task->tk_text, "include" ) == 0 ) {
      t_read_tk( task );
      if ( task->source->imported ) {
         t_test_tk( task, TK_LIT_STRING );
         t_read_tk( task );
      }
      else {
         read_include( task, pos, false );
      }
   }
   else if ( strcmp( task->tk_text, "define" ) == 0 ||
      strcmp( task->tk_text, "libdefine" ) == 0 ) {
      read_define( task );
   }
   else if ( strcmp( task->tk_text, "library" ) == 0 ) {
      t_read_tk( task );
      read_library( task, pos );
   }
   else if ( strcmp( task->tk_text, "encryptstrings" ) == 0 ) {
      task->library->encrypt_str = true;
      t_read_tk( task );
   }
   else if ( strcmp( task->tk_text, "nocompact" ) == 0 ) {
      task->library->format = FORMAT_BIG_E;
      t_read_tk( task );
   }
   else if (
      // NOTE: Not sure what these two are.
      strcmp( task->tk_text, "wadauthor" ) == 0 ||
      strcmp( task->tk_text, "nowadauthor" ) == 0 ) {
      t_diag( task, DIAG_POS_ERR, pos, "directive `%s` not supported",
         task->tk_text );
      t_bail( task );
   }
   else {
      t_diag( task, DIAG_POS_ERR, pos,
         "unknown directive '%s'", task->tk_text );
      t_bail( task );
   }
}

void read_include( struct task* task, struct pos* pos, bool import ) {
   t_test_tk( task, TK_LIT_STRING );
   struct source* source = t_load_included_source( task );
   t_read_tk( task );
   if ( import ) {
      source->imported = true;
      struct library* parent_lib = task->library;
      struct library* lib = add_library( task );
      list_append( &parent_lib->dynamic, lib );
      task->library = lib;
      task->library->imported = true;
      t_read_lib( task );
      t_read_tk( task );
      task->library = parent_lib;
   }
}

void read_library( struct task* task, struct pos* pos ) {
   t_test_tk( task, TK_LIT_STRING );
   if ( ! task->tk_length ) {
      t_diag( task, DIAG_POS_ERR, &task->tk_pos,
         "library name is blank" );
      t_bail( task );
   }
   int length = task->tk_length;
   if ( length > MAX_LIB_NAME_LENGTH ) {
      t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &task->tk_pos, "library name too long" );
      t_diag( task, DIAG_FILE, &task->tk_pos,
         "library name can be up to %d characters long",
         MAX_LIB_NAME_LENGTH );
      length = MAX_LIB_NAME_LENGTH;
   }
   char name[ MAX_LIB_NAME_LENGTH + 1 ];
   memcpy( name, task->tk_text, length );
   name[ length ] = 0;
   t_read_tk( task );
   // Different #library directives in the same library must have the same
   // name.
   if ( task->library->name.length &&
      strcmp( task->library->name.value, name ) != 0 ) {
      t_diag( task, DIAG_POS_ERR, pos, "library has multiple names" );
      t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &task->library->name_pos, "first name given here" );
      t_bail( task );
   }
   // Each library must have a unique name.
   list_iter_t i;
   list_iter_init( &i, &task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib != task->library && strcmp( name, lib->name.value ) == 0 ) {
         t_diag( task, DIAG_POS_ERR, pos,
            "duplicate library name" );
         t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &lib->name_pos,
            "library name previously found here" );
         t_bail( task );
      }
      list_next( &i );
   }
   str_copy( &task->library->name, name, length );
   task->library->importable = true;
   task->library->name_pos = *pos;
}

void read_define( struct task* task ) {
   bool hidden = false;
   if ( task->tk_text[ 0 ] == 'd' && task->library->imported ) {
      hidden = true;
   }
   t_read_tk( task );
   t_test_tk( task, TK_ID );
   struct constant* constant = mem_alloc( sizeof( *constant ) );
   init_object( &constant->object, NODE_CONSTANT );
   constant->object.pos = task->tk_pos;
   if ( hidden ) {
      constant->name = t_make_name( task, task->tk_text,
         task->library->hidden_names );
   }
   else {
      constant->name = t_make_name( task, task->tk_text,
         task->region_upmost->body );
   }
   t_read_tk( task );
   struct read_expr expr;
   t_init_read_expr( &expr );
   expr.in_constant = true;
   t_read_expr( task, &expr );
   constant->expr = expr.node;
   constant->value = 0;
   constant->hidden = hidden;
   constant->lib_id = task->library->id;
   add_unresolved( task->region, &constant->object );
}

void init_object( struct object* object, int node_type ) {
   object->node.type = node_type;
   object->resolved = false;
   object->depth = 0;
   object->next = NULL;
   object->next_scope = NULL;
}

void add_unresolved( struct region* region, struct object* object ) {
   if ( region->unresolved ) {
      region->unresolved_tail->next = object;
   }
   else {
      region->unresolved = object;
   }
   region->unresolved_tail = object;
}

void t_read_import( struct task* task, struct list* local ) {
   t_test_tk( task, TK_IMPORT );
   struct import* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_IMPORT;
   stmt->pos = task->tk_pos;
   stmt->path = NULL;
   stmt->item = NULL;
   stmt->next = NULL;
   t_read_tk( task );
   stmt->path = t_read_path( task );
   t_test_tk( task, TK_COLON );
   t_read_tk( task );
   struct import_item* tail = NULL;
   while ( true ) {
      struct import_item* item = mem_alloc( sizeof( *item ) );
      item->pos = task->tk_pos;
      item->next = NULL;
      item->name = NULL;
      item->alias = NULL;
      item->is_struct = false;
      item->is_link = false;
      // Link with another region.
      if ( task->tk == TK_REGION ) {
         t_read_tk( task );
         t_test_tk( task, TK_ASSIGN );
         t_read_tk( task );
         item->is_link = true;
         // Link with child region of selected region.
         if ( task->tk == TK_ID ) {
            item->name = task->tk_text;
            item->name_pos = task->tk_pos;
            t_read_tk( task );
         }
         // Link with selected region.
         else {
            t_test_tk( task, TK_REGION );
            t_read_tk( task );
         }
      }
      else {
         // Import structure.
         if ( task->tk == TK_STRUCT ) {
            item->is_struct = true;
            t_read_tk( task );
            t_test_tk( task, TK_ID );
            item->name = task->tk_text;
            item->name_pos = task->tk_pos;
            t_read_tk( task );
         }
         // Import object.
         else {
            t_test_tk( task, TK_ID );
            item->name = task->tk_text;
            item->name_pos = task->tk_pos;
            t_read_tk( task );
         }
         // Alias for imported object.
         if ( task->tk == TK_ASSIGN ) {
            item->alias = item->name;
            item->alias_pos = item->name_pos;
            t_read_tk( task );
            // Alias to the selected region. Only do this if "struct" was not
            // specified.
            if ( task->tk == TK_REGION && ! item->is_struct ) {
               item->name = NULL;
               t_read_tk( task );
            }
            else {
               t_test_tk( task, TK_ID );
               item->name = task->tk_text;
               item->name_pos = task->tk_pos;
               t_read_tk( task );
            }
         }
      }
      if ( tail ) {
         tail->next = item;
      }
      else {
         stmt->item = item;
      }
      tail = item;
      if ( task->tk == TK_COMMA ) {
         t_read_tk( task );
      }
      else {
         break;
      }
   }
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
   if ( local ) {
      list_append( local, stmt );
   }
   else {
      list_append( &task->region->imports, stmt );
   }
}

bool t_is_dec( struct task* task ) {
   switch ( task->tk ) {
   case TK_INT:
   case TK_STR:
   case TK_BOOL:
   case TK_VOID:
   case TK_WORLD:
   case TK_GLOBAL:
   case TK_STATIC:
   case TK_ENUM:
   case TK_STRUCT:
   case TK_FUNCTION:
      return true;
   default:
      return false;
   }
}

void t_init_dec( struct dec* dec ) {
   dec->area = DEC_TOP;
   dec->storage_name = "local";
   dec->type = NULL;
   dec->type_make = NULL;
   dec->type_path = NULL;
   dec->name = NULL;
   dec->name_offset = NULL;
   dec->dim = NULL;
   dec->initial = NULL;
   dec->stmt_read = NULL;
   dec->vars = NULL;
   dec->storage = STORAGE_LOCAL;
   dec->storage_index = 0;
   dec->type_needed = false;
   dec->type_void = false;
   dec->type_struct = false;
   dec->initz_str = false;
   dec->is_static = false;
   dec->leave = false;
}

void t_read_dec( struct task* task, struct dec* dec ) {
   dec->pos = task->tk_pos;
   bool func = false;
   if ( task->tk == TK_FUNCTION ) {
      func = true;
      dec->type_needed = true;
      t_read_tk( task );
   }
   read_qual( task, dec );
   read_storage( task, dec );
   read_type( task, dec );
   // No need to continue when only declaring a struct or an enum.
   if ( dec->leave ) {
      goto done;
   }
   bool var = false;
   read_obj:
   read_storage_index( task, dec );
   read_name( task, dec );
   if ( ! var ) {
      // Function:
      if ( func || task->tk == TK_PAREN_L ) {
         read_func( task, dec );
         goto done;
      }
      else {
         var = true;
         // Variable must have a type.
         if ( dec->type_void ) {
            const char* object = "variable";
            if ( dec->area == DEC_MEMBER ) {
               object = "struct member";
            }
            t_diag( task, DIAG_POS_ERR, &dec->type_pos,
               "void type specified for %s", object );
            t_bail( task );
         }
      }
   }
   read_dim( task, dec );
   // Cannot place multi-value variable in scalar portion of storage, where a
   // slot can hold only a single value.
   // TODO: Come up with syntax to navigate the array portion of storage, the
   // struct being the map. This way, you can access the storage data using a
   // struct member instead of an array index.
   if ( dec->type_struct && ! dec->dim && ( dec->storage == STORAGE_WORLD ||
      dec->storage == STORAGE_GLOBAL ) ) {
      t_diag( task, DIAG_POS_ERR, &dec->name_pos,
         "variable of struct type in scalar portion of storage" );
      t_bail( task );
   }
   read_init( task, dec );
   if ( dec->area == DEC_MEMBER ) {
      add_struct_member( task, dec );
   }
   else {
      add_var( task, dec );
   }
   if ( task->tk == TK_COMMA ) {
      t_read_tk( task );
      goto read_obj;
   }
   // Finish:
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
   done: ;
}

void read_qual( struct task* task, struct dec* dec ) {
   if ( task->tk == TK_STATIC ) {
      STATIC_ASSERT( DEC_TOTAL == 4 )
      if ( dec->area == DEC_TOP ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "`static` in region scope" );
         t_bail( task );
      }
      if ( dec->area == DEC_FOR ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "`static` in for-loop initialization" );
         t_bail( task );
      }
      if ( dec->area == DEC_MEMBER ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos, "static struct member" );
         t_bail( task );
      }
      dec->is_static = true;
      dec->type_needed = true;
      t_read_tk( task );
   }
}

void read_storage( struct task* task, struct dec* dec ) {
   bool given = false;
   if ( task->tk == TK_GLOBAL ) {
      dec->storage = STORAGE_GLOBAL;
      dec->storage_pos = task->tk_pos;
      dec->storage_name = "global";
      dec->type_needed = true;
      t_read_tk( task );
      given = true;
   }
   else if ( task->tk == TK_WORLD ) {
      dec->storage = STORAGE_WORLD;
      dec->storage_pos = task->tk_pos;
      dec->storage_name = "world";
      dec->type_needed = true;
      t_read_tk( task );
      given = true;
   }
   else {
      // Variable found at region scope, or a static local variable, has map
      // storage.
      if ( dec->area == DEC_TOP ||
         ( dec->area == DEC_LOCAL && dec->is_static ) ) {
         dec->storage = STORAGE_MAP;
         dec->storage_name = "map";
      }
   }
   // Storage cannot be specified for a structure member.
   if ( given && dec->area == DEC_MEMBER ) {
      t_diag( task, DIAG_POS_ERR, &dec->storage_pos,
         "storage specified for struct member" );
      t_bail( task );
   }
}

void read_type( struct task* task, struct dec* dec ) {
   dec->type_pos = task->tk_pos;
   if ( task->tk == TK_INT ) {
      dec->type = task->type_int;
      t_read_tk( task );
   }
   else if ( task->tk == TK_STR ) {
      dec->type = task->type_str;
      t_read_tk( task );
   }
   else if ( task->tk == TK_BOOL ) {
      dec->type = task->type_bool;
      t_read_tk( task );
   }
   else if ( task->tk == TK_VOID ) {
      dec->type_void = true;
      t_read_tk( task );
   }
   else if ( task->tk == TK_ENUM ) {
      read_enum( task, dec );
   }
   else if ( task->tk == TK_STRUCT ) {
      read_struct( task, dec );
      dec->type_struct = true;
   }
   else {
      t_diag( task, DIAG_POS_ERR, &task->tk_pos, "missing type" );
      t_bail( task );
   }
}

void read_enum( struct task* task, struct dec* dec ) {
   struct pos pos = task->tk_pos;
   t_test_tk( task, TK_ENUM );
   t_read_tk( task );
   if ( task->tk == TK_BRACE_L || dec->type_needed ) {
      t_test_tk( task, TK_BRACE_L );
      t_read_tk( task );
      if ( task->tk == TK_BRACE_R ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos, "empty enum" );
         t_bail( task );
      }
      struct constant* head = NULL;
      struct constant* tail;
      while ( true ) {
         t_test_tk( task, TK_ID );
         struct constant* constant = alloc_constant();
         constant->object.pos = task->tk_pos;
         constant->name = t_make_name( task, task->tk_text,
            task->region->body );
         t_read_tk( task );
         if ( task->tk == TK_ASSIGN ) {
            t_read_tk( task );
            struct read_expr expr;
            t_init_read_expr( &expr );
            expr.in_constant = true;
            t_read_expr( task, &expr );
            constant->expr = expr.node;
         }
         if ( head ) {
            tail->next = constant;
         }
         else {
            head = constant;
         }
         tail = constant;
         if ( task->tk == TK_COMMA ) {
            t_read_tk( task );
            if ( task->tk == TK_BRACE_R ) {
               t_read_tk( task );
               break;
            }
         }
         else {
            t_test_tk( task, TK_BRACE_R );
            t_read_tk( task );
            break;
         }
      }
      struct constant_set* set = mem_alloc( sizeof( *set ) );
      init_object( &set->object, NODE_CONSTANT_SET );
      set->head = head;
      if ( dec->vars ) {
         list_append( dec->vars, set );
      }
      else {
         add_unresolved( task->region, &set->object );
      }
      dec->type = task->type_int;
      if ( ! dec->type_needed ) {
         // Only enum declared.
         if ( task->tk == TK_SEMICOLON ) {
            t_read_tk( task );
            dec->leave = true;
         }
      }
      if ( dec->area == DEC_MEMBER ) {
         t_diag( task, DIAG_POS_ERR, &pos, "enum inside struct" );
         t_bail( task );
      }
   }
   else {
      t_test_tk( task, TK_ID );
      struct constant* constant = alloc_constant();
      constant->object.pos = task->tk_pos;
      constant->name = t_make_name( task, task->tk_text, task->region->body );
      t_read_tk( task );
      t_test_tk( task, TK_ASSIGN );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      expr.in_constant = true;
      t_read_expr( task, &expr );
      constant->expr = expr.node;
      if ( dec->vars ) {
         list_append( dec->vars, constant );
      }
      else {
         add_unresolved( task->region, &constant->object );
      }
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      dec->leave = true;
   }
   STATIC_ASSERT( DEC_TOTAL == 4 );
   if ( dec->area == DEC_FOR ) {
      t_diag( task, DIAG_POS_ERR, &dec->type_pos,
         "enum in for-loop initialization" );
      t_bail( task );
   }
}

struct constant* alloc_constant( void ) {
   struct constant* constant = mem_slot_alloc( sizeof( *constant ) );
   init_object( &constant->object, NODE_CONSTANT );
   constant->name = NULL;
   constant->expr = NULL;
   constant->next = NULL;
   constant->value = 0;
   constant->hidden = false;
   constant->lib_id = 0;
   return constant;
}

void read_struct( struct task* task, struct dec* dec ) {
   t_test_tk( task, TK_STRUCT );
   t_read_tk( task );
   // Definition.
   if ( task->tk == TK_BRACE_L || t_peek( task ) == TK_BRACE_L ) {
      struct name* name = NULL;
      if ( task->tk == TK_ID ) {
         // Don't allow nesting of named structs for now. Maybe later, nested
         // struct support like in C++ will be added. Here is potential syntax
         // for specifying a nested struct, when creating a variable:
         // struct region.struct.my_struct var1;
         // struct upmost.struct.my_struct var2;
         if ( dec->area == DEC_MEMBER ) {
            t_diag( task, DIAG_POS_ERR, &task->tk_pos,
               "name given to nested struct" );
            t_bail( task );
         }
         name = t_make_name( task, task->tk_text, task->region->body_struct );
         t_read_tk( task );
      }
      // When no name is specified, make random name.
      else {
         name = t_make_name( task, "a", task->anon_name );
         task->anon_name = name;
      }
      struct type* type = new_type( task, name );
      type->object.pos = dec->type_pos;
      if ( name == task->anon_name ) {
         type->anon = true;
      }
      // Members:
      t_test_tk( task, TK_BRACE_L );
      t_read_tk( task );
      if ( task->tk == TK_BRACE_R ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos, "empty struct" );
         t_bail( task );
      }
      while ( true ) {
         struct dec member;
         t_init_dec( &member );
         member.area = DEC_MEMBER;
         member.type_make = type;
         member.name_offset = type->body;
         member.type_needed = true;
         member.vars = dec->vars;
         t_read_dec( task, &member );
         if ( task->tk == TK_BRACE_R ) {
            t_read_tk( task );
            break;
         }
      }
      // Nested struct is in the same scope as the parent struct.
      if ( dec->vars ) {
         list_append( dec->vars, type );
      }
      else {
         add_unresolved( task->region, &type->object );
      }
      dec->type = type;
      STATIC_ASSERT( DEC_TOTAL == 4 );
      if ( dec->area == DEC_FOR ) {
         t_diag( task, DIAG_POS_ERR, &dec->type_pos,
            "struct in for-loop initialization" );
         t_bail( task );
      }
      // Only struct declared. Anonymous struct must be part of a variable
      // declaration, so it cannot be declared alone.
      if ( ! dec->type_needed && ! type->anon ) {
         if ( task->tk == TK_SEMICOLON ) {
            t_read_tk( task );
            dec->leave = true;
         }
      }
   }
   // Variable of struct type.
   else {
      dec->type_path = t_read_path( task );
   }
}

void read_storage_index( struct task* task, struct dec* dec ) {
   if ( task->tk == TK_LIT_DECIMAL ) {
      struct pos pos = task->tk_pos;
      if ( dec->area == DEC_MEMBER ) {
         t_diag( task, DIAG_POS_ERR, &pos,
            "storage index specified for struct member" );
         t_bail( task );
      }
      t_test_tk( task, TK_LIT_DECIMAL );
      dec->storage_index = t_extract_literal_value( task );
      t_read_tk( task );
      t_test_tk( task, TK_COLON );
      t_read_tk( task );
      int max = MAX_WORLD_LOCATIONS;
      if ( dec->storage != STORAGE_WORLD ) {
         if ( dec->storage == STORAGE_GLOBAL ) {
            max = MAX_GLOBAL_LOCATIONS;
         }
         else  {
            t_diag( task, DIAG_POS_ERR, &pos,
               "index specified for %s storage", dec->storage_name );
            t_bail( task );
         }
      }
      if ( dec->storage_index >= max ) {
         t_diag( task, DIAG_POS_ERR, &pos,
            "index for %s storage not between 0 and %d", dec->storage_name,
            max - 1 );
         t_bail( task );
      }
   }
   else {
      // Index must be explicitly specified for world and global storages.
      if ( dec->storage == STORAGE_WORLD || dec->storage == STORAGE_GLOBAL ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "missing index for %s storage", dec->storage_name );
         t_bail( task );
      }
   }
}

void read_name( struct task* task, struct dec* dec ) {
   if ( task->tk == TK_ID ) {
      dec->name = t_make_name( task, task->tk_text, dec->name_offset );
      dec->name_pos = task->tk_pos;
      t_read_tk( task );
   }
   else {
      t_diag( task, DIAG_POS_ERR, &task->tk_pos, "missing name" );
      t_bail( task );
   }
}

void read_dim( struct task* task, struct dec* dec ) {
   dec->dim = NULL;
   struct dim* tail = NULL;
   while ( task->tk == TK_BRACKET_L ) {
      struct dim* dim = mem_alloc( sizeof( *dim ) );
      dim->next = NULL;
      dim->size_expr = NULL;
      dim->size = 0;
      dim->element_size = 0;
      dim->pos = task->tk_pos;
      t_read_tk( task );
      // Implicit size.
      if ( task->tk == TK_BRACKET_R ) {
         // Only the first dimension can have an implicit size.
         if ( tail ) {
            t_diag( task, DIAG_POS_ERR, &dim->pos,
               "implicit size in subsequent dimension" );
            t_bail( task );
         }
         // Dimension with implicit size not allowed in struct.
         if ( dec->area == DEC_MEMBER ) {
            t_diag( task, DIAG_POS_ERR, &dim->pos,
               "dimension with implicit size in struct member" );
            t_bail( task );
         }
         t_read_tk( task );
      }
      else {
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         dim->size_expr = expr.node;
         t_test_tk( task, TK_BRACKET_R );
         t_read_tk( task );
      }
      if ( tail ) {
         tail->next = dim;
      }
      else {
         dec->dim = dim;
      }
      tail = dim;
   }
}

void read_init( struct task* task, struct dec* dec ) {
   dec->initial = NULL;
   if ( task->tk == TK_ASSIGN ) {
      if ( dec->area == DEC_MEMBER ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "initializing a struct member" );
         t_bail( task );
      }
      // At this time, there is no good way to initialize a variable having
      // world or global storage at runtime.
      if ( ( dec->storage == STORAGE_WORLD ||
         dec->storage == STORAGE_GLOBAL ) && ( dec->area == DEC_TOP ||
         dec->is_static ) ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "initializing %s variable at start of map",
            dec->storage_name );
         t_bail( task );
      }
      t_read_tk( task );
      if ( task->tk == TK_BRACE_L ) {
         read_multi_init( task, dec, NULL );
         if ( ! dec->dim && ( dec->type && dec->type->primitive ) ) {
            struct multi_value* multi_value =
               ( struct multi_value* ) dec->initial;
            t_diag( task, DIAG_POS_ERR, &multi_value->pos,
               "using brace initializer on scalar variable" );
            t_bail( task );
         }
      }
      else {
         if ( dec->dim ) {
            t_diag( task, DIAG_POS_ERR, &task->tk_pos,
               "missing brace initializer" );
            t_bail( task );
         }
         struct value* value = mem_alloc( sizeof( *value ) );
         init_initial( &value->initial, false );
         struct read_expr expr;
         t_init_read_expr( &expr );
         expr.stmt_read = dec->stmt_read;
         t_read_expr( task, &expr );
         value->expr = expr.node;
         dec->initial = &value->initial;
         dec->initz_str = expr.has_str;
      }
   }
   else {
      // Initializer needs to be present when the size of the initial dimension
      // is implicit. Global and world arrays are an exception, unless they are
      // multi-dimensional.
      if ( dec->dim && ! dec->dim->size_expr && ( (
         dec->storage != STORAGE_WORLD &&
         dec->storage != STORAGE_GLOBAL ) || dec->dim->next ) ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "missing initialization of implicit dimension" );
         t_bail( task );
      }
   }
}

void init_initial( struct initial* initial, bool multi ) {
   initial->next = NULL;
   initial->multi = multi;
   initial->tested = false;
}

void read_multi_init( struct task* task, struct dec* dec,
   struct multi_value_read* parent ) {
   t_test_tk( task, TK_BRACE_L );
   struct multi_value* multi_value = mem_alloc( sizeof( *multi_value ) );
   init_initial( &multi_value->initial, true );
   multi_value->body = NULL;
   multi_value->pos = task->tk_pos;
   multi_value->padding = 0;
   struct multi_value_read read;
   read.multi_value = multi_value;
   read.tail = NULL;
   t_read_tk( task );
   if ( task->tk == TK_BRACE_R ) {
      t_diag( task, DIAG_POS_ERR, &task->tk_pos, "empty initializer" );
      t_bail( task );
   }
   while ( true ) {
      if ( task->tk == TK_BRACE_L ) { 
         read_multi_init( task, dec, &read );
      }
      else {
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         struct value* value = mem_alloc( sizeof( *value ) );
         init_initial( &value->initial, false );
         value->expr = expr.node;
         value->next = NULL;
         value->index = 0;
         if ( read.tail ) {
            read.tail->next = &value->initial;
         }
         else {
            multi_value->body = &value->initial;
         }
         read.tail = &value->initial;
      }
      if ( task->tk == TK_COMMA ) {
         t_read_tk( task );
         if ( task->tk == TK_BRACE_R ) {
            t_read_tk( task );
            break;
         }
      }
      else {
         t_test_tk( task, TK_BRACE_R );
         t_read_tk( task );
         break;
      }
   }
   // Attach multi-value initializer to parent.
   if ( parent ) {
      if ( parent->tail ) {
         parent->tail->next = &multi_value->initial;
      }
      else {
         parent->multi_value->body = &multi_value->initial;
      }
      parent->tail = &multi_value->initial;
   }
   else {
      dec->initial = &multi_value->initial;
   }
}

void add_struct_member( struct task* task, struct dec* dec ) {
   struct type_member* member = mem_alloc( sizeof( *member ) );
   init_object( &member->object, NODE_TYPE_MEMBER );
   member->object.pos = dec->name_pos;
   member->name = dec->name;
   member->type = dec->type;
   member->type_path = dec->type_path;
   member->dim = dec->dim;
   member->next = NULL;
   member->offset = 0;
   member->size = 0;
   if ( dec->type_make->member ) {
      dec->type_make->member_tail->next = member;
   }
   else {
      dec->type_make->member = member;
   }
   dec->type_make->member_tail = member;
}

void add_var( struct task* task, struct dec* dec ) {
   struct var* var = mem_alloc( sizeof( *var ) );
   init_object( &var->object, NODE_VAR );
   var->object.pos = dec->name_pos;
   var->name = dec->name;
   var->type = dec->type;
   var->type_path = dec->type_path;
   var->dim = dec->dim;
   var->initial = dec->initial;
   var->value = NULL;
   var->next = NULL;
   var->storage = dec->storage;
   var->index = dec->storage_index;
   var->size = 0;
   var->initz_zero = false;
   var->hidden = false;
   var->used = false;
   var->initial_has_str = false;
   var->imported = task->library->imported;
   if ( dec->is_static ) {
      var->hidden = true;
   }
   if ( dec->area == DEC_TOP ) {
      add_unresolved( task->region, &var->object );
      list_append( &task->library->vars, var );
   }
   else if ( dec->storage == STORAGE_MAP ) {
      list_append( &task->library->vars, var );
      list_append( dec->vars, var );
   }
   else {
      list_append( dec->vars, var );
   }
}

void read_func( struct task* task, struct dec* dec ) {
   struct func* func = mem_slot_alloc( sizeof( *func ) );
   init_object( &func->object, NODE_FUNC );
   func->object.pos = dec->name_pos;
   func->type = FUNC_ASPEC;
   func->name = dec->name;
   func->params = NULL;
   func->value = dec->type;
   func->impl = NULL;
   func->min_param = 0;
   func->max_param = 0;
   func->hidden = false;
   add_unresolved( task->region, &func->object );
   // Parameter list:
   t_test_tk( task, TK_PAREN_L );
   struct pos params_pos = task->tk_pos;
   t_read_tk( task );
   struct params params;
   init_params( &params );
   if ( task->tk != TK_PAREN_R ) {
      read_params( task, &params );
      func->params = params.node;
      func->min_param = params.min;
      func->max_param = params.max;
   }
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   // Body:
   if ( task->tk == TK_BRACE_L ) {
      func->type = FUNC_USER;
      struct func_user* impl = mem_alloc( sizeof( *impl ) );
      list_init( &impl->labels );
      impl->body = NULL;
      impl->index = 0;
      impl->size = 0;
      impl->usage = 0;
      impl->obj_pos = 0;
      impl->publish = false;
      func->impl = impl;
      // Only read the function body when it is needed.
      if ( ! task->library->imported ) {
         struct stmt_read stmt_read;
         t_init_stmt_read( &stmt_read );
         stmt_read.labels = &impl->labels;
         t_read_block( task, &stmt_read );
         impl->body = stmt_read.block;
      }
      else {
         t_skip_block( task );
      }
      list_append( &task->library->funcs, func );
      list_append( &task->region->items, func );
   }
   else {
      read_bfunc( task, func );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
   }
   if ( dec->area != DEC_TOP ) {
      t_diag( task, DIAG_POS_ERR, &dec->pos, "nested function" );
      t_bail( task );
   }
   if ( dec->storage == STORAGE_WORLD || dec->storage == STORAGE_GLOBAL ) {
      t_diag( task, DIAG_POS_ERR, &dec->storage_pos,
         "storage specified for function" );
      t_bail( task );
   }
   // At this time, returning a struct is not possible. Maybe later, this can
   // be added as part of variable assignment.
   if ( dec->type_struct ) {
      t_diag( task, DIAG_POS_ERR, &dec->type_pos,
         "function returning struct" );
      t_bail( task );
   }
   if ( func->type == FUNC_FORMAT ) {
      if ( ! params.format ) {
         t_diag( task, DIAG_POS_ERR, &params_pos,
            "parameter list missing format parameter" );
         t_bail( task );
      }
   }
   else {
      if ( params.format ) {
         t_diag( task, DIAG_POS_ERR, &params.format_pos,
            "format parameter outside format function" );
         t_bail( task );
      }
   }
}

void init_params( struct params* params ) {
   params->node = NULL;
   params->min = 0;
   params->max = 0;
   params->script = false;
   params->format = false;
} 

void read_params( struct task* task, struct params* params ) {
   if ( task->tk == TK_VOID ) {
      t_read_tk( task );
      return;
   }
   // First parameter of a function can be a format parameter.
   if ( task->tk == TK_BRACE_L ) {
      params->format_pos = task->tk_pos;
      params->format = true;
      ++params->min;
      ++params->max;
      t_read_tk( task );
      t_test_tk( task, TK_BRACE_R );
      t_read_tk( task );
      if ( task->tk == TK_COMMA ) {
         t_read_tk( task );
      }
      else {
         return;
      }
   }
   struct param* tail = NULL;
   while ( true ) {
      struct type* type = task->type_int;
      if ( task->tk == TK_STR ) {
         type = task->type_str;
      }
      else if ( task->tk == TK_BOOL ) {
         type = task->type_bool;
      }
      else {
         t_test_tk( task, TK_INT );
      }
      if ( params->script && type != task->type_int ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "script parameter not of `int` type" );
         t_bail( task );
      }
      struct pos pos = task->tk_pos;
      struct param* param = mem_slot_alloc( sizeof( *param ) );
      init_object( &param->object, NODE_PARAM );
      param->type = type;
      param->next = NULL;
      param->name = NULL;
      param->expr = NULL;
      param->index = 0;
      param->obj_pos = 0;
      ++params->max;
      t_read_tk( task );
      // Name not required for a parameter.
      if ( task->tk == TK_ID ) {
         param->name = t_make_name( task, task->tk_text, task->region->body );
         param->object.pos = task->tk_pos;
         t_read_tk( task );
      }
      if ( task->tk == TK_ASSIGN ) {
         t_read_tk( task );
         struct read_expr expr;
         t_init_read_expr( &expr );
         expr.skip_assign = true;
         t_read_expr( task, &expr );
         param->expr = expr.node;
         if ( params->script ) {
            t_diag( task, DIAG_POS_ERR, &pos, "default parameter in script" );
            t_bail( task );
         }
      }
      else {
         if ( tail && tail->expr ) {
            t_diag( task, DIAG_POS_ERR, &pos,
               "parameter missing default value" );
            t_bail( task );
         }
         ++params->min;
      }
      if ( tail ) {
         tail->next = param;
      }
      else {
         params->node = param;
      }
      tail = param;
      if ( task->tk == TK_COMMA ) {
         t_read_tk( task );
      }
      else {
         break;
      }
   }
   // Format parameter not allowed in a script parameter list.
   if ( params->script && params->format ) {
      t_diag( task, DIAG_POS_ERR, &params->format_pos,
         "format parameter specified for script" );
      t_bail( task );
   }
}

void read_bfunc( struct task* task, struct func* func ) {
   // Action special.
   if ( task->tk == TK_ASSIGN ) {
      t_read_tk( task );
      struct func_aspec* impl = mem_slot_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      impl->id = t_extract_literal_value( task );
      impl->script_callable = false;
      t_read_tk( task );
      t_test_tk( task, TK_COMMA );
      t_read_tk( task );
      t_test_tk( task, TK_LIT_DECIMAL );
      if ( t_extract_literal_value( task ) ) {
         impl->script_callable = true;
      }
      func->impl = impl;
      t_read_tk( task );
   }
   // Extension function.
   else if ( task->tk == TK_ASSIGN_SUB ) {
      t_read_tk( task );
      func->type = FUNC_EXT;
      struct func_ext* impl = mem_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      impl->id = t_extract_literal_value( task );
      func->impl = impl;
      t_read_tk( task );
   }
   // Dedicated function.
   else if ( task->tk == TK_ASSIGN_ADD ) {
      t_read_tk( task );
      func->type = FUNC_DED;
      struct func_ded* impl = mem_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      impl->opcode = t_extract_literal_value( task );
      t_read_tk( task );
      t_test_tk( task, TK_COMMA );
      t_read_tk( task );
      t_test_tk( task, TK_LIT_DECIMAL );
      impl->latent = false;
      if ( task->tk_text[ 0 ] != '0' ) {
         impl->latent = true;
      }
      t_read_tk( task );
      func->impl = impl;
   }
   // Format function.
   else if ( task->tk == TK_ASSIGN_MUL ) {
      t_read_tk( task );
      func->type = FUNC_FORMAT;
      struct func_format* impl = mem_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      impl->opcode = t_extract_literal_value( task );
      func->impl = impl;
      t_read_tk( task );
   }
   // Internal function.
   else {
      t_test_tk( task, TK_ASSIGN_DIV );
      t_read_tk( task );
      func->type = FUNC_INTERNAL;
      struct func_intern* impl = mem_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      struct pos pos = task->tk_pos;
      impl->id = t_extract_literal_value( task );
      t_read_tk( task );
      if ( impl->id >= INTERN_FUNC_STANDALONE_TOTAL ) {
         t_diag( task, DIAG_POS_ERR, &pos,
            "no internal function with ID of %d", impl->id );
         t_bail( task );
      }
      func->impl = impl;
   }
}

void t_read_script( struct task* task ) {
   t_test_tk( task, TK_SCRIPT );
   struct script_read read;
   read.pos = task->tk_pos;
   list_init( &read.labels );
   read.number = NULL;
   read.params = NULL;
   read.body = NULL;
   read.type = SCRIPT_TYPE_CLOSED;
   read.flags = 0;
   read.num_param = 0;
   t_read_tk( task );
   read_script_number( task, &read );
   read_script_param( task, &read );
   read_script_type( task, &read );
   read_script_flag( task, &read );
   read_script_body( task, &read );
   struct script* script = mem_alloc( sizeof( *script ) );
   script->node.type = NODE_SCRIPT;
   script->pos = read.pos;
   script->number = read.number;
   script->type = read.type;
   script->flags = read.flags;
   script->params = read.params;
   script->body = read.body;
   script->labels = read.labels;
   script->num_param = read.num_param;
   script->offset = 0;
   script->size = 0;
   script->tested = false;
   script->publish = false;
   list_append( &task->library->scripts, script );
   list_append( &task->region->items, script );
}

void read_script_number( struct task* task, struct script_read* read ) {
   if ( task->tk == TK_SHIFT_L ) {
      t_read_tk( task );
      // The token between the << and >> tokens must be the digit zero.
      if ( task->tk == TK_LIT_DECIMAL && task->tk_text[ 0 ] == '0' &&
         task->tk_length == 1 ) {
         t_read_tk( task );
         t_test_tk( task, TK_SHIFT_R );
         t_read_tk( task );
      }
      else {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos, "missing `0`" );
         t_bail( task );
      }
   }
   else {
      struct read_expr expr;
      t_init_read_expr( &expr );
      // When reading the script number, the left parenthesis of the
      // parameter list can be mistaken for a function call.
      expr.skip_function_call = true;
      t_read_expr( task, &expr );
      read->number = expr.node;
   }
}

void read_script_param( struct task* task, struct script_read* read ) {
   read->param_pos = task->tk_pos;
   if ( task->tk == TK_PAREN_L ) {
      t_read_tk( task );
      if ( task->tk == TK_PAREN_R ) {
         t_read_tk( task );
      }
      else {
         struct params params;
         init_params( &params );
         params.script = true;
         read_params( task, &params );
         read->params = params.node;
         read->num_param = params.max;
         t_test_tk( task, TK_PAREN_R );
         t_read_tk( task );
      }
   }
}

void read_script_type( struct task* task, struct script_read* read ) {
   switch ( task->tk ) {
   case TK_OPEN: read->type = SCRIPT_TYPE_OPEN; break;
   case TK_RESPAWN: read->type = SCRIPT_TYPE_RESPAWN; break;
   case TK_DEATH: read->type = SCRIPT_TYPE_DEATH; break;
   case TK_ENTER: read->type = SCRIPT_TYPE_ENTER; break;
   case TK_PICKUP: read->type = SCRIPT_TYPE_PICKUP; break;
   case TK_BLUE_RETURN: read->type = SCRIPT_TYPE_BLUE_RETURN; break;
   case TK_RED_RETURN: read->type = SCRIPT_TYPE_RED_RETURN; break;
   case TK_WHITE_RETURN: read->type = SCRIPT_TYPE_WHITE_RETURN; break;
   case TK_LIGHTNING: read->type = SCRIPT_TYPE_LIGHTNING; break;
   case TK_DISCONNECT: read->type = SCRIPT_TYPE_DISCONNECT; break;
   case TK_UNLOADING: read->type = SCRIPT_TYPE_UNLOADING; break;
   case TK_RETURN: read->type = SCRIPT_TYPE_RETURN; break;
   case TK_EVENT: read->type = SCRIPT_TYPE_EVENT; break;
   default: break;
   }
   if ( read->type == SCRIPT_TYPE_CLOSED ) {
      if ( read->num_param > SCRIPT_MAX_PARAMS ) {
         t_diag( task, DIAG_POS_ERR, &read->param_pos,
            "script has over %d parameters", SCRIPT_MAX_PARAMS );
         t_bail( task );
      }
   }
   else if ( read->type == SCRIPT_TYPE_DISCONNECT ) {
      // A disconnect script must have a single parameter. It is the number of
      // the player who disconnected from the server.
      if ( read->num_param < 1 ) {
         t_diag( task, DIAG_POS_ERR, &read->param_pos,
            "disconnect script missing player-number parameter" );
         t_bail( task );
      }
      if ( read->num_param > 1 ) {
         t_diag( task, DIAG_POS_ERR, &read->param_pos,
            "too many parameters in disconnect script" );
         t_bail( task );

      }
      t_read_tk( task );
   }
   else if ( read->type == SCRIPT_TYPE_EVENT ) {
      if ( read->num_param != 3 ) {
         t_diag( task, DIAG_POS_ERR, &read->param_pos,
            "incorrect number of parameters in event script" );
         t_diag( task, DIAG_FILE, &read->param_pos,
            "an event script takes exactly 3 parameters" );
         t_bail( task );
      }
      t_read_tk( task );
   }
   else {
      if ( read->num_param ) {
         t_diag( task, DIAG_POS_ERR, &read->param_pos,
            "parameter list of %s script not empty", task->tk_text );
         t_bail( task );
      }
      t_read_tk( task );
   }
}

void read_script_flag( struct task* task, struct script_read* read ) {
   while ( true ) {
      int flag = SCRIPT_FLAG_NET;
      if ( task->tk != TK_NET ) {
         if ( task->tk == TK_CLIENTSIDE ) {
            flag = SCRIPT_FLAG_CLIENTSIDE;
         }
         else {
            break;
         }
      }
      if ( ! ( read->flags & flag ) ) {
         read->flags |= flag;
         t_read_tk( task );
      }
      else {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos, "duplicate %s flag",
            task->tk_text );
         t_bail( task );
      }
   }
}

void read_script_body( struct task* task, struct script_read* read ) {
   struct stmt_read stmt;
   t_init_stmt_read( &stmt );
   stmt.labels = &read->labels;
   t_read_stmt( task, &stmt );
   read->body = stmt.node;
}

void t_test( struct task* task ) {
   // Associate name with region object.
   list_iter_t i;
   list_iter_init( &i, &task->regions );
   while ( ! list_end( &i ) ) {
      bind_region( task, list_data( &i ) );
      list_next( &i );
   }
   // Import region objects.
   list_iter_init( &i, &task->regions );
   while ( ! list_end( &i ) ) {
      task->region = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &task->region->imports );
      while ( ! list_end( &k ) ) {
         t_import( task, list_data( &k ) );
         list_next( &k );
      }
      list_next( &i );
   }
   // Resolve region objects.
   int resolved = 0;
   bool undef_err = false;
   bool retry = false;
   while ( true ) {
      list_iter_t i;
      list_iter_init( &i, &task->regions );
      while ( ! list_end( &i ) ) {
         task->region = list_data( &i );
         test_region( task, &resolved, &undef_err, &retry );
         list_next( &i );
      }
      if ( retry ) {
         retry = false;
         if ( resolved ) {
            resolved = 0;
         }
         else {
            undef_err = true;
         }
      }
      else {
         break;
      }
   }
   // Determine which scripts and functions to publish.
   list_iter_init( &i, &task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      script->publish = true;
      list_next( &i );
   }
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      impl->publish = true;
      list_next( &i );
   }
   // Test the body of functions, and scripts.
   list_iter_init( &i, &task->regions );
   while ( ! list_end( &i ) ) {
      task->region = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &task->region->items );
      while ( ! list_end( &k ) ) {
         struct node* node = list_data( &k );
         if ( node->type == NODE_FUNC ) {
            struct func* func = ( struct func* ) node;
            struct func_user* impl = func->impl;
            if ( impl->publish ) {
               test_func_body( task, func );
            }
         }
         else {
            struct script* script = ( struct script* ) node;
            if ( script->publish ) {
               test_script( task, script );
               list_append( &task->scripts, script );
            }
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // There should be no duplicate scripts.
   list_iter_init( &i, &task->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      list_iter_t k = i;
      list_next( &k );
      bool dup = false;
      while ( ! list_end( &k ) ) {
         struct script* other_script = list_data( &k );
         if ( t_get_script_number( script ) ==
            t_get_script_number( other_script ) ) {
            if ( ! dup ) {
               t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &script->pos, "duplicate script %d",
                  t_get_script_number( script ) );
               dup = true;
            }
            t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &other_script->pos,
               "script found here", t_get_script_number( script ) );
         }
         list_next( &k );
      }
      if ( dup ) {
         t_bail( task );
      }
      list_next( &i );
   }
   calc_map_var_size( task );
   calc_map_value_index( task );
   count_string_usage( task );
   // Any library that has its contents used is dynamically loaded.
   list_iter_init( &i, &task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib != task->library_main ) {
         bool used = false;
         // Functions.
         list_iter_t k;
         list_iter_init( &k, &lib->funcs );
         while ( ! list_end( &k ) ) {
            struct func* func = list_data( &k );
            struct func_user* impl = func->impl;
            if ( impl->usage ) {
               used = true;
               break;
            }
            list_next( &k );
         }
         // Variables.
         if ( ! used ) {
            list_iter_init( &k, &lib->vars );
            while ( ! list_end( &k ) ) {
               struct var* var = list_data( &k );
               if ( var->storage == STORAGE_MAP && var->used ) {
                  used = true;
                  break;
               }
               list_next( &k );
            }
         }
         // Add library.
         if ( used ) {
            list_iter_init( &k, &task->library_main->dynamic );
            while ( ! list_end( &k ) && list_data( &i ) != lib ) {
               list_next( &k );
            }
            if ( list_end( &i ) ) {
               list_append( &task->library_main->dynamic, lib );
            }
         }
      }
      list_next( &i );
   }
}

void bind_region( struct task* task, struct region* region ) {
   struct object* object = region->unresolved;
   while ( object ) {
      if ( object->node.type == NODE_CONSTANT ) {
         struct constant* constant = ( struct constant* ) object;
         bind_object( task, constant->name, &constant->object );
      }
      else if ( object->node.type == NODE_CONSTANT_SET ) {
         struct constant_set* set = ( struct constant_set* ) object;
         struct constant* constant = set->head;
         while ( constant ) {
            bind_object( task, constant->name, &constant->object );
            constant = constant->next;
         }
      }
      else if ( object->node.type == NODE_VAR ) {
         struct var* var = ( struct var* ) object;
         bind_object( task, var->name, &var->object );
      }
      else if ( object->node.type == NODE_FUNC ) {
         struct func* func = ( struct func* ) object;
         bind_object( task, func->name, &func->object );
      }
      else if ( object->node.type == NODE_TYPE ) {
         struct type* type = ( struct type* ) object;
         if ( type->name->object ) {
            diag_dup_struct( task, type->name, &type->object.pos );
            t_bail( task );
         }
         type->name->object = &type->object;
      }
      object = object->next;
   }
}

void bind_object( struct task* task, struct name* name,
   struct object* object ) {
   if ( name->object ) {
      struct str str;
      str_init( &str );
      t_copy_name( name, false, &str );
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &object->pos, "duplicate name `%s`", str.value );
      t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &name->object->pos,
         "name already used here", str.value );
      t_bail( task );
   }
   name->object = object;
}

void t_import( struct task* task, struct import* stmt ) {
   // Determine region to import from.
   struct region* region = task->region_upmost;
   struct path* path = stmt->path;
   if ( ! path->text ) {
      if ( path->is_region ) {
         region = task->region;
      }
      path = path->next;
   }
   while ( path ) {
      struct name* name = t_make_name( task, path->text, region->body );
      if ( ! name->object || name->object->node.type != NODE_REGION ) {
         t_diag( task, DIAG_POS_ERR, &path->pos,
            "region `%s` not found", path->text );
         t_bail( task );
      }
      region = ( struct region* ) name->object;
      path = path->next;
   }
   // Import objects.
   struct import_item* item = stmt->item;
   while ( item ) {
      // Make link to region.
      if ( item->is_link ) {
         struct region* linked_region = region;
         if ( item->name ) {
            struct name* name = t_make_name( task, item->name, region->body );
            struct object* object = t_get_region_object( task, region, name );
            // The object needs to exist.
            if ( ! object ) {
               t_diag( task, DIAG_POS_ERR, &item->name_pos,
                  "`%s` not found", item->name );
               t_bail( task );
            }
            // The object needs to be a region.
            if ( object->node.type != NODE_REGION ) {
               t_diag( task, DIAG_POS_ERR, &item->name_pos,
                  "`%s` not a region", item->name );
               t_bail( task );
            }
            linked_region = ( struct region* ) object;
         }
         if ( linked_region == task->region ) {
            t_diag( task, DIAG_POS_ERR, &item->pos,
               "region importing self as default region" );
            t_bail( task );
         }
         struct region_link* link = task->region->link;
         while ( link && link->region != linked_region ) {
            link = link->next;
         }
         // Duplicate links are allowed in the source code.
         if ( link ) {
            t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &item->pos, "duplicate import of default region" );
            t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &link->pos,
               "import already made here" );
         }
         else {
            link = mem_alloc( sizeof( *link ) );
            link->next = task->region->link;
            link->region = linked_region;
            link->pos = item->pos;
            task->region->link = link;
         }
      }
      // Import selected region.
      else if ( ! item->name && ! item->alias ) {
         path = stmt->path;
         while ( path->next ) {
            path = path->next;
         }
         if ( ! path->text ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &item->pos, "region imported without name" );
            t_bail( task );
         }
         alias_imported( task, path->text, &path->pos, &region->object );
      }
      else {
         struct object* object = NULL;
         if ( item->is_struct ) {
            struct name* name = t_make_name( task, item->name,
               region->body_struct );
            object = name->object;
         }
         // Alias to selected region.
         else if ( ! item->name ) {
            object = &region->object;
         }
         else {
            struct name* name = t_make_name( task, item->name, region->body );
            object = t_get_region_object( task, region, name );
         }
         if ( ! object ) {
            const char* prefix = "";
            if ( item->is_struct ) {
               prefix = "struct ";
            }
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &item->name_pos, "%s`%s` not found in region", prefix,
               item->name );
            t_bail( task );
         }
         if ( item->alias ) {
            alias_imported( task, item->alias, &item->alias_pos, object );
         }
         else {
            alias_imported( task, item->name, &item->name_pos, object );
         }
      }
      item = item->next;
   }
}

void alias_imported( struct task* task, char* alias_name,
   struct pos* alias_pos, struct object* object ) {
   struct name* name = task->region->body;
   if ( object->node.type == NODE_TYPE ) {
      name = task->region->body_struct;
   }
   name = t_make_name( task, alias_name, name );
   if ( name->object ) {
      // Duplicate imports are allowed as long as both names refer to the
      // same object.
      bool valid = false;
      if ( name->object->node.type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) name->object;
         if ( object == alias->target ) {
            t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               alias_pos, "duplicate import name `%s`", alias_name );
            t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &alias->object.pos,
               "import name already used here", alias_name );
            valid = true;
         }
      }
      if ( ! valid ) {
         diag_dup( task, alias_name, alias_pos, name );
         t_bail( task );
      }
   }
   else {
      struct alias* alias = mem_alloc( sizeof( *alias ) );
      init_object( &alias->object, NODE_ALIAS );
      alias->object.pos = *alias_pos;
      alias->object.resolved = true;
      alias->target = object;
      if ( task->depth ) {
         t_use_local_name( task, name, &alias->object );
      }
      else {
         name->object = &alias->object;
      }
   }
}

void test_region( struct task* task, int* resolved, bool* undef_err,
   bool* retry ) {
   struct object* object = task->region->unresolved;
   struct object* tail = NULL;
   task->region->unresolved = NULL;
   while ( object ) {
      if ( object->node.type == NODE_CONSTANT ) {
         t_test_constant( task, ( struct constant* ) object, *undef_err );
      }
      else if ( object->node.type == NODE_CONSTANT_SET ) {
         t_test_constant_set( task, ( struct constant_set* ) object,
            *undef_err );
      }
      else if ( object->node.type == NODE_TYPE ) {
         t_test_type( task, ( struct type* ) object, *undef_err );
      }
      else if ( object->node.type == NODE_VAR ) {
         test_var( task, ( struct var* ) object, *undef_err );
      }
      else if ( object->node.type == NODE_FUNC ) {
         test_func( task, ( struct func* ) object, *undef_err );
      }
      if ( object->resolved ) {
         struct object* next = object->next;
         object->next = NULL;
         object = next;
         ++*resolved;
      }
      else {
         if ( tail ) {
            tail->next = object;
         }
         else {
            task->region->unresolved = object;
         }
         tail = object;
         struct object* next = object->next;
         object->next = NULL;
         object = next;
         *retry = true;
      }
   }
}

void t_test_constant( struct task* task, struct constant* constant,
   bool undef_err ) {
   // Test name. Only applies in a local scope.
   if ( task->depth ) {
      if ( ! constant->name->object ||
         constant->name->object->depth != task->depth ) {
         t_use_local_name( task, constant->name, &constant->object );
      }
      else {
         struct str str;
         str_init( &str );
         t_copy_name( constant->name, false, &str );
         diag_dup( task, str.value, &constant->object.pos, constant->name );
         t_bail( task );
      }
   }
   // Test expression.
   struct expr_test expr;
   t_init_expr_test( &expr );
   expr.undef_err = undef_err;
   t_test_expr( task, &expr, constant->expr );
   if ( ! expr.undef_erred ) {
      if ( constant->expr->folded ) {
         constant->value = constant->expr->value;
         constant->object.resolved = true;
      }
      else {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &constant->expr->pos, "expression not constant" );
         t_bail( task );
      }
   }
}

void t_test_constant_set( struct task* task, struct constant_set* set,
   bool undef_err ) {
   int value = 0;
   // Find the next unresolved constant.
   struct constant* constant = set->head;
   while ( constant && constant->object.resolved ) {
      value = constant->value;
      constant = constant->next;
   }
   while ( constant ) {
      if ( task->depth ) {
         if ( ! constant->name->object ||
            constant->name->object->depth != task->depth ) {
            t_use_local_name( task, constant->name, &constant->object );
         }
         else {
            struct str str;
            str_init( &str );
            t_copy_name( constant->name, false, &str );
            diag_dup( task, str.value, &constant->object.pos, constant->name );
            t_bail( task );
         }
      }
      if ( constant->expr ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = undef_err;
         t_test_expr( task, &expr, constant->expr );
         if ( expr.undef_erred ) {
            return;
         }
         if ( ! constant->expr->folded ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &expr.pos, "enumerator expression not constant" );
            t_bail( task );
         }
         value = constant->expr->value;
      }
      constant->value = value;
      ++value;
      constant->object.resolved = true;
      constant = constant->next;
   }
   // Set is resolved when all of the constants in it are resolved.
   set->object.resolved = true;
}

void t_test_type( struct task* task, struct type* type, bool undef_err ) {
   // Name.
   if ( task->depth ) {
      if ( type->name->object && type->name->object->depth == task->depth ) {
         diag_dup_struct( task, type->name, &type->object.pos );
         t_bail( task );
      }
      t_use_local_name( task, type->name, &type->object );
   }
   // Members.
   struct type_member* member = type->member;
   while ( member ) {
      if ( ! member->object.resolved ) {
         test_type_member( task, member, undef_err );
         if ( ! member->object.resolved ) {
            return;
         }
      }
      member = member->next;
   }
   type->object.resolved = true;
}

void test_type_member( struct task* task, struct type_member* member,
   bool undef_err ) {
   // Type:
   if ( member->type_path ) {
      if ( ! member->type ) {
         member->type = find_type( task, member->type_path );
      }
      if ( ! member->type->object.resolved ) {
         if ( undef_err ) {
            struct path* path = member->type_path;
            while ( path->next ) {
               path = path->next;
            }
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &path->pos,
               "struct `%s` undefined", path->text );
            t_bail( task );
         }
         return;
      }
   }
   // Name:
   if ( task->depth ) {
      if ( ! member->name->object ||
         member->name->object->depth != task->depth ) {
         t_use_local_name( task, member->name, &member->object );
      }
      else {
         diag_dup_struct_member( task, member->name, &member->object.pos );
         t_bail( task );
      }
   }
   else {
      if ( member->name->object != &member->object ) {
         if ( ! member->name->object ) {
            member->name->object = &member->object;
         }
         else {
            diag_dup_struct_member( task, member->name, &member->object.pos );
            t_bail( task );
         }
      }
   }
   // Dimension.
   struct dim* dim = member->dim;
   // Skip to the next unresolved dimension.
   while ( dim && dim->size ) {
      dim = dim->next;
   }
   while ( dim ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = undef_err;
      t_test_expr( task, &expr, dim->size_expr );
      if ( expr.undef_erred ) {
         return;
      }
      if ( ! dim->size_expr->folded ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
         "array size not a constant expression" );
         t_bail( task );
      }
      if ( dim->size_expr->value <= 0 ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "array size must be greater than 0" );
         t_bail( task );
      }
      dim->size = dim->size_expr->value;
      dim = dim->next;
   }
   member->object.resolved = true;
}

// NOTE: The code here and the code that implements the dot operator in the
// expression subsystem, are very similar. Maybe find a way to merge them.
struct type* find_type( struct task* task, struct path* path ) {
   // Find head of path.
   struct object* object = NULL;
   if ( path->is_upmost ) {
      object = &task->region_upmost->object;
      path = path->next;
   }
   else if ( path->is_region ) {
      object = &task->region->object;
      path = path->next;
   }
   // When no region is specified, search for the head in the current region.
   if ( ! object ) {
      struct name* name = task->region->body;
      if ( ! path->next ) {
         name = task->region->body_struct;
      }
      name = t_make_name( task, path->text, name );
      if ( name->object ) {
         object = name->object;
         path = path->next;
      }
   }
   // When the head is not found in the current region, try linked regions.
   struct region_link* link = NULL;
   if ( ! object ) {
      link = task->region->link;
      while ( link ) {
         struct name* name = link->region->body;
         if ( ! path->next ) {
            name = link->region->body_struct;
         }
         name = t_make_name( task, path->text, name );
         link = link->next;
         if ( name->object ) {
            object = name->object;
            path = path->next;
            break;
         }
      }
   }
   // Error.
   if ( ! object ) {
      const char* msg = "`%s` not found";
      if ( ! path->next ) {
         msg = "struct `%s` not found";
      }
      t_diag( task, DIAG_POS_ERR, &path->pos, msg, path->text );
      t_bail( task );
   }
   // When using a region link, make sure no other object with the same name
   // can be found.
   if ( link ) {
      bool dup = false;
      while ( link ) {
         struct name* name = link->region->body;
         if ( ! path->next ) {
            name = link->region->body_struct;
         }
         name = t_make_name( task, path->text, name );
         if ( name->object ) {
            const char* type = "object";
            if ( ! path->next ) {
               type = "struct";
            }
            if ( ! dup ) {
               t_diag( task, DIAG_POS_ERR, &path->pos,
                  "%s `%s` found in multiple modules", type, path->text );
               t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &object->pos,
                  "%s found here", type );
               dup = true;
            }
            t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &name->object->pos, "%s found here", type );
         }
         link = link->next;
      }
      if ( dup ) {
         t_bail( task );
      }
   }
   // Navigate rest of path.
   while ( true ) {
      // Follow a shortcut. It needs to refer to an object.
      while ( object->node.type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) object;
         object = alias->target;
      }
      if ( ! path ) {
         break;
      }
      if ( object->node.type == NODE_REGION ) {
         struct region* region = ( struct region* ) object;
         struct name* name = region->body;
         if ( ! path->next ) {
            name = region->body_struct;
         }
         name = t_make_name( task, path->text, name );
         if ( name->object ) {
            object = name->object;
         }
         else {
            if ( path->next ) {
               t_diag( task, DIAG_POS_ERR, &path->pos, "region `%s` not found",
                  path->text );
            }
            else {
               t_diag( task, DIAG_POS_ERR, &path->pos, "struct `%s` not found",
                  path->text ); 
            }
            t_bail( task );
         }
      }
      else {
         t_diag( task, DIAG_POS_ERR, &path->pos,
            "accessing something not a region" );
         t_bail( task );
      }
      path = path->next;
   }
   return ( struct type* ) object;
}

void test_var( struct task* task, struct var* var, bool undef_err ) {
   // Type.
   if ( var->type_path ) {
      if ( ! var->type ) {
         var->type = find_type( task, var->type_path );
      }
      if ( ! var->type->object.resolved ) {
         return;
      }
      // An array or a variable with a structure type cannot appear in local
      // storage because there's no standard or efficient way to allocate such
      // a variable.
      if ( var->storage == STORAGE_LOCAL ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &var->object.pos, "variable of struct type in local storage" );
         t_bail( task );
      }
   }
   // Name.
   if ( task->depth ) {
      if ( var->name->object && var->name->object->depth == task->depth ) {
         struct str str;
         str_init( &str );
         t_copy_name( var->name, false, &str );
         diag_dup( task, str.value, &var->object.pos, var->name );
         t_bail( task );
      }
      t_use_local_name( task, var->name, &var->object );
   }
   // Dimension.
   if ( var->dim && var->storage == STORAGE_LOCAL ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &var->object.pos,
         "array in local storage" );
      t_bail( task );
   }
   struct dim* dim = var->dim;
   // Skip to the next unresolved dimension.
   while ( dim && dim->size ) {
      dim = dim->next;
   }
   while ( dim ) {
      if ( dim->size_expr ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = undef_err;
         t_test_expr( task, &expr, dim->size_expr );
         if ( expr.undef_erred ) {
            return;
         }
         else if ( ! dim->size_expr->folded ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "array size not a constant expression" );
            t_bail( task );
         }
         else if ( dim->size_expr->value <= 0 ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "array size must be greater than 0" );
            t_bail( task );
         }
         dim->size = dim->size_expr->value;
      }
      else {
         // Only the first dimension can have an implicit size.
         if ( dim != var->dim ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &dim->pos,
               "implicit size in subsequent dimension" );
            t_bail( task );
         }
      }
      dim = dim->next;
   }
   // Initialization:
   if ( var->initial ) {
      bool undef_erred = false;
      test_init( task, var, undef_err, &undef_erred );
      if ( undef_erred ) {
         return;
      }
   }
   var->object.resolved = true;
}

void test_init( struct task* task, struct var* var, bool undef_err,
   bool* undef_erred ) {
   if ( var->dim ) {
      if ( var->imported ) {
         // TODO: Add error checking.
         if ( ! var->dim->size_expr ) {
            struct multi_value* multi_value =
               ( struct multi_value* ) var->initial;
            struct initial* initial = multi_value->body;
            while ( initial ) {
               initial = initial->next;
               ++var->dim->size;
            }
         }
      }
      else {
         struct multi_value_test test;
         init_multi_value_test( &test,
            NULL,
            ( struct multi_value* ) var->initial,
            var->dim,
            var->type,
            var->type->member,
            undef_err );
         test_multi_value( task, &test );
         if ( test.undef_erred ) {
            *undef_erred = true;
            return;
         }
         // Size of implicit dimension.
         if ( ! var->dim->size_expr ) {
            var->dim->size = test.count;
         }
         if ( test.has_string ) {
            var->initial_has_str = true;
         }
      }
   }
   else if ( ! var->type->primitive ) {
      if ( ! var->imported ) {
         struct multi_value_test test;
         init_multi_value_test( &test,
            NULL,
            ( struct multi_value* ) var->initial,
            NULL,
            var->type,
            var->type->member,
            undef_err );
         test_multi_value_struct( task, &test );
         if ( test.undef_erred ) {
            *undef_erred = true;
            return;
         }
         if ( test.has_string ) {
            var->initial_has_str = true;
         }
      }
   }
   else {
      if ( ! var->imported ) {
         struct value* value = ( struct value* ) var->initial;
         struct expr_test expr_test;
         t_init_expr_test( &expr_test );
         expr_test.undef_err = undef_err;
         t_test_expr( task, &expr_test, value->expr );
         if ( expr_test.undef_erred ) {
            *undef_erred = true;
            return;
         }
         if ( var->storage == STORAGE_MAP && ! value->expr->folded ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &expr_test.pos, "initial value not constant" );
            t_bail( task );
         }
         var->value = value;
         if ( expr_test.has_string ) {
            var->initial_has_str = true;
         }
      }
   }
}

void init_multi_value_test( struct multi_value_test* test,
   struct multi_value_test* parent, struct multi_value* multi_value,
   struct dim* dim, struct type* type, struct type_member* type_member,
   bool undef_err ) {
   test->parent = parent;
   test->multi_value = multi_value;
   test->dim = dim;
   test->type = type;
   test->type_member = type_member;
   test->count = 0;
   test->undef_err = undef_err;
   test->undef_erred = false;
   test->has_string = false;
}

void test_multi_value( struct task* task, struct multi_value_test* test ) {
   struct initial* initial = test->multi_value->body;
   while ( initial ) {
      if ( initial->tested ) {
         goto next;
      }
      // Overflow.
      if ( test->dim->size && test->count >= test->dim->size ) {
         t_diag( task, DIAG_POS_ERR, &test->multi_value->pos,
            "too many values in brace initializer" );
         t_bail( task );
      }
      if ( initial->multi ) {
         struct multi_value* multi_value = ( struct multi_value* ) initial;
         // There needs to be an element to initialize.
         if ( ! test->dim->next && test->type->primitive ) {
            t_diag( task, DIAG_POS_ERR, &multi_value->pos,
               "too many brace initializers" );
            t_bail( task );
         }
         struct multi_value_test nested_test;
         init_multi_value_test( &nested_test, test, multi_value, NULL,
            test->type, NULL, test->undef_err );
         if ( test->dim->next ) {
            nested_test.dim = test->dim->next;
         }
         // Test.
         if ( nested_test.dim ) {
            test_multi_value( task, &nested_test );
         }
         else {
            test_multi_value_struct( task, &nested_test );
         }
         // Stop on failure.
         if ( nested_test.undef_erred ) {
            test->undef_erred = true;
            return;
         }
         initial->tested = true;
         if ( nested_test.has_string ) {
            test->has_string = true;
         }
      }
      else {
         struct value* value = ( struct value* ) initial;
         struct expr_test expr_test;
         t_init_expr_test( &expr_test );
         expr_test.undef_err = test->undef_err;
         t_test_expr( task, &expr_test, value->expr );
         if ( expr_test.undef_erred ) {
            test->undef_erred = true;
            return;
         }
         if ( test->dim->next || ! test->type->primitive ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &expr_test.pos, "missing another brace initializer" );
            t_bail( task );
         }
         if ( ! value->expr->folded ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &expr_test.pos, "initial value not constant" );
            t_bail( task );
         }
         initial->tested = true;
         if ( expr_test.has_string ) {
            test->has_string = true;
         }
      }
      next:
      initial = initial->next;
      ++test->count;
   }
}

void test_multi_value_struct( struct task* task,
   struct multi_value_test* test ) {
   struct type_member* member = test->type->member;
   struct initial* initial = test->multi_value->body;
   while ( initial ) {
      if ( initial->tested ) {
         goto next;
      }
      // Overflow.
      if ( ! member ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &test->multi_value->pos, "too many values in brace initializer" );
         t_bail( task );
      }
      if ( initial->multi ) {
         struct multi_value* multi_value = ( struct multi_value* ) initial;
         if ( ! ( member->dim || ! member->type->primitive ) ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &multi_value->pos, "too many brace initializers" );
            t_bail( task );
         }
         struct multi_value_test nested_test;
         if ( member->dim ) {
            init_multi_value_test( &nested_test, test, multi_value,
               member->dim, member->type, NULL, test->undef_err );
            test_multi_value( task, &nested_test );
         }
         else {
            init_multi_value_test( &nested_test, test, multi_value,
               member->dim, member->type, NULL, test->undef_err );
            test_multi_value_struct( task, &nested_test );
         }
         if ( nested_test.undef_erred ) {
            test->undef_erred = true;
            return;
         }
         initial->tested = true;
      }
      else {
         struct value* value = ( struct value* ) initial;
         struct expr_test expr_test;
         t_init_expr_test( &expr_test );
         expr_test.undef_err = test->undef_err;
         t_test_expr( task, &expr_test, value->expr );
         if ( expr_test.undef_erred ) {
            test->undef_erred = true;
            return;
         }
         if ( member->dim || ! member->type->primitive ) {
            t_diag( task, DIAG_POS_ERR, &expr_test.pos,
               "missing another brace initializer" );
            t_bail( task );
         }
         if ( ! value->expr->folded ) {
            t_diag( task, DIAG_POS_ERR, &expr_test.pos,
               "initial value not constant" );
            t_bail( task );
         }
         // At this time, I know of no good way to initialize a string member.
         // The user will have to initialize the member manually, by using an
         // assignment operation.
         if ( member->type->is_str ) {
            t_diag( task, DIAG_POS_ERR, &value->expr->pos,
               "initializing struct member of string type" );
            t_bail( task );
         }
         initial->tested = true;
      }
      next:
      initial = initial->next;
      ++test->count;
      member = member->next;
   }
}

void t_test_local_var( struct task* task, struct var* var ) {
   test_var( task, var, true );
   calc_var_size( var );
   if ( var->initial && var->initial->multi ) {
      calc_var_value_index( var );
   }
}

void test_func( struct task* task, struct func* func, bool undef_err ) {
   if ( func->type == FUNC_USER ) {
      struct param* start = func->params;
      while ( start && start->object.resolved ) {
         start = start->next;
      }
      // Default arguments:
      struct param* param = start;
      while ( param ) {
         if ( param->expr ) {
            struct expr_test expr;
            t_init_expr_test( &expr );
            expr.undef_err = undef_err;
            t_test_expr( task, &expr, param->expr );
            if ( expr.undef_erred ) {
               break;
            }
         }
         // Any previous parameter is visible inside the expression of a
         // default parameter.
         if ( param->name ) {
            if ( param->name->object &&
               param->name->object->node.type == NODE_PARAM ) {               
               struct str str;
               str_init( &str );
               t_copy_name( param->name, false, &str );
               diag_dup( task, str.value, &param->object.pos, param->name );
               t_bail( task );
            }
            param->object.next_scope = param->name->object;
            param->name->object = &param->object;
         }
         param->object.resolved = true;
         param = param->next;
      }
      // Remove parameters from the top scope.
      struct param* stop = param;
      param = start;
      while ( param != stop ) {
         if ( param->name ) {
            param->name->object = param->object.next_scope;
         }
         param = param->next;
      }
      // When stopped at a parameter, that parameter has not been resolved.
      if ( stop ) {
         return;
      }
      // Name:
      //test_unique_name( task, func->name, &func->object.pos );
      struct func_user* impl = func->impl;
      //if ( ! impl->imported || ! impl->hidden ) { 
      //   func->name->object = ( struct object* ) func;
      //}
      func->object.resolved = true;
   }
   else {
      // Default arguments:
      struct param* param = func->params;
      while ( param && param->object.resolved ) {
         param = param->next;
      }
      while ( param ) {
         if ( param->expr ) {
            struct expr_test expr;
            t_init_expr_test( &expr );
            expr.undef_err = undef_err;
            t_test_expr( task, &expr, param->expr );
            if ( expr.undef_erred ) {
               return;
            }
            // NOTE: For now, for a built-in function, a previous parameter
            // is not visible to a following parameter.
         }
         param->object.resolved = true;
         param = param->next;
      }
      // Name:
      // test_unique_name( task, func->name, &func->object.pos );
      func->name->object = ( struct object* ) func;
      func->object.resolved = true;
   }
}

void test_func_body( struct task* task, struct func* func ) {
   t_add_scope( task );
   struct param* param = func->params;
   while ( param ) {
      if ( param->name ) {
         t_use_local_name( task, param->name, ( struct object* ) param );
      }
      param = param->next;
   }
   struct func_user* impl = func->impl;
   struct stmt_test test;
   t_init_stmt_test( &test, NULL );
   test.func = func;
   test.manual_scope = true;
   test.labels = &impl->labels;
   task->in_func = true;
   t_test_block( task, &test, impl->body );
   task->in_func = false;
   t_pop_scope( task );
}

void test_script( struct task* task, struct script* script ) {
   if ( script->number ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, script->number );
      if ( ! script->number->folded ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "script number not a constant expression" );
         t_bail( task );
      }
      if ( script->number->value < SCRIPT_MIN_NUM ||
         script->number->value > SCRIPT_MAX_NUM ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "script number not between %d and %d", SCRIPT_MIN_NUM,
            SCRIPT_MAX_NUM );
         t_bail( task );
      }
      if ( script->number->value == 0 ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "script number 0 not between << and >>" );
         t_bail( task );
      }
   }
   t_add_scope( task );
   struct param* param = script->params;
   while ( param ) {
      if ( param->name ) {
         if ( param->name->object &&
            param->name->object->node.type == NODE_PARAM ) {
            struct str str;
            str_init( &str );
            t_copy_name( param->name, false, &str );
            diag_dup( task, str.value, &param->object.pos, param->name );
            t_bail( task );
         }
         t_use_local_name( task, param->name, &param->object );
      }
      param->object.resolved = true;
      param = param->next;
   }
   struct stmt_test test;
   t_init_stmt_test( &test, NULL );
   test.in_script = true;
   test.manual_scope = true;
   test.labels = &script->labels;
   t_test_stmt( task, &test, script->body );
   t_pop_scope( task );
   script->tested = true;
}

void calc_map_var_size( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         calc_var_size( list_data( &k ) );
         list_next( &k );
      }
      list_next( &i );
   }
}

void calc_var_size( struct var* var ) {
   // Calculate the size of the variable elements.
   if ( var->dim ) {
      calc_dim_size( var->dim, var->type );
   }
   else {
      // Only calculate the size of the type if it hasn't been already.
      if ( ! var->type->size ) {
         calc_type_size( var->type );
      }
   }
   // Calculate the size of the variable.
   if ( var->dim ) {
      var->size = var->dim->size * var->dim->element_size;
   }
   else {
      var->size = var->type->size;
   }
}

void calc_dim_size( struct dim* dim, struct type* type ) {
   if ( dim->next ) {
      calc_dim_size( dim->next, type );
      dim->element_size = dim->next->size * dim->next->element_size;
   }
   else {
      // Calculate the size of the element type.
      if ( ! type->size ) {
         calc_type_size( type );
      }
      dim->element_size = type->size;
   }
}

void calc_type_size( struct type* type ) {
   int offset = 0;
   struct type_member* member = type->member;
   while ( member ) {
      if ( member->dim ) {
         calc_dim_size( member->dim, member->type );
         if ( member->dim->element_size ) {
            int size = member->dim->size * member->dim->element_size;
            type->size += size;
            member->offset = offset;
            offset += size;
         }
      }
      else if ( ! member->type->primitive ) {
         // Calculate the size of the type if it hasn't been already.
         if ( ! member->type->size ) {
            calc_type_size( member->type );
         }
         if ( member->type->size ) {
            type->size += member->type->size;
            member->offset = offset;
            offset += member->type->size;
         }
      }
      else {
         member->size = member->type->size;
         member->offset = offset;
         offset += member->type->size;
         type->size += member->size;
      }
      member = member->next;
   }
}

void calc_map_value_index( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->initial &&
         var->initial->multi ) {
         calc_var_value_index( var );
      }
      list_next( &i );
   }
}

void calc_var_value_index( struct var* var ) {
   struct value_list list;
   list.head = NULL;
   list.tail = NULL;
   make_value_list( &list, ( struct multi_value* ) var->initial );
   var->value = list.head;
   struct value_index_alloc alloc;
   alloc.value = list.head;
   alloc.index = 0;
   if ( var->dim ) {
      alloc_value_index( &alloc,
         ( struct multi_value* ) var->initial,
         var->type, var->dim );
   }
   else {
      alloc_value_index_struct( &alloc,
         ( struct multi_value* ) var->initial, var->type );
   }
}

void make_value_list( struct value_list* list,
   struct multi_value* multi_value ) {
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( initial->multi ) {
         make_value_list( list, ( struct multi_value* ) initial );
      }
      else {
         struct value* value = ( struct value* ) initial;
         if ( list->head ) {
            list->tail->next = value;
         }
         else {
            list->head = value;
         }
         list->tail = value;
      }
      initial = initial->next;
   }
}

void alloc_value_index( struct value_index_alloc* alloc,
   struct multi_value* multi_value, struct type* type, struct dim* dim ) {
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( initial->multi ) {
         if ( dim->next ) {
            int index = alloc->index;
            alloc_value_index( alloc,
               ( struct multi_value* ) initial, type, dim->next );
            // Skip elements not specified.
            int used = alloc->index - index;
            alloc->index += ( dim->next->size *
               dim->next->element_size ) - used;
         }
         else {
            int index = alloc->index;
            alloc_value_index_struct( alloc,
               ( struct multi_value* ) initial, type );
            // Skip members not specified.
            int used = alloc->index - index;
            alloc->index += type->size - used;
         }
      }
      else {
         alloc->value->index = alloc->index;
         alloc->value = alloc->value->next;
         ++alloc->index;
      }
      initial = initial->next;
   }
}

void alloc_value_index_struct( struct value_index_alloc* alloc,
   struct multi_value* multi_value, struct type* type ) {
   struct type_member* member = type->member;
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( initial->multi ) {
         if ( member->dim ) {
            int index = alloc->index;
            alloc_value_index( alloc,
               ( struct multi_value* ) initial,
               member->type, member->dim );
            int used = alloc->index - index;
            alloc->index += ( member->dim->size *
               member->dim->element_size ) - used;
         }
         else {
            int index = alloc->index;
            alloc_value_index_struct( alloc,
               ( struct multi_value* ) initial, member->type );
            int used = alloc->index - index;
            alloc->index += member->type->size - used;
         }
      }
      else {
         alloc->value->index = alloc->index;
         alloc->value = alloc->value->next;
         ++alloc->index;
      }
      member = member->next;
      initial = initial->next;
   }
}

// Counting the usage of strings is done so only strings that are used are
// outputted into the object file. There is no need to output the default
// arguments of the MorphActor() function if it's never called, say.
void count_string_usage( struct task* task ) {
   // Scripts.
   list_iter_t i;
   list_iter_init( &i, &task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      count_string_usage_node( script->body );
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      count_string_usage_node( &impl->body->node );
      struct param* param = func->params;
      while ( param ) {
         if ( param->expr ) {
            count_string_usage_node( &param->expr->node );
         }
         param = param->next;
      }
      list_next( &i );
   }
   // Variables.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      count_string_usage_node( list_data( &i ) );
      list_next( &i );
   }
}

void count_string_usage_node( struct node* node ) {
   if ( node->type == NODE_BLOCK ) {
      struct block* block = ( struct block* ) node;
      list_iter_t i;
      list_iter_init( &i, &block->stmts );
      while ( ! list_end( &i ) ) {
         count_string_usage_node( list_data( &i ) );
         list_next( &i );
      }
   }
   else if ( node->type == NODE_IF ) {
      struct if_stmt* stmt = ( struct if_stmt* ) node;
      count_string_usage_node( &stmt->expr->node );
      count_string_usage_node( stmt->body );
      if ( stmt->else_body ) {
         count_string_usage_node( stmt->else_body );
      }
   }
   else if ( node->type == NODE_SWITCH ) {
      struct switch_stmt* stmt = ( struct switch_stmt* ) node;
      count_string_usage_node( &stmt->expr->node );
      count_string_usage_node( stmt->body );
   }
   else if ( node->type == NODE_CASE ) {
      struct case_label* label = ( struct case_label* ) node;
      count_string_usage_node( &label->expr->node );
   }
   else if ( node->type == NODE_WHILE ) {
      struct while_stmt* stmt = ( struct while_stmt* ) node;
      count_string_usage_node( &stmt->expr->node );
      count_string_usage_node( stmt->body );
   }
   else if ( node->type == NODE_FOR ) {
      struct for_stmt* stmt = ( struct for_stmt* ) node;
      // Initialization.
      if ( list_size( &stmt->vars ) ) {
         list_iter_t i;
         list_iter_init( &i, &stmt->vars );
         while ( ! list_end( &i ) ) {
            count_string_usage_node( list_data( &i ) );
            list_next( &i );
         }
      }
      else {
         struct expr_link* link = stmt->init;
         while ( link ) {
            count_string_usage_node( &link->expr->node );
            link = link->next;
         }
      }
      // Condition.
      if ( stmt->cond ) {
         count_string_usage_node( &stmt->cond->node );
      }
      // Post expression.
      struct expr_link* link = stmt->post;
      while ( link ) {
         count_string_usage_node( &link->expr->node );
         link = link->next;
      }
      // Body.
      count_string_usage_node( stmt->body );
   }
   else if ( node->type == NODE_RETURN ) {
      struct return_stmt* stmt = ( struct return_stmt* ) node;
      if ( stmt->packed_expr ) {
         count_string_usage_node( &stmt->packed_expr->node );
      }
   }
   else if ( node->type == NODE_FORMAT_ITEM ) {
      struct format_item* item = ( struct format_item* ) node;
      while ( item ) {
         count_string_usage_node( &item->expr->node );
         item = item->next;
      }
   }
   else if ( node->type == NODE_VAR ) {
      struct var* var = ( struct var* ) node;
      if ( var->initial ) {
         count_string_usage_initial( var->initial );
      }
   }
   else if ( node->type == NODE_EXPR ) {
      struct expr* expr = ( struct expr* ) node;
      count_string_usage_node( expr->root );
   }
   else if ( node->type == NODE_PACKED_EXPR ) {
      struct packed_expr* packed_expr = ( struct packed_expr* ) node;
      count_string_usage_node( packed_expr->expr->root );
   }
   else if ( node->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) node;
      count_string_usage_node( usage->object );
   }
   else if ( node->type == NODE_CONSTANT ) {
      struct constant* constant = ( struct constant* ) node;
      // Enumerators might have an automatically generated value, so make sure
      // not to process those.
      if ( constant->expr ) {
         count_string_usage_node( &constant->expr->node );
      }
   }
   else if ( node->type == NODE_INDEXED_STRING_USAGE ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) node;
      usage->string->used = true;
   }
   else if ( node->type == NODE_UNARY ) {
      struct unary* unary = ( struct unary* ) node;
      count_string_usage_node( unary->operand );
   }
   else if ( node->type == NODE_CALL ) {
      struct call* call = ( struct call* ) node;
      count_string_usage_node( call->operand );
      list_iter_t i;
      list_iter_init( &i, &call->args );
      // Format arguments:
      if ( call->func->type == FUNC_FORMAT ) {
         while ( ! list_end( &i ) ) {
            struct node* node = list_data( &i );
            if ( node->type == NODE_FORMAT_ITEM ) {
               count_string_usage_node( node );
            }
            else if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
               struct format_block_usage* usage =
                  ( struct format_block_usage* ) node;
               count_string_usage_node( &usage->block->node );
            }
            else {
               break;
            }
            list_next( &i );
         }
      }
      else if ( call->func->type == FUNC_USER ) {
         struct func_user* impl = call->func->impl;
         impl->usage = 1;
      }
      // Arguments:
      struct param* param = call->func->params;
      while ( ! list_end( &i ) ) {
         struct expr* expr = list_data( &i );
         count_string_usage_node( &expr->node );
         if ( param ) {
            param = param->next;
         }
         list_next( &i );
      }
      // Default arguments:
      while ( param ) {
         count_string_usage_node( &param->expr->node );
         param = param->next;
      }
   }
   else if ( node->type == NODE_BINARY ) {
      struct binary* binary = ( struct binary* ) node;
      count_string_usage_node( binary->lside );
      count_string_usage_node( binary->rside );
   }
   else if ( node->type == NODE_ASSIGN ) {
      struct assign* assign = ( struct assign* ) node;
      count_string_usage_node( assign->lside );
      count_string_usage_node( assign->rside );
   }
   else if ( node->type == NODE_ACCESS ) {
      struct access* access = ( struct access* ) node;
      count_string_usage_node( access->lside );
      count_string_usage_node( access->rside );
   }
   else if ( node->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) node;
      count_string_usage_node( paren->inside );
   }
}

void count_string_usage_initial( struct initial* initial ) {
   while ( initial ) {
      if ( initial->multi ) {
         struct multi_value* multi_value = ( struct multi_value* ) initial;
         count_string_usage_initial( multi_value->body );
      }
      else {
         struct value* value = ( struct value* ) initial;
         count_string_usage_node( &value->expr->node );
      }
      initial = initial->next;
   }
}

void diag_dup( struct task* task, const char* text, struct pos* pos,
   struct name* prev ) {
   t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "duplicate name `%s`", text );
   t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &prev->object->pos,
      "name already used here" );
}

void diag_dup_struct( struct task* task, struct name* name,
   struct pos* pos ) {
   struct str str;
   str_init( &str );
   t_copy_name( name, false, &str );
   t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "duplicate struct `%s`", str.value );
   t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &name->object->pos,
      "struct already found here" );
}

void diag_dup_struct_member( struct task* task, struct name* name,
   struct pos* pos ) {
   struct str str;
   str_init( &str );
   t_copy_name( name, false, &str );
   t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "duplicate struct member `%s`", str.value );
   t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &name->object->pos,
      "struct member already found here", str.value );
}

int t_get_script_number( struct script* script ) {
   if ( script->number ) {
      return script->number->value;
   }
   else {
      return 0;
   }
}

void t_add_scope( struct task* task ) {
   struct scope* scope;
   if ( task->free_scope ) {
      scope = task->free_scope;
      task->free_scope = scope->prev;
   }
   else {
      scope = mem_alloc( sizeof( *scope ) );
   }
   scope->prev = task->scope;
   scope->sweep = NULL;
   scope->region_link = task->region->link;
   scope->imports = NULL;
   task->scope = scope;
   ++task->depth;
}

void t_pop_scope( struct task* task ) {
   if ( task->scope->sweep ) {
      struct sweep* sweep = task->scope->sweep;
      while ( sweep ) {
         // Remove names.
         for ( int i = 0; i < sweep->size; ++i ) {
            struct name* name = sweep->names[ i ];
            name->object = name->object->next_scope;
         }
         // Reuse sweep.
         struct sweep* prev = sweep->prev;
         sweep->prev = task->free_sweep;
         task->free_sweep = sweep;
         sweep = prev;
      }
   }
   struct scope* prev = task->scope->prev;
   task->region->link = task->scope->region_link;
   task->scope->prev = task->free_scope;
   task->free_scope = task->scope;
   task->scope = prev;
   --task->depth;
}

void t_use_local_name( struct task* task, struct name* name,
   struct object* object ) {
   struct sweep* sweep = task->scope->sweep;
   if ( ! sweep || sweep->size == SWEEP_MAX_SIZE ) {
      if ( task->free_sweep ) {
         sweep = task->free_sweep;
         task->free_sweep = sweep->prev;
      }
      else {
         sweep = mem_alloc( sizeof( *sweep ) );
      }
      sweep->size = 0;
      sweep->prev = task->scope->sweep;
      task->scope->sweep = sweep;
   }
   sweep->names[ sweep->size ] = name;
   ++sweep->size;
   object->depth = task->depth;
   object->next_scope = name->object;
   name->object = object;
}