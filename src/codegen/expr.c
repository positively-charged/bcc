#include <limits.h>

#include "phase.h"
#include "pcode.h"

struct operand {
   struct structure* type;
   struct dim* dim;
   enum {
      ACTION_PUSH_VALUE,
      ACTION_PUSH_VAR
   } action;
   enum {
      METHOD_NONE,
      METHOD_INDEXED,
      METHOD_ELEMENT
   } method;
   enum {
      TRIGGER_INDEX
   } trigger;
   int storage;
   int index;
   int base;
   bool push;
   bool push_temp;
   bool pushed;
   bool pushed_element;
};

static void init_operand( struct operand* codegen );
static void visit_operand( struct codegen* codegen, struct operand*,
   struct node* );
static void visit_constant( struct codegen* codegen, struct operand*,
   struct constant* );
static void visit_enumerator( struct codegen* codegen,
   struct operand* operand, struct enumerator* enumerator );
static void visit_unary( struct codegen* codegen, struct operand*,
   struct unary* );
static void visit_inc( struct codegen* codegen,
   struct operand* result, struct inc* inc );
static void inc_array( struct codegen* codegen, struct operand* result,
   struct inc* inc, struct operand* operand );
static void inc_element( struct codegen* codegen, int storage, int index,
   bool do_inc );
static void inc_var( struct codegen* codegen, struct operand* result,
   struct inc* inc, struct operand* operand );
static void inc_indexed( struct codegen* codegen, int storage, int index,
   bool do_inc );
static void visit_call( struct codegen* codegen, struct operand*,
   struct call* );
static void visit_aspec_call( struct codegen* codegen, struct operand*,
   struct call* );
static void visit_ext_call( struct codegen* codegen, struct operand*,
   struct call* );
static int push_nonzero_args( struct codegen* codegen, struct param* params,
   struct list* args, int min );
static void visit_ded_call( struct codegen* codegen, struct operand*,
   struct call* );
static void visit_format_call( struct codegen* codegen, struct operand*,
   struct call* );
static void visit_formatblock_arg( struct codegen* codegen,
   struct format_block_usage* usage );
static void write_formatblock_arg( struct codegen* codegen,
   struct format_block_usage* usage );
static void visit_array_format_item( struct codegen* codegen,
   struct format_item* );
static void visit_user_call( struct codegen* codegen, struct operand*,
   struct call* );
static void write_call_args( struct codegen* codegen, struct operand* operand,
   struct call* call );
static void visit_nested_userfunc_call( struct codegen* codegen,
   struct operand* operand, struct call* call );
static void visit_internal_call( struct codegen* codegen, struct operand*,
   struct call* );
static void write_executewait( struct codegen* codegen, struct call* call,
   bool named_impl );
static void visit_binary( struct codegen* codegen, struct operand*,
   struct binary* );
static void visit_logical( struct codegen* codegen,
   struct operand* result, struct logical* );
static void write_logicalor( struct codegen* codegen,
   struct operand* operand, struct logical* logical );
static void write_logicaland( struct codegen* codegen,
   struct operand* operand, struct logical* logical );
static void visit_assign( struct codegen* codegen, struct operand*,
   struct assign* );
static void visit_conditional( struct codegen* codegen,
   struct operand* operand, struct conditional* cond );
static void write_conditional( struct codegen* codegen,
   struct operand* operand, struct conditional* cond );
static void do_var_name( struct codegen* codegen, struct operand*,
   struct var* );
static void set_var( struct codegen* codegen, struct operand*,
   struct var* );
static void visit_object( struct codegen* codegen, struct operand*,
   struct node* );
static void visit_subscript( struct codegen* codegen, struct operand*,
   struct subscript* );
static void visit_access( struct codegen* codegen, struct operand*,
   struct access* );
static void push_indexed( struct codegen* codegen, int, int );
static void push_element( struct codegen* codegen, int, int );
static void write_strcpy( struct codegen* codegen, struct operand* operand,
   struct strcpy_call* call );

static const int g_aspec_code[] = {
   PCD_LSPEC1,
   PCD_LSPEC2,
   PCD_LSPEC3,
   PCD_LSPEC4,
   PCD_LSPEC5
};

void c_visit_expr( struct codegen* codegen, struct expr* expr ) {
   struct operand operand;
   init_operand( &operand );
   visit_operand( codegen, &operand, expr->root );
   if ( operand.pushed ) {
      c_add_opc( codegen, PCD_DROP );
   }
}

void c_push_expr( struct codegen* codegen, struct expr* expr, bool temp ) {
   struct operand operand;
   init_operand( &operand );
   operand.push = true;
   operand.push_temp = temp;
   visit_operand( codegen, &operand, expr->root );
}

void init_operand( struct operand* operand ) {
   operand->type = NULL;
   operand->dim = NULL;
   operand->action = ACTION_PUSH_VALUE;
   operand->method = METHOD_NONE;
   operand->trigger = TRIGGER_INDEX;
   operand->storage = 0;
   operand->index = 0;
   operand->base = 0;
   operand->push = false;
   operand->push_temp = false;
   operand->pushed = false;
   operand->pushed_element = false;
}

