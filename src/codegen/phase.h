#ifndef SRC_CODEGEN_PHASE_H
#define SRC_CODEGEN_PHASE_H

#include "task.h"

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
   struct block_visit* block_visit;
   struct block_visit* block_visit_free;
   struct block_visit* func_visit;
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
void c_alloc_indexes( struct codegen* phase );

#endif