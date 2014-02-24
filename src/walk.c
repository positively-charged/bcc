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
      TRIGGER_INDEX,
      TRIGGER_FUNCTION
   } trigger;
   int storage;
   int index;
   int func_get;
   int func_set;
   int base;
   bool push;
   bool push_temp;
   bool pushed;
   bool pushed_element;
   bool do_str;
   bool add_str_arg;
};

struct block_walk {
   struct block_walk* prev;
   int size;
   int size_high;
};

static void do_block( struct task*, struct block* );
static void add_block_walk( struct task* );
static void pop_block_walk( struct task* );
static int alloc_scalar( struct task* );
static void dealloc_last_scalar( struct task* );
static void do_var( struct task*, struct var* );
static void do_world_global_init( struct task*, struct var* );
static void do_node( struct task*, struct node* );
static void do_script_jump( struct task*, struct script_jump* );
static void do_label( struct task*, struct label* );
static void do_goto( struct task*, struct goto_stmt* );
static void init_operand( struct operand* );
static void push_expr( struct task*, struct expr*, bool );
static void do_expr_stmt( struct task*, struct expr* );
static void do_operand( struct task*, struct operand*, struct node* );
static void do_constant( struct task*, struct operand*, struct constant* );
static void do_unary( struct task*, struct operand*, struct unary* );
static void do_pre_inc( struct task*, struct operand*, struct unary* );
static void do_post_inc( struct task*, struct operand*, struct unary* );
static void do_call( struct task*, struct operand*, struct call* );
static void do_format_item( struct task*, struct format_item* );
static void do_binary( struct task*, struct operand*, struct binary* );
static void do_assign( struct task*, struct operand*, struct assign* );
static void do_var_name( struct task*, struct operand*, struct var* );
static void set_var( struct task*, struct operand*, struct var* );
static void do_object( struct task*, struct operand*, struct node* );
static void do_subscript( struct task*, struct operand*, struct subscript* );
static void do_access( struct task*, struct operand*, struct access* );
static void push_indexed( struct task*, int, int );
static void push_element( struct task*, int, int );
static void inc_indexed( struct task*, int, int );
static void dec_indexed( struct task*, int, int );
static void inc_element( struct task*, int, int );
static void dec_element( struct task*, int, int );
static void update_indexed( struct task*, int, int, int );
static void update_element( struct task*, int, int, int );
static void do_if( struct task*, struct if_stmt* );
static void do_switch( struct task*, struct switch_stmt* );
static void do_case( struct task*, struct case_label* );
static void do_while( struct task*, struct while_stmt* );
static void do_for( struct task*, struct for_stmt* );
static void do_jump( struct task*, struct jump* );
static void do_jump_target( struct task*, struct jump*, int );
static void do_return( struct task*, struct return_stmt* );
static void do_paltrans( struct task*, struct paltrans* );
static void do_default_params( struct task*, struct func* );
static void do_pcode( struct task*, struct pcode* );

static const int g_aspec_code[] = {
   PC_LSPEC1, PC_LSPEC2, PC_LSPEC3, PC_LSPEC4, PC_LSPEC5 };

void t_write_script_content( struct task* task, struct list* scripts ) {
   list_iter_t i;
   list_iter_init( &i, scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      script->offset = t_tell( task );
      add_block_walk( task );
      struct param* param = script->params;
      while ( param ) {
         param->index = alloc_scalar( task );
         param = param->next;
      }
      list_iter_t k;
      list_iter_init( &k, &script->body->stmts );
      while ( ! list_end( &k ) ) {
         do_node( task, list_data( &k ) );
         list_next( &k );
      }
      t_add_opc( task, PC_TERMINATE );
      script->size = task->block_walk->size_high;
      pop_block_walk( task );
      list_next( &i );
   }
}

void t_write_func_content( struct task* task, struct module* module ) {
   list_iter_t i;
   list_iter_init( &i, &module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );   
      struct func_user* impl = func->impl;
      impl->obj_pos = t_tell( task );
      add_block_walk( task );
      do_default_params( task, func );
      struct param* param = func->params;
      while ( param ) {
         param->index = alloc_scalar( task );
         param = param->next;
      }
      list_iter_t k;
      list_iter_init( &k, &impl->body->stmts );
      while ( ! list_end( &k ) ) {
         do_node( task, list_data( &k ) );
         list_next( &k );
      }
      t_add_opc( task, PC_RETURN_VOID );
      impl->size = task->block_walk->size_high - func->max_param;
      pop_block_walk( task );
      list_next( &i );
   }
}

void do_block( struct task* task, struct block* block ) {
   add_block_walk( task );
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      do_node( task, list_data( &i ) );
      list_next( &i );
   }
   pop_block_walk( task );
}

void add_block_walk( struct task* task ) {
   struct block_walk* walk;
   if ( task->block_walk_free ) {
      walk = task->block_walk_free;
      task->block_walk_free = walk->prev;
   }
   else {
      walk = mem_alloc( sizeof( *walk ) );
   }
   walk->prev = task->block_walk;
   task->block_walk = walk;
   walk->size = 0;
   walk->size_high = 0;
   if ( walk->prev ) {
      walk->size = walk->prev->size;
      walk->size_high = walk->size;
   }
}

void pop_block_walk( struct task* task ) {
   struct block_walk* prev = task->block_walk->prev;
   if ( prev && task->block_walk->size_high > prev->size_high ) {
      prev->size_high = task->block_walk->size_high;
   }
   task->block_walk->prev = task->block_walk_free;
   task->block_walk_free = task->block_walk;
   task->block_walk = prev;
}

int alloc_scalar( struct task* task ) {
   int index = task->block_walk->size;
   ++task->block_walk->size;
   if ( task->block_walk->size > task->block_walk->size_high ) {
      task->block_walk->size_high = task->block_walk->size;
   }
   return index;
}

void dealloc_last_scalar( struct task* task ) {
   --task->block_walk->size;
}

void do_var( struct task* task, struct var* var ) {
   if ( var->storage != STORAGE_MAP ) {
      if ( var->storage == STORAGE_LOCAL ) {
         var->index = alloc_scalar( task );
      }
      if ( var->initial ) {
         if ( var->initial->is_initz ) {
            if ( var->storage == STORAGE_WORLD ||
               var->storage == STORAGE_GLOBAL ) {
               do_world_global_init( task, var );
            }
         }
         else {
            struct value* value = ( struct value* ) var->initial;
            push_expr( task, value->expr, false );
            update_indexed( task, var->storage, var->index, AOP_NONE );
         }
      }
   }
}

