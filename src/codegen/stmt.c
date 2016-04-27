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
static void visit_switch( struct codegen* codegen, struct switch_stmt* );
static void write_switch( struct codegen* codegen, struct switch_stmt* stmt );
static void write_string_switch( struct codegen* codegen,
   struct switch_stmt* stmt );
static void visit_case( struct codegen* codegen, struct case_label* );
static void visit_while( struct codegen* codegen, struct while_stmt* );
static void write_while( struct codegen* codegen, struct while_stmt* stmt );
static void write_folded_while( struct codegen* codegen,
   struct while_stmt* stmt );
static bool while_stmt( struct while_stmt* stmt );
static void visit_for( struct codegen* codegen, struct for_stmt* );
static void visit_foreach( struct codegen* codegen,
   struct foreach_stmt* stmt );
static void foreach_array( struct codegen* codegen, struct foreach_stmt* stmt,
   struct foreach_collection* collection );
static void foreach_ref( struct codegen* codegen, struct foreach_stmt* stmt,
   struct foreach_collection* collection );
static void foreach_str( struct codegen* codegen, struct foreach_stmt* stmt,
   struct foreach_collection* collection );
static void visit_jump( struct codegen* codegen, struct jump* );
static void set_jumps_point( struct codegen* codegen, struct jump* jump,
   struct c_point* point );
static void visit_return( struct codegen* codegen, struct return_stmt* );
static void visit_paltrans( struct codegen* codegen, struct paltrans* );
static void visit_script_jump( struct codegen* codegen, struct script_jump* );
static void visit_label( struct codegen* codegen, struct label* );
static void visit_goto( struct codegen* codegen, struct goto_stmt* );
static void visit_expr_stmt( struct codegen* codegen, struct expr_stmt* stmt );

void c_write_block( struct codegen* codegen, struct block* stmt ) {
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   list_iter_t i;
   list_iter_init( &i, &stmt->stmts );
   while ( ! list_end( &i ) ) {
      write_block_item( codegen, list_data( &i ) );
      list_next( &i );
   }
   pop_local_record( codegen );
}

void init_local_record( struct codegen* codegen,
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

void push_local_record( struct codegen* codegen,
   struct local_record* record ) {
   record->parent = codegen->local_record;
   codegen->local_record = record;
}

void pop_local_record( struct codegen* codegen ) {
   codegen->local_record = codegen->local_record->parent;
}

void write_block_item( struct codegen* codegen, struct node* node ) {
   switch ( node->type ) {
   case NODE_VAR:
      c_visit_var( codegen, ( struct var* ) node );
      break;
   case NODE_FUNC:
      c_visit_nested_func( codegen,
         ( struct func* ) node );
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
      break;
   default:
      c_write_stmt( codegen, node );
      break;
   }
}

void write_assert( struct codegen* codegen, struct assert* assert ) {
   if ( ! assert->is_static && codegen->task->options->write_asserts ) {
      write_runtime_assert( codegen, assert );
   }
}

void write_runtime_assert( struct codegen* codegen, struct assert* assert ) {
   c_push_cond( codegen, assert->cond );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &exit_jump->node );
   c_pcd( codegen, PCD_BEGINPRINT );
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
      c_push_string( codegen, assert->message );
      c_pcd( codegen, PCD_PRINTSTRING );
   }
   // Message properties, in ACS terms: HUDMSG_LOG, 0, CR_RED, 1.5, 0.5, 0.0
   c_pcd( codegen, PCD_MOREHUDMESSAGE );
   c_pcd( codegen, PCD_PUSHNUMBER, ( int ) 0x80000000 );
   c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   c_pcd( codegen, PCD_PUSHNUMBER, 6 );
   c_pcd( codegen, PCD_PUSHNUMBER, 98304 );
   c_pcd( codegen, PCD_PUSHNUMBER, 16384 );
   c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   c_pcd( codegen, PCD_ENDHUDMESSAGEBOLD );
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
   case NODE_EXPR_STMT:
      visit_expr_stmt( codegen,
         ( struct expr_stmt* ) node );
      break;
   case NODE_INLINE_ASM:
      p_visit_inline_asm( codegen,
         ( struct inline_asm* ) node );
      break;
   default:
      UNREACHABLE();
   }
}

