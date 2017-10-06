#include <errno.h>
#include <string.h>
#include <limits.h>

#include "phase.h"

struct format_cast {
   int type;
   struct pos pos;
   bool unknown;
};

struct array_field {
   struct expr* array;
   struct expr* offset;
   struct expr* length;
};

struct strcpy_reading {
   struct expr* array;
   struct expr* array_offset;
   struct expr* array_length;
   struct expr* string;
   struct expr* offset;
   bool array_cast;
};

static void read_op( struct parse* parse, struct expr_reading* reading );
static struct binary* alloc_binary( int op, struct pos* pos );
static struct logical* alloc_logical( int op, struct pos* pos );
static void read_prefix( struct parse* parse, struct expr_reading* reading );
static void read_unary( struct parse* parse, struct expr_reading* reading );
static void read_inc( struct parse* parse, struct expr_reading* reading );
static struct inc* alloc_inc( struct pos pos, bool dec );
static void read_primary( struct parse* parse, struct expr_reading* reading );
static void read_fixed_literal( struct parse* parse,
   struct expr_reading* reading );
static void read_conversion( struct parse* parse,
   struct expr_reading* reading );
static void read_suffix( struct parse* parse, struct expr_reading* reading );
static void read_subscript( struct parse* parse,
   struct expr_reading* reading );
static void read_access( struct parse* parse, struct expr_reading* reading );
static struct access* alloc_access( const char* name, struct pos pos );
static void read_post_inc( struct parse* parse, struct expr_reading* reading );
static void read_call( struct parse* parse, struct expr_reading* reading );
static void read_call_args( struct parse* parse, struct expr_reading* reading,
   struct list* args );
static struct format_item* read_format_item_list( struct parse* parse );
static struct format_item* read_format_item( struct parse* parse );
static void init_format_cast( struct format_cast* cast );
static void read_format_cast( struct parse* parse, struct format_cast* cast );
static bool peek_format_cast( struct parse* parse );
static void init_array_field( struct array_field* field );
static void read_array_field( struct parse* parse, struct array_field* field );
static void read_id( struct parse* parse, struct expr_reading* reading );
static void read_qualified_name_usage( struct parse* parse,
   struct expr_reading* reading );
static void read_name_usage( struct parse* parse,
   struct expr_reading* reading );
static void read_literal( struct parse* parse, struct expr_reading* reading );
static void read_string( struct parse* parse, struct expr_reading* reading );
static void read_boolean( struct parse* parse, struct expr_reading* reading );
static void read_null( struct parse* parse, struct expr_reading* reading );
static void read_upmost( struct parse* parse, struct expr_reading* reading );
static void read_current_namespace( struct parse* parse,
   struct expr_reading* reading );
static int convert_numerictoken_to_int( struct parse* parse, int base );
static int extract_radix_literal( struct parse* parse );
static void read_sure( struct parse* parse, struct expr_reading* reading );
static void read_strcpy( struct parse* parse, struct expr_reading* reading );
static void read_strcpy_call( struct parse* parse,
   struct strcpy_reading* reading );
static void read_memcpy( struct parse* parse, struct expr_reading* reading );
static void read_lengthof( struct parse* parse, struct expr_reading* reading );
static void read_paren( struct parse* parse, struct expr_reading* reading );
static void read_func_literal( struct parse* parse,
   struct expr_reading* reading, struct paren_reading* paren );
static void read_compound_literal( struct parse* parse,
   struct expr_reading* reading, struct paren_reading* paren );
static void read_cast( struct parse* parse, struct expr_reading* reading,
   struct paren_reading* paren );
static void read_paren_expr( struct parse* parse,
   struct expr_reading* reading );

void p_init_expr_reading( struct expr_reading* reading, bool in_constant,
   bool skip_assign, bool skip_call, bool expect_expr ) {
   t_init_pos_id( &reading->pos, INTERNALFILE_COMPILER );
   reading->node = NULL;
   reading->output_node = NULL;
   reading->in_constant = in_constant;
   reading->skip_assign = skip_assign;
   reading->skip_subscript = false;
   reading->skip_call = skip_call;
   reading->expect_expr = expect_expr;
}

