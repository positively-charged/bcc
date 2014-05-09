#include <string.h>

#include "task.h"

struct read {
   struct node* node;
};

struct operand {
   struct func* func;
   struct dim* dim;
   struct region* region;
   struct type* type;
   struct name* name_offset;
   int value;
   bool is_result;
   bool is_usable;
   bool is_space;
   bool folded;
   bool state_access;
   bool state_modify;
};

static void read_op( struct task*, struct read_expr*, struct read* );
static void read_operand( struct task*, struct read_expr*, struct read* );
static void add_binary( struct read*, int, struct pos*, struct read* );
static void add_unary( struct read*, int, struct pos* );
static void read_primary( struct task*, struct read_expr*, struct read* );
static void read_postfix( struct task*, struct read_expr*, struct read* );
static struct access* alloc_access( char*, struct pos, struct node*, bool );
static void read_call( struct task*, struct read_expr*, struct read* );
static void init_operand( struct operand* );
static void use_object( struct task*, struct expr_test*, struct operand*,
   struct object* );
static void read_string( struct task*, struct read_expr*, struct read* );
static void test_expr( struct task*, struct expr_test*, struct expr*,
   struct operand* );
static void test_node( struct task*, struct expr_test*, struct operand*,
   struct node* );
static void test_literal( struct task*, struct operand*, struct literal* );
static void test_string_usage( struct task*, struct expr_test*,
   struct operand*, struct indexed_string_usage* );
static void test_name_usage( struct task*, struct expr_test*, struct operand*,
   struct name_usage* );
static struct object* find_usage_object( struct task*, struct name_usage* );
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
   struct pos pos = task->tk_pos;
   struct read operand;
   read_op( task, read, &operand );
   struct expr* expr = mem_slot_alloc( sizeof( *expr ) );
   expr->node.type = NODE_EXPR;
   expr->root = operand.node;
   expr->pos = pos;
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
      struct pos pos = task->tk_pos;
      t_read_tk( task );
      read_op( task, read, &rside );
      struct assign* assign = mem_alloc( sizeof( *assign ) );
      assign->node.type = NODE_ASSIGN;
      assign->op = op;
      assign->lside = operand->node;
      assign->rside = rside.node;
      assign->pos = pos;
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
      struct pos pos = task->tk_pos;
      t_read_tk( task );
      read_operand( task, expr, operand );
      add_unary( operand, op, &pos );
   }
   else {
      operand->node = NULL;
      read_primary( task, expr, operand );
      read_postfix( task, expr, operand );
   }
}

void add_unary( struct read* operand, int op, struct pos* pos ) {
   struct unary* unary = mem_alloc( sizeof( *unary ) );
   unary->node.type = NODE_UNARY;
   unary->op = op;
   unary->operand = operand->node;
   unary->pos = *pos;
   operand->node = ( struct node* ) unary;
}

void read_primary( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   if ( task->tk == TK_REGION ) {
      static struct node node = { NODE_REGION_HOST };
      operand->node = &node;
      t_read_tk( task );
   }
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
      if ( task->tk == TK_TRUE ) {
         literal->value = 1;
      }
      else {
         literal->value = 0;
      }
      literal->is_bool = true;
      operand->node = &literal->node;
      t_read_tk( task );
   }
   else if ( task->tk == TK_LIT_STRING ) {
      read_string( task, expr, operand );
   }
   else {
      struct literal* literal = mem_slot_alloc( sizeof( *literal ) );
      literal->node.type = NODE_LITERAL;
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
      t_diag( task, DIAG_POS_ERR, &task->tk_pos,
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
         struct read_expr index;
         t_init_read_expr( &index );
         t_read_expr( task, &index );
         t_test_tk( task, TK_BRACKET_R );
         t_read_tk( task );
         subscript->index = index.node;
         subscript->lside = operand->node;
         operand->node = ( struct node* ) subscript;
      }
      else if ( task->tk == TK_DOT ) {
         struct pos pos = task->tk_pos;
         t_read_tk( task );
         t_test_tk( task, TK_ID );
         operand->node = ( struct node* ) alloc_access( task->tk_text, pos,
            operand->node, false );
         t_read_tk( task );
      }
      else if ( task->tk == TK_COLON2 ) {
         struct pos pos = task->tk_pos;
         t_read_tk( task );
         t_test_tk( task, TK_ID );
         operand->node = ( struct node* ) alloc_access( task->tk_text, pos,
            operand->node, true );
         t_read_tk( task );
      }
      else if ( task->tk == TK_PAREN_L && ! expr->skip_function_call ) {
         read_call( task, expr, operand );
      }
      else if ( task->tk == TK_INC ) {
         add_unary( operand, UOP_POST_INC, &task->tk_pos );
         t_read_tk( task );
      }
      else if ( task->tk == TK_DEC ) {
         add_unary( operand, UOP_POST_DEC, &task->tk_pos );
         t_read_tk( task );
      }
      else {
         break;
      }
   }
}

