#include <errno.h>
#include <string.h>
#include <limits.h>

#include "task.h"

struct operand {
   struct func* func;
   struct dim* dim;
   struct region* region;
   struct type* type;
   int value;
   bool complete;
   bool usable;
   bool assignable;
   bool folded;
   bool in_paren;
};

struct call_test {
   struct call* call;
   struct func* func;
   list_iter_t* i;
   int num_args;
};

static void read_op( struct task*, struct expr_reading* );
static void read_operand( struct task*, struct expr_reading* );
static struct binary* alloc_binary( int, struct pos* );
static struct unary* alloc_unary( int, struct pos* );
static void read_primary( struct task*, struct expr_reading* );
static void read_postfix( struct task*, struct expr_reading* );
static struct access* alloc_access( char*, struct pos );
static void read_call( struct task*, struct expr_reading* );
static void read_call_args( struct task* task, struct expr_reading*,
   struct list* );
static struct format_item* read_format_item( struct task*, bool colon );
static void read_format_item_array_value( struct task*, struct format_item* );
static void read_string( struct task*, struct expr_reading* );
static struct indexed_string* intern_indexed_string( struct task*, char* value,
   int length, bool* first_time );
static int convert_numerictoken_to_int( struct task*, int base );
static void test_expr( struct task*, struct expr_test*, struct expr*,
   struct operand* );
static void init_operand( struct operand* );
static void use_object( struct task*, struct expr_test*, struct operand*,
   struct object* );
static void test_node( struct task*, struct expr_test*, struct operand*,
   struct node* );
static void test_literal( struct task*, struct operand*, struct literal* );
static void test_string_usage( struct task*, struct expr_test*,
   struct operand*, struct indexed_string_usage* );
static void test_boolean( struct task*, struct operand*, struct boolean* );
static void test_name_usage( struct task*, struct expr_test*, struct operand*,
   struct name_usage* );
static struct object* find_usage_object( struct task*, struct name_usage* );
static void test_unary( struct task*, struct expr_test*, struct operand*,
   struct unary* );
static void test_subscript( struct task*, struct expr_test*, struct operand*,
   struct subscript* );
static void test_call( struct task*, struct expr_test*, struct operand*,
   struct call* );
static void test_call_args( struct task*, struct expr_test*,
   struct call_test* );
static void test_call_first_arg( struct task*, struct expr_test*,
   struct call_test* );
static void test_call_format_arg( struct task*, struct expr_test*,
   struct call_test* );
static void test_binary( struct task*, struct expr_test*, struct operand*,
   struct binary* );
static void test_assign( struct task*, struct expr_test*, struct operand*,
   struct assign* );
static void test_access( struct task*, struct expr_test*, struct operand*,
   struct access* );

void t_init_expr_reading( struct expr_reading* reading, bool in_constant,
   bool skip_assign, bool skip_call, bool expect_expr ) {
   reading->node = NULL;
   reading->output_node = NULL;
   reading->has_str = false;
   reading->in_constant = in_constant;
   reading->skip_assign = skip_assign;
   reading->skip_call = skip_call;
   reading->expect_expr = expect_expr;
}

void t_read_expr( struct task* task, struct expr_reading* reading ) {
   reading->pos = task->tk_pos;
   read_op( task, reading );
   struct expr* expr = mem_slot_alloc( sizeof( *expr ) );
   expr->node.type = NODE_EXPR;
   expr->root = reading->node;
   expr->pos = reading->pos;
   expr->value = 0;
   expr->folded = false;
   expr->has_str = reading->has_str;
   reading->output_node = expr;
}