void visit_operand( struct codegen* codegen, struct operand* operand,
   struct node* node ) {
   // Select object referenced by the name.
   if ( node->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) node;
      node = usage->object;
   }
   // Visit object.
   if ( node->type == NODE_LITERAL ) {
      struct literal* literal = ( struct literal* ) node;
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, literal->value );
      operand->pushed = true;
   }
   else if ( node->type == NODE_INDEXED_STRING_USAGE ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) node;
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, usage->string->index );
      // Strings in a library need to be tagged.
      if ( codegen->task->library_main->importable ) {
         c_add_opc( codegen, PCD_TAGSTRING );
      }
      operand->pushed = true;
   }
   else if ( node->type == NODE_BOOLEAN ) {
      struct boolean* boolean = ( struct boolean* ) node;
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, boolean->value );
      operand->pushed = true;
   }
   else if ( node->type == NODE_CONSTANT ) {
      visit_constant( codegen, operand, ( struct constant* ) node );
   }
   else if ( node->type == NODE_ENUMERATOR ) {
      visit_enumerator( codegen, operand,
         ( struct enumerator* ) node );
   }
   else if ( node->type == NODE_VAR ) {
      do_var_name( codegen, operand, ( struct var* ) node );
   }
   else if ( node->type == NODE_PARAM ) {
      struct param* param = ( struct param* ) node;
      if ( operand->action == ACTION_PUSH_VALUE ) {
         push_indexed( codegen, STORAGE_LOCAL, param->index );
         operand->pushed = true;
      }
      else {
         operand->storage = STORAGE_LOCAL;
         operand->index = param->index;
      }
   }
   else if ( node->type == NODE_UNARY ) {
      visit_unary( codegen, operand, ( struct unary* ) node );
   }
   else if ( node->type == NODE_SUBSCRIPT || node->type == NODE_ACCESS ) {
      visit_object( codegen, operand, node );
   }
   else if ( node->type == NODE_CALL ) {
      visit_call( codegen, operand, ( struct call* ) node );
   }
   else if ( node->type == NODE_BINARY ) {
      visit_binary( codegen, operand, ( struct binary* ) node );
   }
   else if ( node->type == NODE_LOGICAL ) {
      visit_logical( codegen, operand,
         ( struct logical* ) node );
   }
   else if ( node->type == NODE_ASSIGN ) {
      visit_assign( codegen, operand, ( struct assign* ) node );
   }
   else if ( node->type == NODE_CONDITIONAL ) {
      visit_conditional( codegen, operand, ( struct conditional* ) node );
   }
   else if ( node->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) node;
      visit_operand( codegen, operand, paren->inside );
   }
   else if ( node->type == NODE_FUNC ) {
      struct func* func = ( struct func* ) node;
      if ( func->type == FUNC_ASPEC ) {
         struct func_aspec* impl = func->impl;
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, impl->id );
      }
   }
   else if ( node->type == NODE_STRCPY ) {
      write_strcpy( codegen, operand, ( struct strcpy_call* ) node );
   }
}

void visit_constant( struct codegen* codegen, struct operand* operand,
   struct constant* constant ) {
   c_add_opc( codegen, PCD_PUSHNUMBER );
   c_add_arg( codegen, constant->value );
   if ( codegen->task->library_main->importable && constant->value_node &&
      constant->value_node->has_str ) {
      c_add_opc( codegen, PCD_TAGSTRING );
   }
   operand->pushed = true;
}

void visit_enumerator( struct codegen* codegen, struct operand* operand,
   struct enumerator* enumerator ) {
   c_add_opc( codegen, PCD_PUSHNUMBER );
   c_add_arg( codegen, enumerator->value );
   operand->pushed = true;
}

void visit_unary( struct codegen* codegen, struct operand* operand,
   struct unary* unary ) {
   struct operand object;
   init_operand( &object );
   object.push = true;
   visit_operand( codegen, &object, unary->operand );
   int code = PCD_NONE;
   switch ( unary->op ) {
   case UOP_MINUS:
      c_add_opc( codegen, PCD_UNARYMINUS );
      break;
   case UOP_LOG_NOT:
      c_add_opc( codegen, PCD_NEGATELOGICAL );
      break;
   case UOP_BIT_NOT:
      c_add_opc( codegen, PCD_NEGATEBINARY );
      break;
   // Unary plus is ignored.
   case UOP_PLUS:
   default:
      break;
   }
   operand->pushed = true;
}

void visit_inc( struct codegen* codegen, struct operand* result,
   struct inc* inc ) {
   struct operand operand;
   init_operand( &operand );
   operand.action = ACTION_PUSH_VAR;
   visit_operand( codegen, &operand, inc->operand );
   if ( operand.method == METHOD_ELEMENT ) {
      inc_array( codegen, result, inc, &operand );
   }
   else {
      inc_var( codegen, result, inc, &operand );
   }
}

void inc_array( struct codegen* codegen, struct operand* result,
   struct inc* inc, struct operand* operand ) {
   if ( result->push ) {
      c_add_opc( codegen, PCD_DUP );
      if ( inc->post ) {
         push_element( codegen, operand->storage, operand->index );
         c_add_opc( codegen, PCD_SWAP );
         result->pushed = true;
      }
   }
   inc_element( codegen, operand->storage, operand->index, ( ! inc->dec ) );
   if ( ! inc->post && result->push ) {
      push_element( codegen, operand->storage, operand->index );
      result->pushed = true;
   }
}

void inc_element( struct codegen* codegen, int storage, int index,
   bool do_inc ) {
   int code = PCD_INCMAPARRAY;
   if ( do_inc ) {
      switch ( storage ) {
      case STORAGE_WORLD:
         code = PCD_INCWORLDARRAY;
         break;
      case STORAGE_GLOBAL:
         code = PCD_INCGLOBALARRAY;
         break;
      default:
         break;
      }
   }
   else {
      switch ( storage ) {
      case STORAGE_WORLD:
         code = PCD_DECWORLDARRAY;
         break;
      case STORAGE_GLOBAL:
         code = PCD_DECGLOBALARRAY;
         break;
      default:
         code = PCD_DECMAPARRAY;
         break;
      }
   }
   c_add_opc( codegen, code );
   c_add_arg( codegen, index );
}

