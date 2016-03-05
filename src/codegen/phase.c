#include <string.h>

#include "task.h"
#include "phase.h"

#define MAX_MAP_LOCATIONS 128

static void alloc_mapvars_index( struct codegen* codegen );
static void create_assert_strings( struct codegen* codegen );

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
   codegen->assert_prefix = NULL;
   codegen->runtime_index = 0;
}

void c_publish( struct codegen* codegen ) {
   alloc_mapvars_index( codegen );
   if ( codegen->task->options->write_asserts ) {
      create_assert_strings( codegen );
   }
   c_write_chunk_obj( codegen );
   c_flush( codegen );
}

void alloc_mapvars_index( struct codegen* codegen ) {
   // Variables:
   // 
   // Order of allocation:
   // - arrays
   // - scalars, with-no-value
   // - scalars, with-value
   // - scalars, with-value, hidden
   // - scalars, with-no-value, hidden
   // - arrays, hidden
   // - imported
   //
   // -----------------------------------------------------------------------
   // Arrays.
   int index = 0;
   list_iter_t i;
   list_iter_init( &i, &codegen->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || var->structure ) && ! var->hidden ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Scalars, with-no-value.
   list_iter_init( &i, &codegen->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         ! var->structure && ! var->hidden &&
         ( ! var->value || ! var->value->expr->value ) ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Scalars, with-value.
   list_iter_init( &i, &codegen->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         ! var->structure && ! var->hidden && var->value &&
         var->value->expr->value ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Scalars, with-value, hidden.
   list_iter_init( &i, &codegen->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         ! var->structure && var->hidden && var->value &&
         var->value->expr->value ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Scalars, with-no-value, hidden.
   list_iter_init( &i, &codegen->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         ! var->structure && var->hidden &&
         ( ! var->value || ! var->value->expr->value ) ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Arrays, hidden.
   list_iter_init( &i, &codegen->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || var->structure ) && var->hidden ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Imported.
   list_iter_init( &i, &codegen->task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->used ) {
            var->index = index;
            ++index;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Don't go over the variable limit.
   if ( index > MAX_MAP_LOCATIONS ) {
      t_diag( codegen->task, DIAG_ERR | DIAG_FILE,
         &codegen->task->library_main->file_pos,
         "library uses over maximum %d variables", MAX_MAP_LOCATIONS );
      t_bail( codegen->task );
   }
   // Functions:
   // -----------------------------------------------------------------------
   index = 0;
   // Imported functions:
   list_iter_init( &i, &codegen->task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            impl->index = index;
            ++index;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Functions:
   list_iter_init( &i, &codegen->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      if ( ! impl->hidden ) {
         impl->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Hidden functions:
   list_iter_init( &i, &codegen->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      if ( impl->hidden ) {
         impl->index = index;
         ++index;
      }
      list_next( &i );
   }
   // In Little-E, the field of the function-call instruction that stores the
   // index of the function is a byte in size, allowing up to 256 different
   // functions to be called.
   // NOTE: Maybe automatically switch to the Big-E format? 
   if ( codegen->task->library_main->format == FORMAT_LITTLE_E && index > 256 ) {
      t_diag( codegen->task, DIAG_ERR | DIAG_FILE,
         &codegen->task->library_main->file_pos,
         "library uses over maximum 256 functions" );
      t_diag( codegen->task, DIAG_FILE, &codegen->task->library_main->file_pos,
         "to use more functions, try using the #nocompact directive" );
      t_bail( codegen->task );
   }
}

void create_assert_strings( struct codegen* codegen ) {
   list_iter_t i;
   list_iter_init( &i, &codegen->task->runtime_asserts );
   while ( ! list_end( &i ) ) {
      struct assert* assert = list_data( &i );
      // Create file string.
      const char* file = t_decode_pos_file( codegen->task, &assert->pos );
      struct indexed_string* string = t_intern_string( codegen->task,
         ( char* ) file, strlen( file ) );
      string->used = true;
      assert->file = string;
      // Create custom message string. 
      if ( assert->custom_message ) {
         string = t_intern_string( codegen->task, assert->custom_message,
            strlen( assert->custom_message ) );
         string->used = true;
         assert->message = string;
      }
      list_next( &i );
   }
   // Create standard message-prefix string.
   static const char* message_prefix = "assertion failure";
   codegen->assert_prefix = t_intern_string( codegen->task,
      ( char* ) message_prefix, strlen( message_prefix ) );
   codegen->assert_prefix->used = true;
}