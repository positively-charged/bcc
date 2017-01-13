#include <string.h>

#include "codegen/phase.h"
#include "phase.h"

struct result {
   struct func* func;
   struct var* data_origin;
   struct ref* ref;
   struct enumeration* enumeration;
   struct structure* structure;
   struct dim* dim;
   struct object* object;
   int ref_dim;
   int spec;
   int storage;
   int value;
   bool complete;
   bool usable;
   bool modifiable;
   bool folded;
   bool in_paren;
   bool null;
};

struct call_test {
   struct func* func;
   struct call* call;
   struct param* params;
   int min_param;
   int max_param;
   int num_args;
   bool format_param;
};

static void test_root( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct expr* expr );
static void test_nested_root( struct semantic* semantic,
   struct expr_test* parent, struct expr_test* test, struct result* result,
   struct expr* expr );
static void init_result( struct result* result );
static void test_operand( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_binary( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct binary* binary );
static bool perform_bop( struct semantic* semantic, struct binary* binary,
   struct type_info* lside, struct type_info* rside, struct result* result );
static void fold_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result );
static void fold_bop_int( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside );
static void fold_bop_fixed( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside );
static void fold_bop_bool( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside );
static void fold_bop_str( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside );
static void test_logical( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct logical* logical );
static bool perform_logical( struct semantic* semantic,
   struct logical* logical, struct result* lside, struct result* rside,
   struct result* result );
static bool can_convert_to_boolean( struct result* operand );
static void fold_logical( struct semantic* semantic, struct logical* logical,
   struct result* lside, struct result* rside, struct result* result );
static void test_assign( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct assign* assign );
static bool perform_assign( struct assign* assign, struct result* lside,
   struct type_info* lside_type, struct result* result );
static void test_conditional( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct conditional* cond );
static void test_prefix( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_unary( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct unary* unary );
static bool perform_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand, struct result* result );
static void fold_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand, struct result* result );
static void fold_unary_minus( struct result* operand, struct result* result );
static void fold_logical_not( struct semantic* semantic,
   struct result* operand, struct result* result );
static void test_inc( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct inc* inc );
static bool perform_inc( struct semantic* semantic, struct inc* inc,
   struct result* operand, struct result* result );
static void test_cast( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct cast* cast );
static bool valid_cast( struct semantic* semantic, struct cast* cast,
   struct result* operand );
static void invalid_cast( struct semantic* semantic, struct cast* cast,
   struct result* operand );
static void test_suffix( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_subscript( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct subscript* subscript );
static void test_subscript_array( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct result* lside,
   struct subscript* subscript );
static void warn_bounds_violation( struct semantic* semantic,
   struct subscript* subscript, const char* subject, int upper_bound );
static void test_subscript_str( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct result* lside,
   struct subscript* subscript );
static bool is_array_ref( struct result* result );
static void test_access( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct access* access );
static struct object* access_object( struct semantic* semantic,
   struct access* access, struct result* lside );
static bool is_array( struct result* result );
static bool is_struct( struct result* result );
static void unknown_member( struct semantic* semantic, struct access* access,
   struct result* lside );
static void test_call( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct call* call );
static void init_call_test( struct call_test* test, struct call* call );
static void test_call_operand( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test, struct call* call,
   struct result* operand );
static void test_call_format_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test, struct call* call );
static void test_call_args( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test );
static void test_format_item_list( struct semantic* semantic,
   struct expr_test* expr_test, struct format_item* item );
static void test_format_item( struct semantic* semantic,
   struct expr_test* test, struct format_item* item );
static void test_array_format_item( struct semantic* semantic,
   struct expr_test* expr_test, struct format_item* item );
static void test_int_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct expr* arg );
static void test_msgbuild_format_item( struct semantic* semantic,
   struct expr_test* expr_test, struct format_item* item );
static void test_msgbuild_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct format_item* item,
   struct format_item_msgbuild* extra );
static void add_nested_call( struct func* func, struct call* call );
static void test_remaining_args( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test );
static void test_remaining_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test, struct param* param,
   struct expr* expr );
static void arg_mismatch( struct semantic* semantic, struct pos* pos,
   struct type_info* type, const char* from, struct type_info* required_type,
   const char* where, int number );
static void present_func( struct call_test* test, struct str* msg );
static void test_call_func( struct semantic* semantic, struct call_test* test,
   struct call* call );
static void test_sure( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct sure* sure );
static void test_primary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_literal( struct semantic* semantic, struct result* result,
   struct literal* literal );
static void test_fixed_literal( struct semantic* semantic,
   struct result* result, struct fixed_literal* literal );
static void test_string_usage( struct semantic* semantic,
   struct expr_test* test, struct result* result,
   struct indexed_string_usage* usage );
static void test_boolean( struct semantic* semantic, struct result* result,
   struct boolean* boolean );
static void test_name_usage( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage );
static void test_found_object( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct name_usage* usage,
   struct object* object );
static struct ref* find_map_ref( struct ref* ref,
   struct structure* structure );
static struct ref* find_map_ref_struct( struct structure* structure );
static void test_funcname( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage );
static void test_script_id( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage );
static void select_object( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct object* object );
static void select_constant( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct constant* constant );
static void select_enumerator( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct enumerator* enumerator );
static void select_var( struct semantic* semantic, struct result* result,
   struct var* var );
static void select_param( struct semantic* semantic, struct result* result,
   struct param* param );
static void select_member( struct semantic* semantic, struct result* result,
   struct structure_member* member );
static void select_func( struct semantic* semantic, struct result* result,
   struct func* func );
static void select_alias( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct alias* alias );
static void test_strcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct strcpy_call* call );
static void test_memcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct memcpy_call* call );
static bool is_onedim_intelem_array_ref( struct semantic* semantic,
   struct result* result );
static bool is_int_value( struct semantic* semantic, struct result* result );
static bool is_str_value( struct semantic* semantic, struct result* result );
static void test_conversion( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct conversion* conv );
static bool perform_conversion( struct conversion* conv,
   struct type_info* from );
static void unsupported_conversion( struct semantic* semantic,
   struct type_info* from, int to_spec, struct pos* pos );
static void test_compound_literal( struct semantic* semantic,
   struct expr_test* test, struct result* result,
   struct compound_literal* literal );
static void test_anon_func( struct semantic* semantic, struct result* result,
   struct func* func );
static void test_paren( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct paren* paren );
static void test_upmost( struct semantic* semantic, struct result* result );
static void test_current_namespace( struct semantic* semantic,
   struct result* result );
static void init_type_info( struct semantic* semantic, struct type_info* type,
   struct result* result );
static bool is_value_type( struct semantic* semantic, struct result* result );
static bool is_ref_type( struct result* result );

void s_init_expr_test( struct expr_test* test, bool result_required,
   bool suggest_paren_assign ) {
   test->name_offset = NULL;
   test->var = NULL;
   test->func = NULL;
   test->result_required = result_required;
   test->has_str = false;
   test->undef_erred = false;
   test->suggest_paren_assign = suggest_paren_assign;
}

void s_init_expr_test_enumerator( struct expr_test* test,
   struct enumeration* enumeration ) {
   s_init_expr_test( test, true, false );
   if ( enumeration->name ) {
      test->name_offset = enumeration->body;
   }
}

void s_test_expr( struct semantic* semantic, struct expr_test* test,
   struct expr* expr ) {
   if ( setjmp( test->bail ) == 0 ) {
      struct result result;
      init_result( &result );
      test_root( semantic, test, &result, expr );
   }
   else {
      test->undef_erred = true;
   }
}

void s_test_expr_type( struct semantic* semantic, struct expr_test* test,
   struct type_info* result_type, struct expr* expr ) {
   if ( setjmp( test->bail ) == 0 ) {
      struct result result;
      init_result( &result );
      test_root( semantic, test, &result, expr );
      init_type_info( semantic, result_type, &result );
      s_decay( semantic, result_type );
   }
   else {
      test->undef_erred = true;
   }
}

void s_test_bool_expr( struct semantic* semantic, struct expr* expr ) {
   struct expr_test test;
   s_init_expr_test( &test, true, true );
   struct result result;
   init_result( &result );
   test_root( semantic, &test, &result, expr );
   if ( ! can_convert_to_boolean( &result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &expr->pos,
         "expression cannot be converted to a boolean value" );
      s_bail( semantic );
   }
}

void test_root( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct expr* expr ) {
   test_operand( semantic, test, result, expr->root );
   if ( ! result->complete ) {
      s_diag( semantic, DIAG_POS_ERR, &expr->pos,
         "expression incomplete" );
      s_bail( semantic );
   }
   if ( test->result_required && ! result->usable ) {
      s_diag( semantic, DIAG_POS_ERR, &expr->pos,
         "expression does not produce a value" );
      s_bail( semantic );
   }
   test->var = result->data_origin;
   test->func = result->func;
   expr->spec = result->spec;
   expr->folded = result->folded;
   expr->value = result->value;
   expr->has_str = test->has_str;
}

void test_nested_root( struct semantic* semantic, struct expr_test* parent,
   struct expr_test* test, struct result* result, struct expr* expr ) {
   if ( setjmp( test->bail ) == 0 ) {
      test_root( semantic, test, result, expr );
      parent->has_str = test->has_str;
   }
   else {
      longjmp( parent->bail, 1 );
   }
}

void test_nested_expr( struct semantic* semantic, struct expr_test* parent,
   struct result* result, struct expr* expr ) {
   struct expr_test test;
   s_init_expr_test( &test, true, false );
   test_nested_root( semantic, parent, &test, result, expr );
}

void init_result( struct result* result ) {
   result->func = NULL;
   result->data_origin = NULL;
   result->ref = NULL;
   result->enumeration = NULL;
   result->structure = NULL;
   result->dim = NULL;
   result->object = NULL;
   result->ref_dim = 0;
   result->spec = SPEC_NONE;
   result->storage = STORAGE_LOCAL;
   result->value = 0;
   result->complete = false;
   result->usable = false;
   result->modifiable = false;
   result->folded = false;
   result->in_paren = false;
   result->null = false;
}

void test_operand( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node ) {
   switch ( node->type ) {
   case NODE_BINARY:
      test_binary( semantic, test, result,
         ( struct binary* ) node );
      break;
   case NODE_LOGICAL:
      test_logical( semantic, test, result,
         ( struct logical* ) node );
      break;
   case NODE_ASSIGN:
      test_assign( semantic, test, result,
         ( struct assign* ) node );
      break;
   case NODE_CONDITIONAL:
      test_conditional( semantic, test, result,
         ( struct conditional* ) node );
      break;
   default:
      test_prefix( semantic, test, result,
         node );
      break;
   }
}

