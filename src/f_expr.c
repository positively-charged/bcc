#include <string.h>

#include "f_main.h"
#include "mem.h"
#include "detail.h"

typedef struct {
   node_t* node;
   type_t* type;
   position_t pos;
   int value;
} operand_t;

static void init_operand( operand_t*, position_t* );
static void read_expr_offset( front_t*, operand_t* );
static void read_assign( front_t*, operand_t* );
static void read_cond( front_t*, operand_t* );
static void do_log_or( front_t*, operand_t* );
static void add_binary( operand_t*, int, operand_t* );
static void do_log_and( front_t*, operand_t* );
static void do_bit_or( front_t*, operand_t* );
static void do_bit_xor( front_t*, operand_t* );
static void do_bit_and( front_t*, operand_t* );
static void do_eq( front_t*, operand_t* );
static void do_lt( front_t*, operand_t* );
static void do_shift( front_t*, operand_t* );
static void do_add( front_t*, operand_t* );
static void do_mul( front_t*, operand_t* );
static void do_id( front_t*, operand_t* );
static void do_call( front_t*, operand_t* );
static void read_arg_format_item( front_t*, list_t* );
static void read_arg_format_block( front_t*, list_t* );
static void read_arg_expr( front_t*, list_t* );
static void do_array_access( front_t*, operand_t* );
static void read_comma( front_t*, operand_t* );
static void read_unary( front_t*, operand_t* );
static void add_unary( operand_t*, int );
static void read_postfix( front_t*, operand_t*);
static void read_member_access( front_t*, operand_t* );
static void read_primary( front_t*, operand_t*);
static int add_string( front_t*, token_t* );
static void set_name( front_t*, operand_t*, token_t* );
static void p_expr_finish( front_t*, p_expr_t*, operand_t*, bool is_nested );
static void diag_value_not_usable( front_t*, position_t* );

void p_expr_init( p_expr_t* expr, int offset ) {
   expr->parent = NULL;
   expr->expr = NULL;
   expr->offset = offset;
   expr->has_string = false;
   expr->is_undef = false;
   expr->nested = false;
}

void p_expr_read( front_t* front, p_expr_t* p_expr ) {
   p_expr->parent = front->expr;
   front->expr = p_expr;
   operand_t operand;
   init_operand( &operand, &front->token.pos );
   read_expr_offset( front, &operand );
   expr_t* expr = mem_alloc( sizeof( *expr ) );
   expr->node.type = k_node_expr;
   expr->root = operand.node;
   expr->type = operand.type;
   expr->pos = operand.pos;
   expr->value = 0;
   expr->is_constant = false;
   p_expr->expr = expr;
   front->expr = p_expr->parent;
   // Only analyze a full expression.
   if ( p_expr->nested ) {
      if ( p_expr->is_undef ) { p_expr->parent->is_undef = true; }
      if ( p_expr->has_string ) { p_expr->parent->has_string = true; }
   }
   else {
      // Delay analysis upon undefined symbols.
      if ( p_expr->is_undef ) {
         undef_expr_t* undef_expr = mem_temp_alloc( sizeof( *undef_expr ) );
         undef_expr->node = expr;
         undef_expr->num_undef = 0;
         list_t* stack = &front->undef_stack;
         while ( list_size( stack ) ) {
            undef_usage_t* usage = list_pop_head( stack );
            usage->expr = undef_expr;
            ++undef_expr->num_undef;
         }
      }
      else {
         p_test_expr( front, expr );
      }
   }
}

void init_operand( operand_t* operand, position_t* pos ) {
   operand->node = NULL;
   operand->type = NULL;
   operand->pos = *pos;
}

void read_expr_offset( front_t* front, operand_t* operand ) {
   switch ( front->expr->offset ) {
   case k_expr_comma:
      read_comma( front, operand );
      break;
   case k_expr_assign:
      read_unary( front, operand );
      read_assign( front, operand );
      break;
   default:
      read_unary( front, operand );
      read_cond( front, operand );
      break;
   }
}

void read_comma( front_t* front, operand_t* operand ) {
   read_unary( front, operand );
   read_assign( front, operand );
   while ( tk( front ) == tk_comma ) {
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      read_assign( front, &rside );
      comma_t* comma = mem_alloc( sizeof( *comma ) );
      comma->node.type = k_node_comma;
      comma->lside = operand->node;
      comma->rside = rside.node;
      operand->node = ( node_t* ) comma;
   }
}