void read_op( struct task* task, struct expr_reading* reading ) {
   int op = 0;
   struct binary* mul = NULL;
   struct binary* add = NULL;
   struct binary* shift = NULL;
   struct binary* lt = NULL;
   struct binary* eq = NULL;
   struct binary* bit_and = NULL;
   struct binary* bit_xor = NULL;
   struct binary* bit_or = NULL;
   struct binary* log_and = NULL;
   struct binary* log_or = NULL;

   top:
   read_operand( task, reading );

   op_mul:
   // -----------------------------------------------------------------------
   switch ( task->tk ) {
   case TK_STAR:
      op = BOP_MUL;
      break;
   case TK_SLASH:
      op = BOP_DIV;
      break;
   case TK_MOD:
      op = BOP_MOD;
      break;
   default:
      goto op_add;
   }
   mul = alloc_binary( op, &task->tk_pos );
   mul->lside = reading->node;
   t_read_tk( task );
   read_operand( task, reading );
   mul->rside = reading->node;
   reading->node = &mul->node;
   goto op_mul;

   op_add:
   // -----------------------------------------------------------------------
   if ( add ) {
      add->rside = reading->node;
      reading->node = &add->node;
      add = NULL;
   }
   switch ( task->tk ) {
   case TK_PLUS:
      op = BOP_ADD;
      break;
   case TK_MINUS:
      op = BOP_SUB;
      break;
   default:
      goto op_shift;
   }
   add = alloc_binary( op, &task->tk_pos );
   add->lside = reading->node;
   t_read_tk( task );
   goto top;

   op_shift:
   // -----------------------------------------------------------------------
   if ( shift ) {
      shift->rside = reading->node;
      reading->node = &shift->node;
      shift = NULL;
   }
   switch ( task->tk ) {
   case TK_SHIFT_L:
      op = BOP_SHIFT_L;
      break;
   case TK_SHIFT_R:
      op = BOP_SHIFT_R;
      break;
   default:
      goto op_lt;
   }
   shift = alloc_binary( op, &task->tk_pos );
   shift->lside = reading->node;
   t_read_tk( task );
   goto top;

   op_lt:
   // -----------------------------------------------------------------------
   if ( lt ) {
      lt->rside = reading->node;
      reading->node = &lt->node;
      lt = NULL;
   }
   switch ( task->tk ) {
   case TK_LT:
      op = BOP_LT;
      break;
   case TK_LTE:
      op = BOP_LTE;
      break;
   case TK_GT:
      op = BOP_GT;
      break;
   case TK_GTE:
      op = BOP_GTE;
      break;
   default:
      goto op_eq;
   }
   lt = alloc_binary( op, &task->tk_pos );
   lt->lside = reading->node;
   t_read_tk( task );
   goto top;

   op_eq:
   // -----------------------------------------------------------------------
   if ( eq ) {
      eq->rside = reading->node;
      reading->node = &eq->node;
      eq = NULL;
   }
   switch ( task->tk ) {
   case TK_EQ:
      op = BOP_EQ;
      break;
   case TK_NEQ:
      op = BOP_NEQ;
      break;
   default:
      goto op_bit_and;
   }
   eq = alloc_binary( op, &task->tk_pos );
   eq->lside = reading->node;
   t_read_tk( task );
   goto top;

   op_bit_and:
   // -----------------------------------------------------------------------
   if ( bit_and ) {
      bit_and->rside = reading->node;
      reading->node = &bit_and->node;
      bit_and = NULL;
   }
   if ( task->tk == TK_BIT_AND ) {
      bit_and = alloc_binary( BOP_BIT_AND, &task->tk_pos );
      bit_and->lside = reading->node;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_xor ) {
      bit_xor->rside = reading->node;
      reading->node = &bit_xor->node;
      bit_xor = NULL;
   }
   if ( task->tk == TK_BIT_XOR ) {
      bit_xor = alloc_binary( BOP_BIT_XOR, &task->tk_pos );
      bit_xor->lside = reading->node;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_or ) {
      bit_or->rside = reading->node;
      reading->node = &bit_or->node;
      bit_or = NULL;
   }
   if ( task->tk == TK_BIT_OR ) {
      bit_or = alloc_binary( BOP_BIT_OR, &task->tk_pos );
      bit_or->lside = reading->node;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_and ) {
      log_and->rside = reading->node;
      reading->node = &log_and->node;
      log_and = NULL;
   }
   if ( task->tk == TK_LOG_AND ) {
      log_and = alloc_binary( BOP_LOG_AND, &task->tk_pos );
      log_and->lside = reading->node;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_or ) {
      log_or->rside = reading->node;
      reading->node = &log_or->node;
      log_or = NULL;
   }
   if ( task->tk == TK_LOG_OR ) {
      log_or = alloc_binary( BOP_LOG_OR, &task->tk_pos );
      log_or->lside = reading->node;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( ! reading->skip_assign ) {
      switch ( task->tk ) {
      case TK_ASSIGN:
         op = AOP_NONE;
         break;
      case TK_ASSIGN_ADD:
         op = AOP_ADD;
         break;
      case TK_ASSIGN_SUB:
         op = AOP_SUB;
         break;
      case TK_ASSIGN_MUL:
         op = AOP_MUL;
         break;
      case TK_ASSIGN_DIV:
         op = AOP_DIV;
         break;
      case TK_ASSIGN_MOD:
         op = AOP_MOD;
         break;
      case TK_ASSIGN_SHIFT_L:
         op = AOP_SHIFT_L;
         break;
      case TK_ASSIGN_SHIFT_R:
         op = AOP_SHIFT_R;
         break;
      case TK_ASSIGN_BIT_AND:
         op = AOP_BIT_AND;
         break;
      case TK_ASSIGN_BIT_XOR:
         op = AOP_BIT_XOR;
         break;
      case TK_ASSIGN_BIT_OR:
         op = AOP_BIT_OR;
         break;
      // Finish:
      default:
         return;
      }
      struct assign* assign = mem_alloc( sizeof( *assign ) );
      assign->node.type = NODE_ASSIGN;
      assign->op = op;
      assign->lside = reading->node;
      assign->rside = NULL;
      assign->pos = task->tk_pos;
      t_read_tk( task );
      read_op( task, reading );
      assign->rside = reading->node;
      reading->node = &assign->node;
   }
}

struct binary* alloc_binary( int op, struct pos* pos ) {
   struct binary* binary = mem_slot_alloc( sizeof( *binary ) );
   binary->node.type = NODE_BINARY;
   binary->op = op;
   binary->lside = NULL;
   binary->rside = NULL;
   binary->pos = *pos;
   return binary;
}

void read_operand( struct task* task, struct expr_reading* reading ) {
   int op = UOP_NONE;
   switch ( task->tk ) {
   case TK_INC:
      op = UOP_PRE_INC;
      break;
   case TK_DEC:
      op = UOP_PRE_DEC;
      break;
   case TK_MINUS:
      op = UOP_MINUS;
      break;
   case TK_PLUS:
      op = UOP_PLUS;
      break;
   case TK_LOG_NOT:
      op = UOP_LOG_NOT;
      break;
   case TK_BIT_NOT:
      op = UOP_BIT_NOT;
      break;
   default:
      break;
   }
   if ( op != UOP_NONE ) {
      struct pos pos = task->tk_pos;
      t_read_tk( task );
      read_operand( task, reading );
      struct unary* unary = alloc_unary( op, &pos );
      unary->operand = reading->node;
      reading->node = &unary->node;
   }
   else {
      read_primary( task, reading );
      read_postfix( task, reading );
   }
}

struct unary* alloc_unary( int op, struct pos* pos ) {
   struct unary* unary = mem_alloc( sizeof( *unary ) );
   unary->node.type = NODE_UNARY;
   unary->op = op;
   unary->operand = NULL;
   unary->pos = *pos;
   return unary;
}

void read_primary( struct task* task, struct expr_reading* reading ) {
   if ( task->tk == TK_ID ) {
      struct name_usage* usage = mem_slot_alloc( sizeof( *usage ) );
      usage->node.type = NODE_NAME_USAGE;
      usage->text = task->tk_text;
      usage->pos = task->tk_pos;
      usage->object = NULL;
      usage->lib_id = task->library->id;
      reading->node = &usage->node;
      t_read_tk( task );
   }
   else if ( task->tk == TK_LIT_STRING ) {
      read_string( task, reading );
   }
   else if ( task->tk == TK_REGION ) {
      static struct node node = { NODE_REGION_HOST };
      reading->node = &node;
      t_read_tk( task );
   }
   else if ( task->tk == TK_UPMOST ) {
      static struct node node = { NODE_REGION_UPMOST };
      reading->node = &node;
      t_read_tk( task );
   }
   else if ( task->tk == TK_TRUE ) {
      static struct boolean boolean = { { NODE_BOOLEAN }, 1 };
      reading->node = &boolean.node;
      t_read_tk( task );
   }
   else if ( task->tk == TK_FALSE ) {
      static struct boolean boolean = { { NODE_BOOLEAN }, 0 };
      reading->node = &boolean.node;
      t_read_tk( task );
   }
   else if ( task->tk == TK_PAREN_L ) {
      struct paren* paren = mem_alloc( sizeof( *paren ) );
      paren->node.type = NODE_PAREN;
      t_read_tk( task );
      read_op( task, reading );
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      paren->inside = reading->node;
      reading->node = &paren->node;
   }
   else {
      switch ( task->tk ) {
      case TK_LIT_DECIMAL:
      case TK_LIT_OCTAL:
      case TK_LIT_HEX:
      case TK_LIT_BINARY:
      case TK_LIT_FIXED:
      case TK_LIT_CHAR:
         break;
      default:
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "unexpected %s", t_get_token_name( task->tk ) );
         // For areas of code where an expression is to be expected. Only show
         // this message when not a single piece of the expression is read.
         if ( reading->expect_expr &&
            t_same_pos( &task->tk_pos, &reading->pos ) ) {
            t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
               "expecting an expression here" );
         }
         t_bail( task );
      }
      struct literal* literal = mem_slot_alloc( sizeof( *literal ) );
      literal->node.type = NODE_LITERAL;
      literal->value = t_extract_literal_value( task );
      reading->node = &literal->node;
      t_read_tk( task );
   }
}

