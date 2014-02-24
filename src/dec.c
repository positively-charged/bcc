#include "task.h"

#include <stdio.h>

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
   struct module_link* module_links;
   struct import* imports;
};

struct params {
   struct param* node;
   int min;
   int max;
   bool is_script;
};

struct read_initz {
   struct dim* dim;
   struct initz* initz;
   struct initial* initial;
};

struct initz_test {
   struct initz_test* parent;
   struct initz* initz;
   struct dim* dim;
   struct type* type;
   struct type_member* member;
   int num_initials;
   bool undef_err;
   bool undef_erred;
};

struct value_list {
   struct value* head;
   struct value* head_str;
   struct value* value;
   struct value* value_str;
};

struct alloc_value_index {
   struct value* value;
   struct value* value_str;
   int index;
   int index_str;
};

static struct type* new_type( struct task*, struct name* );
static struct name* new_name( void );
static void init_params( struct params* );
static void read_params( struct task*, struct params* );
static void read_enum( struct task*, struct dec*, bool* );
static struct constant* alloc_constant( void );
static void read_struct( struct task*, struct dec*, bool* );
static void add_unresolved_object( struct module*, struct object* );
static void test_script( struct task*, struct script* );
static struct func* new_func( void );
static void read_func( struct task*, struct dec* );
static void read_bfunc( struct task*, struct func* );
static void read_init( struct task*, struct dec* );
static void read_initz( struct task*, struct dec*, struct read_initz* );
static void test_module( struct task*, int*, bool*, bool* );
static void test_type_member( struct task*, struct type_member*, bool );
static struct type* find_type( struct task*, struct path* );
static void test_var( struct task*, struct var*, bool );
static void test_initz( struct task*, struct initz_test* );
static void test_initz_struct( struct task*, struct initz_test* );
static void calc_dim_size( struct dim*, struct type* );
static void test_func( struct task*, struct func*, bool );
static void test_func_body( struct task*, struct func* );
static void calc_type_size( struct type* );
static void calc_map_initial_index( struct task* );
static void count_string_usage( struct task* );
static void count_string_usage_node( struct node* );
static void count_string_usage_initial( struct initial* );
static void calc_map_var_size( struct task* );
static void calc_var_size( struct var* );
static void calc_initial_index( struct var* );
static void link_values_struct( struct value_list*, struct initz*,
   struct type* );
static void add_value_link( struct value_list*, struct type*, struct value* );
static void alloc_value_index( struct alloc_value_index*, struct initz*,
   struct type*, struct dim* );
static void alloc_value_index_struct( struct alloc_value_index*, struct initz*,
   struct type* );
static void diag_dup_struct_member( struct task*, struct name*, struct pos* );