void read_assign( front_t* front, operand_t* operand ) {
   // Operations with higher precedence must be performed before assignment.
   read_cond( front, operand );
   int op;
   switch ( tk( front ) ) {
   case tk_assign: op = k_assign; break;
   case tk_assign_add: op = k_assign_add; break;
   case tk_assign_sub: op = k_assign_sub; break;
   case tk_assign_mul: op = k_assign_mul; break;
   case tk_assign_div: op = k_assign_div; break;
   case tk_assign_mod: op = k_assign_mod; break;
   case tk_assign_shift_l: op = k_assign_shift_l; break;
   case tk_assign_shift_r: op = k_assign_shift_r; break;
   case tk_assign_bit_and: op = k_assign_bit_and; break;
   case tk_assign_bit_xor: op = k_assign_bit_xor; break;
   case tk_assign_bit_or: op = k_assign_bit_or; break;
   default: return;
   }
   drop( front );
   operand_t rside;
   init_operand( &rside, &front->token.pos );
   read_unary( front, &rside );
   read_cond( front, &rside );
   // Assignment is a right-associative operation, so check if the right side
   // of this assignment is another assignment.
   read_assign( front, &rside );
   assign_t* assign = mem_alloc( sizeof( *assign ) );
   assign->node.type = k_node_assign;
   assign->type = op;
   assign->lside = operand->node;
   assign->rside = rside.node;
   operand->node = ( node_t* ) assign;
}

void read_cond( front_t* front, operand_t* operand ) {
   do_log_or( front, operand );
   if ( tk( front ) != tk_question ) { return; }
   drop( front );
   operand_t lside;
   init_operand( &lside, &front->token.pos );
   read_expr_offset( front, &lside );
   skip( front, tk_colon );
   operand_t rside;
   init_operand( &rside, &front->token.pos );
   read_unary( front, &rside );
   do_log_or( front, &rside );
   read_cond( front, &rside );
   ternary_t* ternary = mem_alloc( sizeof( *ternary ) );
   ternary->node.type = k_node_ternary;
   ternary->test = operand->node;
   ternary->lside = lside.node;
   ternary->rside = rside.node;
   operand->node = ( node_t* ) ternary;
}

void do_log_or( front_t* front, operand_t* operand ) {
   do_log_and( front, operand );
   // In operations that are left-associative, a loop is used to overcome the
   // problem of left recursion.
   while ( tk( front ) == tk_log_or ) {
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      do_log_and( front, &rside );
      add_binary( operand, k_bop_log_or, &rside ); 
   }
}

void add_binary( operand_t* operand, int op, operand_t* rside ) {
   binary_t* binary = mem_alloc( sizeof( *binary ) );
   binary->node.type = k_node_binary;
   binary->op = op;
   binary->lside = operand->node;
   binary->rside = rside->node;
   binary->is_folded = false;
   binary->value = 0;
   operand->node = ( node_t* ) binary;
}

void do_log_and( front_t* front, operand_t* operand ) {
   do_bit_or( front, operand );
   while ( tk( front ) == tk_log_and ) {
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      do_bit_or( front, &rside );
      add_binary( operand, k_bop_log_and, &rside ); 
   }
}

void do_bit_or( front_t* front, operand_t* operand ) {
   do_bit_xor( front, operand );
   while ( tk( front ) == tk_bit_or ) {
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      do_bit_xor( front, &rside );
      add_binary( operand, k_bop_bit_or, &rside );
   }
}

void do_bit_xor( front_t* front, operand_t* operand ) {
   do_bit_and( front, operand );
   while ( tk( front ) == tk_bit_xor ) {
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      do_bit_and( front, &rside );
      add_binary( operand, k_bop_bit_xor, &rside );
   }
}

void do_bit_and( front_t* front, operand_t* operand ) {
   do_eq( front, operand );
   while ( tk( front ) == tk_bit_and ) {
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      do_eq( front, &rside );
      add_binary( operand, k_bop_bit_and, &rside );
   }
}

void do_eq( front_t* front, operand_t* operand ) {
   do_lt( front, operand );
   while ( true ) {
      int op;
      switch ( tk( front ) ) {
      case tk_eq: op = k_bop_equal; break;
      case tk_neq: op = k_bop_not_equal; break;
      default: return;
      }
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      do_lt( front, &rside );
      add_binary( operand, op, &rside );
   }
}

void do_lt( front_t* front, operand_t* operand ) {
   do_shift( front, operand );
   while ( true ) {
      int op;
      switch ( tk( front ) ) {
      case tk_lt: op = k_bop_less_than; break;
      case tk_lte: op = k_bop_less_than_equal; break;
      case tk_gt: op = k_bop_more_than; break;
      case tk_gte: op = k_bop_more_than_equal; break;
      default: return;
      }
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      do_shift( front, &rside );
      add_binary( operand, op, &rside );
   }
}

void do_shift( front_t* front, operand_t* operand ) {
   do_add( front, operand );
   while ( true ) {
      int op;
      switch ( tk( front ) ) {
      case tk_shift_l: op = k_bop_shift_l; break;
      case tk_shift_r: op = k_bop_shift_r; break;
      default: return;
      }
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      do_add( front, &rside );
      add_binary( operand, op, &rside );
   }
}

void do_add( front_t* front, operand_t* operand ) {
   do_mul( front, operand );
   while ( true ) {
      int op;
      switch ( tk( front ) ) {
      case tk_plus: op = k_bop_add; break;
      case tk_minus: op = k_bop_sub; break;
      default: return;
      }
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      do_mul( front, &rside );
      add_binary( operand, op, &rside );
   }
}

