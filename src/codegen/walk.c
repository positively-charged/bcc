#include "phase.h"
#include "pcode.h"

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
   bool nested_func;
};

struct nestedfunc_writing {
   struct func* nested_funcs;
   struct call* nested_calls;
   int temps_start;
   int temps_size;
};

static void write_script( struct codegen* phase, struct script* script );
static void write_userfunc( struct codegen* phase, struct func* func );
static void visit_block( struct codegen* phase, struct block* stmt );
static void add_block_visit( struct codegen* phase );
static void pop_block_visit( struct codegen* phase );
static void visit_var( struct codegen* phase, struct var* var );
static void write_world_initz( struct codegen* phase, struct var* var );
static void init_world_array( struct codegen* phase, struct var* );
static void visit_node( struct codegen* phase, struct node* );
static void visit_script_jump( struct codegen* phase, struct script_jump* );
static void visit_label( struct codegen* phase, struct label* );
static void visit_goto( struct codegen* phase, struct goto_stmt* );
static void visit_expr( struct codegen* phase, struct expr* );
static void visit_packed_expr( struct codegen* phase, struct packed_expr* );
static void init_operand( struct operand* phase );
static void push_expr( struct codegen* phase, struct expr*, bool );
static void visit_operand( struct codegen* phase, struct operand*,
   struct node* );
static void visit_constant( struct codegen* phase, struct operand*,
   struct constant* );
static void visit_unary( struct codegen* phase, struct operand*,
   struct unary* );
static void visit_pre_inc( struct codegen* phase, struct operand*,
   struct unary* );
static void visit_post_inc( struct codegen* phase, struct operand*,
   struct unary* );
static void visit_call( struct codegen* phase, struct operand*,
   struct call* );
static void visit_aspec_call( struct codegen* phase, struct operand*,
   struct call* );
static void visit_ext_call( struct codegen* phase, struct operand*,
   struct call* );
static void visit_ded_call( struct codegen* phase, struct operand*,
   struct call* );
static void visit_format_call( struct codegen* phase, struct operand*,
   struct call* );
static void visit_format_item( struct codegen* phase, struct format_item* );
static void visit_array_format_item( struct codegen* phase,
   struct format_item* );
static void visit_user_call( struct codegen* phase, struct operand*,
   struct call* );
static void write_call_args( struct codegen* phase, struct operand* operand,
   struct call* call );
static void visit_nested_userfunc_call( struct codegen* phase,
   struct operand* operand, struct call* call );
static void visit_internal_call( struct codegen* phase, struct operand*,
   struct call* );
static void visit_binary( struct codegen* phase, struct operand*,
   struct binary* );
static void visit_assign( struct codegen* phase, struct operand*,
   struct assign* );
static void visit_conditional( struct codegen* codegen,
   struct operand* operand, struct conditional* cond );
static void write_conditional( struct codegen* codegen,
   struct operand* operand, struct conditional* cond );
static void do_var_name( struct codegen* phase, struct operand*,
   struct var* );
static void set_var( struct codegen* phase, struct operand*,
   struct var* );
static void visit_object( struct codegen* phase, struct operand*,
   struct node* );
static void visit_subscript( struct codegen* phase, struct operand*,
   struct subscript* );
static void visit_access( struct codegen* phase, struct operand*,
   struct access* );
static void push_indexed( struct codegen* phase, int, int );
static void push_element( struct codegen* phase, int, int );
static void inc_indexed( struct codegen* phase, int, int );
static void dec_indexed( struct codegen* phase, int, int );
static void inc_element( struct codegen* phase, int, int );
static void dec_element( struct codegen* phase, int, int );
static void update_indexed( struct codegen* phase, int, int, int );
static void update_element( struct codegen* phase, int, int, int );
static void visit_if( struct codegen* phase, struct if_stmt* );
static void visit_switch( struct codegen* phase, struct switch_stmt* );
static void visit_case( struct codegen* phase, struct case_label* );
static void visit_while( struct codegen* phase, struct while_stmt* );
static void visit_for( struct codegen* phase, struct for_stmt* );
static void visit_jump( struct codegen* phase, struct jump* );
static void add_jumps( struct codegen* phase, struct jump*, int );
static void visit_return( struct codegen* phase, struct return_stmt* );
static void visit_paltrans( struct codegen* phase, struct paltrans* );
static void add_default_params( struct codegen* phase, struct func*,
   int count_param, bool reset_count_param );
void init_nestedfunc_writing( struct nestedfunc_writing* writing,
   struct func* nested_funcs, struct call* nested_calls, int temps_start );
static void write_nested_funcs( struct codegen* phase,
   struct nestedfunc_writing* writing );
static void write_one_nestedfunc( struct codegen* phase,
   struct nestedfunc_writing* writing, struct func* func );
static void patch_nestedfunc_addresses( struct codegen* phase,
   struct func* func );

static const int g_aspec_code[] = {
   PCD_LSPEC1,
   PCD_LSPEC2,
   PCD_LSPEC3,
   PCD_LSPEC4,
   PCD_LSPEC5
};

void c_write_user_code( struct codegen* phase ) {
   // Scripts.
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      write_script( phase, list_data( &i ) );
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &phase->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      write_userfunc( phase, list_data( &i ) );
      list_next( &i );
   }
   // When utilizing the Little-E format, where instructions can be of
   // different size, add padding so any following data starts at an offset
   // that is multiple of 4.
   if ( phase->task->library_main->format == FORMAT_LITTLE_E ) {
      int i = alignpad( c_tell( phase ), 4 );
      while ( i ) {
         c_add_opc( phase, PCD_TERMINATE );
         --i;
      }
   }
}

void write_script( struct codegen* phase, struct script* script ) {
   script->offset = c_tell( phase );
   add_block_visit( phase );
   visit_node( phase, script->body );
   c_add_opc( phase, PCD_TERMINATE );
   if ( script->nested_funcs ) {
      struct nestedfunc_writing writing;
      init_nestedfunc_writing( &writing, script->nested_funcs,
         script->nested_calls, script->size );
      write_nested_funcs( phase, &writing );
      script->size += writing.temps_size;
   }
   pop_block_visit( phase );
}

void write_userfunc( struct codegen* phase, struct func* func ) {
   struct func_user* impl = func->impl;
   impl->obj_pos = c_tell( phase );
   add_block_visit( phase );
   add_default_params( phase, func, func->max_param, true );
   list_iter_t k;
   list_iter_init( &k, &impl->body->stmts );
   while ( ! list_end( &k ) ) {
      visit_node( phase, list_data( &k ) );
      list_next( &k );
   }
   c_add_opc( phase, PCD_RETURNVOID );
   if ( impl->nested_funcs ) {
      struct nestedfunc_writing writing;
      init_nestedfunc_writing( &writing, impl->nested_funcs,
         impl->nested_calls, impl->size );
      write_nested_funcs( phase, &writing );
      impl->size += writing.temps_size;
   }
   pop_block_visit( phase );
}

void visit_block( struct codegen* phase, struct block* block ) {
   add_block_visit( phase );
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      visit_node( phase, list_data( &i ) );
      list_next( &i );
   }
   pop_block_visit( phase );
}

