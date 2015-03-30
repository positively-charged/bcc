#include <string.h>
#include <limits.h>

#include "phase.h"

#define DEFAULT_SCRIPT_SIZE 20
#define STR_ENCRYPTION_CONSTANT 157135

static void do_sptr( struct codegen* phase );
static void do_svct( struct codegen* phase );
static void do_sflg( struct codegen* phase );
static void do_func( struct codegen* phase );
static void do_fnam( struct codegen* phase );
static void do_strl( struct codegen* phase );
static void do_mini( struct codegen* phase );
static void do_aray( struct codegen* phase );
static void do_aini( struct codegen* phase );
static void do_aini_single( struct codegen* phase, struct var* var );
static void do_load( struct codegen* phase );
static void do_mimp( struct codegen* phase );
static void do_aimp( struct codegen* phase );
static void do_mexp( struct codegen* phase );
static void do_mstr( struct codegen* phase );
static void do_astr( struct codegen* phase );
static void do_atag( struct codegen* codegen );
static void write_atagchunk( struct codegen* codegen, struct var* var );

void c_write_chunk_obj( struct codegen* phase ) {
   if ( phase->task->library_main->format == FORMAT_LITTLE_E ) {
      phase->compress = true;
   }
   // Reserve header.
   c_add_int( phase, 0 );
   c_add_int( phase, 0 );
   c_write_user_code( phase );
   int chunk_pos = c_tell( phase );
   do_sptr( phase );
   do_svct( phase );
   do_sflg( phase );
   do_func( phase );
   do_fnam( phase );
   do_strl( phase );
   do_mini( phase );
   do_aray( phase );
   do_aini( phase );
   do_load( phase );
   do_mimp( phase );
   do_aimp( phase );
   if ( phase->task->library_main->importable ) {
      do_mexp( phase );
      do_mstr( phase );
      do_astr( phase );
      do_atag( phase );
   }
   // NOTE: When a BEHAVIOR lump is below 32 bytes in size, the engine will not
   // process it. It will consider the lump as being of unknown format, even
   // though it appears to be valid, just empty. An unknown format is an error,
   // and will cause the engine to halt execution of other scripts. Pad up to
   // the minimum limit to avoid this situation.
   while ( c_tell( phase ) < 32 ) {
      c_add_byte( phase, 0 );
   }
   c_seek( phase, 0 );
   if ( phase->task->library_main->format == FORMAT_LITTLE_E ) {
      c_add_str( phase, "ACSe" );
   }
   else {
      c_add_str( phase, "ACSE" );
   }
   c_add_int( phase, chunk_pos );
   c_flush( phase );
}

void do_sptr( struct codegen* phase ) {
   if ( list_size( &phase->task->library_main->scripts ) ) {
      struct {
         short number;
         short type;
         int offset;
         int num_param;
      } entry;
      c_add_str( phase, "SPTR" );
      c_add_int( phase, sizeof( entry ) *
         list_size( &phase->task->library_main->scripts ) );
      list_iter_t i;
      list_iter_init( &i, &phase->task->library_main->scripts );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         entry.number = ( short ) t_get_script_number( script );
         entry.type = ( short ) script->type;
         entry.offset = script->offset;
         entry.num_param = script->num_param;
         c_add_sized( phase, &entry, sizeof( entry ) );
         list_next( &i );
      }
   }
}

void do_svct( struct codegen* phase ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->scripts );
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
      c_add_str( phase, "SVCT" );
      c_add_int( phase, sizeof( entry ) * count );
      list_iter_init( &i, &phase->task->library_main->scripts );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         if ( script->size > DEFAULT_SCRIPT_SIZE ) {
            entry.number = ( short ) t_get_script_number( script );
            entry.size = ( short ) script->size;
            c_add_sized( phase, &entry, sizeof( entry ) );
         }
         list_next( &i );
      }
   }
}

void do_sflg( struct codegen* phase ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->scripts );
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
      c_add_str( phase, "SFLG" );
      c_add_int( phase, sizeof( entry ) * count );
      list_iter_init( &i, &phase->task->library_main->scripts );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         if ( script->flags ) {
            entry.number = ( short ) t_get_script_number( script );
            entry.flags = ( short ) script->flags;
            c_add_sized( phase, &entry, sizeof( entry ) );
         }
         list_next( &i );
      }
   }
}