void p_init_palrange_expr_reading( struct expr_reading* reading,
   bool skip_subscript ) {
   p_init_expr_reading( reading, false, false, false, true );
   reading->skip_subscript = skip_subscript;
}

void p_read_expr( struct parse* parse, struct expr_reading* reading ) {
   reading->pos = parse->tk_pos;
   read_op( parse, reading );
   struct expr* expr = t_alloc_expr();
   expr->root = reading->node;
   expr->pos = reading->pos;
   reading->output_node = expr;
}

static void read_op( struct parse* parse, struct expr_reading* reading ) {
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
   struct assign* assign = NULL;

   top:
   read_prefix( parse, reading );

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
   read_prefix( parse, reading );
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

   // Conditional
   // -----------------------------------------------------------------------
   if ( parse->lang == LANG_BCS && parse->tk == TK_QUESTION_MARK ) {
      struct conditional* cond = mem_alloc( sizeof( *cond ) );
      cond->node.type = NODE_CONDITIONAL;
      cond->pos = parse->tk_pos;
      cond->left = reading->node;
      cond->middle = NULL;
      cond->right = NULL;
      cond->left_spec = SPEC_NONE;
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

   // Assignment
   // -----------------------------------------------------------------------
   if ( reading->skip_assign ) {
      goto finish;
   }
   if ( assign ) {
      assign->rside = reading->node;
      reading->node = &assign->node;
      goto finish;
   }
   switch ( parse->lang ) {
   case LANG_ACS95:
      switch ( parse->tk ) {
      case TK_ASSIGN: op = AOP_NONE; break;
      case TK_ASSIGN_ADD: op = AOP_ADD; break;
      case TK_ASSIGN_SUB: op = AOP_SUB; break;
      case TK_ASSIGN_MUL: op = AOP_MUL; break;
      case TK_ASSIGN_DIV: op = AOP_DIV; break;
      case TK_ASSIGN_MOD: op = AOP_MOD; break;
      default: goto finish;
      }
      break;
   default:
      switch ( parse->tk ) {
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
      default: goto finish;
      }
   }
   assign = mem_alloc( sizeof( *assign ) );
   assign->node.type = NODE_ASSIGN;
   assign->op = op;
   assign->lside = reading->node;
   assign->rside = NULL;
   assign->pos = parse->tk_pos;
   assign->lside_type = ASSIGNLSIDE_NONE;
   p_read_tk( parse );
   switch ( parse->lang ) {
   case LANG_ACS95:
      goto top;
   default:
      read_op( parse, reading );
      assign->rside = reading->node;
      reading->node = &assign->node;
      break;
   }
   finish:
   return;
}

static struct binary* alloc_binary( int op, struct pos* pos ) {
   struct binary* binary = mem_slot_alloc( sizeof( *binary ) );
   binary->node.type = NODE_BINARY;
   binary->op = op;
   binary->lside = NULL;
   binary->rside = NULL;
   binary->pos = *pos;
   binary->operand_type = BINARYOPERAND_NONE;
   binary->value = 0;
   binary->folded = false;
   return binary;
}

static struct logical* alloc_logical( int op, struct pos* pos ) {
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

static void read_prefix( struct parse* parse, struct expr_reading* reading ) {
   // Determine which prefix operation to read.
   enum tk prefix = TK_NONE;
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      switch ( parse->tk ) {
      case TK_INC:
      case TK_DEC:
      case TK_MINUS:
      case TK_LOG_NOT:
         prefix = parse->tk;
         break;
      case TK_BIT_NOT:
      case TK_PLUS:
         if ( parse->lang == LANG_ACS ) {
            prefix = parse->tk;
         }
         break;
      default:
         break;
      }
      break;
   default:
      prefix = parse->tk;
   }
   // Read prefix operation.
   switch ( prefix ) {
   case TK_MINUS:
   case TK_PLUS:
   case TK_LOG_NOT:
   case TK_BIT_NOT:
      read_unary( parse, reading );
      break;
   case TK_INC:
   case TK_DEC:
      read_inc( parse, reading );
      break;
   default:
      read_suffix( parse, reading );
   }
}

static void read_unary( struct parse* parse, struct expr_reading* reading ) {
   int op = UOP_BIT_NOT;
   switch ( parse->tk ) {
   case TK_MINUS:
      op = UOP_MINUS;
      break;
   case TK_PLUS:
      op = UOP_PLUS;
      break;
   case TK_LOG_NOT:
      op = UOP_LOG_NOT;
      break;
   default:
      break;
   }
   struct unary* unary = mem_alloc( sizeof( *unary ) );
   unary->node.type = NODE_UNARY;
   unary->op = op;
   unary->operand = NULL;
   unary->pos = parse->tk_pos;
   unary->operand_spec = SPEC_NONE;
   p_read_tk( parse );
   read_prefix( parse, reading );
   unary->operand = reading->node;
   reading->node = &unary->node;
}

static void read_inc( struct parse* parse, struct expr_reading* reading ) {
   struct inc* inc = alloc_inc( parse->tk_pos, ( parse->tk == TK_DEC ) );
   p_read_tk( parse );
   read_prefix( parse, reading );
   inc->operand = reading->node;
   reading->node = &inc->node;
}

static struct inc* alloc_inc( struct pos pos, bool dec ) {
   struct inc* inc = mem_alloc( sizeof( *inc ) );
   inc->node.type = NODE_INC;
   inc->operand = NULL;
   inc->pos = pos;
   inc->post = false;
   inc->dec = dec;
   inc->fixed = false;
   return inc;
}

static void read_primary( struct parse* parse, struct expr_reading* reading ) {
   // Determine which primary to read.
   enum tk primary = TK_NONE;
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      switch ( parse->tk ) {
      case TK_ID:
      case TK_PRINT:
      case TK_PRINTBOLD:
      case TK_LOG:
      case TK_HUDMESSAGE:
      case TK_HUDMESSAGEBOLD:
      case TK_STRPARAM:
      case TK_ACSEXECUTEWAIT:
      case TK_ACSNAMEDEXECUTEWAIT:
         primary = TK_ID;
         break;
      case TK_LIT_DECIMAL:
      case TK_LIT_OCTAL:
      case TK_LIT_HEX:
      case TK_LIT_CHAR:
      case TK_LIT_FIXED:
      case TK_LIT_STRING:
      case TK_LIT_RADIX:
      case TK_STRCPY:
      case TK_PAREN_L:
         primary = parse->tk;
         break;
      default:
         break;
      }
      break;
   default:
      primary = parse->tk;
   }
   // Read primary.
   switch ( primary ) {
   case TK_ID:
      read_id( parse, reading );
      break;
   case TK_LIT_DECIMAL:
   case TK_LIT_OCTAL:
   case TK_LIT_HEX:
   case TK_LIT_BINARY:
   case TK_LIT_RADIX:
   case TK_LIT_CHAR:
      read_literal( parse, reading );
      break;
   case TK_LIT_FIXED:
      read_fixed_literal( parse, reading );
      break;
   case TK_LIT_STRING:
      read_string( parse, reading );
      break;
   case TK_TRUE:
   case TK_FALSE:
      read_boolean( parse, reading );
      break;
   case TK_NULL:
      read_null( parse, reading );
      break;
   case TK_COLONCOLON:
      read_qualified_name_usage( parse, reading );
      break;
   case TK_UPMOST:
      read_upmost( parse, reading );
      break;
   case TK_NAMESPACE:
      read_current_namespace( parse, reading );
      break;
   case TK_STRCPY:
      read_strcpy( parse, reading );
      break;
   case TK_MEMCPY:
      read_memcpy( parse, reading );
      break;
   case TK_LENGTHOF:
      read_lengthof( parse, reading );
      break;
   case TK_INT:
   case TK_FIXED:
   case TK_BOOL:
   case TK_STR:
      read_conversion( parse, reading );
      break;
   case TK_PAREN_L:
      read_paren( parse, reading );
      break;
   default:
      p_unexpect_diag( parse );
      // For areas of code where an expression is to be expected. Only show
      // this message when not a single piece of the expression is read.
      if ( reading->expect_expr &&
         t_same_pos( &parse->tk_pos, &reading->pos ) ) {
         p_unexpect_last_name( parse, NULL, "expression" );
      }
      p_bail( parse );
   }
}

