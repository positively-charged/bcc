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
   struct ns_link* ns_link;
};

static void bind_names( struct semantic* semantic );
static void bind_namespace( struct semantic* semantic, struct ns* ns );
static void bind_namespace_object( struct semantic* semantic,
   struct object* object );
static void bind_enumeration( struct semantic* semantic,
   struct enumeration* enumeration );
static void bind_structure( struct semantic* semantic,
   struct structure* structure );
static void show_private_objects( struct semantic* semantic );
static void hide_private_objects( struct semantic* semantic );
static void perform_usings( struct semantic* semantic );
static void perform_lib_usings( struct semantic* semantic,
   struct library* lib );
static void import_all( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc );
static void import_selection( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc );
static void import_item( struct semantic* semantic, struct ns* ns,
   struct using_item* item );
static struct object* follow_path( struct semantic* semantic,
   struct path* path, bool only_ns );
static void test_objects( struct semantic* semantic );
static void test_all( struct semantic* semantic );
static void test_namespace( struct semantic* semantic, struct ns* ns );
static void test_namespace_object( struct semantic* semantic,
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
static bool implicitly_imported( struct object* object );
static void add_sweep_name( struct semantic* semantic, struct name* name,
   struct object* object );
static struct object* get_nsobject( struct ns* ns, const char* object_name );
static void confirm_compiletime_content( struct semantic* semantic );
static bool is_compiletime_object( struct object* object );

void s_init( struct semantic* semantic, struct task* task,
   struct library* lib ) {
   semantic->task = task;
   semantic->lib = lib;
   semantic->ns = NULL;
   semantic->scope = NULL;
   semantic->free_scope = NULL;
   semantic->free_sweep = NULL;
   semantic->topfunc_test = NULL;
   semantic->func_test = NULL;
   semantic->depth = 0;
   semantic->retest_nss = false;
   semantic->resolved_objects = false;
   semantic->trigger_err = false;
   semantic->in_localscope = false;
}

void s_test( struct semantic* semantic ) {
   bind_names( semantic );
   perform_usings( semantic );
   test_objects( semantic );
   test_objects_bodies( semantic );
   check_dup_scripts( semantic );
   assign_script_numbers( semantic );
   calc_map_var_size( semantic );
   calc_map_value_index( semantic );
   if ( semantic->lib->compiletime ) {
      confirm_compiletime_content( semantic );
      if ( ! semantic->lib->imported ) {
         s_diag( semantic, DIAG_FILE | DIAG_ERR, &semantic->lib->file_pos,
            "compiling a compile-time only library" );
         s_diag( semantic, DIAG_FILE, &semantic->lib->file_pos,
            "a compile-time library can only be #imported" );
         s_bail( semantic );
      }
   }
}

void bind_names( struct semantic* semantic ) {
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      bind_namespace( semantic, lib->upmost_ns );
      list_next( &i );
   }
   bind_namespace( semantic, semantic->lib->upmost_ns );
}

void bind_namespace( struct semantic* semantic, struct ns* ns ) {
   semantic->ns = ns;
   list_iter_t i;
   list_iter_init( &i, &ns->objects );
   while ( ! list_end( &i ) ) {
      bind_namespace_object( semantic, list_data( &i ) );
      list_next( &i );
   }
   hide_private_objects( semantic );
}

void bind_namespace_object( struct semantic* semantic,
   struct object* object ) {
   switch ( object->node.type ) {
      struct constant* constant;
      struct type_alias* type_alias;
      struct var* var;
      struct func* func;
   case NODE_CONSTANT:
      constant = ( struct constant* ) object;
      s_bind_name( semantic, constant->name, &constant->object );
      break;
   case NODE_ENUMERATION:
      bind_enumeration( semantic,
         ( struct enumeration* ) object );
      break;
   case NODE_STRUCTURE:
      bind_structure( semantic,
         ( struct structure* ) object );
      break;
   case NODE_TYPE_ALIAS:
      type_alias = ( struct type_alias* ) object;
      s_bind_name( semantic, type_alias->name, &type_alias->object );
      break;
   case NODE_VAR:
      var = ( struct var* ) object;
      s_bind_name( semantic, var->name, &var->object );
      break;
   case NODE_FUNC:
      func = ( struct func* ) object;
      s_bind_name( semantic, func->name, &func->object );
      break;
   case NODE_NAMESPACE:
      bind_namespace( semantic,
         ( struct ns* ) object );
      break;
   default:
      UNREACHABLE();
   }
}

void bind_enumeration( struct semantic* semantic,
   struct enumeration* enumeration ) {
   if ( enumeration->name ) {
      s_bind_name( semantic, enumeration->name, &enumeration->object );
   }
   struct enumerator* enumerator = enumeration->head;
   while ( enumerator ) {
      s_bind_name( semantic, enumerator->name, &enumerator->object );
      enumerator = enumerator->next;
   }
}

