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
static void bind_object_name( struct semantic* phase, struct name*,
   struct object* object );
static void import_objects( struct semantic* phase );
static void test_objects( struct semantic* phase );
static void test_region( struct semantic* phase, bool* resolved, bool* retry );
static void test_region_object( struct semantic* phase,
   struct object* object );
static void test_objects_bodies( struct semantic* phase );
static void check_dup_scripts( struct semantic* phase );
static void calc_map_var_size( struct semantic* phase );
static void calc_map_value_index( struct semantic* phase );
static void count_string_usage( struct semantic* phase );
static void count_string_usage_node( struct semantic* phase,
   struct node* node );
static void count_string_usage_initial( struct semantic* phase,
   struct initial* initial );
static void add_loadable_libs( struct semantic* phase );

void s_init( struct semantic* phase, struct task* task ) {
   phase->task = task;
   phase->region = NULL;
   phase->scope = NULL;
   phase->free_scope = NULL;
   phase->free_sweep = NULL;
   phase->topfunc_test = NULL;
   phase->func_test = NULL;
   phase->depth = 0;
   phase->undef_err = false;
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
   count_string_usage( phase );
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
      bind_object_name( phase, constant->name, &constant->object );
      break; }
   case NODE_CONSTANT_SET: {
      struct constant_set* set = ( struct constant_set* ) object;
      struct constant* constant = set->head;
      while ( constant ) {
         bind_object_name( phase, constant->name, &constant->object );
         constant = constant->next;
      }
      break; }
   case NODE_VAR: {
      struct var* var = ( struct var* ) object;
      bind_object_name( phase, var->name, &var->object );
      break; }
   case NODE_FUNC: {
      struct func* func = ( struct func* ) object;
      bind_object_name( phase, func->name, &func->object );
      break; }
   case NODE_TYPE: {
      struct type* type = ( struct type* ) object;
      if ( type->name->object ) {
         diag_dup_struct( phase->task, type->name, &type->object.pos );
         s_bail( phase );
      }
      type->name->object = &type->object;
      break; }
   default:
      t_unhandlednode_diag( phase->task, __FILE__, __LINE__, &object->node );
      t_bail( phase->task );
   }
}

void bind_object_name( struct semantic* phase, struct name* name,
   struct object* object ) {
   if ( name->object ) {
      struct str str;
      str_init( &str );
      t_copy_name( name, false, &str );
      s_diag( phase, DIAG_POS_ERR, &object->pos,
         "duplicate name `%s`", str.value );
      s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &name->object->pos,
         "name already used here", str.value );
      s_bail( phase );
   }
   name->object = object;
}

// Executes import-statements found in regions.
void import_objects( struct semantic* phase ) {
   list_iter_t i;
   list_iter_init( &i, &phase->task->regions );
   while ( ! list_end( &i ) ) {
      phase->region = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &phase->region->imports );
      while ( ! list_end( &k ) ) {
         s_import( phase, list_data( &k ) );
         list_next( &k );
      }
      list_next( &i );
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
         phase->undef_err = ( ! resolved );
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
      s_test_type( phase, ( struct type* ) object );
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
   phase->undef_err = true;
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

// Counting the usage of strings is done so only strings that are used are
// outputted into the object file. There is no need to output the default
// arguments of the MorphActor() function if it's never called, say.
void count_string_usage( struct semantic* phase ) {
   // Scripts.
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      count_string_usage_node( phase, script->body );
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &phase->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      count_string_usage_node( phase, &impl->body->node );
      struct param* param = func->params;
      while ( param ) {
         if ( param->default_value ) {
            count_string_usage_node( phase, &param->default_value->node );
         }
         param = param->next;
      }
      list_next( &i );
   }
   // Variables.
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      count_string_usage_node( phase, list_data( &i ) );
      list_next( &i );
   }
}

