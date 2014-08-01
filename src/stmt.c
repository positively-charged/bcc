#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "task.h"

static void read_block( struct task*, struct stmt_reading* );
static void read_case( struct task*, struct stmt_reading* );
static void read_default_case( struct task*, struct stmt_reading* );
static void read_label( struct task*, struct stmt_reading* );
static void read_stmt( struct task*, struct stmt_reading* );
static void read_if( struct task*, struct stmt_reading* );
static void read_switch( struct task*, struct stmt_reading* );
static void read_while( struct task*, struct stmt_reading* );
static void read_do( struct task*, struct stmt_reading* );
static void read_for( struct task*, struct stmt_reading* );
static void read_jump( struct task*, struct stmt_reading* );
static void read_script_jump( struct task*, struct stmt_reading* );
static void read_return( struct task*, struct stmt_reading* );
static void read_goto( struct task*, struct stmt_reading* );
static void read_paltrans( struct task*, struct stmt_reading* );
static void read_format_item( struct task*, struct stmt_reading* );
static void read_palrange_rgb_field( struct task*, struct expr**,
   struct expr**, struct expr** );
static void read_packed_expr( struct task*, struct stmt_reading* );
static struct label* alloc_label( char*, struct pos );
static void link_usable_strings( struct task* );
static void add_usable_string( struct indexed_string**,
   struct indexed_string**, struct indexed_string* );
static void alloc_string_indexes( struct task* );
static void test_block_item( struct task*, struct stmt_test*, struct node* );
static void test_case( struct task*, struct stmt_test*, struct case_label* );
static void test_default_case( struct task*, struct stmt_test*,
   struct case_label* );
static void test_label( struct task*, struct stmt_test*, struct label* );
static void test_if( struct task*, struct stmt_test*, struct if_stmt* );
static void test_switch( struct task*, struct stmt_test*,
   struct switch_stmt* );
static void test_while( struct task*, struct stmt_test*, struct while_stmt* );
static void test_for( struct task* task, struct stmt_test* test,
   struct for_stmt* );
static void test_jump( struct task*, struct stmt_test*, struct jump* );
static void test_script_jump( struct task*, struct stmt_test*,
   struct script_jump* );
static void test_return( struct task*, struct stmt_test*,
   struct return_stmt* );
static void test_goto( struct task*, struct stmt_test*, struct goto_stmt* );
static void test_paltrans( struct task*, struct stmt_test*, struct paltrans* );
static void test_format_item( struct task*, struct stmt_test*,
   struct format_item* );
static void test_packed_expr( struct task*, struct stmt_test*,
   struct packed_expr*, struct pos* );
static void test_goto_in_format_block( struct task*, struct list* );

void t_init( struct task* task, struct options* options, jmp_buf* bail ) {
   t_init_fields_token( task );
   t_init_fields_dec( task );
   task->options = options;
   list_init( &task->libraries );
   task->str_table.head = NULL;
   task->str_table.head_sorted = NULL;
   task->str_table.head_usable = NULL;
   task->str_table.tail = NULL;
   t_init_fields_chunk( task );
   t_init_fields_obj( task );
   list_init( &task->loaded_sources );
   list_init( &task->scripts );
   str_init( &task->tokens.text );
   task->tokens.peeked = 0;
   task->bail = bail;
   task->err_file = NULL;
}

void t_read( struct task* task ) {
   t_make_main_lib( task );
   t_load_main_source( task );
   t_read_tk( task );
   t_read_lib( task );
   link_usable_strings( task );
   alloc_string_indexes( task );
}

void t_print_name( struct name* name ) {
   struct str str;
   str_init( &str );
   t_copy_name( name, true, &str );
   printf( "%s\n", str.value );
   str_deinit( &str );
}

// Gets the top-most object associated with the name, and only retrieves the
// object if it can be used by the current module.
struct object* t_get_region_object( struct task* task, struct region* region,
   struct name* name ) {
   struct object* object = name->object;
   if ( ! object ) {
      return NULL;
   }
   // Find the top-most object.
   while ( object && object->next_scope ) {
      object = object->next_scope;
   }
   if ( object->depth != 0 ) {
      return NULL;
   }
   return object;
}

void t_init_stmt_reading( struct stmt_reading* reading, struct list* labels ) {
   reading->labels = labels;
   reading->node = NULL;
   reading->block_node = NULL;
}

