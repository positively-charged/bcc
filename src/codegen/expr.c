#include <limits.h>

#include "phase.h"
#include "pcode.h"

struct result {
   struct structure* type;
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

static void init_result( struct result* codegen );
static void push_operand( struct codegen* codegen, struct node* node );
static void visit_operand( struct codegen* codegen, struct result* result,
   struct node* node );
static void visit_binary( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_int( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_zfixed( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_zbool( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_str( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void eq_str( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void lt_str( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void concat_str( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void visit_logical( struct codegen* codegen,
   struct result* result, struct logical* logical );
static void write_logical( struct codegen* codegen,
   struct result* result, struct logical* logical );
static void push_logical_operand( struct codegen* codegen,
   struct node* node, int spec );
static void visit_assign( struct codegen* codegen, struct result* result,
   struct assign* assign );
static void visit_conditional( struct codegen* codegen,
   struct result* result, struct conditional* cond );
static void write_conditional( struct codegen* codegen,
   struct result* result, struct conditional* cond );
static void write_middleless_conditional( struct codegen* codegen,
   struct result* result, struct conditional* cond );
static void visit_prefix( struct codegen* codegen, struct result* result,
   struct node* node );
static void visit_unary( struct codegen* codegen, struct result* result,
   struct unary* unary );
static void write_logical_not( struct codegen* codegen, struct result* result,
   struct unary* unary );
static void write_unary( struct codegen* codegen, struct result* result,
   struct unary* unary );
static void visit_inc( struct codegen* codegen,
   struct result* result, struct inc* inc );
static void inc_array( struct codegen* codegen, struct result* result,
   struct inc* inc, struct result* operand );
static void inc_element( struct codegen* codegen, int storage, int index,
   bool do_inc );
static void inc_var( struct codegen* codegen, struct result* result,
   struct inc* inc, struct result* operand );
static void inc_indexed( struct codegen* codegen, int storage, int index,
   bool do_inc );
static void inc_zfixed( struct codegen* codegen, struct inc* inc,
   struct result* operand );
static void visit_suffix( struct codegen* codegen, struct result* result,
   struct node* node );
static void visit_object( struct codegen* codegen, struct result* result,
   struct node* node );
static void visit_subscript( struct codegen* codegen, struct result* result,
   struct subscript* subscript );
static void visit_access( struct codegen* codegen, struct result* result,
   struct access* access );
static void visit_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_aspec_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_ext_call( struct codegen* codegen, struct result* result,
   struct call* call );
static int push_nonzero_args( struct codegen* codegen, struct param* params,
   struct list* args, int min );
static void visit_ded_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_format_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_formatblock_arg( struct codegen* codegen,
   struct format_block_usage* usage );
static void write_format_block( struct codegen* codegen,
   struct format_block_usage* usage );
static void write_jumpable_format_block( struct codegen* codegen,
   struct format_block_usage* usage );
static void visit_array_format_item( struct codegen* codegen,
   struct format_item* item );
static void visit_user_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void write_call_args( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_nested_userfunc_call( struct codegen* codegen,
   struct result* result, struct call* call );
static void visit_internal_call( struct codegen* codegen,
   struct result* result, struct call* call );
static void write_executewait( struct codegen* codegen, struct call* call,
   bool named_impl );
static void visit_primary( struct codegen* codegen, struct result* result,
   struct node* node );
static void visit_literal( struct codegen* codegen, struct result* result,
   struct literal* literal );
static void visit_indexed_string_usage( struct codegen* codegen,
   struct result* result, struct indexed_string_usage* usage );
static void visit_boolean( struct codegen* codegen, struct result* result,
   struct boolean* boolean );
static void visit_name_usage( struct codegen* codegen, struct result* result,
   struct name_usage* usage );
static void visit_constant( struct codegen* codegen, struct result* result,
   struct constant* constant );
static void visit_enumerator( struct codegen* codegen,
   struct result* result, struct enumerator* enumerator );
static void visit_var( struct codegen* codegen, struct result* result,
   struct var* var );
static void set_var( struct codegen* codegen, struct result* result,
   struct var* var );
static void visit_param( struct codegen* codegen, struct result* result,
   struct param* param );
static void visit_func( struct codegen* codegen, struct result* result,
   struct func* func );
static void write_strcpy( struct codegen* codegen, struct result* result,
   struct strcpy_call* call );
static void visit_paren( struct codegen* codegen, struct result* result,
   struct paren* paren );
static void push_indexed( struct codegen* codegen, int storage, int index );
static void push_element( struct codegen* codegen, int storage, int index );

static const int g_aspec_code[] = {
   PCD_LSPEC1,
   PCD_LSPEC2,
   PCD_LSPEC3,
   PCD_LSPEC4,
   PCD_LSPEC5
};

void c_visit_expr( struct codegen* codegen, struct expr* expr ) {
   struct result result;
   init_result( &result );
   visit_operand( codegen, &result, expr->root );
   if ( result.pushed ) {
      c_pcd( codegen, PCD_DROP );
   }
}

void c_push_expr( struct codegen* codegen, struct expr* expr ) {
   struct result result;
   init_result( &result );
   result.push = true;
   result.push_temp = false;
   visit_operand( codegen, &result, expr->root );
}

void c_push_cond( struct codegen* codegen, struct expr* cond ) {
   push_operand( codegen, cond->root );
}

void init_result( struct result* result ) {
   result->type = NULL;
   result->dim = NULL;
   result->action = ACTION_PUSH_VALUE;
   result->method = METHOD_NONE;
   result->trigger = TRIGGER_INDEX;
   result->storage = 0;
   result->index = 0;
   result->base = 0;
   result->push = false;
   result->push_temp = false;
   result->pushed = false;
   result->pushed_element = false;
}

void push_operand( struct codegen* codegen, struct node* node ) {
   struct result result;
   init_result( &result );
   result.push = true;
   visit_operand( codegen, &result, node );
}

void visit_operand( struct codegen* codegen, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_BINARY:
      visit_binary( codegen, result,
         ( struct binary* ) node );
      break;
   case NODE_LOGICAL:
      visit_logical( codegen, result,
         ( struct logical* ) node );
      break;
   case NODE_ASSIGN:
      visit_assign( codegen, result,
         ( struct assign* ) node );
      break;
   case NODE_CONDITIONAL:
      visit_conditional( codegen, result,
         ( struct conditional* ) node );
      break;
   default:
      visit_prefix( codegen, result,
         node );
      break;
   }
}

void visit_binary( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   switch ( binary->lside_spec ) {
   case SPEC_ZRAW:
   case SPEC_ZINT:
      write_binary_int( codegen, result, binary );
      break;
   case SPEC_ZFIXED:
      write_binary_zfixed( codegen, result, binary );
      break;
   case SPEC_ZBOOL:
      write_binary_zbool( codegen, result, binary );
      break;
   case SPEC_ZSTR:
      write_binary_str( codegen, result, binary );
      break;
   default:
      UNREACHABLE()
   }
}

void write_binary_int( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
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
   default: 
      UNREACHABLE()
   }
   c_pcd( codegen, code );
   result->pushed = true;
}

void write_binary_zfixed( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
   int code = PCD_NONE;
   switch ( binary->op ) {
   case BOP_ADD: code = PCD_ADD; break;
   case BOP_SUB: code = PCD_SUBTRACT; break;
   case BOP_MUL: code = PCD_FIXEDMUL; break;
   case BOP_DIV: code = PCD_FIXEDDIV; break;
   case BOP_EQ: code = PCD_EQ; break;
   case BOP_NEQ: code = PCD_NE; break;
   case BOP_LT: code = PCD_LT; break;
   case BOP_LTE: code = PCD_LE; break;
   case BOP_GT: code = PCD_GT; break;
   case BOP_GTE: code = PCD_GE; break;
   default:
      UNREACHABLE()
   }
   c_pcd( codegen, code );
   result->pushed = true;
}

void write_binary_zbool( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
   int code = PCD_NONE;
   switch ( binary->op ) {
   case BOP_EQ: code = PCD_EQ; break;
   case BOP_NEQ: code = PCD_NE; break;
   default:
      UNREACHABLE()
   }
   c_pcd( codegen, code );
   result->pushed = true;
}

void write_binary_str( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
      eq_str( codegen, result, binary );
      break;
   case BOP_LT:
   case BOP_LTE:
   case BOP_GT:
   case BOP_GTE:
      lt_str( codegen, result, binary );
      break;
   case BOP_ADD:
      concat_str( codegen, result, binary );
      break;
   default:
      UNREACHABLE()
   }
}

void eq_str( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
   c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_STRCMP );
   c_pcd( codegen, PCD_NEGATELOGICAL );
   if ( binary->op == BOP_NEQ ) {
      c_pcd( codegen, PCD_NEGATELOGICAL );
   }
   result->pushed = true;
}

void lt_str( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
   c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_STRCMP );
   c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   int code = PCD_LT;
   switch ( binary->op ) {
   case BOP_LTE:
      code = PCD_LE;
      break;
   case BOP_GTE:
      code = PCD_GE;
      break;
   case BOP_GT:
      code = PCD_GT;
      break;
   default:
      UNREACHABLE()
   }
   c_pcd( codegen, code );
   result->pushed = true;
}

void concat_str( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   c_pcd( codegen, PCD_BEGINPRINT );
   push_operand( codegen, binary->lside );
   c_pcd( codegen, PCD_PRINTSTRING );
   push_operand( codegen, binary->rside );
   c_pcd( codegen, PCD_PRINTSTRING );
   c_pcd( codegen, PCD_SAVESTRING );
   result->pushed = true;
}

void visit_logical( struct codegen* codegen,
   struct result* result, struct logical* logical ) {
   if ( logical->folded ) {
      c_pcd( codegen, PCD_PUSHNUMBER, logical->value );
      result->pushed = true;
   }
   else {
      write_logical( codegen, result, logical );
   }
}

// Logical-or and logical-and both perform shortcircuit evaluation.
void write_logical( struct codegen* codegen, struct result* result,
   struct logical* logical ) {
   push_logical_operand( codegen, logical->lside, logical->lside_spec );
   struct c_jump* rside_jump = c_create_jump( codegen,
      ( logical->op == LOP_OR ? PCD_IFNOTGOTO : PCD_IFGOTO ) );
   c_append_node( codegen, &rside_jump->node );
   c_pcd( codegen, PCD_PUSHNUMBER,
      ( logical->op == LOP_OR ? 1 : 0 ) );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &exit_jump->node );
   struct c_point* rside_point = c_create_point( codegen );
   c_append_node( codegen, &rside_point->node );
   rside_jump->point = rside_point;
   push_logical_operand( codegen, logical->rside, logical->rside_spec );
   // Optimization: When doing a calculation temporarily, there's no need to
   // convert the second operand to a 0 or 1. Just use the operand directly.
   if ( ! result->push_temp ) {
      c_pcd( codegen, PCD_NEGATELOGICAL );
      c_pcd( codegen, PCD_NEGATELOGICAL );
   }
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
   result->pushed = true;
}

void push_logical_operand( struct codegen* codegen,
   struct node* node, int spec ) {
   struct result result;
   init_result( &result );
   result.push = true;
   result.push_temp = true;
   visit_operand( codegen, &result, node );
   if ( spec == SPEC_ZSTR ) {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_GETCHAR );
   }
}

void visit_assign( struct codegen* codegen, struct result* result,
   struct assign* assign ) {
   struct result lside;
   init_result( &lside );
   lside.action = ACTION_PUSH_VAR;
   visit_operand( codegen, &lside, assign->lside );
   if ( lside.method == METHOD_ELEMENT ) {
      if ( result->push ) {
         c_pcd( codegen, PCD_DUP );
      }
      push_operand( codegen, assign->rside );
      c_update_element( codegen, lside.storage, lside.index, assign->op );
      if ( result->push ) {
         push_element( codegen, lside.storage, lside.index );
         result->pushed = true;
      }
   }
   else {
      push_operand( codegen, assign->rside );
      if ( assign->op == AOP_NONE && result->push ) {
         c_pcd( codegen, PCD_DUP );
         result->pushed = true;
      }
      c_update_indexed( codegen, lside.storage, lside.index, assign->op );
      if ( assign->op != AOP_NONE && result->push ) {
         push_indexed( codegen, lside.storage, lside.index );
         result->pushed = true;
      }
   }
}

void visit_conditional( struct codegen* codegen, struct result* result,
   struct conditional* cond ) {
   if ( cond->folded ) {
      push_operand( codegen, cond->left_value ?
         ( cond->middle ? cond->middle : cond->left ) : cond->right );
      result->pushed = true;
   }
   else {
      if ( cond->middle ) {
         write_conditional( codegen, result, cond );
      }
      else {
         write_middleless_conditional( codegen, result, cond );
      }
   }
}

void write_conditional( struct codegen* codegen, struct result* result,
   struct conditional* cond ) {
   push_operand( codegen, cond->left );
   struct c_jump* right_jump = c_create_jump( codegen, PCD_IFNOTGOTO );
   c_append_node( codegen, &right_jump->node );
   push_operand( codegen, cond->middle );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &exit_jump->node );
   struct c_point* right_point = c_create_point( codegen );
   c_append_node( codegen, &right_point->node );
   right_jump->point = right_point;
   push_operand( codegen, cond->right );
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
   result->pushed = true;
}

void write_middleless_conditional( struct codegen* codegen,
   struct result* result, struct conditional* cond ) {
   push_operand( codegen, cond->left );
   c_pcd( codegen, PCD_DUP );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &exit_jump->node );
   c_pcd( codegen, PCD_DROP );
   push_operand( codegen, cond->right );
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
   result->pushed = true;
}

void visit_prefix( struct codegen* codegen, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_UNARY:
      visit_unary( codegen, result,
         ( struct unary* ) node );
      break;
   case NODE_INC:
      visit_inc( codegen, result,
         ( struct inc* ) node );
      break;
   default:
      visit_suffix( codegen, result, node );
      break;
   }
}