void bind_structure( struct semantic* semantic, struct structure* structure ) {
   s_bind_name( semantic, structure->name, &structure->object );
   struct structure_member* member = structure->member;
   while ( member ) {
      s_bind_name( semantic, member->name, &member->object );
      member = member->next;
   }
}

void show_private_objects( struct semantic* semantic ) {
   // Local objects.
   list_iter_t i;
   list_iter_init( &i, &semantic->ns->private_objects );
   while ( ! list_end( &i ) ) {
      struct object* object = list_data( &i );
      switch ( object->node.type ) {
         struct var* var;
         struct func* func;
      case NODE_VAR:
         var = ( struct var* ) object;
         var->name->object = &var->object;
         break;
      case NODE_FUNC:
         func = ( struct func* ) object;
         func->name->object = &func->object;
         break;
      default:
         UNREACHABLE();
      }
      list_next( &i );
   }
   // Imports.
   list_iter_init( &i, &semantic->ns->usings );
   while ( ! list_end( &i ) ) {
      struct using_dirc* dirc = list_data( &i );
      if ( dirc->type == USING_SELECTION ) {
         list_iter_t k;
         list_iter_init( &k, &dirc->items );
         while ( ! list_end( &k ) ) {
            struct using_item* item = list_data( &k );
            struct name* name = t_extend_name( semantic->ns->body,
               item->name );
            name->object = item->imported_object;
            list_next( &k );
         }
      }
      list_next( &i );
   }
}

void hide_private_objects( struct semantic* semantic ) {
   // Local objects.
   list_iter_t i;
   list_iter_init( &i, &semantic->ns->private_objects );
   while ( ! list_end( &i ) ) {
      struct object* object = list_data( &i );
      switch ( object->node.type ) {
         struct var* var;
         struct func* func;
      case NODE_VAR:
         var = ( struct var* ) object;
         var->name->object = NULL;
         break;
      case NODE_FUNC:
         func = ( struct func* ) object;
         func->name->object = NULL;
         break;
      default:
         UNREACHABLE();
      }
      list_next( &i );
   }
   // Imported objects.
   list_iter_init( &i, &semantic->ns->usings );
   while ( ! list_end( &i ) ) {
      struct using_dirc* dirc = list_data( &i );
      if ( dirc->type == USING_SELECTION ) {
         list_iter_t k;
         list_iter_init( &k, &dirc->items );
         while ( ! list_end( &k ) ) {
            struct using_item* item = list_data( &k );
            struct name* name = t_extend_name( semantic->ns->body,
               item->name );
            name->object = NULL;
            list_next( &k );
         }
      }
      list_next( &i );
   }
}

void perform_usings( struct semantic* semantic ) {
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->dynamic );
   while ( ! list_end( &i ) ) {
      perform_lib_usings( semantic, list_data( &i ) );
      list_next( &i );
   }
   perform_lib_usings( semantic, semantic->lib );
}

void perform_lib_usings( struct semantic* semantic, struct library* lib ) {
   list_iter_t i;
   list_iter_init( &i, &lib->namespaces );
   while ( ! list_end( &i ) ) {
      semantic->ns = list_data( &i );
      show_private_objects( semantic );
      list_iter_t k;
      list_iter_init( &k, &semantic->ns->usings );
      while ( ! list_end( &k ) ) {
         s_perform_using( semantic, list_data( &k ) );
         list_next( &k );
      }
      hide_private_objects( semantic );
      list_next( &i );
   }
}

void s_perform_using( struct semantic* semantic, struct using_dirc* dirc ) {
   struct ns* ns = ( struct ns* ) follow_path( semantic, dirc->path, true );
   switch ( dirc->type ) {
   case USING_ALL:
      import_all( semantic, ns, dirc );
      break;
   case USING_SELECTION:
      import_selection( semantic, ns, dirc );
      break;
   default:
      UNREACHABLE()
   }
}

void import_all( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc ) {
   if ( ns == semantic->ns ) {
      s_diag( semantic, DIAG_POS_ERR, &dirc->pos,
         "namespace importing itself" );
      s_bail( semantic );
   }
   struct ns_link* link = semantic->ns->links;
   while ( link && link->ns != ns ) {
      link = link->next;
   }
   // Duplicate links are allowed, but warn the user about them.
   if ( link ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS, &dirc->pos,
         "duplicate namespace import" );
      s_diag( semantic, DIAG_POS, &link->pos,
         "namespace already imported here" );
   }
   else {
      link = mem_alloc( sizeof( *link ) );
      link->next = semantic->ns->links;
      link->ns = ns;
      link->pos = dirc->pos;
      semantic->ns->links = link;
   }
}

