#include "task.h"

#include <stdio.h>

struct operand {
   struct type* type;
   struct dim* dim;
   enum {
      ACTION_PUSH_VALUE,
      ACTION_PUSH_VAR
   } action;
   enum {
      METHOD_NONE,
      METHOD_INDEXED,
      METHOD_ELEMENT
   } method;
   enum {
      TRIGGER_INDEX
   } trigger;
   int storage;
   int index;
   int base;
   bool push;
   bool push_temp;
   bool pushed;
   bool pushed_element;
};

struct block_visit {
   struct block_visit* prev;
   struct format_block_usage* format_block_usage;
   int size;
   int size_high;
};

static void visit_block( struct task*, struct block* );
static void add_block_visit( struct task* );
static void pop_block_visit( struct task* );
static int alloc_script_var( struct task* );
static void dealloc_last_script_var( struct task* );
static void visit_var( struct task*, struct var* );
static void init_world_array( struct task*, struct var* );
static void visit_node( struct task*, struct node* );
static void visit_script_jump( struct task*, struct script_jump* );
static void visit_label( struct task*, struct label* );
static void visit_goto( struct task*, struct goto_stmt* );
static void visit_expr( struct task*, struct expr* );
static void visit_packed_expr( struct task*, struct packed_expr* );
static void init_operand( struct operand* );
static void push_expr( struct task*, struct expr*, bool );
static void visit_operand( struct task*, struct operand*, struct node* );
static void visit_constant( struct task*, struct operand*, struct constant* );
static void visit_unary( struct task*, struct operand*, struct unary* );
static void visit_pre_inc( struct task*, struct operand*, struct unary* );
static void visit_post_inc( struct task*, struct operand*, struct unary* );
static void visit_call( struct task*, struct operand*, struct call* );
static void visit_aspec_call( struct task*, struct operand*, struct call* );
static void visit_ext_call( struct task*, struct operand*, struct call* );
static void visit_ded_call( struct task*, struct operand*, struct call* );
static void visit_format_call( struct task*, struct operand*, struct call* );
static void visit_format_item( struct task*, struct format_item* );
static void visit_user_call( struct task*, struct operand*, struct call* );
static void visit_internal_call( struct task*, struct operand*, struct call* );
static void visit_binary( struct task*, struct operand*, struct binary* );
static void visit_assign( struct task*, struct operand*, struct assign* );
static void do_var_name( struct task*, struct operand*, struct var* );
static void set_var( struct task*, struct operand*, struct var* );
static void visit_object( struct task*, struct operand*, struct node* );
static void visit_subscript( struct task*, struct operand*, struct subscript* );
static void visit_access( struct task*, struct operand*, struct access* );
static void push_indexed( struct task*, int, int );
static void push_element( struct task*, int, int );
static void inc_indexed( struct task*, int, int );
static void dec_indexed( struct task*, int, int );
static void inc_element( struct task*, int, int );
static void dec_element( struct task*, int, int );
static void update_indexed( struct task*, int, int, int );
static void update_element( struct task*, int, int, int );
static void visit_if( struct task*, struct if_stmt* );
static void visit_switch( struct task*, struct switch_stmt* );
static void visit_case( struct task*, struct case_label* );
static void visit_while( struct task*, struct while_stmt* );
static void visit_for( struct task*, struct for_stmt* );
static void visit_jump( struct task*, struct jump* );
static void add_jumps( struct task*, struct jump*, int );
static void visit_return( struct task*, struct return_stmt* );
static void visit_paltrans( struct task*, struct paltrans* );
static void add_default_params( struct task*, struct func* );

static const int g_aspec_code[] = {
   PCD_LSPEC1,
   PCD_LSPEC2,
   PCD_LSPEC3,
   PCD_LSPEC4,
   PCD_LSPEC5
};

void t_publish_usercode( struct task* task ) {
   // Scripts.
   list_iter_t i;
   list_iter_init( &i, &task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      script->offset = t_tell( task );
      add_block_visit( task );
      struct param* param = script->params;
      while ( param ) {
         param->index = alloc_script_var( task );
         param = param->next;
      }
      visit_node( task, script->body );
      t_add_opc( task, PCD_TERMINATE );
      script->size = task->block_visit->size_high;
      pop_block_visit( task );
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );   
      struct func_user* impl = func->impl;
      impl->obj_pos = t_tell( task );
      add_block_visit( task );
      struct param* param = func->params;
      while ( param ) {
         param->index = alloc_script_var( task );
         param = param->next;
      }
      add_default_params( task, func );
      list_iter_t k;
      list_iter_init( &k, &impl->body->stmts );
      while ( ! list_end( &k ) ) {
         visit_node( task, list_data( &k ) );
         list_next( &k );
      }
      t_add_opc( task, PCD_RETURNVOID );
      impl->size = task->block_visit->size_high - func->max_param;
      pop_block_visit( task );
      list_next( &i );
   }
   // When utilizing the Little-E format, where instructions can be of
   // different size, add padding so any following data starts at an offset
   // that is multiple of 4.
   if ( task->library_main->format == FORMAT_LITTLE_E ) {
      int i = alignpad( t_tell( task ), 4 );
      while ( i ) {
         t_add_opc( task, PCD_TERMINATE );
         --i;
      }
   }
}

void visit_block( struct task* task, struct block* block ) {
   add_block_visit( task );
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      visit_node( task, list_data( &i ) );
      list_next( &i );
   }
   pop_block_visit( task );
}

void add_block_visit( struct task* task ) {
   struct block_visit* visit;
   if ( task->block_visit_free ) {
      visit = task->block_visit_free;
      task->block_visit_free = visit->prev;
   }
   else {
      visit = mem_alloc( sizeof( *visit ) );
   }
   visit->prev = task->block_visit;
   task->block_visit = visit;
   visit->format_block_usage = NULL;
   visit->size = 0;
   visit->size_high = 0;
   if ( visit->prev ) {
      visit->size = visit->prev->size;
      visit->size_high = visit->size;
   }
}

void pop_block_visit( struct task* task ) {
   struct block_visit* prev = task->block_visit->prev;
   if ( prev && task->block_visit->size_high > prev->size_high ) {
      prev->size_high = task->block_visit->size_high;
   }
   task->block_visit->prev = task->block_visit_free;
   task->block_visit_free = task->block_visit;
   task->block_visit = prev;
}

int alloc_script_var( struct task* task ) {
   int index = task->block_visit->size;
   ++task->block_visit->size;
   if ( task->block_visit->size > task->block_visit->size_high ) {
      task->block_visit->size_high = task->block_visit->size;
   }
   return index;
}

void dealloc_last_script_var( struct task* task ) {
   --task->block_visit->size;
}

