#include "phase.h"

struct result {
   struct func* func;
   struct dim* dim;
   struct structure* type;
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
static bool perform_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result );
static void invalid_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside );
static void present_spec( struct result* result, struct str* string );
static void fold_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result );
static void test_logical( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct logical* logical );
static bool perform_logical( struct semantic* semantic,
   struct logical* logical, struct result* lside, struct result* rside,
   struct result* result );
static bool can_convert_to_boolean( struct result* result );
static void fold_logical( struct semantic* semantic, struct logical* logical,
   struct result* lside, struct result* rside, struct result* result );
static void invalid_logical( struct semantic* semantic,
   struct logical* logical, struct result* lside, struct result* rside );
static void fold_logical( struct semantic* semantic, struct logical* logical,
   struct result* lside, struct result* rside, struct result* result );
static void test_assign( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct assign* assign );
static void test_conditional( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct conditional* cond );
static void test_prefix( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_unary( struct semantic* semantic,
   struct expr_test* test, struct result* result, struct unary* unary );
static void test_inc( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct inc* inc );
static void test_suffix( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_subscript( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct subscript* subscript );
static void test_access( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct access* access );
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
static void test_primary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node );
static void test_literal( struct semantic* semantic, struct result* result,
   struct literal* literal );
static void test_string_usage( struct semantic* semantic,
   struct expr_test* test, struct result* result,
   struct indexed_string_usage* usage );
static void test_boolean( struct semantic* semantic,
   struct result* result, struct boolean* boolean );
static void test_name_usage( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage );
static struct object* find_referenced_object( struct semantic* semantic,
   struct name_usage* usage );
static void select_object( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct object* object );
static void select_constant( struct semantic* semantic, struct result* result,
   struct constant* constant );
static void select_enumerator( struct semantic* semantic,
   struct result* result, struct enumerator* enumerator );
static void select_var( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct var* var );
static void select_param( struct semantic* semantic, struct result* result,
   struct param* param );
static void select_member( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct structure_member* member );
static void select_func( struct semantic* semantic, struct result* result,
   struct func* func );
static void test_strcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct strcpy_call* call );
static void test_paren( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct paren* paren );

void s_init_expr_test( struct expr_test* test, struct stmt_test* stmt_test,
   struct block* format_block, bool result_required,
   bool suggest_paren_assign ) {
   test->stmt_test = stmt_test;
   test->format_block = format_block;
   test->format_block_usage = NULL;
   test->result_required = result_required;
   test->has_string = false;
   test->undef_erred = false;
   test->accept_array = false;
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
   expr->structure = result->type;
   expr->folded = result->folded;
   expr->value = result->value;
   test->pos = expr->pos;
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
   result->dim = NULL;
   result->type = NULL;
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
         "operand on left side not a value" );
      s_bail( semantic );
   }
   struct result rside;
   init_result( &rside );
   test_operand( semantic, test, &rside, binary->rside );
   if ( ! rside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &binary->pos,
         "operand on right side not a value" );
      s_bail( semantic );
   }
   bool performed = perform_bop( semantic, binary, &lside, &rside, result );
   if ( ! performed ) {
      invalid_bop( semantic, binary, &lside, &rside );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( lside.folded && rside.folded ) {
      fold_bop( semantic, binary, &lside, &rside, result );
   }
}