void do_mul( front_t* front, operand_t* operand ) {
   while ( true ) {
      int op;
      switch ( tk( front ) ) {
      case tk_star: op = k_bop_mul; break;
      case tk_slash: op = k_bop_div; break;
      case tk_mod: op = k_bop_mod; break;
      default: return;
      }
      drop( front );
      operand_t rside;
      init_operand( &rside, &front->token.pos );
      read_unary( front, &rside );
      add_binary( operand, op, &rside );
   }
}

// The prefix operations are processed recursively. When the last operation is
// read, do_value() is called to collect the value. Then the prefix operations
// are called on this value. It is important that the prefix operations have an
// existing value to work on.
void read_unary( front_t* front, operand_t* operand ) {
   switch ( tk( front ) ) {
   case tk_increment:
      drop( front );
      read_unary( front, operand );
      add_unary( operand, k_uop_pre_inc );
      break;
   case tk_decrement:
      drop( front );
      read_unary( front, operand );
      add_unary( operand, k_uop_pre_dec );
      break;
   case tk_plus:
      drop( front );
      read_unary( front, operand );
      add_unary( operand, k_uop_plus );
      break;
   case tk_minus:
      drop( front );
      read_unary( front, operand );
      add_unary( operand, k_uop_minus );
      break;
   case tk_log_not:
      drop( front );
      read_unary( front, operand );
      add_unary( operand, k_uop_log_not );
      break;
   case tk_bit_not:
      drop( front );
      read_unary( front, operand );
      add_unary( operand, k_uop_bit_not );
      break;
   default:
      // All prefix operations have been collected and are on the stack. Now
      // get the value before processing the operations.
      read_primary( front, operand );
      read_postfix( front, operand );
      // Return to process the stacked operations.
      break;
   }
}

void add_unary( operand_t* operand, int op ) {
   unary_t* unary = mem_alloc( sizeof( *unary ) );
   unary->node.type = k_node_unary;
   unary->op = op;
   unary->operand = operand->node;
   unary->is_folded = false;
   unary->value = 0;
   operand->node = ( node_t* ) unary;
}

void read_postfix( front_t* front, operand_t* operand ) {
   if ( front->reading_script_number ) { return; }
   while ( true ) {
      switch ( tk( front ) ) {
      case tk_bracket_l:
         do_array_access( front, operand );
         break;
      case tk_paran_l:
         do_call( front, operand );
         break;
      case tk_dot:
         read_member_access( front, operand );
         break;
      case tk_increment:
         add_unary( operand, k_uop_post_inc );
         drop( front );
         break;
      case tk_decrement:
         add_unary( operand, k_uop_post_dec );
         drop( front );
         break;
      default:
         return;
      }
   }
}

void do_array_access( front_t* front, operand_t* operand ) {
   skip( front, tk_bracket_l );
   p_expr_t index;
   p_expr_init( &index, k_expr_comma );
   index.nested = true;
   p_expr_read( front, &index );
   skip( front, tk_bracket_r );
   subscript_t* subscript = mem_alloc( sizeof( *subscript ) );
   subscript->node.type = k_node_subscript;
   subscript->operand = operand->node;
   subscript->index = index.expr;
   operand->node = ( node_t* ) subscript;
}

void do_call( front_t* front, operand_t* operand ) {
   position_t pos = front->token.pos;
   skip( front, tk_paran_l );
   list_t args;
   list_init( &args );
   switch ( tk( front ) ) {
   case tk_paran_r:
      break;
   case tk_id: ;
      // The argument might be a format item.
      token_t* token = tk_peek( front, 0 );
      if ( token->type == tk_colon ) {
         read_arg_format_item( front, &args ); 
      }
      else {
         read_arg_expr( front, &args );
      }
      break;
   case tk_struct:
      read_arg_format_block( front, &args );
      if ( tk( front ) == tk_comma ) {
         drop( front );
         read_arg_expr( front, &args );
      }
      break;
   default:
      read_arg_expr( front, &args );
      break;
   }
   skip( front, tk_paran_r );
   int in = k_call_in_none;
   if ( front->block->flags & k_stmt_format_block ) {
      in = k_call_in_format;
   }
   else if ( front->block->flags & k_stmt_function ) {
      in = k_call_in_func;
   }
   call_t* node = mem_alloc( sizeof( *node ) );
   node->node.type = k_node_call;
   node->func = operand->node;
   node->args = args;
   node->pos = pos;
   node->in = in;
   operand->node = ( node_t* ) node;
}

