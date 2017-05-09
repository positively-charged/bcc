#include <string.h>

#include "parse/phase.h"
#include "codegen/phase.h"
#include "phase.h"

struct result {
   struct type_info type;
   struct func* func;
   struct var* data_origin;
   struct object* object;
   struct dim* dim;
   int value;
   bool complete;
   bool usable;
   bool modifiable;
   bool folded;
   bool in_paren;
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

static void test_nested_expr( struct semantic* semantic,
   struct expr_test* parent_test, struct expr_test* test, struct expr* expr );
static void test_root( struct semantic* semantic, struct expr_test* test,
   struct expr* expr );
static void test_root_with_result( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct expr* expr );
static void init_result( struct result* result );
static void test_operand( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_binary( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct binary* binary );
static bool perform_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result );
static bool perform_bop_primitive( struct semantic* semantic,
   struct binary* binary, struct result* lside, struct result* rside,
   struct result* result );
static bool perform_bop_ref( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result );
static bool perform_bop_ref_compare( struct semantic* semantic,
   struct binary* binary, struct result* lside, struct result* rside,
   struct result* result );
static void invalid_bop( struct semantic* semantic, struct binary* binary,
   struct result* operand );
static void fold_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result );
static void fold_bop_primitive( struct semantic* semantic,
   struct binary* binary, struct result* lside, struct result* rside,
   struct result* result );
static void fold_bop_int( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside );
static void fold_bop_fixed( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside );
static void fold_bop_bool( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside );
static void fold_bop_str( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside );
static void fold_bop_str_compare( struct semantic* semantic,
   struct binary* binary, struct result* lside, struct result* rside );
static void test_logical( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct logical* logical );
static bool perform_logical( struct semantic* semantic,
   struct logical* logical, struct result* lside, struct result* rside,
   struct result* result );
static bool can_convert_to_boolean( struct semantic* semantic,
   struct result* operand );
static void fold_logical( struct semantic* semantic, struct logical* logical,
   struct result* lside, struct result* rside, struct result* result );
static void fold_logical_primitive( struct semantic* semantic,
   struct logical* logical, struct result* lside, struct result* rside );
static void test_assign( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct assign* assign );
static bool perform_assign( struct semantic* semantic, struct assign* assign,
   struct result* lside, struct result* result );
static bool perform_assign_primitive( struct assign* assign,
   struct result* lside, struct result* result );
static bool perform_assign_enum( struct semantic* semantic,
   struct assign* assign, struct result* lside, struct result* result );
static bool perform_assign_ref( struct assign* assign, struct result* lside,
   struct result* result );
static void test_conditional( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct conditional* cond );
static void test_prefix( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_unary( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct unary* unary );
static bool perform_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand, struct result* result );
static bool perform_unary_primitive( struct semantic* semantic,
   struct unary* unary, struct result* operand, struct result* result );
static bool perform_unary_ref( struct semantic* semantic,
   struct unary* unary, struct result* operand, struct result* result );
static void invalid_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand );
static void fold_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand, struct result* result );
static void fold_unary_primitive( struct semantic* semantic,
   struct unary* unary, struct result* operand, struct result* result );
static void fold_unary_ref( struct semantic* semantic,
   struct unary* unary, struct result* operand, struct result* result );
static void test_inc( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct inc* inc );
static bool perform_inc( struct semantic* semantic, struct inc* inc,
   struct result* operand, struct result* result );
static bool perform_primitive_inc( struct semantic* semantic, struct inc* inc,
   struct result* operand, struct result* result );
static void invalid_inc( struct semantic* semantic, struct inc* inc,
   struct result* operand );
static void test_cast( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct cast* cast );
static bool perform_cast( struct semantic* semantic, struct cast* cast,
   struct result* operand, struct result* result );
static bool perform_cast_primitive( struct semantic* semantic,
   struct cast* cast, struct result* operand, struct result* result );
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
static void test_access( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct access* access );
static bool is_ns( struct result* result );
static void test_access_struct( struct semantic* semantic,
   struct expr_test* test, struct access* access, struct result* lside,
   struct result* result );
static struct structure_member* get_structure_member(
   struct semantic* semantic, struct expr_test* test, struct access* access,
   struct result* lside );
static void test_access_ns( struct semantic* semantic,
   struct expr_test* test, struct access* access, struct result* lside,
   struct result* result );
static void test_access_array( struct semantic* semantic,
   struct expr_test* test, struct access* access, struct result* lside,
   struct result* result );
static void test_access_str( struct semantic* semantic,
   struct expr_test* test, struct access* access, struct result* lside,
   struct result* result );
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
static void test_int_arg( struct semantic* semantic, struct expr_test* test,
   struct expr* expr );
static void test_buildmsg( struct semantic* semantic,
   struct expr_test* expr_test, struct call* call );
static void add_nested_call( struct func* func, struct call* call );
static void test_remaining_args( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test );
static void test_remaining_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test, struct param* param,
   struct expr* expr );
static void arg_mismatch( struct semantic* semantic, struct pos* pos,
   struct type_info* type, const char* from, struct type_info* required_type,
   const char* where, int number );
static void present_func( struct semantic* semantic, struct call_test* test,
   struct str* msg );
static void test_call_func( struct semantic* semantic, struct call_test* test,
   struct call* call );
static void test_call_ded( struct semantic* semantic, struct call_test* test,
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
static void test_string( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct indexed_string* string );
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
static void select_structure_member( struct semantic* semantic,
   struct result* lside, struct result* result,
   struct structure_member* member );
static void select_func( struct semantic* semantic, struct result* result,
   struct func* func );
static void select_ns( struct semantic* semantic, struct result* result,
   struct ns* ns );
static void select_alias( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct alias* alias );
static void test_strcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct strcpy_call* call );
static void test_memcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct memcpy_call* call );
static void test_conversion( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct conversion* conv );
static bool perform_conversion( struct semantic* semantic,
   struct conversion* conv, struct expr_test* operand,
   struct result* result );
static void unsupported_conversion( struct semantic* semantic,
   struct conversion* conv, struct expr_test* operand );
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
static void test_null( struct result* result );
static void test_magic_id( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct magic_id* magic_id );
static void expand_magic_id( struct semantic* semantic,
   struct magic_id* magic_id );

void s_init_expr_test( struct expr_test* test, bool result_required,
   bool suggest_paren_assign ) {
   test->buildmsg = NULL;
   s_init_type_info_scalar( &test->type, SPEC_VOID );
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
      test_root( semantic, test, expr );
   }
   else {
      test->undef_erred = true;
   }
}

static void test_nested_expr( struct semantic* semantic,
   struct expr_test* parent_test, struct expr_test* test, struct expr* expr ) {
   if ( setjmp( test->bail ) == 0 ) {
      test_root( semantic, test, expr );
      if ( test->has_str ) {
         parent_test->has_str = true;
      }
   }
   else {
      longjmp( parent_test->bail, 1 );
   }
}

void s_test_bool_expr( struct semantic* semantic, struct expr* expr ) {
   struct expr_test test;
   s_init_expr_test( &test, true, true );
   struct result result;
   init_result( &result );
   test_root_with_result( semantic, &test, &result, expr );
   if ( ! can_convert_to_boolean( semantic, &result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &expr->pos,
         "expression cannot be converted to a boolean value" );
      s_bail( semantic );
   }
}

static void test_root( struct semantic* semantic, struct expr_test* test,
   struct expr* expr ) {
   struct result result;
   init_result( &result );
   test_root_with_result( semantic, test, &result, expr );
}

static void test_root_with_result( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct expr* expr ) {
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
   expr->spec = result->type.spec;
   expr->folded = result->folded;
   expr->value = result->value;
   expr->has_str = test->has_str;
   s_init_type_info_copy( &test->type, &result->type );
}

static void init_result( struct result* result ) {
   s_init_type_info_scalar( &result->type, SPEC_VOID );
   result->func = NULL;
   result->data_origin = NULL;
   result->object = NULL;
   result->dim = NULL;
   result->value = 0;
   result->complete = false;
   result->usable = false;
   result->modifiable = false;
   result->folded = false;
   result->in_paren = false;
}