void t_read_top_stmt( struct task* task, struct stmt_reading* reading,
   bool need_block ) {
   if ( need_block ) {
      read_block( task, reading );
   }
   else {
      read_stmt( task, reading );
   }
   // All goto statements need to refer to valid labels.
   if ( reading->node->type == NODE_BLOCK ) {
      list_iter_t i;
      list_iter_init( &i, reading->labels );
      while ( ! list_end( &i ) ) {
         struct label* label = list_data( &i );
         if ( ! label->defined ) {
            t_diag( task, DIAG_POS_ERR, &label->pos,
               "label `%s` not found", label->name );
            t_bail( task );
         }
         list_next( &i );
      }
   }
}

void read_block( struct task* task, struct stmt_reading* reading ) {
   t_test_tk( task, TK_BRACE_L );
   struct block* block = mem_alloc( sizeof( *block ) );
   block->node.type = NODE_BLOCK;
   list_init( &block->stmts );
   block->pos = task->tk_pos;
   t_read_tk( task );
   while ( true ) {
      if ( t_is_dec( task ) ) {
         struct dec dec;
         t_init_dec( &dec );
         dec.area = DEC_LOCAL;
         dec.name_offset = task->region->body;
         dec.vars = &block->stmts;
         t_read_dec( task, &dec );
      }
      else if ( task->tk == TK_CASE ) {
         read_case( task, reading );
         list_append( &block->stmts, reading->node );
      }
      else if ( task->tk == TK_DEFAULT ) {
         read_default_case( task, reading );
         list_append( &block->stmts, reading->node );
      }
      else if ( task->tk == TK_ID && t_peek( task ) == TK_COLON ) {
         read_label( task, reading );
         list_append( &block->stmts, reading->node );
      }
      else if ( task->tk == TK_IMPORT ) {
         t_read_import( task, &block->stmts );
      }
      else if ( task->tk == TK_BRACE_R ) {
         t_read_tk( task );
         break;
      }
      else {
         read_stmt( task, reading );
         if ( reading->node->type != NODE_NONE ) {
            list_append( &block->stmts, reading->node );
         }
      }
   }
   reading->node = &block->node;
   reading->block_node = block;
}

void read_case( struct task* task, struct stmt_reading* reading ) {
   struct case_label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_CASE;
   label->offset = 0;
   label->next = NULL;
   label->pos = task->tk_pos;
   t_read_tk( task );
   struct expr_reading number;
   t_init_expr_reading( &number, false, false, false, true );
   t_read_expr( task, &number );
   label->number = number.output_node;
   t_test_tk( task, TK_COLON );
   t_read_tk( task );
   reading->node = &label->node;
}

void read_default_case( struct task* task, struct stmt_reading* reading ) {
   struct case_label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_CASE_DEFAULT;
   label->pos = task->tk_pos;
   label->offset = 0;
   t_read_tk( task );
   t_test_tk( task, TK_COLON );
   t_read_tk( task );
   reading->node = &label->node;
}

void read_label( struct task* task, struct stmt_reading* reading ) {
   struct label* label = NULL;
   list_iter_t i;
   list_iter_init( &i, reading->labels );
   while ( ! list_end( &i ) ) {
      struct label* prev = list_data( &i );
      if ( strcmp( prev->name, task->tk_text ) == 0 ) {
         label = prev;
         break;
      }
      list_next( &i );
   }
   if ( label ) {
      if ( label->defined ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "duplicate label `%s`", task->tk_text );
         t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "label already found here" );
         t_bail( task );
      }
      else {
         label->defined = true;
         label->pos = task->tk_pos;
      }
   }
   else {
      label = alloc_label( task->tk_text, task->tk_pos );
      label->defined = true;
      list_append( reading->labels, label );
   }
   t_read_tk( task );
   t_read_tk( task );
   reading->node = &label->node;
}

struct label* alloc_label( char* name, struct pos pos ) {
   struct label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_GOTO_LABEL;
   label->name = name;
   label->defined = false;
   label->pos = pos;
   label->users = NULL;
   label->obj_pos = 0;
   label->format_block = NULL;
   return label;
}