void read_string( struct task* task, struct expr_reading* reading ) {
   t_test_tk( task, TK_LIT_STRING );
   bool first_time = false;
   struct indexed_string* string = intern_indexed_string( task, task->tk_text,
      task->tk_length, &first_time );
   string->in_constant = reading->in_constant;
   string->in_main_file = ( task->source == task->source_main );
   if ( first_time ) {
      string->imported = task->library->imported;
   }
   else {
      // If an imported string is found in the main library, then the string
      // should NOT be considered imported.
      if ( task->library == task->library_main ) {
         string->imported = false;
      }
   }
   struct indexed_string_usage* usage = mem_slot_alloc( sizeof( *usage ) );
   usage->node.type = NODE_INDEXED_STRING_USAGE;
   usage->string = string;
   reading->node = &usage->node;
   reading->has_str = true;
   t_read_tk( task );
}

struct indexed_string* intern_indexed_string( struct task* task, char* value,
   int length, bool* first_time ) {
   struct indexed_string* prev_string = NULL;
   struct indexed_string* string = task->str_table.head_sorted;
   while ( string ) {
      int result = strcmp( string->value, value );
      if ( result == 0 ) {
         break;
      }
      else if ( result > 0 ) {
         string = NULL;
         break;
      }
      prev_string = string;
      string = string->next_sorted;
   }
   // Allocate a new indexed-string when one isn't interned.
   if ( ! string ) {
      string = mem_alloc( sizeof( *string ) );
      string->value = value;
      string->length = length;
      string->index = 0;
      string->next = NULL;
      string->next_sorted = NULL;
      string->next_usable = NULL;
      string->in_constant = false;
      string->used = false;
      string->imported = false;
      string->in_main_file = false;
      if ( task->str_table.head ) {
         task->str_table.tail->next = string;
      }
      else {
         task->str_table.head = string;
      }
      task->str_table.tail = string;
      // List sorted alphabetically.
      if ( prev_string ) {
         string->next_sorted = prev_string->next_sorted;
         prev_string->next_sorted = string;
      }
      else {
         string->next_sorted = task->str_table.head_sorted;
         task->str_table.head_sorted = string;
      }
      if ( first_time ) {
         *first_time = true;
      }
   }
   return string;
}

