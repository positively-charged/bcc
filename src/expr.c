#include <string.h>

#include "task.h"

struct read {
   struct node* node;
};

struct operand {
   struct func* func;
   struct dim* dim;
   struct pos pos;
   struct region* region;
   struct type* type;
   struct name* name_offset;
   enum {
      STATE_CHECK,
      STATE_CHANGE
   } state;
   int value;
   bool is_value;
   bool is_space;
   bool folded;
};

static void read_op( struct task*, struct read_expr*, struct read* );
static void read_operand( struct task*, struct read_expr*, struct read* );
static void add_binary( struct read*, int, struct pos*, struct read* );
static void add_unary( struct read*, int );
static void read_primary( struct task*, struct read_expr*, struct read* );
static void read_postfix( struct task*, struct read_expr*, struct read* );
static void read_call( struct task*, struct read_expr*, struct read* );
static void init_operand( struct operand*, struct name* );
static void use_object( struct task*, struct operand*, struct object* );
static void read_string( struct task*, struct read_expr*, struct read* );
static void init_operand( struct operand*, struct name* );
static void test_node( struct task*, struct expr_test*, struct operand*,
   struct node* );
static void test_name_usage( struct task*, struct expr_test*, struct operand*,
   struct name_usage* );
static void use_object( struct task*, struct operand*, struct object* );
static void test_unary( struct task*, struct expr_test*, struct operand*,
   struct unary* );
static void test_subscript( struct task*, struct expr_test*, struct operand*,
   struct subscript* );
static void test_call( struct task*, struct expr_test*, struct operand*,
   struct call* );
static void test_binary( struct task*, struct expr_test*, struct operand*,
   struct binary* );
static void test_assign( struct task*, struct expr_test*, struct operand*,
   struct assign* );
static void test_format_block_usage( struct task*, struct expr_test*,
   struct call*, struct format_block_usage* );
static void test_access( struct task*, struct expr_test*, struct operand*,
   struct access* );

void t_init_read_expr( struct read_expr* read ) {
   read->node = NULL;
   read->stmt_read = NULL;
   read->has_str = false;
   read->in_constant = false;
   read->skip_assign = false;
   read->skip_function_call = false;
}

void t_read_expr( struct task* task, struct read_expr* read ) {
   struct read operand;
   read_op( task, read, &operand );
   struct expr* expr = mem_slot_alloc( sizeof( *expr ) );
   expr->node.type = NODE_EXPR;
   expr->root = operand.node;
   expr->value = 0;
   expr->folded = false;
   expr->has_str = read->has_str;
   read->node = expr;
}

