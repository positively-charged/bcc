#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "phase.h"

static void read_block( struct parse* parse, struct stmt_reading* reading );
static struct block* alloc_block( void );
static void read_implicit_block( struct parse* parse,
   struct stmt_reading* reading );
static void read_block_item( struct parse* parse, struct stmt_reading* reading,
   struct block* block );
static void read_case( struct parse* parse, struct stmt_reading* );
static void read_default_case( struct parse* parse, struct stmt_reading* );
static void read_label( struct parse* parse, struct stmt_reading* );
static void read_stmt( struct parse* parse, struct stmt_reading* reading,
   struct block* block );
static void read_if( struct parse* parse, struct stmt_reading* );
static void read_cond( struct parse* parse, struct cond* cond );
static void init_heavy_cond( struct heavy_cond* cond );
static void read_heavy_cond( struct parse* parse, struct heavy_cond* cond );
static void init_cond( struct cond* cond );
static void read_switch( struct parse* parse, struct stmt_reading* );
static struct switch_stmt* alloc_switch_stmt( void );
static void read_while( struct parse* parse, struct stmt_reading* reading );
static struct while_stmt* alloc_while( void );
static void read_do( struct parse* parse, struct stmt_reading* reading );
static struct do_stmt* alloc_do( void );
static void read_for( struct parse* parse, struct stmt_reading* );
static void read_for_init( struct parse* parse, struct for_stmt* stmt );
static void read_for_post( struct parse* parse, struct for_stmt* stmt );
static void read_foreach( struct parse* parse, struct stmt_reading* reading );
static struct foreach_stmt* alloc_foreach( void );
static void read_jump( struct parse* parse, struct stmt_reading* );
static void read_script_jump( struct parse* parse, struct stmt_reading* );
static void read_return( struct parse* parse, struct stmt_reading* );
static void read_goto( struct parse* parse, struct stmt_reading* );
static struct goto_stmt* alloc_goto_stmt( struct pos* pos );
static void read_paltrans( struct parse* parse, struct stmt_reading* );
static void read_palrange_rgb( struct parse* parse, struct palrange* range );
static void read_palrange_rgb_field( struct parse* parse, struct expr**,
   struct expr**, struct expr** );
static void read_palrange_colorisation( struct parse* parse,
   struct palrange* range );
static void read_palrange_tint( struct parse* parse,
   struct palrange* range );
static void read_buildmsg_stmt( struct parse* parse,
   struct stmt_reading* reading );
static struct buildmsg* read_buildmsg( struct parse* parse,
   struct stmt_reading* reading );
static void read_expr_stmt( struct parse* parse,
   struct stmt_reading* reading );
static struct label* alloc_label( const char* name, struct pos* pos );
static void read_assert( struct parse* parse, struct stmt_reading* reading );
static struct assert* alloc_assert( struct pos* pos );

void t_print_name( struct name* name ) {
   struct str str;
   str_init( &str );
   t_copy_full_name( name, NAMESEPARATOR_INTERNAL, &str );
   printf( "%s\n", str.value );
   str_deinit( &str );
}

void p_init_stmt_reading( struct stmt_reading* reading, struct list* labels ) {
   reading->labels = labels;
   reading->node = NULL;
   reading->block_node = NULL;
}

void p_read_top_block( struct parse* parse, struct stmt_reading* reading,
   bool explicit_block ) {
   if ( explicit_block ) {
      read_block( parse, reading );
   }
   else {
      read_implicit_block( parse, reading );
   }
}

static void read_block( struct parse* parse, struct stmt_reading* reading ) {
   p_test_tk( parse, TK_BRACE_L );
   struct block* block = alloc_block();
   block->pos = parse->tk_pos;
   p_read_tk( parse );
   while ( parse->tk != TK_BRACE_R ) {
      read_block_item( parse, reading, block );
   }
   p_test_tk( parse, TK_BRACE_R );
   p_read_tk( parse );
   reading->node = &block->node;
   reading->block_node = block;
}

static struct block* alloc_block( void ) {
   struct block* block = mem_alloc( sizeof( *block ) );
   block->node.type = NODE_BLOCK;
   list_init( &block->stmts );
   return block;
}

