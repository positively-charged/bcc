#include <string.h>

#include "phase.h"

#define SWEEP_MAX_SIZE 20

struct sweep {
   struct sweep* prev;
   struct name* names[ SWEEP_MAX_SIZE ];
   int size;
};

struct scope {
   struct scope* prev;
   struct scope* prev_func_scope;
   struct sweep* sweep;
   struct ns_link* ns_link;
   short depth;
};

struct ns_link_retriever {
   struct scope* scope;
   struct ns_link* outer_link;
   struct ns_link* inner_link;
   struct ns_link* link;
};

static void init_worldglobal_vars( struct semantic* semantic );
static void test_acs( struct semantic* semantic );
static void test_module_acs( struct semantic* semantic, struct library* lib );
static void test_module_item_acs( struct semantic* semantic,
   struct node* node );
static void test_bcs( struct semantic* semantic );
static void test_imported_acs_libs( struct semantic* semantic );
static void bind_names( struct semantic* semantic );
static void bind_namespace( struct semantic* semantic,
   struct ns_fragment* fragment );
static void bind_namespace_object( struct semantic* semantic,
   struct object* object );
static void bind_enumeration( struct semantic* semantic,
   struct enumeration* enumeration );
static void bind_structure( struct semantic* semantic,
   struct structure* structure );
static void bind_var( struct semantic* semantic, struct var* var );
static void bind_func( struct semantic* semantic, struct func* func );
static void show_private_objects( struct semantic* semantic );
static void show_enumeration( struct semantic* semantic,
   struct enumeration* enumeration );
static void show_structure( struct semantic* semantic,
   struct structure* structure );
static void show_namespace( struct ns_fragment* fragment );
static void hide_private_objects( struct semantic* semantic );
static void hide_enumeration( struct semantic* semantic,
   struct enumeration* enumeration );
static void hide_structure( struct semantic* semantic,
   struct structure* structure );
static void hide_namespace( struct ns_fragment* fragment );
static void perform_usings( struct semantic* semantic );
static void perform_namespace_usings( struct semantic* semantic,
   struct ns_fragment* fragment );
static void import_all( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc );
static void import_selection( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc );
static void import_item( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc, struct using_item* item );
static void search_linked_object( struct semantic* semantic,
   struct object_search* search );
static void test_objects( struct semantic* semantic );
static void test_all( struct semantic* semantic );
static void test_lib( struct semantic* semantic, struct library* lib );
static void test_namespace( struct semantic* semantic,
   struct ns_fragment* fragment );
static void test_namespace_object( struct semantic* semantic,
   struct object* object );
static void test_objects_bodies( struct semantic* semantic );
static void test_objects_bodies_lib( struct semantic* semantic,
   struct library* lib );
static void test_objects_bodies_ns( struct semantic* semantic,
   struct ns_fragment* fragment );
static void check_dup_scripts( struct semantic* semantic );
static void match_dup_script( struct semantic* semantic, struct script* script,
   struct script* prev_script, bool imported );
static void assign_script_numbers( struct semantic* semantic );
static void bind_private_name( struct name* name, struct object* object );
static void bind_func_name( struct semantic* semantic, struct name* name,
   struct object* object );
static void bind_block_name( struct semantic* semantic, struct name* name,
   struct object* object );
static void dupname_err( struct semantic* semantic, struct name* name,
   struct object* object );
static void unbind_name( struct name* name, struct object* object );
static void insert_namespace_link( struct semantic* semantic,
   struct ns_link* link, bool block_scope );
static void init_ns_link_retriever( struct semantic* semantic,
   struct ns_link_retriever* retriever, struct ns* ns );
static void next_ns_link( struct ns_link_retriever* retriever );
static void dupnameglobal_err( struct semantic* semantic, struct name* name,
   struct object* object );
static void add_sweep_name( struct semantic* semantic, struct scope* scope,
   struct name* name, struct object* object );

void s_init( struct semantic* semantic, struct task* task ) {
   semantic->task = task;
   semantic->main_lib = task->library_main;
   semantic->lib = semantic->main_lib;
   semantic->ns = NULL;
   semantic->ns_fragment = NULL;
   semantic->scope = NULL;
   semantic->func_scope = NULL;
   semantic->free_scope = NULL;
   semantic->free_sweep = NULL;
   semantic->topfunc_test = NULL;
   semantic->func_test = NULL;
   semantic->lang_limits = t_get_lang_limits( semantic->lib->lang );
   init_worldglobal_vars( semantic );
   s_init_type_info_scalar( &semantic->type_int, SPEC_INT );
   semantic->depth = 0;
   semantic->retest_nss = false;
   semantic->resolved_objects = false;
   semantic->trigger_err = false;
   semantic->in_localscope = false;
   semantic->strong_type = false;
   semantic->lang = semantic->lib->lang;
}

static void init_worldglobal_vars( struct semantic* semantic ) {
   for ( int i = 0; i < ARRAY_SIZE( semantic->world_vars ); ++i ) {
      semantic->world_vars[ i ] = NULL;
      semantic->world_arrays[ i ] = NULL;
   }
   for ( int i = 0; i < ARRAY_SIZE( semantic->global_vars ); ++i ) {
      semantic->global_vars[ i ] = NULL;
      semantic->global_arrays[ i ] = NULL;
   }
}

int s_spec( struct semantic* semantic, int spec ) {
   if ( semantic->strong_type ) {
      return spec;
   }
   else {
      switch ( spec ) {
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_BOOL:
      case SPEC_STR:
         return SPEC_RAW;
      default:
         return spec;
      }
   }
}

