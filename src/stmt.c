#include <string.h>

#include "task.h"

#include <stdio.h>

#define MAX_MODULE_NAME_LENGTH 8

static struct module* new_module( void );
static void read_dirc( struct task*, struct pos* );
static void read_stmt( struct task*, struct stmt_read* );
static void read_palrange_rgb_field( struct task*, struct expr**,
   struct expr**, struct expr** );
static struct label* new_label( char*, struct pos );
static void test_block_item( struct task*, struct stmt_test*, struct node* );
static void test_stmt( struct task*, struct stmt_test*, struct node* );
static void test_goto_in_format_block( struct task*, struct list* );

void t_init( struct task* task, struct options* options ) {
   task->options = options;
   task->module = new_module();
   task->str_table.head = NULL;
   task->str_table.tail = NULL;
   task->str_table.size = 0;
   task->importing = false;
   task->format = FORMAT_BIG_E;
   t_init_fields_tk( task );
   t_init_fields_dec( task );
   t_init_fields_chunk( task );
   t_init_fields_obj( task );
}

struct module* new_module( void ) {
   struct module* module = mem_alloc( sizeof( *module ) );
   str_init( &module->name );
   list_init( &module->vars );
   list_init( &module->funcs );
   list_init( &module->scripts );
   list_init( &module->imports );
   return module;
}

void t_read_module( struct task* task ) {
   bool got_header = false;
   if ( task->tk == TK_HASH ) {
      struct pos pos = task->tk_pos;
      t_read_tk( task );
      if ( task->tk == TK_ID && strcmp( task->tk_text, "library" ) == 0 ) {
         t_read_tk( task );
         t_test_tk( task, TK_LIT_STRING );
         if ( ! task->tk_length ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &task->tk_pos, "library name is blank" );
            bail();
         }
         else if ( task->tk_length > MAX_MODULE_NAME_LENGTH ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &task->tk_pos, "library name too long" );
            diag( DIAG_FILE, &task->tk_pos,
               "library name can be up to %d characters long",
               MAX_MODULE_NAME_LENGTH );
            bail();
         }
         else {
            str_append( &task->module->name, task->tk_text );
            got_header = true;
            t_read_tk( task );
         }
      }
      else {
         read_dirc( task, &pos );
      }
   }
   // Header required for an imported module.
   if ( task->importing && ! got_header ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing library name (#library \"<name>\")" );
      bail();
   }
   while ( true ) {
      if ( t_is_dec( task ) ) {
         struct dec dec;
         t_init_dec( &dec );
         dec.name_offset = task->ns->body;
         t_read_dec( task, &dec );
      }
      else if ( task->tk == TK_SCRIPT ) {
         if ( task->importing ) {
            t_skip_block( task );
         }
         else {
            t_read_script( task );
         }
      }
      else if ( task->tk == TK_NAMESPACE ) {
         t_read_namespace( task );
      }
      else if ( task->tk == TK_USING ) {
         if ( task->importing ) {
            t_skip_to_tk( task, TK_SEMICOLON );
            t_read_tk( task );
         }
         else {
            t_read_using( task, NULL );
         }
      }
      else if ( task->tk == TK_HASH ) {
         struct pos pos = task->tk_pos;
         t_read_tk( task );
         read_dirc( task, &pos );
      }
      else if ( task->tk == TK_SEMICOLON ) {
         t_read_tk( task );
      }
      else if ( task->tk == TK_END ) {
         bool more = false;
         t_unload_file( task, &more );
         if ( ! more || task->importing ) {
            break;
         }
      }
      else {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "unexpected token" );
         bail();
      }
   }
}

