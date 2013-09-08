#include "b_main.h"
#include "mem.h"

#define MAIN_SCRIPT 0
#define SCRIPT_TYPE_OPEN 1
#define SPACE_ARRAY 0
#define CS_POINTER_VAR 1
#define CS_SIZE 5000 

typedef struct {
   node_t* node;
   dimension_t* dim;
   func_t* func;
   type_t* type;
   enum {
      k_space_none,
      k_space_var,
      k_space_array,
   } space;
   bool is_static;
   int storage;
   int index;
   enum {
      k_offset_none,
      k_offset_value,
      k_offset_stacked,
   } offset;
   int offset_value;
   enum {
      k_seq_none,
      k_seq_subscript,
      k_seq_member,
   } seq;
   int num_seq;
   int var_elements;
   bool head_dim;
   bool needed;
   bool stack;
   bool stacked;
   bool lvalue;
   bool var;
} value_t;

static void write_names( back_t* );
static void write_main( back_t* );
static void write_static_init( back_t* );
static void write_chunks( back_t* );
static void do_block( back_t*, list_t* );
static void do_node( back_t*, node_t* );
static void do_expr( back_t*, expr_t*, bool );
static void do_operand( back_t*, value_t*, node_t* );
static void do_literal( back_t*, literal_t* );
static void do_name( back_t*, name_t* );
static void do_unary( back_t*, value_t*, unary_t* );
static void do_pre_inc( back_t*, value_t*, unary_t* );
static void do_post_inc( back_t*, value_t*, unary_t* );
static void do_binary( back_t*, value_t*, binary_t* );
static void do_ternary( back_t*, ternary_t* );
static void access_var( back_t*, int, int );
static void access_array( back_t*, int, int );
static void assign_var( back_t*, int, int, int );
static void assign_array( back_t*, int, int, int );
static void inc_var( back_t*, int, int, bool );
static void inc_array( back_t*, int, int, bool );
static void do_jump( back_t*, jump_t* );
static void do_script_jump( back_t*, script_jump_t* );
static void do_return( back_t*, return_t* );
static void do_var_def( back_t*, var_t* );
static void do_if( back_t*, if_t* );
static void do_cond( back_t*, node_t* );
static void do_switch( back_t*, switch_t* );
static void do_case( back_t*, case_t* );
static void do_while( back_t*, while_t* );
static void do_for( back_t*, for_t* );
static void write_jump( back_t*, jump_t*, b_pos_t* );
static void do_format_item( back_t*, format_item_t* );
static bool is_args_constant( list_t* );
static void do_call( back_t*, value_t*, call_t* );
static void do_aspec( back_t*, value_t*, list_t* );
static void do_call_format( back_t*, value_t*, call_t* );
static void do_call_args( back_t*, func_t*, list_t* );
static void do_call_args_direct( back_t*, func_t*, list_t* );
static void do_assign( back_t*, value_t*, assign_t* );
static void do_default_params( back_t*, ufunc_t* );
static bool is_default_param_zero( param_t* );
static void init_value( value_t* );
static void do_member_access( back_t*, value_t*, member_access_t* );
static void do_subscript( back_t*, value_t*, subscript_t* );
static void call_acs_execwait( back_t*, value_t*, call_t* );
static int get_aspec_opcode( int );

void b_write_script( back_t* back, script_t* script ) {
   do_block( back, &script->body );
   b_pcode( back, pc_terminate );
   b_flush( back );
}

void b_write_function( back_t* back, ufunc_t* ufunc ) {
   do_block( back, &ufunc->body );
   b_pcode( back, pc_return_void );
   b_flush( back );
}

void do_block( back_t* back, list_t* stmt_list ) {
   list_iterator_t i = list_iterate( stmt_list );
   while ( ! list_end( &i ) ) {
      do_node( back, list_idata( &i ) );
      list_next( &i );
   }
}

