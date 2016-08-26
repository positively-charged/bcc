#include <string.h>

#include "phase.h"
#include "codegen/phase.h"
#include "codegen/pcode.h"

static struct inline_asm* alloc_inline_asm( void );
static void read_arg( struct parse* parse, struct inline_asm* inline_asm );
static struct inline_asm_arg* alloc_inline_asm_arg( struct pos* pos );

void p_read_asm( struct parse* parse, struct stmt_reading* reading ) {
   parse->create_nltk = true;
   struct inline_asm* inline_asm = alloc_inline_asm();
   p_test_tk( parse, TK_GT );
   p_read_tk( parse );
   p_test_tk( parse, TK_ID );
   inline_asm->name = parse->tk_text;
   inline_asm->pos = parse->tk_pos;
   p_read_tk( parse );
   if ( parse->tk != TK_NL ) {
      while ( true ) {
         read_arg( parse, inline_asm );
         if ( parse->tk == TK_COMMA ) {
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
   }
   p_test_tk( parse, TK_NL );
   parse->create_nltk = false;
   p_read_tk( parse );
   reading->node = &inline_asm->node;
}

struct inline_asm* alloc_inline_asm( void ) {
   struct inline_asm* inline_asm = mem_alloc( sizeof( *inline_asm ) );
   inline_asm->node.type = NODE_INLINE_ASM;
   inline_asm->name = NULL;
   inline_asm->next = NULL;
   list_init( &inline_asm->args );
   inline_asm->opcode = 0;
   inline_asm->obj_pos = 0;
   return inline_asm;
}

void read_arg( struct parse* parse, struct inline_asm* inline_asm ) {
   struct inline_asm_arg* arg = alloc_inline_asm_arg( &parse->tk_pos );
   list_append( &inline_asm->args, arg );
   if (
      parse->tk == TK_LIT_DECIMAL ||
      parse->tk == TK_LIT_OCTAL ||
      parse->tk == TK_LIT_HEX ) {
      arg->value.number = p_extract_literal_value( parse );
      p_read_tk( parse );
   }
   else if ( parse->tk == TK_ID ) {
      arg->type = INLINE_ASM_ARG_ID;
      arg->value.id = parse->tk_text;
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_PAREN_L );
      p_read_tk( parse );
      struct expr_reading expr;
      p_init_expr_reading( &expr, false, false, false, true );
      p_read_expr( parse, &expr );
      arg->type = INLINE_ASM_ARG_EXPR;
      arg->value.expr = expr.output_node;
      p_test_tk( parse, TK_PAREN_R );
      p_read_tk( parse );
   }
}

struct inline_asm_arg* alloc_inline_asm_arg( struct pos* pos ) {
   struct inline_asm_arg* arg = mem_alloc( sizeof( *arg ) );
   arg->type = INLINE_ASM_ARG_NUMBER;
   arg->value.number = 0;
   arg->pos = *pos;
   return arg;
}