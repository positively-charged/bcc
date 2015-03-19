#include "phase.h"

struct local_alloc {
   int index;
   int func_size;
};

struct func_alloc {
   struct local_alloc* local;
   int start_index;
   int size;
};

struct alloc {
   struct codegen* phase;
   struct func_alloc* func;
};

static void visit_script( struct codegen* phase, struct script* script );
static void assign_nestedcalls_id( struct codegen* phase,
   struct func* nested_funcs );
static void visit_func( struct codegen* phase, struct func* func );
static void visit_block( struct alloc* alloc, struct block* block );
static void init_func_alloc( struct func_alloc* alloc, int start_index );
static void init_alloc( struct alloc* alloc, struct codegen* phase,
   struct func_alloc* func_alloc );
static void visit_node( struct alloc* alloc, struct node* node );
static void visit_for( struct alloc* alloc, struct for_stmt* stmt );
static void visit_call( struct alloc* alloc, struct call* call );
static void visit_nested_func( struct alloc* alloc, struct func* func );
static void visit_var( struct alloc* alloc, struct var* var );
static void visit_format_item( struct alloc* alloc, struct format_item* item );
static int alloc_scriptvar( struct alloc* alloc );
static void dealloc_lastscriptvar( struct alloc* alloc );

void t_alloc_indexes( struct codegen* phase ) {
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      visit_script( phase, script );
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &phase->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      visit_func( phase, func );
      struct func_user* impl = func->impl;
      list_next( &i );
   }
}

void assign_nestedcalls_id( struct codegen* phase,
   struct func* nested_funcs ) {
   struct func* nested_func = nested_funcs;
   while ( nested_func ) {
      struct func_user* nested_impl = nested_func->impl;
      struct call* call = nested_impl->nested_calls;
      int id = 0;
      while ( call ) {
         call->nested_call->id = id;
         ++id;
         call = call->nested_call->next;
      }
      nested_func = nested_impl->next_nested;
   }
}

void visit_script( struct codegen* phase, struct script* script ) {
   struct func_alloc func_alloc;
   init_func_alloc( &func_alloc, 0 );
   struct alloc alloc;
   init_alloc( &alloc, phase, &func_alloc );
   struct param* param = script->params;
   while ( param ) {
      param->index = func_alloc.start_index;
      ++func_alloc.start_index;
      ++func_alloc.size;
      param = param->next;
   }
   visit_node( &alloc, script->body );
   script->size = func_alloc.size;
   if ( script->nested_funcs ) {
      assign_nestedcalls_id( phase, script->nested_funcs );
   }
}

void visit_func( struct codegen* phase, struct func* func ) {
   struct func_alloc func_alloc;
   init_func_alloc( &func_alloc, 0 );
   struct alloc alloc;
   init_alloc( &alloc, phase, &func_alloc );
   struct param* param = func->params;
   while ( param ) {
      param->index = func_alloc.start_index;
      ++func_alloc.start_index;
      ++func_alloc.size;
      param = param->next;
   }
   struct func_user* impl = func->impl;
   visit_block( &alloc, impl->body );
   impl->size = func_alloc.size;
   if ( impl->nested_funcs ) {
      assign_nestedcalls_id( phase, impl->nested_funcs );
   }
}

void init_func_alloc( struct func_alloc* alloc, int start_index ) {
   alloc->local = NULL;
   alloc->start_index = start_index;
   alloc->size = 0;
}

void init_alloc( struct alloc* alloc, struct codegen* phase,
   struct func_alloc* func_alloc ) {
   alloc->phase = phase;
   alloc->func = func_alloc;
}

void visit_block( struct alloc* alloc, struct block* block ) {
   struct local_alloc* parent = alloc->func->local;
   struct local_alloc local;
   if ( parent ) {
      local.index = parent->index;
      local.func_size = parent->func_size;
   }
   else {
      local.index = alloc->func->start_index;
      local.func_size = alloc->func->size;
   }
   alloc->func->local = &local;
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      visit_node( alloc, list_data( &i ) );
      list_next( &i );
   }
   alloc->func->local = parent;
}