void do_node( back_t* back, node_t* node ) {
   switch ( node->type ) {
   case k_node_expr:
      do_expr( back, ( expr_t* ) node, false );
      break;
   case k_node_var:
      do_var_def( back, ( var_t* ) node );
      break;
   case k_node_jump:
      do_jump( back, ( jump_t* ) node );
      break;
   case k_node_script_jump:
      do_script_jump( back, ( script_jump_t* ) node );
      break;
   case k_node_return:
      do_return( back, ( return_t* ) node );
      break;
   case k_node_if:
      do_if( back, ( if_t* ) node );
      break;
   case k_node_switch:
      do_switch( back, ( switch_t* ) node );
      break;
   case k_node_case:
      do_case( back, ( case_t* ) node );
      break;
   case k_node_while:
      do_while( back, ( while_t* ) node );
      break;
   case k_node_for:
      do_for( back, ( for_t* ) node );
      break;
   case k_node_format_item:
      do_format_item( back, ( format_item_t* ) node );
      break;
   default:
      break;
   }
}

void do_expr( back_t* back, expr_t* expr, bool keep_stacked ) {
   value_t value;
   init_value( &value );
   value.needed = keep_stacked;
   value.stack = true;
   do_operand( back, &value, expr->root );
   if ( ! keep_stacked && value.stacked ) {
      b_pcode( back, pc_drop );
   }
}

void init_value( value_t* value ) {
   value->node = NULL;
   value->dim = NULL;
   value->func = NULL;
   value->space = k_space_none;
   value->storage = k_storage_local;
   value->index = 0;
   value->offset = k_offset_none;
   value->offset_value = 0;
   value->needed = false;
   value->stack = false;
   value->stacked = false;
   value->head_dim = false;
   value->seq = k_seq_none;
   value->num_seq = 0;
   value->is_static = false;
   value->lvalue = false;
   value->var_elements = 0;
   value->var = false;
}

void do_operand( back_t* back, value_t* value, node_t* node ) {
   if ( node->type == k_node_literal ) {
      literal_t* literal = ( literal_t* ) node;
      b_pcode( back, pc_push_number, literal->value );
      value->stacked = true;
      // Strings in libraries need to be tagged.
      // if ( back->is_lib && literal->type->known == k_type_str ) {
      //    b_pcode( back, pc_tag_string );
      // }
   }
   else if ( node->type == k_node_name ) {
      name_t* name = ( name_t* ) node;
      do_operand( back, value, name->object );
   }
   else if ( node->type == k_node_var ) {
      var_t* var = ( var_t* ) node;
      value->storage = var->storage;
      value->index = var->index;
      value->space = k_space_var;
      // An array is used to store a custom collection of variables.  
      if ( var->type->known == k_type_custom ) {
         value->space = k_space_array;
      }
   }
   else if ( node->type == k_node_func ) {
      value->func = ( func_t* ) node;
   }
   else if ( node->type == k_node_constant ) {
      constant_t* constant = ( constant_t* ) node;
      b_pcode( back, pc_push_number, constant->value );
      value->stacked = true;
   }
   else if ( node->type == k_node_subscript ) {
      do_subscript( back, value, ( subscript_t* ) node );
   }
   else if ( node->type == k_node_member_access ) {
      do_member_access( back, value, ( member_access_t* ) node );
   }
   else if ( node->type == k_node_call ) {
      call_t* call = ( call_t* ) node;
      do_operand( back, value, call->func );
      do_call( back, value, call );
   }
   else if ( node->type == k_node_unary ) {
      do_unary( back, value, ( unary_t* ) node );
   }
   else if ( node->type == k_node_binary ) {
      do_binary( back, value, ( binary_t* ) node );
   }
   else if ( node->type == k_node_assign ) {
      do_assign( back, value, ( assign_t* ) node );
   }
   if ( value->var && ! value->var_elements && value->stack ) {
printf( "a\n" );
      b_pcode( back, pc_push_map_array, SPACE_ARRAY );
      value->stack = false;
   }
}

void do_subscript( back_t* back, value_t* object, subscript_t* sub ) {
   object->var_elements += 1;
   bool finish = false;
   if ( object->seq != k_seq_subscript ) {
      object->seq = k_seq_subscript;
      finish = true;
   }
   do_operand( back, object, sub->operand );
   do_expr( back, sub->index, true );
   if ( object->dim->element_size > 1 ) {
      b_pcode( back, pc_push_number, object->dim->element_size );
      b_pcode( back, pc_mul );
   }
   if ( object->head_dim ) {
      object->head_dim = false;
   }
   b_pcode( back, pc_add );
   object->dim = object->dim->next;
   if ( finish ) {
      if ( object->offset == k_offset_value ) {
         b_pcode( back, pc_push_number, object->offset_value );
         b_pcode( back, pc_add );
         object->offset_value = 0;
      }
      else if ( object->offset == k_offset_stacked ) {
         b_pcode( back, pc_add );
      }
      object->offset = k_offset_stacked;
      ++object->num_seq;
   }
   object->var_elements -= 1;
}