void read_arg_format_item( front_t* front, list_t* args ) {
   while ( true ) {
      test( front, tk_id );
      p_fcast_t fcast;
      p_fcast_init( front, &fcast, &front->token );
      drop( front );
      skip( front, tk_colon );
      p_expr_t arg;
      p_expr_init( &arg, k_expr_assign );
      arg.nested = true;
      p_expr_read( front, &arg );
      format_item_t* item = mem_alloc( sizeof( *item ) );
      item->node.type = k_node_format_item;
      item->cast = fcast.cast;
      item->expr = arg.expr;
      list_append( args, item );
      if ( tk( front ) == tk_comma ) {
         drop( front );
      }
      else {
         break;
      }
   }
   // Expression arguments follow.
   if ( tk( front ) == tk_semicolon ) {
      drop( front );
      read_arg_expr( front, args );
   }
}

void read_arg_format_block( front_t* front, list_t* args ) {
   drop( front );
   p_block_t block;
   p_block_init( &block, front->block, k_stmt_format_block );
   p_block_t* prev = front->block;
   front->block = &block;
   p_new_scope( front );
   p_read_compound( front );
   p_pop_scope( front );
   front->block = prev;
   format_block_t* format = mem_alloc( sizeof( *format ) );
   format->node.type = k_node_format_block;
   format->body = block.stmt_list;
   list_append( args, format );
   switch ( block.flow ) {
   // Cannot return while in a format block.
   case k_flow_dead:
   case k_flow_jump_stmt:
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, NULL,
         "cannot %s while in a format block",
         d_name_jump( block.jump_stmt.type ) );
      f_bail( front );
      break;
   default:
      break;
   }
}

void read_arg_expr( front_t* front, list_t* args ) {
   while ( true ) {
      p_expr_t arg;
      p_expr_init( &arg, k_expr_assign );
      arg.nested = true;
      p_expr_read( front, &arg );
      list_append( args, arg.expr );
      if ( tk( front ) == tk_comma ) {
         drop( front );
      }
      else {
         break;
      }
   }
}

void read_member_access( front_t* front, operand_t* operand ) {
   skip( front, tk_dot );
   test( front, tk_id );
   nkey_t* name = ntbl_goto( front->name_table,
      "!s", front->token.source, "!" );
   drop( front );
   member_access_t* access = mem_alloc( sizeof( *access ) );
   access->node.type = k_node_member_access;
   access->object = operand->node;
   access->name = name;
   access->member = NULL;
   operand->node = ( node_t* ) access;
}

void read_primary( front_t* front, operand_t* operand ) {
   if ( tk( front ) == tk_id ) {
      set_name( front, operand, &front->token );
      drop( front );
   }
   // Sub-expression.
   else if ( tk( front ) == tk_paran_l ) {
      drop( front );
      read_expr_offset( front, operand );
      skip( front, tk_paran_r );
   }
   else {
      p_literal_t literal;
      p_literal_init( front, &literal, &front->token );
      literal_t* node = mem_alloc( sizeof( *node ) );
      node->node.type = k_node_literal;
      node->value = literal.value;
      node->type = literal.type;
      node->pos = literal.pos;
      operand->node = ( node_t* ) node;
      drop( front );
   }
}

void set_name( front_t* front, operand_t* operand, token_t* token ) {
   nkey_t* key = ntbl_goto( front->name_table,
      "!l", token->length,
      "!s", token->source, "!" );
   // If no object is attached to the name, save the name for now. It might be
   // a function defined later.
   if ( ! key->value ) {
      // Extend the name with a special symbol to indicate it's undefined.
      nkey_t* undef_key = ntbl_goto( front->name_table,
         "!o", &key,
         "!s", "?", "!" );
      undef_t* undef = NULL;
      if ( ! undef_key->value ) {
         undef = mem_temp_alloc( sizeof( *undef ) );
         undef->node.type = k_node_undef;
         undef->usage = NULL;
         undef->next = NULL;
         undef->prev = NULL;
         undef->name = key;
         undef->is_called = false;
         p_use_key( front, undef_key, ( node_t* ) undef, false );
         p_attach_undef( front, undef );
      }
      else {
         undef = ( undef_t* ) undef_key->value->node;
      }
      // A single usage of the unknown object.
      undef_usage_t* usage = mem_temp_alloc( sizeof( *usage ) );
      usage->pos = token->pos;
      usage->expr = NULL;
      usage->next = undef->usage;
      undef->usage = usage;
      list_append( &front->undef_stack, usage );
      front->expr->is_undef = true;
      key = undef_key;
   }
   else if ( key->value->node->type == k_node_var ) {
      var_t* var = ( var_t* ) key->value->node;
      //++var->usage;
   }
   else if ( key->value->node->type == k_node_func ) {
      func_t* func = ( func_t* ) key->value->node;
      if ( func->type == k_func_user ) {
         ufunc_t* ufunc = ( ufunc_t* ) func;
         ++ufunc->usage;
      }
   }
   name_t* name = mem_alloc( sizeof( *name ) );
   name->node.type = k_node_name;
   name->key = key;
   name->object = key->value->node;
   name->pos = token->pos;
   operand->node = ( node_t* ) name;
}

#define WHOLE 65536

static int base_to_int( const char*, int );
static int a_translate_fix( const char* );

