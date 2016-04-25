#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "phase.h"

static void read_block( struct parse* parse, struct stmt_reading* );
static void read_case( struct parse* parse, struct stmt_reading* );
static void read_default_case( struct parse* parse, struct stmt_reading* );
static void read_label( struct parse* parse, struct stmt_reading* );
static void read_stmt( struct parse* parse, struct stmt_reading* );
static void read_if( struct parse* parse, struct stmt_reading* );
static void read_switch( struct parse* parse, struct stmt_reading* );
static struct switch_stmt* alloc_switch_stmt( void );
static void read_while( struct parse* parse, struct stmt_reading* );
static void read_do( struct parse* parse, struct stmt_reading* );
static void read_for( struct parse* parse, struct stmt_reading* );
static void read_foreach( struct parse* parse, struct stmt_reading* reading );
static struct foreach_stmt* alloc_foreach( void );
static void read_jump( struct parse* parse, struct stmt_reading* );
static void read_script_jump( struct parse* parse, struct stmt_reading* );
static void read_return( struct parse* parse, struct stmt_reading* );
static void read_goto( struct parse* parse, struct stmt_reading* );
static void read_paltrans( struct parse* parse, struct stmt_reading* );
static void read_palrange_rgb_field( struct parse* parse, struct expr**,
   struct expr**, struct expr** );
static void read_expr_stmt( struct parse* parse,
   struct stmt_reading* reading );
static void read_packed_expr( struct parse* parse,
   struct stmt_reading* reading );
static void read_onetime_msgbuild_func( struct parse* parse,
   struct stmt_reading* reading, struct packed_expr* packed_expr );
static struct label* alloc_label( const char*, struct pos );
static struct import* read_single_import( struct parse* parse );
static struct import_item* read_selected_import_items( struct parse* parse );
static struct import_item* read_import_item( struct parse* parse );
static void read_assert( struct parse* parse, struct stmt_reading* reading );
static struct assert* alloc_assert( struct pos* pos );

void t_print_name( struct name* name ) {
   struct str str;
   str_init( &str );
   t_copy_name( name, true, &str );
   printf( "%s\n", str.value );
   str_deinit( &str );
}

void p_init_stmt_reading( struct stmt_reading* reading, struct list* labels ) {
   reading->labels = labels;
   reading->node = NULL;
   reading->block_node = NULL;
   reading->packed_expr = NULL;
}

void p_read_top_stmt( struct parse* parse, struct stmt_reading* reading,
   bool need_block ) {
   if ( need_block ) {
      read_block( parse, reading );
   }
   else {
      read_stmt( parse, reading );
   }
   // All goto statements need to refer to valid labels.
   if ( reading->node->type == NODE_BLOCK ) {
      list_iter_t i;
      list_iter_init( &i, reading->labels );
      while ( ! list_end( &i ) ) {
         struct label* label = list_data( &i );
         if ( ! label->defined ) {
            p_diag( parse, DIAG_POS_ERR, &label->pos,
               "label `%s` not found", label->name );
            p_bail( parse );
         }
         list_next( &i );
      }
   }
}

void read_block( struct parse* parse, struct stmt_reading* reading ) {
   p_test_tk( parse, TK_BRACE_L );
   struct block* block = mem_alloc( sizeof( *block ) );
   block->node.type = NODE_BLOCK;
   list_init( &block->stmts );
   block->pos = parse->tk_pos;
   p_read_tk( parse );
   while ( true ) {
      if ( p_is_dec( parse ) ) {
         struct dec dec;
         p_init_dec( &dec );
         dec.area = DEC_LOCAL;
         dec.name_offset = parse->ns->body;
         dec.vars = &block->stmts;
         p_read_dec( parse, &dec );
      }
      else if ( parse->tk == TK_CASE ) {
         read_case( parse, reading );
         list_append( &block->stmts, reading->node );
      }
      else if ( parse->tk == TK_DEFAULT ) {
         read_default_case( parse, reading );
         list_append( &block->stmts, reading->node );
      }
      else if ( parse->tk == TK_ID && p_peek( parse ) == TK_COLON ) {
         read_label( parse, reading );
         list_append( &block->stmts, reading->node );
      }
      else if ( parse->tk == TK_BRACE_R ) {
         p_read_tk( parse );
         break;
      }
      else if ( parse->tk == TK_USING ) {
         p_read_using( parse, &block->stmts );
      }
      else if ( parse->tk == TK_ASSERT ||
         ( parse->tk == TK_STATIC && p_peek( parse ) == TK_ASSERT ) ) {
         read_assert( parse, reading );
         list_append( &block->stmts, reading->node );
      }
      else {
         read_stmt( parse, reading );
         if ( reading->node->type != NODE_NONE ) {
            list_append( &block->stmts, reading->node );
         }
      }
   }
   reading->node = &block->node;
   reading->block_node = block;
}

