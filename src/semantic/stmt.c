#include <string.h>

#include "phase.h"

enum builtin_aliases_user {
   BUILTINALIASESUSER_BUILDMSG,
};

struct builtin_aliases {
   struct {
      struct alias alias;
      bool used;
   } append;
};

static void test_block( struct semantic* semantic, struct stmt_test* test,
   struct builtin_aliases* builtin_aliases, struct block* block );
static void init_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases, enum builtin_aliases_user user );
static void bind_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases );
static void test_block_item( struct semantic* semantic, struct stmt_test* test,
   struct node* node );
static void test_case( struct semantic* semantic, struct stmt_test* test,
   struct case_label* label );
static void test_default_case( struct semantic* semantic,
   struct stmt_test* test, struct case_label* label );
static void test_assert( struct semantic* semantic, struct stmt_test* test,
   struct assert* assert );
static void test_stmt( struct semantic* semantic, struct stmt_test* test,
   struct node* node );
static void test_if( struct semantic* semantic, struct stmt_test* test,
   struct if_stmt* stmt );
static void test_cond( struct semantic* semantic, struct cond* cond );
static void test_heavy_cond( struct semantic* semantic, struct stmt_test* test,
   struct heavy_cond* cond );
static struct pos* get_heavy_cond_pos( struct heavy_cond* cond );
static void test_switch( struct semantic* semantic, struct stmt_test* test,
   struct switch_stmt* stmt );
static void test_switch_cond( struct semantic* semantic,
   struct stmt_test* test, struct switch_stmt* stmt );
static void warn_switch_skipped_init( struct semantic* semantic,
   struct block* block );
static void test_while( struct semantic* semantic, struct stmt_test* test,
   struct while_stmt* stmt );
static void test_do( struct semantic* semantic, struct stmt_test* test,
   struct do_stmt* stmt );
static void test_for( struct semantic* semantic, struct stmt_test* test,
   struct for_stmt* stmt );
static void test_foreach( struct semantic* semantic, struct stmt_test* test,
   struct foreach_stmt* stmt );
static void test_jump( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt );
static void test_break( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt );
static void diag_leave( struct semantic* semantic, struct pos* pos,
   bool entering );
static void diag_early_leave( struct semantic* semantic, struct pos* pos );
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
static void test_label( struct semantic* semantic, struct stmt_test* test,
   struct label* label );
static void test_paltrans( struct semantic* semantic, struct stmt_test* test,
   struct paltrans* stmt );
static void test_paltrans_arg( struct semantic* semantic, struct expr* arg,
   bool require_fixed_type );
static void test_palrange_colorisation( struct semantic* semantic,
   struct palrange* range );
static void test_palrange_tint( struct semantic* semantic,
   struct palrange* range );
static void test_buildmsg_stmt( struct semantic* semantic,
   struct stmt_test* test, struct buildmsg_stmt* stmt );
static void test_buildmsg_block( struct semantic* semantic,
   struct stmt_test* test, struct buildmsg* buildmsg );
static bool in_msgbuild_block_range( struct stmt_test* start,
   struct stmt_test* end );
static struct buildmsg* find_buildmsg( struct stmt_test* start,
   struct stmt_test* end );
static void test_expr_stmt( struct semantic* semantic,
   struct expr_stmt* stmt );
static void check_dup_label( struct semantic* semantic );
static void test_goto_in_msgbuild_block( struct semantic* semantic );

void s_init_stmt_test( struct stmt_test* test, struct stmt_test* parent ) {
   test->parent = parent;
   test->switch_stmt = NULL;
   test->buildmsg = NULL;
   test->jump_break = NULL;
   test->jump_continue = NULL;
   test->num_cases = 0;
   test->flow = FLOW_GOING;
   test->in_loop = false;
   test->manual_scope = false;
   test->case_allowed = false;
   test->found_folded_case = false;
}

void s_test_top_block( struct semantic* semantic, struct block* block ) {
   struct stmt_test test;
   s_init_stmt_test( &test, NULL );
   test.manual_scope = true;
   test_block( semantic, &test, NULL, block );
   check_dup_label( semantic );
   test_goto_in_msgbuild_block( semantic );
}