struct access* alloc_access( char* name, struct pos pos, struct node* lside,
   bool is_region ) {
   struct access* access = mem_alloc( sizeof( *access ) );
   access->node.type = NODE_ACCESS;
   access->name = name;
   access->pos = pos;
   access->lside = lside;
   access->rside = NULL;
   access->is_region = is_region;
   return access;
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
   call->operand = operand->node;
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
   test->need_value = true;
   test->has_string = false;
   test->undef_err = false;
   test->undef_erred = false;
   test->accept_array = false;
}

void init_operand( struct operand* operand ) {
   operand->func = NULL;
   operand->dim = NULL;
   operand->type = NULL;
   operand->region = NULL;
   operand->name_offset = NULL;
   operand->value = 0;
   operand->is_result = false;
   operand->is_usable = false;
   operand->is_space = false;
   operand->folded = false;
   operand->state_access = false;
   operand->state_modify = false;
}

void t_test_expr( struct task* task, struct expr_test* test,
   struct expr* expr ) {
   struct operand operand;
   init_operand( &operand );
   operand.name_offset = task->region->body;
   test_expr( task, test, expr, &operand );
}

void test_expr( struct task* task, struct expr_test* test, struct expr* expr,
   struct operand* operand ) {
   if ( setjmp( test->bail ) == 0 ) {
      if ( test->need_value ) {
         operand->state_access = true;
      }
      test_node( task, test, operand, expr->root );
      if ( operand->is_result ) {
         if ( test->need_value && ! operand->is_usable ) {
            t_diag( task, DIAG_POS_ERR, &expr->pos,
               "expression does not produce a value" );
            t_bail( task );
         }
         expr->folded = operand->folded;
         expr->value = operand->value;
         test->pos = expr->pos;
      }
      else {
         t_diag( task, DIAG_POS_ERR, &expr->pos, "expression is incomplete" );
         t_bail( task );
      }
   }
}

void test_node( struct task* task, struct expr_test* test,
   struct operand* operand, struct node* node ) {
   switch ( node->type ) {
   case NODE_LITERAL:
      test_literal( task, operand, ( struct literal* ) node );
      break;
   case NODE_INDEXED_STRING_USAGE:
      test_string_usage( task, test, operand,
         ( struct indexed_string_usage* ) node );
      break;
   case NODE_NAME_USAGE:
      test_name_usage( task, test, operand, ( struct name_usage* ) node );
      break;
   case NODE_UNARY:
      test_unary( task, test, operand, ( struct unary* ) node );
      break;
   case NODE_SUBSCRIPT:
      test_subscript( task, test, operand, ( struct subscript* ) node );
      break;
   case NODE_CALL:
      test_call( task, test, operand, ( struct call* ) node );
      break;
   case NODE_BINARY:
      test_binary( task, test, operand, ( struct binary* ) node );
      break;
   case NODE_ASSIGN:
      test_assign( task, test, operand, ( struct assign* ) node );
      break;
   case NODE_ACCESS:
      test_access( task, test, operand, ( struct access* ) node );
      break;
   case NODE_REGION_HOST:
      operand->region = task->region;
      break;
   case NODE_REGION_UPMOST:
      operand->region = task->region_global;
      break;
   case NODE_PAREN:
      {
         struct paren* paren = ( struct paren* ) node;
         test_node( task, test, operand, paren->inside );
      }
      break;
   default:
      break;
   }
}