void add_block_visit( struct codegen* phase ) {
   struct block_visit* visit;
   if ( phase->block_visit_free ) {
      visit = phase->block_visit_free;
      phase->block_visit_free = visit->prev;
   }
   else {
      visit = mem_alloc( sizeof( *visit ) );
   }
   visit->prev = phase->block_visit;
   phase->block_visit = visit;
   visit->format_block_usage = NULL;
   visit->nested_func = false;
   if ( ! visit->prev ) {
      phase->func_visit = visit;
   }
}

void pop_block_visit( struct codegen* phase ) {
   struct block_visit* prev = phase->block_visit->prev;
   if ( ! prev ) {
      phase->func_visit = NULL;
   }
   phase->block_visit->prev = phase->block_visit_free;
   phase->block_visit_free = phase->block_visit;
   phase->block_visit = prev;
}

void visit_var( struct codegen* phase, struct var* var ) {
   if ( var->storage == STORAGE_LOCAL ) {
      if ( var->value ) {
         push_expr( phase, var->value->expr, false );
         update_indexed( phase, var->storage, var->index, AOP_NONE );
      }
   }
   else if ( var->storage == STORAGE_WORLD ||
      var->storage == STORAGE_GLOBAL ) {
      if ( var->initial && ! var->is_constant_init ) {
         if ( var->initial->multi ) {
            init_world_array( phase, var );
         }
         else {
            write_world_initz( phase, var );
         }
      }
   }
}

void write_world_initz( struct codegen* phase, struct var* var ) {
   if ( var->value->string_initz ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) var->value->expr->root;
      for ( int i = 0; i < usage->string->length; ++i ) {
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, i );
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, usage->string->value[ i ] );
         update_element( phase, var->storage, var->index, AOP_NONE );
      }
   }
   else {
      push_expr( phase, var->value->expr, false );
      update_indexed( phase, var->storage, var->index, AOP_NONE );
   }
}

void init_world_array( struct codegen* phase, struct var* var ) {
   // Nullify array.
   c_add_opc( phase, PCD_PUSHNUMBER );
   c_add_arg( phase, var->size - 1 );
   int loop = c_tell( phase );
   c_add_opc( phase, PCD_CASEGOTO );
   c_add_arg( phase, 0 );
   c_add_arg( phase, 0 );
   c_add_opc( phase, PCD_DUP );
   c_add_opc( phase, PCD_PUSHNUMBER );
   c_add_arg( phase, 0 );
   update_element( phase, var->storage, var->index, AOP_NONE );
   c_add_opc( phase, PCD_PUSHNUMBER );
   c_add_arg( phase, 1 );
   c_add_opc( phase, PCD_SUBTRACT );
   c_add_opc( phase, PCD_GOTO );
   c_add_arg( phase, loop );
   int done = c_tell( phase );
   c_seek( phase, loop );
   c_add_opc( phase, PCD_CASEGOTO );
   c_add_arg( phase, -1 );
   c_add_arg( phase, done );
   c_seek_end( phase );
   // Assign elements.
   struct value* value = var->value;
   while ( value ) {
      // Initialize an element only if the value is not 0, because the array
      // is already nullified. String values from libraries are an exception,
      // because they need to be tagged regardless of the value.
      if ( ( ! value->expr->folded || value->expr->value ) ||
         ( value->expr->has_str && phase->task->library_main->importable ) ) {
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, value->index );
         if ( value->expr->folded && ! value->expr->has_str ) { 
            c_add_opc( phase, PCD_PUSHNUMBER );
            c_add_arg( phase, value->expr->value );
         }
         else {
            push_expr( phase, value->expr, false );
         }
         update_element( phase, var->storage, var->index, AOP_NONE );
      }
      value = value->next;
   }
}

void visit_node( struct codegen* phase, struct node* node ) {
   switch ( node->type ) {
   case NODE_BLOCK:
      visit_block( phase, ( struct block* ) node );
      break;
   case NODE_SCRIPT_JUMP:
      visit_script_jump( phase, ( struct script_jump* ) node );
      break;
   case NODE_GOTO_LABEL:
      visit_label( phase, ( struct label* ) node );
      break;
   case NODE_GOTO:
      visit_goto( phase, ( struct goto_stmt* ) node );
      break;
   case NODE_PACKED_EXPR:
      visit_packed_expr( phase, ( struct packed_expr* ) node );
      break;
   case NODE_VAR:
      visit_var( phase, ( struct var* ) node );
      break;
   case NODE_IF:
      visit_if( phase, ( struct if_stmt* ) node );
      break;
   case NODE_SWITCH:
      visit_switch( phase, ( struct switch_stmt* ) node );
      break;
   case NODE_CASE:
   case NODE_CASE_DEFAULT:
      visit_case( phase, ( struct case_label* ) node );
      break;
   case NODE_WHILE:
      visit_while( phase, ( struct while_stmt* ) node );
      break;
   case NODE_FOR:
      visit_for( phase, ( struct for_stmt* ) node );
      break;
   case NODE_JUMP:
      visit_jump( phase, ( struct jump* ) node );
      break;
   case NODE_RETURN:
      visit_return( phase, ( struct return_stmt* ) node );
      break;
   case NODE_FORMAT_ITEM:
      visit_format_item( phase, ( struct format_item* ) node );
      break;
   case NODE_PALTRANS:
      visit_paltrans( phase, ( struct paltrans* ) node );
      break;
   default:
      break;
   }
}

void visit_script_jump( struct codegen* phase, struct script_jump* stmt ) {
   switch ( stmt->type ) {
   case SCRIPT_JUMP_SUSPEND:
      c_add_opc( phase, PCD_SUSPEND );
      break;
   case SCRIPT_JUMP_RESTART:
      c_add_opc( phase, PCD_RESTART );
      break;
   default:
      c_add_opc( phase, PCD_TERMINATE );
      break;
   }
}

void visit_label( struct codegen* phase, struct label* label ) {
   label->obj_pos = c_tell( phase );
   struct goto_stmt* stmt = label->users;
   while ( stmt ) {
      if ( stmt->obj_pos ) {
         c_seek( phase, stmt->obj_pos );
         c_add_opc( phase, PCD_GOTO );
         c_add_arg( phase, label->obj_pos );
      }
      stmt = stmt->next;
   }
   c_seek_end( phase );
}

void visit_goto( struct codegen* phase, struct goto_stmt* stmt ) {
   if ( stmt->label->obj_pos ) {
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, stmt->label->obj_pos );
   }
   else {
      stmt->obj_pos = c_tell( phase );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, 0 );
   }
}

void visit_expr( struct codegen* phase, struct expr* expr ) {
   struct operand operand;
   init_operand( &operand );
   visit_operand( phase, &operand, expr->root );
   if ( operand.pushed ) {
      c_add_opc( phase, PCD_DROP );
   }
}

void visit_packed_expr( struct codegen* phase, struct packed_expr* stmt ) {
   visit_expr( phase, stmt->expr );
}

