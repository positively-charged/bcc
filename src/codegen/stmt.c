#include "phase.h"
#include "pcode.h"

static void init_local_record( struct codegen* codegen,
   struct local_record* record );
static void push_local_record( struct codegen* codegen,
   struct local_record* record );
static void pop_local_record( struct codegen* codegen );
static void write_block_item( struct codegen* codegen, struct node* node );
static void write_assert( struct codegen* codegen, struct assert* assert );
static void write_runtime_assert( struct codegen* codegen,
   struct assert* assert );
static void visit_if( struct codegen* codegen, struct if_stmt* );
static void push_if_cond( struct codegen* codegen, struct heavy_cond* cond );
static void visit_switch( struct codegen* codegen, struct switch_stmt* );
static void write_switch_casegoto( struct codegen* codegen,
   struct switch_stmt* stmt );
static bool string_switch( struct switch_stmt* stmt );
static void write_switch( struct codegen* codegen, struct switch_stmt* stmt );
static void write_switch_cond( struct codegen* codegen,
   struct switch_stmt* stmt );
static void write_string_switch( struct codegen* codegen,
   struct switch_stmt* stmt );
static void visit_case( struct codegen* codegen, struct case_label* );
static void visit_while( struct codegen* codegen, struct while_stmt* );
static void write_folded_while( struct codegen* codegen,
   struct while_stmt* stmt );
static void write_while( struct codegen* codegen, struct while_stmt* stmt );
static void push_cond( struct codegen* codegen, struct cond* cond );
static void visit_do( struct codegen* codegen, struct do_stmt* stmt );
static void write_folded_do( struct codegen* codegen,
   struct do_stmt* stmt );
static void write_do( struct codegen* codegen, struct do_stmt* stmt );
static void visit_for( struct codegen* codegen, struct for_stmt* );
static void visit_foreach( struct codegen* codegen,
   struct foreach_stmt* stmt );
static void init_foreach_writing( struct foreach_writing* writing );
static void foreach_array( struct codegen* codegen,
   struct foreach_writing* writing, struct foreach_stmt* stmt );
static void foreach_ref_array( struct codegen* codegen,
   struct foreach_writing* writing, struct foreach_stmt* stmt );
static void foreach_str( struct codegen* codegen,
   struct foreach_writing* writing, struct foreach_stmt* stmt );
static void visit_jump( struct codegen* codegen, struct jump* );
static void set_jumps_point( struct codegen* codegen, struct jump* jump,
   struct c_point* point );
static void visit_return( struct codegen* codegen, struct return_stmt* );
static void visit_paltrans( struct codegen* codegen, struct paltrans* );
static void write_palrange_colorisation( struct codegen* codegen,
   struct palrange* range );
static void write_palrange_tint( struct codegen* codegen,
   struct palrange* range );
static void visit_script_jump( struct codegen* codegen, struct script_jump* );
static void visit_label( struct codegen* codegen, struct label* );
static void visit_goto( struct codegen* codegen, struct goto_stmt* );
static void visit_buildmsg_stmt( struct codegen* codegen,
   struct buildmsg_stmt* stmt );
static void write_msgbuild_block( struct codegen* codegen,
   struct buildmsg* buildmsg );
static void write_multi_usage_msgbuild_block( struct codegen* codegen,
   struct buildmsg* buildmsg );
static void visit_expr_stmt( struct codegen* codegen, struct expr_stmt* stmt );

void c_write_block( struct codegen* codegen, struct block* stmt ) {
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   struct list_iter i;
   list_iterate( &stmt->stmts, &i );
   while ( ! list_end( &i ) ) {
      write_block_item( codegen, list_data( &i ) );
      list_next( &i );
   }
   pop_local_record( codegen );
}

static void init_local_record( struct codegen* codegen,
   struct local_record* record ) {
   record->parent = NULL;
   if ( codegen->local_record ) {
      record->index = codegen->local_record->index;
      record->func_size = codegen->local_record->func_size;
   }
   else {
      record->index = codegen->func->start_index;
      record->func_size = codegen->func->size;
   }
}

static void push_local_record( struct codegen* codegen,
   struct local_record* record ) {
   record->parent = codegen->local_record;
   codegen->local_record = record;
}

static void pop_local_record( struct codegen* codegen ) {
   codegen->local_record = codegen->local_record->parent;
}

static void write_block_item( struct codegen* codegen, struct node* node ) {
   switch ( node->type ) {
   case NODE_VAR:
      c_visit_var( codegen, ( struct var* ) node );
      break;
   case NODE_CASE:
   case NODE_CASE_DEFAULT:
      visit_case( codegen, ( struct case_label* ) node );
      break;
   case NODE_GOTO_LABEL:
      visit_label( codegen, ( struct label* ) node );
      break;
   case NODE_ASSERT:
      write_assert( codegen,
         ( struct assert* ) node );
      break;
   case NODE_CONSTANT:
   case NODE_ENUMERATION:
   case NODE_TYPE_ALIAS:
   case NODE_FUNC:
      break;
   default:
      c_write_stmt( codegen, node );
      break;
   }
}

static void write_assert( struct codegen* codegen, struct assert* assert ) {
   if ( ! assert->is_static && codegen->task->options->write_asserts ) {
      write_runtime_assert( codegen, assert );
   }
}