void read_stmt( struct task* task, struct stmt_reading* reading ) {
   switch ( task->tk ) {
   case TK_BRACE_L:
      read_block( task, reading );
      break;
   case TK_IF:
      read_if( task, reading );
      break;
   case TK_SWITCH:
      read_switch( task, reading );
      break;
   case TK_WHILE:
   case TK_UNTIL:
      read_while( task, reading );
      break;
   case TK_DO:
      read_do( task, reading );
      break;
   case TK_FOR:
      read_for( task, reading );
      break;
   case TK_BREAK:
   case TK_CONTINUE:
      read_jump( task, reading );
      break;
   case TK_TERMINATE:
   case TK_RESTART:
   case TK_SUSPEND:
      read_script_jump( task, reading );
      break;
   case TK_RETURN:
      read_return( task, reading );
      break;
   case TK_GOTO:
      read_goto( task, reading );
      break;
   case TK_PALTRANS:
      read_paltrans( task, reading );
      break;
   case TK_SEMICOLON:
      {
         static struct node node = { NODE_NONE };
         reading->node = &node;
         t_read_tk( task );
      }
      break;
   default:
      // Format item in a format block:
      if ( task->tk == TK_ID && t_peek( task ) == TK_ASSIGN_COLON ) {
         read_format_item( task, reading );
      }
      else {
         read_packed_expr( task, reading );
      }
   }
}

void read_if( struct task* task, struct stmt_reading* reading ) {
   t_read_tk( task );
   struct if_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_IF;
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   struct expr_reading cond;
   t_init_expr_reading( &cond, false, false, false, true );
   t_read_expr( task, &cond );
   stmt->cond = cond.output_node;
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   // Warn when the body of an `if` statement is empty. It is assumed that a
   // semicolon is the empty statement.
   if ( task->tk == TK_SEMICOLON ) {
      t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &task->tk_pos, "body of `if` statement is empty (`;`)" );
   }
   read_stmt( task, reading );
   stmt->body = reading->node;
   stmt->else_body = NULL;
   if ( task->tk == TK_ELSE ) {
      t_read_tk( task );
      if ( task->tk == TK_SEMICOLON ) {
         t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &task->tk_pos, "body of `else` is empty (`;`)" );
      }
      read_stmt( task, reading );
      stmt->else_body = reading->node;
   }
   reading->node = &stmt->node;
}

void read_switch( struct task* task, struct stmt_reading* reading ) {
   t_read_tk( task );
   struct switch_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_SWITCH;
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   struct expr_reading cond;
   t_init_expr_reading( &cond, false, false, false, true );
   t_read_expr( task, &cond );
   stmt->cond = cond.output_node;
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   read_stmt( task, reading );
   stmt->body = reading->node;
   reading->node = &stmt->node;
}

void read_while( struct task* task, struct stmt_reading* reading ) {
   struct while_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_WHILE;
   stmt->type = WHILE_WHILE;
   if ( task->tk == TK_WHILE ) {
      t_read_tk( task );
   }
   else {
      t_test_tk( task, TK_UNTIL );
      t_read_tk( task );
      stmt->type = WHILE_UNTIL;
   }
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   struct expr_reading cond;
   t_init_expr_reading( &cond, false, false, false, true );
   t_read_expr( task, &cond );
   stmt->cond = cond.output_node;
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   read_stmt( task, reading );
   stmt->body = reading->node;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   reading->node = &stmt->node;
}

void read_do( struct task* task, struct stmt_reading* reading ) {
   t_read_tk( task );
   struct while_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_WHILE;
   stmt->type = WHILE_DO_WHILE;
   read_stmt( task, reading );
   stmt->body = reading->node;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   if ( task->tk == TK_WHILE ) {
      t_read_tk( task );
   }
   else {
      t_test_tk( task, TK_UNTIL );
      t_read_tk( task );
      stmt->type = WHILE_DO_UNTIL;
   }
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   struct expr_reading cond;
   t_init_expr_reading( &cond, false, false, false, true );
   t_read_expr( task, &cond );
   stmt->cond = cond.output_node;
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
   reading->node = &stmt->node;
}

