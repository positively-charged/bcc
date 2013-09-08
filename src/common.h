#ifndef COMMON_H
#define COMMON_H

#include "list.h"
#include "str.h"
#include "ntbl.h"

typedef struct {
   list_t include_paths;
   const char* source_file;
   const char* object_file;
   enum {
      k_format_zero,
      k_format_big_e,
      k_format_little_e
   } format;
   enum {
      k_action_all,
      k_action_compile,
      k_action_link,
   } action;
   bool encrypt_str;
   // Outputs a file containing the errors in the same format as acc does. This
   // is for showing errors in Doom Builder 2.
   bool err_file;
} options_t;

typedef struct file_t {
   struct file_t* next;
   // The path to the file as given by the user. It doesn't include an include
   // path that might have been prepended in order to load the file.
   str_t user_path;
   // The path that was used to read the file.
   str_t load_path;
   bool imported;
} file_t;

typedef struct active_file_t {
   struct active_file_t* next;
   struct active_file_t* prev;
   file_t* file;
   struct source_t* source;
} active_file_t;

typedef struct {
   file_t* file_head;
   file_t* file;
   active_file_t* active_head;
   active_file_t* active;
   list_t* include_paths;
} file_table_t;

typedef struct {
   file_t* file;
   int line;
   int column;
} position_t;

typedef struct node_t {
   enum {
      k_node_none,
      k_node_unary,
      k_node_binary,
      k_node_ternary,
      k_node_literal,
      k_node_expr,
      k_node_jump,
      k_node_script_jump,
      k_node_var,
      k_node_while,
      // 10
      k_node_for,
      k_node_if,
      k_node_call,
      k_node_format_item,
      k_node_name,
      k_node_comma,
      k_node_array_jump,
      k_node_subscript,
      k_node_case,
      k_node_switch,
      // 20
      k_node_return,
      k_node_format_block,
      k_node_post_inc,
      k_node_member_access,
      k_node_member_call,
      k_node_assign,
      k_node_type,
      k_node_type_member,
      k_node_constant,
      k_node_func,
      // 30
      k_node_undef,
      k_node_script,
      k_node_paltrans,
   } type;
} node_t;

enum {
   k_storage_none,
   k_storage_local,
   k_storage_module,
   k_storage_world,
   k_storage_global
};

typedef struct dimension_t {
   struct dimension_t* next;
   int size;
   int element_size;
} dimension_t;

typedef struct pointer_t {
   struct pointer_t* next;
} pointer_t;

typedef struct type_member_t {
   node_t node;
   struct type_member_t* next;
   node_t* object;
   int offset;
} type_member_t;

typedef struct {
   node_t node;
   nkey_t* name;
   type_member_t* member;
   type_member_t* member_tail;
   enum {
      k_type_int,
      k_type_str,
      k_type_bool,
      k_type_fixed,
      k_type_custom
   } known;
   int size;
   int index;
} type_t;

typedef struct {
   node_t node;
   node_t* root;
   type_t* type;
   position_t pos;
   int value;
   bool is_constant;
   bool is_undef;
   bool has_string;
} expr_t;

typedef struct {
   node_t node;
   position_t pos;
   type_t* type;
   // The value means different things depending on the type of the literal.
   // For integers, it's the numeric value; for strings it's an index into a
   // table of strings.
   int value;
} literal_t;

typedef struct literal_string_t {
   node_t node;
   struct literal_string_t* next;
   const char* value;
   int length;
} literal_string_t;

typedef struct {
   node_t node;
   enum {
      k_uop_none,
      k_uop_minus,
      k_uop_plus,
      k_uop_log_not,
      k_uop_bit_not,
      k_uop_pre_inc,
      k_uop_pre_dec,
      k_uop_post_inc,
      k_uop_post_dec,
      k_uop_addr,
      k_uop_deref,
      k_uop_deref_mem,
   } op;
   node_t* operand;
   position_t pos;
   bool is_folded;
   int value;
} unary_t;

typedef struct {
   node_t node;
   node_t* lside;
   node_t* rside;
   enum {
      k_assign,
      k_assign_add,
      k_assign_sub,
      k_assign_mul,
      k_assign_div,
      k_assign_mod,
      k_assign_shift_l,
      k_assign_shift_r,
      k_assign_bit_and,
      k_assign_bit_xor,
      k_assign_bit_or,
   } type;
   position_t pos;
} assign_t;

