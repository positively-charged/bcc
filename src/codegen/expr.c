#include <limits.h>

#include "phase.h"
#include "pcode.h"

enum { FIXEDNUMBER_WHOLE = 1 << 16 };
enum { NULLVALUE = 0 };
enum { EXTENDEDASPEC_STARTID = 256 };

enum result_description {
   RESULTDESC_ARRAY,
   RESULTDESC_REF,
   RESULTDESC_STRUCT,
   RESULTDESC_PRIMITIVE
};

enum {
   EXPECTING_SPACE,
   EXPECTING_VALUE,
};

struct result {
   struct ref* ref;
   struct structure* structure;
   struct dim* dim;
   enum {
      R_NONE,
      R_VALUE,
      R_VAR,
      R_ARRAY,
      R_ARRAYINDEX,
      R_NULL
   } status;
   int storage;
   int index;
   int ref_dim;
   int diminfo_start;
   bool push;
   bool skip_negate;
   bool null;
   bool safe;
   bool direct;
};

static void init_result( struct result* result, bool push );
static enum result_description describe_result( struct result* result );
static void push_operand( struct codegen* codegen, struct node* node );
static void push_operand_result( struct codegen* codegen,
   struct result* result, struct node* node );
static bool in_shared_array( struct codegen* codegen, struct result* result );
static bool has_dim_info( struct result* result );
static void push_initz( struct codegen* codegen, struct ref* ref,
   struct node* initz );
static void visit_operand( struct codegen* codegen, struct result* result,
   struct node* node );