static void write_runtime_assert( struct codegen* codegen,
   struct assert* assert ) {
   c_push_bool_expr( codegen, assert->cond );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &exit_jump->node );
   c_pcd( codegen, PCD_BEGINPRINT );
   // Make the message red.
   #define MSG_COLOR "\\cg"
   struct indexed_string* string = t_intern_string( codegen->task,
      MSG_COLOR, sizeof( MSG_COLOR ) );
   string->used = true;
   c_append_string( codegen, string );
   c_pcd( codegen, PCD_PUSHNUMBER, string->index_runtime );
   c_pcd( codegen, PCD_PRINTSTRING );
   #undef RED
   // Print file.
   c_push_string( codegen, assert->file );
   c_pcd( codegen, PCD_PRINTSTRING );
   // Push line/column characters.
   c_pcd( codegen, PCD_PUSHNUMBER, ' ' );
   c_pcd( codegen, PCD_PUSHNUMBER, ':' );
   c_pcd( codegen, PCD_PUSHNUMBER, assert->pos.column );
   c_pcd( codegen, PCD_PUSHNUMBER, ':' );
   c_pcd( codegen, PCD_PUSHNUMBER, assert->pos.line );
   c_pcd( codegen, PCD_PUSHNUMBER, ':' );
   // Print line/column.
   c_pcd( codegen, PCD_PRINTCHARACTER );
   c_pcd( codegen, PCD_PRINTNUMBER );
   c_pcd( codegen, PCD_PRINTCHARACTER );
   c_pcd( codegen, PCD_PRINTNUMBER );
   c_pcd( codegen, PCD_PRINTCHARACTER );
   c_pcd( codegen, PCD_PRINTCHARACTER );
   // Print standard message prefix.
   c_push_string( codegen, codegen->assert_prefix );
   c_pcd( codegen, PCD_PRINTSTRING );
   // Print message.
   if ( assert->message ) {
      c_pcd( codegen, PCD_PUSHNUMBER, ' ' );
      c_pcd( codegen, PCD_PUSHNUMBER, ':' );
      c_pcd( codegen, PCD_PRINTCHARACTER );
      c_pcd( codegen, PCD_PRINTCHARACTER );
      c_push_expr( codegen, assert->message );
      c_pcd( codegen, PCD_PRINTSTRING );
   }
   c_pcd( codegen, PCD_ENDLOG );
   c_pcd( codegen, PCD_TERMINATE );
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
}

void c_write_stmt( struct codegen* codegen, struct node* node ) {
   switch ( node->type ) {
   case NODE_BLOCK:
      c_write_block( codegen, ( struct block* ) node );
      break;
   case NODE_IF:
      visit_if( codegen, ( struct if_stmt* ) node );
      break;
   case NODE_SWITCH:
      visit_switch( codegen, ( struct switch_stmt* ) node );
      break;
   case NODE_WHILE:
      visit_while( codegen, ( struct while_stmt* ) node );
      break;
   case NODE_DO:
      visit_do( codegen,
         ( struct do_stmt* ) node );
      break;
   case NODE_FOR:
      visit_for( codegen, ( struct for_stmt* ) node );
      break;
   case NODE_FOREACH:
      visit_foreach( codegen,
         ( struct foreach_stmt* ) node );
      break;
   case NODE_JUMP:
      visit_jump( codegen, ( struct jump* ) node );
      break;
   case NODE_SCRIPT_JUMP:
      visit_script_jump( codegen, ( struct script_jump* ) node );
      break;
   case NODE_RETURN:
      visit_return( codegen, ( struct return_stmt* ) node );
      break;
   case NODE_GOTO:
      visit_goto( codegen, ( struct goto_stmt* ) node );
      break;
   case NODE_PALTRANS:
      visit_paltrans( codegen, ( struct paltrans* ) node );
      break;
   case NODE_BUILDMSG:
      visit_buildmsg_stmt( codegen,
         ( struct buildmsg_stmt* ) node );
      break;
   case NODE_EXPR_STMT:
      visit_expr_stmt( codegen,
         ( struct expr_stmt* ) node );
      break;
   case NODE_INLINE_ASM:
      p_visit_inline_asm( codegen,
         ( struct inline_asm* ) node );
      break;
   case NODE_STRUCTURE:
   case NODE_USING:
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void visit_if( struct codegen* codegen, struct if_stmt* stmt ) {
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   push_if_cond( codegen, &stmt->cond );
   struct c_jump* else_jump = c_create_jump( codegen, PCD_IFNOTGOTO );
   struct c_jump* exit_jump = else_jump;
   c_append_node( codegen, &else_jump->node );
   c_write_stmt( codegen, stmt->body );
   if ( stmt->else_body ) {
      exit_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &exit_jump->node );
      struct c_point* else_point = c_create_point( codegen );
      c_append_node( codegen, &else_point->node );
      else_jump->point = else_point;
      c_write_stmt( codegen, stmt->else_body );
   }
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
   pop_local_record( codegen );
}

static void push_if_cond( struct codegen* codegen, struct heavy_cond* cond ) {
   if ( cond->var ) {
      c_visit_var( codegen, cond->var );
      if ( cond->expr ) {
         c_push_bool_expr( codegen, cond->expr );
      }
      else {
         c_push_bool_cond_var( codegen, cond->var );
      }
   }
   else {
      c_push_bool_expr( codegen, cond->expr );
   }
}

static void visit_switch( struct codegen* codegen, struct switch_stmt* stmt ) {
   switch ( codegen->lang ) {
   case LANG_ACS:
      write_switch( codegen, stmt );
      break;
   case LANG_ACS95:
      write_switch_casegoto( codegen, stmt );
      break;
   case LANG_BCS:
      if ( string_switch( stmt ) ) {
         write_string_switch( codegen, stmt );
      }
      else {
         write_switch( codegen, stmt );
      }
   }
}

static void write_switch_casegoto( struct codegen* codegen,
   struct switch_stmt* stmt ) {
   struct c_point* exit_point = c_create_point( codegen );
   // Condition.
   c_push_expr( codegen, stmt->cond.expr );
   // Case selection.
   bool zero_value_case = false;
   struct case_label* label = stmt->case_head;
   while ( label ) {
      label->point = c_create_point( codegen );
      struct c_casejump* jump = c_create_casejump( codegen,
         label->number->value, label->point );
      c_append_node( codegen, &jump->node );
      if ( label->number->value == 0 ) {
         zero_value_case = true;
      }
      label = label->next;
   }
   // Default case.
   struct c_point* default_point = exit_point;
   if ( stmt->case_default ) {
      default_point = c_create_point( codegen );
      stmt->case_default->point = default_point;
   }
   // Optimization: instead of using PCD_DROP and PCD_GOTO to jump to the
   // default point, use a single instruction to eat up the value and jump.
   if ( zero_value_case ) {
      struct c_jump* default_jump = c_create_jump( codegen, PCD_IFGOTO );
      c_append_node( codegen, &default_jump->node );
      default_jump->point = default_point;
   }
   else {
      c_pcd( codegen, PCD_DROP );
      struct c_jump* default_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &default_jump->node );
      default_jump->point = default_point;
   }
   // Body.
   c_write_stmt( codegen, stmt->body );
   c_append_node( codegen, &exit_point->node );
   set_jumps_point( codegen, stmt->jump_break, exit_point );
}