void read_for( struct task* task, struct stmt_reading* reading ) {
   t_read_tk( task );
   struct for_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_FOR;
   list_init( &stmt->init );
   list_init( &stmt->post );
   stmt->cond = NULL;
   stmt->body = NULL;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   // Optional initialization:
   if ( task->tk != TK_SEMICOLON ) {
      if ( t_is_dec( task ) ) {
         struct dec dec;
         t_init_dec( &dec );
         dec.area = DEC_FOR;
         dec.name_offset = task->region->body;
         dec.vars = &stmt->init;
         t_read_dec( task, &dec );
      }
      else {
         while ( true ) {
            struct expr_reading expr;
            t_init_expr_reading( &expr, false, false, false, true );
            t_read_expr( task, &expr );
            list_append( &stmt->init, expr.output_node );
            if ( task->tk == TK_COMMA ) {
               t_read_tk( task );
            }
            else {
               break;
            }
         }
         t_test_tk( task, TK_SEMICOLON );
         t_read_tk( task );
      }
   }
   else {
      t_read_tk( task );
   }
   // Optional condition:
   if ( task->tk != TK_SEMICOLON ) {
      struct expr_reading cond;
      t_init_expr_reading( &cond, false, false, false, true );
      t_read_expr( task, &cond );
      stmt->cond = cond.output_node;
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
   }
   else {
      t_read_tk( task );
   }
   // Optional post-expression:
   if ( task->tk != TK_PAREN_R ) {
      while ( true ) {
         struct expr_reading expr;
         t_init_expr_reading( &expr, false, false, false, true );
         t_read_expr( task, &expr );
         list_append( &stmt->post, expr.output_node );
         if ( task->tk == TK_COMMA ) {
            t_read_tk( task );
         }
         else {
            break;
         }
      }
   }
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   read_stmt( task, reading );
   stmt->body = reading->node;
   reading->node = &stmt->node;
}

void read_jump( struct task* task, struct stmt_reading* reading ) {
   struct jump* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_JUMP;
   stmt->type = JUMP_BREAK;
   stmt->next = NULL;
   stmt->pos = task->tk_pos;
   stmt->obj_pos = 0;
   if ( task->tk == TK_CONTINUE ) {
      stmt->type = JUMP_CONTINUE;
   }
   t_read_tk( task );
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
   reading->node = &stmt->node;
}

void read_script_jump( struct task* task, struct stmt_reading* reading ) {
   struct script_jump* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_SCRIPT_JUMP;
   stmt->type = SCRIPT_JUMP_TERMINATE;
   stmt->pos = task->tk_pos;
   if ( task->tk == TK_RESTART ) {
      stmt->type = SCRIPT_JUMP_RESTART;
   }
   else if ( task->tk == TK_SUSPEND ) {
      stmt->type = SCRIPT_JUMP_SUSPEND;
   }
   t_read_tk( task );
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
   reading->node = &stmt->node;
}

void read_return( struct task* task, struct stmt_reading* reading ) {
   struct return_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_RETURN;
   stmt->return_value = NULL;
   stmt->pos = task->tk_pos;
   t_read_tk( task );
   if ( task->tk == TK_SEMICOLON ) {
      t_read_tk( task );
   }
   else {
      read_packed_expr( task, reading );
      stmt->return_value = ( struct packed_expr* ) reading->node;
   }
   reading->node = &stmt->node;
}

void read_goto( struct task* task, struct stmt_reading* reading ) {
   struct pos pos = task->tk_pos;
   t_read_tk( task );
   t_test_tk( task, TK_ID );
   struct label* label = NULL;
   list_iter_t i;
   list_iter_init( &i, reading->labels );
   while ( ! list_end( &i ) ) {
      struct label* prev = list_data( &i );
      if ( strcmp( prev->name, task->tk_text ) == 0 ) {
         label = prev;
         break;
      }
      list_next( &i );
   }
   if ( ! label ) {
      label = alloc_label( task->tk_text, task->tk_pos );
      list_append( reading->labels, label );
   }
   t_read_tk( task );
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
   struct goto_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_GOTO;
   stmt->label = label;
   stmt->next = label->users;
   label->users = stmt;
   stmt->obj_pos = 0;
   stmt->format_block = NULL;
   stmt->pos = pos;
   reading->node = &stmt->node;
}

