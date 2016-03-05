#include <string.h>

#include "phase.h"

static void test_block_item( struct semantic* semantic, struct stmt_test*, struct node* );
static void test_case( struct semantic* semantic, struct stmt_test*, struct case_label* );
static void test_default_case( struct semantic* semantic, struct stmt_test*,
   struct case_label* );
static void test_label( struct semantic* semantic, struct stmt_test*, struct label* );
static void test_assert( struct semantic* semantic, struct assert* assert );
static void test_if( struct semantic* semantic, struct stmt_test*, struct if_stmt* );
static void test_switch( struct semantic* semantic, struct stmt_test*,
   struct switch_stmt* );
static void test_switch_case_type( struct semantic* semantic,
   struct switch_stmt* stmt );
static void invalid_case_type( struct semantic* semantic,
   struct switch_stmt* stmt, struct case_label* label );
static void test_while( struct semantic* semantic, struct stmt_test*, struct while_stmt* );
static void test_for( struct semantic* semantic, struct stmt_test* test,
   struct for_stmt* );
static void test_jump( struct semantic* semantic, struct stmt_test*, struct jump* );
static void test_break( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt );
static void test_continue( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt );
static void test_script_jump( struct semantic* semantic, struct stmt_test*,
   struct script_jump* );
static void test_return( struct semantic* semantic, struct stmt_test*,
   struct return_stmt* );
static void return_value_mismatch( struct semantic* semantic,
   struct func* func, struct return_stmt* stmt, struct pos* pos );
static void test_goto( struct semantic* semantic, struct stmt_test*, struct goto_stmt* );
static void test_paltrans( struct semantic* semantic, struct stmt_test*, struct paltrans* );
static void test_paltrans_arg( struct semantic* semantic, struct expr* expr );
static void test_format_item( struct semantic* semantic, struct stmt_test*,
   struct format_item* );
static void test_packed_expr( struct semantic* semantic, struct stmt_test*,
   struct packed_expr*, struct pos* );
static void test_goto_in_format_block( struct semantic* semantic, struct list* );

void s_init_stmt_test( struct stmt_test* test, struct stmt_test* parent ) {
   test->parent = parent;
   test->func = NULL;
   test->labels = NULL;
   test->format_block = NULL;
   test->case_head = NULL;
   test->case_default = NULL;
   test->jump_break = NULL;
   test->jump_continue = NULL;
   test->nested_funcs = NULL;
   test->returns = NULL;
   test->in_loop = false;
   test->in_switch = false;
   test->in_script = false;
   test->manual_scope = false;
}

void s_test_block( struct semantic* semantic, struct stmt_test* test,
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
   if ( ! test->parent ) {
      test_goto_in_format_block( semantic, test->labels );
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
   default:
      s_test_stmt( semantic, test, node );
   }
}

void test_case( struct semantic* semantic, struct stmt_test* test,
   struct case_label* label ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_switch ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "case outside switch statement" );
      s_bail( semantic );
   }
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, false );
   s_test_expr( semantic, &expr, label->number );
   if ( ! label->number->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &label->number->pos,
         "case value not constant" );
      s_bail( semantic );
   }
   struct case_label* prev = NULL;
   struct case_label* curr = target->case_head;
   while ( curr && curr->number->value < label->number->value ) {
      prev = curr;
      curr = curr->next;
   }
   if ( curr && curr->number->value == label->number->value ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos, "duplicate case" );
      s_diag( semantic, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &curr->pos,
         "case with value %d found here", curr->number->value );
      s_bail( semantic );
   }
   if ( prev ) {
      label->next = prev->next;
      prev->next = label;
   }
   else {
      label->next = target->case_head;
      target->case_head = label;
   }
}

void test_default_case( struct semantic* semantic, struct stmt_test* test,
   struct case_label* label ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_switch ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos,
         "default outside switch statement" );
      s_bail( semantic );
   }
   if ( target->case_default ) {
      s_diag( semantic, DIAG_POS_ERR, &label->pos, "duplicate default case" );
      s_diag( semantic, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &target->case_default->pos,
         "default case found here" );
      s_bail( semantic );
   }
   target->case_default = label;
}