void s_test_func_block( struct semantic* semantic, struct func* func,
   struct block* block ) {
   struct stmt_test test;
   s_init_stmt_test( &test, NULL );
   test.manual_scope = true;
   test_block( semantic, &test, NULL, block );
   check_dup_label( semantic );
   test_goto_in_msgbuild_block( semantic );
   // If the return type of the function is still `auto`, then the function has
   // no return statements in its body, so the return type is `void`.
   if ( func->return_spec == SPEC_AUTO ) {
      func->return_spec = SPEC_VOID;
   }
   if ( semantic->lang == LANG_BCS ) {
      if ( func->return_spec != SPEC_VOID && test.flow != FLOW_DEAD ) {
         s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
            "function missing return statement" );
         s_bail( semantic );
      }
   }
   else if ( semantic->lang == LANG_ACS ) {
      bool return_stmt_at_end = false;
      if ( list_size( &block->stmts ) > 0 ) {
         struct node* node = list_tail( &block->stmts );
         return_stmt_at_end = ( node->type == NODE_RETURN );
      }
      if ( func->return_spec != SPEC_VOID && ! return_stmt_at_end ) {
         s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
            "function missing return statement at end of body" );
         s_bail( semantic );
      }
   }
}

static void test_block( struct semantic* semantic, struct stmt_test* test,
   struct builtin_aliases* builtin_aliases, struct block* block ) {
   if ( ! test->manual_scope ) {
      s_add_scope( semantic, false );
   }
   if ( builtin_aliases ) {
      bind_builtin_aliases( semantic, builtin_aliases );
   }
   struct list_iter i;
   list_iterate( &block->stmts, &i );
   while ( ! list_end( &i ) ) {
      test_block_item( semantic, test, list_data( &i ) );
      list_next( &i );
   }
   if ( ! test->manual_scope ) {
      s_pop_scope( semantic );
   }
}

// THOUGHT: Instead of creating an alias for each builtin object, maybe making
// a link to a builtin namespace would be better.
static void init_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases, enum builtin_aliases_user user ) {
   // append().
   if ( user == BUILTINALIASESUSER_BUILDMSG ) {
      struct alias* alias = &aliases->append.alias;
      s_init_alias( alias );
      alias->object.resolved = true;
      alias->target = &semantic->task->append_func->object;
      aliases->append.used = true;
   }
   else {
      aliases->append.used = false;
   }
}

static void bind_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases ) {
   if ( aliases->append.used ) {
      struct name* name = t_extend_name( semantic->ns->body, "append" );
      s_bind_local_name( semantic, name, &aliases->append.alias.object, true );
   }
}

static void test_block_item( struct semantic* semantic, struct stmt_test* test,
   struct node* node ) {
   struct stmt_test nested_test;
   s_init_stmt_test( &nested_test, test );
   switch ( node->type ) {
   case NODE_CONSTANT:
      s_test_constant( semantic,
         ( struct constant* ) node );
      break;
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
      test_case( semantic, &nested_test,
         ( struct case_label* ) node );
      break;
   case NODE_CASE_DEFAULT:
      test_default_case( semantic, &nested_test,
         ( struct case_label* ) node );
      break;
   case NODE_ASSERT:
      test_assert( semantic, &nested_test,
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
      test_stmt( semantic, &nested_test, node );
   }
   // Flow.
   if ( (
      node->type == NODE_CASE ||
      node->type == NODE_CASE_DEFAULT ) ) {
      if ( ( nested_test.flow == FLOW_GOING && test->flow != FLOW_BREAKING ) ||
         nested_test.flow == FLOW_RESET ) {
         test->flow = FLOW_GOING;
      }
   }
   else {
      if ( test->flow == FLOW_GOING ) {
         test->flow = nested_test.flow;
      }
   }
}

static void test_case( struct semantic* semantic, struct stmt_test* test,
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
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, label->number );
   if ( s_describe_type( &switch_test->cond_type ) == TYPEDESC_ENUM ) {
      s_reveal( &expr.type );
   }
   if ( ! label->number->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &label->number->pos,
         "case value not constant" );
      s_bail( semantic );
   }
   // Check case type.
   if ( ! s_same_type( &expr.type, &switch_test->cond_type ) ) {
      s_type_mismatch( semantic, "case-value", &expr.type,
         "switch-condition", &switch_test->cond_type,
         &label->number->pos );
      s_bail( semantic );
   }
   // For a string-based switch statement, make sure each case is a valid
   // string.
   if ( switch_test->cond_type.spec == SPEC_STR &&
      ! t_lookup_string( semantic->task, label->number->value ) ) {
      s_diag( semantic, DIAG_POS_ERR, &label->number->pos,
         "case value not a valid string" );
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
   ++switch_test->num_cases;
   // Flow.
   if ( switch_test->switch_stmt->cond.expr &&
      switch_test->switch_stmt->cond.expr->folded ) {
      if ( label->number->value ==
         switch_test->switch_stmt->cond.expr->value ) {
         switch_test->found_folded_case = true;
         test->flow = FLOW_RESET;
      }
      else {
         test->flow = FLOW_DEAD;
      }
   }
}