void s_test( struct semantic* semantic ) {
   switch ( semantic->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      test_acs( semantic );
      break;
   default:
      test_bcs( semantic );
      break;
   }
   if ( list_size( &semantic->main_lib->scripts ) >
      semantic->lang_limits->max_scripts ) {
      s_diag( semantic, DIAG_FILE | DIAG_ERR, &semantic->main_lib->file_pos,
         "too many scripts (have %d, but maximum is %d)",
         list_size( &semantic->main_lib->scripts ),
         semantic->lang_limits->max_scripts );
      s_bail( semantic );
   }
   // NOTE: Some strings are generated in the codegen phase, so the check
   // should be performed there.
   if ( semantic->task->str_table.size > semantic->lang_limits->max_strings ) {
      s_diag( semantic, DIAG_FILE | DIAG_ERR, &semantic->main_lib->file_pos,
         "too many strings (have %d, but maximum is %d)",
         semantic->task->str_table.size,
         semantic->lang_limits->max_strings );
      s_bail( semantic );
   }
}

static void test_acs( struct semantic* semantic ) {
   semantic->trigger_err = true;
   // Test imported modules.
   struct list_iter i;
   list_iterate( &semantic->main_lib->dynamic_acs, &i );
   while ( ! list_end( &i ) ) {
      test_module_acs( semantic, list_data( &i ) );
      list_next( &i );
   }
   // Test module.
   test_module_acs( semantic, semantic->main_lib );
   check_dup_scripts( semantic );
   assign_script_numbers( semantic );
}

static void test_module_acs( struct semantic* semantic, struct library* lib ) {
   semantic->ns_fragment = lib->upmost_ns_fragment;
   semantic->ns = lib->upmost_ns_fragment->ns;
   // In ACS, one can use functions before they are declared.
   struct list_iter i;
   list_iterate( &lib->objects, &i );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      if ( node->type == NODE_FUNC ) {
         struct func* func = ( struct func* ) node;
         bind_namespace_object( semantic, &func->object );
         s_test_func( semantic, func );
      }
      list_next( &i );
   }
   list_iterate( &lib->objects, &i );
   while ( ! list_end( &i ) ) {
      test_module_item_acs( semantic, list_data( &i ) );
      list_next( &i );
   }
   // Constants created through #define are visible only in the library in
   // which they are created.
   list_iterate( &lib->objects, &i );
   while ( ! list_end( &i ) ) {
      struct object* object = list_data( &i );
      if ( object->node.type == NODE_CONSTANT ) {
         struct constant* constant = ( struct constant* ) object;
         if ( constant->hidden ) {
            constant->name->object = NULL;
         }
      }
      list_next( &i );
   }
}

static void test_module_item_acs( struct semantic* semantic,
   struct node* node ) {
   switch ( node->type ) {
      struct constant* constant;
      struct var* var;
      struct func* func;
   case NODE_CONSTANT:
      constant = ( struct constant* ) node;
      bind_namespace_object( semantic, &constant->object );
      s_test_constant( semantic, constant );
      break;
   case NODE_VAR:
      var = ( struct var* ) node;
      bind_namespace_object( semantic, &var->object );
      s_test_var( semantic, var );
      break;
   case NODE_FUNC:
      func = ( struct func* ) node;
      if ( func->type == FUNC_USER ) {
         struct func_user* impl = func->impl;
         if ( impl->body ) {
            s_test_func_body( semantic, func );
         }
      }
      break;
   case NODE_SCRIPT:
      s_test_script( semantic,
         ( struct script* ) node );
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
}

static void test_bcs( struct semantic* semantic ) {
   test_imported_acs_libs( semantic );
   bind_names( semantic );
   perform_usings( semantic );
   test_objects( semantic );
   test_objects_bodies( semantic );
   check_dup_scripts( semantic );
   assign_script_numbers( semantic );
   // TODO: Refactor this.
   if ( ! semantic->main_lib->importable ) {
      struct list_iter i;
      list_iterate( &semantic->main_lib->vars, &i );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         var->hidden = true;
         list_next( &i );
      }
   }
}

static void test_imported_acs_libs( struct semantic* semantic ) {
   semantic->trigger_err = true;
   struct list_iter i;
   list_iterate( &semantic->main_lib->dynamic_acs, &i );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib->lang == LANG_ACS ) {
         test_module_acs( semantic, lib );
      }
      list_next( &i );
   }
   semantic->trigger_err = false;
}

static void bind_names( struct semantic* semantic ) {
   struct list_iter i;
   list_iterate( &semantic->main_lib->dynamic_bcs, &i );
   while ( ! list_end( &i ) ) {
      semantic->lib = list_data( &i );
      bind_namespace( semantic, semantic->lib->upmost_ns_fragment );
      hide_private_objects( semantic );
      list_next( &i );
   }
   semantic->lib = semantic->main_lib;
   bind_namespace( semantic, semantic->lib->upmost_ns_fragment );
   hide_private_objects( semantic );
}

static void bind_namespace( struct semantic* semantic,
   struct ns_fragment* fragment ) {
   struct ns* ns = fragment->ns;
   while ( ns && ! ( ns->name->object &&
      ns->name->object->node.type == NODE_NAMESPACE ) ) {
      s_bind_name( semantic, ns->name, &ns->object );
      ns = ns->parent;
   }
   semantic->ns_fragment = fragment;
   semantic->ns = fragment->ns;
   struct list_iter i;
   list_iterate( &fragment->objects, &i );
   while ( ! list_end( &i ) ) {
      bind_namespace_object( semantic, list_data( &i ) );
      list_next( &i );
   }
}