void test_literal( struct task* task, struct operand* operand,
   struct literal* literal ) {
   operand->value = literal->value;
   operand->folded = true;
   operand->is_result = true;
   operand->is_usable = true;
   if ( literal->is_bool ) {
      operand->type = task->type_bool;
   }
   else {
      operand->type = task->type_int;
   }
}

void test_string_usage( struct task* task, struct expr_test* test,
   struct operand* operand, struct indexed_string_usage* usage ) {
   operand->value = usage->string->index;
   operand->folded = true;
   operand->is_result = true;
   operand->is_usable = true;
   operand->type = task->type_str;
   test->has_string = true;
}

void test_name_usage( struct task* task, struct expr_test* test,
   struct operand* operand, struct name_usage* usage ) {
   struct object* object = find_usage_object( task, usage );
   if ( object && object->resolved ) {
      use_object( task, test, operand, object );
      usage->object = &object->node;
   }
   // Object not found or isn't valid.
   else {
      if ( test->undef_err ) {
         if ( object ) {
            t_diag( task, DIAG_POS_ERR, &usage->pos, "`%s` undefined",
               usage->text );
         }
         else {
            t_diag( task, DIAG_POS_ERR, &usage->pos, "`%s` not found",
               usage->text );
         }
         t_bail( task );
      }
      else {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
   }
}

struct object* find_usage_object( struct task* task,
   struct name_usage* usage ) {
   // Try searching in the current scope.
   struct name* name = t_make_name( task, usage->text, task->region->body );
   if ( name->object ) {
      return name->object;
   }
   // Try searching in any of the linked regions.
   struct object* object = NULL;
   struct region_link* link = task->region->link;
   while ( link && ! object ) {
      name = t_make_name( task, usage->text, link->region->body );
      object = t_get_region_object( task, link->region, name );
      link = link->next;
   }
   // Object could not be found.
   if ( ! object ) {
      return NULL;
   }
   // If an object is found through a region link, make sure no other object
   // with the same name can be found using any remaining region link. We can
   // use the first object found, but I'd rather we disallow the usage when
   // multiple objects are visible.
   int dup = 0;
   while ( link ) {
      name = t_make_name( task, usage->text, link->region->body );
      if ( name->object ) {
         if ( ! dup ) {
            t_diag( task, DIAG_POS_ERR, &usage->pos,
               "multiple objects with name `%s`", usage->text );
            t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &object->pos,
               "object found here" );
         }
         t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &name->object->pos,
            "object found here" );
         ++dup;
      }
      link = link->next;
   }
   if ( dup ) {
      t_bail( task );
   }
   return object;
}