static void test_default_case( struct semantic* semantic,
   struct stmt_test* test, struct case_label* label ) {
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
   // Flow.
   if ( switch_test->switch_stmt->cond.expr &&
      switch_test->switch_stmt->cond.expr->folded ) {
      if ( switch_test->found_folded_case ) {
         test->flow = FLOW_DEAD;
      }
      else {
         test->flow = FLOW_RESET;
      }
   }
}

static void test_assert( struct semantic* semantic, struct stmt_test* test,
   struct assert* assert ) {
   s_test_bool_expr( semantic, assert->cond );
   if ( assert->is_static ) {
      if ( ! assert->cond->folded ) {
         s_diag( semantic, DIAG_POS_ERR, &assert->cond->pos,
            "static-assert condition not constant" );
         s_bail( semantic );
      }
   }
   if ( assert->message ) {
      struct expr_test test;
      s_init_expr_test( &test, true, false );
      s_test_expr( semantic, &test, assert->message );
      struct type_info required_type;
      s_init_type_info_scalar( &required_type, SPEC_STR );
      if ( ! s_instance_of( &required_type, &test.type ) ) {
         s_type_mismatch( semantic, "argument", &test.type,
            "required", &required_type, &assert->message->pos );
         s_bail( semantic );
      }
      if ( assert->is_static ) {
         if ( ! assert->message->folded ) {
            s_diag( semantic, DIAG_POS_ERR, &assert->message->pos,
               "static-assert message not a constant expression" );
            s_bail( semantic );
         }
      }
   }
   // Execute static-assert.
   if ( assert->is_static ) {
      if ( ! assert->cond->value ) {
         struct indexed_string* string = NULL;
         if ( assert->message ) {
            string = t_lookup_string( semantic->task,
               assert->message->value );
            if ( ! string ) {
               s_diag( semantic, DIAG_POS_ERR, &assert->message->pos,
                  "static-assert message not a valid string" );
               s_bail( semantic );
            }
         }
         s_diag( semantic, DIAG_POS_ERR, &assert->pos,
            "assertion failure%s%s",
            string ? ": " : "",
            string ? string->value : "" );
         s_bail( semantic );
      }
   }
   else {
      // Flow.
      if ( assert->cond->folded && ! assert->cond->value ) {
         test->flow = FLOW_DEAD;
      }
   }
}

static void test_stmt( struct semantic* semantic, struct stmt_test* test,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_BLOCK:
      test_block( semantic, test, NULL,
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
   case NODE_DO:
      test_do( semantic, test,
         ( struct do_stmt* ) node );
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
   case NODE_GOTO_LABEL:
      test_label( semantic, test,
         ( struct label* ) node );
      break;
   case NODE_PALTRANS:
      test_paltrans( semantic, test,
         ( struct paltrans* ) node );
      break;
   case NODE_BUILDMSG:
      test_buildmsg_stmt( semantic, test,
         ( struct buildmsg_stmt* ) node );
      break;
   case NODE_EXPR_STMT:
      test_expr_stmt( semantic,
         ( struct expr_stmt* ) node );
      break;
   case NODE_INLINE_ASM:
      p_test_inline_asm( semantic, test,
         ( struct inline_asm* ) node );
      break;
   default:
      S_UNREACHABLE( semantic );
   }
}