void visit_var( struct task* task, struct var* var ) {
   if ( var->storage == STORAGE_LOCAL ) {
      if ( var->value ) {
         push_expr( task, var->value->expr, false );
         var->index = alloc_script_var( task );
         update_indexed( task, var->storage, var->index, AOP_NONE );
      }
      else {
         var->index = alloc_script_var( task );
      }
   }
   else if ( var->storage == STORAGE_WORLD ||
      var->storage == STORAGE_GLOBAL ) {
      if ( var->initial && ! var->is_constant_init ) {
         if ( var->initial->multi ) {
            init_world_array( task, var );
         }
         else {
            push_expr( task, var->value->expr, false );
            update_indexed( task, var->storage, var->index, AOP_NONE );
         }
      }
   }
}

void init_world_array( struct task* task, struct var* var ) {
   // Nullify array.
   t_add_opc( task, PCD_PUSHNUMBER );
   t_add_arg( task, var->size - 1 );
   int loop = t_tell( task );
   t_add_opc( task, PCD_CASEGOTO );
   t_add_arg( task, 0 );
   t_add_arg( task, 0 );
   t_add_opc( task, PCD_DUP );
   t_add_opc( task, PCD_PUSHNUMBER );
   t_add_arg( task, 0 );
   update_element( task, var->storage, var->index, AOP_NONE );
   t_add_opc( task, PCD_PUSHNUMBER );
   t_add_arg( task, 1 );
   t_add_opc( task, PCD_SUBTRACT );
   t_add_opc( task, PCD_GOTO );
   t_add_arg( task, loop );
   int done = t_tell( task );
   t_seek( task, loop );
   t_add_opc( task, PCD_CASEGOTO );
   t_add_arg( task, -1 );
   t_add_arg( task, done );
   t_seek_end( task );
   // Assign elements.
   struct value* value = var->value;
   while ( value ) {
      // Initialize an element only if the value is not 0, because the array
      // is already nullified. String values from libraries are an exception,
      // because they need to be tagged regardless of the value.
      if ( ( ! value->expr->folded || value->expr->value ) ||
         ( value->expr->has_str && task->library_main->importable ) ) {
         t_add_opc( task, PCD_PUSHNUMBER );
         t_add_arg( task, value->index );
         if ( value->expr->folded && ! value->expr->has_str ) { 
            t_add_opc( task, PCD_PUSHNUMBER );
            t_add_arg( task, value->expr->value );
         }
         else {
            push_expr( task, value->expr, false );
         }
         update_element( task, var->storage, var->index, AOP_NONE );
      }
      value = value->next;
   }
}

void visit_node( struct task* task, struct node* node ) {
   switch ( node->type ) {
   case NODE_BLOCK:
      visit_block( task, ( struct block* ) node );
      break;
   case NODE_SCRIPT_JUMP:
      visit_script_jump( task, ( struct script_jump* ) node );
      break;
   case NODE_GOTO_LABEL:
      visit_label( task, ( struct label* ) node );
      break;
   case NODE_GOTO:
      visit_goto( task, ( struct goto_stmt* ) node );
      break;
   case NODE_PACKED_EXPR:
      visit_packed_expr( task, ( struct packed_expr* ) node );
      break;
   case NODE_VAR:
      visit_var( task, ( struct var* ) node );
      break;
   case NODE_IF:
      visit_if( task, ( struct if_stmt* ) node );
      break;
   case NODE_SWITCH:
      visit_switch( task, ( struct switch_stmt* ) node );
      break;
   case NODE_CASE:
   case NODE_CASE_DEFAULT:
      visit_case( task, ( struct case_label* ) node );
      break;
   case NODE_WHILE:
      visit_while( task, ( struct while_stmt* ) node );
      break;
   case NODE_FOR:
      visit_for( task, ( struct for_stmt* ) node );
      break;
   case NODE_JUMP:
      visit_jump( task, ( struct jump* ) node );
      break;
   case NODE_RETURN:
      visit_return( task, ( struct return_stmt* ) node );
      break;
   case NODE_FORMAT_ITEM:
      visit_format_item( task, ( struct format_item* ) node );
      break;
   case NODE_PALTRANS:
      visit_paltrans( task, ( struct paltrans* ) node );
      break;
   default:
      break;
   }
}

void visit_script_jump( struct task* task, struct script_jump* stmt ) {
   switch ( stmt->type ) {
   case SCRIPT_JUMP_SUSPEND:
      t_add_opc( task, PCD_SUSPEND );
      break;
   case SCRIPT_JUMP_RESTART:
      t_add_opc( task, PCD_RESTART );
      break;
   default:
      t_add_opc( task, PCD_TERMINATE );
      break;
   }
}

void visit_label( struct task* task, struct label* label ) {
   label->obj_pos = t_tell( task );
   struct goto_stmt* stmt = label->users;
   while ( stmt ) {
      if ( stmt->obj_pos ) {
         t_seek( task, stmt->obj_pos );
         t_add_opc( task, PCD_GOTO );
         t_add_arg( task, label->obj_pos );
      }
      stmt = stmt->next;
   }
   t_seek_end( task );
}

void visit_goto( struct task* task, struct goto_stmt* stmt ) {
   if ( stmt->label->obj_pos ) {
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, stmt->label->obj_pos );
   }
   else {
      stmt->obj_pos = t_tell( task );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, 0 );
   }
}

void visit_expr( struct task* task, struct expr* expr ) {
   struct operand operand;
   init_operand( &operand );
   visit_operand( task, &operand, expr->root );
   if ( operand.pushed ) {
      t_add_opc( task, PCD_DROP );
   }
}

void visit_packed_expr( struct task* task, struct packed_expr* stmt ) {
   visit_expr( task, stmt->expr );
}

void push_expr( struct task* task, struct expr* expr, bool temp ) {
   struct operand operand;
   init_operand( &operand );
   operand.push = true;
   operand.push_temp = temp;
   visit_operand( task, &operand, expr->root );
}

void init_operand( struct operand* operand ) {
   operand->type = NULL;
   operand->dim = NULL;
   operand->action = ACTION_PUSH_VALUE;
   operand->method = METHOD_NONE;
   operand->trigger = TRIGGER_INDEX;
   operand->storage = 0;
   operand->index = 0;
   operand->base = 0;
   operand->push = false;
   operand->push_temp = false;
   operand->pushed = false;
   operand->pushed_element = false;
}

