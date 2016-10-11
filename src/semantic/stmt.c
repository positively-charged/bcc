#include <string.h>

#include "phase.h"

static void test_block( struct semantic* semantic, struct stmt_test* test,
   struct block* block );
static void test_block_item( struct semantic* semantic, struct stmt_test* test,
   struct node* node );
static void test_case( struct semantic* semantic, struct stmt_test* test,
   struct case_label* label );
static void test_default_case( struct semantic* semantic,
   struct stmt_test* test, struct case_label* label );
static void test_assert( struct semantic* semantic, struct assert* assert );
static void test_stmt( struct semantic* semantic, struct stmt_test* test,
   struct node* node );
static void test_if( struct semantic* semantic, struct stmt_test* test,
   struct if_stmt* stmt );
static void test_cond( struct semantic* semantic, struct cond* cond );
static void test_switch( struct semantic* semantic, struct stmt_test* test,
   struct switch_stmt* stmt );
static void test_switch_cond( struct semantic* semantic,
   struct stmt_test* test, struct switch_stmt* stmt );
static void warn_switch_skipped_init( struct semantic* semantic,
   struct block* block );
static void test_while( struct semantic* semantic, struct stmt_test* test,
   struct while_stmt* stmt );
static void test_for( struct semantic* semantic, struct stmt_test* test,
   struct for_stmt* stmt );
static void test_foreach( struct semantic* semantic, struct stmt_test* test,
   struct foreach_stmt* stmt );
static void test_jump( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt );
static void test_break( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt );
static void test_continue( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt );
static void test_script_jump( struct semantic* semantic,
   struct stmt_test* test, struct script_jump* stmt );
static void test_return( struct semantic* semantic, struct stmt_test* test,
   struct return_stmt* stmt );
static void test_return_value( struct semantic* semantic,
   struct stmt_test* test, struct return_stmt* stmt );
static void test_goto( struct semantic* semantic, struct stmt_test* test,
   struct goto_stmt* stmt );
static void test_paltrans( struct semantic* semantic, struct stmt_test* test,
   struct paltrans* stmt );
static void test_paltrans_arg( struct semantic* semantic, struct expr* arg,
   bool require_fixed_type );
static void test_expr_stmt( struct semantic* semantic,
   struct expr_stmt* stmt );
static void check_dup_label( struct semantic* semantic );

void s_init_stmt_test( struct stmt_test* test, struct stmt_test* parent ) {
   test->parent = parent;
   test->switch_stmt = NULL;
   test->jump_break = NULL;
   test->jump_continue = NULL;
   test->in_loop = false;
   test->manual_scope = false;
   test->case_allowed = false;
}

void s_test_body( struct semantic* semantic, struct node* node ) {
   struct stmt_test test;
   s_init_stmt_test( &test, NULL );
   test.manual_scope = true;
   if ( node->type == NODE_BLOCK ) {
      test_block( semantic, &test,
         ( struct block* ) node );
      check_dup_label( semantic );
   }
   else {
      test_stmt( semantic, &test, node );
   }
}

void test_block( struct semantic* semantic, struct stmt_test* test,
   struct block* block ) {
   if ( ! test->manual_scope ) {
      s_add_scope( semantic, false );
   }
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      struct stmt_test nested;
      s_init_stmt_test( &nested, test );
      test_block_item( semantic, &nested, list_data( &i ) );
      list_next( &i );
   }
   if ( ! test->manual_scope ) {
      s_pop_scope( semantic );
   }
}