void use_object( struct task* task, struct expr_test* test,
   struct operand* operand, struct object* object ) {
   if ( object->node.type == NODE_REGION ) {
      operand->region = ( struct region* ) object;
   }
   else if ( object->node.type == NODE_CONSTANT ) {
      struct constant* constant = ( struct constant* ) object;
      operand->type = task->type_int;
      operand->value = constant->value;
      operand->folded = true;
      operand->is_result = true;
      operand->is_usable = true;
   }
   else if ( object->node.type == NODE_VAR ) {
      struct var* var = ( struct var* ) object;
      operand->type = var->type;
      if ( var->dim ) {
         operand->dim = var->dim;
         // NOTE: I'm not too happy with the idea of this field. It is
         // contagious. Blame the Hypnotoad.
         if ( test->accept_array ) {
            operand->is_result = true;
         }
      }
      else if ( var->type->primitive ) {
         operand->is_result = true;
         operand->is_usable = true;
         operand->is_space = true;
      }
      // Keep track of how the variable is used.
      var->state_accessed = operand->state_access;
      var->state_modified = operand->state_modify;
   }
   else if ( object->node.type == NODE_TYPE_MEMBER ) {
      struct type_member* member = ( struct type_member* ) object;
      operand->type = member->type;
      if ( member->dim ) {
         operand->dim = member->dim;
         if ( test->accept_array ) {
            operand->is_result = true;
         }
      }
      else if ( member->type->primitive ) {
         operand->is_result = true;
         operand->is_usable = true;
         operand->is_space = true;
      }
   }
   else if ( object->node.type == NODE_FUNC ) {
      operand->func = ( struct func* ) object;
      if ( operand->func->type == FUNC_USER ) {
         struct func_user* impl = operand->func->impl;
         ++impl->usage;
      }
      // When using just the name of an action special, a value is produced,
      // which is the ID of the action special.
      else if ( operand->func->type == FUNC_ASPEC ) {
         operand->is_result = true;
         operand->is_usable = true;
         struct func_aspec* impl = operand->func->impl;
         operand->value = impl->id;
         operand->folded = true;
      }
   }
   else if ( object->node.type == NODE_PARAM ) {
      struct param* param = ( struct param* ) object;
      operand->type = param->type;
      operand->is_result = true;
      operand->is_usable = true;
      operand->is_space = true;
   }
   else if ( object->node.type == NODE_ALIAS ) {
      struct alias* alias = ( struct alias* ) object;
      use_object( task, test, operand, alias->target );
   }
}

void test_unary( struct task* task, struct expr_test* test,
   struct operand* operand, struct unary* unary ) {
   struct operand target;
   init_operand( &target );
   target.name_offset = operand->name_offset;
   if ( unary->op == UOP_PRE_INC || unary->op == UOP_PRE_DEC ||
      unary->op == UOP_POST_INC || unary->op == UOP_POST_DEC ) {
      target.state_modify = true;
      if ( operand->state_access ) {
         target.state_access = true;
      }
      test_node( task, test, &target, unary->operand );
      // Only an l-value can be incremented.
      if ( ! target.is_space ) {
         const char* action = "incremented";
         if ( unary->op == UOP_PRE_DEC || unary->op == UOP_POST_DEC ) {
            action = "decremented";
         }
         t_diag( task, DIAG_POS_ERR, &unary->pos, "operand cannot be %s",
            action );
         t_bail( task );
      }
   }
   else {
      target.state_access = true;
      test_node( task, test, &target, unary->operand );
      // Remaining operations require a value to work on.
      if ( ! target.is_usable ) {
         t_diag( task, DIAG_POS_ERR, &unary->pos,
            "operand of unary operation not a value" );
         t_bail( task );
      }
   }
   // Compile-time evaluation.
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
   operand->is_result = true;
   operand->is_usable = true;
   // Type of the result.
   if ( unary->op == UOP_LOG_NOT ) {
      operand->type = task->type_bool;
   }
   else {
      operand->type = task->type_int;
   }
}

