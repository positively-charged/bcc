#include "phase.h"

struct result {
   struct func* func;
   struct var* data_origin;
   struct ref* ref;
   struct enumeration* enumeration;
   struct structure* structure;
   struct dim* dim;
   struct ns* ns;
   int ref_dim;
   int spec;
   int value;
   bool complete;
   bool usable;
   bool assignable;
   bool folded;
   bool in_paren;
};

struct call_test {
   struct call* call;
   struct func* func;
   list_iter_t* i;
   int num_args;
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
static bool perform_bop( struct binary* binary, struct result* operand,
   struct result* result );
static void fold_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result );
static void test_logical( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct logical* logical );
static bool perform_logical( struct logical* logical, struct result* lside,
   struct result* rside, struct result* result );
static bool can_convert_to_boolean( struct result* operand );
static void fold_logical( struct logical* logical, struct result* lside,
   struct result* rside, struct result* result );
static void test_assign( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct assign* assign );
static bool perform_assign( struct assign* assign, struct result* lside,
   struct result* result );
static void test_conditional( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct conditional* cond );
static void test_prefix( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_unary( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct unary* unary );
static bool perform_unary( struct unary* unary, struct result* operand,
   struct result* result );
static void fold_unary( struct semantic* semantic, struct unary* unary,
   struct result* operand, struct result* result );
static void fold_unary_minus( struct result* operand, struct result* result );
static void fold_logical_not( struct semantic* semantic,
   struct result* operand, struct result* result );
static void test_inc( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct inc* inc );
static bool perform_inc( struct inc* inc, struct result* operand,
   struct result* result );
static void test_cast( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct cast* cast );
static bool valid_cast( struct cast* cast, struct result* operand );
static void invalid_cast( struct semantic* semantic, struct cast* cast,
   struct result* operand );
static void test_suffix( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_subscript( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct subscript* subscript );
static bool is_array_ref( struct result* result );
static void test_access( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct access* access );
static bool is_struct_ref( struct result* result );
static void unknown_member( struct semantic* semantic, struct access* access,
   struct object* object );
static void test_call( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct call* call );
static void test_call_args( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test );
static void test_call_first_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test );
static void test_call_format_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test );
static void test_format_item_list( struct semantic* semantic,
   struct expr_test* expr_test, struct format_item* item );
static void test_format_item( struct semantic* semantic,
   struct expr_test* expr_test, struct format_item* item );
static void test_array_format_item( struct semantic* semantic,
   struct expr_test* expr_test, struct format_item* item );
static void test_remaining_args( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test );
static void arg_mismatch( struct semantic* semantic, struct pos* pos,
   struct result* result, struct result* required_result, int number );
static void test_primary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_literal( struct result* result, struct literal* literal );
static void test_fixed_literal( struct result* result,
   struct fixed_literal* literal );
static void test_string_usage( struct expr_test* test, struct result* result,
   struct indexed_string_usage* usage );
static void test_boolean( struct result* result, struct boolean* boolean );
static void test_name_usage( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage );
static struct object* find_referenced_object( struct semantic* semantic,
   struct name_usage* usage );
static void select_object( struct expr_test* test, struct result* result,
   struct object* object );
static void select_constant( struct result* result,
   struct constant* constant );
static void select_enumerator( struct result* result,
   struct enumerator* enumerator );
static void select_enumeration( struct result* result,
   struct enumeration* enumeration );
static void select_var( struct expr_test* test, struct result* result,
   struct var* var );
static void select_param( struct result* result, struct param* param );
static void select_member( struct expr_test* test, struct result* result,
   struct structure_member* member );
static void select_func( struct result* result, struct func* func );
static void select_namespace( struct result* result, struct ns* ns );
static void test_strcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct strcpy_call* call );
static bool is_onedim_intelem_array_ref( struct result* result );
static bool is_int_value( struct result* result );
static bool is_str_value( struct result* result );
static void test_paren( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct paren* paren );
static void test_upmost( struct semantic* semantic, struct result* result );
static void init_type_info( struct type_info* type, struct result* result );
static bool same_type( struct result* a, struct result* b );
static bool is_value_type( struct result* result );
static bool is_ref_type( struct result* result );

void s_init_expr_test( struct expr_test* test, struct stmt_test* stmt_test,
   struct block* format_block, bool result_required,
   bool suggest_paren_assign ) {
   test->stmt_test = stmt_test;
   test->format_block = format_block;
   test->format_block_usage = NULL;
   test->result_required = result_required;
   test->has_string = false;
   test->undef_erred = false;
   test->suggest_paren_assign = suggest_paren_assign;
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
      init_type_info( result_type, &result );
   }
   else {
      test->undef_erred = true;
   }
}

void s_test_cond( struct semantic* semantic, struct expr* expr ) {
   struct expr_test test;
   s_init_expr_test( &test, NULL, NULL, true, true );
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
   expr->spec = result->spec;
   expr->folded = result->folded;
   expr->value = result->value;
}

void test_nested_root( struct semantic* semantic, struct expr_test* parent,
   struct expr_test* test, struct result* result, struct expr* expr ) {
   if ( setjmp( test->bail ) == 0 ) {
      test_root( semantic, test, result, expr );
      parent->has_string = test->has_string;
   }
   else {
      longjmp( parent->bail, 1 );
   }
}