void test_binary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct binary* binary ) {
   struct result lside;
   init_result( &lside );
   test_operand( semantic, test, &lside, binary->lside );
   if ( ! lside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &binary->pos,
         "left operand unusable" );
      s_bail( semantic );
   }
   struct result rside;
   init_result( &rside );
   test_operand( semantic, test, &rside, binary->rside );
   if ( ! rside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &binary->pos,
         "right operand unusable" );
      s_bail( semantic );
   }
   struct type_info lside_type;
   struct type_info rside_type;
   init_type_info( semantic, &lside_type, &lside );
   init_type_info( semantic, &rside_type, &rside );
   s_decay( semantic, &lside_type );
   s_decay( semantic, &rside_type );
   if ( ! s_same_type( &lside_type, &rside_type ) ) {
      s_type_mismatch( semantic, "left-operand", &lside_type,
         "right-operand", &rside_type, &binary->pos );
      s_bail( semantic );
   }
   if ( ! perform_bop( semantic, binary, &lside_type, &rside_type, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &binary->pos,
         "invalid binary operation" );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( lside.folded && rside.folded ) {
      fold_bop( semantic, binary, &lside, &rside, result );
   }
}

bool perform_bop( struct semantic* semantic, struct binary* binary,
   struct type_info* lside, struct type_info* rside, struct result* result ) {
   // Value type.
   if ( s_is_value_type( lside ) ) {
      int spec = SPEC_NONE;
      switch ( binary->op ) {
      case BOP_EQ:
      case BOP_NEQ:
         switch ( lside->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
         case SPEC_BOOL:
         case SPEC_STR:
            spec = s_spec( semantic, SPEC_BOOL );
            break;
         default:
            break;
         }
         break;
      case BOP_BIT_OR:
      case BOP_BIT_XOR:
      case BOP_BIT_AND:
      case BOP_SHIFT_L:
      case BOP_SHIFT_R:
      case BOP_MOD:
         switch ( lside->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
            spec = lside->spec;
            break;
         default:
            break;
         }
         break;
      case BOP_LT:
      case BOP_LTE:
      case BOP_GT:
      case BOP_GTE:
         switch ( lside->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
         case SPEC_STR:
            spec = s_spec( semantic, SPEC_BOOL );
            break;
         default:
            break;
         }
         break;
      case BOP_ADD:
         switch ( lside->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
         case SPEC_STR:
            spec = lside->spec;
            break;
         default:
            break;
         }
         break;
      case BOP_SUB:
      case BOP_MUL:
      case BOP_DIV:
         switch ( lside->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
            spec = lside->spec;
            break;
         default:
            break;
         }
         break;
      default:
         break;
      }
      if ( spec == SPEC_NONE ) {
         return false;
      }
      result->spec = spec;
      result->complete = true;
      result->usable = true;
      binary->operand_type = lside->spec;
      if ( lside->spec == SPEC_RAW || rside->spec == SPEC_RAW ) {
         result->spec = SPEC_RAW;
         binary->operand_type = SPEC_RAW;
      }
      return true;
   }
   // Reference type.
   else {
      switch ( binary->op ) {
      case BOP_EQ:
      case BOP_NEQ:
         binary->operand_type = ( lside->ref->type == REF_FUNCTION ||
            rside->ref->type == REF_FUNCTION ) ? BINARYOPERAND_REFFUNC :
            BINARYOPERAND_REF;
         result->spec = s_spec( semantic, SPEC_BOOL );
         result->complete = true;
         result->usable = true;
         return true;
      default:
         return false;
      }
   }
}

void fold_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result ) {
   if ( is_value_type( semantic, lside ) ) {
      switch ( lside->spec ) {
      case SPEC_INT:
      case SPEC_RAW:
         fold_bop_int( semantic, binary, lside, rside );
         break;
      case SPEC_FIXED:
         fold_bop_fixed( semantic, binary, lside, rside );
         break;
      case SPEC_BOOL:
         fold_bop_bool( semantic, binary, lside, rside );
         break;
      case SPEC_STR:
         fold_bop_str( semantic, binary, lside, rside );
         break;
      default:
         break;
      }
      result->value = binary->value;
      result->folded = binary->folded;
   }
}

void fold_bop_int( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside ) {
   int l = lside->value;
   int r = rside->value;
   // Division and modulo get special treatment because of the possibility
   // of a division by zero.
   if ( ( binary->op == BOP_DIV || binary->op == BOP_MOD ) && r == 0 ) {
      s_diag( semantic, DIAG_POS_ERR, &binary->pos,
         "division by zero" );
      s_bail( semantic );
   }
   switch ( binary->op ) {
   case BOP_MOD: l %= r; break;
   case BOP_MUL: l *= r; break;
   case BOP_DIV: l /= r; break;
   case BOP_ADD: l += r; break;
   case BOP_SUB: l -= r; break;
   case BOP_SHIFT_R: l >>= r; break;
   case BOP_SHIFT_L: l <<= r; break;
   case BOP_GTE: l = l >= r; break;
   case BOP_GT: l = l > r; break;
   case BOP_LTE: l = l <= r; break;
   case BOP_LT: l = l < r; break;
   case BOP_NEQ: l = l != r; break;
   case BOP_EQ: l = l == r; break;
   case BOP_BIT_AND: l = l & r; break;
   case BOP_BIT_XOR: l = l ^ r; break;
   case BOP_BIT_OR: l = l | r; break;
   default: break;
   }
   binary->value = l;
   binary->folded = true;
}

void fold_bop_fixed( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside ) {
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
   case BOP_LT:
   case BOP_LTE:
   case BOP_GT:
   case BOP_GTE:
   case BOP_ADD:
   case BOP_SUB:
      break;
   case BOP_DIV:
   case BOP_MUL:
      // TODO.
      return;
   default:
      return;
   }
   int l = lside->value;
   int r = rside->value;
   switch ( binary->op ) {
   case BOP_EQ: l = ( l == r ); break;
   case BOP_NEQ: l = ( l != r ); break;
   case BOP_LT: l = ( l < r ); break;
   case BOP_LTE: l = ( l <= r ); break;
   case BOP_GT: l = ( l > r ); break;
   case BOP_GTE: l = ( l >= r ); break;
   case BOP_ADD: l += r; break;
   case BOP_SUB: l -= r; break;
   default:
      break;
   }
   binary->value = l;
   binary->folded = true;
}

void fold_bop_bool( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside ) {
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
      break;
   default:
      return;
   }
   int l = lside->value;
   int r = rside->value;
   switch ( binary->op ) {
   case BOP_EQ: l = ( l == r ); break;
   case BOP_NEQ: l = ( l != r ); break;
   default:
      break;
   }
   binary->value = l;
   binary->folded = true;
}

void fold_bop_str( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside ) {
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
      break;
   case BOP_LT:
   case BOP_LTE:
   case BOP_GT:
   case BOP_GTE:
      // TODO.
      return;
   default:
      return;
   }
   int l = lside->value;
   int r = rside->value;
   switch ( binary->op ) {
   case BOP_EQ: l = ( l == r ); break;
   case BOP_NEQ: l = ( l != r ); break;
   default:
      break;
   }
   binary->value = l;
   binary->folded = true;
}

void test_logical( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct logical* logical ) {
   struct result lside;
   init_result( &lside );
   test_operand( semantic, test, &lside, logical->lside );
   if ( ! lside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &logical->pos,
         "left operand unusable" );
      s_bail( semantic );
   }
   struct result rside;
   init_result( &rside );
   test_operand( semantic, test, &rside, logical->rside );
   if ( ! rside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &logical->pos,
         "right operand unusable" );
      s_bail( semantic );
   }
   if ( ! perform_logical( semantic, logical, &lside, &rside, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &logical->pos,
         "invalid logical operation" );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( lside.folded && rside.folded ) {
      fold_logical( semantic, logical, &lside, &rside, result );
   }
}

bool perform_logical( struct semantic* semantic, struct logical* logical,
   struct result* lside, struct result* rside, struct result* result ) {
   if ( can_convert_to_boolean( lside ) && can_convert_to_boolean( rside ) ) {
      result->spec = s_spec( semantic, SPEC_BOOL );
      result->complete = true;
      result->usable = true;
      logical->lside_spec = lside->spec;
      logical->rside_spec = rside->spec;
      return true;
   }
   return false;
}

// NOTE: Im not sure if this function is necessary, since every usable result
// can be converted to a boolean.
bool can_convert_to_boolean( struct result* operand ) {
   if ( is_ref_type( operand ) ) {
      return true;
   }
   else {
      switch ( operand->spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_BOOL:
      case SPEC_STR:
         return true;
      default:
         return false;
      }
   }
}

void fold_logical( struct semantic* semantic, struct logical* logical,
   struct result* lside, struct result* rside, struct result* result ) {
   if ( is_value_type( semantic, lside ) ) {
      int l = 0;
      switch ( lside->spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_BOOL:
      case SPEC_STR:
         l = lside->value;
         break;
      default:
         UNREACHABLE();
      }
      int r = 0;
      switch ( rside->spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_BOOL:
      case SPEC_STR:
         r = rside->value;
         break;
      default:
         UNREACHABLE();
      }
      switch ( logical->op ) {
      case LOP_OR: l = ( l || r ); break;
      case LOP_AND: l = ( l && r ); break;
      default:
         UNREACHABLE();
      }
      logical->value = l;
      logical->folded = true;
   }
   result->value = logical->value;
   result->folded = logical->folded;
}