void inc_var( struct codegen* codegen, struct operand* result,
   struct inc* inc, struct operand* operand ) {
   if ( inc->post && result->push ) {
      push_indexed( codegen, operand->storage, operand->index );
      result->pushed = true;
   }
   inc_indexed( codegen, operand->storage, operand->index, ( ! inc->dec ) );
   if ( ! inc->post && result->push ) {
      push_indexed( codegen, operand->storage, operand->index );
      result->pushed = true;
   }
}

void inc_indexed( struct codegen* codegen, int storage, int index,
   bool do_inc ) {
   int code = PCD_INCSCRIPTVAR;
   if ( do_inc ) {
      switch ( storage ) {
      case STORAGE_MAP:
         code = PCD_INCMAPVAR;
         break;
      case STORAGE_WORLD:
         code = PCD_INCWORLDVAR;
         break;
      case STORAGE_GLOBAL:
         code = PCD_INCGLOBALVAR;
         break;
      default:
         break;
      }
   }
   else {
      switch ( storage ) {
      case STORAGE_MAP:
         code = PCD_DECMAPVAR;
         break;
      case STORAGE_WORLD:
         code = PCD_DECWORLDVAR;
         break;
      case STORAGE_GLOBAL:
         code = PCD_DECGLOBALVAR;
         break;
      default:
         code = PCD_DECSCRIPTVAR;
         break;
      }
   }
   c_add_opc( codegen, code );
   c_add_arg( codegen, index );
}

void visit_call( struct codegen* codegen, struct operand* operand,
   struct call* call ) {
   switch ( call->func->type ) {
   case FUNC_ASPEC:
      visit_aspec_call( codegen, operand, call );
      break;
   case FUNC_EXT:
      visit_ext_call( codegen, operand, call );
      break;
   case FUNC_DED:
      visit_ded_call( codegen, operand, call );
      break;
   case FUNC_FORMAT:
      visit_format_call( codegen, operand, call );
      break;
   case FUNC_USER:
      visit_user_call( codegen, operand, call );
      break;
   case FUNC_INTERNAL:
      visit_internal_call( codegen, operand, call );
      break;
   }
}

void visit_aspec_call( struct codegen* codegen, struct operand* operand,
   struct call* call ) {
   int count = push_nonzero_args( codegen, call->func->params,
      &call->args, 0 );
   struct func_aspec* aspec = call->func->impl;
   if ( operand->push ) {
      while ( count < 5 ) {
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, 0 );
         ++count;
      }
      c_add_opc( codegen, PCD_LSPEC5RESULT );
      c_add_arg( codegen, aspec->id );
      operand->pushed = true;
   }
   else if ( count ) {
      c_add_opc( codegen, g_aspec_code[ count - 1 ] );
      c_add_arg( codegen, aspec->id );
   }
   else {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
      c_add_opc( codegen, PCD_LSPEC1 );
      c_add_arg( codegen, aspec->id );
   }
}

void visit_ext_call( struct codegen* codegen, struct operand* operand,
   struct call* call ) {
   int count = push_nonzero_args( codegen, call->func->params, &call->args,
      call->func->min_param );
   struct func_ext* impl = call->func->impl;
   c_add_opc( codegen, PCD_CALLFUNC );
   c_add_arg( codegen, count );
   c_add_arg( codegen, impl->id );
   operand->pushed = true;
}

// Pushes the specified function arguments onto the stack. A minimum amount of
// arguments will always be pushed. After the minimum amount, only those
// arguments up to, and including, the last non-zero argument will be pushed.
// There is no need to pass the trailing zero arguments because the engine will
// implicitly pass them.
int push_nonzero_args( struct codegen* codegen, struct param* params,
   struct list* args, int min ) {
   int count = 0;
   list_iter_t k;
   list_iter_init( &k, args );
   struct param* param = params;
   // Add to the count the minimum arguments required.
   while ( count < min ) {
      param = param->next;
      list_next( &k );
      ++count;
   }
   // Add to the count the range of arguments that contains all remaining
   // non-zero arguments.
   int i = count;
   while ( param ) {
      struct expr* arg = param->default_value;
      if ( ! list_end( &k ) ) {
         arg = list_data( &k );
         list_next( &k );
      }
      ++i;
      bool zero = ( arg->folded && ! arg->has_str && arg->value == 0 );
      if ( ! zero ) {
         count = i;
      }
      param = param->next;
   }
   // Write arguments.
   i = 0;
   param = params;
   list_iter_init( &k, args );
   while ( i < count ) {
      struct expr* arg = param->default_value;
      if ( ! list_end( &k ) ) {
         arg = list_data( &k );
         list_next( &k );
      }
      c_push_expr( codegen, arg, false );
      if ( param->used ) {
         c_add_opc( codegen, PCD_DUP );
         c_add_opc( codegen, PCD_ASSIGNSCRIPTVAR );
         c_add_arg( codegen, param->index );
      }
      param = param->next;
      ++i;
   }
   return count;
}

void visit_ded_call( struct codegen* codegen, struct operand* operand,
   struct call* call ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( param ) {
      struct expr* arg = param->default_value;
      if ( ! list_end( &i ) ) {
         arg = list_data( &i );
         list_next( &i );
      }
      c_push_expr( codegen, arg, false );
      if ( param->used ) {
         c_add_opc( codegen, PCD_DUP );
         c_add_opc( codegen, PCD_ASSIGNSCRIPTVAR );
         c_add_arg( codegen, param->index );
      }
      param = param->next;
   }
   struct func_ded* ded = call->func->impl;
   c_add_opc( codegen, ded->opcode );
   if ( call->func->return_type ) {
      operand->pushed = true;
   }
}

