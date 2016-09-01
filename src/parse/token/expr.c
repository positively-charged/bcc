/*

   TODO: Make an abstraction for manipulating the expression result. This is so
   we could catch overflow errors and other errors; and for converting textual
   representations of numbers.

*/

#include <string.h>

#include "phase.h"

struct calc {
   jmp_buf bail;
};

static int eval_binary( struct parse* parse, struct calc* calc );
static int eval_prefix( struct parse* parse, struct calc* calc );
static int eval_primary( struct parse* parse, struct calc* calc );
static int eval_ch( struct parse* parse );
static int eval_id( struct parse* parse );
static int eval_defined( struct parse* parse );
static int eval_number( struct parse* parse );
static int eval_paren( struct parse* parse, struct calc* calc );

int p_eval_prep_expr( struct parse* parse ) {
   parse->read_flags = READF_ESCAPESEQ;
   p_read_expanpreptk( parse );
   struct pos pos = parse->token->pos;
   if ( parse->token->type == TK_NL ) {
      p_diag( parse, DIAG_POS_ERR, &pos,
         "missing expression" );
      p_bail( parse );
   }
   int value = 0;
   struct calc calc;
   if ( setjmp( calc.bail ) == 0 ) {
      value = eval_binary( parse, &calc );
   }
   else {
      p_diag( parse, DIAG_POS_ERR, &pos,
         "invalid expression" );
      p_bail( parse );
   }
   parse->read_flags = READF_NONE;
   return value;
}