void do_member_access( back_t* back, value_t* object, member_access_t* access ) {
   bool finish = false;
   if ( object->seq != k_seq_member ) {
      object->seq = k_seq_member;
      finish = true;
   }
   do_operand( back, object, access->object );
   type_member_t* member = access->member;
   object->offset_value += member->offset;
   if ( finish ) {
      if ( object->num_seq ) {
         // In the middle of a sequence, only add to the offset if the member
         // offset is nonzero. Otherwise, it's a useless calculation.
         if ( object->offset_value ) {
            b_pcode( back, pc_push_number, object->offset_value );
            b_pcode( back, pc_add );
         }
      }
      else {
         b_pcode( back, pc_push_number, object->offset_value );
      }
      object->offset = k_offset_stacked;
      object->offset_value = 0;
/*
      if ( member->object->type == k_node_array ) {
         array_t* array = ( array_t* ) member->object;
         object->type = array->type;
         object->dim = array->dim;
         object->head_dim = true;
      }*/
      ++object->num_seq;
   }
}

void do_unary( back_t* back, value_t* value, unary_t* unary ) {
   if ( unary->is_folded ) {
      b_pcode( back, pc_push_number, unary->value );
   }
   else if ( unary->op == k_uop_pre_inc || unary->op == k_uop_pre_dec ) {
      do_pre_inc( back, value, unary );
   }
   else if ( unary->op == k_uop_post_inc || unary->op == k_uop_post_dec ) {
      do_post_inc( back, value, unary );
   }
   else if ( unary->op == k_uop_log_not ) {
      do_operand( back, value, unary->operand );
      b_pcode( back, pc_negate_logical );
      value->stacked = true;
   }
   else if ( unary->op == k_uop_bit_not ) {
      do_operand( back, value, unary->operand );
      b_pcode( back, pc_negate_binary );
      value->stacked = true;
   }
   else if ( unary->op == k_uop_minus ) {
      do_operand( back, value, unary->operand );
      b_pcode( back, pc_unary_minus );
      value->stacked = true;
   }
   else if ( unary->op == k_uop_addr ) {
      do_operand( back, value, unary->operand );
      b_pcode( back, pc_push_number, value->index );
      value->stacked = true;
   }
   else if ( unary->op == k_uop_deref ) {
      do_operand( back, value, unary->operand );
      b_pcode( back, pc_push_map_array, SPACE_ARRAY );
      value->stacked = true;
   }
}

void do_pre_inc( back_t* back, value_t* value, unary_t* unary ) {
   bool dec = ( unary->op == k_uop_pre_dec );
   value_t operand;
   init_value( &operand );
   do_operand( back, &operand, unary->operand );
   if ( operand.space == k_space_array ) {
      if ( value->needed ) {
         b_pcode( back, pc_dup );
      }
      inc_array( back, operand.storage, operand.index, dec );
      if ( value->needed ) {
         access_array( back, operand.storage, operand.index );
         value->stacked = true;
      }
   }
   else if ( operand.space == k_space_var ) {
      inc_var( back, operand.storage, operand.index, dec );
      if ( value->needed ) {
         access_var( back, operand.storage, operand.index );
         value->stacked = true;
      }
   }
}

void do_post_inc( back_t* back, value_t* value, unary_t* unary ) {
   bool dec = ( unary->op == k_uop_post_dec );
   value_t operand;
   init_value( &operand );
   do_operand( back, &operand, unary->operand );
   if ( operand.space == k_space_array ) {
      if ( value->needed ) {
         b_pcode( back, pc_dup );
         access_array( back, operand.storage, operand.index );
         b_pcode( back, pc_swap );
         value->stacked = true;
      }
      inc_array( back, operand.storage, operand.index, dec );
   }
   else if ( operand.space == k_space_var ) {
      if ( value->needed ) {
         access_var( back, operand.storage, operand.index );
         value->stacked = true;
      }
      inc_var( back, operand.storage, operand.index, dec );
   }
}

