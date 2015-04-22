#include "phase.h"

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

static void determine_publishable_objects( struct semantic* phase );
static void bind_names( struct semantic* phase );
static void bind_regionobject_name( struct semantic* phase,
   struct object* object );
static void import_objects( struct semantic* semantic );
static void test_objects( struct semantic* phase );
static void test_region( struct semantic* phase, bool* resolved, bool* retry );
static void test_region_object( struct semantic* phase,
   struct object* object );
static void test_objects_bodies( struct semantic* phase );
static void check_dup_scripts( struct semantic* phase );
static void calc_map_var_size( struct semantic* phase );
static void calc_map_value_index( struct semantic* phase );
static void add_loadable_libs( struct semantic* phase );
static void dupname_err( struct semantic* semantic, struct name* name,
   struct object* object );
static void add_sweep_name( struct semantic* semantic, struct name* name,
   struct object* object );
static void find_next_object( struct semantic* semantic,
   struct object_search* search );
static void find_head_object( struct semantic* semantic,
   struct object_search* search );
static void find_tail_object( struct semantic* semantic,
   struct object_search* search );

void s_init( struct semantic* phase, struct task* task ) {
   phase->task = task;
   phase->region = NULL;
   phase->scope = NULL;
   phase->free_scope = NULL;
   phase->free_sweep = NULL;
   phase->topfunc_test = NULL;
   phase->func_test = NULL;
   phase->depth = 0;
   phase->trigger_err = false;
   phase->in_localscope = false;
}

void s_test( struct semantic* phase ) {
   determine_publishable_objects( phase );
   bind_names( phase );
   import_objects( phase );
   test_objects( phase );
   test_objects_bodies( phase );
   check_dup_scripts( phase );
   calc_map_var_size( phase );
   calc_map_value_index( phase );
   add_loadable_libs( phase );
}

// Determines which objects be written into the object file.
void determine_publishable_objects( struct semantic* phase ) {
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      script->publish = true;
      list_next( &i );
   }
   list_iter_init( &i, &phase->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      impl->publish = true;
      list_next( &i );
   }
}

// Goes through every object in every region and connects the name of the
// object to the object.
void bind_names( struct semantic* phase ) {
   list_iter_t i;
   list_iter_init( &i, &phase->task->regions );
   while ( ! list_end( &i ) ) {
      struct region* region = list_data( &i );
      struct object* object = region->unresolved;
      while ( object ) {
         bind_regionobject_name( phase, object );
         object = object->next;
      }
      list_next( &i );
   }
}

void bind_regionobject_name( struct semantic* phase, struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT: {
      struct constant* constant = ( struct constant* ) object;
      s_bind_name( phase, constant->name, &constant->object );
      break; }
   case NODE_CONSTANT_SET: {
      struct constant_set* set = ( struct constant_set* ) object;
      struct constant* constant = set->head;
      while ( constant ) {
         s_bind_name( phase, constant->name, &constant->object );
         constant = constant->next;
      }
      break; }
   case NODE_VAR: {
      struct var* var = ( struct var* ) object;
      s_bind_name( phase, var->name, &var->object );
      break; }
   case NODE_FUNC: {
      struct func* func = ( struct func* ) object;
      s_bind_name( phase, func->name, &func->object );
      break; }
   case NODE_TYPE: {
      struct type* type = ( struct type* ) object;
      s_bind_name( phase, type->name, &type->object );
      break; }
   default:
      t_unhandlednode_diag( phase->task, __FILE__, __LINE__, &object->node );
      t_bail( phase->task );
   }
}

// Executes import-statements found in regions.
void import_objects( struct semantic* semantic ) {
   while ( true ) {
      bool erred = false;
      bool resolved = false;
      list_iter_t i;
      list_iter_init( &i, &semantic->task->regions );
      while ( ! list_end( &i ) ) {
         semantic->region = list_data( &i );
         list_iter_t k;
         list_iter_init( &k, &semantic->region->imports );
         while ( ! list_end( &k ) ) {
            struct import_status status;
            s_import( semantic, list_data( &k ), &status );
            if ( status.resolved ) {
               resolved = true;
            }
            if ( status.erred ) {
               erred = true;
            }
            list_next( &k );
         }
         list_next( &i );
      }
      if ( erred ) {
         semantic->trigger_err = ( ! resolved );
      }
      else {
         break;
      }
   }
}