void p_literal_init( front_t* front, p_literal_t* literal, token_t* token ) {
   switch ( token->type ) {
   case tk_lit_octal:
      literal->value = base_to_int( token->source, 8 );
      break;
   case tk_lit_decimal:
      literal->value = base_to_int( token->source, 10 );
      break;
   case tk_lit_hex:
      literal->value = base_to_int( token->source, 16 );
      break;
   case tk_lit_fixed:
      literal->value = a_translate_fix( token->source );
      break;
   case tk_lit_char:
      literal->value = token->source[ 0 ];
      break;
   case tk_lit_string:
      literal->value = add_string( front, token );
      break;
   // Error.
   default:
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &token->pos,
         "expecting a literal but found something else" );
      f_bail( front );
      break;
   }
   //literal->type = get_builtin_type( token->type );
   literal->pos = token->pos;
}

// Retrieves the numeric value of a hexadecimal digit.
static int digit_value( char digit ) {
   switch ( digit ) {
   case 'a': case 'A': return 10;
   case 'b': case 'B': return 11;
   case 'c': case 'C': return 12;
   case 'd': case 'D': return 13;
   case 'e': case 'E': return 14;
   case 'f': case 'F': return 15;
   // Decimal digit:
   default:
      return digit - '0';
}
}

// Supports base 2 to 16, all using the hexadecimal digits, which includes the
// decimal digits. It assumes the digits are valid for the base specified.
static int base_to_int( const char* digit, int base ) {
   const char* start = digit;
   int result = 0;
   // Convert to value.
   while ( *digit ) {
      result = result * base + digit_value( *digit );
      ++digit;
   }
   // Check for overflow. The result must match the string exactly.
   int test = result;
   while ( digit != start ) {
      --digit;
      if ( test % base != digit_value( *digit ) ) {
         //printf( "Overflow\n" );
         break;
      }
      else {
         test /= base;
      }
   }
   return result;
}

int a_translate_fix( const char* ch ) {
   int result = 0;
   int space = WHOLE;

   // Whole part.
   while ( 1 ) {
      int digit = *ch - '0';
      result += space * digit;
      ++ch;
      if ( *ch != '.' ) {
         result *= 10;
      }
      else {
         break;
      }
   }
   ++ch;

   // Fraction part.
   int rem = 0;
   int fraction = 0;
   int place_value = 1;
   int div = 10;
   while ( space && *ch ) {
      int digit = *ch - '0';

      // Remainder.
      rem += ( space % 10 ) * place_value;
      fraction += rem * digit;
      fraction *= 10;
      place_value *= 10;
      div *= 10;

      // Whole.
      space /= 10;
      result += space * digit;

      ++ch;
   }

   return ( result + ( fraction / div ) );
}

// Saves the string literal into the string table, returning its index.
int add_string( front_t* front, token_t* token ) {
   // Find the first string smaller than the new string.
   indexed_string_t* string_prev = NULL;
   indexed_string_t* string = front->str_table.sorted_head;
   while ( string ) {
      if ( string->length < token->length ) {
         break;
      }
      if ( string->length == token->length ) {
         int cmp = memcmp( string->value, token->source, token->length );
         // If the string already exists in the list, don't duplicate it.
         if ( cmp == 0 ) {
            return string->index;
         }
         else if ( cmp < 0 ) {
            break;
         }
      }
      string_prev = string;
      string = string->sorted_next;
   }

   void* block = mem_alloc( sizeof( indexed_string_t ) + token->length + 1 );
   string = block;
   string->value = ( ( char* ) block ) + sizeof( *string );
   memcpy( string->value, token->source, token->length );
   string->value[ token->length ] = 0;
   string->index = front->str_table.size;
   string->length = token->length;
   string->next = NULL;
   string->sorted_next = NULL;
   ++front->str_table.size;

   // Update sorted list.
   if ( string_prev ) {
      string->sorted_next = string_prev->sorted_next;
      string_prev->sorted_next = string;
   }
   else {
      string->sorted_next = front->str_table.sorted_head;
      front->str_table.sorted_head = string;
   }

   // Append to sequence list.
   if ( front->str_table.tail ) {
      front->str_table.tail->next = string;
      front->str_table.tail = string;
   }
   else {
      front->str_table.head = string;
      front->str_table.tail = string;
   }

   return string->index;
}

typedef struct {
   type_t* type;
   nkey_t* func;
   bool is_constant;
   bool is_value;
   bool is_space;
   bool is_array;
   int value;
   position_t* pos;
   node_t* node;
   dimension_t* dim;
   node_t** origin;
} test_t;

static void expr_test_node( front_t*, test_t*, node_t* );
static void test_literal( front_t*, test_t*, literal_t* );
static void test_unary( front_t*, test_t*, unary_t* );
static void test_binary( front_t*, test_t*, binary_t* );
static void test_assign( front_t*, test_t*, assign_t* );
static void test_subscript( front_t*, test_t*, subscript_t* );
static void test_call( front_t*, test_t*, call_t* );
static void test_name( front_t*, test_t*, name_t* );
static void eval_unary( test_t*, unary_t* );
static void eval_binary( front_t*, test_t*, test_t*, binary_t* );
static void eval_conditional( test_t*, test_t*, test_t* );
static void test_access( front_t*, test_t*, member_access_t* );