void read_op( struct task* task, struct read_expr* read,
   struct read* operand ) {
   int op;

   struct read add;
   struct read* add_parent = NULL;
   int add_op = 0;
   struct pos add_op_pos;

   struct read shift;
   struct read* shift_parent = NULL;
   int shift_op = 0;
   struct pos shift_op_pos;

   struct read lt;
   struct read* lt_parent = NULL;
   int lt_op = 0;
   struct pos lt_op_pos;

   struct read eq;
   struct read* eq_parent = NULL;
   int eq_op = 0;
   struct pos eq_op_pos;

   struct read bit_and;
   struct read* bit_and_parent = NULL;
   struct pos bit_and_pos;

   struct read bit_xor;
   struct read* bit_xor_parent = NULL;
   struct pos bit_xor_pos;

   struct read bit_or;
   struct read* bit_or_parent = NULL;
   struct pos bit_or_pos;

   struct read log_and;
   struct read* log_and_parent = NULL;
   struct pos log_and_pos;

   struct read log_or;
   struct read* log_or_parent = NULL;
   struct pos log_or_pos;

   top:
   // -----------------------------------------------------------------------
   read_operand( task, read, operand );

   op_mul:
   // -----------------------------------------------------------------------
   if ( task->tk == TK_STAR ) {
      op = BOP_MUL;
   }
   else if ( task->tk == TK_SLASH ) {
      op = BOP_DIV;
   }
   else if ( task->tk == TK_MOD ) {
      op = BOP_MOD;
   }
   else {
      goto op_add;
   }
   struct pos op_pos = task->tk_pos;
   t_read_tk( task );
   struct read rside;
   read_operand( task, read, &rside );
   add_binary( operand, op, &op_pos, &rside );
   op = 0;
   goto op_mul;

   op_add:
   // -----------------------------------------------------------------------
   if ( add_parent ) {
      add_binary( add_parent, add_op, &add_op_pos, operand );
      operand = add_parent;
      add_parent = NULL;
   }
   if ( task->tk == TK_PLUS ) {
      op = BOP_ADD;
   }
   else if ( task->tk == TK_MINUS ) {
      op = BOP_SUB;
   }
   else {
      goto op_shift;
   }
   add_parent = operand;
   operand = &add;
   add_op = op;
   add_op_pos = task->tk_pos;
   t_read_tk( task );
   goto top;

   op_shift:
   // -----------------------------------------------------------------------
   if ( shift_parent ) {
      add_binary( shift_parent, shift_op, &shift_op_pos, operand );
      operand = shift_parent;
      shift_parent = NULL;
   }
   if ( task->tk == TK_SHIFT_L ) {
      op = BOP_SHIFT_L;
   }
   else if ( task->tk == TK_SHIFT_R ) {
      op = BOP_SHIFT_R;
   }
   else {
      goto op_lt;
   }
   shift_parent = operand;
   operand = &shift;
   shift_op = op;
   shift_op_pos = task->tk_pos;
   t_read_tk( task );
   goto top;

   op_lt:
   // -----------------------------------------------------------------------
   if ( lt_parent ) {
      add_binary( lt_parent, lt_op, &lt_op_pos, operand );
      operand = lt_parent;
      lt_parent = NULL;
   }
   if ( task->tk == TK_LT ) {
      op = BOP_LT;
   }
   else if ( task->tk == TK_LTE ) {
      op = BOP_LTE;
   }
   else if ( task->tk == TK_GT ) {
      op = BOP_GT;
   }
   else if ( task->tk == TK_GTE ) {
      op = BOP_GTE;
   }
   else {
      goto op_eq;
   }
   lt_parent = operand;
   operand = &lt;
   lt_op = op;
   lt_op_pos = task->tk_pos;
   t_read_tk( task );
   goto top;

   op_eq:
   // -----------------------------------------------------------------------
   if ( eq_parent ) {
      add_binary( eq_parent, eq_op, &eq_op_pos, operand );
      operand = eq_parent;
      eq_parent = NULL;
   }
   if ( task->tk == TK_EQ ) {
      op = BOP_EQ;
   }
   else if ( task->tk == TK_NEQ ) {
      op = BOP_NEQ;
   }
   else {
      goto op_bit_and;
   }
   eq_parent = operand;
   operand = &eq;
   eq_op = op;
   eq_op_pos = task->tk_pos;
   t_read_tk( task );
   goto top;

   op_bit_and:
   // -----------------------------------------------------------------------
   if ( bit_and_parent ) {
      add_binary( bit_and_parent, BOP_BIT_AND, &bit_and_pos, operand );
      operand = bit_and_parent;
      bit_and_parent = NULL;
   }
   if ( task->tk == TK_BIT_AND ) {
      bit_and_parent = operand;
      operand = &bit_and;
      bit_and_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_xor_parent ) {
      add_binary( bit_xor_parent, BOP_BIT_XOR, &bit_xor_pos, operand );
      operand = bit_xor_parent;
      bit_xor_parent = NULL;
   }
   if ( task->tk == TK_BIT_XOR ) {
      bit_xor_parent = operand;
      operand = &bit_xor;
      bit_xor_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_or_parent ) {
      add_binary( bit_or_parent, BOP_BIT_OR, &bit_or_pos, operand );
      operand = bit_or_parent;
      bit_or_parent = NULL;
   }
   if ( task->tk == TK_BIT_OR ) {
      bit_or_parent = operand;
      operand = &bit_or;
      bit_or_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_and_parent ) {
      add_binary( log_and_parent, BOP_LOG_AND, &log_and_pos, operand );
      operand = log_and_parent;
      log_and_parent = NULL;
   }
   if ( task->tk == TK_LOG_AND ) {
      log_and_parent = operand;
      operand = &log_and;
      log_and_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_or_parent ) {
      add_binary( log_or_parent, BOP_LOG_OR, &log_or_pos, operand );
      operand = log_or_parent;
      log_or_parent = NULL;
   }
   if ( task->tk == TK_LOG_OR ) {
      log_or_parent = operand;
      operand = &log_or;
      log_or_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( ! read->skip_assign ) {
      switch ( task->tk ) {
      case TK_ASSIGN: op = AOP_NONE; break;
      case TK_ASSIGN_ADD: op = AOP_ADD; break;
      case TK_ASSIGN_SUB: op = AOP_SUB; break;
      case TK_ASSIGN_MUL: op = AOP_MUL; break;
      case TK_ASSIGN_DIV: op = AOP_DIV; break;
      case TK_ASSIGN_MOD: op = AOP_MOD; break;
      case TK_ASSIGN_SHIFT_L: op = AOP_SHIFT_L; break;
      case TK_ASSIGN_SHIFT_R: op = AOP_SHIFT_R; break;
      case TK_ASSIGN_BIT_AND: op = AOP_BIT_AND; break;
      case TK_ASSIGN_BIT_XOR: op = AOP_BIT_XOR; break;
      case TK_ASSIGN_BIT_OR: op = AOP_BIT_OR; break;
      // Finish.
      default: return;
      }
      t_read_tk( task );
      struct pos rside_pos = task->tk_pos;
      read_op( task, read, &rside );
      struct assign* assign = mem_alloc( sizeof( *assign ) );
      assign->node.type = NODE_ASSIGN;
      assign->op = op;
      assign->lside = operand->node;
      assign->rside = rside.node;
      operand->node = ( struct node* ) assign;
   }
}

void add_binary( struct read* lside, int op, struct pos* pos,
   struct read* rside ) {
   struct binary* binary = mem_slot_alloc( sizeof( *binary ) );
   binary->node.type = NODE_BINARY;
   binary->op = op;
   binary->lside = lside->node;
   binary->rside = rside->node;
   binary->pos = *pos;
   lside->node = ( struct node* ) binary;
}

void read_operand( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   // Prefix operations:
   int op = UOP_NONE;
   switch ( task->tk ) {
   case TK_INC: op = UOP_PRE_INC; break;
   case TK_DEC: op = UOP_PRE_DEC; break;
   case TK_MINUS: op = UOP_MINUS; break;
   case TK_PLUS: op = UOP_PLUS; break;
   case TK_LOG_NOT: op = UOP_LOG_NOT; break;
   case TK_BIT_NOT: op = UOP_BIT_NOT; break;
   default: break;
   }
   if ( op ) {
      t_read_tk( task );
      struct pos pos = task->tk_pos;
      read_operand( task, expr, operand );
      add_unary( operand, op );
   }
   else {
      operand->node = NULL;
      read_primary( task, expr, operand );
      read_postfix( task, expr, operand );
   }
}


void add_unary( struct read* operand, int op ) {
   struct unary* unary = mem_alloc( sizeof( *unary ) );
   unary->node.type = NODE_UNARY;
   unary->op = op;
   unary->operand = operand->node;
   operand->node = ( struct node* ) unary;
}

void read_primary( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   // Current region.
   if ( task->tk == TK_REGION ) {
      static struct node node = { NODE_REGION_SELF };
      operand->node = &node;
      t_read_tk( task );
   }
   // Global region.
   else if ( task->tk == TK_UPMOST ) {
      static struct node node = { NODE_REGION_UPMOST };
      operand->node = &node;
      t_read_tk( task );
   }
   else if ( task->tk == TK_ID ) {
      struct name_usage* usage = mem_slot_alloc( sizeof( *usage ) );
      usage->node.type = NODE_NAME_USAGE;
      usage->text = task->tk_text;
      usage->pos = task->tk_pos;
      usage->object = NULL;
      operand->node = ( struct node* ) usage;
      t_read_tk( task );
   }
   else if ( task->tk == TK_PAREN_L ) {
      struct paren* paren = mem_alloc( sizeof( *paren ) );
      paren->node.type = NODE_PAREN;
      paren->pos = task->tk_pos;
      t_read_tk( task );
      read_op( task, expr, operand );
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      paren->inside = operand->node;
      operand->node = ( struct node* ) paren;
   }
   // For now, the boolean literals will just be aliases to numeric constants.
   else if ( task->tk == TK_TRUE || task->tk == TK_FALSE ) {
      struct literal* literal = mem_slot_alloc( sizeof( *literal ) );
      literal->node.type = NODE_LITERAL;
      literal->pos = task->tk_pos;
      literal->value = 0;
      literal->is_bool = true;
      if ( task->tk == TK_TRUE ) {
         literal->value = 1;
      }
      operand->node = &literal->node;
      t_read_tk( task );
   }
   else if ( task->tk == TK_LIT_STRING ) {
      read_string( task, expr, operand );
   }
   else {
      struct literal* literal = mem_slot_alloc( sizeof( *literal ) );
      literal->node.type = NODE_LITERAL;
      literal->pos = task->tk_pos;
      literal->value = t_read_literal( task );
      literal->is_bool = false;
      operand->node = ( struct node* ) literal;
   }
}

void read_string( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   t_test_tk( task, TK_LIT_STRING );
   struct indexed_string* prev = NULL;
   struct indexed_string* string = task->str_table.head_sorted;
   while ( string ) {
      int result = strcmp( string->value, task->tk_text );
      if ( result == 0 ) {
         break;
      }
      else if ( result > 0 ) {
         string = NULL;
         break;
      }
      else {
         prev = string;
         string = string->next_sorted;
      }
   }
   // Allocate a new string.
   if ( ! string ) {
      void* block = mem_alloc(
         sizeof( struct indexed_string ) + task->tk_length + 1 );
      string = block;
      string->value = ( ( char* ) block ) + sizeof( *string );
      memcpy( string->value, task->tk_text, task->tk_length );
      string->value[ task->tk_length ] = 0;
      string->length = task->tk_length;
      string->index = 0;
      string->next = NULL;
      string->next_sorted = NULL;
      string->next_usable = NULL;
      string->in_constant = expr->in_constant;
      string->used = false;
      string->imported = false;
      if ( task->library->imported ) {
         string->imported = true;
      }
      // Appearance insert.
      if ( task->str_table.head ) {
         task->str_table.tail->next = string;
      }
      else {
         task->str_table.head = string;
      }
      task->str_table.tail = string;
      // Sorted insert.
      if ( prev ) {
         string->next_sorted = prev->next_sorted;
         prev->next_sorted = string;
      }
      else {
         string->next_sorted = task->str_table.head_sorted;
         task->str_table.head_sorted = string;
      }
   }
   else {
      // If the imported string is found in the main library, then the string
      // doesn't need to be marked as "imported."
      if ( ! task->library->imported ) {
         string->imported = false;
      }
   }
   struct indexed_string_usage* usage = mem_slot_alloc( sizeof( *usage ) );
   usage->node.type = NODE_INDEXED_STRING_USAGE;
   usage->pos = task->tk_pos;
   usage->string = string;
   operand->node = &usage->node;
   expr->has_str = true;
   t_read_tk( task );
}

int t_read_literal( struct task* task ) {
   int value = 0;
   if ( task->tk == TK_LIT_DECIMAL ) {
      value = strtol( task->tk_text, NULL, 10 );
   }
   else if ( task->tk == TK_LIT_OCTAL ) {
      value = strtol( task->tk_text, NULL, 8 );
   }
   else if ( task->tk == TK_LIT_HEX ) {
      value = strtol( task->tk_text, NULL, 16 );
   }
   else if ( task->tk == TK_LIT_FIXED ) {
      double num = atof( task->tk_text );
      value =
         // Whole.
         ( ( int ) num << 16 ) +
         // Fraction.
         ( int ) ( ( 1 << 16 ) * ( num - ( int ) num ) );
   }
   else if ( task->tk == TK_LIT_CHAR ) {
      value = task->tk_text[ 0 ];
   }
   else {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing primary expression" );
      t_bail( task );
   }
   t_read_tk( task );
   return value;
}

void read_postfix( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   while ( true ) {
      if ( task->tk == TK_BRACKET_L ) {
         struct subscript* subscript = mem_alloc( sizeof( *subscript ) );
         subscript->node.type = NODE_SUBSCRIPT;
         subscript->pos = task->tk_pos;
         t_read_tk( task );
         struct read index;
         read_op( task, expr, &index );
         t_test_tk( task, TK_BRACKET_R );
         t_read_tk( task );
         subscript->index = index.node;
         subscript->lside = operand->node;
         operand->node = ( struct node* ) subscript;
      }
      else if ( task->tk == TK_DOT ) {
         t_read_tk( task );
         t_test_tk( task, TK_ID );
         struct access* access = mem_alloc( sizeof( *access ) );
         access->node.type = NODE_ACCESS;
         access->name = task->tk_text;
         access->pos = task->tk_pos;
         t_read_tk( task );
         access->lside = operand->node;
         access->rside = NULL;
         operand->node = ( struct node* ) access;
      }
      else if ( task->tk == TK_PAREN_L && ! expr->skip_function_call ) {
         read_call( task, expr, operand );
      }
      else if ( task->tk == TK_INC ) {
         add_unary( operand, UOP_POST_INC );
         t_read_tk( task );
      }
      else if ( task->tk == TK_DEC ) {
         add_unary( operand, UOP_POST_DEC );
         t_read_tk( task );
      }
      else {
         break;
      }
   }
}

void read_call( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   struct pos pos = task->tk_pos;
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   struct list args;
   list_init( &args );
   int count = 0;
   // Format list:
   if ( task->tk == TK_ID && t_peek( task ) == TK_COLON ) {
      list_append( &args, t_read_format_item( task, true ) );
      ++count;
      if ( task->tk == TK_SEMICOLON ) {
         t_read_tk( task );
         while ( true ) {
            struct read_expr arg;
            t_init_read_expr( &arg );
            t_read_expr( task, &arg );
            list_append( &args, arg.node );
            ++count;
            if ( task->tk == TK_COMMA ) {
               t_read_tk( task );
            }
            else {
               break;
            }
         }
      }
   }
   // Format block:
   else if ( task->tk == TK_BRACE_L ) {
      struct format_block_usage* usage = mem_alloc( sizeof( *usage ) );
      usage->node.type = NODE_FORMAT_BLOCK_USAGE;
      usage->block = NULL;
      usage->next = NULL;
      usage->pos = task->tk_pos;
      usage->obj_pos = 0;
      list_append( &args, usage );
      t_read_tk( task );
      t_test_tk( task, TK_BRACE_R );
      t_read_tk( task );
      if ( task->tk == TK_SEMICOLON ) {
         t_read_tk( task );
         while ( true ) {
            struct read_expr arg;
            t_init_read_expr( &arg );
            t_read_expr( task, &arg );
            list_append( &args, arg.node );
            ++count;
            if ( task->tk == TK_COMMA ) {
               t_read_tk( task );
            }
            else {
               break;
            }
         }
      }
   }
   else {
      // This relic is not necessary in new code. The compiler is smart enough
      // to figure out when to use the constant version of an instruction.
      if ( task->tk == TK_CONST ) {
         t_read_tk( task );
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
      }
      if ( task->tk != TK_PAREN_R ) {
         while ( true ) {
            struct read_expr arg;
            t_init_read_expr( &arg );
            t_read_expr( task, &arg );
            list_append( &args, arg.node );
            count += 1;
            if ( task->tk == TK_COMMA ) {
               t_read_tk( task );
            }
            else {
               break;
            }
         }
      }
   }
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   struct call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_CALL;
   call->pos = pos;
   call->func = NULL;
   call->func_tree = operand->node;
   call->args = args;
   operand->node = ( struct node* ) call;
}

struct format_item* t_read_format_item( struct task* task, bool colon ) {
   struct format_item* head = NULL;
   struct format_item* tail;
   while ( true ) {
      t_test_tk( task, TK_ID );
      struct format_item* item = mem_alloc( sizeof( *item ) );
      item->node.type = NODE_FORMAT_ITEM;
      item->cast = FCAST_DECIMAL;
      item->pos = task->tk_pos;
      item->next = NULL;
      item->expr = NULL;
      bool unknown = false;
      switch ( task->tk_text[ 0 ] ) {
      case 'a': item->cast = FCAST_ARRAY; break;
      case 'b': item->cast = FCAST_BINARY; break;
      case 'c': item->cast = FCAST_CHAR; break;
      case 'd':
      case 'i': break;
      case 'f': item->cast = FCAST_FIXED; break;
      case 'k': item->cast = FCAST_KEY; break;
      case 'l': item->cast = FCAST_LOCAL_STRING; break;
      case 'n': item->cast = FCAST_NAME; break;
      case 's': item->cast = FCAST_STRING; break;
      case 'x': item->cast = FCAST_HEX; break;
      default:
         unknown = true;
         break;
      }
      if ( unknown || task->tk_length != 1 ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &task->tk_pos, "unknown format cast `%s`", task->tk_text );
         t_bail( task );
      }
      t_read_tk( task );
      // In a format block, only the `:=` separator is allowed, because `:`
      // will conflict with a goto label.
      if ( colon ) {
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
      }
      else {
         t_test_tk( task, TK_ASSIGN_COLON );
         t_read_tk( task );
      }
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      item->expr = expr.node;
      if ( head ) {
         tail->next = item;
      }
      else {
         head = item;
      }
      tail = item;
      if ( task->tk == TK_COMMA ) {
         t_read_tk( task );
      }
      else {
         return head;
      }
   }
}

