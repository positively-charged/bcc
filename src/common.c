#include "mem.h"

#include "common.h"

void job_init_options( options_t* options ) {
   list_init( &options->include_paths );
   options->source_file = NULL;
   options->object_file = "output.o";
   options->format = k_format_little_e;
   options->encrypt_str = false;
   options->err_file = false;
}

void job_init_file_table( file_table_t* table, list_t* include_paths,
   const char* source_path ) {
   table->file_head = NULL;
   table->file = NULL;
   table->active_head = NULL;
   table->active = NULL;
   table->include_paths = include_paths;
}

void job_deinit_file_table( file_table_t* table ) {
   file_t* file = table->file_head;
   while ( file ) {
      str_del( &file->user_path );
      str_del( &file->load_path );
      file = file->next;
   }
}

file_t* job_active_file( file_table_t* table ) {
   if ( table->active ) {
      return table->active->file;
   }
   else {
      return NULL;
   }
}

void job_init_ast( ast_t* ast, ntbl_t* name_table ) {
   ast->module = NULL;
   ast->name_table = name_table;
   list_init( &ast->vars );
   list_init( &ast->arrays );
   list_init( &ast->funcs );
}