// ACS compatability: In ACS, block-items are parsed as statements. As a
// result, you can have weird code like the following, and it does exist in the
// wild: if ( 0 ) int test = 123; switch ( 1 ) case 0:
// To support such code, we implicitly create a block.
static void read_implicit_block( struct parse* parse,
   struct stmt_reading* reading ) {
   // No need for an implicit block when one is explicitly specified.
   if ( parse->tk == TK_BRACE_L ) {
      read_block( parse, reading );
   }
   else {
      struct block* block = alloc_block();
      block->pos = parse->tk_pos;
      read_block_item( parse, reading, block );
      reading->node = &block->node;
      reading->block_node = block;
   }
}

static void read_block_item( struct parse* parse, struct stmt_reading* reading,
   struct block* block ) {
   reading->node = NULL;
   read_stmt( parse, reading, block );
   if ( reading->node ) {
      list_append( &block->stmts, reading->node );
   }
}

// In certain programming languages, constructs like declarations and case
// labels can only occur in block statements. In ACS, these are parsed as a
// statement, so you can get weird code like the following:
//    if ( 0 ) int a = 123;
//    switch ( 0 ) default:
// For the sake of being compatible with ACS, we do the same in BCS.
static void read_stmt( struct parse* parse, struct stmt_reading* reading,
   struct block* block ) {
   // using directive.
   if ( parse->tk == TK_USING ||
      ( parse->tk == TK_LET && p_peek( parse ) == TK_USING ) ) {
      p_read_local_using( parse, &block->stmts );
      return;
   }
   // Declaration.
   if ( p_is_local_dec( parse ) ) {
      struct dec dec;
      p_init_dec( &dec );
      dec.area = DEC_LOCAL;
      dec.name_offset = parse->ns->body;
      dec.vars = &block->stmts;
      p_read_local_dec( parse, &dec );
      return;
   }
   // goto label.
   if ( parse->lang == LANG_BCS && parse->tk == TK_ID &&
      p_peek( parse ) == TK_COLON ) {
      read_label( parse, reading );
      return;
   }
   // assert/static-assert.
   if ( parse->tk == TK_ASSERT ||
      ( parse->tk == TK_STATIC && p_peek( parse ) == TK_ASSERT ) ) {
      read_assert( parse, reading );
      return;
   }
   // Determine which statement to read.
   enum tk stmt = TK_NONE;
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      switch ( parse->tk ) {
      case TK_CASE:
      case TK_DEFAULT:
      case TK_BRACE_L:
      case TK_IF:
      case TK_SWITCH:
      case TK_WHILE:
      case TK_UNTIL:
      case TK_DO:
      case TK_BREAK:
      case TK_CONTINUE:
      case TK_TERMINATE:
      case TK_RESTART:
      case TK_SUSPEND:
      case TK_SEMICOLON:
      // ACS.
      case TK_RETURN:
      case TK_PALTRANS:
         stmt = parse->tk;
         break;
      case TK_FOR:
         // for-loop not available in ACS95.
         if ( parse->lang == LANG_ACS ) {
            stmt = TK_FOR;
         }
         break;
      default:
         break;
      }
      break;
   case LANG_BCS:
      stmt = parse->tk;
      break;
   default:
      P_UNREACHABLE( parse );
   }
   // Read statement.
   switch ( stmt ) {
   case TK_BRACE_L:
      read_block( parse, reading );
      break;
   case TK_IF:
      read_if( parse, reading );
      break;
   case TK_SWITCH:
      read_switch( parse, reading );
      break;
   case TK_CASE:
      read_case( parse, reading );
      break;
   case TK_DEFAULT:
      read_default_case( parse, reading );
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
      p_read_tk( parse );
      break;
   case TK_BUILDMSG:
      read_buildmsg_stmt( parse, reading );
      break;
   default:
      read_expr_stmt( parse, reading );
      break;
   }
}

