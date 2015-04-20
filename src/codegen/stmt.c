#include "phase.h"
#include "pcode.h"

static void write_block_item( struct codegen* codegen, struct node* node );
static void visit_if( struct codegen* codegen, struct if_stmt* );
static void visit_switch( struct codegen* codegen, struct switch_stmt* );
static void visit_case( struct codegen* codegen, struct case_label* );
static void visit_while( struct codegen* codegen, struct while_stmt* );
static void visit_for( struct codegen* codegen, struct for_stmt* );
static void visit_jump( struct codegen* codegen, struct jump* );
static void add_jumps( struct codegen* codegen, struct jump*, int );
static void visit_return( struct codegen* codegen, struct return_stmt* );
static void visit_paltrans( struct codegen* codegen, struct paltrans* );
static void visit_script_jump( struct codegen* codegen, struct script_jump* );
static void visit_label( struct codegen* codegen, struct label* );
static void visit_goto( struct codegen* codegen, struct goto_stmt* );
static void visit_packed_expr( struct codegen* codegen, struct packed_expr* );

void c_write_block( struct codegen* codegen, struct block* block,
   bool add_visit ) {
   if ( add_visit ) {
      c_add_block_visit( codegen );
   }
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      write_block_item( codegen, list_data( &i ) );
      list_next( &i );
   }
   if ( add_visit ) {
      c_pop_block_visit( codegen );
   }
}

void c_add_block_visit( struct codegen* codegen ) {
   struct block_visit* visit;
   if ( codegen->block_visit_free ) {
      visit = codegen->block_visit_free;
      codegen->block_visit_free = visit->prev;
   }
   else {
      visit = mem_alloc( sizeof( *visit ) );
   }
   visit->prev = codegen->block_visit;
   codegen->block_visit = visit;
   visit->format_block_usage = NULL;
   visit->nested_func = false;
   if ( ! visit->prev ) {
      codegen->func_visit = visit;
   }
}

void c_pop_block_visit( struct codegen* codegen ) {
   struct block_visit* prev = codegen->block_visit->prev;
   if ( ! prev ) {
      codegen->func_visit = NULL;
   }
   codegen->block_visit->prev = codegen->block_visit_free;
   codegen->block_visit_free = codegen->block_visit;
   codegen->block_visit = prev;
}

void write_block_item( struct codegen* codegen, struct node* node ) {
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
   default:
      c_write_stmt( codegen, node );
      break;
   }
}

void c_write_stmt( struct codegen* codegen, struct node* node ) {
   switch ( node->type ) {
   case NODE_BLOCK:
      c_write_block( codegen, ( struct block* ) node, true );
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
   case NODE_FORMAT_ITEM:
      c_visit_format_item( codegen, ( struct format_item* ) node );
      break;
   case NODE_PALTRANS:
      visit_paltrans( codegen, ( struct paltrans* ) node );
      break;
   case NODE_PACKED_EXPR:
      visit_packed_expr( codegen, ( struct packed_expr* ) node );
      break;
   default:
      break;
   }
}

void visit_if( struct codegen* codegen, struct if_stmt* stmt ) {
   c_push_expr( codegen, stmt->cond, true );
   int cond = c_tell( codegen );
   c_add_opc( codegen, PCD_IFNOTGOTO );
   c_add_arg( codegen, 0 );
   c_write_stmt( codegen, stmt->body );
   int bail = c_tell( codegen );
   if ( stmt->else_body ) {
      // Exit from if block:
      int bail_if_block = c_tell( codegen );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, 0 ); 
      bail = c_tell( codegen );
      c_write_stmt( codegen, stmt->else_body );
      int stmt_end = c_tell( codegen );
      c_seek( codegen, bail_if_block );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, stmt_end );
   }
   c_seek( codegen, cond );
   c_add_opc( codegen, PCD_IFNOTGOTO );
   c_add_arg( codegen, bail );
   c_seek_end( codegen );
}

void visit_switch( struct codegen* codegen, struct switch_stmt* stmt ) {
   c_push_expr( codegen, stmt->cond, true );
   int num_cases = 0;
   struct case_label* label = stmt->case_head;
   while ( label ) {
      ++num_cases;
      label = label->next;
   }
   int test = c_tell( codegen );
   if ( num_cases ) {
      c_add_opc( codegen, PCD_CASEGOTOSORTED );
      c_add_arg( codegen, 0 );
      for ( int i = 0; i < num_cases; ++i ) {
         c_add_arg( codegen, 0 );
         c_add_arg( codegen, 0 );
      }
   }
   c_add_opc( codegen, PCD_DROP );
   int fail = c_tell( codegen );
   c_add_opc( codegen, PCD_GOTO );
   c_add_arg( codegen, 0 );
   c_write_stmt( codegen, stmt->body );
   int done = c_tell( codegen );
   if ( num_cases ) {
      c_seek( codegen, test );
      c_add_opc( codegen, PCD_CASEGOTOSORTED );
      c_add_arg( codegen, num_cases );
      label = stmt->case_head;
      while ( label ) {
         c_add_arg( codegen, label->number->value );
         c_add_arg( codegen, label->offset );
         label = label->next;
      }
   }
   c_seek( codegen, fail );
   c_add_opc( codegen, PCD_GOTO );
   int fail_pos = done;
   if ( stmt->case_default ) {
      fail_pos = stmt->case_default->offset;
   }
   c_add_arg( codegen, fail_pos );
   add_jumps( codegen, stmt->jump_break, done );
}