void push_expr( struct codegen* phase, struct expr* expr, bool temp ) {
   struct operand operand;
   init_operand( &operand );
   operand.push = true;
   operand.push_temp = temp;
   visit_operand( phase, &operand, expr->root );
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

void visit_operand( struct codegen* phase, struct operand* operand,
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
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, literal->value );
      operand->pushed = true;
   }
   else if ( node->type == NODE_INDEXED_STRING_USAGE ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) node;
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, usage->string->index );
      // Strings in a library need to be tagged.
      if ( phase->task->library_main->importable ) {
         c_add_opc( phase, PCD_TAGSTRING );
      }
      operand->pushed = true;
   }
   else if ( node->type == NODE_BOOLEAN ) {
      struct boolean* boolean = ( struct boolean* ) node;
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, boolean->value );
      operand->pushed = true;
   }
   else if ( node->type == NODE_CONSTANT ) {
      visit_constant( phase, operand, ( struct constant* ) node );
   }
   else if ( node->type == NODE_VAR ) {
      do_var_name( phase, operand, ( struct var* ) node );
   }
   else if ( node->type == NODE_PARAM ) {
      struct param* param = ( struct param* ) node;
      if ( operand->action == ACTION_PUSH_VALUE ) {
         push_indexed( phase, STORAGE_LOCAL, param->index );
         operand->pushed = true;
      }
      else {
         operand->storage = STORAGE_LOCAL;
         operand->index = param->index;
      }
   }
   else if ( node->type == NODE_UNARY ) {
      visit_unary( phase, operand, ( struct unary* ) node );
   }
   else if ( node->type == NODE_SUBSCRIPT || node->type == NODE_ACCESS ) {
      visit_object( phase, operand, node );
   }
   else if ( node->type == NODE_CALL ) {
      visit_call( phase, operand, ( struct call* ) node );
   }
   else if ( node->type == NODE_BINARY ) {
      visit_binary( phase, operand, ( struct binary* ) node );
   }
   else if ( node->type == NODE_ASSIGN ) {
      visit_assign( phase, operand, ( struct assign* ) node );
   }
   else if ( node->type == NODE_CONDITIONAL ) {
      visit_conditional( phase, operand, ( struct conditional* ) node );
   }
   else if ( node->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) node;
      visit_operand( phase, operand, paren->inside );
   }
   else if ( node->type == NODE_FUNC ) {
      struct func* func = ( struct func* ) node;
      if ( func->type == FUNC_ASPEC ) {
         struct func_aspec* impl = func->impl;
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, impl->id );
      }
   }
}

void visit_constant( struct codegen* phase, struct operand* operand,
   struct constant* constant ) {
   c_add_opc( phase, PCD_PUSHNUMBER );
   c_add_arg( phase, constant->value );
   if ( phase->task->library_main->importable && constant->value_node &&
      constant->value_node->has_str ) {
      c_add_opc( phase, PCD_TAGSTRING );
   }
   operand->pushed = true;
}

void visit_unary( struct codegen* phase, struct operand* operand,
   struct unary* unary ) {
   switch ( unary->op ) {
      struct operand object;
   case UOP_PRE_INC:
   case UOP_PRE_DEC:
      visit_pre_inc( phase, operand, unary );
      break;
   case UOP_POST_INC:
   case UOP_POST_DEC:
      visit_post_inc( phase, operand, unary );
      break;
   default:
      init_operand( &object );
      object.push = true;
      visit_operand( phase, &object, unary->operand );
      int code = PCD_NONE;
      switch ( unary->op ) {
      case UOP_MINUS:
         c_add_opc( phase, PCD_UNARYMINUS );
         break;
      case UOP_LOG_NOT:
         c_add_opc( phase, PCD_NEGATELOGICAL );
         break;
      case UOP_BIT_NOT:
         c_add_opc( phase, PCD_NEGATEBINARY );
         break;
      // Unary plus is ignored.
      case UOP_PLUS:
      default:
         break;
      }
      operand->pushed = true;
   }
}

void visit_pre_inc( struct codegen* phase, struct operand* operand,
   struct unary* unary ) {
   struct operand object;
   init_operand( &object );
   object.action = ACTION_PUSH_VAR;
   visit_operand( phase, &object, unary->operand );
   if ( object.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         c_add_opc( phase, PCD_DUP );
      }
      if ( unary->op == UOP_PRE_INC ) {
         inc_element( phase, object.storage, object.index );
      }
      else {
         dec_element( phase, object.storage, object.index );
      }
      if ( operand->push ) {
         push_element( phase, object.storage, object.index );
         operand->pushed = true;
      }
   }
   else {
      if ( unary->op == UOP_PRE_INC ) {
         inc_indexed( phase, object.storage, object.index );
      }
      else {
         dec_indexed( phase, object.storage, object.index );
      }
      if ( operand->push ) {
         push_indexed( phase, object.storage, object.index );
         operand->pushed = true;
      }
   }
}

void visit_post_inc( struct codegen* phase, struct operand* operand,
   struct unary* unary ) {
   struct operand object;
   init_operand( &object );
   object.action = ACTION_PUSH_VAR;
   visit_operand( phase, &object, unary->operand );
   if ( object.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         c_add_opc( phase, PCD_DUP );
         push_element( phase, object.storage, object.index );
         c_add_opc( phase, PCD_SWAP );
         operand->pushed = true;
      }
      if ( unary->op == UOP_POST_INC ) {
         inc_element( phase, object.storage, object.index );
      }
      else {
         dec_element( phase, object.storage, object.index );
      }
   }
   else {
      if ( operand->push ) {
         push_indexed( phase, object.storage, object.index );
         operand->pushed = true;
      }
      if ( unary->op == UOP_POST_INC ) {
         inc_indexed( phase, object.storage, object.index );
      }
      else {
         dec_indexed( phase, object.storage, object.index );
      }
   }
}

void visit_call( struct codegen* phase, struct operand* operand,
   struct call* call ) {
   switch ( call->func->type ) {
   case FUNC_ASPEC:
      visit_aspec_call( phase, operand, call );
      break;
   case FUNC_EXT:
      visit_ext_call( phase, operand, call );
      break;
   case FUNC_DED:
      visit_ded_call( phase, operand, call );
      break;
   case FUNC_FORMAT:
      visit_format_call( phase, operand, call );
      break;
   case FUNC_USER:
      visit_user_call( phase, operand, call );
      break;
   case FUNC_INTERNAL:
      visit_internal_call( phase, operand, call );
      break;
   }
}

void visit_aspec_call( struct codegen* phase, struct operand* operand,
   struct call* call ) {
   int num = 0;
   list_iter_t i;
   list_iter_init( &i, &call->args );
   while ( ! list_end( &i ) ) {
      push_expr( phase, list_data( &i ), false );
      list_next( &i );
      ++num;
   }
   struct func_aspec* aspec = call->func->impl;
   if ( operand->push ) {
      while ( num < 5 ) {
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, 0 );
         ++num;
      }
      c_add_opc( phase, PCD_LSPEC5RESULT );
      c_add_arg( phase, aspec->id );
   }
   else if ( num ) {
      c_add_opc( phase, g_aspec_code[ num - 1 ] );
      c_add_arg( phase, aspec->id );
   }
   else {
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, 0 );
      c_add_opc( phase, PCD_LSPEC1 );
      c_add_arg( phase, aspec->id );
   }
}

