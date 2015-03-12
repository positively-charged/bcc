#ifndef SRC_SEMANTIC_PHASE_H
#define SRC_SEMANTIC_PHASE_H

#include "task.h"

struct stmt_test {
   struct stmt_test* parent;
   struct func* func;
   struct list* labels;
   struct block* format_block;
   struct case_label* case_head;
   struct case_label* case_default;
   struct jump* jump_break;
   struct jump* jump_continue;
   struct import2* import;
   bool in_loop;
   bool in_switch;
   bool in_script;
   bool manual_scope;
};

struct expr_test {
   jmp_buf bail;
   struct stmt_test* stmt_test;
   struct block* format_block;
   struct format_block_usage* format_block_usage;
   struct pos pos;
   bool result_required;
   bool has_string;
   bool undef_err;
   bool undef_erred;
   bool accept_array;
   bool suggest_paren_assign;
};

struct semantic {
   struct task* task;
   struct region* region;
   struct scope* scope;
   struct scope* free_scope;
   struct sweep* free_sweep;
   int depth;
   bool in_func;
};

void s_init( struct semantic* phase, struct task* task );
void s_test( struct semantic* phase );
void s_test_constant( struct semantic* phase, struct constant*, bool undef_err );
void s_test_constant_set( struct semantic* phase, struct constant_set*, bool undef_err );
void s_test_type( struct semantic* phase, struct type*, bool undef_err );
void s_test_var( struct semantic* phase, struct var* var, bool undef_err );
void s_test_func( struct semantic* phase, struct func* func, bool undef_err );
void s_test_func_body( struct semantic* phase, struct func* func );
void s_test_local_var( struct semantic* phase, struct var* );
void s_init_expr_test( struct expr_test* test, struct stmt_test* stmt_test,
   struct block* format_block, bool result_required, bool undef_err,
   bool suggest_paren_assign );
void s_test_expr( struct semantic* phase, struct expr_test*, struct expr* );
void s_init_stmt_test( struct stmt_test*, struct stmt_test* );
void s_test_top_block( struct semantic* phase, struct stmt_test*, struct block* );
void s_test_stmt( struct semantic* phase, struct stmt_test*, struct node* );
void s_test_block( struct semantic* phase, struct stmt_test*, struct block* );
void s_test_format_item( struct semantic* phase, struct format_item*, struct stmt_test*,
   struct expr_test*, struct block* );
void s_import( struct semantic* phase, struct import* );
void diag_dup( struct task*, const char* text, struct pos*, struct name* );
void diag_dup_struct( struct task*, struct name*, struct pos* );
void s_add_scope( struct semantic* phase );
void s_pop_scope( struct semantic* phase );
void s_test_script( struct semantic* phase, struct script* script );
void s_calc_var_size( struct var* var );
void s_calc_var_value_index( struct var* var );
void s_bind_local_name( struct semantic* phase, struct name* name,
   struct object* object );

#endif