void visit_operand( struct task* task, struct operand* operand,
   struct node* node ) {
   // Select object referenced by the name.
   if ( node->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) node;
      node = usage->object;
      if ( node->type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) node;
         node = &alias->target->node;
      }
   }
   // Visit object.
   if ( node->type == NODE_LITERAL ) {
      struct literal* literal = ( struct literal* ) node;
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, literal->value );
      operand->pushed = true;
   }
   else if ( node->type == NODE_INDEXED_STRING_USAGE ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) node;
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, usage->string->index );
      // Strings in a library need to be tagged.
      if ( task->library_main->importable ) {
         t_add_opc( task, PCD_TAGSTRING );
      }
      operand->pushed = true;
   }
   else if ( node->type == NODE_BOOLEAN ) {
      struct boolean* boolean = ( struct boolean* ) node;
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, boolean->value );
      operand->pushed = true;
   }
   else if ( node->type == NODE_CONSTANT ) {
      visit_constant( task, operand, ( struct constant* ) node );
   }
   else if ( node->type == NODE_VAR ) {
      do_var_name( task, operand, ( struct var* ) node );
   }
   else if ( node->type == NODE_PARAM ) {
      struct param* param = ( struct param* ) node;
      if ( operand->action == ACTION_PUSH_VALUE ) {
         push_indexed( task, STORAGE_LOCAL, param->index );
         operand->pushed = true;
      }
      else {
         operand->storage = STORAGE_LOCAL;
         operand->index = param->index;
      }
   }
   else if ( node->type == NODE_UNARY ) {
      visit_unary( task, operand, ( struct unary* ) node );
   }
   else if ( node->type == NODE_SUBSCRIPT || node->type == NODE_ACCESS ) {
      visit_object( task, operand, node );
   }
   else if ( node->type == NODE_CALL ) {
      visit_call( task, operand, ( struct call* ) node );
   }
   else if ( node->type == NODE_BINARY ) {
      visit_binary( task, operand, ( struct binary* ) node );
   }
   else if ( node->type == NODE_ASSIGN ) {
      visit_assign( task, operand, ( struct assign* ) node );
   }
   else if ( node->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) node;
      visit_operand( task, operand, paren->inside );
   }
   else if ( node->type == NODE_FUNC ) {
      struct func* func = ( struct func* ) node;
      if ( func->type == FUNC_ASPEC ) {
         struct func_aspec* impl = func->impl;
         t_add_opc( task, PCD_PUSHNUMBER );
         t_add_arg( task, impl->id );
      }
   }
}

void visit_constant( struct task* task, struct operand* operand,
   struct constant* constant ) {
   t_add_opc( task, PCD_PUSHNUMBER );
   t_add_arg( task, constant->value );
   if ( task->library_main->importable && constant->value_node &&
      constant->value_node->has_str ) {
      t_add_opc( task, PCD_TAGSTRING );
   }
   operand->pushed = true;
}

void visit_unary( struct task* task, struct operand* operand,
   struct unary* unary ) {
   switch ( unary->op ) {
      struct operand object;
   case UOP_PRE_INC:
   case UOP_PRE_DEC:
      visit_pre_inc( task, operand, unary );
      break;
   case UOP_POST_INC:
   case UOP_POST_DEC:
      visit_post_inc( task, operand, unary );
      break;
   default:
      init_operand( &object );
      object.push = true;
      visit_operand( task, &object, unary->operand );
      int code = PCD_NONE;
      switch ( unary->op ) {
      case UOP_MINUS:
         t_add_opc( task, PCD_UNARYMINUS );
         break;
      case UOP_LOG_NOT:
         t_add_opc( task, PCD_NEGATELOGICAL );
         break;
      case UOP_BIT_NOT:
         t_add_opc( task, PCD_NEGATEBINARY );
         break;
      // Unary plus is ignored.
      case UOP_PLUS:
      default:
         break;
      }
      operand->pushed = true;
   }
}

void visit_pre_inc( struct task* task, struct operand* operand,
   struct unary* unary ) {
   struct operand object;
   init_operand( &object );
   object.action = ACTION_PUSH_VAR;
   visit_operand( task, &object, unary->operand );
   if ( object.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         t_add_opc( task, PCD_DUP );
      }
      if ( unary->op == UOP_PRE_INC ) {
         inc_element( task, object.storage, object.index );
      }
      else {
         dec_element( task, object.storage, object.index );
      }
      if ( operand->push ) {
         push_element( task, object.storage, object.index );
         operand->pushed = true;
      }
   }
   else {
      if ( unary->op == UOP_PRE_INC ) {
         inc_indexed( task, object.storage, object.index );
      }
      else {
         dec_indexed( task, object.storage, object.index );
      }
      if ( operand->push ) {
         push_indexed( task, object.storage, object.index );
         operand->pushed = true;
      }
   }
}

void visit_post_inc( struct task* task, struct operand* operand,
   struct unary* unary ) {
   struct operand object;
   init_operand( &object );
   object.action = ACTION_PUSH_VAR;
   visit_operand( task, &object, unary->operand );
   if ( object.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         t_add_opc( task, PCD_DUP );
         push_element( task, object.storage, object.index );
         t_add_opc( task, PCD_SWAP );
         operand->pushed = true;
      }
      if ( unary->op == UOP_POST_INC ) {
         inc_element( task, object.storage, object.index );
      }
      else {
         dec_element( task, object.storage, object.index );
      }
   }
   else {
      if ( operand->push ) {
         push_indexed( task, object.storage, object.index );
         operand->pushed = true;
      }
      if ( unary->op == UOP_POST_INC ) {
         inc_indexed( task, object.storage, object.index );
      }
      else {
         dec_indexed( task, object.storage, object.index );
      }
   }
}

void visit_call( struct task* task, struct operand* operand,
   struct call* call ) {
   switch ( call->func->type ) {
   case FUNC_ASPEC:
      visit_aspec_call( task, operand, call );
      break;
   case FUNC_EXT:
      visit_ext_call( task, operand, call );
      break;
   case FUNC_DED:
      visit_ded_call( task, operand, call );
      break;
   case FUNC_FORMAT:
      visit_format_call( task, operand, call );
      break;
   case FUNC_USER:
      visit_user_call( task, operand, call );
      break;
   case FUNC_INTERNAL:
      visit_internal_call( task, operand, call );
      break;
   }
}

void visit_aspec_call( struct task* task, struct operand* operand,
   struct call* call ) {
   int num = 0;
   list_iter_t i;
   list_iter_init( &i, &call->args );
   while ( ! list_end( &i ) ) {
      push_expr( task, list_data( &i ), false );
      list_next( &i );
      ++num;
   }
   struct func_aspec* aspec = call->func->impl;
   if ( operand->push ) {
      while ( num < 5 ) {
         t_add_opc( task, PCD_PUSHNUMBER );
         t_add_arg( task, 0 );
         ++num;
      }
      t_add_opc( task, PCD_LSPEC5RESULT );
      t_add_arg( task, aspec->id );
   }
   else if ( num ) {
      t_add_opc( task, g_aspec_code[ num - 1 ] );
      t_add_arg( task, aspec->id );
   }
   else {
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, 0 );
      t_add_opc( task, PCD_LSPEC1 );
      t_add_arg( task, aspec->id );
   }
}

