#ifndef F_MAIN_H
#define F_MAIN_H

#include <stdio.h>
#include <stdbool.h>
#include <setjmp.h>

#include "common.h"
#include "f_tkid.h"

/*

   Parses the token input, generating an abstract syntax tree. Both parsing
   and semantic analysis are performed at the same time.

*/

#define TK_QUEUE_SIZE 5

enum {
   k_stmt_none,
   k_stmt_script = 0x1,
   k_stmt_function = 0x2,
   k_stmt_loop = 0x4,
   k_stmt_switch = 0x8,
   k_stmt_switch_direct = 0x10,
   k_stmt_format_block = 0x20,
};

enum {
   k_expr_comma,
   k_expr_assign,
   k_expr_cond,
};

enum {
   k_tk_file_err_none,
   k_tk_file_err_open,
   k_tk_file_err_dup,
};

enum {
   k_dec_top,
   k_dec_local,
   k_dec_for,
   k_dec_param,
};

#define DIAG_NONE 0x0
#define DIAG_NOTICE 0x1
#define DIAG_WARNING 0x2
#define DIAG_ERR 0x4
#define DIAG_LINE 0x8
#define DIAG_COLUMN 0x10

typedef struct {
   char* source;
   position_t pos;
   tk_t type;
   int length;
} token_t;

// The expression where an undefined object is used.
typedef struct {
   expr_t* node;
   int num_undef;
} undef_expr_t;

// Where the undefined object is used.
typedef struct undef_usage_t {
   struct undef_usage_t* next;
   undef_expr_t* expr;
   position_t pos;
} undef_usage_t;

// The undefined object.
typedef struct undef_t {
   node_t node;
   undef_usage_t* usage;
   // Link all undefined objects together so they can all be checked at the
   // end.
   struct undef_t* prev;
   struct undef_t* next;
   nkey_t* name;
   bool is_called;
} undef_t;

typedef struct nval_t {
   struct nval_t* prev;
   node_t* node;
   int depth;
} nval_t;

typedef struct p_scope_t {
   struct p_scope_t* prev;
   int size;
   int size_high;
} p_scope_t;

typedef struct {
   type_t* type;
   position_t pos;
   int value;
   bool is_string;
} p_literal_t;

typedef struct {
   position_t pos;
   enum {
      k_params_script,
      k_params_func,
      k_params_bfunc,
   } type;
   int num_param;
   int min_param;
   list_t var_list;
   bool default_given;
} p_params_t;

typedef struct p_block_t {
   struct p_block_t* parent;
   list_t stmt_list;
   enum {
      k_flow_going,
      k_flow_dead,
      k_flow_jump_stmt
   } flow;
   struct {
      position_t pos;
      int type;
   } jump_stmt;
   int flags;
   bool in_func;
   bool is_func_return;
   bool is_break;
   bool is_continue;
   jump_t* jump_break;
   jump_t* jump_continue;
} p_block_t;

typedef struct p_expr_t {
   struct p_expr_t* parent;
   expr_t* expr;
   int offset;
   bool has_string;
   bool is_undef;
   bool nested;
} p_expr_t;

typedef struct {
   case_t* head_case;
   case_t* tail_case;
   case_t* default_case;
   case_t* head_case_sorted;
   int count;
} p_case_t;

typedef struct {
   int cast;
} p_fcast_t;

typedef struct front_t {
   FILE* fh;
   jmp_buf bail;
   ast_t* ast;
   module_t* module;
   p_block_t* block;
   p_case_t* case_stmt;
   p_expr_t* expr;
   p_params_t* params;
   type_t* type2;
   options_t* options;
   struct sweep_t* sweep;
   struct undef_t* undef_head;
   struct undef_t* undef_tail;
   ntbl_t* name_table;
   p_scope_t* scope;
   nval_t* nval;
   nkey_t* anon_type;
   file_table_t* file_table;
   struct source_t* source;
   list_t* dec_for;
   type_t* dec_type;
   str_table_t str_table;
   list_t undef_stack;
   list_t scopes;
   list_t anon_types;
   token_t token;
   struct {
      token_t tokens[ TK_QUEUE_SIZE ];
      int pos;
      int size;
   } queue;
   int func_type;
   int num_errs;
   int scope_size;
   int scope_size_high;
   short depth;
   short depth_size;
   bool reading_script_number;
   bool importing;
   bool err_file;
   struct {
      nkey_t* t_int;
      nkey_t* t_str;
      nkey_t* t_bool;
      nkey_t* t_fixed;
   } basic_types;
} front_t;

void f_init( front_t*, options_t*, ntbl_t*, file_table_t*, ast_t* );
void f_deinit( front_t* );
void f_bail( front_t* );
void f_diag( front_t*, int, position_t*, const char*, ... );

int tk_load_file( front_t*, const char* );
bool tk_pop_file( front_t* );
void tk_read( front_t* );
// IMPORTANT: Don't peek beyond the queue size.
token_t* tk_peek( front_t*, int );
// tk_t tk_peek( front_t* );

bool p_build_tree( front_t* );
tk_t tk( front_t* );
void drop( front_t* );
void skip( front_t*, tk_t );
token_t* test( front_t*, tk_t );
void p_new_scope( front_t* );
void p_pop_scope( front_t* );
nkey_t* p_get_unique_name( front_t* );
void p_use_key( front_t*, nkey_t*, node_t*, bool );
void p_attach_undef( front_t*, undef_t* );
void p_detach_undef( front_t*, undef_t* );
int p_remove_undef( front_t*, nkey_t* );
bool p_is_dec( front_t* );
void p_read_dec( front_t*, int );
void p_read_compound( front_t* );
void p_block_init( p_block_t*, p_block_t*, int );
void p_block_init_script( p_block_t* );
void p_block_init_ufunc( p_block_t*, bool );
void p_literal_init( front_t*, p_literal_t*, token_t* );
void p_expr_init( p_expr_t*, int );
void p_expr_read( front_t*, p_expr_t* );
void p_test_expr( front_t*, expr_t* );
void p_fcast_init( front_t*, p_fcast_t*, token_t* );
void p_read_script( front_t* );

#endif