void t_init_fields_dec( struct task* task ) {
   task->depth = 0;
   task->scope = NULL;
   task->free_scope = NULL;
   task->free_sweep = NULL;
   task->root_name = new_name();
   task->anon_name = t_make_name( task, "!anon.", task->root_name );
   str_init( &task->str );
   struct type* type = new_type( task,
      t_make_name( task, "int", task->root_name ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_int = type;
   type = new_type( task,
      t_make_name( task, "str", task->root_name ) );
   type->object.resolved = true;
   type->size_str = 1;
   type->primitive = true;
   type->is_str = true;
   task->type_str = type;
   type = new_type( task,
      t_make_name( task, "bool", task->root_name ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_bool = type;
}

struct name* new_name( void ) {
   struct name* name = mem_alloc( sizeof( *name ) );
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
   struct name* name = start;
   if ( full ) {
      while ( name->ch ) {
         name = name->parent;
         ++length;
      }
   }
   else {
      while ( name->ch && name->ch != '.' ) {
         name = name->parent;
         ++length;
      }
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
   dec->vars = NULL;
   dec->storage = STORAGE_LOCAL;
   dec->storage_index = 0;
   dec->type_needed = false;
   dec->storage_given = false;
   dec->initial_str = false;
   dec->is_static = false;
}

void t_read_dec( struct task* task, struct dec* dec ) {
   dec->pos = task->tk_pos;
   bool func = false;
   if ( task->tk == TK_FUNCTION ) {
      func = true;
      dec->type_needed = true;
      t_read_tk( task );
   }
   // Qualifiers:
   if ( task->tk == TK_STATIC ) {
      STATIC_ASSERT( DEC_TOTAL == 4 )
      if ( dec->area == DEC_FOR ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "`static` in for-loop initialization" );
         t_bail( task );
      }
      if ( dec->area == DEC_MEMBER ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "static struct member" );
         t_bail( task );
      }
      dec->is_static = true;
      dec->type_needed = true;
      t_read_tk( task );
   }
   // Storage:
   if ( task->tk == TK_GLOBAL ) {
      dec->storage = STORAGE_GLOBAL;
      dec->storage_pos = task->tk_pos;
      dec->storage_name = "global";
      dec->storage_given = true;
      dec->type_needed = true;
      t_read_tk( task );
   }
   else if ( task->tk == TK_WORLD ) {
      dec->storage = STORAGE_WORLD;
      dec->storage_pos = task->tk_pos;
      dec->storage_name = "world";
      dec->storage_given = true;
      dec->type_needed = true;
      t_read_tk( task );
   }
   else if ( dec->area == DEC_TOP ||
      ( dec->area == DEC_LOCAL && dec->is_static ) ) {
      dec->storage = STORAGE_MAP;
      dec->storage_name = "map";
   }
   // Storage cannot be specified for a structure member.
   if ( dec->storage_given && dec->area == DEC_MEMBER ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &dec->storage_pos,
         "storage specified for struct member" );
      t_bail( task );
   }
   // Type:
   dec->type_pos = task->tk_pos;
   dec->type_path = NULL;
   bool void_type = false;
   bool struct_type = false;
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
      void_type = true;
      t_read_tk( task );
   }
   else if ( task->tk == TK_ENUM ) {
      bool leave = false;
      read_enum( task, dec, &leave );
      if ( leave ) {
         goto done;
      }
   }
   else if ( task->tk == TK_STRUCT ) {
      bool leave = false;
      read_struct( task, dec, &leave );
      if ( leave ) {
         goto done;
      }
      struct_type = true;
   }
   else {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing type" );
      t_bail( task );
   }
   read_obj:
   // Storage index:
   if ( task->tk == TK_LIT_DECIMAL ) {
      struct pos pos = task->tk_pos;
      if ( dec->area == DEC_MEMBER ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "storage index specified for struct member" );
         t_bail( task );
      }
      dec->storage_index = t_read_literal( task );
      t_test_tk( task, TK_COLON );
      t_read_tk( task );
      int max = MAX_WORLD_LOCATIONS;
      if ( dec->storage != STORAGE_WORLD ) {
         if ( dec->storage == STORAGE_GLOBAL ) {
            max = MAX_GLOBAL_LOCATIONS;
         }
         else  {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "index specified for %s storage", dec->storage_name );
            t_bail( task );
         }
      }
      if ( dec->storage_index >= max ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "index for %s storage not between 0 and %d",
            dec->storage_name, max - 1 );
         t_bail( task );
      }
      else if ( dec->area == DEC_MEMBER ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &dec->storage_pos, "storage specified for a struct member" );
         t_bail( task );
      }
   }
   else {
      // Index must be explicitly specified for these storages.
      if ( dec->storage == STORAGE_WORLD || dec->storage == STORAGE_GLOBAL ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "missing index for %s storage", dec->storage_name );
         t_bail( task );
      }
   }
   // Name:
   if ( task->tk == TK_ID ) {
      dec->name = t_make_name( task, task->tk_text, dec->name_offset );
      dec->name_pos = task->tk_pos;
      t_read_tk( task );
   }
   else {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing name" );
      t_bail( task );
   }
   // Function:
   if ( func || task->tk == TK_PAREN_L ) {
      read_func( task, dec );
      goto done;
   }
   // Variables should have a type.
   if ( void_type ) {
      const char* object = "variable";
      if ( dec->area == DEC_MEMBER ) {
         object = "struct member";
      }
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &dec->type_pos,
         "void type specified for %s", object );
      t_bail( task );
   }
   // Array dimension:
   dec->dim = NULL;
   bool dim_implicit = false;
   struct dim* dim_tail = NULL;
   while ( task->tk == TK_BRACKET_L ) {
      struct pos pos = task->tk_pos;
      t_read_tk( task );
      struct dim* dim = mem_alloc( sizeof( *dim ) );
      dim->next = NULL;
      dim->count = 0;
      dim->count_expr = NULL;
      dim->size = 0;
      dim->size_str = 0;
      // Implicit size.
      if ( task->tk == TK_BRACKET_R ) {
         // Only the first dimension can have an implicit size.
         if ( dim_tail ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "implicit size in subsequent dimension" );
            t_bail( task );
         }
         // Implicit-size dimension not allowed in a struct member.
         else if ( dec->area == DEC_MEMBER ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "dimension with implicit size in struct member" );
            t_bail( task );
         }
         dim_implicit = true;
         t_read_tk( task );
      }
      else {
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         dim->count_expr = expr.node;
         t_test_tk( task, TK_BRACKET_R );
         t_read_tk( task );
      }
      if ( dim_tail ) {
         dim_tail->next = dim;
      }
      else {
         dec->dim = dim;
      }
      dim_tail = dim;
   }
   // Cannot place a structure in the scalar portion of a storage.
   if ( struct_type && ! dec->dim && ( dec->storage == STORAGE_WORLD ||
      dec->storage == STORAGE_GLOBAL ) ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &dec->name_pos,
         "variable of struct type in scalar part of storage" );
      t_bail( task );
   }
   // Initialization:
   dec->initial = NULL;
   if ( task->tk == TK_ASSIGN ) {
      if ( dec->area == DEC_MEMBER ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &task->tk_pos, "initializing a struct member" );
         t_bail( task );
      }
      read_init( task, dec );
   }
   else if ( dim_implicit && ( (
      dec->storage != STORAGE_WORLD &&
      dec->storage != STORAGE_GLOBAL ) || dec->dim->next ) ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing initialization" );
      t_bail( task );
   }
   // Add:
   if ( dec->area == DEC_MEMBER ) {
      struct type_member* member = mem_alloc( sizeof( *member ) );
      t_init_object( &member->object, NODE_TYPE_MEMBER );
      member->object.pos = dec->name_pos;
      member->name = dec->name;
      member->type = dec->type;
      member->type_path = dec->type_path;
      member->dim = dec->dim;
      member->next = NULL;
      member->offset = 0;
      member->offset_str = 0;
      member->size = 0;
      member->size_str = 0;
      if ( dec->type_make->member ) {
         dec->type_make->member_tail->next = member;
         dec->type_make->member_tail = member;
      }
      else {
         dec->type_make->member = member;
         dec->type_make->member_tail = member;
      }
   }
   else {
      struct var* var = mem_alloc( sizeof( *var ) );
      t_init_object( &var->object, NODE_VAR );
      var->object.pos = dec->name_pos;
      var->name = dec->name;
      var->type = dec->type;
      var->type_path = dec->type_path;
      var->dim = dec->dim;
      var->initial = dec->initial;
      var->value = NULL;
      var->value_str = NULL;
      var->storage = dec->storage;
      var->usage = 0;
      var->index = dec->storage_index;
      var->index_str = 0;
      var->size = 0;
      var->size_str = 0;
      var->initial_zero = false;
      var->hidden = false;
      var->shared = false;
      var->shared_str = false;
      var->state_checked = false;
      var->state_changed = false;
      var->has_interface = false;
      var->use_interface = false;
      if ( dec->is_static ) {
         var->hidden = true;
      }
      if ( dec->area == DEC_TOP ) {
         list_append( &task->module->vars, var );
         add_unresolved_object( task->module, &var->object );
      }
      else if ( dec->storage == STORAGE_MAP ) {
         list_append( &task->module->vars, var );
         list_append( dec->vars, var );
      }
      else {
         list_append( dec->vars, var );
      }
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

void read_enum( struct task* task, struct dec* dec, bool* leave ) {
   t_test_tk( task, TK_ENUM );
   struct pos pos = task->tk_pos;
   t_read_tk( task );
   if ( task->tk == TK_BRACE_L || dec->type_needed ) {
      t_test_tk( task, TK_BRACE_L );
      t_read_tk( task );
      if ( task->tk == TK_BRACE_R ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "enum is empty" );
         t_bail( task );
      }
      struct constant* head = NULL;
      struct constant* tail;
      while ( true ) {
         t_test_tk( task, TK_ID );
         struct constant* constant = alloc_constant();
         constant->object.pos = task->tk_pos;
         constant->name = t_make_name( task, task->tk_text,
            task->module->body );
         constant->visible = true;
         t_read_tk( task );
         if ( task->tk == TK_ASSIGN ) {
            t_read_tk( task );
            struct read_expr expr;
            t_init_read_expr( &expr );
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
      t_init_object( &set->object, NODE_CONSTANT_SET );
      set->head = head;
      if ( dec->vars ) {
         list_append( dec->vars, set );
      }
      else {
         add_unresolved_object( task->module, &set->object );
      }
      dec->type = task->type_int;
      if ( ! dec->type_needed ) {
         if ( task->tk == TK_SEMICOLON ) {
            t_read_tk( task );
            *leave = true;
         }
      }
   }
   else {
      t_test_tk( task, TK_ID );
      struct constant* constant = alloc_constant();
      constant->object.pos = task->tk_pos;
      constant->name = t_make_name( task, task->tk_text,
         task->module->body );
      constant->visible = true;
      t_read_tk( task );
      t_test_tk( task, TK_ASSIGN );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      constant->expr = expr.node;
      if ( dec->vars ) {
         list_append( dec->vars, constant );
      }
      else {
         add_unresolved_object( task->module, &constant->object );
      }
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      *leave = true;
   }
   STATIC_ASSERT( DEC_TOTAL == 4 );
   if ( dec->area == DEC_FOR ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
         "enum in for-loop initialization" );
      t_bail( task );
   }
}

struct constant* alloc_constant( void ) {
   struct constant* constant = mem_alloc( sizeof( *constant ) );
   t_init_object( &constant->object, NODE_CONSTANT );
   constant->name = NULL;
   constant->expr = NULL;
   constant->next = NULL;
   constant->value = 0;
   constant->visible = false;
   return constant;
}

void t_init_object( struct object* object, int node_type ) {
   object->node.type = node_type;
   object->resolved = false;
   object->depth = 0;
   object->next = NULL;
   object->next_scope = NULL;
}

void add_unresolved_object( struct module* module, struct object* object ) {
   if ( module->unresolved ) {
      module->unresolved_tail->next = object;
      module->unresolved_tail = object;
   }
   else {
      module->unresolved = object;
      module->unresolved_tail = object;
   }
}

void read_struct( struct task* task, struct dec* dec, bool* leave ) {
   t_test_tk( task, TK_STRUCT );
   struct pos pos = task->tk_pos;
   t_read_tk( task );
   // Variable declaration:
   if ( task->tk != TK_BRACE_L && t_peek( task ) != TK_BRACE_L ) {
      struct path* path = NULL;
      if ( task->tk == TK_MODULE ) {
         t_read_tk( task );
         t_test_tk( task, TK_DOT );
         t_read_tk( task );
         path = mem_alloc( sizeof( *path ) );
         path->text = NULL;
      }
      dec->type_path = t_read_path( task );
      if ( path ) {
         path->next = dec->type_path;
         dec->type_path = path;
      }
   }
   else {
      struct type* type;
      if ( task->tk == TK_ID ) {
         struct name* name = t_make_name( task, task->tk_text,
            task->module->body_struct );
         type = new_type( task, name );
         type->object.pos = pos;
         t_read_tk( task );
      }
      else {
         task->anon_name = t_make_name( task, "a", task->anon_name );
         type = new_type( task, task->anon_name );
         type->object.pos = pos;
         type->anon = true;
      }
      // Members:
      t_test_tk( task, TK_BRACE_L );
      t_read_tk( task );
      if ( task->tk == TK_BRACE_R ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "struct is empty" );
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
      if ( dec->vars ) {
         list_append( dec->vars, type );
      }
      else {
         add_unresolved_object( task->module, &type->object );
      }
      dec->type = type;
      STATIC_ASSERT( DEC_TOTAL == 4 );
      if ( dec->area == DEC_FOR ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "struct in for-loop initialization" );
         t_bail( task );
      }
      // Anonymous struct must be part of a variable declaration.
      if ( ! dec->type_needed && ! type->anon ) {
         if ( task->tk == TK_SEMICOLON ) {
            t_read_tk( task );
            *leave = true;
         }
      }
   }
}