void read_case( struct parse* parse, struct stmt_reading* reading ) {
   struct case_label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_CASE;
   label->offset = 0;
   label->next = NULL;
   label->point = NULL;
   label->pos = parse->tk_pos;
   p_read_tk( parse );
   struct expr_reading number;
   p_init_expr_reading( &number, false, false, false, true );
   p_read_expr( parse, &number );
   label->number = number.output_node;
   p_test_tk( parse, TK_COLON );
   p_read_tk( parse );
   reading->node = &label->node;
}

void read_default_case( struct parse* parse, struct stmt_reading* reading ) {
   struct case_label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_CASE_DEFAULT;
   label->pos = parse->tk_pos;
   label->offset = 0;
   p_read_tk( parse );
   p_test_tk( parse, TK_COLON );
   p_read_tk( parse );
   reading->node = &label->node;
}

void read_label( struct parse* parse, struct stmt_reading* reading ) {
   struct label* label = NULL;
   list_iter_t i;
   list_iter_init( &i, reading->labels );
   while ( ! list_end( &i ) ) {
      struct label* prev = list_data( &i );
      if ( strcmp( prev->name, parse->tk_text ) == 0 ) {
         label = prev;
         break;
      }
      list_next( &i );
   }
   if ( label ) {
      if ( label->defined ) {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "duplicate label `%s`", parse->tk_text );
         p_diag( parse, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "label already found here" );
         p_bail( parse );
      }
      else {
         label->defined = true;
         label->pos = parse->tk_pos;
      }
   }
   else {
      label = alloc_label( parse->tk_text, parse->tk_pos );
      label->defined = true;
      list_append( reading->labels, label );
   }
   p_read_tk( parse );
   p_read_tk( parse );
   reading->node = &label->node;
}

struct label* alloc_label( const char* name, struct pos pos ) {
   struct label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_GOTO_LABEL;
   label->name = name;
   label->defined = false;
   label->pos = pos;
   label->users = NULL;
   label->point = NULL;
   return label;
}

void read_stmt( struct parse* parse, struct stmt_reading* reading ) {
   switch ( parse->tk ) {
   case TK_BRACE_L:
      read_block( parse, reading );
      break;
   case TK_IF:
      read_if( parse, reading );
      break;
   case TK_SWITCH:
      read_switch( parse, reading );
      break;
   case TK_WHILE:
   case TK_UNTIL:
      read_while( parse, reading );
      break;
   case TK_DO:
      read_do( parse, reading );
      break;
   case TK_FOR:
      read_for( parse, reading );
      break;
   case TK_FOREACH:
      read_foreach( parse, reading );
      break;
   case TK_BREAK:
   case TK_CONTINUE:
      read_jump( parse, reading );
      break;
   case TK_TERMINATE:
   case TK_RESTART:
   case TK_SUSPEND:
      read_script_jump( parse, reading );
      break;
   case TK_RETURN:
      read_return( parse, reading );
      break;
   case TK_GOTO:
      read_goto( parse, reading );
      break;
   case TK_PALTRANS:
      read_paltrans( parse, reading );
      break;
   case TK_GT:
      p_read_asm( parse, reading );
      break;
   case TK_SEMICOLON:
      {
         static struct node node = { NODE_NONE };
         reading->node = &node;
         p_read_tk( parse );
      }
      break;
   default:
      read_expr_stmt( parse, reading );
      break;
   }
}

void read_if( struct parse* parse, struct stmt_reading* reading ) {
   p_read_tk( parse );
   struct if_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_IF;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading cond;
   p_init_expr_reading( &cond, false, false, false, true );
   p_read_expr( parse, &cond );
   stmt->cond = cond.output_node;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   // Warn when the body of an `if` statement is empty. It is assumed that a
   // semicolon is the empty statement.
   if ( parse->tk == TK_SEMICOLON ) {
      p_diag( parse, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &parse->tk_pos, "body of `if` statement is empty (`;`)" );
   }
   read_stmt( parse, reading );
   stmt->body = reading->node;
   stmt->else_body = NULL;
   if ( parse->tk == TK_ELSE ) {
      p_read_tk( parse );
      if ( parse->tk == TK_SEMICOLON ) {
         p_diag( parse, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &parse->tk_pos, "body of `else` is empty (`;`)" );
      }
      read_stmt( parse, reading );
      stmt->else_body = reading->node;
   }
   reading->node = &stmt->node;
}