void test_block_item( struct semantic* semantic, struct stmt_test* test,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_ENUMERATION:
      s_test_enumeration( semantic,
         ( struct enumeration* ) node );
      break;
   case NODE_VAR:
      s_test_local_var( semantic,
         ( struct var* ) node );
      break;
   case NODE_STRUCTURE:
      s_test_struct( semantic,
         ( struct structure* ) node );
      break;
   case NODE_FUNC:
      s_test_nested_func( semantic,
         ( struct func* ) node );
      break;
   case NODE_CASE:
      test_case( semantic, test,
         ( struct case_label* ) node );
      break;
   case NODE_CASE_DEFAULT:
      test_default_case( semantic, test,
         ( struct case_label* ) node );
      break;
   case NODE_ASSERT:
      test_assert( semantic,
         ( struct assert* ) node );
      break;
   case NODE_USING:
      s_perform_using( semantic,
         ( struct using_dirc* ) node );
      break;
   case NODE_TYPE_ALIAS:
      s_test_type_alias( semantic,
         ( struct type_alias* ) node );
      break;
   default:
      test_stmt( semantic, test, node );
   }
}

void test_case( struct semantic* semantic, struct stmt_test* test,
   struct case_label* label ) {
   struct stmt_test* switch_test = test->parent;
   while ( switch_test && ! switch_test->switch_stmt ) {
      switch_test = switch_test->parent;
   }
   if ( ! switch_test ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "case outside switch statement" );
      s_bail( semantic );
   }
   if ( ! test->parent->case_allowed ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "case nested inside another statement" );
      s_bail( semantic );
   }
   struct type_info type;
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr_type( semantic, &expr, &type, label->number );
   if ( ! label->number->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &label->number->pos,
         "case value not constant" );
      s_bail( semantic );
   }
   // Check case type.
   if ( ! s_same_type( &type, &switch_test->cond_type ) ) {
      s_type_mismatch( semantic, "case-value", &type,
         "switch-condition", &switch_test->cond_type,
         &label->number->pos );
      s_bail( semantic );
   }
   // Check for a duplicate case.
   struct case_label* prev = NULL;
   struct case_label* curr = switch_test->switch_stmt->case_head;
   while ( curr && curr->number->value < label->number->value ) {
      prev = curr;
      curr = curr->next;
   }
   if ( curr && curr->number->value == label->number->value ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "duplicate case" );
      s_diag( semantic, DIAG_POS, &curr->pos,
         "case with same value previously found here" );
      s_bail( semantic );
   }
   // Append case.
   if ( prev ) {
      label->next = prev->next;
      prev->next = label;
   }
   else {
      label->next = switch_test->switch_stmt->case_head;
      switch_test->switch_stmt->case_head = label;
   }
}

void test_default_case( struct semantic* semantic, struct stmt_test* test,
   struct case_label* label ) {
   struct stmt_test* switch_test = test->parent;
   while ( switch_test && ! switch_test->switch_stmt ) {
      switch_test = switch_test->parent;
   }
   if ( ! switch_test ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "default outside switch statement" );
      s_bail( semantic );
   }
   if ( ! test->parent->case_allowed ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "default case nested inside another statement" );
      s_bail( semantic );
   }
   if ( switch_test->switch_stmt->case_default ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "duplicate default case" );
      s_diag( semantic, DIAG_POS,
         &switch_test->switch_stmt->case_default->pos,
         "default case found here" );
      s_bail( semantic );
   }
   switch_test->switch_stmt->case_default = label;
}

void test_assert( struct semantic* semantic, struct assert* assert ) {
   s_test_bool_expr( semantic, assert->cond );
   if ( assert->is_static ) {
      if ( ! assert->cond->folded ) {
         s_diag( semantic, DIAG_POS_ERR, &assert->cond->pos,
            "static-assert condition not constant" );
         s_bail( semantic );
      }
      if ( ! assert->cond->value ) {
         s_diag( semantic, DIAG_POS, &assert->pos,
            "assertion failure%s%s",
            assert->custom_message ? ": " : "",
            assert->custom_message ? assert->custom_message : "" );
         s_bail( semantic );
      }
   }
}

