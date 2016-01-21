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
};

static void determine_publishable_objects( struct semantic* semantic );
static void bind_names( struct semantic* semantic );
static void bind_lib( struct semantic* semantic, struct library* lib );
static void bind_object( struct semantic* semantic, struct object* object );
static void test_objects( struct semantic* semantic );
static void test_region( struct semantic* semantic, bool* resolved, bool* retry );
static void test_region_object( struct semantic* semantic,
   struct object* object );
static void test_objects_bodies( struct semantic* semantic );
static void check_dup_scripts( struct semantic* semantic );
static void match_duplicate_script( struct semantic* semantic,
   struct script* script, struct script* other_script, bool* first_match );
static void assign_script_numbers( struct semantic* semantic );
static void calc_map_var_size( struct semantic* semantic );
static void calc_map_value_index( struct semantic* semantic );
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
static void unbind_all( struct semantic* semantic );
static void unbind_lib( struct library* lib );
static void unbind_object( struct object* object );
static void unbind_enum( struct constant_set* enum_ );
static void unbind_struct( struct type* struct_ );

void s_init( struct semantic* semantic, struct task* task,
   struct library* lib ) {
   semantic->task = task;
   semantic->lib = lib;
   semantic->region = NULL;
   semantic->scope = NULL;
   semantic->free_scope = NULL;
   semantic->free_sweep = NULL;
   semantic->topfunc_test = NULL;
   semantic->func_test = NULL;
   semantic->depth = 0;
   semantic->trigger_err = false;
   semantic->in_localscope = false;
}

void s_test( struct semantic* semantic ) {
   determine_publishable_objects( semantic );
   bind_names( semantic );
   test_objects( semantic );
   test_objects_bodies( semantic );
   check_dup_scripts( semantic );
   assign_script_numbers( semantic );
   calc_map_var_size( semantic );
   calc_map_value_index( semantic );
   if ( semantic->lib->imported ) {
      unbind_all( semantic );
   }
}

// Determines which objects be written into the object file.
void determine_publishable_objects( struct semantic* semantic ) {
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      script->publish = true;
      list_next( &i );
   }
   list_iter_init( &i, &semantic->lib->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      impl->publish = true;
      list_next( &i );
   }
}

// Goes through every object in every region and connects the name of the
// object to the object.
void bind_names( struct semantic* semantic ) {
   // Imported objects.
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->dynamic );
   while ( ! list_end( &i ) ) {
      bind_lib( semantic, list_data( &i ) );
      list_next( &i );
   }
   // Objects of the current library.
   bind_lib( semantic, semantic->lib );
}

void bind_lib( struct semantic* semantic, struct library* lib ) {
   list_iter_t i;
   list_iter_init( &i, &lib->objects );
   while ( ! list_end( &i ) ) {
      bind_object( semantic, list_data( &i ) );
      list_next( &i );
   }
}

void bind_object( struct semantic* semantic, struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT: {
      struct constant* constant = ( struct constant* ) object;
      s_bind_name( semantic, constant->name, &constant->object );
      break; }
   case NODE_CONSTANT_SET: {
      struct constant_set* set = ( struct constant_set* ) object;
      struct constant* constant = set->head;
      while ( constant ) {
         s_bind_name( semantic, constant->name, &constant->object );
         constant = constant->next;
      }
      break; }
   case NODE_VAR: {
      struct var* var = ( struct var* ) object;
      s_bind_name( semantic, var->name, &var->object );
      break; }
   case NODE_FUNC: {
      struct func* func = ( struct func* ) object;
      s_bind_name( semantic, func->name, &func->object );
      break; }
   case NODE_TYPE: {
      struct type* type = ( struct type* ) object;
      s_bind_name( semantic, type->name, &type->object );
      break; }
   default:
      t_unhandlednode_diag( semantic->task, __FILE__, __LINE__, &object->node );
      t_bail( semantic->task );
   }
}