void do_func( struct codegen* phase ) {
   int count = list_size( &phase->task->library_main->funcs );
   // Imported functions:
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->dynamic );
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
   c_add_str( phase, "FUNC" );
   c_add_int( phase, sizeof( entry ) * count );
   // Imported functions:
   list_iter_init( &i, &phase->task->library_main->dynamic );
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
            entry.value = ( char ) ( func->return_type != NULL );
            c_add_sized( phase, &entry, sizeof( entry ) );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Visible functions:
   list_iter_init( &i, &phase->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      entry.params = ( char ) func->max_param;
      if ( func->min_param != func->max_param ) {
         ++entry.params;
      }
      entry.size = ( char ) ( impl->size - func->max_param );
      entry.value = ( char ) ( func->return_type != NULL );
      entry.offset = impl->obj_pos;
      c_add_sized( phase, &entry, sizeof( entry ) );
      list_next( &i );
   }
}

void do_fnam( struct codegen* phase ) {
   int count = 0;
   int size = 0;
   // Imported functions:
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->dynamic );
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
   list_iter_init( &i, &phase->task->library_main->funcs );
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
   c_add_str( phase, "FNAM" );
   c_add_int( phase, offset + size + padding );
   c_add_int( phase, count );
   // Offsets:
   // -----------------------------------------------------------------------
   // Imported functions.
   list_iter_init( &i, &phase->task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            c_add_int( phase, offset );
            offset += t_full_name_length( func->name ) + 1;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &phase->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      c_add_int( phase, offset );
      offset += t_full_name_length( func->name ) + 1;
      list_next( &i );
   }
   // Names:
   // -----------------------------------------------------------------------
   // Imported functions.
   struct str str;
   str_init( &str );
   list_iter_init( &i, &phase->task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            t_copy_name( func->name, true, &str );
            c_add_sized( phase, str.value, str.length + 1 );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &phase->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      t_copy_name( func->name, true, &str );
      c_add_sized( phase, str.value, str.length + 1 );
      list_next( &i );
   }
   str_deinit( &str );
   while ( padding ) {
      c_add_byte( phase, 0 );
      --padding;
   }
}

void do_strl( struct codegen* phase ) {
   int i = 0;
   int count = 0;
   int size = 0;
   struct indexed_string* string = phase->task->str_table.head_usable;
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
   if ( phase->task->library_main->encrypt_str ) {
      name = "STRE";
   }
   c_add_str( phase, name );
   c_add_int( phase, offset + size + padding );
   // String count.
   c_add_int( phase, 0 );
   c_add_int( phase, count );
   c_add_int( phase, 0 );
   // Offsets.
   i = 0;
   string = phase->task->str_table.head_usable;
   while ( i < count ) {
      if ( string->used ) {
         c_add_int( phase, offset );
         // Plus one for the NUL character.
         offset += string->length + 1;
      }
      else {
         c_add_int( phase, 0 );
      }
      string = string->next_usable;
      ++i;
   }
   // Strings.
   offset = offset_initial;
   string = phase->task->str_table.head_usable;
   while ( string ) {
      if ( string->used ) {
         if ( phase->task->library_main->encrypt_str ) {
            int key = offset * STR_ENCRYPTION_CONSTANT;
            // Each character of the string is encoded, including the NUL
            // character.
            for ( int i = 0; i <= string->length; ++i ) {
               char ch = ( char )
                  ( ( ( int ) string->value[ i ] ) ^ ( key + i / 2 ) );
               c_add_byte( phase, ch );
            }
            offset += string->length + 1;
         }
         else {
            c_add_sized( phase, string->value, string->length + 1 );
         }
      }
      string = string->next_usable;
   }
   while ( padding ) {
      c_add_byte( phase, 0 );
      --padding;
   }
}

void do_mini( struct codegen* phase ) {
   struct var* first_var = NULL;
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->vars );
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
   c_add_str( phase, "MINI" );
   c_add_int( phase, sizeof( int ) * ( count + 1 ) );
   c_add_int( phase, first_var->index );
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && var->value && var->value->expr->value ) {
         c_add_int( phase, var->value->expr->value );
      }
      list_next( &i );
   }
}