void do_world_global_init( struct task* task, struct var* var ) {
   // Nullify array.
   t_add_opc( task, PC_PUSH_NUMBER );
   t_add_arg( task, var->size + var->size_str - 1 );
   int loop = t_tell( task );
   t_add_opc( task, PC_CASE_GOTO );
   t_add_arg( task, 0 );
   t_add_arg( task, 0 );
   t_add_opc( task, PC_DUP );
   t_add_opc( task, PC_PUSH_NUMBER );
   t_add_arg( task, 0 );
   update_element( task, var->storage, var->index, AOP_NONE );
   t_add_opc( task, PC_PUSH_NUMBER );
   t_add_arg( task, 1 );
   t_add_opc( task, PC_SUB );
   t_add_opc( task, PC_GOTO );
   t_add_arg( task, loop );
   int done = t_tell( task );
   t_seek( task, loop );
   t_add_opc( task, PC_CASE_GOTO );
   t_add_arg( task, -1 );
   t_add_arg( task, done );
   t_seek( task, OBJ_SEEK_END );
   // Assign elements.
   struct value* value = var->value;
   while ( value ) {
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, value->index );
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, value->expr->value );
      update_element( task, var->storage, var->index, AOP_NONE );
      value = value->next;
   }
   value = var->value_str;
   while ( value ) {
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, var->size + value->index );
      t_add_opc( task, PC_PUSH_NUMBER );
      int string_index = 0;
      if ( value->expr->root->type == NODE_INDEXED_STRING_USAGE ) {
         struct indexed_string_usage* usage =
            ( struct indexed_string_usage* ) value->expr->root;
         string_index = usage->string->index;
      }
      t_add_arg( task, string_index );
      if ( task->module->name.value ) {
         t_add_opc( task, PC_TAG_STRING );
      }
      update_element( task, var->storage, var->index, AOP_NONE );
      value = value->next;
   }
}

void do_node( struct task* task, struct node* node ) {
   switch ( node->type ) {
   case NODE_BLOCK:
      do_block( task, ( struct block* ) node );
      break;
   case NODE_SCRIPT_JUMP:
      do_script_jump( task, ( struct script_jump* ) node );
      break;
   case NODE_GOTO_LABEL:
      do_label( task, ( struct label* ) node );
      break;
   case NODE_GOTO:
      do_goto( task, ( struct goto_stmt* ) node );
      break;
   case NODE_EXPR:
      do_expr_stmt( task, ( struct expr* ) node );
      break;
   case NODE_FORMAT_EXPR: {
         struct format_expr* format_expr = ( struct format_expr* ) node;
         do_expr_stmt( task, format_expr->expr );
         break;
      }
   case NODE_VAR:
      do_var( task, ( struct var* ) node );
      break;
   case NODE_IF:
      do_if( task, ( struct if_stmt* ) node );
      break;
   case NODE_SWITCH:
      do_switch( task, ( struct switch_stmt* ) node );
      break;
   case NODE_CASE:
   case NODE_CASE_DEFAULT:
      do_case( task, ( struct case_label* ) node );
      break;
   case NODE_WHILE:
      do_while( task, ( struct while_stmt* ) node );
      break;
   case NODE_FOR:
      do_for( task, ( struct for_stmt* ) node );
      break;
   case NODE_JUMP:
      do_jump( task, ( struct jump* ) node );
      break;
   case NODE_RETURN:
      do_return( task, ( struct return_stmt* ) node );
      break;
   case NODE_FORMAT_ITEM:
      do_format_item( task, ( struct format_item* ) node );
      break;
   case NODE_PALTRANS:
      do_paltrans( task, ( struct paltrans* ) node );
      break;
   case NODE_PCODE:
      do_pcode( task, ( struct pcode* ) node );
      break;
   default:
      break;
   }
}

void do_script_jump( struct task* task, struct script_jump* stmt ) {
   if ( stmt->type == SCRIPT_JUMP_SUSPEND ) {
      t_add_opc( task, PC_SUSPEND );
   }
   else if ( stmt->type == SCRIPT_JUMP_RESTART ) {
      t_add_opc( task, PC_RESTART );
   }
   else {
      t_add_opc( task, PC_TERMINATE );
   }
}

void do_label( struct task* task, struct label* label ) {
   label->obj_pos = t_tell( task );
   struct goto_stmt* stmt = label->stmts;
   while ( stmt ) {
      if ( stmt->obj_pos ) {
         t_seek( task, stmt->obj_pos );
         t_add_opc( task, PC_GOTO );
         t_add_arg( task, label->obj_pos );
      }
      stmt = stmt->next;
   }
   list_iter_t i;
   list_iter_init( &i, &label->users );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      if ( node->type == NODE_PCODE_OFFSET ) {
         struct pcode_offset* macro = ( struct pcode_offset* ) node;
         t_seek( task, macro->obj_pos );
         t_add_int( task, label->obj_pos );
      }
      else {
         struct goto_stmt* stmt = ( struct goto_stmt* ) node;
         if ( stmt->obj_pos ) {
            t_seek( task, stmt->obj_pos );
            t_add_opc( task, PC_GOTO );
            t_add_arg( task, label->obj_pos );
         }
      }
      list_next( &i );
   }
   t_seek( task, OBJ_SEEK_END );
}

void do_goto( struct task* task, struct goto_stmt* stmt ) {
   if ( stmt->label->obj_pos ) {
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, stmt->label->obj_pos );
   }
   else {
      stmt->obj_pos = t_tell( task );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, 0 );
   }
}

void do_expr_stmt( struct task* task, struct expr* expr ) {
   struct operand operand;
   init_operand( &operand );
   do_operand( task, &operand, expr->root );
   if ( operand.pushed ) {
      t_add_opc( task, PC_DROP );
   }
}

void init_operand( struct operand* operand ) {
   operand->type = NULL;
   operand->dim = NULL;
   operand->action = ACTION_PUSH_VALUE;
   operand->method = METHOD_NONE;
   operand->trigger = TRIGGER_INDEX;
   operand->storage = 0;
   operand->index = 0;
   operand->func_get = 0;
   operand->func_set = 0;
   operand->base = 0;
   operand->push = false;
   operand->push_temp = false;
   operand->pushed = false;
   operand->pushed_element = false;
   operand->do_str = false;
   operand->add_str_arg = false;
}

void push_expr( struct task* task, struct expr* expr, bool temp ) {
   struct operand operand;
   init_operand( &operand );
   operand.push = true;
   operand.push_temp = temp;
   do_operand( task, &operand, expr->root );
}

void do_operand( struct task* task, struct operand* operand,
   struct node* node ) {
   if ( node->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) node;
      node = usage->object;
      if ( node->type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) node;
         node = &alias->target->node;
      }
   }
   if ( node->type == NODE_LITERAL ) {
      struct literal* literal = ( struct literal* ) node;
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, literal->value );
      operand->pushed = true;
   }
   else if ( node->type == NODE_INDEXED_STRING_USAGE ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) node;
      if ( usage->string->usage ) {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, usage->string->index );
         // Strings in a library need to be tagged.
         if ( task->module_main->visible ) {
            t_add_opc( task, PC_TAG_STRING );
         }
      }
      else {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, 0 );
      }
      operand->pushed = true;
   }
   else if ( node->type == NODE_CONSTANT ) {
      do_constant( task, operand, ( struct constant* ) node );
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
      do_unary( task, operand, ( struct unary* ) node );
   }
   else if ( node->type == NODE_SUBSCRIPT || node->type == NODE_ACCESS ) {
      do_object( task, operand, node );
   }
   else if ( node->type == NODE_CALL ) {
      do_call( task, operand, ( struct call* ) node );
   }
   else if ( node->type == NODE_BINARY ) {
      do_binary( task, operand, ( struct binary* ) node );
   }
   else if ( node->type == NODE_ASSIGN ) {
      do_assign( task, operand, ( struct assign* ) node );
   }
   else if ( node->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) node;
      do_operand( task, operand, paren->inside );
   }
   else if ( node->type == NODE_FUNC ) {
      struct func* func = ( struct func* ) node;
      if ( func->type == FUNC_ASPEC ) {
         struct func_aspec* impl = func->impl;
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, impl->id );
      }
   }
}