static void visit_binary( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_int( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_fixed( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_bool( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_str( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void eq_str( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void lt_str( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void concat_str( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_ref( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void eq_ref( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void write_binary_ref_func( struct codegen* codegen,
   struct result* result, struct binary* binary );
static void eq_func( struct codegen* codegen, struct result* result,
   struct binary* binary );
static void visit_logical( struct codegen* codegen,
   struct result* result, struct logical* logical );
static void write_logical_acs( struct codegen* codegen, struct result* result,
   struct logical* logical );
static void write_logical( struct codegen* codegen,
   struct result* result, struct logical* logical );
static void push_logical_operand( struct codegen* codegen,
   struct node* node, int spec );
static void convert_to_boolean( struct codegen* codegen, struct result* result,
   int spec );
static void visit_assign( struct codegen* codegen, struct result* result,
   struct assign* assign );
static void assign_value( struct codegen* codegen, struct assign* assign,
   struct result* result );
static void assign_value_var( struct codegen* codegen, struct assign* assign,
   struct result* lside, struct result* result );
static void assign_value_array( struct codegen* codegen, struct assign* assign,
   struct result* lside, struct result* result );
static void assign_fixed( struct codegen* codegen, struct assign* assign,
   struct result* result );
static void assign_fixed_muldiv( struct codegen* codegen,
   struct assign* assign, struct result* result );
static void assign_fixed_muldiv_var( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result );
static void assign_fixed_muldiv_array( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result );
static void assign_str( struct codegen* codegen, struct assign* assign,
   struct result* result );
static void assign_str_add( struct codegen* codegen, struct assign* assign,
   struct result* result );
static void assign_str_add_var( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result );
static void assign_str_add_array( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result );
static void assign_array_ref( struct codegen* codegen, struct assign* assign,
   struct result* result );
static void assign_array_ref_var( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result );
static void assign_array_ref_array( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result );
static void visit_conditional( struct codegen* codegen,
   struct result* result, struct conditional* cond );
static void write_conditional( struct codegen* codegen,
   struct result* result, struct conditional* cond );
static void write_middleless_conditional( struct codegen* codegen,
   struct result* result, struct conditional* cond );
static void visit_prefix( struct codegen* codegen, struct result* result,
   struct node* node );
static void visit_unary( struct codegen* codegen, struct result* result,
   struct unary* unary );
static void write_logical_not( struct codegen* codegen, struct result* result,
   struct unary* unary );
static void write_unary( struct codegen* codegen, struct result* result,
   struct unary* unary );
static void visit_inc( struct codegen* codegen,
   struct result* result, struct inc* inc );
static void inc_array( struct codegen* codegen, struct result* result,
   struct inc* inc, struct result* operand );
static void inc_element( struct codegen* codegen, int storage, int index,
   bool do_inc );
static void inc_var( struct codegen* codegen, struct result* result,
   struct inc* inc, struct result* operand );
static void inc_indexed( struct codegen* codegen, int storage, int index,
   bool do_inc );
static void inc_fixed( struct codegen* codegen, struct inc* inc,
   struct result* operand );
static void visit_cast( struct codegen* codegen, struct result* result,
   struct cast* cast );
static void visit_suffix( struct codegen* codegen, struct result* result,
   struct node* node );
static void visit_subscript( struct codegen* codegen, struct result* result,
   struct subscript* subscript );
static void subscript_array( struct codegen* codegen,
   struct subscript* subscript, struct result* lside, struct result* result );
static void copy_diminfo( struct codegen* codegen, struct result* lside );
static void subscript_array_reference( struct codegen* codegen,
   struct subscript* subscript, struct result* lside, struct result* result );
static void subscript_str( struct codegen* codegen,
   struct subscript* subscript, struct result* result );
static void visit_access( struct codegen* codegen, struct result* result,
   struct access* access );
static void access_structure_member( struct codegen* codegen,
   struct structure_member* member, struct result* lside,
   struct result* result );
static void visit_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_aspec_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_ext_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_ded_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_format_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void call_format( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_format_item( struct codegen* codegen,
   struct format_item* item );
static void visit_array_format_item( struct codegen* codegen,
   struct format_item* item );
static void visit_msgbuild_format_item( struct codegen* codegen,
   struct format_item* item );
static void visit_user_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void call_local_user_func( struct codegen* codegen,
   struct result* result, struct call* call );
static void call_user_func( struct codegen* codegen, struct result* result,
   struct call* call );
static void write_call_args( struct codegen* codegen, struct call* call );
static void push_arg( struct codegen* codegen, struct param* param,
   struct expr* expr );
static void set_user_func_call_result( struct codegen* codegen,
   struct call* call, struct result* result );
static void visit_sample_call( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_internal_call( struct codegen* codegen,
   struct result* result, struct call* call );
static void write_executewait( struct codegen* codegen, struct call* call,
   bool named_impl );
static void call_array_length( struct codegen* codegen, struct result* result,
   struct call* call );
static void visit_sure( struct codegen* codegen, struct result* result,
   struct sure* sure );
static bool is_null_check_needed( struct result* result );
static void write_null_check( struct codegen* codegen );
static void visit_primary( struct codegen* codegen, struct result* result,
   struct node* node );
static void visit_literal( struct codegen* codegen, struct result* result,
   struct literal* literal );
static void visit_fixed_literal( struct codegen* codegen,
   struct result* result, struct fixed_literal* literal );
static void visit_indexed_string_usage( struct codegen* codegen,
   struct result* result, struct indexed_string_usage* usage );
static void visit_string( struct codegen* codegen,
   struct result* result, struct indexed_string* string );
static void visit_boolean( struct codegen* codegen, struct result* result,
   struct boolean* boolean );
static void visit_name_usage( struct codegen* codegen, struct result* result,
   struct name_usage* usage );
static void visit_qualified_name_usage( struct codegen* codegen,
   struct result* result, struct qualified_name_usage* usage );
static void visit_general_name_usage( struct codegen* codegen,
   struct result* result, struct node* object );
static void visit_constant( struct codegen* codegen, struct result* result,
   struct constant* constant );
static void visit_enumerator( struct codegen* codegen,
   struct result* result, struct enumerator* enumerator );
static void visit_var( struct codegen* codegen, struct result* result,
   struct var* var );
static void visit_shary_ref_var( struct codegen* codegen,
   struct result* result, struct var* var );
static void visit_ref_var( struct codegen* codegen, struct result* result,
   struct var* var );
static void visit_param( struct codegen* codegen, struct result* result,
   struct param* param );
static void visit_func( struct codegen* codegen, struct result* result,
   struct func* func );
static void visit_magic_id( struct codegen* codegen, struct result* result,
   struct magic_id* magic_id );
static void visit_strcpy( struct codegen* codegen, struct result* result,
   struct strcpy_call* call );
static void visit_memcpy( struct codegen* codegen, struct result* result,
   struct memcpy_call* call );
static void copy_array( struct codegen* codegen, struct result* result,
   struct memcpy_call* call );
static bool push_offset_scale_factor( struct codegen* codegen,
   struct result* result );
static void push_array_size( struct codegen* codegen, struct result* result );
static void push_array_length( struct codegen* codegen, struct result* result,
   bool dim_info_pushed );
static void push_ref_array_length( struct codegen* codegen,
   struct result* result, bool dim_info_pushed );
static void copy_struct( struct codegen* codegen, struct result* result,
   struct memcpy_call* call );
static void visit_lengthof( struct codegen* codegen, struct result* result,
   struct lengthof* call );
static void visit_conversion( struct codegen* codegen, struct result* result,
   struct conversion* conv );
static void visit_null( struct codegen* codegen, struct result* result );
static void visit_paren( struct codegen* codegen, struct result* result,
   struct paren* paren );
static void visit_compound_literal( struct codegen* codegen,
   struct result* result, struct compound_literal* literal );
static void push_indexed( struct codegen* codegen, int storage, int index );
static void push_element( struct codegen* codegen, int storage, int index );
static void inc_dimtrack( struct codegen* codegen );
static void next_diminfo( struct codegen* codegen );
static void push_diminfo( struct codegen* codegen );

static const int g_aspec_code[] = {
   PCD_LSPEC1,
   PCD_LSPEC2,
   PCD_LSPEC3,
   PCD_LSPEC4,
   PCD_LSPEC5
};

void c_visit_expr( struct codegen* codegen, struct expr* expr ) {
   struct result result;
   init_result( &result, false );
   visit_operand( codegen, &result, expr->root );
   switch ( result.status ) {
   case R_VALUE:
   case R_ARRAYINDEX:
      c_pcd( codegen, PCD_DROP );
      break;
   case R_NONE:
   case R_VAR:
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

void c_push_expr( struct codegen* codegen, struct expr* expr ) {
   push_operand( codegen, expr->root );
}

void c_push_bool_expr( struct codegen* codegen, struct expr* cond ) {
   push_logical_operand( codegen, cond->root, cond->spec );
}

void c_push_bool_cond_var( struct codegen* codegen, struct var* var ) {
   struct result result;
   init_result( &result, true );
   result.skip_negate = true;
   visit_var( codegen, &result, var );
   convert_to_boolean( codegen, &result, var->spec );
}

void c_push_initz_expr( struct codegen* codegen, struct ref* ref,
   struct expr* expr ) {
   push_initz( codegen, ref, expr->root );
}

static void init_result( struct result* result, bool push ) {
   result->ref = NULL;
   result->structure = NULL;
   result->dim = NULL;
   result->status = R_NONE;
   result->storage = 0;
   result->index = 0;
   result->ref_dim = 0;
   result->diminfo_start = 0;
   result->push = push;
   result->skip_negate = false;
   result->null = false;
   result->safe = false;
   result->direct = false;
}

static enum result_description describe_result( struct result* result ) {
   if ( result->dim ) {
      return RESULTDESC_ARRAY;
   }
   else if ( result->ref ) {
      return RESULTDESC_REF;
   }
   else if ( result->structure ) {
      return RESULTDESC_STRUCT;
   }
   else {
      return RESULTDESC_PRIMITIVE;
   }
}

static void push_operand( struct codegen* codegen, struct node* node ) {
   struct result result;
   init_result( &result, true );
   push_operand_result( codegen, &result, node );
}

static void push_operand_result( struct codegen* codegen,
   struct result* result, struct node* node ) {
   visit_operand( codegen, result, node );
   switch ( describe_result( result ) ) {
   case RESULTDESC_ARRAY:
      if ( in_shared_array( codegen, result ) ) {
         if ( has_dim_info( result ) ) {
            c_pcd( codegen, PCD_PUSHNUMBER, result->diminfo_start );
            c_update_dimtrack( codegen );
         }
      }
      else {
         if ( result->status == R_NONE ) {
            c_pcd( codegen, PCD_PUSHNUMBER, 0 );
            result->status = R_ARRAYINDEX;
         }
      }
      break;
   case RESULTDESC_STRUCT:
      if ( result->status == R_NONE ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         result->status = R_ARRAYINDEX;
      }
      break;
   case RESULTDESC_REF:
   case RESULTDESC_PRIMITIVE:
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static bool in_shared_array( struct codegen* codegen, struct result* result ) {
   return ( codegen->shary.used && result->storage == STORAGE_MAP &&
      result->index == codegen->shary.index );
}

static bool has_dim_info( struct result* result ) {
   return ( result->diminfo_start > 0 );
}

static void push_initz( struct codegen* codegen, struct ref* ref,
   struct node* initz ) {
   struct result result;
   init_result( &result, true );
   push_operand_result( codegen, &result, initz );
}

static void visit_operand( struct codegen* codegen, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_BINARY:
      visit_binary( codegen, result,
         ( struct binary* ) node );
      break;
   case NODE_LOGICAL:
      visit_logical( codegen, result,
         ( struct logical* ) node );
      break;
   case NODE_ASSIGN:
      visit_assign( codegen, result,
         ( struct assign* ) node );
      break;
   case NODE_CONDITIONAL:
      visit_conditional( codegen, result,
         ( struct conditional* ) node );
      break;
   default:
      visit_prefix( codegen, result,
         node );
      break;
   }
}

static void visit_binary( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   switch ( binary->operand_type ) {
   case BINARYOPERAND_PRIMITIVERAW:
   case BINARYOPERAND_PRIMITIVEINT:
      write_binary_int( codegen, result, binary );
      break;
   case BINARYOPERAND_PRIMITIVEFIXED:
      write_binary_fixed( codegen, result, binary );
      break;
   case BINARYOPERAND_PRIMITIVEBOOL:
      write_binary_bool( codegen, result, binary );
      break;
   case BINARYOPERAND_PRIMITIVESTR:
      write_binary_str( codegen, result, binary );
      break;
   case BINARYOPERAND_REF:
      write_binary_ref( codegen, result, binary );
      break;
   case BINARYOPERAND_REFFUNC:
      write_binary_ref_func( codegen, result, binary );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void write_binary_int( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
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
   case BOP_MUL: code = PCD_MULTIPLY; break;
   case BOP_DIV: code = PCD_DIVIDE; break;
   case BOP_MOD: code = PCD_MODULUS; break;
   default: 
      C_UNREACHABLE( codegen );
   }
   c_pcd( codegen, code );
   result->status = R_VALUE;
}

static void write_binary_fixed( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
   int code = PCD_NONE;
   switch ( binary->op ) {
   case BOP_ADD: code = PCD_ADD; break;
   case BOP_SUB: code = PCD_SUBTRACT; break;
   case BOP_MUL: code = PCD_FIXEDMUL; break;
   case BOP_DIV: code = PCD_FIXEDDIV; break;
   case BOP_EQ: code = PCD_EQ; break;
   case BOP_NEQ: code = PCD_NE; break;
   case BOP_LT: code = PCD_LT; break;
   case BOP_LTE: code = PCD_LE; break;
   case BOP_GT: code = PCD_GT; break;
   case BOP_GTE: code = PCD_GE; break;
   default:
      C_UNREACHABLE( codegen );
   }
   c_pcd( codegen, code );
   result->status = R_VALUE;
}

static void write_binary_bool( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
   int code = PCD_NONE;
   switch ( binary->op ) {
   case BOP_EQ: code = PCD_EQ; break;
   case BOP_NEQ: code = PCD_NE; break;
   default:
      C_UNREACHABLE( codegen );
   }
   c_pcd( codegen, code );
   result->status = R_VALUE;
}

static void write_binary_str( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
      eq_str( codegen, result, binary );
      break;
   case BOP_LT:
   case BOP_LTE:
   case BOP_GT:
   case BOP_GTE:
      lt_str( codegen, result, binary );
      break;
   case BOP_ADD:
      concat_str( codegen, result, binary );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void eq_str( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
   c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_STRCMP );
   c_pcd( codegen, PCD_NEGATELOGICAL );
   if ( binary->op == BOP_NEQ ) {
      c_pcd( codegen, PCD_NEGATELOGICAL );
   }
   result->status = R_VALUE;
}

static void lt_str( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
   c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_STRCMP );
   c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   int code = PCD_LT;
   switch ( binary->op ) {
   case BOP_LT:
      break;
   case BOP_LTE:
      code = PCD_LE;
      break;
   case BOP_GTE:
      code = PCD_GE;
      break;
   case BOP_GT:
      code = PCD_GT;
      break;
   default:
      C_UNREACHABLE( codegen );
   }
   c_pcd( codegen, code );
   result->status = R_VALUE;
}

static void concat_str( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   c_pcd( codegen, PCD_BEGINPRINT );
   push_operand( codegen, binary->lside );
   c_pcd( codegen, PCD_PRINTSTRING );
   push_operand( codegen, binary->rside );
   c_pcd( codegen, PCD_PRINTSTRING );
   c_pcd( codegen, PCD_SAVESTRING );
   result->status = R_VALUE;
}

static void write_binary_ref( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
      eq_ref( codegen, result, binary );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void eq_ref( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   struct result lside;
   init_result( &lside, true );
   push_operand_result( codegen, &lside, binary->lside );
   struct result rside;
   init_result( &rside, true );
   push_operand_result( codegen, &rside, binary->rside );
   STATIC_ASSERT( NULLVALUE == 0 );
   if ( lside.null && rside.null ) {
      c_pcd( codegen, ( binary->op == BOP_EQ ) ? PCD_EQ : PCD_NE );
   }
   else if ( ( lside.null && ! rside.null ) ||
      ( rside.null && ! lside.null ) ) {
      // A direct array or struct reference is always valid. Depending on the
      // operation, force a true or false result.
      if ( ( lside.null && rside.direct ) || ( rside.null && lside.direct ) ) {
         // Assume the equal operation here. Since a valid reference is not
         // equal to null, we want false as the result here. Since one of the
         // operands is null, which has a 0 value, executing PCD_ANDLOGICAL
         // will give us a 0, regardless of the offset.
         c_pcd( codegen, PCD_ANDLOGICAL );
         // Negate the result of the equal operation to get the result for the
         // not-equal operation. (`a != b` is the same as `! ( a == b )`.)
         if ( binary->op == BOP_NEQ ) {
            c_pcd( codegen, PCD_NEGATELOGICAL );
         }
      }
      // For array references (shared array offsets).
      else {
         c_pcd( codegen, ( binary->op == BOP_EQ ) ? PCD_EQ : PCD_NE );
      }
   }
   else {
      // For two references to be the same, they must point to the same array
      // and have the same offset.
      if ( lside.index == rside.index ) {
         c_pcd( codegen, ( binary->op == BOP_EQ ) ? PCD_EQ : PCD_NE );
      }
      else {
         c_pcd( codegen, PCD_DROP );
         c_pcd( codegen, PCD_DROP );
         c_pcd( codegen, PCD_PUSHNUMBER,
            ( binary->op == BOP_EQ ) ? 0 : 1 );
      }
   }
   result->status = R_VALUE;
}

static void write_binary_ref_func( struct codegen* codegen,
   struct result* result, struct binary* binary ) {
   switch ( binary->op ) {
   case BOP_EQ:
   case BOP_NEQ:
      eq_func( codegen, result, binary );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void eq_func( struct codegen* codegen, struct result* result,
   struct binary* binary ) {
   push_operand( codegen, binary->lside );
   push_operand( codegen, binary->rside );
   c_pcd( codegen, ( binary->op == BOP_EQ ) ? PCD_EQ : PCD_NE );
   result->status = R_VALUE;
}

static void visit_logical( struct codegen* codegen,
   struct result* result, struct logical* logical ) {
   if ( logical->folded ) {
      c_pcd( codegen, PCD_PUSHNUMBER, logical->value );
      result->status = R_VALUE;
   }
   else {
      switch ( codegen->lang ) {
      case LANG_ACS:
      case LANG_ACS95:
         write_logical_acs( codegen, result, logical );
         break;
      default:
         write_logical( codegen, result, logical );
         break;
      }
   }
}

static void write_logical_acs( struct codegen* codegen, struct result* result,
   struct logical* logical ) {
   push_operand( codegen, logical->lside );
   push_operand( codegen, logical->rside );
   c_pcd( codegen, logical->op == LOP_AND ? PCD_ANDLOGICAL : PCD_ORLOGICAL );
}

// Logical-or and logical-and both perform shortcircuit evaluation.
static void write_logical( struct codegen* codegen, struct result* result,
   struct logical* logical ) {
   push_logical_operand( codegen, logical->lside, logical->lside_spec );
   struct c_jump* rside_jump = c_create_jump( codegen,
      ( logical->op == LOP_OR ? PCD_IFNOTGOTO : PCD_IFGOTO ) );
   c_append_node( codegen, &rside_jump->node );
   c_pcd( codegen, PCD_PUSHNUMBER,
      ( logical->op == LOP_OR ? 1 : 0 ) );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &exit_jump->node );
   struct c_point* rside_point = c_create_point( codegen );
   c_append_node( codegen, &rside_point->node );
   rside_jump->point = rside_point;
   push_logical_operand( codegen, logical->rside, logical->rside_spec );
   // Optimization: When doing a calculation temporarily, there's no need to
   // convert the second operand to a 0 or 1. Just use the operand directly.
   if ( ! result->skip_negate ) {
      c_pcd( codegen, PCD_NEGATELOGICAL );
      c_pcd( codegen, PCD_NEGATELOGICAL );
   }
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
   result->status = R_VALUE;
}

static void push_logical_operand( struct codegen* codegen,
   struct node* node, int spec ) {
   struct result result;
   init_result( &result, true );
   result.skip_negate = true;
   push_operand_result( codegen, &result, node );
   convert_to_boolean( codegen, &result, spec );
}

// NOTE: Doesn't actually convert a value to 0 or 1. Should rename.
static void convert_to_boolean( struct codegen* codegen, struct result* result,
   int spec ) {
   switch ( describe_result( result ) ) {
   case RESULTDESC_ARRAY:
   case RESULTDESC_STRUCT:
      // A direct array or struct reference is always valid. Push a 1 and use
      // the PCD_ORLOGICAL instruction to force a true result, regardless of
      // what the offset might be. For an indirect reference, the offset will
      // always be non-zero, so no additional code is necessary.
      if ( result->direct ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 1 );
         c_pcd( codegen, PCD_ORLOGICAL );
      }
      break;
   case RESULTDESC_PRIMITIVE:
      if ( spec == SPEC_STR ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_GETCHAR );
      }
      break;
   case RESULTDESC_REF:
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void visit_assign( struct codegen* codegen, struct result* result,
   struct assign* assign ) {
   switch ( assign->lside_type ) {
   case ASSIGNLSIDE_PRIMITIVE:
   case ASSIGNLSIDE_REF:
      assign_value( codegen, assign, result );
      break;
   case ASSIGNLSIDE_PRIMITIVEFIXED:
      assign_fixed( codegen, assign, result );
      break;
   case ASSIGNLSIDE_PRIMITIVESTR:
      assign_str( codegen, assign, result );
      break;
   case ASSIGNLSIDE_REFARRAY:
      assign_array_ref( codegen, assign, result );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void assign_value( struct codegen* codegen, struct assign* assign,
   struct result* result ) {
   struct result lside;
   init_result( &lside, EXPECTING_SPACE );
   visit_operand( codegen, &lside, assign->lside );
   switch ( lside.status ) {
   case R_VAR:
      assign_value_var( codegen, assign, &lside, result );
      break;
   case R_ARRAYINDEX:
      assign_value_array( codegen, assign, &lside, result );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void assign_value_var( struct codegen* codegen, struct assign* assign,
   struct result* lside, struct result* result ) {
   push_operand( codegen, assign->rside );
   if ( assign->op == AOP_NONE && result->push ) {
      c_pcd( codegen, PCD_DUP );
      result->status = R_VALUE;
   }
   c_update_indexed( codegen, lside->storage, lside->index, assign->op );
   if ( assign->op != AOP_NONE && result->push ) {
      push_indexed( codegen, lside->storage, lside->index );
      result->status = R_VALUE;
   }
}

static void assign_value_array( struct codegen* codegen, struct assign* assign,
   struct result* lside, struct result* result ) {
   if ( result->push ) {
      c_pcd( codegen, PCD_DUP );
   }
   push_operand( codegen, assign->rside );
   c_update_element( codegen, lside->storage, lside->index, assign->op );
   if ( result->push ) {
      push_element( codegen, lside->storage, lside->index );
      result->status = R_VALUE;
   }
}

static void assign_fixed( struct codegen* codegen, struct assign* assign,
   struct result* result ) {
   switch ( assign->op ) {
   case AOP_NONE:
   case AOP_ADD:
   case AOP_SUB:
      assign_value( codegen, assign, result );
      break;
   case AOP_MUL:
   case AOP_DIV:
      assign_fixed_muldiv( codegen, assign, result );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void assign_fixed_muldiv( struct codegen* codegen,
   struct assign* assign, struct result* result ) {
   struct result lside;
   init_result( &lside, EXPECTING_SPACE );
   visit_operand( codegen, &lside, assign->lside );
   switch ( lside.status ) {
   case R_VAR:
      assign_fixed_muldiv_var( codegen, assign, &lside, result );
      break;
   case R_ARRAYINDEX:
      assign_fixed_muldiv_array( codegen, assign, &lside, result );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void assign_fixed_muldiv_var( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result ) {
   push_indexed( codegen, lside->storage, lside->index );
   push_operand( codegen, assign->rside );
   if ( assign->op == AOP_DIV ) {
      c_pcd( codegen, PCD_FIXEDDIV );
   }
   else {
      c_pcd( codegen, PCD_FIXEDMUL );
   }
   if ( result->push ) {
      c_pcd( codegen, PCD_DUP );
   }
   c_update_indexed( codegen, lside->storage, lside->index, AOP_NONE );
   if ( result->push ) {
      result->status = R_VALUE;
   }
}

static void assign_fixed_muldiv_array( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result ) {
   if ( result->push ) {
      c_pcd( codegen, PCD_DUP );
   }
   c_pcd( codegen, PCD_DUP );
   c_push_element( codegen, lside->storage, lside->index );
   push_operand( codegen, assign->rside );
   if ( assign->op == AOP_DIV ) {
      c_pcd( codegen, PCD_FIXEDDIV );
   }
   else {
      c_pcd( codegen, PCD_FIXEDMUL );
   }
   c_update_element( codegen, lside->storage, lside->index, AOP_NONE );
   if ( result->push ) {
      c_push_element( codegen, lside->storage, lside->index );
      result->status = R_VALUE;
   }
}

static void assign_str( struct codegen* codegen, struct assign* assign,
   struct result* result ) {
   switch ( assign->op ) {
   case AOP_NONE:
      assign_value( codegen, assign, result );
      break;
   case AOP_ADD:
      assign_str_add( codegen, assign, result );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void assign_str_add( struct codegen* codegen, struct assign* assign,
   struct result* result ) {
   struct result lside;
   init_result( &lside, EXPECTING_SPACE );
   visit_operand( codegen, &lside, assign->lside );
   switch ( lside.status ) {
   case R_VAR:
      assign_str_add_var( codegen, assign, &lside, result );
      break;
   case R_ARRAYINDEX:
      assign_str_add_array( codegen, assign, &lside, result );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void assign_str_add_var( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result ) {
   c_pcd( codegen, PCD_BEGINPRINT );
   push_indexed( codegen, lside->storage, lside->index );
   c_pcd( codegen, PCD_PRINTSTRING );
   push_operand( codegen, assign->rside );
   c_pcd( codegen, PCD_PRINTSTRING );
   c_pcd( codegen, PCD_SAVESTRING );
   if ( result->push ) {
      c_pcd( codegen, PCD_DUP );
      result->status = R_VALUE;
   }
   c_update_indexed( codegen, lside->storage, lside->index, AOP_NONE );
}

static void assign_str_add_array( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result ) {
   if ( result->push ) {
      c_pcd( codegen, PCD_DUP );
   }
   c_pcd( codegen, PCD_BEGINPRINT );
   c_pcd( codegen, PCD_DUP );
   c_push_element( codegen, lside->storage, lside->index );
   c_pcd( codegen, PCD_PRINTSTRING );
   push_operand( codegen, assign->rside );
   c_pcd( codegen, PCD_PRINTSTRING );
   c_pcd( codegen, PCD_SAVESTRING );
   c_update_element( codegen, lside->storage, lside->index, AOP_NONE );
   if ( result->push ) {
      c_push_element( codegen, lside->storage, lside->index );
      result->status = R_VALUE;
   }
}

static void assign_array_ref( struct codegen* codegen, struct assign* assign,
   struct result* result ) {
   struct result lside;
   init_result( &lside, EXPECTING_SPACE );
   visit_operand( codegen, &lside, assign->lside );
   switch ( lside.status ) {
   case R_VAR:
      assign_array_ref_var( codegen, assign, &lside, result );
      break;
   case R_ARRAYINDEX:
      assign_array_ref_array( codegen, assign, &lside, result );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void assign_array_ref_var( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result ) {
   // Push array offset.
   struct result rside;
   init_result( &rside, EXPECTING_VALUE );
   visit_operand( codegen, &rside, assign->rside );
   if ( result->push ) {
      c_pcd( codegen, PCD_DUP );
   }
   // Push dimension information offset.
   if ( rside.null ) {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   }
   else {
      if ( rside.dim ) {
         c_pcd( codegen, PCD_PUSHNUMBER, rside.diminfo_start );
      }
      else {
         c_push_dimtrack( codegen );
      }
   }
   c_update_indexed( codegen, lside->storage, lside->index + 1, AOP_NONE );
   c_update_indexed( codegen, lside->storage, lside->index, AOP_NONE );
   if ( result->push ) {
      result->ref = rside.ref;
      result->structure = rside.structure;
      result->dim = rside.dim;
      result->diminfo_start = rside.diminfo_start;
      result->storage = rside.storage;
      result->index = rside.index;
      result->status = R_ARRAYINDEX;
   }
}

static void assign_array_ref_array( struct codegen* codegen,
   struct assign* assign, struct result* lside, struct result* result ) {
   if ( result->push ) {
      c_pcd( codegen, PCD_DUP );
   }
   c_pcd( codegen, PCD_DUP );
   // Copy array offset.
   struct result rside;
   init_result( &rside, EXPECTING_VALUE );
   visit_operand( codegen, &rside, assign->rside );
   c_update_element( codegen, lside->storage, lside->index, AOP_NONE );
   // Copy dimension information offset.
   c_pcd( codegen, PCD_PUSHNUMBER, 1 );
   c_pcd( codegen, PCD_ADD );
   if ( rside.null ) {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   }
   else {
      if ( rside.dim ) {
         c_pcd( codegen, PCD_PUSHNUMBER, rside.diminfo_start );
      }
      else {
         c_push_dimtrack( codegen );
      }
   }
   c_update_element( codegen, lside->storage, lside->index, AOP_NONE );
   if ( result->push ) {
      push_element( codegen, lside->storage, lside->index );
      result->ref = rside.ref;
      result->dim = rside.dim;
      result->diminfo_start = rside.diminfo_start;
      result->storage = rside.storage;
      result->index = rside.index;
      result->status = R_ARRAYINDEX;
   }
}

void c_init_local_var( struct codegen* codegen, struct var* var ) {
   struct result rside;
   init_result( &rside, true );
   push_operand_result( codegen, &rside, var->value->expr->root );
   // Copy dimension information offset.
   if ( var->ref && var->ref->type == REF_ARRAY ) {
      if ( rside.null ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      }
      else {
         if ( rside.dim ) {
            c_pcd( codegen, PCD_PUSHNUMBER, rside.diminfo_start );
         }
         else {
            c_push_dimtrack( codegen );
         }
      }
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, var->index + 1 );
   }
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, var->index );
}

static void visit_conditional( struct codegen* codegen, struct result* result,
   struct conditional* cond ) {
   if ( cond->middle ) {
      write_conditional( codegen, result, cond );
   }
   else {
      write_middleless_conditional( codegen, result, cond );
   }
}

static void write_conditional( struct codegen* codegen, struct result* result,
   struct conditional* cond ) {
   // Condition.
   push_logical_operand( codegen, cond->left, cond->left_spec );
   struct c_jump* right_jump = c_create_jump( codegen, PCD_IFNOTGOTO );
   c_append_node( codegen, &right_jump->node );
   // When condition is true.
   struct result middle;
   init_result( &middle, true );
   push_operand_result( codegen, &middle, cond->middle );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &exit_jump->node );
   // When condition is not true.
   struct c_point* right_point = c_create_point( codegen );
   c_append_node( codegen, &right_point->node );
   right_jump->point = right_point;
   struct result right;
   init_result( &right, true );
   push_operand_result( codegen, &right, cond->right );
   // Done.
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
   struct result* operand = ( middle.null ) ? &right : &middle;
   result->ref = cond->ref;
   result->structure = operand->structure;
   result->storage = operand->storage;
   result->index = operand->index;
   result->status = operand->status;
}

static void write_middleless_conditional( struct codegen* codegen,
   struct result* result, struct conditional* cond ) {
   // Condition and left operand.
   struct result left;
   init_result( &left, true );
   push_operand_result( codegen, &left, cond->left );
   c_pcd( codegen, PCD_DUP );
   convert_to_boolean( codegen, &left, cond->left_spec );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &exit_jump->node );
   c_pcd( codegen, PCD_DROP );
   // Right operand.
   struct result right;
   init_result( &right, true );
   push_operand_result( codegen, &right, cond->right );
   // Done.
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
   struct result* operand = ( left.null ) ? &right : &left;
   result->ref = cond->ref;
   result->structure = operand->structure;
   result->storage = operand->storage;
   result->index = operand->index;
   result->status = operand->status;
}

static void visit_prefix( struct codegen* codegen, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_UNARY:
      visit_unary( codegen, result,
         ( struct unary* ) node );
      break;
   case NODE_INC:
      visit_inc( codegen, result,
         ( struct inc* ) node );
      break;
   case NODE_CAST:
      visit_cast( codegen, result,
         ( struct cast* ) node );
      break;
   default:
      visit_suffix( codegen, result, node );
      break;
   }
}

static void visit_unary( struct codegen* codegen, struct result* result,
   struct unary* unary ) {
   if ( unary->op == UOP_LOG_NOT ) {
      write_logical_not( codegen, result, unary );
   }
   else {
      write_unary( codegen, result, unary );
   }
}

static void write_logical_not( struct codegen* codegen, struct result* result,
   struct unary* unary ) {
   push_logical_operand( codegen, unary->operand, unary->operand_spec );
   c_pcd( codegen, PCD_NEGATELOGICAL );
   result->status = R_VALUE;
}

static void write_unary( struct codegen* codegen, struct result* result,
   struct unary* unary ) {
   push_operand( codegen, unary->operand );
   switch ( unary->op ) {
   case UOP_MINUS:
      c_pcd( codegen, PCD_UNARYMINUS );
      break;
   case UOP_BIT_NOT:
      c_pcd( codegen, PCD_NEGATEBINARY );
      break;
   case UOP_PLUS:
      // Unary plus is ignored.
      break;
   default:
      C_UNREACHABLE( codegen );
   }
   result->status = R_VALUE;
}

static void visit_inc( struct codegen* codegen, struct result* result,
   struct inc* inc ) {
   struct result operand;
   init_result( &operand, false );
   visit_operand( codegen, &operand, inc->operand );
   if ( operand.status == R_ARRAYINDEX ) {
      inc_array( codegen, result, inc, &operand );
   }
   else {
      inc_var( codegen, result, inc, &operand );
   }
}

static void inc_array( struct codegen* codegen, struct result* result,
   struct inc* inc, struct result* operand ) {
   if ( result->push ) {
      c_pcd( codegen, PCD_DUP );
      if ( inc->post ) {
         push_element( codegen, operand->storage, operand->index );
         c_pcd( codegen, PCD_SWAP );
         result->status = R_VALUE;
      }
   }
   if ( inc->fixed ) {
      inc_fixed( codegen, inc, operand );
   }
   else {
      inc_element( codegen, operand->storage, operand->index, ( ! inc->dec ) );
   }
   if ( ! inc->post && result->push ) {
      push_element( codegen, operand->storage, operand->index );
      result->status = R_VALUE;
   }
}

static void inc_element( struct codegen* codegen, int storage, int index,
   bool do_inc ) {
   int code = PCD_INCSCRIPTARRAY;
   if ( do_inc ) {
      switch ( storage ) {
      case STORAGE_MAP:
         code = PCD_INCMAPARRAY;
         break;
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
      case STORAGE_MAP:
         code = PCD_DECMAPARRAY;
         break;
      case STORAGE_WORLD:
         code = PCD_DECWORLDARRAY;
         break;
      case STORAGE_GLOBAL:
         code = PCD_DECGLOBALARRAY;
         break;
      default:
         code = PCD_DECSCRIPTARRAY;
         break;
      }
   }
   c_pcd( codegen, code, index );
}

static void inc_var( struct codegen* codegen, struct result* result,
   struct inc* inc, struct result* operand ) {
   if ( inc->post && result->push ) {
      push_indexed( codegen, operand->storage, operand->index );
      result->status = R_VALUE;
   }
   if ( inc->fixed ) {
      inc_fixed( codegen, inc, operand );
   }
   else {
      inc_indexed( codegen, operand->storage, operand->index, ( ! inc->dec ) );
   }
   if ( ! inc->post && result->push ) {
      push_indexed( codegen, operand->storage, operand->index );
      result->status = R_VALUE;
   }
}

static void inc_indexed( struct codegen* codegen, int storage, int index,
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
   c_pcd( codegen, code, index );
}

static void inc_fixed( struct codegen* codegen, struct inc* inc,
   struct result* operand ) {
   enum { FIXED_WHOLE = 65536 };
   c_pcd( codegen, PCD_PUSHNUMBER, FIXED_WHOLE );
   if ( operand->status == R_ARRAYINDEX ) {
      c_update_element( codegen, operand->storage, operand->index,
         ( inc->dec ? AOP_SUB : AOP_ADD ) );
   }
   else {
      c_update_indexed( codegen, operand->storage, operand->index,
         ( inc->dec ? AOP_SUB : AOP_ADD ) );
   }
}

static void visit_cast( struct codegen* codegen, struct result* result,
   struct cast* cast ) {
   visit_operand( codegen, result, cast->operand );
}

static void visit_suffix( struct codegen* codegen, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_SUBSCRIPT:
      visit_subscript( codegen, result,
         ( struct subscript* ) node );
      break;
   case NODE_ACCESS:
      visit_access( codegen, result,
         ( struct access* ) node );
      break;
   case NODE_CALL:
      visit_call( codegen, result,
         ( struct call* ) node );
      break;
   case NODE_SURE:
      visit_sure( codegen, result,
         ( struct sure* ) node );
      break;
   default:
      visit_primary( codegen, result,
         node );
      break;
   }
}

static void visit_subscript( struct codegen* codegen, struct result* result,
   struct subscript* subscript ) {
   struct result lside;
   init_result( &lside, true );
   visit_suffix( codegen, &lside, subscript->lside );
   if ( lside.dim ) {
      subscript_array( codegen, subscript, &lside, result );
   }
   else if ( lside.ref && lside.ref->type == REF_ARRAY ) {
      subscript_array_reference( codegen, subscript, &lside, result );
   }
   else if ( subscript->string ) {
      subscript_str( codegen, subscript, result );
   }
   else {
      C_UNREACHABLE( codegen );
   }
}

static void subscript_array( struct codegen* codegen,
   struct subscript* subscript, struct result* lside, struct result* result ) {
   c_push_expr( codegen, subscript->index );
   if ( lside->dim->element_size > 1 ) {
      c_pcd( codegen, PCD_PUSHNUMBER, lside->dim->element_size );
      c_pcd( codegen, PCD_MULTIPLY );
   }
   if ( lside->status == R_ARRAYINDEX ) {
      c_pcd( codegen, PCD_ADD );
   }
   // Sub-array element.
   if ( lside->dim->next ) {
      result->ref = lside->ref;
      result->structure = lside->structure;
      result->dim = lside->dim->next;
      // For an array that is passed by reference, move to the dimension
      // information of the sub-array.
      if ( has_dim_info( lside ) ) {
         result->diminfo_start = lside->diminfo_start + 1;
      }
      result->storage = lside->storage;
      result->index = lside->index;
      result->status = R_ARRAYINDEX;
      result->direct = lside->direct;
   }
   // Reference element.
   else if ( lside->ref ) {
      result->ref = lside->ref;
      result->structure = lside->structure;
      if ( result->push ) {
         if ( result->ref->type == REF_ARRAY ) {
            copy_diminfo( codegen, lside );
         }
         push_element( codegen, lside->storage, lside->index );
         result->storage = STORAGE_MAP;
         result->index = codegen->shary.index;
      }
      else {
         result->storage = lside->storage;
         result->index = lside->index;
      }
      result->status = R_ARRAYINDEX;
   }
   // Structure element.
   else if ( lside->structure ) {
      result->structure = lside->structure;
      result->storage = lside->storage;
      result->index = lside->index;
      result->status = R_ARRAYINDEX;
      result->direct = lside->direct;
   }
   // Primitive element.
   else {
      if ( result->push ) {
         push_element( codegen, lside->storage, lside->index );
         result->status = R_VALUE;
      }
      else {
         result->storage = lside->storage;
         result->index = lside->index;
         result->status = R_ARRAYINDEX;
      }
   }
}

static void copy_diminfo( struct codegen* codegen, struct result* lside ) {
   c_pcd( codegen, PCD_DUP );
   c_pcd( codegen, PCD_PUSHNUMBER, 1 );
   c_pcd( codegen, PCD_ADD );
   push_element( codegen, lside->storage, lside->index );
   c_update_dimtrack( codegen );
}

static void subscript_array_reference( struct codegen* codegen,
   struct subscript* subscript, struct result* lside, struct result* result ) {
   if ( lside->ref_dim == 0 ) {
      struct ref_array* array = ( struct ref_array* ) lside->ref;
      lside->ref_dim = array->dim_count;
      if ( array->ref.nullable && ! lside->safe ) {
         write_null_check( codegen ); 
      }
   }
   // Calculate offset to element.
   c_push_expr( codegen, subscript->index );
   if ( lside->ref_dim > 1 ) {
      next_diminfo( codegen );
      push_diminfo( codegen );
      c_pcd( codegen, PCD_MULTIPLY );
   }
   else if ( lside->ref->next ) {
      if ( lside->ref->next->type == REF_ARRAY ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 2 );
         c_pcd( codegen, PCD_MULTIPLY );
      }
   }
   else if ( lside->structure ) {
      c_pcd( codegen, PCD_PUSHNUMBER, lside->structure->size );
      c_pcd( codegen, PCD_MULTIPLY );
   }
   if ( lside->status == R_ARRAYINDEX ) {
      c_pcd( codegen, PCD_ADD );
   }
   // Sub-array element.
   if ( lside->ref_dim > 1 ) {
      result->ref = lside->ref;
      result->structure = lside->structure;
      result->storage = lside->storage;
      result->index = lside->index;
      result->ref_dim = lside->ref_dim - 1;
      result->status = R_ARRAYINDEX;
   }
   // Reference element.
   else if ( lside->ref->next ) {
      result->ref = lside->ref->next;
      result->structure = lside->structure;
      if ( result->push ) {
         if ( result->ref->type == REF_ARRAY ) {
            copy_diminfo( codegen, lside );
         }
         push_element( codegen, lside->storage, lside->index );
         result->storage = STORAGE_MAP;
         result->index = codegen->shary.index;
      }
      else {
         result->storage = lside->storage;
         result->index = lside->index;
      }
      result->status = R_ARRAYINDEX;
   }
   // Structure element.
   else if ( lside->structure ) {
      result->structure = lside->structure;
      result->storage = lside->storage;
      result->index = lside->index;
      result->status = R_ARRAYINDEX;
   }
   // Primitive element.
   else {
      if ( result->push ) {
         push_element( codegen, lside->storage, lside->index );
         result->status = R_VALUE;
      }
      else {
         result->storage = lside->storage;
         result->index = lside->index;
         result->status = R_ARRAYINDEX;
      }
   }
}

static void subscript_str( struct codegen* codegen,
   struct subscript* subscript, struct result* result ) {
   c_push_expr( codegen, subscript->index );
   c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_GETCHAR );
   result->status = R_VALUE;
}

static void visit_access( struct codegen* codegen, struct result* result,
   struct access* access ) {
   switch ( access->type ) {
      struct result lside;
   case ACCESS_STRUCTURE:
   case ACCESS_ARRAY:
      init_result( &lside, true );
      visit_suffix( codegen, &lside, access->lside );
      if ( ! lside.dim && lside.ref && lside.ref->nullable && ! lside.safe ) {
         write_null_check( codegen );
      }
      switch ( access->rside->type ) {
      case NODE_STRUCTURE_MEMBER:
         access_structure_member( codegen,
            ( struct structure_member* ) access->rside, &lside, result );
         break;
      case NODE_FUNC:
         *result = lside;
         break;
      default:
         C_UNREACHABLE( codegen );
      }
      break;
   case ACCESS_NAMESPACE:
      switch ( access->rside->type ) {
      case NODE_CONSTANT:
         visit_constant( codegen, result,
            ( struct constant* ) access->rside );
         break;
      case NODE_ENUMERATOR:
         visit_enumerator( codegen, result,
            ( struct enumerator* ) access->rside );
         break;
      case NODE_VAR:
         visit_var( codegen, result,
            ( struct var* ) access->rside );
         break;
      case NODE_FUNC:
         visit_func( codegen, result,
            ( struct func* ) access->rside );
         break;
      case NODE_INDEXED_STRING_USAGE:
         visit_indexed_string_usage( codegen, result,
            ( struct indexed_string_usage* ) access->rside );
         break;
      case NODE_NAMESPACE:
         break;
      default:
         C_UNREACHABLE( codegen );
      }
      break;
   case ACCESS_STR:
      visit_suffix( codegen, result, access->lside );
   }
}

static void access_structure_member( struct codegen* codegen,
   struct structure_member* member, struct result* lside,
   struct result* result ) {
   if ( lside->status == R_ARRAYINDEX ) {
      // Adding a zero doesn't change the final offset.
      if ( member->offset > 0 ) {
         c_pcd( codegen, PCD_PUSHNUMBER, member->offset );
         c_pcd( codegen, PCD_ADD );
      }
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, member->offset );
   }
   // Array member.
   if ( member->dim ) {
      result->ref = member->ref;
      result->structure = member->structure;
      result->dim = member->dim;
      result->diminfo_start = member->diminfo_start;
      result->storage = lside->storage;
      result->index = lside->index;
      result->status = R_ARRAYINDEX;
      result->direct = lside->direct;
   }
   // Reference member.
   else if ( member->ref ) {
      result->ref = member->ref;
      result->structure = member->structure;
      if ( result->push ) {
         if ( result->ref->type == REF_ARRAY ) {
            copy_diminfo( codegen, lside );
         }
         push_element( codegen, lside->storage, lside->index );
         result->storage = STORAGE_MAP;
         result->index = codegen->shary.index;
      }
      else {
         result->storage = lside->storage;
         result->index = lside->index;
      }
      result->status = R_ARRAYINDEX;
   }
   // Structure member.
   else if ( member->structure ) {
      result->structure = member->structure;
      result->storage = lside->storage;
      result->index = lside->index;
      result->status = R_ARRAYINDEX;
      result->direct = lside->direct;
   }
   // Primitive member.
   else {
      if ( result->push ) {
         push_element( codegen, lside->storage, lside->index );
         result->status = R_VALUE;
      }
      else {
         result->storage = lside->storage;
         result->index = lside->index;
         result->status = R_ARRAYINDEX;
      }
   }
}

static void visit_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   switch ( call->func->type ) {
   case FUNC_ASPEC:
      visit_aspec_call( codegen, result, call );
      break;
   case FUNC_EXT:
      visit_ext_call( codegen, result, call );
      break;
   case FUNC_DED:
      visit_ded_call( codegen, result, call );
      break;
   case FUNC_FORMAT:
      visit_format_call( codegen, result, call );
      break;
   case FUNC_USER:
      visit_user_call( codegen, result, call );
      break;
   case FUNC_SAMPLE:
      visit_sample_call( codegen, result, call );
      break;
   case FUNC_INTERNAL:
      visit_internal_call( codegen, result, call );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void visit_aspec_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   // Determine the amount of arguments to push. Skip pushing leading arguments
   // that are zero because the engine will provide those.
   int count = 0;
   int arg_number = 0;
   struct list_iter i;
   list_iterate( &call->args, &i );
   while ( ! list_end( &i ) ) {
      struct expr* arg = list_data( &i );
      bool skippable_arg = (
         arg->folded &&
         arg->value == 0 &&
         arg->has_str == false );
      if ( ! skippable_arg ) {
         count = arg_number + 1;
      }
      ++arg_number;
      list_next( &i );
   }
   // Push arguments.
   arg_number = 0;
   list_iterate( &call->args, &i );
   while ( ! list_end( &i ) && arg_number < count ) {
      c_push_expr( codegen, list_data( &i ) );
      ++arg_number;
      list_next( &i );
   }
   // Call action special.
   struct func_aspec* aspec = call->func->impl;
   if ( aspec->id >= EXTENDEDASPEC_STARTID ) {
      while ( count < 5 ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         ++count;
      }
      if ( result->push ) {
         c_pcd( codegen, PCD_LSPEC5EXRESULT, aspec->id );
         result->status = R_VALUE;
      }
      else {
         c_pcd( codegen, PCD_LSPEC5EX, aspec->id );
      }
   }
   else {
      if ( result->push ) {
         while ( count < 5 ) {
            c_pcd( codegen, PCD_PUSHNUMBER, 0 );
            ++count;
         }
         c_pcd( codegen, PCD_LSPEC5RESULT, aspec->id );
         result->status = R_VALUE;
      }
      else if ( count > 0 ) {
         c_pcd( codegen, g_aspec_code[ count - 1 ], aspec->id );
      }
      else {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         c_pcd( codegen, PCD_LSPEC1, aspec->id );
      }
   }
}

static void visit_ext_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   struct list_iter i;
   list_iterate( &call->args, &i );
   while ( ! list_end( &i ) ) {
      c_push_expr( codegen, list_data( &i ) );
      list_next( &i );
   }
   struct func_ext* impl = call->func->impl;
   c_pcd( codegen, PCD_CALLFUNC, list_size( &call->args ), impl->id );
   result->status = R_VALUE;
}

static void visit_ded_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   // Push arguments.
   struct param* param = call->func->params;
   struct list_iter i;
   list_iterate( &call->args, &i );
   while ( ! list_end( &i ) ) {
      c_push_expr( codegen, list_data( &i ) );
      if ( param ) {
         param = param->next;
      }
      list_next( &i );
   }
   // Push default arguments.
   int count = list_size( &call->args );
   while ( count < call->func->max_param ) {
      if ( param ) {
         c_push_expr( codegen, param->default_value );
         param = param->next;
      }
      else {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      }
      ++count;
   }
   struct func_ded* ded = call->func->impl;
   c_pcd( codegen, ded->opcode );
   if ( call->func->return_spec != SPEC_VOID ) {
      result->status = R_VALUE;
   }
}

static void visit_user_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   struct func_user* impl = call->func->impl;
   if ( impl->local ) {
      call_local_user_func( codegen, result, call );
   }
   else {
      call_user_func( codegen, result, call );
   }
}

static void call_local_user_func( struct codegen* codegen,
   struct result* result, struct call* call ) {
   // Push ID of entry to identify return address.
   c_pcd( codegen, PCD_PUSHNUMBER, call->nested_call->id );
   write_call_args( codegen, call );
   struct c_jump* prologue_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &prologue_jump->node );
   call->nested_call->prologue_jump = prologue_jump;
   struct c_point* return_point = c_create_point( codegen );
   c_append_node( codegen, &return_point->node );
   call->nested_call->return_point = return_point;
   if ( call->func->return_spec != SPEC_VOID ) {
      set_user_func_call_result( codegen, call, result );
   }
}

static void call_user_func( struct codegen* codegen, struct result* result,
   struct call* call ) {
   write_call_args( codegen, call );
   struct func_user* impl = call->func->impl;
   if ( call->func->return_spec != SPEC_VOID && result->push ) {
      c_pcd( codegen, PCD_CALL, impl->index );
      set_user_func_call_result( codegen, call, result );
   }
   else {
      c_pcd( codegen, PCD_CALLDISCARD, impl->index );
   }
}

static void write_call_args( struct codegen* codegen, struct call* call ) {
   struct param* param = call->func->params;
   // Push arguments.
   struct list_iter i;
   list_iterate( &call->args, &i );
   while ( ! list_end( &i ) ) {
      push_arg( codegen, param, list_data( &i ) );
      param = param->next;
      list_next( &i );
   }
   // Push default arguments.
   while ( param ) {
      push_arg( codegen, param, param->default_value );
      param = param->next;
   }
}

static void push_arg( struct codegen* codegen, struct param* param,
   struct expr* expr ) {
   struct result arg;
   init_result( &arg, true );
   visit_operand( codegen, &arg, expr->root );
   if ( arg.dim ) {
      c_pcd( codegen, PCD_PUSHNUMBER, arg.diminfo_start );
   }
   else if ( arg.ref && arg.ref->type == REF_ARRAY ) {
      c_push_dimtrack( codegen );
   }
   else if ( arg.null ) {
      // Dimension information.
      if ( param->ref && param->ref->type == REF_ARRAY ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      }
   }
}

static void set_user_func_call_result( struct codegen* codegen,
   struct call* call, struct result* result ) {
   // Reference-type result.
   if ( call->func->ref ) {
      result->ref = call->func->ref;
      result->structure = call->func->structure;
      switch ( result->ref->type ) {
      case REF_STRUCTURE:
      case REF_ARRAY:
         result->storage = STORAGE_MAP;
         result->index = codegen->shary.index;
         result->status = R_ARRAYINDEX;
         break;
      case REF_FUNCTION:
         result->status = R_VALUE;
         break;
      default:
         C_UNREACHABLE( codegen );
      }
   }
   // Primitive-type result.
   else {
      result->status = R_VALUE;
   }
}

static void visit_sample_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   struct result operand;
   init_result( &operand, true );
   visit_suffix( codegen, &operand, call->operand );
   // Null check.
   if ( codegen->task->library_main->header && operand.ref->nullable &&
      ! operand.safe ) {
      write_null_check( codegen );
   }
   // Push arguments.
   if ( list_size( &call->args ) > 0 ) {
      int caller = c_alloc_script_var( codegen );
      c_update_indexed( codegen, STORAGE_LOCAL, caller, AOP_NONE );
      struct list_iter i;
      list_iterate( &call->args, &i );
      struct param* param = call->ref_func->params;
      while ( ! list_end( &i ) ) {
         push_arg( codegen, param, list_data( &i ) );
         param = param->next;
         list_next( &i );
      }
      push_indexed( codegen, STORAGE_LOCAL, caller );
      c_dealloc_last_script_var( codegen );
   }
   c_pcd( codegen, PCD_CALLSTACK );
   // Reference-type result. 
   if ( operand.ref->next ) {
      result->ref = operand.ref->next;
      result->structure = operand.structure;
      switch ( result->ref->type ) {
      case REF_STRUCTURE:
      case REF_ARRAY:
         result->storage = STORAGE_MAP;
         result->index = codegen->shary.index;
         result->status = R_ARRAYINDEX;
         break;
      case REF_FUNCTION:
         result->status = R_VALUE;
         break;
      default:
         C_UNREACHABLE( codegen );
      }
   }
   // Primitive-type result.
   else {
      result->status = R_VALUE;
   }
}

static void visit_format_call( struct codegen* codegen, struct result* result,
   struct call* call ) {
   if ( call->func == codegen->task->append_func ) {
      visit_format_item( codegen, call->format_item );
   }
   else {
      call_format( codegen, result, call );
   }
}

static void call_format( struct codegen* codegen, struct result* result,
   struct call* call ) {
   c_pcd( codegen, PCD_BEGINPRINT );
   visit_format_item( codegen, call->format_item );
   if ( call->func->min_param > 0 ) {
      c_pcd( codegen, PCD_MOREHUDMESSAGE );
      int count = 0;
      struct list_iter i;
      list_iterate( &call->args, &i );
      while ( ! list_end( &i ) ) {
         if ( count == call->func->min_param ) {
            c_pcd( codegen, PCD_OPTHUDMESSAGE );
         }
         c_push_expr( codegen, list_data( &i ) );
         ++count;
         list_next( &i );
      }
   }
   struct func_format* format = call->func->impl;
   c_pcd( codegen, format->opcode );
   if ( call->func->return_spec != SPEC_VOID ) {
      result->status = R_VALUE;
   }
}

static void visit_format_item( struct codegen* codegen,
   struct format_item* item ) {
   while ( item ) {
      if ( item->cast == FCAST_ARRAY ) {
         visit_array_format_item( codegen, item );
      }
      else if ( item->cast == FCAST_BUILDMSG ) {
         visit_msgbuild_format_item( codegen, item );
      }
      else {
         static const int casts[] = {
            PCD_PRINTBINARY,
            PCD_PRINTCHARACTER,
            PCD_PRINTNUMBER,
            PCD_PRINTFIXED,
            PCD_PRINTNUMBER,
            PCD_PRINTBIND,
            PCD_PRINTLOCALIZED,
            PCD_PRINTNAME,
            PCD_PRINTSTRING,
            PCD_PRINTHEX };
         STATIC_ASSERT( FCAST_TOTAL == 12 );
         c_push_expr( codegen, item->value );
         c_pcd( codegen, casts[ item->cast - 1 ] );
      }
      item = item->next;
   }
}

static void visit_array_format_item( struct codegen* codegen,
   struct format_item* item ) {
   struct result object;
   init_result( &object, true );
   visit_operand( codegen, &object, item->value->root );
   if ( object.status != R_ARRAYINDEX ) {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   }
   c_pcd( codegen, PCD_PUSHNUMBER, object.index );
   if ( item->extra ) {
      struct format_item_array* extra = item->extra;
      c_push_expr( codegen, extra->offset );
      if ( extra->length ) {
         c_push_expr( codegen, extra->length );
      }
      else {
         enum { MAX_LENGTH = INT_MAX };
         c_pcd( codegen, PCD_PUSHNUMBER, MAX_LENGTH );
      }
   }
   int code = PCD_NONE;
   if ( item->extra ) {
      switch ( object.storage ) {
      case STORAGE_LOCAL:
         code = PCD_PRINTSCRIPTCHRANGE;
         break;
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
      case STORAGE_LOCAL:
         code = PCD_PRINTSCRIPTCHARARRAY;
         break;
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
   c_pcd( codegen, code );
}

static void visit_msgbuild_format_item( struct codegen* codegen,
   struct format_item* item ) {
   struct format_item_buildmsg* extra = item->extra;
   // Inline the message-building block if it only has a single user.
   if ( list_size( &extra->usage->buildmsg->usages ) == 1 ) {
      c_write_block( codegen, extra->usage->buildmsg->block );
   }
   else {
      struct c_point* jump_point = c_create_point( codegen );
      c_append_node( codegen, &jump_point->node );
      extra->usage->point = jump_point;
   }
}

static void visit_internal_call( struct codegen* codegen,
   struct result* result, struct call* call ) {
   struct func_intern* impl = call->func->impl;
   switch ( impl->id ) {
   case INTERN_FUNC_ACS_EXECWAIT:
   case INTERN_FUNC_ACS_NAMEDEXECUTEWAIT:
      write_executewait( codegen, call,
         ( impl->id == INTERN_FUNC_ACS_NAMEDEXECUTEWAIT ) );
      break;
   case INTERN_FUNC_STR_LENGTH:
      push_operand( codegen, call->operand );
      c_pcd( codegen, PCD_STRLEN );
      result->status = R_VALUE;
      break;
   case INTERN_FUNC_ARRAY_LENGTH:
      call_array_length( codegen, result, call );
      result->status = R_VALUE;
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void write_executewait( struct codegen* codegen, struct call* call,
   bool named_impl ) {
   struct list_iter i;
   list_iterate( &call->args, &i );
   c_push_expr( codegen, list_data( &i ) );
   c_pcd( codegen, PCD_DUP );
   list_next( &i );
   if ( ! list_end( &i ) ) {
      // Second argument to Acs_Execute is 0--the current map. Ignore what the
      // user specified.
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      list_next( &i );
      while ( ! list_end( &i ) ) {
         c_push_expr( codegen, list_data( &i ) );
         list_next( &i );
      }
   }
   if ( named_impl ) {
      c_pcd( codegen, PCD_CALLFUNC, list_size( &call->args ), 39 );
      c_pcd( codegen, PCD_DROP );
      c_pcd( codegen, PCD_SCRIPTWAITNAMED );
   }
   else {
      c_pcd( codegen, g_aspec_code[ list_size( &call->args ) - 1 ], 80 );
      c_pcd( codegen, PCD_SCRIPTWAIT );
   }
}

static void call_array_length( struct codegen* codegen, struct result* result,
   struct call* call ) {
   struct result operand;
   init_result( &operand, true );
   visit_operand( codegen, &operand, call->operand );
   if ( operand.status == R_ARRAYINDEX ) {
      c_pcd( codegen, PCD_DROP );
   }
   push_array_length( codegen, &operand, false );
}

static void visit_sure( struct codegen* codegen, struct result* result,
   struct sure* sure ) {
   struct result operand;
   init_result( &operand, true );
   visit_suffix( codegen, &operand, sure->operand );
   if ( ! sure->already_safe ) {
      write_null_check( codegen );
   }
   result->ref = sure->ref;
   result->dim = operand.dim;
   result->diminfo_start = operand.diminfo_start;
   result->structure = operand.structure;
   result->storage = operand.storage;
   result->index = operand.index;
   result->status = operand.status;
   result->safe = true;
   result->direct = operand.direct;
}

static bool is_null_check_needed( struct result* result ) {
   switch ( describe_result( result ) ) {
   case RESULTDESC_REF:
      return ( result->ref->nullable && ! result->safe );
   default:
      return false;
   }
}

static void write_null_check( struct codegen* codegen ) {
   if ( codegen->null_handler ) {
      struct func_user* impl = codegen->null_handler->impl;
      c_pcd( codegen, PCD_CASEGOTO, NULLVALUE, impl->obj_pos );
   }
   else {
      C_INTERNAL_ERR( codegen,
         "asked to write a null check, but the null handler is not "
         "allocated" );
      c_bail( codegen );
   }
}

static void visit_primary( struct codegen* codegen, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_LITERAL:
      visit_literal( codegen, result,
         ( struct literal* ) node );
      break;
   case NODE_FIXED_LITERAL:
      visit_fixed_literal( codegen, result,
         ( struct fixed_literal* ) node );
      break;
   case NODE_INDEXED_STRING_USAGE:
      visit_indexed_string_usage( codegen, result,
         ( struct indexed_string_usage* ) node );
      break;
   case NODE_BOOLEAN:
      visit_boolean( codegen, result,
         ( struct boolean* ) node );
      break;
   case NODE_NAME_USAGE:
      visit_name_usage( codegen, result,
         ( struct name_usage* ) node );
      break;
   case NODE_QUALIFIEDNAMEUSAGE:
      visit_qualified_name_usage( codegen, result,
         ( struct qualified_name_usage* ) node );
      break;
   case NODE_STRCPY:
      visit_strcpy( codegen, result,
         ( struct strcpy_call* ) node );
      break;
   case NODE_MEMCPY:
      visit_memcpy( codegen, result,
         ( struct memcpy_call* ) node );
      break;
   case NODE_LENGTHOF:
      visit_lengthof( codegen, result,
         ( struct lengthof* ) node );
      break;
   case NODE_CONVERSION:
      visit_conversion( codegen, result,
         ( struct conversion* ) node );
      break;
   case NODE_NULL:
      visit_null( codegen, result );
      break;
   case NODE_PAREN:
      visit_paren( codegen, result,
         ( struct paren* ) node );
      break;
   case NODE_COMPOUNDLITERAL:
      visit_compound_literal( codegen, result,
         ( struct compound_literal* ) node );
      break;
   case NODE_FUNC:
      visit_func( codegen, result,
         ( struct func* ) node );
      break;
   case NODE_MAGICID:
      visit_magic_id( codegen, result,
         ( struct magic_id* ) node );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void visit_literal( struct codegen* codegen, struct result* result,
   struct literal* literal ) {
   c_pcd( codegen, PCD_PUSHNUMBER, literal->value );
   result->status = R_VALUE;
}

static void visit_fixed_literal( struct codegen* codegen,
   struct result* result, struct fixed_literal* literal ) {
   c_pcd( codegen, PCD_PUSHNUMBER, literal->value );
   result->status = R_VALUE;
}

static void visit_indexed_string_usage( struct codegen* codegen,
   struct result* result, struct indexed_string_usage* usage ) {
   visit_string( codegen, result, usage->string );
}

static void visit_string( struct codegen* codegen,
   struct result* result, struct indexed_string* string ) {
   c_push_string( codegen, string );
   result->status = R_VALUE;
}

void c_push_string( struct codegen* codegen,
   struct indexed_string* string ) {
   c_append_string( codegen, string );
   c_pcd( codegen, PCD_PUSHNUMBER, string->index_runtime );
   // Strings in a library need to be tagged.
   if ( codegen->task->library_main->importable ) {
      c_pcd( codegen, PCD_TAGSTRING );
   }
   string->used = true;
}

static void visit_boolean( struct codegen* codegen, struct result* result,
   struct boolean* boolean ) {
   c_pcd( codegen, PCD_PUSHNUMBER, boolean->value );
   result->status = R_VALUE;
}

static void visit_name_usage( struct codegen* codegen, struct result* result,
   struct name_usage* usage ) {
   visit_general_name_usage( codegen, result, usage->object );
}

static void visit_qualified_name_usage( struct codegen* codegen,
   struct result* result, struct qualified_name_usage* usage ) {
   visit_general_name_usage( codegen, result, usage->object );
}

static void visit_general_name_usage( struct codegen* codegen,
   struct result* result, struct node* object ) {
   switch ( object->type ) {
   case NODE_CONSTANT:
      visit_constant( codegen, result,
         ( struct constant* ) object );
      break;
   case NODE_ENUMERATOR:
      visit_enumerator( codegen, result,
         ( struct enumerator* ) object );
      break;
   case NODE_VAR:
      visit_var( codegen, result,
         ( struct var* ) object );
      break;
   case NODE_PARAM:
      visit_param( codegen, result,
         ( struct param* ) object );
      break;
   case NODE_FUNC:
      visit_func( codegen, result,
         ( struct func* ) object );
      break;
   case NODE_INDEXED_STRING_USAGE:
      visit_indexed_string_usage( codegen, result,
         ( struct indexed_string_usage* ) object );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void visit_constant( struct codegen* codegen, struct result* result,
   struct constant* constant ) {
   struct indexed_string* string = constant->has_str ?
      t_lookup_string( codegen->task, constant->value ) : NULL;
   if ( string ) {
      c_push_string( codegen, string );
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, constant->value );
   }
   result->status = R_VALUE;
}

static void visit_enumerator( struct codegen* codegen, struct result* result,
   struct enumerator* enumerator ) {
   struct indexed_string* string = NULL;
   if ( enumerator->has_str ) {
      string = t_lookup_string( codegen->task, enumerator->value );
   }
   if ( string ) {
      c_push_string( codegen, string );
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, enumerator->value );
   }
   result->status = R_VALUE;
}

static void visit_var( struct codegen* codegen, struct result* result,
   struct var* var ) {
   // Array.
   if ( var->dim ) {
      result->ref = var->ref;
      result->structure = var->structure;
      result->dim = var->dim;
      result->diminfo_start = var->diminfo_start;
      if ( var->in_shared_array ) {
         c_pcd( codegen, PCD_PUSHNUMBER, var->index );
         result->storage = STORAGE_MAP;
         result->index = codegen->shary.index;
         result->status = R_ARRAYINDEX;
      }
      else {
         result->storage = var->storage;
         result->index = var->index;
         result->status = R_ARRAY;
      }
      result->direct = true;
   }
   // Reference variable.
   else if ( var->ref ) {
      result->ref = var->ref;
      result->structure = var->structure;
      if ( var->in_shared_array ) {
         visit_shary_ref_var( codegen, result, var );
      }
      else {
         visit_ref_var( codegen, result, var );
      }
   }
   // Structure variable.
   else if ( var->structure ) {
      result->structure = var->structure;
      if ( var->in_shared_array ) {
         c_pcd( codegen, PCD_PUSHNUMBER, var->index );
         result->storage = STORAGE_MAP;
         result->index = codegen->shary.index;
         result->status = R_ARRAYINDEX;
      }
      else {
         result->storage = var->storage;
         result->index = var->index;
      }
      result->direct = true;
   }
   // Primitive variable.
   else {
      if ( var->in_shared_array ) {
         c_pcd( codegen, PCD_PUSHNUMBER, var->index );
         if ( result->push ) {
            push_element( codegen, STORAGE_MAP, codegen->shary.index );
            result->status = R_VALUE;
         }
         else {
            result->storage = STORAGE_MAP;
            result->index = codegen->shary.index;
            result->status = R_ARRAYINDEX;
         }
      }
      else {
         if ( result->push ) {
            push_indexed( codegen, var->storage, var->index );
            result->status = R_VALUE;
         }
         else {
            result->storage = var->storage;
            result->index = var->index;
            result->status = R_VAR;
         }
      }
   }
}

static void visit_shary_ref_var( struct codegen* codegen,
   struct result* result, struct var* var ) {
   c_pcd( codegen, PCD_PUSHNUMBER, var->index );
   if ( result->push ) {
      if ( var->ref->type == REF_ARRAY ) {
         c_pcd( codegen, PCD_PUSHNUMBER, var->index + 1 );
         push_element( codegen, STORAGE_MAP, codegen->shary.index );
         c_update_dimtrack( codegen );
      }
      push_element( codegen, STORAGE_MAP, codegen->shary.index );
   }
   result->storage = STORAGE_MAP;
   result->index = codegen->shary.index;
   result->status = R_ARRAYINDEX;
}

static void visit_ref_var( struct codegen* codegen, struct result* result,
   struct var* var ) {
   if ( result->push ) {
      // A map variable holds array-reference information in an array.
      if ( var->storage == STORAGE_MAP && var->ref->type == REF_ARRAY ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         c_pcd( codegen, PCD_PUSHNUMBER, 1 );
         c_push_element( codegen, var->storage, var->index );
         c_update_dimtrack( codegen );
         c_push_element( codegen, var->storage, var->index );
      }
      else {
         push_indexed( codegen, var->storage, var->index );
         if ( var->ref->type == REF_ARRAY ) {
            push_indexed( codegen, var->storage, var->index + 1 );
            c_update_dimtrack( codegen );
         }
      }
      result->storage = STORAGE_MAP;
      result->index = codegen->shary.index;
      result->status = R_ARRAYINDEX;
   }
   else {
      result->storage = var->storage;
      result->index = var->index;
      // A map variable holds array-reference information in an array.
      if ( var->storage == STORAGE_MAP && var->ref->type == REF_ARRAY ) {
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         result->status = R_ARRAYINDEX;
      }
      else {
         result->status = R_VAR;
      }
   }
}

static void visit_param( struct codegen* codegen, struct result* result,
   struct param* param ) {
   // Reference parameter.
   if ( param->ref ) {
      result->ref = param->ref;
      result->structure = param->structure;
      if ( result->push ) {
         push_indexed( codegen, STORAGE_LOCAL, param->index );
         if ( param->ref->type == REF_ARRAY ) {
            push_indexed( codegen, STORAGE_LOCAL, param->index + 1 );
            c_update_dimtrack( codegen );
         }
         result->storage = STORAGE_MAP;
         result->index = codegen->shary.index;
         result->status = R_ARRAYINDEX;
      }
      else {
         result->storage = STORAGE_LOCAL;
         result->index = param->index;
         result->status = R_VAR;
      }
   }
   // Primitive parameter.
   else {
      if ( result->push ) {
         push_indexed( codegen, STORAGE_LOCAL, param->index );
         result->status = R_VALUE;
      }
      else {
         result->storage = STORAGE_LOCAL;
         result->index = param->index;
         result->status = R_VAR;
      }
   }
}

static void visit_func( struct codegen* codegen, struct result* result,
   struct func* func ) {
   if ( func->type == FUNC_USER ) {
      struct func_user* impl = func->impl;
      c_pcd( codegen, ( impl->local ) ?
         PCD_PUSHNUMBER : PCD_PUSHFUNCTION,
         impl->index );
      result->status = R_VALUE;
   }
   else if ( func->type == FUNC_ASPEC ) {
      struct func_aspec* impl = func->impl;
      c_pcd( codegen, PCD_PUSHNUMBER, impl->id );
   }
   else if ( func->type == FUNC_EXT ) {
      struct func_ext* impl = func->impl;
      c_pcd( codegen, PCD_PUSHNUMBER, -impl->id );
   }
}

static void visit_magic_id( struct codegen* codegen, struct result* result,
   struct magic_id* magic_id ) {
   visit_string( codegen, result, magic_id->string );
}

static void visit_strcpy( struct codegen* codegen, struct result* result,
   struct strcpy_call* call ) {
   // Push array offset. This is the start of the array.
   struct result array;
   init_result( &array, EXPECTING_VALUE );
   visit_operand( codegen, &array, call->array->root );
   switch ( array.status ) {
   case R_ARRAY:
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      break;
   case R_ARRAYINDEX:
      break;
   default:
      C_UNREACHABLE( codegen );
   }
   // Null check.
   if ( is_null_check_needed( &array ) ) {
      write_null_check( codegen );
   }
   // Push array index.
   c_pcd( codegen, PCD_PUSHNUMBER, array.index );
   // Push offset for the starting element. The string characters will be
   // stored in the array starting from this element.
   if ( call->array_offset ) {
      c_push_expr( codegen, call->array_offset );
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   }
   // Push number of characters to copy from the string.
   if ( call->array_length ) {
      c_push_expr( codegen, call->array_length );
   }
   else {
      enum { MAX_LENGTH = INT_MAX };
      c_pcd( codegen, PCD_PUSHNUMBER, MAX_LENGTH );
   }
   // Push string.
   c_push_expr( codegen, call->string );
   // Push substring offset. The string will be copied starting from this
   // offset.
   if ( call->offset ) {
      c_push_expr( codegen, call->offset );
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   }
   switch ( array.storage ) {
   case STORAGE_LOCAL:
      c_pcd( codegen, PCD_STRCPYTOSCRIPTCHRANGE );
      break;
   case STORAGE_MAP:
      c_pcd( codegen, PCD_STRCPYTOMAPCHRANGE );
      break;
   case STORAGE_WORLD:
      c_pcd( codegen, PCD_STRCPYTOWORLDCHRANGE );
      break;
   case STORAGE_GLOBAL:
      c_pcd( codegen, PCD_STRCPYTOGLOBALCHRANGE );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
   result->status = R_VALUE;
}

static void visit_memcpy( struct codegen* codegen, struct result* result,
   struct memcpy_call* call ) {
   switch ( call->type ) {
   case MEMCPY_ARRAY:
      copy_array( codegen, result, call );
      break;
   case MEMCPY_STRUCT:
      copy_struct( codegen, result, call );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void copy_array( struct codegen* codegen, struct result* result,
   struct memcpy_call* call ) {
   struct c_point* fail_point = c_create_point( codegen );
   struct c_jump* jump = NULL;
   int size = c_alloc_script_var( codegen );
   // Evaluate destination.
   int destination_offset = 0;
   bool destination_offset_allocated = false;
   struct result destination;
   init_result( &destination, true );
   visit_operand( codegen, &destination, call->destination->root );
   // Null check.
   if ( destination.ref && destination.ref->type == REF_ARRAY &&
      destination.ref->nullable && ! destination.safe ) {
      write_null_check( codegen );
   }
   if ( destination.status == R_ARRAYINDEX || call->destination_offset ) {
      destination_offset = c_alloc_script_var( codegen );
      destination_offset_allocated = true;
      if ( destination.status == R_ARRAYINDEX ) {
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, destination_offset );
      }
   }
   push_array_size( codegen, &destination );
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, size );
   // Evaluate destination-offset.
   if ( call->destination_offset ) {
      bool scale = push_offset_scale_factor( codegen, &destination );
      c_push_expr( codegen, call->destination_offset );
      if ( scale ) {
         c_pcd( codegen, PCD_MULTIPLY );
      }
      int offset = c_alloc_script_var( codegen );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, offset );
      if ( destination.status == R_ARRAYINDEX ) {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
         c_pcd( codegen, PCD_ADDSCRIPTVAR, destination_offset );
      }
      else {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, destination_offset );
      }
      // available-size = destination-size - destination-offset;
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
      c_pcd( codegen, PCD_SUBSCRIPTVAR, size );
      // Check: destination-offset >= 0 && available-size > 0
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_GE );
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_GT );
      c_pcd( codegen, PCD_ANDLOGICAL );
      jump = c_create_jump( codegen, PCD_IFNOTGOTO );
      c_append_node( codegen, &jump->node );
      jump->point = fail_point;
      c_dealloc_last_script_var( codegen );
      // Evaluate destination-length.
      if ( call->destination_length ) {
         bool scale = push_offset_scale_factor( codegen, &destination );
         c_push_expr( codegen, call->destination_length );
         if ( scale ) {
            c_pcd( codegen, PCD_MULTIPLY );
         }
         int length = c_alloc_script_var( codegen );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, length );
         // Check: destination-length > 0 && destination-length <= copy-size
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, length );
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         c_pcd( codegen, PCD_GT );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, length );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
         c_pcd( codegen, PCD_LE );
         c_pcd( codegen, PCD_ANDLOGICAL );
         jump = c_create_jump( codegen, PCD_IFNOTGOTO );
         c_append_node( codegen, &jump->node );
         jump->point = fail_point;
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, length );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, size );
         c_dealloc_last_script_var( codegen );
      }
   }
   // Evaluate source.
   int source_offset = 0;
   bool source_offset_allocated = false;
   struct result source;
   init_result( &source, true );
   visit_operand( codegen, &source, call->source->root );
   // Null check.
   if ( source.ref && source.ref->type == REF_ARRAY && source.ref->nullable &&
      ! source.safe ) {
      write_null_check( codegen );
   }
   if ( source.status == R_ARRAYINDEX || call->source_offset ) {
      source_offset = c_alloc_script_var( codegen );
      source_offset_allocated = true;
      if ( source.status == R_ARRAYINDEX ) {
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, source_offset );
      }
   }
   int source_size = c_alloc_script_var( codegen );
   push_array_size( codegen, &source );
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, source_size );
   // Evaluate source-offset.
   if ( call->source_offset ) {
      bool scale = push_offset_scale_factor( codegen, &source );
      c_push_expr( codegen, call->source_offset );
      if ( scale ) {
         c_pcd( codegen, PCD_MULTIPLY );
      }
      int offset = c_alloc_script_var( codegen );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, offset );
      if ( source.status == R_ARRAYINDEX ) {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
         c_pcd( codegen, PCD_ADDSCRIPTVAR, source_offset );
      }
      else {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, source_offset );
      }
      // Check: source-offset >= 0
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_GE );
      jump = c_create_jump( codegen, PCD_IFNOTGOTO );
      c_append_node( codegen, &jump->node );
      jump->point = fail_point;
      if ( call->destination_length ) {
         // Check: source-size - source-offset >= copy-size
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, source_size );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
         c_pcd( codegen, PCD_SUBTRACT );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
         c_pcd( codegen, PCD_GE );
         jump = c_create_jump( codegen, PCD_IFNOTGOTO );
         c_append_node( codegen, &jump->node );
         jump->point = fail_point;
      }
      else {
         // Check: source-size - source-offset > 0
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, source_size );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
         c_pcd( codegen, PCD_SUBTRACT );
         c_pcd( codegen, PCD_GE );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, source_size );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, offset );
         c_pcd( codegen, PCD_SUBTRACT );
         c_pcd( codegen, PCD_DUP );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, size );
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         c_pcd( codegen, PCD_GT );
         c_pcd( codegen, PCD_ANDLOGICAL );
         jump = c_create_jump( codegen, PCD_IFNOTGOTO );
         c_append_node( codegen, &jump->node );
         jump->point = fail_point;
      }
      c_dealloc_last_script_var( codegen );
   }
   else {
      if ( call->destination_length ) {
         // Check: source-size >= copy-size
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, source_size );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
         c_pcd( codegen, PCD_GE );
         jump = c_create_jump( codegen, PCD_IFNOTGOTO );
         c_append_node( codegen, &jump->node );
         jump->point = fail_point;
      }
      else {
         // Check: destination-size >= source-size
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, source_size );
         c_pcd( codegen, PCD_GE );
         jump = c_create_jump( codegen, PCD_IFNOTGOTO );
         c_append_node( codegen, &jump->node );
         jump->point = fail_point;
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, source_size );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, size );
      }
   }
   // Move the offsets to the end of the array, since the copying is done
   // backwards, from the end, to the beginning.
   if ( destination_offset_allocated ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
      c_pcd( codegen, PCD_ADDSCRIPTVAR, destination_offset );
   }
   if ( source_offset_allocated ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
      c_pcd( codegen, PCD_ADDSCRIPTVAR, source_offset );
   }
   // Copy elements.
   struct c_point* copy_element_point = c_create_point( codegen );
   c_append_node( codegen, &copy_element_point->node );
   c_pcd( codegen, PCD_DECSCRIPTVAR, size );
   // Move to next source element.
   if ( source_offset_allocated ) {
      c_pcd( codegen, PCD_DECSCRIPTVAR, source_offset );
   }
   // Move to next destination element.
   if ( destination_offset_allocated ) {
      c_pcd( codegen, PCD_DECSCRIPTVAR, destination_offset );
   }
   // Push amount of elements to copy.
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
   // Push offset to destination element. When the array starts at offset zero,
   // there is no need for an offset; just use the length variable, since it
   // is going down to zero as well.
   if ( destination_offset_allocated ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, destination_offset );
   }
   else {
      c_pcd( codegen, PCD_DUP );
   }
   // Push offset to source element.
   if ( source_offset_allocated ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, source_offset );
   }
   else {
      // When the offset to the destination element is pushed, it hides the
      // length, so retrieve the length from the variable.
      if ( destination_offset_allocated ) {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, size );
      }
      else {
         c_pcd( codegen, PCD_DUP );
      }
   }
   push_element( codegen, source.storage, source.index );
   c_update_element( codegen, destination.storage, destination.index,
      AOP_NONE );
   struct c_jump* cond_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &cond_jump->node );
   cond_jump->point = copy_element_point;
   // Exit.
   c_pcd( codegen, PCD_PUSHNUMBER, 1 );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &exit_jump->node );
   c_append_node( codegen, &fail_point->node );
   c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   struct c_point* exit_point = c_create_point( codegen );
   c_append_node( codegen, &exit_point->node );
   exit_jump->point = exit_point;
   result->status = R_VALUE;
   // Done.
   if ( source.status == R_ARRAYINDEX ) {
      c_dealloc_last_script_var( codegen );
   }
   if ( destination.status == R_ARRAYINDEX ) {
      c_dealloc_last_script_var( codegen );
   }
   c_dealloc_last_script_var( codegen );
}