bool perform_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside, struct result* result ) {
   // Binary operators must take operands of the same type.
   if ( lside->spec != rside->spec ) {
      return false;
   }
   int spec = SPEC_NONE;
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
      switch ( lside->spec ) {
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
      switch ( lside->spec ) {
      case SPEC_ZRAW:
      case SPEC_ZINT:
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
      switch ( lside->spec ) {
      case SPEC_ZRAW:
      case SPEC_ZINT:
      case SPEC_ZFIXED:
      case SPEC_ZSTR:
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
      case SPEC_ZRAW:
      case SPEC_ZINT:
      case SPEC_ZFIXED:
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
   result->type = semantic->task->type_int;
   result->complete = true;
   result->usable = true;
   binary->lside_spec = lside->spec;
   return true;
}

void invalid_bop( struct semantic* semantic, struct binary* binary,
   struct result* lside, struct result* rside ) {
   // TODO: Move to token phase.
   static const char* op_names[] = {
      "|", "^", "&", "==", "!=", "<", "<=", ">", ">=",
      "<<", ">>", "+", "-", "*", "/", "%"
   };
   const char* op = ( binary->op - 1 < ARRAY_SIZE( op_names ) ) ?
      op_names[ binary->op - 1 ] : "?";
   struct str operand_a;
   str_init( &operand_a );
   present_spec( lside, &operand_a );
   struct str operand_b;
   str_init( &operand_b );
   present_spec( rside, &operand_b );
   s_diag( semantic, DIAG_POS_ERR, &binary->pos,
      "invalid operation: `%s` %s `%s`", operand_a.value, op,
      operand_b.value );
}

void present_spec( struct result* result, struct str* string ) {
   switch ( result->spec ) {
   case SPEC_ZRAW:
      str_append( string, "zraw" );
      break;
   case SPEC_ZINT:
      str_append( string, "zint" );
      break;
   case SPEC_ZFIXED:
      str_append( string, "zfixed" );
      break;
   case SPEC_ZBOOL:
      str_append( string, "zbool" );
      break;
   case SPEC_ZSTR:
      str_append( string, "zstr" );
      break;
   default:
      break;
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
   struct result rside;
   init_result( &rside );
   test_operand( semantic, test, &rside, logical->rside );
   bool performed = perform_logical( semantic,
      logical, &lside, &rside, result );
   if ( ! performed ) {
      invalid_logical( semantic, logical, &lside, &rside );
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
      result->type = semantic->task->type_bool;
      result->spec = SPEC_ZBOOL;
      result->complete = true;
      result->usable = true;
      logical->lside_spec = lside->spec;
      logical->rside_spec = rside->spec;
      return true;
   }
   return false;
}

bool can_convert_to_boolean( struct result* result ) {
   switch ( result->spec ) {
   case SPEC_ZRAW:
   case SPEC_ZINT:
   case SPEC_ZFIXED:
   case SPEC_ZBOOL:
   case SPEC_ZSTR:
      return true;
   default:
      return false;
   }
}

void invalid_logical( struct semantic* semantic, struct logical* logical,
   struct result* lside, struct result* rside ) {
   // TODO: Move to token phase.
   const char* op = "";
   switch ( logical->op ) {
   case LOP_OR: op = "||"; break;
   case LOP_AND: op = "&&"; break;
   default:
      UNREACHABLE()
   }
   struct str operand_a;
   str_init( &operand_a );
   present_spec( lside, &operand_a );
   struct str operand_b;
   str_init( &operand_b );
   present_spec( rside, &operand_b );
   s_diag( semantic, DIAG_POS_ERR, &logical->pos,
      "invalid operation: `%s` %s `%s`", operand_a.value, op,
      operand_b.value );
}

void fold_logical( struct semantic* semantic, struct logical* logical,
   struct result* lside, struct result* rside, struct result* result ) {
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
   // To avoid the error where the user wanted equality operator but instead
   // typed in the assignment operator, suggest that assignment be wrapped in
   // parentheses.
   if ( test->suggest_paren_assign && ! result->in_paren ) {
      s_diag( semantic, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &assign->pos, "assignment operation not in parentheses" );
   }
   struct result lside;
   init_result( &lside );
   test_operand( semantic, test, &lside, assign->lside );
   if ( ! lside.assignable ) {
      s_diag( semantic, DIAG_POS_ERR, &assign->pos,
         "cannot assign to operand on left side" );
      s_bail( semantic );
   }
   struct result rside;
   init_result( &rside );
   test_operand( semantic, test, &rside, assign->rside );
   if ( ! rside.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &assign->pos,
         "right side of assignment not a value" );
      s_bail( semantic );
   }
   result->complete = true;
   result->usable = true;
   result->type = lside.type;
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
   // A string has an index. This index could be 0. The problem is that the
   // value 0 indicates a failed condition, but the string surely is a valid
   // value. So for now, disallow the usage of a string as the left operand.
   if ( left.type == semantic->task->type_str ) {
      s_diag( semantic, DIAG_POS_ERR, &cond->pos,
         "left operand of `str` type" );
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
   if ( middle.type != right.type ) {
      s_diag( semantic, DIAG_POS_ERR, &cond->pos,
         "%s and right operands of different type",
         cond->middle ? "middle" : "left" );
      s_bail( semantic );
   }
   result->type = left.type;
   result->complete = true;
   if ( result->type ) {
      result->usable = true;
   }
   // Compile-time evaluation.
   if ( left.folded && middle.folded && right.folded ) {
      cond->left_value = left.value;
      cond->folded = true;
      result->value = left.value ? middle.value : right.value;
      result->folded = true;
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
   default:
      test_suffix( semantic, test, result,
         node );
      break;
   }
}

void test_unary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct unary* unary ) {
   struct result target;
   init_result( &target );
   test_operand( semantic, test, &target, unary->operand );
   // Remaining operations require a value to work on.
   if ( ! target.usable ) {
      s_diag( semantic, DIAG_POS_ERR, &unary->pos,
         "operand of unary operation not a value" );
      s_bail( semantic );
   }
   // Compile-time evaluation.
   if ( target.folded ) {
      switch ( unary->op ) {
      case UOP_MINUS:
         result->value = ( - target.value );
         break;
      case UOP_PLUS:
         result->value = target.value;
         break;
      case UOP_LOG_NOT:
         result->value = ( ! target.value );
         break;
      case UOP_BIT_NOT:
         result->value = ( ~ target.value );
         break;
      default:
         break;
      }
      result->folded = true;
   }
   result->complete = true;
   result->usable = true;
   // Type of the result.
   if ( unary->op == UOP_LOG_NOT ) {
      result->type = semantic->task->type_bool;
   }
   else {
      result->type = semantic->task->type_int;
   }
}

void test_inc( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct inc* inc ) {
   struct result operand;
   init_result( &operand );
   test_operand( semantic, test, &operand, inc->operand );
   // Only an l-value can be incremented.
   if ( ! operand.assignable ) {
      s_diag( semantic, DIAG_POS_ERR, &inc->pos,
         "operand cannot be %s", inc->dec ? "decremented" : "incremented" );
      s_bail( semantic );
   }
   result->complete = true;
   result->usable = true;
   result->type = semantic->task->type_int;
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
   test_operand( semantic, test, &lside, subscript->lside );
   if ( ! lside.dim ) {
      s_diag( semantic, DIAG_POS_ERR, &subscript->pos,
         "operand not an array" );
      s_bail( semantic );
   }
   struct expr_test index;
   s_init_expr_test( &index, test->stmt_test, test->format_block, true,
      test->suggest_paren_assign );
   struct result root;
   init_result( &root );
   test_nested_root( semantic, test, &index, &root, subscript->index );
   // Out-of-bounds warning for a constant index.
   if ( lside.dim->size && subscript->index->folded && (
      subscript->index->value < 0 ||
      subscript->index->value >= lside.dim->size ) ) {
      s_diag( semantic, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &subscript->index->pos, "array index out-of-bounds" );
   }
   result->type = lside.type;
   result->dim = lside.dim->next;
   if ( result->dim ) {
      if ( test->accept_array ) {
         result->complete = true;
      }
   }
   else {
      if ( result->type->primitive ) {
         result->complete = true;
         result->usable = true;
         result->assignable = true;
      }
   }
}

void test_access( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct access* access ) {
   struct result lside;
   init_result( &lside );
   test_operand( semantic, test, &lside, access->lside );
   if ( ! ( lside.type && ! lside.dim ) ) {
      s_diag( semantic, DIAG_POS_ERR, &access->pos,
         "left operand not of struct type" );
      s_bail( semantic );
   }
   struct name* name = t_extend_name( lside.type->body, access->name );
   if ( ! name->object ) {
      // The right operand might be a member of a structure that hasn't been
      // processed yet because the structure appears later in the source code.
      // Leave for now.
      // TODO: The name should already refer to a valid member by this stage.
      // Need to refactor the declaration code, then remove this.
      if ( ! semantic->trigger_err ) {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
      if ( lside.type->anon ) {
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "`%s` not member of anonymous struct", access->name );
         s_bail( semantic );
      }
      else {
         struct str str;
         str_init( &str );
         t_copy_name( lside.type->name, false, &str );
         s_diag( semantic, DIAG_POS_ERR, &access->pos,
            "`%s` not member of struct `%s`", access->name,
            str.value );
         s_bail( semantic );
      }
   }
   if ( ! name->object->resolved ) {
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
   select_object( semantic, test, result, name->object );
   access->rside = ( struct node* ) name->object;
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
   test_operand( semantic, expr_test, &callee, call->operand );
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
   result->type = test.func->return_type;
   result->complete = true;
   if ( test.func->return_type ) {
      result->usable = true;
   }
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
   // Test remaining arguments.
   while ( ! list_end( &i ) ) {
      struct expr_test nested;
      s_init_expr_test( &nested,
         expr_test->stmt_test,
         expr_test->format_block, true, false );
      struct result root;
      init_result( &root );
      test_nested_root( semantic, expr_test, &nested, &root, list_data( &i ) );
      ++test->num_args;
      list_next( &i );
   }
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
         func->min_param != func->max_param ? "up_to " : "",
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
   nested.accept_array = true;
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

void test_primary( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct node* node ) {
   switch ( node->type ) {
   case NODE_LITERAL:
      test_literal( semantic, result,
         ( struct literal* ) node );
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
   case NODE_PAREN:
      test_paren( semantic, test, result,
         ( struct paren* ) node );
      break;
   default:
      break;
   }
}

void test_literal( struct semantic* semantic, struct result* result,
   struct literal* literal ) {
   result->value = literal->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   result->type = semantic->task->type_int;
   result->spec = SPEC_ZINT;
}

void test_string_usage( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct indexed_string_usage* usage ) {
   result->value = usage->string->index;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   result->type = semantic->task->type_str;
   result->spec = SPEC_ZSTR;
   test->has_string = true;
}

void test_boolean( struct semantic* semantic, struct result* result,
   struct boolean* boolean ) {
   result->value = boolean->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
   result->type = semantic->task->type_bool;
   result->spec = SPEC_ZBOOL;
}

void test_name_usage( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct name_usage* usage ) {
   struct object* object = find_referenced_object( semantic, usage );
   if ( object && object->resolved ) {
      select_object( semantic, test, result, object );
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

   // Try searching in the upmost scope.
   struct name* name = t_extend_name( semantic->task->body, usage->text );
   if ( name->object ) {
      struct object* object = name->object;
      if ( object->node.type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) object;
         object = alias->target;
      }
      return object;
   }

   return NULL;
}

void select_object( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT:
      select_constant( semantic, result, 
         ( struct constant* ) object );
      break;
   case NODE_ENUMERATOR:
      select_enumerator( semantic, result,
         ( struct enumerator* ) object );
      break;
   case NODE_VAR:
      select_var( semantic, test, result,
         ( struct var* ) object );
      break;
   case NODE_PARAM:
      select_param( semantic, result,
         ( struct param* ) object );
      break;
   case NODE_STRUCTURE_MEMBER:
      select_member( semantic, test, result,
         ( struct structure_member* ) object );
      break;
   case NODE_FUNC:
      select_func( semantic, result,
         ( struct func* ) object );
      break;
   default:
      break;
   }
}

void select_constant( struct semantic* semantic, struct result* result,
   struct constant* constant ) {
   // TODO: Add type as a field.
   result->type = constant->value_node ?
      constant->value_node->structure :
      semantic->task->type_int;
   result->spec = SPEC_ZINT;
   result->value = constant->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

void select_enumerator( struct semantic* semantic, struct result* result,
   struct enumerator* enumerator ) {
   result->type = semantic->task->type_int;
   result->spec = SPEC_ZINT;
   result->value = enumerator->value;
   result->folded = true;
   result->complete = true;
   result->usable = true;
}

void select_var( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct var* var ) {
   result->type = var->structure;
   result->spec = var->spec;
   if ( var->dim ) {
      result->dim = var->dim;
      // NOTE: I'm not too happy with the idea of this field. It is
      // contagious. Blame the Hypnotoad.
      if ( test->accept_array ) {
         result->complete = true;
      }
   }
   else if ( var->structure->primitive ) {
      result->complete = true;
      result->usable = true;
      result->assignable = true;
   }
   var->used = true;
}

void select_param( struct semantic* semantic, struct result* result,
   struct param* param ) {
   param->used = true;
   result->type = param->structure;
   result->complete = true;
   result->usable = true;
   result->assignable = true;
}

void select_member( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct structure_member* member ) {
   result->type = member->structure;
   if ( member->dim ) {
      result->dim = member->dim;
      if ( test->accept_array ) {
         result->complete = true;
      }
   }
   else if ( member->structure->primitive ) {
      result->complete = true;
      result->usable = true;
      result->assignable = true;
   }
}

void select_func( struct semantic* semantic, struct result* result,
   struct func* func ) {
   if ( result->func->type == FUNC_USER ) {
      struct func_user* impl = result->func->impl;
      ++impl->usage;
   }
   // When using just the name of an action special, a value is produced,
   // which is the ID of the action special.
   else if ( result->func->type == FUNC_ASPEC ) {
      result->complete = true;
      result->usable = true;
      struct func_aspec* impl = result->func->impl;
      result->value = impl->id;
      result->folded = true;
   }
}

void test_strcpy( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct strcpy_call* call ) {
   // Array.
   struct expr_test nested;
   s_init_expr_test( &nested,
      test->stmt_test, test->format_block,
      false, false );
   nested.accept_array = true;
   struct result root;
   init_result( &root );
   test_nested_root( semantic, test, &nested, &root, call->array );
   if ( ! root.dim ) {
      s_diag( semantic, DIAG_POS_ERR, &nested.pos,
         "argument not an array" );
      s_bail( semantic );
   }
   // Array-offset.
   if ( call->array_offset ) {
      s_init_expr_test( &nested,
         test->stmt_test, test->format_block,
         false, false );
      init_result( &root );
      test_nested_root( semantic, test, &nested, &root, call->array_offset );
      // Array-length.
      if ( call->array_length ) {
         s_init_expr_test( &nested,
            test->stmt_test, test->format_block,
            false, false );
         init_result( &root );
         test_nested_root( semantic, test, &nested, &root,
            call->array_length );
      }
   }
   // String.
   s_init_expr_test( &nested,
      test->stmt_test, test->format_block,
      false, false );
   init_result( &root );
   test_nested_root( semantic, test, &nested, &root, call->string );
   // String-offset.
   if ( call->offset ) {
      s_init_expr_test( &nested,
         test->stmt_test, test->format_block,
         false, false );
      init_result( &root );
      test_nested_root( semantic, test, &nested, &root, call->offset );
   }
   result->complete = true;
   result->usable = true;
   result->type = semantic->task->type_bool;
}

void test_paren( struct semantic* semantic, struct expr_test* test,
   struct result* result, struct paren* paren ) {
   result->in_paren = true;
   test_operand( semantic, test, result, paren->inside );
   result->in_paren = false;
}