void import_selection( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc ) {
   list_iter_t i;
   list_iter_init( &i, &dirc->items );
   while ( ! list_end( &i ) ) {
      import_item( semantic, ns, list_data( &i ) );
      list_next( &i );
   }
}

void import_item( struct semantic* semantic, struct ns* ns,
   struct using_item* item ) {
   // Locate object.
   struct object* object = get_nsobject( ns, item->name );
   if ( ! object ) {
      s_unknown_ns_object( semantic, ns, item->name, &item->pos );
      s_bail( semantic );
   }
   // Bind object to name in current namespace.
   struct name* name = t_extend_name( semantic->ns->body, item->name );
   // Duplicate imports are allowed as long as both names refer to the
   // same object.
   if ( name->object == object ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS, &item->pos,
         "duplicate import-name `%s`", item->name );
      //s_diag( semantic, DIAG_DIAG_POS, &alias->object.pos,
      //   "import-name already used here" );
      return;
   }
   s_bind_name( semantic, name, object );
   item->imported_object = object;
}

struct object* s_follow_path( struct semantic* semantic, struct path* path ) {
   return follow_path( semantic, path, false );
}

struct object* follow_path( struct semantic* semantic, struct path* path,
   bool only_ns ) {
   struct ns* ns = NULL;
   struct object* object = NULL;
   if ( path->upmost ) {
      ns = semantic->lib->upmost_ns;
      object = &ns->object;
      path = path->next;
   }
   while ( path ) {
      object = ns ? get_nsobject( ns, path->text ) :
         s_search_object( semantic, path->text );
      if ( ! object ) {
         s_diag( semantic, DIAG_POS_ERR, &path->pos,
            "`%s` not found", path->text );
         s_bail( semantic );
      }
      if ( path->next || only_ns ) {
         if ( object->node.type != NODE_NAMESPACE ) {
            s_diag( semantic, DIAG_POS_ERR, &path->pos,
               "`%s` not a namespace", path->text );
            s_bail( semantic );
         }
         ns = ( struct ns* ) object;
      }
      path = path->next;
   }
   return object;
}

struct path* s_last_path_part( struct path* path ) {
   while ( path->next ) {
      path = path->next;
   }
   return path;
}

struct object* s_search_object( struct semantic* semantic,
   const char* object_name ) {
   struct ns* ns = semantic->ns;
   while ( ns ) {
      // Search in the namespace.
      struct name* name = t_extend_name( ns->body, object_name );
      if ( name->object ) {
         return name->object;
      }
      // Search in any of the linked namespaces.
      // NOTE: The linked namespaces and the parent namespaces of each linked
      // namespace are not searched.
      struct ns_link* link = ns->links;
      while ( link ) {
         struct object* object = get_nsobject( link->ns, object_name );
         if ( object ) {
            return object;
         }
         link = link->next;
      }
      // Search in the parent namespace.
      ns = ns->parent;
   }
   return NULL;
}

void test_objects( struct semantic* semantic ) {
   while ( true ) {
      semantic->retest_nss = false;
      semantic->resolved_objects = false;
      test_all( semantic );
      if ( semantic->retest_nss ) {
         // Continue resolving as long as something got resolved. If nothing
         // gets resolved in the previous run, then nothing can be resolved
         // anymore. So report errors.
         if ( ! semantic->resolved_objects ) {
            semantic->trigger_err = true;
         }
      }
      else {
         break;
      }
   }
}

void test_all( struct semantic* semantic ) {
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      test_namespace( semantic, lib->upmost_ns );
      list_next( &i );
   }
   test_namespace( semantic, semantic->lib->upmost_ns );
}

void test_namespace( struct semantic* semantic, struct ns* ns ) {
   semantic->ns = ns;
   show_private_objects( semantic );
   struct object* object = ns->unresolved;
   ns->unresolved = NULL;
   ns->unresolved_tail = NULL;
   while ( object ) {
      struct object* next_object = object->next;
      object->next = NULL;
      test_namespace_object( semantic, object );
      if ( object->resolved ) {
         semantic->resolved_objects = true;
      }
      else {
         t_append_unresolved_namespace_object( ns, object );
      }
      object = next_object;
   }
   if ( ! ns->unresolved ) {
      ns->object.resolved = true;
   }
   else {
      semantic->retest_nss = true;
   }
   hide_private_objects( semantic );
}

void test_nested_namespace( struct semantic* semantic, struct ns* ns ) {
   struct ns* parent_ns = semantic->ns;
   test_namespace( semantic, ns );
   semantic->ns = parent_ns;
}