void test_label( struct semantic* semantic, struct stmt_test* test,
   struct label* label ) {
   // The label might be inside a format block. Find this block.
   struct stmt_test* target = test;
   while ( target && ! target->format_block ) {
      target = target->parent;
   }
   if ( target ) {
      label->format_block = target->format_block;
   }
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

void s_test_stmt( struct semantic* semantic, struct stmt_test* test,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_BLOCK:
      s_test_block( semantic, test, ( struct block* ) node );
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
   case NODE_FORMAT_ITEM:
      test_format_item( semantic, test, ( struct format_item* ) node );
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
   s_test_stmt( semantic, &body, stmt->body );
   if ( stmt->else_body ) {
      s_init_stmt_test( &body, test );
      s_test_stmt( semantic, &body, stmt->else_body );
   }
}

void test_switch( struct semantic* semantic, struct stmt_test* test,
   struct switch_stmt* stmt ) {
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, true );
   s_test_expr( semantic, &expr, stmt->cond );
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.in_switch = true;
   s_test_stmt( semantic, &body, stmt->body );
   stmt->case_head = body.case_head;
   stmt->case_default = body.case_default;
   stmt->jump_break = body.jump_break;
   test_switch_case_type( semantic, stmt );
}

void test_switch_case_type( struct semantic* semantic,
   struct switch_stmt* stmt ) {
   struct case_label* label = stmt->case_head;
   while ( label ) {
      if ( label->number->spec != stmt->cond->spec ) {
         invalid_case_type( semantic, stmt, label );
         s_bail( semantic );
      }
      label = label->next;
   }
}

void invalid_case_type( struct semantic* semantic,
   struct switch_stmt* stmt, struct case_label* label ) {
   struct str type;
   str_init( &type );
   s_present_spec( stmt->cond->spec, &type );
   struct str label_type;
   str_init( &label_type );
   s_present_spec( label->number->spec, &label_type );
   s_diag( semantic, DIAG_POS_ERR, &label->pos,
      "case-value/switch-condition type mismatch" );
   s_diag( semantic, DIAG_POS, &label->pos,
      "`%s` case-value, but `%s` switch-condition",
      label_type.value, type.value );
   str_deinit( &type );
   str_deinit( &label_type );
}

void test_while( struct semantic* semantic, struct stmt_test* test,
   struct while_stmt* stmt ) {
   if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
      s_test_cond( semantic, stmt->cond );
   }
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.in_loop = true;
   s_test_stmt( semantic, &body, stmt->body );
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
         s_init_expr_test( &expr, NULL, NULL, false, false );
         s_test_expr( semantic, &expr, ( struct expr* ) node );
      }
      else {
         s_test_local_var( semantic, ( struct var* ) node );
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
      s_init_expr_test( &expr, NULL, NULL, false, false );
      s_test_expr( semantic, &expr, list_data( &i ) );
      list_next( &i );
   }
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.in_loop = true;
   s_test_stmt( semantic, &body, stmt->body );
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
   while ( target && ! target->in_loop && ! target->in_switch ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
         "break outside loop or switch" );
      s_bail( semantic );
   }
   stmt->next = target->jump_break;
   target->jump_break = stmt;
   // Jumping out of a format block is not allowed.
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
            "leaving format block with a break statement" );
         s_bail( semantic );
      }
      target = target->parent;
   }
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
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
            "leaving format block with a continue statement" );
         s_bail( semantic );
      }
      target = target->parent;
   }
}

void test_script_jump( struct semantic* semantic, struct stmt_test* test,
   struct script_jump* stmt ) {
   static const char* names[] = { "terminate", "restart", "suspend" };
   STATIC_ASSERT( ARRAY_SIZE( names ) == SCRIPT_JUMP_TOTAL );
   struct stmt_test* target = test;
   while ( target && ! target->in_script ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
         "`%s` outside script", names[ stmt->type ] );
      s_bail( semantic );
   }
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
            "`%s` inside format block", names[ stmt->type ] );
         s_bail( semantic );
      }
      target = target->parent;
   }
}