void test_assign( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct assign* assign ) {
   struct result lside;
   init_result( &lside );
   test_operand( semantic, test, &lside, assign->lside );
   if ( ! lside.modifiable ) {
      s_diag( semantic, DIAG_POS_ERR, &assign->pos,
         "cannot assign to left operand" );
      s_bail( semantic );
   }
   struct result rside;
   init_result( &rside );
   test_operand( semantic, test, &rside, assign->rside );
   if ( ! rside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &assign->pos,
         "right operand unusable" );
      s_bail( semantic );
   }
   struct type_info lside_type;
   struct type_info rside_type;
   init_type_info( semantic, &lside_type, &lside );
   init_type_info( semantic, &rside_type, &rside );
   s_decay( semantic, &rside_type );
   if ( ! s_instance_of( &lside_type, &rside_type ) ) {
      s_type_mismatch( semantic, "left-operand", &lside_type,
         "right-operand", &rside_type, &assign->pos );
      s_bail( semantic );
   }
   if ( rside.func && rside.func->type == FUNC_USER ) {
      struct func_user* impl = rside.func->impl;
      if ( impl->local ) {
         s_diag( semantic, DIAG_POS_ERR, &assign->pos,
            "assigning a local function" );
         s_bail( semantic );
      }
   }
   if ( ! perform_assign( assign, &lside, &lside_type, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &assign->pos,
         "invalid assignment operation" );
      s_bail( semantic );
   }
   if ( is_ref_type( &lside ) && rside.data_origin ) {
      rside.data_origin->addr_taken = true;
      if ( ! rside.data_origin->hidden ) {
         s_diag( semantic, DIAG_POS_ERR, &assign->pos,
            "non-private right operand (references only work with private map "
            "variables)" );
         s_bail( semantic );
      }
   }
   // Record the fact that the object was modified.
   if ( lside.object ) {
      switch ( lside.object->node.type ) {
      case NODE_VAR: {
            struct var* var = ( struct var* ) lside.object;
            var->modified = true;
         }
         break;
      default:
         break;
      }
   }
   // To avoid the error where the user wanted equality operator but instead
   // typed in the assignment operator, suggest that assignment be wrapped in
   // parentheses.
   if ( test->suggest_paren_assign && ! result->in_paren ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS, &assign->pos,
         "assignment operation not in parentheses" );
   }
   assign->spec = lside.spec;
}

bool perform_assign( struct assign* assign, struct result* lside,
   struct type_info* lside_type, struct result* result ) {
   // Value type.
   if ( s_is_value_type( lside_type ) ) {
      bool valid = false;
      switch ( assign->op ) {
      case AOP_NONE:
         switch ( lside->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
         case SPEC_BOOL:
         case SPEC_STR:
         case SPEC_ENUM:
            valid = true;
            break;
         default:
            break;
         }
         break;
      case AOP_ADD:
         switch ( lside->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
         case SPEC_STR:
            valid = true;
            break;
         default:
            break;
         }
         break;
      case AOP_SUB:
      case AOP_MUL:
      case AOP_DIV:
         switch ( lside->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
            valid = true;
            break;
         default:
            break;
         }
         break;
      case AOP_MOD:
      case AOP_SHIFT_L:
      case AOP_SHIFT_R:
      case AOP_BIT_AND:
      case AOP_BIT_XOR:
      case AOP_BIT_OR:
         switch ( lside->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
            valid = true;
            break;
         default:
            break;
         }
         break;
      default:
         break;
      }
      if ( ! valid ) {
         return false;
      }
      result->spec = lside->spec;
      result->complete = true;
      result->usable = true;
      return true;
   }
   // Reference type.
   else {
      // Only plain assignment can be performed on a reference type.
      if ( assign->op != AOP_NONE ) {
         return false;
      }
      result->ref = lside->ref;
      result->structure = lside->structure;
      result->spec = lside->spec;
      result->complete = true;
      result->usable = true;
      if ( result->ref && result->ref->type == REF_ARRAY ) {
         struct ref_array* part = ( struct ref_array* ) result->ref;
         result->ref_dim = part->dim_count;
      }
      return true;
   }
}

void test_conditional( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct conditional* cond ) {
   struct result left;
   init_result( &left );
   test_operand( semantic, test, &left, cond->left );
   if ( ! left.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &cond->pos,
         "left operand not a value" );
      s_bail( semantic );
   }
   if ( ! can_convert_to_boolean( &left ) ) {
      s_diag( semantic, DIAG_POS_ERR, &cond->pos,
         "left operand cannot be converted to a boolean value" );
      s_bail( semantic );
   }
   struct result middle = left;
   if ( cond->middle ) {
      init_result( &middle );
      test_operand( semantic, test, &middle, cond->middle );
   }
   if ( middle.func && middle.func->type == FUNC_USER ) {
      struct func_user* impl = middle.func->impl;
      if ( impl->local ) {
         s_diag( semantic, DIAG_POS_ERR, &cond->pos,
            "%s a local function", cond->middle ?
            "middle operand" : "left operand" );
         s_bail( semantic );
      }
   }
   struct result right;
   init_result( &right );
   test_operand( semantic, test, &right, cond->right );
   if ( right.func && right.func->type == FUNC_USER ) {
      struct func_user* impl = right.func->impl;
      if ( impl->local ) {
         s_diag( semantic, DIAG_POS_ERR, &cond->pos,
            "right operand a local function" );
         s_bail( semantic );
      }
   }
   struct type_info middle_type;
   init_type_info( semantic, &middle_type, &middle );
   s_decay( semantic, &middle_type );
   struct type_info right_type;
   init_type_info( semantic, &right_type, &right );
   s_decay( semantic, &right_type );
   if ( ! s_same_type( &middle_type, &right_type ) ) {
      s_type_mismatch( semantic, cond->middle ?
         "middle-operand" : "left-operand", &middle_type, "right-operand",
         &right_type, &cond->pos );
      s_bail( semantic );
   }
   struct type_snapshot snapshot;
   if ( ! middle.null ) {
      s_take_type_snapshot( &middle_type, &snapshot );
   }
   else {
      s_take_type_snapshot( &right_type, &snapshot );
   }
   result->ref = snapshot.ref;
   result->structure = snapshot.structure;
   result->enumeration = snapshot.enumeration;
   result->spec = snapshot.spec;
   result->complete = true;
   result->usable = ( snapshot.spec != SPEC_VOID );
   cond->ref = snapshot.ref;
   cond->left_spec = left.spec;
   if ( is_ref_type( &middle ) ) {
      if ( middle.data_origin ) {
         middle.data_origin->addr_taken = true;
         if ( ! middle.data_origin->hidden ) {
            s_diag( semantic, DIAG_POS_ERR, &cond->pos,
               "non-private %s (references only work with private "
               "map variables)", cond->middle ? "middle operand" :
               "left operand" );
            s_bail( semantic );
         }
      }
      if ( right.data_origin ) {
         right.data_origin->addr_taken = true;
         if ( ! right.data_origin->hidden ) {
            s_diag( semantic, DIAG_POS_ERR, &cond->pos,
               "non-private right operand (references only work with private "
               "map variables)" );
            s_bail( semantic );
         }
      }
   }
   // Compile-time evaluation.
   if ( left.folded && middle.folded && right.folded ) {
      if ( is_value_type( semantic, &left ) &&
         is_value_type( semantic, &right ) ) {
         result->value = ( left.value != 0 ) ? middle.value : right.value;
         result->folded = true;
         cond->left_value = left.value;
         cond->folded = true;
      }
   }
}

void test_prefix( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node ) {
   switch ( node->type ) {
   case NODE_UNARY:
      test_unary( semantic, test, result,
         ( struct unary* ) node );
      break;
   case NODE_INC:
      test_inc( semantic, test, result,
         ( struct inc* ) node );
      break;
   case NODE_CAST:
      test_cast( semantic, test, result,
         ( struct cast* ) node );
      break;
   default:
      test_suffix( semantic, test, result,
         node );
      break;
   }
}

void test_unary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct unary* unary ) {
   struct result operand;
   init_result( &operand );
   test_operand( semantic, test, &operand, unary->operand );
   if ( ! operand.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &unary->pos,
         "operand not a value" );
      s_bail( semantic );
   }
   if ( ! perform_unary( semantic, unary, &operand, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &unary->pos,
         "invalid unary operation" );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( operand.folded ) {
      fold_unary( semantic, unary, &operand, result );
   }
}

bool perform_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand, struct result* result ) {
   // Value type.
   if ( is_value_type( semantic, operand ) ) {
      int spec = SPEC_NONE;
      switch ( unary->op ) {
      case UOP_MINUS:
      case UOP_PLUS:
         switch ( operand->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
            spec = operand->spec;
         default:
            break;
         }
         break;
      case UOP_BIT_NOT:
         switch ( operand->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
            spec = operand->spec;
         default:
            break;
         }
         break;
      case UOP_LOG_NOT:
         spec = s_spec( semantic, SPEC_BOOL );
         break;
      default:
         UNREACHABLE()
      }
      if ( spec != SPEC_NONE ) {
         result->spec = spec;
         result->complete = true;
         result->usable = true;
         unary->operand_spec = operand->spec;
         return true;
      }
      else {
         return false;
      }
   }
   // Reference type.
   else {
      // Only logical-not can be performed on a reference type.
      if ( unary->op == UOP_LOG_NOT ) {
         result->spec = s_spec( semantic, SPEC_BOOL );
         result->complete = true;
         result->usable = true;
         return true;
      }
      else {
         return false;
      }
   }
}

void fold_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand, struct result* result ) {
   switch ( unary->op ) {
   case UOP_MINUS:
      fold_unary_minus( operand, result );
      break;
   case UOP_PLUS:
      result->value = operand->value;
      result->folded = true;
      break;
   case UOP_LOG_NOT:
      fold_logical_not( semantic, operand, result );
      break;
   case UOP_BIT_NOT:
      result->value = ( ~ operand->value );
      result->folded = true;
      break;
   default:
      UNREACHABLE()
   }
}

void fold_unary_minus( struct result* operand, struct result* result ) {
   switch ( operand->spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
      // TODO: Warn on overflow and underflow.
      result->value = ( - operand->value );
      result->folded = true;
      break;
   default:
      UNREACHABLE()
   }
}

void fold_logical_not( struct semantic* semantic, struct result* operand,
   struct result* result ) {
   switch ( operand->spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
   case SPEC_BOOL:
   case SPEC_STR:
      result->value = ( ! operand->value );
      result->folded = true;
      break;
   default:
      break;
   }
}

void test_inc( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct inc* inc ) {
   struct result operand;
   init_result( &operand );
   test_operand( semantic, test, &operand, inc->operand );
   if ( ! operand.modifiable ) {
      s_diag( semantic, DIAG_POS_ERR, &inc->pos,
         "operand cannot be %s", inc->dec ?
            "decremented" : "incremented" );
      s_bail( semantic );
   }
   if ( ! perform_inc( semantic, inc, &operand, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &inc->pos,
         "invalid %s operation", inc->dec ?
            "decrement" : "increment" );
      s_bail( semantic );
   }
   // Record the fact that the object is modified.
   if ( operand.object ) {
      switch ( operand.object->node.type ) {
      case NODE_VAR: {
            struct var* var = ( struct var* ) operand.object;
            var->modified = true;
         }
         break;
      default:
         break;
      }
   }
}

bool perform_inc( struct semantic* semantic, struct inc* inc,
   struct result* operand,
   struct result* result ) {
   // Value type.
   if ( is_value_type( semantic, operand ) ) {
      switch ( operand->spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
         break;
      default:
         return false;
      }
      result->spec = operand->spec;
      result->complete = true;
      result->usable = true;
      inc->fixed = ( operand->spec == SPEC_FIXED );
      return true;
   }
   // Reference type.
   else {
      return false;
   }
}