static bool push_offset_scale_factor( struct codegen* codegen,
   struct result* result ) {
   if ( result->dim ) {
      if ( result->dim->element_size > 1 ) {
         c_pcd( codegen, PCD_PUSHNUMBER, result->dim->element_size );
         return true;
      }
   }
   else if ( result->ref && result->ref->type == REF_ARRAY ) {
      if ( result->ref_dim == 0 ) {
         struct ref_array* array = ( struct ref_array* ) result->ref;
         result->ref_dim = array->dim_count;
      }
      // Sub-array.
      if ( result->ref_dim > 1 ) {
         c_push_dimtrack( codegen );
         c_pcd( codegen, PCD_PUSHNUMBER, 1 );
         c_pcd( codegen, PCD_ADD );
         c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
         return true;
      }
      // Structure element.
      else if ( result->structure ) {
         if ( result->structure->size > 1 ) {
            c_pcd( codegen, PCD_PUSHNUMBER, result->structure->size );
            return true;
         }
      }
   }
   return false;
}

static void push_array_size( struct codegen* codegen, struct result* result ) {
   if ( result->dim ) {
      c_pcd( codegen, PCD_PUSHNUMBER, t_dim_size( result->dim ) );
   }
   else if ( result->ref && result->ref->type == REF_ARRAY ) {
      push_diminfo( codegen );
   }
   else {
      C_UNREACHABLE( codegen );
   }
}