void read_switch( struct parse* parse, struct stmt_reading* reading ) {
   p_read_tk( parse );
   struct switch_stmt* stmt = alloc_switch_stmt();
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading cond;
   p_init_expr_reading( &cond, false, false, false, true );
   p_read_expr( parse, &cond );
   stmt->cond = cond.output_node;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_stmt( parse, reading );
   stmt->body = reading->node;
   reading->node = &stmt->node;
}

struct switch_stmt* alloc_switch_stmt( void ) {
   struct switch_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_SWITCH;
   stmt->cond = NULL;
   stmt->case_head = NULL;
   stmt->case_default = NULL;
   stmt->jump_break = NULL;
   stmt->body = NULL;
   return stmt;
}

void read_while( struct parse* parse, struct stmt_reading* reading ) {
   struct while_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_WHILE;
   stmt->type = WHILE_WHILE;
   if ( parse->tk == TK_WHILE ) {
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_UNTIL );
      p_read_tk( parse );
      stmt->type = WHILE_UNTIL;
   }
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading cond;
   p_init_expr_reading( &cond, false, false, false, true );
   p_read_expr( parse, &cond );
   stmt->cond = cond.output_node;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_stmt( parse, reading );
   stmt->body = reading->node;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   reading->node = &stmt->node;
}

void read_do( struct parse* parse, struct stmt_reading* reading ) {
   p_read_tk( parse );
   struct while_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_WHILE;
   stmt->type = WHILE_DO_WHILE;
   read_stmt( parse, reading );
   stmt->body = reading->node;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   if ( parse->tk == TK_WHILE ) {
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_UNTIL );
      p_read_tk( parse );
      stmt->type = WHILE_DO_UNTIL;
   }
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading cond;
   p_init_expr_reading( &cond, false, false, false, true );
   p_read_expr( parse, &cond );
   stmt->cond = cond.output_node;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   reading->node = &stmt->node;
}