// Analyzes the objects found in regions. The bodies of scripts and functions
// are analyzed elsewhere.
void test_objects( struct semantic* phase ) {
   while ( true ) {
      bool resolved = false;
      bool retry = false;
      list_iter_t i;
      list_iter_init( &i, &phase->task->regions );
      while ( ! list_end( &i ) ) {
         phase->region = list_data( &i );
         test_region( phase, &resolved, &retry );
         list_next( &i );
      }
      if ( retry ) {
         // Continue resolving as long as something got resolved. If nothing
         // gets resolved during the last run, then nothing can be resolved
         // anymore. So report errors.
         phase->trigger_err = ( ! resolved );
      }
      else {
         break;
      }
   }
}

void test_region( struct semantic* phase, bool* resolved, bool* retry ) {
   struct object* object = phase->region->unresolved;
   struct object* tail = NULL;
   phase->region->unresolved = NULL;
   while ( object ) {
      test_region_object( phase, object );
      if ( object->resolved ) {
         struct object* next = object->next;
         object->next = NULL;
         object = next;
         *resolved = true;
      }
      else {
         if ( tail ) {
            tail->next = object;
         }
         else {
            phase->region->unresolved = object;
         }
         tail = object;
         struct object* next = object->next;
         object->next = NULL;
         object = next;
         *retry = true;
      }
   }
   phase->region->object.resolved = ( phase->region->unresolved == NULL );
}

void test_region_object( struct semantic* phase, struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT:
      s_test_constant( phase, ( struct constant* ) object );
      break;
   case NODE_CONSTANT_SET:
      s_test_constant_set( phase, ( struct constant_set* ) object );
      break;
   case NODE_TYPE:
      s_test_struct( phase, ( struct type* ) object );
      break;
   case NODE_VAR:
      s_test_var( phase, ( struct var* ) object );
      break;
   case NODE_FUNC:
      s_test_func( phase, ( struct func* ) object );
      break;
   default:
      t_unhandlednode_diag( phase->task, __FILE__, __LINE__, &object->node );
      t_bail( phase->task );
   }
}

// Analyzes the body of functions, and scripts.
void test_objects_bodies( struct semantic* phase ) {
   phase->trigger_err = true;
   list_iter_t i;
   list_iter_init( &i, &phase->task->regions );
   while ( ! list_end( &i ) ) {
      phase->region = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &phase->region->items );
      while ( ! list_end( &k ) ) {
         struct node* node = list_data( &k );
         if ( node->type == NODE_FUNC ) {
            struct func* func = ( struct func* ) node;
            struct func_user* impl = func->impl;
            if ( impl->publish ) {
               s_test_func_body( phase, func );
            }
         }
         else {
            struct script* script = ( struct script* ) node;
            if ( script->publish ) {
               s_test_script( phase, script );
               list_append( &phase->task->scripts, script );
            }
         }
         list_next( &k );
      }
      list_next( &i );
   }
}

void check_dup_scripts( struct semantic* phase ) {
   list_iter_t i;
   list_iter_init( &i, &phase->task->scripts );
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
               s_diag( phase, DIAG_POS_ERR, &script->pos,
                  "duplicate script %d", t_get_script_number( script ) );
               dup = true;
            }
            s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &other_script->pos, "script found here",
               t_get_script_number( script ) );
         }
         list_next( &k );
      }
      if ( dup ) {
         s_bail( phase );
      }
      list_next( &i );
   }
}

void calc_map_var_size( struct semantic* phase ) {
   list_iter_t i;
   list_iter_init( &i, &phase->task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         s_calc_var_size( list_data( &k ) );
         list_next( &k );
      }
      list_next( &i );
   }
}

void calc_map_value_index( struct semantic* phase ) {
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->initial ) {
         s_calc_var_value_index( var );
      }
      list_next( &i );
   }
}

// NOTE: Maybe move this to the back-end?
void add_loadable_libs( struct semantic* phase ) {
   // Any library that has its contents used is dynamically loaded.
   list_iter_t i;
   list_iter_init( &i, &phase->task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib != phase->task->library_main ) {
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
            list_iter_init( &k, &phase->task->library_main->dynamic );
            while ( ! list_end( &k ) && list_data( &i ) != lib ) {
               list_next( &k );
            }
            if ( list_end( &i ) ) {
               list_append( &phase->task->library_main->dynamic, lib );
            }
         }
      }
      list_next( &i );
   }
}