void visit_user_call( struct codegen* codegen, struct operand* operand,
   struct call* call ) {
   struct func_user* impl = call->func->impl;
   if ( impl->nested ) {
      visit_nested_userfunc_call( codegen, operand, call );
   }
   else {
      write_call_args( codegen, operand, call );
      if ( call->func->return_type ) {
         c_add_opc( codegen, PCD_CALL );
         c_add_arg( codegen, impl->index );
         operand->pushed = true;
      }
      else {
         c_add_opc( codegen, PCD_CALLDISCARD );
         c_add_arg( codegen, impl->index );
      }
   }
}

void write_call_args( struct codegen* codegen, struct operand* operand,
   struct call* call ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct param* param = call->func->params;
   while ( ! list_end( &i ) ) {
      c_push_expr( codegen, list_data( &i ), false );
      list_next( &i );
      param = param->next;
   }
   // Default arguments.
   while ( param ) {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
      param = param->next;
   }
   // Number of real arguments passed, for a function with default
   // parameters.
   if ( call->func->min_param != call->func->max_param ) {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, list_size( &call->args ) );
   }
}

void visit_nested_userfunc_call( struct codegen* codegen,
   struct operand* operand, struct call* call ) {
   // Push ID of entry to identify return address. 
   c_add_opc( codegen, PCD_PUSHNUMBER );
   c_add_arg( codegen, call->nested_call->id );
   write_call_args( codegen, operand, call );
   call->nested_call->enter_pos = c_tell( codegen );
   c_add_opc( codegen, PCD_GOTO );
   c_add_arg( codegen, 0 );
   call->nested_call->leave_pos = c_tell( codegen );
   if ( call->func->return_type ) {
      operand->pushed = true;
   }
}

void visit_format_call( struct codegen* codegen, struct operand* operand,
   struct call* call ) {
   c_add_opc( codegen, PCD_BEGINPRINT );
   list_iter_t i;
   list_iter_init( &i, &call->args );
   struct node* node = list_data( &i );
   // Format-block:
   if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
      visit_formatblock_arg( codegen, ( struct format_block_usage* ) node );
   }
   // Format-list:
   else {
      c_visit_format_item( codegen, list_data( &i ) );
   }
   list_next( &i );
   // Other arguments.
   if ( call->func->max_param > 1 ) {
      c_add_opc( codegen, PCD_MOREHUDMESSAGE );
      int param = 1;
      while ( ! list_end( &i ) ) {
         if ( param == call->func->min_param ) {
            c_add_opc( codegen, PCD_OPTHUDMESSAGE );
         }
         c_push_expr( codegen, list_data( &i ), false );
         ++param;
         list_next( &i );
      }
   }
   struct func_format* format = call->func->impl;
   c_add_opc( codegen, format->opcode );
   if ( call->func->return_type ) {
      operand->pushed = true;
   }
}

void visit_formatblock_arg( struct codegen* codegen,
   struct format_block_usage* usage ) {
   // When a format block is used more than once in the same expression,
   // instead of duplicating the code, use a goto instruction to enter the
   // format block. At each usage except the last, before the format block
   // is used, a unique number is pushed. This number is used to determine
   // the return location. 
   if ( usage->next ) {
      usage->obj_pos = c_tell( codegen );
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, 0 );
      if ( ! codegen->block_visit->format_block_usage ) {
         codegen->block_visit->format_block_usage = usage;
      }
   }
   else {
      write_formatblock_arg( codegen, usage );
   }
}

void write_formatblock_arg( struct codegen* codegen,
   struct format_block_usage* usage ) {
   // Return address of the last usage. Only needed when the format-block is
   // used more than once.
   if ( codegen->block_visit->format_block_usage ) {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
   }
   int block_pos = c_tell( codegen );
   c_write_block( codegen, usage->block, true );
   usage = codegen->block_visit->format_block_usage;
   if ( ! usage ) {
      return;
   }
   // Update block jumps.
   int count = 1;
   while ( usage->next ) {
      c_seek( codegen, usage->obj_pos );
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, count );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, block_pos );
      usage->obj_pos = c_tell( codegen );
      usage = usage->next;
      ++count;
   }
   // Publish return jumps. A sorted-case-goto can be used here, but a
   // case-goto will suffice for now.
   c_seek_end( codegen );
   usage = codegen->block_visit->format_block_usage;
   count = 1;
   while ( usage->next ) {
      c_add_opc( codegen, PCD_CASEGOTO );
      c_add_arg( codegen, count );
      c_add_arg( codegen, usage->obj_pos );
      usage = usage->next;
      ++count;
   }
   c_add_opc( codegen, PCD_DROP );
   codegen->block_visit->format_block_usage = NULL;
}

void c_visit_format_item( struct codegen* codegen, struct format_item* item ) {
   while ( item ) {
      if ( item->cast == FCAST_ARRAY ) {
         visit_array_format_item( codegen, item );
         
      }
      else {
         static const int casts[] = {
            PCD_PRINTBINARY,
            PCD_PRINTCHARACTER,
            PCD_PRINTNUMBER,
            PCD_PRINTFIXED,
            PCD_PRINTBIND,
            PCD_PRINTLOCALIZED,
            PCD_PRINTNAME,
            PCD_PRINTSTRING,
            PCD_PRINTHEX };
         STATIC_ASSERT( FCAST_TOTAL == 10 );
         c_push_expr( codegen, item->value, false );
         c_add_opc( codegen, casts[ item->cast - 1 ] );
      }
      item = item->next;
   }
}