void visit_node( struct alloc* alloc, struct node* node ) {
   switch ( node->type ) {
   case NODE_UNARY: {
      struct unary* unary = ( struct unary* ) node;
      visit_node( alloc, unary->operand );
      break; }
   case NODE_BINARY: {
      struct binary* binary = ( struct binary* ) node;
      visit_node( alloc, binary->lside );
      visit_node( alloc, binary->rside );
      break; }
   case NODE_EXPR: {
      struct expr* expr = ( struct expr* ) node;
      visit_node( alloc, expr->root );
      break; }
   case NODE_IF: {
      struct if_stmt* stmt = ( struct if_stmt* ) node;
      visit_node( alloc, stmt->body );
      if ( stmt->else_body ) {
         visit_node( alloc, stmt->else_body );
      }
      break; }
   case NODE_WHILE: {
      struct while_stmt* stmt = ( struct while_stmt* ) node;
      visit_node( alloc, stmt->body );
      break; }
   case NODE_FOR:
      visit_for( alloc, ( struct for_stmt* ) node );
      break;
   case NODE_CALL:
      visit_call( alloc, ( struct call* ) node );
      break;
   case NODE_FORMAT_ITEM:
      visit_format_item( alloc, ( struct format_item* ) node );
      break;
   case NODE_FUNC:
      visit_nested_func( alloc, ( struct func* ) node );
      break;
   case NODE_ACCESS: {
      struct access* access = ( struct access* ) node;
      visit_node( alloc, access->lside );
      visit_node( alloc, access->rside );
      break; }
   case NODE_PAREN: {
      struct paren* paren = ( struct paren* ) node;
      visit_node( alloc, paren->inside );
      break; }
   case NODE_SUBSCRIPT: {
      struct subscript* sub = ( struct subscript* ) node;
      visit_node( alloc, sub->lside );
      visit_node( alloc, &sub->index->node );
      break; }
   case NODE_SWITCH: {
      struct switch_stmt* stmt = ( struct switch_stmt* ) node;
      visit_node( alloc, stmt->body );
      break; }
   case NODE_BLOCK:
      visit_block( alloc, ( struct block* ) node );
      break;
   case NODE_VAR:
      visit_var( alloc, ( struct var* ) node );
      break;
   case NODE_CONSTANT: {
      struct constant* constant = ( struct constant* ) node;
      if ( constant->value_node ) {
         visit_node( alloc, &constant->value_node->node );
      }
      break; }
   case NODE_RETURN: {
      struct return_stmt* stmt = ( struct return_stmt* ) node;
      if ( stmt->return_value ) {
         visit_node( alloc, &stmt->return_value->node );
      }
      break; }
   case NODE_PACKED_EXPR: {
      struct packed_expr* stmt = ( struct packed_expr* ) node;
      visit_node( alloc, &stmt->expr->node );
      if ( stmt->block ) {
         visit_block( alloc, stmt->block );
      }
      break; }
   case NODE_LITERAL:
   case NODE_INDEXED_STRING_USAGE:
   case NODE_JUMP:
   case NODE_SCRIPT_JUMP:
   case NODE_FORMAT_BLOCK_USAGE:
   case NODE_NAME_USAGE:
   case NODE_ASSIGN:
   case NODE_CASE:
   case NODE_CASE_DEFAULT:
   case NODE_GOTO:
   case NODE_GOTO_LABEL:
   case NODE_TYPE_MEMBER:
   case NODE_BOOLEAN:
   case NODE_REGION:
   case NODE_REGION_HOST:
   case NODE_REGION_UPMOST:
      // Ignored.
      break;
   default:
      t_unhandlednode_diag( alloc->phase->task, __FILE__, __LINE__, node );
      t_bail( alloc->phase->task );
   }
}

void visit_for( struct alloc* alloc, struct for_stmt* stmt ) {
   list_iter_t i;
   list_iter_init( &i, &stmt->init );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      if ( node->type == NODE_VAR ) {
         visit_node( alloc, node );
      }
      list_next( &i );
   }
   visit_node( alloc, stmt->body );
}

void visit_call( struct alloc* alloc, struct call* call ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   while ( ! list_end( &i ) ) {
      visit_node( alloc, list_data( &i ) );
      list_next( &i );
   }
}

void visit_nested_func( struct alloc* alloc, struct func* func ) {
   struct func_user* impl = func->impl;
   struct func_alloc* parent = alloc->func;
   impl->index_offset = parent->local->index;
   struct func_alloc func_alloc;
   init_func_alloc( &func_alloc, impl->index_offset );
   // Allocate index for each parameters.
   struct param* param = func->params;
   while ( param ) {
      param->index = func_alloc.start_index;
      ++func_alloc.start_index;
      ++func_alloc.size;
      param = param->next;
   }
   alloc->func = &func_alloc;
   visit_block( alloc, impl->body );
   impl->size = func_alloc.size;
   alloc->func = parent;
   int new_size = parent->local->func_size + func_alloc.size;
   if ( parent->size < new_size ) {
      parent->size = new_size;
   }
}

void visit_var( struct alloc* alloc, struct var* var ) {
   if ( var->storage == STORAGE_LOCAL ) {
      var->index = alloc_scriptvar( alloc );
   }
}

void visit_format_item( struct alloc* alloc, struct format_item* item ) {
   if ( item->cast == FCAST_ARRAY && item->extra ) {
      struct format_item_array* extra = item->extra;
      if ( extra->length ) {
         extra->offset_var = alloc_scriptvar( alloc );
         dealloc_lastscriptvar( alloc );
      }
   }
}

// Increases the space size of local variables by one, returning the index of
// the space slot.
int alloc_scriptvar( struct alloc* alloc ) {
   struct local_alloc* local = alloc->func->local;
   int index = local->index;
   ++local->index;
   ++local->func_size;
   if ( local->func_size > alloc->func->size ) {
      alloc->func->size = local->func_size;
   }
   return index;
}

// Decreases the space size of local variables by one.
void dealloc_lastscriptvar( struct alloc* alloc ) {
   --alloc->func->local->index;
   --alloc->func->local->func_size;
}