static void push_array_length( struct codegen* codegen, struct result* result,
   bool dim_info_pushed ) {
   if ( result->dim ) {
      c_pcd( codegen, PCD_PUSHNUMBER, result->dim->length );
   }
   else if ( result->ref && result->ref->type == REF_ARRAY ) {
      push_ref_array_length( codegen, result, dim_info_pushed );
   }
   else {
      C_UNREACHABLE( codegen );
   }
}

static void push_ref_array_length( struct codegen* codegen,
   struct result* result, bool dim_info_pushed ) {
   int dim_count = result->ref_dim;
   if ( dim_count == 0 ) {
      struct ref_array* array = ( struct ref_array* ) result->ref;
      dim_count = array->dim_count;
   }
   if ( ! dim_info_pushed ) {
      c_push_dimtrack( codegen );
   }
   // Sub-array.
   if ( dim_count > 1 ) {
      c_pcd( codegen, PCD_DUP );
      c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
      c_pcd( codegen, PCD_SWAP );
      c_pcd( codegen, PCD_PUSHNUMBER, 1 );
      c_pcd( codegen, PCD_ADD );
      c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
      c_pcd( codegen, PCD_DIVIDE );
   }
   // Reference element.
   else if ( result->ref->next ) {
      if ( result->ref->next->type == REF_ARRAY ) {
         c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
         c_pcd( codegen, PCD_PUSHNUMBER, ARRAYREF_SIZE );
         c_pcd( codegen, PCD_DIVIDE );
      }
      else {
         c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
      }
   }
   // Structure element.
   else if ( result->structure ) {
      if ( result->structure->size > 1 ) {
         c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
         c_pcd( codegen, PCD_PUSHNUMBER, result->structure->size );
         c_pcd( codegen, PCD_DIVIDE );
      }
      else {
         c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
      }
   }
   // Primitive element.
   else {
      c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
   }
}

