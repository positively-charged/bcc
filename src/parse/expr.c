#include <errno.h>
#include <string.h>
#include <limits.h>

#include "phase.h"

struct format_cast {
   int type;
   struct pos pos;
   bool unknown;
   bool colon;
};

struct array_field {
   struct expr* array;
   struct expr* offset;
   struct expr* length;
};

static void read_op( struct parse* parse, struct expr_reading* reading );
static void read_operand( struct parse* parse, struct expr_reading* reading );
static struct binary* alloc_binary( int op, struct pos* pos );
static struct logical* alloc_logical( int op, struct pos* pos );
static struct unary* alloc_unary( int op, struct pos* pos );
static void read_inc( struct parse* parse, struct expr_reading* reading );
static struct inc* alloc_inc( struct pos pos, bool dec );
static void read_cast( struct parse* parse, struct expr_reading* reading );
static void read_primary( struct parse* parse, struct expr_reading* reading );
static void read_fixed_literal( struct parse* parse,
   struct expr_reading* reading );
static int extract_fixed_literal_value( const char* text );
static void read_postfix( struct parse* parse, struct expr_reading* reading );
static struct access* alloc_access( const char* name, struct pos pos );
static void read_post_inc( struct parse* parse, struct expr_reading* reading );
static void read_call( struct parse* parse, struct expr_reading* reading );
static void read_call_args( struct parse* parse, struct expr_reading* reading,
   struct list* args );
static struct format_item* read_format_item( struct parse* parse, bool colon );
static void init_format_cast( struct format_cast* cast, bool colon );
static void read_format_cast( struct parse* parse, struct format_cast* cast );
static void init_array_field( struct array_field* field );
static void read_array_field( struct parse* parse, struct array_field* field );
static void read_string( struct parse* parse, struct expr_reading* reading );
static int convert_numerictoken_to_int( struct parse* parse, int base );
static void read_strcpy( struct parse* parse, struct expr_reading* reading );

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

void p_read_expr( struct parse* parse, struct expr_reading* reading ) {
   reading->pos = parse->tk_pos;
   read_op( parse, reading );
   struct expr* expr = mem_slot_alloc( sizeof( *expr ) );
   expr->node.type = NODE_EXPR;
   expr->root = reading->node;
   expr->pos = reading->pos;
   expr->spec = SPEC_NONE;
   expr->value = 0;
   expr->folded = false;
   expr->has_str = reading->has_str;
   reading->output_node = expr;
}