void do_call( back_t* back, value_t* value, call_t* call ) {
   func_t* func = value->func;
   list_iterator_t i = list_iterate( &call->args );
   if ( func->type == k_func_format ) {
      do_call_format( back, value, call );
   }
   else if ( func->type == k_func_aspec ) {
      do_aspec( back, value, &call->args );
   }
   else if ( func->type == k_func_ext ) {
      do_call_args( back, func, &call->args );
      ext_func_t* ext = ( ext_func_t* ) func;
      b_pcode( back, pc_call_func, list_size( &call->args ), ext->id );
      if ( func->return_type ) {
         value->stacked = true;
      }
   }
   else if ( func->type == k_func_ded ) {
      ded_func_t* ded = ( ded_func_t* ) func;
      if ( ded->opcode_constant && is_args_constant( &call->args ) ) {
         b_pcode( back, pp_opcode, ded->opcode_constant );
         do_call_args_direct( back, func, &call->args );
      }
      else {
         do_call_args( back, func, &call->args );
         b_pcode( back, pp_opcode, ded->opcode );
      }
      if ( func->return_type ) {
         value->stacked = true;
      }
   }
   else if ( func->type == k_func_user ) {
      do_call_args( back, func, &call->args );
      ufunc_t* ufunc = ( ufunc_t* ) func;
      if ( ufunc->func.return_type ) {
         b_pcode( back, pc_call, ufunc->index );
         value->stacked = true;
      }
      else {
         b_pcode( back, pc_call_discard, ufunc->index );
      }
   }
   else if ( func->type == k_func_internal ) {
      ifunc_t* ifunc = ( ifunc_t* ) func;
      switch ( ifunc->id ) {
      case k_ifunc_str_subscript:
         if ( ! value->stacked ) {
            access_var( back, value->storage, value->index );
         }
         do_call_args( back, func, &call->args );
         b_pcode( back, pc_call_func, 2, 15 );
         value->stacked = true;
         break;
      case k_ifunc_str_length:
         access_var( back, value->storage, value->index );
         b_pcode( back, pc_str_len );
         break;
      case k_ifunc_acs_execwait:
         call_acs_execwait( back, value, call );
         break;
      default:
         break;
      }
   }
}

void do_aspec( back_t* back, value_t* value, list_t* args ) {
   int num = 0;
   list_iterator_t i = list_iterate( args );
   while ( ! list_end( &i ) ) {
      do_expr( back, list_idata( &i ), true );
      ++num;
      list_next( &i );
   }
   aspec_t* aspec = ( aspec_t* ) value->func;
   if ( value->needed ) {
      // Pad with zeros, if needed, to reach 5 arguments.
      while ( num < 5 ) {
         b_pcode( back, pc_push_number, 0 );
         ++num;
      }
      b_pcode( back, pc_lspec5_result, aspec->id );
   }
   else if ( num ) {
      b_pcode( back, get_aspec_opcode( num ), aspec->id );
   }
   else {
      b_pcode( back, pc_push_number, 0 );
      b_pcode( back, pc_lspec1, aspec->id );
   }
}

int get_aspec_opcode( int num_args ) {
   static int code[] = {
      pc_lspec1, pc_lspec2, pc_lspec3, pc_lspec4, pc_lspec5 };
   return code[ num_args - 1 ];
}

void call_acs_execwait( back_t* back, value_t* value, call_t* call ) {
   list_iterator_t args = list_iterate( &call->args );
   expr_t* expr = list_idata( &args );
   do_expr( back, expr, true );
   b_pcode( back, pc_dup );
   list_next( &args );
   while ( ! list_end( &args ) ) {
      do_expr( back, list_idata( &args ), true );
      list_next( &args );
   }
   b_pcode( back, get_aspec_opcode( list_size( &call->args ) ), 80 );
   b_pcode( back, pc_script_wait );
}