void t_init_expr_test( struct expr_test* test ) {
   test->stmt_test = NULL;
   test->format_block = NULL;
   test->format_block_usage = NULL;
   test->needed = true;
   test->has_string = false;
   test->undef_err = false;
   test->undef_erred = false;
}

void t_test_expr( struct task* task, struct expr_test* test,
   struct expr* expr ) {
   if ( setjmp( test->bail ) == 0 ) {
      struct operand operand;
      init_operand( &operand, task->region->body );
      test_node( task, test, &operand, expr->root );
      expr->folded = operand.folded;
      expr->value = operand.value;
      test->pos = operand.pos;
      if ( test->needed && ! operand.is_value ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &operand.pos, "expression does not produce a value" );
         t_bail( task );
      }
   }
}

void init_operand( struct operand* operand, struct name* offset ) {
   operand->func = NULL;
   operand->dim = NULL;
   operand->type = NULL;
   operand->region = NULL;
   operand->name_offset = offset;
   operand->state = STATE_CHECK;
   operand->value = 0;
   operand->is_value = false;
   operand->is_space = false;
   operand->folded = false;
}

void test_node( struct task* task, struct expr_test* test,
   struct operand* operand, struct node* node ) {
   if ( node->type == NODE_LITERAL ) {
      struct literal* literal = ( struct literal* ) node;
      operand->pos = literal->pos;
      operand->folded = true;
      operand->value = literal->value;
      operand->is_value = true;
      operand->type = task->type_int;
      if ( literal->is_bool ) {
         operand->type = task->type_bool;
      }
   }
   else if ( node->type == NODE_INDEXED_STRING_USAGE ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) node;
      operand->pos = usage->pos;
      operand->folded = true;
      operand->value = usage->string->index;
      operand->is_value = true;
      operand->type = task->type_str;
      test->has_string = true;
   }
   else if ( node->type == NODE_NAME_USAGE ) {
      test_name_usage( task, test, operand, ( struct name_usage* ) node );
   }
   else if ( node->type == NODE_UNARY ) {
      test_unary( task, test, operand, ( struct unary* ) node );
   }
   else if ( node->type == NODE_SUBSCRIPT ) {
      test_subscript( task, test, operand, ( struct subscript* ) node );
   }
   else if ( node->type == NODE_CALL ) {
      test_call( task, test, operand, ( struct call* ) node );
   }
   else if ( node->type == NODE_BINARY ) {
      test_binary( task, test, operand, ( struct binary* ) node );
   }
   else if ( node->type == NODE_ASSIGN ) {
      test_assign( task, test, operand, ( struct assign* ) node );
   }
   else if ( node->type == NODE_ACCESS ) {
      test_access( task, test, operand, ( struct access* ) node );
   }
   else if ( node->type == NODE_REGION_SELF ) {
      operand->region = task->region;
   }
   else if ( node->type == NODE_REGION_UPMOST ) {
      operand->region = task->region_global;
   }
   else if ( node->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) node;
      test_node( task, test, operand, paren->inside );
      operand->pos = paren->pos;
   }
}