void test_return( struct semantic* semantic, struct stmt_test* test,
   struct return_stmt* stmt ) {
   struct stmt_test* target = test;
   while ( target && ! target->func ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
         "return statement outside function" );
      s_bail( semantic );
   }
   if ( stmt->return_value ) {
      struct pos pos;
      test_packed_expr( semantic, test, stmt->return_value, &pos );
      if ( target->func->return_spec == SPEC_VOID ) {
         s_diag( semantic, DIAG_POS_ERR, &pos,
            "returning value in void function" );
         s_bail( semantic );
      }

/*
      struct type_attr type;
      s_init_expr_test( test, &type );
      s_test_type_expr( semantic );
      struct type_attr required_type;
      s_init_type_attr( &type,
         stmt->return_value->spec,
         stmt->return_value->structure );
*/
      //if ( ! s_same_types( &type ) ) {
      // Return value must be of the same type as the return type.
      if ( stmt->return_value->expr->spec != target->func->return_spec ) {
         return_value_mismatch( semantic, target->func, stmt, &pos );
         s_bail( semantic );
      }
   }
   else {
      if ( target->func->return_spec != SPEC_VOID ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
            "missing return value" );
         s_bail( semantic );
      }
   }
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
            "leaving format block with a return statement" );
         s_bail( semantic );
      }
      target = target->parent;
   }
   stmt->next = semantic->func_test->returns;
   semantic->func_test->returns = stmt;
}

void return_value_mismatch( struct semantic* semantic,
   struct func* func, struct return_stmt* stmt, struct pos* pos ) {
   struct str return_type;
   str_init( &return_type );
   s_present_spec( func->return_spec, &return_type );
   struct str value_type;
   str_init( &value_type );
   s_present_spec( stmt->return_value->expr->spec, &value_type );
   s_diag( semantic, DIAG_POS_ERR, pos,
      "return-value/return-type type mismatch" );
   s_diag( semantic, DIAG_POS, pos,
      "`%s` return-value, but `%s` return-type", value_type.value,
      return_type.value );
   str_deinit( &return_type );
   str_deinit( &value_type );
}

void test_goto( struct semantic* semantic, struct stmt_test* test,
   struct goto_stmt* stmt ) {
   struct stmt_test* target = test;
   while ( target ) {
      if ( target->format_block ) {
         stmt->format_block = target->format_block;
         break;
      }
      target = target->parent;
   }
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
   s_init_expr_test( &arg, NULL, NULL, true, false );
   s_test_expr( semantic, &arg, expr );
}

void test_format_item( struct semantic* semantic, struct stmt_test* test,
   struct format_item* item ) {
   s_test_formatitemlist_stmt( semantic, test, item );
   struct stmt_test* target = test;
   while ( target && ! target->format_block ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( semantic, DIAG_POS_ERR, &item->pos,
         "format item outside format block" );
      s_bail( semantic );
   }
}

void test_packed_expr( struct semantic* semantic, struct stmt_test* test,
   struct packed_expr* packed, struct pos* expr_pos ) {
   // Test expression.
   struct expr_test expr_test;
   s_init_expr_test( &expr_test, test, packed->block, false, false );
   s_test_expr( semantic, &expr_test, packed->expr );
   if ( expr_pos ) {
      *expr_pos = packed->expr->pos;
   }
   // Test format block.
   if ( packed->block ) {
      struct stmt_test nested;
      s_init_stmt_test( &nested, test );
      nested.format_block = packed->block;
      s_test_block( semantic, &nested, nested.format_block );
      if ( ! expr_test.format_block_usage ) {
         s_diag( semantic, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &packed->block->pos, "unused format block" );
      }
   }
}

void test_goto_in_format_block( struct semantic* semantic, struct list* labels ) {
   list_iter_t i;
   list_iter_init( &i, labels );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      if ( label->format_block ) {
         if ( label->users ) {
            struct goto_stmt* stmt = label->users;
            while ( stmt ) {
               if ( stmt->format_block != label->format_block ) {
                  s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
                     "entering format block with a goto statement" );
                  s_diag( semantic, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                     "point of entry is here" ); 
                  s_bail( semantic );
               }
               stmt = stmt->next;
            }
         }
         else {
            // If a label is unused, the user might have used the syntax of a
            // label to create a format-item.
            s_diag( semantic, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &label->pos, "unused label in format block" );
         }
      }
      else {
         struct goto_stmt* stmt = label->users;
         while ( stmt ) {
            if ( stmt->format_block ) {
               s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
                  "leaving format block with a goto statement" );
               s_diag( semantic, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                  "destination of goto statement is here" );
               s_bail( semantic );
            }
            stmt = stmt->next;
         }
      }
      list_next( &i );
   }
}