typedef struct {
   node_t node;
   enum {
      k_bop_log_or,
      k_bop_log_and,
      k_bop_bit_or,
      k_bop_bit_xor,
      k_bop_bit_and,
      k_bop_equal,
      k_bop_not_equal,
      k_bop_less_than,
      k_bop_less_than_equal,
      k_bop_more_than,
      k_bop_more_than_equal,
      k_bop_shift_l,
      k_bop_shift_r,
      k_bop_add,
      k_bop_sub,
      k_bop_mul,
      k_bop_div,
      k_bop_mod
   } op;
   node_t* lside;
   node_t* rside;
   position_t pos;
   bool is_folded;
   int value;
} binary_t;

// Operation consisting of three operands. There's only one ternary operator at
// this time: the conditional (?:) operator.
typedef struct {
   node_t node;
   node_t* test;
   node_t* lside;
   node_t* rside;
   position_t pos;
} ternary_t;

typedef struct {
   node_t node;
   node_t* lside;
   node_t* rside;
} comma_t;

typedef struct {
   node_t node;
   node_t* operand;
   expr_t* index;
} subscript_t;

typedef struct {
   node_t node;
   node_t* func;
   position_t pos;
   list_t args;
   // For checking whether a call to a latent function is allowed.
   enum {
      k_call_in_none,
      k_call_in_func,
      k_call_in_format,
   } in;
} call_t;

typedef struct {
   node_t node;
   node_t* object;
   nkey_t* name;
   type_member_t* member;
} member_access_t;

typedef struct {
   node_t node;
   node_t* object;
   nkey_t* func;
   list_t args;
} member_call_t;

typedef struct {
   node_t node;
   // For now, the types of items are hard-coded. In the future, maybe user
   // functions will be able to use a specifier-list argument.
   enum {
      k_fcast_array,
      k_fcast_binary,
      k_fcast_char,
      k_fcast_decimal,
      k_fcast_fixed,
      k_fcast_key,
      k_fcast_local_string,
      k_fcast_name,
      k_fcast_string,
      k_fcast_hex
   } cast;
   expr_t* expr;
} format_item_t;

typedef struct {
   node_t node;
   nkey_t* key;
   node_t* object;
   position_t pos;
} name_t;

typedef struct {
   node_t node;
   dimension_t* dim;
   enum {
      k_dimension_mark_enter,
      k_dimension_mark_leave,
   } type;
} dimension_mark_t;

typedef struct {
   node_t node;
   int index;
} array_jump_t;

enum {
   k_jump_terminate,
   k_jump_suspend,
   k_jump_restart,
   k_jump_break,
   k_jump_continue,
   k_jump_return,
};

typedef struct jump_t {
   node_t node;
   struct jump_t* next;
   int type;
   struct b_pos_t* offset;
} jump_t;

typedef struct {
   node_t node;
   int type;
} script_jump_t;

typedef struct {
   node_t node;
   node_t* cond;
   list_t body;
   // The else body is NULL when it's not specified.
   list_t else_body;
} if_t;

typedef struct case_t {
   node_t node;
   struct case_t* next;
   struct case_t* next_sorted;
   // The default case is specified by a NULL expression.  
   expr_t* expr;
   position_t pos;
   struct b_pos_t* offset;
} case_t;

typedef struct {
   node_t node;
   node_t* cond;
   case_t* head_case;
   // Cases sorted by their value, in ascending order. Default case not
   // included in this list.
   case_t* head_case_sorted;
   case_t* default_case;
   jump_t* jump_break;
   list_t body;
   int case_count;
} switch_t;

typedef struct {
   node_t node;
   enum {
      k_while_while,
      k_while_until,
      k_while_do_while,
      k_while_do_until
   } type;
   node_t* cond;
   list_t body;
   jump_t* jump_break;
   jump_t* jump_early;
} while_t;

typedef struct {
   node_t node;
   list_t init;
   expr_t* init_expr;
   node_t* cond;
   expr_t* post;
   list_t body;
   jump_t* jump_break;
   jump_t* jump_continue;
} for_t;

typedef struct {
   node_t node;
   expr_t* expr;
} return_t;

typedef struct {
   node_t node;
   list_t body;
} format_block_t;

typedef struct initial_t {
   struct initial_t* next;
   node_t* value;
} initial_t;

