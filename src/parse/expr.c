#include <errno.h>
#include <string.h>
#include <limits.h>

#include "phase.h"

static void read_op( struct parse* phase, struct expr_reading* reading );
static void read_operand( struct parse* phase, struct expr_reading* reading );
static struct binary* alloc_binary( int op, struct pos* pos );
static struct unary* alloc_unary( int op, struct pos* pos );
static void read_primary( struct parse* phase, struct expr_reading* reading );
static void read_postfix( struct parse* phase, struct expr_reading* reading );
static struct access* alloc_access( char* name, struct pos pos );
static void read_call( struct parse* phase, struct expr_reading* reading );
static void read_call_args( struct parse* phase, struct expr_reading* reading,
   struct list* args );
static struct format_item* read_format_item( struct parse* phase, bool colon );
static void read_format_item_array_value( struct parse* phase,
   struct format_item* item );
static void read_string( struct parse* phase, struct expr_reading* reading );
static struct indexed_string* intern_indexed_string( struct parse* phase,
   char* value, int length, bool* first_time );
static int convert_numerictoken_to_int( struct parse* phase, int base );

void p_init_expr_reading( struct expr_reading* reading, bool in_constant,
   bool skip_assign, bool skip_call, bool expect_expr ) {
   reading->node = NULL;
   reading->output_node = NULL;
   reading->has_str = false;
   reading->in_constant = in_constant;
   reading->skip_assign = skip_assign;
   reading->skip_call = skip_call;
   reading->expect_expr = expect_expr;
}

void p_read_expr( struct parse* phase, struct expr_reading* reading ) {
   reading->pos = phase->tk_pos;
   read_op( phase, reading );
   struct expr* expr = mem_slot_alloc( sizeof( *expr ) );
   expr->node.type = NODE_EXPR;
   expr->root = reading->node;
   expr->pos = reading->pos;
   expr->value = 0;
   expr->folded = false;
   expr->has_str = reading->has_str;
   reading->output_node = expr;
}

