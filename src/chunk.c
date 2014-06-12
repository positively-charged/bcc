#include <string.h>
#include <limits.h>

#include "task.h"

#define MAX_MAP_LOCATIONS 128
#define DEFAULT_SCRIPT_SIZE 20
#define STR_ENCRYPTION_CONSTANT 157135

static void alloc_index( struct task* );
static void do_sptr( struct task* );
static void do_svct( struct task* );
static void do_sflg( struct task* );
static void do_func( struct task* );
static void do_fnam( struct task* );
static void do_strl( struct task* );
static void do_mini( struct task* );
static void do_aray( struct task* );
static void do_aini( struct task* );
static void do_aini_single( struct task*, struct var* );
static void do_load( struct task* );
static void do_mimp( struct task* );
static void do_aimp( struct task* );
static void do_mexp( struct task* );
static void do_mstr( struct task* );
static void do_astr( struct task* );

void t_init_fields_chunk( struct task* task ) {
   task->compress = false;
   task->block_walk = NULL;
   task->block_walk_free = NULL;
}

void t_publish( struct task* task ) {
   alloc_index( task );
   if ( task->library_main->format == FORMAT_LITTLE_E ) {
      task->compress = true;
   }
   // Reserve header.
   t_add_int( task, 0 );
   t_add_int( task, 0 );
   t_publish_usercode( task );
   int chunk_pos = t_tell( task );
   do_sptr( task );
   do_svct( task );
   do_sflg( task );
   do_func( task );
   do_fnam( task );
   do_strl( task );
   do_mini( task );
   do_aray( task );
   do_aini( task );
   do_load( task );
   do_mimp( task );
   do_aimp( task );
   if ( task->library_main->importable ) {
      do_mexp( task );
      do_mstr( task );
      do_astr( task );
   }
   // NOTE: When a BEHAVIOR lump is below 32 bytes in size, the engine will not
   // process it. It will consider the lump as being of unknown format, even
   // though it appears to be valid, just empty. An unknown format is an error,
   // and will cause the engine to halt execution of other scripts. Pad up to
   // the minimum limit to avoid this situation.
   while ( t_tell( task ) < 32 ) {
      t_add_byte( task, 0 );
   }
   t_seek( task, 0 );
   if ( task->library_main->format == FORMAT_LITTLE_E ) {
      t_add_str( task, "ACSe" );
   }
   else {
      t_add_str( task, "ACSE" );
   }
   t_add_int( task, chunk_pos );
   t_flush( task );
}

void alloc_index( struct task* task ) {
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
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && ! var->hidden ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Scalars, with-no-value.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden &&
         ( ! var->value || ! var->value->expr->value ) ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Scalars, with-value.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden && var->value &&
         var->value->expr->value ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Scalars, with-value, hidden.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && var->hidden && var->value &&
         var->value->expr->value ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Scalars, with-no-value, hidden.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && var->hidden &&
         ( ! var->value || ! var->value->expr->value ) ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Arrays, hidden.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && var->hidden ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Imported.
   list_iter_init( &i, &task->library_main->dynamic );
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
      t_diag( task, DIAG_ERR | DIAG_FILE, &task->library_main->file_pos,
         "library uses over maximum %d variables", MAX_MAP_LOCATIONS );
      t_bail( task );
   }
   // Functions:
   // -----------------------------------------------------------------------
   index = 0;
   // Imported functions:
   list_iter_init( &i, &task->library_main->dynamic );
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
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( ! func->hidden ) {
         struct func_user* impl = func->impl;
         impl->index = index;
         ++index;
      }
      list_next( &i );
   }
   // In Little-E, the field of the function-call instruction that stores the
   // index of the function is a byte in size, allowing up to 256 different
   // functions to be called.
   // NOTE: Maybe automatically switch to the Big-E format? 
   if ( task->library_main->format == FORMAT_LITTLE_E && index > 256 ) {
      t_diag( task, DIAG_ERR | DIAG_FILE, &task->library_main->file_pos,
         "library uses over maximum 256 functions" );
      t_diag( task, DIAG_FILE, &task->library_main->file_pos,
         "to use more functions, try using the #nocompact directive" );
      t_bail( task );
   }
}