void visit_ext_call( struct task* task, struct operand* operand,
   struct call* call ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( ! list_end( &i ) ) {
      struct expr* expr = list_data( &i );
      struct operand arg;
      init_operand( &arg );
      arg.push = true;
      visit_operand( task, &arg, expr->root );
      list_next( &i );
      param = param->next;
   }
   int count = list_size( &call->args );
   int skipped = 0;
   while ( param ) {
      if ( param->default_value->folded && ! param->default_value->value ) {
         ++skipped;
      }
      else {
         count += skipped;
         while ( skipped ) {
            t_add_opc( task, PCD_PUSHNUMBER );
            t_add_arg( task, 0 );
            --skipped;
         }
         push_expr( task, param->default_value, false );
         ++count;
      }
      param = param->next;
   }
   struct func_ext* impl = call->func->impl;
   t_add_opc( task, PCD_CALLFUNC );
   t_add_arg( task, count );
   t_add_arg( task, impl->id );
   if ( call->func->return_type ) {
      operand->pushed = true;
   }
}

void visit_ded_call( struct task* task, struct operand* operand,
   struct call* call ) {
   struct func_ded* ded = call->func->impl;
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( ! list_end( &i ) ) {
      push_expr( task, list_data( &i ), false );
      list_next( &i );
      param = param->next;
   }
   // Default arguments.
   while ( param ) {
      push_expr( task, param->default_value, false );
      param = param->next;
   }
   t_add_opc( task, ded->opcode );
   if ( call->func->return_type ) {
      operand->pushed = true;
   }
}

void visit_user_call( struct task* task, struct operand* operand,
   struct call* call ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( ! list_end( &i ) ) {
      push_expr( task, list_data( &i ), false );
      list_next( &i );
      param = param->next;
   }
   // Default arguments.
   while ( param ) {
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, 0 );
      param = param->next;
   }
   // Number of real arguments passed, for a function with default
   // parameters.
   if ( call->func->min_param != call->func->max_param ) {
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, list_size( &call->args ) );
   }
   struct func_user* impl = call->func->impl;
   if ( call->func->return_type ) {
      t_add_opc( task, PCD_CALL );
      t_add_arg( task, impl->index );
      operand->pushed = true;
   }
   else {
      t_add_opc( task, PCD_CALLDISCARD );
      t_add_arg( task, impl->index );
   }
}

void visit_format_call( struct task* task, struct operand* operand,
   struct call* call ) {
   t_add_opc( task, PCD_BEGINPRINT );
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct node* node = list_data( &i );
   // Format-block:
   if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
      struct format_block_usage* usage =
         ( struct format_block_usage* ) node;
      // When a format block is used more than once in the same expression,
      // instead of duplicating the code, use a goto instruction to enter the
      // format block. At each usage except the last, before the format block
      // is used, a unique number is pushed. This number is used to determine
      // the return location. 
      if ( usage->next ) {
         usage->obj_pos = t_tell( task );
         t_add_opc( task, PCD_PUSHNUMBER );
         t_add_arg( task, 0 );
         t_add_opc( task, PCD_GOTO );
         t_add_arg( task, 0 );
         if ( ! task->block_visit->format_block_usage ) {
            task->block_visit->format_block_usage = usage;
         }
      }
      else {
         int block_pos = t_tell( task );
         visit_block( task, usage->block );
         usage = task->block_visit->format_block_usage;
         if ( usage ) {
            // Update block jumps.
            int count = 0;
            while ( usage->next ) {
               t_seek( task, usage->obj_pos );
               t_add_opc( task, PCD_PUSHNUMBER );
               t_add_arg( task, count );
               t_add_opc( task, PCD_GOTO );
               t_add_arg( task, block_pos );
               usage->obj_pos = t_tell( task );
               usage = usage->next;
               ++count;
            }
            // Publish return jumps. A sorted-case-goto can be used here, but a
            // case-goto will suffice for now.
            t_seek_end( task );
            usage = task->block_visit->format_block_usage;
            count = 0;
            while ( usage->next ) {
               t_add_opc( task, PCD_CASEGOTO );
               t_add_arg( task, count );
               t_add_arg( task, usage->obj_pos );
               usage = usage->next;
               ++count;
            }
            t_add_opc( task, PCD_DROP );
            task->block_visit->format_block_usage = NULL;
         }
      }
      list_next( &i );
   }
   // Format-list:
   else {
      visit_format_item( task, list_data( &i ) );
      list_next( &i );
   }
   // Other arguments.
   if ( call->func->max_param > 1 ) {
      t_add_opc( task, PCD_MOREHUDMESSAGE );
      int param = 1;
      while ( ! list_end( &i ) ) {
         if ( param == call->func->min_param ) {
            t_add_opc( task, PCD_OPTHUDMESSAGE );
         }
         push_expr( task, list_data( &i ), false );
         ++param;
         list_next( &i );
      }
   }
   struct func_format* format = call->func->impl;
   t_add_opc( task, format->opcode );
   if ( call->func->return_type ) {
      operand->pushed = true;
   }
} 

void visit_format_item( struct task* task, struct format_item* item ) {
   while ( item ) {
      if ( item->cast == FCAST_ARRAY ) {
         struct operand object;
         init_operand( &object );
         object.action = ACTION_PUSH_VAR;
         visit_operand( task, &object, item->value->root );
         t_add_opc( task, PCD_PUSHNUMBER );
         t_add_arg( task, object.index );
         int code = PCD_PRINTMAPCHARARRAY;
         switch ( object.storage ) {
         case STORAGE_WORLD:
            code = PCD_PRINTWORLDCHARARRAY;
            break;
         case STORAGE_GLOBAL:
            code = PCD_PRINTGLOBALCHARARRAY;
            break;
         default:
            break;
         }
         t_add_opc( task, code );
      }
      else {
         static const int casts[] = {
            PCD_PRINTBINARY,
            PCD_PRINTCHARACTER,
            PCD_PRINTNUMBER,
            PCD_PRINTFIXED,
            PCD_PRINTBIND,
            PCD_PRINTLOCALIZED,
            PCD_PRINTNAME,
            PCD_PRINTSTRING,
            PCD_PRINTHEX };
         STATIC_ASSERT( FCAST_TOTAL == 10 );
         push_expr( task, item->value, false );
         t_add_opc( task, casts[ item->cast - 1 ] );
      }
      item = item->next;
   }
}

void visit_internal_call( struct task* task, struct operand* operand,
   struct call* call ) {
   struct func_intern* impl = call->func->impl; 
   if ( impl->id == INTERN_FUNC_ACS_EXECWAIT ) {
      list_iter_t i;
      list_iter_init( &i, &call->args );
      push_expr( task, list_data( &i ), false );
      t_add_opc( task, PCD_DUP );
      list_next( &i );
      // Second argument unused.
      list_next( &i );
      // Second argument to Acs_Execute is 0--the current map.
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, 0 );
      while ( ! list_end( &i ) ) {
         push_expr( task, list_data( &i ), true );
         list_next( &i );
      }
      t_add_opc( task, g_aspec_code[ list_size( &call->args ) - 1 ] );
      t_add_arg( task, 80 );
      t_add_opc( task, PCD_SCRIPTWAIT );
   }
   else if ( impl->id == INTERN_FUNC_STR_LENGTH ) {
      visit_operand( task, operand, call->operand );
      t_add_opc( task, PCD_STRLEN );
   }
   else if ( impl->id == INTERN_FUNC_STR_AT ) {
      visit_operand( task, operand, call->operand );
      push_expr( task, list_head( &call->args ), true );
      t_add_opc( task, PCD_CALLFUNC );
      t_add_arg( task, 2 );
      t_add_arg( task, 15 );
   }
}