void do_aray( struct codegen* phase ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->vars );
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
   c_add_str( phase, "ARAY" );
   c_add_int( phase, sizeof( entry ) * count );
   // Arrays.
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && ! var->hidden ) { 
         entry.number = var->index;
         entry.size = var->size;
         c_add_sized( phase, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
   // Hidden arrays.
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && var->hidden ) {
         entry.number = var->index;
         entry.size = var->size;
         c_add_sized( phase, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
}

void do_aini( struct codegen* phase ) {
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && var->value ) {
         do_aini_single( phase, var );
      }
      list_next( &i );
   }
}

void do_aini_single( struct codegen* phase, struct var* var ) {
   int count = 0;
   struct value* value = var->value;
   while ( value ) {
      if ( value->string_initz ) {
         struct indexed_string_usage* usage =
            ( struct indexed_string_usage* ) value->expr->root;
         count = value->index + usage->string->length;
      }
      else {
         if ( value->expr->value ) {
            count = value->index + 1;
         }
      }
      value = value->next;
   }
   if ( ! count ) {
      return;
   }
   c_add_str( phase, "AINI" );
   c_add_int( phase, sizeof( int ) + sizeof( int ) * count );
   c_add_int( phase, var->index );
   count = 0;
   value = var->value;
   while ( value ) {
      // Nullify uninitialized space.
      if ( count < value->index && ( ( value->expr->value &&
         ! value->string_initz ) || value->string_initz ) ) {
         c_add_int_zero( phase, value->index - count );
         count = value->index;
      }
      if ( value->string_initz ) {
         struct indexed_string_usage* usage =
            ( struct indexed_string_usage* ) value->expr->root;
         for ( int i = 0; i < usage->string->length; ++i ) {
            c_add_int( phase, usage->string->value[ i ] );
         }
         count += usage->string->length;
      }
      else {
         if ( value->expr->value ) {
            c_add_int( phase, value->expr->value );
            ++count;
         }
      }
      value = value->next;
   }
}

void do_load( struct codegen* phase ) {
   int size = 0;
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      size += lib->name.length + 1;
      list_next( &i );
   }
   if ( size ) {
      int padding = alignpad( size, 4 );
      c_add_str( phase, "LOAD" );
      c_add_int( phase, size + padding );
      list_iter_init( &i, &phase->task->library_main->dynamic );
      while ( ! list_end( &i ) ) {
         struct library* lib = list_data( &i );
         c_add_sized( phase, lib->name.value, lib->name.length + 1 );
         list_next( &i );
      }
      while ( padding ) {
         c_add_byte( phase, 0 );
         --padding;
      }
   }
}

// NOTE: This chunk might cause any subsequent chunk to be misaligned.
void do_mimp( struct codegen* phase ) {
   int size = 0;
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->dynamic );
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
   c_add_str( phase, "MIMP" );
   c_add_int( phase, size );
   struct str str;
   str_init( &str );
   list_iter_init( &i, &phase->task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->used && ! var->dim &&
            var->type->primitive ) {
            c_add_int( phase, var->index );
            t_copy_name( var->name, true, &str );
            c_add_sized( phase, str.value, str.length + 1 );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   str_deinit( &str );
}

// NOTE: This chunk might cause any subsequent chunk to be misaligned.
void do_aimp( struct codegen* phase ) {
   int count = 0;
   int size = sizeof( int );
   list_iter_t i;
   list_iter_init( &i, &phase->task->libraries );
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
   c_add_str( phase, "AIMP" );
   c_add_int( phase, size );
   c_add_int( phase, count );
   struct str str;
   str_init( &str );
   list_iter_init( &i, &phase->task->libraries );
   list_next( &i );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->used &&
            ( ! var->type->primitive || var->dim ) ) {
            c_add_int( phase, var->index );
            c_add_int( phase, var->size );
            t_copy_name( var->name, true, &str );
            c_add_sized( phase, str.value, str.length + 1 );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   str_deinit( &str );
}