void visit_unary( struct codegen* codegen, struct result* result,
   struct unary* unary ) {
   if ( unary->op == UOP_LOG_NOT ) {
      write_logical_not( codegen, result, unary );
   }
   else {
      write_unary( codegen, result, unary );
   }
}

void write_logical_not( struct codegen* codegen, struct result* result,
   struct unary* unary ) {
   push_logical_operand( codegen, unary->operand, unary->operand_spec );
   c_pcd( codegen, PCD_NEGATELOGICAL );
   result->pushed = true;
}

void write_unary( struct codegen* codegen, struct result* result,
   struct unary* unary ) {
   push_operand( codegen, unary->operand );
   switch ( unary->op ) {
   case UOP_MINUS:
      c_pcd( codegen, PCD_UNARYMINUS );
      break;
   case UOP_BIT_NOT:
      c_pcd( codegen, PCD_NEGATEBINARY );
      break;
   case UOP_PLUS:
      // Unary plus is ignored.
      break;
   default:
      UNREACHABLE()
   }
   result->pushed = true;
}

void visit_inc( struct codegen* codegen, struct result* result,
   struct inc* inc ) {
   struct result operand;
   init_result( &operand );
   operand.action = ACTION_PUSH_VAR;
   visit_operand( codegen, &operand, inc->operand );
   if ( operand.method == METHOD_ELEMENT ) {
      inc_array( codegen, result, inc, &operand );
   }
   else {
      inc_var( codegen, result, inc, &operand );
   }
}