int eval_binary( struct parse* parse, struct calc* calc ) {
   int mul = 0;
   int mul_lside = 0;
   int add = 0;
   int add_lside = 0;
   int shift = 0;
   int shift_lside = 0;
   int lt = 0;
   int lt_lside = 0;
   int eq = 0;
   int eq_lside = 0;
   int bit_and = 0;
   int bit_and_lside = 0;
   int bit_xor = 0;
   int bit_xor_lside = 0;
   int bit_or = 0;
   int bit_or_lside = 0;
   int log_and = 0;
   int log_and_lside = 0;
   int log_or = 0;
   int log_or_lside = 0;
   int operand;
   struct pos pos;

   top:
   operand = eval_prefix( parse, calc );

   mul:
   // -----------------------------------------------------------------------
   switch ( parse->token->type ) {
   case TK_STAR:
   case TK_SLASH:
   case TK_MOD:
      mul = parse->token->type;
      pos = parse->token->pos;
      p_read_expanpreptk( parse );
      break;
   default:
      goto add;
   }
   mul_lside = operand;
   operand = eval_prefix( parse, calc );
   if ( mul == TK_STAR ) {
      operand = ( mul_lside * operand );
   }
   else {
      if ( ! operand ) {
         p_diag( parse, DIAG_POS_ERR, &pos,
            "division by zero" );
         p_bail( parse );
      }
      operand = ( mul == TK_SLASH ) ?
         ( mul_lside / operand ) :
         ( mul_lside % operand );
   }
   goto mul;

   add:
   // -----------------------------------------------------------------------
   switch ( add ) {
   case TK_PLUS:
      operand = ( add_lside + operand );
      break;
   case TK_MINUS:
      operand = ( add_lside - operand );
      break;
   }
   switch ( parse->token->type ) {
   case TK_PLUS:
   case TK_MINUS:
      add = parse->token->type;
      add_lside = operand;
      p_read_expanpreptk( parse );
      goto top;
   default:
      add = 0;
   }

   // -----------------------------------------------------------------------
   switch ( shift ) {
   case TK_SHIFT_L:
      operand = ( shift_lside << operand );
      break;
   case TK_SHIFT_R:
      operand = ( shift_lside >> operand );
      break;
   }
   switch ( parse->token->type ) {
   case TK_SHIFT_L:
   case TK_SHIFT_R:
      shift = parse->token->type;
      shift_lside = operand;
      p_read_expanpreptk( parse );
      goto top;
   default:
      shift = 0;
   }

   // -----------------------------------------------------------------------
   switch ( lt ) {
   case TK_LT:
      operand = ( lt_lside < operand );
      break;
   case TK_LTE:
      operand = ( lt_lside <= operand );
      break;
   case TK_GT:
      operand = ( lt_lside > operand );
      break;
   case TK_GTE:
      operand = ( lt_lside >= operand );
      break;
   }
   switch ( parse->token->type ) {
   case TK_LT:
   case TK_LTE:
   case TK_GT:
   case TK_GTE:
      lt = parse->token->type;
      lt_lside = operand;
      p_read_expanpreptk( parse );
      goto top;
   default:
      lt = 0;
   }

   // -----------------------------------------------------------------------
   switch ( eq ) {
   case TK_EQ:
      operand = ( eq_lside == operand );
      break;
   case TK_NEQ:
      operand = ( eq_lside != operand );
      break;
   }
   switch ( parse->token->type ) {
   case TK_EQ:
   case TK_NEQ:
      eq = parse->token->type;
      eq_lside = operand;
      p_read_expanpreptk( parse );
      goto top;
   default:
      eq = 0;
   }

   // -----------------------------------------------------------------------
   if ( bit_and ) {
      operand = ( bit_and_lside & operand );
      bit_and = 0;
   }
   if ( parse->token->type == TK_BIT_AND ) {
      bit_and = parse->token->type;
      bit_and_lside = operand;
      p_read_expanpreptk( parse );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_xor ) {
      operand = ( bit_xor_lside ^ operand );
      bit_xor = 0;
   }
   if ( parse->token->type == TK_BIT_XOR ) {
      bit_xor = parse->token->type;
      bit_xor_lside = operand;
      p_read_expanpreptk( parse );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_or ) {
      operand = ( bit_or_lside | operand );
      bit_or = 0;
   }
   if ( parse->token->type == TK_BIT_OR ) {
      bit_or = parse->token->type;
      bit_or_lside = operand;
      p_read_expanpreptk( parse );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_and ) {
      operand = ( log_and_lside && operand );
      log_and = 0;
   }
   if ( parse->token->type == TK_LOG_AND ) {
      log_and = parse->token->type;
      log_and_lside = operand;
      p_read_expanpreptk( parse );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_or ) {
      operand = ( log_or_lside || operand );
      log_or = 0;
   }
   if ( parse->token->type == TK_LOG_OR ) {
      log_or = parse->token->type;
      log_or_lside = operand;
      p_read_expanpreptk( parse );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( parse->token->type == TK_QUESTION_MARK ) {
      p_read_expanpreptk( parse );
      // Since the parser considers the middle operand optional, for
      // consistency, also make it optional in the preprocessor.
      int middle = operand;
      if ( parse->token->type != TK_COLON ) {
         middle = eval_binary( parse, calc );
      }
      p_test_preptk( parse, TK_COLON );
      p_read_expanpreptk( parse );
      int right = eval_binary( parse, calc );
      operand = ( operand ? middle : right );
   }

   return operand;
}

int eval_prefix( struct parse* parse, struct calc* calc ) {
   switch ( parse->token->type ) {
      int value;
   case TK_PLUS:
      p_read_expanpreptk( parse );
      value = eval_prefix( parse, calc );
      return value;
   case TK_MINUS:
      p_read_expanpreptk( parse );
      value = eval_prefix( parse, calc );
      return ( - value );
   case TK_LOG_NOT:
      p_read_expanpreptk( parse );
      value = eval_prefix( parse, calc );
      return ( ! value );
   case TK_BIT_NOT:
      p_read_expanpreptk( parse );
      value = eval_prefix( parse, calc );
      return ( ~ value );
   default:
      value = eval_primary( parse, calc );
      return value;
   }
}

int eval_primary( struct parse* parse, struct calc* calc ) {
   switch ( parse->token->type ) {
   case TK_LIT_CHAR:
      return eval_ch( parse );
   case TK_ID:
      return eval_id( parse );
   case TK_LIT_DECIMAL:
   case TK_LIT_OCTAL:
   case TK_LIT_HEX:
      return eval_number( parse );
   case TK_PAREN_L:
      return eval_paren( parse, calc );
   default:
      longjmp( calc->bail, 1 );
      return 0;
   }
}

int eval_ch( struct parse* parse ) {
   int value = parse->token->text[ 0 ];
   p_read_expanpreptk( parse );
   return value;
}

int eval_id( struct parse* parse ) {
   if ( strcmp( parse->token->text, "defined" ) == 0 ) {
      return eval_defined( parse );
   }
   else {
      p_read_expanpreptk( parse );
      return 0;
   }
}

int eval_defined( struct parse* parse ) {
   p_test_preptk( parse, TK_ID );
   p_read_preptk( parse );
   bool paren = false;
   if ( parse->token->type == TK_PAREN_L ) {
      p_read_preptk( parse );
      paren = true;
   }
   p_test_preptk( parse, TK_ID );
   bool defined = p_is_macro_defined( parse, parse->token->text );
   if ( paren ) {
      p_read_preptk( parse );
      p_test_preptk( parse, TK_PAREN_R );
   }
   p_read_expanpreptk( parse );
   return ( int ) defined;
}

int eval_number( struct parse* parse ) {
   int value = 0;
   switch ( parse->token->type ) {
   case TK_LIT_DECIMAL:
      value = strtol( parse->token->text, NULL, 10 );
      break;
   case TK_LIT_OCTAL:
      value = strtol( parse->token->text, NULL, 8 );
      break;
   case TK_LIT_HEX:
      value = strtol( parse->token->text, NULL, 16 );
      break;
   default:
      break;
   }
   p_read_expanpreptk( parse );
   return value;
}

int eval_paren( struct parse* parse, struct calc* calc ) {
   p_test_preptk( parse, TK_PAREN_L );
   p_read_expanpreptk( parse );
   int value = eval_binary( parse, calc );
   p_test_preptk( parse, TK_PAREN_R );
   p_read_expanpreptk( parse );
   return value;
}