inline static bool string_switch( struct switch_stmt* stmt ) {
   return ( stmt->cond.expr ?
      stmt->cond.expr->spec == SPEC_STR :
      stmt->cond.var->spec == SPEC_STR );
}

static void write_switch( struct codegen* codegen, struct switch_stmt* stmt ) {
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   struct c_point* exit_point = c_create_point( codegen );
   // Case selection.
   write_switch_cond( codegen, stmt );
   struct c_sortedcasejump* sorted_jump = c_create_sortedcasejump( codegen );
   c_append_node( codegen, &sorted_jump->node );
   struct case_label* label = stmt->case_head;
   while ( label ) {
      label->point = c_create_point( codegen );
      struct c_casejump* jump = c_create_casejump( codegen,
         label->number->value, label->point );
      c_append_casejump( sorted_jump, jump );
      label = label->next;
   }
   c_pcd( codegen, PCD_DROP );
   struct c_point* default_point = exit_point;
   if ( stmt->case_default ) {
      default_point = c_create_point( codegen );
      stmt->case_default->point = default_point;
   }
   struct c_jump* default_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &default_jump->node );
   default_jump->point = default_point;
   // Body.
   c_write_stmt( codegen, stmt->body );
   c_append_node( codegen, &exit_point->node );
   set_jumps_point( codegen, stmt->jump_break, exit_point );
   pop_local_record( codegen );
}

static void write_switch_cond( struct codegen* codegen,
   struct switch_stmt* stmt ) {
   if ( stmt->cond.var ) {
      c_visit_var( codegen, stmt->cond.var );
      if ( stmt->cond.expr ) {
         c_push_expr( codegen, stmt->cond.expr );
      }
      else {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, stmt->cond.var->index );
      }
   }
   else {
      c_push_expr( codegen, stmt->cond.expr );
   }
}

// NOTE: Right now, the implementation does a linear search when attempting to
// find the correct case. A binary search might be possible. 
static void write_string_switch( struct codegen* codegen,
   struct switch_stmt* stmt ) {
   struct c_point* exit_point = c_create_point( codegen );
   // Case selection.
   write_switch_cond( codegen, stmt );
   struct case_label* label = stmt->case_head;
   while ( label ) {
      if ( label->next ) {
         c_pcd( codegen, PCD_DUP );
      }
      c_push_string( codegen, t_lookup_string( codegen->task,
         label->number->value ) );
      c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_STRCMP );
      struct c_jump* next_jump = c_create_jump( codegen, PCD_IFGOTO );
      c_append_node( codegen, &next_jump->node );
      if ( label->next ) {
         // Match.
         c_pcd( codegen, PCD_DROP );
         struct c_jump* jump = c_create_jump( codegen, PCD_GOTO );
         c_append_node( codegen, &jump->node );
         label->point = c_create_point( codegen );
         jump->point = label->point;
         // Jump to next case.
         struct c_point* next_point = c_create_point( codegen );
         c_append_node( codegen, &next_point->node );
         next_jump->point = next_point;
      }
      else {
         // On the last case, there is no need to drop the duplicated condition
         // string because it's not duplicated, so just go directly to the case
         // if a match is made.
         next_jump->opcode = PCD_IFNOTGOTO;
         label->point = c_create_point( codegen );
         next_jump->point = label->point;
      }
      label = label->next;
   }
   // The last case eats up the condition string. If no cases are present, the
   // string needs to be manually dropped.
   if ( ! stmt->case_head ) {
      c_pcd( codegen, PCD_DROP );
   }
   struct c_point* default_point = exit_point;
   if ( stmt->case_default ) {
      default_point = c_create_point( codegen );
      stmt->case_default->point = default_point;
   }
   struct c_jump* default_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &default_jump->node );
   default_jump->point = default_point;
   // Body.
   c_write_stmt( codegen, stmt->body );
   c_append_node( codegen, &exit_point->node );
   set_jumps_point( codegen, stmt->jump_break, exit_point );
}

static void visit_case( struct codegen* codegen, struct case_label* label ) {
   c_append_node( codegen, &label->point->node );
}