void inc_array( struct codegen* codegen, struct result* result,
   struct inc* inc, struct result* operand ) {
   if ( result->push ) {
      c_pcd( codegen, PCD_DUP );
      if ( inc->post ) {
         push_element( codegen, operand->storage, operand->index );
         c_pcd( codegen, PCD_SWAP );
         result->pushed = true;
      }
   }
   if ( inc->zfixed ) {
      inc_zfixed( codegen, inc, operand );
   }
   else {
      inc_element( codegen, operand->storage, operand->index, ( ! inc->dec ) );
   }
   if ( ! inc->post && result->push ) {
      push_element( codegen, operand->storage, operand->index );
      result->pushed = true;
   }
}

void inc_element( struct codegen* codegen, int storage, int index,
   bool do_inc ) {
   int code = PCD_INCMAPARRAY;
   if ( do_inc ) {
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
   }
   else {
      switch ( storage ) {
      case STORAGE_WORLD:
         code = PCD_DECWORLDARRAY;
         break;
      case STORAGE_GLOBAL:
         code = PCD_DECGLOBALARRAY;
         break;
      default:
         code = PCD_DECMAPARRAY;
         break;
      }
   }
   c_pcd( codegen, code, index );
}

void inc_var( struct codegen* codegen, struct result* result,
   struct inc* inc, struct result* operand ) {
   if ( inc->post && result->push ) {
      push_indexed( codegen, operand->storage, operand->index );
      result->pushed = true;
   }
   if ( inc->zfixed ) {
      inc_zfixed( codegen, inc, operand );
   }
   else {
      inc_indexed( codegen, operand->storage, operand->index, ( ! inc->dec ) );
   }
   if ( ! inc->post && result->push ) {
      push_indexed( codegen, operand->storage, operand->index );
      result->pushed = true;
   }
}