void visit_case( struct codegen* codegen, struct case_label* label ) {
   label->offset = c_tell( codegen );
}

void visit_while( struct codegen* codegen, struct while_stmt* stmt ) {
   int test = 0;
   int done = 0;
   if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
      int jump = 0;
      if ( ! stmt->cond->folded || (
         ( stmt->type == WHILE_WHILE && ! stmt->cond->value ) ||
         ( stmt->type == WHILE_UNTIL && stmt->cond->value ) ) ) {
         jump = c_tell( codegen );
         c_add_opc( codegen, PCD_GOTO );
         c_add_arg( codegen, 0 );
      }
      int body = c_tell( codegen );
      c_write_stmt( codegen, stmt->body );
      if ( stmt->cond->folded ) {
         if ( ( stmt->type == WHILE_WHILE && stmt->cond->value ) ||
            ( stmt->type == WHILE_UNTIL && ! stmt->cond->value ) ) {
            c_add_opc( codegen, PCD_GOTO );
            c_add_arg( codegen, body );
            done = c_tell( codegen );
            test = body;
         }
         else {
            done = c_tell( codegen );
            test = done;
            c_seek( codegen, jump );
            c_add_opc( codegen, PCD_GOTO );
            c_add_arg( codegen, done );
         }
      }
      else {
         test = c_tell( codegen );
         c_push_expr( codegen, stmt->cond, true );
         int code = PCD_IFGOTO;
         if ( stmt->type == WHILE_UNTIL ) {
            code = PCD_IFNOTGOTO;
         }
         c_add_opc( codegen, code );
         c_add_arg( codegen, body );
         done = c_tell( codegen );
         c_seek( codegen, jump );
         c_add_opc( codegen, PCD_GOTO );
         c_add_arg( codegen, test );
      }
   }
   // do-while / do-until.
   else {
      int body = c_tell( codegen );
      c_write_stmt( codegen, stmt->body );
      // Condition:
      if ( stmt->cond->folded ) {
         // Optimization: Only loop when the condition is satisfied.
         if ( ( stmt->type == WHILE_DO_WHILE && stmt->cond->value ) ||
            ( stmt->type == WHILE_DO_UNTIL && ! stmt->cond->value ) ) {
            c_add_opc( codegen, PCD_GOTO );
            c_add_arg( codegen, body );
            done = c_tell( codegen );
            test = body;
         }
         else {
            done = c_tell( codegen );
            test = done;
         }
      }
      else {
         test = c_tell( codegen );
         c_push_expr( codegen, stmt->cond, true );
         int code = PCD_IFGOTO;
         if ( stmt->type == WHILE_DO_UNTIL ) {
            code = PCD_IFNOTGOTO;
         }
         c_add_opc( codegen, code );
         c_add_arg( codegen, body );
         done = c_tell( codegen );
      }
   }
   add_jumps( codegen, stmt->jump_continue, test );
   add_jumps( codegen, stmt->jump_break, done );
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
   list_iter_t i;
   list_iter_init( &i, &stmt->init );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      switch ( node->type ) {
      case NODE_EXPR:
         c_visit_expr( codegen, ( struct expr* ) node );
         break;
      case NODE_VAR:
         c_visit_var( codegen, ( struct var* ) node );
         break;
      default:
         break;
      }
      list_next( &i );
   }
   // Jump to condition.
   int jump = 0;
   if ( stmt->cond ) {
      if ( ! stmt->cond->folded || ! stmt->cond->value ) {
         jump = c_tell( codegen );
         c_add_opc( codegen, PCD_GOTO );
         c_add_arg( codegen, 0 );
      }
   }
   // Body.
   int body = c_tell( codegen );
   c_write_stmt( codegen, stmt->body );
   // Post expressions.
   int post = c_tell( codegen );
   list_iter_init( &i, &stmt->post );
   while ( ! list_end( &i ) ) {
      c_visit_expr( codegen, list_data( &i ) );
      list_next( &i );
   }
   // Condition.
   int test = 0;
   if ( stmt->cond ) {
      if ( stmt->cond->folded ) {
         if ( stmt->cond->value ) {
            c_add_opc( codegen, PCD_GOTO );
            c_add_arg( codegen, body );
         }
      }
      else {
         test = c_tell( codegen );
         c_push_expr( codegen, stmt->cond, true );
         c_add_opc( codegen, PCD_IFGOTO );
         c_add_arg( codegen, body );
      }
   }
   else {
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, body );
   }
   // Jump to condition.
   int done = c_tell( codegen );
   if ( stmt->cond ) {
      if ( stmt->cond->folded ) {
         if ( ! stmt->cond->value ) {
            c_seek( codegen, jump );
            c_add_opc( codegen, PCD_GOTO );
            c_add_arg( codegen, done );
         }
      }
      else {
         c_seek( codegen, jump );
         c_add_opc( codegen, PCD_GOTO );
         c_add_arg( codegen, test );
      }
   }
   add_jumps( codegen, stmt->jump_continue, post );
   add_jumps( codegen, stmt->jump_break, done );
}