static void read_case( struct parse* parse, struct stmt_reading* reading ) {
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

static void read_default_case( struct parse* parse,
   struct stmt_reading* reading ) {
   struct case_label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_CASE_DEFAULT;
   label->pos = parse->tk_pos;
   label->offset = 0;
   p_read_tk( parse );
   p_test_tk( parse, TK_COLON );
   p_read_tk( parse );
   reading->node = &label->node;
}

static void read_label( struct parse* parse, struct stmt_reading* reading ) {
   struct label* label = alloc_label( parse->tk_text, &parse->tk_pos );
   list_append( reading->labels, label );
   p_test_tk( parse, TK_ID );
   p_read_tk( parse );
   p_test_tk( parse, TK_COLON );
   p_read_tk( parse );
   reading->node = &label->node;
}

static struct label* alloc_label( const char* name, struct pos* pos ) {
   struct label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_GOTO_LABEL;
   label->pos = *pos;
   label->name = name;
   label->buildmsg = NULL;
   label->point = NULL;
   list_init( &label->users );
   return label;
}

static void read_if( struct parse* parse, struct stmt_reading* reading ) {
   p_read_tk( parse );
   struct if_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_IF;
   init_heavy_cond( &stmt->cond );
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   read_heavy_cond( parse, &stmt->cond );
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   // Warn when the body of an `if` statement is empty. It is assumed that a
   // semicolon is the empty statement.
   if ( parse->tk == TK_SEMICOLON ) {
      p_diag( parse, DIAG_WARN | DIAG_POS, &parse->tk_pos,
         "body of `if` statement is empty (`;`)" );
   }
   read_implicit_block( parse, reading );
   stmt->body = reading->node;
   stmt->else_body = NULL;
   if ( parse->tk == TK_ELSE ) {
      p_read_tk( parse );
      if ( parse->tk == TK_SEMICOLON ) {
         p_diag( parse, DIAG_WARN | DIAG_POS, &parse->tk_pos,
            "body of `else` is empty (`;`)" );
      }
      read_implicit_block( parse, reading );
      stmt->else_body = reading->node;
   }
   reading->node = &stmt->node;
}

static void init_cond( struct cond* cond ) {
   cond->u.node = NULL;
}

static void read_cond( struct parse* parse, struct cond* cond ) {
   if ( parse->lang == LANG_BCS && p_is_local_dec( parse ) ) {
      cond->u.var = p_read_cond_var( parse );
   }
   else {
      struct expr_reading expr;
      p_init_expr_reading( &expr, false, false, false, true );
      p_read_expr( parse, &expr );
      cond->u.expr = expr.output_node;
   }
}

static void init_heavy_cond( struct heavy_cond* cond ) {
   cond->var = NULL;
   cond->expr = NULL;
}

static void read_heavy_cond( struct parse* parse, struct heavy_cond* cond ) {
   bool read_expr = false;
   if ( parse->lang == LANG_BCS && p_is_local_dec( parse ) ) {
      cond->var = p_read_cond_var( parse );
      if ( parse->tk == TK_SEMICOLON ) {
         p_read_tk( parse );
         read_expr = true;
      }
   }
   else {
      read_expr = true;
   }
   if ( read_expr ) {
      struct expr_reading expr;
      p_init_expr_reading( &expr, false, false, false, true );
      p_read_expr( parse, &expr );
      cond->expr = expr.output_node;
   }
}

static void read_switch( struct parse* parse, struct stmt_reading* reading ) {
   p_read_tk( parse );
   struct switch_stmt* stmt = alloc_switch_stmt();
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   read_heavy_cond( parse, &stmt->cond );
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_implicit_block( parse, reading );
   stmt->body = reading->node;
   reading->node = &stmt->node;
}

static struct switch_stmt* alloc_switch_stmt( void ) {
   struct switch_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_SWITCH;
   init_heavy_cond( &stmt->cond );
   stmt->case_head = NULL;
   stmt->case_default = NULL;
   stmt->jump_break = NULL;
   stmt->body = NULL;
   return stmt;
}

static void read_while( struct parse* parse, struct stmt_reading* reading ) {
   struct while_stmt* stmt = alloc_while();
   if ( parse->tk == TK_WHILE ) {
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_UNTIL );
      p_read_tk( parse );
      stmt->until = true;
   }
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   read_cond( parse, &stmt->cond );
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_implicit_block( parse, reading );
   stmt->body = reading->block_node;
   reading->node = &stmt->node;
}

static struct while_stmt* alloc_while( void ) {
   struct while_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_WHILE;
   init_cond( &stmt->cond );
   stmt->body = NULL;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   stmt->until = false;
   return stmt;
}

static void read_do( struct parse* parse, struct stmt_reading* reading ) {
   p_test_tk( parse, TK_DO );
   p_read_tk( parse );
   struct do_stmt* stmt = alloc_do();
   read_implicit_block( parse, reading );
   stmt->body = reading->block_node;
   if ( parse->tk == TK_WHILE ) {
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_UNTIL );
      p_read_tk( parse );
      stmt->until = true;
   }
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   stmt->cond = expr.output_node;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   reading->node = &stmt->node;
}