void do_sptr( struct task* task ) {
   if ( list_size( &task->library_main->scripts ) ) {
      struct {
         short number;
         short type;
         int offset;
         int num_param;
      } entry;
      t_add_str( task, "SPTR" );
      t_add_int( task, sizeof( entry ) *
         list_size( &task->library_main->scripts ) );
      list_iter_t i;
      list_iter_init( &i, &task->library_main->scripts );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         entry.number = ( short ) t_get_script_number( script );
         entry.type = ( short ) script->type;
         entry.offset = script->offset;
         entry.num_param = script->num_param;
         t_add_sized( task, &entry, sizeof( entry ) );
         list_next( &i );
      }
   }
}

void do_svct( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->size > DEFAULT_SCRIPT_SIZE ) {
         ++count;
      }
      list_next( &i );
   }
   if ( count ) {
      struct {
         short number;
         short size;
      } entry;
      t_add_str( task, "SVCT" );
      t_add_int( task, sizeof( entry ) * count );
      list_iter_init( &i, &task->library_main->scripts );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         if ( script->size > DEFAULT_SCRIPT_SIZE ) {
            entry.number = ( short ) t_get_script_number( script );
            entry.size = ( short ) script->size;
            t_add_sized( task, &entry, sizeof( entry ) );
         }
         list_next( &i );
      }
   }
}

void do_sflg( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->flags ) {
         ++count;
      }
      list_next( &i );
   }
   if ( count ) {
      struct {
         short number;
         short flags;
      } entry;
      t_add_str( task, "SFLG" );
      t_add_int( task, sizeof( entry ) * count );
      list_iter_init( &i, &task->library_main->scripts );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         if ( script->flags ) {
            entry.number = ( short ) t_get_script_number( script );
            entry.flags = ( short ) script->flags;
            t_add_sized( task, &entry, sizeof( entry ) );
         }
         list_next( &i );
      }
   }
}

void do_func( struct task* task ) {
   int count = list_size( &task->library_main->funcs );
   // Imported functions:
   list_iter_t i;
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            ++count;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   struct  {
      char params;
      char size;
      char value;
      char padding;
      int offset;
   } entry;
   entry.padding = 0;
   t_add_str( task, "FUNC" );
   t_add_int( task, sizeof( entry ) * count );
   // Imported functions:
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
      entry.size = 0;
      entry.offset = 0;
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            entry.params = ( char ) func->max_param;
            // In functions with optional parameters, a hidden parameter used
            // to store the number of arguments passed, is found after the
            // last function parameter.
            if ( func->min_param != func->max_param ) {
               ++entry.params;
            }
            entry.value = ( char ) ( func->value != NULL );
            t_add_sized( task, &entry, sizeof( entry ) );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Visible functions:
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      entry.params = ( char ) func->max_param;
      if ( func->min_param != func->max_param ) {
         ++entry.params;
      }
      entry.size = ( char ) impl->size;
      entry.value = ( char ) ( func->value != NULL );
      entry.offset = impl->obj_pos;
      t_add_sized( task, &entry, sizeof( entry ) );
      list_next( &i );
   }
}

void do_fnam( struct task* task ) {
   int count = 0;
   int size = 0;
   // Imported functions:
   list_iter_t i;
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            size += t_full_name_length( func->name ) + 1;
            ++count;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Functions:
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      size += t_full_name_length( func->name ) + 1;
      ++count;
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   int offset =
      sizeof( int ) +
      sizeof( int ) * count;
   int padding = alignpad( offset + size, 4 );
   t_add_str( task, "FNAM" );
   t_add_int( task, offset + size + padding );
   t_add_int( task, count );
   // Offsets:
   // -----------------------------------------------------------------------
   // Imported functions.
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            t_add_int( task, offset );
            offset += t_full_name_length( func->name ) + 1;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      t_add_int( task, offset );
      offset += t_full_name_length( func->name ) + 1;
      list_next( &i );
   }
   // Names:
   // -----------------------------------------------------------------------
   // Imported functions.
   struct str str;
   str_init( &str );
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            t_copy_name( func->name, true, &str );
            t_add_sized( task, str.value, str.length + 1 );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      t_copy_name( func->name, true, &str );
      t_add_sized( task, str.value, str.length + 1 );
      list_next( &i );
   }
   str_deinit( &str );
   while ( padding ) {
      t_add_byte( task, 0 );
      --padding;
   }
}