void test_stmt( struct semantic* semantic, struct stmt_test* test,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_BLOCK:
      test_block( semantic, test,
         ( struct block* ) node );
      break;
   case NODE_IF:
      test_if( semantic, test,
         ( struct if_stmt* ) node );
      break;
   case NODE_SWITCH:
      test_switch( semantic, test,
         ( struct switch_stmt* ) node );
      break;
   case NODE_WHILE:
      test_while( semantic, test,
         ( struct while_stmt* ) node );
      break;
   case NODE_FOR:
      test_for( semantic, test,
         ( struct for_stmt* ) node );
      break;
   case NODE_FOREACH:
      test_foreach( semantic, test,
         ( struct foreach_stmt* ) node );
      break;
   case NODE_JUMP:
      test_jump( semantic, test,
         ( struct jump* ) node );
      break;
   case NODE_SCRIPT_JUMP:
      test_script_jump( semantic, test,
         ( struct script_jump* ) node );
      break;
   case NODE_RETURN:
      test_return( semantic, test,
         ( struct return_stmt* ) node );
      break;
   case NODE_GOTO:
      test_goto( semantic, test,
         ( struct goto_stmt* ) node );
      break;
   case NODE_PALTRANS:
      test_paltrans( semantic, test,
         ( struct paltrans* ) node );
      break;
   case NODE_EXPR_STMT:
      test_expr_stmt( semantic,
         ( struct expr_stmt* ) node );
      break;
   case NODE_INLINE_ASM:
      p_test_inline_asm( semantic, test,
         ( struct inline_asm* ) node );
      break;
   case NODE_GOTO_LABEL:
      break;
   default:
      UNREACHABLE();
   }
}

void test_if( struct semantic* semantic, struct stmt_test* test,
   struct if_stmt* stmt ) {
   s_add_scope( semantic, false );
   test_cond( semantic, &stmt->cond );
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   test_stmt( semantic, &body, stmt->body );
   if ( stmt->else_body ) {
      s_init_stmt_test( &body, test );
      test_stmt( semantic, &body, stmt->else_body );
   }
   s_pop_scope( semantic );
}

void test_cond( struct semantic* semantic, struct cond* cond ) {
   if ( cond->u.node->type == NODE_VAR ) {
      s_test_local_var( semantic, cond->u.var );
      // NOTE: For now, we don't check to see if the variable or its
      // initializer can be converted to a boolean. We assume it can.
   }
   else {
      s_test_bool_expr( semantic, cond->u.expr );
   }
}

void test_switch( struct semantic* semantic, struct stmt_test* test,
   struct switch_stmt* stmt ) {
   test->switch_stmt = stmt;
   s_add_scope( semantic, false );
   test_switch_cond( semantic, test, stmt );
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.case_allowed = true;
   test_stmt( semantic, &body, stmt->body );
   stmt->jump_break = test->jump_break;
   if ( stmt->case_head || stmt->case_default ) {
      warn_switch_skipped_init( semantic, ( struct block* ) stmt->body );
   }
   s_pop_scope( semantic );
}

void test_switch_cond( struct semantic* semantic, struct stmt_test* test,
   struct switch_stmt* stmt ) {
   if ( stmt->cond.u.node->type == NODE_VAR ) {
      s_test_local_var( semantic, stmt->cond.u.var );
      s_init_type_info( &test->cond_type, stmt->cond.u.var->ref,
         stmt->cond.u.var->structure, stmt->cond.u.var->enumeration,
         stmt->cond.u.var->dim, stmt->cond.u.var->spec,
         stmt->cond.u.var->storage );
      s_decay( semantic, &test->cond_type );
   }
   else {
      struct expr_test expr;
      s_init_expr_test( &expr, true, true );
      s_test_expr_type( semantic, &expr, &test->cond_type, stmt->cond.u.expr );
   }
   // Only condition of a primitive type is supported.
   if ( ! s_is_primitive_type( &test->cond_type ) ) {
      s_diag( semantic, DIAG_POS_ERR, stmt->cond.u.node->type == NODE_VAR ?
         &stmt->cond.u.var->object.pos : &stmt->cond.u.expr->pos,
         "condition of switch statement not of primitive type" );
      s_bail( semantic );
   }
}