int t_extract_literal_value( struct task* task ) {
   if ( task->tk == TK_LIT_DECIMAL ) {
      return convert_numerictoken_to_int( task, 10 );
   }
   else if ( task->tk == TK_LIT_OCTAL ) {
      return convert_numerictoken_to_int( task, 8 );
   }
   else if ( task->tk == TK_LIT_HEX ) {
      // NOTE: The hexadecimal literal is converted without considering whether
      // the number can fit into a signed integer, the destination type.
      int value = 0;
      int i = 0;
      while ( task->tk_text[ i ] ) {
         int digit_value = 0;
         switch ( task->tk_text[ i ] ) {
         case 'a': case 'A': digit_value = 0xA; break;
         case 'b': case 'B': digit_value = 0xB; break;
         case 'c': case 'C': digit_value = 0xC; break;
         case 'd': case 'D': digit_value = 0xD; break;
         case 'e': case 'E': digit_value = 0xE; break;
         case 'f': case 'F': digit_value = 0xF; break;
         default:
            digit_value = task->tk_text[ i ] - '0';
            break;
         }
         // NOTE: Undefined behavior may occur when shifting of a signed
         // integer leads to overflow.
         value = ( ( value << 4 ) | digit_value );
         ++i;
      }
      return value;
   }
   else if ( task->tk == TK_LIT_BINARY ) {
      unsigned int temp = 0;
      for ( int i = 0; task->tk_text[ i ]; ++i ) {
         temp = ( temp << 1 ) | ( task->tk_text[ i ] - '0' );
      }
      int value = 0;
      memcpy( &value, &temp, sizeof( value ) );
      return value;
   }
   else if ( task->tk == TK_LIT_FIXED ) {
      double value = atof( task->tk_text );
      return
         // Whole.
         ( ( int ) value << 16 ) +
         // Fraction.
         ( int ) ( ( 1 << 16 ) * ( value - ( int ) value ) );
   }
   else if ( task->tk == TK_LIT_CHAR ) {
      return task->tk_text[ 0 ];
   }
   else {
      return 0;
   }
}

// Maximum positive number the game engine will consider valid. It is assumed
// that the `long` type on the host system running the compiler can hold this
// value.
#define ENGINE_MAX_INT_VALUE 2147483647

// It is assumed this function accepts a numeric token as input. Token of a
// negative number is not possible because negating a number is a unary
// operation, and is done elsewhere.
// TODO: Do underflow/overflow check on intermediate results of a constant
// expression, not just on a literal operand.
int convert_numerictoken_to_int( struct task* task, int base ) {
   errno = 0;
   char* temp = NULL;
   long value = strtol( task->tk_text, &temp, base );
   // NOTE: The token should already be of the form that strtol() accepts.
   // Maybe make this an internal compiler error, as a sanity check?
   if ( temp == task->tk_text || *temp != '\0' ) {
      t_diag( task, DIAG_POS_ERR, &task->tk_pos,
         "invalid numeric value `%s`", task->tk_text );
      t_bail( task );
   }
   if ( ( value == LONG_MAX && errno == ERANGE ) ||
      value > ( long ) ENGINE_MAX_INT_VALUE ) {
      t_diag( task, DIAG_POS_ERR, &task->tk_pos,
         "numeric value `%s` is too large", task->tk_text );
      t_bail( task );
   }
   return ( int ) value;
}