test_t new_operand( test_t* other ) {
   test_t operand = { NULL, NULL, false, false, false, false, 0, NULL, NULL };
   return operand;
}

void expr_test( front_t* front, test_t* operand, node_t* node ) {
   operand->func = NULL;
   operand->is_constant = false;
   operand->is_value = false;
   operand->is_space = false;
   operand->is_array = false;
   operand->value = 0;
   operand->pos = NULL;
   operand->node = NULL;
   operand->dim = NULL;
   //operand->origin = NULL;
   expr_test_node( front, operand, node );
}

void p_test_expr( front_t* front, expr_t* expr ) {
   test_t operand = { NULL, false, 0 };
   operand.origin = &expr->root;
   expr_test_node( front, &operand, expr->root );
   expr->is_constant = operand.is_constant;
   expr->value = operand.value;
   expr->type = operand.type;
}

void expr_test_node( front_t* front, test_t* operand, node_t* node ) {
   switch ( node->type ) {
   case k_node_literal:
      test_literal( front, operand, ( literal_t* ) node );
      break;
   case k_node_call:
      test_call( front, operand, ( call_t* ) node );
      break;
   case k_node_unary:
      test_unary( front, operand, ( unary_t* ) node );
      break;
   case k_node_subscript:
      test_subscript( front, operand, ( subscript_t* ) node );
      break;
   case k_node_binary:
      test_binary( front, operand, ( binary_t* ) node );
      break;
   case k_node_assign:
      test_assign( front, operand, ( assign_t* ) node );
      break;
   case k_node_name:
      test_name( front, operand, ( name_t* ) node );
      break;
   case k_node_member_access:
      test_access( front, operand, ( member_access_t* ) node );
      break;
   case k_node_expr: {
         expr_t* expr = ( expr_t* ) node;
         expr_test_node( front, operand, expr->root );
         expr->value = operand->value;
         expr->is_constant = operand->is_constant;
      }
      break;
   default:
      break;
   }
}

void test_access( front_t* front, test_t* operand, member_access_t* access ) {
   test_t object;
   expr_test( front, &object, access->object );
   char buff[ 100 ];
   const char* string = ntbl_save( access->name, buff, 100 );
   nkey_t* key = ntbl_goto( front->name_table,
      "!a",
      "!o", &object.type->name,
      "!s", ".",
      "!s", string, "!" );
   if ( ! key || ! key->value ) {
      printf( "struct has no member: %s\n", string );
      f_bail( front );
   }
   type_member_t* member = ( type_member_t* ) key->value->node;
   if ( member->object->type == k_node_var ) {
      var_t* var = ( var_t* ) member->object;
      operand->type = var->type;
      operand->is_space = true;
      operand->is_value = true;
      if ( var->dim ) {
         operand->node = ( node_t* ) var;
         operand->dim = var->dim;
      }
   }
}

void test_literal( front_t* front, test_t* operand, literal_t* literal ) {
   operand->is_constant = true;
   operand->value = literal->value;
   operand->type = literal->type;
   operand->is_value = true;
   operand->pos = &literal->pos;
}

void test_call( front_t* front, test_t* operand, call_t* call ) {
   test_t object;
   expr_test( front, &object, call->func );
   func_t* func = ( func_t* ) object.func->value->node;
   // Call to a latent function cannot appear in a function or a format block.
   if ( func->latent && call->in != k_call_in_none ) {
      const char* area = "function";
      if ( call->in == k_call_in_format ) {
         area = "format block";
      }
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, object.pos,
         "latent function called in %s", area );
      f_bail( front );
   }
   int arg_count = 0;
   bool has_format_arg = false;
   list_iterator_t i = list_iterate( &call->args );
   if ( ! list_end( &i ) ) {
      node_t* node = list_idata( &i );
      // Format-items and -blocks together count as a single argument.
      if ( node->type == k_node_format_item ||
         node->type == k_node_format_block ) {
         has_format_arg = true;
         arg_count = 1;
      }
      // Test the arguments.
      while ( true ) {
         if ( node->type != k_node_format_block ) {
            test_t arg;
            if ( node->type == k_node_format_item ) {
               format_item_t* item = ( format_item_t* ) node;
               arg.origin = &item->expr->root;
               expr_test( front, &arg, &item->expr->node );
               if ( item->cast == k_fcast_array ) {
                  //if ( ! arg.is_array ) {
                   //  printf( "not array\n" );
                   //  job_abort( ast->job );
                  //}
               }
               else {
                  if ( ! arg.is_value ) {
                     diag_value_not_usable( front, arg.pos );
                     f_bail( front );
                  }
               }
            }
            else {
               expr_test( front, &arg, node );
               ++arg_count;
               // Argument must be a value.
               if ( ! arg.is_value ) {
                  diag_value_not_usable( front, arg.pos );
                  f_bail( front );
               }
            }
         }
         list_next( &i );
         if ( list_end( &i ) ) {
            break;
         }
         node = list_idata( &i );
      }
   }
   // Number of arguments must be correct.
   int param_count = -1;
   if ( arg_count >= func->min_param ) {
      if ( arg_count > func->num_param ) {
         param_count = func->num_param;
      }
   }
   else {
      param_count = func->min_param;
   }
   if ( param_count != -1 ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &call->pos,
         "function expects %d argument%s but %d given", param_count,
         param_count == 1 ? "" : "s", arg_count );
      f_bail( front );
   }
   // Format function needs to have a format-list argument.
   if ( func->type == k_func_format && ! has_format_arg ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &call->pos,
         "missing format-list argument" );
      f_bail( front );
   }
   // Function needs to be a format function if format-list argument is given.
   if ( has_format_arg && func->type != k_func_format ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, operand->pos,
         "function not a format-function" );
      f_bail( front );
   }
   operand->is_value = ( func->return_type != NULL );
   operand->is_constant = false;
}