void visit_binary( struct task* task, struct operand* operand,
   struct binary* binary ) {
   // Logical-or and logical-and both perform shortcircuit evaluation.
   if ( binary->op == BOP_LOG_OR ) {
      struct operand lside;
      init_operand( &lside );
      lside.push = true;
      lside.push_temp = true;
      visit_operand( task, &lside, binary->lside );
      int test = t_tell( task );
      t_add_opc( task, PCD_IFNOTGOTO );
      t_add_arg( task, 0 );
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, 1 );
      int jump = t_tell( task );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, 0 );
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      rside.push_temp = true;
      int next = t_tell( task );
      visit_operand( task, &rside, binary->rside );
      // Optimization: When doing a calculation temporarily, there's no need to
      // convert the second operand to a 0 or 1. Just use the operand directly.
      if ( ! operand->push_temp ) {
         t_add_opc( task, PCD_NEGATELOGICAL );
         t_add_opc( task, PCD_NEGATELOGICAL );
      }
      int done = t_tell( task );
      t_seek( task, test );
      t_add_opc( task, PCD_IFNOTGOTO );
      t_add_arg( task, next );
      t_seek( task, jump );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, done );
      t_seek_end( task );
      operand->pushed = true;
   }
   else if ( binary->op == BOP_LOG_AND ) {
      struct operand lside;
      init_operand( &lside );
      lside.push = true;
      lside.push_temp = true;
      visit_operand( task, &lside, binary->lside );
      int test = t_tell( task );
      t_add_opc( task, PCD_IFGOTO );
      t_add_arg( task, 0 );
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, 0 );
      int jump = t_tell( task );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, 0 );
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      rside.push_temp = true;
      int next = t_tell( task );
      visit_operand( task, &rside, binary->rside );
      if ( ! operand->push_temp ) {
         t_add_opc( task, PCD_NEGATELOGICAL );
         t_add_opc( task, PCD_NEGATELOGICAL );
      }
      int done = t_tell( task );
      t_seek( task, test );
      t_add_opc( task, PCD_IFGOTO );
      t_add_arg( task, next );
      t_seek( task, jump );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, done );
      t_seek_end( task );
      operand->pushed = true;
   }
   else {
      struct operand lside;
      init_operand( &lside );
      lside.push = true;
      visit_operand( task, &lside, binary->lside );
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      visit_operand( task, &rside, binary->rside );
      int code = PCD_NONE;
      switch ( binary->op ) {
      case BOP_BIT_OR: code = PCD_ORBITWISE; break;
      case BOP_BIT_XOR: code = PCD_EORBITWISE; break;
      case BOP_BIT_AND: code = PCD_ANDBITWISE; break;
      case BOP_EQ: code = PCD_EQ; break;
      case BOP_NEQ: code = PCD_NE; break;
      case BOP_LT: code = PCD_LT; break;
      case BOP_LTE: code = PCD_LE; break;
      case BOP_GT: code = PCD_GT; break;
      case BOP_GTE: code = PCD_GE; break;
      case BOP_SHIFT_L: code = PCD_LSHIFT; break;
      case BOP_SHIFT_R: code = PCD_RSHIFT; break;
      case BOP_ADD: code = PCD_ADD; break;
      case BOP_SUB: code = PCD_SUBTRACT; break;
      case BOP_MUL: code = PCD_MULIPLY; break;
      case BOP_DIV: code = PCD_DIVIDE; break;
      case BOP_MOD: code = PCD_MODULUS; break;
      default: break;
      }
      t_add_opc( task, code );
      operand->pushed = true;
   }
}

void set_var( struct task* task, struct operand* operand, struct var* var ) {
   operand->type = var->type;
   operand->dim = var->dim;
   operand->storage = var->storage;
   if ( ! var->type->primitive || var->dim ) {
      operand->method = METHOD_ELEMENT;
      operand->index = var->index;
   }
   else {
      operand->method = METHOD_INDEXED;
      operand->index = var->index;
   }
}

void do_var_name( struct task* task, struct operand* operand,
   struct var* var ) {
   set_var( task, operand, var );
   // For element-based variables, an index marking the start of the variable
   // data needs to be on the stack.
   if ( operand->method == METHOD_ELEMENT ) {
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, operand->base );
   }
   else {
      if ( operand->action == ACTION_PUSH_VALUE ) {
         push_indexed( task, operand->storage, operand->index );
         operand->pushed = true;
      }
   }
}

void visit_object( struct task* task, struct operand* operand,
   struct node* node ) {
   if ( node->type == NODE_ACCESS ) {
      visit_access( task, operand, ( struct access* ) node );
   }
   else if ( node->type == NODE_SUBSCRIPT ) {
      visit_subscript( task, operand, ( struct subscript* ) node );
   }
   if ( operand->method == METHOD_ELEMENT ) {
      if ( operand->pushed_element ) {
         if ( operand->base ) {
            t_add_opc( task, PCD_PUSHNUMBER );
            t_add_arg( task, operand->base );
            t_add_opc( task, PCD_ADD );
         }
      }
      else {
         t_add_opc( task, PCD_PUSHNUMBER );
         t_add_arg( task, operand->base );
      }
   }
   if ( operand->action == ACTION_PUSH_VALUE &&
      operand->method != METHOD_NONE ) {
      if ( operand->method == METHOD_ELEMENT ) {
         push_element( task, operand->storage, operand->index );
      }
      else {
         push_indexed( task, operand->storage, operand->index );
      }
      operand->pushed = true;
   }
}

void visit_subscript( struct task* task, struct operand* operand,
   struct subscript* subscript ) {
   struct node* lside = subscript->lside;
   while ( lside->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) lside;
      lside = paren->inside;
   }
   if ( lside->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) lside;
      lside = usage->object;
      if ( lside->type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) lside;
         lside = &alias->target->node;
      }
   }
   // Left side:
   if ( lside->type == NODE_VAR ) {
      set_var( task, operand, ( struct var* ) lside );
   }
   else if ( lside->type == NODE_ACCESS ) {
      visit_access( task, operand, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      visit_subscript( task, operand, ( struct subscript* ) lside );
   }
   // Dimension:
   struct operand index;
   init_operand( &index );
   index.push = true;
   index.push_temp = true;
   visit_operand( task, &index, subscript->index->root );
   if ( operand->dim->next ) {
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, operand->dim->element_size );
      t_add_opc( task, PCD_MULIPLY );
   }
   else if ( ! operand->type->primitive ) {
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, operand->type->size );
      t_add_opc( task, PCD_MULIPLY );
   }
   if ( operand->pushed_element ) {
      t_add_opc( task, PCD_ADD );
   }
   else {
      operand->pushed_element = true;
   }
   operand->dim = operand->dim->next;
}