void test_cast( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct cast* cast ) {
   struct result operand;
   init_result( &operand );
   test_operand( semantic, test, &operand, cast->operand );
   if ( ! operand.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &cast->pos,
         "cast operand not a value" );
      s_bail( semantic );
   }
   if ( ! valid_cast( semantic, cast, &operand ) ) {
      invalid_cast( semantic, cast, &operand );
      s_bail( semantic );
   }
   result->spec = cast->spec;
   result->complete = true;
   result->usable = true;
   if ( operand.folded ) {
      result->value = operand.value;
      result->folded = true;
   }
}

bool valid_cast( struct semantic* semantic, struct cast* cast,
   struct result* operand ) {
   // Value type.
   if ( is_value_type( semantic, operand ) ) {
      bool valid = false;
      switch ( cast->spec ) {
      case SPEC_RAW:
      case SPEC_INT:
         switch ( operand->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
         case SPEC_BOOL:
         case SPEC_STR:
            valid = true;
            break;
         default:
            break;
         }
         break;
      case SPEC_FIXED:
      case SPEC_BOOL:
      case SPEC_STR:
         switch ( operand->spec ) {
         case SPEC_RAW:
         case SPEC_INT:
            valid = true;
            break;
         default:
            valid = ( cast->spec == operand->spec );
            break;
         }
         break;
      default:
         break;
      }
      return valid;
   }
   // Reference type.
   else {
      return false;
   }
}

// TODO: Change error message.
void invalid_cast( struct semantic* semantic, struct cast* cast,
   struct result* operand ) {
   struct type_info cast_type;
   s_init_type_info( &cast_type, NULL, NULL, NULL, NULL, cast->spec,
      STORAGE_LOCAL );
   struct type_info operand_type;
   init_type_info( semantic, &operand_type, operand );
   struct str cast_type_s;
   str_init( &cast_type_s );
   s_present_type( &cast_type, &cast_type_s );
   struct str operand_type_s;
   str_init( &operand_type_s );
   s_present_type( &operand_type, &operand_type_s );
   s_diag( semantic, DIAG_POS_ERR, &cast->pos,
      "%s operand cannot be cast to %s",
       operand_type_s.value, cast_type_s.value );
   str_deinit( &cast_type_s );
   str_deinit( &operand_type_s );
}

void test_suffix( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node ) {
   switch ( node->type ) {
   case NODE_SUBSCRIPT:
      test_subscript( semantic, test, result,
         ( struct subscript* ) node );
      break;
   case NODE_ACCESS:
      test_access( semantic, test, result,
         ( struct access* ) node );
      break;
   case NODE_CALL:
      test_call( semantic, test, result,
         ( struct call* ) node );
      break;
   case NODE_SURE:
      test_sure( semantic, test, result,
         ( struct sure* ) node );
      break;
   default:
      test_primary( semantic, test, result,
         node );
      break;
   }
}

void test_subscript( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct subscript* subscript ) {
   struct result lside;
   init_result( &lside );
   test_suffix( semantic, test, &lside, subscript->lside );
   if ( is_array_ref( &lside ) ) {
      test_subscript_array( semantic, test, result, &lside, subscript );
   }
   else if ( lside.spec == SPEC_STR ) {
      test_subscript_str( semantic, test, result, &lside, subscript );
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &subscript->pos,
         "subscript operation not supported for given operand" );
      s_bail( semantic );
   }
}

bool is_array_ref( struct result* result ) {
   return ( result->dim || ( result->ref && result->ref->type == REF_ARRAY ) );
}

void test_subscript_array( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct result* lside, struct subscript* subscript ) {
   struct result index;
   init_result( &index );
   test_nested_expr( semantic, test, &index, subscript->index );
   // Index must be of integer type.
   struct type_info type;
   init_type_info( semantic, &type, &index );
   s_decay( semantic, &type );
   if ( ! s_same_type( &type, &semantic->type_int ) ) {
      s_type_mismatch( semantic, "index", &type,
         "required", &semantic->type_int, &subscript->index->pos );
      s_bail( semantic );
   }
   // Out-of-bounds warning for a constant index.
   if ( lside->dim && lside->dim->length && subscript->index->folded ) {
      warn_bounds_violation( semantic, subscript, "dimension-length",
         lside->dim->length );
   }
   // Populate @lside with information about array reference.
   if ( ! lside->dim && lside->ref_dim == 0 ) {
      struct ref_array* array = ( struct ref_array* ) lside->ref;
      lside->ref_dim = array->dim_count;
      if ( lside->ref->nullable ) {
         semantic->lib->uses_nullable_refs = true;
      }
   }
   // Move on to array element:
   // Sub-array.
   if ( ( lside->dim && lside->dim->next ) || lside->ref_dim > 1 ) {
      result->data_origin = lside->data_origin;
      result->ref = lside->ref;
      result->structure = lside->structure;
      result->enumeration = lside->enumeration;
      result->spec = lside->spec;
      result->storage = lside->storage;
      if ( lside->dim ) {
         result->dim = lside->dim->next;
      }
      else {
         result->ref_dim = lside->ref_dim - 1;
      }
      if ( lside->dim && index.folded ) {
         result->value = lside->value + lside->dim->element_size * index.value;
         result->folded = true;
      }
   }
   // Reference element.
   else if ( ( lside->dim && lside->ref ) ||
      ( lside->ref_dim == 1 && lside->ref->next ) ) {
      result->structure = lside->structure;
      result->enumeration = lside->enumeration;
      result->spec = lside->spec;
      if ( lside->dim ) {
         result->ref = lside->ref;
      }
      else {
         result->ref = lside->ref->next;
      }
      result->modifiable = true;
   }
   // Structure element.
   else if ( lside->structure ) {
      result->data_origin = lside->data_origin;
      result->structure = lside->structure;
      result->spec = lside->spec;
      if ( lside->ref ) {
         struct ref_array* array = ( struct ref_array* ) lside->ref;
         result->storage = array->storage;
      }
      else {
         result->storage = lside->storage;
      }
   }
   // Primitive element.
   else {
      result->enumeration = lside->enumeration;
      result->spec = s_spec( semantic, lside->spec );
      result->modifiable = true;
   }
   result->usable = true;
   result->complete = true;
}

void warn_bounds_violation( struct semantic* semantic,
   struct subscript* subscript, const char* subject, int upper_bound ) {
   if ( subscript->index->value < 0 ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS, &subscript->index->pos,
         "index out-of-bounds: index (%d) < 0", subscript->index->value );
   }
   else if ( subscript->index->value >= upper_bound ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS, &subscript->index->pos,
         "index out-of-bounds: index (%d) >= %s (%d)",
         subscript->index->value, subject, upper_bound );
   }
}

void test_subscript_str( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct result* lside, struct subscript* subscript ) {
   struct result index;
   init_result( &index );
   test_nested_expr( semantic, test, &index, subscript->index );
   // Index must be of integer type.
   struct type_info type;
   init_type_info( semantic, &type, &index );
   s_decay( semantic, &type );
   if ( ! s_same_type( &type, &semantic->type_int ) ) {
      s_type_mismatch( semantic, "index", &type,
         "required", &semantic->type_int, &subscript->index->pos );
      s_bail( semantic );
   }
   // Out-of-bounds warning for a constant index.
   if ( lside->folded && subscript->index->folded ) {
      struct indexed_string* string = t_lookup_string( semantic->task,
         lside->value );
      if ( string ) {
         warn_bounds_violation( semantic, subscript, "string-length",
            string->length );
      }
   }
   result->spec = s_spec( semantic, SPEC_INT );
   result->complete = true;
   result->usable = true;
   subscript->string = true;
}

void test_access( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct access* access ) {
   struct result lside;
   init_result( &lside );
   test_suffix( semantic, test, &lside, access->lside );
   struct object* object = access_object( semantic, access, &lside );
   if ( ! object ) {
      unknown_member( semantic, access, &lside );
      s_bail( semantic );
   }
   if ( ! object->resolved && object->node.type != NODE_NAMESPACE ) {
      if ( semantic->trigger_err ) {
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "right operand (`%s`) undefined", access->name );
         s_bail( semantic );
      }
      else {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
   }
   select_object( semantic, test, result, object );
   access->rside = &object->node;
   if ( object->node.type == NODE_STRUCTURE_MEMBER ) {
      if ( lside.ref && ! result->ref &&
         ( result->dim || result->spec == SPEC_STRUCT ) ) {
         struct ref_struct* structure = ( struct ref_struct* ) lside.ref;
         result->storage = structure->storage;
      }
      else {
         result->storage = lside.storage;
      }
      result->data_origin = lside.data_origin;
   }
}

struct object* access_object( struct semantic* semantic, struct access* access,
   struct result* lside ) {
   if ( is_struct( lside ) ) {
      struct name* name = t_extend_name( lside->structure->body,
         access->name );
      if ( lside->ref && lside->ref->nullable ) {
         semantic->lib->uses_nullable_refs = true;
      }
      return name->object;
   }
   else if ( lside->object && lside->object->node.type == NODE_NAMESPACE ) {
      access->type = ACCESS_NAMESPACE;
      return s_get_ns_object( ( struct ns* ) lside->object, access->name,
         NODE_NONE );
   }
   else if ( is_array( lside ) ) {
      access->type = ACCESS_ARRAY;
      struct name* name = t_extend_name( semantic->task->array_name, "." );
      name = t_extend_name( name, access->name );
      if ( lside->ref && lside->ref->nullable ) {
         semantic->lib->uses_nullable_refs = true;
      }
      return name->object;
   }
   else if ( is_value_type( semantic, lside ) && lside->spec == SPEC_STR ) {
      access->type = ACCESS_STR;
      struct name* name = t_extend_name( semantic->task->str_name, "." );
      name = t_extend_name( name, access->name );
      return name->object;
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &access->pos,
         "left operand does not support member access" );
      s_bail( semantic );
      return NULL;
   }
}

bool is_array( struct result* result ) {
   return ( result->dim || ( result->ref && result->ref->type == REF_ARRAY ) );
}

bool is_struct( struct result* result ) {
   return ( ! result->dim && (
      ( result->ref && result->ref->type == REF_STRUCTURE ) ||
      ( ! result->ref && result->spec == SPEC_STRUCT ) ) );
}