void do_call_format( back_t* back, value_t* value, call_t* call ) {
   func_t* func = value->func;
   b_pcode( back, pc_begin_print );
   list_iterator_t i = list_iterate( &call->args );
   node_t* node = list_head( &call->args );
   if ( node->type == k_node_format_item ) {
      while ( ! list_end( &i ) ) {
         node_t* hold = list_idata( &i );
         if ( hold->type != k_node_format_item ) { break; }
         static int casts[] = {
            0, pc_print_binary, pc_print_character, pc_print_number,
            pc_print_fixed, pc_print_bind, pc_print_localized,
            pc_print_name, pc_print_string, pc_print_hex };
         format_item_t* item = ( format_item_t* ) hold;
         // The array cast requires special handling because one can print an
         // array from multiple storages.
         if ( item->cast == k_fcast_array ) {
            value_t arg;
            init_value( &arg );
            do_operand( back, &arg, item->expr->root );
            if ( ! arg.num_seq ) {
               b_pcode( back, pc_push_number, 0 );
            }
            b_pcode( back, pc_push_number, arg.index );
            int code = pc_print_map_char_array;
            switch ( arg.storage ) {
            case k_storage_world:
               code = pc_print_world_char_array;
               break;
            case k_storage_global:
               code = pc_print_global_char_array;
               break;
            default:
               break;
            }
            b_pcode( back, code );
         }
         else {
            do_expr( back, item->expr, true );
            b_pcode( back, casts[ item->cast ] );
         }
         list_next( &i );
      }
   }
   else {
      format_block_t* format = ( format_block_t* ) node;
      do_block( back, &format->body );
      list_next( &i );
   }
   // Other arguments.
   int num = 1;
   if ( func->num_param > 1 ) {
      b_pcode( back, pc_more_hud_message );
      // Output up to the optional arguments.
      while ( ! list_end( &i ) && num < func->min_param ) {
         do_expr( back, list_idata( &i ), true );
         list_next( &i );
         ++num;
      }
      b_pcode( back, pc_opt_hud_message );
   }
   format_func_t* f_func = ( format_func_t* ) func;
   b_pcode( back, f_func->opcode );
}

bool is_args_constant( list_t* args ) {
   list_iterator_t i = list_iterate( args );
   while ( ! list_end( &i ) ) {
      expr_t* expr = list_idata( &i );
      if ( ! expr->is_constant ) { return false; }
      list_next( &i );
   }
   return true;
}

void do_call_args( back_t* back, func_t* func, list_t* args ) {
   int given = 0;
   list_iterator_t i = list_iterate( args );
   while ( ! list_end( &i ) ) {
      do_expr( back, list_idata( &i ), true );
      list_next( &i );
      ++given;
   }
   // Default arguments.
   if ( given < func->num_param ) {
      int left = func->num_param - given;
      while ( left ) {
         b_pcode( back, pc_push_number, 0 );
         --left;
      }
   }
   // Value for the argument-count parameter. 
   if ( func->type == k_func_user && func->min_param != func->num_param ) {
      b_pcode( back, pc_push_number, given );
   }
}

void do_call_args_direct( back_t* back, func_t* func, list_t* args ) {
   int num = 0;
   list_iterator_t i = list_iterate( args );
   while ( ! list_end( &i ) ) {
      expr_t* expr = list_idata( &i );
      b_pcode( back, pp_arg, expr->value );
      ++num;
      list_next( &i );
   }
   while ( num < func->num_param ) {
      b_pcode( back, pp_arg, 0 );
      ++num;
   }
}