void read_paltrans( struct task* task, struct stmt_reading* reading ) {
   t_read_tk( task );
   struct paltrans* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_PALTRANS;
   stmt->ranges = NULL;
   stmt->ranges_tail = NULL;
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   struct expr_reading expr;
   t_init_expr_reading( &expr, false, false, false, true );
   t_read_expr( task, &expr );
   stmt->number = expr.output_node;
   while ( task->tk == TK_COMMA ) {
      struct palrange* range = mem_alloc( sizeof( *range ) );
      range->next = NULL;
      t_read_tk( task );
      t_init_expr_reading( &expr, false, false, false, true );
      t_read_expr( task, &expr );
      range->begin = expr.output_node;
      t_test_tk( task, TK_COLON );
      t_read_tk( task );
      t_init_expr_reading( &expr, false, true, false, true );
      t_read_expr( task, &expr );
      range->end = expr.output_node;
      t_test_tk( task, TK_ASSIGN );
      t_read_tk( task );
      if ( task->tk == TK_BRACKET_L ) {
         read_palrange_rgb_field( task, &range->value.rgb.red1,
            &range->value.rgb.green1, &range->value.rgb.blue1 );
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
         read_palrange_rgb_field( task, &range->value.rgb.red2,
            &range->value.rgb.green2, &range->value.rgb.blue2 );
         range->rgb = true;
      }
      else {
         t_init_expr_reading( &expr, false, false, false, true );
         t_read_expr( task, &expr );
         range->value.ent.begin = expr.output_node;
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
         t_init_expr_reading( &expr, false, false, false, true );
         t_read_expr( task, &expr );
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
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
   reading->node = &stmt->node;
}

void read_palrange_rgb_field( struct task* task, struct expr** r,
   struct expr** g, struct expr** b ) {
   t_test_tk( task, TK_BRACKET_L );
   t_read_tk( task );
   struct expr_reading expr;
   t_init_expr_reading( &expr, false, false, false, true );
   t_read_expr( task, &expr );
   *r = expr.output_node;
   t_test_tk( task, TK_COMMA );
   t_read_tk( task );
   t_init_expr_reading( &expr, false, false, false, true );
   t_read_expr( task, &expr );
   *g = expr.output_node;
   t_test_tk( task, TK_COMMA );
   t_read_tk( task );
   t_init_expr_reading( &expr, false, false, false, true );
   t_read_expr( task, &expr );
   *b = expr.output_node;
   t_test_tk( task, TK_BRACKET_R );
   t_read_tk( task ); 
}

void read_format_item( struct task* task, struct stmt_reading* reading ) {
   struct format_item* item = t_read_format_item( task, false );
   reading->node = &item->node;
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
}

void read_packed_expr( struct task* task, struct stmt_reading* reading ) {
   struct expr_reading expr;
   t_init_expr_reading( &expr, false, false, false, false );
   t_read_expr( task, &expr );
   struct packed_expr* packed = mem_alloc( sizeof( *packed ) );
   packed->node.type = NODE_PACKED_EXPR;
   packed->expr = expr.output_node;
   packed->block = NULL;
   // With format block.
   if ( task->tk == TK_ASSIGN_COLON ) {
      t_read_tk( task );
      read_block( task, reading );
      packed->block = reading->block_node;
   }
   else {
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
   }
   reading->node = &packed->node;
}

void link_usable_strings( struct task* task ) {
   // Link together the strings that have the potential to be used. To reduce
   // the number of unused indexes that have to be published, we want used
   // strings to appear first. This is done to try and prevent the case where
   // you have a string with index, say, 20 that is used, and all strings
   // before are not, but you still have to publish the other 20 indexes
   // because it is required by the format of the STRL chunk.
   struct indexed_string* head = NULL;
   struct indexed_string* tail;
   // Strings in main file of the library appear first.
   struct indexed_string* string = task->str_table.head;
   while ( string ) {
      if ( ! string->imported && string->in_main_file ) {
         add_usable_string( &head, &tail, string );
      }
      string = string->next;
   }
   // Strings part of the library but found in a secondary file appear next.
   string = task->str_table.head;
   while ( string ) {
      if ( ! string->imported && ! string->in_main_file ) {
         add_usable_string( &head, &tail, string );
      }
      string = string->next;
   }
   // Strings in an imported library follow. Only a string found in a constant
   // is useful.
   string = task->str_table.head;
   while ( string ) {
      if ( string->imported && string->in_constant ) {
         add_usable_string( &head, &tail, string );
      }
      string = string->next;
   }
   task->str_table.head_usable = head;
}

void add_usable_string( struct indexed_string** head,
   struct indexed_string** tail, struct indexed_string* string ) {
   if ( *head ) {
      ( *tail )->next_usable = string;
   }
   else {
      *head = string;
   }
   *tail = string;
}

void alloc_string_indexes( struct task* task ) {
   int index = 0;
   struct indexed_string* string = task->str_table.head_usable;
   while ( string ) {
      string->index = index;
      ++index;
      string = string->next_usable;
   }
}

void t_init_stmt_test( struct stmt_test* test, struct stmt_test* parent ) {
   test->parent = parent;
   test->func = NULL;
   test->labels = NULL;
   test->format_block = NULL;
   test->case_head = NULL;
   test->case_default = NULL;
   test->jump_break = NULL;
   test->jump_continue = NULL;
   test->import = NULL;
   test->in_loop = false;
   test->in_switch = false;
   test->in_script = false;
   test->manual_scope = false;
}

void t_test_block( struct task* task, struct stmt_test* test,
   struct block* block ) {
   if ( ! test->manual_scope ) {
      t_add_scope( task );
   }
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      struct stmt_test nested;
      t_init_stmt_test( &nested, test );
      test_block_item( task, &nested, list_data( &i ) );
      list_next( &i );
   }
   if ( ! test->manual_scope ) {
      t_pop_scope( task );
   }
   if ( ! test->parent ) {
      test_goto_in_format_block( task, test->labels );
   }
}

void test_block_item( struct task* task, struct stmt_test* test,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_CONSTANT:
      t_test_constant( task, ( struct constant* ) node, true );
      break;
   case NODE_CONSTANT_SET:
      t_test_constant_set( task, ( struct constant_set* ) node, true );
      break;
   case NODE_VAR:
      t_test_local_var( task, ( struct var* ) node );
      break;
   case NODE_TYPE:
      t_test_type( task, ( struct type* ) node, true );
      break;
   case NODE_CASE:
      test_case( task, test, ( struct case_label* ) node );
      break;
   case NODE_CASE_DEFAULT:
      test_default_case( task, test, ( struct case_label* ) node );
      break;
   case NODE_GOTO_LABEL:
      test_label( task, test, ( struct label* ) node );
      break;
   case NODE_IMPORT:
      t_import( task, ( struct import* ) node );
      break;
   default:
      t_test_stmt( task, test, node );
   }
}