static void bind_namespace_object( struct semantic* semantic,
   struct object* object ) {
   switch ( object->node.type ) {
      struct constant* constant;
      struct type_alias* type_alias;
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
      bind_var( semantic,
         ( struct var* ) object );
      break;
   case NODE_FUNC:
      bind_func( semantic,
         ( struct func* ) object );
      break;
   case NODE_NAMESPACEFRAGMENT:
      bind_namespace( semantic,
         ( struct ns_fragment* ) object );
      break;
   default:
      UNREACHABLE();
   }
}

static void bind_enumeration( struct semantic* semantic,
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

static void bind_structure( struct semantic* semantic,
   struct structure* structure ) {
   s_bind_name( semantic, structure->name, &structure->object );
   struct structure_member* member = structure->member;
   while ( member ) {
      s_bind_name( semantic, member->name, &member->object );
      member = member->next;
   }
}

static void bind_var( struct semantic* semantic, struct var* var ) {
   if ( ! var->anon ) {
      if ( var->name->object && var->name->object->node.type == NODE_VAR ) {
         struct var* binded_var = ( struct var* ) var->name->object;
         if ( binded_var->external ) {
            if ( var->external ) {
               return;
            }
            else {
               binded_var->name->object = NULL;
            }
         }
         else {
            if ( var->external ) {
               return;
            }
         }
      }
      s_bind_name( semantic, var->name, &var->object );
   }
}

static void bind_func( struct semantic* semantic, struct func* func ) {
   // Prefer to bind function definitions over declarations. If no definition
   // is found, bind the initial declaration.
   if ( func->name->object && func->name->object->node.type == NODE_FUNC ) {
      struct func* binded_func = ( struct func* ) func->name->object;
      if ( binded_func->external ) {
         // Keep initial declaration.
         if ( func->external ) {
            return;
         }
         // Replace declaration with definition.
         else {
            binded_func->name->object = NULL;
         }
      }
      else {
         if ( func->external ) {
            return;
         }
      }
   }
   s_bind_name( semantic, func->name, &func->object );
}

static void show_private_objects( struct semantic* semantic ) {
   struct list_iter i;
   list_iterate( &semantic->lib->private_objects, &i );
   while ( ! list_end( &i ) ) {
      struct object* object = list_data( &i );
      switch ( object->node.type ) {
      case NODE_CONSTANT:
         bind_private_name(
            ( ( struct constant* ) object )->name, object );
         break;
      case NODE_ENUMERATION:
         show_enumeration( semantic,
            ( struct enumeration* ) object );
         break;
      case NODE_STRUCTURE:
         show_structure( semantic,
            ( struct structure* ) object );
         break;
      case NODE_VAR:
         bind_private_name(
            ( ( struct var* ) object )->name, object );
         break;
      case NODE_FUNC:
         bind_private_name(
            ( ( struct func* ) object )->name, object );
         break;
      case NODE_TYPE_ALIAS:
         bind_private_name(
            ( ( struct type_alias* ) object )->name, object );
         break;
      case NODE_NAMESPACEFRAGMENT:
         show_namespace(
            ( struct ns_fragment* ) object );
         break;
      default:
         UNREACHABLE();
      }
      list_next( &i );
   }
}

static void show_enumeration( struct semantic* semantic,
   struct enumeration* enumeration ) {
   // Enumeration.
   if ( enumeration->name ) {
      bind_private_name( enumeration->name, &enumeration->object );
   }
   // Enumerators.
   struct enumerator* enumerator = enumeration->head;
   while ( enumerator ) {
      bind_private_name( enumerator->name, &enumerator->object );
      enumerator = enumerator->next;
   }
}

static void show_structure( struct semantic* semantic,
   struct structure* structure ) {
   // Structure.
   if ( structure->name ) {
      bind_private_name( structure->name, &structure->object );
   }
   // Members.
   struct structure_member* member = structure->member;
   while ( member ) {
      bind_private_name( member->name, &member->object );
      member = member->next;
   }
}

static void show_namespace( struct ns_fragment* fragment ) {
   // If all the fragments of a namespace are private, then the whole
   // namespace will be hidden. Reveal the namespace again.
   if ( fragment->ns->hidden ) {
      bind_private_name( fragment->ns->name, &fragment->ns->object );
   }
}

static void hide_private_objects( struct semantic* semantic ) {
   struct list_iter i;
   list_iterate( &semantic->lib->private_objects, &i );
   while ( ! list_end( &i ) ) {
      struct object* object = list_data( &i );
      switch ( object->node.type ) {
         struct constant* constant;
         struct var* var;
         struct func* func;
      case NODE_CONSTANT:
         unbind_name(
            ( ( struct constant* ) object )->name, object );
         break;
      case NODE_ENUMERATION:
         hide_enumeration( semantic,
            ( struct enumeration* ) object );
         break;
      case NODE_STRUCTURE:
         hide_structure( semantic,
            ( struct structure* ) object );
         break;
      case NODE_VAR:
         unbind_name(
            ( ( struct var* ) object )->name, object );
         break;
      case NODE_FUNC:
         unbind_name(
            ( ( struct func* ) object )->name, object );
         break;
      case NODE_TYPE_ALIAS:
         unbind_name(
            ( ( struct type_alias* ) object )->name, object );
         break;
      case NODE_NAMESPACEFRAGMENT:
         hide_namespace(
            ( struct ns_fragment* ) object );
         break;
      default:
         UNREACHABLE();
      }
      list_next( &i );
   }
}

static void hide_enumeration( struct semantic* semantic,
   struct enumeration* enumeration ) {
   if ( enumeration->name ) {
      unbind_name( enumeration->name, &enumeration->object );
   }
   struct enumerator* enumerator = enumeration->head;
   while ( enumerator ) {
      unbind_name( enumerator->name, &enumerator->object );
      enumerator = enumerator->next;
   }
}

static void hide_structure( struct semantic* semantic,
   struct structure* structure ) {
   if ( structure->name ) {
      unbind_name( structure->name, &structure->object );
   }
   struct structure_member* member = structure->member;
   while ( member ) {
      unbind_name( member->name, &member->object );
      member = member->next;
   }
}

static void hide_namespace( struct ns_fragment* fragment ) {
   // Hide the namespace when all of its fragments are private.
   if ( fragment->ns->hidden ) {
      unbind_name( fragment->ns->name, &fragment->ns->object );
   }
}

static void perform_usings( struct semantic* semantic ) {
   // Execute using directives in imported libraries.
   struct list_iter i;
   list_iterate( &semantic->main_lib->dynamic_bcs, &i );
   while ( ! list_end( &i ) ) {
      semantic->lib = list_data( &i );
      perform_namespace_usings( semantic, semantic->lib->upmost_ns_fragment );
      list_next( &i );
   }
   // Execute using directives in main library.
   semantic->lib = semantic->main_lib;
   perform_namespace_usings( semantic,
      semantic->main_lib->upmost_ns_fragment );
}

static void perform_namespace_usings( struct semantic* semantic,
   struct ns_fragment* fragment ) {
   struct ns_fragment* parent_fragment = semantic->ns_fragment;
   semantic->ns_fragment = fragment;
   semantic->ns = fragment->ns;
   struct list_iter i;
   list_iterate( &fragment->usings, &i );
   while ( ! list_end( &i ) ) {
      s_perform_using( semantic, list_data( &i ) );
      list_next( &i );
   }
   list_iterate( &fragment->fragments, &i );
   while ( ! list_end( &i ) ) {
      perform_namespace_usings( semantic, list_data( &i ) );
      list_next( &i );
   }
   semantic->ns_fragment = parent_fragment;
   semantic->ns = parent_fragment->ns;
}

void s_perform_using( struct semantic* semantic, struct using_dirc* dirc ) {
   struct follower follower;
   s_init_follower( &follower, dirc->path, NODE_NAMESPACE );
   s_follow_path( semantic, &follower );
   switch ( dirc->type ) {
   case USING_ALL:
      import_all( semantic, follower.result.ns, dirc );
      break;
   case USING_SELECTION:
      import_selection( semantic, follower.result.ns, dirc );
      break;
   default:
      UNREACHABLE()
   }
}

static void import_all( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc ) {
   if ( ns == semantic->ns ) {
      s_diag( semantic, DIAG_POS_ERR, &dirc->pos,
         "namespace importing itself" );
      s_bail( semantic );
   }
   struct ns_link_retriever links;
   init_ns_link_retriever( semantic, &links, semantic->ns );
   next_ns_link( &links );
   while ( links.link && links.link->ns != ns ) {
      next_ns_link( &links );
   }
   // Duplicate links are allowed, but warn the user about them.
   if ( links.link ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS, &dirc->pos,
         "duplicate namespace import" );
      s_diag( semantic, DIAG_POS, &links.link->pos,
         "namespace already imported here" );
   }
   else {
      struct ns_link* link = mem_alloc( sizeof( *link ) );
      link->next = NULL;
      link->ns = ns;
      link->pos = dirc->pos;
      insert_namespace_link( semantic, link, dirc->force_local_scope );
   }
}