void inc_indexed( struct codegen* codegen, int storage, int index,
   bool do_inc ) {
   int code = PCD_INCSCRIPTVAR;
   if ( do_inc ) {
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
   }
   else {
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
         code = PCD_DECSCRIPTVAR;
         break;
      }
   }
   c_pcd( codegen, code, index );
}

void inc_zfixed( struct codegen* codegen, struct inc* inc,
   struct result* operand ) {
   enum { ZFIXED_WHOLE = 65536 };
   c_pcd( codegen, PCD_PUSHNUMBER, ZFIXED_WHOLE );
   if ( operand->method == METHOD_ELEMENT ) {
      c_update_element( codegen, operand->storage, operand->index,
         ( inc->dec ? AOP_SUB : AOP_ADD ) );
   }
   else {
      c_update_indexed( codegen, operand->storage, operand->index,
         ( inc->dec ? AOP_SUB : AOP_ADD ) );
   }
}

void visit_suffix( struct codegen* codegen, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_SUBSCRIPT:
   case NODE_ACCESS:
      visit_object( codegen, result,
         node );
      break;
   case NODE_CALL:
      visit_call( codegen, result,
         ( struct call* ) node );
      break;
   default:
      visit_primary( codegen, result,
         node );
      break;
   }
}

void visit_object( struct codegen* codegen, struct result* result,
   struct node* node ) {
   if ( node->type == NODE_ACCESS ) {
      visit_access( codegen, result, ( struct access* ) node );
   }
   else if ( node->type == NODE_SUBSCRIPT ) {
      visit_subscript( codegen, result, ( struct subscript* ) node );
   }
   if ( result->method == METHOD_ELEMENT ) {
      if ( result->pushed_element ) {
         if ( result->base ) {
            c_pcd( codegen, PCD_PUSHNUMBER, result->base );
            c_pcd( codegen, PCD_ADD );
         }
      }
      else {
         c_pcd( codegen, PCD_PUSHNUMBER, result->base );
      }
   }
   if ( result->action == ACTION_PUSH_VALUE &&
      result->method != METHOD_NONE ) {
      if ( result->method == METHOD_ELEMENT ) {
         push_element( codegen, result->storage, result->index );
      }
      else {
         push_indexed( codegen, result->storage, result->index );
      }
      result->pushed = true;
   }
}

void visit_subscript( struct codegen* codegen, struct result* result,
   struct subscript* subscript ) {
   struct node* lside = subscript->lside;
   while ( lside->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) lside;
      lside = paren->inside;
   }
   if ( lside->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) lside;
      lside = usage->object;
   }
   // Left side:
   if ( lside->type == NODE_VAR ) {
      set_var( codegen, result, ( struct var* ) lside );
   }
   else if ( lside->type == NODE_ACCESS ) {
      visit_access( codegen, result, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      visit_subscript( codegen, result, ( struct subscript* ) lside );
   }
   // Dimension:
   push_operand( codegen, subscript->index->root );
   if ( result->dim->next ) {
      c_pcd( codegen, PCD_PUSHNUMBER, result->dim->element_size );
      c_pcd( codegen, PCD_MULIPLY );
   }
   else if ( ! result->type->primitive ) {
      c_pcd( codegen, PCD_PUSHNUMBER, result->type->size );
      c_pcd( codegen, PCD_MULIPLY );
   }
   if ( result->pushed_element ) {
      c_pcd( codegen, PCD_ADD );
   }
   else {
      result->pushed_element = true;
   }
   result->dim = result->dim->next;
}