struct type* new_type( struct task* task, struct name* name ) {
   struct type* type = mem_alloc( sizeof( *type ) );
   t_init_object( &type->object, NODE_TYPE );
   type->name = name;
   type->body = t_make_name( task, ".", name );
   type->member = NULL;
   type->member_tail = NULL;
   type->size = 0;
   type->size_str = 0;
   type->primitive = false;
   type->is_str = false;
   type->anon = false;
   return type;
}

void read_init( struct task* task, struct dec* dec ) {
   t_test_tk( task, TK_ASSIGN );
   // At this time, there is no way to initialize an array with world or global
   // storage at namespace scope.
   if ( dec->area == DEC_TOP &&
      ( dec->storage == STORAGE_WORLD || dec->storage == STORAGE_GLOBAL ) ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "initializing %s variable at namespace scope",
         dec->storage_name );
      t_bail( task );
   }
   t_read_tk( task );
   if ( task->tk == TK_BRACE_L ) {
      read_initz( task, dec, NULL );
   }
   else if ( dec->dim ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing initializer" );
      t_bail( task );
   }
   else {
      struct value* value = mem_alloc( sizeof( *value ) );
      value->initial.next = NULL;
      value->initial.is_initz = false;
      value->initial.tested = false;
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      value->expr = expr.node;
      value->next = NULL;
      value->index = 0;
      dec->initial = &value->initial;
      dec->initial_str = expr.has_str;
   }
}