void visit_if( struct codegen* codegen, struct if_stmt* stmt ) {
   c_push_cond( codegen, stmt->cond );
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
}

void visit_switch( struct codegen* codegen, struct switch_stmt* stmt ) {
   if ( stmt->cond->spec == SPEC_STR ) {
      write_string_switch( codegen, stmt );
   }
   else {
      write_switch( codegen, stmt );
   }
}

void write_switch ( struct codegen* codegen, struct switch_stmt* stmt ) {
   struct c_point* exit_point = c_create_point( codegen );
   // Case selection.
   c_push_expr( codegen, stmt->cond );
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
}

// NOTE: Right now, the implementation does a linear search when attempting to
// find the correct case. A binary search might be possible. 
void write_string_switch( struct codegen* codegen, struct switch_stmt* stmt ) {
   struct c_point* exit_point = c_create_point( codegen );
   // Case selection.
   c_push_expr( codegen, stmt->cond );
   struct case_label* label = stmt->case_head;
   while ( label ) {
      if ( label->next ) {
         c_pcd( codegen, PCD_DUP );
      }
      c_push_string( codegen, t_lookup_string( codegen->task,
         label->number->value ) );
      c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_STRCMP );
      struct c_jump* jump = c_create_jump( codegen, PCD_IFNOTGOTO );
      c_append_node( codegen, &jump->node );
      label->point = c_create_point( codegen );
      jump->point = label->point;
      label = label->next;
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

void visit_case( struct codegen* codegen, struct case_label* label ) {
   c_append_node( codegen, &label->point->node );
}

void visit_while( struct codegen* codegen, struct while_stmt* stmt ) {
   if ( stmt->cond->folded ) {
      write_folded_while( codegen, stmt );
   }
   else {
      write_while( codegen, stmt );
   }
}

void write_while( struct codegen* codegen, struct while_stmt* stmt ) {
   bool while_until = ( stmt->type == WHILE_WHILE ||
      stmt->type == WHILE_UNTIL );
   // Initial jump to condition for while/until loop.
   struct c_jump* cond_jump = NULL;
   if ( while_until ) {
      cond_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &cond_jump->node );
   }
   // Body.
   struct c_point* body_point = c_create_point( codegen );
   c_append_node( codegen, &body_point->node );
   c_write_stmt( codegen, stmt->body );
   // Condition.
   struct c_point* cond_point = c_create_point( codegen );
   c_append_node( codegen, &cond_point->node );
   c_push_cond( codegen, stmt->cond );
   if ( while_until ) {
      cond_jump->point = cond_point;
   }
   struct c_jump* body_jump = c_create_jump( codegen,
      ( while_stmt( stmt ) ? PCD_IFGOTO : PCD_IFNOTGOTO ) );
   c_append_node( codegen, &body_jump->node );
   body_jump->point = body_point;
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   set_jumps_point( codegen, stmt->jump_break, exit_point );
   set_jumps_point( codegen, stmt->jump_continue, cond_point );
}