void read_postfix( struct task* task, struct expr_reading* reading ) {
   while ( true ) {
      if ( task->tk == TK_BRACKET_L ) {
         struct subscript* sub = mem_alloc( sizeof( *sub ) );
         sub->node.type = NODE_SUBSCRIPT;
         sub->pos = task->tk_pos;
         t_read_tk( task );
         struct expr_reading index;
         t_init_expr_reading( &index, reading->in_constant, false, false,
            true );
         t_read_expr( task, &index );
         t_test_tk( task, TK_BRACKET_R );
         t_read_tk( task );
         sub->index = index.output_node;
         sub->lside = reading->node;
         reading->node = &sub->node;
      }
      else if ( task->tk == TK_DOT ) {
         struct pos pos = task->tk_pos;
         t_read_tk( task );
         t_test_tk( task, TK_ID );
         struct access* access = alloc_access( task->tk_text, pos );
         access->lside = reading->node;
         reading->node = &access->node;
         t_read_tk( task );
      }
      else if ( task->tk == TK_COLON_2 ) {
         struct pos pos = task->tk_pos;
         t_read_tk( task );
         t_test_tk( task, TK_ID );
         struct access* access = alloc_access( task->tk_text, pos );
         access->lside = reading->node;
         access->is_region = true;
         reading->node = &access->node;
         t_read_tk( task );
      }
      else if ( task->tk == TK_PAREN_L ) {
         if ( ! reading->skip_call ) {
            read_call( task, reading );
         }
         else {
            break;
         }
      }
      else if ( task->tk == TK_INC ) {
         struct unary* inc = alloc_unary( UOP_POST_INC, &task->tk_pos );
         inc->operand = reading->node;
         reading->node = &inc->node;
         t_read_tk( task );
      }
      else if ( task->tk == TK_DEC ) {
         struct unary* dec = alloc_unary( UOP_POST_DEC, &task->tk_pos );
         dec->operand = reading->node;
         reading->node = &dec->node;
         t_read_tk( task );
      }
      else {
         break;
      }
   }
}

struct access* alloc_access( char* name, struct pos pos ) {
   struct access* access = mem_alloc( sizeof( *access ) );
   access->node.type = NODE_ACCESS;
   access->name = name;
   access->pos = pos;
   access->lside = NULL;
   access->rside = NULL;
   access->is_region = false;
   return access;
}

void read_call( struct task* task, struct expr_reading* reading ) {
   struct pos pos = task->tk_pos;
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   struct list args;
   list_init( &args );
   // Format list:
   if ( task->tk == TK_ID && t_peek( task ) == TK_COLON ) {
      list_append( &args, t_read_format_item( task, true ) );
      if ( task->tk == TK_SEMICOLON ) {
         t_read_tk( task );
         read_call_args( task, reading, &args );
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
         read_call_args( task, reading, &args );
      }
   }
   else {
      // This relic is not necessary in new code. The compiler is smart enough
      // to figure out when to use the constant variant of an instruction.
      if ( task->tk == TK_CONST ) {
         t_read_tk( task );
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
      }
      if ( task->tk != TK_PAREN_R ) {
         read_call_args( task, reading, &args );
      }
   }
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   struct call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_CALL;
   call->pos = pos;
   call->operand = reading->node;
   call->func = NULL;
   call->args = args;
   reading->node = &call->node;
}

void read_call_args( struct task* task, struct expr_reading* reading,
   struct list* args ) {
   while ( true ) {
      struct expr_reading arg;
      t_init_expr_reading( &arg, reading->in_constant, false, false, true );
      t_read_expr( task, &arg );
      list_append( args, arg.output_node );
      if ( task->tk == TK_COMMA ) {
         t_read_tk( task );
      }
      else {
         break;
      }
   }
}