void read_initz( struct task* task, struct dec* dec,
   struct read_initz* parent ) {
   t_test_tk( task, TK_BRACE_L );
   struct initz* initz = mem_alloc( sizeof( *initz ) );
   initz->initial.next = NULL;
   initz->initial.is_initz = true;
   initz->initial.tested = false;
   initz->body = NULL;
   initz->body_curr = NULL;
   initz->pos = task->tk_pos;
   initz->padding = 0;
   initz->padding_str = 0;
   struct read_initz ri;
   ri.initz = initz;
   ri.initial = NULL;
   if ( ! dec->dim && dec->type->primitive ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &task->tk_pos, "using multi-value initializer on single-value "
         "variable" );
      t_bail( task );
   }
   t_read_tk( task );
   if ( task->tk == TK_BRACE_R ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &task->tk_pos, "initializer is empty" );
      t_bail( task );
   }
   while ( true ) {
      if ( task->tk == TK_BRACE_L ) { 
         read_initz( task, dec, &ri );
      }
      else {
         struct value* value = mem_alloc( sizeof( *value ) );
         value->initial.next = NULL;
         value->initial.is_initz = false;
         value->initial.tested = false;
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         value->expr = expr.node;
         value->next = NULL;
         value->index = 0;
         if ( ri.initial ) {
            ri.initial->next = ( struct initial* ) value;
         }
         else {
            initz->body = ( struct initial* ) value;
            initz->body_curr = initz->body;
         }
         ri.initial = ( struct initial* ) value;
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
   if ( parent ) {
      if ( parent->initial ) {
         parent->initial->next = ( struct initial* ) initz;
         parent->initial = ( struct initial* ) initz;
      }
      else {
         parent->initz->body = ( struct initial* ) initz;
         parent->initz->body_curr = parent->initz->body;
         parent->initial = ( struct initial* ) initz;
      }
   }
   else {
      dec->initial = ( struct initial* ) initz;
   }
}

void read_func( struct task* task, struct dec* dec ) {
   if ( dec->area != DEC_TOP ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &dec->pos,
         "nested function" );
      t_bail( task );
   }
   else if ( dec->storage == STORAGE_WORLD ||
      dec->storage == STORAGE_GLOBAL ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &dec->storage_pos, "storage specified for function" );
      t_bail( task );
   }
   else if ( ( dec->type && ! dec->type->primitive ) || dec->type_path ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &dec->type_pos,
         "function returning struct" );
      t_bail( task );
   }
   struct func* func = mem_alloc( sizeof( *func ) );
   t_init_object( &func->object, NODE_FUNC );
   func->object.pos = dec->name_pos;
   func->type = FUNC_ASPEC;
   func->name = NULL;
   func->params = NULL;
   func->min_param = 0;
   func->max_param = 0;
   func->value = NULL;
   func->impl = NULL;
   func->value = dec->type;
   func->name = dec->name;
   func->hidden = false;
   if ( dec->is_static ) {
      func->hidden = true;
   }
   add_unresolved_object( task->module, &func->object );
   // Parameter list:
   t_test_tk( task, TK_PAREN_L );
   struct pos params_pos = task->tk_pos;
   t_read_tk( task );
   // First parameter of a function declaration can be a format parameter.
   struct pos format_pos;
   format_pos.module = -1;
   if ( task->tk == TK_FORMAT ) {
      format_pos = task->tk_pos;
      t_read_tk( task );
   }
   struct params params;
   init_params( &params );
   if ( task->tk != TK_PAREN_R ) {
      if ( format_pos.module != -1 ) {
         t_test_tk( task, TK_COMMA );
         t_read_tk( task );
      }
      read_params( task, &params );
   }
   func->params = params.node;
   func->min_param = params.min;
   func->max_param = params.max;
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   // Body:
   if ( task->tk == TK_BRACE_L ) {
      func->type = FUNC_USER;
      struct func_user* impl = mem_alloc( sizeof( *impl ) );
      list_init( &impl->labels );
      impl->index = 0;
      impl->size = 0;
      impl->usage = 0;
      impl->obj_pos = 0;
      func->impl = impl;
      struct stmt_read stmt_read;
      t_init_stmt_read( &stmt_read );
      stmt_read.labels = &impl->labels;
      t_read_block( task, &stmt_read );
      impl->body = stmt_read.block;
      list_append( &task->module->funcs, func );
      list_append( &task->module->items, func );
   }
   else {
      read_bfunc( task, func );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
   }
   if ( func->type == FUNC_FORMAT ) {
      if ( format_pos.module == -1 ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &params_pos,
            "parameter list missing format parameter" );
         t_bail( task );
      }
      ++func->min_param;
      ++func->max_param;
   }
   else {
      if ( format_pos.module != -1 ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &format_pos,
            "format parameter outside format function" );
         t_bail( task );
      }
   }
}

void init_params( struct params* params ) {
   params->node = NULL;
   params->min = 0;
   params->max = 0;
   params->is_script = false;
} 

void read_params( struct task* task, struct params* params ) {
   if ( task->tk == TK_VOID ) {
      t_read_tk( task );
      return;
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
      if ( params->is_script && type != task->type_int ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &task->tk_pos, "script parameter not of `int` type" );
         t_bail( task );
      }
      struct param* param = mem_alloc( sizeof( *param ) );
      t_init_object( &param->object, NODE_PARAM );
      param->pos = task->tk_pos;
      param->next = NULL;
      param->type = type;
      param->name = NULL;
      param->expr = NULL;
      param->index = 0;
      param->obj_pos = 0;
      ++params->max;
      t_read_tk( task );
      // Name not required for a parameter.
      if ( task->tk == TK_ID ) {
         param->name = t_make_name( task, task->tk_text, task->module->body );
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
         if ( params->is_script ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &param->pos, "default parameter in script" );
            t_bail( task );
         }
      }
      else if ( tail && tail->expr ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param->pos,
            "parameter missing default value" );
         t_bail( task );
      }
      else {
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
}

void read_bfunc( struct task* task, struct func* func ) {
   if ( task->tk == TK_ASSIGN ) {
      t_read_tk( task );
      struct func_aspec* impl = mem_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      impl->id = t_read_literal( task );
      impl->script_callable = false;
      t_test_tk( task, TK_COMMA );
      t_read_tk( task );
      if ( t_read_literal( task ) ) {
         impl->script_callable = true;
      }
      func->impl = impl;
   }
   else if ( task->tk == TK_ASSIGN_SUB ) {
      t_read_tk( task );
      func->type = FUNC_EXT;
      struct func_ext* impl = mem_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      impl->id = t_read_literal( task );
      func->impl = impl;
   }
   else if ( task->tk == TK_ASSIGN_ADD ) {
      t_read_tk( task );
      func->type = FUNC_DED;
      struct func_ded* impl = mem_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      impl->opcode = t_read_literal( task );
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
   else if ( task->tk == TK_ASSIGN_MUL ) {
      t_read_tk( task );
      func->type = FUNC_FORMAT;
      struct func_format* impl = mem_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      impl->opcode = t_read_literal( task );
      func->impl = impl;
   }
   else {
      t_test_tk( task, TK_ASSIGN_DIV );
      t_read_tk( task );
      func->type = FUNC_INTERNAL;
      struct func_intern* impl = mem_alloc( sizeof( *impl ) );
      t_test_tk( task, TK_LIT_DECIMAL );
      struct pos pos = task->tk_pos;
      impl->id = t_read_literal( task );
      if ( impl->id >= INTERN_FUNC_TOTAL ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "no internal function with ID of %d", impl->id );
         t_bail( task );
      }
      func->impl = impl;
   }
}