static void test_if( struct semantic* semantic, struct stmt_test* test,
   struct if_stmt* stmt ) {
   s_add_scope( semantic, false );
   test_heavy_cond( semantic, test, &stmt->cond );
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   test_stmt( semantic, &body, stmt->body );
   struct stmt_test else_body;
   s_init_stmt_test( &else_body, test );
   if ( stmt->else_body ) {
      test_stmt( semantic, &else_body, stmt->else_body );
   }
   // Flow.
   // Constant condition:
   if ( stmt->cond.expr && stmt->cond.expr->folded ) {
      if ( stmt->cond.expr->value != 0 ) {
         test->flow = body.flow;
      }
      else {
         if ( stmt->else_body ) {
            test->flow = else_body.flow;
         }
      }
   }
   // Runtime condition:
   else {
      if ( body.flow == FLOW_BREAKING ||
         ( stmt->else_body && else_body.flow == FLOW_BREAKING ) ) {
         test->flow = FLOW_BREAKING;
      }
      else if ( body.flow == FLOW_DEAD &&
         ( stmt->else_body && else_body.flow == FLOW_DEAD ) ) {
         test->flow = FLOW_DEAD;
      }
   }
   s_pop_scope( semantic );
}

static void test_heavy_cond( struct semantic* semantic, struct stmt_test* test,
   struct heavy_cond* cond ) {
   if ( cond->var ) {
      s_test_local_var( semantic, cond->var );
      if ( test->switch_stmt ) {
         s_init_type_info( &test->cond_type, cond->var->ref,
            cond->var->structure, cond->var->enumeration, cond->var->dim,
            cond->var->spec, cond->var->storage );
      }
      if ( ! cond->var->force_local_scope ) {
         s_diag( semantic, DIAG_POS_ERR, &cond->var->object.pos,
            "non-local condition variable (to make the variable local, add "
            "the `let` keyword before the variable type)" );
         s_bail( semantic );
      }
   }
   if ( cond->expr ) {
      if ( test->switch_stmt ) {
         struct expr_test expr;
         s_init_expr_test( &expr, true, true );
         s_test_expr( semantic, &expr, cond->expr );
         s_init_type_info_copy( &test->cond_type, &expr.type );
         s_reveal( &test->cond_type );
      }
      else {
         s_test_bool_expr( semantic, cond->expr );
      }
      // For now, force the user to use the condition variable.
      if ( cond->var && ! cond->var->used ) {
         s_diag( semantic, DIAG_POS_ERR, &cond->var->object.pos,
            "condition variable not used in expression part of condition" );
         s_bail( semantic );
      }
   }
}

static struct pos* get_heavy_cond_pos( struct heavy_cond* cond ) {
   if ( cond->var && ! cond->expr ) {
      return &cond->var->object.pos;
   }
   else {
      return &cond->expr->pos;
   }
}

static void test_cond( struct semantic* semantic, struct cond* cond ) {
   if ( cond->u.node->type == NODE_VAR ) {
      s_test_local_var( semantic, cond->u.var );
      // NOTE: For now, we don't check to see if the variable or its
      // initializer can be converted to a boolean. We assume it can.
   }
   else {
      s_test_bool_expr( semantic, cond->u.expr );
   }
}