struct format_item* t_read_format_item( struct task* task, bool colon ) {
   struct format_item* head = NULL;
   struct format_item* tail;
   while ( true ) {
      struct format_item* item = read_format_item( task, colon );
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

struct format_item* read_format_item( struct task* task, bool colon ) {
   t_test_tk( task, TK_ID );
   struct format_item* item = mem_alloc( sizeof( *item ) );
   item->node.type = NODE_FORMAT_ITEM;
   item->cast = FCAST_DECIMAL;
   item->pos = task->tk_pos;
   item->next = NULL;
   item->value = NULL;
   item->extra = NULL;
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
      t_diag( task, DIAG_POS_ERR, &task->tk_pos, "unknown format cast `%s`",
         task->tk_text );
      t_bail( task );
   }
   t_read_tk( task );
   // In a format block, the `:=` separator is used, because an identifier
   // and a colon creates a goto-statement label.
   if ( colon ) {
      t_test_tk( task, TK_COLON );
      t_read_tk( task );
   }
   else {
      t_test_tk( task, TK_ASSIGN_COLON );
      t_read_tk( task );
   }
   if ( item->cast == FCAST_ARRAY ) {
      read_format_item_array_value( task, item );
   }
   else {
      struct expr_reading value;
      t_init_expr_reading( &value, false, false, false, true );
      t_read_expr( task, &value );
      item->value = value.output_node;
   }
   return item;
}

void read_format_item_array_value( struct task* task,
   struct format_item* item ) {
   bool paren = false;
   if ( task->tk == TK_PAREN_L ) {
      paren = true;
      t_read_tk( task );
   }
   // Array field.
   struct expr_reading expr;
   t_init_expr_reading( &expr, false, false, false, true );
   t_read_expr( task, &expr );
   item->value = expr.output_node;
   if ( paren ) {
      // Offset field.
      if ( task->tk == TK_COMMA ) {
         t_read_tk( task );
         t_init_expr_reading( &expr, false, false, false, true );
         t_read_expr( task, &expr );
         struct expr* offset = expr.output_node;
         // Length field.
         struct expr* length = NULL;
         if ( task->tk == TK_COMMA ) {
            t_read_tk( task );
            t_init_expr_reading( &expr, false, false, false, true );
            t_read_expr( task, &expr );
            length = expr.output_node;
         }
         struct format_item_array* extra = mem_alloc( sizeof( *extra ) );
         extra->offset = offset;
         extra->length = length;
         item->extra = extra;
      }
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
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
   test->suggest_paren_assign = false;
}

void init_operand( struct operand* operand ) {
   operand->func = NULL;
   operand->dim = NULL;
   operand->type = NULL;
   operand->region = NULL;
   operand->value = 0;
   operand->complete = false;
   operand->usable = false;
   operand->assignable = false;
   operand->folded = false;
   operand->in_paren = false;
}

void t_test_expr( struct task* task, struct expr_test* test,
   struct expr* expr ) {
   struct operand operand;
   init_operand( &operand );
   test_expr( task, test, expr, &operand );
}

void test_expr( struct task* task, struct expr_test* test, struct expr* expr,
   struct operand* operand ) {
   if ( setjmp( test->bail ) == 0 ) {
      test_node( task, test, operand, expr->root );
      if ( operand->complete ) {
         if ( test->need_value && ! operand->usable ) {
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
   case NODE_BOOLEAN:
      test_boolean( task, operand, ( struct boolean* ) node );
      break;
   case NODE_REGION_HOST:
      operand->region = task->region;
      break;
   case NODE_REGION_UPMOST:
      operand->region = task->region_upmost;
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
   case NODE_PAREN:
      {
         operand->in_paren = true;
         struct paren* paren = ( struct paren* ) node;
         test_node( task, test, operand, paren->inside );
         operand->in_paren = false;
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
   operand->complete = true;
   operand->usable = true;
   operand->type = task->type_int;
}

void test_string_usage( struct task* task, struct expr_test* test,
   struct operand* operand, struct indexed_string_usage* usage ) {
   operand->value = usage->string->index;
   operand->folded = true;
   operand->complete = true;
   operand->usable = true;
   operand->type = task->type_str;
   test->has_string = true;
}

void test_boolean( struct task* task, struct operand* operand,
   struct boolean* boolean ) {
   operand->value = boolean->value;
   operand->folded = true;
   operand->complete = true;
   operand->usable = true;
   operand->type = task->type_bool;
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
   struct object* object = NULL;
   struct name* name;
   // Try searching in the hidden compartment of the library.
   if ( usage->lib_id ) {
      list_iter_t i;
      list_iter_init( &i, &task->libraries );
      struct library* lib;
      while ( true ) {
         lib = list_data( &i );
         if ( lib->id == usage->lib_id ) {
            break;
         }
         list_next( &i );
      }
      name = t_make_name( task, usage->text, lib->hidden_names );
      if ( name->object ) {
         object = name->object;
         goto done;
      }
   }
   // Try searching in the current scope.
   name = t_make_name( task, usage->text, task->region->body );
   if ( name->object ) {
      object = name->object;
      goto done;
   }
   // Try searching in any of the linked regions.
   struct region_link* link = task->region->link;
   while ( link && ! object ) {
      name = t_make_name( task, usage->text, link->region->body );
      object = t_get_region_object( task, link->region, name );
      link = link->next;
   }
   // Object could not be found.
   if ( ! object ) {
      goto done;
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
   done:
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
      operand->complete = true;
      operand->usable = true;
   }
   else if ( object->node.type == NODE_VAR ) {
      struct var* var = ( struct var* ) object;
      operand->type = var->type;
      if ( var->dim ) {
         operand->dim = var->dim;
         // NOTE: I'm not too happy with the idea of this field. It is
         // contagious. Blame the Hypnotoad.
         if ( test->accept_array ) {
            operand->complete = true;
         }
      }
      else if ( var->type->primitive ) {
         operand->complete = true;
         operand->usable = true;
         operand->assignable = true;
      }
      var->used = true;
   }
   else if ( object->node.type == NODE_TYPE_MEMBER ) {
      struct type_member* member = ( struct type_member* ) object;
      operand->type = member->type;
      if ( member->dim ) {
         operand->dim = member->dim;
         if ( test->accept_array ) {
            operand->complete = true;
         }
      }
      else if ( member->type->primitive ) {
         operand->complete = true;
         operand->usable = true;
         operand->assignable = true;
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
         operand->complete = true;
         operand->usable = true;
         struct func_aspec* impl = operand->func->impl;
         operand->value = impl->id;
         operand->folded = true;
      }
   }
   else if ( object->node.type == NODE_PARAM ) {
      struct param* param = ( struct param* ) object;
      operand->type = param->type;
      operand->complete = true;
      operand->usable = true;
      operand->assignable = true;
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
   if ( unary->op == UOP_PRE_INC || unary->op == UOP_PRE_DEC ||
      unary->op == UOP_POST_INC || unary->op == UOP_POST_DEC ) {
      test_node( task, test, &target, unary->operand );
      // Only an l-value can be incremented.
      if ( ! target.assignable ) {
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
      test_node( task, test, &target, unary->operand );
      // Remaining operations require a value to work on.
      if ( ! target.usable ) {
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
   operand->complete = true;
   operand->usable = true;
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
         operand->complete = true;
      }
   }
   else {
      if ( operand->type->primitive ) {
         operand->complete = true;
         operand->usable = true;
         operand->assignable = true;
      }
   }
}

void test_call( struct task* task, struct expr_test* expr_test,
   struct operand* operand, struct call* call ) {
   struct call_test test = {
      .call = call,
      .func = NULL,
      .i = NULL,
      .num_args = 0
   };
   struct operand callee;
   init_operand( &callee );
   test_node( task, expr_test, &callee, call->operand );
   if ( ! callee.func ) {
      t_diag( task, DIAG_POS_ERR, &call->pos, "operand not a function" );
      t_bail( task );
   }
   test.func = callee.func;
   test_call_args( task, expr_test, &test );
   // Some action-specials cannot be called from a script.
   if ( test.func->type == FUNC_ASPEC ) {
      struct func_aspec* impl = test.func->impl;
      if ( ! impl->script_callable ) {
         struct str str;
         str_init( &str );
         t_copy_name( test.func->name, false, &str );
         t_diag( task, DIAG_POS_ERR, &call->pos,
            "action-special `%s` called from script", str.value );
         t_bail( task );
      }
   }
   // Latent function cannot be called in a function or a format block.
   if ( test.func->type == FUNC_DED ) {
      struct func_ded* impl = test.func->impl;
      if ( impl->latent ) {
         bool erred = false;
         struct stmt_test* stmt = expr_test->stmt_test;
         while ( stmt && ! stmt->format_block ) {
            stmt = stmt->parent;
         }
         if ( stmt || task->in_func ) {
            t_diag( task, DIAG_POS_ERR, &call->pos,
               "calling latent function inside a %s",
               stmt ? "format block" : "function" );
            // Show educational note to user.
            if ( task->in_func ) {
               struct str str;
               str_init( &str );
               t_copy_name( test.func->name, false, &str );
               t_diag( task, DIAG_FILE, &call->pos,
                  "waiting functions like `%s` can only be called inside a "
                  "script", str.value );
            }
            t_bail( task );
         }
      }
   }
   call->func = test.func;
   operand->type = test.func->return_type;
   operand->complete = true;
   if ( test.func->return_type ) {
      operand->usable = true;
   }
}

void test_call_args( struct task* task, struct expr_test* expr_test,
   struct call_test* test ) {
   struct call* call = test->call;
   struct func* func = test->func;
   list_iter_t i;
   list_iter_init( &i, &call->args );
   test->i = &i;
   test_call_first_arg( task, expr_test, test );
   // Test remaining arguments.
   while ( ! list_end( &i ) ) {
      struct expr_test arg;
      t_init_expr_test( &arg );
      arg.format_block = expr_test->format_block;
      arg.need_value = true;
      arg.undef_err = expr_test->undef_err;
      t_test_expr( task, &arg, list_data( &i ) );
      if ( arg.undef_erred ) {
         expr_test->undef_erred = true;
         longjmp( expr_test->bail, 1 );
      }
      ++test->num_args;
      list_next( &i );
   }
   // Number of arguments must be correct.
   if ( test->num_args < func->min_param ) {
      t_diag( task, DIAG_POS_ERR, &call->pos,
         "not enough arguments in function call" );
      struct str str;
      str_init( &str );
      t_copy_name( func->name, false, &str );
      t_diag( task, DIAG_FILE, &call->pos,
         "function `%s` needs %s%d argument%s", str.value,
         func->min_param != func->max_param ? "at least " : "",
         func->min_param, func->min_param != 1 ? "s" : "" );
      t_bail( task );
   }
   if ( test->num_args > func->max_param ) {
      t_diag( task, DIAG_POS_ERR, &call->pos,
         "too many arguments in function call" );
      struct str str;
      str_init( &str );
      t_copy_name( func->name, false, &str );
      t_diag( task, DIAG_FILE, &call->pos,
         "function `%s` takes %s%d argument%s", str.value,
         func->min_param != func->max_param ? "up_to " : "",
         func->max_param, func->max_param != 1 ? "s" : "" );
      t_bail( task );
   }
}

void test_call_first_arg( struct task* task, struct expr_test* expr_test,
   struct call_test* test ) {
   if ( test->func->type == FUNC_FORMAT ) {
      test_call_format_arg( task, expr_test, test );
   }
   else {
      if ( ! list_end( &test->i ) ) {
         struct node* node = list_data( test->i );
         if ( node->type == NODE_FORMAT_ITEM ) {
            struct format_item* item = ( struct format_item* ) node;
            t_diag( task, DIAG_POS_ERR, &item->pos,
               "passing format-item to non-format function" );
            t_bail( task );
         }
         else if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
            struct format_block_usage* usage =
               ( struct format_block_usage* ) node;
            t_diag( task, DIAG_POS_ERR, &usage->pos,
               "passing format-block to non-format function" );
            t_bail( task );
         }
      }
   }
}

void test_call_format_arg( struct task* task, struct expr_test* expr_test,
   struct call_test* test ) {
   list_iter_t* i = test->i;
   struct node* node = NULL;
   if ( ! list_end( i ) ) {
      node = list_data( i );
   }
   if ( ! node || ( node->type != NODE_FORMAT_ITEM &&
      node->type != NODE_FORMAT_BLOCK_USAGE ) ) {
      t_diag( task, DIAG_POS_ERR, &test->call->pos,
         "function call missing format argument" );
      t_bail( task );
   }
   // Format-block:
   if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
      if ( ! expr_test->format_block ) {
         t_diag( task, DIAG_POS_ERR, &test->call->pos,
            "function call missing format-block" );
         t_bail( task );
      }
      struct format_block_usage* usage = ( struct format_block_usage* ) node;
      usage->block = expr_test->format_block;
      // Attach usage to the end of the usage list.
      struct format_block_usage* prev = expr_test->format_block_usage;
      while ( prev && prev->next ) {
         prev = prev->next;
      }
      if ( prev ) {
         prev->next = usage;
      }
      else {
         expr_test->format_block_usage = usage;
      }
      list_next( i );
   }
   // Format-list:
   else {
      while ( ! list_end( i ) ) {
         struct node* node = list_data( i );
         if ( node->type != NODE_FORMAT_ITEM ) {
            break;
         }
         t_test_format_item( task, ( struct format_item* ) node,
            expr_test->stmt_test, expr_test, expr_test->format_block );
         list_next( i );
      }
   }
   // Both a format-block or a format-list count as a single argument.
   ++test->num_args;
}

// This function handles a format-item found inside a function call,
// and a free format-item, the one found in a format-block.
void t_test_format_item( struct task* task, struct format_item* item,
   struct stmt_test* stmt_test, struct expr_test* expr_test,
   struct block* format_block ) {
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
      test_expr( task, &expr, item->value, &arg );
      if ( expr.undef_erred ) {
         expr_test->undef_erred = true;
         longjmp( expr_test->bail, 1 );
      }
      if ( item->cast == FCAST_ARRAY ) {
         if ( ! arg.dim ) {
            t_diag( task, DIAG_POS_ERR, &item->value->pos,
               "argument not an array" );
            t_bail( task );
         }
         if ( arg.dim->next ) {
            t_diag( task, DIAG_POS_ERR, &item->value->pos,
               "array argument not of single dimension" );
            t_bail( task );
         }
         if ( item->extra ) {
            struct format_item_array* extra = item->extra;
            // Test offset.
            t_init_expr_test( &expr );
            expr.stmt_test = stmt_test;
            expr.format_block = format_block;
            expr.undef_err = true;
            t_test_expr( task, &expr, extra->offset );
            // Test length.
            if ( extra->length ) {
               t_init_expr_test( &expr );
               expr.stmt_test = stmt_test;
               expr.format_block = format_block;
               expr.undef_err = true;
               t_test_expr( task, &expr, extra->length );
            }
         }
      }
      item = item->next;
   }
}

void test_binary( struct task* task, struct expr_test* test,
   struct operand* operand, struct binary* binary ) {
   struct operand lside;
   init_operand( &lside );
   test_node( task, test, &lside, binary->lside );
   if ( ! lside.usable ) {
      t_diag( task, DIAG_POS_ERR, &binary->pos,
         "operand on left side not a value" );
      t_bail( task );
   }
   struct operand rside;
   init_operand( &rside );
   test_node( task, test, &rside, binary->rside );
   if ( ! rside.usable ) {
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
   operand->complete = true;
   operand->usable = true;
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
   // To avoid the error where the user wanted equality operator but instead
   // typed in the assignment operator, suggest that assignment be wrapped in
   // parentheses.
   if ( test->suggest_paren_assign && ! operand->in_paren ) {
      t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &assign->pos, "assignment operation not in parentheses" );
   }
   struct operand lside;
   init_operand( &lside );
   test_node( task, test, &lside, assign->lside );
   if ( ! lside.assignable ) {
      t_diag( task, DIAG_POS_ERR, &assign->pos,
         "cannot assign to operand on left side" );
      t_bail( task );
   }
   struct operand rside;
   init_operand( &rside );
   test_node( task, test, &rside, assign->rside );
   if ( ! rside.usable ) {
      t_diag( task, DIAG_POS_ERR, &assign->pos,
         "right side of assignment not a value" );
      t_bail( task );
   }
   operand->complete = true;
   operand->usable = true;
   operand->type = lside.type;
}

void test_access( struct task* task, struct expr_test* test,
   struct operand* operand, struct access* access ) {
   struct object* object = NULL;
   struct operand lside;
   init_operand( &lside );
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