void t_read_script( struct task* task ) {
   t_test_tk( task, TK_SCRIPT );
   struct script* script = mem_alloc( sizeof( *script ) );
   script->pos = task->tk_pos;
   script->type = SCRIPT_TYPE_CLOSED;
   script->flags = 0;
   script->number = NULL;
   script->params = NULL;
   script->num_param = 0;
   script->offset = 0;
   script->size = 0;
   script->tested = false;
   list_init( &script->labels );
   t_read_tk( task );
   // Script number:
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
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "missing the digit 0" );
         t_bail( task );
      }
   }
   else {
      struct read_expr expr;
      t_init_read_expr( &expr );
      // When reading the script number, the left parenthesis of the
      // parameter list can be mistaken for a function call. Avoid parsing
      // function calls in this area.
      expr.skip_function_call = true;
      t_read_expr( task, &expr );
      script->number = expr.node;
   }
   // Parameter list:
   struct pos param_pos = task->tk_pos;
   if ( task->tk == TK_PAREN_L ) {
      t_read_tk( task );
      struct params params;
      init_params( &params );
      params.is_script = true;
      read_params( task, &params );
      script->params = params.node;
      script->num_param = params.max;
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
   }
   // Script type:
   switch ( task->tk ) {
   case TK_OPEN: script->type = SCRIPT_TYPE_OPEN; break;
   case TK_RESPAWN: script->type = SCRIPT_TYPE_RESPAWN; break;
   case TK_DEATH: script->type = SCRIPT_TYPE_DEATH; break;
   case TK_ENTER: script->type = SCRIPT_TYPE_ENTER; break;
   case TK_PICKUP: script->type = SCRIPT_TYPE_PICKUP; break;
   case TK_BLUE_RETURN: script->type = SCRIPT_TYPE_BLUE_RETURN; break;
   case TK_RED_RETURN: script->type = SCRIPT_TYPE_RED_RETURN; break;
   case TK_WHITE_RETURN: script->type = SCRIPT_TYPE_WHITE_RETURN; break;
   case TK_LIGHTNING: script->type = SCRIPT_TYPE_LIGHTNING; break;
   case TK_DISCONNECT: script->type = SCRIPT_TYPE_DISCONNECT; break;
   case TK_UNLOADING: script->type = SCRIPT_TYPE_UNLOADING; break;
   case TK_RETURN: script->type = SCRIPT_TYPE_RETURN; break;
   default: break;
   }
   if ( script->type == SCRIPT_TYPE_CLOSED ) {
      if ( script->num_param > SCRIPT_MAX_PARAMS ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param_pos,
            "script has over %d parameters", SCRIPT_MAX_PARAMS );
         t_bail( task );
      }
   }
   else if ( script->type == SCRIPT_TYPE_DISCONNECT ) {
      // A disconnect script must have a single parameter. It is the number of
      // the player who disconnected from the server.
      if ( script->num_param < 1 ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param_pos,
            "missing player-number parameter in disconnect script" );
         t_bail( task );
      }
      if ( script->num_param > 1 ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param_pos,
            "too many parameters in disconnect script" );
         t_bail( task );

      }
      t_read_tk( task );
   }
   else {
      if ( script->num_param ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param_pos,
            "parameter list of %s script not empty", task->tk_text );
         t_bail( task );
      }
      t_read_tk( task );
   }
   // Script flags:
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
      if ( ! ( script->flags & flag ) ) {
         script->flags |= flag;
         t_read_tk( task );
      }
      else {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "duplicate %s flag", task->tk_text );
         t_bail( task );
      }
   }
   // Body:
   struct stmt_read stmt;
   t_init_stmt_read( &stmt );
   stmt.labels = &script->labels;
   t_read_block( task, &stmt );
   script->body = stmt.block;
   list_append( &task->module->scripts, script );
   list_append( &task->module->items, script );
}

void t_read_define( struct task* task ) {
   bool visible = false;
   if ( task->tk_text[ 0 ] == 'l' ) {
      visible = true;
   }
   t_read_tk( task );
   t_test_tk( task, TK_ID );
   struct constant* constant = mem_alloc( sizeof( *constant ) );
   t_init_object( &constant->object, NODE_CONSTANT );
   constant->object.pos = task->tk_pos;
   constant->name = t_make_name( task, task->tk_text, task->module->body );
   t_read_tk( task );
   struct read_expr expr;
   t_init_read_expr( &expr );
   t_read_expr( task, &expr );
   constant->expr = expr.node;
   constant->value = 0;
   constant->visible = visible;
   add_unresolved_object( task->module, &constant->object );
}

void t_test( struct task* task ) {
   // Resolve top-scope objects.
   int resolved = 0;
   bool undef_err = false;
   bool retry = false;
   while ( true ) {
      list_iter_t i;
      list_iter_init( &i, &task->loaded_modules );
      while ( ! list_end( &i ) ) {
         task->module = list_data( &i );
         test_module( task, &resolved, &undef_err, &retry );
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
   // Test the body of functions, and scripts.
   list_iter_t i;
   list_iter_init( &i, &task->loaded_modules );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      // Only test those objects that will be output.
      if ( module->publish ) {
         task->module = module;
         list_iter_t k;
         list_iter_init( &k, &module->items );
         while ( ! list_end( &k ) ) {
            struct node* node = list_data( &k );
            if ( node->type == NODE_FUNC ) {
               test_func_body( task, ( struct func* ) node );
            }
            else {
               struct script* script = ( struct script* ) node;
               test_script( task, script );
               list_append( &task->scripts, script );
            }
            list_next( &k );
         }
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
   calc_map_initial_index( task );
   count_string_usage( task );
   task->module = task->module_main;
   task->module = task->module_main;
}

void test_module( struct task* task, int* resolved, bool* undef_err,
   bool* retry ) {
   struct object* object = task->module->unresolved;
   struct object* tail = NULL;
   task->module->unresolved = NULL;
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
            task->module->unresolved = object;
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
            &expr.pos, "expression not constant" );
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
   while ( dim && dim->count ) {
      dim = dim->next;
   }
   while ( dim ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = undef_err;
      t_test_expr( task, &expr, dim->count_expr );
      if ( expr.undef_erred ) {
         return;
      }
      if ( ! dim->count_expr->folded ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
         "array size not a constant expression" );
         t_bail( task );
      }
      if ( dim->count_expr->value <= 0 ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "array size must be greater than 0" );
         t_bail( task );
      }
      dim->count = dim->count_expr->value;
      dim = dim->next;
   }
   member->object.resolved = true;
}