void diag_value_not_usable( front_t* front, position_t* pos ) {
   f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, pos,
      "expression does not produce a usable value" );
}

void test_name( front_t* front, test_t* operand, name_t* name ) {
   operand->pos = &name->pos;
   if ( name->object->type == k_node_constant ) {
      constant_t* constant = ( constant_t* ) name->object;
      nkey_t* key = ntbl_goto( front->name_table, "!s", "int", "!" );
      operand->type = ( type_t* ) key->value->node;
      operand->is_constant = true;
      operand->is_value = true;
      operand->value = constant->value;
   }
   else if ( name->object->type == k_node_var ) {
      var_t* var = ( var_t* ) name->object;
      operand->type = var->type;
      operand->is_space = true;
      operand->is_value = true;
      if ( var->dim ) {
         operand->is_array = true;
         operand->dim = var->dim;
      }
   }
   else if ( name->object->type == k_node_func ) {
      operand->func = name->key;
      operand->pos = &name->pos;
   }
   // When an expression is tested, all of its undefined objects should already
   // be defined. Adjust the undefined nodes.
   else if ( name->object->type == k_node_undef ) {
      undef_t* undef = ( undef_t* ) name->object;
      name->key = undef->name;
      name->object = undef->name->value->node;
      operand->func = name->key;
   }
}

const char* name_unary_op( int op ) {
   static const char* name[] = {
      "unary-minus", "unary-plus", "!", "~", "pre-increment", "pre-decrement",
      "post-increment", "post-decrement" };
   return name[ op ];
}

void test_unary( front_t* front, test_t* operand, unary_t* unary ) {
   expr_test_node( front, operand, unary->operand );
   if ( unary->op == k_uop_pre_inc || unary->op == k_uop_pre_dec ||
      unary->op == k_uop_post_inc || unary->op == k_uop_post_dec ) {
      // Only an l-value can be incremented.
      if ( ! operand->is_space ) {
         const char* action = "incremented";
         if ( unary->op == k_uop_pre_dec || unary->op == k_uop_post_dec ) {
            action = "decremented";
         }
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, operand->pos,
            "operand cannot be %s", action );
         f_bail( front );
      }
      // An increment or decrement operation produces an r-value.
      operand->is_value = true;
      operand->is_space = false;
      operand->is_constant = false;
   }
   else {
      // The rest of the unary operations require a value to work on.
      if ( ! operand->is_value ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, operand->pos,
            "operand cannot be used in %s operation",
            name_unary_op( unary->op ) );
         f_bail( front );
      }
      eval_unary( operand, unary );
      operand->is_value = true;
      operand->is_space = false;
   }
}

void test_subscript( front_t* front, test_t* operand,
   subscript_t* subscript ) {
   test_t object;
   expr_test( front, &object, subscript->operand );
   // At this time, only arrays can have the subscript operation.
   if ( ! object.node && ! object.is_array ) {
      nkey_t* key = ntbl_goto( front->name_table,
         "!a",
         "!o", &object.type->name,
         "!s", ".[]", "!" );
      if ( key ) {
         member_access_t* access = mem_alloc( sizeof( *access ) );
         access->node.type = k_node_member_access;
         access->object = subscript->operand;
         access->name = key;
         call_t* call = mem_alloc( sizeof( *call ) );
         call->node.type = k_node_call;
         call->func = &access->node;
         list_init( &call->args );
         list_append( &call->args, subscript->index );
         call->in = k_call_in_none;
         *operand->origin = &call->node;
         operand->is_value = true;
         return;
      }
      ntbl_show_key( key );
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, operand->pos,
         "[] operation not available for operand" );
      f_bail( front );
   }
   // Make sure there are dimensions left to view.
   if ( ! object.dim ) {
//msg_show( front->msg, k_msg_pos, operand->pos );
      f_bail( front );
   }
   test_t index;
   expr_test( front, &index, subscript->index->root );
   // Index must be a value.
   if ( ! index.is_value ) {
      diag_value_not_usable( front, index.pos );
      f_bail( front );
   }
   operand->dim = object.dim->next;
   operand->node = object.node;
   operand->type = object.type;
   // Value.
   if ( operand->dim ) {
      operand->is_value = false;
      operand->is_space = false;
      operand->is_array = true;
   }
   else {
      operand->is_value = true;
      operand->is_space = true;
   }
}