void init_result( struct result* result ) {
   result->func = NULL;
   result->data_origin = NULL;
   result->ref = NULL;
   result->enumeration = NULL;
   result->structure = NULL;
   result->dim = NULL;
   result->ns = NULL;
   result->ref_dim = 0;
   result->spec = SPEC_VOID;
   result->value = 0;
   result->complete = false;
   result->usable = false;
   result->assignable = false;
   result->folded = false;
   result->in_paren = false;
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
   if ( ! same_type( &lside, &rside ) ) {
      s_diag( semantic, DIAG_POS_ERR, &binary->pos,
         "left operand and right operand of different type" );
      s_bail( semantic );
   }
   if ( ! perform_bop( binary, &lside, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &binary->pos,
         "invalid binary operation" );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( lside.folded && rside.folded ) {
      fold_bop( semantic, binary, &lside, &rside, result );
   }
}

bool perform_bop( struct binary* binary, struct result* operand,
   struct result* result ) {
   // Value type.
   if ( is_value_type( operand ) ) {
      int spec = SPEC_NONE;
      switch ( binary->op ) {
      case BOP_EQ:
      case BOP_NEQ:
         switch ( operand->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
         case SPEC_ZBOOL:
         case SPEC_ZSTR:
            spec = SPEC_ZBOOL;
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
         switch ( operand->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
            spec = operand->spec;
            break;
         default:
            break;
         }
         break;
      case BOP_LT:
      case BOP_LTE:
      case BOP_GT:
      case BOP_GTE:
         switch ( operand->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
         case SPEC_ZSTR:
            spec = SPEC_ZBOOL;
            break;
         default:
            break;
         }
         break;
      case BOP_ADD:
         switch ( operand->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
         case SPEC_ZSTR:
            spec = operand->spec;
            break;
         default:
            break;
         }
         break;
      case BOP_SUB:
      case BOP_MUL:
      case BOP_DIV:
         switch ( operand->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
            spec = operand->spec;
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
      binary->lside_spec = operand->spec;
      return true;
   }
   // Reference type.
   else {
      switch ( binary->op ) {
      case BOP_EQ:
      case BOP_NEQ:
         result->spec = SPEC_ZBOOL;
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
   binary->folded = true;
   binary->value = l;
   result->folded = true;
   result->value = l;
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
   if ( ! perform_logical( logical, &lside, &rside, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &logical->pos,
         "invalid logical operation" );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( lside.folded && rside.folded ) {
      fold_logical( logical, &lside, &rside, result );
   }
}

bool perform_logical( struct logical* logical, struct result* lside,
   struct result* rside, struct result* result ) {
   if ( can_convert_to_boolean( lside ) && can_convert_to_boolean( rside ) ) {
      result->spec = SPEC_ZBOOL;
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
      case SPEC_ZRAW:
      case SPEC_ZINT:
      case SPEC_ZFIXED:
      case SPEC_ZBOOL:
      case SPEC_ZSTR:
      case SPEC_ENUM:
         return true;
      default:
         return false;
      }
   }
}

void fold_logical( struct logical* logical, struct result* lside,
   struct result* rside, struct result* result ) {
   // Get value of left side.
   int l = 0;
   switch ( lside->spec ) {
   case SPEC_ZRAW:
   case SPEC_ZINT:
   case SPEC_ZFIXED:
   case SPEC_ZBOOL:
      l = lside->value;
      break;
   case SPEC_ZSTR:
      // TODO: Implement.
      return;
   default:
      UNREACHABLE()
   }
   // Get value of right side.
   int r = 0;
   switch ( lside->spec ) {
   case SPEC_ZRAW:
   case SPEC_ZINT:
   case SPEC_ZFIXED:
   case SPEC_ZBOOL:
      r = lside->value;
      break;
   case SPEC_ZSTR:
      // TODO: Implement.
      return;
   default:
      UNREACHABLE()
   }
   int value = 0;
   switch ( logical->op ) {
   case LOP_OR: value = ( l || r ); break;
   case LOP_AND: value = ( l && r ); break;
   default:
      UNREACHABLE()
   }
   result->folded = true;
   result->value = value;
   logical->folded = true;
   logical->value = value;
}

void test_assign( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct assign* assign ) {
   struct result lside;
   init_result( &lside );
   test_operand( semantic, test, &lside, assign->lside );
   if ( ! lside.assignable ) {
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
   if ( lside.enumeration ) {
      lside.spec = SPEC_ENUM;
   }
   if ( rside.enumeration ) {
      rside.spec = SPEC_ENUM;
   }
   if ( ! same_type( &lside, &rside ) ) {
      s_diag( semantic, DIAG_POS_ERR, &assign->pos,
         "left operand and right operand of different type" );
      s_bail( semantic );
   }
   if ( ! perform_assign( assign, &lside, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &assign->pos,
         "invalid assignment operation" );
      s_bail( semantic );
   }
   if ( rside.data_origin ) {
      // At this time, a reference can be made only to private data.
      if ( ! rside.data_origin->ref && ! rside.data_origin->hidden ) {
         s_diag( semantic, DIAG_POS_ERR, &assign->pos,
            "right operand not a reference to a private variable" );
         s_bail( semantic );
      }
      rside.data_origin->addr_taken = true;
   }
   // To avoid the error where the user wanted equality operator but instead
   // typed in the assignment operator, suggest that assignment be wrapped in
   // parentheses.
   if ( test->suggest_paren_assign && ! result->in_paren ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS, &assign->pos,
         "assignment operation not in parentheses" );
   }
}

bool perform_assign( struct assign* assign, struct result* lside,
   struct result* result ) {
   // Value type.
   if ( is_value_type( lside ) ) {
      bool valid = false;
      switch ( assign->op ) {
      case AOP_NONE:
         switch ( lside->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
         case SPEC_ZBOOL:
         case SPEC_ZSTR:
         case SPEC_ENUM:
            valid = true;
            break;
         default:
            break;
         }
         break;
      case AOP_ADD:
         switch ( lside->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
         case SPEC_ZSTR:
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
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
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
         case SPEC_ZRAW:
         case SPEC_ZINT:
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
   struct result middle = left;
   if ( cond->middle ) {
      init_result( &middle );
      test_operand( semantic, test, &middle, cond->middle );
   }
   struct result right;
   init_result( &right );
   test_operand( semantic, test, &right, cond->right );
   if ( ! same_type( &middle, &right ) ) {
      s_diag( semantic, DIAG_POS_ERR, &cond->pos,
         "%s operand and right operand of different type",
         cond->middle ? "middle" : "left" );
      s_bail( semantic );
   }
   result->ref = right.ref;
   result->structure = right.structure;
   result->dim = right.dim;
   result->spec = right.spec;
   result->complete = true;
   result->usable = ( right.spec != SPEC_VOID );
   if ( ! result->dim && result->ref ) {
      if ( result->ref->type == REF_ARRAY ) {
         struct ref_array* part = ( struct ref_array* ) result->ref;
         result->ref_dim = part->dim_count;
      }
   }
   // Compile-time evaluation.
   if ( left.folded && middle.folded && right.folded ) {
      result->value = left.value ? middle.value : right.value;
      result->folded = true;
      cond->left_value = left.value;
      cond->folded = true;
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
   if ( ! perform_unary( unary, &operand, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &unary->pos,
         "invalid unary operation" );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( operand.folded ) {
      fold_unary( semantic, unary, &operand, result );
   }
}

bool perform_unary( struct unary* unary, struct result* operand,
   struct result* result ) {
   // Value type.
   if ( is_value_type( operand ) ) {
      int spec = SPEC_NONE;
      switch ( unary->op ) {
      case UOP_MINUS:
      case UOP_PLUS:
         switch ( operand->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
            spec = operand->spec;
         default:
            break;
         }
         break;
      case UOP_BIT_NOT:
         switch ( operand->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
            spec = operand->spec;
         default:
            break;
         }
         break;
      case UOP_LOG_NOT:
         switch ( operand->spec ) {
         case SPEC_ZRAW:
            spec = SPEC_ZRAW;
            break;
         default:
            spec = SPEC_ZBOOL;
            break;
         }
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
         result->spec = SPEC_ZBOOL;
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
   case SPEC_ZRAW:
   case SPEC_ZINT:
   case SPEC_ZFIXED:
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
   case SPEC_ZRAW:
   case SPEC_ZINT:
   case SPEC_ZFIXED:
   case SPEC_ZBOOL:
      result->value = ( ! operand->value );
      result->folded = true;
      break;
   case SPEC_ZSTR: {
      struct indexed_string* string = t_lookup_string( semantic->task,
         operand->value );
      result->value = ( string->length == 0 );
      result->folded = true;
      break; } 
   default:
      break;
   }
}

void test_inc( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct inc* inc ) {
   struct result operand;
   init_result( &operand );
   test_operand( semantic, test, &operand, inc->operand );
   if ( ! operand.assignable ) {
      s_diag( semantic, DIAG_POS_ERR, &inc->pos,
         "operand cannot be %s", inc->dec ?
            "decremented" : "incremented" );
      s_bail( semantic );
   }
   if ( ! perform_inc( inc, &operand, result ) ) {
      s_diag( semantic, DIAG_POS_ERR, &inc->pos,
         "invalid %s operation", inc->dec ?
            "decrement" : "increment" );
      s_bail( semantic );
   }
}

bool perform_inc( struct inc* inc, struct result* operand,
   struct result* result ) {
   // Value type.
   if ( is_value_type( operand ) ) {
      switch ( operand->spec ) {
      case SPEC_ZRAW:
      case SPEC_ZINT:
      case SPEC_ZFIXED:
         break;
      default:
         return false;
      }
      result->spec = operand->spec;
      result->complete = true;
      result->usable = true;
      inc->zfixed = ( operand->spec == SPEC_ZFIXED );
      return true;
   }
   // Reference type.
   else {
      return false;
   }
}

void test_cast( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct cast* cast ) {
   test_operand( semantic, test, result, cast->operand );
   if ( ! result->usable ) {
      s_diag( semantic, DIAG_POS_ERR, &cast->pos,
         "cast operand not a value" );
      s_bail( semantic );
   }
   if ( ! valid_cast( cast, result ) ) {
      invalid_cast( semantic, cast, result );
      s_bail( semantic );
   }
   result->spec = cast->spec;
}

bool valid_cast( struct cast* cast, struct result* operand ) {
   // Value type.
   if ( is_value_type( operand ) ) {
      bool valid = false;
      switch ( cast->spec ) {
      case SPEC_ZRAW:
      case SPEC_ZINT:
         switch ( operand->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
         case SPEC_ZBOOL:
         case SPEC_ZSTR:
            valid = true;
            break;
         default:
            break;
         }
         break;
      case SPEC_ZFIXED:
      case SPEC_ZBOOL:
      case SPEC_ZSTR:
         switch ( operand->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
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
      switch ( cast->spec ) {
      case SPEC_ZRAW:
      case SPEC_ZINT:
         return true;
      default:
         return false;
      }
   }
}

// TODO: Change error message.
void invalid_cast( struct semantic* semantic, struct cast* cast,
   struct result* operand ) {
   struct type_info cast_type;
   s_init_type_info( &cast_type, cast->spec, NULL, NULL, NULL, NULL );
   struct type_info operand_type;
   init_type_info( &operand_type, operand );
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
   if ( ! is_array_ref( &lside ) ) {
      s_diag( semantic, DIAG_POS_ERR, &subscript->pos,
         "operand not an array reference" );
      s_bail( semantic );
   }
   struct expr_test index;
   s_init_expr_test( &index, test->stmt_test, test->format_block, true,
      test->suggest_paren_assign );
   s_test_expr( semantic, &index, subscript->index );
   // Index must be of integer type.
   if ( ! ( subscript->index->spec == SPEC_ZRAW ||
      subscript->index->spec == SPEC_ZINT ) ) {
      s_diag( semantic, DIAG_POS_ERR, &subscript->pos,
         "index of non-integer type" );
      s_bail( semantic );
   }
   // Out-of-bounds warning for a constant index.
   if ( lside.dim && lside.dim->size && subscript->index->folded && (
      subscript->index->value < 0 ||
      subscript->index->value >= lside.dim->size ) ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS, &subscript->index->pos,
         "index out of bounds" );
   }
   result->ref = lside.ref;
   result->structure = lside.structure;
   result->dim = lside.dim;
   result->ref_dim = lside.ref_dim;
   result->spec = lside.spec;
   result->usable = true;
   result->complete = true;
   // Move past the current dimension.
   bool reached_element = false;
   if ( result->dim ) {
      result->dim = result->dim->next;
      reached_element = ( result->dim == NULL );
   }
   else {
      --result->ref_dim;
      if ( result->ref_dim == 0 ) {
         result->ref = result->ref->next;
         reached_element = true;
      }
   }
   // When all dimensions are tested, propogate properties of array element.
   if ( reached_element ) {
      // Reference element.
      if ( result->ref ) {
         if ( result->ref->type == REF_ARRAY ) {
            struct ref_array* part = ( struct ref_array* ) result->ref;
            result->ref_dim = part->dim_count;
         }
         result->assignable = true;
         result->complete = true;
         result->usable = true;
      }
      else {
         // Scalar element.
         if ( result->spec != SPEC_STRUCT ) {
            result->assignable = true;
            result->complete = true;
         }
      }
   }
}

bool is_array_ref( struct result* result ) {
   return ( result->dim || result->ref_dim > 0 );
}

void test_access( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct access* access ) {
   struct result lside;
   init_result( &lside );
   test_suffix( semantic, test, &lside, access->lside );
   struct name* name = NULL;
   if ( is_struct_ref( &lside ) ) {
      name = lside.structure->name;
   }
   else if ( ! lside.usable && lside.enumeration ) {
      name = lside.enumeration->name;
   }
   else if ( lside.ns ) {
      name = lside.ns->name;
   }
   if ( ! name ) {
      s_diag( semantic, DIAG_POS_ERR, &access->pos,
         "left operand does not support member access" );
      s_bail( semantic );
   }
   struct name* member_name = t_extend_name( name, "." );
   member_name = t_extend_name( member_name, access->name );
   if ( ! member_name->object ) {
      unknown_member( semantic, access, name->object );
      s_bail( semantic );
   }
   if ( ! member_name->object->resolved ) {
      if ( semantic->trigger_err ) {
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "right operand `%s` undefined", access->name );
         s_bail( semantic );
      }
      else {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
   }
   select_object( test, result, member_name->object );
   access->rside = &member_name->object->node;
}

bool is_struct_ref( struct result* result ) {
   bool explicit_ref = ( ! result->dim && result->ref &&
      result->ref->type == REF_STRUCTURE && result->spec == SPEC_STRUCT );
   bool implicit_ref = ( ! result->dim && ! result->ref &&
      result->spec == SPEC_STRUCT );
   return ( explicit_ref || implicit_ref );
}

void unknown_member( struct semantic* semantic, struct access* access,
   struct object* object ) {
   if ( object->node.type == NODE_STRUCTURE ) {
      struct structure* type = ( struct structure* ) object;
      if ( type->anon ) {
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "`%s` not a member of anonymous struct", access->name );
         s_bail( semantic );
      }
      else {
         struct str str;
         str_init( &str );
         t_copy_name( type->name, false, &str );
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "`%s` not a member of struct `%s`", access->name,
            str.value );
         str_deinit( &str );
      }
   }
   else if ( object->node.type == NODE_ENUMERATION ) {
      struct enumeration* enumeration = ( struct enumeration* ) object;
      struct str str;
      str_init( &str );
      t_copy_name( enumeration->name, false, &str );
      s_diag( semantic, DIAG_POS_ERR, &access->pos,
         "`%s` not an enumerator of enumeration `%s`", access->name,
         str.value );
      str_deinit( &str );
   }
   else if ( object->node.type == NODE_NAMESPACE ) {
      s_unknown_ns_object( semantic,
         ( struct ns* ) object, access->name, &access->pos );
      s_bail( semantic );
   }
}

void s_unknown_ns_object( struct semantic* semantic, struct ns* ns,
   const char* object_name, struct pos* pos ) {
   if ( ns == semantic->task->upmost_ns ) {
      s_diag( semantic, DIAG_POS_ERR, pos,
         "`%s` not an object of upmost namespace", object_name );
   }
   else {
      struct str name;
      str_init( &name );
      t_copy_name( ns->name, true, &name );
      s_diag( semantic, DIAG_POS_ERR, pos,
         "`%s` not an object of namespace `%s`", object_name,
         name.value );
      str_deinit( &name );
   }
}

void test_call( struct semantic* semantic, struct expr_test* expr_test,
   struct result* result, struct call* call ) {
   struct call_test test = {
      .call = call,
      .func = NULL,
      .i = NULL,
      .num_args = 0
   };
   struct result callee;
   init_result( &callee );
   test_suffix( semantic, expr_test, &callee, call->operand );
   if ( ! callee.func ) {
      s_diag( semantic, DIAG_POS_ERR, &call->pos,
         "operand not a function" );
      s_bail( semantic );
   }
   test.func = callee.func;
   test_call_args( semantic, expr_test, &test );
   // Some action-specials cannot be called from a script.
   if ( test.func->type == FUNC_ASPEC ) {
      struct func_aspec* impl = test.func->impl;
      if ( ! impl->script_callable ) {
         struct str str;
         str_init( &str );
         t_copy_name( test.func->name, false, &str );
         s_diag( semantic, DIAG_POS_ERR, &call->pos,
            "action-special `%s` called from script", str.value );
         s_bail( semantic );
      }
   }
   // Latent function cannot be called in a function or a format block.
   if ( test.func->type == FUNC_DED ) {
      struct func_ded* impl = test.func->impl;
      if ( impl->latent ) {
         bool erred = false;
         struct stmt_test* stmt = expr_test->stmt_test;
         while ( stmt && ! stmt->format_block ) {
            stmt = stmt->parent;
         }
         if ( stmt || semantic->func_test->func ) {
            s_diag( semantic, DIAG_POS_ERR, &call->pos,
               "calling latent function inside a %s",
               stmt ? "format block" : "function" );
            // Show educational note to user.
            if ( semantic->func_test->func ) {
               struct str str;
               str_init( &str );
               t_copy_name( test.func->name, false, &str );
               s_diag( semantic, DIAG_FILE, &call->pos,
                  "waiting functions like `%s` can only be called inside a "
                  "script", str.value );
            }
            s_bail( semantic );
         }
      }
   }
   call->func = test.func;
   result->spec = test.func->return_spec;
   result->complete = true;
   result->usable = ( test.func->return_spec != SPEC_VOID );
   if ( call->func->type == FUNC_USER ) {
      struct func_user* impl = call->func->impl;
      if ( impl->nested ) {
         struct nested_call* nested = mem_alloc( sizeof( *nested ) );
         nested->next = impl->nested_calls;
         nested->id = 0;
         nested->prologue_jump = NULL;
         nested->return_point = NULL;
         impl->nested_calls = call;
         call->nested_call = nested;
      }
   }
}

void test_call_args( struct semantic* semantic, struct expr_test* expr_test,
   struct call_test* test ) {
   struct call* call = test->call;
   struct func* func = test->func;
   list_iter_t i;
   list_iter_init( &i, &call->args );
   test->i = &i;
   test_call_first_arg( semantic, expr_test, test );
   test_remaining_args( semantic, expr_test, test );
   // Number of arguments must be correct.
   if ( test->num_args < func->min_param ) {
      s_diag( semantic, DIAG_POS_ERR, &call->pos,
         "not enough arguments in function call" );
      struct str str;
      str_init( &str );
      t_copy_name( func->name, false, &str );
      s_diag( semantic, DIAG_FILE, &call->pos,
         "function `%s` needs %s%d argument%s", str.value,
         func->min_param != func->max_param ? "at least " : "",
         func->min_param, func->min_param != 1 ? "s" : "" );
      s_bail( semantic );
   }
   if ( test->num_args > func->max_param ) {
      s_diag( semantic, DIAG_POS_ERR, &call->pos,
         "too many arguments in function call" );
      struct str str;
      str_init( &str );
      t_copy_name( func->name, false, &str );
      s_diag( semantic, DIAG_FILE, &call->pos,
         "function `%s` takes %s%d argument%s", str.value,
         func->min_param != func->max_param ? "at most " : "",
         func->max_param, func->max_param != 1 ? "s" : "" );
      s_bail( semantic );
   }
}

void test_call_first_arg( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test ) {
   if ( test->func->type == FUNC_FORMAT ) {
      test_call_format_arg( semantic, expr_test, test );
   }
   else {
      if ( ! list_end( test->i ) ) {
         struct node* node = list_data( test->i );
         if ( node->type == NODE_FORMAT_ITEM ) {
            struct format_item* item = ( struct format_item* ) node;
            s_diag( semantic, DIAG_POS_ERR, &item->pos,
               "passing format-item to non-format function" );
            s_bail( semantic );
         }
         else if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
            struct format_block_usage* usage =
               ( struct format_block_usage* ) node;
            s_diag( semantic, DIAG_POS_ERR, &usage->pos,
               "passing format-block to non-format function" );
            s_bail( semantic );
         }
      }
   }
}

void test_call_format_arg( struct semantic* semantic, struct expr_test* expr_test,
   struct call_test* test ) {
   list_iter_t* i = test->i;
   struct node* node = NULL;
   if ( ! list_end( i ) ) {
      node = list_data( i );
   }
   if ( ! node || ( node->type != NODE_FORMAT_ITEM &&
      node->type != NODE_FORMAT_BLOCK_USAGE ) ) {
      s_diag( semantic, DIAG_POS_ERR, &test->call->pos,
         "function call missing format argument" );
      s_bail( semantic );
   }
   // Format-block:
   if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
      if ( ! expr_test->format_block ) {
         s_diag( semantic, DIAG_POS_ERR, &test->call->pos,
            "function call missing format-block" );
         s_bail( semantic );
      }
      struct format_block_usage* usage = ( struct format_block_usage* ) node;
      usage->block = expr_test->format_block;
      // Attach usage to the end of the usage list.
      struct format_block_usage* prev = expr_test->format_block_usage;
      while ( prev && prev->next ) {
         prev = prev->next;
      }
      if ( prev ) {
         prev->next = usage;
      }
      else {
         expr_test->format_block_usage = usage;
      }
      list_next( i );
   }
   // Format-list:
   else {
      test_format_item_list( semantic, expr_test, ( struct format_item* ) node );
      list_next( i );
   }
   // Both a format-block or a format-list count as a single argument.
   ++test->num_args;
}

// This function handles a format-item found inside a function call,
// and a free format-item, the one found in a format-block.
void test_format_item_list( struct semantic* semantic, struct expr_test* test,
   struct format_item* item ) {
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

void test_format_item( struct semantic* semantic, struct expr_test* test,
   struct format_item* item ) {
   struct expr_test nested;
   s_init_expr_test( &nested,
      test->stmt_test, test->format_block,
      true, false );
   struct result root;
   init_result( &root );
   test_nested_root( semantic, test, &nested, &root, item->value );
}

void test_array_format_item( struct semantic* semantic, struct expr_test* test,
   struct format_item* item ) {
   struct expr_test nested;
   s_init_expr_test( &nested,
      test->stmt_test, test->format_block,
      false, false );
   // When using the array format-cast, accept an array as the result of the
   // expression.
   // nested.accept_array = true;
   struct result root;
   init_result( &root );
   test_nested_root( semantic, test, &nested, &root, item->value );
   if ( ! root.dim ) {
      s_diag( semantic, DIAG_POS_ERR, &item->value->pos,
         "argument not an array" );
      s_bail( semantic );
   }
   if ( root.dim->next ) {
      s_diag( semantic, DIAG_POS_ERR, &item->value->pos,
         "array argument not of single dimension" );
      s_bail( semantic );
   }
   // Test optional fields: offset and length.
   if ( item->extra ) {
      struct format_item_array* extra = item->extra;
      s_init_expr_test( &nested,
         test->stmt_test, test->format_block,
         true, false );
      init_result( &root );
      test_nested_root( semantic, test, &nested, &root, extra->offset );
      if ( extra->length ) {
         s_init_expr_test( &nested,
            test->stmt_test, test->format_block,
            true, false );
         init_result( &root );
         test_nested_root( semantic, test, &nested, &root, extra->length );
      }
   }
}

void s_test_formatitemlist_stmt( struct semantic* semantic,
   struct stmt_test* stmt_test, struct format_item* item ) {
   struct expr_test test;
   s_init_expr_test( &test, stmt_test, NULL, true, false );
   if ( setjmp( test.bail ) == 0 ) {
      test_format_item_list( semantic, &test, item );
   }
}

void test_remaining_args( struct semantic* semantic,
   struct expr_test* expr_test, struct call_test* test ) {
   struct param* param = test->func->params;
   while ( ! list_end( test->i ) ) {
      struct expr* expr = list_data( test->i );
      struct expr_test nested;
      s_init_expr_test( &nested,
         expr_test->stmt_test,
         expr_test->format_block, true, false );
      struct result result;
      init_result( &result );
      test_nested_root( semantic, expr_test, &nested, &result, expr );
      if ( param ) {
         struct result required_result;
         init_result( &required_result );
         required_result.spec = param->spec;
         if ( ! same_type( &result, &required_result ) ) {
            arg_mismatch( semantic, &expr->pos, &result, &required_result,
               test->num_args + 1 );
            s_bail( semantic );
         }
         param = param->next;
      }
      ++test->num_args;
      list_next( test->i );
   }
}

void arg_mismatch( struct semantic* semantic, struct pos* pos,
   struct result* result, struct result* required_result, int number ) {
   struct type_info arg_type;
   init_type_info( &arg_type, result );
   struct type_info param_type;
   init_type_info( &param_type, required_result );
   struct str arg_type_s;
   str_init( &arg_type_s );
   s_present_type( &arg_type, &arg_type_s );
   struct str param_type_s;
   str_init( &param_type_s );
   s_present_type( &param_type, &param_type_s );
   s_diag( semantic, DIAG_POS_ERR, pos,
      "argument/parameter type mismatch (in argument %d)", number );
   s_diag( semantic, DIAG_POS, pos,
      "`%s` argument, but `%s` parameter",
      arg_type_s.value, param_type_s.value );
   str_deinit( &param_type_s );
   str_deinit( &arg_type_s );
}

void test_primary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node ) {
   switch ( node->type ) {
   case NODE_LITERAL:
      test_literal( result,
         ( struct literal* ) node );
      break;
   case NODE_FIXED_LITERAL:
      test_fixed_literal( result,
         ( struct fixed_literal* ) node );
      break;
   case NODE_INDEXED_STRING_USAGE:
      test_string_usage( test, result,
         ( struct indexed_string_usage* ) node );
      break;
   case NODE_BOOLEAN:
      test_boolean( result,
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
   case NODE_PAREN:
      test_paren( semantic, test, result,
         ( struct paren* ) node );
      break;
   case NODE_UPMOST:
      test_upmost( semantic, result );
      break;
   default:
      UNREACHABLE();
   }
}

void test_literal( struct result* result, struct literal* literal ) {
   result->value = literal->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   result->spec = SPEC_ZINT;
}

void test_fixed_literal( struct result* result,
   struct fixed_literal* literal ) {
   result->value = literal->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   result->spec = SPEC_ZFIXED;
}

void test_string_usage( struct expr_test* test, struct result* result,
   struct indexed_string_usage* usage ) {
   result->value = usage->string->index;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   result->spec = SPEC_ZSTR;
   test->has_string = true;
}

void test_boolean( struct result* result, struct boolean* boolean ) {
   result->value = boolean->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   result->spec = SPEC_ZBOOL;
}

void test_name_usage( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage ) {
   struct object* object = find_referenced_object( semantic, usage );
   if ( object && object->resolved ) {
      select_object( test, result, object );
      usage->object = &object->node;
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

struct object* find_referenced_object( struct semantic* semantic,
   struct name_usage* usage ) {
   // Try searching in the hidden compartment of the library.
   if ( usage->lib_id ) {
      list_iter_t i;
      list_iter_init( &i, &semantic->task->libraries );
      struct library* lib;
      while ( true ) {
         lib = list_data( &i );
         if ( lib->id == usage->lib_id ) {
            break;
         }
         list_next( &i );
      }
      struct name* name = t_extend_name( lib->hidden_names, usage->text );
      if ( name->object ) {
         return name->object;
      }
   }
   return s_search_object( semantic, usage->text );
}

void select_object( struct expr_test* test, struct result* result,
   struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT:
      select_constant( result, 
         ( struct constant* ) object );
      break;
   case NODE_ENUMERATOR:
      select_enumerator( result,
         ( struct enumerator* ) object );
      break;
   case NODE_ENUMERATION:
      select_enumeration( result,
         ( struct enumeration* ) object );
      break;
   case NODE_VAR:
      select_var( test, result,
         ( struct var* ) object );
      break;
   case NODE_PARAM:
      select_param( result,
         ( struct param* ) object );
      break;
   case NODE_STRUCTURE_MEMBER:
      select_member( test, result,
         ( struct structure_member* ) object );
      break;
   case NODE_FUNC:
      select_func( result,
         ( struct func* ) object );
      break;
   case NODE_NAMESPACE:
      select_namespace( result,
         ( struct ns* ) object );
      break;
   default:
      break;
   }
}

void select_constant( struct result* result, struct constant* constant ) {
   result->spec = SPEC_ZINT;
   result->value = constant->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

void select_enumerator( struct result* result,
   struct enumerator* enumerator ) {
   result->enumeration = enumerator->enumeration;
   result->spec = SPEC_ZINT;
   result->value = enumerator->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

void select_enumeration( struct result* result,
   struct enumeration* enumeration ) {
   result->enumeration = enumeration;
}

void select_var( struct expr_test* test, struct result* result,
   struct var* var ) {
   result->data_origin = var;
   result->ref = var->ref;
   result->structure = var->structure;
   result->enumeration = var->enumeration;
   result->dim = var->dim;
   result->spec = var->spec;
   result->usable = true;
   if ( is_ref_type( result ) ) {
      if ( result->ref ) {
         if ( result->ref->type == REF_ARRAY ) {
            struct ref_array* part = ( struct ref_array* ) result->ref;
            result->ref_dim = part->dim_count;
         }
         if ( ! result->dim ) {
            result->assignable = true;
         }
      }
      else {
      }
      result->complete = true;
   }
   else {
      if ( ! result->structure ) {
         result->complete = true;
         result->assignable = true;
         if ( result->spec == SPEC_ENUM ) {
            result->spec = SPEC_ZINT;
         }
      }
   }
   var->used = true;
}

void select_param( struct result* result, struct param* param ) {
   result->spec = param->spec;
   result->complete = true;
   result->usable = true;
   result->assignable = true;
   param->used = true;
}

void select_member( struct expr_test* test, struct result* result,
   struct structure_member* member ) {
   result->ref = member->ref;
   result->structure = member->structure;
   result->enumeration = member->enumeration;
   result->dim = member->dim;
   result->spec = member->spec;
   result->usable = true;
   if ( is_ref_type( result ) ) {
      if ( result->ref ) {
         if ( result->ref->type == REF_ARRAY ) {
            struct ref_array* part = ( struct ref_array* ) result->ref;
            result->ref_dim = part->dim_count;
         }
         if ( ! result->dim ) {
            result->assignable = true;
         }
      }
      else {
      }
      result->complete = true;
   }
   else {
      if ( ! result->structure ) {
         result->complete = true;
         result->assignable = true;
         if ( result->spec == SPEC_ENUM ) {
            result->spec = SPEC_ZINT;
         }
      }
   }
}

void select_func( struct result* result, struct func* func ) {
   if ( func->type == FUNC_USER ) {
      struct func_user* impl = func->impl;
      ++impl->usage;
   }
   // When using just the name of an action special, a value is produced,
   // which is the ID of the action special.
   else if ( func->type == FUNC_ASPEC ) {
      result->spec = SPEC_ZINT;
      result->complete = true;
      result->usable = true;
      struct func_aspec* impl = func->impl;
      result->value = impl->id;
      result->folded = true;
   }
   result->func = func;
}

void select_namespace( struct result* result, struct ns* ns ) {
   result->ns = ns;
}

void test_strcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct strcpy_call* call ) {
   // Array.
   struct result arg;
   init_result( &arg );
   test_root( semantic, test, &arg, call->array );
   if ( ! is_onedim_intelem_array_ref( &arg ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->array->pos,
         "array argument not a one-dimensional, "
         "integer element-type array-reference" );
      s_bail( semantic );
   }
   // Array-offset.
   if ( call->array_offset ) {
      init_result( &arg );
      test_root( semantic, test, &arg, call->array_offset );
      if ( ! ( arg.usable && is_int_value( &arg ) ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->array_offset->pos,
            "array-offset argument not an integer value" );
         s_bail( semantic );
      }
      // Array-length.
      if ( call->array_length ) {
         init_result( &arg );
         test_root( semantic, test, &arg, call->array_length );
         if ( ! ( arg.usable && is_int_value( &arg ) ) ) {
            s_diag( semantic, DIAG_POS_ERR, &call->array_length->pos,
               "array-length argument not an integer value" );
            s_bail( semantic );
         }
      }
   }
   // String.
   init_result( &arg );
   test_root( semantic, test, &arg, call->string );
   if ( ! ( arg.usable && is_str_value( &arg ) ) ) {
      s_diag( semantic, DIAG_POS_ERR, &call->string->pos,
         "string argument not a string value" );
      s_bail( semantic );
   }
   // String-offset.
   if ( call->offset ) {
      init_result( &arg );
      test_root( semantic, test, &arg, call->offset );
      if ( ! ( arg.usable && is_int_value( &arg ) ) ) {
         s_diag( semantic, DIAG_POS_ERR, &call->offset->pos,
            "string-offset argument not an integer value" );
         s_bail( semantic );
      }
   }
   result->spec = SPEC_ZBOOL;
   result->complete = true;
   result->usable = true;
}

bool is_onedim_intelem_array_ref( struct result* result ) {
   bool onedim_array = ( result->dim && ! result->dim->next ) ||
      ( result->ref_dim == 1 );
   bool int_elem = ( result->spec == SPEC_ZRAW ||
      result->spec == SPEC_ZINT );
   return ( onedim_array && int_elem );
}

bool is_int_value( struct result* result ) {
   return ( is_value_type( result ) && ( result->spec == SPEC_ZRAW ||
      result->spec == SPEC_ZINT ) );
}

bool is_str_value( struct result* result ) {
   return ( is_value_type( result ) && result->spec == SPEC_ZSTR );
}

void test_paren( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct paren* paren ) {
   result->in_paren = true;
   test_operand( semantic, test, result, paren->inside );
   result->in_paren = false;
}

void test_upmost( struct semantic* semantic, struct result* result ) {
   result->ns = semantic->task->upmost_ns;
}

inline void init_type_info( struct type_info* type, struct result* result ) {
   s_init_type_info( type, result->spec, result->ref, result->dim,
      result->structure, result->enumeration );
}

bool same_type( struct result* a, struct result* b ) {
   struct type_info a_type;
   init_type_info( &a_type, a );
   struct type_info b_type;
   init_type_info( &b_type, b );
   return s_same_type( &a_type, &b_type );
}

bool is_value_type( struct result* result ) {
   struct type_info type;
   init_type_info( &type, result );
   return s_is_value_type( &type );
}

bool is_ref_type( struct result* result ) {
   return ( result->ref || result->dim || result->structure );
}