void visit_array_format_item( struct codegen* codegen,
   struct format_item* item ) {
   struct operand object;
   init_operand( &object );
   object.action = ACTION_PUSH_VAR;
   visit_operand( codegen, &object, item->value->root );
   c_add_opc( codegen, PCD_PUSHNUMBER );
   c_add_arg( codegen, object.index );
   if ( item->extra ) {
      struct format_item_array* extra = item->extra;
      c_push_expr( codegen, extra->offset, false );
      if ( extra->length ) {
         c_push_expr( codegen, extra->length, false );
      }
      else {
         enum { MAX_LENGTH = INT_MAX };
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, MAX_LENGTH );
      }
   }
   int code = PCD_NONE;
   if ( item->extra ) {
      switch ( object.storage ) {
      case STORAGE_MAP:
         code = PCD_PRINTMAPCHRANGE;
         break;
      case STORAGE_WORLD:
         code = PCD_PRINTWORLDCHRANGE;
         break;
      case STORAGE_GLOBAL:
         code = PCD_PRINTGLOBALCHRANGE;
         break;
      default:
         break;
      }
   }
   else {
      switch ( object.storage ) {
      case STORAGE_MAP:
         code = PCD_PRINTMAPCHARARRAY;
         break;
      case STORAGE_WORLD:
         code = PCD_PRINTWORLDCHARARRAY;
         break;
      case STORAGE_GLOBAL:
         code = PCD_PRINTGLOBALCHARARRAY;
         break;
      default:
         break;
      }
   }
   c_add_opc( codegen, code );
}

void visit_internal_call( struct codegen* codegen, struct operand* operand,
   struct call* call ) {
   struct func_intern* impl = call->func->impl; 
   if ( impl->id == INTERN_FUNC_ACS_EXECWAIT ||
      impl->id == INTERN_FUNC_ACS_NAMEDEXECUTEWAIT ) {
      write_executewait( codegen, call,
         ( impl->id == INTERN_FUNC_ACS_NAMEDEXECUTEWAIT ) );
   }
   else if ( impl->id == INTERN_FUNC_STR_LENGTH ) {
      visit_operand( codegen, operand, call->operand );
      c_add_opc( codegen, PCD_STRLEN );
   }
   else if ( impl->id == INTERN_FUNC_STR_AT ) {
      visit_operand( codegen, operand, call->operand );
      c_push_expr( codegen, list_head( &call->args ), true );
      c_add_opc( codegen, PCD_CALLFUNC );
      c_add_arg( codegen, 2 );
      c_add_arg( codegen, 15 );
   }
}

void write_executewait( struct codegen* codegen, struct call* call,
   bool named_impl ) {
   list_iter_t i;
   list_iter_init( &i, &call->args );
   c_push_expr( codegen, list_data( &i ), false );
   c_add_opc( codegen, PCD_DUP );
   list_next( &i );
   if ( ! list_end( &i ) ) {
      // Second argument to Acs_Execute is 0--the current map. Ignore what the
      // user specified.
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
      list_next( &i );
      while ( ! list_end( &i ) ) {
         c_push_expr( codegen, list_data( &i ), true );
         list_next( &i );
      }
   }
   if ( named_impl ) {
      c_add_opc( codegen, PCD_CALLFUNC );
      c_add_arg( codegen, list_size( &call->args ) );
      c_add_arg( codegen, 39 );
      c_add_opc( codegen, PCD_DROP );
      c_add_opc( codegen, PCD_SCRIPTWAITNAMED );
   }
   else {
      c_add_opc( codegen, g_aspec_code[ list_size( &call->args ) - 1 ] );
      c_add_arg( codegen, 80 );
      c_add_opc( codegen, PCD_SCRIPTWAIT );
   }
}

void visit_binary( struct codegen* codegen, struct operand* operand,
   struct binary* binary ) {
   struct operand lside;
   init_operand( &lside );
   lside.push = true;
   visit_operand( codegen, &lside, binary->lside );
   struct operand rside;
   init_operand( &rside );
   rside.push = true;
   visit_operand( codegen, &rside, binary->rside );
   int code = PCD_NONE;
   switch ( binary->op ) {
   case BOP_BIT_OR: code = PCD_ORBITWISE; break;
   case BOP_BIT_XOR: code = PCD_EORBITWISE; break;
   case BOP_BIT_AND: code = PCD_ANDBITWISE; break;
   case BOP_EQ: code = PCD_EQ; break;
   case BOP_NEQ: code = PCD_NE; break;
   case BOP_LT: code = PCD_LT; break;
   case BOP_LTE: code = PCD_LE; break;
   case BOP_GT: code = PCD_GT; break;
   case BOP_GTE: code = PCD_GE; break;
   case BOP_SHIFT_L: code = PCD_LSHIFT; break;
   case BOP_SHIFT_R: code = PCD_RSHIFT; break;
   case BOP_ADD: code = PCD_ADD; break;
   case BOP_SUB: code = PCD_SUBTRACT; break;
   case BOP_MUL: code = PCD_MULIPLY; break;
   case BOP_DIV: code = PCD_DIVIDE; break;
   case BOP_MOD: code = PCD_MODULUS; break;
   default: break;
   }
   c_add_opc( codegen, code );
   operand->pushed = true;
}

void visit_logical( struct codegen* codegen,
   struct operand* result, struct logical* logical ) {
   if ( logical->folded ) {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, logical->value );
      result->pushed = true;
   }
   else {
      switch ( logical->op ) {
      case LOP_OR:
         write_logicalor( codegen, result, logical );
         break;
      case LOP_AND:
         write_logicaland( codegen, result, logical );
         break;
      default:
         UNREACHABLE();
      }
   }
}