// NOTE: The code here and the code that implements the dot operator in the
// expression subsystem, are very similar. Maybe find a way to merge them.
struct type* find_type( struct task* task, struct path* path ) {
   struct object* object = &task->module->object;
   if ( path->text ) {
      // Try the current module.
      struct name* name = task->module->body;
      if ( ! path->next ) {
         name = task->module->body_struct;
      }
      name = t_make_name( task, path->text, name );
      object = name->object;
      // Try linked modules.
      if ( ! object ) {
         struct module_link* link = task->module->links;
         while ( link ) {
            name = link->module->body;
            if ( ! path->next ) {
               name = link->module->body_struct;
            }
            name = t_make_name( task, path->text, name );
            object = name->object;
            link = link->next;
            if ( object ) {
               break;
            }
         }
         // Error.
         if ( ! object ) {
            const char* msg = "`%s` not found";
            if ( ! path->next ) {
               msg = "struct `%s` not found";
            }
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &path->pos,
               msg, path->text );
            t_bail( task );
         }
         // Make sure no other object with the same name can be found.
         bool dup = false;
         while ( link ) {
            name = link->module->body;
            if ( ! path->next ) {
               name = link->module->body_struct;
            }
            name = t_make_name( task, path->text, name );
            if ( name->object ) {
               const char* type = "object";
               if ( ! path->next ) {
                  type = "struct";
               }
               if ( ! dup ) {
                  t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                     &path->pos, "%s `%s` found in multiple modules", type,
                     path->text );
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
   }
   else {
      path = path->next;
      struct name* name = t_make_name( task, path->text, task->module->body );
      object = t_get_module_object( task, task->module, name );
      if ( ! object ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &path->pos,
            "`%s` not found in module", path->text );
         t_bail( task );
      }
   }
   // Navigate the rest of the path.
   while ( true ) {
      // Follow a shortcut. It needs to refer to an object.
      while ( object->node.type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) object;
         object = alias->target;
      }
      if ( ! path->next ) {
         break;
      }
      path = path->next;
      if ( object->node.type == NODE_MODULE ) {
         struct module* module = ( struct module* ) object;
         struct name* name = module->body;
         if ( ! path->next ) {
            name = module->body_struct;
         }
         name = t_make_name( task, path->text, name );
         object = name->object;
         if ( ! object ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &path->pos,
               "`%s` not found in module", path->text );
            t_bail( task );
         }
      }
      else if ( object->node.type == NODE_PACKAGE ) {
         struct package* package = ( struct package* ) object;
         struct name* name = t_make_name( task, path->text, package->body );
         object = name->object;
         if ( ! object ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &path->pos,
               "`%s` not found in package", path->text );
            t_bail( task );
         }
      }
      else {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &path->pos,
            "accessing something not a module or package" );
         t_bail( task );
      }
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
   while ( dim && dim->count ) {
      dim = dim->next;
   }
   while ( dim ) {
      if ( dim->count_expr ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = undef_err;
         t_test_expr( task, &expr, dim->count_expr );
         if ( expr.undef_erred ) {
            return;
         }
         else if ( ! dim->count_expr->folded ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "array size not a constant expression" );
            t_bail( task );
         }
         else if ( dim->count_expr->value <= 0 ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "array size must be greater than 0" );
            t_bail( task );
         }
         dim->count = dim->count_expr->value;
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
      if ( var->dim ) {
         struct initz_test initz;
         initz.parent = NULL;
         initz.initz = ( struct initz* ) var->initial;
         initz.dim = var->dim;
         initz.type = var->type;
         initz.member = var->type->member;
         initz.num_initials = 0;
         initz.undef_err = undef_err;
         initz.undef_erred = false;
         test_initz( task, &initz );
         if ( initz.undef_erred ) {
            return;
         }
         // Size of implicit dimension.
         if ( ! var->dim->count_expr ) {
            var->dim->count = initz.num_initials;
         }
      }
      else if ( ! var->type->primitive ) {
         struct initz_test initz;
         initz.parent = NULL;
         initz.initz = ( struct initz* ) var->initial;
         initz.dim = NULL;
         initz.type = var->type;
         initz.member = var->type->member;
         initz.num_initials = 0;
         initz.undef_err = undef_err;
         initz.undef_erred = false;
         test_initz_struct( task, &initz );
         if ( initz.undef_erred ) {
            return;
         }
      }
      else {
         struct value* value = ( struct value* ) var->initial;
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = undef_err;
         t_test_expr( task, &expr, value->expr );
         if ( expr.undef_erred ) {
            return;
         }
         if ( var->storage == STORAGE_MAP && ! value->expr->folded ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "initial value not constant" );
            t_bail( task );
         }
         if ( var->type->is_str ) {
            var->value_str = value;
         }
         else {
            var->value = value;
         }
      }
   }
   var->object.resolved = true;
}

void test_initz( struct task* task, struct initz_test* it ) {
   struct initial* initial = it->initz->body;
   while ( initial ) {
      if ( initial->tested ) {
         goto next;
      }
      else if ( it->dim->count && it->num_initials >= it->dim->count ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &it->initz->pos,
            "too many values in initializer for dimension of size %d",
            it->dim->count );
         t_bail( task );
      }
      if ( initial->is_initz ) {
         struct initz* initz = ( struct initz* ) initial;
         if ( it->dim->next ) {
            struct initz_test nested = {
               .parent = it,
               .initz = initz,
               .type = it->type,
               .dim = it->dim->next,
               .undef_err = it->undef_err };
            test_initz( task, &nested );
            if ( nested.undef_erred ) {
               it->undef_erred = true;
               return;
            }
            initial->tested = true;
         }
         else if ( ! it->type->primitive ) {
            struct initz_test nested = {
               .parent = it,
               .initz = initz,
               .type = it->type,
               .undef_err = it->undef_err };
            test_initz_struct( task, &nested );
            if ( nested.undef_erred ) {
               it->undef_erred = true;
               return;
            }
            initial->tested = true;
         }
         else {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &initz->pos, "too many initializers" );
            t_bail( task );
         }
      }
      else {
         struct value* value = ( struct value* ) initial;
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = it->undef_err;
         t_test_expr( task, &expr, value->expr );
         if ( expr.undef_erred ) {
            it->undef_erred = true;
            return;
         }
         else if ( it->dim->next || ! it->type->primitive ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "missing another initializer" );
            t_bail( task );
         }
         else if ( ! value->expr->folded ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "initial value not constant" );
            t_bail( task );
         }
         initial->tested = true;
      }
      next:
      initial = initial->next;
      ++it->num_initials;
   }
}