static void import_selection( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc ) {
   struct list_iter i;
   list_iterate( &dirc->items, &i );
   while ( ! list_end( &i ) ) {
      import_item( semantic, ns, dirc, list_data( &i ) );
      list_next( &i );
   }
}

static void import_item( struct semantic* semantic, struct ns* ns,
   struct using_dirc* dirc, struct using_item* item ) {
   // Locate object.
   struct object* object;
   switch ( item->type ) {
   case USINGITEM_STRUCT:
      object = s_get_ns_object( ns, item->name, NODE_STRUCTURE );
      if ( ! object ) {
         s_diag( semantic, DIAG_POS_ERR, &item->pos,
            "struct `%s` not found", item->name );
         s_bail( semantic );
      }
      break;
   case USINGITEM_ENUM:
      object = s_get_ns_object( ns, item->name, NODE_ENUMERATION );
      if ( ! object ) {
         s_diag( semantic, DIAG_POS_ERR, &item->pos,
            "enum `%s` not found", item->name );
         s_bail( semantic );
      }
      if ( object->node.type != NODE_ENUMERATION ) {
         s_diag( semantic, DIAG_POS_ERR, &item->pos,
            "`%s` not an enum", item->name );
         s_bail( semantic );
      }
      break;
   default:
      object = s_get_ns_object( ns, item->name, NODE_NONE );
      if ( ! object ) {
         s_unknown_ns_object( semantic, ns, item->name, &item->pos );
         s_bail( semantic );
      }
   }
   // Bind object to name in current namespace.
   struct name* body;
   switch ( item->type ) {
   case USINGITEM_STRUCT:
      body = semantic->ns->body_structs;
      break;
   case USINGITEM_ENUM:
      body = semantic->ns->body_enums;
      break;
   default:
      body = semantic->ns->body;
   }
   struct name* name = t_extend_name( body, item->usage_name );
   // Duplicate imports are allowed as long as both names refer to the same
   // object.
   if ( name->object && name->object->node.type == NODE_ALIAS ) {
      struct alias* alias = ( struct alias* ) name->object;
      if ( alias->target == object ) {
         s_diag( semantic, DIAG_POS | DIAG_WARN, &item->pos,
            "duplicate import of %s`%s`",
            item->type == USINGITEM_STRUCT ? "struct " :
            item->type == USINGITEM_ENUM ? "enum " : "", item->name );
         s_diag( semantic, DIAG_POS, &alias->object.pos,
            "import already made here" );
         item->alias = alias;
         return;
      }
   }
   struct alias* alias = s_alloc_alias();
   alias->object.pos = item->pos;
   alias->object.resolved = true;
   alias->target = object;
   if ( semantic->in_localscope ) {
      s_bind_local_name( semantic, name, &alias->object,
         dirc->force_local_scope );
   }
   else {
      s_bind_name( semantic, name, &alias->object );
   }
   item->alias = alias;
}

