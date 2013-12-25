#include <string.h>
#include <limits.h>

#include "task.h"

#include <stdio.h>

#define MAX_MAP_LOCATIONS 128
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
   struct ns_link* ns_link;
   int size;
   int size_high;
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

struct resolve {
   struct ns* ns;
   struct var* var;
   int index_int;
   int index_str;
   int count;
   bool undef;
   bool do_err;
   bool retry;
};

struct path_test {
   struct path* path;
   struct name* name;
   bool do_struct;
};

struct initz_test {
   struct initz_test* parent;
   struct initz* initz;
   struct dim* dim;
   struct type* type;
   struct type_member* member;
   int num_initials;
   bool err;
   bool erred;
};

struct test_var {
   struct var* var;
   struct ns* ns;
   bool do_err;
   bool local;
};

static struct ns* new_ns( struct task*, struct name* );
static struct name* new_name( void );
static void use_local_name( struct task*, struct name*, struct object* );
static struct type* new_type( struct task*, struct name* );
static void diag_dup( const char*, struct pos*, struct name* );
static void init_params( struct params* );
static void read_params( struct task*, struct params* );
static void read_enum( struct task*, struct dec*, bool* );
static void read_struct( struct task*, struct dec* );
static void init_object( struct object*, int );
static void add_ns_link( struct ns*, struct ns_link* );
static void add_unresolved( struct ns*, struct object* );
static struct path* read_path( struct task* );
static void test_script( struct task*, struct script* );
static struct func* new_func( void );
static void read_func( struct task*, struct dec* );
static void read_bfunc( struct task*, struct func* );
static char* save_name( struct name*, char*, int );
static void read_init( struct task*, struct dec* );
static void read_initz( struct task*, struct dec*, struct read_initz* );
static void show_unresolved( void );
static void test_ns( struct task*, struct resolve*, struct ns* );
static void init_path_test( struct path_test*, struct path* );
static void test_path( struct task*, struct path_test* );
static void test_type_member( struct task*, struct type_member*, bool );
static void test_var( struct task*, struct test_var* );
static void test_init( struct task*, struct var*, bool, bool* );
static void init_initz_test( struct initz_test* );
static void test_initz( struct task*, struct initz_test* );
static void test_initz_struct( struct task*, struct initz_test* );
static void calc_dim_size( struct dim*, struct type* );
static void test_func( struct task*, struct resolve*, struct func* );
static void test_func_body( struct task*, struct func* );
static void alloc_index( struct task* );
static void calc_type_size( struct type* );
static void test_unique_name( struct task*, struct name*, struct pos* );
static void alloc_string_index( struct task* );
static void calc_initials_index( struct task* );
static void count_string_usage( struct task* );
static void find_string_usage_node( struct node* );
static void count_string_usage_node( struct node* );
static bool find_integer_array( struct task*, struct type*, struct dim* );