void s_add_scope( struct semantic* phase ) {
   struct scope* scope;
   if ( phase->free_scope ) {
      scope = phase->free_scope;
      phase->free_scope = scope->prev;
   }
   else {
      scope = mem_alloc( sizeof( *scope ) );
   }
   scope->prev = phase->scope;
   scope->sweep = NULL;
   scope->region_link = phase->region->link;
   scope->imports = NULL;
   phase->scope = scope;
   ++phase->depth;
   phase->in_localscope = ( phase->depth > 0 );
}

void s_pop_scope( struct semantic* phase ) {
   if ( phase->scope->sweep ) {
      struct sweep* sweep = phase->scope->sweep;
      while ( sweep ) {
         // Remove names.
         for ( int i = 0; i < sweep->size; ++i ) {
            struct name* name = sweep->names[ i ];
            name->object = name->object->next_scope;
         }
         // Reuse sweep.
         struct sweep* prev = sweep->prev;
         sweep->prev = phase->free_sweep;
         phase->free_sweep = sweep;
         sweep = prev;
      }
   }
   struct scope* prev = phase->scope->prev;
   phase->region->link = phase->scope->region_link;
   phase->scope->prev = phase->free_scope;
   phase->free_scope = phase->scope;
   phase->scope = prev;
   --phase->depth;
   phase->in_localscope = ( phase->depth > 0 ); 
}

void s_bind_name( struct semantic* semantic, struct name* name,
   struct object* object ) {
   if ( ! name->object || name->object->depth < semantic->depth ) {
      if ( semantic->depth ) {
         add_sweep_name( semantic, name, object );
      }
      else {
         name->object = object;
      }
   }
   else {
      dupname_err( semantic, name, object );
   }
}

void dupname_err( struct semantic* semantic, struct name* name,
   struct object* object ) {
   struct str str;
   str_init( &str );
   t_copy_name( name, false, &str );
   if ( object->node.type == NODE_TYPE ) {
      s_diag( semantic, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &object->pos, "duplicate struct `%s`", str.value );
      s_diag( semantic, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &name->object->pos, "struct already found here" );
   }
   else if ( object->node.type == NODE_TYPE_MEMBER ) {
      s_diag( semantic, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &object->pos, "duplicate struct-member `%s`", str.value );
      s_diag( semantic, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &name->object->pos, "struct-member already found here",
         str.value );
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &object->pos,
         "duplicate name `%s`", str.value );
      s_diag( semantic, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &name->object->pos, "name already used here", str.value );
   }
   s_bail( semantic );
}

void add_sweep_name( struct semantic* semantic, struct name* name,
   struct object* object ) {
   struct sweep* sweep = semantic->scope->sweep;
   if ( ! sweep || sweep->size == SWEEP_MAX_SIZE ) {
      if ( semantic->free_sweep ) {
         sweep = semantic->free_sweep;
         semantic->free_sweep = sweep->prev;
      }
      else {
         sweep = mem_alloc( sizeof( *sweep ) );
      }
      sweep->size = 0;
      sweep->prev = semantic->scope->sweep;
      semantic->scope->sweep = sweep; 
   }
   sweep->names[ sweep->size ] = name;
   ++sweep->size;
   object->depth = semantic->depth;
   object->next_scope = name->object;
   name->object = object;
}

void s_init_object_search( struct object_search* search, struct path* path,
   bool get_struct ) {
   search->path = path;
   search->object = NULL;
   search->struct_object = NULL;
   search->get_struct = get_struct;
}

void s_find_object( struct semantic* semantic, struct object_search* search ) {
   bool get_struct = search->get_struct;
   while ( search->path ) {
      search->get_struct = ( get_struct && ! search->path->next );
      find_next_object( semantic, search );
      search->path = search->path->next;
   }
}