void test_case( struct task* task, struct stmt_test* test,
   struct case_label* label ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_switch ) {
      target = target->parent;
   }
   if ( ! target ) {
      t_diag( task, DIAG_POS_ERR, &label->pos,
         "case outside switch statement" );
      t_bail( task );
   }
   struct expr_test expr;
   t_init_expr_test( &expr );
   expr.undef_err = true;
   t_test_expr( task, &expr, label->number );
   if ( ! label->number->folded ) {
      t_diag( task, DIAG_POS_ERR, &expr.pos, "case value not constant" );
      t_bail( task );
   }
   struct case_label* prev = NULL;
   struct case_label* curr = target->case_head;
   while ( curr && curr->number->value < label->number->value ) {
      prev = curr;
      curr = curr->next;
   }
   if ( curr && curr->number->value == label->number->value ) {
      t_diag( task, DIAG_POS_ERR, &label->pos, "duplicate case" );
      t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &curr->pos,
         "case with value %d found here", curr->number->value );
      t_bail( task );
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

void test_default_case( struct task* task, struct stmt_test* test,
   struct case_label* label ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_switch ) {
      target = target->parent;
   }
   if ( ! target ) {
      t_diag( task, DIAG_POS_ERR, &label->pos,
         "default outside switch statement" );
      t_bail( task );
   }
   if ( target->case_default ) {
      t_diag( task, DIAG_POS_ERR, &label->pos, "duplicate default case" );
      t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &target->case_default->pos,
         "default case found here" );
      t_bail( task );
   }
   target->case_default = label;
}