void read_dirc( struct task* task, struct pos* pos ) {
   t_test_tk( task, TK_ID );
   if ( strcmp( task->tk_text, "define" ) == 0 ||
      strcmp( task->tk_text, "libdefine" ) == 0 ) {
      t_read_define( task );
   }
   else if ( strcmp( task->tk_text, "include" ) == 0 ||
      strcmp( task->tk_text, "import" ) == 0 ) {
      bool import = false;
      if ( task->tk_text[ 1 ] == 'm' ) {
         import = true;
      }
      t_read_tk( task );
      t_test_tk( task, TK_LIT_STRING );
      // Don't load files when in an imported module.
      if ( task->importing ) {
         t_read_tk( task );
         return;
      }
      if ( ! t_load_file( task, task->tk_text ) ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &task->tk_pos, "failed to load file: %s", task->tk_text );
         bail();
      }
      t_read_tk( task );
      if ( import ) {
         struct module* parent = task->module;
         struct ns* ns = task->ns;
         task->module = new_module();
         task->ns = task->ns_global;
         task->importing = true;
         t_read_module( task );
         task->importing = false;
         list_append( &parent->imports, task->module );
         task->module = parent;
         task->ns = ns;
      }
   }
   else if ( strcmp( task->tk_text, "library" ) == 0 ) {
      t_read_tk( task );
      t_test_tk( task, TK_LIT_STRING );
      t_read_tk( task );
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
         "#library directive after other code" );
      diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
         "#library directive must appear at the very top, "
         "before any other code" );
      bail();
   }
   else if ( strcmp( task->tk_text, "encryptstrings" ) == 0 ) {
      t_read_tk( task );
      if ( ! task->importing ) {
         task->options->encrypt_str = true;
      }
   }
   else if (
      // NOTE: Not sure what these two are. 
      strcmp( task->tk_text, "wadauthor" ) == 0 ||
      strcmp( task->tk_text, "nowadauthor" ) == 0 ||
      // For now, the output format will be selected automatically.
      strcmp( task->tk_text, "nocompact" ) == 0 ) {
      diag( DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
         "directive `%s` not supported", task->tk_text );
      t_read_tk( task );
   }
   else {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
         "unknown directive '%s'", task->tk_text );
      bail();
   }
}

void t_init_stmt_read( struct stmt_read* read ) {
   read->block = NULL;
   read->node = NULL;
   read->labels = NULL;
   read->depth = 0;
}

void t_read_block( struct task* task, struct stmt_read* read ) {
   t_test_tk( task, TK_BRACE_L );
   t_read_tk( task );
   ++read->depth;
   struct block* block = mem_alloc( sizeof( *block ) );
   block->node.type = NODE_BLOCK;
   list_init( &block->stmts );
   while ( true ) {
      if ( t_is_dec( task ) ) {
         struct dec dec;
         t_init_dec( &dec );
         dec.area = DEC_LOCAL;
         dec.name_offset = task->ns->body;
         dec.vars = &block->stmts;
         t_read_dec( task, &dec );
      }
      else if ( task->tk == TK_CASE ) {
         struct case_label* label = mem_alloc( sizeof( *label ) );
         label->node.type = NODE_CASE;
         label->offset = 0;
         label->next = NULL;
         label->pos = task->tk_pos;
         t_read_tk( task );
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         label->expr = expr.node;
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
         list_append( &block->stmts, label );
      }
      else if ( task->tk == TK_DEFAULT ) {
         struct case_label* label = mem_alloc( sizeof( *label ) );
         label->node.type = NODE_CASE_DEFAULT;
         label->pos = task->tk_pos;
         label->offset = 0;
         t_read_tk( task );
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
         list_append( &block->stmts, label );
      }
      // goto label:
      else if ( task->tk == TK_ID && t_peek_usable_ch( task ) == ':' ) {
         struct label* label = NULL;
         list_iter_t i;
         list_iter_init( &i, read->labels );
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
               diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &task->tk_pos, "duplicate label '%s'", task->tk_text );
               diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                  "label '%s' is found here", task->tk_text );
               bail();
            }
            else {
               label->defined = true;
               label->pos = task->tk_pos;
            }
         }
         else {
            label = new_label( task->tk_text, task->tk_pos );
            label->defined = true;
            list_append( read->labels, label );
         }
         t_read_tk( task );
         t_read_tk( task );
         list_append( &block->stmts, label );
      }
      // Format item:
      else if ( task->tk == TK_ID && t_coming_up_insertion_token( task ) ) {
         while ( true ) {
            list_append( &block->stmts,
               t_read_format_item( task, TK_INSERTION ) );
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
      else if ( task->tk == TK_USING ) {
         struct node* local = NULL;
         t_read_using( task, &local );
         list_append( &block->stmts, local );
      }
      else if ( task->tk == TK_BRACE_R ) {
         t_read_tk( task );
         break;
      }
      else {
         read_stmt( task, read );
         if ( read->node->type != NODE_NONE ) {
            list_append( &block->stmts, read->node );
         }
      }
   }
   read->node = ( struct node* ) block;
   read->block = block;
   --read->depth;
   if ( read->depth == 0 ) {
      // All goto statements need to refer to valid labels.
      list_iter_t i;
      list_iter_init( &i, read->labels );
      while ( ! list_end( &i ) ) {
         struct label* label = list_data( &i );
         if ( ! label->defined ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
               "label '%s' not found", label->name );
            bail();
         }
         list_next( &i );
      }
   }
}

