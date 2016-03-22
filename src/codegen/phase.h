#ifndef SRC_CODEGEN_PHASE_H
#define SRC_CODEGEN_PHASE_H

#include "task.h"
#include "linear.h"

#define BUFFER_SIZE 65536

struct buffer {
   struct buffer* next;
   char data[ BUFFER_SIZE ];
   int used;
   int pos;
};

struct immediate {
   struct immediate* next;
   int value;
};

struct local_record {
   struct format_block_usage* format_block_usage;
   struct local_record* parent;
   int index;
   int func_size;
};

struct func_record {
   int start_index;
   int size;
   bool nested_func;
};

enum {
   RESERVEDSCRIPTVAR_DIM,
   RESERVEDSCRIPTVAR_TOTAL
};

struct codegen {
   struct task* task;
   struct buffer* buffer_head;
   struct buffer* buffer;
   bool compress;
   int opc;
   int opc_args;
   struct immediate* immediate;
   struct immediate* immediate_tail;
   struct immediate* free_immediate;
   int immediate_count;
   bool push_immediate;
   struct func_record* func;
   struct local_record* local_record;
   struct c_node* node;
   struct c_node* node_head;
   struct c_node* node_tail;
   struct c_node* free_nodes[ C_NODE_TOTAL ];
   struct c_pcode* pcode;
   struct c_pcode_arg* pcodearg_tail;
   struct c_pcode_arg* free_pcode_args;
   struct indexed_string* assert_prefix;
   int runtime_index;
   struct list used_strings;
   int shared_array_index;
   int shared_array_size;
   int shared_array_diminfo_size;
   bool shared_array_used;
};

void c_init( struct codegen*, struct task* );
void c_init_obj( struct codegen* );
void c_publish( struct codegen* );
void c_write_chunk_obj( struct codegen* );
void c_add_byte( struct codegen*, char );
void c_add_short( struct codegen*, short );
void c_add_int( struct codegen*, int );
void c_add_int_zero( struct codegen*, int );
// NOTE: Does not include the NUL character.
void c_add_str( struct codegen*, const char* );
void c_add_sized( struct codegen*, const void*, int size );
void c_add_opc( struct codegen*, int );
void c_add_arg( struct codegen*, int );
void c_seek( struct codegen*, int );
void c_seek_end( struct codegen* );
int c_tell( struct codegen* );
void c_flush( struct codegen* );
void c_write_user_code( struct codegen* );
void c_push_expr( struct codegen* codegen, struct expr* expr );
void c_push_cond( struct codegen* codegen, struct expr* cond );
void c_update_indexed( struct codegen* codegen, int, int, int );
void c_update_element( struct codegen* codegen, int, int, int );
void c_add_block_visit( struct codegen* codegen );
void c_pop_block_visit( struct codegen* codegen );
void c_write_block( struct codegen* codegen, struct block* stmt );
void c_write_stmt( struct codegen* codegen, struct node* node );
void c_visit_expr( struct codegen* codegen, struct expr* );
void c_visit_var( struct codegen* codegen, struct var* var );
void c_visit_format_item( struct codegen* codegen, struct format_item* item );
struct pcode* c_get_pcode_info( int code );
void c_opc( struct codegen* codegen, int code );
void c_unoptimized_opc( struct codegen* codegen, int code );
void c_arg( struct codegen* codegen, int value );
void c_point_arg( struct codegen* codegen, struct c_point* point );
void c_pcd( struct codegen* codegen, int code, ... );
void c_seek_node( struct codegen* codegen, struct c_node* node );
void c_append_node( struct codegen* codegen, struct c_node* node );
struct c_point* c_create_point( struct codegen* codegen );
struct c_jump* c_create_jump( struct codegen* codegen, int opcode );
struct c_casejump* c_create_casejump( struct codegen* codegen, int value,
   struct c_point* point );
struct c_sortedcasejump* c_create_sortedcasejump( struct codegen* codegen );
void c_append_casejump( struct c_sortedcasejump* sorted_jump,
   struct c_casejump* jump );
void c_flush_pcode( struct codegen* codegen );
void c_visit_nested_func( struct codegen* codegen, struct func* func );
void p_visit_inline_asm( struct codegen* codegen,
   struct inline_asm* inline_asm );
void c_write_opc( struct codegen* codegen, int opcode );
void c_write_arg( struct codegen* codegen, int arg );
void c_push_string( struct codegen* codegen, struct indexed_string* string );
int c_alloc_script_var( struct codegen* codegen );
void c_dealloc_last_script_var( struct codegen* codegen );

#endif