static void visit_while( struct codegen* codegen, struct while_stmt* stmt ) {
   if ( stmt->cond.u.node->type == NODE_EXPR && stmt->cond.u.expr->folded ) {
      write_folded_while( codegen, stmt );
   }
   else {
      write_while( codegen, stmt );
   }
}

static void write_folded_while( struct codegen* codegen,
   struct while_stmt* stmt ) {
   bool true_cond = ( ! stmt->until && stmt->cond.u.expr->value != 0 ) ||
      ( stmt->until && stmt->cond.u.expr->value == 0 );
   // A loop with a constant false condition never executes, so jump to the
   // exit right away. Nonetheless, the loop is still written because it can be
   // entered with a goto-statement.
   struct c_jump* exit_jump = NULL;
   if ( ! true_cond ) {
      exit_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &exit_jump->node );
   }
   // Body.
   struct c_point* body_point = c_create_point( codegen );
   c_append_node( codegen, &body_point->node );
   c_write_block( codegen, stmt->body );
   // Jump to top of body. With a false condition, the loop will never
   // execute and so this jump is not needed.
   if ( true_cond ) {
      struct c_jump* body_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &body_jump->node );
      body_jump->point = body_point;
   }
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   if ( ! true_cond ) {
      exit_jump->point = exit_point;
   }
   set_jumps_point( codegen, stmt->jump_break, exit_point );
   set_jumps_point( codegen, stmt->jump_continue,
      ( true_cond ) ? body_point : exit_point );
}

static void write_while( struct codegen* codegen, struct while_stmt* stmt ) {
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   struct c_jump* cond_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &cond_jump->node );
   // Body.
   struct c_point* body_point = c_create_point( codegen );
   c_append_node( codegen, &body_point->node );
   c_write_block( codegen, stmt->body );
   // Condition.
   struct c_point* cond_point = c_create_point( codegen );
   c_append_node( codegen, &cond_point->node );
   cond_jump->point = cond_point;
   push_cond( codegen, &stmt->cond );
   struct c_jump* body_jump = c_create_jump( codegen,
      ( stmt->until ? PCD_IFNOTGOTO : PCD_IFGOTO ) );
   c_append_node( codegen, &body_jump->node );
   body_jump->point = body_point;
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   set_jumps_point( codegen, stmt->jump_break, exit_point );
   set_jumps_point( codegen, stmt->jump_continue, cond_point );
   pop_local_record( codegen );
}

static void push_cond( struct codegen* codegen, struct cond* cond ) {
   if ( cond->u.node->type == NODE_VAR ) {
      c_visit_var( codegen, cond->u.var );
      c_push_bool_cond_var( codegen, cond->u.var );
   }
   else {
      c_push_bool_expr( codegen, cond->u.expr );
   }
}

static void visit_do( struct codegen* codegen, struct do_stmt* stmt ) {
   if ( stmt->cond->folded ) {
      write_folded_do( codegen, stmt );
   }
   else {
      write_do( codegen, stmt );
   }
}

static void write_folded_do( struct codegen* codegen, struct do_stmt* stmt ) {
   bool true_cond = ( ! stmt->until && stmt->cond->value != 0 ) ||
      ( stmt->until && stmt->cond->value == 0 );
   // A loop with a constant false condition never executes, so jump to the
   // exit right away. Nonetheless, the loop is still written because it can be
   // entered with a goto-statement.
   struct c_jump* exit_jump = NULL;
   if ( ! true_cond ) {
      exit_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &exit_jump->node );
   }
   // Body.
   struct c_point* body_point = c_create_point( codegen );
   c_append_node( codegen, &body_point->node );
   c_write_block( codegen, stmt->body );
   // Jump to top of body. With a false condition, the loop will never
   // execute and so this jump is not needed.
   if ( true_cond ) {
      struct c_jump* body_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &body_jump->node );
      body_jump->point = body_point;
   }
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   if ( ! true_cond ) {
      exit_jump->point = exit_point;
   }
   set_jumps_point( codegen, stmt->jump_break, exit_point );
   set_jumps_point( codegen, stmt->jump_continue,
      ( true_cond ) ? body_point : exit_point );
}

static void write_do( struct codegen* codegen, struct do_stmt* stmt ) {
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   // Body.
   struct c_point* body_point = c_create_point( codegen );
   c_append_node( codegen, &body_point->node );
   c_write_block( codegen, stmt->body );
   // Condition.
   struct c_point* cond_point = c_create_point( codegen );
   c_append_node( codegen, &cond_point->node );
   c_push_bool_expr( codegen, stmt->cond );
   struct c_jump* body_jump = c_create_jump( codegen,
      ( stmt->until ? PCD_IFNOTGOTO : PCD_IFGOTO ) );
   c_append_node( codegen, &body_jump->node );
   body_jump->point = body_point;
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   set_jumps_point( codegen, stmt->jump_break, exit_point );
   set_jumps_point( codegen, stmt->jump_continue, cond_point );
   pop_local_record( codegen );
}