static void copy_struct( struct codegen* codegen, struct result* result,
   struct memcpy_call* call ) {
   int dst_ofs = 0;
   struct result dst;
   init_result( &dst, true );
   visit_operand( codegen, &dst, call->destination->root );
   if ( dst.status == R_ARRAYINDEX ) {
      dst_ofs = c_alloc_script_var( codegen );
      c_pcd( codegen, PCD_PUSHNUMBER, dst.structure->size );
      c_pcd( codegen, PCD_ADD );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, dst_ofs );
   }
   int src_ofs = 0;
   struct result src;
   init_result( &src, true );
   visit_operand( codegen, &src, call->source->root );
   if ( src.status == R_ARRAYINDEX ) {
      src_ofs = c_alloc_script_var( codegen );
      c_pcd( codegen, PCD_PUSHNUMBER, dst.structure->size );
      c_pcd( codegen, PCD_ADD );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, src_ofs );
   }
   int length = c_alloc_script_var( codegen );
   c_pcd( codegen, PCD_PUSHNUMBER, dst.structure->size );
   c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, length );
   struct c_point* copy_point = c_create_point( codegen );
   c_append_node( codegen, &copy_point->node );
   if ( dst.status == R_ARRAYINDEX ) {
      c_pcd( codegen, PCD_DECSCRIPTVAR, dst_ofs );
   }
   if ( src.status == R_ARRAYINDEX ) {
      c_pcd( codegen, PCD_DECSCRIPTVAR, src_ofs );
   }
   c_pcd( codegen, PCD_DECSCRIPTVAR, length );
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, length );
   // Push destination offset.
   if ( dst.status == R_ARRAYINDEX ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, dst_ofs );
   }
   else {
      c_pcd( codegen, PCD_DUP );
   }
   // Push source offset.
   if ( src.status == R_ARRAYINDEX ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, src_ofs );
   }
   else {
      if ( dst.status == R_ARRAYINDEX ) {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, length );
      }
      else {
         c_pcd( codegen, PCD_DUP );
      }
   }
   // Push and assign source element.
   push_element( codegen, src.storage, src.index );
   c_update_element( codegen, dst.storage, dst.index, AOP_NONE );
   // Keep going until: length == 0
   struct c_jump* cond_jump = c_create_jump( codegen, PCD_IFGOTO );
   c_append_node( codegen, &cond_jump->node );
   cond_jump->point = copy_point;
   c_dealloc_last_script_var( codegen );
   if ( src.status == R_ARRAYINDEX ) {
      c_dealloc_last_script_var( codegen );
   }
   if ( dst.status == R_ARRAYINDEX ) {
      c_dealloc_last_script_var( codegen );
   }
   if ( result->push ) {
      c_pcd( codegen, PCD_PUSHNUMBER, 1 );
      result->status = R_VALUE;
   }
}