void test_namespace_object( struct semantic* semantic,
   struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT:
      s_test_constant( semantic,
         ( struct constant* ) object );
      break;
   case NODE_ENUMERATION:
      s_test_enumeration( semantic,
         ( struct enumeration* ) object );
      break;
   case NODE_STRUCTURE:
      s_test_struct( semantic,
         ( struct structure* ) object );
      break;
   case NODE_TYPE_ALIAS:
      s_test_type_alias( semantic,
         ( struct type_alias* ) object );
      break;
   case NODE_VAR:
      s_test_var( semantic,
         ( struct var* ) object );
      break;
   case NODE_FUNC:
      s_test_func( semantic,
         ( struct func* ) object );
      break;
   case NODE_NAMESPACE:
      test_nested_namespace( semantic,
         ( struct ns* ) object );
      break;
   default:
      UNREACHABLE();
      if ( semantic->trigger_err ) {
         s_bail( semantic );
      }
   }
}

void test_objects_bodies( struct semantic* semantic ) {
   semantic->trigger_err = true;
   list_iter_t i;
   list_iter_init( &i, &semantic->task->library_main->namespaces );
   while ( ! list_end( &i ) ) {
      semantic->ns = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &semantic->ns->scripts );
      while ( ! list_end( &k ) ) {
         s_test_script( semantic, list_data( &k ) );
         list_next( &k );
      }
      list_iter_init( &k, &semantic->ns->funcs );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         if ( func->type == FUNC_USER ) {
            s_test_func_body( semantic, func );
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
   scope->ns_link = semantic->ns->links;
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
   semantic->ns->links = semantic->scope->ns_link;
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
   if ( object->node.type == NODE_STRUCTURE_MEMBER ) {
      s_diag( semantic, DIAG_POS_ERR, &object->pos,
         "duplicate struct member `%s`", str.value );
      s_diag( semantic, DIAG_POS, &name->object->pos,
         "struct member already found here", str.value );
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &object->pos,
         "duplicate name `%s`%s", str.value,
         implicitly_imported( object ) ?
            " (implicitly imported)" : ""  );
      s_diag( semantic, DIAG_POS,
         &name->object->pos, "name already used here%s",
         implicitly_imported( name->object ) ?
            " (implicitly imported)" : "" );
   }
   s_bail( semantic );
}

bool implicitly_imported( struct object* object ) {
   if ( object->node.type == NODE_ALIAS ) {
      struct alias* alias = ( struct alias* ) object;
      return alias->implicit;
   }
   return false;
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

// Retrieves an object from a namespace.
struct object* get_nsobject( struct ns* ns, const char* object_name ) {
   struct name* name = t_extend_name( ns->body, object_name );
   if ( name->object ) {
      struct object* object = name->object;
      while ( object->next_scope ) {
         object = object->next_scope;
      }
      if ( object->depth == 0 ) {
         return object;
      }
   }
   return NULL;
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

// Compile-time modules can only have constant information like constants,
// structure definitions, and builtin function definitions.
void confirm_compiletime_content( struct semantic* semantic ) {
/*
   list_iter_t i;
   list_iter_init( &i, &semantic->lib->objects );
   while ( ! list_end( &i ) ) {
      struct object* object = list_data( &i );
      if ( ! is_compiletime_object( object ) ) {
         s_diag( semantic, DIAG_POS_ERR, &object->pos,
            "runtime-time object found in compile-time library" );
         s_bail( semantic );
      }
      list_next( &i );
   }
*/
}

bool is_compiletime_object( struct object* object ) {
   if ( object->node.type == NODE_FUNC ) {
      struct func* func = ( struct func* ) object;
      return ( func->type != FUNC_USER );
   }
   else {
      switch ( object->node.type ) {
      case NODE_CONSTANT:
      case NODE_ENUMERATION:
      case NODE_STRUCTURE:
         return true;
      default:
         return false;
      }
   }
}

struct alias* s_alloc_alias( void ) {
   struct alias* alias = mem_alloc( sizeof( *alias ) );
   t_init_object( &alias->object, NODE_ALIAS );
   alias->target = NULL;
   alias->implicit = false;
   return alias;
}

void s_type_mismatch( struct semantic* semantic, const char* label_a,
   struct type_info* type_a, const char* label_b, struct type_info* type_b,
   struct pos* pos ) {
   struct str string_a;
   str_init( &string_a );
   s_present_type( type_a, &string_a );
   struct str string_b;
   str_init( &string_b );
   s_present_type( type_b, &string_b );
   s_diag( semantic, DIAG_POS_ERR, pos,
      "%s type (`%s`) different from %s type (`%s`)",
      label_a, string_a.value, label_b, string_b.value );
   str_deinit( &string_a );
   str_deinit( &string_b );
}