void test_label( struct task* task, struct stmt_test* test,
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

void t_test_stmt( struct task* task, struct stmt_test* test,
   struct node* node ) {
   if ( node->type == NODE_BLOCK ) {
      t_test_block( task, test, ( struct block* ) node );
   }
   else if ( node->type == NODE_IF ) {
      test_if( task, test, ( struct if_stmt* ) node );
   }
   else if ( node->type == NODE_SWITCH ) {
      test_switch( task, test, ( struct switch_stmt* ) node );
   }
   else if ( node->type == NODE_WHILE ) {
      test_while( task, test, ( struct while_stmt* ) node );
   }
   else if ( node->type == NODE_FOR ) {
      test_for( task, test, ( struct for_stmt* ) node );
   }
   else if ( node->type == NODE_JUMP ) {
      test_jump( task, test, ( struct jump* ) node );
   }
   else if ( node->type == NODE_SCRIPT_JUMP ) {
      test_script_jump( task, test, ( struct script_jump* ) node );
   }
   else if ( node->type == NODE_RETURN ) {
      test_return( task, test, ( struct return_stmt* ) node );
   }
   else if ( node->type == NODE_GOTO ) {
      test_goto( task, test, ( struct goto_stmt* ) node );
   }
   else if ( node->type == NODE_PALTRANS ) {
      test_paltrans( task, test, ( struct paltrans* ) node );
   }
   else if ( node->type == NODE_FORMAT_ITEM ) {
      test_format_item( task, test, ( struct format_item* ) node );
   }
   else if ( node->type == NODE_PACKED_EXPR ) {
      test_packed_expr( task, test, ( struct packed_expr* ) node, NULL );
   }
}

void test_if( struct task* task, struct stmt_test* test,
   struct if_stmt* stmt ) {
   struct expr_test expr;
   t_init_expr_test( &expr );
   expr.undef_err = true;
   expr.suggest_paren_assign = true;
   t_test_expr( task, &expr, stmt->cond );
   struct stmt_test body;
   t_init_stmt_test( &body, test );
   t_test_stmt( task, &body, stmt->body );
   if ( stmt->else_body ) {
      t_init_stmt_test( &body, test );
      t_test_stmt( task, &body, stmt->else_body );
   }
}

void test_switch( struct task* task, struct stmt_test* test,
   struct switch_stmt* stmt ) {
   struct expr_test expr;
   t_init_expr_test( &expr );
   expr.undef_err = true;
   t_test_expr( task, &expr, stmt->cond );
   struct stmt_test body;
   t_init_stmt_test( &body, test );
   body.in_switch = true;
   t_test_stmt( task, &body, stmt->body );
   stmt->case_head = body.case_head;
   stmt->case_default = body.case_default;
   stmt->jump_break = body.jump_break;
}

void test_while( struct task* task, struct stmt_test* test,
   struct while_stmt* stmt ) {
   if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      expr.suggest_paren_assign = true;
      t_test_expr( task, &expr, stmt->cond );
   }
   struct stmt_test body;
   t_init_stmt_test( &body, test );
   body.in_loop = true;
   t_test_stmt( task, &body, stmt->body );
   stmt->jump_break = body.jump_break;
   stmt->jump_continue = body.jump_continue;
   if ( stmt->type == WHILE_DO_WHILE || stmt->type == WHILE_DO_UNTIL ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      expr.suggest_paren_assign = true;
      t_test_expr( task, &expr, stmt->cond );
   }
}

void test_for( struct task* task, struct stmt_test* test,
   struct for_stmt* stmt ) {
   t_add_scope( task );
   // Initialization.
   list_iter_t i;
   list_iter_init( &i, &stmt->init );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      if ( node->type == NODE_EXPR ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = true;
         expr.need_value = false;
         t_test_expr( task, &expr, ( struct expr* ) node );
      }
      else {
         t_test_local_var( task, ( struct var* ) node );
      }
      list_next( &i );
   }
   // Condition.
   if ( stmt->cond ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      expr.suggest_paren_assign = true;
      t_test_expr( task, &expr, stmt->cond );
   }
   // Post expressions.
   list_iter_init( &i, &stmt->post );
   while ( ! list_end( &i ) ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      expr.need_value = false;
      t_test_expr( task, &expr, list_data( &i ) );
      list_next( &i );
   }
   struct stmt_test body;
   t_init_stmt_test( &body, test );
   body.in_loop = true;
   t_test_stmt( task, &body, stmt->body );
   stmt->jump_break = body.jump_break;
   stmt->jump_continue = body.jump_continue;
   t_pop_scope( task );
}

void test_jump( struct task* task, struct stmt_test* test,
   struct jump* stmt ) {
   if ( stmt->type == JUMP_BREAK ) {
      struct stmt_test* target = test;
      while ( target && ! target->in_loop && ! target->in_switch ) {
         target = target->parent;
      }
      if ( ! target ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
            "break outside loop or switch" );
         t_bail( task );
      }
      stmt->next = target->jump_break;
      target->jump_break = stmt;
      // Jumping out of a format block is not allowed.
      struct stmt_test* finish = target;
      target = test;
      while ( target != finish ) {
         if ( target->format_block ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &stmt->pos,
               "leaving format block with a break statement" );
            t_bail( task );
         }
         target = target->parent;
      }
   }
   else {
      struct stmt_test* target = test;
      while ( target && ! target->in_loop ) {
         target = target->parent;
      }
      if ( ! target ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
            "continue outside loop" );
         t_bail( task );
      }
      stmt->next = target->jump_continue;
      target->jump_continue = stmt;
      struct stmt_test* finish = target;
      target = test;
      while ( target != finish ) {
         if ( target->format_block ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &stmt->pos,
               "leaving format block with a continue statement" );
            t_bail( task );
         }
         target = target->parent;
      }
   }
}