void t_init_fields_dec( struct task* task ) {
   task->depth = 0;
   task->scope = NULL;
   task->free_scope = NULL;
   task->free_sweep = NULL;
   struct ns* ns = new_ns( task, new_name() );
   ns->name->object = &ns->object;
   task->ns = ns;
   task->ns_global = ns;
   d_list_init( &task->nss );
   d_list_append( &task->nss, ns );
   task->anon_name = t_make_name( task, "!anon.", ns->body );
   str_init( &task->str );
   struct type* type = new_type( task,
      t_make_name( task, "int", ns->body ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_int = type;
   type = new_type( task,
      t_make_name( task, "str", ns->body ) );
   type->object.resolved = true;
   type->size_str = 1;
   type->primitive = true;
   type->is_str = true;
   task->type_str = type;
   type = new_type( task,
      t_make_name( task, "bool", ns->body ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_bool = type;
   type = new_type( task,
      t_make_name( task, "fixed", ns->body ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_fixed = type; 
   task->space_size = 0;
   task->space_size_str = 0;
   task->in_func = false;
}

struct name* new_name( void ) {
   struct name* name = mem_alloc( sizeof( *name ) );
   name->parent = NULL;
   name->next = NULL;
   name->drop = NULL;
   name->ch = 0;
   name->object = NULL;
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

void t_copy_name( struct name* start, struct str* str, char ch ) {
   int i = 0;
   struct name* name = start;
   while ( name && name->ch != ch ) {
      name = name->parent;
      ++i;
   }
   if ( str->buff_length < i + 1 ) {
      str_grow( str, i );
   }
   str->length = i;
   str->value[ i ] = 0;
   name = start;
   while ( i > 0 ) {
      --i;
      str->value[ i ] = name->ch;
      name = name->parent;
   }
}

void t_copy_full_name( struct name* start, struct str* str ) {
   int i = 0;
   struct name* name = start;
   // The dot of the unnamed, global namespace is not included.
   while ( name->ch != '.' || name->parent->ch ) {
      name = name->parent;
      ++i;
   }
   if ( str->buff_length < i + 1 ) {
      str_grow( str, i );
   }
   str->length = i;
   str->value[ i ] = 0;
   name = start;
   while ( i > 0 ) {
      --i;
      str->value[ i ] = name->ch;
      name = name->parent;
   }
}

int t_full_name_length( struct name* name ) {
   int i = 0;
   // The dot of the unnamed, global namespace is not included.
   while ( name->ch != '.' || name->parent->ch ) {
      name = name->parent;
      ++i;
   }
   return i;
}

void t_read_namespace( struct task* task ) {
   t_test_tk( task, TK_NAMESPACE );
   t_read_tk( task );
   task->ns = task->ns_global;
   if ( task->tk == TK_ID ) {
      while ( true ) {
         struct name* name = t_make_name( task, task->tk_text,
            task->ns->body );
         if ( name->object ) {
            // NOTE: When reading the source code to build the tree, the only
            // objects visible should be namespaces.
            task->ns = ( struct ns* ) name->object;
         }
         else {
            struct ns* ns = new_ns( task, name );
            ns->object.pos = task->tk_pos;
            ns->object.resolved = true;
            name->object = ( struct object* ) ns;
            d_list_append( &task->nss, ns );
            task->ns = ns;
         }
         t_read_tk( task );
         if ( task->tk == TK_DOT ) {
            t_read_tk( task );
            t_test_tk( task, TK_ID );
         }
         else {
            break;
         }
      }
   }
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
}

struct ns* new_ns( struct task* task, struct name* name ) {
   struct ns* ns = mem_alloc( sizeof( *ns ) );
   init_object( &ns->object, NODE_NAMESPACE );
   ns->name = name;
   ns->body = t_make_name( task, ".", name );
   ns->body_struct = t_make_name( task, ".struct.", name );
   list_init( &ns->items );
   ns->ns_link = NULL;
   ns->unresolved = NULL;
   ns->unresolved_tail = NULL;
   return ns;
}

void init_object( struct object* object, int node_type ) {
   object->node.type = node_type;
   object->resolved = false;
   object->depth = 0;
   object->link = NULL;
}

void t_read_using( struct task* task, struct node** local ) {
   t_test_tk( task, TK_USING );
   struct pos pos = task->tk_pos;
   t_read_tk( task );
   if ( task->tk == TK_NAMESPACE ) {
      t_read_tk( task );
      struct ns_link* link = mem_alloc( sizeof( *link ) );
      init_object( &link->object, NODE_NAMESPACE_LINK );
      link->object.pos = pos;
      link->path = read_path( task );
      link->ns = NULL;
      link->next = NULL;
      if ( local ) {
         *local = &link->object.node;
      }
      else {
         add_ns_link( task->ns, link );
         add_unresolved( task->ns, &link->object );
      }
   }
   else {
      struct shortcut* shortcut = mem_alloc( sizeof( *shortcut ) );
      init_object( &shortcut->object, NODE_SHORTCUT );
      shortcut->path = NULL;
      shortcut->alias = NULL;
      shortcut->target = NULL;
      shortcut->import_struct = false;
      // Alias to the global namespace:
      if ( task->tk == TK_COLON ) {
         t_read_tk( task );
         t_test_tk( task, TK_ID );
         shortcut->alias = t_make_name( task, task->tk_text, task->ns->body );
         shortcut->object.pos = task->tk_pos;
         t_read_tk( task );
      }
      else {
         struct name* offset = task->ns->body;
         if ( task->tk == TK_STRUCT ) {
            shortcut->import_struct = true;
            offset = task->ns->body_struct;
            t_read_tk( task );
         }
         shortcut->path = read_path( task );
         // Custom alias to the referenced object:
         if ( task->tk == TK_COLON ) {
            t_read_tk( task );
            t_test_tk( task, TK_ID );
            shortcut->alias = t_make_name( task, task->tk_text, offset );
            shortcut->object.pos = task->tk_pos;
            t_read_tk( task );
         }
         else {
            struct path* path = shortcut->path;
            while ( path->next ) {
               path = path->next;
            }
            shortcut->alias = t_make_name( task, path->text, offset );
            shortcut->object.pos = path->pos;
         }
      }
      if ( local ) {
         *local = &shortcut->object.node;
      }
      else {
         add_unresolved( task->ns, &shortcut->object );
      }
   }
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
}

struct path* read_path( struct task* task ) {
   struct path* head = NULL;
   struct path* tail;
   while ( true ) {
      t_test_tk( task, TK_ID );
      struct path* path = mem_alloc( sizeof( *path ) );
      path->text = task->tk_text;
      path->pos = task->tk_pos;
      path->next = NULL;
      if ( head ) {
         tail->next = path;
      }
      else {
         head = path;
      }
      tail = path;
      t_read_tk( task );
      if ( task->tk == TK_DOT ) {
         t_read_tk( task );
      }
      else {
         break;
      }
   }
   return head; 
}

void add_ns_link( struct ns* ns, struct ns_link* link ) {
   // When searching for a name using namespace links, the link that is
   // parsed last will be the first link used in the search, then the
   // second-last, all the way to the first one parsed.
   link->next = ns->ns_link;
   ns->ns_link = link;
}

void add_unresolved( struct ns* ns, struct object* object ) {
   if ( ns->unresolved ) {
      ns->unresolved_tail->link = object;
   }
   else {
      ns->unresolved = object;
   }
   ns->unresolved_tail = object;
}

bool t_is_dec( struct task* task ) {
   switch ( task->tk ) {
   case TK_INT:
   case TK_STR:
   case TK_BOOL:
   case TK_FIXED:
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
   dec->type = NULL;
   dec->vars = NULL;
   dec->storage = STORAGE_LOCAL;
   dec->storage_index = 0;
   dec->storage_name = "local";
   dec->type_needed = false;
   dec->storage_given = false;
   dec->initial_str = false;
}

void t_read_dec( struct task* task, struct dec* dec ) {
   dec->pos = task->tk_pos;
   bool func = false;
   if ( task->tk == TK_FUNCTION ) {
      if ( dec->area != DEC_TOP ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &dec->pos,
            "nested function" );
         bail();
      }
      func = true;
      dec->type_needed = true;
      t_read_tk( task );
   }
   // Static:
   bool is_static = false;
   if ( task->tk == TK_STATIC ) {
      STATIC_ASSERT( DEC_TOTAL == 4 )
      if ( dec->area == DEC_TOP ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "`static` in top scope" );
         bail();
      }
      if ( dec->area == DEC_FOR ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "`static` in for-loop initialization" );
         bail();
      }
      if ( dec->area == DEC_MEMBER ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "`static` in struct member declaration" );
         bail();
      }
      is_static = true;
      t_read_tk( task );
   }
   // Storage:
   if ( task->tk == TK_GLOBAL ) {
      dec->storage = STORAGE_GLOBAL;
      dec->storage_pos = task->tk_pos;
      dec->storage_name = "global";
      dec->storage_given = true;
      t_read_tk( task );
   }
   else if ( task->tk == TK_WORLD ) {
      dec->storage = STORAGE_WORLD;
      dec->storage_pos = task->tk_pos;
      dec->storage_name = "world";
      dec->storage_given = true;
      t_read_tk( task );
   }
   else if ( is_static ) {
      if ( dec->area == DEC_LOCAL ) {
         dec->storage = STORAGE_MAP;
         dec->storage_pos = task->tk_pos;
         dec->storage_name = "map";
      }
   }
   else if ( dec->area == DEC_TOP ) {
      dec->storage = STORAGE_MAP;
      dec->storage_name = "map";
   }
   // Type:
   dec->type_path = NULL;
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
   else if ( task->tk == TK_FIXED ) {
      dec->type = task->type_fixed;
      t_read_tk( task );
   }
   else if ( task->tk == TK_ENUM ) {
      bool leave = false;
      read_enum( task, dec, &leave );
      if ( leave ) {
         return;
      }
   }
   else if ( task->tk == TK_STRUCT ) {
      read_struct( task, dec );
      if ( ! dec->type_needed && task->tk == TK_SEMICOLON ) {
         t_read_tk( task );
         return;
      }
   }
   else if ( task->tk == TK_VOID ) {
      t_read_tk( task );
   }
   else {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing type in declaration" );
      bail();
   }
   read_obj:
   // Storage index:
   if ( task->tk == TK_LIT_DECIMAL ) {
      if ( dec->area == DEC_MEMBER ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "index specified for a struct member" );
         bail();
      }
      struct pos pos = task->tk_pos;
      dec->storage_index = t_read_literal( task );
      t_test_tk( task, TK_COLON );
      t_read_tk( task );
      int max = MAX_WORLD_LOCATIONS;
      if ( dec->storage != STORAGE_WORLD ) {
         if ( dec->storage == STORAGE_GLOBAL ) {
            max = MAX_GLOBAL_LOCATIONS;
         }
         else  {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "index specified for %s storage", dec->storage_name );
            bail();
         }
      }
      if ( dec->storage_index >= max ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "index for %s storage not between 0 and %d",
            dec->storage_name, max - 1 );
         bail();
      }
      else if ( dec->area == DEC_MEMBER ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &dec->storage_pos, "storage specified for a struct member" );
         bail();
      }
   }
   else {
      // Index must be explicitly specified for these storages.
      if ( dec->storage == STORAGE_WORLD || dec->storage == STORAGE_GLOBAL ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "missing index for %s storage", dec->storage_name );
         bail();
      }
   }
   // Name:
   if ( task->tk == TK_ID ) {
      dec->name = t_make_name( task, task->tk_text, dec->name_offset );
      dec->name_pos = task->tk_pos;
      t_read_tk( task );
   }
   else {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing name in declaration" );
      bail();
   }
   // Function:
   if ( func ) {
      read_func( task, dec );
      return;
   }
   // Array dimension:
   dec->dim = NULL;
   bool dim_implicit = false;
   struct dim* dim_tail = NULL;
   while ( task->tk == TK_BRACKET_L ) {
      struct pos pos = task->tk_pos;
      struct dim* dim = mem_alloc( sizeof( *dim ) );
      dim->next = NULL;
      dim->count = 0;
      dim->count_expr = NULL;
      dim->size = 0;
      dim->size_str = 0;
      t_read_tk( task );
      // Implicit size.
      if ( task->tk == TK_BRACKET_R ) {
         // Only the first dimension can have an implicit size.
         if ( dim_tail ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "implicit size in subsequent dimension" );
            bail();
         }
         // Implicit-size dimension not allowed in a struct member.
         else if ( dec->area == DEC_MEMBER ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "dimension with implicit size in struct member" );
            bail();
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
   // Initialization:
   dec->initial = NULL;
   if ( task->importing ) {
      t_skip_to_tk( task, TK_SEMICOLON );
   }
   else {
      if ( task->tk == TK_ASSIGN ) {
         if ( dec->area == DEC_MEMBER ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &task->tk_pos, "initializing a struct member" );
            bail();
         }
         read_init( task, dec );
      }
      else if ( dim_implicit && ( (
         dec->storage != STORAGE_WORLD &&
         dec->storage != STORAGE_GLOBAL ) || dec->dim->next ) ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "missing initialization" );
         bail();
      }
   }
   // Add:
   if ( dec->area == DEC_MEMBER ) {
      struct type_member* member = mem_alloc( sizeof( *member ) );
      init_object( &member->object, NODE_TYPE_MEMBER );
      member->object.pos = dec->name_pos;
      member->next = NULL;
      member->name = dec->name;
      member->type = dec->type;
      member->dim = dec->dim;
      member->type_path = dec->type_path;
      member->offset = 0;
      member->offset_str = 0;
      if ( dec->new_type->member ) {
         dec->new_type->member_tail->next = member;
         dec->new_type->member_tail = member;
      }
      else {
         dec->new_type->member = member;
         dec->new_type->member_tail = member;
         dec->new_type->member_curr = member;
      }
   }
   else {
      struct var* var = mem_alloc( sizeof( *var ) );
      init_object( &var->object, NODE_VAR );
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
      var->imported = false;
      var->hidden = false;
      var->shared = false;
      var->printer = 0;
      var->printer_offset = 0;
      var->flags = 0;
      if ( is_static ) {
         var->hidden = true;
      }
      if ( dec->area == DEC_TOP ) {
         if ( task->importing ) {
            var->imported = true;
         }
         list_append( &task->module->vars, var );
         add_unresolved( task->ns, ( struct object* ) var );
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
}

void read_enum( struct task* task, struct dec* dec, bool* leave ) {
   t_test_tk( task, TK_ENUM );
   struct pos pos = task->tk_pos;
   t_read_tk( task );
   if ( task->tk == TK_BRACE_L ) {
      t_read_tk( task );
      struct constant* head = NULL;
      struct constant* tail;
      while ( true ) {
         t_test_tk( task, TK_ID );
         struct constant* constant = mem_alloc( sizeof( *constant ) );
         init_object( &constant->object, NODE_CONSTANT );
         constant->object.pos = task->tk_pos;
         constant->name = t_make_name( task, task->tk_text, task->ns->body );
         constant->expr = NULL;
         constant->next = NULL;
         constant->value = 0;
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
      init_object( &set->object, NODE_CONSTANT_SET );
      set->head = head;
      if ( dec->vars ) {
         list_append( dec->vars, set );
      }
      else {
         add_unresolved( task->ns, ( struct object* ) set );
      }
      if ( dec->type_needed || task->tk != TK_SEMICOLON ) {
         dec->type = task->type_int;
      }
      else {
         t_read_tk( task );
         *leave = true;
      }
   }
   else {
      t_test_tk( task, TK_ID );
      struct constant* constant = mem_alloc( sizeof( *constant ) );
      init_object( &constant->object, NODE_CONSTANT );
      constant->object.pos = task->tk_pos;
      constant->name = t_make_name( task, task->tk_text, task->ns->body );
      constant->next = NULL;
      t_read_tk( task );
      t_test_tk( task, TK_ASSIGN );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      constant->expr = expr.node;
      constant->value = 0;
      if ( dec->area == DEC_TOP ) {
         add_unresolved( task->ns, ( struct object* ) constant );
      }
      else {
         list_append( dec->vars, constant );
      }
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      *leave = true;
   }
   STATIC_ASSERT( DEC_TOTAL == 4 );
   if ( dec->area == DEC_FOR ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
         "enum declaration in for-loop initialization" );
      bail();
   }
}

void read_struct( struct task* task, struct dec* dec ) {
   t_test_tk( task, TK_STRUCT );
   struct pos pos = task->tk_pos;
   t_read_tk( task );
   // Variable declaration:
   if ( task->tk == TK_ID && t_peek_usable_ch( task ) != '{' ) {
      dec->type_path = read_path( task );
   }
   else {
      struct type* type;
      if ( task->tk == TK_ID ) {
         struct name* name = t_make_name( task, task->tk_text,
            task->ns->body_struct );
         type = new_type( task, name );
         type->object.pos = task->tk_pos;
         t_read_tk( task );
      }
      else {
         task->anon_name = t_make_name( task, "a", task->anon_name );
         type = new_type( task, task->anon_name );
         type->anon = true;
      }
      // Members:
      t_test_tk( task, TK_BRACE_L );
      t_read_tk( task );
      while ( true ) {
         struct dec member;
         t_init_dec( &member );
         member.area = DEC_MEMBER;
         member.new_type = type;
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
         add_unresolved( task->ns, &type->object );
      }
      dec->type = type;
      STATIC_ASSERT( DEC_TOTAL == 4 );
      if ( dec->area == DEC_FOR ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "struct declaration in for-loop initialization" );
         bail();
      }
   }
}