void test_initz_struct( struct task* task, struct initz_test* it ) {
   struct type_member* member = it->type->member;
   struct initial* initial = it->initz->body;
   while ( initial ) {
      if ( initial->tested ) {
         goto next;
      }
      if ( ! member ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &it->initz->pos, "too many values in initializer" );
         t_bail( task );
      }
      if ( initial->is_initz ) {
         struct initz* initz = ( struct initz* ) initial;
         if ( member->dim ) {
            struct initz_test nested = {
               .parent = it,
               .initz = initz,
               .type = member->type,
               .dim = member->dim,
               .undef_err = it->undef_err };
            test_initz( task, &nested );
            if ( nested.undef_erred ) {
               it->undef_erred = true;
               return;
            }
            initial->tested = true;
         }
         else if ( ! member->type->primitive ) {
            struct initz_test nested = {
               .parent = it,
               .initz = initz,
               .type = member->type,
               .undef_err = it->undef_err };
            test_initz_struct( task, &nested );
            if ( nested.undef_erred ) {
               it->undef_erred = true;
               return;
            }
            initial->tested = true;
         }
         else {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &initz->pos, "too many initializers" );
            t_bail( task );
         }
      }
      else {
         struct value* value = ( struct value* ) initial;
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = it->undef_err;
         t_test_expr( task, &expr, value->expr );
         if ( expr.undef_erred ) {
            it->undef_erred = true;
            return;
         }
         else if ( member->dim || ! member->type->primitive ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &expr.pos, "missing another initializer" );
            t_bail( task );
         }
         else if ( ! value->expr->folded ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "initial value not constant" );
            t_bail( task );
         }
         initial->tested = true;
      }
      next:
      initial = initial->next;
      ++it->num_initials;
      member = member->next;
   }
}

void t_test_local_var( struct task* task, struct var* var ) {
   test_var( task, var, true );
   calc_var_size( var );
   if ( var->initial && var->initial->is_initz ) {
      calc_initial_index( var );
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
               t_copy_name( param->name, false, &task->str );
               diag_dup( task, task->str.value, &param->object.pos, param->name );
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
            t_copy_name( param->name, false, &task->str );
            diag_dup( task, task->str.value, &param->object.pos, param->name );
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
   t_test_block( task, &test, script->body );
   t_pop_scope( task );
   script->tested = true;
}

void calc_map_var_size( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->loaded_modules );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &module->vars );
      while ( ! list_end( &k ) ) {
         calc_var_size( list_data( &k ) );
         list_next( &k );
      }
      list_next( &i );
   }
}

void calc_var_size( struct var* var ) {
   if ( var->dim ) {
      calc_dim_size( var->dim, var->type );
   }
   else if ( ! var->type->primitive && ! var->type->size &&
      ! var->type->size_str ) {
      calc_type_size( var->type );
   }
   if ( var->dim ) {
      var->size = var->dim->size * var->dim->count;
      var->size_str = var->dim->size_str * var->dim->count;
   }
   else {
      var->size = var->type->size;
      var->size_str = var->type->size_str;
   }
}

void calc_dim_size( struct dim* dim, struct type* type ) {
   if ( dim->next ) {
      calc_dim_size( dim->next, type );
      dim->size = dim->next->count * dim->next->size;
      dim->size_str = dim->next->count * dim->next->size_str;
   }
   else {
      // Calculate the size of the element type, if not already done.
      if ( ! type->size && ! type->size_str ) {
         calc_type_size( type );
      }
      dim->size = type->size;
      dim->size_str = type->size_str;
   }
}