void test_script_jump( struct task* task, struct stmt_test* test,
   struct script_jump* stmt ) {
   static const char* names[] = { "terminate", "restart", "suspend" };
   STATIC_ASSERT( ARRAY_SIZE( names ) == SCRIPT_JUMP_TOTAL );
   struct stmt_test* target = test;
   while ( target && ! target->in_script ) {
      target = target->parent;
   }
   if ( ! target ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
         "`%s` outside script", names[ stmt->type ] );
      t_bail( task );
   }
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &stmt->pos,
            "`%s` inside format block", names[ stmt->type ] );
         t_bail( task );
      }
      target = target->parent;
   }
}

void test_return( struct task* task, struct stmt_test* test,
   struct return_stmt* stmt ) {
   struct stmt_test* target = test;
   while ( target && ! target->func ) {
      target = target->parent;
   }
   if ( ! target ) {
      t_diag( task, DIAG_POS_ERR, &stmt->pos,
         "return statement outside function" );
      t_bail( task );
   }
   if ( stmt->return_value ) {
      struct pos pos;
      test_packed_expr( task, test, stmt->return_value, &pos );
      if ( ! target->func->return_type ) {
         t_diag( task, DIAG_POS_ERR, &pos,
            "returning value in void function" );
         t_bail( task );
      }
   }
   else {
      if ( target->func->return_type ) {
         t_diag( task, DIAG_POS_ERR, &stmt->pos, "missing return value" );
         t_bail( task );
      }
   }
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         t_diag( task, DIAG_POS_ERR, &stmt->pos,
            "leaving format block with a return statement" );
         t_bail( task );
      }
      target = target->parent;
   }
}

void test_goto( struct task* task, struct stmt_test* test,
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

void test_paltrans( struct task* task, struct stmt_test* test,
   struct paltrans* stmt ) {
   struct expr_test expr;
   t_init_expr_test( &expr );
   expr.undef_err = true;
   t_test_expr( task, &expr, stmt->number );
   struct palrange* range = stmt->ranges;
   while ( range ) {
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, range->begin );
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, range->end );
      if ( range->rgb ) {
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->value.rgb.red1 );
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->value.rgb.green1 );
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->value.rgb.blue1 );
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->value.rgb.red2 );
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->value.rgb.green2 );
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->value.rgb.blue2 );
      }
      else {
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->value.ent.begin );
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->value.ent.end );
      }
      range = range->next;
   }
}

void test_format_item( struct task* task, struct stmt_test* test,
   struct format_item* item ) {
   t_test_format_item( task, item, test, NULL, task->region->body, NULL );
   struct stmt_test* target = test;
   while ( target && ! target->format_block ) {
      target = target->parent;
   }
   if ( ! target ) {
      t_diag( task, DIAG_POS_ERR, &item->pos,
         "format item outside format block" );
      t_bail( task );
   }
}

void test_packed_expr( struct task* task, struct stmt_test* test,
   struct packed_expr* packed, struct pos* expr_pos ) {
   // Test expression.
   struct expr_test expr_test;
   t_init_expr_test( &expr_test );
   expr_test.stmt_test = test;
   expr_test.undef_err = true;
   expr_test.need_value = false;
   expr_test.format_block = packed->block;
   t_test_expr( task, &expr_test, packed->expr );
   if ( expr_pos ) {
      *expr_pos = expr_test.pos;
   }
   // Test format block.
   if ( packed->block ) {
      struct stmt_test nested;
      t_init_stmt_test( &nested, test );
      nested.format_block = packed->block;
      t_test_block( task, &nested, nested.format_block );
      if ( ! expr_test.format_block_usage ) {
         t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &packed->block->pos, "unused format block" );
      }
   }
}

void test_goto_in_format_block( struct task* task, struct list* labels ) {
   list_iter_t i;
   list_iter_init( &i, labels );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      if ( label->format_block ) {
         if ( label->users ) {
            struct goto_stmt* stmt = label->users;
            while ( stmt ) {
               if ( stmt->format_block != label->format_block ) {
                  t_diag( task, DIAG_POS_ERR, &stmt->pos,
                     "entering format block with a goto statement" );
                  t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                     "point of entry is here" ); 
                  t_bail( task );
               }
               stmt = stmt->next;
            }
         }
         else {
            // If a label is unused, the user might have used the syntax of a
            // label to create a format-item.
            t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &label->pos, "unused label in format block" );
         }
      }
      else {
         struct goto_stmt* stmt = label->users;
         while ( stmt ) {
            if ( stmt->format_block ) {
               t_diag( task, DIAG_POS_ERR, &stmt->pos,
                  "leaving format block with a goto statement" );
               t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                  "destination of goto statement is here" );
               t_bail( task );
            }
            stmt = stmt->next;
         }
      }
      list_next( &i );
   }
}