#include <string.h>

#include "phase.h"


static void test_block( struct semantic* semantic, struct stmt_test* test,
   struct block* block );
static void test_block_item( struct semantic* semantic, struct stmt_test*, struct node* );
static void test_case( struct semantic* semantic, struct stmt_test* test,
   struct case_label* label );
static void test_default_case( struct semantic* semantic,
   struct stmt_test* test, struct case_label* label );
static void test_label( struct semantic* semantic, struct stmt_test*, struct label* );
static void test_assert( struct semantic* semantic, struct assert* assert );
static void test_stmt( struct semantic* semantic, struct stmt_test* test,
   struct node* node );
static void test_if( struct semantic* semantic, struct stmt_test*, struct if_stmt* );
static void test_switch( struct semantic* semantic, struct stmt_test*,
   struct switch_stmt* );
static void test_while( struct semantic* semantic, struct stmt_test*, struct while_stmt* );
static void test_for( struct semantic* semantic, struct stmt_test* test,
   struct for_stmt* );
static void test_foreach( struct semantic* semantic, struct stmt_test* test,
   struct foreach_stmt* stmt );
static void test_jump( struct semantic* semantic, struct stmt_test*, struct jump* );
static void test_break( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt );
static void test_continue( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt );
static void test_script_jump( struct semantic* semantic, struct stmt_test*,
   struct script_jump* );
static void test_return( struct semantic* semantic, struct stmt_test*,
   struct return_stmt* );
static void test_goto( struct semantic* semantic, struct stmt_test*, struct goto_stmt* );
static void test_paltrans( struct semantic* semantic, struct stmt_test*, struct paltrans* );
static void test_paltrans_arg( struct semantic* semantic, struct expr* expr );
static void test_packed_expr( struct semantic* semantic, struct stmt_test*,
   struct packed_expr*, struct pos* );

void s_init_stmt_test( struct stmt_test* test, struct stmt_test* parent ) {
   test->parent = parent;
   test->switch_stmt = NULL;
   test->jump_break = NULL;
   test->jump_continue = NULL;
   test->in_loop = false;
   test->manual_scope = false;
}

void s_test_body( struct semantic* semantic, struct node* node ) {
   struct stmt_test test;
   s_init_stmt_test( &test, NULL );
   test.manual_scope = true;
   if ( node->type == NODE_BLOCK ) {
      test_block( semantic, &test,
         ( struct block* ) node );
   }
   else {
      test_stmt( semantic, &test, node );
   }
}

void test_block( struct semantic* semantic, struct stmt_test* test,
   struct block* block ) {
   if ( ! test->manual_scope ) {
      s_add_scope( semantic );
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
   case NODE_CONSTANT:
      s_test_constant( semantic, ( struct constant* ) node );
      break;
   case NODE_ENUMERATION:
      s_test_enumeration( semantic,
         ( struct enumeration* ) node );
      break;
   case NODE_VAR:
      s_test_local_var( semantic, ( struct var* ) node );
      break;
   case NODE_STRUCTURE:
      s_test_struct( semantic, ( struct structure* ) node );
      break;
   case NODE_FUNC:
      s_test_func( semantic, ( struct func* ) node );
      break;
   case NODE_CASE:
      test_case( semantic, test, ( struct case_label* ) node );
      break;
   case NODE_CASE_DEFAULT:
      test_default_case( semantic, test, ( struct case_label* ) node );
      break;
   case NODE_GOTO_LABEL:
      test_label( semantic, test, ( struct label* ) node );
      break;
   case NODE_ASSERT:
      test_assert( semantic,
         ( struct assert* ) node );
      break;
   case NODE_USING:
      s_perform_using( semantic,
         ( struct using_dirc* ) node );
      break;
   default:
      test_stmt( semantic, test, node );
   }
}

void test_case( struct semantic* semantic, struct stmt_test* test,
   struct case_label* label ) {
   struct switch_stmt* switch_stmt = NULL;
   struct stmt_test* search_test = test;
   while ( search_test && ! switch_stmt ) {
      switch_stmt = search_test->switch_stmt;
      search_test = search_test->parent;
   }
   if ( ! switch_stmt ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "case outside switch statement" );
      s_bail( semantic );
   }
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, label->number );
   if ( ! label->number->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &label->number->pos,
         "case value not constant" );
      s_bail( semantic );
   }
   // Check case type.
   struct type_info cond_type;
   s_init_type_info( &cond_type, NULL, NULL, NULL, NULL,
      switch_stmt->cond->spec );
   struct type_info case_type;
   s_init_type_info( &case_type, NULL, NULL, NULL, NULL,
      label->number->spec );
   if ( ! s_same_type( &case_type, &cond_type ) ) {
      s_type_mismatch( semantic, "case-value", &case_type,
         "switch-condition", &cond_type, &label->number->pos );
      s_bail( semantic );
   }
   // Check for a duplicate case.
   struct case_label* prev = NULL;
   struct case_label* curr = switch_stmt->case_head;
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
      label->next = switch_stmt->case_head;
      switch_stmt->case_head = label;
   }
}