static void test_switch( struct semantic* semantic, struct stmt_test* test,
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
   // For a condition of an enumeration type, make sure each enumerator is
   // handled. When a default case is provided, it is assumed that each
   // unspecified case is handled.
   if ( s_describe_type( &test->cond_type ) == TYPEDESC_ENUM && ! (
      test->num_cases == test->cond_type.enumeration->num_enumerators ||
      stmt->case_default ) ) {
      enum { MISSING_CASE_SHOW_LIMIT = 10 };
      int missing = 0;
      struct enumerator* enumerator = test->cond_type.enumeration->head;
      while ( enumerator ) {
         struct case_label* label = stmt->case_head;
         while ( label && label->number->value != enumerator->value ) {
            label = label->next;
         }
         if ( ! label ) {
            // Report an error on the first missing case.
            if ( missing == 0 ) {
               s_diag( semantic, DIAG_POS_ERR,
                  get_heavy_cond_pos( &stmt->cond ),
                  "not all enumerators handled by switch statement" );
            }
            // Show the user some of the unhandled enumerators.
            if ( missing < MISSING_CASE_SHOW_LIMIT ) {
               struct str name;
               str_init( &name );
               t_copy_name( enumerator->name, &name );
               s_diag( semantic, DIAG_POS | DIAG_NOTE,
                  get_heavy_cond_pos( &stmt->cond ),
                  "a case is missing for enumerator `%s`", name.value );
               str_deinit( &name );
            }
            ++missing;
         }
         enumerator = enumerator->next;
      }
      if ( missing > 0 ) {
         int not_shown = missing - MISSING_CASE_SHOW_LIMIT;
         if ( not_shown > 0 ) {
            s_diag( semantic, DIAG_POS | DIAG_NOTE,
               get_heavy_cond_pos( &stmt->cond ),
               "a case is missing for %d more enumerator%s", not_shown,
               not_shown == 1 ? "" : "s" );
         }
         s_bail( semantic );
      }
   }
   // Flow.
   if ( ( test->found_folded_case || stmt->case_default ) &&
      body.flow == FLOW_DEAD ) {
      test->flow = FLOW_DEAD;
   }
   s_pop_scope( semantic );
}

static void test_switch_cond( struct semantic* semantic,
   struct stmt_test* test, struct switch_stmt* stmt ) {
   test_heavy_cond( semantic, test, &stmt->cond );
   // Only condition of a primitive type is supported.
   if ( ! s_is_primitive_type( &test->cond_type ) ) {
      s_diag( semantic, DIAG_POS_ERR, stmt->cond.expr ?
         &stmt->cond.expr->pos : &stmt->cond.var->object.pos,
         "condition of switch statement not of primitive type" );
      s_bail( semantic );
   }
}

// Just check any of the variables appearing before the first case for now.
static void warn_switch_skipped_init( struct semantic* semantic,
   struct block* block ) {
   struct var* last_var = NULL;
   struct list_iter i;
   list_iterate( &block->stmts, &i );
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

static void test_while( struct semantic* semantic, struct stmt_test* test,
   struct while_stmt* stmt ) {
   s_add_scope( semantic, false );
   test->in_loop = true;
   test_cond( semantic, &stmt->cond );
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   test_block( semantic, &body, NULL, stmt->body );
   stmt->jump_break = test->jump_break;
   stmt->jump_continue = test->jump_continue;
   // Flow.
   if ( stmt->cond.u.node->type == NODE_EXPR && stmt->cond.u.expr->folded ) {
      if ( ( ! stmt->until && stmt->cond.u.expr->value != 0 ) ||
         ( stmt->until && stmt->cond.u.expr->value == 0 ) ) {
         if ( body.flow != FLOW_BREAKING ) {
            test->flow = FLOW_DEAD;
         }
      }
   }
   s_pop_scope( semantic );
}

static void test_do( struct semantic* semantic, struct stmt_test* test,
   struct do_stmt* stmt ) {
   s_add_scope( semantic, false );
   test->in_loop = true;
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   test_block( semantic, &body, NULL, stmt->body );
   stmt->jump_break = test->jump_break;
   stmt->jump_continue = test->jump_continue;
   s_test_bool_expr( semantic, stmt->cond );
   // Flow.
   if ( stmt->cond->folded ) {
      if ( ( ! stmt->until && stmt->cond->value != 0 ) ||
         ( stmt->until && stmt->cond->value == 0 ) ) {
         if ( body.flow != FLOW_BREAKING ) {
            test->flow = FLOW_DEAD;
         }
      }
      else {
         if ( body.flow == FLOW_DEAD ) {
            test->flow = FLOW_DEAD;
         }
      }
   }
   else {
      if ( body.flow == FLOW_DEAD ) {
         test->flow = FLOW_DEAD;
      }
   }
   s_pop_scope( semantic );
}

static void test_for( struct semantic* semantic, struct stmt_test* test,
   struct for_stmt* stmt ) {
   test->in_loop = true;
   s_add_scope( semantic, false );
   // Initialization.
   struct list_iter i;
   list_iterate( &stmt->init, &i );
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
      else if (
         node->type == NODE_STRUCTURE ||
         node->type == NODE_ENUMERATION ) {
         struct object* object = ( struct object* ) node;
         s_diag( semantic, DIAG_POS_ERR, &object->pos,
            "%s declared in for-loop initialization",
            node->type == NODE_STRUCTURE ? "struct" : "enum" );
         s_bail( semantic );
      }
      else {
         S_UNREACHABLE( semantic );
      }
      list_next( &i );
   }
   // Condition.
   if ( stmt->cond.u.node ) {
      test_cond( semantic, &stmt->cond );
   }
   // Post expressions.
   list_iterate( &stmt->post, &i );
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
   // Flow.
   if ( ! stmt->cond.u.node || ( stmt->cond.u.node->type == NODE_EXPR &&
      stmt->cond.u.expr->value != 0 ) ) {
      if ( body.flow != FLOW_BREAKING ) {
         test->flow = FLOW_DEAD;
      }
   }
   s_pop_scope( semantic );
}