void test_name_usage( struct task* task, struct expr_test* test,
   struct operand* operand, struct name_usage* usage ) {
   struct name* name = t_make_name( task, usage->text, task->region->body );
   struct object* object = name->object;
   // Try the linked modules.
   if ( ! object ) {
      struct region_link* link = task->region->link;
      while ( link ) {
         name = t_make_name( task, usage->text, link->region->body );
         object = t_get_region_object( task, link->region, name );
         link = link->next;
         if ( object ) {
            break;
         }
      }
      // Make sure no other object with the same name can be found.
      int dup = 0;
      while ( link ) {
         struct name* other_name = t_make_name( task, usage->text,
            link->region->body );
         if ( other_name->object ) {
            if ( ! dup ) {
               t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &usage->pos, "multiple objects with name `%s`",
                  usage->text );
               t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &name->object->pos, "object found here" );
            }
            t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &other_name->object->pos, "object found here" );
            ++dup;
         }
         link = link->next;
      }
      if ( dup ) {
         t_bail( task );
      }
   }
   if ( object && object->resolved ) {
      use_object( task, operand, object );
      usage->object = ( struct node* ) object;
      operand->pos = usage->pos;
   }
   else if ( test->undef_err ) {
      if ( ! object ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &usage->pos,
            "`%s` not found", usage->text );
      }
      else {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &usage->pos,
            "`%s` undefined", usage->text );
      }
      t_bail( task );
   }
   else {
      test->undef_erred = true;
      longjmp( test->bail, 1 );
   }
}