void test_default_case( struct semantic* semantic, struct stmt_test* test,
   struct case_label* label ) {
   struct switch_stmt* switch_stmt = NULL;
   struct stmt_test* search_test = test;
   while ( search_test && ! switch_stmt ) {
      switch_stmt = search_test->switch_stmt;
      search_test = search_test->parent;
   }
   if ( ! switch_stmt ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "default outside switch statement" );
      s_bail( semantic );
   }
   if ( switch_stmt->case_default ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "duplicate default case" );
      s_diag( semantic, DIAG_POS,
         &switch_stmt->case_default->pos,
         "default case found here" );
      s_bail( semantic );
   }
   switch_stmt->case_default = label;
}

void test_label( struct semantic* semantic, struct stmt_test* test,
   struct label* label ) {
}

void test_assert( struct semantic* semantic, struct assert* assert ) {
   s_test_cond( semantic, assert->cond );
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
      test_block( semantic, test, ( struct block* ) node );
      break;
   case NODE_IF:
      test_if( semantic, test, ( struct if_stmt* ) node );
      break;
   case NODE_SWITCH:
      test_switch( semantic, test, ( struct switch_stmt* ) node );
      break;
   case NODE_WHILE:
      test_while( semantic, test, ( struct while_stmt* ) node );
      break;
   case NODE_FOR:
      test_for( semantic, test, ( struct for_stmt* ) node );
      break;
   case NODE_FOREACH:
      test_foreach( semantic, test,
         ( struct foreach_stmt* ) node );
      break;
   case NODE_JUMP:
      test_jump( semantic, test, ( struct jump* ) node );
      break;
   case NODE_SCRIPT_JUMP:
      test_script_jump( semantic, test, ( struct script_jump* ) node );
      break;
   case NODE_RETURN:
      test_return( semantic, test, ( struct return_stmt* ) node );
      break;
   case NODE_GOTO:
      test_goto( semantic, test, ( struct goto_stmt* ) node );
      break;
   case NODE_PALTRANS:
      test_paltrans( semantic, test, ( struct paltrans* ) node );
      break;
   case NODE_PACKED_EXPR:
      test_packed_expr( semantic, test, ( struct packed_expr* ) node, NULL );
      break;
   case NODE_INLINE_ASM:
      p_test_inline_asm( semantic, test,
         ( struct inline_asm* ) node );
      break;
   default:
      // TODO: Internal compiler error.
      break;
   }
}

void test_if( struct semantic* semantic, struct stmt_test* test,
   struct if_stmt* stmt ) {
   s_test_cond( semantic, stmt->cond );
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   test_stmt( semantic, &body, stmt->body );
   if ( stmt->else_body ) {
      s_init_stmt_test( &body, test );
      test_stmt( semantic, &body, stmt->else_body );
   }
}

void test_switch( struct semantic* semantic, struct stmt_test* test,
   struct switch_stmt* stmt ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, true );
   s_test_expr( semantic, &expr, stmt->cond );
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.switch_stmt = stmt;
   test_stmt( semantic, &body, stmt->body );
   stmt->jump_break = body.jump_break;
}

void test_while( struct semantic* semantic, struct stmt_test* test,
   struct while_stmt* stmt ) {
   if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
      s_test_cond( semantic, stmt->cond );
   }
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.in_loop = true;
   test_stmt( semantic, &body, stmt->body );
   stmt->jump_break = body.jump_break;
   stmt->jump_continue = body.jump_continue;
   if ( stmt->type == WHILE_DO_WHILE || stmt->type == WHILE_DO_UNTIL ) {
      s_test_cond( semantic, stmt->cond );
   }
}