void read_stmt( struct task* task, struct stmt_read* read ) {
   if ( task->tk == TK_BRACE_L ) {
      t_read_block( task, read );
   }
   else if ( task->tk == TK_IF ) {
      t_read_tk( task );
      struct if_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_IF;
      t_test_tk( task, TK_PAREN_L );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->expr = expr.node;
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      read_stmt( task, read );
      stmt->body = read->node;
      stmt->else_body = NULL;
      if ( task->tk == TK_ELSE ) {
         t_read_tk( task );
         read_stmt( task, read );
         stmt->else_body = read->node;
      }
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_SWITCH ) {
      t_read_tk( task );
      struct switch_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_SWITCH;
      t_test_tk( task, TK_PAREN_L );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->expr = expr.node;
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      read_stmt( task, read );
      stmt->body = read->node;
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_WHILE || task->tk == TK_UNTIL ) {
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
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->expr = expr.node;
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      read_stmt( task, read );
      stmt->body = read->node;
      stmt->jump_break = NULL;
      stmt->jump_continue = NULL;
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_DO ) {
      t_read_tk( task );
      struct while_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_WHILE;
      stmt->type = WHILE_DO_WHILE;
      read_stmt( task, read );
      stmt->body = read->node;
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
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->expr = expr.node;
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_FOR ) {
      t_read_tk( task );
      struct for_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_FOR;
      stmt->init = NULL;
      list_init( &stmt->vars );
      stmt->expr = NULL;
      stmt->post = NULL;
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
            dec.name_offset = task->ns->body;
            dec.vars = &stmt->vars;
            t_read_dec( task, &dec );
         }
         else {
            struct expr_link* tail = NULL;
            while ( true ) {
               struct expr_link* link = mem_alloc( sizeof( *link ) );
               link->node.type = NODE_EXPR_LINK;
               link->next = NULL;
               struct read_expr expr;
               t_init_read_expr( &expr );
               t_read_expr( task, &expr );
               link->expr = expr.node;
               if ( tail ) {
                  tail->next = link;
               }
               else {
                  stmt->init = link;
               }
               tail = link;
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
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         stmt->expr = expr.node;
         t_test_tk( task, TK_SEMICOLON );
         t_read_tk( task );
      }
      else {
         t_read_tk( task );
      }
      // Optional post-expression:
      if ( task->tk != TK_PAREN_R ) {
         struct expr_link* tail = NULL;
         while ( true ) {
            struct expr_link* link = mem_alloc( sizeof( *link ) );
            link->node.type = NODE_EXPR_LINK;
            link->next = NULL;
            struct read_expr expr;
            t_init_read_expr( &expr );
            t_read_expr( task, &expr );
            link->expr = expr.node;
            if ( tail ) {
               tail->next = link;
            }
            else {
               stmt->post = link;
            }
            tail = link;
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
      read_stmt( task, read );
      stmt->body = read->node;
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_BREAK || task->tk == TK_CONTINUE ) {
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
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_TERMINATE || task->tk == TK_RESTART ||
      task->tk == TK_SUSPEND ) {
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
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_RETURN ) {
      struct return_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_RETURN;
      stmt->expr = NULL;
      stmt->pos = task->tk_pos;
      t_read_tk( task );
      if ( task->tk != TK_SEMICOLON ) {
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         stmt->expr = expr.node;
      }
      t_test_tk( task, TK_SEMICOLON );
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_GOTO ) {
      struct pos pos = task->tk_pos;
      t_read_tk( task );
      t_test_tk( task, TK_ID );
      struct label* label = NULL;
      list_iter_t i;
      list_iter_init( &i, read->labels );
      while ( ! list_end( &i ) ) {
         struct label* prev = list_data( &i );
         if ( strcmp( prev->name, task->tk_text ) == 0 ) {
            label = prev;
            break;
         }
         list_next( &i );
      }
      if ( ! label ) {
         label = new_label( task->tk_text, task->tk_pos );
         list_append( read->labels, label );
      }
      t_read_tk( task );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      struct goto_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_GOTO;
      stmt->label = label;
      stmt->next = label->stmts;
      label->stmts = stmt;
      stmt->obj_pos = 0;
      stmt->format_block = NULL;
      stmt->pos = pos;
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_PALTRANS ) {
      t_read_tk( task );
      struct paltrans* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_PALTRANS;
      stmt->ranges = NULL;
      stmt->ranges_tail = NULL;
      t_test_tk( task, TK_PAREN_L );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->number = expr.node;
      while ( task->tk == TK_COMMA ) {
         struct palrange* range = mem_alloc( sizeof( *range ) );
         range->next = NULL;
         t_read_tk( task );
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         range->begin = expr.node;
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
         t_init_read_expr( &expr );
         expr.skip_assign = true;
         t_read_expr( task, &expr );
         range->end = expr.node;
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
            t_init_read_expr( &expr );
            t_read_expr( task, &expr );
            range->value.ent.begin = expr.node;
            t_test_tk( task, TK_COLON );
            t_read_tk( task );
            t_init_read_expr( &expr );
            t_read_expr( task, &expr );
            range->value.ent.end = expr.node;
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
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_SEMICOLON ) {
      static struct node empty = { NODE_NONE };
      read->node = &empty;
      t_read_tk( task );
   }
   else {
      struct read_expr expr;
      t_init_read_expr( &expr );
      expr.stmt_read = read;
      t_read_expr( task, &expr );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      read->node = ( struct node* ) expr.node;
   }
}

struct label* new_label( char* name, struct pos pos ) {
   struct label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_GOTO_LABEL;
   label->name = name;
   label->defined = false;
   label->pos = pos;
   label->stmts = NULL;
   label->obj_pos = 0;
   label->format_block = NULL;
   return label;
}

void read_palrange_rgb_field( struct task* task, struct expr** r,
   struct expr** g, struct expr** b ) {
   t_test_tk( task, TK_BRACKET_L );
   t_read_tk( task );
   struct read_expr expr;
   t_init_read_expr( &expr );
   t_read_expr( task, &expr );
   *r = expr.node;
   t_test_tk( task, TK_COMMA );
   t_read_tk( task );
   t_init_read_expr( &expr );
   t_read_expr( task, &expr );
   *g = expr.node;
   t_test_tk( task, TK_COMMA );
   t_read_tk( task );
   t_init_read_expr( &expr );
   t_read_expr( task, &expr );
   *b = expr.node;
   t_test_tk( task, TK_BRACKET_R );
   t_read_tk( task ); 
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
   test->in_loop = false;
   test->in_switch = false;
   test->in_script = false;
   test->manual_scope = false;
}

void t_skip_semicolon( struct task* task ) {
   while ( task->tk != TK_SEMICOLON ) {
      t_read_tk( task );
   }
   t_read_tk( task );
}

void t_skip_past_tk( struct task* task, enum tk needed ) {
   while ( true ) {
      if ( task->tk == needed ) {
         t_read_tk( task );
         break;
      }
      else if ( task->tk == TK_END ) {
         bail();
      }
      else {
         t_read_tk( task );
      }
   }
}

void t_skip_block( struct task* task ) {
   while ( true ) {
      if ( task->tk == TK_BRACE_L ) {
         t_read_tk( task );
         break;
      }
      else {
         t_read_tk( task );
      }
   }
   int depth = 1;
   while ( true ) {
      if ( task->tk == TK_BRACE_L ) {
         ++depth;
         t_read_tk( task );
      }
      else if ( task->tk == TK_BRACE_R ) {
         --depth;
         t_read_tk( task );
         if ( ! depth ) {
            break;
         }
      }
      else if ( task->tk == TK_END ) {
         bail();
      }
      else {
         t_read_tk( task );
      }
   }
}

void t_skip_to_tk( struct task* task, enum tk tk ) {
   while ( true ) {
      if ( task->tk == tk ) {
         break;
      }
      t_read_tk( task );
   }
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
   if ( node->type == NODE_CONSTANT ) {
      t_test_constant( task, ( struct constant* ) node, true, true );
   }
   else if ( node->type == NODE_CONSTANT_SET ) {
      t_test_constant_set( task, ( struct constant_set* ) node, true,
         true );
   }
   else if ( node->type == NODE_TYPE ) {
      t_test_type( task, ( struct type* ) node, true );
   }
   else if ( node->type == NODE_VAR ) {
      t_test_local_var( task, ( struct var* ) node );
   }
   else if ( node->type == NODE_CASE ) {
      struct case_label* label = ( struct case_label* ) node;
      struct stmt_test* target = test;
      while ( target && ! target->in_switch ) {
         target = target->parent;
      }
      if ( ! target ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "case outside switch statement" );
         bail();
      }
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, label->expr );
      if ( ! label->expr->folded ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "case value not constant" );
         bail();
      }
      struct case_label* prev = NULL;
      struct case_label* curr = target->case_head;
      while ( curr && curr->expr->value < label->expr->value ) {
         prev = curr;
         curr = curr->next;
      }
      if ( curr && curr->expr->value == label->expr->value ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "duplicate case" );
         diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &curr->pos,
            "case with value %d is found here", label->expr->value );
         bail();
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
   else if ( node->type == NODE_CASE_DEFAULT ) {
      struct case_label* label = ( struct case_label* ) node;
      struct stmt_test* target = test;
      while ( target && ! target->in_switch ) {
         target = target->parent;
      }
      if ( ! target ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "default outside switch statement" );
         bail();
      }
      else if ( target->case_default ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "duplicate default case" );
         diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &target->case_default->pos,
            "default case is found here" );
         bail();
      }
      target->case_default = label;
   }
   else if ( node->type == NODE_GOTO_LABEL ) {
      struct stmt_test* target = test;
      while ( target ) {
         if ( target->format_block ) {
            struct label* label = ( struct label* ) node;
            label->format_block = target->format_block;
            break;
         }
         target = target->parent;
      }
   }
   else if ( node->type == NODE_FORMAT_ITEM ) {
      t_test_format_item( task, ( struct format_item* ) node, test,
         task->ns->body );
      struct stmt_test* target = test;
      while ( target && ! target->format_block ) {
         target = target->parent;
      }
      if ( ! target ) {
         struct format_item* item = ( struct format_item* ) node;
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &item->pos,
            "format item outside format block" );
         bail();
      }
   }
   else if ( node->type == NODE_SHORTCUT ) {
      t_test_shortcut( task, ( struct shortcut* ) node, true );
   }
   else if ( node->type == NODE_NAMESPACE_LINK ) {
      t_test_ns_link( task, ( struct ns_link* ) node, true );
   }
   else {
      test_stmt( task, test, node );
   }
}