void do_binary( back_t* back, value_t* value, binary_t* binary ) {
   // Logical-or and logical-and both have short-circuit evaluation.
   if ( binary->op == k_bop_log_or ) {
      value_t lside;
      init_value( &lside );
      lside.needed = true;
      do_operand( back, &lside, binary->lside );
      b_pos_t* bail_pos = b_new_pos( back );
      value_t rside;
      init_value( &rside );
      rside.needed = true;
      b_pos_t* rside_pos = b_new_pos( back );
      do_operand( back, &rside, binary->rside );
      b_pcode( back, pc_negate_logical );
      b_pcode( back, pc_negate_logical );
      b_pos_t* end_pos = b_new_pos( back );
      b_seek( back, bail_pos );
      b_pcode( back, pp_if_not_goto, rside_pos );
      b_pcode( back, pc_push_number, 1 );
      b_pcode( back, pp_goto, end_pos );
      b_seek( back, NULL );
   }
   else if ( binary->op == k_bop_log_and ) {
      value_t lside;
      init_value( &lside );
      lside.needed = true;
      do_operand( back, &lside, binary->lside );
      b_pos_t* bail_pos = b_new_pos( back );
      value_t rside;
      init_value( &rside );
      rside.needed = true;
      b_pos_t* rside_pos = b_new_pos( back );
      do_operand( back, &rside, binary->rside );
      b_pcode( back, pc_negate_logical );
      b_pcode( back, pc_negate_logical );
      b_pos_t* end_pos = b_new_pos( back );
      b_seek( back, bail_pos );
      b_pcode( back, pp_if_goto, rside_pos );
      b_pcode( back, pc_push_number, 0 );
      b_pcode( back, pp_goto, end_pos );
      b_seek( back, NULL );
   }
   else if ( binary->is_folded ) {
      b_pcode( back, pc_push_number, binary->value );
   }
   else {
      value_t lside;
      init_value( &lside );
      lside.needed = true;
      lside.stack = true;
      do_operand( back, &lside, binary->lside );
      value_t rside;
      init_value( &rside );
      rside.needed = true;
      rside.stack = true;
      do_operand( back, &rside, binary->rside );
      opcode_t code = pc_none;
      switch ( binary->op ) {
      case k_bop_bit_or: code = pc_or_bitwise; break;
      case k_bop_bit_xor: code = pc_eor_bitwise; break;
      case k_bop_bit_and: code = pc_and_bitwise; break;
      case k_bop_equal: code = pc_eq; break;
      case k_bop_not_equal: code = pc_ne; break;
      case k_bop_less_than: code = pc_lt; break;
      case k_bop_less_than_equal: code = pc_le; break;
      case k_bop_more_than: code = pc_gt; break;
      case k_bop_more_than_equal: code = pc_ge; break;
      case k_bop_shift_l: code = pc_lshift; break;
      case k_bop_shift_r: code = pc_rshift; break;
      case k_bop_add: code = pc_add; break;
      case k_bop_sub: code = pc_sub; break;
      case k_bop_mul: code = pc_mul; break;
      case k_bop_div: code = pc_div; break;
      case k_bop_mod: code = pc_mod; break;
      default: break;
      }
      b_pcode( back, code );
   }
   value->stacked = true;
}

void do_assign( back_t* back, value_t* value, assign_t* assign ) {
   value_t lside;
   init_value( &lside );
   lside.needed = true;
   do_operand( back, &lside, assign->lside );
   value_t rside;
   init_value( &rside );
   rside.needed = true;
   do_operand( back, &rside, assign->rside );
   b_pcode( back, pc_assign_map_array, SPACE_ARRAY );
   if ( value->needed ) {
      access_array( back, lside.storage, lside.index );
      value->stacked = true;
   }
}

void access_var( back_t* back, int storage, int index ) {
   b_pcode( back, pc_push_map_var, CS_POINTER_VAR );
   b_pcode( back, pc_push_number, index );
   b_pcode( back, pc_add );
   b_pcode( back, pc_push_map_array, SPACE_ARRAY );
}

void access_array( back_t* back, int storage, int index ) {
   int code = pc_push_map_array;
   switch ( storage ) {
   case k_storage_world: code = pc_push_world_array; break;
   case k_storage_global: code = pc_push_global_array; break;
   default: break;
   }
   b_pcode( back, code, index );
}

void do_ternary( back_t* back, ternary_t* ternary ) {
/*
   // Test.
   do_node( back, ternary->test );
   b_seq_pcode( back, pc_if_goto );
   int test = o_pos( back );
   o_int( back, 0 );

   // Right-side (False).
   do_node( back, ternary->rside );
   o_pcode( back, pc_goto );
   int rside = o_pos( back );
   o_int( back, 0 );

   // Left-side (True).
   int lside = o_pos( back );
   do_node( back, ternary->lside );

   // Finish.
   int exit_pos = o_pos( back );
   o_seek( back, test );
   o_int( back, lside );
   o_seek( back, rside );
   o_int( back, exit_pos );
   o_seek_end( back );*/
}

void assign_var( back_t* back, int storage, int index, int op ) {
   b_pcode( back, pc_push_map_var, CS_POINTER_VAR );
   b_pcode( back, pc_push_number, index );
   b_pcode( back, pc_add );
   b_pcode( back, pc_assign_map_array, SPACE_ARRAY );
}