void visit_ext_call( struct codegen* phase, struct operand* operand,
   struct call* call ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( ! list_end( &i ) ) {
      struct expr* expr = list_data( &i );
      struct operand arg;
      init_operand( &arg );
      arg.push = true;
      visit_operand( phase, &arg, expr->root );
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
            c_add_opc( phase, PCD_PUSHNUMBER );
            c_add_arg( phase, 0 );
            --skipped;
         }
         push_expr( phase, param->default_value, false );
         ++count;
      }
      param = param->next;
   }
   struct func_ext* impl = call->func->impl;
   c_add_opc( phase, PCD_CALLFUNC );
   c_add_arg( phase, count );
   c_add_arg( phase, impl->id );
   operand->pushed = true;
}

void visit_ded_call( struct codegen* phase, struct operand* operand,
   struct call* call ) {
   struct func_ded* ded = call->func->impl;
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( ! list_end( &i ) ) {
      push_expr( phase, list_data( &i ), false );
      list_next( &i );
      param = param->next;
   }
   // Default arguments.
   while ( param ) {
      push_expr( phase, param->default_value, false );
      param = param->next;
   }
   c_add_opc( phase, ded->opcode );
   if ( call->func->return_type ) {
      operand->pushed = true;
   }
}

void visit_user_call( struct codegen* phase, struct operand* operand,
   struct call* call ) {
   struct func_user* impl = call->func->impl;
   if ( impl->nested ) {
      visit_nested_userfunc_call( phase, operand, call );
   }
   else {
      write_call_args( phase, operand, call );
      if ( call->func->return_type ) {
         c_add_opc( phase, PCD_CALL );
         c_add_arg( phase, impl->index );
         operand->pushed = true;
      }
      else {
         c_add_opc( phase, PCD_CALLDISCARD );
         c_add_arg( phase, impl->index );
      }
   }
}

void write_call_args( struct codegen* phase, struct operand* operand,
   struct call* call ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( ! list_end( &i ) ) {
      push_expr( phase, list_data( &i ), false );
      list_next( &i );
      param = param->next;
   }
   // Default arguments.
   while ( param ) {
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, 0 );
      param = param->next;
   }
   // Number of real arguments passed, for a function with default
   // parameters.
   if ( call->func->min_param != call->func->max_param ) {
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, list_size( &call->args ) );
   }
}

void visit_nested_userfunc_call( struct codegen* phase,
   struct operand* operand, struct call* call ) {
   // Push ID of entry to identify return address. 
   c_add_opc( phase, PCD_PUSHNUMBER );
   c_add_arg( phase, call->nested_call->id );
   write_call_args( phase, operand, call );
   call->nested_call->enter_pos = c_tell( phase );
   c_add_opc( phase, PCD_GOTO );
   c_add_arg( phase, 0 );
   call->nested_call->leave_pos = c_tell( phase );
   if ( call->func->return_type ) {
      operand->pushed = true;
   }
}

void visit_format_call( struct codegen* phase, struct operand* operand,
   struct call* call ) {
   c_add_opc( phase, PCD_BEGINPRINT );
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
         usage->obj_pos = c_tell( phase );
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, 0 );
         c_add_opc( phase, PCD_GOTO );
         c_add_arg( phase, 0 );
         if ( ! phase->block_visit->format_block_usage ) {
            phase->block_visit->format_block_usage = usage;
         }
      }
      else {
         int block_pos = c_tell( phase );
         visit_block( phase, usage->block );
         usage = phase->block_visit->format_block_usage;
         if ( usage ) {
            // Update block jumps.
            int count = 0;
            while ( usage->next ) {
               c_seek( phase, usage->obj_pos );
               c_add_opc( phase, PCD_PUSHNUMBER );
               c_add_arg( phase, count );
               c_add_opc( phase, PCD_GOTO );
               c_add_arg( phase, block_pos );
               usage->obj_pos = c_tell( phase );
               usage = usage->next;
               ++count;
            }
            // Publish return jumps. A sorted-case-goto can be used here, but a
            // case-goto will suffice for now.
            c_seek_end( phase );
            usage = phase->block_visit->format_block_usage;
            count = 0;
            while ( usage->next ) {
               c_add_opc( phase, PCD_CASEGOTO );
               c_add_arg( phase, count );
               c_add_arg( phase, usage->obj_pos );
               usage = usage->next;
               ++count;
            }
            c_add_opc( phase, PCD_DROP );
            phase->block_visit->format_block_usage = NULL;
         }
      }
      list_next( &i );
   }
   // Format-list:
   else {
      visit_format_item( phase, list_data( &i ) );
      list_next( &i );
   }
   // Other arguments.
   if ( call->func->max_param > 1 ) {
      c_add_opc( phase, PCD_MOREHUDMESSAGE );
      int param = 1;
      while ( ! list_end( &i ) ) {
         if ( param == call->func->min_param ) {
            c_add_opc( phase, PCD_OPTHUDMESSAGE );
         }
         push_expr( phase, list_data( &i ), false );
         ++param;
         list_next( &i );
      }
   }
   struct func_format* format = call->func->impl;
   c_add_opc( phase, format->opcode );
   if ( call->func->return_type ) {
      operand->pushed = true;
   }
} 

void visit_format_item( struct codegen* phase, struct format_item* item ) {
   while ( item ) {
      if ( item->cast == FCAST_ARRAY ) {
         visit_array_format_item( phase, item );
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
         push_expr( phase, item->value, false );
         c_add_opc( phase, casts[ item->cast - 1 ] );
      }
      item = item->next;
   }
}

// Example: Print( a:( array, offset ) );
// Implementation:
//    int *offset_array = array + offset
//    Print( a: offset_array )
// Example: Print( a:( array, offset, length ) );
// Implementation:
//    int *offset_array = array + offset;
//    int last_ch = offset_array[ length - 1 ];
//    array[ length - 1 ] = 0;
//    Print( a: offset_array, c: last_ch );
//    array[ length - 1 ] = last_ch;
void visit_array_format_item( struct codegen* phase, struct format_item* item ) {
   int label_zerolength = 0;
   int label_done = 0;
   struct format_item_array* extra = item->extra;
   struct operand object;
   init_operand( &object );
   object.action = ACTION_PUSH_VAR;
   visit_operand( phase, &object, item->value->root );
   if ( extra && extra->offset ) {
      push_expr( phase, extra->offset, true );
      c_add_opc( phase, PCD_ADD );
   }
   if ( extra && extra->length ) {
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, extra->offset_var );
      push_expr( phase, extra->length, false );
      label_zerolength = c_tell( phase );
      c_add_opc( phase, PCD_CASEGOTO );
      c_add_arg( phase, 0 );
      c_add_arg( phase, 0 );
      c_add_opc( phase, PCD_PUSHSCRIPTVAR );
      c_add_arg( phase, extra->offset_var );
      c_add_opc( phase, PCD_ADD );
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, 1 );
      c_add_opc( phase, PCD_SUBTRACT );
      c_add_opc( phase, PCD_DUP );
      c_add_opc( phase, PCD_DUP );
      push_element( phase, object.storage, object.index );
      c_add_opc( phase, PCD_SWAP );
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, 0 );
      update_element( phase, object.storage, object.index, AOP_NONE );
      c_add_opc( phase, PCD_PUSHSCRIPTVAR );
      c_add_arg( phase, extra->offset_var );
   }
   c_add_opc( phase, PCD_PUSHNUMBER );
   c_add_arg( phase, object.index );
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
   c_add_opc( phase, code );
   if ( extra && extra->length ) {
      c_add_opc( phase, PCD_DUP );
      c_add_opc( phase, PCD_PRINTCHARACTER );
      update_element( phase, object.storage, object.index, AOP_NONE );
      label_done = c_tell( phase );
      c_seek( phase, label_zerolength );
      c_add_opc( phase, PCD_CASEGOTO );
      c_add_arg( phase, 0 );
      c_add_arg( phase, label_done );
      c_seek_end( phase );
   }
}