static void read_id( struct parse* parse, struct expr_reading* reading ) {
   if ( p_peek( parse ) == TK_COLONCOLON ) {
      read_qualified_name_usage( parse, reading );
   }
   else {
      read_name_usage( parse, reading );
   }
}

static void read_qualified_name_usage( struct parse* parse,
   struct expr_reading* reading ) {
   struct qualified_name_usage* usage = mem_alloc( sizeof( *usage ) );
   usage->node.type = NODE_QUALIFIEDNAMEUSAGE;
   usage->path = p_read_path( parse );
   usage->object = NULL;
   reading->node = &usage->node;
}

static void read_name_usage( struct parse* parse,
   struct expr_reading* reading ) {
   p_test_tk( parse, TK_ID );
   struct name_usage* usage = mem_slot_alloc( sizeof( *usage ) );
   usage->node.type = NODE_NAME_USAGE;
   usage->text = parse->tk_text;
   usage->pos = parse->tk_pos;
   usage->object = NULL;
   reading->node = &usage->node;
   p_read_tk( parse );
}

static void read_literal( struct parse* parse, struct expr_reading* reading ) {
   struct literal* literal = mem_slot_alloc( sizeof( *literal ) );
   literal->node.type = NODE_LITERAL;
   literal->value = p_extract_literal_value( parse );
   reading->node = &literal->node;
   p_read_tk( parse );
}