// for-loop layout:
// <initialization>
// <goto-condition>
// <body>
// <post-expression-list>
// <condition>
//   if 1: <goto-body>
//
// for-loop layout (constant-condition, 1):
// <initialization>
// <body>
// <post-expression-list>
// <goto-body>
//
// for-loop layout (constant-condition, 0):
// <initialization>
// <goto-done>
// <body>
// <post-expression-list>
// <done>
static void visit_for( struct codegen* codegen, struct for_stmt* stmt ) {
   // Initialization.
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   struct list_iter i;
   list_iterate( &stmt->init, &i );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      switch ( node->type ) {
      case NODE_EXPR:
         c_visit_expr( codegen,
            ( struct expr* ) node );
         break;
      case NODE_VAR:
         c_visit_var( codegen,
            ( struct var* ) node );
         break;
      default:
         break;
      }
      list_next( &i );
   }
   // Only test a condition that isn't both constant and true.
   bool test_cond = false;
   if ( stmt->cond.u.node ) {
      if ( stmt->cond.u.node->type == NODE_VAR ) {
         test_cond = true;
      }
      else {
         test_cond = ( ! ( stmt->cond.u.expr->folded &&
            stmt->cond.u.expr->value != 0 ) );
      }
   }
   // Jump to condition.
   struct c_jump* cond_jump = NULL;
   if ( test_cond ) {
      cond_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &cond_jump->node );
   }
   // Body.
   struct c_point* body_point = c_create_point( codegen );
   c_append_node( codegen, &body_point->node );
   c_write_stmt( codegen, stmt->body );
   // Post expressions.
   struct c_point* post_point = NULL;
   if ( list_size( &stmt->post ) ) {
      post_point = c_create_point( codegen );
      c_append_node( codegen, &post_point->node );
      list_iterate( &stmt->post, &i );
      while ( ! list_end( &i ) ) {
         struct node* node = list_data( &i );
         if ( node->type == NODE_EXPR ) {
            c_visit_expr( codegen, ( struct expr* ) node );
         }
         list_next( &i );
      }
   }
   // Condition.
   struct c_point* cond_point = NULL;
   if ( test_cond ) {
      cond_point = c_create_point( codegen );
      c_append_node( codegen, &cond_point->node );
      push_cond( codegen, &stmt->cond );
      cond_jump->point = cond_point;
   }
   // Jump to body.
   struct c_jump* body_jump = c_create_jump( codegen,
      ( test_cond ) ? PCD_IFGOTO : PCD_GOTO );
   c_append_node( codegen, &body_jump->node );
   body_jump->point = body_point;
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   // Patch jump statements.
   set_jumps_point( codegen, stmt->jump_break, exit_point );
   set_jumps_point( codegen, stmt->jump_continue,
      post_point ? post_point :
      cond_point ? cond_point :
      body_point );
   pop_local_record( codegen );
}

static void visit_foreach( struct codegen* codegen,
   struct foreach_stmt* stmt ) {
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   struct foreach_writing writing;
   init_foreach_writing( &writing );
   c_push_foreach_collection( codegen, &writing, stmt->collection );
   switch ( writing.collection ) {
   case FOREACHCOLLECTION_ARRAY:
      foreach_array( codegen, &writing, stmt );
      break;
   case FOREACHCOLLECTION_ARRAYREF:
      foreach_ref_array( codegen, &writing, stmt );
      break;
   case FOREACHCOLLECTION_STR:
      foreach_str( codegen, &writing, stmt );
      break;
   }
   // Patch jump statements.
   set_jumps_point( codegen, stmt->jump_break, writing.break_point );
   set_jumps_point( codegen, stmt->jump_continue, writing.continue_point );
   pop_local_record( codegen );
}

static void init_foreach_writing( struct foreach_writing* writing ) {
   writing->structure = NULL;
   writing->dim = NULL;
   writing->break_point = NULL;
   writing->continue_point = NULL;
   writing->storage = STORAGE_LOCAL;
   writing->index = 0;
   writing->diminfo = 0;
   writing->collection = FOREACHCOLLECTION_STR;
   writing->item = FOREACHITEM_PRIMITIVE;
   writing->pushed_base = false;
}