void count_string_usage_node( struct semantic* phase, struct node* node ) {
   switch ( node->type ) {
   case NODE_UNARY: {
      struct unary* unary = ( struct unary* ) node;
      count_string_usage_node( phase, unary->operand );
      break; }
   case NODE_BINARY: {
      struct binary* binary = ( struct binary* ) node;
      count_string_usage_node( phase, binary->lside );
      count_string_usage_node( phase, binary->rside );
      break; }
   case NODE_EXPR: {
      struct expr* expr = ( struct expr* ) node;
      count_string_usage_node( phase, expr->root );
      break; }
   case NODE_INDEXED_STRING_USAGE: {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) node;
      usage->string->used = true;
      break; }
   case NODE_IF: {
      struct if_stmt* stmt = ( struct if_stmt* ) node;
      count_string_usage_node( phase, &stmt->cond->node );
      count_string_usage_node( phase, stmt->body );
      if ( stmt->else_body ) {
         count_string_usage_node( phase, stmt->else_body );
      }
      break; }
   case NODE_WHILE: {
      struct while_stmt* stmt = ( struct while_stmt* ) node;
      count_string_usage_node( phase, &stmt->cond->node );
      count_string_usage_node( phase, stmt->body );
      break; }
   case NODE_FOR: {
      struct for_stmt* stmt = ( struct for_stmt* ) node;
      // Initialization.
      list_iter_t i;
      list_iter_init( &i, &stmt->init );
      while ( ! list_end( &i ) ) {
         count_string_usage_node( phase, list_data( &i ) );
         list_next( &i );
      }
      // Condition.
      if ( stmt->cond ) {
         count_string_usage_node( phase, &stmt->cond->node );
      }
      // Post expression.
      list_iter_init( &i, &stmt->post );
      while ( ! list_end( &i ) ) {
         count_string_usage_node( phase, list_data( &i ) );
         list_next( &i );
      }
      // Body.
      count_string_usage_node( phase, stmt->body );
      break; }
   case NODE_CALL: {
      struct call* call = ( struct call* ) node;
      count_string_usage_node( phase, call->operand );
      list_iter_t i;
      list_iter_init( &i, &call->args );
      // Format arguments:
      if ( call->func->type == FUNC_FORMAT ) {
         while ( ! list_end( &i ) ) {
            struct node* node = list_data( &i );
            if ( node->type == NODE_FORMAT_ITEM ) {
               count_string_usage_node( phase, node );
            }
            else if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
               struct format_block_usage* usage =
                  ( struct format_block_usage* ) node;
               count_string_usage_node( phase, &usage->block->node );
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
         count_string_usage_node( phase, &expr->node );
         if ( param ) {
            param = param->next;
         }
         list_next( &i );
      }
      // Default arguments:
      while ( param ) {
         count_string_usage_node( phase, &param->default_value->node );
         param = param->next;
      }
      break; }
   case NODE_FORMAT_ITEM: {
      struct format_item* item = ( struct format_item* ) node;
      while ( item ) {
         count_string_usage_node( phase, &item->value->node );
         item = item->next;
      }
      break; }
   case NODE_ACCESS: {
      struct access* access = ( struct access* ) node;
      count_string_usage_node( phase, access->lside );
      count_string_usage_node( phase, access->rside );
      break; }
   case NODE_PAREN: {
      struct paren* paren = ( struct paren* ) node;
      count_string_usage_node( phase, paren->inside );
      break; }
   case NODE_CASE: {
      struct case_label* label = ( struct case_label* ) node;
      count_string_usage_node( phase, &label->number->node );
      break; }
   case NODE_SWITCH: {
      struct switch_stmt* stmt = ( struct switch_stmt* ) node;
      count_string_usage_node( phase, &stmt->cond->node );
      count_string_usage_node( phase, stmt->body );
      break; }
   case NODE_BLOCK: {
      struct block* block = ( struct block* ) node;
      list_iter_t i;
      list_iter_init( &i, &block->stmts );
      while ( ! list_end( &i ) ) {
         count_string_usage_node( phase, list_data( &i ) );
         list_next( &i );
      }
      break; }
   case NODE_VAR: {
      struct var* var = ( struct var* ) node;
      struct value* value = var->value;
      while ( value ) {
         if ( ! value->string_initz ) {
            count_string_usage_node( phase, &value->expr->node );
         }
         value = value->next;
      }
      break; }
   case NODE_ASSIGN: {
      struct assign* assign = ( struct assign* ) node;
      count_string_usage_node( phase, assign->lside );
      count_string_usage_node( phase, assign->rside );
      break; }
   case NODE_CONSTANT: {
      struct constant* constant = ( struct constant* ) node;
      // Enumerators might have an automatically generated value, so make sure
      // not to process those.
      if ( constant->value_node ) {
         count_string_usage_node( phase, &constant->value_node->node );
      }
      break; }
   case NODE_RETURN: {
      struct return_stmt* stmt = ( struct return_stmt* ) node;
      if ( stmt->return_value ) {
         count_string_usage_node( phase, &stmt->return_value->node );
      }
      break; }
   case NODE_NAME_USAGE: {
      struct name_usage* usage = ( struct name_usage* ) node;
      count_string_usage_node( phase, usage->object );
      break; }
   case NODE_PACKED_EXPR: {
      struct packed_expr* packed_expr = ( struct packed_expr* ) node;
      count_string_usage_node( phase, packed_expr->expr->root );
      break; }
   case NODE_NONE:
   case NODE_LITERAL:
   case NODE_JUMP:
   case NODE_SCRIPT_JUMP:
   case NODE_FORMAT_BLOCK_USAGE:
   case NODE_FUNC:
   case NODE_SUBSCRIPT:
   case NODE_CASE_DEFAULT:
   case NODE_GOTO:
   case NODE_GOTO_LABEL:
   case NODE_TYPE:
   case NODE_TYPE_MEMBER:
   case NODE_CONSTANT_SET:
   case NODE_PARAM:
   case NODE_PALTRANS:
   case NODE_ALIAS:
   case NODE_BOOLEAN:
   case NODE_IMPORT:
   case NODE_REGION:
   case NODE_REGION_HOST:
   case NODE_REGION_UPMOST:
   case NODE_SCRIPT:
      // Nothing to do with these nodes.
      break;
   default:
      t_unhandlednode_diag( phase->task, __FILE__, __LINE__, node );
      t_bail( phase->task );
   }
}

void count_string_usage_initial( struct semantic* phase,
   struct initial* initial ) {
   while ( initial ) {
      if ( initial->multi ) {
         struct multi_value* multi_value = ( struct multi_value* ) initial;
         count_string_usage_initial( phase, multi_value->body );
      }
      else {
         struct value* value = ( struct value* ) initial;
         count_string_usage_node( phase, &value->expr->node );
      }
      initial = initial->next;
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
}

void s_bind_local_name( struct semantic* phase, struct name* name,
   struct object* object ) {
   struct sweep* sweep = phase->scope->sweep;
   if ( ! sweep || sweep->size == SWEEP_MAX_SIZE ) {
      if ( phase->free_sweep ) {
         sweep = phase->free_sweep;
         phase->free_sweep = sweep->prev;
      }
      else {
         sweep = mem_alloc( sizeof( *sweep ) );
      }
      sweep->size = 0;
      sweep->prev = phase->scope->sweep;
      phase->scope->sweep = sweep; 
   }
   sweep->names[ sweep->size ] = name;
   ++sweep->size;
   object->depth = phase->depth;
   object->next_scope = name->object;
   name->object = object;
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