void do_strl( struct task* task ) {
   int i = 0;
   int count = 0;
   int size = 0;
   struct indexed_string* string = task->str_table.head_usable;
   while ( string ) {
      ++i;
      if ( string->used ) {
         // Plus one for the NUL character.
         size += string->length + 1;
         count = i;
      }
      string = string->next_usable;
   }
   if ( ! count ) {
      return;
   }
   int offset = 
      // String count, padded with a zero on each size.
      sizeof( int ) * 3 +
      // String offsets.
      sizeof( int ) * count;
   int padding = alignpad( offset + size, 4 );
   int offset_initial = offset;
   const char* name = "STRL";
   if ( task->library_main->encrypt_str ) {
      name = "STRE";
   }
   t_add_str( task, name );
   t_add_int( task, offset + size + padding );
   // String count.
   t_add_int( task, 0 );
   t_add_int( task, count );
   t_add_int( task, 0 );
   // Offsets.
   i = 0;
   string = task->str_table.head_usable;
   while ( i < count ) {
      if ( string->used ) {
         t_add_int( task, offset );
         // Plus one for the NUL character.
         offset += string->length + 1;
      }
      else {
         t_add_int( task, 0 );
      }
      string = string->next_usable;
      ++i;
   }
   // Strings.
   offset = offset_initial;
   string = task->str_table.head_usable;
   while ( string ) {
      if ( string->used ) {
         if ( task->library_main->encrypt_str ) {
            int key = offset * STR_ENCRYPTION_CONSTANT;
            // Each character of the string is encoded, including the NUL
            // character.
            for ( int i = 0; i <= string->length; ++i ) {
               char ch = ( char )
                  ( ( ( int ) string->value[ i ] ) ^ ( key + i / 2 ) );
               t_add_byte( task, ch );
            }
            offset += string->length + 1;
         }
         else {
            t_add_sized( task, string->value, string->length + 1 );
         }
      }
      string = string->next_usable;
   }
   while ( padding ) {
      t_add_byte( task, 0 );
      --padding;
   }
}

void do_mini( struct task* task ) {
   struct var* first_var = NULL;
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && var->value && var->value->expr->value ) {
         if ( ! first_var ) {
            first_var = var;
         }
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   t_add_str( task, "MINI" );
   t_add_int( task, sizeof( int ) * ( count + 1 ) );
   t_add_int( task, first_var->index );
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && var->value && var->value->expr->value ) {
         t_add_int( task, var->value->expr->value );
      }
      list_next( &i );
   }
}

void do_aray( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) ) {
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   struct {
      int number;
      int size;
   } entry;
   t_add_str( task, "ARAY" );
   t_add_int( task, sizeof( entry ) * count );
   // Arrays.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && ! var->hidden ) { 
         entry.number = var->index;
         entry.size = var->size;
         t_add_sized( task, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
   // Hidden arrays.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && var->hidden ) {
         entry.number = var->index;
         entry.size = var->size;
         t_add_sized( task, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
}

void do_aini( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && var->value ) {
         do_aini_single( task, var );
      }
      list_next( &i );
   }
}

void do_aini_single( struct task* task, struct var* var ) {
   int count = 0;
   struct value* value = var->value;
   while ( value ) {
      if ( value->expr->value ) {
         count = value->index + 1;
      }
      value = value->next;
   }
   if ( count ) {
      t_add_str( task, "AINI" );
      t_add_int( task, sizeof( int ) + sizeof( int ) * count );
      t_add_int( task, var->index );
      count = 0;
      value = var->value;
      while ( value ) {
         if ( value->expr->value ) {
            if ( count < value->index ) {
               t_add_int_zero( task, value->index - count );
               count = value->index;
            }
            t_add_int( task, value->expr->value );
            ++count;
         }
         value = value->next;
      }
   }
}

void do_load( struct task* task ) {
   int size = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      size += lib->name.length + 1;
      list_next( &i );
   }
   if ( size ) {
      int padding = alignpad( size, 4 );
      t_add_str( task, "LOAD" );
      t_add_int( task, size + padding );
      list_iter_init( &i, &task->library_main->dynamic );
      while ( ! list_end( &i ) ) {
         struct library* lib = list_data( &i );
         t_add_sized( task, lib->name.value, lib->name.length + 1 );
         list_next( &i );
      }
      while ( padding ) {
         t_add_byte( task, 0 );
         --padding;
      }
   }
}