void visit_access( struct codegen* codegen, struct result* result,
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
   }
   // See if the left side is a namespace.
   struct node* object = lside;
   if ( object->type == NODE_ACCESS ) {
      struct access* nested = ( struct access* ) object;
      object = nested->rside;
   }
   // Left side:
   if ( lside->type == NODE_VAR ) {
      set_var( codegen, result, ( struct var* ) lside );
   }
   else if ( lside->type == NODE_CONSTANT ) {
      visit_constant( codegen, result, ( struct constant* ) lside );
   }
   else if ( lside->type == NODE_ACCESS ) {
      visit_access( codegen, result, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      visit_subscript( codegen, result, ( struct subscript* ) lside );
   }
   else {
      visit_operand( codegen, result, lside );
   }
   // Right side:
   if ( rside && rside->type == NODE_STRUCTURE_MEMBER ) {
      struct structure_member* member = ( struct structure_member* ) rside;
      c_pcd( codegen, PCD_PUSHNUMBER, member->offset );
      if ( result->pushed_element ) {
         c_pcd( codegen, PCD_ADD );
      }
      else {
         result->pushed_element = true;
      }
      result->type = member->structure;
      result->dim = member->dim;
   }
}

void visit_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   switch ( call->func->type ) {
   case FUNC_ASPEC:
      visit_aspec_call( codegen, result, call );
      break;
   case FUNC_EXT:
      visit_ext_call( codegen, result, call );
      break;
   case FUNC_DED:
      visit_ded_call( codegen, result, call );
      break;
   case FUNC_FORMAT:
      visit_format_call( codegen, result, call );
      break;
   case FUNC_USER:
      visit_user_call( codegen, result, call );
      break;
   case FUNC_INTERNAL:
      visit_internal_call( codegen, result, call );
      break;
   }
}

void visit_aspec_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   int count = push_nonzero_args( codegen, call->func->params,
      &call->args, 0 );
   struct func_aspec* aspec = call->func->impl;
   if ( result->push ) {
      while ( count < 5 ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         ++count;
      }
      c_pcd( codegen, PCD_LSPEC5RESULT, aspec->id );
      result->pushed = true;
   }
   else if ( count ) {
      c_pcd( codegen, g_aspec_code[ count - 1 ], aspec->id );
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_LSPEC1, aspec->id );
   }
}

void visit_ext_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   int count = push_nonzero_args( codegen, call->func->params, &call->args,
      call->func->min_param );
   struct func_ext* impl = call->func->impl;
   c_pcd( codegen, PCD_CALLFUNC, count, impl->id );
   result->pushed = true;
}

// Pushes the specified function arguments onto the stack. A minimum amount of
// arguments will always be pushed. After the minimum amount, only those
// arguments up to, and including, the last non-zero argument will be pushed.
// There is no need to pass the trailing zero arguments because the engine will
// implicitly pass them.
int push_nonzero_args( struct codegen* codegen, struct param* params,
   struct list* args, int min ) {
   int count = 0;
   list_iter_t k;
   list_iter_init( &k, args );
   struct param* param = params;
   // Add to the count the minimum arguments required.
   while ( count < min ) {
      param = param->next;
      list_next( &k );
      ++count;
   }
   // Add to the count the range of arguments that contains all remaining
   // non-zero arguments.
   int i = count;
   while ( param ) {
      struct expr* arg = param->default_value;
      if ( ! list_end( &k ) ) {
         arg = list_data( &k );
         list_next( &k );
      }
      ++i;
      bool zero = ( arg->folded && ! arg->has_str && arg->value == 0 );
      if ( ! zero ) {
         count = i;
      }
      param = param->next;
   }
   // Write arguments.
   i = 0;
   param = params;
   list_iter_init( &k, args );
   while ( i < count ) {
      struct expr* arg = param->default_value;
      if ( ! list_end( &k ) ) {
         arg = list_data( &k );
         list_next( &k );
      }
      c_push_expr( codegen, arg );
      if ( param->used ) {
         c_pcd( codegen, PCD_DUP );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, param->index );
      }
      param = param->next;
      ++i;
   }
   return count;
}

void visit_ded_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( param ) {
      struct expr* arg = param->default_value;
      if ( ! list_end( &i ) ) {
         arg = list_data( &i );
         list_next( &i );
      }
      c_push_expr( codegen, arg );
      if ( param->used ) {
         c_pcd( codegen, PCD_DUP );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, param->index );
      }
      param = param->next;
   }
   struct func_ded* ded = call->func->impl;
   c_pcd( codegen, ded->opcode );
   if ( call->func->return_type ) {
      result->pushed = true;
   }
}