void use_object( struct task* task, struct operand* operand,
   struct object* object ) {
   if ( object->node.type == NODE_REGION ) {
      operand->region = ( struct region* ) object;
   }
   else if ( object->node.type == NODE_CONSTANT ) {
      struct constant* constant = ( struct constant* ) object;
      operand->type = task->type_int;
      operand->value = constant->value;
      operand->folded = true;
      operand->is_value = true;
   }
   else if ( object->node.type == NODE_VAR ) {
      struct var* var = ( struct var* ) object;
      ++var->usage;
      operand->type = var->type;
      if ( var->dim ) {
         operand->dim = var->dim;
      }
      else if ( var->type->primitive ) {
         operand->is_value = true;
         operand->is_space = true;
      }
      // Keep track of how the variable is used.
      if ( operand->state == STATE_CHECK ) {
         var->state_checked = true;
      }
      else {
         var->state_changed = true;
      }
   }
   else if ( object->node.type == NODE_TYPE_MEMBER ) {
      struct type_member* member = ( struct type_member* ) object;
      operand->type = member->type;
      if ( member->dim ) {
         operand->dim = member->dim;
      }
      else if ( member->type->primitive ) {
         operand->is_value = true;
         operand->is_space = true;
      }
   }
   else if ( object->node.type == NODE_FUNC ) {
      operand->func = ( struct func* ) object;
      if ( operand->func->type == FUNC_USER ) {
         struct func_user* impl = operand->func->impl;
         ++impl->usage;
      }
      // When just using the name of an action special, its ID is returned.
      else if ( operand->func->type == FUNC_ASPEC ) {
         operand->is_value = true;
      }
   }
   else if ( object->node.type == NODE_PARAM ) {
      struct param* param = ( struct param* ) object;
      operand->type = param->type;
      operand->is_value = true;
      operand->is_space = true;
   }
   else if ( object->node.type == NODE_ALIAS ) {
      struct alias* alias = ( struct alias* ) object;
      use_object( task, operand, alias->target );
   }
}