void calc_type_size( struct type* type ) {
   int offset = 0;
   int offset_str = 0;
   struct type_member* member = type->member;
   while ( member ) {
      if ( member->dim ) {
         if ( ! member->dim->size && ! member->dim->size_str ) {
            calc_dim_size( member->dim, member->type );
         }
         if ( member->dim->size ) {
            int size = member->dim->size * member->dim->count;
            type->size += size;
            member->offset = offset;
            offset += size;
         }
         if ( member->dim->size_str ) {
            int size_str = member->dim->size_str * member->dim->count;
            type->size_str += size_str;
            member->offset_str = offset_str;
            offset_str += size_str;
         }
      }
      else if ( ! member->type->primitive ) {
         if ( ! member->type->size && ! member->type->size_str ) {
            calc_type_size( member->type );
         }
         if ( member->type->size ) {
            type->size += member->type->size;
            member->offset = offset;
            offset += member->type->size;
         }
         if ( member->type->size_str ) {
            type->size_str += member->type->size_str;
            member->offset_str = offset_str;
            offset_str += member->type->size_str;
         }
      }
      else if ( member->type->is_str ) {
         member->size_str = member->type->size_str;
         member->offset_str = offset_str;
         offset_str += member->type->size_str;
         type->size_str += member->type->size_str;
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

void link_values( struct value_list* list, struct initz* initz,
   struct type* type, struct dim* dim ) {
   struct initial* initial = initz->body;
   while ( initial ) {
      if ( initial->is_initz ) {
         if ( dim->next ) {
            link_values( list, ( struct initz* ) initial, type, dim->next );
         }
         else {
            link_values_struct( list, ( struct initz* ) initial, type );
         }
      }
      else {
         add_value_link( list, type, ( struct value* ) initial );
      }
      initial = initial->next;
   }
}

void link_values_struct( struct value_list* list, struct initz* initz,
   struct type* type ) {
   struct type_member* member = type->member;
   struct initial* initial = initz->body;
   while ( initial ) {
      if ( initial->is_initz ) {
         if ( member->dim ) {
            link_values( list, ( struct initz* ) initial, member->type,
               member->dim );
         }
         else {
            link_values_struct( list, ( struct initz* ) initial,
               member->type );
         }
      }
      else {
         add_value_link( list, member->type, ( struct value* ) initial );
      }
      member = member->next;
      initial = initial->next;
   }
}

void add_value_link( struct value_list* list, struct type* type,
   struct value* value ) {
   if ( type->is_str ) {
      if ( list->head_str ) {
         list->value_str->next = value;
      }
      else {
         list->head_str = value;
      }
      list->value_str = value;
   }
   else {
      if ( list->head ) {
         list->value->next = value;
      }
      else {
         list->head = value;
      }
      list->value = value;
   }
}

void calc_map_initial_index( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->loaded_modules );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      if ( module->publish ) {
         list_iter_t k;
         list_iter_init( &k, &module->vars );
         while ( ! list_end( &k ) ) {
            struct var* var = list_data( &k );
            if ( var->storage == STORAGE_MAP && var->initial &&
               var->initial->is_initz ) {
               calc_initial_index( var );
            }
            list_next( &k );
         }
      }
      list_next( &i );
   }
}

void calc_initial_index( struct var* var ) {
   struct value_list list;
   list.head = NULL;
   list.head_str = NULL;
   list.value = NULL;
   list.value_str = NULL;
   if ( var->dim ) {
      link_values( &list, ( struct initz* ) var->initial, var->type,
         var->dim );
   }
   else {
      link_values_struct( &list, ( struct initz* ) var->initial,
         var->type );
   }
   var->value = list.head;
   var->value_str = list.head_str;
   struct alloc_value_index alloc;
   alloc.value = list.head;
   alloc.value_str = list.head_str;
   alloc.index = 0;
   alloc.index_str = 0;
   if ( var->dim ) {
      alloc_value_index( &alloc, ( struct initz* ) var->initial,
         var->type, var->dim );
   }
   else {
      alloc_value_index_struct( &alloc, ( struct initz* ) var->initial,
         var->type );
   }
}

void alloc_value_index( struct alloc_value_index* alloc, struct initz* initz,
   struct type* type, struct dim* dim ) {
   int count = 0;
   struct initial* initial = initz->body;
   while ( initial ) {
      if ( initial->is_initz ) {
         if ( dim->next ) {
            alloc_value_index( alloc, ( struct initz* ) initial, type,
               dim->next );
         }
         else {
            alloc_value_index_struct( alloc, ( struct initz* ) initial, type );
         }
      }
      else if ( type->is_str ) {
         alloc->value_str->index = alloc->index_str;
         alloc->value_str = alloc->value_str->next;
         ++alloc->index_str;
      }
      else {
         alloc->value->index = alloc->index;
         alloc->value = alloc->value->next;
         ++alloc->index;
      }
      ++count;
      initial = initial->next;
   }
   // Skip past the elements not specified.
   alloc->index += ( dim->count - count ) * dim->size;
   alloc->index_str += ( dim->count - count ) * dim->size_str;
}

void alloc_value_index_struct( struct alloc_value_index* alloc,
   struct initz* initz, struct type* type ) {
   int size = 0;
   int size_str = 0;
   struct type_member* member = type->member;
   struct initial* initial = initz->body;
   while ( initial ) {
      if ( initial->is_initz ) {
         if ( member->dim ) {
            alloc_value_index( alloc, ( struct initz* ) initial,
               member->type, member->dim );
         }
         else {
            alloc_value_index_struct( alloc, ( struct initz* ) initial,
               member->type );
         }
      }
      else if ( member->type->is_str ) {
         alloc->value_str->index = alloc->index_str;
         alloc->value_str = alloc->value_str->next;
         ++alloc->index_str;
         ++size_str;
      }
      else {
         alloc->value->index = alloc->index;
         alloc->value = alloc->value->next;
         ++alloc->index;
         ++size;
      }
      member = member->next;
      initial = initial->next;
   }
   // Skip past member data that was not specified. 
   alloc->index += type->size - size;
   alloc->index_str += type->size_str - size_str;
}

// Counting the usage of strings is done so only strings that are used are
// outputted into the object file. There is no need to output the default
// arguments of the MorphActor() function if it's never called, say.
void count_string_usage( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->loaded_modules );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      if ( module->publish ) {
         count_string_usage_node( &module->object.node );
      }
      list_next( &i );
   }
}

void count_string_usage_node( struct node* node ) {
   if ( node->type == NODE_MODULE ) {
      struct module* module = ( struct module* ) node;
      // Scripts.
      list_iter_t i;
      list_iter_init( &i, &module->scripts );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         count_string_usage_node( &script->body->node );
         list_next( &i );
      }
      // Functions.
      list_iter_init( &i, &module->funcs );
      while ( ! list_end( &i ) ) {
         struct func* func = list_data( &i );
         struct func_user* impl = func->impl;
         count_string_usage_node( &impl->body->node );
         list_next( &i );
      }
      // Variables.
      list_iter_init( &i, &module->vars );
      while ( ! list_end( &i ) ) {
         count_string_usage_node( list_data( &i ) );
         list_next( &i );
      }
   }
   else if ( node->type == NODE_BLOCK ) {
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
      if ( stmt->expr ) {
         count_string_usage_node( &stmt->expr->node );
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
      count_string_usage_node( &stmt->expr->node );
   }
   else if ( node->type == NODE_FORMAT_ITEM ) {
      struct format_item* item = ( struct format_item* ) node;
      count_string_usage_node( &item->expr->node );
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
   else if ( node->type == NODE_FORMAT_EXPR ) {
      struct format_expr* expr = ( struct format_expr* ) node;
      count_string_usage_node( &expr->expr->node );
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
      ++usage->string->usage;
   }
   else if ( node->type == NODE_UNARY ) {
      struct unary* unary = ( struct unary* ) node;
      count_string_usage_node( unary->operand );
   }
   else if ( node->type == NODE_CALL ) {
      struct call* call = ( struct call* ) node;
      list_iter_t i;
      list_iter_init( &i, &call->args );
      // Format arguments:
      if ( call->func->type == FUNC_FORMAT ) {
         while ( ! list_end( &i ) ) {
            struct node* node = list_data( &i );
            if ( node->type == NODE_FORMAT_ITEM ) {
               count_string_usage_node( node );
            }
            else if ( node->type == NODE_FORMAT_BLOCK_ARG ) {
               struct format_block_arg* arg =
                  ( struct format_block_arg* ) node;
               count_string_usage_node( &arg->format_block->node );
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
         param = param->next;
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
      count_string_usage_node( access->rside );
   }
   else if ( node->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) node;
      count_string_usage_node( paren->inside );
   }
}

void count_string_usage_initial( struct initial* initial ) {
   while ( initial ) {
      if ( initial->is_initz ) {
         struct initz* initz = ( struct initz* ) initial;
         count_string_usage_initial( initz->body );
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
   scope->module_links = task->module->links;
   scope->imports = NULL;
   task->scope = scope;
   ++task->depth;
}

void t_add_scope_import( struct task* task, struct import* stmt ) {
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
   task->module->links = task->scope->module_links;
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