void visit_user_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   struct func_user* impl = call->func->impl;
   if ( impl->nested ) {
      visit_nested_userfunc_call( codegen, result, call );
   }
   else {
      write_call_args( codegen, result, call );
      if ( call->func->return_type ) {
         c_pcd( codegen, PCD_CALL, impl->index );
         result->pushed = true;
      }
      else {
         c_pcd( codegen, PCD_CALLDISCARD, impl->index );
      }
   }
}

void write_call_args( struct codegen* codegen, struct result* result,
   struct call* call ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( ! list_end( &i ) ) {
      c_push_expr( codegen, list_data( &i ) );
      list_next( &i );
      param = param->next;
   }
   // Default arguments.
   while ( param ) {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      param = param->next;
   }
   // Number of real arguments passed, for a function with default
   // parameters.
   if ( call->func->min_param != call->func->max_param ) {
      c_pcd( codegen, PCD_PUSHNUMBER, list_size( &call->args ) );
   }
}

void visit_nested_userfunc_call( struct codegen* codegen,
   struct result* result, struct call* call ) {
   // Push ID of entry to identify return address.
   c_pcd( codegen, PCD_PUSHNUMBER, call->nested_call->id );
   write_call_args( codegen, result, call );
   struct c_jump* prologue_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &prologue_jump->node );
   call->nested_call->prologue_jump = prologue_jump;
   struct c_point* return_point = c_create_point( codegen );
   c_append_node( codegen, &return_point->node );
   call->nested_call->return_point = return_point;
   if ( call->func->return_type ) {
      result->pushed = true;
   }
}

void visit_format_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   c_pcd( codegen, PCD_BEGINPRINT );
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct node* node = list_data( &i );
   // Format-block:
   if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
      visit_formatblock_arg( codegen, ( struct format_block_usage* ) node );
   }
   // Format-list:
   else {
      c_visit_format_item( codegen, list_data( &i ) );
   }
   list_next( &i );
   // Other arguments.
   if ( call->func->max_param > 1 ) {
      c_pcd( codegen, PCD_MOREHUDMESSAGE );
      int param = 1;
      while ( ! list_end( &i ) ) {
         if ( param == call->func->min_param ) {
            c_pcd( codegen, PCD_OPTHUDMESSAGE );
         }
         c_push_expr( codegen, list_data( &i ) );
         ++param;
         list_next( &i );
      }
   }
   struct func_format* format = call->func->impl;
   c_pcd( codegen, format->opcode );
   if ( call->func->return_type ) {
      result->pushed = true;
   }
}

void visit_formatblock_arg( struct codegen* codegen,
   struct format_block_usage* usage ) {
   if ( usage->next ) {
      struct c_point* point = c_create_point( codegen );
      c_append_node( codegen, &point->node );
      usage->point = point;
      if ( ! codegen->local_record->format_block_usage ) {
         codegen->local_record->format_block_usage = usage;
      }
   }
   else {
      if ( codegen->local_record->format_block_usage ) {
         write_jumpable_format_block( codegen, usage );
         codegen->local_record->format_block_usage = NULL;
      }
      else {
         write_format_block( codegen, usage );
      }
   }
}

void write_format_block( struct codegen* codegen,
   struct format_block_usage* usage ) {
   c_write_block( codegen, usage->block );
}

// When a format block is used more than once in the same expression, instead
// of duplicating the code, use a goto instruction to enter the format block.
// At each usage except the last, before the format block is used, a unique
// number is pushed. This number is used to determine the return location.
void write_jumpable_format_block( struct codegen* codegen,
   struct format_block_usage* usage ) {
   enum { LAST_USAGE_ID = 0 };
   enum { STARTING_ID = LAST_USAGE_ID + 1 };
   c_pcd( codegen, PCD_PUSHNUMBER, LAST_USAGE_ID );
   struct c_point* enter_point = c_create_point( codegen );
   c_append_node( codegen, &enter_point->node );
   c_write_block( codegen, usage->block );
   // Update jumps.
   usage = codegen->local_record->format_block_usage;
   int entry_id = STARTING_ID;
   while ( usage->next ) {
      c_seek_node( codegen, &usage->point->node );
      c_pcd( codegen, PCD_PUSHNUMBER, entry_id );
      struct c_jump* jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &jump->node );
      jump->point = enter_point;
      struct c_point* return_point = c_create_point( codegen );
      c_append_node( codegen, &return_point->node );
      usage->point = return_point;
      ++entry_id;
      usage = usage->next;
   }
   c_seek_node( codegen, codegen->node_tail );
   // Publish return-table.
   struct c_sortedcasejump* return_table = c_create_sortedcasejump( codegen );
   c_append_node( codegen, &return_table->node );
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   struct c_casejump* entry = c_create_casejump( codegen,
      LAST_USAGE_ID, exit_point );
   c_append_casejump( return_table, entry );
   usage = codegen->local_record->format_block_usage;
   entry_id = STARTING_ID;
   while ( usage->next ) {
      entry = c_create_casejump( codegen, entry_id, usage->point );
      c_append_casejump( return_table, entry );
      ++entry_id;
      usage = usage->next;
   }
}