static void test_operand( struct semantic* semantic, struct expr_test* test,
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

static void test_binary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct binary* binary ) {
   struct result lside;
   init_result( &lside );
   test_operand( semantic, test, &lside, binary->lside );
   if ( ! lside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &binary->pos,
         "left operand not a value" );
      s_bail( semantic );
   }
   struct result rside;
   init_result( &rside );
   test_operand( semantic, test, &rside, binary->rside );
   if ( ! rside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &binary->pos,
         "right operand not a value" );
      s_bail( semantic );
   }
   if ( ! s_same_type( &lside.type, &rside.type ) ) {
      s_type_mismatch( semantic, "left-operand", &lside.type,
         "right-operand", &rside.type, &binary->pos );
      s_bail( semantic );
   }
   // Implicit `raw` cast: 
   // Mixing a `raw` operand with an operand of another type will implicitly
   // cast the other operand to `raw`. I don't like this being here. Since
   // this cast happens here only, it will do for now. This cast will probably
   // be better off done in the type.c file.
   switch ( s_describe_type( &lside.type ) ) {
   case TYPEDESC_PRIMITIVE:
   case TYPEDESC_ENUM:
      if ( lside.type.spec == SPEC_RAW || rside.type.spec == SPEC_RAW ) {
         lside.type.spec = SPEC_RAW;
         rside.type.spec = SPEC_RAW;
      }
      break;
   default:
      break;
   }
   if ( ! perform_bop( semantic, binary, &lside, &rside, result ) ) {
      invalid_bop( semantic, binary, &lside );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( lside.folded && rside.folded ) {
      fold_bop( semantic, binary, &lside, &rside, result );
   }
}

static bool perform_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result ) {
   switch ( s_describe_type( &lside->type ) ) {
   case TYPEDESC_PRIMITIVE:
   case TYPEDESC_ENUM:
      return perform_bop_primitive( semantic, binary, lside, rside, result );
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
   case TYPEDESC_NULLREF:
      return perform_bop_ref( semantic, binary, lside, rside, result );
   default:
      UNREACHABLE();
      return false;
   }
}

static bool perform_bop_primitive( struct semantic* semantic,
   struct binary* binary, struct result* lside, struct result* rside,
   struct result* result ) {
   if ( lside->type.spec == SPEC_RAW || rside->type.spec == SPEC_RAW ) {
      lside->type.spec = SPEC_RAW;
      rside->type.spec = SPEC_RAW;
   }
   int result_spec = SPEC_NONE;
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
      switch ( lside->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_BOOL:
      case SPEC_STR:
         result_spec = SPEC_BOOL;
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
      switch ( lside->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
         result_spec = lside->type.spec;
         break;
      default:
         break;
      }
      break;
   case BOP_LT:
   case BOP_LTE:
   case BOP_GT:
   case BOP_GTE:
      switch ( lside->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_STR:
         result_spec = SPEC_BOOL;
         break;
      default:
         break;
      }
      break;
   case BOP_ADD:
      switch ( lside->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_STR:
         result_spec = lside->type.spec;
         break;
      default:
         break;
      }
      break;
   case BOP_SUB:
   case BOP_MUL:
   case BOP_DIV:
      switch ( lside->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
         result_spec = lside->type.spec;
         break;
      default:
         break;
      }
      break;
   default:
      break;
   }
   if ( result_spec == SPEC_NONE ) {
      return false;
   }
   s_init_type_info_scalar( &result->type, s_spec( semantic, result_spec ) );
   result->complete = true;
   result->usable = true;
   binary->operand_type = lside->type.spec;
   return true;
}

static bool perform_bop_ref( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result ) {
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
      return perform_bop_ref_compare( semantic, binary, lside, rside, result );
   default:
      return false;
   }
}

static bool perform_bop_ref_compare( struct semantic* semantic,
   struct binary* binary, struct result* lside, struct result* rside,
   struct result* result ) {
   if ( s_describe_type( &lside->type ) == TYPEDESC_FUNCREF ||
      s_describe_type( &rside->type ) == TYPEDESC_FUNCREF ) {
      binary->operand_type = BINARYOPERAND_REFFUNC;
   }
   else {
      binary->operand_type = BINARYOPERAND_REF;
   }
   s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_BOOL ) );
   result->complete = true;
   result->usable = true;
   // I had a bug in my BCS code in which I compared an anonymous struct
   // element of an array to null (this operation will always return false,
   // since a struct element is always a valid reference), but what I really
   // wanted to do is compare a reference member of that anonymous struct
   // element to null. So when comparing to null, to avoid potential bugs, if
   // the result will always be the same, notify the user about it.
   if ( ( s_is_null( &lside->type ) && ! s_is_nullable( &rside->type ) ) ||
      ( s_is_null( &rside->type ) && ! s_is_nullable( &lside->type ) ) ) {
      s_diag( semantic, DIAG_NOTE | DIAG_POS, &binary->pos,
         "this comparison will always be %s",
         binary->op == BOP_EQ ? "false" : "true" );
   }
   return true;
}

static void invalid_bop( struct semantic* semantic, struct binary* binary,
   struct result* operand ) {
   struct str type;
   str_init( &type );
   s_present_type( &operand->type, &type );
   s_diag( semantic, DIAG_POS_ERR, &binary->pos,
      "invalid binary operation for operand type (`%s`)",
      type.value );
   str_deinit( &type );
}