static void test_foreach( struct semantic* semantic, struct stmt_test* test,
   struct foreach_stmt* stmt ) {
   test->in_loop = true;
   s_add_scope( semantic, false );
   struct var* key = stmt->key;
   struct var* value = stmt->value;
   // Collection.
   struct expr_test expr;
   s_init_expr_test( &expr, false, false );
   s_test_expr( semantic, &expr, stmt->collection );
   struct type_iter iter;
   s_iterate_type( semantic, &expr.type, &iter );
   if ( ! iter.available ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->collection->pos,
         "expression not of iterable type" );
      s_bail( semantic );
   }
   // Note which variable is passed by reference.
   if ( s_is_ref_type( &iter.value ) ) {
      if ( expr.var ) {
         if ( ! expr.var->hidden ) {
            s_diag( semantic, DIAG_POS_ERR, &stmt->collection->pos,
               "non-private collection (references only work with private map "
               "variables)" );
            s_bail( semantic );
         }
         expr.var->addr_taken = true;
      }
      if ( expr.structure_member ) {
         expr.structure_member->addr_taken = true;
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

static void test_jump( struct semantic* semantic, struct stmt_test* test,
   struct jump* stmt ) {
   switch ( stmt->type ) {
   case JUMP_BREAK:
      test_break( semantic, test, stmt );
      break;
   case JUMP_CONTINUE:
      test_continue( semantic, test, stmt );
   }
}

static void test_break( struct semantic* semantic, struct stmt_test* test,
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
   if ( in_msgbuild_block_range( test, target ) ) {
      diag_early_leave( semantic, &stmt->pos );
      s_bail( semantic );
   }
   stmt->next = target->jump_break;
   target->jump_break = stmt;
   test->flow = FLOW_BREAKING;
}

static void diag_leave( struct semantic* semantic, struct pos* pos,
   bool entering ) {
   s_diag( semantic, DIAG_POS_ERR, pos,
      "%s a message-building block %s",
      entering ? "entering" : "leaving",
      entering ? "late" : "early" );
   s_diag( semantic, DIAG_FILE | DIAG_NOTE, pos,
      "a message-building block must run from start to finish" );
}

static void diag_early_leave( struct semantic* semantic, struct pos* pos ) {
   diag_leave( semantic, pos, false );
}

static void test_continue( struct semantic* semantic, struct stmt_test* test,
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
   if ( in_msgbuild_block_range( test, target ) ) {
      diag_early_leave( semantic, &stmt->pos );
      s_bail( semantic );
   }
   stmt->next = target->jump_continue;
   target->jump_continue = stmt;
   test->flow = FLOW_DEAD;
}

static void test_script_jump( struct semantic* semantic,
   struct stmt_test* test, struct script_jump* stmt ) {
   static const char* names[] = { "terminate", "restart", "suspend" };
   STATIC_ASSERT( ARRAY_SIZE( names ) == SCRIPT_JUMP_TOTAL );
   if ( ! semantic->func_test->script ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->pos,
         "%s statement outside script", names[ stmt->type ] );
      s_bail( semantic );
   }
   if ( s_in_msgbuild_block( semantic ) ) {
      diag_early_leave( semantic, &stmt->pos );
      s_bail( semantic );
   }
}

static void test_return( struct semantic* semantic, struct stmt_test* test,
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
   if ( s_in_msgbuild_block( semantic ) ) {
      diag_early_leave( semantic, &stmt->pos );
      s_bail( semantic );
   }
   stmt->next = semantic->func_test->returns;
   semantic->func_test->returns = stmt;
   test->flow = FLOW_DEAD;
}

static void test_return_value( struct semantic* semantic,
   struct stmt_test* test, struct return_stmt* stmt ) {
   struct func* func = semantic->func_test->func;
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   expr.buildmsg = stmt->buildmsg;
   s_test_expr( semantic, &expr, stmt->return_value );
   if ( stmt->buildmsg ) {
      test_buildmsg_block( semantic, test, stmt->buildmsg );
   }
   if ( func->return_spec == SPEC_VOID && ! func->ref ) {
      s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->pos,
         "returning a value from void function" );
      s_bail( semantic );
   }
   // Return value must be of the same type as the return type.
   if ( func->return_spec == SPEC_AUTOENUM ) {
      if ( ! s_is_enumerator( &expr.type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->pos,
            "return value not an enumerator" );
         s_bail( semantic );
      }
      func->enumeration = expr.type.enumeration;
      func->return_spec = SPEC_ENUM;
   }
   else if ( func->return_spec == SPEC_AUTO ) {
      if ( s_is_null( &expr.type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->pos,
            "cannot deduce return type from `null`" );
         s_bail( semantic );
      }
      if ( s_describe_type( &expr.type ) == TYPEDESC_PRIMITIVE ) {
         func->return_spec = expr.type.spec;
      }
      else {
         struct type_snapshot snapshot;
         s_take_type_snapshot( &expr.type, &snapshot );
         func->ref = snapshot.ref;
         func->enumeration = snapshot.enumeration;
         func->structure = snapshot.structure;
         func->return_spec = snapshot.spec;
      }
   }
   else {
      struct type_info return_type;
      s_init_type_info( &return_type, func->ref, func->structure,
         func->enumeration, NULL, func->return_spec, STORAGE_LOCAL );
      if ( ! s_instance_of( &return_type, &expr.type ) ) {
         s_type_mismatch( semantic, "return-value", &expr.type,
            "function-return", &return_type, &stmt->return_value->pos );
         s_bail( semantic );
      }
   }
   if ( expr.func && expr.func->type == FUNC_USER ) {
      struct func_user* impl = expr.func->impl;
      if ( impl->local ) {
         s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->pos,
            "returning local function" );
         s_bail( semantic );
      }
   }
   // Note which variable is passed by reference.
   if ( s_is_ref_type( &expr.type ) ) {
      if ( expr.var ) {
         if ( ! expr.var->hidden ) {
            s_diag( semantic, DIAG_POS_ERR, &stmt->return_value->pos,
               "non-private return-value (references only work with private map "
               "variables)" );
            s_bail( semantic );
         }
         expr.var->addr_taken = true;
      }
      if ( expr.structure_member ) {
         expr.structure_member->addr_taken = true;
      }
   }
}