static void read_string( struct parse* parse, struct expr_reading* reading ) {
   p_test_tk( parse, TK_LIT_STRING );
   if ( parse->tk_length > parse->lang_limits->max_string_length ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "string too long (its length is %d, but maximum length is %d)",
         parse->tk_length, parse->lang_limits->max_string_length );
      p_bail( parse );
   }
   // In ACS, a string in an imported library is discarded.
   if ( parse->lang == LANG_ACS && parse->lib->imported ) {
      reading->node = ( struct node* ) t_alloc_literal();
   }
   else {
      struct indexed_string* string = t_intern_string( parse->task,
         parse->tk_text, parse->tk_length );
      string->in_source_code = true;
      struct indexed_string_usage* usage = mem_slot_alloc( sizeof( *usage ) );
      usage->node.type = NODE_INDEXED_STRING_USAGE;
      usage->string = string;
      reading->node = &usage->node;
   }
   p_read_tk( parse );
}

static void read_boolean( struct parse* parse, struct expr_reading* reading ) {
   if ( parse->tk == TK_TRUE ) {
      static struct boolean boolean = { { NODE_BOOLEAN }, 1 };
      reading->node = &boolean.node;
      p_read_tk( parse );
   }
   else {
      static struct boolean boolean = { { NODE_BOOLEAN }, 0 };
      reading->node = &boolean.node;
      p_test_tk( parse, TK_FALSE );
      p_read_tk( parse );
   }
}

static void read_null( struct parse* parse, struct expr_reading* reading ) {
   p_test_tk( parse, TK_NULL );
   p_read_tk( parse );
   static struct node node = { NODE_NULL };
   reading->node = &node;
}

static void read_upmost( struct parse* parse, struct expr_reading* reading ) {
   if ( p_peek( parse ) == TK_COLONCOLON ) {
      read_qualified_name_usage( parse, reading );
   }
   else {
      p_test_tk( parse, TK_UPMOST );
      static struct node node = { NODE_UPMOST };
      reading->node = &node;
      p_read_tk( parse );
   }
}

