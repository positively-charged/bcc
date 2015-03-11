#include "task.h"
#include "phase.h"

void c_init( struct codegen* phase, struct task* task ) {
   phase->task = task;
   // phase->main_lib = task->main_lib;
   // phase->libs = &task->libs;
   // phase->str_table = &task->str_table;
   // phase->compress = ( phase->main_lib->format == FORMAT_LITTLE_E );
   phase->compress = false;
   phase->block_visit = NULL;
   phase->block_visit_free = NULL;
   c_init_obj( phase );
}

void c_publish( struct codegen* phase ) {
   c_write_chunk_obj( phase );
   c_flush( phase );
}