struct type* new_type( struct task* task, struct name* name ) {
   struct type* type = mem_alloc( sizeof( *type ) );
   init_object( &type->object, NODE_TYPE );
   type->name = name;
   type->body = t_make_name( task, ".", name );
   type->member = NULL;
   type->member_tail = NULL;
   type->member_curr = NULL;
   type->size = 0;
   type->size_str = 0;
   type->primitive = false;
   type->is_str = false;
   type->undef = NULL;
   type->anon = false;
   return type;
}

void read_init( struct task* task, struct dec* dec ) {
   t_test_tk( task, TK_ASSIGN );
   // At this time, there is no way to initialize an array with world or global
   // storage at namespace scope.
   if ( dec->area == DEC_TOP &&
      ( dec->storage == STORAGE_WORLD || dec->storage == STORAGE_GLOBAL ) ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "initializing %s variable at namespace scope",
         dec->storage_name );
      bail();
   }
   t_read_tk( task );
   if ( task->tk == TK_BRACE_L ) {
      read_initz( task, dec, NULL );
   }
   else if ( dec->dim ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing initializer" );
      bail();
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
/*
   if ( ! dec->dim && dec->type->primitive ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "initializer used on a scalar variable" );
      bail();
   } */
   t_read_tk( task );
   if ( task->tk == TK_BRACE_R ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &initz->pos,
         "initializer is empty" );
      bail();
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
   if ( dec->storage == STORAGE_WORLD || dec->storage == STORAGE_GLOBAL ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &dec->storage_pos, "storage specified for function" );
      bail();
   }
   else if ( dec->type && ! dec->type->primitive ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &dec->type_pos,
         "returning something not a scalar" );
      bail();
   }
   struct func* func = new_func();
   func->object.pos = dec->name_pos;
   func->value = dec->type;
   func->name = dec->name;
   func->impl = NULL;
   add_unresolved( task->ns, ( struct object* ) func );
   // Parameter list:
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   struct params params;
   init_params( &params );
   read_params( task, &params );
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
      impl->imported = false;
      func->impl = impl;
      if ( task->importing ) {
         t_skip_block( task );
         impl->imported = true;
      }
      else {
         struct stmt_read stmt_read;
         t_init_stmt_read( &stmt_read );
         stmt_read.labels = &impl->labels;
         t_read_block( task, &stmt_read );
         impl->body = stmt_read.block;
         list_append( &task->ns->items, func );
      }
      list_append( &task->module->funcs, func );
   }
   else {
      read_bfunc( task, func );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
   }
}