void assign_array( back_t* back, int storage, int index, int op ) {
   static int code[] = {
      pc_assign_map_array, pc_assign_world_array, pc_assign_global_array,
      pc_add_map_array, pc_add_world_array, pc_add_global_array,
      pc_sub_map_var, pc_sub_world_array, pc_sub_global_array,
      pc_mul_map_array, pc_mul_world_array, pc_mul_global_array,
      pc_div_map_array, pc_div_world_array, pc_div_global_array,
      pc_mod_map_array, pc_mod_world_array, pc_mod_global_array,
      pc_ls_map_array, pc_ls_world_array, pc_ls_global_array,
      pc_rs_map_array, pc_rs_world_array, pc_rs_global_array,
      pc_and_map_array, pc_and_world_array, pc_and_global_array,
      pc_eor_map_array, pc_eor_world_array, pc_eor_global_array,
      pc_or_map_array, pc_or_world_array, pc_or_global_array };
   int pos = 0;
   switch ( storage ) {
   case k_storage_world: pos = 1; break;
   case k_storage_global: pos = 2; break;
   default: break;
   }
   b_pcode( back, code[ op * 3 + pos ], index );
}

void inc_var( back_t* back, int storage, int index, bool dec ) {
   int code = 0;
   if ( dec ) {
      switch ( storage ) {
      case k_storage_module: code = pc_dec_map_var; break;
      case k_storage_world: code = pc_dec_world_var; break;
      case k_storage_global: code = pc_dec_global_var; break;
      default: code = pc_dec_script_var; break;
      }
   }
   else {
      switch ( storage ) {
      case k_storage_module: code = pc_inc_map_var; break;
      case k_storage_world: code = pc_inc_world_var; break;
      case k_storage_global: code = pc_inc_global_var; break;
      default: code = pc_inc_script_var; break;
      }
   }
   b_pcode( back, code, index );
}

void inc_array( back_t* back, int storage, int index, bool dec ) {
   int code = 0;
   if ( dec ) {
      switch ( storage ) {
      case k_storage_world: code = pc_dec_world_array; break;
      case k_storage_global: code = pc_dec_global_array; break;
      default: code = pc_dec_map_array; break;
      }
   }
   else {
      switch ( storage ) {
      case k_storage_world: code = pc_inc_world_array; break;
      case k_storage_global: code = pc_inc_global_array; break;
      default: code = pc_inc_map_array; break;
      }
   }
   b_pcode( back, code, index );
}

void do_format_item( back_t* back, format_item_t* item ) {
   static int casts[] = {
      0, pc_print_binary, pc_print_character, pc_print_number, pc_print_fixed,
      pc_print_bind, pc_print_localized, pc_print_name, pc_print_string,
      pc_print_hex };
   // The array cast requires special handling because one can print an
   // array from multiple storages.
   if ( item->cast == k_fcast_array ) {
      // do_operand( back, 
   }
   else {
      do_expr( back, item->expr, true );
      b_pcode( back, casts[ item->cast ] );
   }
}

void do_jump( back_t* back, jump_t* jump ) {
   jump->offset = b_new_pos( back );
}

void do_script_jump( back_t* back, script_jump_t* jump ) {
   int code = pc_terminate;
   switch ( jump->type ) {
   case k_jump_restart: code = pc_restart; break;
   case k_jump_suspend: code = pc_suspend; break;
   default: break;
   }
   b_pcode( back, code );
}

void do_return( back_t* back, return_t* stmt ) {
   if ( stmt->expr ) {
      do_expr( back, stmt->expr, true );
      b_pcode( back, pc_return_val );
   }
   else {
      b_pcode( back, pc_return_void );
   }
}

void do_var_def( back_t* back, var_t* var ) {
   if ( var->initial ) {
      do_expr( back, ( expr_t* ) var->initial->value, true );
      assign_var( back, var->storage, var->index, k_assign );
   }
}

/*
void do_array_def( back_t* back, array_t* array ) {
   int index = 0;
   list_iterator_t i = list_iterate( &array->initials );
   while ( ! list_end( &i ) ) {
      node_t* node = list_idata( &i );
      if ( node->type == k_node_array_jump ) {
         array_jump_t* jump = ( array_jump_t* ) node;
         index = jump->index;
      }
      else {
         b_pcode( back, pc_push_number, index );
         do_expr( back, ( expr_t* ) node, true );
         assign_array( back, array->storage, array->index, k_assign );
         ++index;
      }
      list_next( &i );
   }
}*/