void visit_internal_call( struct codegen* phase, struct operand* operand,
   struct call* call ) {
   struct func_intern* impl = call->func->impl; 
   if ( impl->id == INTERN_FUNC_ACS_EXECWAIT ) {
      list_iter_t i;
      list_iter_init( &i, &call->args );
      push_expr( phase, list_data( &i ), false );
      c_add_opc( phase, PCD_DUP );
      list_next( &i );
      // Second argument unused.
      list_next( &i );
      // Second argument to Acs_Execute is 0--the current map.
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, 0 );
      while ( ! list_end( &i ) ) {
         push_expr( phase, list_data( &i ), true );
         list_next( &i );
      }
      c_add_opc( phase, g_aspec_code[ list_size( &call->args ) - 1 ] );
      c_add_arg( phase, 80 );
      c_add_opc( phase, PCD_SCRIPTWAIT );
   }
   else if ( impl->id == INTERN_FUNC_STR_LENGTH ) {
      visit_operand( phase, operand, call->operand );
      c_add_opc( phase, PCD_STRLEN );
   }
   else if ( impl->id == INTERN_FUNC_STR_AT ) {
      visit_operand( phase, operand, call->operand );
      push_expr( phase, list_head( &call->args ), true );
      c_add_opc( phase, PCD_CALLFUNC );
      c_add_arg( phase, 2 );
      c_add_arg( phase, 15 );
   }
}

void visit_binary( struct codegen* phase, struct operand* operand,
   struct binary* binary ) {
   // Logical-or and logical-and both perform shortcircuit evaluation.
   if ( binary->op == BOP_LOG_OR ) {
      struct operand lside;
      init_operand( &lside );
      lside.push = true;
      lside.push_temp = true;
      visit_operand( phase, &lside, binary->lside );
      int test = c_tell( phase );
      c_add_opc( phase, PCD_IFNOTGOTO );
      c_add_arg( phase, 0 );
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, 1 );
      int jump = c_tell( phase );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, 0 );
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      rside.push_temp = true;
      int next = c_tell( phase );
      visit_operand( phase, &rside, binary->rside );
      // Optimization: When doing a calculation temporarily, there's no need to
      // convert the second operand to a 0 or 1. Just use the operand directly.
      if ( ! operand->push_temp ) {
         c_add_opc( phase, PCD_NEGATELOGICAL );
         c_add_opc( phase, PCD_NEGATELOGICAL );
      }
      int done = c_tell( phase );
      c_seek( phase, test );
      c_add_opc( phase, PCD_IFNOTGOTO );
      c_add_arg( phase, next );
      c_seek( phase, jump );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, done );
      c_seek_end( phase );
      operand->pushed = true;
   }
   else if ( binary->op == BOP_LOG_AND ) {
      struct operand lside;
      init_operand( &lside );
      lside.push = true;
      lside.push_temp = true;
      visit_operand( phase, &lside, binary->lside );
      int test = c_tell( phase );
      c_add_opc( phase, PCD_IFGOTO );
      c_add_arg( phase, 0 );
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, 0 );
      int jump = c_tell( phase );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, 0 );
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      rside.push_temp = true;
      int next = c_tell( phase );
      visit_operand( phase, &rside, binary->rside );
      if ( ! operand->push_temp ) {
         c_add_opc( phase, PCD_NEGATELOGICAL );
         c_add_opc( phase, PCD_NEGATELOGICAL );
      }
      int done = c_tell( phase );
      c_seek( phase, test );
      c_add_opc( phase, PCD_IFGOTO );
      c_add_arg( phase, next );
      c_seek( phase, jump );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, done );
      c_seek_end( phase );
      operand->pushed = true;
   }
   else {
      struct operand lside;
      init_operand( &lside );
      lside.push = true;
      visit_operand( phase, &lside, binary->lside );
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      visit_operand( phase, &rside, binary->rside );
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
      c_add_opc( phase, code );
      operand->pushed = true;
   }
}

void set_var( struct codegen* phase, struct operand* operand, struct var* var ) {
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

void do_var_name( struct codegen* phase, struct operand* operand,
   struct var* var ) {
   set_var( phase, operand, var );
   // For element-based variables, an index marking the start of the variable
   // data needs to be on the stack.
   if ( operand->method == METHOD_ELEMENT ) {
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, operand->base );
   }
   else {
      if ( operand->action == ACTION_PUSH_VALUE ) {
         push_indexed( phase, operand->storage, operand->index );
         operand->pushed = true;
      }
   }
}

void visit_object( struct codegen* phase, struct operand* operand,
   struct node* node ) {
   if ( node->type == NODE_ACCESS ) {
      visit_access( phase, operand, ( struct access* ) node );
   }
   else if ( node->type == NODE_SUBSCRIPT ) {
      visit_subscript( phase, operand, ( struct subscript* ) node );
   }
   if ( operand->method == METHOD_ELEMENT ) {
      if ( operand->pushed_element ) {
         if ( operand->base ) {
            c_add_opc( phase, PCD_PUSHNUMBER );
            c_add_arg( phase, operand->base );
            c_add_opc( phase, PCD_ADD );
         }
      }
      else {
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, operand->base );
      }
   }
   if ( operand->action == ACTION_PUSH_VALUE &&
      operand->method != METHOD_NONE ) {
      if ( operand->method == METHOD_ELEMENT ) {
         push_element( phase, operand->storage, operand->index );
      }
      else {
         push_indexed( phase, operand->storage, operand->index );
      }
      operand->pushed = true;
   }
}

void visit_subscript( struct codegen* phase, struct operand* operand,
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
      set_var( phase, operand, ( struct var* ) lside );
   }
   else if ( lside->type == NODE_ACCESS ) {
      visit_access( phase, operand, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      visit_subscript( phase, operand, ( struct subscript* ) lside );
   }
   // Dimension:
   struct operand index;
   init_operand( &index );
   index.push = true;
   index.push_temp = true;
   visit_operand( phase, &index, subscript->index->root );
   if ( operand->dim->next ) {
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, operand->dim->element_size );
      c_add_opc( phase, PCD_MULIPLY );
   }
   else if ( ! operand->type->primitive ) {
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, operand->type->size );
      c_add_opc( phase, PCD_MULIPLY );
   }
   if ( operand->pushed_element ) {
      c_add_opc( phase, PCD_ADD );
   }
   else {
      operand->pushed_element = true;
   }
   operand->dim = operand->dim->next;
}