void find_next_object( struct semantic* semantic,
   struct object_search* search ) {
   if ( ! search->object ) {
      find_head_object( semantic, search );
   }
   else {
      find_tail_object( semantic, search );
   }
   // Error.
   if ( ! search->object ) {
      s_diag( semantic, DIAG_POS_ERR, &search->path->pos,
         "%s`%s` not found", ( search->get_struct ? "struct " : "" ),
         search->path->text );
      s_bail( semantic );
   }
   else if ( search->get_struct &&
      search->object->node.type != NODE_TYPE ) {
      s_diag( semantic, DIAG_POS_ERR, &search->path->pos,
         "object not a struct" );
      s_bail( semantic );
   }
   else if ( search->path->next &&
      search->object->node.type != NODE_REGION ) {
      s_diag( semantic, DIAG_POS_ERR, &search->path->pos,
         "`%s` not a region", search->path->text );
      s_bail( semantic );
   }
}

void find_head_object( struct semantic* semantic,
   struct object_search* search ) {
   if ( search->path->is_upmost ) {
      search->object = &semantic->task->region_upmost->object;
      return;
   }
   else if ( search->path->is_region ) {
      search->object = &semantic->region->object;
      return;
   }
   // Search for the head in the current region.
   struct regobjget result = s_get_regionobject( semantic,
      semantic->region, search->path->text, search->get_struct );
   search->object = result.object;
   search->struct_object = result.struct_object;
   if ( search->object ) {
      return;
   }
   // Search for the head in the linked regions.
   struct regionlink_search linked;
   s_init_regionlink_search( &linked, semantic->region,
      search->path->text, &search->path->pos, search->get_struct );
   s_find_linkedobject( semantic, &linked );
   search->object = linked.object;
   search->struct_object = linked.struct_object;
}

// Assumes that the object currently found is a region.
void find_tail_object( struct semantic* semantic,
   struct object_search* search ) {
   struct regobjget result = s_get_regionobject( semantic,
      ( struct region* ) search->object, search->path->text,
      search->get_struct );
   search->object = result.object;
   search->struct_object = result.struct_object;
}

void s_init_regionlink_search( struct regionlink_search* search,
   struct region* region, const char* name, struct pos* name_pos,
   bool get_struct ) {
   search->region = region;
   search->name = name;
   search->name_pos = name_pos;
   search->object = NULL;
   search->struct_object = NULL;
   search->get_struct = get_struct;
}

void s_find_linkedobject( struct semantic* semantic,
   struct regionlink_search* search ) {
   struct region_link* link = search->region->link;
   while ( link ) {
      struct regobjget result = s_get_regionobject( semantic,
         link->region, search->name, search->get_struct );
      link = link->next;
      if ( result.object ) {
         search->object = result.object;
         search->struct_object = result.struct_object;
         break;
      }
   }
   // Make sure no other object with the same name can be found.
   struct object* object = search->object;
   bool dup = false;
   while ( link ) {
      struct regobjget result = s_get_regionobject( semantic,
         link->region, search->name, search->get_struct );
      if ( result.object ) {
         if ( ! dup ) {
            s_diag( semantic, DIAG_POS_ERR, search->name_pos,
               "multiple instances of %s`%s`",
               ( search->get_struct ? "struct " : "" ), search->name );
            dup = true;
         }
         s_diag( semantic, DIAG_POS, &object->pos,
            "instance found here, and" );
         object = result.object;
      }
      link = link->next;
   }
   if ( dup ) {
      s_diag( semantic, DIAG_POS, &object->pos,
         "instance found here" );
      s_bail( semantic );
   }
}

struct regobjget s_get_regionobject( struct semantic* semantic,
   struct region* region, const char* lookup, bool get_struct ) {
   struct name* name = t_make_name( semantic->task, lookup,
      ( get_struct ? region->body_struct : region->body ) );
   struct object* object = name->object;
   if ( object ) {
      while ( object->next_scope ) {
         object = object->next_scope;
      }
      if ( object->depth == 0 ) {
         if ( object->node.type == NODE_ALIAS ) {
            struct alias* alias = ( struct alias* ) object;
            object = alias->target;
         }
      }
   }
   struct regobjget result;
   result.object = NULL;
   result.struct_object = NULL;
   if ( object ) {
      if ( get_struct ) {
         if ( object->node.type == NODE_TYPE ) {
            result.object = object;
            result.struct_object = ( struct type* ) object;
         }
      }
      else {
         result.object = object;
      }
   }
   return result;
}

void s_diag( struct semantic* phase, int flags, ... ) {
   va_list args;
   va_start( args, flags );
   t_diag_args( phase->task, flags, &args );
   va_end( args );
}

void s_bail( struct semantic* phase ) {
   t_bail( phase->task );
}