void do_mexp( struct codegen* phase ) {
   int count = 0;
   int size = 0;
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->vars );
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
   c_add_str( phase, "MEXP" );
   c_add_int( phase, offset + size + padding );
   // Number of variables.
   c_add_int( phase, count );
   // Offsets
   // -----------------------------------------------------------------------
   // Arrays.
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && ! var->hidden ) {
         c_add_int( phase, offset );
         offset += t_full_name_length( var->name ) + 1;
      }
      list_next( &i );
   }
   // Scalars, with-no-value.
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden &&
         ( ! var->value || ! var->value->expr->value ) ) {
         c_add_int( phase, offset );
         offset += t_full_name_length( var->name ) + 1;
      }
      list_next( &i );
   }
   // Scalars, with-value.
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden && var->value &&
         var->value->expr->value ) {
         c_add_int( phase, offset );
         offset += t_full_name_length( var->name ) + 1;
      }
      list_next( &i );
   }
   // Name
   // -----------------------------------------------------------------------
   struct str str;
   str_init( &str );
   // Arrays.
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP &&
         ( var->dim || ! var->type->primitive ) && ! var->hidden ) {
         t_copy_name( var->name, true, &str );
         c_add_sized( phase, str.value, str.length + 1 );
      }
      list_next( &i );
   }
   // Scalars, with-no-value.
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden &&
         ( ! var->value || ! var->value->expr->value ) ) {
         t_copy_name( var->name, true, &str );
         c_add_sized( phase, str.value, str.length + 1 );
      }
      list_next( &i );
   }
   // Scalars, with-value.
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && ! var->hidden && var->value &&
         var->value->expr->value ) {
         t_copy_name( var->name, true, &str );
         c_add_sized( phase, str.value, str.length + 1 );
      }
      list_next( &i );
   }
   str_deinit( &str );
   while ( padding ) {
      c_add_byte( phase, 0 );
      --padding;
   }
}

void do_mstr( struct codegen* phase ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim &&
         var->type->primitive && var->initial_has_str ) {
         ++count;
      }
      list_next( &i );
   }
   if ( count ) {
      c_add_str( phase, "MSTR" );
      c_add_int( phase, sizeof( int ) * count );
      list_iter_init( &i, &phase->task->library_main->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->dim &&
            var->type->primitive && var->initial_has_str ) {
            c_add_int( phase, var->index );
         }
         list_next( &i );
      }
   }
}

void do_astr( struct codegen* phase ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->dim &&
         var->type->primitive && var->initial_has_str ) {
         ++count;
      }
      list_next( &i );
   }
   if ( count ) {
      c_add_str( phase, "ASTR" );
      c_add_int( phase, sizeof( int ) * count );
      list_iter_init( &i, &phase->task->library_main->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && var->dim &&
            var->type->primitive && var->initial_has_str ) {
            c_add_int( phase, var->index );
         }
         list_next( &i );
      }
   }
}

void do_atag( struct codegen* codegen ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &codegen->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->type->primitive &&
         var->initial_has_str ) {
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   list_iter_init( &i, &codegen->task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->type->primitive &&
         var->initial_has_str ) {
         write_atagchunk( codegen, var );
      }
      list_next( &i );
   }
}

void write_atagchunk( struct codegen* codegen, struct var* var ) {
   enum { CHUNK_VERSION = 0 };
   enum {
      TAG_INTEGER,
      TAG_STRING,
      TAG_FUNCTION
   };
   int count = 0;
   struct value* value = var->value;
   while ( value ) {
      if ( value->expr->has_str && ! value->string_initz ) {
         count = value->index + 1;
      }
      value = value->next;
   }
   if ( ! count ) {
      return;
   }
   c_add_str( codegen, "ATAG" );
   c_add_int( codegen,
      // Version.
      sizeof( char ) +
      // Array number.
      sizeof( int ) +
      // Number of elements to tag.
      sizeof( char ) * count );
   c_add_byte( codegen, CHUNK_VERSION );
   c_add_int( codegen, var->index );
   value = var->value;
   int written = 0;
   while ( written < count ) {
      if ( value->expr->has_str && ! value->string_initz ) {
         while ( written < value->index ) {
            c_add_byte( codegen, TAG_INTEGER );
            ++written;
         }
         c_add_byte( codegen, TAG_STRING );
         ++written;
      }
      value = value->next;
   }
}