void visit_access( struct task* task, struct operand* operand,
   struct access* access ) {
   struct node* lside = access->lside;
   struct node* rside = access->rside;
   while ( lside->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) lside;
      lside = paren->inside;
   }
   if ( lside->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) lside;
      lside = usage->object;
      if ( lside->type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) lside;
         lside = &alias->target->node;
      }
   }
   // See if the left side is a namespace.
   struct node* object = lside;
   if ( object->type == NODE_ACCESS ) {
      struct access* nested = ( struct access* ) object;
      object = nested->rside;
      if ( object->type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) object;
         object = &alias->target->node;
      }
   }
   // When the left side is a module, only process the right side.
   if ( object->type == NODE_REGION || object->type == NODE_REGION_HOST ||
      object->type == NODE_REGION_UPMOST ) {
      lside = access->rside;
      if ( lside->type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) lside;
         lside = &alias->target->node;
      }
      rside = NULL;
   }
   // Left side:
   if ( lside->type == NODE_VAR ) {
      set_var( task, operand, ( struct var* ) lside );
   }
   else if ( lside->type == NODE_CONSTANT ) {
      visit_constant( task, operand, ( struct constant* ) lside );
   }
   else if ( lside->type == NODE_ACCESS ) {
      visit_access( task, operand, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      visit_subscript( task, operand, ( struct subscript* ) lside );
   }
   else {
      visit_operand( task, operand, lside );
   }
   // Right side:
   if ( rside && rside->type == NODE_TYPE_MEMBER ) {
      struct type_member* member = ( struct type_member* ) rside;
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, member->offset );
      if ( operand->pushed_element ) {
         t_add_opc( task, PCD_ADD );
      }
      else {
         operand->pushed_element = true;
      }
      operand->type = member->type;
      operand->dim = member->dim;
   }
}

void visit_assign( struct task* task, struct operand* operand,
   struct assign* assign ) {
   struct operand lside;
   init_operand( &lside );
   lside.action = ACTION_PUSH_VAR;
   visit_operand( task, &lside, assign->lside );
   if ( lside.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         t_add_opc( task, PCD_DUP );
      }
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      visit_operand( task, &rside, assign->rside );
      update_element( task, lside.storage, lside.index, assign->op );
      if ( operand->push ) {
         push_element( task, lside.storage, lside.index );
         operand->pushed = true;
      }
   }
   else {
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      visit_operand( task, &rside, assign->rside );
      if ( assign->op == AOP_NONE && operand->push ) {
         t_add_opc( task, PCD_DUP );
         operand->pushed = true;
      }
      update_indexed( task, lside.storage, lside.index, assign->op );
      if ( assign->op != AOP_NONE && operand->push ) {
         push_indexed( task, lside.storage, lside.index );
         operand->pushed = true;
      }
   }
}