void test_stmt( struct task* task, struct stmt_test* test,
   struct node* node ) {
   if ( node->type == NODE_BLOCK ) {
      t_test_block( task, test, ( struct block* ) node );
   }
   else if ( node->type == NODE_IF ) {
      struct if_stmt* stmt = ( struct if_stmt* ) node;
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, stmt->expr );
      struct stmt_test body;
      t_init_stmt_test( &body, test );
      test_stmt( task, &body, stmt->body );
      if ( stmt->else_body ) {
         t_init_stmt_test( &body, test );
         test_stmt( task, &body, stmt->else_body );
      }
   }
   else if ( node->type == NODE_SWITCH ) {
      struct switch_stmt* stmt = ( struct switch_stmt* ) node;
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, stmt->expr );
      struct stmt_test body;
      t_init_stmt_test( &body, test );
      body.in_switch = true;
      test_stmt( task, &body, stmt->body );
      stmt->case_head = body.case_head;
      stmt->case_default = body.case_default;
      stmt->jump_break = body.jump_break;
   }
   else if ( node->type == NODE_WHILE ) {
      struct while_stmt* stmt = ( struct while_stmt* ) node;
      if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, stmt->expr );
      }
      struct stmt_test body;
      t_init_stmt_test( &body, test );
      body.in_loop = true;
      test_stmt( task, &body, stmt->body );
      stmt->jump_break = body.jump_break;
      stmt->jump_continue = body.jump_continue;
      if ( stmt->type == WHILE_DO_WHILE || stmt->type == WHILE_DO_UNTIL ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, stmt->expr );
      }
   }
   else if ( node->type == NODE_FOR ) {
      struct for_stmt* stmt = ( struct for_stmt* ) node;
      t_add_scope( task );
      if ( stmt->init ) {
         struct expr_link* link = stmt->init;
         while ( link ) {
            struct expr_test expr;
            t_init_expr_test( &expr );
            expr.undef_err = true;
            expr.needed = false;
            t_test_expr( task, &expr, link->expr );
            link = link->next;
         }
      }
      else if ( list_size( &stmt->vars ) ) {
         list_iter_t i;
         list_iter_init( &i, &stmt->vars );
         while ( ! list_end( &i ) ) {
            struct node* node = list_data( &i );
            if ( node->type == NODE_VAR ) {
               t_test_local_var( task, ( struct var* ) node );
            }
            list_next( &i );
         }
      }
      if ( stmt->expr ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, stmt->expr );
      }
      if ( stmt->post ) {
         struct expr_link* link = stmt->post;
         while ( link ) {
            struct expr_test expr;
            t_init_expr_test( &expr );
            expr.undef_err = true;
            expr.needed = false;
            t_test_expr( task, &expr, link->expr );
            link = link->next;
         }
      }
      struct stmt_test body;
      t_init_stmt_test( &body, test );
      body.in_loop = true;
      test_stmt( task, &body, stmt->body );
      stmt->jump_break = body.jump_break;
      stmt->jump_continue = body.jump_continue;
      t_pop_scope( task );
   }
   else if ( node->type == NODE_JUMP ) {
      struct jump* stmt = ( struct jump* ) node;
      if ( stmt->type == JUMP_BREAK ) {
         struct stmt_test* target = test;
         while ( target && ! target->in_loop && ! target->in_switch ) {
            target = target->parent;
         }
         if ( ! target ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
               "break outside loop or switch" );
            bail();
         }
         stmt->next = target->jump_break;
         target->jump_break = stmt;
         // Jumping out of a format block is not allowed.
         struct stmt_test* finish = target;
         target = test;
         while ( target != finish ) {
            if ( target->format_block ) {
               diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &stmt->pos,
                  "leaving format block with a break statement" );
               bail();
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
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
               "continue outside loop" );
            bail();
         }
         stmt->next = target->jump_continue;
         target->jump_continue = stmt;
         struct stmt_test* finish = target;
         target = test;
         while ( target != finish ) {
            if ( target->format_block ) {
               diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &stmt->pos,
                  "leaving format block with a continue statement" );
               bail();
            }
            target = target->parent;
         }
      }
   }
   else if ( node->type == NODE_SCRIPT_JUMP ) {
      static const char* names[] = { "terminate", "restart", "suspend" };
      STATIC_ASSERT( ARRAY_SIZE( names ) == SCRIPT_JUMP_TOTAL );
      struct stmt_test* target = test;
      while ( target && ! target->in_script ) {
         target = target->parent;
      }
      if ( ! target ) {
         struct script_jump* stmt = ( struct script_jump* ) node;
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
            "%s statement outside script", names[ stmt->type ] );
         bail();
      }
      struct stmt_test* finish = target;
      target = test;
      while ( target != finish ) {
         if ( target->format_block ) {
            struct script_jump* stmt = ( struct script_jump* ) node;
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &stmt->pos,
               "%s statement inside format block", names[ stmt->type ] );
            bail();
         }
         target = target->parent;
      }
   }
   else if ( node->type == NODE_RETURN ) {
      struct return_stmt* stmt = ( struct return_stmt* ) node;
      struct stmt_test* target = test;
      while ( target && ! target->func ) {
         target = target->parent;
      }
      if ( ! target ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos, 
            "return statement outside function" );
         bail();
      }
      if ( stmt->expr ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, stmt->expr );
         if ( ! target->func->value ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "returning value in void function" );
            bail();
         }
      }
      else if ( target->func->value ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
            "missing return value" );
         bail();
      }
      struct stmt_test* finish = target;
      target = test;
      while ( target != finish ) {
         if ( target->format_block ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &stmt->pos,
               "leaving format block with a return statement" );
            bail();
         }
         target = target->parent;
      }
   }
   else if ( node->type == NODE_GOTO ) {
      struct stmt_test* target = test;
      while ( target ) {
         if ( target->format_block ) {
            struct goto_stmt* stmt = ( struct goto_stmt* ) node;
            stmt->format_block = target->format_block;
            break;
         }
         target = target->parent;
      }
   }
   else if ( node->type == NODE_PALTRANS ) {
      struct paltrans* stmt = ( struct paltrans* ) node;
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
   else if ( node->type == NODE_EXPR ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.stmt_test = test;
      expr.undef_err = true;
      expr.needed = false;
      t_test_expr( task, &expr, ( struct expr* ) node );
   }
}

void test_goto_in_format_block( struct task* task, struct list* labels ) {
   list_iter_t i;
   list_iter_init( &i, labels );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      if ( label->format_block ) {
         struct goto_stmt* stmt = label->stmts;
         while ( stmt ) {
            if ( stmt->format_block != label->format_block ) {
               diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &stmt->pos,
                  "entering format block with a goto statement" );
               diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                  "point of entry is here" ); 
               bail();
            }
            stmt = stmt->next;
         }
      }
      else {
         struct goto_stmt* stmt = label->stmts;
         while ( stmt ) {
            if ( stmt->format_block ) {
               diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &stmt->pos,
                  "leaving format block with a goto statement" );
               diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                  "destination of goto statement is here" );
               bail();
            }
            stmt = stmt->next;
         }
      }
      list_next( &i );
   }
}