void test_unary( struct task* task, struct expr_test* test,
   struct operand* operand, struct unary* unary ) {
   struct operand target;
   init_operand( &target, operand->name_offset );
   target.state = STATE_CHANGE;
   // target.state_access = true;
   // target.state_update = true;
   test_node( task, test, &target, unary->operand );
   if ( unary->op == UOP_PRE_INC || unary->op == UOP_PRE_DEC ||
      unary->op == UOP_POST_INC || unary->op == UOP_POST_DEC ) {
      // Only an l-value can be incremented.
      if ( ! target.is_space ) {
         const char* action = "incremented";
         if ( unary->op == UOP_PRE_DEC || unary->op == UOP_POST_DEC ) {
            action = "decremented";
         }
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &target.pos, "operand cannot be %s", action );
         t_bail( task );
      }
   }
   else {
      // Remaining operations require a value to work on.
      if ( ! target.is_value ) {
         t_diag( task, DIAG_POS_ERR, &target.pos,
            "operand cannot be used in unary operation" );
         t_bail( task );
      }
   }
   if ( target.folded ) {
      switch ( unary->op ) {
      case UOP_MINUS:
         operand->value = ( - target.value );
         break;
      case UOP_PLUS:
         operand->value = target.value;
         break;
      case UOP_LOG_NOT:
         operand->value = ( ! target.value );
         break;
      case UOP_BIT_NOT:
         operand->value = ( ~ target.value );
         break;
      default:
         break;
      }
      operand->folded = true;
   }
   operand->pos = target.pos;
   operand->is_value = true;
}

void test_subscript( struct task* task, struct expr_test* test,
   struct operand* operand, struct subscript* subscript ) {
   struct operand target;
   init_operand( &target, operand->name_offset );
   test_node( task, test, &target, subscript->lside );
   if ( ! target.dim ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &subscript->pos,
         "accessing something not an array" );
      t_bail( task );
   }
   struct operand index;
   init_operand( &index, operand->name_offset );
   test_node( task, test, &index, subscript->index );
   operand->pos = target.pos;
   operand->type = target.type;
   operand->dim = target.dim->next;
   if ( ! operand->dim ) {
      operand->is_space = true;
      operand->is_value = true;
   }
}