static void fold_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result ) {
   switch ( s_describe_type( &lside->type ) ) {
   case TYPEDESC_PRIMITIVE:
   case TYPEDESC_ENUM:
      fold_bop_primitive( semantic, binary, lside, rside, result );
      break;
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
   case TYPEDESC_NULLREF:
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
}

static void fold_bop_primitive( struct semantic* semantic,
   struct binary* binary, struct result* lside, struct result* rside,
   struct result* result ) {
   switch ( lside->type.spec ) {
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
      UNREACHABLE();
      s_bail( semantic );
   }
   result->value = binary->value;
   result->folded = binary->folded;
}

static void fold_bop_int( struct semantic* semantic, struct binary* binary,
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

static void fold_bop_fixed( struct semantic* semantic, struct binary* binary,
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

static void fold_bop_bool( struct semantic* semantic, struct binary* binary,
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

static void fold_bop_str( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside ) {
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
   case BOP_LT:
   case BOP_LTE:
   case BOP_GT:
   case BOP_GTE:
      fold_bop_str_compare( semantic, binary, lside, rside );
      break;
   default:
      break;
   }
}

static void fold_bop_str_compare( struct semantic* semantic,
   struct binary* binary, struct result* lside, struct result* rside ) {
   struct indexed_string* lside_str =
      t_lookup_string( semantic->task, lside->value );
   struct indexed_string* rside_str =
      t_lookup_string( semantic->task, rside->value );
   int result = strcmp( lside_str->value, rside_str->value );
   switch ( binary->op ) {
   case BOP_EQ: binary->value = ( result == 0 ); break;
   case BOP_NEQ: binary->value = ( result != 0 ); break;
   case BOP_LT: binary->value = ( result < 0 ); break;
   case BOP_LTE: binary->value = ( result <= 0 ); break;
   case BOP_GT: binary->value = ( result > 0 ); break;
   case BOP_GTE: binary->value = ( result >= 0 ); break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
   binary->folded = true;
}

static void test_logical( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct logical* logical ) {
   struct result lside;
   init_result( &lside );
   test_operand( semantic, test, &lside, logical->lside );
   if ( ! lside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &logical->pos,
         "left operand not a value" );
      s_bail( semantic );
   }
   struct result rside;
   init_result( &rside );
   test_operand( semantic, test, &rside, logical->rside );
   if ( ! rside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &logical->pos,
         "right operand not a value" );
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

static bool perform_logical( struct semantic* semantic,
   struct logical* logical, struct result* lside, struct result* rside,
   struct result* result ) {
   if ( can_convert_to_boolean( semantic, lside ) &&
      can_convert_to_boolean( semantic, rside ) ) {
      s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_BOOL ) );
      result->complete = true;
      result->usable = true;
      logical->lside_spec = lside->type.spec;
      logical->rside_spec = rside->type.spec;
      return true;
   }
   return false;
}

// NOTE: Not sure if this function is necessary, since every usable result can
// be converted to a boolean.
static bool can_convert_to_boolean( struct semantic* semantic,
   struct result* operand ) {
   switch ( s_describe_type( &operand->type ) ) {
   case TYPEDESC_PRIMITIVE:
   case TYPEDESC_ENUM:
      switch ( operand->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_BOOL:
      case SPEC_STR:
         return true;
      default:
         break;
      }
      return false;
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
   case TYPEDESC_NULLREF:
      return true;
   default:
      UNREACHABLE();
      return false;
   }
}

static void fold_logical( struct semantic* semantic, struct logical* logical,
   struct result* lside, struct result* rside, struct result* result ) {
   switch ( s_describe_type( &lside->type ) ) {
   case TYPEDESC_PRIMITIVE:
   case TYPEDESC_ENUM:
      fold_logical_primitive( semantic, logical, lside, rside );
      break;
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
   case TYPEDESC_NULLREF:
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
   result->value = logical->value;
   result->folded = logical->folded;
}

static void fold_logical_primitive( struct semantic* semantic,
   struct logical* logical, struct result* lside, struct result* rside ) {
   int l = 0;
   switch ( lside->type.spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
   case SPEC_BOOL:
   case SPEC_STR:
      l = lside->value;
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
   int r = 0;
   switch ( rside->type.spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
   case SPEC_BOOL:
   case SPEC_STR:
      r = rside->value;
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
   switch ( logical->op ) {
   case LOP_OR: l = ( l || r ); break;
   case LOP_AND: l = ( l && r ); break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
   logical->value = l;
   logical->folded = true;
}

static void test_assign( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct assign* assign ) {
   struct result lside;
   init_result( &lside );
   test_operand( semantic, test, &lside, assign->lside );
   s_reveal( &lside.type );
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
         "right operand not a value" );
      s_bail( semantic );
   }
   if ( ! s_instance_of( &lside.type, &rside.type ) ) {
      s_type_mismatch( semantic, "left-operand", &lside.type,
         "right-operand", &rside.type, &assign->pos );
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
   if ( ! perform_assign( semantic, assign, &lside, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &assign->pos,
         "invalid assignment operation" );
      s_bail( semantic );
   }
   if ( s_is_ref_type( &lside.type ) && rside.data_origin ) {
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
      if ( lside.object->node.type == NODE_VAR ) {
         struct var* var = ( struct var* ) lside.object;
         var->modified = true;
      }
   }
   // To avoid the error where the user wanted equality operator but instead
   // typed in the assignment operator, suggest that assignment be wrapped in
   // parentheses.
   if ( test->suggest_paren_assign && ! result->in_paren ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS, &assign->pos,
         "assignment operation not in parentheses" );
   }
   assign->spec = lside.type.spec;
}

static bool perform_assign( struct semantic* semantic, struct assign* assign,
   struct result* lside, struct result* result ) {
   switch ( s_describe_type( &lside->type ) ) {
   case TYPEDESC_PRIMITIVE:
      return perform_assign_primitive( assign, lside, result );
   case TYPEDESC_ENUM:
      return perform_assign_enum( semantic, assign, lside, result );
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
      return perform_assign_ref( assign, lside, result );
   default:
      UNREACHABLE();
      return false;
   }
}

static bool perform_assign_primitive( struct assign* assign,
   struct result* lside, struct result* result ) {
   bool valid = false;
   switch ( assign->op ) {
   case AOP_NONE:
      switch ( lside->type.spec ) {
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
      switch ( lside->type.spec ) {
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
      switch ( lside->type.spec ) {
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
      switch ( lside->type.spec ) {
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
   s_init_type_info_scalar( &result->type, lside->type.spec );
   result->complete = true;
   result->usable = true;
   return true;
}

static bool perform_assign_enum( struct semantic* semantic,
   struct assign* assign, struct result* lside, struct result* result ) {
   if ( assign->op == AOP_NONE ) {
      s_init_type_info_copy( &result->type, &lside->type );
      s_decay( semantic, &result->type );
      result->complete = true;
      result->usable = true;
      return true;
   }
   return false;
}

static bool perform_assign_ref( struct assign* assign, struct result* lside,
   struct result* result ) {
   if ( assign->op == AOP_NONE ) {
      s_init_type_info_copy( &result->type, &lside->type );
      result->complete = true;
      result->usable = true;
      return true;
   }
   return false;
}

static void test_conditional( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct conditional* cond ) {
   struct result left;
   init_result( &left );
   test_operand( semantic, test, &left, cond->left );
   if ( ! left.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &cond->pos,
         "left operand not a value" );
      s_bail( semantic );
   }
   if ( ! can_convert_to_boolean( semantic, &left ) ) {
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
   struct type_info result_type;
   if ( ! s_common_type( &middle.type, &right.type, &result_type ) ) {
      s_type_mismatch( semantic, cond->middle ?
         "middle-operand" : "left-operand", &middle.type, "right-operand",
         &right.type, &cond->pos );
      s_bail( semantic );
   }
   struct type_snapshot snapshot;
   s_take_type_snapshot( &result_type, &snapshot );
   s_init_type_info( &result->type, snapshot.ref, snapshot.structure,
      snapshot.enumeration, NULL, snapshot.spec, STORAGE_LOCAL );
   result->complete = true;
   result->usable = ( ! s_is_void( &result_type ) );
   cond->ref = snapshot.ref;
   cond->left_spec = left.type.spec;
   if ( s_is_ref_type( &middle.type ) ) {
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
      if ( s_is_value_type( &left.type ) && s_is_value_type( &right.type ) ) {
         result->value = ( left.value != 0 ) ? middle.value : right.value;
         result->folded = true;
         cond->left_value = left.value;
         cond->folded = true;
      }
   }
}

static void test_prefix( struct semantic* semantic, struct expr_test* test,
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

static void test_unary( struct semantic* semantic, struct expr_test* test,
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
      invalid_unary( semantic, unary, &operand );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( operand.folded ) {
      fold_unary( semantic, unary, &operand, result );
   }
}

static bool perform_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand, struct result* result ) {
   switch ( s_describe_type( &operand->type ) ) {
   case TYPEDESC_PRIMITIVE:
      return perform_unary_primitive( semantic, unary, operand, result );
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
   case TYPEDESC_NULLREF:
      return perform_unary_ref( semantic, unary, operand, result );
   default:
      UNREACHABLE();
      return false;
   }
}

static bool perform_unary_primitive( struct semantic* semantic,
   struct unary* unary, struct result* operand, struct result* result ) {
   int spec = SPEC_NONE;
   switch ( unary->op ) {
   case UOP_MINUS:
   case UOP_PLUS:
      switch ( operand->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
         spec = operand->type.spec;
         break;
      default:
         break;
      }
      break;
   case UOP_BIT_NOT:
      switch ( operand->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
         spec = operand->type.spec;
         break;
      default:
         break;
      }
      break;
   case UOP_LOG_NOT:
      spec = SPEC_BOOL;
      break;
   default:
      UNREACHABLE()
   }
   if ( spec != SPEC_NONE ) {
      s_init_type_info_scalar( &result->type, s_spec( semantic, spec ) );
      result->complete = true;
      result->usable = true;
      unary->operand_spec = operand->type.spec;
      return true;
   }
   else {
      return false;
   }
}

static bool perform_unary_ref( struct semantic* semantic,
   struct unary* unary, struct result* operand, struct result* result ) {
   switch ( unary->op ) {
   case UOP_LOG_NOT:
      s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_BOOL ) );
      result->complete = true;
      result->usable = true;
      return true;
   default:
      return false;
   }
}

static void invalid_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand ) {
   struct str type;
   str_init( &type );
   s_present_type( &operand->type, &type );
   s_diag( semantic, DIAG_POS_ERR, &unary->pos,
      "invalid unary operation for operand type (`%s`)", type.value );
   str_deinit( &type );
}

static void fold_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand, struct result* result ) {
   switch ( s_describe_type( &operand->type ) ) {
   case TYPEDESC_PRIMITIVE:
      fold_unary_primitive( semantic, unary, operand, result );
      break;
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
   case TYPEDESC_NULLREF:
      fold_unary_ref( semantic, unary, operand, result );
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
}

static void fold_unary_primitive( struct semantic* semantic,
   struct unary* unary, struct result* operand, struct result* result ) {
   switch ( unary->op ) {
   case UOP_MINUS:
      switch ( operand->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
         // TODO: Warn on overflow and underflow.
         result->value = ( - operand->value );
         result->folded = true;
         break;
      default:
         UNREACHABLE();
         s_bail( semantic );
      }
      break;
   case UOP_PLUS:
      switch ( operand->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
         result->value = operand->value;
         result->folded = true;
         break;
      default:
         UNREACHABLE();
         s_bail( semantic );
      }
      break;
   case UOP_LOG_NOT:
      switch ( operand->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_FIXED:
      case SPEC_BOOL:
      case SPEC_STR:
         result->value = ( ! operand->value );
         result->folded = true;
         break;
      default:
         UNREACHABLE();
         s_bail( semantic );
      }
      break;
   case UOP_BIT_NOT:
      switch ( operand->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
         result->value = ( ~ operand->value );
         result->folded = true;
         break;
      default:
         UNREACHABLE();
         s_bail( semantic );
      }
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
}

static void fold_unary_ref( struct semantic* semantic,
   struct unary* unary, struct result* operand, struct result* result ) {
   switch ( unary->op ) {
   case UOP_LOG_NOT:
      // If the operand is folded, that means that the operand is a direct
      // reference to the object. So the result is always true.
      result->value = 1;
      result->folded = true;
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
}

static void test_inc( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct inc* inc ) {
   struct result operand;
   init_result( &operand );
   test_operand( semantic, test, &operand, inc->operand );
   s_reveal( &operand.type );
   if ( ! operand.modifiable ) {
      s_diag( semantic, DIAG_POS_ERR, &inc->pos,
         "operand cannot be %s", inc->dec ?
            "decremented" : "incremented" );
      s_bail( semantic );
   }
   if ( ! perform_inc( semantic, inc, &operand, result ) ) {
      invalid_inc( semantic, inc, &operand );
      s_bail( semantic );
   }
   if ( operand.object ) {
      if ( operand.object->node.type == NODE_VAR ) {
         struct var* var = ( struct var* ) operand.object;
         var->modified = true;
      }
   }
}

static bool perform_inc( struct semantic* semantic, struct inc* inc,
   struct result* operand, struct result* result ) {
   switch ( s_describe_type( &operand->type ) ) {
   case TYPEDESC_PRIMITIVE:
      return perform_primitive_inc( semantic, inc, operand, result );
   default:
      return false;
   }
}

static bool perform_primitive_inc( struct semantic* semantic, struct inc* inc,
   struct result* operand, struct result* result ) {
   switch ( operand->type.spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
      s_init_type_info_scalar( &result->type, operand->type.spec );
      result->complete = true;
      result->usable = true;
      inc->fixed = ( operand->type.spec == SPEC_FIXED );
      return true;
   default:
      return false;
   }
}

static void invalid_inc( struct semantic* semantic, struct inc* inc,
   struct result* operand ) {
   struct str string;
   str_init( &string );
   s_present_type( &operand->type, &string );
   s_diag( semantic, DIAG_POS_ERR, &inc->pos,
      "%s operation not supported for given operand (`%s`)",
      inc->dec ? "decrement" : "increment", string.value );
   str_deinit( &string );
}

static void test_cast( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct cast* cast ) {
   struct result operand;
   init_result( &operand );
   test_operand( semantic, test, &operand, cast->operand );
   if ( ! operand.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &cast->pos,
         "cast operand not a value" );
      s_bail( semantic );
   }
   if ( ! perform_cast( semantic, cast, &operand, result ) ) {
      invalid_cast( semantic, cast, &operand );
      s_bail( semantic );
   }
}

static bool perform_cast( struct semantic* semantic, struct cast* cast,
   struct result* operand, struct result* result ) {
   switch ( s_describe_type( &operand->type ) ) {
   case TYPEDESC_PRIMITIVE:
      return perform_cast_primitive( semantic, cast, operand, result );
   default:
      return false;
   }
}

static bool perform_cast_primitive( struct semantic* semantic,
   struct cast* cast, struct result* operand, struct result* result ) {
   bool valid = false;
   switch ( cast->spec ) {
   case SPEC_RAW:
   case SPEC_INT:
      switch ( operand->type.spec ) {
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
      switch ( operand->type.spec ) {
      case SPEC_RAW:
      case SPEC_INT:
         valid = true;
         break;
      default:
         valid = ( cast->spec == operand->type.spec );
      }
      break;
   default:
      break;
   }
   if ( ! valid ) {
      return false;
   }
   s_init_type_info_scalar( &result->type, cast->spec );
   result->complete = true;
   result->usable = true;
   // NOTE: Enumeration variables must not be modified via casts. Only
   // primitive variables can be modified.
   struct type_info revealed_type;
   s_init_type_info_copy( &revealed_type, &operand->type );
   s_reveal( &revealed_type );
   if ( s_describe_type( &revealed_type ) == TYPEDESC_PRIMITIVE ) {
      result->modifiable = operand->modifiable;
   }
   if ( operand->folded ) {
      result->value = operand->value;
      result->folded = true;
   }
   return true;
}

static void invalid_cast( struct semantic* semantic, struct cast* cast,
   struct result* operand ) {
   struct type_info cast_type;
   s_init_type_info( &cast_type, NULL, NULL, NULL, NULL, cast->spec,
      STORAGE_LOCAL );
   struct str cast_type_s;
   str_init( &cast_type_s );
   s_present_type( &cast_type, &cast_type_s );
   struct str operand_type_s;
   str_init( &operand_type_s );
   s_present_type( &operand->type, &operand_type_s );
   s_diag( semantic, DIAG_POS_ERR, &cast->pos,
      "operand (`%s`) cannot be cast to specified type (`%s`)",
       operand_type_s.value, cast_type_s.value );
   str_deinit( &cast_type_s );
   str_deinit( &operand_type_s );
}

static void test_suffix( struct semantic* semantic, struct expr_test* test,
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

static void test_subscript( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct subscript* subscript ) {
   struct result lside;
   init_result( &lside );
   test_suffix( semantic, test, &lside, subscript->lside );
   enum type_description desc = s_describe_type( &lside.type );
   if ( desc == TYPEDESC_ARRAYREF ) {
      test_subscript_array( semantic, test, result, &lside, subscript );
   }
   else if ( desc == TYPEDESC_PRIMITIVE && lside.type.spec == SPEC_STR ) {
      test_subscript_str( semantic, test, result, &lside, subscript );
   }
   else {
      struct str string;
      str_init( &string );
      s_present_type( &lside.type, &string );
      s_diag( semantic, DIAG_POS_ERR, &subscript->pos,
         "subscript operation not supported for given operand (`%s`)",
         string.value );
      str_deinit( &string );
      s_bail( semantic );
   }
}

static void test_subscript_array( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct result* lside,
   struct subscript* subscript ) {
   struct expr_test index;
   s_init_expr_test( &index, true, false );
   test_nested_expr( semantic, test, &index, subscript->index );
   // Index must be of integer type.
   if ( ! s_same_type( &index.type, &semantic->type_int ) ) {
      s_type_mismatch( semantic, "index", &index.type,
         "required", &semantic->type_int, &subscript->index->pos );
      s_bail( semantic );
   }
   // Out-of-bounds warning for a constant index.
   if ( lside->dim && lside->dim->length && subscript->index->folded ) {
      warn_bounds_violation( semantic, subscript, "dimension-length",
         lside->dim->length );
   }
   enum subscript_result element = s_subscript_array_ref( semantic,
      &lside->type, &result->type );
   switch ( element ) {
   case SUBSCRIPTRESULT_SUBARRAY:
      result->data_origin = lside->data_origin;
      if ( lside->dim ) {
         if ( subscript->index->folded ) {
            result->value = lside->value +
               lside->dim->element_size * subscript->index->value;
            result->folded = true;
         }
         result->dim = lside->dim->next;
      }
      break;
   case SUBSCRIPTRESULT_STRUCT:
      result->data_origin = lside->data_origin;
      break;
   case SUBSCRIPTRESULT_REF:
   case SUBSCRIPTRESULT_PRIMITIVE:
      result->modifiable = true;
   }
   result->usable = true;
   result->complete = true;
   if ( lside->type.ref->nullable ) {
      semantic->lib->uses_nullable_refs = true;
   }
}

static void warn_bounds_violation( struct semantic* semantic,
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

static void test_subscript_str( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct result* lside,
   struct subscript* subscript ) {
   struct expr_test index;
   s_init_expr_test( &index, true, false );
   test_nested_expr( semantic, test, &index, subscript->index );
   // Index must be of integer type.
   if ( ! s_same_type( &index.type, &semantic->type_int ) ) {
      s_type_mismatch( semantic, "index", &index.type,
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
   s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_INT ) );
   result->complete = true;
   result->usable = true;
   subscript->string = true;
}

static void test_access( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct access* access ) {
   struct result lside;
   init_result( &lside );
   test_suffix( semantic, test, &lside, access->lside );
   if ( s_is_struct_ref( &lside.type ) ) {
      test_access_struct( semantic, test, access, &lside, result );
   }
   else if ( is_ns( &lside ) ) {
      test_access_ns( semantic, test, access, &lside, result );
   }
   else if ( s_is_array_ref( &lside.type ) ) {
      test_access_array( semantic, test, access, &lside, result );
   }
   else if ( s_is_str( &lside.type ) ) {
      test_access_str( semantic, test, access, &lside, result );
   }
   else {
      struct str string;
      str_init( &string );
      s_present_type( &lside.type, &string ); 
      s_diag( semantic, DIAG_POS_ERR, &access->pos,
         "left operand (`%s`) does not support member access", string.value );
      str_deinit( &string );
      s_bail( semantic );
   }
}

static bool is_ns( struct result* result ) {
   return ( result->object && result->object->node.type == NODE_NAMESPACE );
}

static void test_access_struct( struct semantic* semantic,
   struct expr_test* test, struct access* access, struct result* lside,
   struct result* result ) {
   struct structure_member* member =
      get_structure_member( semantic, test, access, lside );
   access->rside = &member->object.node;
   select_structure_member( semantic, lside, result, member );
   result->data_origin = lside->data_origin;
   if ( lside->type.ref && lside->type.ref->nullable ) {
      semantic->lib->uses_nullable_refs = true;
   }
}

static struct structure_member* get_structure_member(
   struct semantic* semantic, struct expr_test* test, struct access* access,
   struct result* lside ) {
   struct name* name = t_extend_name( lside->type.structure->body,
      access->name );
   if ( ! ( name->object &&
      name->object->node.type == NODE_STRUCTURE_MEMBER ) ) {
      if ( lside->type.structure->anon ) {
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "`%s` not a member of anonymous struct", access->name );
      }
      else {
         struct str str;
         str_init( &str );
         t_copy_name( lside->type.structure->name, false, &str );
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "`%s` not a member of struct `%s`", access->name,
            str.value );
         str_deinit( &str );
      }
      s_bail( semantic );
   }
   if ( ! name->object->resolved ) {
      if ( semantic->trigger_err ) {
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "struct member (`%s`) undefined", access->name );
         s_bail( semantic );
      }
      else {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
   }
   return ( struct structure_member* ) name->object;
}

static void test_access_ns( struct semantic* semantic,
   struct expr_test* test, struct access* access, struct result* lside,
   struct result* result ) {
   struct ns* ns = ( struct ns* ) lside->object;
   struct object* object = s_get_ns_object( ns, access->name, NODE_NONE );
   if ( ! object ) {
      s_unknown_ns_object( semantic, ns, access->name, &access->pos );
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
   case NODE_FUNC:
      select_func( semantic, result,
         ( struct func* ) object );
      break;
   case NODE_NAMESPACE:
      select_ns( semantic, result,
         ( struct ns* ) object );
      break;
   case NODE_ALIAS:
      select_alias( semantic, test, result,
         ( struct alias* ) object );
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
   access->type = ACCESS_NAMESPACE;
   access->rside = &object->node;
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

static void test_access_array( struct semantic* semantic,
   struct expr_test* test, struct access* access, struct result* lside,
   struct result* result ) {
   struct name* name = t_extend_name( semantic->task->array_name, "." );
   name = t_extend_name( name, access->name );
   if ( ! name->object ) {
      s_diag( semantic, DIAG_POS_ERR, &access->pos,
         "`%s` not a member of the array type", access->name );
      s_bail( semantic );
   }
   switch ( name->object->node.type ) {
   case NODE_FUNC:
      select_func( semantic, result,
         ( struct func* ) name->object );
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
   access->type = ACCESS_ARRAY;
   access->rside = &name->object->node;
   if ( lside->type.ref && lside->type.ref->nullable ) {
      semantic->lib->uses_nullable_refs = true;
   }
}

static void test_access_str( struct semantic* semantic,
   struct expr_test* test, struct access* access, struct result* lside,
   struct result* result ) {
   struct name* name = t_extend_name( semantic->task->str_name, "." );
   name = t_extend_name( name, access->name );
   if ( ! name->object ) {
      s_diag( semantic, DIAG_POS_ERR, &access->pos,
         "`%s` not a member of the `str` type", access->name );
      s_bail( semantic );
   }
   switch ( name->object->node.type ) {
   case NODE_FUNC:
      select_func( semantic, result,
         ( struct func* ) name->object );
      break;
   default:
      UNREACHABLE();
      s_bail( semantic );
   }
   access->type = ACCESS_STR;
   access->rside = &name->object->node;
}

static void test_call( struct semantic* semantic, struct expr_test* expr_test,
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
         s_init_type_info( &result->type, operand.func->ref,
            operand.func->structure, operand.func->enumeration, NULL,
            operand.func->return_spec, STORAGE_LOCAL );
         result->usable = true;
      }
      // Primitive return-value.
      else {
         s_init_type_info( &result->type, NULL, NULL,
            operand.func->enumeration, NULL,
            s_spec( semantic, operand.func->return_spec ), STORAGE_LOCAL );
         s_decay( semantic, &result->type );
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
   else if ( operand.type.ref && operand.type.ref->type == REF_FUNCTION ) {
      struct ref_func* func = ( struct ref_func* ) operand.type.ref;
      // Reference return-value.
      if ( operand.type.ref->next ) {
         s_init_type_info( &result->type, operand.type.ref->next,
            operand.type.structure, operand.type.enumeration, NULL,
            operand.type.spec, operand.type.storage );
         result->usable = true;
      }
      // Primitive return-value.
      else {
         s_init_type_info( &result->type, NULL, NULL, operand.type.enumeration,
            NULL, s_spec( semantic, operand.type.spec ), STORAGE_LOCAL );
         s_decay( semantic, &result->type );
         result->usable = ( operand.type.spec != SPEC_VOID );
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

static void init_call_test( struct call_test* test, struct call* call ) {
   test->func = NULL;
   test->call = call;
   test->params = NULL;
   test->min_param = 0;
   test->max_param = 0;
   test->num_args = 0;
   test->format_param = false;
}

static void test_call_operand( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test, struct call* call,
   struct result* operand ) {
   init_result( operand );
   test_suffix( semantic, expr_test, operand, call->operand );
   if ( operand->func ) {
      test->func = operand->func;
      test->params = operand->func->params;
      test->min_param = operand->func->min_param;
      test->max_param = operand->func->max_param;
      test->format_param = ( operand->func->type == FUNC_FORMAT );
   }
   else if ( ! operand->type.dim && operand->type.ref &&
      operand->type.ref->type == REF_FUNCTION ) {
      struct ref_func* func = ( struct ref_func* ) operand->type.ref;
      test->params = func->params;
      test->min_param = func->min_param;
      test->max_param = func->max_param;
      if ( operand->type.ref->nullable ) {
         semantic->lib->uses_nullable_refs = true;
      }
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &call->pos,
         "operand not a function" );
      s_bail( semantic );
   }
}

static void test_call_format_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test, struct call* call ) {
   if ( test->format_param ) {
      if ( call->format_item ) {
         test_format_item_list( semantic, expr_test, call->format_item );
      }
      else if ( expr_test->buildmsg ) {
         test_buildmsg( semantic, expr_test, call );
      }
      else {
         s_diag( semantic, DIAG_POS_ERR, &call->pos,
            "function call missing message argument" );
         s_bail( semantic );
      }
   }
   else {
      if ( call->format_item ) {
         s_diag( semantic, DIAG_POS_ERR, &call->format_item->pos,
            "passing format-item to non-format function" );
         s_bail( semantic );
      }
   }
}

static void test_format_item_list( struct semantic* semantic,
   struct expr_test* test, struct format_item* item ) {
   while ( item ) {
      if ( item->cast == FCAST_ARRAY ) {
         test_array_format_item( semantic, test, item );
      }
      else {
         test_format_item( semantic, test, item );
      }
      item = item->next;
   }
}

static void test_format_item( struct semantic* semantic,
   struct expr_test* test, struct format_item* item ) {
   struct expr_test arg;
   s_init_expr_test( &arg, true, false );
   test_nested_expr( semantic, test, &arg, item->value );
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
   struct type_info required_type;
   s_init_type_info_scalar( &required_type, spec );
   if ( ! s_instance_of( &required_type, &arg.type ) ) {
      s_type_mismatch( semantic, "argument", &arg.type,
         "required", &required_type, &item->value->pos );
      s_bail( semantic );
   }
}

static void test_array_format_item( struct semantic* semantic,
   struct expr_test* test, struct format_item* item ) {
   struct expr_test arg;
   s_init_expr_test( &arg, false, false );
   test_nested_expr( semantic, test, &arg, item->value );
   static struct ref_array array = {
      { NULL, { 0, 0, 0 }, REF_ARRAY, true, false }, 1, STORAGE_MAP, 0 };
   struct type_info required_type;
   s_init_type_info( &required_type, &array.ref, NULL, NULL, NULL,
      s_spec( semantic, SPEC_INT ), STORAGE_MAP );
   if ( ! s_instance_of( &required_type, &arg.type ) ) {
      s_type_mismatch( semantic, "argument", &arg.type,
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

static void test_int_arg( struct semantic* semantic, struct expr_test* test,
   struct expr* expr ) {
   struct expr_test arg;
   s_init_expr_test( &arg, true, false );
   test_nested_expr( semantic, test, &arg, expr );
   struct type_info required_type;
   s_init_type_info_scalar( &required_type, SPEC_INT );
   if ( ! s_instance_of( &required_type, &arg.type ) ) {
      s_type_mismatch( semantic, "argument", &arg.type,
         "required", &required_type, &expr->pos );
      s_bail( semantic );
   }
}

static void test_buildmsg( struct semantic* semantic,
   struct expr_test* expr_test, struct call* call ) {
   struct buildmsg_usage* usage = mem_alloc( sizeof( *usage ) );
   usage->buildmsg = expr_test->buildmsg;
   usage->point = NULL;
   list_append( &expr_test->buildmsg->usages, usage );
   struct format_item* item = t_alloc_format_item();
   item->cast = FCAST_BUILDMSG;
   struct format_item_buildmsg* extra = mem_alloc( sizeof( *extra ) );
   extra->usage = usage;
   item->extra = extra;
   call->format_item = item;
}

static void add_nested_call( struct func* func, struct call* call ) {
   struct func_user* impl = func->impl;
   struct nested_call* nested = mem_alloc( sizeof( *nested ) );
   nested->next = impl->nested_calls;
   nested->id = 0;
   nested->prologue_jump = NULL;
   nested->return_point = NULL;
   impl->nested_calls = call;
   call->nested_call = nested;
}

static void test_call_args( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test ) {
   struct call* call = test->call;
   test_remaining_args( semantic, expr_test, test );
   // Number of arguments must be correct.
   if ( test->num_args < test->min_param ) {
      s_diag( semantic, DIAG_POS_ERR, &call->pos,
         "not enough %sarguments in function call",
         test->format_param ? "regular " : "" );
      struct str str;
      str_init( &str );
      present_func( semantic, test, &str );
      s_diag( semantic, DIAG_POS | DIAG_NOTE, &call->pos,
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
      present_func( semantic, test, &str );
      s_diag( semantic, DIAG_POS | DIAG_NOTE, &call->pos,
         "%s takes %s%d %sargument%s", str.value,
         test->min_param != test->max_param ? "at most " : "",
         test->max_param, test->format_param ? "regular " : "",
         test->max_param != 1 ? "s" : "" );
      s_bail( semantic );
   }
}

static void test_remaining_args( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test ) {
   struct param* param = test->params;
   struct list_iter i;
   list_iterate( &test->call->args, &i );
   while ( ! list_end( &i ) ) {
      test_remaining_arg( semantic, expr_test, test, param, list_data( &i ) );
      if ( param ) {
         param = param->next;
      }
      list_next( &i );
   }
}

static void test_remaining_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test, struct param* param,
   struct expr* expr ) {
   struct expr_test arg;
   s_init_expr_test( &arg, true, false );
   test_nested_expr( semantic, expr_test, &arg, expr );
   if ( param ) {
      struct type_info param_type;
      s_init_type_info( &param_type, param->ref, param->structure,
         param->enumeration, NULL, param->spec, STORAGE_LOCAL );
      if ( ! s_instance_of( &param_type, &arg.type ) ) {
         arg_mismatch( semantic, &expr->pos, &arg.type,
            "parameter", &param_type, "argument", test->num_args + 1 );
         s_bail( semantic );
      }
      if ( arg.func && arg.func->type == FUNC_USER ) {
         struct func_user* impl = arg.func->impl;
         if ( impl->local ) {
            s_diag( semantic, DIAG_POS_ERR, &expr->pos,
               "passing a local function as an argument" );
            s_bail( semantic );
         }
      }
   }
   ++test->num_args;
   if ( s_is_ref_type( &arg.type ) && arg.var ) {
      arg.var->addr_taken = true;
      if ( ! arg.var->hidden ) {
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

static void arg_mismatch( struct semantic* semantic, struct pos* pos,
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

static void present_func( struct semantic* semantic, struct call_test* test,
   struct str* msg ) {
   if ( test->func ) {
      if ( test->func->name &&
         test->func->name != semantic->task->blank_name ) {
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

static void test_call_func( struct semantic* semantic, struct call_test* test,
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
   else if ( test->func->type == FUNC_DED ) {
      test_call_ded( semantic, test, call );
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

static void test_call_ded( struct semantic* semantic, struct call_test* test,
   struct call* call ) {
   struct func_ded* impl = test->func->impl;
   // Latent functions cannot be called inside functions or message-building
   // blocks.
   if ( semantic->func_test && impl->latent ) {
      if ( s_in_msgbuild_block( semantic ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->pos,
            "calling a latent function inside a message-building block" );
         s_bail( semantic );
      }
      else if ( semantic->func_test->func ) {
         s_diag( semantic, DIAG_POS_ERR, &call->pos,
            "calling a latent function inside a function" );
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

static void test_sure( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct sure* sure ) {
   struct result operand;
   init_result( &operand );
   test_suffix( semantic, test, &operand, sure->operand );
   if ( ! s_is_ref_type( &operand.type ) ) {
      s_diag( semantic, DIAG_POS_ERR, &sure->pos,
         "operand not a reference" );
      s_bail( semantic );
   }
   if ( s_is_null( &operand.type ) ) {
      s_init_type_info_null( &result->type );
      result->complete = true;
      result->usable = true;
      semantic->lib->uses_nullable_refs = true;
   }
   else {
      sure->ref = s_dup_ref( operand.type.ref );
      if ( sure->ref->nullable ) {
         sure->ref->nullable = false;
         semantic->lib->uses_nullable_refs = true;
      }
      else {
         sure->already_safe = true;
      }
      s_init_type_info( &result->type, sure->ref, operand.type.structure,
         operand.type.enumeration, NULL, operand.type.spec,
         operand.type.storage );
      result->data_origin = operand.data_origin;
      result->complete = true;
      result->usable = true;
   }
}

static void test_primary( struct semantic* semantic, struct expr_test* test,
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
      test_null( result );
      break;
   case NODE_MAGICID:
      test_magic_id( semantic, test, result,
         ( struct magic_id* ) node );
      break;
   default:
      UNREACHABLE();
   }
}

static void test_literal( struct semantic* semantic, struct result* result,
   struct literal* literal ) {
   s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_INT ) );
   result->value = literal->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

static void test_fixed_literal( struct semantic* semantic,
   struct result* result, struct fixed_literal* literal ) {
   s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_FIXED ) );
   result->value = literal->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

static void test_string_usage( struct semantic* semantic,
   struct expr_test* test, struct result* result,
   struct indexed_string_usage* usage ) {
   test_string( semantic, test, result, usage->string );
}

static void test_string( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct indexed_string* string ) {
   s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_STR ) );
   result->value = string->index;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   test->has_str = true;
}

static void test_boolean( struct semantic* semantic, struct result* result,
   struct boolean* boolean ) {
   s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_BOOL ) );
   result->value = boolean->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

static void test_name_usage( struct semantic* semantic, struct expr_test* test,
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

static void test_found_object( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct name_usage* usage,
   struct object* object ) {
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
   select_object( semantic, test, result, object );
   usage->object = &object->node;
}

static struct ref* find_map_ref( struct ref* ref,
   struct structure* structure ) {
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

static struct ref* find_map_ref_struct( struct structure* structure ) {
   struct ref* ref = NULL;
   struct structure_member* member = structure->member;
   while ( member && ! ref ) {
      ref = find_map_ref( member->ref, ( member->spec == SPEC_STRUCT ) ?
         member->structure : NULL );
      member = member->next;
   }
   return ref;
}

static void select_object( struct semantic* semantic, struct expr_test* test,
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
   case NODE_FUNC:
      select_func( semantic, result,
         ( struct func* ) object );
      break;
   case NODE_NAMESPACE:
      select_ns( semantic, result,
         ( struct ns* ) object );
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

static void select_constant( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct constant* constant ) {
   s_init_type_info_scalar( &result->type,
      s_spec( semantic, constant->spec ) );
   result->value = constant->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   if ( constant->has_str ) {
      test->has_str = true;
   }
}

static void select_enumerator( struct semantic* semantic,
   struct expr_test* test, struct result* result,
   struct enumerator* enumerator ) {
   s_init_type_info( &result->type, NULL, NULL, enumerator->enumeration, NULL,
      s_spec( semantic, enumerator->enumeration->base_type ), STORAGE_LOCAL );
   s_decay( semantic, &result->type );
   result->value = enumerator->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   if ( enumerator->has_str ) {
      test->has_str = true;
   }
}

static void select_var( struct semantic* semantic, struct result* result,
   struct var* var ) {
   // Array.
   if ( var->dim ) {
      s_init_type_info( &result->type, var->ref, var->structure,
         var->enumeration, var->dim, var->spec, var->storage );
      s_decay( semantic, &result->type );
      result->data_origin = var;
      result->dim = var->dim;
      if ( var->storage == STORAGE_MAP ) {
         result->folded = true;
      }
   }
   // Reference variable.
   else if ( var->ref ) {
      s_init_type_info( &result->type, var->ref, var->structure,
         var->enumeration, NULL, var->spec, var->storage );
      result->modifiable = ( ! var->constant );
   }
   // Structure variable.
   else if ( var->structure ) {
      result->data_origin = var;
      s_init_type_info( &result->type, NULL, var->structure, NULL, NULL,
         var->spec, var->storage );
      s_decay( semantic, &result->type );
      if ( var->storage == STORAGE_MAP ) {
         result->folded = true;
      }
   }
   // Primitive variable.
   else {
      s_init_type_info( &result->type, NULL, NULL, var->enumeration, NULL,
         s_spec( semantic, var->spec ), var->storage );
      s_decay( semantic, &result->type );
      result->modifiable = ( ! var->constant );
   }
   result->object = &var->object;
   result->complete = true;
   result->usable = true;
   var->used = true;
}

static void select_param( struct semantic* semantic, struct result* result,
   struct param* param ) {
   // Reference parameter.
   if ( param->ref ) {
      s_init_type_info( &result->type, param->ref, param->structure,
         param->enumeration, NULL, param->spec, STORAGE_LOCAL );
   }
   // Primitive parameter.
   else {
      s_init_type_info( &result->type, NULL, NULL, param->enumeration, NULL,
         s_spec( semantic, param->spec ), STORAGE_LOCAL );
      s_decay( semantic, &result->type );
   }
   result->complete = true;
   result->usable = true;
   result->modifiable = true;
   param->used = true;
}

static void select_structure_member( struct semantic* semantic,
   struct result* lside, struct result* result,
   struct structure_member* member ) {
   int storage = lside->type.storage;
   if ( lside->type.ref ) {
      struct ref_struct* structure = ( struct ref_struct* ) lside->type.ref;
      storage = structure->storage;
   }
   // Array member.
   if ( member->dim ) {
      s_init_type_info( &result->type, member->ref, member->structure,
         member->enumeration, member->dim, member->spec, storage );
      s_decay( semantic, &result->type );
      result->dim = member->dim;
   }
   // Reference member.
   else if ( member->ref ) {
      s_init_type_info( &result->type, member->ref, member->structure,
         member->enumeration, NULL, member->spec, storage );
      result->modifiable = true;
   }
   // Structure member.
   else if ( member->structure ) {
      s_init_type_info( &result->type, NULL, member->structure, NULL, NULL,
         member->spec, storage );
      s_decay( semantic, &result->type );
   }
   // Primitive member.
   else {
      s_init_type_info( &result->type, NULL, NULL, member->enumeration, NULL,
         s_spec( semantic, member->spec ), storage );
      s_decay( semantic, &result->type );
      result->modifiable = true;
   }
   result->complete = true;
   result->usable = true;
}

static void select_func( struct semantic* semantic, struct result* result,
   struct func* func ) {
   if ( func->type == FUNC_USER ) {
      struct func_user* impl = func->impl;
      s_init_type_info_func( &result->type, func->ref, func->structure,
         func->enumeration, func->params, func->return_spec, func->min_param,
         func->max_param, impl->local );
      result->usable = true;
      result->folded = true;
      result->complete = true;
      ++impl->usage;
   }
   // When an action-special is not called, it decays into an integer value.
   // The value is the ID of the action-special.
   else if ( func->type == FUNC_ASPEC ) {
      s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_INT ) );
      struct func_aspec* impl = func->impl;
      result->value = impl->id;
      result->usable = true;
      result->folded = true;
      result->complete = true;
   }
   // An extension function can also decay into an integer.
   else if ( func->type == FUNC_EXT ) {
      s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_INT ) );
      struct func_ext* impl = func->impl;
      result->value = -impl->id;
      result->usable = true;
      result->folded = true;
      result->complete = true;
   }
   else {
      s_init_type_info_builtin_func( &result->type );
   }
   result->func = func;
}

static void select_ns( struct semantic* semantic, struct result* result,
   struct ns* ns ) {
   result->object = &ns->object;
}

static void select_alias( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct alias* alias ) {
   select_object( semantic, test, result, alias->target );
}

static void test_strcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct strcpy_call* call ) {
   // Array.
   struct expr_test arg;
   s_init_expr_test( &arg, false, false );
   test_nested_expr( semantic, test, &arg, call->array );
   if ( ! s_is_onedim_int_array_ref( semantic, &arg.type ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->array->pos,
         "array argument not a one-dimensional, "
         "integer element-type array-reference" );
      s_bail( semantic );
   }
   // Array-offset.
   if ( call->array_offset ) {
      s_init_expr_test( &arg, true, false );
      test_nested_expr( semantic, test, &arg, call->array_offset );
      if ( ! s_is_int_value( &arg.type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->array_offset->pos,
            "array-offset argument not an integer value" );
         s_bail( semantic );
      }
      // Array-length.
      if ( call->array_length ) {
         s_init_expr_test( &arg, true, false );
         test_nested_expr( semantic, test, &arg, call->array_length );
         if ( ! s_is_int_value( &arg.type ) ) {
            s_diag( semantic, DIAG_POS_ERR, &call->array_length->pos,
               "array-length argument not an integer value" );
            s_bail( semantic );
         }
      }
   }
   // String.
   s_init_expr_test( &arg, true, false );
   test_nested_expr( semantic, test, &arg, call->string );
   if ( ! s_is_str_value( &arg.type ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->string->pos,
         "string argument not a string value" );
      s_bail( semantic );
   }
   // String-offset.
   if ( call->offset ) {
      s_init_expr_test( &arg, true, false );
      test_nested_expr( semantic, test, &arg, call->offset );
      if ( ! s_is_int_value( &arg.type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->offset->pos,
            "string-offset argument not an integer value" );
         s_bail( semantic );
      }
   }
   s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_BOOL ) );
   result->complete = true;
   result->usable = true;
}

static void test_memcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct memcpy_call* call ) {
   // Destination.
   struct expr_test dst;
   s_init_expr_test( &dst, false, false );
   test_nested_expr( semantic, test, &dst, call->destination );
   if ( ! ( s_is_array_ref( &dst.type ) || s_is_struct( &dst.type ) ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->destination->pos,
         "destination not an array or structure" );
      s_bail( semantic );
   }
   // It doesn't look pretty if we allow struct variables to be specified with
   // the array format-item, so make sure the argument is an array.
   if ( call->array_cast && ! s_is_array_ref( &dst.type ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->destination->pos,
         "destination not an array" );
      s_bail( semantic );
   }
   // Destination-offset.
   if ( call->destination_offset ) {
      struct expr_test arg;
      s_init_expr_test( &arg, true, false );
      test_nested_expr( semantic, test, &arg, call->destination_offset );
      if ( ! s_is_int_value( &arg.type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->destination_offset->pos,
            "destination-offset not an integer value" );
         s_bail( semantic );
      }
      // Destination-length.
      if ( call->destination_length ) {
         s_init_expr_test( &arg, true, false );
         test_nested_expr( semantic, test, &arg, call->destination_length );
         if ( ! s_is_int_value( &arg.type ) ) {
            s_diag( semantic, DIAG_POS_ERR, &call->destination_length->pos,
               "destination-length not an integer value" );
            s_bail( semantic );
         }
      }
   }
   // Source.
   struct expr_test src;
   s_init_expr_test( &src, false, false );
   test_nested_expr( semantic, test, &src, call->source );
   if ( ! s_same_type( &src.type, &dst.type ) ) {
      s_type_mismatch( semantic, "source", &src.type,
         "destination", &dst.type, &call->source->pos );
      s_bail( semantic );
   }
   // Source-offset.
   if ( call->source_offset ) {
      struct expr_test arg;
      s_init_expr_test( &arg, true, false );
      test_nested_expr( semantic, test, &arg, call->source_offset );
      if ( ! s_is_int_value( &arg.type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->source_offset->pos,
            "source-offset not an integer value" );
         s_bail( semantic );
      }
      if ( ! s_is_array_ref( &dst.type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->source_offset->pos,
            "source-offset specified for non-array source" );
         s_bail( semantic );
      }
   }
   s_init_type_info_scalar( &result->type, s_spec( semantic, SPEC_BOOL ) );
   result->complete = true;
   result->usable = true;
   if ( s_is_struct( &dst.type ) ) {
      call->type = MEMCPY_STRUCT;
   }
}

static void test_conversion( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct conversion* conv ) {
   struct expr_test operand;
   s_init_expr_test( &operand, true, false );
   test_nested_expr( semantic, test, &operand, conv->expr );
   if ( ! perform_conversion( semantic, conv, &operand, result ) ) {
      unsupported_conversion( semantic, conv, &operand );
      s_bail( semantic );
   }
}

static bool perform_conversion( struct semantic* semantic,
   struct conversion* conv, struct expr_test* operand,
   struct result* result ) {
   switch ( s_describe_type( &operand->type ) ) {
   case TYPEDESC_PRIMITIVE:
      switch ( conv->spec ) {
      // Converting from a string to either an integer or a fixed-point number
      // requires a lot of generated code. For now, do not do these conversions
      // as part of the compiler. Instead, use a library function to perform
      // these conversions.
      case SPEC_INT:
      case SPEC_FIXED:
         switch ( operand->type.spec ) {
         case SPEC_INT:
         case SPEC_FIXED:
         case SPEC_BOOL:
            conv->spec_from = operand->type.spec;
            s_init_type_info_scalar( &result->type,
               s_spec( semantic, conv->spec ) );
            result->complete = true;
            result->usable = true;
            return true;
         default:
            break;
         }
         break;
      case SPEC_STR:
      case SPEC_BOOL:
         switch ( operand->type.spec ) {
         case SPEC_INT:
         case SPEC_FIXED:
         case SPEC_BOOL:
         case SPEC_STR:
            conv->spec_from = operand->type.spec;
            s_init_type_info_scalar( &result->type,
               s_spec( semantic, conv->spec ) );
            result->complete = true;
            result->usable = true;
            return true;
         default:
            break;
         }
         break;
      default:
         break;
      }
      return false;
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
   case TYPEDESC_NULLREF:
      switch ( conv->spec ) {
      case SPEC_BOOL:
         conv->from_ref = true;
         s_init_type_info_scalar( &result->type,
            s_spec( semantic, conv->spec ) );
         result->complete = true;
         result->usable = true;
         return true;
      default:
         break;
      }
      return false;
   default:
      return false;
   }
}

static void unsupported_conversion( struct semantic* semantic,
   struct conversion* conv, struct expr_test* operand ) {
   struct str from;
   str_init( &from );
   s_present_type( &operand->type, &from );
   struct type_info type;
   s_init_type_info_scalar( &type, conv->spec );
   struct str to;
   str_init( &to );
   s_present_type( &type, &to );
   s_diag( semantic, DIAG_POS_ERR, &conv->expr->pos,
      "conversion from `%s` to `%s` unsupported", from.value, to.value );
   str_deinit( &from );
   str_deinit( &to );
}

static void test_compound_literal( struct semantic* semantic,
   struct expr_test* test, struct result* result,
   struct compound_literal* literal ) {
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

static void test_anon_func( struct semantic* semantic, struct result* result,
   struct func* func ) {
   if ( semantic->in_localscope ) {
      s_test_nested_func( semantic, func );
   }
   select_func( semantic, result, func );
}

static void test_paren( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct paren* paren ) {
   result->in_paren = true;
   test_operand( semantic, test, result, paren->inside );
   result->in_paren = false;
}

static void test_upmost( struct semantic* semantic, struct result* result ) {
   result->object = &semantic->task->upmost_ns->object;
}

static void test_current_namespace( struct semantic* semantic,
   struct result* result ) {
   result->object = &semantic->ns->object;
}

static void test_null( struct result* result ) {
   s_init_type_info_null( &result->type );
   result->complete = true;
   result->usable = true;
   result->folded = true;
}

static void test_magic_id( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct magic_id* magic_id ) {
   expand_magic_id( semantic, magic_id );
   test_string( semantic, test, result, magic_id->string );
}

static void expand_magic_id( struct semantic* semantic,
   struct magic_id* magic_id ) {
   struct str name;
   str_init( &name );
   switch ( magic_id->name ) {
   case MAGICID_NAMESPACE:
      if ( semantic->ns == semantic->task->upmost_ns ) {
         str_append( &name, "" );
      }
      else {
         t_copy_name( semantic->ns->name, true, &name );
      }
      break;
   case MAGICID_FUNCTION:
      if ( ! ( semantic->func_test && semantic->func_test->func ) ) {
         s_diag( semantic, DIAG_POS_ERR, &magic_id->pos,
            "using %s outside a function",
            p_get_token_info( TK_FUNCTIONNAME )->shared_text );
         s_bail( semantic );
      }
      if ( semantic->func_test->func->name ) {
         t_copy_name( semantic->func_test->func->name, false, &name );
      }
      else {
         // TODO: Create a nicer name.
         str_append( &name, "" );
      }
      break;
   case MAGICID_SCRIPT:
      if ( ! ( semantic->func_test && semantic->func_test->script ) ) {
         s_diag( semantic, DIAG_POS_ERR, &magic_id->pos,
            "using %s outside a script",
            p_get_token_info( TK_SCRIPTNAME )->shared_text );
         s_bail( semantic );
      }
      if ( semantic->func_test->script->named_script ) {
         struct indexed_string* string = t_lookup_string( semantic->task,
            semantic->func_test->script->number->value );
         str_append( &name, string->value );
      }
      else {
         str_append_number( &name,
            semantic->func_test->script->number->value );
      }
      break;
   }
   magic_id->string = t_intern_string_copy( semantic->task,
      name.value, name.length );
   str_deinit( &name );
}