void c_visit_format_item( struct codegen* codegen, struct format_item* item ) {
   while ( item ) {
      if ( item->cast == FCAST_ARRAY ) {
         visit_array_format_item( codegen, item );
         
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
         c_push_expr( codegen, item->value );
         c_pcd( codegen, casts[ item->cast - 1 ] );
      }
      item = item->next;
   }
}

void visit_array_format_item( struct codegen* codegen,
   struct format_item* item ) {
   struct result object;
   init_result( &object );
   object.action = ACTION_PUSH_VAR;
   visit_operand( codegen, &object, item->value->root );
   c_pcd( codegen, PCD_PUSHNUMBER, object.index );
   if ( item->extra ) {
      struct format_item_array* extra = item->extra;
      c_push_expr( codegen, extra->offset );
      if ( extra->length ) {
         c_push_expr( codegen, extra->length );
      }
      else {
         enum { MAX_LENGTH = INT_MAX };
         c_pcd( codegen, PCD_PUSHNUMBER, MAX_LENGTH );
      }
   }
   int code = PCD_NONE;
   if ( item->extra ) {
      switch ( object.storage ) {
      case STORAGE_MAP:
         code = PCD_PRINTMAPCHRANGE;
         break;
      case STORAGE_WORLD:
         code = PCD_PRINTWORLDCHRANGE;
         break;
      case STORAGE_GLOBAL:
         code = PCD_PRINTGLOBALCHRANGE;
         break;
      default:
         break;
      }
   }
   else {
      switch ( object.storage ) {
      case STORAGE_MAP:
         code = PCD_PRINTMAPCHARARRAY;
         break;
      case STORAGE_WORLD:
         code = PCD_PRINTWORLDCHARARRAY;
         break;
      case STORAGE_GLOBAL:
         code = PCD_PRINTGLOBALCHARARRAY;
         break;
      default:
         break;
      }
   }
   c_pcd( codegen, code );
}

void visit_internal_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   struct func_intern* impl = call->func->impl; 
   if ( impl->id == INTERN_FUNC_ACS_EXECWAIT ||
      impl->id == INTERN_FUNC_ACS_NAMEDEXECUTEWAIT ) {
      write_executewait( codegen, call,
         ( impl->id == INTERN_FUNC_ACS_NAMEDEXECUTEWAIT ) );
   }
   else if ( impl->id == INTERN_FUNC_STR_LENGTH ) {
      visit_operand( codegen, result, call->operand );
      c_pcd( codegen, PCD_STRLEN );
   }
   else if ( impl->id == INTERN_FUNC_STR_AT ) {
      visit_operand( codegen, result, call->operand );
      c_push_expr( codegen, list_head( &call->args ) );
      c_pcd( codegen, PCD_CALLFUNC, 2, 15 );
   }
}

void write_executewait( struct codegen* codegen, struct call* call,
   bool named_impl ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   c_push_expr( codegen, list_data( &i ) );
   c_pcd( codegen, PCD_DUP );
   list_next( &i );
   if ( ! list_end( &i ) ) {
      // Second argument to Acs_Execute is 0--the current map. Ignore what the
      // user specified.
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      list_next( &i );
      while ( ! list_end( &i ) ) {
         c_push_expr( codegen, list_data( &i ) );
         list_next( &i );
      }
   }
   if ( named_impl ) {
      c_pcd( codegen, PCD_CALLFUNC, list_size( &call->args ), 39 );
      c_pcd( codegen, PCD_DROP );
      c_pcd( codegen, PCD_SCRIPTWAITNAMED );
   }
   else {
      c_pcd( codegen, g_aspec_code[ list_size( &call->args ) - 1 ], 80 );
      c_pcd( codegen, PCD_SCRIPTWAIT );
   }
}


void visit_primary( struct codegen* codegen, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_LITERAL:
      visit_literal( codegen, result,
         ( struct literal* ) node );
      break;
   case NODE_INDEXED_STRING_USAGE:
      visit_indexed_string_usage( codegen, result,
         ( struct indexed_string_usage* ) node );
      break;
   case NODE_BOOLEAN:
      visit_boolean( codegen, result,
         ( struct boolean* ) node );
      break;
   case NODE_NAME_USAGE:
      visit_name_usage( codegen, result,
         ( struct name_usage* ) node );
      break;
   case NODE_STRCPY:
      write_strcpy( codegen, result,
         ( struct strcpy_call* ) node );
      break;
   case NODE_PAREN:
      visit_paren( codegen, result,
         ( struct paren* ) node );
      break;
   default:
      break;
   }
}

void visit_literal( struct codegen* codegen, struct result* result,
   struct literal* literal ) {
   c_pcd( codegen, PCD_PUSHNUMBER, literal->value );
   result->pushed = true;
}

void visit_indexed_string_usage( struct codegen* codegen,
   struct result* result, struct indexed_string_usage* usage ) {
   c_pcd( codegen, PCD_PUSHNUMBER, usage->string->index );
   // Strings in a library need to be tagged.
   if ( codegen->task->library_main->importable ) {
      c_pcd( codegen, PCD_TAGSTRING );
   }
   usage->string->used = true;
   result->pushed = true;
}