void unknown_member( struct semantic* semantic, struct access* access,
   struct result* lside ) {
   if ( is_struct( lside ) ) {
      if ( lside->structure->anon ) {
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "`%s` not a member of anonymous struct", access->name );
      }
      else {
         struct str str;
         str_init( &str );
         t_copy_name( lside->structure->name, false, &str );
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "`%s` not a member of struct `%s`", access->name,
            str.value );
         str_deinit( &str );
      }
   }
   else if ( lside->object && lside->object->node.type == NODE_NAMESPACE ) {
      s_unknown_ns_object( semantic, ( struct ns* ) lside->object,
         access->name, &access->pos );
   }
   else if ( is_array( lside ) ) {
      s_diag( semantic, DIAG_POS_ERR, &access->pos,
         "`%s` not a property of an array", access->name );
   }
   else if ( is_value_type( semantic, lside ) && lside->spec == SPEC_STR ) {
      s_diag( semantic, DIAG_POS_ERR, &access->pos,
         "`%s` not a member of `str` type", access->name );
   }
   else {
      UNREACHABLE();
   }
}

void s_unknown_ns_object( struct semantic* semantic, struct ns* ns,
   const char* object_name, struct pos* pos ) {
   if ( ns == semantic->task->upmost_ns ) {
      s_diag( semantic, DIAG_POS_ERR, pos,
         "`%s` not found in upmost namespace", object_name );
   }
   else {
      struct str name;
      str_init( &name );
      t_copy_name( ns->name, true, &name );
      s_diag( semantic, DIAG_POS_ERR, pos,
         "`%s` not found in namespace `%s`", object_name,
         name.value );
      str_deinit( &name );
   }
}

void test_call( struct semantic* semantic, struct expr_test* expr_test,
   struct result* result, struct call* call ) {
   struct result operand;
   struct call_test test;
   init_call_test( &test, call );
   test_call_operand( semantic, expr_test, &test, call, &operand );
   test_call_format_arg( semantic, expr_test, &test, call );
   test_call_args( semantic, expr_test, &test );
   if ( test.func ) {
      test_call_func( semantic, &test, call );
   }
   // Return-value from function.
   if ( operand.func ) {
      // Reference return-value.
      if ( operand.func->ref ) {
         result->ref = operand.func->ref;
         result->structure = operand.func->structure;
         result->spec = operand.func->return_spec;
         result->usable = true;
      }
      // Primitive return-value.
      else {
         result->enumeration = operand.func->enumeration;
         result->spec = s_spec( semantic, operand.func->return_spec );
         result->usable = ( operand.func->return_spec != SPEC_VOID );
      }
      result->complete = true;
      call->func = operand.func;
      if ( operand.func->type == FUNC_USER ) {
         struct func_user* impl = operand.func->impl;
         if ( impl->nested ) {
            add_nested_call( operand.func, call );
         }
      }
   }
   // Return-value from function reference.
   else if ( operand.ref && operand.ref->type == REF_FUNCTION ) {
      struct ref_func* func = ( struct ref_func* ) operand.ref;
      // Reference return-value.
      if ( operand.ref->next ) {
         result->ref = operand.ref->next;
         result->structure = operand.structure;
         result->spec = operand.spec;
         result->usable = true;
      }
      // Primitive return-value.
      else {
         result->enumeration = operand.enumeration;
         result->spec = s_spec( semantic, operand.spec );
         result->usable = ( operand.spec != SPEC_VOID );
      }
      result->complete = true;
      static struct func dummy_func;
      dummy_func.type = FUNC_SAMPLE;
      call->func = &dummy_func;
      call->ref_func = func;
   }
   else {
      UNREACHABLE();
   }
}

void init_call_test( struct call_test* test, struct call* call ) {
   test->func = NULL;
   test->call = call;
   test->params = NULL;
   test->min_param = 0;
   test->max_param = 0;
   test->num_args = 0;
   test->format_param = false;
}

void test_call_operand( struct semantic* semantic, struct expr_test* expr_test,
   struct call_test* test, struct call* call, struct result* operand ) {
   init_result( operand );
   test_suffix( semantic, expr_test, operand, call->operand );
   if ( operand->func ) {
      test->func = operand->func;
      test->params = operand->func->params;
      test->min_param = operand->func->min_param;
      test->max_param = operand->func->max_param;
      test->format_param = ( operand->func->type == FUNC_FORMAT );
   }
   else if ( ! operand->dim && operand->ref &&
      operand->ref->type == REF_FUNCTION ) {
      struct ref_func* func = ( struct ref_func* ) operand->ref;
      test->params = func->params;
      test->min_param = func->min_param;
      test->max_param = func->max_param;
      if ( operand->ref->nullable ) {
         semantic->lib->uses_nullable_refs = true;
      }
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &call->pos,
         "operand not a function" );
      s_bail( semantic );
   }
}

void test_call_format_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test, struct call* call ) {
   if ( test->format_param ) {
      if ( ! call->format_item ) {
         s_diag( semantic, DIAG_POS_ERR, &call->pos,
            "function call missing format argument" );
         s_bail( semantic );
      }
      test_format_item_list( semantic, expr_test, call->format_item );
   }
   else {
      if ( call->format_item ) {
         s_diag( semantic, DIAG_POS_ERR, &call->format_item->pos,
            "passing format-item to non-format function" );
         s_bail( semantic );
      }
   }
}

void test_format_item_list( struct semantic* semantic, struct expr_test* test,
   struct format_item* item ) {
   while ( item ) {
      if ( item->cast == FCAST_ARRAY ) {
         test_array_format_item( semantic, test, item );
      }
      else if ( item->cast == FCAST_MSGBUILD ) {
         test_msgbuild_format_item( semantic, test, item );
      }
      else {
         test_format_item( semantic, test, item );
      }
      item = item->next;
   }
}

void test_format_item( struct semantic* semantic, struct expr_test* test,
   struct format_item* item ) {
   struct result result;
   init_result( &result );
   struct expr_test nested_test;
   s_init_expr_test( &nested_test, true, false );
   test_nested_root( semantic, test, &nested_test, &result, item->value );
   int spec = SPEC_INT;
   switch ( item->cast ) {
   case FCAST_BINARY:
   case FCAST_CHAR:
   case FCAST_DECIMAL:
   case FCAST_NAME:
   case FCAST_HEX:
      break;
   case FCAST_FIXED:
      spec = SPEC_FIXED;
      break;
   case FCAST_RAW:
      spec = SPEC_RAW;
      break;
   case FCAST_KEY:
   case FCAST_LOCAL_STRING:
   case FCAST_STRING:
      spec = SPEC_STR;
      break;
   default:
      UNREACHABLE();
   }
   struct type_info type;
   init_type_info( semantic, &type, &result );
   s_decay( semantic, &type );
   struct type_info required_type;
   s_init_type_info_scalar( &required_type, spec );
   if ( ! s_instance_of( &required_type, &type ) ) {
      s_type_mismatch( semantic, "argument", &type,
         "required", &required_type, &item->value->pos );
      s_bail( semantic );
   }
}

void test_array_format_item( struct semantic* semantic, struct expr_test* test,
   struct format_item* item ) {
   struct expr_test nested;
   s_init_expr_test( &nested, false, false );
   struct result root;
   init_result( &root );
   test_nested_root( semantic, test, &nested, &root, item->value );
   struct type_info type;
   init_type_info( semantic, &type, &root );
   s_decay( semantic, &type );
   static struct ref_array array = {
      { NULL, { 0, 0, 0 }, REF_ARRAY, true }, 1, STORAGE_MAP, 0 };
   struct type_info required_type;
   s_init_type_info( &required_type, &array.ref, NULL, NULL, NULL,
      s_spec( semantic, SPEC_INT ), STORAGE_MAP );
   if ( ! s_instance_of( &required_type, &type ) ) {
      s_type_mismatch( semantic, "argument", &type,
         "required", &required_type, &item->value->pos );
      s_bail( semantic );
   }
   if ( item->extra ) {
      struct format_item_array* extra = item->extra;
      test_int_arg( semantic, test, extra->offset );
      if ( extra->length ) {
         test_int_arg( semantic, test, extra->length );
      }
   }
}

void test_int_arg( struct semantic* semantic, struct expr_test* expr_test,
   struct expr* arg ) {
   struct expr_test nested_expr_test;
   s_init_expr_test( &nested_expr_test, true, false );
   struct result root;
   init_result( &root );
   test_nested_root( semantic, expr_test, &nested_expr_test, &root, arg );
   struct type_info type;
   init_type_info( semantic, &type, &root );
   s_decay( semantic, &type );
   struct type_info required_type;
   s_init_type_info_scalar( &required_type, SPEC_INT );
   if ( ! s_instance_of( &required_type, &type ) ) {
      s_type_mismatch( semantic, "argument", &type,
         "required", &required_type, &arg->pos );
      s_bail( semantic );
   }
}

void test_msgbuild_format_item( struct semantic* semantic,
   struct expr_test* expr_test, struct format_item* item ) {
   struct format_item_msgbuild* extra = item->extra;
   test_msgbuild_arg( semantic, expr_test, item, extra ); 
   if ( extra->func ) {
      struct func_user* impl = extra->func->impl;
      if ( impl->nested ) {
         struct call* call = t_alloc_call();
         call->func = extra->func;
         ++impl->usage;
         add_nested_call( extra->func, call ); 
         extra->call = call;
      }
   }
}

void test_msgbuild_arg( struct semantic* semantic, struct expr_test* expr_test,
   struct format_item* item, struct format_item_msgbuild* extra ) {
   struct expr_test nested;
   s_init_expr_test( &nested, true, false );
   struct result root;
   init_result( &root );
   test_nested_root( semantic, expr_test, &nested, &root, item->value );
   struct type_info type;
   init_type_info( semantic, &type, &root );
   s_decay( semantic, &type );
   struct type_info required_type;
   s_init_type_info_func( &required_type,
      NULL, NULL, NULL, NULL, SPEC_VOID, 0, 0, true );
   if ( ! s_instance_of( &required_type, &type ) ) {
      s_type_mismatch( semantic, "argument", &type,
         "required", &required_type, &item->value->pos );
      s_bail( semantic );
   }
   extra->func = root.func;
}

void add_nested_call( struct func* func, struct call* call ) {
   struct func_user* impl = func->impl;
   struct nested_call* nested = mem_alloc( sizeof( *nested ) );
   nested->next = impl->nested_calls;
   nested->id = 0;
   nested->prologue_jump = NULL;
   nested->return_point = NULL;
   impl->nested_calls = call;
   call->nested_call = nested;
}