// Requested node must be one of the following:
// - NODE_STRUCTURE
// - NODE_ENUMERATION
// - NODE_NAMESPACE
// - NODE_NONE (Any other object)
void s_init_follower( struct follower* follower, struct path* path,
   int requested_node ) {
   follower->path = path;
   follower->result.object = NULL;
   follower->requested_node = requested_node;
}

void s_follow_path( struct semantic* semantic, struct follower* follower ) {
   struct object* object = NULL;
   struct path* path = follower->path;
   // Multi-part path.
   if ( path->next ) {
      // Head.
      struct ns* ns = NULL;
      if ( path->upmost ) {
         ns = semantic->task->upmost_ns;
         path = path->next;
      }
      else if ( path->current_ns ) {
         ns = semantic->ns;
         path = path->next;
      }
      else {
         struct object_search search;
         s_init_object_search( &search, NODE_NONE, &path->pos, path->text );
         s_search_object( semantic, &search );
         object = search.object;
         if ( ! object ) {
            s_diag( semantic, DIAG_POS_ERR, &path->pos,
               "`%s` not found", path->text );
            s_bail( semantic );
         }
         if ( object->node.type != NODE_NAMESPACE ) {
            s_diag( semantic, DIAG_POS_ERR, &path->pos,
               "`%s` not a namespace", path->text );
            s_bail( semantic );
         }
         ns = ( struct ns* ) object;
         path = path->next;
      }
      // Middle.
      while ( path->next ) {
         object = s_get_ns_object( ns, path->text, NODE_NONE );
         if ( ! object ) {
            s_diag( semantic, DIAG_POS_ERR, &path->pos,
               "`%s` not found", path->text );
            s_bail( semantic );
         }
         if ( object->node.type != NODE_NAMESPACE ) {
            s_diag( semantic, DIAG_POS_ERR, &path->pos,
               "`%s` not a namespace", path->text );
            s_bail( semantic );
         }
         ns = ( struct ns* ) object;
         path = path->next;
      }
      // Tail.
      object = s_get_ns_object( ns, path->text, follower->requested_node );
   }
   // Single-part path.
   else {
      if ( path->upmost ) {
         object = &semantic->task->upmost_ns->object;
      }
      else if ( path->current_ns ) {
         object = &semantic->ns->object;
      }
      else {
         struct object_search search;
         s_init_object_search( &search, follower->requested_node, &path->pos,
            path->text );
         s_search_object( semantic, &search );
         object = search.object;
      }
   }
   // Done.
   if ( ! object ) {
      const char* prefix;
      switch ( follower->requested_node ) {
      case NODE_STRUCTURE:
         prefix = "struct ";
         break;
      case NODE_ENUMERATION:
         prefix = "enum ";
      default:
         prefix = "";
      }
      s_diag( semantic, DIAG_POS_ERR, &path->pos,
         "%s`%s` not found", prefix, path->text );
      s_bail( semantic );
   }
   if ( follower->requested_node == NODE_NAMESPACE &&
      object->node.type != NODE_NAMESPACE ) {
      s_diag( semantic, DIAG_POS_ERR, &path->pos,
         "`%s` not a namespace", path->text );
      s_bail( semantic );
   }
   follower->result.object = object;
   follower->path = path;
}

void s_init_object_search( struct object_search* search, int requested_node,
   struct pos* pos, const char* name ) {
   search->ns = NULL;
   search->name = name;
   search->pos = pos;
   search->object = NULL;
   search->requested_node = requested_node;
}

void s_search_object( struct semantic* semantic,
   struct object_search* search ) {
   search->ns = semantic->ns;
   while ( search->ns ) {
      // Search in the namespace.
      struct name* body;
      switch ( search->requested_node ) {
      case NODE_STRUCTURE:
         body = search->ns->body_structs;
         break;
      case NODE_ENUMERATION:
         body = search->ns->body_enums;
         break;
      default:
         body = search->ns->body;
      }
      struct name* name = t_extend_name( body, search->name );
      if ( name->object ) {
         search->object = name->object;
         break;
      }
      // Search in any of the linked namespaces.
      search_linked_object( semantic, search );
      if ( search->object ) {
         break;
      }
      // Search in the parent namespace.
      search->ns = search->ns->parent;
   }
   if ( search->object && search->object->node.type == NODE_ALIAS ) {
      struct alias* alias = ( struct alias* ) search->object;
      search->object = alias->target;
   }
}