static void foreach_array( struct codegen* codegen,
   struct foreach_writing* writing, struct foreach_stmt* stmt ) {
   // Allocate key variable.
   int key = c_alloc_script_var( codegen );
   c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, key );
   if ( stmt->key ) {
      // Allocate another key variable (the user's key variable) if the key
      // variable is intended to be modified.
      if ( stmt->key->modified ) {
         stmt->key->index = c_alloc_script_var( codegen );
      }
      else {
         stmt->key->index = key;
      }
   }
   // Allocate value variable.
   c_visit_var( codegen, stmt->value );
   int value = stmt->value->index;
   // Allocate another value variable (the user's value variable) if the value
   // variable is intended to be modified.
   switch ( writing->item ) {
   case FOREACHITEM_SUBARRAY:
   case FOREACHITEM_STRUCT:
      if ( stmt->value->modified ) {
         c_visit_var( codegen, stmt->value );
      }
      break;
   case FOREACHITEM_ARRAYREF:
   case FOREACHITEM_PRIMITIVE:
      break;
   }
   // When iterating over sub-arrays, the dimension information will be the
   // same for each sub-array, so assign it only once.
   if ( writing->item == FOREACHITEM_SUBARRAY ) {
      c_pcd( codegen, PCD_PUSHNUMBER, writing->diminfo + 1 );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, value + 1 );
   }
   // Allocate offset variable.
   int offset = 0;
   switch ( writing->item ) {
   case FOREACHITEM_SUBARRAY:
   case FOREACHITEM_STRUCT:
      // Combine the offset and value variables into a single variable when
      // possible. When the item is a sub-array or a structure, the value
      // variable will contain a reference, which is the same as the offset.
      offset = value;
      break;
   case FOREACHITEM_ARRAYREF:
      offset = c_alloc_script_var( codegen );
      break;
   case FOREACHITEM_PRIMITIVE:
      // Combine the offset and key variables into a single variable when
      // possible. When the item is of size 1 and the array offset is 0 (base
      // offset not pushed), the key variable can be used as the offset
      // variable.
      if ( ! writing->pushed_base ) {
         offset = key;
      }
      else {
         offset = c_alloc_script_var( codegen );
      }
   }
   // Initialize offset variable with the base offset.
   bool separate_offset_var = ( offset != key );
   if ( separate_offset_var ) {
      if ( ! writing->pushed_base ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      }
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, offset );
   }
   // Beginning of loop.
   struct c_point* start_point = c_create_point( codegen );
   c_append_node( codegen, &start_point->node );
   // Initialize user's key variable.
   if ( stmt->key && stmt->key->index != key ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, key );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->key->index );
   }
   // Initialize user's value variable.
   switch ( writing->item ) {
   case FOREACHITEM_SUBARRAY:
   case FOREACHITEM_STRUCT:
      if ( stmt->value->index != value ) {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, value );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index );
         if ( writing->item == FOREACHITEM_SUBARRAY ) {
            c_pcd( codegen, PCD_PUSHSCRIPTVAR, value + 1 );
            c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index + 1 );
         }
      }
      break;
   case FOREACHITEM_ARRAYREF:
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
      c_push_element( codegen, writing->storage, writing->index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, value );
      c_pcd( codegen, PCD_INCSCRIPTVAR, offset );
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
      c_push_element( codegen, writing->storage, writing->index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, value + 1 );
      break;
   case FOREACHITEM_PRIMITIVE:
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
      c_push_element( codegen, writing->storage, writing->index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, value );
      break;
   }
   // Body.
   c_write_stmt( codegen, stmt->body );
   // Increment.
   struct c_point* increment_point = c_create_point( codegen );
   c_append_node( codegen, &increment_point->node );
   writing->continue_point = increment_point;
   // Increment key variable.
   c_pcd( codegen, PCD_INCSCRIPTVAR, key );
   // Increment offset variable.
   switch ( writing->item ) {
   case FOREACHITEM_SUBARRAY:
   case FOREACHITEM_STRUCT:
      if ( writing->dim->element_size > 1 ) {
         c_pcd( codegen, PCD_PUSHNUMBER, writing->dim->element_size );
         c_pcd( codegen, PCD_ADDSCRIPTVAR, offset );
      }
      else {
         c_pcd( codegen, PCD_INCSCRIPTVAR, offset );
      }
      break;
   case FOREACHITEM_ARRAYREF:
      // The offset variable is already incremented once above to get the
      // dimension information. Increment the offset variable again to get the
      // starting offset of the next element.
      c_pcd( codegen, PCD_INCSCRIPTVAR, offset );
      break;
   case FOREACHITEM_PRIMITIVE:
      if ( separate_offset_var ) {
         c_pcd( codegen, PCD_INCSCRIPTVAR, offset );
      }
      break;
   }
   // Condition.
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, key );
   c_pcd( codegen, PCD_PUSHNUMBER, writing->dim->length );
   c_pcd( codegen, PCD_LT );
   // Jump to start of loop body.
   struct c_jump* start_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &start_jump->node );
   start_jump->point = start_point;
   // Exit.
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   writing->break_point = exit_point;
}