// Just check any of the variables appearing before the first case for now.
void warn_switch_skipped_init( struct semantic* semantic,
   struct block* block ) {
   struct var* last_var = NULL;
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      if ( node->type == NODE_VAR ) {
         struct var* var = ( struct var* ) node;
         if ( var->initial ) {
            s_diag( semantic, DIAG_POS | DIAG_WARN, &var->object.pos,
               "initialization of this variable will be skipped" );
            last_var = var;
         }
      }
      else if (
         node->type == NODE_CASE ||
         node->type == NODE_CASE_DEFAULT ) {
         break;
      }
      list_next( &i );
   }
   if ( last_var ) {
      s_diag( semantic, DIAG_POS, &last_var->object.pos,
         "the switch statement jumps directly to a case without executing any "
         "prior code" );
   }
}

void test_while( struct semantic* semantic, struct stmt_test* test,
   struct while_stmt* stmt ) {
   test->in_loop = true;
   s_add_scope( semantic, false );
   if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
      test_cond( semantic, &stmt->cond );
   }
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   test_stmt( semantic, &body, stmt->body );
   stmt->jump_break = test->jump_break;
   stmt->jump_continue = test->jump_continue;
   if ( stmt->type == WHILE_DO_WHILE || stmt->type == WHILE_DO_UNTIL ) {
      test_cond( semantic, &stmt->cond );
   }
   s_pop_scope( semantic );
}

void test_for( struct semantic* semantic, struct stmt_test* test,
   struct for_stmt* stmt ) {
   test->in_loop = true;
   s_add_scope( semantic, false );
   // Initialization.
   list_iter_t i;
   list_iter_init( &i, &stmt->init );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      if ( node->type == NODE_EXPR ) {
         struct expr_test expr;
         s_init_expr_test( &expr, false, false );
         s_test_expr( semantic, &expr, ( struct expr* ) node );
      }
      else if ( node->type == NODE_VAR ) {
         s_test_local_var( semantic, ( struct var* ) node );
      }
      else if ( node->type == NODE_STRUCTURE ) {
         struct structure* structure = ( struct structure* ) node;
         s_diag( semantic, DIAG_POS_ERR, &structure->object.pos,
            "struct in for-loop initialization" );
         s_bail( semantic );
      }
      else {
         UNREACHABLE();
      }
      list_next( &i );
   }
   // Condition.
   if ( stmt->cond.u.node ) {
      test_cond( semantic, &stmt->cond );
   }
   // Post expressions.
   list_iter_init( &i, &stmt->post );
   while ( ! list_end( &i ) ) {
      struct expr_test expr;
      s_init_expr_test( &expr, false, false );
      s_test_expr( semantic, &expr, list_data( &i ) );
      list_next( &i );
   }
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   test_stmt( semantic, &body, stmt->body );
   stmt->jump_break = test->jump_break;
   stmt->jump_continue = test->jump_continue;
   s_pop_scope( semantic );
}

void test_foreach( struct semantic* semantic, struct stmt_test* test,
   struct foreach_stmt* stmt ) {
   test->in_loop = true;
   s_add_scope( semantic, false );
   struct var* key = stmt->key;
   struct var* value = stmt->value;
   // Collection.
   struct type_info collection_type;
   struct expr_test expr;
   s_init_expr_test( &expr, false, false );
   s_test_expr_type( semantic, &expr, &collection_type, stmt->collection );
   struct type_iter iter;
   s_iterate_type( semantic, &collection_type, &iter );
   if ( ! iter.available ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->collection->pos,
         "expression not of iterable type" );
      s_bail( semantic );
   }
   if ( s_is_ref_type( &iter.value ) && expr.var ) {
      expr.var->addr_taken = true;
      if ( ! expr.var->hidden ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->collection->pos,
            "non-private collection (references only work with private map "
            "variables)" );
         s_bail( semantic );
      }
   }
   // Key.
   if ( stmt->key ) {
      s_test_foreach_var( semantic, &iter.key, key );
      struct type_info type;
      s_init_type_info( &type, key->ref, key->structure, key->enumeration,
         key->dim, key->spec, key->storage );
      if ( ! s_instance_of( &type, &iter.key ) ) {
         s_type_mismatch( semantic, "key", &type,
            "collection-key", &iter.key, &key->object.pos );
         s_bail( semantic );
      }
   }
   // Value.
   s_test_foreach_var( semantic, &iter.value, value );
   struct type_info type;
   s_init_type_info( &type, value->ref, value->structure, value->enumeration,
      value->dim, value->spec, value->storage );
   if ( ! s_instance_of( &type, &iter.value ) ) {
      s_type_mismatch( semantic, "value", &type,
         "collection-value", &iter.value, &value->object.pos );
      s_bail( semantic );
   }
   // Body.
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   test_stmt( semantic, &body, stmt->body );
   stmt->jump_break = test->jump_break;
   stmt->jump_continue = test->jump_continue;
   s_pop_scope( semantic );
}