static void search_linked_object( struct semantic* semantic,
   struct object_search* search ) {
   struct ns_link_retriever links;
   init_ns_link_retriever( semantic, &links, search->ns );
   next_ns_link( &links );
   while ( links.link && ! search->object ) {
      search->object = s_get_ns_object( links.link->ns, search->name,
         search->requested_node );
      next_ns_link( &links );
   }
   // Make sure no other object with the same name can be found.
   while ( links.link && ! s_get_ns_object( links.link->ns, search->name,
      search->requested_node ) ) {
      next_ns_link( &links );
   }
   if ( links.link ) {
      const char* prefix;
      switch ( search->requested_node ) {
      case NODE_STRUCTURE:
         prefix = "struct ";
         break;
      case NODE_ENUMERATION:
         prefix = "enum ";
         break;
      default:
         prefix = "";
      }
      s_diag( semantic, DIAG_POS_ERR, search->pos,
         "multiple instances of %s`%s` found (you must choose one)",
         prefix, search->name );
      s_diag( semantic, DIAG_POS, &search->object->pos,
         "%s`%s` found here", prefix, search->name );
      while ( links.link ) {
         struct object* object = s_get_ns_object( links.link->ns, search->name,
            search->requested_node );
         if ( object ) {
            s_diag( semantic, DIAG_POS, &object->pos,
               "another %s`%s` found here", prefix, search->name );
         }
         next_ns_link( &links );
      }
      s_bail( semantic );
   }
}

// Retrieves an object from a namespace.
struct object* s_get_ns_object( struct ns* ns, const char* object_name,
   int requested_node ) {
   struct name* body;
   switch ( requested_node ) {
   case NODE_STRUCTURE:
      body = ns->body_structs;
      break;
   case NODE_ENUMERATION:
      body = ns->body_enums;
      break;
   default:
      body = ns->body;
   }
   struct name* name = t_extend_name( body, object_name );
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

static void test_objects( struct semantic* semantic ) {
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

static void test_all( struct semantic* semantic ) {
   struct list_iter i;
   list_iterate( &semantic->main_lib->dynamic_bcs, &i );
   while ( ! list_end( &i ) ) {
      test_lib( semantic, list_data( &i ) );
      list_next( &i );
   }
   test_lib( semantic, semantic->main_lib );
}

static void test_lib( struct semantic* semantic, struct library* lib ) {
   struct library* prev_lib = semantic->lib;
   semantic->lib = lib;
   show_private_objects( semantic );
   test_namespace( semantic, lib->upmost_ns_fragment );
   hide_private_objects( semantic );
   semantic->lib = prev_lib;
}

static void test_namespace( struct semantic* semantic,
   struct ns_fragment* fragment ) {
   semantic->ns = fragment->ns;
   semantic->ns_fragment = fragment;
   semantic->strong_type = fragment->strict;
   struct object* object = fragment->unresolved;
   fragment->unresolved = NULL;
   fragment->unresolved_tail = NULL;
   while ( object ) {
      struct object* next_object = object->next;
      object->next = NULL;
      test_namespace_object( semantic, object );
      if ( object->resolved ) {
         semantic->resolved_objects = true;
      }
      else {
         t_append_unresolved_namespace_object( fragment, object );
      }
      object = next_object;
   }
   if ( ! fragment->unresolved ) {
      fragment->object.resolved = true;
   }
   else {
      semantic->retest_nss = true;
   }
}

static void test_nested_namespace( struct semantic* semantic,
   struct ns_fragment* fragment ) {
   struct ns_fragment* parent_fragment = semantic->ns_fragment;
   test_namespace( semantic, fragment );
   semantic->ns_fragment = parent_fragment;
   semantic->ns = parent_fragment->ns;
   semantic->strong_type = parent_fragment->strict;
}

static void test_namespace_object( struct semantic* semantic,
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
   case NODE_NAMESPACEFRAGMENT:
      test_nested_namespace( semantic,
         ( struct ns_fragment* ) object );
      break;
   default:
      UNREACHABLE();
      if ( semantic->trigger_err ) {
         s_bail( semantic );
      }
   }
}

static void test_objects_bodies( struct semantic* semantic ) {
   semantic->trigger_err = true;
   struct list_iter i;
   list_iterate( &semantic->main_lib->dynamic_bcs, &i );
   while ( ! list_end( &i ) ) {
      test_objects_bodies_lib( semantic, list_data( &i ) );
      list_next( &i );
   }
   test_objects_bodies_lib( semantic, semantic->main_lib );
}

static void test_objects_bodies_lib( struct semantic* semantic,
   struct library* lib ) {
   struct library* prev_lib = semantic->lib;
   semantic->lib = lib;
   show_private_objects( semantic );
   test_objects_bodies_ns( semantic, lib->upmost_ns_fragment );
   hide_private_objects( semantic );
   semantic->lib = prev_lib;
}

static void test_objects_bodies_ns( struct semantic* semantic,
   struct ns_fragment* fragment ) {
   struct ns_fragment* parent_fragment = semantic->ns_fragment;
   semantic->ns = fragment->ns;
   semantic->ns_fragment = fragment;
   semantic->strong_type = fragment->strict;
   struct list_iter i;
   list_iterate( &fragment->runnables, &i );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      switch ( node->type ) {
      case NODE_SCRIPT:
         s_test_script( semantic,
            ( struct script* ) node );
         break;
      case NODE_FUNC:
         // Test only the bodies of functions found in the main library. For
         // an imported library, function bodies are stripped away during
         // parsing, so there is nothing to test.
         if ( semantic->lib == semantic->main_lib ) {
            struct func* func = ( struct func* ) node;
            if ( func->type == FUNC_USER ) {
               s_test_func_body( semantic, func );
            }
         }
         break;
      case NODE_NAMESPACEFRAGMENT:
         test_objects_bodies_ns( semantic,
            ( struct ns_fragment* ) node );
         break;
      default:
         UNREACHABLE();
         s_bail( semantic );
      }
      list_next( &i );
   }
   semantic->ns_fragment = parent_fragment;
   semantic->ns = parent_fragment->ns;
   semantic->strong_type = parent_fragment->strict;
}

