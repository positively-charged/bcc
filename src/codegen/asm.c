#include "phase.h"

static void write_arg( struct codegen* codegen, struct inline_asm_arg* arg );
static void write_expr_arg( struct codegen* codegen,
   struct inline_asm_arg* arg );

void p_visit_inline_asm( struct codegen* codegen,
   struct inline_asm* inline_asm ) {
   list_iter_t i;
   list_iter_init( &i, &inline_asm->args );
   c_unoptimized_opc( codegen, inline_asm->opcode );
   while ( ! list_end( &i ) ) {
      write_arg( codegen, list_data( &i ) );
      list_next( &i );
   }
}

void write_arg( struct codegen* codegen, struct inline_asm_arg* arg ) {
   switch ( arg->type ) {
   case INLINE_ASM_ARG_NUMBER:
      c_arg( codegen, arg->value.number );
      break;
   case INLINE_ASM_ARG_LABEL:
      if ( ! arg->value.label->point ) {
         arg->value.label->point = c_create_point( codegen );
      }
      c_point_arg( codegen, arg->value.label->point );
      break;
   case INLINE_ASM_ARG_VAR:
      c_arg( codegen, arg->value.var->index );
      break;
   case INLINE_ASM_ARG_PARAM:
      c_arg( codegen, arg->value.param->index );
      break;
   case INLINE_ASM_ARG_FUNC:
      if ( arg->value.func->type == FUNC_EXT ) {
         struct func_ext* impl = arg->value.func->impl;
         c_arg( codegen, impl->id );
      }
      else {
         struct func_user* impl = arg->value.func->impl;
         c_arg( codegen, impl->index );
      }
      break;
   case INLINE_ASM_ARG_EXPR:
      write_expr_arg( codegen, arg );
      break;
   default:
      UNREACHABLE();
   }
}

void write_expr_arg( struct codegen* codegen, struct inline_asm_arg* arg ) {
   c_arg( codegen, arg->value.expr->value );
   if ( arg->value.expr->has_str ) {
      struct indexed_string* string = t_lookup_string( codegen->task,
         arg->value.expr->value );
      if ( string ) {
         c_append_string( codegen, string );
      }
   }
}