void visit_access( struct codegen* phase, struct operand* operand,
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
      set_var( phase, operand, ( struct var* ) lside );
   }
   else if ( lside->type == NODE_CONSTANT ) {
      visit_constant( phase, operand, ( struct constant* ) lside );
   }
   else if ( lside->type == NODE_ACCESS ) {
      visit_access( phase, operand, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      visit_subscript( phase, operand, ( struct subscript* ) lside );
   }
   else {
      visit_operand( phase, operand, lside );
   }
   // Right side:
   if ( rside && rside->type == NODE_TYPE_MEMBER ) {
      struct type_member* member = ( struct type_member* ) rside;
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, member->offset );
      if ( operand->pushed_element ) {
         c_add_opc( phase, PCD_ADD );
      }
      else {
         operand->pushed_element = true;
      }
      operand->type = member->type;
      operand->dim = member->dim;
   }
}

void visit_assign( struct codegen* phase, struct operand* operand,
   struct assign* assign ) {
   struct operand lside;
   init_operand( &lside );
   lside.action = ACTION_PUSH_VAR;
   visit_operand( phase, &lside, assign->lside );
   if ( lside.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         c_add_opc( phase, PCD_DUP );
      }
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      visit_operand( phase, &rside, assign->rside );
      update_element( phase, lside.storage, lside.index, assign->op );
      if ( operand->push ) {
         push_element( phase, lside.storage, lside.index );
         operand->pushed = true;
      }
   }
   else {
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      visit_operand( phase, &rside, assign->rside );
      if ( assign->op == AOP_NONE && operand->push ) {
         c_add_opc( phase, PCD_DUP );
         operand->pushed = true;
      }
      update_indexed( phase, lside.storage, lside.index, assign->op );
      if ( assign->op != AOP_NONE && operand->push ) {
         push_indexed( phase, lside.storage, lside.index );
         operand->pushed = true;
      }
   }
}

void visit_conditional( struct codegen* codegen, struct operand* operand,
   struct conditional* cond ) {
   if ( cond->folded ) {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, cond->value );
      operand->pushed = true;
   }
   else {
      write_conditional( codegen, operand, cond );
   }
}

void write_conditional( struct codegen* codegen, struct operand* operand,
   struct conditional* cond ) {
   struct operand value;
   init_operand( &value );
   value.push = true;
   visit_operand( codegen, &value, cond->left );
   int left_done = 0;
   int middle_done = 0;
   if ( cond->middle ) {
      left_done = c_tell( codegen );
      c_add_opc( codegen, PCD_IFNOTGOTO );
      c_add_arg( codegen, 0 );
      init_operand( &value );
      value.push = true;
      visit_operand( codegen, &value, cond->middle );
      middle_done = c_tell( codegen );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, 0 );
   }
   else {
      c_add_opc( codegen, PCD_DUP );
      left_done = c_tell( codegen );
      c_add_opc( codegen, PCD_IFGOTO );
      c_add_arg( codegen, 0 );
      c_add_opc( codegen, PCD_DROP );
   }
   init_operand( &value );
   value.push = true;
   visit_operand( codegen, &value, cond->right );
   int done = c_tell( codegen );
   if ( cond->middle ) {
      c_seek( codegen, middle_done );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, done );
      int right = c_tell( codegen );
      c_seek( codegen, left_done );
      c_add_opc( codegen, PCD_IFNOTGOTO );
      c_add_arg( codegen, right );
      c_seek_end( codegen );
   }
   else {
      c_seek( codegen, left_done );
      c_add_opc( codegen, PCD_IFGOTO );
      c_add_arg( codegen, done );
      c_seek_end( codegen );
   }
   operand->pushed = value.pushed;
}

void push_indexed( struct codegen* phase, int storage, int index ) {
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
   c_add_opc( phase, code );
   c_add_arg( phase, index );
}

void push_element( struct codegen* phase, int storage, int index ) {
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
   c_add_opc( phase, code );
   c_add_arg( phase, index );
}

void update_indexed( struct codegen* phase, int storage, int index, int op ) {
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
   c_add_opc( phase, code[ op * 4 + pos ] );
   c_add_arg( phase, index );
}

void update_element( struct codegen* phase, int storage, int index, int op ) {
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
   c_add_opc( phase, code[ op * 3 + pos ] );
   c_add_arg( phase, index );
}

void inc_indexed( struct codegen* phase, int storage, int index ) {
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
   c_add_opc( phase, code );
   c_add_arg( phase, index );
}

void dec_indexed( struct codegen* phase, int storage, int index ) {
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
   c_add_opc( phase, code );
   c_add_arg( phase, index );
}

void inc_element( struct codegen* phase, int storage, int index ) {
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
   c_add_opc( phase, code );
   c_add_arg( phase, index );
}

void dec_element( struct codegen* phase, int storage, int index ) {
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
   c_add_opc( phase, code );
   c_add_arg( phase, index );
}

void visit_if( struct codegen* phase, struct if_stmt* stmt ) {
   push_expr( phase, stmt->cond, true );
   int cond = c_tell( phase );
   c_add_opc( phase, PCD_IFNOTGOTO );
   c_add_arg( phase, 0 );
   visit_node( phase, stmt->body );
   int bail = c_tell( phase );
   if ( stmt->else_body ) {
      // Exit from if block:
      int bail_if_block = c_tell( phase );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, 0 ); 
      bail = c_tell( phase );
      visit_node( phase, stmt->else_body );
      int stmt_end = c_tell( phase );
      c_seek( phase, bail_if_block );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, stmt_end );
   }
   c_seek( phase, cond );
   c_add_opc( phase, PCD_IFNOTGOTO );
   c_add_arg( phase, bail );
   c_seek_end( phase );
}

void visit_switch( struct codegen* phase, struct switch_stmt* stmt ) {
   push_expr( phase, stmt->cond, true );
   int num_cases = 0;
   struct case_label* label = stmt->case_head;
   while ( label ) {
      ++num_cases;
      label = label->next;
   }
   int test = c_tell( phase );
   if ( num_cases ) {
      c_add_opc( phase, PCD_CASEGOTOSORTED );
      c_add_arg( phase, 0 );
      for ( int i = 0; i < num_cases; ++i ) {
         c_add_arg( phase, 0 );
         c_add_arg( phase, 0 );
      }
   }
   c_add_opc( phase, PCD_DROP );
   int fail = c_tell( phase );
   c_add_opc( phase, PCD_GOTO );
   c_add_arg( phase, 0 );
   visit_node( phase, stmt->body );
   int done = c_tell( phase );
   if ( num_cases ) {
      c_seek( phase, test );
      c_add_opc( phase, PCD_CASEGOTOSORTED );
      c_add_arg( phase, num_cases );
      label = stmt->case_head;
      while ( label ) {
         c_add_arg( phase, label->number->value );
         c_add_arg( phase, label->offset );
         label = label->next;
      }
   }
   c_seek( phase, fail );
   c_add_opc( phase, PCD_GOTO );
   int fail_pos = done;
   if ( stmt->case_default ) {
      fail_pos = stmt->case_default->offset;
   }
   c_add_arg( phase, fail_pos );
   add_jumps( phase, stmt->jump_break, done );
}