static struct do_stmt* alloc_do( void ) {
   struct do_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_DO;
   stmt->cond = NULL;
   stmt->body = NULL;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   stmt->until = false;
   return stmt;
}

static void read_for( struct parse* parse, struct stmt_reading* reading ) {
   p_test_tk( parse, TK_FOR );
   p_read_tk( parse );
   struct for_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_FOR;
   list_init( &stmt->init );
   list_init( &stmt->post );
   init_cond( &stmt->cond );
   stmt->body = NULL;
   stmt->jump_break = NULL;
   stmt->jump_continue = NULL;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   // Optional initialization.
   if ( parse->tk != TK_SEMICOLON ) {
      read_for_init( parse, stmt );
   }
   else {
      p_read_tk( parse );
   }
   // In BCS, the condition is optional.
   if ( ! ( parse->lang == LANG_BCS && parse->tk == TK_SEMICOLON ) ) {
      read_cond( parse, &stmt->cond );
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   // In BCS, the post-expression is optional.
   if ( ! ( parse->lang == LANG_BCS && parse->tk == TK_PAREN_R ) ) {
      read_for_post( parse, stmt );
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_implicit_block( parse, reading );
   stmt->body = reading->node;
   reading->node = &stmt->node;
}

static void read_for_init( struct parse* parse, struct for_stmt* stmt ) {
   if ( p_is_local_dec( parse ) ) {
      struct dec dec;
      p_init_for_dec( parse, &dec, &stmt->init );
      p_read_for_var( parse, &dec );
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

static void read_for_post( struct parse* parse, struct for_stmt* stmt ) {
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

static void read_foreach( struct parse* parse, struct stmt_reading* reading ) {
   struct foreach_stmt* stmt = alloc_foreach();
   p_test_tk( parse, TK_FOREACH );
   p_read_tk( parse );
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   p_read_foreach_item( parse, stmt );
   struct expr_reading collection;
   p_init_expr_reading( &collection, false, false, false, true );
   p_read_expr( parse, &collection );
   stmt->collection = collection.output_node;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_implicit_block( parse, reading );
   stmt->body = reading->node;
   reading->node = &stmt->node;
}

static struct foreach_stmt* alloc_foreach( void ) {
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

static void read_jump( struct parse* parse, struct stmt_reading* reading ) {
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

static void read_script_jump( struct parse* parse,
   struct stmt_reading* reading ) {
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

static void read_return( struct parse* parse, struct stmt_reading* reading ) {
   p_test_tk( parse, TK_RETURN );
   struct return_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_RETURN;
   stmt->return_value = NULL;
   stmt->buildmsg = NULL;
   stmt->next = NULL;
   stmt->epilogue_jump = NULL;
   stmt->pos = parse->tk_pos;
   p_read_tk( parse );
   if ( parse->tk == TK_BUILDMSG ) {
      stmt->buildmsg = read_buildmsg( parse, reading );
      stmt->return_value = stmt->buildmsg->expr;
   }
   else {
      if ( parse->tk != TK_SEMICOLON ) {
         struct expr_reading expr;
         p_init_expr_reading( &expr, false, false, false, true );
         p_read_expr( parse, &expr );
         stmt->return_value = expr.output_node;
      }
      p_test_tk( parse, TK_SEMICOLON );
      p_read_tk( parse );
   }
   reading->node = &stmt->node;
}

static void read_goto( struct parse* parse, struct stmt_reading* reading ) {
   struct goto_stmt* stmt = alloc_goto_stmt( &parse->tk_pos );
   p_test_tk( parse, TK_GOTO );
   p_read_tk( parse );
   p_test_tk( parse, TK_ID );
   stmt->label_name = parse->tk_text;
   stmt->label_name_pos = parse->tk_pos;
   p_read_tk( parse );
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   reading->node = &stmt->node;
}

static struct goto_stmt* alloc_goto_stmt( struct pos* pos ) {
   struct goto_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_GOTO;
   stmt->obj_pos = 0;
   stmt->label = NULL;
   stmt->label_name = NULL;
   stmt->pos = *pos;
   t_init_pos_id( &stmt->label_name_pos, INTERNALFILE_COMPILER );
   stmt->buildmsg = NULL;
   return stmt;
}

static void read_paltrans( struct parse* parse,
   struct stmt_reading* reading ) {
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
      range->rgb = false;
      range->saturated = false;
      range->colorisation = false;
      range->tint = false;
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
      if ( parse->tk == TK_MOD ) {
         p_read_tk( parse );
         read_palrange_rgb( parse, range );
         range->saturated = true;
      }
      else if ( parse->tk == TK_HASH ) {
         read_palrange_colorisation( parse, range );
      }
      else if ( parse->tk == TK_AT ) {
         read_palrange_tint( parse, range );
      }
      else if ( parse->tk == TK_BRACKET_L ) {
         read_palrange_rgb( parse, range );
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

static void read_palrange_rgb( struct parse* parse, struct palrange* range ) {
   read_palrange_rgb_field( parse, &range->value.rgb.red1,
      &range->value.rgb.green1, &range->value.rgb.blue1 );
   p_test_tk( parse, TK_COLON );
   p_read_tk( parse );
   read_palrange_rgb_field( parse, &range->value.rgb.red2,
      &range->value.rgb.green2, &range->value.rgb.blue2 );
}

static void read_palrange_rgb_field( struct parse* parse, struct expr** r,
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

static void read_palrange_colorisation( struct parse* parse,
   struct palrange* range ) {
   p_test_tk( parse, TK_HASH );
   p_read_tk( parse );
   read_palrange_rgb_field( parse,
      &range->value.colorisation.red,
      &range->value.colorisation.green,
      &range->value.colorisation.blue );
   range->colorisation = true;
}

static void read_palrange_tint( struct parse* parse, struct palrange* range ) {
   p_test_tk( parse, TK_AT );
   p_read_tk( parse );
   // NOTE: The syntax for this range is problematic. Here, we parse an
   // expression, followed by a left bracket. But the left bracket can be
   // considered a part of the expression, since it marks the start of a
   // subscript operation. At this time, skip parsing subscript operations in
   // this range. To use a subscript operation, put the expression between
   // parentheses.
   struct expr_reading expr;
   p_init_palrange_expr_reading( &expr, true );
   p_read_expr( parse, &expr );
   range->value.tint.amount = expr.output_node;
   read_palrange_rgb_field( parse,
      &range->value.tint.red,
      &range->value.tint.green,
      &range->value.tint.blue );
   range->tint = true;
}

static void read_buildmsg_stmt( struct parse* parse,
   struct stmt_reading* reading ) {
   struct buildmsg_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_BUILDMSG;
   stmt->buildmsg = read_buildmsg( parse, reading );
   reading->node = &stmt->node;
}

static struct buildmsg* read_buildmsg( struct parse* parse,
   struct stmt_reading* reading ) {
   p_test_tk( parse, TK_BUILDMSG );
   p_read_tk( parse );
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_block( parse, reading );
   struct buildmsg* buildmsg = mem_alloc( sizeof( *buildmsg ) );
   buildmsg->expr = expr.output_node;
   buildmsg->block = reading->block_node;
   list_init( &buildmsg->usages );
   return buildmsg;
}

static void read_expr_stmt( struct parse* parse,
   struct stmt_reading* reading ) {
   struct expr_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_EXPR_STMT;
   list_init( &stmt->expr_list );
   while ( true ) {
      struct expr_reading expr;
      p_init_expr_reading( &expr, false, false, false, false );
      p_read_expr( parse, &expr );
      list_append( &stmt->expr_list, expr.output_node );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   reading->node = &stmt->node;
}

static void read_assert( struct parse* parse, struct stmt_reading* reading ) {
   struct assert* assert = alloc_assert( &parse->tk_pos );
   if ( parse->tk == TK_STATIC ) {
      assert->is_static = true;
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_ASSERT );
   p_read_tk( parse );
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   assert->cond = expr.output_node;
   if ( parse->tk == TK_COMMA ) {
      p_read_tk( parse );
      p_init_expr_reading( &expr, false, false, false, true );
      p_read_expr( parse, &expr );
      assert->message = expr.output_node;
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   reading->node = &assert->node;
   if ( ! assert->is_static ) {
      list_append( &parse->task->runtime_asserts, assert );
   }
}

static struct assert* alloc_assert( struct pos* pos ) {
   struct assert* assert = mem_alloc( sizeof( *assert ) );
   assert->node.type = NODE_ASSERT;
   assert->next = NULL;
   assert->cond = NULL;
   assert->pos = *pos;
   assert->message = NULL;
   assert->file = NULL;
   assert->is_static = false;
   return assert;
}