void test_call( struct task* task, struct expr_test* test,
   struct operand* operand, struct call* call ) {
   struct operand callee;
   init_operand( &callee, operand->name_offset );
   test_node( task, test, &callee, call->func_tree );
   if ( ! callee.func ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &callee.pos,
         "calling something not a function" );
      t_bail( task );
   }
   // Some action specials cannot be used in a script.
   if ( callee.func->type == FUNC_ASPEC ) {
      struct func_aspec* impl = callee.func->impl;
      if ( ! impl->script_callable ) {
         struct str str;
         str_init( &str );
         t_copy_name( callee.func->name, false, &str );
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &callee.pos,
            "action-special `%s` called from script", str.value );
         t_bail( task );
      }
   }
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &call->args );
   if ( callee.func->type == FUNC_FORMAT ) {
      struct node* node = NULL;
      if ( ! list_end( &i ) ) {
         node = list_data( &i );
      }
      if ( ! node || ( node->type != NODE_FORMAT_ITEM &&
         node->type != NODE_FORMAT_BLOCK_USAGE ) ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &call->pos,
            "function call missing format argument" );
         t_bail( task );
      }
      // Format-block:
      if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
         if ( ! test->format_block ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &call->pos, "function call missing format block" );
            t_bail( task );
         }
         struct format_block_usage* usage =
            ( struct format_block_usage* ) node;
         usage->block = test->format_block;
         struct format_block_usage* prev = test->format_block_usage;
         while ( prev && prev->next ) {
            prev = prev->next;
         }
         if ( prev ) {
            prev->next = usage;
         }
         else {
            test->format_block_usage = usage;
         }
         list_next( &i );
      }
      // Format-list:
      else {
         while ( ! list_end( &i ) ) {
            struct node* node = list_data( &i );
            if ( node->type == NODE_FORMAT_ITEM ) {
               t_test_format_item( task, ( struct format_item* ) node,
                  test->stmt_test, operand->name_offset, test->format_block );
               list_next( &i );
            }
            else {
               break;
            }
         }
      }
      ++count;
   }
   else {
      if ( list_size( &call->args ) ) {
         struct node* node = list_data( &i );
         if ( node->type == NODE_FORMAT_ITEM ) {
            struct format_item* item = ( struct format_item* ) node;
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &item->pos,
               "passing format item to non-format function" );
            t_bail( task );
         }
         else if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
            struct format_block_usage* usage =
               ( struct format_block_usage* ) node;
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &usage->pos, "passing format block to non-format function" );
            t_bail( task );
         }
      }
   }
   // Arguments:
   while ( ! list_end( &i ) ) {
      struct expr_test arg;
      t_init_expr_test( &arg );
      arg.undef_err = true;
      arg.needed = true;
      arg.format_block = test->format_block;
      t_test_expr( task, &arg, list_data( &i ) );
      list_next( &i );
      ++count;
   }
   if ( count < callee.func->min_param ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &call->pos,
         "not enough arguments in function call" );
      const char* at_least = "";
      if ( callee.func->min_param != callee.func->max_param ) {
         at_least = "at least ";
      }
      const char* s = "";
      if ( callee.func->min_param != 1 ) {
         s = "s";
      }
      struct str str;
      str_init( &str );
      t_copy_name( callee.func->name, false, &str );
      t_diag( task, DIAG_FILE, &call->pos, "function `%s` needs %s%d argument%s",
         str.value, at_least, callee.func->min_param, s );
      t_bail( task );
   }
   else if ( count > callee.func->max_param ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &call->pos,
         "too many arguments in function call" );
      const char* up_to = "";
      if ( callee.func->min_param != callee.func->max_param ) {
         up_to = "up to ";
      }
      const char* s = "";
      if ( callee.func->max_param != 1 ) {
         s = "s";
      }
      struct str str;
      str_init( &str );
      t_copy_name( callee.func->name, false, &str );
      t_diag( task, DIAG_FILE, &call->pos,
         "function `%s` takes %s%d argument%s",
         str.value, up_to, callee.func->max_param, s );
      t_bail( task );
   }
   // Call to a latent function cannot occur in a function or a format block.
   if ( callee.func->type == FUNC_DED ) {
      struct func_ded* impl = callee.func->impl;
      if ( impl->latent ) {
         if ( task->in_func ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &callee.pos,
               "calling latent function inside function body" );       
            struct str str;
            str_init( &str );
            t_copy_name( callee.func->name, false, &str );
            t_diag( task, DIAG_FILE, &callee.pos,
               "waiting functions like `%s` can only be called inside a script",
               str.value );
            t_bail( task );
         }
         // Waiting function cannot be called from inside a format block.
         struct stmt_test* stmt = test->stmt_test;
         while ( stmt && ! stmt->format_block ) {
            stmt = stmt->parent;
         }
         if ( stmt ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &callee.pos,
               "calling latent function inside format block" );  
            t_bail( task ); 
         }
      }
   }
   call->func = callee.func;
   operand->pos = callee.pos;
   operand->type = callee.func->value;
   if ( operand->type ) {
      operand->is_value = true;
   }
}