void read_op( struct parse* parse, struct expr_reading* reading ) {
   int op = 0;
   struct binary* mul = NULL;
   struct binary* add = NULL;
   struct binary* shift = NULL;
   struct binary* lt = NULL;
   struct binary* eq = NULL;
   struct binary* bit_and = NULL;
   struct binary* bit_xor = NULL;
   struct binary* bit_or = NULL;
   struct logical* log_and = NULL;
   struct logical* log_or = NULL;

   top:
   read_operand( parse, reading );

   op_mul:
   // -----------------------------------------------------------------------
   switch ( parse->tk ) {
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
   mul = alloc_binary( op, &parse->tk_pos );
   mul->lside = reading->node;
   p_read_tk( parse );
   read_operand( parse, reading );
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
   switch ( parse->tk ) {
   case TK_PLUS:
      op = BOP_ADD;
      break;
   case TK_MINUS:
      op = BOP_SUB;
      break;
   default:
      goto op_shift;
   }
   add = alloc_binary( op, &parse->tk_pos );
   add->lside = reading->node;
   p_read_tk( parse );
   goto top;

   op_shift:
   // -----------------------------------------------------------------------
   if ( shift ) {
      shift->rside = reading->node;
      reading->node = &shift->node;
      shift = NULL;
   }
   switch ( parse->tk ) {
   case TK_SHIFT_L:
      op = BOP_SHIFT_L;
      break;
   case TK_SHIFT_R:
      op = BOP_SHIFT_R;
      break;
   default:
      goto op_lt;
   }
   shift = alloc_binary( op, &parse->tk_pos );
   shift->lside = reading->node;
   p_read_tk( parse );
   goto top;

   op_lt:
   // -----------------------------------------------------------------------
   if ( lt ) {
      lt->rside = reading->node;
      reading->node = &lt->node;
      lt = NULL;
   }
   switch ( parse->tk ) {
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
   lt = alloc_binary( op, &parse->tk_pos );
   lt->lside = reading->node;
   p_read_tk( parse );
   goto top;

   op_eq:
   // -----------------------------------------------------------------------
   if ( eq ) {
      eq->rside = reading->node;
      reading->node = &eq->node;
      eq = NULL;
   }
   switch ( parse->tk ) {
   case TK_EQ:
      op = BOP_EQ;
      break;
   case TK_NEQ:
      op = BOP_NEQ;
      break;
   default:
      goto op_bit_and;
   }
   eq = alloc_binary( op, &parse->tk_pos );
   eq->lside = reading->node;
   p_read_tk( parse );
   goto top;

   op_bit_and:
   // -----------------------------------------------------------------------
   if ( bit_and ) {
      bit_and->rside = reading->node;
      reading->node = &bit_and->node;
      bit_and = NULL;
   }
   if ( parse->tk == TK_BIT_AND ) {
      bit_and = alloc_binary( BOP_BIT_AND, &parse->tk_pos );
      bit_and->lside = reading->node;
      p_read_tk( parse );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_xor ) {
      bit_xor->rside = reading->node;
      reading->node = &bit_xor->node;
      bit_xor = NULL;
   }
   if ( parse->tk == TK_BIT_XOR ) {
      bit_xor = alloc_binary( BOP_BIT_XOR, &parse->tk_pos );
      bit_xor->lside = reading->node;
      p_read_tk( parse );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_or ) {
      bit_or->rside = reading->node;
      reading->node = &bit_or->node;
      bit_or = NULL;
   }
   if ( parse->tk == TK_BIT_OR ) {
      bit_or = alloc_binary( BOP_BIT_OR, &parse->tk_pos );
      bit_or->lside = reading->node;
      p_read_tk( parse );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_and ) {
      log_and->rside = reading->node;
      reading->node = &log_and->node;
      log_and = NULL;
   }
   if ( parse->tk == TK_LOG_AND ) {
      log_and = alloc_logical( LOP_AND, &parse->tk_pos );
      log_and->lside = reading->node;
      p_read_tk( parse );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_or ) {
      log_or->rside = reading->node;
      reading->node = &log_or->node;
      log_or = NULL;
   }
   if ( parse->tk == TK_LOG_OR ) {
      log_or = alloc_logical( LOP_OR, &parse->tk_pos );
      log_or->lside = reading->node;
      p_read_tk( parse );
      goto top;
   }

   // Conditional operator.
   // -----------------------------------------------------------------------
   if ( parse->tk == TK_QUESTION_MARK ) {
      struct conditional* cond = mem_alloc( sizeof( *cond ) );
      cond->node.type = NODE_CONDITIONAL;
      cond->pos = parse->tk_pos;
      cond->left = reading->node;
      cond->middle = NULL;
      cond->right = NULL;
      cond->left_value = 0;
      cond->folded = false;
      p_read_tk( parse );
      // Middle operand is optional.
      if ( parse->tk != TK_COLON ) {
         read_op( parse, reading );
         cond->middle = reading->node;
      }
      p_test_tk( parse, TK_COLON );
      p_read_tk( parse );
      read_op( parse, reading );
      cond->right = reading->node;
      reading->node = &cond->node;
   }

   // -----------------------------------------------------------------------
   if ( ! reading->skip_assign ) {
      switch ( parse->tk ) {
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
      assign->pos = parse->tk_pos;
      p_read_tk( parse );
      read_op( parse, reading );
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
   binary->lside_spec = SPEC_NONE;
   binary->value = 0;
   binary->folded = false;
   return binary;
}

struct logical* alloc_logical( int op, struct pos* pos ) {
   struct logical* logical = mem_alloc( sizeof( *logical ) );
   logical->node.type = NODE_LOGICAL;
   logical->op = op;
   logical->lside = NULL;
   logical->rside = NULL;
   logical->pos = *pos;
   logical->lside_spec = SPEC_NONE;
   logical->rside_spec = SPEC_NONE;
   logical->value = 0;
   logical->folded = false;
   return logical;
}

void read_operand( struct parse* parse, struct expr_reading* reading ) {
   int op = UOP_NONE;
   switch ( parse->tk ) {
   case TK_INC:
   case TK_DEC:
      read_inc( parse, reading );
      return;
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
   case TK_CAST:
      read_cast( parse, reading );
      return;
   default:
      break;
   }
   if ( op != UOP_NONE ) {
      struct pos pos = parse->tk_pos;
      p_read_tk( parse );
      read_operand( parse, reading );
      struct unary* unary = alloc_unary( op, &pos );
      unary->operand = reading->node;
      reading->node = &unary->node;
   }
   else {
      read_primary( parse, reading );
      read_postfix( parse, reading );
   }
}

struct unary* alloc_unary( int op, struct pos* pos ) {
   struct unary* unary = mem_alloc( sizeof( *unary ) );
   unary->node.type = NODE_UNARY;
   unary->op = op;
   unary->operand = NULL;
   unary->pos = *pos;
   unary->operand_spec = SPEC_NONE;
   return unary;
}

void read_inc( struct parse* parse, struct expr_reading* reading ) {
   struct inc* inc = alloc_inc( parse->tk_pos, ( parse->tk == TK_DEC ) );
   p_read_tk( parse );
   read_operand( parse, reading );
   inc->operand = reading->node;
   reading->node = &inc->node;
}

struct inc* alloc_inc( struct pos pos, bool dec ) {
   struct inc* inc = mem_alloc( sizeof( *inc ) );
   inc->node.type = NODE_INC;
   inc->operand = NULL;
   inc->pos = pos;
   inc->post = false;
   inc->dec = dec;
   inc->zfixed = false;
   return inc;
}

// TODO: Read type information from dec.c.
void read_cast( struct parse* parse, struct expr_reading* reading ) {
   struct pos pos = parse->tk_pos;
   p_test_tk( parse, TK_CAST );
   p_read_tk( parse );
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   int spec = SPEC_NONE;
   switch ( parse->tk ) {
   case TK_ZRAW:
      spec = SPEC_ZRAW;
      p_read_tk( parse );
      break;
   case TK_ZINT:
      spec = SPEC_ZINT;
      p_read_tk( parse );
      break;
   case TK_ZFIXED:
      spec = SPEC_ZFIXED;
      p_read_tk( parse );
      break;
   case TK_ZBOOL:
      spec = SPEC_ZBOOL;
      p_read_tk( parse );
      break;
   case TK_ZSTR:
      spec = SPEC_ZSTR;
      p_read_tk( parse );
      break;
   default:
      UNREACHABLE();
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   read_operand( parse, reading );
   struct cast* cast = mem_alloc( sizeof( *cast ) );
   cast->node.type = NODE_CAST;
   cast->operand = reading->node;
   cast->pos = pos;
   cast->spec = spec;
   reading->node = &cast->node;
}

void read_primary( struct parse* parse, struct expr_reading* reading ) {
   if ( parse->tk == TK_ID ) {
      struct name_usage* usage = mem_slot_alloc( sizeof( *usage ) );
      usage->node.type = NODE_NAME_USAGE;
      usage->text = parse->tk_text;
      usage->pos = parse->tk_pos;
      usage->object = NULL;
      usage->lib_id = parse->task->library->id;
      reading->node = &usage->node;
      p_read_tk( parse );
   }
   else if ( parse->tk == TK_LIT_STRING ) {
      read_string( parse, reading );
   }
   else if ( parse->tk == TK_TRUE ) {
      static struct boolean boolean = { { NODE_BOOLEAN }, 1 };
      reading->node = &boolean.node;
      p_read_tk( parse );
   }
   else if ( parse->tk == TK_FALSE ) {
      static struct boolean boolean = { { NODE_BOOLEAN }, 0 };
      reading->node = &boolean.node;
      p_read_tk( parse );
   }
   else if ( parse->tk == TK_STRCPY ) {
      read_strcpy( parse, reading );
   }
   else if ( parse->tk == TK_PAREN_L ) {
      struct paren* paren = mem_alloc( sizeof( *paren ) );
      paren->node.type = NODE_PAREN;
      p_read_tk( parse );
      read_op( parse, reading );
      p_test_tk( parse, TK_PAREN_R );
      p_read_tk( parse );
      paren->inside = reading->node;
      reading->node = &paren->node;
   }
   else if ( parse->tk == TK_LIT_FIXED ) {
      read_fixed_literal( parse, reading );
   }
   else {
      switch ( parse->tk ) {
      case TK_LIT_DECIMAL:
      case TK_LIT_OCTAL:
      case TK_LIT_HEX:
      case TK_LIT_BINARY:
      case TK_LIT_CHAR:
         break;
      default:
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "unexpected %s", p_get_token_name( parse->tk ) );
         // For areas of code where an expression is to be expected. Only show
         // this message when not a single piece of the expression is read.
         if ( reading->expect_expr &&
            t_same_pos( &parse->tk_pos, &reading->pos ) ) {
            p_diag( parse, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &parse->tk_pos,
               "expecting an expression here" );
         }
         p_bail( parse );
      }
      struct literal* literal = mem_slot_alloc( sizeof( *literal ) );
      literal->node.type = NODE_LITERAL;
      literal->value = p_extract_literal_value( parse );
      reading->node = &literal->node;
      p_read_tk( parse );
   }
}

void read_string( struct parse* parse, struct expr_reading* reading ) {
   p_test_tk( parse, TK_LIT_STRING );
   bool first_time = false;
   struct indexed_string* string = t_intern_string_get_status( parse->task,
      parse->tk_text, parse->tk_length, &first_time );
   if ( first_time ) {
      string->imported = parse->task->library->imported;
   }
   else {
      // If an imported string is found in the main library, then the string
      // should NOT be considered imported.
      if ( parse->task->library == parse->task->library_main ) {
         string->imported = false;
      }
   }
   struct indexed_string_usage* usage = mem_slot_alloc( sizeof( *usage ) );
   usage->node.type = NODE_INDEXED_STRING_USAGE;
   usage->string = string;
   reading->node = &usage->node;
   reading->has_str = true;
   p_read_tk( parse );
}

int p_extract_literal_value( struct parse* parse ) {
   if ( parse->tk == TK_LIT_DECIMAL ) {
      return convert_numerictoken_to_int( parse, 10 );
   }
   else if ( parse->tk == TK_LIT_OCTAL ) {
      return convert_numerictoken_to_int( parse, 8 );
   }
   else if ( parse->tk == TK_LIT_HEX ) {
      // NOTE: The hexadecimal literal is converted without considering whether
      // the number can fit into a signed integer, the destination type.
      int value = 0;
      int i = 0;
      while ( parse->tk_text[ i ] ) {
         int digit_value = 0;
         switch ( parse->tk_text[ i ] ) {
         case 'a': case 'A': digit_value = 0xA; break;
         case 'b': case 'B': digit_value = 0xB; break;
         case 'c': case 'C': digit_value = 0xC; break;
         case 'd': case 'D': digit_value = 0xD; break;
         case 'e': case 'E': digit_value = 0xE; break;
         case 'f': case 'F': digit_value = 0xF; break;
         default:
            digit_value = parse->tk_text[ i ] - '0';
            break;
         }
         // NOTE: Undefined behavior may occur when shifting of a signed
         // integer leads to overflow.
         value = ( ( value << 4 ) | digit_value );
         ++i;
      }
      return value;
   }
   else if ( parse->tk == TK_LIT_BINARY ) {
      unsigned int temp = 0;
      for ( int i = 0; parse->tk_text[ i ]; ++i ) {
         temp = ( temp << 1 ) | ( parse->tk_text[ i ] - '0' );
      }
      int value = 0;
      memcpy( &value, &temp, sizeof( value ) );
      return value;
   }
   else if ( parse->tk == TK_LIT_CHAR ) {
      return parse->tk_text[ 0 ];
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
int convert_numerictoken_to_int( struct parse* parse, int base ) {
   errno = 0;
   char* temp = NULL;
   long value = strtol( parse->tk_text, &temp, base );
   // NOTE: The token should already be of the form that strtol() accepts.
   // Maybe make this an internal compiler error, as a sanity check?
   if ( temp == parse->tk_text || *temp != '\0' ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "invalid numeric value `%s`", parse->tk_text );
      p_bail( parse );
   }
   if ( ( value == LONG_MAX && errno == ERANGE ) ||
      value > ( long ) ENGINE_MAX_INT_VALUE ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "numeric value `%s` is too large", parse->tk_text );
      p_bail( parse );
   }
   return ( int ) value;
}

void read_fixed_literal( struct parse* parse, struct expr_reading* reading ) {
   struct fixed_literal* literal = mem_slot_alloc( sizeof( *literal ) );
   literal->node.type = NODE_FIXED_LITERAL;
   literal->value = extract_fixed_literal_value( parse->tk_text );
   reading->node = &literal->node;
   p_read_tk( parse );
}

int extract_fixed_literal_value( const char* text ) {
   double value = atof( text );
   return
      // Whole.
      ( ( int ) value << 16 ) +
      // Fraction.
      ( int ) ( ( 1 << 16 ) * ( value - ( int ) value ) );
}

void read_postfix( struct parse* parse, struct expr_reading* reading ) {
   while ( true ) {
      if ( parse->tk == TK_BRACKET_L ) {
         struct subscript* sub = mem_alloc( sizeof( *sub ) );
         sub->node.type = NODE_SUBSCRIPT;
         sub->pos = parse->tk_pos;
         p_read_tk( parse );
         struct expr_reading index;
         p_init_expr_reading( &index, reading->in_constant, false, false,
            true );
         p_read_expr( parse, &index );
         p_test_tk( parse, TK_BRACKET_R );
         p_read_tk( parse );
         sub->index = index.output_node;
         sub->lside = reading->node;
         reading->node = &sub->node;
      }
      else if ( parse->tk == TK_DOT ) {
         struct pos pos = parse->tk_pos;
         p_read_tk( parse );
         p_test_tk( parse, TK_ID );
         struct access* access = alloc_access( parse->tk_text, pos );
         access->lside = reading->node;
         reading->node = &access->node;
         p_read_tk( parse );
      }
      else if ( parse->tk == TK_PAREN_L ) {
         if ( ! reading->skip_call ) {
            read_call( parse, reading );
         }
         else {
            break;
         }
      }
      else if ( parse->tk == TK_INC || parse->tk == TK_DEC ) {
         read_post_inc( parse, reading );
      }
      else {
         break;
      }
   }
}

struct access* alloc_access( const char* name, struct pos pos ) {
   struct access* access = mem_alloc( sizeof( *access ) );
   access->node.type = NODE_ACCESS;
   access->name = name;
   access->pos = pos;
   access->lside = NULL;
   access->rside = NULL;
   return access;
}

void read_post_inc( struct parse* parse, struct expr_reading* reading ) {
   struct inc* inc = alloc_inc( parse->tk_pos, ( parse->tk == TK_DEC ) );
   inc->post = true;
   inc->operand = reading->node;
   reading->node = &inc->node;
   p_read_tk( parse );
}

void read_call( struct parse* parse, struct expr_reading* reading ) {
   struct pos pos = parse->tk_pos;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct list args;
   list_init( &args );
   // Format list:
   if ( parse->tk == TK_ID && p_peek( parse ) == TK_COLON ) {
      list_append( &args, p_read_format_item( parse, true ) );
      if ( parse->tk == TK_SEMICOLON ) {
         p_read_tk( parse );
         read_call_args( parse, reading, &args );
      }
   }
   // Format block:
   else if ( parse->tk == TK_BRACE_L ) {
      struct format_block_usage* usage = mem_alloc( sizeof( *usage ) );
      usage->node.type = NODE_FORMAT_BLOCK_USAGE;
      usage->block = NULL;
      usage->next = NULL;
      usage->point = NULL;
      usage->pos = parse->tk_pos;
      list_append( &args, usage );
      p_read_tk( parse );
      p_test_tk( parse, TK_BRACE_R );
      p_read_tk( parse );
      if ( parse->tk == TK_SEMICOLON ) {
         p_read_tk( parse );
         read_call_args( parse, reading, &args );
      }
   }
   else {
      // This relic is not necessary in new code. The compiler is smart enough
      // to figure out when to use the constant variant of an instruction.
      if ( parse->tk == TK_CONST ) {
         p_read_tk( parse );
         p_test_tk( parse, TK_COLON );
         p_read_tk( parse );
      }
      if ( parse->tk != TK_PAREN_R ) {
         read_call_args( parse, reading, &args );
      }
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   struct call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_CALL;
   call->pos = pos;
   call->operand = reading->node;
   call->func = NULL;
   call->nested_call = NULL;
   call->args = args;
   reading->node = &call->node;
}

void read_call_args( struct parse* parse, struct expr_reading* reading,
   struct list* args ) {
   while ( true ) {
      struct expr_reading arg;
      p_init_expr_reading( &arg, reading->in_constant, false, false, true );
      p_read_expr( parse, &arg );
      list_append( args, arg.output_node );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
}

struct format_item* p_read_format_item( struct parse* parse, bool colon ) {
   struct format_item* head = NULL;
   struct format_item* tail;
   while ( true ) {
      struct format_item* item = read_format_item( parse, colon );
      if ( head ) {
         tail->next = item;
      }
      else {
         head = item;
      }
      tail = item;
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         return head;
      }
   }
}

struct format_item* read_format_item( struct parse* parse, bool colon ) {
   p_test_tk( parse, TK_ID );
   struct format_item* item = mem_alloc( sizeof( *item ) );
   item->node.type = NODE_FORMAT_ITEM;
   item->cast = FCAST_DECIMAL;
   item->pos = parse->tk_pos;
   item->next = NULL;
   item->value = NULL;
   item->extra = NULL;
   struct format_cast cast;
   init_format_cast( &cast, colon );
   read_format_cast( parse, &cast );
   item->cast = cast.type;
   if ( item->cast == FCAST_ARRAY ) {
      struct array_field field;
      init_array_field( &field );
      read_array_field( parse, &field );
      item->value = field.array;
      if ( field.offset ) {
         struct format_item_array* extra = mem_alloc( sizeof( *extra ) );
         extra->offset = field.offset;
         extra->length = field.length;
         item->extra = extra;
      }
   }
   else {
      struct expr_reading value;
      p_init_expr_reading( &value, false, false, false, true );
      p_read_expr( parse, &value );
      item->value = value.output_node;
   }
   return item;
}

void init_format_cast( struct format_cast* cast, bool colon ) {
   cast->unknown = false;
   cast->type = FCAST_DECIMAL;
   cast->colon = colon;
}

void read_format_cast( struct parse* parse, struct format_cast* cast ) {
   cast->pos = parse->tk_pos;
   switch ( parse->tk_text[ 0 ] ) {
   case 'a': cast->type = FCAST_ARRAY; break;
   case 'b': cast->type = FCAST_BINARY; break;
   case 'c': cast->type = FCAST_CHAR; break;
   case 'd':
   case 'i': break;
   case 'f': cast->type = FCAST_FIXED; break;
   case 'k': cast->type = FCAST_KEY; break;
   case 'l': cast->type = FCAST_LOCAL_STRING; break;
   case 'n': cast->type = FCAST_NAME; break;
   case 's': cast->type = FCAST_STRING; break;
   case 'x': cast->type = FCAST_HEX; break;
   default:
      cast->unknown = true;
      break;
   }
   if ( cast->unknown || parse->tk_length != 1 ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "unknown format-cast `%s`", parse->tk_text );
      p_bail( parse );
   }
   p_read_tk( parse );
   // In a format block, the `:=` separator is used, because an identifier
   // and a colon creates a goto-statement label.
   if ( cast->colon ) {
      p_test_tk( parse, TK_COLON );
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_ASSIGN_COLON );
      p_read_tk( parse );
   }
}

void init_array_field( struct array_field* field ) {
   field->array = NULL;
   field->offset = NULL;
   field->length = NULL;
}

void read_array_field( struct parse* parse, struct array_field* field ) {
   bool paren = false;
   if ( parse->tk == TK_PAREN_L ) {
      paren = true;
      p_read_tk( parse );
   }
   // Array field.
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   field->array = expr.output_node;
   if ( ! paren ) {
      return;
   }
   // Offset field.
   if ( parse->tk == TK_COMMA ) {
      p_read_tk( parse );
      p_init_expr_reading( &expr, false, false, false, true );
      p_read_expr( parse, &expr );
      field->offset = expr.output_node;
      // Length field.
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
         p_init_expr_reading( &expr, false, false, false, true );
         p_read_expr( parse, &expr );
         field->length = expr.output_node;
      }
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
}

void read_strcpy( struct parse* parse, struct expr_reading* reading ) {
   p_test_tk( parse, TK_STRCPY );
   p_read_tk( parse );
   struct strcpy_call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_STRCPY;
   call->array = NULL;
   call->array_offset = NULL;
   call->array_length = NULL;
   call->string = NULL;
   call->offset = NULL;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   // Array field.
   struct format_cast cast;
   init_format_cast( &cast, true );
   read_format_cast( parse, &cast );
   if ( cast.type != FCAST_ARRAY ) {
      p_diag( parse, DIAG_POS_ERR, &cast.pos,
         "not an array format-cast" );
      p_diag( parse, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &cast.pos,
         "expecting `a:` here" );
      p_bail( parse );
   }
   struct array_field field;
   init_array_field( &field );
   read_array_field( parse, &field );
   call->array = field.array;
   call->array_offset = field.offset;
   call->array_length = field.length;
   p_test_tk( parse, TK_COMMA );
   p_read_tk( parse );
   // String field.
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   call->string = expr.output_node;
   // String-offset field. Optional.
   if ( parse->tk == TK_COMMA ) {
      p_read_tk( parse );
      p_init_expr_reading( &expr, false, false, false, true );
      p_read_expr( parse, &expr );
      call->offset = expr.output_node;
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   reading->node = &call->node;
}