static void test_goto( struct semantic* semantic, struct stmt_test* test,
   struct goto_stmt* stmt ) {
   stmt->buildmsg = semantic->func_test->enclosing_buildmsg;
   struct list_iter i;
   list_iterate( semantic->func_test->labels, &i );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      if ( strcmp( label->name, stmt->label_name ) == 0 ) {
         stmt->label = label;
         list_append( &label->users, stmt ); 
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

static void test_label( struct semantic* semantic, struct stmt_test* test,
   struct label* label ) {
   label->buildmsg = semantic->func_test->enclosing_buildmsg;
}

static void test_paltrans( struct semantic* semantic, struct stmt_test* test,
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
      else if ( range->colorisation ) {
         test_palrange_colorisation( semantic, range );
      }
      else if ( range->tint ) {
         test_palrange_tint( semantic, range );
      }
      else {
         test_paltrans_arg( semantic, range->value.ent.begin, false );
         test_paltrans_arg( semantic, range->value.ent.end, false );
      }
      range = range->next;
   }
}

static void test_paltrans_arg( struct semantic* semantic, struct expr* arg,
   bool require_fixed_type ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, arg );
   struct type_info required_type;
   s_init_type_info_scalar( &required_type, s_spec( semantic,
      require_fixed_type ? SPEC_FIXED : SPEC_INT ) );
   if ( ! s_instance_of( &required_type, &expr.type ) ) {
      s_type_mismatch( semantic, "argument", &expr.type,
         "required", &required_type, &arg->pos );
      s_bail( semantic );
   }
}