// Logical-or and logical-and both perform shortcircuit evaluation.
void write_logicalor( struct codegen* codegen, struct operand* operand,
   struct logical* logical ) {
   struct operand lside;
   init_operand( &lside );
   lside.push = true;
   lside.push_temp = true;
   visit_operand( codegen, &lside, logical->lside );
   int test = c_tell( codegen );
   c_add_opc( codegen, PCD_IFNOTGOTO );
   c_add_arg( codegen, 0 );
   c_add_opc( codegen, PCD_PUSHNUMBER );
   c_add_arg( codegen, 1 );
   int jump = c_tell( codegen );
   c_add_opc( codegen, PCD_GOTO );
   c_add_arg( codegen, 0 );
   struct operand rside;
   init_operand( &rside );
   rside.push = true;
   rside.push_temp = true;
   int next = c_tell( codegen );
   visit_operand( codegen, &rside, logical->rside );
   // Optimization: When doing a calculation temporarily, there's no need to
   // convert the second operand to a 0 or 1. Just use the operand directly.
   if ( ! operand->push_temp ) {
      c_add_opc( codegen, PCD_NEGATELOGICAL );
      c_add_opc( codegen, PCD_NEGATELOGICAL );
   }
   int done = c_tell( codegen );
   c_seek( codegen, test );
   c_add_opc( codegen, PCD_IFNOTGOTO );
   c_add_arg( codegen, next );
   c_seek( codegen, jump );
   c_add_opc( codegen, PCD_GOTO );
   c_add_arg( codegen, done );
   c_seek_end( codegen );
   operand->pushed = true;
}

void write_logicaland( struct codegen* codegen, struct operand* operand,
   struct logical* logical ) {
   struct operand lside;
   init_operand( &lside );
   lside.push = true;
   lside.push_temp = true;
   visit_operand( codegen, &lside, logical->lside );
   int test = c_tell( codegen );
   c_add_opc( codegen, PCD_IFGOTO );
   c_add_arg( codegen, 0 );
   c_add_opc( codegen, PCD_PUSHNUMBER );
   c_add_arg( codegen, 0 );
   int jump = c_tell( codegen );
   c_add_opc( codegen, PCD_GOTO );
   c_add_arg( codegen, 0 );
   struct operand rside;
   init_operand( &rside );
   rside.push = true;
   rside.push_temp = true;
   int next = c_tell( codegen );
   visit_operand( codegen, &rside, logical->rside );
   if ( ! operand->push_temp ) {
      c_add_opc( codegen, PCD_NEGATELOGICAL );
      c_add_opc( codegen, PCD_NEGATELOGICAL );
   }
   int done = c_tell( codegen );
   c_seek( codegen, test );
   c_add_opc( codegen, PCD_IFGOTO );
   c_add_arg( codegen, next );
   c_seek( codegen, jump );
   c_add_opc( codegen, PCD_GOTO );
   c_add_arg( codegen, done );
   c_seek_end( codegen );
   operand->pushed = true;
}

void set_var( struct codegen* codegen, struct operand* operand, struct var* var ) {
   operand->type = var->structure;
   operand->dim = var->dim;
   operand->storage = var->storage;
   if ( ! var->structure->primitive || var->dim ) {
      operand->method = METHOD_ELEMENT;
      operand->index = var->index;
   }
   else {
      operand->method = METHOD_INDEXED;
      operand->index = var->index;
   }
}

void do_var_name( struct codegen* codegen, struct operand* operand,
   struct var* var ) {
   set_var( codegen, operand, var );
   // For element-based variables, an index marking the start of the variable
   // data needs to be on the stack.
   if ( operand->method == METHOD_ELEMENT ) {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, operand->base );
   }
   else {
      if ( operand->action == ACTION_PUSH_VALUE ) {
         push_indexed( codegen, operand->storage, operand->index );
         operand->pushed = true;
      }
   }
}

void visit_object( struct codegen* codegen, struct operand* operand,
   struct node* node ) {
   if ( node->type == NODE_ACCESS ) {
      visit_access( codegen, operand, ( struct access* ) node );
   }
   else if ( node->type == NODE_SUBSCRIPT ) {
      visit_subscript( codegen, operand, ( struct subscript* ) node );
   }
   if ( operand->method == METHOD_ELEMENT ) {
      if ( operand->pushed_element ) {
         if ( operand->base ) {
            c_add_opc( codegen, PCD_PUSHNUMBER );
            c_add_arg( codegen, operand->base );
            c_add_opc( codegen, PCD_ADD );
         }
      }
      else {
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, operand->base );
      }
   }
   if ( operand->action == ACTION_PUSH_VALUE &&
      operand->method != METHOD_NONE ) {
      if ( operand->method == METHOD_ELEMENT ) {
         push_element( codegen, operand->storage, operand->index );
      }
      else {
         push_indexed( codegen, operand->storage, operand->index );
      }
      operand->pushed = true;
   }
}

void visit_subscript( struct codegen* codegen, struct operand* operand,
   struct subscript* subscript ) {
   struct node* lside = subscript->lside;
   while ( lside->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) lside;
      lside = paren->inside;
   }
   if ( lside->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) lside;
      lside = usage->object;
   }
   // Left side:
   if ( lside->type == NODE_VAR ) {
      set_var( codegen, operand, ( struct var* ) lside );
   }
   else if ( lside->type == NODE_ACCESS ) {
      visit_access( codegen, operand, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      visit_subscript( codegen, operand, ( struct subscript* ) lside );
   }
   // Dimension:
   struct operand index;
   init_operand( &index );
   index.push = true;
   index.push_temp = true;
   visit_operand( codegen, &index, subscript->index->root );
   if ( operand->dim->next ) {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, operand->dim->element_size );
      c_add_opc( codegen, PCD_MULIPLY );
   }
   else if ( ! operand->type->primitive ) {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, operand->type->size );
      c_add_opc( codegen, PCD_MULIPLY );
   }
   if ( operand->pushed_element ) {
      c_add_opc( codegen, PCD_ADD );
   }
   else {
      operand->pushed_element = true;
   }
   operand->dim = operand->dim->next;
}