void visit_jump( struct codegen* codegen, struct jump* jump ) {
   jump->obj_pos = c_tell( codegen );
   c_add_opc( codegen, PCD_GOTO );
   c_add_arg( codegen, 0 );
}

void add_jumps( struct codegen* codegen, struct jump* jump, int pos ) {
   while ( jump ) {
      c_seek( codegen, jump->obj_pos );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, pos );
      jump = jump->next;
   }
   c_seek_end( codegen );
}

void visit_return( struct codegen* codegen, struct return_stmt* stmt ) {
   if ( codegen->func_visit->nested_func ) {
      if ( stmt->return_value ) {
         c_push_expr( codegen, stmt->return_value->expr, false );
      }
      stmt->obj_pos = c_tell( codegen );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, 0 );
   }
   else {
      if ( stmt->return_value ) {
         c_push_expr( codegen, stmt->return_value->expr, false );
         c_add_opc( codegen, PCD_RETURNVAL );
      }
      else {
         c_add_opc( codegen, PCD_RETURNVOID );
      }
   }
}

void visit_paltrans( struct codegen* codegen, struct paltrans* trans ) {
   c_push_expr( codegen, trans->number, true );
   c_add_opc( codegen, PCD_STARTTRANSLATION );
   struct palrange* range = trans->ranges;
   while ( range ) {
      c_push_expr( codegen, range->begin, true );
      c_push_expr( codegen, range->end, true );
      if ( range->rgb ) {
         c_push_expr( codegen, range->value.rgb.red1, true );
         c_push_expr( codegen, range->value.rgb.green1, true );
         c_push_expr( codegen, range->value.rgb.blue1, true );
         c_push_expr( codegen, range->value.rgb.red2, true );
         c_push_expr( codegen, range->value.rgb.green2, true );
         c_push_expr( codegen, range->value.rgb.blue2, true );
         c_add_opc( codegen, PCD_TRANSLATIONRANGE2 );
      }
      else {
         c_push_expr( codegen, range->value.ent.begin, true );
         c_push_expr( codegen, range->value.ent.end, true );
         c_add_opc( codegen, PCD_TRANSLATIONRANGE1 );
      }
      range = range->next;
   }
   c_add_opc( codegen, PCD_ENDTRANSLATION );
}

void visit_script_jump( struct codegen* codegen, struct script_jump* stmt ) {
   switch ( stmt->type ) {
   case SCRIPT_JUMP_SUSPEND:
      c_add_opc( codegen, PCD_SUSPEND );
      break;
   case SCRIPT_JUMP_RESTART:
      c_add_opc( codegen, PCD_RESTART );
      break;
   default:
      c_add_opc( codegen, PCD_TERMINATE );
      break;
   }
}

void visit_label( struct codegen* codegen, struct label* label ) {
   label->obj_pos = c_tell( codegen );
   struct goto_stmt* stmt = label->users;
   while ( stmt ) {
      if ( stmt->obj_pos ) {
         c_seek( codegen, stmt->obj_pos );
         c_add_opc( codegen, PCD_GOTO );
         c_add_arg( codegen, label->obj_pos );
      }
      stmt = stmt->next;
   }
   c_seek_end( codegen );
}

void visit_goto( struct codegen* codegen, struct goto_stmt* stmt ) {
   if ( stmt->label->obj_pos ) {
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, stmt->label->obj_pos );
   }
   else {
      stmt->obj_pos = c_tell( codegen );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, 0 );
   }
}

void visit_packed_expr( struct codegen* codegen, struct packed_expr* stmt ) {
   c_visit_expr( codegen, stmt->expr );
}