static void test_palrange_colorisation( struct semantic* semantic,
   struct palrange* range ) {
   test_paltrans_arg( semantic, range->value.colorisation.red, false );
   test_paltrans_arg( semantic, range->value.colorisation.green, false );
   test_paltrans_arg( semantic, range->value.colorisation.blue, false );
}

static void test_palrange_tint( struct semantic* semantic,
   struct palrange* range ) {
   test_paltrans_arg( semantic, range->value.tint.amount, false );
   test_paltrans_arg( semantic, range->value.tint.red, false );
   test_paltrans_arg( semantic, range->value.tint.green, false );
   test_paltrans_arg( semantic, range->value.tint.blue, false );
}

static void test_buildmsg_stmt( struct semantic* semantic,
   struct stmt_test* test, struct buildmsg_stmt* stmt ) {
   test->buildmsg = stmt->buildmsg;
   struct expr_test expr_test;
   s_init_expr_test( &expr_test, false, false );
   expr_test.buildmsg = stmt->buildmsg;
   s_test_expr( semantic, &expr_test, stmt->buildmsg->expr );
   test_buildmsg_block( semantic, test, stmt->buildmsg );
}

static void test_buildmsg_block( struct semantic* semantic,
   struct stmt_test* test, struct buildmsg* buildmsg ) {
   struct buildmsg* prev = semantic->func_test->enclosing_buildmsg;
   semantic->func_test->enclosing_buildmsg = buildmsg;
   struct stmt_test block_test;
   s_init_stmt_test( &block_test, test );
   block_test.buildmsg = buildmsg;
   struct builtin_aliases aliases;
   init_builtin_aliases( semantic, &aliases, BUILTINALIASESUSER_BUILDMSG );
   test_block( semantic, &block_test, &aliases, buildmsg->block );
   if ( ! list_size( &buildmsg->usages ) ) {
      s_diag( semantic, DIAG_POS_ERR, &buildmsg->block->pos,
         "unused message-building block" );
      s_bail( semantic );
   }
   semantic->func_test->enclosing_buildmsg = prev;
}

bool s_in_msgbuild_block( struct semantic* semantic ) {
   return ( semantic->func_test->enclosing_buildmsg != NULL );
}

static bool in_msgbuild_block_range( struct stmt_test* start,
   struct stmt_test* end ) {
   return ( find_buildmsg( start, end ) != NULL );
}

static struct buildmsg* find_buildmsg( struct stmt_test* start,
   struct stmt_test* end ) {
   struct stmt_test* test = start;
   while ( test != end ) {
      if ( test->buildmsg ) {
         return test->buildmsg;
      }
      test = test->parent;
   }
   return NULL;
}

static void test_expr_stmt( struct semantic* semantic,
   struct expr_stmt* stmt ) {
   struct list_iter i;
   list_iterate( &stmt->expr_list, &i );
   bool require_assign = ( list_size( &stmt->expr_list ) > 1 );
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

static void check_dup_label( struct semantic* semantic ) {
   struct list_iter i;
   list_iterate( semantic->func_test->labels, &i );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      list_next( &i );
      struct list_iter k = i;
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

static void test_goto_in_msgbuild_block( struct semantic* semantic ) {
   struct list_iter i;
   list_iterate( semantic->func_test->labels, &i );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      struct list_iter k;
      list_iterate( &label->users, &k );
      while ( ! list_end( &k ) ) {
         struct goto_stmt* stmt = list_data( &k );
         if ( stmt->buildmsg != label->buildmsg ) {
            diag_leave( semantic, &stmt->pos, ( stmt->buildmsg == NULL ) );
            s_bail( semantic );
         }
         list_next( &k );
      }
      list_next( &i );
   }
}