static void read_current_namespace( struct parse* parse,
   struct expr_reading* reading ) {
   if ( p_peek( parse ) == TK_COLONCOLON ) {
      read_qualified_name_usage( parse, reading );
   }
   else {
      p_test_tk( parse, TK_NAMESPACE );
      static struct node node = { NODE_CURRENTNAMESPACE };
      reading->node = &node;
      p_read_tk( parse );
   }
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
   else if ( parse->tk == TK_LIT_RADIX ) {
      return extract_radix_literal( parse );
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
static int convert_numerictoken_to_int( struct parse* parse, int base ) {
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
      ENGINE_MAX_INT_VALUE - value < 0 ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "numeric value `%s` is too large", parse->tk_text );
      p_bail( parse );
   }
   return ( int ) value;
}

static int extract_radix_literal( struct parse* parse ) {
   static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
   enum { LOWER_LIMIT = 2 };
   enum { UPPER_LIMIT = ARRAY_SIZE( digits ) - 1 };
   int i = 0;
   int base = 0;
   while ( parse->tk_text[ i ] != '_' ) {
      base = base * 10 + parse->tk_text[ i ] - '0';
      if ( base > UPPER_LIMIT ) {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "radix too large (maximum radix: %d)", UPPER_LIMIT );
         p_bail( parse );
      }
      ++i;
   }
   if ( base < LOWER_LIMIT ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "radix too small (minimum radix: %d)", LOWER_LIMIT );
      p_bail( parse );
   }
   ++i;
   int value = 0;
   int first_digit_pos = i;
   while ( parse->tk_text[ i ] ) {
      int digit_value = 0;
      while ( parse->tk_text[ i ] != digits[ digit_value ] &&
         digit_value < base ) {
         ++digit_value;
      }
      if ( digit_value == base ) {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "invalid digit (`%c`) in position %d of radix-%d literal",
            parse->tk_text[ i ], i - first_digit_pos + 1, base );
         p_bail( parse );
      }
      // TODO: Check for overflow.
      value = base * value + digit_value;
      ++i;
   }
   return value;
}

static void read_fixed_literal( struct parse* parse,
   struct expr_reading* reading ) {
   struct fixed_literal* literal = mem_slot_alloc( sizeof( *literal ) );
   literal->node.type = NODE_FIXED_LITERAL;
   literal->value = p_extract_fixed_literal_value( parse->tk_text );
   reading->node = &literal->node;
   p_read_tk( parse );
}

int p_extract_fixed_literal_value( const char* text ) {
   double value = atof( text );
   return
      // Whole.
      ( ( int ) value << 16 ) +
      // Fraction.
      ( int ) ( ( 1 << 16 ) * ( value - ( int ) value ) );
}