void test_for( struct semantic* semantic, struct stmt_test* test,
   struct for_stmt* stmt ) {
   s_add_scope( semantic );
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
   if ( stmt->cond ) {
      s_test_cond( semantic, stmt->cond );
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
   body.in_loop = true;
   test_stmt( semantic, &body, stmt->body );
   stmt->jump_break = body.jump_break;
   stmt->jump_continue = body.jump_continue;
   s_pop_scope( semantic );
}

void test_foreach( struct semantic* semantic, struct stmt_test* test,
   struct foreach_stmt* stmt ) {
   s_add_scope( semantic );
   struct var* key = stmt->key;
   struct var* value = stmt->value;
   if ( key ) {
      s_bind_name( semantic, key->name, &key->object );
   }
   s_bind_name( semantic, value->name, &value->object );
   // Collection.
   struct type_info collection_type;
   struct expr_test expr;
   s_init_expr_test( &expr, false, false );
   s_test_expr_type( semantic, &expr, &collection_type, stmt->collection );
   struct type_iter iter;
   s_iterate_type( &collection_type, &iter );
   if ( ! iter.available ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->collection->pos,
         "expression not of iterable type" );
      s_bail( semantic );
   }
   // Key.
   if ( stmt->key ) {
      s_test_foreach_var( semantic, &iter.key, key );
      struct type_info type;
      s_init_type_info( &type, key->ref, key->structure, key->enumeration,
         key->dim, key->spec );
      if ( ! s_same_type( &type, &iter.key ) ) {
         s_type_mismatch( semantic, "key", &type,
            "collection-key", &iter.key, &key->object.pos );
         s_bail( semantic );
      }
   }
   // Value.
   s_test_foreach_var( semantic, &iter.value, value );
   struct type_info type;
   s_init_type_info( &type, value->ref, value->structure, value->enumeration,
      value->dim, value->spec );
   if ( ! s_same_type( &type, &iter.value ) ) {
      s_type_mismatch( semantic, "value", &type,
         "collection-value", &iter.value, &value->object.pos );
      s_bail( semantic );
   }
   // Body.
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.in_loop = true;
   test_stmt( semantic, &body, stmt->body );
   stmt->jump_break = body.jump_break;
   stmt->jump_continue = body.jump_continue;
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
   struct func* func = semantic->func_test->func;
   if ( ! func ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
         "return statement outside function" );
      s_bail( semantic );
   }
   if ( stmt->return_value ) {
      struct type_info type;
      struct expr_test expr;
      s_init_expr_test( &expr, true, false );
      s_test_expr_type( semantic, &expr, &type, stmt->return_value->expr );
      if ( func->return_spec == SPEC_VOID ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->expr->pos,
            "returning a value in void function" );
         s_bail( semantic );
      }
      // Return value must be of the same type as the return type.
      struct type_info return_type;
      s_init_type_info( &return_type, func->ref, func->structure,
         func->enumeration, NULL, func->return_spec );
      if ( ! s_same_type( &type, &return_type ) ) {
         s_type_mismatch( semantic, "return-value", &type,
            "function-return", &return_type, &stmt->return_value->expr->pos );
         s_bail( semantic );
      }
   }
   else {
      if ( func->return_spec != SPEC_VOID ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
            "missing return value" );
         s_bail( semantic );
      }
   }
   stmt->next = semantic->func_test->returns;
   semantic->func_test->returns = stmt;
}

void test_goto( struct semantic* semantic, struct stmt_test* test,
   struct goto_stmt* stmt ) {
}

void test_paltrans( struct semantic* semantic, struct stmt_test* test,
   struct paltrans* stmt ) {
   test_paltrans_arg( semantic, stmt->number );
   struct palrange* range = stmt->ranges;
   while ( range ) {
      test_paltrans_arg( semantic, range->begin );
      test_paltrans_arg( semantic, range->end );
      if ( range->rgb ) {
         test_paltrans_arg( semantic, range->value.rgb.red1 );
         test_paltrans_arg( semantic, range->value.rgb.green1 );
         test_paltrans_arg( semantic, range->value.rgb.blue1 );
         test_paltrans_arg( semantic, range->value.rgb.red2 );
         test_paltrans_arg( semantic, range->value.rgb.green2 );
         test_paltrans_arg( semantic, range->value.rgb.blue2 );
      }
      else {
         test_paltrans_arg( semantic, range->value.ent.begin );
         test_paltrans_arg( semantic, range->value.ent.end );
      }
      range = range->next;
   }
}

void test_paltrans_arg( struct semantic* semantic, struct expr* expr ) {
   struct expr_test arg;
   s_init_expr_test( &arg, true, false );
   s_test_expr( semantic, &arg, expr );
}

void test_packed_expr( struct semantic* semantic, struct stmt_test* test,
   struct packed_expr* packed, struct pos* expr_pos ) {
   if ( packed->msgbuild_func ) {
      s_test_func( semantic, packed->msgbuild_func );
   }
   struct expr_test expr_test;
   s_init_expr_test_stmt( &expr_test, packed->msgbuild_func );
   s_test_expr( semantic, &expr_test, packed->expr );
   if ( expr_pos ) {
      *expr_pos = packed->expr->pos;
   }
   if ( packed->msgbuild_func ) {
      struct func_user* impl = packed->msgbuild_func->impl;
      if ( ! impl->usage ) {
         s_diag( semantic, DIAG_POS | DIAG_WARN,
            &packed->msgbuild_func->object.pos,
            "one-time message-building function not used" );
      }
   }
}