static void check_dup_scripts( struct semantic* semantic ) {
   struct list_iter i;
   list_iterate( &semantic->main_lib->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct list_iter k = i;
      list_next( &k );
      while ( ! list_end( &k ) ) {
         match_dup_script( semantic, list_data( &k ), list_data( &i ), false );
         list_next( &k );
      }
      // Check for duplicates in an imported library.
      struct list_iter j;
      list_iterate( &semantic->main_lib->dynamic, &j );
      while ( ! list_end( &j ) ) {
         struct library* lib = list_data( &j );
         list_iterate( &lib->scripts, &k );
         while ( ! list_end( &k ) ) {
            match_dup_script( semantic, list_data( &i ), list_data( &k ),
               true );
            list_next( &k );
         }
         list_next( &j );
      }
      list_next( &i );
   }
}

static void match_dup_script( struct semantic* semantic, struct script* script,
   struct script* prev_script, bool imported ) {
   // Script names.
   if ( script->named_script && prev_script->named_script ) {
      struct indexed_string* name =
         t_lookup_string( semantic->task, script->number->value );
      struct indexed_string* prev_name =
         t_lookup_string( semantic->task, prev_script->number->value );
      if ( strcasecmp( name->value, prev_name->value ) == 0 ) {
         if ( imported ) {
            s_diag( semantic, DIAG_POS | DIAG_WARN, &script->pos,
               "script \"%s\" already found in an imported library",
               name->value );
            s_diag( semantic, DIAG_POS, &prev_script->pos,
               "script in imported library found here" );
         }
         else {
            s_diag( semantic, DIAG_POS_ERR, &script->pos,
               "duplicate script \"%s\"", name->value );
            s_diag( semantic, DIAG_POS, &prev_script->pos,
               "script already found here" );
            s_bail( semantic );
         }
      }
   }
   // Script numbers.
   else if ( ! script->named_script && ! prev_script->named_script ) {
      if ( script->number->value == prev_script->number->value ) {
         if ( imported ) {
            s_diag( semantic, DIAG_POS | DIAG_WARN, &script->pos,
               "script %d already found in an imported library",
               script->number->value );
            s_diag( semantic, DIAG_POS, &prev_script->pos,
               "script in imported library found here" );
         }
         else {
            s_diag( semantic, DIAG_POS_ERR, &script->pos,
               "duplicate script %d", script->number->value );
            s_diag( semantic, DIAG_POS, &prev_script->pos,
               "script already found here" );
            s_bail( semantic );
         }
      }
   }
}

static void assign_script_numbers( struct semantic* semantic ) {
   int named_script_number = -1;
   struct list_iter i;
   list_iterate( &semantic->main_lib->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->named_script ) {
         script->assigned_number = named_script_number;
         --named_script_number;
      }
      else {
         script->assigned_number = script->number->value;
      }
      list_next( &i );
   }
}

void s_add_scope( struct semantic* semantic, bool func_scope ) {
   struct scope* scope;
   if ( semantic->free_scope ) {
      scope = semantic->free_scope;
      semantic->free_scope = scope->prev;
   }
   else {
      scope = mem_alloc( sizeof( *scope ) );
   }
   ++semantic->depth;
   scope->prev = semantic->scope;
   scope->sweep = NULL;
   scope->ns_link = NULL;
   scope->depth = semantic->depth;
   semantic->scope = scope;
   semantic->in_localscope = ( semantic->depth > 0 );
   if ( func_scope ) {
      scope->prev_func_scope = semantic->func_scope;
      semantic->func_scope = scope;
   }
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
   struct scope* scope = semantic->scope;
   semantic->scope = scope->prev;
   if ( scope == semantic->func_scope ) {
      semantic->func_scope = scope->prev_func_scope;
   }
   scope->prev = semantic->free_scope;
   semantic->free_scope = scope;
   --semantic->depth;
   semantic->in_localscope = ( semantic->depth > 0 );
}

// Namespace scope.
void s_bind_name( struct semantic* semantic, struct name* name,
   struct object* object ) {
   if ( name->object ) {
      dupname_err( semantic, name, object );
   }
   name->object = object;
}

// Binds namespace-level private objects.
static void bind_private_name( struct name* name, struct object* object ) {
   if ( object != name->object ) {
      object->next_scope = name->object;
      name->object = object;
   }
}

// Local scope.
void s_bind_local_name( struct semantic* semantic, struct name* name,
   struct object* object, bool block_scope ) {
   if ( block_scope ) {
      bind_block_name( semantic, name, object );
   }
   else {
      // In ACS, an object in function scope cannot hide an object that is in
      // global (namespace) scope.
      if ( semantic->lang == LANG_ACS && name->object &&
         name->object->depth == 0 ) {
         dupnameglobal_err( semantic, name, object );
      }
      bind_func_name( semantic, name, object );
   }
}