static void read_conversion( struct parse* parse,
   struct expr_reading* reading ) {
   int spec = SPEC_INT;
   switch ( parse->tk ) {
   case TK_FIXED:
      spec = SPEC_FIXED;
      p_read_tk( parse );
      break;
   case TK_BOOL:
      spec = SPEC_BOOL;
      p_read_tk( parse );
      break;
   case TK_STR:
      spec = SPEC_STR;
      p_read_tk( parse );
      break;
   default:
      p_test_tk( parse, TK_INT );
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   struct conversion* conv = mem_alloc( sizeof( *conv ) );
   conv->node.type = NODE_CONVERSION;
   conv->expr = expr.output_node;
   conv->spec = spec;
   conv->spec_from = SPEC_NONE;
   conv->from_ref = false;
   reading->node = &conv->node;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
}

static void read_suffix( struct parse* parse, struct expr_reading* reading ) {
   read_primary( parse, reading );
   while ( true ) {
      // Determine which suffix operation to read.
      enum tk suffix = TK_NONE;
      switch ( parse->lang ) {
      case LANG_ACS:
         switch ( parse->tk ) {
         case TK_BRACKET_L:
         case TK_PAREN_L:
         case TK_INC:
         case TK_DEC:
            suffix = parse->tk;
            break;
         default:
            break;
         }
         break;
      case LANG_ACS95:
         switch ( parse->tk ) {
         case TK_PAREN_L:
         case TK_INC:
         case TK_DEC:
            suffix = parse->tk;
            break;
         default:
            break;
         }
         break;
      default:
         suffix = parse->tk;
      }
      // Read suffix operation.
      switch ( suffix ) {
      case TK_BRACKET_L:
         if ( reading->skip_subscript ) {
            return;
         }
         read_subscript( parse, reading );
         break;
      case TK_DOT:
         read_access( parse, reading );
         break;
      case TK_PAREN_L:
         if ( ! reading->skip_call ) {
            read_call( parse, reading );
         }
         else {
            return;
         }
         break;
      case TK_INC:
      case TK_DEC:
         read_post_inc( parse, reading );
         break;
      case TK_LOG_NOT:
         read_sure( parse, reading );
         break;
      default:
         return;
      }
   }
}

static void read_subscript( struct parse* parse,
   struct expr_reading* reading ) {
   struct subscript* subscript = mem_alloc( sizeof( *subscript ) );
   subscript->node.type = NODE_SUBSCRIPT;
   subscript->pos = parse->tk_pos;
   p_read_tk( parse );
   struct expr_reading index;
   p_init_expr_reading( &index, reading->in_constant, false, false, true );
   p_read_expr( parse, &index );
   p_test_tk( parse, TK_BRACKET_R );
   p_read_tk( parse );
   subscript->index = index.output_node;
   subscript->lside = reading->node;
   subscript->string = false;
   reading->node = &subscript->node;
}

static void read_access( struct parse* parse, struct expr_reading* reading ) { 
   struct pos pos = parse->tk_pos;
   p_read_tk( parse );
   p_test_tk( parse, TK_ID );
   struct access* access = alloc_access( parse->tk_text, pos );
   access->lside = reading->node;
   reading->node = &access->node;
   p_read_tk( parse );
}

static struct access* alloc_access( const char* name, struct pos pos ) {
   struct access* access = mem_alloc( sizeof( *access ) );
   access->node.type = NODE_ACCESS;
   access->name = name;
   access->pos = pos;
   access->lside = NULL;
   access->rside = NULL;
   access->type = ACCESS_STRUCTURE;
   return access;
}

static void read_post_inc( struct parse* parse,
   struct expr_reading* reading ) {
   struct inc* inc = alloc_inc( parse->tk_pos, ( parse->tk == TK_DEC ) );
   inc->post = true;
   inc->operand = reading->node;
   reading->node = &inc->node;
   p_read_tk( parse );
}

static void read_call( struct parse* parse, struct expr_reading* reading ) {
   struct call* call = t_alloc_call();
   call->pos = parse->tk_pos;
   call->operand = reading->node;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   if ( peek_format_cast( parse ) ) {
      call->format_item = read_format_item_list( parse );
      if ( parse->tk == TK_SEMICOLON ) {
         p_read_tk( parse );
         read_call_args( parse, reading, &call->args );
      }
   }
   else {
      if ( parse->tk != TK_PAREN_R ) {
         // This relic is not necessary in new code. The compiler is smart
         // enough to figure out when to use the constant variant of an
         // instruction.
         if ( parse->tk == TK_CONST ) {
            p_read_tk( parse );
            p_test_tk( parse, TK_COLON );
            p_read_tk( parse );
            call->constant = true;
         }
         read_call_args( parse, reading, &call->args );
      }
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   reading->node = &call->node;
}

static void read_call_args( struct parse* parse, struct expr_reading* reading,
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

static struct format_item* read_format_item_list( struct parse* parse ) {
   struct format_item* head = NULL;
   struct format_item* tail = NULL;
   while ( true ) {
      struct format_item* item = read_format_item( parse );
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

static struct format_item* read_format_item( struct parse* parse ) {
   struct format_item* item = t_alloc_format_item();
   item->pos = parse->tk_pos;
   struct format_cast cast;
   init_format_cast( &cast );
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

static void init_format_cast( struct format_cast* cast ) {
   cast->unknown = false;
   cast->type = FCAST_DECIMAL;
}

static void read_format_cast( struct parse* parse, struct format_cast* cast ) {
   cast->pos = parse->tk_pos;
   switch ( parse->lang ) {
   case LANG_ACS95:
      switch ( parse->tk_text[ 0 ] ) {
      case 'c': cast->type = FCAST_CHAR; break;
      case 'd': cast->type = FCAST_DECIMAL; break;
      case 'i': cast->type = FCAST_RAW; break;
      case 's': cast->type = FCAST_STRING; break;
      default:
         cast->unknown = true;
      }
      break;
   default:
      switch ( parse->tk_text[ 0 ] ) {
      case 'a': cast->type = FCAST_ARRAY; break;
      case 'b': cast->type = FCAST_BINARY; break;
      case 'c': cast->type = FCAST_CHAR; break;
      case 'd': cast->type = FCAST_DECIMAL; break;
      case 'f': cast->type = FCAST_FIXED; break;
      case 'i': cast->type = FCAST_RAW; break;
      case 'k': cast->type = FCAST_KEY; break;
      case 'l': cast->type = FCAST_LOCAL_STRING; break;
      case 'n': cast->type = FCAST_NAME; break;
      case 's': cast->type = FCAST_STRING; break;
      case 'x': cast->type = FCAST_HEX; break;
      default:
         cast->unknown = true;
      }
      if ( parse->tk_length != 1 ) {
         cast->unknown = true;
      }
   }
   if ( cast->unknown ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "unknown format-cast: %s", parse->tk_text );
      p_bail( parse );
   }
   p_read_tk( parse );
   p_test_tk( parse, TK_COLON );
   p_read_tk( parse );
}

static bool peek_format_cast( struct parse* parse ) {
   return ( parse->tk == TK_ID && p_peek( parse ) == TK_COLON );
}

static void init_array_field( struct array_field* field ) {
   field->array = NULL;
   field->offset = NULL;
   field->length = NULL;
}

static void read_array_field( struct parse* parse,
   struct array_field* field ) {
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

static void read_sure( struct parse* parse, struct expr_reading* reading ) {
   p_test_tk( parse, TK_LOG_NOT );
   struct sure* sure = mem_alloc( sizeof( *sure ) );
   sure->node.type = NODE_SURE;
   sure->pos = parse->tk_pos;
   sure->operand = reading->node;
   sure->ref = NULL;
   sure->already_safe = false;
   reading->node = &sure->node;
   p_read_tk( parse );
   p_test_tk( parse, TK_LOG_NOT );
   p_read_tk( parse );
}

static void read_strcpy( struct parse* parse, struct expr_reading* reading ) {
   p_test_tk( parse, TK_STRCPY );
   p_read_tk( parse );
   struct strcpy_reading call_r;
   read_strcpy_call( parse, &call_r );
   struct strcpy_call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_STRCPY;
   call->array = call_r.array;
   call->array_offset = call_r.array_offset;
   call->array_length = call_r.array_length;
   call->string = call_r.string;
   call->offset = call_r.offset;
   reading->node = &call->node;
}

static void read_strcpy_call( struct parse* parse,
   struct strcpy_reading* reading ) {
   reading->array = NULL;
   reading->array_offset = NULL;
   reading->array_length = NULL;
   reading->string = NULL;
   reading->offset = NULL;
   reading->array_cast = false;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   // Array field. The format-cast is optional in BCS.
   if ( ! ( parse->lang == LANG_BCS && ! peek_format_cast( parse ) ) ) {
      struct format_cast cast;
      init_format_cast( &cast );
      read_format_cast( parse, &cast );
      if ( cast.type != FCAST_ARRAY ) {
         p_diag( parse, DIAG_SYNTAX | DIAG_POS_ERR, &cast.pos,
            "unexpected format-cast" );
         p_diag( parse, DIAG_POS, &cast.pos,
            "expecting `a:` here" );
         p_bail( parse );
      }
      struct array_field field;
      init_array_field( &field );
      read_array_field( parse, &field );
      reading->array = field.array;
      reading->array_offset = field.offset;
      reading->array_length = field.length;
      reading->array_cast = true;
   }
   else {
      struct expr_reading expr;
      p_init_expr_reading( &expr, false, false, false, true );
      p_read_expr( parse, &expr );
      reading->array = expr.output_node;
   }
   p_test_tk( parse, TK_COMMA );
   p_read_tk( parse );
   // String field.
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   reading->string = expr.output_node;
   // String-offset field. Optional.
   if ( parse->tk == TK_COMMA ) {
      p_read_tk( parse );
      p_init_expr_reading( &expr, false, false, false, true );
      p_read_expr( parse, &expr );
      reading->offset = expr.output_node;
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
}

static void read_memcpy( struct parse* parse, struct expr_reading* reading ) {
   p_test_tk( parse, TK_MEMCPY );
   p_read_tk( parse );
   struct strcpy_reading call_r;
   read_strcpy_call( parse, &call_r );
   struct memcpy_call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_MEMCPY;
   call->destination = call_r.array;
   call->destination_offset = call_r.array_offset;
   call->destination_length = call_r.array_length;
   call->source = call_r.string;
   call->source_offset = call_r.offset;
   call->type = MEMCPY_ARRAY;
   call->array_cast = call_r.array_cast;
   reading->node = &call->node;
}

static void read_lengthof( struct parse* parse,
   struct expr_reading* reading ) {
   p_test_tk( parse, TK_LENGTHOF );
   p_read_tk( parse );
   struct lengthof* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_LENGTHOF;
   call->operand = NULL;
   call->value = 0;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading operand;
   p_init_expr_reading( &operand, false, false, false, true );
   p_read_expr( parse, &operand );
   call->operand = operand.output_node;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   reading->node = &call->node;
}

static void read_paren( struct parse* parse, struct expr_reading* reading ) {
   if ( p_is_paren_type( parse ) ) {
      struct paren_reading paren;
      p_init_paren_reading( parse, &paren );
      p_read_paren_type( parse, &paren );
      if ( paren.func ) {
         read_func_literal( parse, reading, &paren );
      }
      else if ( paren.var ) {
         read_compound_literal( parse, reading, &paren );
      }
      else {
         read_cast( parse, reading, &paren );
      }
   }
   else {
      read_paren_expr( parse, reading );
   }
}

static void read_func_literal( struct parse* parse,
   struct expr_reading* reading, struct paren_reading* paren ) {
   reading->node = &paren->func->object.node;
}

static void read_compound_literal( struct parse* parse,
   struct expr_reading* reading,
   struct paren_reading* paren ) {
   struct compound_literal* literal = mem_alloc( sizeof( *literal ) );
   literal->node.type = NODE_COMPOUNDLITERAL;
   literal->var = paren->var;
   reading->node = &literal->node;
}

static void read_cast( struct parse* parse, struct expr_reading* reading,
   struct paren_reading* paren ) {
   read_prefix( parse, reading );
   struct cast* cast = mem_alloc( sizeof( *cast ) );
   cast->node.type = NODE_CAST;
   cast->operand = reading->node;
   cast->pos = paren->cast.pos;
   cast->spec = paren->cast.spec;
   reading->node = &cast->node;
}

static void read_paren_expr( struct parse* parse,
   struct expr_reading* reading ) {
   struct paren* paren = mem_alloc( sizeof( *paren ) );
   paren->node.type = NODE_PAREN;
   paren->inside = NULL;
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   struct expr_reading nested_expr;
   p_init_expr_reading( &nested_expr, reading->in_constant, false, false,
      reading->expect_expr );
   read_op( parse, &nested_expr );
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   paren->inside = nested_expr.node;
   reading->node = &paren->node;
}