void write_folded_while( struct codegen* codegen, struct while_stmt* stmt ) {
   bool true_cond = ( while_stmt( stmt ) && stmt->cond->value != 0 ) ||
      ( stmt->cond->value == 0 );
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
   c_write_stmt( codegen, stmt->body );
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

bool while_stmt( struct while_stmt* stmt ) {
   return ( stmt->type == WHILE_WHILE || stmt->type == WHILE_DO_WHILE );
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
void visit_for( struct codegen* codegen, struct for_stmt* stmt ) {
   // Initialization.
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   list_iter_t i;
   list_iter_init( &i, &stmt->init );
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
   bool test_cond = ( stmt->cond &&
      ! ( stmt->cond->folded && stmt->cond->value != 0 ) );
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
      list_iter_init( &i, &stmt->post );
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
      c_push_cond( codegen, stmt->cond );
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

void visit_foreach( struct codegen* codegen, struct foreach_stmt* stmt ) {
   struct local_record record;
   init_local_record( codegen, &record );
   push_local_record( codegen, &record );
   // Allocate variables to hold the key and value.
   if ( stmt->key ) {
      c_visit_var( codegen, stmt->key );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->key->index );
   }
   c_visit_var( codegen, stmt->value );
   // Push collection onto the stack.
   struct foreach_collection collection;
   c_push_foreach_collection( codegen, &collection, stmt->collection );
   if ( collection.dim ) {
      foreach_array( codegen, stmt, &collection );
   }
   else if ( collection.ref ) {
      foreach_ref( codegen, stmt, &collection );
   }
   else {
      foreach_str( codegen, stmt, &collection );
   }
   // Patch jump statements.
/*
   set_jumps_point( codegen, stmt->jump_break, exit_point );
   set_jumps_point( codegen, stmt->jump_continue,
      post_point ? post_point :
      cond_point ? cond_point :
      body_point ); */
   pop_local_record( codegen );
}

void foreach_array( struct codegen* codegen, struct foreach_stmt* stmt,
   struct foreach_collection* collection ) {
   // Initialize variable that stores the element offset.
   // -----------------------------------------------------------------------
   // When the array elements are of size 1, we use a single variable to hold
   // both the key and the element offset, since both values will be the same.
   int offset_var = 0;
   if ( stmt->key && collection->element_size_one_primitive ) {
      offset_var = stmt->key->index;
   }
   else {
      offset_var = c_alloc_script_var( codegen );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, offset_var );
   }
   // Begin processing element.
   // -----------------------------------------------------------------------
   struct c_point* element_point = c_create_point( codegen );
   c_append_node( codegen, &element_point->node );
   // Calculate address to element.
   // -----------------------------------------------------------------------
   // Duplicate the base offset so it can be reused on the next element.
   if ( collection->base_pushed ) {
      c_pcd( codegen, PCD_DUP );
   }
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset_var );
   if ( collection->base_pushed ) {
      c_pcd( codegen, PCD_ADD );
   }
   // Initialize value variable with element.
   // -----------------------------------------------------------------------
   // Implicit array reference.
   if ( collection->dim->next ) {
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index );
      c_pcd( codegen, PCD_PUSHNUMBER, collection->diminfo_start + 1 );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index + 1 );
   }
   // Implicit structure reference.
   else if ( collection->structure ) {
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index );
   }
   // Array reference.
   else if ( collection->ref && collection->ref->type == REF_ARRAY ) {
      c_pcd( codegen, PCD_DUP );
      c_pcd( codegen, PCD_PUSHNUMBER, 1 );
      c_pcd( codegen, PCD_ADD );
      c_push_element( codegen, collection->storage, collection->index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index + 1 );
      c_push_element( codegen, collection->storage, collection->index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index );
   }
   // Primitive.
   else {
      c_push_element( codegen, collection->storage, collection->index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index );
   }
   // Initialize key variable.
   // -----------------------------------------------------------------------
   if ( stmt->key ) {
      // Push the key onto the stack because the user is allowed to change the
      // key variable. The key variable will be restored after the body is done
      // executing.
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, stmt->key->index );
   }
   // Body.
   // -----------------------------------------------------------------------
   c_write_stmt( codegen, stmt->body );
   // Restore the key variable.
   // -----------------------------------------------------------------------
   if ( stmt->key ) {
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->key->index );
      c_pcd( codegen, PCD_INCSCRIPTVAR, stmt->key->index );
   }
   // Move to the next element.
   // -----------------------------------------------------------------------
   if ( collection->dim->element_size > 1 ) {
      c_pcd( codegen, PCD_PUSHNUMBER, collection->dim->element_size );
      c_pcd( codegen, PCD_ADDSCRIPTVAR, offset_var );
   }
   else {
      // Don't increment the offset variable when it's the same as the key
      // variable, since the key is already incremented.
      if ( ! ( stmt->key && stmt->key->index == offset_var ) ) {
         c_pcd( codegen, PCD_INCSCRIPTVAR, offset_var );
      }
   }
   // See if another element is present.
   // -----------------------------------------------------------------------
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset_var );
   c_pcd( codegen, PCD_PUSHNUMBER,
      collection->dim->length * collection->dim->element_size );
   c_pcd( codegen, PCD_LT );
   struct c_jump* element_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &element_jump->node );
   element_jump->point = element_point;
   // Exit.
   // -----------------------------------------------------------------------
   if ( collection->base_pushed ) { 
      c_pcd( codegen, PCD_DROP );
   }
}