void read_op( struct parse* phase, struct expr_reading* reading ) {
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
   read_operand( phase, reading );

   op_mul:
   // -----------------------------------------------------------------------
   switch ( phase->tk ) {
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
   mul = alloc_binary( op, &phase->tk_pos );
   mul->lside = reading->node;
   p_read_tk( phase );
   read_operand( phase, reading );
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
   switch ( phase->tk ) {
   case TK_PLUS:
      op = BOP_ADD;
      break;
   case TK_MINUS:
      op = BOP_SUB;
      break;
   default:
      goto op_shift;
   }
   add = alloc_binary( op, &phase->tk_pos );
   add->lside = reading->node;
   p_read_tk( phase );
   goto top;

   op_shift:
   // -----------------------------------------------------------------------
   if ( shift ) {
      shift->rside = reading->node;
      reading->node = &shift->node;
      shift = NULL;
   }
   switch ( phase->tk ) {
   case TK_SHIFT_L:
      op = BOP_SHIFT_L;
      break;
   case TK_SHIFT_R:
      op = BOP_SHIFT_R;
      break;
   default:
      goto op_lt;
   }
   shift = alloc_binary( op, &phase->tk_pos );
   shift->lside = reading->node;
   p_read_tk( phase );
   goto top;

   op_lt:
   // -----------------------------------------------------------------------
   if ( lt ) {
      lt->rside = reading->node;
      reading->node = &lt->node;
      lt = NULL;
   }
   switch ( phase->tk ) {
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
   lt = alloc_binary( op, &phase->tk_pos );
   lt->lside = reading->node;
   p_read_tk( phase );
   goto top;

   op_eq:
   // -----------------------------------------------------------------------
   if ( eq ) {
      eq->rside = reading->node;
      reading->node = &eq->node;
      eq = NULL;
   }
   switch ( phase->tk ) {
   case TK_EQ:
      op = BOP_EQ;
      break;
   case TK_NEQ:
      op = BOP_NEQ;
      break;
   default:
      goto op_bit_and;
   }
   eq = alloc_binary( op, &phase->tk_pos );
   eq->lside = reading->node;
   p_read_tk( phase );
   goto top;

   op_bit_and:
   // -----------------------------------------------------------------------
   if ( bit_and ) {
      bit_and->rside = reading->node;
      reading->node = &bit_and->node;
      bit_and = NULL;
   }
   if ( phase->tk == TK_BIT_AND ) {
      bit_and = alloc_binary( BOP_BIT_AND, &phase->tk_pos );
      bit_and->lside = reading->node;
      p_read_tk( phase );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_xor ) {
      bit_xor->rside = reading->node;
      reading->node = &bit_xor->node;
      bit_xor = NULL;
   }
   if ( phase->tk == TK_BIT_XOR ) {
      bit_xor = alloc_binary( BOP_BIT_XOR, &phase->tk_pos );
      bit_xor->lside = reading->node;
      p_read_tk( phase );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_or ) {
      bit_or->rside = reading->node;
      reading->node = &bit_or->node;
      bit_or = NULL;
   }
   if ( phase->tk == TK_BIT_OR ) {
      bit_or = alloc_binary( BOP_BIT_OR, &phase->tk_pos );
      bit_or->lside = reading->node;
      p_read_tk( phase );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_and ) {
      log_and->rside = reading->node;
      reading->node = &log_and->node;
      log_and = NULL;
   }
   if ( phase->tk == TK_LOG_AND ) {
      log_and = alloc_binary( BOP_LOG_AND, &phase->tk_pos );
      log_and->lside = reading->node;
      p_read_tk( phase );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_or ) {
      log_or->rside = reading->node;
      reading->node = &log_or->node;
      log_or = NULL;
   }
   if ( phase->tk == TK_LOG_OR ) {
      log_or = alloc_binary( BOP_LOG_OR, &phase->tk_pos );
      log_or->lside = reading->node;
      p_read_tk( phase );
      goto top;
   }

   // Conditional operator.
   // -----------------------------------------------------------------------
   if ( phase->tk == TK_QUESTION_MARK ) {
      struct conditional* cond = mem_alloc( sizeof( *cond ) );
      cond->node.type = NODE_CONDITIONAL;
      cond->pos = phase->tk_pos;
      cond->left = reading->node;
      cond->middle = NULL;
      cond->right = NULL;
      p_read_tk( phase );
      // Middle operand is optional.
      if ( phase->tk != TK_COLON ) {
         read_op( phase, reading );
         cond->middle = reading->node;
      }
      p_test_tk( phase, TK_COLON );
      p_read_tk( phase );
      read_op( phase, reading );
      cond->right = reading->node;
      reading->node = &cond->node;
   }

   // -----------------------------------------------------------------------
   if ( ! reading->skip_assign ) {
      switch ( phase->tk ) {
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
      assign->pos = phase->tk_pos;
      p_read_tk( phase );
      read_op( phase, reading );
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

void read_operand( struct parse* phase, struct expr_reading* reading ) {
   int op = UOP_NONE;
   switch ( phase->tk ) {
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
      struct pos pos = phase->tk_pos;
      p_read_tk( phase );
      read_operand( phase, reading );
      struct unary* unary = alloc_unary( op, &pos );
      unary->operand = reading->node;
      reading->node = &unary->node;
   }
   else {
      read_primary( phase, reading );
      read_postfix( phase, reading );
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

void read_primary( struct parse* phase, struct expr_reading* reading ) {
   if ( phase->tk == TK_ID ) {
      struct name_usage* usage = mem_slot_alloc( sizeof( *usage ) );
      usage->node.type = NODE_NAME_USAGE;
      usage->text = phase->tk_text;
      usage->pos = phase->tk_pos;
      usage->object = NULL;
      usage->lib_id = phase->task->library->id;
      reading->node = &usage->node;
      p_read_tk( phase );
   }
   else if ( phase->tk == TK_LIT_STRING ) {
      read_string( phase, reading );
   }
   else if ( phase->tk == TK_REGION ) {
      static struct node node = { NODE_REGION_HOST };
      reading->node = &node;
      p_read_tk( phase );
   }
   else if ( phase->tk == TK_UPMOST ) {
      static struct node node = { NODE_REGION_UPMOST };
      reading->node = &node;
      p_read_tk( phase );
   }
   else if ( phase->tk == TK_TRUE ) {
      static struct boolean boolean = { { NODE_BOOLEAN }, 1 };
      reading->node = &boolean.node;
      p_read_tk( phase );
   }
   else if ( phase->tk == TK_FALSE ) {
      static struct boolean boolean = { { NODE_BOOLEAN }, 0 };
      reading->node = &boolean.node;
      p_read_tk( phase );
   }
   else if ( phase->tk == TK_PAREN_L ) {
      struct paren* paren = mem_alloc( sizeof( *paren ) );
      paren->node.type = NODE_PAREN;
      p_read_tk( phase );
      read_op( phase, reading );
      p_test_tk( phase, TK_PAREN_R );
      p_read_tk( phase );
      paren->inside = reading->node;
      reading->node = &paren->node;
   }
   else {
      switch ( phase->tk ) {
      case TK_LIT_DECIMAL:
      case TK_LIT_OCTAL:
      case TK_LIT_HEX:
      case TK_LIT_BINARY:
      case TK_LIT_FIXED:
      case TK_LIT_CHAR:
         break;
      default:
         p_diag( phase, DIAG_POS_ERR, &phase->tk_pos,
            "unexpected %s", p_get_token_name( phase->tk ) );
         // For areas of code where an expression is to be expected. Only show
         // this message when not a single piece of the expression is read.
         if ( reading->expect_expr &&
            t_same_pos( &phase->tk_pos, &reading->pos ) ) {
            p_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &phase->tk_pos,
               "expecting an expression here" );
         }
         p_bail( phase );
      }
      struct literal* literal = mem_slot_alloc( sizeof( *literal ) );
      literal->node.type = NODE_LITERAL;
      literal->value = p_extract_literal_value( phase );
      reading->node = &literal->node;
      p_read_tk( phase );
   }
}

void read_string( struct parse* phase, struct expr_reading* reading ) {
   p_test_tk( phase, TK_LIT_STRING );
   bool first_time = false;
   struct indexed_string* string = intern_indexed_string( phase, phase->tk_text,
      phase->tk_length, &first_time );
   string->in_constant = reading->in_constant;
   string->in_main_file = ( phase->source == phase->main_source );
   if ( first_time ) {
      string->imported = phase->task->library->imported;
   }
   else {
      // If an imported string is found in the main library, then the string
      // should NOT be considered imported.
      if ( phase->task->library == phase->task->library_main ) {
         string->imported = false;
      }
   }
   struct indexed_string_usage* usage = mem_slot_alloc( sizeof( *usage ) );
   usage->node.type = NODE_INDEXED_STRING_USAGE;
   usage->string = string;
   reading->node = &usage->node;
   reading->has_str = true;
   p_read_tk( phase );
}

struct indexed_string* intern_indexed_string( struct parse* phase, char* value,
   int length, bool* first_time ) {
   struct indexed_string* prev_string = NULL;
   struct indexed_string* string =  phase->task->str_table.head_sorted;
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
      if ( phase->task->str_table.head ) {
         phase->task->str_table.tail->next = string;
      }
      else {
         phase->task->str_table.head = string;
      }
      phase->task->str_table.tail = string;
      // List sorted alphabetically.
      if ( prev_string ) {
         string->next_sorted = prev_string->next_sorted;
         prev_string->next_sorted = string;
      }
      else {
         string->next_sorted = phase->task->str_table.head_sorted;
         phase->task->str_table.head_sorted = string;
      }
      if ( first_time ) {
         *first_time = true;
      }
   }
   return string;
}

int p_extract_literal_value( struct parse* phase ) {
   if ( phase->tk == TK_LIT_DECIMAL ) {
      return convert_numerictoken_to_int( phase, 10 );
   }
   else if ( phase->tk == TK_LIT_OCTAL ) {
      return convert_numerictoken_to_int( phase, 8 );
   }
   else if ( phase->tk == TK_LIT_HEX ) {
      // NOTE: The hexadecimal literal is converted without considering whether
      // the number can fit into a signed integer, the destination type.
      int value = 0;
      int i = 0;
      while ( phase->tk_text[ i ] ) {
         int digit_value = 0;
         switch ( phase->tk_text[ i ] ) {
         case 'a': case 'A': digit_value = 0xA; break;
         case 'b': case 'B': digit_value = 0xB; break;
         case 'c': case 'C': digit_value = 0xC; break;
         case 'd': case 'D': digit_value = 0xD; break;
         case 'e': case 'E': digit_value = 0xE; break;
         case 'f': case 'F': digit_value = 0xF; break;
         default:
            digit_value = phase->tk_text[ i ] - '0';
            break;
         }
         // NOTE: Undefined behavior may occur when shifting of a signed
         // integer leads to overflow.
         value = ( ( value << 4 ) | digit_value );
         ++i;
      }
      return value;
   }
   else if ( phase->tk == TK_LIT_BINARY ) {
      unsigned int temp = 0;
      for ( int i = 0; phase->tk_text[ i ]; ++i ) {
         temp = ( temp << 1 ) | ( phase->tk_text[ i ] - '0' );
      }
      int value = 0;
      memcpy( &value, &temp, sizeof( value ) );
      return value;
   }
   else if ( phase->tk == TK_LIT_FIXED ) {
      double value = atof( phase->tk_text );
      return
         // Whole.
         ( ( int ) value << 16 ) +
         // Fraction.
         ( int ) ( ( 1 << 16 ) * ( value - ( int ) value ) );
   }
   else if ( phase->tk == TK_LIT_CHAR ) {
      return phase->tk_text[ 0 ];
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
int convert_numerictoken_to_int( struct parse* phase, int base ) {
   errno = 0;
   char* temp = NULL;
   long value = strtol( phase->tk_text, &temp, base );
   // NOTE: The token should already be of the form that strtol() accepts.
   // Maybe make this an internal compiler error, as a sanity check?
   if ( temp == phase->tk_text || *temp != '\0' ) {
      p_diag( phase, DIAG_POS_ERR, &phase->tk_pos,
         "invalid numeric value `%s`", phase->tk_text );
      p_bail( phase );
   }
   if ( ( value == LONG_MAX && errno == ERANGE ) ||
      value > ( long ) ENGINE_MAX_INT_VALUE ) {
      p_diag( phase, DIAG_POS_ERR, &phase->tk_pos,
         "numeric value `%s` is too large", phase->tk_text );
      p_bail( phase );
   }
   return ( int ) value;
}

void read_postfix( struct parse* phase, struct expr_reading* reading ) {
   while ( true ) {
      if ( phase->tk == TK_BRACKET_L ) {
         struct subscript* sub = mem_alloc( sizeof( *sub ) );
         sub->node.type = NODE_SUBSCRIPT;
         sub->pos = phase->tk_pos;
         p_read_tk( phase );
         struct expr_reading index;
         p_init_expr_reading( &index, reading->in_constant, false, false,
            true );
         p_read_expr( phase, &index );
         p_test_tk( phase, TK_BRACKET_R );
         p_read_tk( phase );
         sub->index = index.output_node;
         sub->lside = reading->node;
         reading->node = &sub->node;
      }
      else if ( phase->tk == TK_DOT ) {
         struct pos pos = phase->tk_pos;
         p_read_tk( phase );
         p_test_tk( phase, TK_ID );
         struct access* access = alloc_access( phase->tk_text, pos );
         access->lside = reading->node;
         reading->node = &access->node;
         p_read_tk( phase );
      }
      else if ( phase->tk == TK_COLON_2 ) {
         struct pos pos = phase->tk_pos;
         p_read_tk( phase );
         p_test_tk( phase, TK_ID );
         struct access* access = alloc_access( phase->tk_text, pos );
         access->lside = reading->node;
         access->is_region = true;
         reading->node = &access->node;
         p_read_tk( phase );
      }
      else if ( phase->tk == TK_PAREN_L ) {
         if ( ! reading->skip_call ) {
            read_call( phase, reading );
         }
         else {
            break;
         }
      }
      else if ( phase->tk == TK_INC ) {
         struct unary* inc = alloc_unary( UOP_POST_INC, &phase->tk_pos );
         inc->operand = reading->node;
         reading->node = &inc->node;
         p_read_tk( phase );
      }
      else if ( phase->tk == TK_DEC ) {
         struct unary* dec = alloc_unary( UOP_POST_DEC, &phase->tk_pos );
         dec->operand = reading->node;
         reading->node = &dec->node;
         p_read_tk( phase );
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

void read_call( struct parse* phase, struct expr_reading* reading ) {
   struct pos pos = phase->tk_pos;
   p_test_tk( phase, TK_PAREN_L );
   p_read_tk( phase );
   struct list args;
   list_init( &args );
   // Format list:
   if ( phase->tk == TK_ID && p_peek( phase ) == TK_COLON ) {
      list_append( &args, p_read_format_item( phase, true ) );
      if ( phase->tk == TK_SEMICOLON ) {
         p_read_tk( phase );
         read_call_args( phase, reading, &args );
      }
   }
   // Format block:
   else if ( phase->tk == TK_BRACE_L ) {
      struct format_block_usage* usage = mem_alloc( sizeof( *usage ) );
      usage->node.type = NODE_FORMAT_BLOCK_USAGE;
      usage->block = NULL;
      usage->next = NULL;
      usage->pos = phase->tk_pos;
      usage->obj_pos = 0;
      list_append( &args, usage );
      p_read_tk( phase );
      p_test_tk( phase, TK_BRACE_R );
      p_read_tk( phase );
      if ( phase->tk == TK_SEMICOLON ) {
         p_read_tk( phase );
         read_call_args( phase, reading, &args );
      }
   }
   else {
      // This relic is not necessary in new code. The compiler is smart enough
      // to figure out when to use the constant variant of an instruction.
      if ( phase->tk == TK_CONST ) {
         p_read_tk( phase );
         p_test_tk( phase, TK_COLON );
         p_read_tk( phase );
      }
      if ( phase->tk != TK_PAREN_R ) {
         read_call_args( phase, reading, &args );
      }
   }
   p_test_tk( phase, TK_PAREN_R );
   p_read_tk( phase );
   struct call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_CALL;
   call->pos = pos;
   call->operand = reading->node;
   call->func = NULL;
   call->nested_call = NULL;
   call->args = args;
   reading->node = &call->node;
}

void read_call_args( struct parse* phase, struct expr_reading* reading,
   struct list* args ) {
   while ( true ) {
      struct expr_reading arg;
      p_init_expr_reading( &arg, reading->in_constant, false, false, true );
      p_read_expr( phase, &arg );
      list_append( args, arg.output_node );
      if ( phase->tk == TK_COMMA ) {
         p_read_tk( phase );
      }
      else {
         break;
      }
   }
}

struct format_item* p_read_format_item( struct parse* phase, bool colon ) {
   struct format_item* head = NULL;
   struct format_item* tail;
   while ( true ) {
      struct format_item* item = read_format_item( phase, colon );
      if ( head ) {
         tail->next = item;
      }
      else {
         head = item;
      }
      tail = item;
      if ( phase->tk == TK_COMMA ) {
         p_read_tk( phase );
      }
      else {
         return head;
      }
   }
}

struct format_item* read_format_item( struct parse* phase, bool colon ) {
   p_test_tk( phase, TK_ID );
   struct format_item* item = mem_alloc( sizeof( *item ) );
   item->node.type = NODE_FORMAT_ITEM;
   item->cast = FCAST_DECIMAL;
   item->pos = phase->tk_pos;
   item->next = NULL;
   item->value = NULL;
   item->extra = NULL;
   bool unknown = false;
   switch ( phase->tk_text[ 0 ] ) {
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
   if ( unknown || phase->tk_length != 1 ) {
      p_diag( phase, DIAG_POS_ERR, &phase->tk_pos,
         "unknown format cast `%s`", phase->tk_text );
      p_bail( phase );
   }
   p_read_tk( phase );
   // In a format block, the `:=` separator is used, because an identifier
   // and a colon creates a goto-statement label.
   if ( colon ) {
      p_test_tk( phase, TK_COLON );
      p_read_tk( phase );
   }
   else {
      p_test_tk( phase, TK_ASSIGN_COLON );
      p_read_tk( phase );
   }
   if ( item->cast == FCAST_ARRAY ) {
      read_format_item_array_value( phase, item );
   }
   else {
      struct expr_reading value;
      p_init_expr_reading( &value, false, false, false, true );
      p_read_expr( phase, &value );
      item->value = value.output_node;
   }
   return item;
}

void read_format_item_array_value( struct parse* phase,
   struct format_item* item ) {
   bool paren = false;
   if ( phase->tk == TK_PAREN_L ) {
      paren = true;
      p_read_tk( phase );
   }
   // Array field.
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( phase, &expr );
   item->value = expr.output_node;
   if ( paren ) {
      // Offset field.
      if ( phase->tk == TK_COMMA ) {
         p_read_tk( phase );
         p_init_expr_reading( &expr, false, false, false, true );
         p_read_expr( phase, &expr );
         struct expr* offset = expr.output_node;
         // Length field.
         struct expr* length = NULL;
         if ( phase->tk == TK_COMMA ) {
            p_read_tk( phase );
            p_init_expr_reading( &expr, false, false, false, true );
            p_read_expr( phase, &expr );
            length = expr.output_node;
         }
         struct format_item_array* extra = mem_alloc( sizeof( *extra ) );
         extra->offset = offset;
         extra->length = length;
         extra->offset_var = 0;
         item->extra = extra;
      }
      p_test_tk( phase, TK_PAREN_R );
      p_read_tk( phase );
   }
}