void do_constant( struct task* task, struct operand* operand,
   struct constant* constant ) {
   if ( constant->expr &&
      constant->expr->root->type == NODE_INDEXED_STRING_USAGE ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) constant->expr->root;
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, usage->string->index );
      if ( task->module->name.value ) {
         t_add_opc( task, PC_TAG_STRING );
      }
   }
   else {
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, constant->value );
   }
   operand->pushed = true;
}

void do_unary( struct task* task, struct operand* operand,
   struct unary* unary ) {
   if ( unary->op == UOP_PRE_INC || unary->op == UOP_PRE_DEC ) {
      do_pre_inc( task, operand, unary );
   }
   else if ( unary->op == UOP_POST_INC || unary->op == UOP_POST_DEC ) {
      do_post_inc( task, operand, unary );
   }
   else {
      struct operand target;
      init_operand( &target );
      target.push = true;
      do_operand( task, &target, unary->operand );
      int code = PC_NONE;
      switch ( unary->op ) {
      case UOP_MINUS:
         code = PC_UNARY_MINUS;
         break;
      // Unary plus is ignored.
      case UOP_LOG_NOT:
         code = PC_NEGATE_LOGICAL;
         break;
      case UOP_BIT_NOT:
         code = PC_NEGATE_BINARY;
         break;
      default:
         break;
      }
      if ( code ) {
         t_add_opc( task, code );
      }
      operand->pushed = true;
   }
}

void do_pre_inc( struct task* task, struct operand* operand,
   struct unary* unary ) {
   struct operand object;
   init_operand( &object );
   object.action = ACTION_PUSH_VAR;
   do_operand( task, &object, unary->operand );
   if ( object.trigger == TRIGGER_FUNCTION ) {
      if ( object.method == METHOD_ELEMENT ) {
         t_add_opc( task, PC_DUP );
         if ( operand->push ) {
            t_add_opc( task, PC_DUP );
         }
         if ( object.add_str_arg ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, ( int ) object.do_str );
         }
      }
      t_add_opc( task, PC_CALL );
      t_add_arg( task, object.func_get );
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, 1 );
      if ( unary->op == UOP_PRE_INC ) {
         t_add_opc( task, PC_ADD );
      }
      else {
         t_add_opc( task, PC_SUB );
      }
      if ( object.method == METHOD_INDEXED ) {
         if ( operand->push ) {
            t_add_opc( task, PC_DUP );
            operand->pushed = true;
         }
      }
      if ( object.add_str_arg ) {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, ( int ) object.do_str );
      }
      t_add_opc( task, PC_CALL_DISCARD );
      t_add_arg( task, object.func_set );
      if ( object.method == METHOD_ELEMENT ) {
         if ( operand->push ) {
            if ( object.add_str_arg ) {
               t_add_opc( task, PC_PUSH_NUMBER );
               t_add_arg( task, ( int ) object.do_str );
            }
            t_add_opc( task, PC_CALL );
            t_add_arg( task, object.func_get );
            operand->pushed = true;
         }
      }
   }
   else if ( object.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         t_add_opc( task, PC_DUP );
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

void do_post_inc( struct task* task, struct operand* operand,
   struct unary* unary ) {
   struct operand object;
   init_operand( &object );
   object.action = ACTION_PUSH_VAR;
   do_operand( task, &object, unary->operand );
   if ( object.trigger == TRIGGER_FUNCTION ) {
      if ( object.method == METHOD_ELEMENT ) {
         if ( operand->push ) {
            t_add_opc( task, PC_DUP );
            if ( object.add_str_arg ) {
               t_add_opc( task, PC_PUSH_NUMBER );
               t_add_arg( task, ( int ) object.do_str );
            }
            t_add_opc( task, PC_CALL );
            t_add_arg( task, object.func_get );
            t_add_opc( task, PC_SWAP );
            operand->pushed = true;
         }
         t_add_opc( task, PC_DUP );
         if ( object.add_str_arg ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, ( int ) object.do_str );
         }
      }
      t_add_opc( task, PC_CALL );
      t_add_arg( task, object.func_get );
      if ( object.method == METHOD_INDEXED ) {
         if ( operand->push ) {
            t_add_opc( task, PC_DUP );
            operand->pushed = true;
         }
      }
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, 1 );
      if ( unary->op == UOP_POST_INC ) {
         t_add_opc( task, PC_ADD );
      }
      else {
         t_add_opc( task, PC_SUB );
      }
      if ( object.add_str_arg ) {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, ( int ) object.do_str );
      }
      t_add_opc( task, PC_CALL_DISCARD );
      t_add_arg( task, object.func_set );
   }
   else if ( object.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         t_add_opc( task, PC_DUP );
         push_element( task, object.storage, object.index );
         t_add_opc( task, PC_SWAP );
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

void do_call( struct task* task, struct operand* operand, struct call* call ) {
   struct func* func = call->func;
   if ( func->type == FUNC_ASPEC ) {
      int num = 0;
      list_iter_t i;
      list_iter_init( &i, &call->args );
      while ( ! list_end( &i ) ) {
         push_expr( task, list_data( &i ), false );
         list_next( &i );
         ++num;
      }
      struct func_aspec* aspec = func->impl;
      if ( operand->push ) {
         while ( num < 5 ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, 0 );
            ++num;
         }
         t_add_opc( task, PC_LSPEC5_RESULT );
         t_add_arg( task, aspec->id );
      }
      else if ( num ) {
         t_add_opc( task, g_aspec_code[ num - 1 ] );
         t_add_arg( task, aspec->id );
      }
      else {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, 0 );
         t_add_opc( task, PC_LSPEC1 );
         t_add_arg( task, aspec->id );
      }
   }
   else if ( func->type == FUNC_EXT ) {
      list_iter_t i;
      list_iter_init( &i, &call->args );
      struct param* param = func->params;
      while ( ! list_end( &i ) ) {
         struct expr* expr = list_data( &i );
         struct operand arg;
         init_operand( &arg );
         arg.push = true;
         do_operand( task, &arg, expr->root );
         list_next( &i );
         param = param->next;
      }
      int count = list_size( &call->args );
      int skipped = 0;
      while ( param ) {
         if ( param->expr->folded && ! param->expr->value ) {
            ++skipped;
         }
         else {
            count += skipped;
            while ( skipped ) {
               t_add_opc( task, PC_PUSH_NUMBER );
               t_add_arg( task, 0 );
               --skipped;
            }
            struct operand arg;
            init_operand( &arg );
            arg.push = true;
            do_operand( task, &arg, param->expr->root );
            ++count;
         }
         param = param->next;
      }
      struct func_ext* impl = func->impl;
      t_add_opc( task, PC_CALL_FUNC );
      t_add_arg( task, count );
      t_add_arg( task, impl->id );
      if ( func->value ) {
         operand->pushed = true;
      }
   }
   else if ( func->type == FUNC_DED ) {
      struct func_ded* ded = func->impl;
      list_iter_t i;
      list_iter_init( &i, &call->args );
      struct param* param = func->params;
      while ( ! list_end( &i ) ) {
         push_expr( task, list_data( &i ), false );
         list_next( &i );
         param = param->next;
      }
      // Default arguments.
      while ( param ) {
         struct operand arg;
         init_operand( &arg );
         arg.push = true;
         do_operand( task, &arg, param->expr->root );
         param = param->next;
      }
      t_add_opc( task, ded->opcode );
      if ( func->value ) {
         operand->pushed = true;
      }
   }
   else if ( func->type == FUNC_FORMAT ) {
      t_add_opc( task, PC_BEGIN_PRINT );
      list_iter_t i;
      list_iter_init( &i, &call->args );
      struct node* node = list_data( &i );
      // Format-block:
      if ( node->type == NODE_FORMAT_BLOCK_ARG ) {
         struct format_block_arg* arg = ( struct format_block_arg* ) node;
         do_block( task, arg->format_block );
         list_next( &i );
      }
      // Format-list:
      else {
         while ( ! list_end( &i ) ) {
            struct node* node = list_data( &i );
            if ( node->type == NODE_FORMAT_ITEM ) {
               do_format_item( task, ( struct format_item* ) node );
               list_next( &i );
            }
            else {
               break;
            }
         }
      }
      // Other arguments.
      if ( func->max_param > 1 ) {
         t_add_opc( task, PC_MORE_HUD_MESSAGE );
         int param = 1;
         while ( ! list_end( &i ) ) {
            if ( param == func->min_param ) {
               t_add_opc( task, PC_OPT_HUD_MESSAGE );
            }
            push_expr( task, list_data( &i ), false );
            ++param;
            list_next( &i );
         }
      }
      struct func_format* format = func->impl;
      t_add_opc( task, format->opcode );
   }
   else if ( func->type == FUNC_USER ) {
      list_iter_t i;
      list_iter_init( &i, &call->args );
      struct param* param = func->params;
      while ( ! list_end( &i ) ) {
         push_expr( task, list_data( &i ), false );
         list_next( &i );
         param = param->next;
      }
      // Default arguments.
      while ( param ) {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, 0 );
         param = param->next;
      }
      // Number of real arguments passed, for a function with default
      // parameters.
      if ( func->min_param != func->max_param ) {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, list_size( &call->args ) );
      }
      struct func_user* impl = func->impl;
      if ( func->value ) {
         t_add_opc( task, PC_CALL );
         t_add_arg( task, impl->index );
         operand->pushed = true;
      }
      else {
         t_add_opc( task, PC_CALL_DISCARD );
         t_add_arg( task, impl->index );
      }
   }
   else if ( func->type == FUNC_INTERNAL ) {
      struct func_intern* impl = func->impl; 
      if ( impl->id == INTERN_FUNC_ACS_EXECWAIT ) {
         list_iter_t i;
         list_iter_init( &i, &call->args );
         push_expr( task, list_data( &i ), false );
         t_add_opc( task, PC_DUP );
         list_next( &i );
         while ( ! list_end( &i ) ) {
            push_expr( task, list_data( &i ), true );
            list_next( &i );
         }
         t_add_opc( task, g_aspec_code[ list_size( &call->args ) - 1 ] );
         t_add_arg( task, 80 );
         t_add_opc( task, PC_SCRIPT_WAIT );
      }
   }
}