void visit_case( struct codegen* phase, struct case_label* label ) {
   label->offset = c_tell( phase );
}

void visit_while( struct codegen* phase, struct while_stmt* stmt ) {
   int test = 0;
   int done = 0;
   if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
      int jump = 0;
      if ( ! stmt->cond->folded || (
         ( stmt->type == WHILE_WHILE && ! stmt->cond->value ) ||
         ( stmt->type == WHILE_UNTIL && stmt->cond->value ) ) ) {
         jump = c_tell( phase );
         c_add_opc( phase, PCD_GOTO );
         c_add_arg( phase, 0 );
      }
      int body = c_tell( phase );
      visit_node( phase, stmt->body );
      if ( stmt->cond->folded ) {
         if ( ( stmt->type == WHILE_WHILE && stmt->cond->value ) ||
            ( stmt->type == WHILE_UNTIL && ! stmt->cond->value ) ) {
            c_add_opc( phase, PCD_GOTO );
            c_add_arg( phase, body );
            done = c_tell( phase );
            test = body;
         }
         else {
            done = c_tell( phase );
            test = done;
            c_seek( phase, jump );
            c_add_opc( phase, PCD_GOTO );
            c_add_arg( phase, done );
         }
      }
      else {
         test = c_tell( phase );
         push_expr( phase, stmt->cond, true );
         int code = PCD_IFGOTO;
         if ( stmt->type == WHILE_UNTIL ) {
            code = PCD_IFNOTGOTO;
         }
         c_add_opc( phase, code );
         c_add_arg( phase, body );
         done = c_tell( phase );
         c_seek( phase, jump );
         c_add_opc( phase, PCD_GOTO );
         c_add_arg( phase, test );
      }
   }
   // do-while / do-until.
   else {
      int body = c_tell( phase );
      visit_node( phase, stmt->body );
      // Condition:
      if ( stmt->cond->folded ) {
         // Optimization: Only loop when the condition is satisfied.
         if ( ( stmt->type == WHILE_DO_WHILE && stmt->cond->value ) ||
            ( stmt->type == WHILE_DO_UNTIL && ! stmt->cond->value ) ) {
            c_add_opc( phase, PCD_GOTO );
            c_add_arg( phase, body );
            done = c_tell( phase );
            test = body;
         }
         else {
            done = c_tell( phase );
            test = done;
         }
      }
      else {
         test = c_tell( phase );
         push_expr( phase, stmt->cond, true );
         int code = PCD_IFGOTO;
         if ( stmt->type == WHILE_DO_UNTIL ) {
            code = PCD_IFNOTGOTO;
         }
         c_add_opc( phase, code );
         c_add_arg( phase, body );
         done = c_tell( phase );
      }
   }
   add_jumps( phase, stmt->jump_continue, test );
   add_jumps( phase, stmt->jump_break, done );
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
void visit_for( struct codegen* phase, struct for_stmt* stmt ) {
   // Initialization.
   list_iter_t i;
   list_iter_init( &i, &stmt->init );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      switch ( node->type ) {
      case NODE_EXPR:
         visit_expr( phase, ( struct expr* ) node );
         break;
      case NODE_VAR:
         visit_var( phase, ( struct var* ) node );
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
         jump = c_tell( phase );
         c_add_opc( phase, PCD_GOTO );
         c_add_arg( phase, 0 );
      }
   }
   // Body.
   int body = c_tell( phase );
   visit_node( phase, stmt->body );
   // Post expressions.
   int post = c_tell( phase );
   list_iter_init( &i, &stmt->post );
   while ( ! list_end( &i ) ) {
      visit_expr( phase, list_data( &i ) );
      list_next( &i );
   }
   // Condition.
   int test = 0;
   if ( stmt->cond ) {
      if ( stmt->cond->folded ) {
         if ( stmt->cond->value ) {
            c_add_opc( phase, PCD_GOTO );
            c_add_arg( phase, body );
         }
      }
      else {
         test = c_tell( phase );
         push_expr( phase, stmt->cond, true );
         c_add_opc( phase, PCD_IFGOTO );
         c_add_arg( phase, body );
      }
   }
   else {
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, body );
   }
   // Jump to condition.
   int done = c_tell( phase );
   if ( stmt->cond ) {
      if ( stmt->cond->folded ) {
         if ( ! stmt->cond->value ) {
            c_seek( phase, jump );
            c_add_opc( phase, PCD_GOTO );
            c_add_arg( phase, done );
         }
      }
      else {
         c_seek( phase, jump );
         c_add_opc( phase, PCD_GOTO );
         c_add_arg( phase, test );
      }
   }
   add_jumps( phase, stmt->jump_continue, post );
   add_jumps( phase, stmt->jump_break, done );
}

void visit_jump( struct codegen* phase, struct jump* jump ) {
   jump->obj_pos = c_tell( phase );
   c_add_opc( phase, PCD_GOTO );
   c_add_arg( phase, 0 );
}

void add_jumps( struct codegen* phase, struct jump* jump, int pos ) {
   while ( jump ) {
      c_seek( phase, jump->obj_pos );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, pos );
      jump = jump->next;
   }
   c_seek_end( phase );
}

void visit_return( struct codegen* phase, struct return_stmt* stmt ) {
   if ( phase->func_visit->nested_func ) {
      if ( stmt->return_value ) {
         push_expr( phase, stmt->return_value->expr, false );
      }
      stmt->obj_pos = c_tell( phase );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, 0 );
   }
   else {
      if ( stmt->return_value ) {
         push_expr( phase, stmt->return_value->expr, false );
         c_add_opc( phase, PCD_RETURNVAL );
      }
      else {
         c_add_opc( phase, PCD_RETURNVOID );
      }
   }
}

void visit_paltrans( struct codegen* phase, struct paltrans* trans ) {
   push_expr( phase, trans->number, true );
   c_add_opc( phase, PCD_STARTTRANSLATION );
   struct palrange* range = trans->ranges;
   while ( range ) {
      push_expr( phase, range->begin, true );
      push_expr( phase, range->end, true );
      if ( range->rgb ) {
         push_expr( phase, range->value.rgb.red1, true );
         push_expr( phase, range->value.rgb.green1, true );
         push_expr( phase, range->value.rgb.blue1, true );
         push_expr( phase, range->value.rgb.red2, true );
         push_expr( phase, range->value.rgb.green2, true );
         push_expr( phase, range->value.rgb.blue2, true );
         c_add_opc( phase, PCD_TRANSLATIONRANGE2 );
      }
      else {
         push_expr( phase, range->value.ent.begin, true );
         push_expr( phase, range->value.ent.end, true );
         c_add_opc( phase, PCD_TRANSLATIONRANGE1 );
      }
      range = range->next;
   }
   c_add_opc( phase, PCD_ENDTRANSLATION );
}