void test_subscript( struct task* task, struct expr_test* test,
   struct operand* operand, struct subscript* subscript ) {
   struct operand lside;
   init_operand( &lside );
   lside.name_offset = operand->name_offset;
   if ( operand->state_modify ) {
      lside.state_modify = true;
      lside.state_access = operand->state_access;
   }
   else {
      lside.state_access = true;
   }
   test_node( task, test, &lside, subscript->lside );
   if ( ! lside.dim ) {
      t_diag( task, DIAG_POS_ERR, &subscript->pos, "operand not an array" );
      t_bail( task );
   }
   struct expr_test index;
   t_init_expr_test( &index );
   index.format_block = test->format_block;
   index.need_value = true;
   index.undef_err = test->undef_err;
   t_test_expr( task, &index, subscript->index );
   if ( index.undef_erred ) {
      test->undef_erred = true;
      longjmp( test->bail, 1 );
   }
   // Out-of-bounds warning for a constant index.
   if ( lside.dim->size && subscript->index->folded && (
      subscript->index->value < 0 ||
      subscript->index->value >= lside.dim->size ) ) {
      t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &subscript->index->pos, "array index out-of-bounds" );
   }
   operand->type = lside.type;
   operand->dim = lside.dim->next;
   if ( operand->dim ) {
      if ( test->accept_array ) {
         operand->is_result = true;
      }
   }
   else {
      operand->is_result = true;
      operand->is_usable = true;
      operand->is_space = true;
   }
}