struct func* new_func( void ) {
   struct func* func = mem_alloc( sizeof( *func ) );
   init_object( &func->object, NODE_FUNC );
   func->type = FUNC_ASPEC;
   func->name = NULL;
   func->params = NULL;
   func->min_param = 0;
   func->max_param = 0;
   func->value = NULL;
   func->impl = NULL;
   return func;
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
      else if ( task->tk == TK_FIXED ) {
         type = task->type_fixed;
      }
      else {
         t_test_tk( task, TK_INT );
      }
      if ( params->is_script && type != task->type_int ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &task->tk_pos, "script parameter not of `int` type" );
         bail();
      }
      struct param* param = mem_alloc( sizeof( *param ) );
      init_object( &param->object, NODE_PARAM );
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
         param->name = t_make_name( task, task->tk_text, task->ns->body );
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
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &param->pos, "default parameter in script" );
            bail();
         }
      }
      else if ( tail && tail->expr ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param->pos,
            "parameter missing default value" );
         bail();
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
      // Implicit format-list argument.
      ++func->min_param;
      ++func->max_param;
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
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "no internal function with ID of %d", impl->id );
         bail();
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
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "missing the digit 0" );
         bail();
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
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param_pos,
            "script has over %d parameters", SCRIPT_MAX_PARAMS );
         bail();
      }
   }
   else if ( script->type == SCRIPT_TYPE_DISCONNECT ) {
      // A disconnect script must have a single parameter. It is the number of
      // the player who disconnected from the server.
      if ( script->num_param < 1 ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param_pos,
            "missing player-number parameter in disconnect script" );
         bail();
      }
      if ( script->num_param > 1 ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param_pos,
            "too many parameters in disconnect script" );
         bail();

      }
      t_read_tk( task );
   }
   else {
      if ( script->num_param ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &param_pos,
            "parameter list of %s script not empty", task->tk_text );
         bail();
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
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "duplicate %s flag", task->tk_text );
         bail();
      }
   }
   // Body:
   struct stmt_read stmt;
   t_init_stmt_read( &stmt );
   stmt.labels = &script->labels;
   t_read_block( task, &stmt );
   script->body = stmt.block;
   list_append( &task->module->scripts, script );
   list_append( &task->ns->items, script );
}

void t_read_define( struct task* task ) {
   bool lib = false;
   if ( task->tk_text[ 0 ] == 'l' ) {
      lib = true;
   }
   t_read_tk( task );
   t_test_tk( task, TK_ID );
   struct constant* constant = mem_alloc( sizeof( *constant ) );
   init_object( &constant->object, NODE_CONSTANT );
   constant->object.pos = task->tk_pos;
   // Constants defined using #define and #libdefine are placed in the global
   // namespace, even if they are defined inside a nested namespace.
   constant->name = t_make_name( task, task->tk_text, task->ns_global->body );
   t_read_tk( task );
   struct read_expr expr;
   t_init_read_expr( &expr );
   t_read_expr( task, &expr );
   constant->expr = expr.node;
   constant->value = 0;
   // NOTE: Memory is wasted when parsing a #define in a library. The constant
   // is read and made, then let loose. Maybe find a better way?
   if ( lib || ! task->importing ) {
      add_unresolved( task->ns_global, &constant->object );
   }
}

void t_test( struct task* task ) {
   // Resolve namespace content.
   struct resolve resolve = { NULL };
   while ( true ) {
      d_list_iter_t i;
      d_list_iter_init_head( &i, &task->nss );
      resolve.count = 0;
      resolve.retry = false;
      while ( ! d_list_end( &i ) ) {
         resolve.ns = d_list_data( &i );
         task->ns = resolve.ns;
         test_ns( task, &resolve, resolve.ns );
         d_list_next( &i );
      }
      if ( ! resolve.retry ) {
         break;
      }
      // If we couldn't resolve any items this round, then we won't resolve
      // any in subsequent rounds. Show errors instead.
      if ( ! resolve.count ) {
         resolve.do_err = true;
      }
   }
   // Finish testing the body of functions, and scripts.
   d_list_iter_t i;
   d_list_iter_init_head( &i, &task->nss );
   while ( ! d_list_end( &i ) ) {
      struct ns* ns = d_list_data( &i );
      task->ns = ns;
      list_iter_t i_item;
      list_iter_init( &i_item, &ns->items );
      while ( ! list_end( &i_item ) ) {
         struct node* node = list_data( &i_item );
         if ( node->type == NODE_FUNC ) {
            struct func* func = ( struct func* ) node;
            struct func_user* impl = func->impl;
            if ( ! impl->imported ) {
               test_func_body( task, func );
            }
         }
         else {
            test_script( task, ( struct script* ) node );
         }
         list_next( &i_item );
      }
      d_list_next( &i );
   }
   calc_initials_index( task );
   count_string_usage( task );
   alloc_string_index( task );
   alloc_index( task );
}

void test_ns( struct task* task, struct resolve* resolve, struct ns* ns ) {
   // Resolve namespace links.
   struct object* object = ns->unresolved;
   struct object* tail = NULL;
   ns->unresolved = NULL;
   while ( object ) {
      if ( object->node.type == NODE_SHORTCUT ) {
         t_test_shortcut( task, ( struct shortcut* ) object, resolve->do_err );
      }
      else if ( object->node.type == NODE_NAMESPACE_LINK ) {
         t_test_ns_link( task, ( struct ns_link* ) object, resolve->do_err );
      }
      else if ( object->node.type == NODE_CONSTANT ) {
         t_test_constant( task, ( struct constant* ) object, false,
            resolve->do_err );
      }
      else if ( object->node.type == NODE_CONSTANT_SET ) {
         t_test_constant_set( task, ( struct constant_set* ) object, false,
            resolve->do_err );
      }
      else if ( object->node.type == NODE_TYPE ) {
         t_test_type( task, ( struct type* ) object, resolve->do_err );
      }
      else if ( object->node.type == NODE_VAR ) {
         struct test_var var = {
            .ns = task->ns,
            .var = ( struct var* ) object,
            .do_err = resolve->do_err,
            .local = false };
         test_var( task, &var );
      }
      else if ( object->node.type == NODE_FUNC ) {
         test_func( task, resolve, ( struct func* ) object );
      }
      if ( ! object->resolved ) {
         //struct object* next = object->link;
         //object->link = ns->unresolved;
         //ns->unresolved = object;
         if ( tail ) {
            tail->link = object;
            tail = object;
         }
         else {
            ns->unresolved = object;
            tail = object;
         }
         struct object* next = object->link;
         object->link = NULL;
         object = next;
         resolve->retry = true;
      }
      else {
         struct object* next = object->link;
         object->link = NULL;
         object = next;
         ++resolve->count;
      }
   }
}