static void foreach_ref_array( struct codegen* codegen,
   struct foreach_writing* writing, struct foreach_stmt* stmt ) {
   // Allocate key variable.
   int key = 0;
   if ( stmt->key ) {
      key = c_alloc_script_var( codegen );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, key );
      // Allocate another key variable (the user's key variable) if the key
      // variable is intended to be modified.
      if ( stmt->key->modified ) {
         stmt->key->index = c_alloc_script_var( codegen );
      }
      else {
         stmt->key->index = key;
      }
   }
   // Allocate value variable.
   c_visit_var( codegen, stmt->value );
   int value = stmt->value->index;
   // Allocate another value variable (the user's value variable) if the value
   // variable is intended to be modified.
   switch ( writing->item ) {
   case FOREACHITEM_SUBARRAY:
   case FOREACHITEM_STRUCT:
      if ( stmt->value->modified ) {
         c_visit_var( codegen, stmt->value );
      }
      break;
   case FOREACHITEM_ARRAYREF:
   case FOREACHITEM_PRIMITIVE:
      break;
   }
   // When iterating over sub-arrays, the dimension information will be the
   // same for each sub-array, so assign it only once.
   if ( writing->item == FOREACHITEM_SUBARRAY ) {
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, value + 1 );
   }
   // Allocate number-of-elements-left variable.
   int left = c_alloc_script_var( codegen );
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, left );
   // Allocate offset variable.
   int offset = 0;
   switch ( writing->item ) {
   case FOREACHITEM_SUBARRAY:
   case FOREACHITEM_STRUCT:
      // Combine the offset and value variables into a single variable when
      // possible. When the item is a sub-array or a structure, the value
      // variable will contain a reference, which is the same as the offset.
      offset = value;
      break;
   case FOREACHITEM_ARRAYREF:
   case FOREACHITEM_PRIMITIVE:
      offset = c_alloc_script_var( codegen );
      break;
   }
   // Initialize offset variable with the base offset. For an array reference
   // collection, the base offset is always pushed.
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, offset );
   // Beginning of loop.
   struct c_point* start_point = c_create_point( codegen );
   c_append_node( codegen, &start_point->node );
   // Initialize user's key variable.
   if ( stmt->key && stmt->key->index != key ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, key );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->key->index );
   }
   // Initialize user' value variable.
   switch ( writing->item ) {
   case FOREACHITEM_SUBARRAY:
   case FOREACHITEM_STRUCT:
      if ( stmt->value->index != value ) {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, value );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index );
         if ( writing->item == FOREACHITEM_SUBARRAY ) {
            c_pcd( codegen, PCD_PUSHSCRIPTVAR, value + 1 );
            c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index + 1 );
         }
      }
      break;
   case FOREACHITEM_ARRAYREF:
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
      c_push_element( codegen, writing->storage, writing->index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, value );
      c_pcd( codegen, PCD_INCSCRIPTVAR, offset );
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
      c_push_element( codegen, writing->storage, writing->index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, value + 1 );
      break;
   case FOREACHITEM_PRIMITIVE:
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
      c_push_element( codegen, writing->storage, writing->index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, value );
      break;
   }
   // Body.
   c_write_stmt( codegen, stmt->body );
   // Increment.
   struct c_point* increment_point = c_create_point( codegen );
   c_append_node( codegen, &increment_point->node );
   writing->continue_point = increment_point;
   // Increment key variable.
   if ( stmt->key ) {
      c_pcd( codegen, PCD_INCSCRIPTVAR, key );
   }
   // Increment offset variable.
   switch ( writing->item ) {
   case FOREACHITEM_SUBARRAY:
      // Because the iteration variable containing the offset to the dimension
      // information is assigned only once and remains unchanged, use it to get
      // the sub-array size, which is the increment amount.
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, value + 1 );
      c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
      c_pcd( codegen, PCD_ADDSCRIPTVAR, offset );
      break;
   case FOREACHITEM_STRUCT:
      if ( writing->structure->size > 1 ) {
         c_pcd( codegen, PCD_PUSHNUMBER, writing->structure->size );
         c_pcd( codegen, PCD_ADDSCRIPTVAR, offset );
      }
      else {
         c_pcd( codegen, PCD_INCSCRIPTVAR, offset );
      }
      break;
   case FOREACHITEM_ARRAYREF:
      // The offset variable is already incremented once above to get the
      // dimension information. Increment the offset variable again to get the
      // starting offset of the next element.
      c_pcd( codegen, PCD_INCSCRIPTVAR, offset );
      break;
   case FOREACHITEM_PRIMITIVE:
      c_pcd( codegen, PCD_INCSCRIPTVAR, offset );
      break;
   }
   // Condition.
   c_pcd( codegen, PCD_DECSCRIPTVAR, left );
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, left );
   // Jump to start of loop body.
   struct c_jump* start_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &start_jump->node );
   start_jump->point = start_point;
   // Exit.
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   writing->break_point = exit_point;
}

static void foreach_str( struct codegen* codegen,
   struct foreach_writing* writing, struct foreach_stmt* stmt ) {
   // Allocate string variable.
   int string = c_alloc_script_var( codegen );
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, string );
   // Allocate key variable.
   int key = c_alloc_script_var( codegen );
   c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, key );
   if ( stmt->key ) {
      // Allocate another key variable (the user's key variable) if the key
      // variable is intended to be modified.
      if ( stmt->key->modified ) {
         stmt->key->index = c_alloc_script_var( codegen );
      }
      else {
         stmt->key->index = key;
      }
   }
   // Allocate value variable.
   c_visit_var( codegen, stmt->value );
   // Jump to condition.
   struct c_jump* cond_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &cond_jump->node );
   // Beginning of loop.
   struct c_point* start_point = c_create_point( codegen );
   c_append_node( codegen, &start_point->node );
   // Body.
   c_write_stmt( codegen, stmt->body );
   // Increment.
   struct c_point* increment_point = c_create_point( codegen );
   c_append_node( codegen, &increment_point->node );
   writing->continue_point = increment_point;
   // Increment key variable.
   c_pcd( codegen, PCD_INCSCRIPTVAR, key );
   // Condition.
   struct c_point* cond_point = c_create_point( codegen );
   c_append_node( codegen, &cond_point->node );
   cond_jump->point = cond_point;
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, string );
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, key );
   if ( stmt->key && stmt->key->index != key ) {
      c_pcd( codegen, PCD_DUP );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->key->index );
   }
   c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_GETCHAR );
   c_pcd( codegen, PCD_DUP );
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index );
   // Jump to start of loop body.
   struct c_jump* start_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &start_jump->node );
   start_jump->point = start_point;
   // Exit.
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   writing->break_point = exit_point;
}

static void visit_jump( struct codegen* codegen, struct jump* jump ) {
   struct c_jump* point_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &point_jump->node );
   jump->point_jump = point_jump;
}

static void set_jumps_point( struct codegen* codegen, struct jump* jump,
   struct c_point* point ) {
   while ( jump ) {
      jump->point_jump->point = point;
      jump = jump->next;
   }
}

static void visit_return( struct codegen* codegen, struct return_stmt* stmt ) {
   // Push return value.
   if ( stmt->return_value ) {
      c_push_initz_expr( codegen, codegen->func->func->ref,
         stmt->return_value );
      if ( stmt->buildmsg ) {
         write_msgbuild_block( codegen, stmt->buildmsg );
      }
   }
   // Exit.
   if ( codegen->func->nested_func ) {
      struct c_jump* epilogue_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &epilogue_jump->node );
      stmt->epilogue_jump = epilogue_jump;
   }
   else {
      if ( stmt->return_value ) {
         c_pcd( codegen, PCD_RETURNVAL );
      }
      else {
         c_pcd( codegen, PCD_RETURNVOID );
      }
   }
}