void test_binary( front_t* front, test_t* lside, binary_t* binary ) {
   lside->origin = &binary->lside;
   expr_test_node( front, lside, binary->lside );
   test_t rside;
   rside.origin = &binary->rside;
   expr_test( front, &rside, binary->rside );
   // Left side must be a
   position_t* pos = NULL;
   const char* side = "lside";
   if ( ! lside->is_value ) {
      pos = lside->pos;
   }
   if ( ! rside.is_value ) {
      pos = rside.pos;
      side = "rside";
   }
   if ( pos ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, pos,
         "%s cannot be used in binary operation", side );
      f_bail( front );
   }
   // Perform constant folding by evaluating the expression at compile time.
   if ( lside->is_constant && rside.is_constant ) {
      eval_binary( front, lside, &rside, binary );
   }
   else {
      lside->is_constant = false;
   }
   lside->is_value = true;
}

void test_assign( front_t* front, test_t* lside, assign_t* assign ) {
   expr_test_node( front, lside, assign->lside );
   test_t rside;
   expr_test( front, &rside, assign->rside );
   // Left operand must be a valid spot to assign to.
   if ( ! lside->is_space ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, lside->pos,
         "left side cannot be used in assignment" );
      f_bail( front );
   }
   // Right operand must be a value.
   if ( ! rside.is_value ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, rside.pos,
         "right side cannot be used in assignment" );
      f_bail( front );
   }
   // The result of an assignment is the value of the left operand, not the
   // left operand itself.
   lside->is_value = true;
   lside->is_space = false;
   lside->is_constant = false;
}

void eval_unary( test_t* operand, unary_t* unary ) {
   if ( ! operand->is_constant ) {
      return;
   }
   else if ( unary->op == k_uop_bit_not ) {
      operand->value = ( ~ operand->value );
      operand->is_constant = true;
      unary->is_folded = true;
      unary->value = operand->value;
   }
   else if ( unary->op == k_uop_log_not ) {
      operand->value = ( ! operand->value );
      operand->is_constant = true;
      unary->is_folded = true;
      unary->value = operand->value;
   }
   else if ( unary->op == k_uop_minus ) {
      operand->value = ( - operand->value );
      operand->is_constant = true;
      unary->is_folded = true;
      unary->value = operand->value;
   }
   else if ( unary->op == k_uop_plus ) {
      operand->value = ( + operand->value );
      operand->is_constant = true;
      unary->is_folded = true;
      unary->value = operand->value;
   }
   else {
      // The increment operations require a variable as an operand, so they
      // cannot be evaluated at compile time.
      operand->is_constant = false;
   }
}

void eval_binary( front_t* front, test_t* lside, test_t* rside,
   binary_t* binary ) {
   int value = lside->value;
   int r = rside->value;
   // Division and modulo get special treatment because of the possibility
   // of a division by zero.
   if ( binary->op == k_bop_div || binary->op == k_bop_mod ) {
      if ( ! r ) {
         f_diag( front, DIAG_WARNING | DIAG_LINE | DIAG_COLUMN, rside->pos,
            "division by zero" );
         lside->is_constant = false;
         return;
      }
      if ( binary->op == k_bop_div ) {
         value /= r;
      }
      else {
         value %= r;
      }
   }
   else {
      switch ( binary->op ) {
      case k_bop_log_or: value = ( value || r ); break;
      case k_bop_log_and: value = ( value && r ); break;
      case k_bop_bit_or: value |= r; break;
      case k_bop_bit_xor: value ^= r; break;
      case k_bop_bit_and: value &= r; break;
      case k_bop_equal: value = ( value == r ); break;
      case k_bop_not_equal: value = ( value != r ); break;
      case k_bop_less_than: value = ( value < r ); break;
      case k_bop_less_than_equal: value = ( value <= r ); break;
      case k_bop_more_than: value = ( value > r ); break;
      case k_bop_more_than_equal: value = ( value >= r ); break;
      case k_bop_shift_l: value <<= r; break;
      case k_bop_shift_r: value >>= r; break;
      case k_bop_add: value += r; break;
      case k_bop_sub: value -= r; break;
      case k_bop_mul: value *= r; break;
      default: break;
      }
   }
   binary->is_folded = true;
   binary->value = value;
   lside->value = value;
   lside->is_constant = true;
}

void eval_conditional( test_t* test, test_t* lside, test_t* rside ) {
   if ( test->value ) {
      test->value = lside->value;
   }
   else {
      test->value = rside->value;
   }
}