void t_test_ns_link( struct task* task, struct ns_link* link,
   bool undef_err )  {
   struct path_test test;
   init_path_test( &test, link->path );
   test_path( task, &test );
   if ( ! test.name ) {
      if ( undef_err ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &test.path->pos, "'%s' is undefined", test.path->text );
         bail();
      }
   }
   else if ( test.name->object->node.type != NODE_NAMESPACE ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &test.path->pos, "'%s' not a namespace", test.path->text );
      bail();
   }
   else {
      struct ns* ns = ( struct ns* ) test.name->object;
      if ( ns == task->ns ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &link->object.pos,
            "namespace making link to itself" );
         bail();
      }
      struct ns_link* older_link = task->ns->ns_link;
      while ( older_link ) {
         if ( ns == older_link->ns ) {
            if ( ns == task->ns_global ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &link->object.pos,
               "duplicate link to global namespace" );
            diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &older_link->object.pos,
               "link to global namespace first made here" );

            }
            else {
               t_copy_full_name( ns->name, &task->str );
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &link->object.pos,
               "duplicate link to namespace `%s`", task->str.value );
            diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &older_link->object.pos,
               "link to namespace `%s` first made here", task->str.value );
            }
            bail();
         }
         older_link = older_link->next;
      }
      if ( task->depth ) {
         link->next = task->ns->ns_link;
         task->ns->ns_link = link;
      }
      link->ns = ns;
      link->object.resolved = true;
   }
}

void t_test_shortcut( struct task* task, struct shortcut* shortcut,
   bool undef_err ) {
   if ( shortcut->path ) {
      struct path_test test;
      init_path_test( &test, shortcut->path );
      test.do_struct = shortcut->import_struct;
      test_path( task, &test );
      if ( ! test.name || ! test.name->object->resolved ) {
         if ( undef_err ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &test.path->pos, "'%s' is undefined", test.path->text );
            bail();
         }
         return;
      }
      shortcut->target = test.name;
      shortcut->object.resolved = true;
   }
   else {
      shortcut->target = task->ns_global->name;
      shortcut->object.resolved = true;
   }
   test_unique_name( task, shortcut->alias, &shortcut->object.pos );
   if ( task->depth ) {
      use_local_name( task, shortcut->alias, &shortcut->object );
   }
   else {
      shortcut->alias->object = ( struct object* ) shortcut;
   }
}

void init_path_test( struct path_test* test, struct path* path ) {
   test->path = path;
   test->name = NULL;
   test->do_struct = false;
}

void test_path( struct task* task, struct path_test* test ) {
   // First search in the current namespace.
   struct path* path = test->path;
   struct name* name = task->ns->body;
   if ( ! path->next && test->do_struct ) {
      name = task->ns->body_struct;
   }
   name = t_make_name( task, path->text, name );
   // If the object is not found in the current namespace, search in the
   // linked namespaces, from the bottom to the top.
   if ( ! name->object ) {
      struct ns_link* link = task->ns->ns_link;
      while ( link && link->ns && ! name->object ) {
         if ( ! path->next && test->do_struct ) {
            name = link->ns->body_struct;
         }
         else {
            name = link->ns->body;
         }
         name = t_make_name( task, path->text, name );
         link = link->next;
      }
   }
   // From the starting object, navigate to the final object.
   while ( name->object ) {
      // Follow a shortcut. It needs to refer to an object.
      while ( name->object->node.type == NODE_SHORTCUT &&
         name->object->resolved ) {
         struct shortcut* shortcut = ( struct shortcut* ) name->object;
         name = shortcut->target;
      }
      if ( ! path->next ) {
         test->name = name;
         break;
      }
      path = path->next;
      if ( name->object->node.type == NODE_NAMESPACE ) {
         struct ns* ns = ( struct ns* ) name->object;
         if ( ! path->next && test->do_struct ) {
            name = ns->body_struct;
         }
         else {
            name = ns->body;
         }
      }
      else {
         break;
      }
      name = t_make_name( task, path->text, name );
   }
   test->path = path;
}

void t_test_constant( struct task* task, struct constant* constant,
   bool local, bool undef_err ) {
   if ( constant->name->object &&
      constant->name->object->depth == task->depth ) {
      t_copy_name( constant->name, &task->str, '.' );
      diag_dup( task->str.value, &constant->object.pos, constant->name );
      bail();
   }
   struct expr_test expr;
   t_init_expr_test( &expr );
   expr.undef_err = undef_err;
   t_test_expr( task, &expr, constant->expr );
   if ( expr.undef_erred ) {
      return;
   }
   else if ( constant->expr->folded ) {
      constant->value = constant->expr->value;
      constant->name->object = ( struct object* ) constant;
      constant->object.resolved = true;
   }
   else {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &expr.pos, "expression not constant" );
      bail();
   }
}

void test_unique_name( struct task* task, struct name* name,
   struct pos* pos ) {
   if ( name->object && name->object->depth == task->depth ) {
      t_copy_name( name, &task->str, '.' );
      diag_dup( task->str.value, pos, name );
      bail();
   }
}

void t_test_constant_set( struct task* task, struct constant_set* set,
   bool local, bool undef_err ) {
   int value = 0;
   struct constant* constant = set->head;
   while ( constant && constant->object.resolved ) {
      value = constant->value;
      constant = constant->next;
   }
   while ( constant ) {
      if ( constant->name->object &&
         constant->name->object->depth == task->depth ) {
         t_copy_name( constant->name, &task->str, '.' );
         diag_dup( task->str.value, &constant->object.pos, constant->name );
         bail();
      }
      if ( constant->expr ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = undef_err;
         t_test_expr( task, &expr, constant->expr );
         if ( expr.undef_erred ) {
            return;
         }
         else if ( ! constant->expr->folded ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &expr.pos, "enumerator expression not constant" );
            bail();
         }
         value = constant->expr->value;
      }
      constant->value = value;
      ++value;
      if ( local ) {
         use_local_name( task, constant->name, ( struct object* ) constant );
      }
      else {
         constant->name->object = ( struct object* ) constant;
      }
      constant->object.resolved = true;
      constant = constant->next;
   }
   set->object.resolved = true;
}