static void visit_paltrans( struct codegen* codegen, struct paltrans* trans ) {
   c_push_expr( codegen, trans->number );
   c_pcd( codegen, PCD_STARTTRANSLATION );
   struct palrange* range = trans->ranges;
   while ( range ) {
      c_push_expr( codegen, range->begin );
      c_push_expr( codegen, range->end );
      if ( range->rgb || range->saturated ) {
         c_push_expr( codegen, range->value.rgb.red1 );
         c_push_expr( codegen, range->value.rgb.green1 );
         c_push_expr( codegen, range->value.rgb.blue1 );
         c_push_expr( codegen, range->value.rgb.red2 );
         c_push_expr( codegen, range->value.rgb.green2 );
         c_push_expr( codegen, range->value.rgb.blue2 );
         if ( range->saturated ) {
            c_pcd( codegen, PCD_TRANSLATIONRANGE3 );
         }
         else {
            c_pcd( codegen, PCD_TRANSLATIONRANGE2 );
         }
      }
      else if ( range->colorisation ) {
         write_palrange_colorisation( codegen, range );
      }
      else if ( range->tint ) {
         write_palrange_tint( codegen, range );
      }
      else {
         c_push_expr( codegen, range->value.ent.begin );
         c_push_expr( codegen, range->value.ent.end );
         c_pcd( codegen, PCD_TRANSLATIONRANGE1 );
      }
      range = range->next;
   }
   c_pcd( codegen, PCD_ENDTRANSLATION );
}

static void write_palrange_colorisation( struct codegen* codegen,
   struct palrange* range ) {
   c_push_expr( codegen, range->value.colorisation.red );
   c_push_expr( codegen, range->value.colorisation.green );
   c_push_expr( codegen, range->value.colorisation.blue );
   c_pcd( codegen, PCD_TRANSLATIONRANGE4 );
}

static void write_palrange_tint( struct codegen* codegen,
   struct palrange* range ) {
   c_push_expr( codegen, range->value.tint.amount );
   c_push_expr( codegen, range->value.tint.red );
   c_push_expr( codegen, range->value.tint.green );
   c_push_expr( codegen, range->value.tint.blue );
   c_pcd( codegen, PCD_TRANSLATIONRANGE5 );
}

static void visit_script_jump( struct codegen* codegen,
   struct script_jump* stmt ) {
   switch ( stmt->type ) {
   case SCRIPT_JUMP_SUSPEND:
      c_pcd( codegen, PCD_SUSPEND );
      break;
   case SCRIPT_JUMP_RESTART:
      c_pcd( codegen, PCD_RESTART );
      break;
   default:
      c_pcd( codegen, PCD_TERMINATE );
      break;
   }
}

static void visit_label( struct codegen* codegen, struct label* label ) {
   if ( ! label->point ) {
      label->point = c_create_point( codegen );
   }
   c_append_node( codegen, &label->point->node );
}

static void visit_goto( struct codegen* codegen, struct goto_stmt* stmt ) {
   struct c_jump* jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &jump->node );
   if ( ! stmt->label->point ) {
      stmt->label->point = c_create_point( codegen );
   }
   jump->point = stmt->label->point;
}

static void visit_buildmsg_stmt( struct codegen* codegen,
   struct buildmsg_stmt* stmt ) {
   c_visit_expr( codegen, stmt->buildmsg->expr );
   write_msgbuild_block( codegen, stmt->buildmsg );
}

static void write_msgbuild_block( struct codegen* codegen,
   struct buildmsg* buildmsg ) {
   // When a message-building block is used more than once in the same
   // expression, instead of duplicating the block code, use a goto instruction
   // to enter the block. Single-usage blocks are inlined at the call site.
   if ( list_size( &buildmsg->usages ) > 1 ) {
      write_multi_usage_msgbuild_block( codegen, buildmsg );
   }
}

static void write_multi_usage_msgbuild_block( struct codegen* codegen,
   struct buildmsg* buildmsg ) {
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &exit_jump->node );
   struct c_point* enter_point = c_create_point( codegen );
   c_append_node( codegen, &enter_point->node );
   c_write_block( codegen, buildmsg->block );
   // Create jumps into the message-building block.
   unsigned int entry_number = 0;
   struct list_iter i;
   list_iterate( &buildmsg->usages, &i );
   while ( ! list_end( &i ) ) {
      struct buildmsg_usage* usage = list_data( &i );
      c_seek_node( codegen, &usage->point->node );
      c_pcd( codegen, PCD_PUSHNUMBER, entry_number );
      struct c_jump* jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &jump->node );
      jump->point = enter_point;
      struct c_point* return_point = c_create_point( codegen );
      c_append_node( codegen, &return_point->node );
      usage->point = return_point;
      ++entry_number;
      list_next( &i );
   }
   c_seek_node( codegen, codegen->node_tail );
   // Create return table.
   struct c_sortedcasejump* return_table = c_create_sortedcasejump( codegen );
   c_append_node( codegen, &return_table->node );
   entry_number = 0;
   list_iterate( &buildmsg->usages, &i );
   while ( ! list_end( &i ) ) {
      struct buildmsg_usage* usage = list_data( &i );
      struct c_casejump* entry = c_create_casejump( codegen, entry_number,
         usage->point );
      c_append_casejump( return_table, entry );
      ++entry_number;
      list_next( &i );
   }
   // Exit.
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
}

static void visit_expr_stmt( struct codegen* codegen,
   struct expr_stmt* stmt ) {
   struct list_iter i;
   list_iterate( &stmt->expr_list, &i );
   while ( ! list_end( &i ) ) {
      c_visit_expr( codegen, list_data( &i ) );
      list_next( &i );
   }
}