void do_if( back_t* back, if_t* stmt ) {
   do_cond( back, stmt->cond );
   b_pos_t* test = b_new_pos( back );
   do_block( back, &stmt->body );
   b_pos_t* end = b_new_pos( back );
   if ( list_size( &stmt->else_body ) ) {
      do_block( back, &stmt->else_body );
      b_pos_t* else_end = b_new_pos( back );
      b_seek( back, end );
      b_pcode( back, pp_goto, else_end );
      end = b_new_pos( back );
   }
   b_seek( back, test );
   b_pcode( back, pp_if_not_goto, end );
   b_seek( back, NULL );
}

void do_cond( back_t* back, node_t* node ) {
   if ( node->type == k_node_expr ) {
      do_expr( back, ( expr_t* ) node, true );
   }
}

void do_switch( back_t* back, switch_t* stmt ) {
   do_cond( back, stmt->cond );
   int num_cases = 0;
   case_t* item = stmt->head_case_sorted;
   while ( item ) {
      ++num_cases;
      item = item->next_sorted;
   }
   b_pos_t* case_pos = b_new_pos( back );
   do_block( back, &stmt->body );
   b_pos_t* bail_pos = b_new_pos( back );
   write_jump( back, stmt->jump_break, bail_pos );
   b_seek( back, case_pos );
   if ( num_cases ) {
      b_pcode( back, pp_opcode, pc_case_goto_sorted );
      b_pcode( back, pp_arg, num_cases );
      item = stmt->head_case_sorted;
      while ( item ) {
         b_pcode( back, pp_arg, item->expr->value );
         b_pcode( back, pp_arg_pos, item->offset );
         item = item->next_sorted;
      }
   }
   b_pcode( back, pc_drop );
   if ( stmt->default_case ) {
      bail_pos = stmt->default_case->offset;
   }
   b_pcode( back, pp_goto, bail_pos );
   b_seek( back, NULL );
}

void do_case( back_t* back, case_t* stmt ) {
   stmt->offset = b_new_pos( back );
}

void do_while( back_t* back, while_t* stmt ) {
   if ( stmt->type == k_while_while || stmt->type == k_while_until ) {
      b_pos_t* cond_pos = b_new_pos( back );
      do_cond( back, stmt->cond );
      b_pos_t* check_pos = b_new_pos( back );
      do_block( back, &stmt->body );
      b_pcode( back, pp_goto, cond_pos );
      b_pos_t* end_pos = b_new_pos( back );
      write_jump( back, stmt->jump_early, cond_pos );
      write_jump( back, stmt->jump_break, end_pos );
      b_seek( back, check_pos );
      opcode_t code = pp_if_goto;
      if ( stmt->type == k_while_while ) {
         code = pp_if_not_goto;
      }
      b_pcode( back, code, end_pos );
      b_seek( back, NULL );
   }
   else {
      b_pos_t* block_pos = b_new_pos( back );
      do_block( back, &stmt->body );
      write_jump( back, stmt->jump_early, b_new_pos( back ) );
      do_cond( back, stmt->cond );
      opcode_t code = pp_if_not_goto;
      if ( stmt->type == k_while_do_while ) {
         code = pp_if_goto;
      }
      b_pcode( back, code, block_pos );
      write_jump( back, stmt->jump_break, b_new_pos( back ) );
   }
}

void do_for( back_t* back, for_t* stmt ) {
   list_iterator_t i = list_iterate( &stmt->init );
   while ( ! list_end( &i ) ) {
      do_node( back, list_idata( &i ) );
      list_next( &i );
   }
   b_pos_t* cond_pos = b_new_pos( back );
   b_pos_t* check_pos = NULL;
   if ( stmt->cond ) {
      do_cond( back, stmt->cond );
      check_pos = b_new_pos( back );
   }
   do_block( back, &stmt->body );
   b_pos_t* skip_pos = b_new_pos( back );
   if ( stmt->post ) { 
      do_expr( back, stmt->post, false );
   }
   b_pcode( back, pp_goto, cond_pos );
   b_pos_t* end_pos = b_new_pos( back );
   if ( stmt->cond ) {
      b_seek( back, check_pos );
      b_pcode( back, pp_if_not_goto, end_pos );
      b_seek( back, NULL );
   }
   write_jump( back, stmt->jump_continue, skip_pos );
   write_jump( back, stmt->jump_break, end_pos );
}

void write_jump( back_t* back, jump_t* jump, b_pos_t* pos ) {
   while ( jump ) {
      b_seek( back, jump->offset );
      b_pcode( back, pp_goto, pos );
      jump = jump->next;
   }
   b_seek( back, NULL );
}