void test_binary( struct task* task, struct expr_test* test,
   struct operand* operand, struct binary* binary ) {
   struct operand lside;
   init_operand( &lside, operand->name_offset );
   test_node( task, test, &lside, binary->lside );
   if ( ! lside.is_value ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &binary->pos,
         "left side of binary operation not a value" );
      t_bail( task );
   }
   struct operand rside;
   init_operand( &rside, operand->name_offset );
   test_node( task, test, &rside, binary->rside );
   if ( ! rside.is_value ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &binary->pos,
         "right side of binary operation not a value" );
      t_bail( task );
   }
   operand->pos = binary->pos;
   if ( lside.folded && rside.folded ) {
      int l = lside.value;
      int r = rside.value;
      // Division and modulo get special treatment because of the possibility
      // of a division by zero.
      if ( ( binary->op == BOP_DIV || binary->op == BOP_MOD ) && ! r ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &rside.pos,
            "division by zero" );
         t_bail( task );
      }
      switch ( binary->op ) {
      case BOP_MOD: l %= r; break;
      case BOP_MUL: l *= r; break;
      case BOP_DIV: l /= r; break;
      case BOP_ADD: l += r; break;
      case BOP_SUB: l -= r; break;
      case BOP_SHIFT_R: l >>= r; break;
      case BOP_SHIFT_L: l <<= r; break;
      case BOP_GTE: l = l >= r; break;
      case BOP_GT: l = l > r; break;
      case BOP_LTE: l = l <= r; break;
      case BOP_LT: l = l < r; break;
      case BOP_NEQ: l = l != r; break;
      case BOP_EQ: l = l == r; break;
      case BOP_BIT_AND: l = l & r; break;
      case BOP_BIT_XOR: l = l ^ r; break;
      case BOP_BIT_OR: l = l | r; break;
      case BOP_LOG_AND: l = l && r; break;
      case BOP_LOG_OR: l = l || r; break;
      default: break;
      }
      operand->value = l;
      operand->folded = true;
   }
   operand->is_value = true;
}

void test_assign( struct task* task, struct expr_test* test,
   struct operand* operand, struct assign* assign ) {
   struct operand lside;
   init_operand( &lside, operand->name_offset );
   lside.state = STATE_CHANGE;
   test_node( task, test, &lside, assign->lside );
   if ( ! lside.is_space ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &lside.pos,
         "cannot assign to operand" );
      t_bail( task );
   }
   struct operand rside;
   init_operand( &rside, operand->name_offset );
   test_node( task, test, &rside, assign->rside );
   if ( ! rside.is_value ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &rside.pos,
         "right side of assignment is not a value" );
      t_bail( task );
   }
   operand->is_value = true;
   operand->type = lside.type;
}

void t_test_format_item( struct task* task, struct format_item* item,
   struct stmt_test* stmt_test, struct name* name_offset,
   struct block* format_block ) {
   while ( item ) {
      struct expr_test expr_test;
      t_init_expr_test( &expr_test );
      expr_test.stmt_test = stmt_test;
      expr_test.undef_err = true;
      expr_test.format_block = format_block;
      expr_test.needed = true;
      struct operand arg;
      init_operand( &arg, name_offset );
      test_node( task, &expr_test, &arg, item->expr->root );
      if ( item->cast == FCAST_ARRAY ) {
         if ( ! arg.dim ) {
            t_diag( task, DIAG_POS_ERR, &arg.pos, "argument not an array" );
            t_bail( task );
         }
         if ( arg.dim->next ) {
            t_diag( task, DIAG_POS_ERR, &arg.pos,
               "array argument not of single dimension" );
            t_bail( task );
         }
      }
      item = item->next;
   }
}

struct name* t_make_module_name( struct task* task, struct module* module,
   char* text, struct name* offset ) {
   struct name* name = t_make_name( task, text, offset );
   if ( name->object ) {
      // Imported objects are not visible from outside a module.
      if ( name->object->node.type == NODE_ALIAS ) {
         if ( module != task->module ) {
            return NULL;
         }
      }
      // Constant made through #define is not visible to another module.
      else if ( name->object->node.type == NODE_CONSTANT ) {
         struct constant* constant = ( struct constant* ) name->object;
         if ( ! constant->visible ) {
            return NULL;
         }
      }
   }
   return name;
}

void test_access( struct task* task, struct expr_test* test,
   struct operand* operand, struct access* access ) {
   struct type* type = NULL;
   struct region* region = NULL;
   struct operand lside;
   init_operand( &lside, operand->name_offset );
   test_node( task, test, &lside, access->lside );
   if ( lside.region ) {
      region = lside.region;
   }
   else if ( lside.type && ! lside.dim ) {
      type = lside.type;
   }
   else {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &access->pos,
         "object does not supported the . operator" );
      t_bail( task );
   }
   struct name* name_offset = NULL;
   if ( type ) {
      name_offset = type->body;
   }
   else {
      name_offset = region->body;
   }
   struct name* name = t_make_name( task, access->name, name_offset );
   struct object* object = name->object;
   if ( region ) {
      object = t_get_region_object( task, region, name );
   }
   if ( ! object ) {
      if ( ! test->undef_err ) {
         test->undef_erred = true;
         longjmp( test->bail, 0 );
      }
      if ( region ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &access->pos,
            "`%s` not found in region", access->name );
         t_bail( task );
      }
      else if ( type ) {
         // TODO: Remember to implement for anonynous structs!
         const char* subject = "type";
         if ( ! type->primitive ) {
            subject = "struct";
         }
         struct str str;
         str_init( &str );
         t_copy_name( type->name, false, &str );
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &access->pos,
            "member `%s` not found in %s `%s`", access->name, subject,
            str.value );
         t_bail( task );
      }
   }
   if ( ! object->resolved ) {
      test->undef_erred = true;
      longjmp( test->bail, 1 );
   }
   use_object( task, operand, object );
   access->rside = ( struct node* ) object;
   operand->pos = access->pos;
}