void visit_boolean( struct codegen* codegen, struct result* result,
   struct boolean* boolean ) {
   c_pcd( codegen, PCD_PUSHNUMBER, boolean->value );
   result->pushed = true;
}

void visit_name_usage( struct codegen* codegen, struct result* result,
   struct name_usage* usage ) {
   switch ( usage->object->type ) {
   case NODE_CONSTANT:
      visit_constant( codegen, result,
         ( struct constant* ) usage->object );
      break;
   case NODE_ENUMERATOR:
      visit_enumerator( codegen, result,
         ( struct enumerator* ) usage->object );
      break;
   case NODE_VAR:
      visit_var( codegen, result,
         ( struct var* ) usage->object );
      break;
   case NODE_PARAM:
      visit_param( codegen, result,
         ( struct param* ) usage->object );
      break;
   case NODE_FUNC:
      visit_func( codegen, result,
         ( struct func* ) usage->object );
      break;
   default:
      break;
   }
}

void visit_constant( struct codegen* codegen, struct result* result,
   struct constant* constant ) {
   c_pcd( codegen, PCD_PUSHNUMBER, constant->value );
   if ( codegen->task->library_main->importable && constant->value_node &&
      constant->value_node->has_str ) {
      c_pcd( codegen, PCD_TAGSTRING );
   }
   result->pushed = true;
}

void visit_enumerator( struct codegen* codegen, struct result* result,
   struct enumerator* enumerator ) {
   c_pcd( codegen, PCD_PUSHNUMBER, enumerator->value );
   result->pushed = true;
}

void visit_var( struct codegen* codegen, struct result* result,
   struct var* var ) {
   set_var( codegen, result, var );
   // For element-based variables, an index marking the start of the variable
   // data needs to be on the stack.
   if ( result->method == METHOD_ELEMENT ) {
      c_pcd( codegen, PCD_PUSHNUMBER, result->base );
   }
   else {
      if ( result->action == ACTION_PUSH_VALUE ) {
         push_indexed( codegen, result->storage, result->index );
         result->pushed = true;
      }
   }
}

void set_var( struct codegen* codegen, struct result* result, struct var* var ) {
   result->type = var->structure;
   result->dim = var->dim;
   result->storage = var->storage;
   if ( ! var->structure->primitive || var->dim ) {
      result->method = METHOD_ELEMENT;
      result->index = var->index;
   }
   else {
      result->method = METHOD_INDEXED;
      result->index = var->index;
   }
}

void visit_param( struct codegen* codegen, struct result* result,
   struct param* param ) {
   if ( result->action == ACTION_PUSH_VALUE ) {
      push_indexed( codegen, STORAGE_LOCAL, param->index );
      result->pushed = true;
   }
   else {
      result->storage = STORAGE_LOCAL;
      result->index = param->index;
   }
}

void visit_func( struct codegen* codegen, struct result* result,
   struct func* func ) {
   if ( func->type == FUNC_ASPEC ) {
      struct func_aspec* impl = func->impl;
      c_pcd( codegen, PCD_PUSHNUMBER, impl->id );
   }
}

void write_strcpy( struct codegen* codegen, struct result* result,
   struct strcpy_call* call ) {
   struct result object;
   init_result( &object );
   object.action = ACTION_PUSH_VAR;
   visit_operand( codegen, &object, call->array->root );
   c_pcd( codegen, PCD_PUSHNUMBER, object.index );
   // Offset within the array.
   if ( call->array_offset ) {
      c_push_expr( codegen, call->array_offset );
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   }
   // Number of characters to copy from the string.
   if ( call->array_length ) {
      c_push_expr( codegen, call->array_length );
   }
   else {
      enum { MAX_LENGTH = INT_MAX };
      c_pcd( codegen, PCD_PUSHNUMBER, MAX_LENGTH );
   }
   // String field.
   c_push_expr( codegen, call->string );
   // String-offset field.
   if ( call->offset ) {
      c_push_expr( codegen, call->offset );
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   }
   int code = PCD_STRCPYTOMAPCHRANGE;
   switch ( object.storage ) {
   case STORAGE_WORLD:
      code = PCD_STRCPYTOWORLDCHRANGE;
      break;
   case STORAGE_GLOBAL:
      code = PCD_STRCPYTOGLOBALCHRANGE;
      break;
   default:
      break;
   }
   c_pcd( codegen, code );
   result->pushed = true;
}

void visit_paren( struct codegen* codegen, struct result* result,
   struct paren* paren ) {
   visit_operand( codegen, result, paren->inside );
}

void push_indexed( struct codegen* codegen, int storage, int index ) {
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
   c_pcd( codegen, code, index );
}

void push_element( struct codegen* codegen, int storage, int index ) {
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
   c_pcd( codegen, code, index );
}

void c_update_indexed( struct codegen* codegen, int storage, int index,
   int op ) {
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
   c_pcd( codegen, code[ op * 4 + pos ], index );
}

void c_update_element( struct codegen* codegen, int storage, int index,
   int op ) {
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
   c_pcd( codegen, code[ op * 3 + pos ], index );
}