void test_call( struct task* task, struct expr_test* test,
   struct operand* operand, struct call* call ) {
   struct operand callee;
   init_operand( &callee );
   callee.name_offset = operand->name_offset;
   test_node( task, test, &callee, call->operand );
   if ( ! callee.func ) {
      t_diag( task, DIAG_POS_ERR, &call->pos, "operand not a function" );
      t_bail( task );
   }
   // Some action specials cannot be called from a script.
   if ( callee.func->type == FUNC_ASPEC ) {
      struct func_aspec* impl = callee.func->impl;
      if ( ! impl->script_callable ) {
         struct str str;
         str_init( &str );
         t_copy_name( callee.func->name, false, &str );
         t_diag( task, DIAG_POS_ERR, &call->pos,
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
         t_diag( task, DIAG_POS_ERR, &call->pos,
            "function call missing format argument" );
         t_bail( task );
      }
      // Format-block:
      if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
         if ( ! test->format_block ) {
            t_diag( task, DIAG_POS_ERR, &call->pos,
               "function call missing format block" );
            t_bail( task );
         }
         struct format_block_usage* usage =
            ( struct format_block_usage* ) node;
         usage->block = test->format_block;
         // Attach usage to the end of the usage list.
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
            if ( node->type != NODE_FORMAT_ITEM ) {
               break;
            }
            t_test_format_item( task, ( struct format_item* ) node,
               test->stmt_test, test, operand->name_offset,
               test->format_block );
            list_next( &i );
         }
      }
      // Both a format-block or a format-list count as a single argument.
      ++count;
   }
   else {
      if ( list_size( &call->args ) ) {
         struct node* node = list_data( &i );
         if ( node->type == NODE_FORMAT_ITEM ) {
            struct format_item* item = ( struct format_item* ) node;
            t_diag( task, DIAG_POS_ERR, &item->pos,
               "passing format item to non-format function" );
            t_bail( task );
         }
         else if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
            struct format_block_usage* usage =
               ( struct format_block_usage* ) node;
            t_diag( task, DIAG_POS_ERR, &usage->pos,
               "passing format block to non-format function" );
            t_bail( task );
         }
      }
   }
   // Arguments:
   while ( ! list_end( &i ) ) {
      struct expr_test arg;
      t_init_expr_test( &arg );
      arg.format_block = test->format_block;
      arg.need_value = true;
      arg.undef_err = test->undef_err;
      t_test_expr( task, &arg, list_data( &i ) );
      if ( arg.undef_erred ) {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
      list_next( &i );
      ++count;
   }
   if ( count < callee.func->min_param ) {
      t_diag( task, DIAG_POS_ERR, &call->pos,
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
      t_diag( task, DIAG_POS_ERR, &call->pos,
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
   // Latent function cannot be called in a function or a format block.
   if ( callee.func->type == FUNC_DED ) {
      struct func_ded* impl = callee.func->impl;
      if ( impl->latent ) {
         bool erred = false;
         struct stmt_test* stmt = test->stmt_test;
         while ( stmt && ! stmt->format_block ) {
            stmt = stmt->parent;
         }
         if ( stmt ) {
            t_diag( task, DIAG_POS_ERR, &call->pos,
               "calling latent function inside format block" );
            erred = true;
         }
         else {
            if ( task->in_func ) {
               t_diag( task, DIAG_POS_ERR, &call->pos,
                  "calling latent function inside function body" );
               erred = true;
            }
         }
         // Show small educational note and bail.
         if ( erred ) {
            struct str str;
            str_init( &str );
            t_copy_name( callee.func->name, false, &str );
            t_diag( task, DIAG_FILE, &call->pos,
               "waiting functions like `%s` can only be called inside a "
               "script", str.value );
            t_bail( task );
         }
      }
   }
   call->func = callee.func;
   operand->type = callee.func->value;
   operand->is_result = true;
   // Only functions that return a value produce a usable value.
   if ( operand->type ) {
      operand->is_usable = true;
   }
}

// This function handles a format-item found inside a function call,
// and a free format-item, the one found in a format-block.
void t_test_format_item( struct task* task, struct format_item* item,
   struct stmt_test* stmt_test, struct expr_test* expr_test,
   struct name* name_offset, struct block* format_block ) {
   while ( item ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.stmt_test = stmt_test;
      expr.format_block = format_block;
      if ( expr_test ) {
         expr.undef_err = expr_test->undef_err;
      }
      else {
         expr.undef_err = true;
      }
      // When using the array cast, make acceptable an expression whose
      // result is an array.
      if ( item->cast == FCAST_ARRAY ) {
         expr.accept_array = true;
         expr.need_value = false;
      }
      struct operand arg;
      init_operand( &arg );
      arg.name_offset = name_offset;
      test_expr( task, &expr, item->expr, &arg );
      if ( expr.undef_erred ) {
         expr_test->undef_erred = true;
         longjmp( expr_test->bail, 1 );
      }
      if ( item->cast == FCAST_ARRAY ) {
         if ( ! arg.dim ) {
            t_diag( task, DIAG_POS_ERR, &item->expr->pos,
               "argument not an array" );
            t_bail( task );
         }
         if ( arg.dim->next ) {
            t_diag( task, DIAG_POS_ERR, &item->expr->pos,
               "array argument not of single dimension" );
            t_bail( task );
         }
      }
      item = item->next;
   }
}

void test_binary( struct task* task, struct expr_test* test,
   struct operand* operand, struct binary* binary ) {
   struct operand lside;
   init_operand( &lside );
   lside.name_offset = operand->name_offset;
   lside.state_access = true;
   test_node( task, test, &lside, binary->lside );
   if ( ! lside.is_usable ) {
      t_diag( task, DIAG_POS_ERR, &binary->pos,
         "operand on left side not a value" );
      t_bail( task );
   }
   struct operand rside;
   init_operand( &rside );
   rside.name_offset = operand->name_offset;
   rside.state_access = true;
   test_node( task, test, &rside, binary->rside );
   if ( ! rside.is_usable ) {
      t_diag( task, DIAG_POS_ERR, &binary->pos,
         "operand on right side not a value" );
      t_bail( task );
   }
   // Compile-time evaluation.
   if ( lside.folded && rside.folded ) {
      int l = lside.value;
      int r = rside.value;
      // Division and modulo get special treatment because of the possibility
      // of a division by zero.
      if ( ( binary->op == BOP_DIV || binary->op == BOP_MOD ) && ! r ) {
         t_diag( task, DIAG_POS_ERR, &binary->pos, "division by zero" );
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
   operand->is_result = true;
   operand->is_usable = true;
   // Type of the result.
   switch ( binary->op ) {
   case BOP_NONE:
   case BOP_MOD:
   case BOP_MUL:
   case BOP_DIV:
   case BOP_ADD:
   case BOP_SUB:
   case BOP_SHIFT_L:
   case BOP_SHIFT_R:
   case BOP_BIT_AND:
   case BOP_BIT_XOR:
   case BOP_BIT_OR:
      operand->type = task->type_int;
      break;
   case BOP_GTE:
   case BOP_GT:
   case BOP_LTE:
   case BOP_LT:
   case BOP_NEQ:
   case BOP_EQ:
   case BOP_LOG_AND:
   case BOP_LOG_OR:
      operand->type = task->type_bool;
      break;
   }
}

void test_assign( struct task* task, struct expr_test* test,
   struct operand* operand, struct assign* assign ) {
   struct operand lside;
   init_operand( &lside );
   lside.name_offset = operand->name_offset;
   lside.state_modify = true;
   lside.state_access = operand->state_access;
   test_node( task, test, &lside, assign->lside );
   if ( ! lside.is_space ) {
      t_diag( task, DIAG_POS_ERR, &assign->pos,
         "cannot assign to operand on left side" );
      t_bail( task );
   }
   struct operand rside;
   init_operand( &rside );
   rside.name_offset = operand->name_offset;
   rside.state_access = true;
   test_node( task, test, &rside, assign->rside );
   if ( ! rside.is_usable ) {
      t_diag( task, DIAG_POS_ERR, &assign->pos,
         "right side of assignment not a value" );
      t_bail( task );
   }
   operand->is_result = true;
   operand->is_usable = true;
   operand->type = lside.type;
}

void test_access( struct task* task, struct expr_test* test,
   struct operand* operand, struct access* access ) {
   struct object* object = NULL;
   struct operand lside;
   init_operand( &lside );
   lside.name_offset = operand->name_offset;
   if ( operand->state_modify ) {
      lside.state_modify = true;
      lside.state_access = operand->state_access;
   }
   else {
      lside.state_access = true;
   }
   test_node( task, test, &lside, access->lside );
   // `::` operator.
   if ( access->is_region ) {
      if ( lside.region ) {
         struct name* name = t_make_name( task, access->name,
            lside.region->body );
         object = t_get_region_object( task, lside.region, name );
         if ( ! object ) {
            if ( ! test->undef_err ) {
               test->undef_erred = true;
               longjmp( test->bail, 0 );
            }
            t_diag( task, DIAG_POS_ERR, &access->pos,
               "`%s` not found in region", access->name );
            t_bail( task );
         }
      }
      else {
         t_diag( task, DIAG_POS_ERR, &access->pos,
            "left operand not a region" );
         t_bail( task );
      }
   }
   // `.` operator.
   else {
      if ( lside.type && ! lside.dim ) {
         struct name* name = t_make_name( task, access->name,
            lside.type->body );
         object = name->object;
         if ( ! object ) {
            // The right operand might be a member of a struct type that hasn't
            // been processed yet because the struct appears later in the
            // source code. Leave for now.
            if ( ! test->undef_err ) {
               test->undef_erred = true;
               longjmp( test->bail, 0 );
            }
            // Anonymous struct.
            if ( lside.type->anon ) {
               t_diag( task, DIAG_POS_ERR, &access->pos,
                  "`%s` not member of anonymous struct", access->name );
               t_bail( task );
            }
            // Named struct.
            else {
               struct str str;
               str_init( &str );
               t_copy_name( lside.type->name, false, &str );
               t_diag( task, DIAG_POS_ERR, &access->pos,
                  "`%s` not member of struct `%s`", access->name,
                  str.value );
               t_bail( task );
            }
         }
      }
      else {
         t_diag( task, DIAG_POS_ERR, &access->pos,
            "left operand does not work with . operator" );
         t_bail( task );
      }
   }
   // Object needs to be valid before proceeding.
   if ( object->resolved ) {
      use_object( task, test, operand, object );
      access->rside = ( struct node* ) object;
   }
   else {
      if ( test->undef_err ) {
         t_diag( task, DIAG_POS_ERR, &access->pos, "operand `%s` undefined",
            access->name );
         t_bail( task );
      }
      else {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
   }
}