#include "task.h"
#include "phase.h"

void c_init( struct codegen* codegen, struct task* task ) {
   codegen->task = task;
   // codegen->main_lib = task->main_lib;
   // codegen->libs = &task->libs;
   // codegen->str_table = &task->str_table;
   // codegen->compress = ( codegen->main_lib->format == FORMAT_LITTLE_E );
   codegen->compress = false;
   codegen->func = NULL;
   codegen->local_record = NULL;
   c_init_obj( codegen );
   codegen->node = NULL;
   codegen->node_head = NULL;
   codegen->node_tail = NULL;
   for ( int i = 0; i < C_NODE_TOTAL; ++i ) {
      codegen->free_nodes[ i ] = NULL;
   }
   codegen->pcode = NULL;
   codegen->pcodearg_tail = NULL;
   codegen->free_pcode_args = NULL;
}

void c_publish( struct codegen* codegen ) {
   c_alloc_indexes( codegen );
   c_write_chunk_obj( codegen );
   c_flush( codegen );
}