void t_test_type( struct task* task, struct type* type, bool undef_err ) {
   // Name:
   if ( type->name->object != ( struct object* ) type ) {
      if ( type->name->object && type->name->object->depth == task->depth ) {
         t_copy_name( type->name, &task->str, '.' );
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &type->object.pos, "duplicate struct \'%s\'", task->str.value );
         diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &type->name->object->pos,
            "struct '%s' is found here", task->str.value );
         bail();
      }
      if ( task->depth ) {
         use_local_name( task, type->name, ( struct object* ) type );
      }
      else {
         type->name->object = ( struct object* ) type;
      }
   }
   // Members:
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
      struct path_test path_test;
      init_path_test( &path_test, member->type_path );
      test_path( task, &path_test );
      if ( ! path_test.name ) {
         if ( undef_err ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &path_test.path->pos, "'%s' is undefined",
               path_test.path->text );
            bail();
         }
         return;
      }
      else if ( path_test.name->object->node.type != NODE_TYPE ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &path_test.path->pos, "'%s' is not a type",
            path_test.path->text );
         bail();
      }
      else if ( ! path_test.name->object->resolved ) {
         if ( undef_err ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &path_test.path->pos, "type '%s' is undefined",
               path_test.path->text );
            bail();
         }
         return;
      }
      member->type = ( struct type* ) path_test.name->object;
   }
   // Name:
   if ( member->name->object != ( struct object* ) member ) {
      if ( member->name->object &&
         member->name->object->depth == task->depth ) {
         t_copy_name( member->name, &task->str, '.' );
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &member->object.pos, "duplicate struct member \'%s\'",
            task->str.value );
         diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &member->name->object->pos,
            "struct member '%s' is found here", task->str.value );
         bail();
      }
      if ( task->depth ) {
         use_local_name( task, member->name, ( struct object* ) member );
      }
      else {
         member->name->object = ( struct object* ) member;
      }
   }
   // Dimension:
   struct dim* dim = member->dim;
   while ( dim ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = undef_err;
      t_test_expr( task, &expr, dim->count_expr );
      if ( expr.undef_erred ) {
         return;
      }
      else if ( ! dim->count_expr->folded ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "array size not a constant expression" );
         bail();
      }
      else if ( dim->count_expr->value <= 0 ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "array size must be greater than 0" );
         bail();
      }
      dim->count = dim->count_expr->value;
      dim = dim->next;
   }
   member->object.resolved = true;
}

void t_test_local_var( struct task* task, struct var* var ) {
   struct test_var resolve = {
      .ns = task->ns,
      .var = var,
      .do_err = true,
      .local = true };
   test_var( task, &resolve );
   if ( var->dim ) {
      calc_dim_size( var->dim, var->type );
   }
   else if ( ! var->type->primitive && ! var->type->size &&
      ! var->type->size_str ) {
      calc_type_size( var->type );
   }
}

void test_var( struct task* task, struct test_var* test ) {
   struct var* var = test->var;
   // Type:
   if ( var->type_path ) {
      struct path_test pt;
      init_path_test( &pt, var->type_path );
      pt.do_struct = true;
      test_path( task, &pt );
      if ( ! pt.name ) {
         if ( test->do_err ) {
            const char* object = "object";
            if ( pt.path->next ) {
               object = "struct";
            }
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &pt.path->pos, "%s '%s' not found", object, pt.path->text );
            bail();
         }
         return;
      }
      else if ( pt.name->object->node.type != NODE_TYPE ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pt.path->pos,
            "'%s' is not a type", pt.path->text );
         bail();
      }
      else if ( ! pt.name->object->resolved ) {
         if ( test->do_err ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &pt.path->pos, "type '%s' is undefined", pt.path->text );
            bail();
         }
         return;
      }
      else {
         var->type = ( struct type* ) pt.name->object;
      }
   }
   else if ( ! var->type->object.resolved ) {
      return;
   }
   // An array or a variable with a structure type cannot appear in local
   // storage because there's no standard or efficient way to allocate such a
   // variable.
   if ( ! var->type->primitive && var->storage == STORAGE_LOCAL ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &var->object.pos,
         "variable with struct type in local storage" );
      bail();
   }
   // Name:
   test_unique_name( task, var->name, &var->object.pos );
   // Dimension:
   if ( ! var->imported ) {
      if ( var->dim && var->storage == STORAGE_LOCAL ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &var->object.pos,
            "array in local storage" );
         bail();
      }
      struct dim* dim = var->dim;
      while ( dim ) {
         if ( dim->count_expr ) {
            struct expr_test expr;
            t_init_expr_test( &expr );
            expr.undef_err = test->do_err;
            t_test_expr( task, &expr, dim->count_expr );
            if ( expr.undef_erred ) {
               return;
            }
            else if ( ! dim->count_expr->folded ) {
               diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
                  "array size not a constant expression" );
               bail();
            }
            else if ( dim->count_expr->value <= 0 ) {
               diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
                  "array size must be greater than 0" );
               bail();
            }
            dim->count = dim->count_expr->value;
         }
         // Only the first dimension can have an implicit size.
         else if ( dim != var->dim ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &dim->pos,
               "implicit size in subsequent dimension" );
            bail();
         }
         dim = dim->next;
      }
   }
   // Initialization:
   if ( var->initial ) {
      bool erred = false;
      test_init( task, var, test->do_err, &erred );
      if ( erred ) {
         return;
      }
   }
   if ( test->local ) {
      use_local_name( task, var->name, ( struct object* ) var );
   }
   else {
      var->name->object = ( struct object* ) var;
   }
   var->object.resolved = true;
}

void test_init( struct task* task, struct var* var, bool err, bool* erred ) {
   if ( var->dim ) {
      struct initz_test it = {
         .parent = NULL,
         .initz = ( struct initz* ) var->initial,
         .dim = var->dim,
         .num_initials = 0,
         .type = var->type,
         .member = var->type->member,
         .err = err };
      test_initz( task, &it );
      if ( ! it.erred ) {
         // Implicit size of array dimension.
         if ( ! var->dim->count_expr ) {
            var->dim->count = it.num_initials;
         }
      }
      else {
         *erred = true;
      }
   }
   else if ( ! var->type->primitive ) {
      struct initz_test it = {
         .parent = NULL,
         .initz = ( struct initz* ) var->initial,
         .dim = NULL,
         .num_initials = 0,
         .type = var->type,
         .member = var->type->member,
         .err = err };
      test_initz_struct( task, &it );
      if ( it.erred ) {
         *erred = true;
      }
   }
   else {
      struct value* value = ( struct value* ) var->initial;
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = err;
      t_test_expr( task, &expr, value->expr );
      if ( expr.undef_erred ) {
         *erred = true;
      }
      else if ( var->storage == STORAGE_MAP && ! value->expr->folded ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "initial value not constant" );
         bail();
      }
      else {
         if ( var->type->is_str ) {
            var->value_str = value;
         }
         else {
            var->value = value;
         }
      }
   }
}