void add_default_params( struct codegen* phase, struct func* func,
   int count_param, bool reset_count_param ) {
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
      c_add_opc( phase, PCD_PUSHSCRIPTVAR );
      c_add_arg( phase, count_param );
      int jump = c_tell( phase );
      c_add_opc( phase, PCD_CASEGOTOSORTED );
      c_add_arg( phase, 0 );
      param = start;
      int i = 0;
      while ( param && i < num_cases ) {
         c_add_arg( phase, 0 );
         c_add_arg( phase, 0 );
         param = param->next;
         ++i;
      }
      c_add_opc( phase, PCD_DROP );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, 0 );
      param = start;
      while ( param ) {
         param->obj_pos = c_tell( phase );
         if ( ! param->default_value->folded || param->default_value->value ) {
            push_expr( phase, param->default_value, false );
            update_indexed( phase, STORAGE_LOCAL, param->index, AOP_NONE );
         }
         param = param->next;
      }
      int done = c_tell( phase );
      // Add case positions.
      c_seek( phase, jump );
      c_add_opc( phase, PCD_CASEGOTOSORTED );
      c_add_arg( phase, num_cases );
      num = func->min_param;
      param = start;
      while ( param && num_cases ) {
         c_add_arg( phase, num );
         c_add_arg( phase, param->obj_pos );
         param = param->next;
         --num_cases;
         ++num;
      }
      c_add_opc( phase, PCD_DROP );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, done );
      c_seek( phase, done );
   }
   if ( start ) {
      // Reset the parameter-count parameter.
      if ( reset_count_param ) {
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, 0 );
         c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
         c_add_arg( phase, count_param );
      }
   }
}

void init_nestedfunc_writing( struct nestedfunc_writing* writing,
   struct func* nested_funcs, struct call* nested_calls, int temps_start ) {
   writing->nested_funcs = nested_funcs;
   writing->nested_calls = nested_calls;
   writing->temps_start = temps_start;
   writing->temps_size = 0;
}

void write_nested_funcs( struct codegen* phase,
   struct nestedfunc_writing* writing ) {
   phase->func_visit->nested_func = true;
   struct func* func = writing->nested_funcs;
   while ( func ) {
      write_one_nestedfunc( phase, writing, func );
      struct func_user* impl = func->impl;
      func = impl->next_nested;
   }
   // Insert address of function into each call.
   func = writing->nested_funcs;
   while ( func ) {
      struct func_user* impl = func->impl;
      patch_nestedfunc_addresses( phase, func );
      func = impl->next_nested;
   }
   c_seek_end( phase );
   phase->func_visit->nested_func = false;
}

void write_one_nestedfunc( struct codegen* phase,
   struct nestedfunc_writing* writing, struct func* func ) {
   struct func_user* impl = func->impl;

   // Count number of times function is called.
   int num_entries = 0;
   struct call* call = impl->nested_calls;
   while ( call ) {
      ++num_entries;
      struct nested_call* nested = call->nested_call;
      call = nested->next;
   }
   // Don't write the function when it isn't used.
   if ( ! num_entries ) {
      return;
   }

   // Prologue:
   // -----------------------------------------------------------------------
   impl->obj_pos = c_tell( phase );

   // Assign arguments to temporary space.
   int temps_index = writing->temps_start;
   if ( func->min_param != func->max_param ) {
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, temps_index );
      ++temps_index;
   }
   struct param* param = func->params;
   while ( param ) {
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, temps_index );
      ++temps_index;
      param = param->next;
   }

   // The function uses variables of the script or function as its workspace.
   // Save the values of these variables onto the stack. They will be restored
   // at epilogue.
   int i = 0;
   while ( i < impl->size ) {
      c_add_opc( phase, PCD_PUSHSCRIPTVAR );
      c_add_arg( phase, impl->index_offset + i );
      ++i;
   }

   // Activate parameters.
   int temps_size = temps_index - writing->temps_start;
   param = func->params;
   while ( param ) {
      --temps_index;
      c_add_opc( phase, PCD_PUSHSCRIPTVAR );
      c_add_arg( phase, temps_index );
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, param->index );
      param = param->next;
   }
   if ( func->min_param != func->max_param ) {
      --temps_index;
      add_default_params( phase, func, temps_index, false );
   }

   // Body:
   // -----------------------------------------------------------------------
   int body_pos = c_tell( phase );
   visit_block( phase, impl->body );
   if ( func->return_type ) {
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, 0 );
   }

   // Epilogue:
   // -----------------------------------------------------------------------
   int epilogue_pos = c_tell( phase );

   // Temporarily save the return-value.
   int temps_return = 0;
   if ( impl->size > 2 && func->return_type ) {
      // Use the last temporary variable.
      temps_return = writing->temps_start;
      temps_size += ( ! temps_size ? 1 : 0 );
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, temps_return );
   }

   // Restore previous values of script variables.
   i = 0;
   while ( i < impl->size ) {
      if ( impl->size <= 2 && func->return_type ) {
         c_add_opc( phase, PCD_SWAP );
      }
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, impl->index_offset + impl->size - i - 1 );
      ++i;
   }

   // Return-value.
   if ( func->return_type ) {
      if ( impl->size > 2 ) {
         c_add_opc( phase, PCD_PUSHSCRIPTVAR );
         c_add_arg( phase, temps_return );
      }
      c_add_opc( phase, PCD_SWAP );
   }

   // Output return table.
   impl->return_pos = c_tell( phase );
   c_add_opc( phase, PCD_CASEGOTOSORTED );
   c_add_arg( phase, num_entries );
   call = impl->nested_calls;
   while ( call ) {
      c_add_arg( phase, 0 );
      c_add_arg( phase, 0 );
      call = call->nested_call->next;
   }

   // Patch address of return-statements.
   struct return_stmt* stmt = impl->returns;
   while ( stmt ) {
      c_seek( phase, stmt->obj_pos );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, epilogue_pos );
      stmt = stmt->next;
   }
   c_seek_end( phase );

   if ( temps_size > writing->temps_size ) {
      writing->temps_size = temps_size;
   }
}

void patch_nestedfunc_addresses( struct codegen* phase, struct func* func ) {
   struct func_user* impl = func->impl; 
   // Correct calls to this function.
   int num_entries = 0;
   struct call* call = impl->nested_calls;
   while ( call ) {
      c_seek( phase, call->nested_call->enter_pos );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, impl->obj_pos );
      call = call->nested_call->next;
      ++num_entries;
   }
   // Correct return-addresses in return-table.
   c_seek( phase, impl->return_pos );
   c_add_opc( phase, PCD_CASEGOTOSORTED );
   c_add_arg( phase, num_entries );
   call = impl->nested_calls;
   while ( call ) {
      c_add_arg( phase, call->nested_call->id );
      c_add_arg( phase, call->nested_call->leave_pos );
      call = call->nested_call->next;
   }
}