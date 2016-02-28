#ifndef SRC_SEMANTIC_PHASE_H
#define SRC_SEMANTIC_PHASE_H

#include "task.h"

struct regobjget {
   struct object* object;
   struct structure* struct_object;
};

struct object_search {
   struct path* path;
   struct object* object;
   struct structure* struct_object;
   bool get_struct;
};

struct stmt_test {
   struct stmt_test* parent;
   struct func* func;
   struct list* labels;
   struct block* format_block;
   struct case_label* case_head;
   struct case_label* case_default;
   struct jump* jump_break;
   struct jump* jump_continue;
   struct func* nested_funcs;
   struct return_stmt* returns;
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
   bool undef_erred;
   bool accept_array;
   bool suggest_paren_assign;
};

struct semantic {
   struct task* task;
   struct library* lib;
   struct scope* scope;
   struct scope* free_scope;
   struct sweep* free_sweep;
   struct stmt_test* topfunc_test;
   struct stmt_test* func_test;
   int depth;
   bool trigger_err;
   bool in_localscope;
};

void s_init( struct semantic* semantic, struct task* task,
   struct library* lib );
void s_test( struct semantic* semantic );
void s_test_constant( struct semantic* semantic, struct constant* );
void s_test_constant_set( struct semantic* semantic, struct enumeration* );
void s_test_struct( struct semantic* semantic, struct structure* type );
void s_test_var( struct semantic* semantic, struct var* var );
void s_test_func( struct semantic* semantic, struct func* func );
void s_test_func_body( struct semantic* semantic, struct func* func );
void s_test_local_var( struct semantic* semantic, struct var* );
void s_init_expr_test( struct expr_test* test, struct stmt_test* stmt_test,
   struct block* format_block, bool result_required,
   bool suggest_paren_assign );
void s_test_expr( struct semantic* semantic, struct expr_test*, struct expr* );
void s_init_stmt_test( struct stmt_test*, struct stmt_test* );
void s_test_top_block( struct semantic* semantic, struct stmt_test*, struct block* );
void s_test_stmt( struct semantic* semantic, struct stmt_test*, struct node* );
void s_test_block( struct semantic* semantic, struct stmt_test*, struct block* );
void s_test_formatitemlist_stmt( struct semantic* semantic,
   struct stmt_test* stmt_test, struct format_item* item );
void s_add_scope( struct semantic* semantic );
void s_pop_scope( struct semantic* semantic );
void s_test_script( struct semantic* semantic, struct script* script );
void s_calc_var_size( struct var* var );
void s_calc_var_value_index( struct var* var );
void s_bind_name( struct semantic* semantic, struct name* name,
   struct object* object );
void s_diag( struct semantic* semantic, int flags, ... );
void s_bail( struct semantic* semantic );
void s_init_object_search( struct object_search* search, struct path* path,
   bool get_struct );
void s_find_object( struct semantic* semantic, struct object_search* search );
void s_present_spec( int spec, struct str* string );

#endif