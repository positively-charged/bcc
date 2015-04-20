#include "task.h"
#include "phase.h"

void c_init( struct codegen* codegen, struct task* task ) {
   codegen->task = task;
   // codegen->main_lib = task->main_lib;
   // codegen->libs = &task->libs;
   // codegen->str_table = &task->str_table;
   // codegen->compress = ( codegen->main_lib->format == FORMAT_LITTLE_E );
   codegen->compress = false;
   codegen->block_visit = NULL;
   codegen->block_visit_free = NULL;
   codegen->func_visit = NULL;
   c_init_obj( codegen );
}

void c_publish( struct codegen* codegen ) {
   c_alloc_indexes( codegen );
   c_write_chunk_obj( codegen );
   c_flush( codegen );
}