void visit_access( struct codegen* codegen, struct operand* operand,
   struct access* access ) {
   struct node* lside = access->lside;
   struct node* rside = access->rside;
   while ( lside->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) lside;
      lside = paren->inside;
   }
   if ( lside->type == NODE_NAME_USAGE ) {
      struct name_usage* usage = ( struct name_usage* ) lside;
      lside = usage->object;
   }
   // See if the left side is a namespace.
   struct node* object = lside;
   if ( object->type == NODE_ACCESS ) {
      struct access* nested = ( struct access* ) object;
      object = nested->rside;
   }
   // Left side:
   if ( lside->type == NODE_VAR ) {
      set_var( codegen, operand, ( struct var* ) lside );
   }
   else if ( lside->type == NODE_CONSTANT ) {
      visit_constant( codegen, operand, ( struct constant* ) lside );
   }
   else if ( lside->type == NODE_ACCESS ) {
      visit_access( codegen, operand, ( struct access* ) lside );
   }
   else if ( lside->type == NODE_SUBSCRIPT ) {
      visit_subscript( codegen, operand, ( struct subscript* ) lside );
   }
   else {
      visit_operand( codegen, operand, lside );
   }
   // Right side:
   if ( rside && rside->type == NODE_STRUCTURE_MEMBER ) {
      struct structure_member* member = ( struct structure_member* ) rside;
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, member->offset );
      if ( operand->pushed_element ) {
         c_add_opc( codegen, PCD_ADD );
      }
      else {
         operand->pushed_element = true;
      }
      operand->type = member->structure;
      operand->dim = member->dim;
   }
}

void visit_assign( struct codegen* codegen, struct operand* operand,
   struct assign* assign ) {
   struct operand lside;
   init_operand( &lside );
   lside.action = ACTION_PUSH_VAR;
   visit_operand( codegen, &lside, assign->lside );
   if ( lside.method == METHOD_ELEMENT ) {
      if ( operand->push ) {
         c_add_opc( codegen, PCD_DUP );
      }
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      visit_operand( codegen, &rside, assign->rside );
      c_update_element( codegen, lside.storage, lside.index, assign->op );
      if ( operand->push ) {
         push_element( codegen, lside.storage, lside.index );
         operand->pushed = true;
      }
   }
   else {
      struct operand rside;
      init_operand( &rside );
      rside.push = true;
      visit_operand( codegen, &rside, assign->rside );
      if ( assign->op == AOP_NONE && operand->push ) {
         c_add_opc( codegen, PCD_DUP );
         operand->pushed = true;
      }
      c_update_indexed( codegen, lside.storage, lside.index, assign->op );
      if ( assign->op != AOP_NONE && operand->push ) {
         push_indexed( codegen, lside.storage, lside.index );
         operand->pushed = true;
      }
   }
}

void visit_conditional( struct codegen* codegen, struct operand* operand,
   struct conditional* cond ) {
   if ( cond->folded ) {
      struct operand value;
      init_operand( &value );
      value.push = true;
      visit_operand( codegen, &value, cond->left_value ?
         ( cond->middle ? cond->middle : cond->left ) : cond->right );
      operand->pushed = true;
   }
   else {
      write_conditional( codegen, operand, cond );
   }
}

void write_conditional( struct codegen* codegen, struct operand* operand,
   struct conditional* cond ) {
   struct operand value;
   init_operand( &value );
   value.push = true;
   visit_operand( codegen, &value, cond->left );
   int left_done = 0;
   int middle_done = 0;
   if ( cond->middle ) {
      left_done = c_tell( codegen );
      c_add_opc( codegen, PCD_IFNOTGOTO );
      c_add_arg( codegen, 0 );
      init_operand( &value );
      value.push = true;
      visit_operand( codegen, &value, cond->middle );
      middle_done = c_tell( codegen );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, 0 );
   }
   else {
      c_add_opc( codegen, PCD_DUP );
      left_done = c_tell( codegen );
      c_add_opc( codegen, PCD_IFGOTO );
      c_add_arg( codegen, 0 );
      c_add_opc( codegen, PCD_DROP );
   }
   init_operand( &value );
   value.push = true;
   visit_operand( codegen, &value, cond->right );
   int done = c_tell( codegen );
   if ( cond->middle ) {
      c_seek( codegen, middle_done );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, done );
      int right = c_tell( codegen );
      c_seek( codegen, left_done );
      c_add_opc( codegen, PCD_IFNOTGOTO );
      c_add_arg( codegen, right );
      c_seek_end( codegen );
   }
   else {
      c_seek( codegen, left_done );
      c_add_opc( codegen, PCD_IFGOTO );
      c_add_arg( codegen, done );
      c_seek_end( codegen );
   }
   operand->pushed = value.pushed;
}

void push_indexed( struct codegen* codegen, int storage, int index ) {
   int code = PCD_PUSHSCRIPTVAR;
   switch ( storage ) {
   case STORAGE_MAP:
      code = PCD_PUSHMAPVAR;
      break;
   case STORAGE_WORLD:
      code = PCD_PUSHWORLDVAR;
      break;
   case STORAGE_GLOBAL:
      code = PCD_PUSHGLOBALVAR;
      break;
   default:
      break;
   }
   c_add_opc( codegen, code );
   c_add_arg( codegen, index );
}