// NOTE: This chunk might cause any subsequent chunk to be misaligned.
void do_mimp( struct task* task ) {
   int size = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->used && ! var->dim &&
            var->type->primitive ) {
            size += sizeof( int ) + t_full_name_length( var->name ) + 1;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   if ( ! size ) {
      return;
   }
   t_add_str( task, "MIMP" );
   t_add_int( task, size );
   struct str str;
   str_init( &str );
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->used && ! var->dim &&
            var->type->primitive ) {
            t_add_int( task, var->index );
            t_copy_name( var->name, true, &str );
            t_add_sized( task, str.value, str.length + 1 );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   str_deinit( &str );
}

// NOTE: This chunk might cause any subsequent chunk to be misaligned.
void do_aimp( struct task* task ) {
   int count = 0;
   int size = sizeof( int );
   list_iter_t i;
   list_iter_init( &i, &task->libraries );
   list_next( &i );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->used &&
            ( var->dim || ! var->type->primitive ) ) {
            size +=
               // Array index.
               sizeof( int ) +
               // Array size.
               sizeof( int ) +
               // Name of array.
               t_full_name_length( var->name ) + 1;
            ++count;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   t_add_str( task, "AIMP" );
   t_add_int( task, size );
   t_add_int( task, count );
   struct str str;
   str_init( &str );
   list_iter_init( &i, &task->libraries );
   list_next( &i );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->used &&
            ( ! var->type->primitive || var->dim ) ) {
            t_add_int( task, var->index );
            t_add_int( task, var->size );
            t_copy_name( var->name, true, &str );
            t_add_sized( task, str.value, str.length + 1 );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   str_deinit( &str );
}

void do_mexp( struct task* task ) {
   int count = 0;
   int size = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->hidden ) {
         size += t_full_name_length( var->name ) + 1;
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   int offset =
      // Number of variables. A zero is NOT padded on each side.
      sizeof( int ) +
      sizeof( int ) * count;
   int padding = alignpad( offset + size, 4 );
   t_add_str( task, "MEXP" );
   t_add_int( task, offset + size + padding );
   // Number of variables.
   t_add_int( task, count );
   // Offsets
   // -----------------------------------------------------------------------
   // Arrays.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && ! var->hidden ) {
         t_add_int( task, offset );
         offset += t_full_name_length( var->name ) + 1;
      }
      list_next( &i );
   }
   // Scalars, with-no-value.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden &&
         ( ! var->value || ! var->value->expr->value ) ) {
         t_add_int( task, offset );
         offset += t_full_name_length( var->name ) + 1;
      }
      list_next( &i );
   }
   // Scalars, with-value.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden && var->value &&
         var->value->expr->value ) {
         t_add_int( task, offset );
         offset += t_full_name_length( var->name ) + 1;
      }
      list_next( &i );
   }
   // Name
   // -----------------------------------------------------------------------
   struct str str;
   str_init( &str );
   // Arrays.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && ! var->hidden ) {
         t_copy_name( var->name, true, &str );
         t_add_sized( task, str.value, str.length + 1 );
      }
      list_next( &i );
   }
   // Scalars, with-no-value.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden &&
         ( ! var->value || ! var->value->expr->value ) ) {
         t_copy_name( var->name, true, &str );
         t_add_sized( task, str.value, str.length + 1 );
      }
      list_next( &i );
   }
   // Scalars, with-value.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden && var->value &&
         var->value->expr->value ) {
         t_copy_name( var->name, true, &str );
         t_add_sized( task, str.value, str.length + 1 );
      }
      list_next( &i );
   }
   str_deinit( &str );
   while ( padding ) {
      t_add_byte( task, 0 );
      --padding;
   }
}

void do_mstr( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && var->initial_has_str ) {
         ++count;
      }
      list_next( &i );
   }
   if ( count ) {
      t_add_str( task, "MSTR" );
      t_add_int( task, sizeof( int ) * count );
      list_iter_init( &i, &task->library_main->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->dim &&
            var->type->primitive && var->initial_has_str ) {
            t_add_int( task, var->index );
         }
         list_next( &i );
      }
   }
}

void do_astr( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->dim &&
         var->type->primitive && var->initial_has_str ) {
         ++count;
      }
      list_next( &i );
   }
   if ( count ) {
      t_add_str( task, "ASTR" );
      t_add_int( task, sizeof( int ) * count );
      list_iter_init( &i, &task->library_main->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && var->dim &&
            var->type->primitive && var->initial_has_str ) {
            t_add_int( task, var->index );
         }
         list_next( &i );
      }
   }
}