void test_call_args( struct semantic* semantic, struct expr_test* expr_test,
   struct call_test* test ) {
   struct call* call = test->call;
   test_remaining_args( semantic, expr_test, test );
   // Number of arguments must be correct.
   if ( test->num_args < test->min_param ) {
      s_diag( semantic, DIAG_POS_ERR, &call->pos,
         "not enough %sarguments in function call",
         test->format_param ? "regular " : "" );
      struct str str;
      str_init( &str );
      present_func( test, &str );
      s_diag( semantic, DIAG_POS, &call->pos,
         "%s needs %s%d %sargument%s", str.value,
         test->min_param != test->max_param ? "at least " : "",
         test->min_param, test->format_param ? "regular " : "",
         test->min_param != 1 ? "s" : "" );
      s_bail( semantic );
   }
   if ( test->num_args > test->max_param ) {
      s_diag( semantic, DIAG_POS_ERR, &call->pos,
         "too many %sarguments in function call",
         test->format_param ? "regular " : "" );
      struct str str;
      str_init( &str );
      present_func( test, &str );
      s_diag( semantic, DIAG_POS, &call->pos,
         "%s takes %s%d %sargument%s", str.value,
         test->min_param != test->max_param ? "at most " : "",
         test->max_param, test->format_param ? "regular " : "",
         test->max_param != 1 ? "s" : "" );
      s_bail( semantic );
   }
}

void test_remaining_args( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test ) {
   struct param* param = test->params;
   list_iter_t i;
   list_iter_init( &i, &test->call->args );
   while ( ! list_end( &i ) ) {
      test_remaining_arg( semantic, expr_test, test, param, list_data( &i ) );
      if ( param ) {
         param = param->next;
      }
      list_next( &i );
   }
}

void test_remaining_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test, struct param* param,
   struct expr* expr ) {
   struct expr_test nested;
   s_init_expr_test( &nested, true, false );
   struct result result;
   init_result( &result );
   test_nested_root( semantic, expr_test, &nested, &result, expr );
   if ( param ) {
      struct type_info type;
      struct type_info param_type;
      init_type_info( semantic, &type, &result );
      s_init_type_info( &param_type, param->ref, param->structure,
         param->enumeration, NULL, param->spec, STORAGE_LOCAL );
      s_decay( semantic, &type );
      if ( ! s_instance_of( &param_type, &type ) ) {
         arg_mismatch( semantic, &expr->pos, &type,
            "parameter", &param_type, "argument", test->num_args + 1 );
         s_bail( semantic );
      }
      if ( result.func && result.func->type == FUNC_USER ) {
         struct func_user* impl = result.func->impl;
         if ( impl->local ) {
            s_diag( semantic, DIAG_POS_ERR, &expr->pos,
               "passing a local function as an argument" );
            s_bail( semantic );
         }
      }
   }
   ++test->num_args;
   if ( is_ref_type( &result ) && result.data_origin ) {
      result.data_origin->addr_taken = true;
      if ( ! result.data_origin->hidden ) {
         s_diag( semantic, DIAG_POS_ERR, &expr->pos,
            "non-private argument (references only work with private "
            "map variables)" );
         s_bail( semantic );
      }
   }
   if ( test->call->constant && ! expr->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &expr->pos,
         "non-constant argument in constant function call" );
      s_bail( semantic );
   }
}

void arg_mismatch( struct semantic* semantic, struct pos* pos,
   struct type_info* type, const char* from, struct type_info* required_type,
   const char* where, int number ) {
   struct str type_s;
   str_init( &type_s );
   s_present_type( type, &type_s );
   struct str required_type_s;
   str_init( &required_type_s );
   s_present_type( required_type, &required_type_s );
   s_diag( semantic, DIAG_POS_ERR, pos,
      "argument type (`%s`) different from %s type (`%s`) (in %s %d)",
      type_s.value, from, required_type_s.value, where, number );
   str_deinit( &required_type_s );
   str_deinit( &type_s );
}

void present_func( struct call_test* test, struct str* msg ) {
   if ( test->func ) {
      if ( test->func->name ) {
         struct str name;
         str_init( &name );
         t_copy_name( test->func->name, false, &name );
         str_append( msg, "function " );
         str_append( msg, "`" );
         str_append( msg, name.value );
         str_append( msg, "`" );
         str_deinit( &name );
      }
      else {
         str_append( msg, "anonymous function" );
      }
   }
   else {
      str_append( msg, "referenced function" );
   }
}

void test_call_func( struct semantic* semantic, struct call_test* test,
   struct call* call ) {
   // Some action-specials cannot be called from a script.
   if ( test->func->type == FUNC_ASPEC ) {
      struct func_aspec* impl = test->func->impl;
      if ( ! impl->script_callable ) {
         struct str str;
         str_init( &str );
         t_copy_name( test->func->name, false, &str );
         s_diag( semantic, DIAG_POS_ERR, &call->pos,
            "action-special `%s` called from script", str.value );
         s_bail( semantic );
      }
   }
   // Latent function cannot be called in a function.
   else if ( test->func->type == FUNC_DED ) {
      struct func_ded* impl = test->func->impl;
      if ( impl->latent ) {
         if ( semantic->func_test->func ) {
            s_diag( semantic, DIAG_POS_ERR, &call->pos,
               "calling latent function inside a function" );
            // Show educational note to user.
            if ( semantic->func_test->func ) {
               struct str str;
               str_init( &str );
               t_copy_name( test->func->name, false, &str );
               s_diag( semantic, DIAG_FILE, &call->pos,
                  "waiting functions like `%s` can only be called inside a "
                  "script", str.value );
               str_deinit( &str );
            }
            s_bail( semantic );
         }
      }
   }
   // Message-building function can only be called when building a message.
   if ( test->func->msgbuild && ! ( semantic->func_test->func &&
      semantic->func_test->func->msgbuild ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->pos,
         "calling message-building function where no message is being built" );
      s_bail( semantic );
   }
   // Determine if any function being tested is called recursively.
   struct func_test* func_test = semantic->func_test;
   while ( func_test && func_test->func != test->func ) {
      func_test = func_test->parent;
   }
   if ( func_test ) {
      struct func_test* end = func_test->parent;
      func_test = semantic->func_test;
      while ( func_test != end ) {
         struct func_user* impl = func_test->func->impl;
         impl->recursive = RECURSIVE_POSSIBLY;
         func_test = func_test->parent;
      }
   }
   // Make sure the function can be constant-called.
   if ( test->call->constant ) {
      bool supported = ( test->func->type == FUNC_ASPEC ||
         test->func->type == FUNC_EXT );
      if ( test->func->type == FUNC_DED ) {
         struct func_ded* impl = test->func->impl;
         supported = ( c_get_direct_pcode( impl->opcode ) != NULL );
      }
      if ( ! supported ) {
         s_diag( semantic, DIAG_POS_ERR, &call->pos,
            "constant-calling an unsupported function" );
         s_bail( semantic );
      }
   }
}

void test_sure( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct sure* sure ) {
   struct result operand;
   init_result( &operand );
   test_suffix( semantic, test, &operand, sure->operand );
   struct type_info type;
   init_type_info( semantic, &type, &operand );
   s_decay( semantic, &type );
   if ( ! s_is_ref_type( &type ) ) {
      s_diag( semantic, DIAG_POS_ERR, &sure->pos,
         "operand not a reference" );
      s_bail( semantic );
   }
   if ( operand.null ) {
      result->null = true;
      result->complete = true;
      result->usable = true;
      semantic->lib->uses_nullable_refs = true;
   }
   else {
      //if ( type.ref->nullable ) {
         size_t size = 0;
         switch ( type.ref->type ) {
         case REF_STRUCTURE:
            size = sizeof( struct ref_struct );
            break;
         case REF_ARRAY:
            size = sizeof( struct ref_array );
            break;
         case REF_FUNCTION:
            size = sizeof( struct ref_func );
            break;
         default:
            UNREACHABLE();
         }
         result->ref = &test->temp_ref.ref;
         memcpy( result->ref, type.ref, size );
      //}
      //else {
     //    result->ref = type.ref;
      if ( type.ref->nullable ) {
         result->ref->nullable = false;
         semantic->lib->uses_nullable_refs = true;
      }
      else {
         sure->already_safe = true;
      }
      result->structure = type.structure;
      result->enumeration = type.enumeration;
      result->spec = type.spec;
      result->complete = true;
      result->usable = true;
   }
}

void test_primary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node ) {
   switch ( node->type ) {
   case NODE_LITERAL:
      test_literal( semantic, result,
         ( struct literal* ) node );
      break;
   case NODE_FIXED_LITERAL:
      test_fixed_literal( semantic, result,
         ( struct fixed_literal* ) node );
      break;
   case NODE_INDEXED_STRING_USAGE:
      test_string_usage( semantic, test, result,
         ( struct indexed_string_usage* ) node );
      break;
   case NODE_BOOLEAN:
      test_boolean( semantic, result,
         ( struct boolean* ) node );
      break;
   case NODE_NAME_USAGE:
      test_name_usage( semantic, test, result,
         ( struct name_usage* ) node );
      break;
   case NODE_STRCPY:
      test_strcpy( semantic, test, result,
         ( struct strcpy_call* ) node );
      break;
   case NODE_MEMCPY:
      test_memcpy( semantic, test, result,
         ( struct memcpy_call* ) node );
      break;
   case NODE_CONVERSION:
      test_conversion( semantic, test, result,
         ( struct conversion* ) node );
      break;
   case NODE_COMPOUNDLITERAL:
      test_compound_literal( semantic, test, result,
         ( struct compound_literal* ) node );
      break;
   case NODE_FUNC:
      test_anon_func( semantic, result,
         ( struct func* ) node );
      break;
   case NODE_PAREN:
      test_paren( semantic, test, result,
         ( struct paren* ) node );
      break;
   case NODE_UPMOST:
      test_upmost( semantic, result );
      break;
   case NODE_CURRENTNAMESPACE:
      test_current_namespace( semantic, result );
      break;
   case NODE_NULL:
      result->null = true;
      result->complete = true;
      result->usable = true;
      result->folded = true;
      break;
   default:
      UNREACHABLE();
   }
}