void push_element( struct codegen* codegen, int storage, int index ) {
   int code = PCD_PUSHMAPARRAY;
   switch ( storage ) {
   case STORAGE_WORLD:
      code = PCD_PUSHWORLDARRAY;
      break;
   case STORAGE_GLOBAL:
      code = PCD_PUSHGLOBALARRAY;
      break;
   default:
      break;
   }
   c_add_opc( codegen, code );
   c_add_arg( codegen, index );
}

void c_update_indexed( struct codegen* codegen, int storage, int index,
   int op ) {
   static const int code[] = {
      PCD_ASSIGNSCRIPTVAR, PCD_ASSIGNMAPVAR, PCD_ASSIGNWORLDVAR,
         PCD_ASSIGNGLOBALVAR,
      PCD_ADDSCRIPTVAR, PCD_ADDMAPVAR, PCD_ADDWORLDVAR, PCD_ADDGLOBALVAR,
      PCD_SUBSCRIPTVAR, PCD_SUBMAPVAR, PCD_SUBWORLDVAR, PCD_SUBGLOBALVAR,
      PCD_MULSCRIPTVAR, PCD_MULMAPVAR, PCD_MULWORLDVAR, PCD_MULGLOBALVAR,
      PCD_DIVSCRIPTVAR, PCD_DIVMAPVAR, PCD_DIVWORLDVAR, PCD_DIVGLOBALVAR,
      PCD_MODSCRIPTVAR, PCD_MODMAPVAR, PCD_MODWORLDVAR, PCD_MODGLOBALVAR,
      PCD_LSSCRIPTVAR, PCD_LSMAPVAR, PCD_LSWORLDVAR, PCD_LSGLOBALVAR,
      PCD_RSSCRIPTVAR, PCD_RSMAPVAR, PCD_RSWORLDVAR, PCD_RSGLOBALVAR,
      PCD_ANDSCRIPTVAR, PCD_ANDMAPVAR, PCD_ANDWORLDVAR, PCD_ANDGLOBALVAR,
      PCD_EORSCRIPTVAR, PCD_EORMAPVAR, PCD_EORWORLDVAR, PCD_EORGLOBALVAR,
      PCD_ORSCRIPTVAR, PCD_ORMAPVAR, PCD_ORWORLDVAR, PCD_ORGLOBALVAR };
   int pos = 0;
   switch ( storage ) {
   case STORAGE_MAP: pos = 1; break;
   case STORAGE_WORLD: pos = 2; break;
   case STORAGE_GLOBAL: pos = 3; break;
   default: break;
   }
   c_add_opc( codegen, code[ op * 4 + pos ] );
   c_add_arg( codegen, index );
}

void c_update_element( struct codegen* codegen, int storage, int index,
   int op ) {
   static const int code[] = {
      PCD_ASSIGNMAPARRAY, PCD_ASSIGNWORLDARRAY, PCD_ASSIGNGLOBALARRAY,
      PCD_ADDMAPARRAY, PCD_ADDWORLDARRAY, PCD_ADDGLOBALARRAY,
      PCD_SUBMAPARRAY, PCD_SUBWORLDARRAY, PCD_SUBGLOBALARRAY,
      PCD_MULMAPARRAY, PCD_MULWORLDARRAY, PCD_MULGLOBALARRAY,
      PCD_DIVMAPARRAY, PCD_DIVWORLDARRAY, PCD_DIVGLOBALARRAY,
      PCD_MODMAPARRAY, PCD_MODWORLDARRAY, PCD_MODGLOBALARRAY,
      PCD_LSMAPARRAY, PCD_LSWORLDARRAY, PCD_LSGLOBALARRAY,
      PCD_RSMAPARRAY, PCD_RSWORLDARRAY, PCD_RSGLOBALARRAY,
      PCD_ANDMAPARRAY, PCD_ANDWORLDARRAY, PCD_ANDGLOBALARRAY,
      PCD_EORMAPARRAY, PCD_EORWORLDARRAY, PCD_EORGLOBALARRAY,
      PCD_ORMAPARRAY, PCD_ORWORLDARRAY, PCD_ORGLOBALARRAY };
   int pos = 0;
   switch ( storage ) {
   case STORAGE_WORLD: pos = 1; break;
   case STORAGE_GLOBAL: pos = 2; break;
   default: break;
   }
   c_add_opc( codegen, code[ op * 3 + pos ] );
   c_add_arg( codegen, index );
}

void write_strcpy( struct codegen* codegen, struct operand* operand,
   struct strcpy_call* call ) {
   struct operand object;
   init_operand( &object );
   object.action = ACTION_PUSH_VAR;
   visit_operand( codegen, &object, call->array->root );
   c_add_opc( codegen, PCD_PUSHNUMBER );
   c_add_arg( codegen, object.index );
   // Offset within the array.
   if ( call->array_offset ) {
      c_push_expr( codegen, call->array_offset, false );
   }
   else {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
   }
   // Number of characters to copy from the string.
   if ( call->array_length ) {
      c_push_expr( codegen, call->array_length, false );
   }
   else {
      enum { MAX_LENGTH = INT_MAX };
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, MAX_LENGTH );
   }
   // String field.
   c_push_expr( codegen, call->string, false );
   // String-offset field.
   if ( call->offset ) {
      c_push_expr( codegen, call->offset, false );
   }
   else {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
   }
   int code = PCD_STRCPYTOMAPCHRANGE;
   switch ( object.storage ) {
   case STORAGE_WORLD:
      code = PCD_STRCPYTOWORLDCHRANGE;
      break;
   case STORAGE_GLOBAL:
      code = PCD_STRCPYTOGLOBALCHRANGE;
      break;
   default:
      break;
   }
   c_add_opc( codegen, code );
   operand->pushed = true;
}