void do_format_item( struct task* task, struct format_item* item ) {
   if ( item->cast == FCAST_ARRAY ) {
      struct operand object;
      init_operand( &object );
      object.action = ACTION_PUSH_VAR;
      do_operand( task, &object, item->expr->root );
      if ( object.trigger == TRIGGER_FUNCTION ) {
         int loop = t_tell( task );
         t_add_opc( task, PC_DUP );
         // Get value.
         if ( object.add_str_arg ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, ( int ) object.do_str );
         }
         t_add_opc( task, PC_CALL );
         t_add_arg( task, object.func_get );
         // Bail out on NUL character.
         int bail = t_tell( task );
         t_add_opc( task, PC_CASE_GOTO );
         t_add_arg( task, 0 );
         t_add_arg( task, 0 );
         t_add_opc( task, PC_PRINT_CHARACTER );
         // Next element.
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, 1 );
         t_add_opc( task, PC_ADD );
         t_add_opc( task, PC_GOTO );
         t_add_arg( task, loop );
         int done = t_tell( task );
         t_add_opc( task, PC_DROP );
         t_seek( task, bail );
         t_add_opc( task, PC_CASE_GOTO );
         t_add_arg( task, 0 );
         t_add_arg( task, done );
         t_seek( task, OBJ_SEEK_END );
      }
      else {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, object.index );
         int code = PC_PRINT_MAP_CHAR_ARRAY;
         switch ( object.storage ) {
         case STORAGE_WORLD:
            code = PC_PRINT_WORLD_CHAR_ARRAY;
            break;
         case STORAGE_GLOBAL:
            code = PC_PRINT_GLOBAL_CHAR_ARRAY;
            break;
         default:
            break;
         }
         t_add_opc( task, code );
      }
   }
   else {
      static const int casts[] = {
         PC_PRINT_BINARY,
         PC_PRINT_CHARACTER,
         PC_PRINT_NUMBER,
         PC_PRINT_FIXED,
         PC_PRINT_BIND,
         PC_PRINT_LOCALIZED,
         PC_PRINT_NAME,
         PC_PRINT_STRING,
         PC_PRINT_HEX };
      STATIC_ASSERT( FCAST_TOTAL == 10 );
      push_expr( task, item->expr, false );
      t_add_opc( task, casts[ item->cast - 1 ] );
   }
}