void test_literal( struct semantic* semantic, struct result* result,
   struct literal* literal ) {
   result->spec = s_spec( semantic, SPEC_INT );
   result->value = literal->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

void test_fixed_literal( struct semantic* semantic, struct result* result,
   struct fixed_literal* literal ) {
   result->spec = s_spec( semantic, SPEC_FIXED );
   result->value = literal->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

void test_string_usage( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct indexed_string_usage* usage ) {
   result->spec = s_spec( semantic, SPEC_STR );
   result->value = usage->string->index;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   test->has_str = true;
}

void test_boolean( struct semantic* semantic, struct result* result,
   struct boolean* boolean ) {
   result->spec = s_spec( semantic, SPEC_BOOL );
   result->value = boolean->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

void test_name_usage( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage ) {
   struct object* object = NULL;
   if ( test->name_offset ) {
      struct name* name = t_extend_name( test->name_offset, usage->text );
      object = name->object;
   }
   if ( ! object ) {
      struct object_search search;
      s_init_object_search( &search, NODE_NONE, &usage->pos, usage->text );
      s_search_object( semantic, &search );
      object = search.object;
   }
   if ( object && ( object->resolved ||
      object->node.type == NODE_NAMESPACE ) ) {
      test_found_object( semantic, test, result, usage, object );
   }
   // Object not found or isn't valid.
   else {
      if ( semantic->trigger_err ) {
         if ( object ) {
            s_diag( semantic, DIAG_POS_ERR, &usage->pos,
               "`%s` undefined", usage->text );
         }
         else {
            s_diag( semantic, DIAG_POS_ERR, &usage->pos,
               "`%s` not found", usage->text );
         }
         s_bail( semantic );
      }
      else {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
   }
}

void test_found_object( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage, struct object* object ) {
   // The engine does not support accessing of arrays in another library unless
   // explicitly imported. So array and struct references only work in the
   // library which contains the referenced data.
   if ( object->node.type == NODE_VAR &&
      ( ( struct var* ) object )->imported ) {
      struct var* var = ( struct var* ) object;
      struct ref* ref = find_map_ref( var->ref, ( var->spec == SPEC_STRUCT ) ?
         var->structure : NULL );
      if ( ref ) {
         s_diag( semantic, DIAG_POS_ERR, &usage->pos,
            "imported variable's type includes a reference-to-%s type",
            ref->type == REF_STRUCTURE ? "struct" : "array" );
         s_diag( semantic, DIAG_POS, &ref->pos,
            "this reference-to-%s type is found here",
            ref->type == REF_STRUCTURE ? "struct" : "array" );
         s_diag( semantic, DIAG_POS, &ref->pos,
            "%s references do not work between libraries",
            ref->type == REF_STRUCTURE ? "struct" : "array" );
         s_bail( semantic );
      }
   }
   else if ( object->node.type == NODE_FUNC &&
      ( ( struct func* ) object )->type == FUNC_USER &&
      ( ( struct func* ) object )->imported ) {
      struct func* func = ( struct func* ) object;
      struct ref* ref = find_map_ref( func->ref, NULL );
      if ( ref ) {
         s_diag( semantic, DIAG_POS_ERR, &usage->pos,
            "imported function's return type includes a reference-to-%s type",
            ref->type == REF_STRUCTURE ? "struct" : "array" );
         s_diag( semantic, DIAG_POS, &ref->pos,
            "this reference-to-%s type is found here",
            ref->type == REF_STRUCTURE ? "struct" : "array" );
         s_diag( semantic, DIAG_POS, &ref->pos,
            "%s references do not work between libraries",
            ref->type == REF_STRUCTURE ? "struct" : "array" );
         s_bail( semantic );
      }
      struct param* param = func->params;
      while ( param ) {
         struct ref* ref = find_map_ref( param->ref, NULL );
         if ( ref ) {
            s_diag( semantic, DIAG_POS_ERR, &usage->pos,
               "imported function has parameter whose type includes a "
               "reference-to-%s type",
               ref->type == REF_STRUCTURE ? "struct" : "array" );
            s_diag( semantic, DIAG_POS, &ref->pos,
               "this reference-to-%s type is found here",
               ref->type == REF_STRUCTURE ? "struct" : "array" );
            s_diag( semantic, DIAG_POS, &ref->pos,
               "%s references do not work between libraries",
               ref->type == REF_STRUCTURE ? "struct" : "array" );
            s_bail( semantic );
         }
         param = param->next;
      }
   }
   // A nested function qualified with the `static` keyword cannot use the
   // local-storage variables of, or call the local functions of, an enclosing
   // script or function.
   bool local_object = false;
   if ( object->node.type == NODE_VAR ) {
      struct var* var = ( struct var* ) object;
      local_object = ( var->storage == STORAGE_LOCAL );
   }
   else if ( object->node.type == NODE_FUNC ) {
      struct func* func = ( struct func* ) object;
      if ( func->type == FUNC_USER ) {
         struct func_user* impl = func->impl;
         local_object = impl->local;
      }
   }
   if ( local_object ) {
      struct func_test* func_test = semantic->func_test;
      while ( func_test && ! ( func_test->func && ! (
         ( struct func_user* ) func_test->func->impl )->local ) ) {
         func_test = func_test->parent;
      }
      if ( func_test && func_test->func->object.depth >= object->depth ) {
         s_diag( semantic, DIAG_POS, &usage->pos,
            "%s outside a static function cannot be used",
            object->node.type == NODE_FUNC ? "local functions" :
            "local-storage variables" );
         s_bail( semantic );
      }
   }
   if ( object->node.type == NODE_FUNCNAME ) {
      test_funcname( semantic, test, result, usage );
   }
   else if ( object->node.type == NODE_SCRIPTID ) {
      test_script_id( semantic, test, result, usage );
   }
   else {
      select_object( semantic, test, result, object );
      usage->object = &object->node;
   }
}

struct ref* find_map_ref( struct ref* ref, struct structure* structure ) {
   if ( ref ) {
      while ( ref ) {
         if ( ref->type == REF_FUNCTION ) {
            struct ref_func* func = ( struct ref_func* ) ref;
            struct param* param = func->params;
            while ( param ) {
               struct ref* nested_ref = find_map_ref( param->ref, NULL );
               if ( nested_ref ) {
                  return nested_ref;
               }
               param = param->next;
            }
         }
         else {
            return ref;
         }
         ref = ref->next;
      }
   }
   else {
      if ( structure && structure->has_ref_member ) {
         return find_map_ref_struct( structure );
      }
   }
   return NULL;
}

struct ref* find_map_ref_struct( struct structure* structure ) {
   struct ref* ref = NULL;
   struct structure_member* member = structure->member;
   while ( member && ! ref ) {
      ref = find_map_ref( member->ref, ( member->spec == SPEC_STRUCT ) ?
         member->structure : NULL );
      member = member->next;
   }
   return ref;
}

void test_funcname( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage ) {
   struct str name;
   str_init( &name );
   if ( semantic->func_test->func->name ) {
      t_copy_name( semantic->func_test->func->name, false, &name );
   }
   else {
      // TODO: Create a nicer name.
      str_append( &name, "" );
   }
   struct indexed_string_usage* string_usage = t_alloc_indexed_string_usage();
   string_usage->string = t_intern_string_copy( semantic->task,
      name.value, name.length );
   usage->object = &string_usage->node;
   test_string_usage( semantic, test, result, string_usage );
   str_deinit( &name );
}

void test_script_id( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage ) {
   struct str name;
   str_init( &name );
   if ( semantic->func_test->script->named_script ) {
      struct indexed_string* string = t_lookup_string( semantic->task,
         semantic->func_test->script->number->value );
      str_append( &name, string->value );
   }
   else {
      str_append_number( &name, semantic->func_test->script->number->value );
   }
   struct indexed_string_usage* string_usage = t_alloc_indexed_string_usage();
   string_usage->string = t_intern_string_copy( semantic->task,
      name.value, name.length );
   usage->object = &string_usage->node;
   test_string_usage( semantic, test, result, string_usage );
   str_deinit( &name );
}

void select_object( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT:
      select_constant( semantic, test, result, 
         ( struct constant* ) object );
      break;
   case NODE_ENUMERATOR:
      select_enumerator( semantic, test, result,
         ( struct enumerator* ) object );
      break;
   case NODE_VAR:
      select_var( semantic, result,
         ( struct var* ) object );
      break;
   case NODE_PARAM:
      select_param( semantic, result,
         ( struct param* ) object );
      break;
   case NODE_STRUCTURE_MEMBER:
      select_member( semantic, result,
         ( struct structure_member* ) object );
      break;
   case NODE_FUNC:
      select_func( semantic, result,
         ( struct func* ) object );
      break;
   case NODE_NAMESPACE:
      result->object = object;
      break;
   case NODE_ALIAS:
      select_alias( semantic, test, result,
         ( struct alias* ) object );
      break;
   case NODE_STRUCTURE:
   case NODE_ENUMERATION:
   case NODE_TYPE_ALIAS:
      break;
   default:
      UNREACHABLE();
   }
}

void select_constant( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct constant* constant ) {
   result->spec = s_spec( semantic, constant->spec );
   result->value = constant->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   if ( constant->has_str ) {
      test->has_str = true;
   }
}

void select_enumerator( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct enumerator* enumerator ) {
   result->enumeration = enumerator->enumeration;
   result->spec = s_spec( semantic, enumerator->enumeration->base_type );
   result->value = enumerator->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   if ( enumerator->has_str ) {
      test->has_str = true;
   }
}

void select_var( struct semantic* semantic, struct result* result,
   struct var* var ) {
   // Array.
   if ( var->dim ) {
      result->data_origin = var;
      result->ref = var->ref;
      result->structure = var->structure;
      result->enumeration = var->enumeration;
      result->dim = var->dim;
      result->spec = var->spec;
      result->storage = var->storage;
      if ( var->storage == STORAGE_MAP ) {
         result->folded = true;
      }
   }
   // Reference variable.
   else if ( var->ref ) {
      result->ref = var->ref;
      result->structure = var->structure;
      result->enumeration = var->enumeration;
      result->spec = var->spec;
      result->modifiable = ( ! var->constant );
   }
   // Structure variable.
   else if ( var->structure ) {
      result->data_origin = var;
      result->structure = var->structure;
      result->spec = var->spec;
      result->storage = var->storage;
      if ( var->storage == STORAGE_MAP ) {
         result->folded = true;
      }
   }
   // Primitive variable.
   else {
      result->enumeration = var->enumeration;
      result->spec = s_spec( semantic, var->spec );
      result->modifiable = ( ! var->constant );
   }
   result->object = &var->object;
   result->complete = true;
   result->usable = true;
   var->used = true;
}

void select_param( struct semantic* semantic, struct result* result,
   struct param* param ) {
   // Reference parameter.
   if ( param->ref ) {
      result->ref = param->ref;
      result->structure = param->structure;
      result->spec = param->spec;
   }
   // Primitive parameter.
   else {
      result->enumeration = param->enumeration;
      result->spec = s_spec( semantic, param->spec );
   }
   result->complete = true;
   result->usable = true;
   result->modifiable = true;
   param->used = true;
}

void select_member( struct semantic* semantic, struct result* result,
   struct structure_member* member ) {
   // Array member.
   if ( member->dim ) {
      result->ref = member->ref;
      result->structure = member->structure;
      result->enumeration = member->enumeration;
      result->dim = member->dim;
      result->spec = member->spec;
   }
   // Reference member.
   else if ( member->ref ) {
      result->ref = member->ref;
      result->structure = member->structure;
      result->enumeration = member->enumeration;
      result->spec = member->spec;
      result->modifiable = true;
   }
   // Structure member.
   else if ( member->structure ) {
      result->structure = member->structure;
      result->spec = member->spec;
   }
   // Primitive member.
   else {
      result->enumeration = member->enumeration;
      result->spec = s_spec( semantic, member->spec );
      result->modifiable = true;
   }
   result->complete = true;
   result->usable = true;
}

void select_func( struct semantic* semantic, struct result* result,
   struct func* func ) {
   if ( func->type == FUNC_USER ) {
      result->usable = true;
      result->folded = true;
      struct func_user* impl = func->impl;
      ++impl->usage;
      if ( ! impl->local || func->msgbuild ) {
         result->complete = true;
      }
   }
   // When an action-special is not called, it decays into an integer value.
   // The value is the ID of the action-special.
   else if ( func->type == FUNC_ASPEC ) {
      result->spec = s_spec( semantic, SPEC_INT );
      struct func_aspec* impl = func->impl;
      result->value = impl->id;
      result->usable = true;
      result->folded = true;
      result->complete = true;
   }
   // An extension function can also decay into an integer.
   else if ( func->type == FUNC_EXT ) {
      result->spec = s_spec( semantic, SPEC_INT );
      struct func_ext* impl = func->impl;
      result->value = -impl->id;
      result->usable = true;
      result->folded = true;
      result->complete = true;
   }
   result->func = func;
}

void select_alias( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct alias* alias ) {
   select_object( semantic, test, result, alias->target );
}

void test_strcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct strcpy_call* call ) {
   // Array.
   struct result arg;
   init_result( &arg );
   test_root( semantic, test, &arg, call->array );
   if ( ! is_onedim_intelem_array_ref( semantic, &arg ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->array->pos,
         "array argument not a one-dimensional, "
         "integer element-type array-reference" );
      s_bail( semantic );
   }
   // Array-offset.
   if ( call->array_offset ) {
      init_result( &arg );
      test_root( semantic, test, &arg, call->array_offset );
      if ( ! ( arg.usable && is_int_value( semantic, &arg ) ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->array_offset->pos,
            "array-offset argument not an integer value" );
         s_bail( semantic );
      }
      // Array-length.
      if ( call->array_length ) {
         init_result( &arg );
         test_root( semantic, test, &arg, call->array_length );
         if ( ! ( arg.usable && is_int_value( semantic, &arg ) ) ) {
            s_diag( semantic, DIAG_POS_ERR, &call->array_length->pos,
               "array-length argument not an integer value" );
            s_bail( semantic );
         }
      }
   }
   // String.
   init_result( &arg );
   test_root( semantic, test, &arg, call->string );
   if ( ! ( arg.usable && is_str_value( semantic, &arg ) ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->string->pos,
         "string argument not a string value" );
      s_bail( semantic );
   }
   // String-offset.
   if ( call->offset ) {
      init_result( &arg );
      test_root( semantic, test, &arg, call->offset );
      if ( ! ( arg.usable && is_int_value( semantic, &arg ) ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->offset->pos,
            "string-offset argument not an integer value" );
         s_bail( semantic );
      }
   }
   result->spec = s_spec( semantic, SPEC_BOOL );
   result->complete = true;
   result->usable = true;
}

void test_memcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct memcpy_call* call ) {
   // Destination.
   struct result dst;
   init_result( &dst );
   test_root( semantic, test, &dst, call->destination );
   if ( ! ( is_array_ref( &dst ) || is_struct( &dst ) ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->destination->pos,
         "destination not an array or structure" );
      s_bail( semantic );
   }
   // It doesn't look pretty if we allow struct variables to be specified with
   // the array format-item, so make sure the argument is an array.
   if ( call->array_cast && ! is_array_ref( &dst ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->destination->pos,
         "destination not an array" );
      s_bail( semantic );
   }
   // Destination-offset.
   if ( call->destination_offset ) {
      struct result arg;
      init_result( &arg );
      test_root( semantic, test, &arg, call->destination_offset );
      if ( ! is_int_value( semantic, &arg ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->destination_offset->pos,
            "destination-offset not an integer value" );
         s_bail( semantic );
      }
      // Destination-length.
      if ( call->destination_length ) {
         init_result( &arg );
         test_root( semantic, test, &arg, call->destination_length );
         if ( ! is_int_value( semantic, &arg ) ) {
            s_diag( semantic, DIAG_POS_ERR, &call->destination_length->pos,
               "destination-length not an integer value" );
            s_bail( semantic );
         }
      }
   }
   // Source.
   struct result src;
   init_result( &src );
   test_root( semantic, test, &src, call->source );
   struct type_info dst_type;
   struct type_info src_type;
   init_type_info( semantic, &dst_type, &dst );
   init_type_info( semantic, &src_type, &src );
   s_decay( semantic, &dst_type );
   s_decay( semantic, &src_type );
   if ( ! s_same_type( &src_type, &dst_type ) ) {
      s_type_mismatch( semantic, "source", &src_type,
         "destination", &dst_type, &call->source->pos );
      s_bail( semantic );
   }
   // Source-offset.
   if ( call->source_offset ) {
      struct result arg;
      init_result( &arg );
      test_root( semantic, test, &arg, call->source_offset );
      if ( ! is_int_value( semantic, &arg ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->source_offset->pos,
            "source-offset not an integer value" );
         s_bail( semantic );
      }
      if ( ! is_array_ref( &dst ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->source_offset->pos,
            "source-offset specified for non-array source" );
         s_bail( semantic );
      }
   }
   result->spec = s_spec( semantic, SPEC_BOOL );
   result->complete = true;
   result->usable = true;
   if ( is_struct( &dst ) ) {
      call->type = MEMCPY_STRUCT;
   }
}

bool is_onedim_intelem_array_ref( struct semantic* semantic,
   struct result* result ) {
   bool onedim_array = ( result->dim && ! result->dim->next ) ||
      ( result->ref_dim == 1 );
   bool int_elem = ( result->spec == SPEC_INT );
   return ( onedim_array && int_elem );
}

bool is_int_value( struct semantic* semantic, struct result* result ) {
   return ( is_value_type( semantic, result ) &&
      result->spec == s_spec( semantic, SPEC_INT ) );
}

bool is_str_value( struct semantic* semantic, struct result* result ) {
   return ( is_value_type( semantic, result ) &&
      result->spec == s_spec( semantic, SPEC_STR ) );
}

void test_conversion( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct conversion* conv ) {
   struct type_info type;
   struct expr_test nested_test;
   s_init_expr_test( &nested_test, true, false );
   s_test_expr_type( semantic, &nested_test, &type, conv->expr );
   if ( ! perform_conversion( conv, &type ) ) {
      unsupported_conversion( semantic, &type, conv->spec, &conv->expr->pos );
      s_bail( semantic );
   }
   conv->spec_from = s_spec( semantic, type.spec );
   conv->from_ref = ( type.ref != NULL );
   result->spec = s_spec( semantic, conv->spec );
   result->complete = true;
   result->usable = true;
}

bool perform_conversion( struct conversion* conv, struct type_info* from ) { 
   if ( s_is_value_type( from ) ) {
      // Converting from a string to either an integer or a fixed-point number
      // requires a lot of generated code. For now, use a library function to
      // perform these conversions.
      switch ( conv->spec ) {
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_BOOL:
      case SPEC_STR:
         switch ( from->spec ) {
         case SPEC_INT:
         case SPEC_FIXED:
         case SPEC_BOOL:
            return true;
         }
      default:
         return false;
      }
   }
   else {
      switch ( conv->spec ) {
      case SPEC_BOOL:
         return true;
      default:
         return false;
      }
   }
}

void unsupported_conversion( struct semantic* semantic, struct type_info* from,
   int to_spec, struct pos* pos ) {
   struct str from_s;
   str_init( &from_s );
   s_present_type( from, &from_s );
   struct type_info to;
   s_init_type_info_scalar( &to, to_spec );
   struct str to_s;
   str_init( &to_s );
   s_present_type( &to, &to_s );
   s_diag( semantic, DIAG_POS_ERR, pos,
      "conversion from `%s` to `%s` unsupported", from_s.value,
      to_s.value );
   str_deinit( &from_s );
   str_deinit( &to_s );
}

void test_compound_literal( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct compound_literal* literal ) {
   if ( semantic->in_localscope ) {
      s_test_local_var( semantic, literal->var );
   }
   else {
      s_test_var( semantic, literal->var );
      if ( ! literal->var->object.resolved ) {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
   }
   select_var( semantic, result, literal->var );
}

void test_anon_func( struct semantic* semantic, struct result* result,
   struct func* func ) {
   if ( semantic->in_localscope ) {
      s_test_nested_func( semantic, func );
   }
   select_func( semantic, result, func );
}

void test_paren( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct paren* paren ) {
   result->in_paren = true;
   test_operand( semantic, test, result, paren->inside );
   result->in_paren = false;
}

void test_upmost( struct semantic* semantic, struct result* result ) {
   result->object = &semantic->task->upmost_ns->object;
}

void test_current_namespace( struct semantic* semantic,
   struct result* result ) {
   result->object = &semantic->ns->object;
}

void init_type_info( struct semantic* semantic, struct type_info* type,
   struct result* result ) {
   if ( result->func ) {
      switch ( result->func->type ) {
      case FUNC_USER:
         s_init_type_info_func( type, result->func->ref,
            result->func->structure, result->func->enumeration,
            result->func->params, result->func->return_spec,
            result->func->min_param, result->func->max_param,
            result->func->msgbuild );
         break;
      case FUNC_ASPEC:
      case FUNC_EXT:
         s_init_type_info_scalar( type, s_spec( semantic, SPEC_INT ) );
         break;
      default:
         s_init_type_info_builtin_func( type );
      }
   }
   else if ( result->ref_dim >= 1 ) {
      s_init_type_info_array_ref( type, result->ref->next, result->structure,
         result->enumeration, result->ref_dim, result->spec );
   }
   else if ( result->null ) {
      s_init_type_info_null( type );
   }
   else {
      s_init_type_info( type, result->ref, result->structure,
         result->enumeration, result->dim, result->spec, result->storage );
   }
}

bool is_value_type( struct semantic* semantic, struct result* result ) {
   struct type_info type;
   init_type_info( semantic, &type, result );
   return s_is_value_type( &type );
}

bool is_ref_type( struct result* result ) {
   return ( result->ref || result->dim || result->structure || result->func ||
      result->null );
}