typedef struct {
   node_t node;
   position_t pos;
   type_t* type;
   nkey_t* name;
   dimension_t* dim;
   initial_t* initial;
   int storage;
   int index;
   int ref_depth;
   int size;
   int usage;
   bool lifetime_program;
   bool initial_not_zero;
   bool has_string;
} var_t;

typedef struct {
   node_t node;
   type_t* type;
   expr_t* expr;
   position_t pos;
   int value;
   bool hidden;
} constant_t;

typedef struct {
   nkey_t* name;
   var_t* var;
   expr_t* default_value;
   int offset;
   struct b_pos_t* pos;
} param_t;

typedef struct {
   node_t node;
   position_t pos;
   type_t* return_type;
   enum {
      k_func_undefined,
      k_func_aspec,
      k_func_ded,
      k_func_ext,
      k_func_format,
      k_func_user,
      k_func_internal,
   } type;
   int num_param;
   int min_param;
   bool latent;
} func_t;

// Action specials have "void" return type and all "int" parameters.
typedef struct {
   func_t func;
   int id;
} aspec_t;

// An extension function doesn't have a unique pcode reserved for it, but is
// instead called using the pc_call_func pcode.
typedef struct {
   func_t func;
   int id;
} ext_func_t;

// A function with an opcode allocated to it.
typedef struct {
   func_t func;
   int opcode;
   int opcode_constant;
} ded_func_t;

// Format functions are dedicated functions and have their first parameter a
// specifier list.
typedef struct {
   func_t func;
   int opcode;
} format_func_t;

// Functions created by the user.
typedef struct ufunc_t {
   func_t func;
   struct ufunc_t* next;
   nkey_t* name;
   list_t params;
   list_t body;
   int index;
   int offset;
   int size;
   int usage;
} ufunc_t;

typedef struct {
   func_t func;
   enum {
      k_ifunc_acs_execwait,
      k_ifunc_str_subscript,
      k_ifunc_str_length,
   } id;
} ifunc_t;

typedef struct {
   node_t node;
   position_t pos;
   // When NULL, script number 0 is assumed.
   expr_t* number;
   enum {
      k_script_type_closed,
      k_script_type_open,
      k_script_type_respawn,
      k_script_type_death,
      k_script_type_enter,
      k_script_type_pickup,
      k_script_type_blue_return,
      k_script_type_red_return,
      k_script_type_white_return,
      k_script_type_lightning,
      k_script_type_unloading,
      k_script_type_disconnect,
      k_script_type_return
   } type;
   enum {
      k_script_flag_net = 1,
      k_script_flag_clientside = 2
   } flags;
   list_t params;
   list_t body;
   int offset;
   int size;
} script_t;

typedef struct palrange_t {
   struct palrange_t* next;
   expr_t* begin;
   expr_t* end;
   union {
      struct {
         expr_t* begin;
         expr_t* end;
      } ent;
      struct {
         expr_t* red1;
         expr_t* green1;
         expr_t* blue1;
         expr_t* red2;
         expr_t* green2;
         expr_t* blue2;
      } rgb;
   } value;
   bool rgb;
} palrange_t;

typedef struct {
   node_t node;
   expr_t* number;
   palrange_t* ranges;
   palrange_t* ranges_tail;
} paltrans_t;

typedef struct {
   expr_t* opcode;
   list_t args;
} asm_line_t;

typedef struct {
   node_t node;
   list_t lines;
} asm_t;

typedef struct indexed_string_t {
   struct indexed_string_t* next;
   struct indexed_string_t* sorted_next;
   char* value;
   int length;
   int index;
} indexed_string_t;

typedef struct {
   indexed_string_t* head;
   indexed_string_t* tail;
   indexed_string_t* sorted_head;
   int size;
} str_table_t;

typedef struct {
   str_t name;
   list_t scripts;
   list_t funcs;
   list_t vars;
   list_t arrays;
   list_t hidden_vars;
   list_t hidden_arrays;
   // World and global variables.
   list_t other_scalars;
   list_t other_arrays;
   list_t customs;
   list_t imports;
} module_t;

typedef struct {
   ufunc_t* main_func;
   module_t* module;
   ntbl_t* name_table;
   str_table_t str_table;
   list_t vars;
   list_t arrays;
   list_t funcs;
} ast_t;

void job_init_ast( ast_t*, ntbl_t* ); 
void job_init_options( options_t* );
void job_init_file_table( file_table_t*, list_t* include_paths,
   const char* source_path );
void job_deinit_file_table( file_table_t* );
file_t* job_active_file( file_table_t* );

#endif