void test_initz( struct task* task, struct initz_test* it ) {
   struct initial* initial = it->initz->body;
   while ( initial ) {
      if ( initial->tested ) {
         goto next;
      }
      else if ( it->dim->count && it->num_initials >= it->dim->count ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &it->initz->pos,
            "too many values in initializer for dimension of size %d",
            it->dim->count );
         bail();
      }
      if ( initial->is_initz ) {
         struct initz* initz = ( struct initz* ) initial;
         if ( it->dim->next ) {
            struct initz_test nested = {
               .parent = it,
               .initz = initz,
               .type = it->type,
               .dim = it->dim->next,
               .err = it->err };
            test_initz( task, &nested );
            if ( nested.erred ) {
               it->erred = true;
               return;
            }
            initial->tested = true;
         }
         else if ( ! it->type->primitive ) {
            struct initz_test nested = {
               .parent = it,
               .initz = initz,
               .type = it->type,
               .err = it->err };
            test_initz_struct( task, &nested );
            if ( nested.erred ) {
               it->erred = true;
               return;
            }
            initial->tested = true;
         }
         else {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &initz->pos, "too many initializers" );
            bail();
         }
      }
      else {
         struct value* value = ( struct value* ) initial;
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = it->err;
         t_test_expr( task, &expr, value->expr );
         if ( expr.undef_erred ) {
            it->erred = true;
            return;
         }
         else if ( it->dim->next || ! it->type->primitive ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "missing another initializer" );
            bail();
         }
         else if ( ! value->expr->folded ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "initial value not constant" );
            bail();
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
      else if ( ! member ) {
         if ( it->type->anon ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &it->initz->pos,
               "too many values in initializer for anonymous struct" );
         }
         else {
            t_copy_name( it->type->name, &task->str, '.' );
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &it->initz->pos,
               "too many values in initializer for struct '%s'",
               task->str.value );
         }
         bail();
      }
      if ( initial->is_initz ) {
         struct initz* initz = ( struct initz* ) initial;
         if ( member->dim ) {
            struct initz_test nested = {
               .parent = it,
               .initz = initz,
               .type = member->type,
               .dim = member->dim,
               .err = it->err };
            test_initz( task, &nested );
            if ( nested.erred ) {
               it->erred = true;
               return;
            }
            initial->tested = true;
         }
         else if ( ! member->type->primitive ) {
            struct initz_test nested = {
               .parent = it,
               .initz = initz,
               .type = member->type,
               .err = it->err };
            test_initz_struct( task, &nested );
            if ( nested.erred ) {
               it->erred = true;
               return;
            }
            initial->tested = true;
         }
         else {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &initz->pos, "too many initializers" );
            bail();
         }
      }
      else {
         struct value* value = ( struct value* ) initial;
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = it->err;
         t_test_expr( task, &expr, value->expr );
         if ( expr.undef_erred ) {
            it->erred = true;
            return;
         }
         else if ( member->dim || ! member->type->primitive ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &expr.pos, "missing another initializer" );
            bail();
         }
         else if ( ! value->expr->folded ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "initial value not constant" );
            bail();
         }
         initial->tested = true;
      }
      next:
      initial = initial->next;
      ++it->num_initials;
      member = member->next;
   }
}

void test_func( struct task* task, struct resolve* resolve,
   struct func* func ) {
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
            expr.undef_err = resolve->do_err;
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
               t_copy_name( param->name, &task->str, '.' );
               diag_dup( task->str.value, &param->object.pos, param->name );
               bail();
            }
            param->object.link = param->name->object;
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
            param->name->object = param->object.link;
         }
         param = param->next;
      }
      // When stopped at a parameter, that parameter has not been resolved.
      if ( stop ) {
         return;
      }
      // Name:
      test_unique_name( task, func->name, &func->object.pos );
      func->name->object = ( struct object* ) func;
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
            expr.undef_err = resolve->do_err;
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
      test_unique_name( task, func->name, &func->object.pos );
      func->name->object = ( struct object* ) func;
      func->object.resolved = true;
   }
}

void test_func_body( struct task* task, struct func* func ) {
   t_add_scope( task );
   struct param* param = func->params;
   while ( param ) {
      if ( param->name ) {
         use_local_name( task, param->name, ( struct object* ) param );
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
   impl->size = task->scope->size_high - func->max_param;
   t_pop_scope( task );
}

void test_script( struct task* task, struct script* script ) {
   if ( script->number ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, script->number );
      if ( ! script->number->folded ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "script number not a constant expression" );
         bail();
      }
      if ( script->number->value < SCRIPT_MIN_NUM ||
         script->number->value > SCRIPT_MAX_NUM ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "script number not between %d and %d", SCRIPT_MIN_NUM,
            SCRIPT_MAX_NUM );
         bail();
      }
      if ( script->number->value == 0 ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "script number 0 not between << and >>" );
         bail();
      }
   }
   // There should be no duplicate scripts in the same module.
   list_iter_t i;
   list_iter_init( &i, &task->module->scripts );
   while ( ! list_end( &i ) ) {
      struct script* other = list_data( &i );
      if ( script != other && other->tested ) {
         if ( t_get_script_number( script ) == t_get_script_number( other ) ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &script->pos,
               "duplicate script %d", t_get_script_number( script ) );
            diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &other->pos,
               "script %d first found here", t_get_script_number( script ) );
            bail();
         }
      }
      list_next( &i );
   }
   t_add_scope( task );
   struct param* param = script->params;
   while ( param ) {
      if ( param->name ) {
         if ( param->name->object &&
            param->name->object->node.type == NODE_PARAM ) {
            t_copy_name( param->name, &task->str, '.' );
            diag_dup( task->str.value, &param->object.pos, param->name );
            bail();
         }
         use_local_name( task, param->name, &param->object );
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
   script->size = task->scope->size_high;
   t_pop_scope( task );
   script->tested = true;
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
   scope->size = 0;
   scope->size_high = 0;
   scope->prev = task->scope;
   scope->sweep = NULL;
   scope->ns_link = task->ns->ns_link;
   task->scope = scope;
   task->depth += 1;
   if ( scope->prev ) {
      scope->size = scope->prev->size;
      scope->size_high = scope->size;
   }
}

void t_pop_scope( struct task* task ) {
   struct scope* prev = task->scope->prev;
   if ( prev && task->scope->size_high > prev->size_high ) {
      prev->size_high = task->scope->size_high;
   }
   if ( task->scope->sweep ) {
      struct sweep* sweep = task->scope->sweep;
      while ( sweep ) {
         // Remove names.
         for ( int i = 0; i < sweep->size; i += 1 ) {
            struct name* name = sweep->names[ i ];
            name->object = name->object->link;
         }
         // Reuse sweep.
         struct sweep* prev = sweep->prev;
         sweep->prev = task->free_sweep;
         task->free_sweep = sweep;
         sweep = prev;
      }
   }
   task->ns->ns_link = task->scope->ns_link;
   task->scope->prev = task->free_scope;
   task->free_scope = task->scope;
   task->scope = prev;
   task->depth -= 1;
}

void use_local_name( struct task* task, struct name* name,
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
   sweep->size += 1;
   object->depth = task->depth;
   object->link = name->object;
   name->object = object;
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

void alloc_index( struct task* task ) {
   // Calculate the size of the map variables.
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      calc_var_size( list_data( &i ) );
      list_next( &i );
   }
   list_iter_init( &i, &task->module->imports );
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
   // Determine whether the shared arrays are needed.
   int left = MAX_MAP_LOCATIONS;
   bool space_int = false;
   bool space_str = false;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP ) {
         if ( ! var->type->primitive ) {
            if ( var->type->size ) {
               space_int = true;
               if ( left ) { 
                  --left;
               }
            }
            if ( var->type->size_str ) {
               space_str = true;
               if ( left ) { 
                  --left;
               }
            }
         }
         else if ( var->type->is_str ) {
            if ( left ) {
               --left;
            }
            else {
               space_str = true;
            }
         }
         else {
            if ( left ) {
               --left;
            }
            else {
               space_int = true;
            }
         }
      }
      list_next( &i );
   }
   // Allocate indexes.
   int index = 0;
   if ( space_int ) {
      ++index;
   }
   if ( space_str ) {
      ++index;
   }
   // Shared:
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->type->primitive ) {
         var->shared = true;
         if ( var->size ) {
            var->index = task->space_size;
            task->space_size += var->size;
         }
         if ( var->size_str ) {
            var->index_str = task->space_size_str;
            task->space_size_str += var->size_str;
         }
      }
      list_next( &i );
   }
   // Arrays.
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive && var->dim ) {
         if ( index < MAX_MAP_LOCATIONS ) {
            var->index = index;
            ++index;
         }
         // When an index can no longer be allocated, combine any remaining
         // variables into an array.
         else if ( var->type->is_str ) {
            var->shared = true;
            var->index_str = task->space_size_str;
            task->space_size_str += var->size_str;
         }
         else  {
            var->shared = true;
            var->index = task->space_size;
            task->space_size += var->size;
         }
      }
      list_next( &i );
   }
   // Scalars:
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         ! var->dim ) {
         if ( index < MAX_MAP_LOCATIONS ) {
            var->index = index;
            ++index;
         }
         else if ( var->type->is_str ) {
            var->shared = true;
            var->index_str = task->space_size_str;
            task->space_size_str += var->size_str;
         }
         else  {
            var->shared = true;
            var->index = task->space_size;
            task->space_size += var->size;
         }
      }
      list_next( &i );
   }
   // Functions:
   int used = 0;
   list_iter_init( &i, &task->module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      if ( impl->usage ) {
         ++used;
      }
      list_next( &i );
   }
   // The only problem that prevents the usage of the Little-E format is the
   // instruction for calling a user function. In Little-E, the field that
   // stores the index of the function is a byte in size, allowing up to 256
   // functions.
   if ( used <= UCHAR_MAX ) {
      task->format = FORMAT_LITTLE_E;
   }
   // To interface with an imported variable, getter and setter functions are
   // used. These are only used by the user of the library. From inside the
   // library, the variables are interfaced with directly.
   index = 0;
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t i_var;
      list_iter_init( &i_var, &module->vars );
      while ( ! list_end( &i_var ) ) {
         struct var* var = list_data( &i_var );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            var->get = index;
            ++index;
            var->set = index;
            ++index;
            var->flags |= VAR_FLAG_INTERFACE_GET_SET;
            // For now, a function will be allocated for printing an imported
            // array. In the future, I'd like to find a better solution.
            if ( find_integer_array( task, var->type, var->dim ) ) {
               var->printer = index;
               ++index;
               var->flags |= VAR_FLAG_INTERFACE_PRINTER;
            }
         }
         list_next( &i_var );
      }
      list_next( &i );
   }
   // Imported functions:
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t i_func;
      list_iter_init( &i_func, &module->funcs );
      while ( ! list_end( &i_func ) ) {
         struct func* func = list_data( &i_func );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            impl->index += index;
            ++index;
         }
         list_next( &i_func );
      }
      list_next( &i );
   }
   // Getter and setter functions for interfacing with the global variables of
   // a library.
   if ( task->module->name.value ) {
      list_iter_init( &i, &task->module->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->hidden ) {
            var->get = index;
            ++index;
printf( "%d\n", index );
            var->set = index;
            ++index;
            var->flags |= VAR_FLAG_INTERFACE_GET_SET;
            if ( find_integer_array( task, var->type, var->dim ) ) {
               var->printer = index;
               ++index;
               var->flags |= VAR_FLAG_INTERFACE_PRINTER;
            }
         }
         list_next( &i );
      }
   }
   // User functions:
   list_iter_init( &i, &task->module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      impl->index = index;
      ++index;
      list_next( &i );
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

bool find_integer_array( struct task* task, struct type* type,
   struct dim* dim ) {
   if ( type == task->type_int && dim ) {
      return true;
   }
   else {
      struct type_member* member = type->member;
      while ( member ) {
         if ( member->type->primitive ) {
            if ( member->type == task->type_int && member->dim ) {
               return true;
            }
         }
         else {
            find_integer_array( task, member->type, NULL );
         }
         member = member->next;
      }
      return false;
   }
}

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

static void link_values_struct( struct value_list*, struct initz*,
   struct type* );
static void add_value_link( struct value_list*, struct type*, struct value* );
static void alloc_value_index( struct alloc_value_index*, struct initz*,
   struct type*, struct dim* );
static void alloc_value_index_struct( struct alloc_value_index*, struct initz*,
   struct type* );

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

void calc_initials_index( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->initial &&
         var->initial->is_initz ) {
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
      list_next( &i );
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

void alloc_string_index( struct task* task ) {
   int index = 0;
   struct indexed_string* string = task->str_table.head;
   while ( string ) {
      if ( string->usage ) {
         string->index = index;
         ++index;
      }
      string = string->next;
   }
}

static void count_string_usage_initial( struct initial* );

void count_string_usage( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->module->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      find_string_usage_node( ( struct node* ) script->body );
      list_next( &i );
   }
   list_iter_init( &i, &task->module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      find_string_usage_node( ( struct node* ) impl->body );
      list_next( &i );
   }
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->initial ) {
         count_string_usage_initial( var->initial );
      }
      list_next( &i );
   }