// Analyzes the objects found in regions. The bodies of scripts and functions
// are analyzed elsewhere.
void test_objects( struct semantic* semantic ) {
   while ( true ) {
      bool resolved = false;
      bool retry = false;
      list_iter_t i;
      list_iter_init( &i, &semantic->task->regions );
      while ( ! list_end( &i ) ) {
         semantic->region = list_data( &i );
         test_region( semantic, &resolved, &retry );
         list_next( &i );
      }
      if ( retry ) {
         // Continue resolving as long as something got resolved. If nothing
         // gets resolved during the last run, then nothing can be resolved
         // anymore. So report errors.
         semantic->trigger_err = ( ! resolved );
      }
      else {
         break;
      }
   }
}

void test_region( struct semantic* semantic, bool* resolved, bool* retry ) {
   struct object* object = semantic->region->unresolved;
   struct object* tail = NULL;
   semantic->region->unresolved = NULL;
   while ( object ) {
      test_region_object( semantic, object );
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
            semantic->region->unresolved = object;
         }
         tail = object;
         struct object* next = object->next;
         object->next = NULL;
         object = next;
         *retry = true;
      }
   }
   semantic->region->object.resolved = ( semantic->region->unresolved == NULL );
}

void test_region_object( struct semantic* semantic, struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT:
      s_test_constant( semantic, ( struct constant* ) object );
      break;
   case NODE_CONSTANT_SET:
      s_test_constant_set( semantic, ( struct constant_set* ) object );
      break;
   case NODE_TYPE:
      s_test_struct( semantic, ( struct type* ) object );
      break;
   case NODE_VAR:
      s_test_var( semantic, ( struct var* ) object );
      break;
   case NODE_FUNC:
      s_test_func( semantic, ( struct func* ) object );
      break;
   default:
      t_unhandlednode_diag( semantic->task, __FILE__, __LINE__, &object->node );
      t_bail( semantic->task );
   }
}

// Analyzes the body of functions, and scripts.
void test_objects_bodies( struct semantic* semantic ) {
   semantic->trigger_err = true;
   list_iter_t i;
   list_iter_init( &i, &semantic->task->regions );
   while ( ! list_end( &i ) ) {
      semantic->region = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &semantic->region->items );
      while ( ! list_end( &k ) ) {
         struct node* node = list_data( &k );
         if ( node->type == NODE_FUNC ) {
            struct func* func = ( struct func* ) node;
            struct func_user* impl = func->impl;
            s_test_func_body( semantic, func );
         }
         else {
            struct script* script = ( struct script* ) node;
            s_test_script( semantic, script );
         }
         list_next( &k );
      }
      list_next( &i );
   }
}

void check_dup_scripts( struct semantic* semantic ) {
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->scripts );
   while ( ! list_end( &i ) ) {
      list_iter_t k = i;
      list_next( &k );
      bool first_match = false;
      while ( ! list_end( &k ) ) {
         match_duplicate_script( semantic, list_data( &i ), list_data( &k ),
            &first_match );
         list_next( &k );
      }
      if ( first_match ) {
         s_bail( semantic );
      }
      list_next( &i );
   }
}

void match_duplicate_script( struct semantic* semantic, struct script* script,
   struct script* other_script, bool* first_match ) {
   bool match = false;
   struct indexed_string* name = NULL;
   if ( script->named_script ) {
      if ( ! other_script->named_script ) {
         return;
      }
      name = t_lookup_string( semantic->task, script->number->value );
      struct indexed_string* other_name = t_lookup_string( semantic->task,
         other_script->number->value );
      match = ( name == other_name );
   }
   else {
      if ( other_script->named_script ) {
         return;
      }
      match = (
         t_get_script_number( script ) ==
         t_get_script_number( other_script ) );
   }
   if ( ! match ) {
      return;
   }
   if ( ! *first_match ) {
      if ( script->named_script ) {
         s_diag( semantic, DIAG_POS_ERR, &script->pos,
            "duplicate script \"%s\"", name->value );
      }
      else {
         s_diag( semantic, DIAG_POS_ERR, &script->pos,
            "duplicate script %d", t_get_script_number( script ) );
      }
      *first_match = true;
   }
   s_diag( semantic, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
      &other_script->pos, "script found here" );
}