void test_jump( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt ) {
   switch ( stmt->type ) {
   case JUMP_BREAK:
      test_break( semantic, test, stmt );
      break;
   case JUMP_CONTINUE:
      test_continue( semantic, test, stmt );
      break;
   default:
      // TODO: Internal compiler error.
      break;
   }
}

void test_break( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_loop && ! target->switch_stmt ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
         "break outside loop or switch" );
      s_bail( semantic );
   }
   stmt->next = target->jump_break;
   target->jump_break = stmt;
}

void test_continue( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_loop ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
         "continue outside loop" );
      s_bail( semantic );
   }
   stmt->next = target->jump_continue;
   target->jump_continue = stmt;
}

void test_script_jump( struct semantic* semantic, struct stmt_test* test,
   struct script_jump* stmt ) {
   static const char* names[] = { "terminate", "restart", "suspend" };
   STATIC_ASSERT( ARRAY_SIZE( names ) == SCRIPT_JUMP_TOTAL );
   if ( ! semantic->func_test->script ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
         "`%s` outside script", names[ stmt->type ] );
      s_bail( semantic );
   }
}

void test_return( struct semantic* semantic, struct stmt_test* test,
   struct return_stmt* stmt ) {
   if ( ! semantic->func_test->func ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
         "return statement outside function" );
      s_bail( semantic );
   }
   if ( stmt->return_value ) {
      test_return_value( semantic, test, stmt );
   }
   else {
      if ( semantic->func_test->func->return_spec == SPEC_AUTO ) {
         semantic->func_test->func->return_spec = SPEC_VOID;
      }
      if ( ! ( semantic->func_test->func->return_spec == SPEC_VOID &&
         ! semantic->func_test->func->ref ) ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
            "missing return value" );
         s_bail( semantic );
      }
   }
   stmt->next = semantic->func_test->returns;
   semantic->func_test->returns = stmt;
}

void test_return_value( struct semantic* semantic, struct stmt_test* test,
   struct return_stmt* stmt ) {
   struct func* func = semantic->func_test->func;
   struct type_info type;
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr_type( semantic, &expr, &type, stmt->return_value );
   if ( func->return_spec == SPEC_VOID && ! func->ref ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->pos,
         "returning a value from void function" );
      s_bail( semantic );
   }
   // Return value must be of the same type as the return type.
   if ( func->return_spec == SPEC_AUTO ) {
      if ( s_is_null( &type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->pos,
            "cannot deduce return type from `null`" );
         s_bail( semantic );
      }
      struct type_snapshot snapshot;
      s_take_type_snapshot( &type, &snapshot );
      func->ref = snapshot.ref;
      func->structure = snapshot.structure;
      func->return_spec = snapshot.spec;
   }
   else {
      struct type_info return_type;
      s_init_type_info( &return_type, func->ref, func->structure,
         func->enumeration, NULL, func->return_spec, STORAGE_LOCAL );
      if ( ! s_instance_of( &return_type, &type ) ) {
         s_type_mismatch( semantic, "return-value", &type,
            "function-return", &return_type, &stmt->return_value->pos );
         s_bail( semantic );
      }
   }
   if ( expr.func && expr.func->type == FUNC_USER ) {
      struct func_user* impl = expr.func->impl;
      if ( impl->nested ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->pos,
            "returning nested function" );
         s_bail( semantic );
      }
   }
   if ( s_is_ref_type( &type ) && expr.var ) {
      expr.var->addr_taken = true;
      if ( ! expr.var->hidden ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->pos,
            "non-private return-value (references only work with private map "
            "variables)" );
         s_bail( semantic );
      }
   }
}