void foreach_ref( struct codegen* codegen, struct foreach_stmt* stmt,
   struct foreach_collection* collection ) {
}

void foreach_str( struct codegen* codegen, struct foreach_stmt* stmt,
   struct foreach_collection* collection ) {
   int offset_var = 0;
   // Reuse the key variable as the offset variable.
   if ( stmt->key ) {
      offset_var = stmt->key->index;
   }
   else {
      offset_var = c_alloc_script_var( codegen );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, offset_var );
   }
   struct c_jump* cond_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &cond_jump->node );
   // Pre-body.
   struct c_point* start_point = c_create_point( codegen );
   c_append_node( codegen, &start_point->node );
   if ( stmt->key ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset_var );
   }
   // Body.
   c_write_stmt( codegen, stmt->body );
   // Post-body.
   if ( stmt->key ) {
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, offset_var );
   }
   c_pcd( codegen, PCD_INCSCRIPTVAR, offset_var );
   // Condition.
   struct c_point* cond_point = c_create_point( codegen );
   c_append_node( codegen, &cond_point->node );
   c_pcd( codegen, PCD_DUP );
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset_var );
   c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_GETCHAR );
   c_pcd( codegen, PCD_DUP );
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, stmt->value->index );
   cond_jump->point = cond_point;
   // Jump to body.
   struct c_jump* body_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &body_jump->node );
   body_jump->point = start_point;
   c_pcd( codegen, PCD_DROP );
}

void visit_jump( struct codegen* codegen, struct jump* jump ) {
   struct c_jump* point_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &point_jump->node );
   jump->point_jump = point_jump;
}

void set_jumps_point( struct codegen* codegen, struct jump* jump,
   struct c_point* point ) {
   while ( jump ) {
      jump->point_jump->point = point;
      jump = jump->next;
   }
}

void visit_return( struct codegen* codegen, struct return_stmt* stmt ) {
   if ( codegen->func->nested_func ) {
      if ( stmt->return_value ) {
         c_push_expr( codegen, stmt->return_value->expr );
      }
      struct c_jump* epilogue_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &epilogue_jump->node );
      stmt->epilogue_jump = epilogue_jump;
   }
   else {
      if ( stmt->return_value ) {
         c_push_expr( codegen, stmt->return_value->expr );
         c_pcd( codegen, PCD_RETURNVAL );
      }
      else {
         c_pcd( codegen, PCD_RETURNVOID );
      }
   }
}

void visit_paltrans( struct codegen* codegen, struct paltrans* trans ) {
   c_push_expr( codegen, trans->number );
   c_pcd( codegen, PCD_STARTTRANSLATION );
   struct palrange* range = trans->ranges;
   while ( range ) {
      c_push_expr( codegen, range->begin );
      c_push_expr( codegen, range->end );
      if ( range->rgb ) {
         c_push_expr( codegen, range->value.rgb.red1 );
         c_push_expr( codegen, range->value.rgb.green1 );
         c_push_expr( codegen, range->value.rgb.blue1 );
         c_push_expr( codegen, range->value.rgb.red2 );
         c_push_expr( codegen, range->value.rgb.green2 );
         c_push_expr( codegen, range->value.rgb.blue2 );
         c_pcd( codegen, PCD_TRANSLATIONRANGE2 );
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

void visit_script_jump( struct codegen* codegen, struct script_jump* stmt ) {
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

void visit_label( struct codegen* codegen, struct label* label ) {
   if ( ! label->point ) {
      label->point = c_create_point( codegen );
   }
   c_append_node( codegen, &label->point->node );
}

void visit_goto( struct codegen* codegen, struct goto_stmt* stmt ) {
   struct c_jump* jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &jump->node );
   if ( ! stmt->label->point ) {
      stmt->label->point = c_create_point( codegen );
   }
   jump->point = stmt->label->point;
}

void visit_expr_stmt( struct codegen* codegen, struct expr_stmt* stmt ) {
   c_visit_expr( codegen, stmt->packed_expr->expr );
}