// Function scope.
static void bind_func_name( struct semantic* semantic, struct name* name,
   struct object* object ) {
   if ( ! name->object || name->object->depth < semantic->func_scope->depth ) {
      add_sweep_name( semantic, semantic->func_scope, name, object );
   }
   else {
      dupname_err( semantic, name, object );
   }
}

// Local scope.
static void bind_block_name( struct semantic* semantic, struct name* name,
   struct object* object ) {
   if ( ! name->object || name->object->depth < semantic->depth ) {
      add_sweep_name( semantic, semantic->scope, name, object );
   }
   else {
      dupname_err( semantic, name, object );
   }
}

static void dupname_err( struct semantic* semantic, struct name* name,
   struct object* object ) {
   const char* category =
      ( object->node.type == NODE_STRUCTURE ) ? "struct" :
      ( object->node.type == NODE_STRUCTURE_MEMBER ) ? "struct member" :
      ( object->node.type == NODE_ENUMERATION ) ? "enum" :
      "object";
   struct str object_name;
   str_init( &object_name );
   t_copy_name( name, false, &object_name );
   s_diag( semantic, DIAG_POS_ERR, &object->pos,
      "duplicate %s name, `%s`", category, object_name.value );
   if ( name->object->pos.id == INTERNALFILE_COMPILER ) {
      s_diag( semantic, DIAG_POS | DIAG_NOTE, &name->object->pos,
         "`%s` is the name of a builtin %s", object_name.value,
         ( name->object->node.type == NODE_FUNC ) ? "function" : "object" );         
   }
   else {
      s_diag( semantic, DIAG_POS | DIAG_NOTE, &name->object->pos,
         "`%s` is the name of this %s", object_name.value, category );
   }
   s_bail( semantic );
}

static void unbind_name( struct name* name, struct object* object ) {
   // Only unbind the object if it is binded.
   if ( object == name->object ) {
      name->object = object->next_scope;
      object->next_scope = NULL;
   }
}

static void insert_namespace_link( struct semantic* semantic,
   struct ns_link* link, bool block_scope ) {
   if ( semantic->in_localscope ) {
      struct scope* target_scope = ( block_scope ) ?
         semantic->scope :
         semantic->func_scope;
      link->next = target_scope->ns_link;
      target_scope->ns_link = link;
   }
   else {
      link->next = semantic->ns->links;
      semantic->ns->links = link;
   }
}

static void init_ns_link_retriever( struct semantic* semantic,
   struct ns_link_retriever* retriever, struct ns* ns ) {
   retriever->scope = NULL;
   retriever->outer_link = ns->links;
   retriever->inner_link = NULL;
   retriever->link = NULL;
   // Only go over the local namespace links when examining the current
   // namespace. 
   if ( ns == semantic->ns && semantic->scope ) {
      retriever->scope = semantic->scope;
      retriever->inner_link = retriever->scope->ns_link;
   }
}

static void next_ns_link( struct ns_link_retriever* retriever ) {
   // Look for namespace link in local scopes.
   while ( retriever->scope ) {
      if ( retriever->inner_link ) {
         retriever->link = retriever->inner_link;
         retriever->inner_link = retriever->inner_link->next;
         return;
      }
      if ( retriever->scope->prev ) {
         retriever->scope = retriever->scope->prev;
         retriever->inner_link = retriever->scope->ns_link;
      }
      else {
         retriever->scope = NULL;
      }
   }
   // Look for namespace link in namespaces.
   if ( retriever->outer_link ) {
      retriever->link = retriever->outer_link;
      retriever->outer_link = retriever->outer_link->next;
      return;
   }
   // Nothing more.
   retriever->link = NULL;
}

static void dupnameglobal_err( struct semantic* semantic, struct name* name,
   struct object* object ) {
   struct str object_name;
   str_init( &object_name );
   t_copy_name( name, false, &object_name );
   s_diag( semantic, DIAG_POS_ERR, &object->pos,
      "local object has the same name, `%s`, as a global object",
      object_name.value );
   s_diag( semantic, DIAG_POS, &name->object->pos,
      "`%s` is the name of this global object", object_name.value );
   s_bail( semantic );
}

static void add_sweep_name( struct semantic* semantic, struct scope* scope,
   struct name* name, struct object* object ) {
   struct sweep* sweep = scope->sweep;
   if ( ! sweep || sweep->size == SWEEP_MAX_SIZE ) {
      if ( semantic->free_sweep ) {
         sweep = semantic->free_sweep;
         semantic->free_sweep = sweep->prev;
      }
      else {
         sweep = mem_alloc( sizeof( *sweep ) );
      }
      sweep->size = 0;
      sweep->prev = scope->sweep;
      scope->sweep = sweep; 
   }
   sweep->names[ sweep->size ] = name;
   ++sweep->size;
   object->depth = scope->depth;
   object->next_scope = name->object;
   name->object = object;
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

struct alias* s_alloc_alias( void ) {
   struct alias* alias = mem_alloc( sizeof( *alias ) );
   s_init_alias( alias );
   return alias;
}

void s_init_alias( struct alias* alias ) {
   t_init_object( &alias->object, NODE_ALIAS );
   t_init_pos_id( &alias->object.pos, INTERNALFILE_COMPILER );
   alias->target = NULL;
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