/*
   struct indexed_string* string = task->module->str_table.head;
   while ( string ) {
      printf( "\"%s\" %d\n", string->value, string->usage );
      string = string->next;
   }*/
}

void find_string_usage_node( struct node* node ) {
   if ( node->type == NODE_BLOCK ) {
      struct block* block = ( struct block* ) node;
      list_iter_t i;
      list_iter_init( &i, &block->stmts );
      while ( ! list_end( &i ) ) {
         find_string_usage_node( list_data( &i ) );
         list_next( &i );
      }
   }
   else if ( node->type == NODE_EXPR ) {
      struct expr* expr = ( struct expr* ) node;
      count_string_usage_node( expr->root );
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
   else if ( node->type == NODE_CALL ) {
      struct call* call = ( struct call* ) node;
      list_iter_t i;
      list_iter_init( &i, &call->args );
      // Format arguments:
      if ( call->func->type == FUNC_FORMAT ) {
         while ( ! list_end( &i ) ) {
            struct node* node = list_data( &i );
            if ( node->type == NODE_FORMAT_ITEM || node->type == NODE_BLOCK ) {
               count_string_usage_node( node );
               list_next( &i );
            }
            else {
               break;
            }
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
         count_string_usage_node( expr->root );
         param = param->next;
         list_next( &i );
      }
      // Default arguments:
      while ( param ) {
         count_string_usage_node( param->expr->root );
         param = param->next;
      }
   }
   else if ( node->type == NODE_FORMAT_ITEM ) {
      struct format_item* item = ( struct format_item* ) node;
      count_string_usage_node( item->expr->root );
   }
   else if ( node->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) node;
      count_string_usage_node( usage->object );
   }
   else if ( node->type == NODE_VAR ) {
      struct var* var = ( struct var* ) node;
      if ( var->initial && ! var->initial->is_initz ) {
         struct value* value = ( struct value* ) var->initial;
         count_string_usage_node( value->expr->root );
      }
   }
   else if ( node->type == NODE_EXPR ) {
      struct expr* expr = ( struct expr* ) node;
      count_string_usage_node( expr->root );
   }
   else if ( node->type == NODE_INDEXED_STRING_USAGE ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) node;
      ++usage->string->usage;
   }
}

void count_string_usage_initial( struct initial* initial ) {
   if ( initial->is_initz ) {
      struct initz* initz = ( struct initz* ) initial;
      initial = initz->body;
      while ( initial ) {
         count_string_usage_initial( initial );
         initial = initial->next;
      }
   }
   else {
      struct value* value = ( struct value* ) initial;
      count_string_usage_node( value->expr->root );
   }
}

void diag_dup( const char* text, struct pos* pos, struct name* prev ) {
   diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "duplicate name \'%s\'", text );
   diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &prev->object->pos,
      "name '%s' is used here", text );
}