void test_goto( struct semantic* semantic, struct stmt_test* test,
   struct goto_stmt* stmt ) {
   list_iter_t i;
   list_iter_init( &i, semantic->func_test->labels );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      if ( strcmp( label->name, stmt->label_name ) == 0 ) {
         stmt->label = label;
         break;
      }
      list_next( &i );
   }
   if ( ! stmt->label ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->label_name_pos,
         "label `%s` not found", stmt->label_name );
      s_bail( semantic );
   }
}

void test_paltrans( struct semantic* semantic, struct stmt_test* test,
   struct paltrans* stmt ) {
   test_paltrans_arg( semantic, stmt->number, false );
   struct palrange* range = stmt->ranges;
   while ( range ) {
      test_paltrans_arg( semantic, range->begin, false );
      test_paltrans_arg( semantic, range->end, false );
      if ( range->rgb || range->saturated ) {
         test_paltrans_arg( semantic,
            range->value.rgb.red1,
            range->saturated );
         test_paltrans_arg( semantic,
            range->value.rgb.green1,
            range->saturated );
         test_paltrans_arg( semantic,
            range->value.rgb.blue1,
            range->saturated );
         test_paltrans_arg( semantic,
            range->value.rgb.red2,
            range->saturated );
         test_paltrans_arg( semantic,
            range->value.rgb.green2,
            range->saturated );
         test_paltrans_arg( semantic,
            range->value.rgb.blue2,
            range->saturated );
      }
      else {
         test_paltrans_arg( semantic, range->value.ent.begin, false );
         test_paltrans_arg( semantic, range->value.ent.end, false );
      }
      range = range->next;
   }
}

void test_paltrans_arg( struct semantic* semantic, struct expr* arg,
   bool require_fixed_type ) {
   struct type_info type;
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr_type( semantic, &expr, &type, arg );
   struct type_info required_type;
   s_init_type_info_scalar( &required_type, s_spec( semantic,
      require_fixed_type ? SPEC_FIXED : SPEC_INT ) );
   if ( ! s_instance_of( &required_type, &type ) ) {
      s_type_mismatch( semantic, "argument", &type,
         "required", &required_type, &arg->pos );
      s_bail( semantic );
   }
}

void test_expr_stmt( struct semantic* semantic, struct expr_stmt* stmt ) {
   list_iter_t i;
   list_iter_init( &i, &stmt->expr_list );
   bool require_assign = ( list_size( &stmt->expr_list ) > 1 );
   struct expr* first_expr = list_head( &stmt->expr_list );
   while ( ! list_end( &i ) ) {
      struct expr* expr = list_data( &i );
      switch ( semantic->lang ) {
      case LANG_ACS:
      case LANG_ACS95:
         if ( require_assign && expr->root->type != NODE_ASSIGN ) {
            s_diag( semantic, DIAG_POS_ERR, &expr->pos,
               "expression not an assignment operation" );
            s_bail( semantic );
         }
         break;
      default:
         break;
      }
      struct expr_test expr_test;
      s_init_expr_test( &expr_test, false, false );
      s_test_expr( semantic, &expr_test, expr );
      list_next( &i );
   }
}

void check_dup_label( struct semantic* semantic ) {
   list_iter_t i;
   list_iter_init( &i, semantic->func_test->labels );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      list_next( &i );
      list_iter_t k = i;
      while ( ! list_end( &k ) ) {
         struct label* other_label = list_data( &k );
         if ( strcmp( label->name, other_label->name ) == 0 ) {
            s_diag( semantic, DIAG_POS_ERR, &label->pos,
               "duplicate label `%s`", label->name );
            s_diag( semantic, DIAG_POS, &other_label->pos,
               "label already found here" );
            s_bail( semantic );
         }
         list_next( &k );
      }
   }
}