static void visit_lengthof( struct codegen* codegen, struct result* result,
   struct lengthof* call ) {
   // Length known at compile time.
   if ( call->operand->folded ) {
      c_pcd( codegen, PCD_PUSHNUMBER, call->value );
   }
   // Length must be determined at run time.
   else {
      struct result operand;
      init_result( &operand, true );
      push_operand_result( codegen, &operand, call->operand->root );
      // Null check.
      if ( operand.ref && operand.ref->type == REF_ARRAY &&
         operand.ref->nullable && ! operand.safe ) {
         write_null_check( codegen );
      }
      // Drop the array address because we do not need it for this operation.
      if ( operand.status == R_ARRAYINDEX ) {
         c_pcd( codegen, PCD_DROP );
      }
      push_array_length( codegen, &operand, false );
   }
   result->status = R_VALUE;
}

static void visit_conversion( struct codegen* codegen, struct result* result,
   struct conversion* conv ) {
   switch ( conv->spec ) {
   case SPEC_INT:
      switch ( conv->spec_from ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_BOOL:
         c_push_expr( codegen, conv->expr );
         result->status = R_VALUE;
         break;
      // When shifting by 16 a negative fixed-point number, the result seems
      // to be off by one if the fixed-point number contains a fractional part.
      // I'm not sure exactly what is going on, but it might have something to
      // do with the representation of signed integers. For now, just do some
      // basic math to get the right result.
      case SPEC_FIXED:
         c_push_expr( codegen, conv->expr );
         c_pcd( codegen, PCD_PUSHNUMBER, FIXEDNUMBER_WHOLE );
         c_pcd( codegen, PCD_DIVIDE );
         result->status = R_VALUE;
         break;
      default:
         C_UNREACHABLE( codegen );
      }
      break;
   case SPEC_FIXED:
      switch ( conv->spec_from ) {
      case SPEC_RAW:
      case SPEC_INT:
      case SPEC_BOOL:
         c_push_expr( codegen, conv->expr );
         c_pcd( codegen, PCD_PUSHNUMBER, 16 );
         c_pcd( codegen, PCD_LSHIFT );
         result->status = R_VALUE;
         break;
      case SPEC_FIXED:
         c_push_expr( codegen, conv->expr );
         result->status = R_VALUE;
         break;
      default:
         C_UNREACHABLE( codegen );
      }
      break;
   case SPEC_BOOL:
      if ( conv->from_ref ) {
         struct result arg;
         init_result( &arg, true );
         push_operand_result( codegen, &arg, conv->expr->root );
         c_pcd( codegen, PCD_NEGATELOGICAL );
         c_pcd( codegen, PCD_NEGATELOGICAL );
         result->status = R_VALUE;
      }
      else {
         switch ( conv->spec_from ) {
         case SPEC_RAW:
         case SPEC_INT:
         case SPEC_FIXED:
            c_push_expr( codegen, conv->expr );
            c_pcd( codegen, PCD_NEGATELOGICAL );
            c_pcd( codegen, PCD_NEGATELOGICAL );
            result->status = R_VALUE;
            break;
         case SPEC_BOOL:
            c_push_expr( codegen, conv->expr );
            result->status = R_VALUE;
            break;
         case SPEC_STR:
            c_push_expr( codegen, conv->expr );
            c_pcd( codegen, PCD_PUSHNUMBER, 0 );
            c_pcd( codegen, PCD_CALLFUNC, 2, EXTFUNC_GETCHAR );
            c_pcd( codegen, PCD_NEGATELOGICAL );
            c_pcd( codegen, PCD_NEGATELOGICAL );
            result->status = R_VALUE;
            break;
         default:
            C_UNREACHABLE( codegen );
         }
      }
      break;
   case SPEC_STR:
      switch ( conv->spec_from ) {
         struct c_casejump* jump;
         struct c_point* point;
      case SPEC_RAW:
      case SPEC_INT:
         c_pcd( codegen, PCD_BEGINPRINT );
         c_push_expr( codegen, conv->expr );
         c_pcd( codegen, PCD_PRINTNUMBER );
         c_pcd( codegen, PCD_SAVESTRING );
         result->status = R_VALUE;
         break;
      case SPEC_FIXED:
         c_pcd( codegen, PCD_BEGINPRINT );
         c_push_expr( codegen, conv->expr );
         c_pcd( codegen, PCD_PRINTFIXED );
         c_pcd( codegen, PCD_SAVESTRING );
         result->status = R_VALUE;
         break;
      case SPEC_BOOL:
         c_pcd( codegen, PCD_BEGINPRINT );
         c_push_expr( codegen, conv->expr );
         jump = c_create_casejump( codegen, 0, NULL );
         c_append_node( codegen, &jump->node );
         c_pcd( codegen, PCD_PRINTNUMBER );
         point = c_create_point( codegen );
         c_append_node( codegen, &point->node );
         jump->point = point;
         c_pcd( codegen, PCD_SAVESTRING );
         result->status = R_VALUE;
         break;
      case SPEC_STR:
         c_push_expr( codegen, conv->expr );
         result->status = R_VALUE;
         break;
      default:
         C_UNREACHABLE( codegen );
      }
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void visit_null( struct codegen* codegen, struct result* result ) {
   c_pcd( codegen, PCD_PUSHNUMBER, NULLVALUE );
   result->null = true;
   result->status = R_VALUE;
}

static void visit_paren( struct codegen* codegen, struct result* result,
   struct paren* paren ) {
   visit_operand( codegen, result, paren->inside );
}

static void visit_compound_literal( struct codegen* codegen,
   struct result* result, struct compound_literal* literal ) {
   c_visit_var( codegen, literal->var );
   visit_var( codegen, result, literal->var );
}

static void push_indexed( struct codegen* codegen, int storage, int index ) {
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
   c_pcd( codegen, code, index );
}

static void push_element( struct codegen* codegen, int storage, int index ) {
   int code = PCD_PUSHSCRIPTARRAY;
   switch ( storage ) {
   case STORAGE_MAP:
      code = PCD_PUSHMAPARRAY;
      break;
   case STORAGE_WORLD:
      code = PCD_PUSHWORLDARRAY;
      break;
   case STORAGE_GLOBAL:
      code = PCD_PUSHGLOBALARRAY;
      break;
   default:
      break;
   }
   c_pcd( codegen, code, index );
}

void c_push_element( struct codegen* codegen, int storage, int index ) {
   push_element( codegen, storage, index );
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
   c_pcd( codegen, code[ op * 4 + pos ], index );
}

void c_update_element( struct codegen* codegen, int storage, int index,
   int op ) {
   static const int code[] = {
      PCD_ASSIGNSCRIPTARRAY,
      PCD_ASSIGNMAPARRAY,
      PCD_ASSIGNWORLDARRAY,
      PCD_ASSIGNGLOBALARRAY,
      PCD_ADDSCRIPTARRAY,
      PCD_ADDMAPARRAY,
      PCD_ADDWORLDARRAY,
      PCD_ADDGLOBALARRAY,
      PCD_SUBSCRIPTARRAY,
      PCD_SUBMAPARRAY,
      PCD_SUBWORLDARRAY,
      PCD_SUBGLOBALARRAY,
      PCD_MULSCRIPTARRAY,
      PCD_MULMAPARRAY,
      PCD_MULWORLDARRAY,
      PCD_MULGLOBALARRAY,
      PCD_DIVSCRIPTARRAY,
      PCD_DIVMAPARRAY,
      PCD_DIVWORLDARRAY,
      PCD_DIVGLOBALARRAY,
      PCD_MODSCRIPTARRAY,
      PCD_MODMAPARRAY,
      PCD_MODWORLDARRAY,
      PCD_MODGLOBALARRAY,
      PCD_LSSCRIPTARRAY,
      PCD_LSMAPARRAY,
      PCD_LSWORLDARRAY,
      PCD_LSGLOBALARRAY,
      PCD_RSSCRIPTARRAY,
      PCD_RSMAPARRAY,
      PCD_RSWORLDARRAY,
      PCD_RSGLOBALARRAY,
      PCD_ANDSCRIPTARRAY,
      PCD_ANDMAPARRAY,
      PCD_ANDWORLDARRAY,
      PCD_ANDGLOBALARRAY,
      PCD_EORSCRIPTARRAY,
      PCD_EORMAPARRAY,
      PCD_EORWORLDARRAY,
      PCD_EORGLOBALARRAY,
      PCD_ORSCRIPTARRAY,
      PCD_ORMAPARRAY,
      PCD_ORWORLDARRAY,
      PCD_ORGLOBALARRAY
   };
   int pos = 0;
   switch ( storage ) {
   case STORAGE_MAP: pos = 1; break;
   case STORAGE_WORLD: pos = 2; break;
   case STORAGE_GLOBAL: pos = 3; break;
   default: break;
   }
   c_pcd( codegen, code[ op * 4 + pos ], index );
}

void c_push_foreach_collection( struct codegen* codegen,
   struct foreach_writing* writing, struct expr* expr ) {
   struct result result;
   init_result( &result, true );
   visit_operand( codegen, &result, expr->root );
   writing->structure = result.structure;
   writing->dim = result.dim;
   writing->storage = result.storage;
   writing->index = result.index;
   writing->diminfo = result.diminfo_start;
   writing->pushed_base = ( result.status == R_ARRAYINDEX );
   // Array.
   if ( result.dim ) {
      writing->collection = FOREACHCOLLECTION_ARRAY;
      // Sub-array.
      if ( result.dim->next ) {
         writing->item = FOREACHITEM_SUBARRAY;
      }
      // Reference element.
      else if ( result.ref ) {
         if ( result.ref->type == REF_ARRAY ) {
            writing->item = FOREACHITEM_ARRAYREF;
         }
      }
      // Structure element.
      else if ( result.structure ) {
         writing->item = FOREACHITEM_STRUCT;
      }
   }
   // Array (reference).
   else if ( result.ref && result.ref->type == REF_ARRAY ) {
      writing->collection = FOREACHCOLLECTION_ARRAYREF;
      push_array_length( codegen, &result, false );
      int ref_dim = result.ref_dim;
      if ( ref_dim == 0 ) {
         struct ref_array* array = ( struct ref_array* ) result.ref;
         ref_dim = array->dim_count;
      }
      // Sub-array.
      if ( ref_dim > 1 ) {
         inc_dimtrack( codegen );
         c_push_dimtrack( codegen );
         writing->item = FOREACHITEM_SUBARRAY;
      }
      // Reference element.
      else if ( result.ref->next ) {
         if ( result.ref->next->type == REF_ARRAY ) {
            writing->item = FOREACHITEM_ARRAYREF;
         }
      }
      // Structure element.
      else if ( result.structure ) {
         writing->item = FOREACHITEM_STRUCT;
      }
   }
}

void c_push_dimtrack( struct codegen* codegen ) {
   if ( codegen->shary.used ) {
      if ( codegen->shary.dim_counter_var ) {
         push_indexed( codegen, STORAGE_MAP, codegen->shary.dim_counter );
      }
      else {
         c_pcd( codegen, PCD_PUSHNUMBER, SHAREDARRAYFIELD_DIMTRACK );
         c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
      }
   }
   else {
      C_INTERNAL_ERR( codegen,
         "asked to push dimension information, but shared array is not "
         "allocated", __FILE__, __LINE__ ); 
      c_bail( codegen );
   }
}

void c_update_dimtrack( struct codegen* codegen ) {
   if ( codegen->shary.used ) {
      if ( codegen->shary.dim_counter_var ) {
         c_update_indexed( codegen, STORAGE_MAP, codegen->shary.dim_counter,
            AOP_NONE );
      }
      else {
         c_pcd( codegen, PCD_PUSHNUMBER, SHAREDARRAYFIELD_DIMTRACK );
         c_pcd( codegen, PCD_SWAP );
         c_pcd( codegen, PCD_ASSIGNMAPARRAY, codegen->shary.index );
      }
   }
   else {
      C_INTERNAL_ERR( codegen,
         "asked to update dimension information, but shared array is not "
         "allocated" );
      c_bail( codegen );
   }
}

static void inc_dimtrack( struct codegen* codegen ) {
   if ( codegen->shary.used ) {
      if ( codegen->shary.dim_counter_var ) {
         c_pcd( codegen, PCD_INCMAPVAR, codegen->shary.dim_counter );
      }
      else {
         c_pcd( codegen, PCD_PUSHNUMBER, SHAREDARRAYFIELD_DIMTRACK );
         c_pcd( codegen, PCD_INCMAPARRAY, codegen->shary.index );
      }
   }
   else {
      C_INTERNAL_ERR( codegen,
         "asked to increment dimension information, but shared array is not "
         "allocated" );
      c_bail( codegen );
   }
}

void c_inc_dimtrack( struct codegen* codegen ) {
   inc_dimtrack( codegen );
}

static void next_diminfo( struct codegen* codegen ) {
   inc_dimtrack( codegen );
}

static void push_diminfo( struct codegen* codegen ) {
   c_push_dimtrack( codegen );
   c_pcd( codegen, PCD_PUSHMAPARRAY, codegen->shary.index );
}