void do_binary( struct task* task, struct operand* operand,
   struct binary* binary ) {
   // Logical-or and logical-and both perform shortcircuit evaluation.
   if ( binary->op == BOP_LOG_OR ) {
      struct operand lside;
      init_operand( &lside );
      lside.push = true;
      lside.push_temp = true;
      do_operand( task, &lside, binary->lside );
      int test = t_tell( task );
      t_add_opc( task, PC_IF_NOT_GOTO );
      t_add_arg( task, 0 );
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, 1 );
      int jump = t_tell( task );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, 0 );
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      rside.push_temp = true;
      int next = t_tell( task );
      do_operand( task, &rside, binary->rside );
      // Optimization: When doing a calculation temporarily, there's no need to
      // convert the second operand to a 0 or 1. Just use the operand directly.
      if ( ! operand->push_temp ) {
         t_add_opc( task, PC_NEGATE_LOGICAL );
         t_add_opc( task, PC_NEGATE_LOGICAL );
      }
      int done = t_tell( task );
      t_seek( task, test );
      t_add_opc( task, PC_IF_NOT_GOTO );
      t_add_arg( task, next );
      t_seek( task, jump );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, done );
      t_seek( task, OBJ_SEEK_END );
      operand->pushed = true;
   }
   else if ( binary->op == BOP_LOG_AND ) {
      struct operand lside;
      init_operand( &lside );
      lside.push = true;
      lside.push_temp = true;
      do_operand( task, &lside, binary->lside );
      int test = t_tell( task );
      t_add_opc( task, PC_IF_GOTO );
      t_add_arg( task, 0 );
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, 0 );
      int jump = t_tell( task );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, 0 );
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      rside.push_temp = true;
      int next = t_tell( task );
      do_operand( task, &rside, binary->rside );
      if ( ! operand->push_temp ) {
         t_add_opc( task, PC_NEGATE_LOGICAL );
         t_add_opc( task, PC_NEGATE_LOGICAL );
      }
      int done = t_tell( task );
      t_seek( task, test );
      t_add_opc( task, PC_IF_GOTO );
      t_add_arg( task, next );
      t_seek( task, jump );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, done );
      t_seek( task, OBJ_SEEK_END );
      operand->pushed = true;
   }
   else {
      struct operand lside;
      init_operand( &lside );
      lside.push = true;
      do_operand( task, &lside, binary->lside );
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      do_operand( task, &rside, binary->rside );
      int code = PC_NONE;
      switch ( binary->op ) {
      case BOP_BIT_OR: code = PC_OR_BITWISE; break;
      case BOP_BIT_XOR: code = PC_EOR_BITWISE; break;
      case BOP_BIT_AND: code = PC_AND_BITWISE; break;
      case BOP_EQ: code = PC_EQ; break;
      case BOP_NEQ: code = PC_NE; break;
      case BOP_LT: code = PC_LT; break;
      case BOP_LTE: code = PC_LE; break;
      case BOP_GT: code = PC_GT; break;
      case BOP_GTE: code = PC_GE; break;
      case BOP_SHIFT_L: code = PC_LSHIFT; break;
      case BOP_SHIFT_R: code = PC_RSHIFT; break;
      case BOP_ADD: code = PC_ADD; break;
      case BOP_SUB: code = PC_SUB; break;
      case BOP_MUL: code = PC_MUL; break;
      case BOP_DIV: code = PC_DIV; break;
      case BOP_MOD: code = PC_MOD; break;
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
   if ( var->use_interface ) {
      if ( var->dim || ! var->type->primitive ) {
         operand->method = METHOD_ELEMENT;
      }
      else {
         operand->method = METHOD_INDEXED;
      }
      if ( var->storage == STORAGE_MAP ) {
         operand->trigger = TRIGGER_FUNCTION;
         operand->func_get = var->get;
         operand->func_set = var->set;
         if ( ! var->type->primitive ) {
            operand->add_str_arg = true;
         }
      }
      else {
         operand->index = var->index;
      }
   }
   else if ( ! var->type->primitive ) {
      operand->method = METHOD_ELEMENT;
      if ( var->storage == STORAGE_WORLD || var->storage == STORAGE_GLOBAL ) {
         operand->index = var->index;
         // In a world or global array, the first half contains the integer
         // members, and the second half contains the string members.
         if ( operand->do_str ) {
            operand->base = var->size;
         }
      }
      // For map storage.
      else {
         if ( operand->do_str ) {
            if ( var->shared_str ) {
               operand->index = SHARED_ARRAY_STR;
               operand->base = var->index_str;
            }
            else {
               operand->index = var->index_str;
            }
         }
         else {
            if ( var->shared ) {
               operand->index = SHARED_ARRAY;
               operand->base = var->index;
            }
            else {
               operand->index = var->index;
            }
         }
      }
   }
   else if ( var->dim ) {
      operand->method = METHOD_ELEMENT;
      if ( var->shared ) {
         operand->index = SHARED_ARRAY;
         operand->base = var->index;
      }
      else if ( var->shared_str ) {
         operand->index = SHARED_ARRAY_STR;
         operand->base = var->index_str;
      }
      else {
         operand->index = var->index;
      }
   }
   else {
      if ( var->shared ) {
         operand->method = METHOD_ELEMENT;
         operand->index = SHARED_ARRAY;
         operand->base = var->index;
      }
      else if ( var->shared_str ) {
         operand->method = METHOD_ELEMENT;
         operand->index = SHARED_ARRAY_STR;
         operand->base = var->index_str;
      }
      else {
         operand->method = METHOD_INDEXED;
         operand->index = var->index;
      }
   }
}

void do_var_name( struct task* task, struct operand* operand,
   struct var* var ) {
   set_var( task, operand, var );
   // For element-based variables, an index marking the start of the variable
   // data needs to be on the stack.
   if ( operand->method == METHOD_ELEMENT ) {
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, operand->base );
   }
   else {
      if ( operand->action == ACTION_PUSH_VALUE ) {
         if ( operand->trigger == TRIGGER_FUNCTION ) {
            t_add_opc( task, PC_CALL );
            t_add_arg( task, operand->func_get );
         }
         else {
            push_indexed( task, operand->storage, operand->index );
         }
         operand->pushed = true;
      }
   }
}

void do_object( struct task* task, struct operand* operand,
   struct node* node ) {
   if ( node->type == NODE_ACCESS ) {
      struct access* access = ( struct access* ) node;
      if ( access->rside->type == NODE_TYPE_MEMBER ) {
         struct type_member* member = ( struct type_member* ) access->rside;
         operand->do_str = member->type->is_str;
      }
      do_access( task, operand, access );
   }
   else if ( node->type == NODE_SUBSCRIPT ) {
      do_subscript( task, operand, ( struct subscript* ) node );
   }
   if ( operand->method == METHOD_ELEMENT ) {
      if ( operand->pushed_element ) {
         if ( operand->base ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, operand->base );
            t_add_opc( task, PC_ADD );
         }
      }
      else {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, operand->base );
      }
   }
   if ( operand->action == ACTION_PUSH_VALUE &&
      operand->method != METHOD_NONE ) {
      if ( operand->trigger == TRIGGER_FUNCTION ) {
         if ( operand->add_str_arg ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, ( int ) operand->do_str );
         }
         t_add_opc( task, PC_CALL );
         t_add_arg( task, operand->func_get );
      }
      else if ( operand->method == METHOD_ELEMENT ) {
         push_element( task, operand->storage, operand->index );
      }
      else {
         push_indexed( task, operand->storage, operand->index );
      }
      operand->pushed = true;
   }
}