void assign_script_numbers( struct semantic* semantic ) {
   int named_script_number = -1;
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->named_script ) {
         script->assigned_number = named_script_number;
         --named_script_number;
      }
      else {
         script->assigned_number = t_get_script_number( script );
      }
      list_next( &i );
   }
}

void calc_map_var_size( struct semantic* semantic ) {
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->vars );
   while ( ! list_end( &i ) ) {
      s_calc_var_size( list_data( &i ) );
      list_next( &i );
   }
}

void calc_map_value_index( struct semantic* semantic ) {
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->initial ) {
         s_calc_var_value_index( var );
      }
      list_next( &i );
   }
}

void s_add_scope( struct semantic* semantic ) {
   struct scope* scope;
   if ( semantic->free_scope ) {
      scope = semantic->free_scope;
      semantic->free_scope = scope->prev;
   }
   else {
      scope = mem_alloc( sizeof( *scope ) );
   }
   scope->prev = semantic->scope;
   scope->sweep = NULL;
   scope->region_link = semantic->region->link;
   semantic->scope = scope;
   ++semantic->depth;
   semantic->in_localscope = ( semantic->depth > 0 );
}

void s_pop_scope( struct semantic* semantic ) {
   if ( semantic->scope->sweep ) {
      struct sweep* sweep = semantic->scope->sweep;
      while ( sweep ) {
         // Remove names.
         for ( int i = 0; i < sweep->size; ++i ) {
            struct name* name = sweep->names[ i ];
            name->object = name->object->next_scope;
         }
         // Reuse sweep.
         struct sweep* prev = sweep->prev;
         sweep->prev = semantic->free_sweep;
         semantic->free_sweep = sweep;
         sweep = prev;
      }
   }
   struct scope* prev = semantic->scope->prev;
   semantic->region->link = semantic->scope->region_link;
   semantic->scope->prev = semantic->free_scope;
   semantic->free_scope = semantic->scope;
   semantic->scope = prev;
   --semantic->depth;
   semantic->in_localscope = ( semantic->depth > 0 ); 
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
   struct name* name = t_extend_name(
      ( get_struct ? region->body_struct : region->body ), lookup );
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

void s_diag( struct semantic* semantic, int flags, ... ) {
   va_list args;
   va_start( args, flags );
   t_diag_args( semantic->task, flags, &args );
   va_end( args );
}

void s_bail( struct semantic* semantic ) {
   t_bail( semantic->task );
}

void unbind_all( struct semantic* semantic ) {
   // Imported objects.
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->dynamic );
   while ( ! list_end( &i ) ) {
      unbind_lib( list_data( &i ) );
      list_next( &i );
   }
   // Objects of the current library.
   unbind_lib( semantic->lib );
}

void unbind_lib( struct library* lib ) {
   list_iter_t i;
   list_iter_init( &i, &lib->objects );
   while ( ! list_end( &i ) ) {
      unbind_object( list_data( &i ) );
      list_next( &i );
   }
}

void unbind_object( struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT: {
      struct constant* constant = ( struct constant* ) object;
      constant->name->object = NULL;
      break; }
   case NODE_CONSTANT_SET:
      unbind_enum(
         ( struct constant_set* ) object );
      break;
   case NODE_TYPE:
      unbind_struct( ( struct type* ) object );
      break;
   case NODE_VAR: {
      struct var* var = ( struct var* ) object;
      var->name->object = NULL;
      break; }
   case NODE_FUNC: {
      struct func* func = ( struct func* ) object;
      func->name->object = NULL;
      break; }
   default:
      UNREACHABLE();
   }
}

void unbind_enum( struct constant_set* enum_ ) {
   struct constant* enumerator = enum_->head;
   while ( enumerator ) {
      enumerator->name->object = NULL;
      enumerator = enumerator->next;
   }
}

void unbind_struct( struct type* struct_ ) {
   struct_->name->object = NULL;
   struct type_member* member = struct_->member;
   while ( member ) {
      member->name->object = NULL;
      member = member->next;
   }
}