void read_for( struct parse* parse, struct stmt_reading* reading ) {
   p_read_tk( parse );
   struct for_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_FOR;
   list_init( &stmt->init );
   list_init( &stmt->post );
   stmt->cond = NULL;
   stmt->body = NULL;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   // Optional initialization:
   if ( parse->tk != TK_SEMICOLON ) {
      if ( p_is_dec( parse ) ) {
         struct dec dec;
         p_init_dec( &dec );
         dec.area = DEC_FOR;
         dec.name_offset = parse->ns->body;
         dec.vars = &stmt->init;
         p_read_dec( parse, &dec );
      }
      else {
         while ( true ) {
            struct expr_reading expr;
            p_init_expr_reading( &expr, false, false, false, true );
            p_read_expr( parse, &expr );
            list_append( &stmt->init, expr.output_node );
            if ( parse->tk == TK_COMMA ) {
               p_read_tk( parse );
            }
            else {
               break;
            }
         }
         p_test_tk( parse, TK_SEMICOLON );
         p_read_tk( parse );
      }
   }
   else {
      p_read_tk( parse );
   }
   // Optional condition:
   if ( parse->tk != TK_SEMICOLON ) {
      struct expr_reading cond;
      p_init_expr_reading( &cond, false, false, false, true );
      p_read_expr( parse, &cond );
      stmt->cond = cond.output_node;
      p_test_tk( parse, TK_SEMICOLON );
      p_read_tk( parse );
   }
   else {
      p_read_tk( parse );
   }
   // Optional post-expression:
   if ( parse->tk != TK_PAREN_R ) {
      while ( true ) {
         struct expr_reading expr;
         p_init_expr_reading( &expr, false, false, false, true );
         p_read_expr( parse, &expr );
         list_append( &stmt->post, expr.output_node );
         if ( parse->tk == TK_COMMA ) {
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_stmt( parse, reading );
   stmt->body = reading->node;
   reading->node = &stmt->node;
}

void read_foreach( struct parse* parse, struct stmt_reading* reading ) {
   struct foreach_stmt* stmt = alloc_foreach();
   p_test_tk( parse, TK_FOREACH );
   p_read_tk( parse );
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   p_read_foreach_item( parse, stmt );
   p_test_tk( parse, TK_IN );
   p_read_tk( parse );
   struct expr_reading collection;
   p_init_expr_reading( &collection, false, false, false, true );
   p_read_expr( parse, &collection );
   stmt->collection = collection.output_node;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_stmt( parse, reading );
   stmt->body = reading->node;
   reading->node = &stmt->node;
}

struct foreach_stmt* alloc_foreach( void ) {
   struct foreach_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_FOREACH;
   stmt->key = NULL;
   stmt->value = NULL;
   stmt->collection = NULL;
   stmt->body = NULL;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   return stmt;
}

void read_jump( struct parse* parse, struct stmt_reading* reading ) {
   struct jump* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_JUMP;
   stmt->type = JUMP_BREAK;
   stmt->next = NULL;
   stmt->point_jump = NULL;
   stmt->pos = parse->tk_pos;
   stmt->obj_pos = 0;
   if ( parse->tk == TK_CONTINUE ) {
      stmt->type = JUMP_CONTINUE;
   }
   p_read_tk( parse );
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   reading->node = &stmt->node;
}

void read_script_jump( struct parse* parse, struct stmt_reading* reading ) {
   struct script_jump* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_SCRIPT_JUMP;
   stmt->type = SCRIPT_JUMP_TERMINATE;
   stmt->pos = parse->tk_pos;
   if ( parse->tk == TK_RESTART ) {
      stmt->type = SCRIPT_JUMP_RESTART;
   }
   else if ( parse->tk == TK_SUSPEND ) {
      stmt->type = SCRIPT_JUMP_SUSPEND;
   }
   p_read_tk( parse );
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   reading->node = &stmt->node;
}

void read_return( struct parse* parse, struct stmt_reading* reading ) {
   struct return_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_RETURN;
   stmt->return_value = NULL;
   stmt->next = NULL;
   stmt->epilogue_jump = NULL;
   stmt->pos = parse->tk_pos;
   p_read_tk( parse );
   if ( parse->tk == TK_SEMICOLON ) {
      p_read_tk( parse );
   }
   else {
      read_packed_expr( parse, reading );
      stmt->return_value = reading->packed_expr;
   }
   reading->node = &stmt->node;
}

void read_goto( struct parse* parse, struct stmt_reading* reading ) {
   struct pos pos = parse->tk_pos;
   p_read_tk( parse );
   p_test_tk( parse, TK_ID );
   struct label* label = NULL;
   list_iter_t i;
   list_iter_init( &i, reading->labels );
   while ( ! list_end( &i ) ) {
      struct label* prev = list_data( &i );
      if ( strcmp( prev->name, parse->tk_text ) == 0 ) {
         label = prev;
         break;
      }
      list_next( &i );
   }
   if ( ! label ) {
      label = alloc_label( parse->tk_text, parse->tk_pos );
      list_append( reading->labels, label );
   }
   p_read_tk( parse );
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   struct goto_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_GOTO;
   stmt->label = label;
   stmt->next = label->users;
   label->users = stmt;
   stmt->obj_pos = 0;
   stmt->pos = pos;
   reading->node = &stmt->node;
}

void read_paltrans( struct parse* parse, struct stmt_reading* reading ) {
   p_read_tk( parse );
   struct paltrans* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_PALTRANS;
   stmt->ranges = NULL;
   stmt->ranges_tail = NULL;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   stmt->number = expr.output_node;
   while ( parse->tk == TK_COMMA ) {
      struct palrange* range = mem_alloc( sizeof( *range ) );
      range->next = NULL;
      p_read_tk( parse );
      p_init_expr_reading( &expr, false, false, false, true );
      p_read_expr( parse, &expr );
      range->begin = expr.output_node;
      p_test_tk( parse, TK_COLON );
      p_read_tk( parse );
      p_init_expr_reading( &expr, false, true, false, true );
      p_read_expr( parse, &expr );
      range->end = expr.output_node;
      p_test_tk( parse, TK_ASSIGN );
      p_read_tk( parse );
      if ( parse->tk == TK_BRACKET_L ) {
         read_palrange_rgb_field( parse, &range->value.rgb.red1,
            &range->value.rgb.green1, &range->value.rgb.blue1 );
         p_test_tk( parse, TK_COLON );
         p_read_tk( parse );
         read_palrange_rgb_field( parse, &range->value.rgb.red2,
            &range->value.rgb.green2, &range->value.rgb.blue2 );
         range->rgb = true;
      }
      else {
         p_init_expr_reading( &expr, false, false, false, true );
         p_read_expr( parse, &expr );
         range->value.ent.begin = expr.output_node;
         p_test_tk( parse, TK_COLON );
         p_read_tk( parse );
         p_init_expr_reading( &expr, false, false, false, true );
         p_read_expr( parse, &expr );
         range->value.ent.end = expr.output_node;
         range->rgb = false;
      }
      if ( stmt->ranges ) {
         stmt->ranges_tail->next = range;
         stmt->ranges_tail = range;
      }
      else {
         stmt->ranges = range;
         stmt->ranges_tail = range;
      }
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   reading->node = &stmt->node;
}

void read_palrange_rgb_field( struct parse* parse, struct expr** r,
   struct expr** g, struct expr** b ) {
   p_test_tk( parse, TK_BRACKET_L );
   p_read_tk( parse );
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   *r = expr.output_node;
   p_test_tk( parse, TK_COMMA );
   p_read_tk( parse );
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   *g = expr.output_node;
   p_test_tk( parse, TK_COMMA );
   p_read_tk( parse );
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   *b = expr.output_node;
   p_test_tk( parse, TK_BRACKET_R );
   p_read_tk( parse ); 
}

void read_expr_stmt( struct parse* parse, struct stmt_reading* reading ) {
   read_packed_expr( parse, reading );
   struct expr_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_EXPR_STMT;
   stmt->packed_expr = reading->packed_expr;
   reading->node = &stmt->node;
}

void read_packed_expr( struct parse* parse, struct stmt_reading* reading ) {
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, false );
   p_read_expr( parse, &expr );
   struct packed_expr* packed_expr = mem_alloc( sizeof( *packed_expr ) );
   packed_expr->expr = expr.output_node;
   packed_expr->msgbuild_func = NULL;
   if ( parse->tk == TK_MSGBUILD ) {
      read_onetime_msgbuild_func( parse, reading, packed_expr );
   }
   else if ( parse->tk == TK_SEMICOLON ) {
      p_read_tk( parse );
   }
   else {
      p_unexpect_diag( parse );
      p_unexpect_item( parse, NULL, TK_MSGBUILD );
      p_unexpect_last( parse, NULL, TK_SEMICOLON );
      p_bail( parse );
   }
   reading->packed_expr = packed_expr;
}

void read_onetime_msgbuild_func( struct parse* parse,
   struct stmt_reading* reading, struct packed_expr* packed_expr ) {
   p_test_tk( parse, TK_MSGBUILD );
   p_read_tk( parse );
   read_block( parse, reading );
   struct func_user* impl = t_alloc_func_user();
   impl->body = reading->block_node;
   impl->nested = true;
   struct func* func = t_alloc_func();
   func->object.pos = impl->body->pos;
   func->type = FUNC_USER;
   func->impl = impl;
   func->msgbuild = true;
   packed_expr->msgbuild_func = func;
}

void read_assert( struct parse* parse, struct stmt_reading* reading ) {
   struct assert* assert = alloc_assert( &parse->tk_pos );
   if ( parse->tk == TK_STATIC ) {
      assert->is_static = true;
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_ASSERT );
   p_read_tk( parse );
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading cond;
   p_init_expr_reading( &cond, false, false, false, true );
   p_read_expr( parse, &cond );
   assert->cond = cond.output_node;
   if ( parse->tk == TK_COMMA ) {
      p_read_tk( parse );
      p_test_tk( parse, TK_LIT_STRING );
      assert->custom_message = parse->tk_text;
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   reading->node = &assert->node;
   if ( ! assert->is_static ) {
      list_append( &parse->task->runtime_asserts, assert );
   }
}

struct assert* alloc_assert( struct pos* pos ) {
   struct assert* assert = mem_alloc( sizeof( *assert ) );
   assert->node.type = NODE_ASSERT;
   assert->next = NULL;
   assert->cond = NULL;
   assert->pos = *pos;
   assert->custom_message = NULL;
   assert->file = NULL;
   assert->message = NULL;
   assert->is_static = false;
   return assert;
}