void do_subscript( struct task* task, struct operand* operand,
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
      do_access( task, operand, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      do_subscript( task, operand, ( struct subscript* ) lside );
   }
   // Dimension:
   struct operand index;
   init_operand( &index );
   index.push = true;
   index.push_temp = true;
   do_operand( task, &index, subscript->index );
   if ( operand->dim->next ) {
      t_add_opc( task, PC_PUSH_NUMBER );
      if ( operand->do_str ) {
         t_add_arg( task, operand->dim->size_str );
      }
      else {
         t_add_arg( task, operand->dim->size );
      }
      t_add_opc( task, PC_MUL );
   }
   else if ( ! operand->type->primitive ) {
      t_add_opc( task, PC_PUSH_NUMBER );
      if ( operand->do_str ) {
         t_add_arg( task, operand->type->size_str );
      }
      else {
         t_add_arg( task, operand->type->size );
      }
      t_add_opc( task, PC_MUL );
   }
   if ( operand->pushed_element ) {
      t_add_opc( task, PC_ADD );
   }
   else {
      operand->pushed_element = true;
   }
   operand->dim = operand->dim->next;
}

void do_access( struct task* task, struct operand* operand,
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
   if ( object->type == NODE_MODULE || object->type == NODE_MODULE_SELF ) {
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
      do_constant( task, operand, ( struct constant* ) lside );
   }
   else if ( lside->type == NODE_ACCESS ) {
      do_access( task, operand, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      do_subscript( task, operand, ( struct subscript* ) lside );
   }
   // Right side:
   if ( rside && rside->type == NODE_TYPE_MEMBER ) {
      struct type_member* member = ( struct type_member* ) rside;
      t_add_opc( task, PC_PUSH_NUMBER );
      if ( operand->do_str ) {
         t_add_arg( task, member->offset_str );
      }
      else {
         t_add_arg( task, member->offset );
      }
      if ( operand->pushed_element ) {
         t_add_opc( task, PC_ADD );
      }
      else {
         operand->pushed_element = true;
      }
      operand->type = member->type;
      operand->dim = member->dim;
   }
}

void do_assign( struct task* task, struct operand* operand,
   struct assign* assign ) {
   struct operand lside;
   init_operand( &lside );
   lside.action = ACTION_PUSH_VAR;
   do_operand( task, &lside, assign->lside );
   if ( lside.trigger == TRIGGER_FUNCTION ) {
      if ( lside.method == METHOD_ELEMENT ) {
         if ( assign->op != AOP_NONE ) {
            t_add_opc( task, PC_DUP );
         }
         if ( operand->push ) {
            t_add_opc( task, PC_DUP );
         }
      }
      // When using a compound assignment operator, the value of the variable
      // is retrieved and manipulated. 
      if ( assign->op != AOP_NONE ) {
         if ( lside.add_str_arg ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, ( int ) lside.do_str );
         }
         t_add_opc( task, PC_CALL );
         t_add_arg( task, lside.func_get );
      }
      // Push right side.
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      do_operand( task, &rside, assign->rside );
      // Manipulate the retrieved value using the pushed right side.
      if ( assign->op != AOP_NONE ) {
         static const int code[] = {
            PC_ADD,
            PC_SUB,
            PC_MUL,
            PC_DIV,
            PC_MOD,
            PC_LSHIFT,
            PC_RSHIFT,
            PC_AND_BITWISE,
            PC_EOR_BITWISE,
            PC_OR_BITWISE };
         STATIC_ASSERT( AOP_TOTAL == 11 )
         t_add_opc( task, code[ assign->op - 1 ] );
      }
      if ( lside.method == METHOD_INDEXED ) {
         if ( operand->push ) {
            t_add_opc( task, PC_DUP );
         }
      }
      // Save the value.
      if ( lside.add_str_arg ) {
         t_add_opc( task, PC_PUSH_NUMBER );
         t_add_arg( task, ( int ) lside.do_str );
      }
      t_add_opc( task, PC_CALL_DISCARD );
      t_add_arg( task, lside.func_set );
      // Retrieve new value if needed further.
      if ( lside.method == METHOD_ELEMENT ) {
         if ( operand->push ) {
            if ( lside.add_str_arg ) {
               t_add_opc( task, PC_PUSH_NUMBER );
               t_add_arg( task, ( int ) lside.do_str );
            }
            t_add_opc( task, PC_CALL );
            t_add_arg( task, lside.func_get );
         }
      }
   }
   else if ( lside.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         t_add_opc( task, PC_DUP );
      }
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      do_operand( task, &rside, assign->rside );
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
      do_operand( task, &rside, assign->rside );
      if ( assign->op == AOP_NONE && operand->push ) {
         t_add_opc( task, PC_DUP );
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
   int code = PC_PUSH_SCRIPT_VAR;
   switch ( storage ) {
   case STORAGE_MAP:
      code = PC_PUSH_MAP_VAR;
      break;
   case STORAGE_WORLD:
      code = PC_PUSH_WORLD_VAR;
      break;
   case STORAGE_GLOBAL:
      code = PC_PUSH_GLOBAL_VAR;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void push_element( struct task* task, int storage, int index ) {
   int code = PC_PUSH_MAP_ARRAY;
   switch ( storage ) {
   case STORAGE_WORLD:
      code = PC_PUSH_WORLD_ARRAY;
      break;
   case STORAGE_GLOBAL:
      code = PC_PUSH_GLOBAL_ARRAY;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void update_indexed( struct task* task, int storage, int index, int op ) {
   static const int code[] = {
      PC_ASSIGN_SCRIPT_VAR, PC_ASSIGN_MAP_VAR, PC_ASSIGN_WORLD_VAR,
         PC_ASSIGN_GLOBAL_VAR,
      PC_ADD_SCRIPT_VAR, PC_ADD_MAP_VAR, PC_ADD_WORLD_VAR, PC_ADD_GLOBAL_VAR,
      PC_SUB_SCRIPT_VAR, PC_SUB_MAP_VAR, PC_SUB_WORLD_VAR, PC_SUB_GLOBAL_VAR,
      PC_MUL_SCRIPT_VAR, PC_MUL_MAP_VAR, PC_MUL_WORLD_VAR, PC_MUL_GLOBAL_VAR,
      PC_DIV_SCRIPT_VAR, PC_DIV_MAP_VAR, PC_DIV_WORLD_VAR, PC_DIV_GLOBAL_VAR,
      PC_MOD_SCRIPT_VAR, PC_MOD_MAP_VAR, PC_MOD_WORLD_VAR, PC_MOD_GLOBAL_VAR,
      PC_LS_SCRIPT_VAR, PC_LS_MAP_VAR, PC_LS_WORLD_VAR, PC_LS_GLOBAL_VAR,
      PC_RS_SCRIPT_VAR, PC_RS_MAP_VAR, PC_RS_WORLD_VAR, PC_RS_GLOBAL_VAR,
      PC_AND_SCRIPT_VAR, PC_AND_MAP_VAR, PC_AND_WORLD_VAR, PC_AND_GLOBAL_VAR,
      PC_EOR_SCRIPT_VAR, PC_EOR_MAP_VAR, PC_EOR_WORLD_VAR, PC_EOR_GLOBAL_VAR,
      PC_OR_SCRIPT_VAR, PC_OR_MAP_VAR, PC_OR_WORLD_VAR, PC_OR_GLOBAL_VAR };
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
      PC_ASSIGN_MAP_ARRAY, PC_ASSIGN_WORLD_ARRAY, PC_ASSIGN_GLOBAL_ARRAY,
      PC_ADD_MAP_ARRAY, PC_ADD_WORLD_ARRAY, PC_ADD_GLOBAL_ARRAY,
      PC_SUB_MAP_ARRAY, PC_SUB_WORLD_ARRAY, PC_SUB_GLOBAL_ARRAY,
      PC_MUL_MAP_ARRAY, PC_MUL_WORLD_ARRAY, PC_MUL_GLOBAL_ARRAY,
      PC_DIV_MAP_ARRAY, PC_DIV_WORLD_ARRAY, PC_DIV_GLOBAL_ARRAY,
      PC_MOD_MAP_ARRAY, PC_MOD_WORLD_ARRAY, PC_MOD_GLOBAL_ARRAY,
      PC_LS_MAP_ARRAY, PC_LS_WORLD_ARRAY, PC_LS_GLOBAL_ARRAY,
      PC_RS_MAP_ARRAY, PC_RS_WORLD_ARRAY, PC_RS_GLOBAL_ARRAY,
      PC_AND_MAP_ARRAY, PC_AND_WORLD_ARRAY, PC_AND_GLOBAL_ARRAY,
      PC_EOR_MAP_ARRAY, PC_EOR_WORLD_ARRAY, PC_EOR_GLOBAL_ARRAY,
      PC_OR_MAP_ARRAY, PC_OR_WORLD_ARRAY, PC_OR_GLOBAL_ARRAY };
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
   int code = PC_INC_SCRIPT_VAR;
   switch ( storage ) {
   case STORAGE_MAP:
      code = PC_INC_MAP_VAR;
      break;
   case STORAGE_WORLD:
      code = PC_INC_WORLD_VAR;
      break;
   case STORAGE_GLOBAL:
      code = PC_INC_GLOBAL_VAR;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void dec_indexed( struct task* task, int storage, int index ) {
   int code = PC_DEC_SCRIPT_VAR;
   switch ( storage ) {
   case STORAGE_MAP:
      code = PC_DEC_MAP_VAR;
      break;
   case STORAGE_WORLD:
      code = PC_DEC_WORLD_VAR;
      break;
   case STORAGE_GLOBAL:
      code = PC_DEC_GLOBAL_VAR;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void inc_element( struct task* task, int storage, int index ) {
   int code = PC_INC_MAP_ARRAY;
   switch ( storage ) {
   case STORAGE_WORLD:
      code = PC_INC_WORLD_ARRAY;
      break;
   case STORAGE_GLOBAL:
      code = PC_INC_GLOBAL_ARRAY;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void dec_element( struct task* task, int storage, int index ) {
   int code = PC_DEC_MAP_ARRAY;
   switch ( storage ) {
   case STORAGE_WORLD:
      code = PC_DEC_WORLD_ARRAY;
      break;
   case STORAGE_GLOBAL:
      code = PC_DEC_GLOBAL_ARRAY;
      break;
   default:
      break;
   }
   t_add_opc( task, code );
   t_add_arg( task, index );
}

void do_if( struct task* task, struct if_stmt* stmt ) {
   struct operand expr;
   init_operand( &expr );
   expr.push = true;
   expr.push_temp = true;
   do_operand( task, &expr, stmt->expr->root );
   int cond = t_tell( task );
   t_add_opc( task, PC_IF_NOT_GOTO );
   t_add_arg( task, 0 );
   do_node( task, stmt->body );
   int bail = t_tell( task );
   if ( stmt->else_body ) {
      // Exit from if block:
      int bail_if_block = t_tell( task );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, 0 ); 
      bail = t_tell( task );
      do_node( task, stmt->else_body );
      int stmt_end = t_tell( task );
      t_seek( task, bail_if_block );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, stmt_end );
   }
   t_seek( task, cond );
   t_add_opc( task, PC_IF_NOT_GOTO );
   t_add_arg( task, bail );
   t_seek( task, OBJ_SEEK_END );
}

void do_switch( struct task* task, struct switch_stmt* stmt ) {
   struct operand expr;
   init_operand( &expr );
   expr.push = true;
   expr.push_temp = true;
   do_operand( task, &expr, stmt->expr->root );
   int num_cases = 0;
   struct case_label* label = stmt->case_head;
   while ( label ) {
      ++num_cases;
      label = label->next;
   }
   int test = t_tell( task );
   if ( num_cases ) {
      t_add_opc( task, PC_CASE_GOTO_SORTED );
      t_add_arg( task, 0 );
      for ( int i = 0; i < num_cases; ++i ) {
         t_add_arg( task, 0 );
         t_add_arg( task, 0 );
      }
   }
   t_add_opc( task, PC_DROP );
   int fail = t_tell( task );
   t_add_opc( task, PC_GOTO );
   t_add_arg( task, 0 );
   do_node( task, stmt->body );
   int done = t_tell( task );
   if ( num_cases ) {
      t_seek( task, test );
      t_add_opc( task, PC_CASE_GOTO_SORTED );
      t_add_arg( task, num_cases );
      label = stmt->case_head;
      while ( label ) {
         t_add_arg( task, label->expr->value );
         t_add_arg( task, label->offset );
         label = label->next;
      }
   }
   t_seek( task, fail );
   t_add_opc( task, PC_GOTO );
   int fail_pos = done;
   if ( stmt->case_default ) {
      fail_pos = stmt->case_default->offset;
   }
   t_add_arg( task, fail_pos );
   do_jump_target( task, stmt->jump_break, done );
}

void do_case( struct task* task, struct case_label* label ) {
   label->offset = t_tell( task );
}

void do_while( struct task* task, struct while_stmt* stmt ) {
   // Optimization: Don't output the loop if the condition is known at
   // compile-time and is false. It's dead code.
   if ( stmt->expr->folded && ( ( (
      stmt->type == WHILE_WHILE || stmt->type == WHILE_DO_WHILE ) &&
      ! stmt->expr->value ) || ( (
      stmt->type == WHILE_UNTIL || stmt->type == WHILE_DO_UNTIL ) &&
      stmt->expr->value ) ) ) {
      return;
   }
   if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
      int jump = 0;
      // Optimization: If the loop condition is constant, the code for checking
      // the condition is not needed.
      if ( ! stmt->expr->folded ) {
         jump = t_tell( task );
         t_add_opc( task, PC_GOTO );
         t_add_arg( task, 0 );
      }
      int body = t_tell( task );
      do_node( task, stmt->body );
      int test = t_tell( task );
      int done = 0;
      if ( ! stmt->expr->folded ) {
         struct operand expr;
         init_operand( &expr );
         expr.push = true;
         expr.push_temp = true;
         do_operand( task, &expr, stmt->expr->root );
         int code = PC_IF_GOTO;
         if ( stmt->type == WHILE_UNTIL ) {
            code = PC_IF_NOT_GOTO;
         }
         t_add_opc( task, code );
         t_add_arg( task, body );
         done = t_tell( task );
         t_seek( task, jump );
         t_add_opc( task, PC_GOTO );
         t_add_arg( task, test );
      }
      else {
         t_add_opc( task, PC_GOTO );
         t_add_arg( task, body );
         done = t_tell( task );
      }
      do_jump_target( task, stmt->jump_continue, test );
      do_jump_target( task, stmt->jump_break, done );
   }
   else {
      int body = t_tell( task );
      do_node( task, stmt->body );
      int test = t_tell( task );
      int done = 0;
      if ( ! stmt->expr->folded ) {
         struct operand expr;
         init_operand( &expr );
         expr.push = true;
         expr.push_temp = true;
         do_operand( task, &expr, stmt->expr->root );
         int code = PC_IF_GOTO;
         if ( stmt->type == WHILE_DO_UNTIL ) {
            code = PC_IF_NOT_GOTO;
         }
         t_add_opc( task, code );
         t_add_arg( task, body );
         done = t_tell( task );
      }
      else {
         t_add_opc( task, PC_GOTO );
         t_add_arg( task, body );
         done = t_tell( task );
      }
      do_jump_target( task, stmt->jump_continue, test );
      do_jump_target( task, stmt->jump_break, done );
   }
}

void do_for( struct task* task, struct for_stmt* stmt ) {
   if ( stmt->init ) {
      struct expr_link* link = stmt->init;
      while ( link ) {
         do_expr_stmt( task, link->expr );
         link = link->next;
      }
   }
   else if ( list_size( &stmt->vars ) ) {
      list_iter_t i;
      list_iter_init( &i, &stmt->vars );
      while ( ! list_end( &i ) ) {
         do_var( task, list_data( &i ) );
         list_next( &i );
      }
   }
   int jump = 0;
   if ( stmt->expr ) {
      // Optimization:
      if ( stmt->expr->folded ) {
         if ( ! stmt->expr->value ) {
            return;
         }
      }
      // Optimization:
      else {
         jump = t_tell( task );
         t_add_opc( task, PC_GOTO );
         t_add_arg( task, 0 );
      }
   }
   int body = t_tell( task );
   do_node( task, stmt->body );
   int next = t_tell( task );
   if ( stmt->post ) {
      struct expr_link* link = stmt->post;
      while ( link ) {
         do_expr_stmt( task, link->expr );
         link = link->next;
      }
   }
   int test = 0;
   if ( stmt->expr ) {
      // Optimization:
      if ( stmt->expr->folded ) {
         t_add_opc( task, PC_GOTO );
         t_add_arg( task, body );
      }
      else {
         test = t_tell( task );
         struct operand operand;
         init_operand( &operand );
         operand.push = true;
         operand.push_temp = true;
         do_operand( task, &operand, stmt->expr->root );
         t_add_opc( task, PC_IF_GOTO );
         t_add_arg( task, body );
      }
   }
   else {
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, body );
   }
   int done = t_tell( task );
   if ( stmt->expr ) {
      if ( ! stmt->expr->folded ) {
         t_seek( task, jump );
         t_add_opc( task, PC_GOTO );
         t_add_arg( task, test );
      }
   }
   do_jump_target( task, stmt->jump_continue, next );
   do_jump_target( task, stmt->jump_break, done );
}

void do_jump( struct task* task, struct jump* jump ) {
   jump->obj_pos = t_tell( task );
   t_add_opc( task, PC_GOTO );
   t_add_arg( task, 0 );
}

void do_jump_target( struct task* task, struct jump* jump, int pos ) {
   while ( jump ) {
      t_seek( task, jump->obj_pos );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, pos );
      jump = jump->next;
   }
   t_seek( task, OBJ_SEEK_END );
}

void do_return( struct task* task, struct return_stmt* stmt ) {
   if ( stmt->expr ) {
      struct operand operand;
      init_operand( &operand );
      operand.push = true;
      do_operand( task, &operand, stmt->expr->root );
      t_add_opc( task, PC_RETURN_VAL );
   }
   else if ( stmt->expr_format ) {
      struct operand operand;
      init_operand( &operand );
      operand.push = true;
      do_operand( task, &operand, stmt->expr_format->expr->root );
      t_add_opc( task, PC_RETURN_VAL );
   }
   else {
      t_add_opc( task, PC_RETURN_VOID );
   }
}

void do_paltrans( struct task* task, struct paltrans* trans ) {
   push_expr( task, trans->number, true );
   t_add_opc( task, PC_START_TRANSLATION );
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
         t_add_opc( task, PC_TRANSLATION_RANGE2 );
      }
      else {
         push_expr( task, range->value.ent.begin, true );
         push_expr( task, range->value.ent.end, true );
         t_add_opc( task, PC_TRANSLATION_RANGE1 );
      }
      range = range->next;
   }
   t_add_opc( task, PC_END_TRANSLATION );
}

void do_default_params( struct task* task, struct func* func ) {
   struct param* param = func->params;
   while ( param && ! param->expr ) {
      param = param->next;
   }
   int num = 0;
   int num_cases = 0;
   struct param* start = param;
   while ( param ) {
      ++num;
      if ( ! param->expr->folded || param->expr->value ) {
         num_cases = num;
      }
      param = param->next;
   }
   if ( num_cases ) {
      // A hidden parameter is used to store the number of arguments passed to
      // the function. This parameter is found after the last visible parameter.
      t_add_opc( task, PC_PUSH_SCRIPT_VAR );
      t_add_arg( task, func->max_param );
      int jump = t_tell( task );
      t_add_opc( task, PC_CASE_GOTO_SORTED );
      t_add_arg( task, 0 );
      param = start;
      int i = 0;
      while ( param && i < num_cases ) {
         t_add_arg( task, 0 );
         t_add_arg( task, 0 );
         param = param->next;
         ++i;
      }
      t_add_opc( task, PC_DROP );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, 0 );
      param = start;
      while ( param ) {
         param->obj_pos = t_tell( task );
         if ( ! param->expr->folded || param->expr->value ) {
            push_expr( task, param->expr, false );
            update_indexed( task, STORAGE_LOCAL, param->index, AOP_NONE );
         }
         param = param->next;
      }
      int done = t_tell( task );
      // Add case positions.
      t_seek( task, jump );
      t_add_opc( task, PC_CASE_GOTO_SORTED );
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
      t_add_opc( task, PC_DROP );
      t_add_opc( task, PC_GOTO );
      t_add_arg( task, done );
      t_seek( task, done );
   }
   if ( start ) {
      // Reset the parameter-count parameter.
      t_add_opc( task, PC_PUSH_NUMBER );
      t_add_arg( task, 0 );
      t_add_opc( task, PC_ASSIGN_SCRIPT_VAR );
      t_add_arg( task, func->max_param );
   }
}

void do_pcode( struct task* task, struct pcode* stmt ) {
   t_add_opc( task, stmt->opcode->value );
   list_iter_t i;
   list_iter_init( &i, &stmt->args );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      if ( node->type == NODE_PCODE_OFFSET ) {
         struct pcode_offset* macro = ( struct pcode_offset* ) node;
         if ( macro->label->obj_pos ) {
            t_add_arg( task, macro->label->obj_pos );
         }
         else {
            macro->obj_pos = t_tell( task );
            t_add_arg( task, 0 );
         }
      }
      else {
         struct expr* arg = ( struct expr* ) node;
         t_add_arg( task, arg->value );
      }
      list_next( &i );
   }
}