void push_indexed( struct task* task, int storage, int index ) {
   int code = PCD_PUSHSCRIPTVAR;
   switch ( storage ) {
   case STORAGE_MAP:
      code = PCD_PUSHMAPVAR;
      break;
   case STORAGE_WORLD:
      code = PCD_PUSHWORLDVAR;
      break;
   case STORAGE_GLOBAL:
      code = PCD_PUSHGLOBALVAR;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void push_element( struct task* task, int storage, int index ) {
   int code = PCD_PUSHMAPARRAY;
   switch ( storage ) {
   case STORAGE_WORLD:
      code = PCD_PUSHWORLDARRAY;
      break;
   case STORAGE_GLOBAL:
      code = PCD_PUSHGLOBALARRAY;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void update_indexed( struct task* task, int storage, int index, int op ) {
   static const int code[] = {
      PCD_ASSIGNSCRIPTVAR, PCD_ASSIGNMAPVAR, PCD_ASSIGNWORLDVAR,
         PCD_ASSIGNGLOBALVAR,
      PCD_ADDSCRIPTVAR, PCD_ADDMAPVAR, PCD_ADDWORLDVAR, PCD_ADDGLOBALVAR,
      PCD_SUBSCRIPTVAR, PCD_SUBMAPVAR, PCD_SUBWORLDVAR, PCD_SUBGLOBALVAR,
      PCD_MULSCRIPTVAR, PCD_MULMAPVAR, PCD_MULWORLDVAR, PCD_MULGLOBALVAR,
      PCD_DIVSCRIPTVAR, PCD_DIVMAPVAR, PCD_DIVWORLDVAR, PCD_DIVGLOBALVAR,
      PCD_MODSCRIPTVAR, PCD_MODMAPVAR, PCD_MODWORLDVAR, PCD_MODGLOBALVAR,
      PCD_LSSCRIPTVAR, PCD_LSMAPVAR, PCD_LSWORLDVAR, PCD_LSGLOBALVAR,
      PCD_RSSCRIPTVAR, PCD_RSMAPVAR, PCD_RSWORLDVAR, PCD_RSGLOBALVAR,
      PCD_ANDSCRIPTVAR, PCD_ANDMAPVAR, PCD_ANDWORLDVAR, PCD_ANDGLOBALVAR,
      PCD_EORSCRIPTVAR, PCD_EORMAPVAR, PCD_EORWORLDVAR, PCD_EORGLOBALVAR,
      PCD_ORSCRIPTVAR, PCD_ORMAPVAR, PCD_ORWORLDVAR, PCD_ORGLOBALVAR };
   int pos = 0;
   switch ( storage ) {
   case STORAGE_MAP: pos = 1; break;
   case STORAGE_WORLD: pos = 2; break;
   case STORAGE_GLOBAL: pos = 3; break;
   default: break;
   }
   t_add_opc( task, code[ op * 4 + pos ] );
   t_add_arg( task, index );
}

void update_element( struct task* task, int storage, int index, int op ) {
   static const int code[] = {
      PCD_ASSIGNMAPARRAY, PCD_ASSIGNWORLDARRAY, PCD_ASSIGNGLOBALARRAY,
      PCD_ADDMAPARRAY, PCD_ADDWORLDARRAY, PCD_ADDGLOBALARRAY,
      PCD_SUBMAPARRAY, PCD_SUBWORLDARRAY, PCD_SUBGLOBALARRAY,
      PCD_MULMAPARRAY, PCD_MULWORLDARRAY, PCD_MULGLOBALARRAY,
      PCD_DIVMAPARRAY, PCD_DIVWORLDARRAY, PCD_DIVGLOBALARRAY,
      PCD_MODMAPARRAY, PCD_MODWORLDARRAY, PCD_MODGLOBALARRAY,
      PCD_LSMAPARRAY, PCD_LSWORLDARRAY, PCD_LSGLOBALARRAY,
      PCD_RSMAPARRAY, PCD_RSWORLDARRAY, PCD_RSGLOBALARRAY,
      PCD_ANDMAPARRAY, PCD_ANDWORLDARRAY, PCD_ANDGLOBALARRAY,
      PCD_EORMAPARRAY, PCD_EORWORLDARRAY, PCD_EORGLOBALARRAY,
      PCD_ORMAPARRAY, PCD_ORWORLDARRAY, PCD_ORGLOBALARRAY };
   int pos = 0;
   switch ( storage ) {
   case STORAGE_WORLD: pos = 1; break;
   case STORAGE_GLOBAL: pos = 2; break;
   default: break;
   }
   t_add_opc( task, code[ op * 3 + pos ] );
   t_add_arg( task, index );
}

void inc_indexed( struct task* task, int storage, int index ) {
   int code = PCD_INCSCRIPTVAR;
   switch ( storage ) {
   case STORAGE_MAP:
      code = PCD_INCMAPVAR;
      break;
   case STORAGE_WORLD:
      code = PCD_INCWORLDVAR;
      break;
   case STORAGE_GLOBAL:
      code = PCD_INCGLOBALVAR;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void dec_indexed( struct task* task, int storage, int index ) {
   int code = PCD_DECSCRIPTVAR;
   switch ( storage ) {
   case STORAGE_MAP:
      code = PCD_DECMAPVAR;
      break;
   case STORAGE_WORLD:
      code = PCD_DECWORLDVAR;
      break;
   case STORAGE_GLOBAL:
      code = PCD_DECGLOBALVAR;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void inc_element( struct task* task, int storage, int index ) {
   int code = PCD_INCMAPARRAY;
   switch ( storage ) {
   case STORAGE_WORLD:
      code = PCD_INCWORLDARRAY;
      break;
   case STORAGE_GLOBAL:
      code = PCD_INCGLOBALARRAY;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void dec_element( struct task* task, int storage, int index ) {
   int code = PCD_DECMAPARRAY;
   switch ( storage ) {
   case STORAGE_WORLD:
      code = PCD_DECWORLDARRAY;
      break;
   case STORAGE_GLOBAL:
      code = PCD_DECGLOBALARRAY;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void visit_if( struct task* task, struct if_stmt* stmt ) {
   push_expr( task, stmt->cond, true );
   int cond = t_tell( task );
   t_add_opc( task, PCD_IFNOTGOTO );
   t_add_arg( task, 0 );
   visit_node( task, stmt->body );
   int bail = t_tell( task );
   if ( stmt->else_body ) {
      // Exit from if block:
      int bail_if_block = t_tell( task );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, 0 ); 
      bail = t_tell( task );
      visit_node( task, stmt->else_body );
      int stmt_end = t_tell( task );
      t_seek( task, bail_if_block );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, stmt_end );
   }
   t_seek( task, cond );
   t_add_opc( task, PCD_IFNOTGOTO );
   t_add_arg( task, bail );
   t_seek_end( task );
}

void visit_switch( struct task* task, struct switch_stmt* stmt ) {
   push_expr( task, stmt->cond, true );
   int num_cases = 0;
   struct case_label* label = stmt->case_head;
   while ( label ) {
      ++num_cases;
      label = label->next;
   }
   int test = t_tell( task );
   if ( num_cases ) {
      t_add_opc( task, PCD_CASEGOTOSORTED );
      t_add_arg( task, 0 );
      for ( int i = 0; i < num_cases; ++i ) {
         t_add_arg( task, 0 );
         t_add_arg( task, 0 );
      }
   }
   t_add_opc( task, PCD_DROP );
   int fail = t_tell( task );
   t_add_opc( task, PCD_GOTO );
   t_add_arg( task, 0 );
   visit_node( task, stmt->body );
   int done = t_tell( task );
   if ( num_cases ) {
      t_seek( task, test );
      t_add_opc( task, PCD_CASEGOTOSORTED );
      t_add_arg( task, num_cases );
      label = stmt->case_head;
      while ( label ) {
         t_add_arg( task, label->number->value );
         t_add_arg( task, label->offset );
         label = label->next;
      }
   }
   t_seek( task, fail );
   t_add_opc( task, PCD_GOTO );
   int fail_pos = done;
   if ( stmt->case_default ) {
      fail_pos = stmt->case_default->offset;
   }
   t_add_arg( task, fail_pos );
   add_jumps( task, stmt->jump_break, done );
}

void visit_case( struct task* task, struct case_label* label ) {
   label->offset = t_tell( task );
}

void visit_while( struct task* task, struct while_stmt* stmt ) {
   int test = 0;
   int done = 0;
   if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
      int jump = 0;
      if ( ! stmt->cond->folded || (
         ( stmt->type == WHILE_WHILE && ! stmt->cond->value ) ||
         ( stmt->type == WHILE_UNTIL && stmt->cond->value ) ) ) {
         jump = t_tell( task );
         t_add_opc( task, PCD_GOTO );
         t_add_arg( task, 0 );
      }
      int body = t_tell( task );
      visit_node( task, stmt->body );
      if ( stmt->cond->folded ) {
         if ( ( stmt->type == WHILE_WHILE && stmt->cond->value ) ||
            ( stmt->type == WHILE_UNTIL && ! stmt->cond->value ) ) {
            t_add_opc( task, PCD_GOTO );
            t_add_arg( task, body );
            done = t_tell( task );
            test = body;
         }
         else {
            done = t_tell( task );
            test = done;
            t_seek( task, jump );
            t_add_opc( task, PCD_GOTO );
            t_add_arg( task, done );
         }
      }
      else {
         test = t_tell( task );
         push_expr( task, stmt->cond, true );
         int code = PCD_IFGOTO;
         if ( stmt->type == WHILE_UNTIL ) {
            code = PCD_IFNOTGOTO;
         }
         t_add_opc( task, code );
         t_add_arg( task, body );
         done = t_tell( task );
         t_seek( task, jump );
         t_add_opc( task, PCD_GOTO );
         t_add_arg( task, test );
      }
   }
   // do-while / do-until.
   else {
      int body = t_tell( task );
      visit_node( task, stmt->body );
      // Condition:
      if ( stmt->cond->folded ) {
         // Optimization: Only loop when the condition is satisfied.
         if ( ( stmt->type == WHILE_DO_WHILE && stmt->cond->value ) ||
            ( stmt->type == WHILE_DO_UNTIL && ! stmt->cond->value ) ) {
            t_add_opc( task, PCD_GOTO );
            t_add_arg( task, body );
            done = t_tell( task );
            test = body;
         }
         else {
            done = t_tell( task );
            test = done;
         }
      }
      else {
         test = t_tell( task );
         push_expr( task, stmt->cond, true );
         int code = PCD_IFGOTO;
         if ( stmt->type == WHILE_DO_UNTIL ) {
            code = PCD_IFNOTGOTO;
         }
         t_add_opc( task, code );
         t_add_arg( task, body );
         done = t_tell( task );
      }
   }
   add_jumps( task, stmt->jump_continue, test );
   add_jumps( task, stmt->jump_break, done );
}

// for-loop layout:
// <initialization>
// <goto-condition>
// <body>
// <post-expression-list>
// <condition>
//   if 1: <goto-body>
//
// for-loop layout (constant-condition, 1):
// <initialization>
// <body>
// <post-expression-list>
// <goto-body>
//
// for-loop layout (constant-condition, 0):
// <initialization>
// <goto-done>
// <body>
// <post-expression-list>
// <done>
void visit_for( struct task* task, struct for_stmt* stmt ) {
   // Initialization.
   list_iter_t i;
   list_iter_init( &i, &stmt->init );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      switch ( node->type ) {
      case NODE_EXPR:
         visit_expr( task, ( struct expr* ) node );
         break;
      case NODE_VAR:
         visit_var( task, ( struct var* ) node );
         break;
      default:
         break;
      }
      list_next( &i );
   }
   // Jump to condition.
   int jump = 0;
   if ( stmt->cond ) {
      if ( ! stmt->cond->folded || ! stmt->cond->value ) {
         jump = t_tell( task );
         t_add_opc( task, PCD_GOTO );
         t_add_arg( task, 0 );
      }
   }
   // Body.
   int body = t_tell( task );
   visit_node( task, stmt->body );
   // Post expressions.
   int post = t_tell( task );
   list_iter_init( &i, &stmt->post );
   while ( ! list_end( &i ) ) {
      visit_expr( task, list_data( &i ) );
      list_next( &i );
   }
   // Condition.
   int test = 0;
   if ( stmt->cond ) {
      if ( stmt->cond->folded ) {
         if ( stmt->cond->value ) {
            t_add_opc( task, PCD_GOTO );
            t_add_arg( task, body );
         }
      }
      else {
         test = t_tell( task );
         push_expr( task, stmt->cond, true );
         t_add_opc( task, PCD_IFGOTO );
         t_add_arg( task, body );
      }
   }
   else {
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, body );
   }
   // Jump to condition.
   int done = t_tell( task );
   if ( stmt->cond ) {
      if ( stmt->cond->folded ) {
         if ( ! stmt->cond->value ) {
            t_seek( task, jump );
            t_add_opc( task, PCD_GOTO );
            t_add_arg( task, done );
         }
      }
      else {
         t_seek( task, jump );
         t_add_opc( task, PCD_GOTO );
         t_add_arg( task, test );
      }
   }
   add_jumps( task, stmt->jump_continue, post );
   add_jumps( task, stmt->jump_break, done );
}

void visit_jump( struct task* task, struct jump* jump ) {
   jump->obj_pos = t_tell( task );
   t_add_opc( task, PCD_GOTO );
   t_add_arg( task, 0 );
}

void add_jumps( struct task* task, struct jump* jump, int pos ) {
   while ( jump ) {
      t_seek( task, jump->obj_pos );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, pos );
      jump = jump->next;
   }
   t_seek_end( task );
}

void visit_return( struct task* task, struct return_stmt* stmt ) {
   if ( stmt->return_value ) {
      push_expr( task, stmt->return_value->expr, false );
      t_add_opc( task, PCD_RETURNVAL );
   }
   else {
      t_add_opc( task, PCD_RETURNVOID );
   }
}

void visit_paltrans( struct task* task, struct paltrans* trans ) {
   push_expr( task, trans->number, true );
   t_add_opc( task, PCD_STARTTRANSLATION );
   struct palrange* range = trans->ranges;
   while ( range ) {
      push_expr( task, range->begin, true );
      push_expr( task, range->end, true );
      if ( range->rgb ) {
         push_expr( task, range->value.rgb.red1, true );
         push_expr( task, range->value.rgb.green1, true );
         push_expr( task, range->value.rgb.blue1, true );
         push_expr( task, range->value.rgb.red2, true );
         push_expr( task, range->value.rgb.green2, true );
         push_expr( task, range->value.rgb.blue2, true );
         t_add_opc( task, PCD_TRANSLATIONRANGE2 );
      }
      else {
         push_expr( task, range->value.ent.begin, true );
         push_expr( task, range->value.ent.end, true );
         t_add_opc( task, PCD_TRANSLATIONRANGE1 );
      }
      range = range->next;
   }
   t_add_opc( task, PCD_ENDTRANSLATION );
}

void add_default_params( struct task* task, struct func* func ) {
   // Find first default parameter.
   struct param* param = func->params;
   while ( param && ! param->default_value ) {
      param = param->next;
   }
   int num = 0;
   int num_cases = 0;
   struct param* start = param;
   while ( param ) {
      ++num;
      if ( ! param->default_value->folded || param->default_value->value ) {
         num_cases = num;
      }
      param = param->next;
   }
   if ( num_cases ) {
      // A hidden parameter is used to store the number of arguments passed to
      // the function. This parameter is found after the last visible parameter.
      t_add_opc( task, PCD_PUSHSCRIPTVAR );
      t_add_arg( task, func->max_param );
      int jump = t_tell( task );
      t_add_opc( task, PCD_CASEGOTOSORTED );
      t_add_arg( task, 0 );
      param = start;
      int i = 0;
      while ( param && i < num_cases ) {
         t_add_arg( task, 0 );
         t_add_arg( task, 0 );
         param = param->next;
         ++i;
      }
      t_add_opc( task, PCD_DROP );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, 0 );
      param = start;
      while ( param ) {
         param->obj_pos = t_tell( task );
         if ( ! param->default_value->folded || param->default_value->value ) {
            push_expr( task, param->default_value, false );
            update_indexed( task, STORAGE_LOCAL, param->index, AOP_NONE );
         }
         param = param->next;
      }
      int done = t_tell( task );
      // Add case positions.
      t_seek( task, jump );
      t_add_opc( task, PCD_CASEGOTOSORTED );
      t_add_arg( task, num_cases );
      num = func->min_param;
      param = start;
      while ( param && num_cases ) {
         t_add_arg( task, num );
         t_add_arg( task, param->obj_pos );
         param = param->next;
         --num_cases;
         ++num;
      }
      t_add_opc( task, PCD_DROP );
      t_add_opc( task, PCD_GOTO );
      t_add_arg( task, done );
      t_seek( task, done );
   }
   if ( start ) {
      // Reset the parameter-count parameter.
      t_add_opc( task, PCD_PUSHNUMBER );
      t_add_arg( task, 0 );
      t_add_opc( task, PCD_ASSIGNSCRIPTVAR );
      t_add_arg( task, func->max_param );
   }
}