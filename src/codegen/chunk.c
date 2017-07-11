#include <string.h>
#include <limits.h>

#include "phase.h"

#define STR_ENCRYPTION_CONSTANT 157135

struct value_writing {
   int count;
};

struct atag_writing {
   struct var* var;
   struct value* value;
   struct list_iter iter;
   int index;
   int base;
   bool shary;
};

static void do_sptr( struct codegen* codegen );
static void do_svct( struct codegen* codegen );
static bool svct_script( struct script* script );
static void do_sflg( struct codegen* codegen );
static void do_snam( struct codegen* codegen );
static void do_func( struct codegen* codegen );
static void do_fnam( struct codegen* codegen );
static void do_strl( struct codegen* codegen );
static void do_mini( struct codegen* codegen );
static void write_mini_value( struct codegen* codegen, struct value* value );
static void do_aray( struct codegen* codegen );
static void do_aini( struct codegen* codegen );
static void write_aini( struct codegen* codegen, struct var* var );
static int count_nonzero_value( struct codegen* codegen,
   struct var* var );
static bool is_zero_value( struct codegen* codegen, struct value* value );
static bool is_nonzero_value( struct codegen* codegen, struct value* value );
static int get_value_size( struct codegen* codegen, struct value* value );
static void init_value_writing( struct value_writing* writing );
static void write_value_list( struct codegen* codegen,
   struct value_writing* writing, struct value* value );
static void write_multi_value_list( struct codegen* codegen,
   struct value_writing* writing, struct value* value, int base );
static void write_value( struct codegen* codegen,
   struct value_writing* writing, struct value* value, int base );
static void write_aini_shary( struct codegen* codegen );
static int count_shary_initz( struct codegen* codegen );
static void write_diminfo( struct codegen* codegen );
static void do_load( struct codegen* codegen );
static void do_mimp( struct codegen* codegen );
static void do_aimp( struct codegen* codegen );
static void do_mexp( struct codegen* codegen );
static void do_mstr( struct codegen* codegen );
static bool mstr_var( struct var* var );
static void do_astr( struct codegen* codegen );
static bool astr_var( struct var* var );
static void do_atag( struct codegen* codegen );
static void init_atag_writing_var( struct atag_writing* writing,
   struct var* var );
static void init_atag_writing_shary( struct codegen* codegen,
   struct atag_writing* writing );
static void init_atag_writing( struct atag_writing* writing, int index );
static void iterate_atag( struct codegen* codegen,
   struct atag_writing* writing );
static void next_atag( struct atag_writing* writing );
static void write_atag_chunk( struct codegen* codegen,
   struct atag_writing* writing );
static void do_sary( struct codegen* codegen );
static void write_sary_chunk( struct codegen* codegen, const char* chunk_name,
   int index, struct list* vars );
static bool script_array( struct var* var );
static void do_alib( struct codegen* codegen );

void c_write_chunk_obj( struct codegen* codegen ) {
   if ( codegen->task->library_main->format == FORMAT_LITTLE_E ) {
      codegen->compress = true;
   }
   // Reserve header.
   c_add_int( codegen, 0 );
   c_add_int( codegen, 0 );
   c_write_user_code( codegen );
   int chunk_pos = c_tell( codegen );
   do_sptr( codegen );
   do_svct( codegen );
   do_sflg( codegen );
   do_snam( codegen );
   do_func( codegen );
   do_fnam( codegen );
   do_strl( codegen );
   do_mini( codegen );
   do_aray( codegen );
   do_aini( codegen );
   do_load( codegen );
   do_mimp( codegen );
   do_aimp( codegen );
   do_sary( codegen );
   if ( codegen->task->library_main->importable ) {
      do_mexp( codegen );
      do_mstr( codegen );
      do_astr( codegen );
      do_atag( codegen );
      do_alib( codegen );
   }
   c_add_int( codegen, chunk_pos );
   c_add_str( codegen, codegen->compress ? "ACSe" : "ACSE" );
   int dummy_offset = c_tell( codegen );
   // Write dummy scripts.
   if ( codegen->task->library_main->wadauthor ) {
      int count = 0;
      struct list_iter i;
      list_iterate( &codegen->task->library_main->scripts, &i );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         if ( script->assigned_number >= 0 &&
            script->assigned_number <= 255 ) {
            ++count;
         }
         list_next( &i );
      }
      c_add_int( codegen, count );
      list_iterate( &codegen->task->library_main->scripts, &i );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         if ( script->assigned_number >= 0 &&
            script->assigned_number <= 255 ) {
            c_add_int( codegen, script->assigned_number );
            c_add_int( codegen, codegen->dummy_script_offset );
            c_add_int( codegen, script->num_param );
         }
         list_next( &i );
      }
   }
   else {
      c_add_int( codegen, 0 );
   }
   // Write dummy strings.
   c_add_int( codegen, 0 );
   // NOTE: When a BEHAVIOR lump is below 32 bytes in size, the engine will not
   // process it. It will consider the lump as being of unknown format, even
   // though it appears to be valid, just empty. An unknown format is an error,
   // and will cause the engine to halt execution of other scripts. Pad up to
   // the minimum limit to avoid this situation.
   while ( c_tell( codegen ) < 32 ) {
      c_add_byte( codegen, 0 );
   }
   // Write header.
   c_seek( codegen, 0 );
   c_add_sized( codegen, "ACS\0", 4 );
   c_add_int( codegen, dummy_offset );
   c_flush( codegen );
}

static void do_sptr( struct codegen* codegen ) {
   if ( ! list_size( &codegen->task->library_main->scripts ) ) {
      return;
   }
   struct {
      short number;
      char type;
      char num_param;
      int offset;
   } entry;
   c_add_str( codegen, "SPTR" );
   c_add_int( codegen, sizeof( entry ) *
      list_size( &codegen->task->library_main->scripts ) );
   struct list_iter i;
   list_iterate( &codegen->task->library_main->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      entry.number = ( short ) script->assigned_number;
      entry.type = ( char ) script->type;
      entry.num_param = ( char ) script->num_param;
      entry.offset = script->offset;
      c_add_sized( codegen, &entry, sizeof( entry ) );
      list_next( &i );
   }
}

static void do_svct( struct codegen* codegen ) {
   int count = 0;
   struct list_iter i;
   list_iterate( &codegen->task->library_main->scripts, &i );
   while ( ! list_end( &i ) ) {
      count += ( int ) svct_script( list_data( &i ) );
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   struct {
      short number;
      short size;
   } entry;
   c_add_str( codegen, "SVCT" );
   c_add_int( codegen, sizeof( entry ) * count );
   list_iterate( &codegen->task->library_main->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( svct_script( script ) ) {
         entry.number = ( short ) script->assigned_number;
         entry.size = ( short ) script->size;
         c_add_sized( codegen, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
}

inline static bool svct_script( struct script* script ) {
   enum { DEFAULT_SCRIPT_SIZE = 20 };
   return ( script->size > DEFAULT_SCRIPT_SIZE );
}

static void do_sflg( struct codegen* codegen ) {
   int count = 0;
   struct list_iter i;
   list_iterate( &codegen->task->library_main->scripts, &i );
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
      c_add_str( codegen, "SFLG" );
      c_add_int( codegen, sizeof( entry ) * count );
      list_iterate( &codegen->task->library_main->scripts, &i );
      while ( ! list_end( &i ) ) {
         struct script* script = list_data( &i );
         if ( script->flags ) {
            entry.number = ( short ) script->assigned_number;
            entry.flags = ( short ) script->flags;
            c_add_sized( codegen, &entry, sizeof( entry ) );
         }
         list_next( &i );
      }
   }
}

static void do_snam( struct codegen* codegen ) {
   int count = 0;
   int total_length = 0;
   struct list_iter i;
   list_iterate( &codegen->task->library_main->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->named_script ) {
         struct indexed_string* name = t_lookup_string( codegen->task,
            script->number->value );
         total_length += name->length + 1;
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   int size =
      sizeof( int ) +
      sizeof( int ) * count +
      total_length;
   int padding = alignpad( size, 4 );
   size += padding;
   c_add_str( codegen, "SNAM" );
   c_add_int( codegen, size );
   c_add_int( codegen, count );
   // Offsets
   // -----------------------------------------------------------------------
   list_iterate( &codegen->task->library_main->scripts, &i );
   int offset = sizeof( int ) + sizeof( int ) * count;
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->named_script ) {
         c_add_int( codegen, offset );
         struct indexed_string* name = t_lookup_string( codegen->task,
            script->number->value );
         offset += name->length + 1;
      }
      list_next( &i );
   }
   // Text
   // -----------------------------------------------------------------------
   list_iterate( &codegen->task->library_main->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->named_script ) {
         struct indexed_string* name = t_lookup_string( codegen->task,
            script->number->value );
         c_add_sized( codegen, name->value, name->length + 1 );
      }
      list_next( &i );
   }
   while ( padding ) {
      c_add_byte( codegen, 0 );
      --padding;
   }
}

static void do_func( struct codegen* codegen ) {
   if ( list_size( &codegen->funcs ) == 0 ) {
      return;
   }
   struct {
      char params;
      char size;
      char value;
      char padding;
      int offset;
   } entry;
   entry.padding = 0;
   c_add_str( codegen, "FUNC" );
   c_add_int( codegen, sizeof( entry ) * list_size( &codegen->funcs ) );
   struct list_iter i;
   list_iterate( &codegen->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      entry.params = ( char ) c_total_param_size( func );
      entry.value = ( char ) ( func->return_spec != SPEC_VOID );
      if ( func->imported ) {
         entry.size = 0;
         entry.offset = 0;
      }
      else {
         struct func_user* impl = func->impl;
         entry.size = ( char ) ( impl->size - entry.params );
         entry.offset = impl->obj_pos;
      }
      c_add_sized( codegen, &entry, sizeof( entry ) );
      list_next( &i );
   }
}

int c_total_param_size( struct func* func ) {
   int size = 0;
   struct param* param = func->params;
   while ( param ) {
      size += param->size;
      param = param->next;
   }
   return size;
}

// If a library has only private functions and the FNAM chunk is missing, the
// game engine will crash when attempting to execute the `acsprofile` command.
// Always output the FNAM chunk if the FUNC chunk is present, even if the FNAM
// chunk will end up being empty.
static void do_fnam( struct codegen* codegen ) {
   if ( list_size( &codegen->funcs ) == 0 ) {
      return;
   }
   int count = 0;
   int size = 0;
   struct list_iter i;
   list_iterate( &codegen->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( ! func->hidden ) {
         size += t_full_name_length( func->name ) + 1;
         ++count;
      }
      list_next( &i );
   }
   int offset =
      sizeof( int ) +
      sizeof( int ) * count;
   int padding = alignpad( offset + size, 4 );
   c_add_str( codegen, "FNAM" );
   c_add_int( codegen, offset + size + padding );
   c_add_int( codegen, count );
   // Offsets.
   list_iterate( &codegen->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( ! func->hidden ) {
         c_add_int( codegen, offset );
         offset += t_full_name_length( func->name ) + 1;
      }
      list_next( &i );
   }
   // Names.
   struct str str;
   str_init( &str );
   list_iterate( &codegen->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( ! func->hidden ) {
         t_copy_name( func->name, true, &str );
         c_add_sized( codegen, str.value, str.length + 1 );
      }
      list_next( &i );
   }
   str_deinit( &str );
   while ( padding ) {
      c_add_byte( codegen, 0 );
      --padding;
   }
}

static void do_strl( struct codegen* codegen ) {
   if ( ! list_size( &codegen->used_strings ) ) {
      return;
   }
   int size = 0;
   struct list_iter i;
   list_iterate( &codegen->used_strings, &i );
   while ( ! list_end( &i ) ) {
      struct indexed_string* string = list_data( &i );
      // Plus one for the NUL character.
      size += string->length + 1;
      list_next( &i );
   }
   int offset = 
      // String count, padded with a zero on each size.
      sizeof( int ) * 3 +
      // String offsets.
      sizeof( int ) * list_size( &codegen->used_strings );
   int padding = alignpad( offset + size, 4 );
   int offset_initial = offset;
   const char* name = "STRL";
   if ( codegen->task->library_main->encrypt_str ) {
      name = "STRE";
   }
   c_add_str( codegen, name );
   c_add_int( codegen, offset + size + padding );
   // String count.
   c_add_int( codegen, 0 );
   c_add_int( codegen, list_size( &codegen->used_strings ) );
   c_add_int( codegen, 0 );
   // Offsets.
   list_iterate( &codegen->used_strings, &i );
   while ( ! list_end( &i ) ) {
      struct indexed_string* string = list_data( &i );
      c_add_int( codegen, offset );
      offset += string->length + 1;
      list_next( &i );
   }
   // Strings.
   offset = offset_initial;
   list_iterate( &codegen->used_strings, &i );
   while ( ! list_end( &i ) ) {
      struct indexed_string* string = list_data( &i );
      if ( codegen->task->library_main->encrypt_str ) {
         int key = offset * STR_ENCRYPTION_CONSTANT;
         // Each character of the string is encoded, including the NUL
         // character.
         for ( int i = 0; i <= string->length; ++i ) {
            char ch = ( char )
               ( ( ( int ) string->value[ i ] ) ^ ( key + i / 2 ) );
            c_add_byte( codegen, ch );
         }
         offset += string->length + 1;
      }
      else {
         c_add_sized( codegen, string->value, string->length + 1 );
      }
      list_next( &i );
   }
   while ( padding ) {
      c_add_byte( codegen, 0 );
      --padding;
   }
}

static void do_mini( struct codegen* codegen ) {
   struct list_iter i;
   list_iterate( &codegen->scalars, &i );
   struct var* first_var = NULL;
   while ( ! list_end( &i ) && ! first_var ) {
      struct var* var = list_data( &i );
      if ( c_is_nonzero_scalar_var( var ) ) {
         first_var = var;
      }
      list_next( &i );
   }
   struct var* last_var = first_var;
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( c_is_nonzero_scalar_var( var ) ) {
         last_var = var;
      }
      list_next( &i );
   }
   if ( ! first_var ) {
      return;
   }
   int count = ( last_var->index + 1 ) - first_var->index;
   c_add_str( codegen, "MINI" );
   c_add_int( codegen,
      sizeof( int ) + // Index of first variable in the sequence.
      sizeof( int ) * count ); // Initializers.
   c_add_int( codegen, first_var->index );
   list_iterate( &codegen->scalars, &i );
   while ( ! list_end( &i ) && list_data( &i ) != first_var ) {
      list_next( &i );
   }
   bool processed_last_var = false;
   while ( ! list_end( &i ) && ! processed_last_var ) {
      struct var* var = list_data( &i );
      if ( c_is_nonzero_scalar_var( var ) ) {
         write_mini_value( codegen, var->value );
      }
      else {
         c_add_int( codegen, 0 );
      }
      processed_last_var = ( var == last_var );
      list_next( &i );
   }
}

static void write_mini_value( struct codegen* codegen, struct value* value ) {
   switch ( value->type ) {
   case VALUE_EXPR:
      c_add_int( codegen, value->expr->value );
      break;
   case VALUE_STRING:
      c_add_int( codegen, value->more.string.string->index_runtime );
      break;
   case VALUE_STRUCTREF:
      c_add_int( codegen, value->more.structref.offset );
      break;
   case VALUE_FUNCREF:
      {
         struct func_user* impl = value->more.funcref.func->impl;
         c_add_int( codegen, impl->index );
      }
      break;
   default:
      UNREACHABLE();
      c_bail( codegen );
   }
}

static void do_aray( struct codegen* codegen ) {
   int count = list_size( &codegen->arrays );
   if ( codegen->shary.used ) {
      ++count;
   }
   if ( count == 0 ) {
      return;
   }
   struct {
      int number;
      int size;
   } entry;
   c_add_str( codegen, "ARAY" );
   c_add_int( codegen, sizeof( entry ) * count );
   struct list_iter i;
   list_iterate( &codegen->arrays, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      entry.number = var->index;
      entry.size = var->size;
      c_add_sized( codegen, &entry, sizeof( entry ) );
      list_next( &i );
   }
   if ( codegen->shary.used ) {
      entry.number = codegen->shary.index;
      entry.size = codegen->shary.size;
      c_add_sized( codegen, &entry, sizeof( entry ) );
   }
}

static void do_aini( struct codegen* codegen ) {
   struct list_iter i;
   list_iterate( &codegen->arrays, &i );
   while ( ! list_end( &i ) ) {
      write_aini( codegen, list_data( &i ) );
      list_next( &i );
   }
   if ( codegen->shary.used ) {
      write_aini_shary( codegen );
   }
}

static void write_aini( struct codegen* codegen, struct var* var ) {
   int count = count_nonzero_value( codegen, var );
   if ( count == 0 ) {
      return;
   }
   c_add_str( codegen, "AINI" );
   c_add_int( codegen,
      sizeof( int ) + // Array index.
      sizeof( int ) * count ); // Number of elements to initialize.
   c_add_int( codegen, var->index );
   struct value_writing writing;
   init_value_writing( &writing );
   write_value_list( codegen, &writing, var->value );
}

static int count_nonzero_value( struct codegen* codegen,
   struct var* var ) {
   int count = 0;
   struct value* value = var->value;
   while ( value ) {
      if ( is_nonzero_value( codegen, value ) ) {
         count = value->index + get_value_size( codegen, value );
      }
      value = value->next;
   }
   return count;
}

static bool is_zero_value( struct codegen* codegen, struct value* value ) {
   switch ( value->type ) {
      struct func_user* impl;
   case VALUE_EXPR:
      return ( value->expr->value == 0 );
   case VALUE_STRING:
      return ( value->more.string.string->length == 0 );
   case VALUE_STRINGINITZ:
      return ( value->more.stringinitz.string->length == 0 );
   case VALUE_ARRAYREF:
   case VALUE_STRUCTREF:
      return false;
   case VALUE_FUNCREF:
      impl = value->more.funcref.func->impl;
      return ( impl->index == 0 );
   default:
      UNREACHABLE();
      c_bail( codegen );
   }
   return false;
}

static bool is_nonzero_value( struct codegen* codegen, struct value* value ) {
   return ( ! is_zero_value( codegen, value ) );
}

static int get_value_size( struct codegen* codegen, struct value* value ) {
   switch ( value->type ) {
   case VALUE_EXPR:
   case VALUE_STRING:
   case VALUE_STRUCTREF:
   case VALUE_FUNCREF:
      return 1;
   case VALUE_STRINGINITZ:
      // NOTE: NUL character not included.
      return value->more.stringinitz.string->length;
   case VALUE_ARRAYREF:
      // Offset to the array and offset to the dimension information.
      return 2;
   default:
      UNREACHABLE();
      c_bail( codegen );
      return 0;
   }
}

static void init_value_writing( struct value_writing* writing ) {
   writing->count = 0;
}

static void write_value_list( struct codegen* codegen,
   struct value_writing* writing, struct value* value ) {
   write_multi_value_list( codegen, writing, value, 0 );
}

static void write_multi_value_list( struct codegen* codegen,
   struct value_writing* writing, struct value* value, int base ) {
   while ( value ) {
      if ( is_nonzero_value( codegen, value ) ) {
         write_value( codegen, writing, value, base );
      }
      value = value->next;
   }
}

static void write_value( struct codegen* codegen,
   struct value_writing* writing, struct value* value, int base ) {
   // Nullify uninitialized space.
   int index = base + value->index;
   if ( writing->count < index ) {
      c_add_int_zero( codegen, index - writing->count );
      writing->count = index;
   }
   // Write value.
   switch ( value->type ) {
   case VALUE_EXPR:
      c_add_int( codegen, value->expr->value );
      ++writing->count;
      break;
   case VALUE_STRING:
      c_add_int( codegen, value->more.string.string->index_runtime );
      ++writing->count;
      break;
   case VALUE_STRINGINITZ:
      {
         struct indexed_string* string = value->more.stringinitz.string;
         for ( int i = 0; i < string->length; ++i ) {
            c_add_int( codegen, string->value[ i ] );
         }
         writing->count += string->length;
      }
      break;
   case VALUE_ARRAYREF:
      c_add_int( codegen, value->more.arrayref.offset );
      c_add_int( codegen, value->more.arrayref.diminfo );
      writing->count += 2;
      break;
   case VALUE_STRUCTREF:
      c_add_int( codegen, value->more.structref.offset );
      ++writing->count;
      break;
   case VALUE_FUNCREF:
      {
         struct func_user* impl = value->more.funcref.func->impl;
         c_add_int( codegen, impl->index );
         ++writing->count;
      }
      break;
   default:
      UNREACHABLE();
      c_bail( codegen );
   }
}

static void write_aini_shary( struct codegen* codegen ) {
   c_add_str( codegen, "AINI" );
   c_add_int( codegen,
      sizeof( int ) +
      sizeof( int ) * count_shary_initz( codegen ) );
   c_add_int( codegen, codegen->shary.index );
   // Write null-element/dimension-tracker.
   c_add_int( codegen, 0 );
   // Write array dimension information.
   write_diminfo( codegen );
   // Write initializers.
   struct value_writing writing;
   init_value_writing( &writing );
   struct list_iter i;
   list_iterate( &codegen->shary.vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      write_multi_value_list( codegen, &writing, var->value,
         var->index - codegen->shary.data_offset );
      list_next( &i );
   }
}

static int count_shary_initz( struct codegen* codegen ) {
   int count = 0;
   // Null-element/dimension-tracker.
   ++count;
   // Dimension information.
   count += codegen->shary.diminfo_size;
   // Data.
   int index = count;
   struct list_iter i;
   list_iterate( &codegen->shary.vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      int count = count_nonzero_value( codegen, var );
      if ( count != 0 ) {
         index = var->index + count;
      }
      list_next( &i );
   }
   count += index - count;
   return count;
}

static void write_diminfo( struct codegen* codegen ) {
   struct list_iter i;
   list_iterate( &codegen->shary.dims, &i );
   while ( ! list_end( &i ) ) {
      struct dim* dim = list_data( &i );
      c_add_int( codegen, t_dim_size( dim ) );
      list_next( &i );
   }
}

static void do_load( struct codegen* codegen ) {
   int size = 0;
   struct list_iter i;
   list_iterate( &codegen->task->library_main->dynamic, &i );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib->name.length > 0 ) {
         size += lib->name.length + 1;
      }
      list_next( &i );
   }
   list_iterate( &codegen->task->library_main->links, &i );
   while ( ! list_end( &i ) ) {
      struct library_link* link = list_data( &i );
      if ( link->needed ) {
         size += strlen( link->name ) + 1;
      }
      list_next( &i );
   }
   if ( size ) {
      int padding = alignpad( size, 4 );
      c_add_str( codegen, "LOAD" );
      c_add_int( codegen, size + padding );
      list_iterate( &codegen->task->library_main->dynamic, &i );
      while ( ! list_end( &i ) ) {
         struct library* lib = list_data( &i );
         if ( lib->name.length > 0 ) {
            c_add_sized( codegen, lib->name.value, lib->name.length + 1 );
         }
         list_next( &i );
      }
      list_iterate( &codegen->task->library_main->links, &i );
      while ( ! list_end( &i ) ) {
         struct library_link* link = list_data( &i );
         if ( link->needed ) {
            c_add_sized( codegen, link->name, strlen( link->name ) + 1 );
         }
         list_next( &i );
      }
      while ( padding ) {
         c_add_byte( codegen, 0 );
         --padding;
      }
   }
}

// NOTE: acc does not pad this chunk at the end, so this chunk might cause any
// subsequent chunk to be misaligned.
static void do_mimp( struct codegen* codegen ) {
   int size = 0;
   struct list_iter i;
   list_iterate( &codegen->imported_scalars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      size += sizeof( int ) + // Index of variable.
         t_full_name_length( var->name ) + 1; // Plus one for NUL character.
      list_next( &i );
   }
   if ( size == 0 ) {
      return;
   }
   c_add_str( codegen, "MIMP" );
   c_add_int( codegen, size );
   struct str str;
   str_init( &str );
   list_iterate( &codegen->imported_scalars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      c_add_int( codegen, var->index );
      t_copy_name( var->name, true, &str );
      c_add_sized( codegen, str.value, str.length + 1 );
      list_next( &i );
   }
   str_deinit( &str );
}

// NOTE: acc does not pad this chunk at the end, so this chunk might cause any
// subsequent chunk to be misaligned.
static void do_aimp( struct codegen* codegen ) {
   if ( list_size( &codegen->imported_arrays ) == 0 ) {
      return;
   }
   int size =
      // Number of imported arrays.
      sizeof( int );
   struct list_iter i;
   list_iterate( &codegen->imported_arrays, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      size +=
         // Array index.
         sizeof( int ) +
         // Array size.
         sizeof( int ) +
         // Array name, plus one for the NUL character.
         t_full_name_length( var->name ) + 1;
      list_next( &i );
   }
   c_add_str( codegen, "AIMP" );
   c_add_int( codegen, size );
   c_add_int( codegen, list_size( &codegen->imported_arrays ) );
   struct str str;
   str_init( &str );
   list_iterate( &codegen->imported_arrays, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      c_add_int( codegen, var->index );
      c_add_int( codegen, var->size );
      t_copy_name( var->name, true, &str );
      c_add_sized( codegen, str.value, str.length + 1 );
      list_next( &i );
   }
   str_deinit( &str );
}

static void do_mexp( struct codegen* codegen ) {
   int count = 0;
   int size = 0;
   struct list_iter i;
   list_iterate( &codegen->vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( ! var->hidden ) {
         size += t_full_name_length( var->name ) + 1;
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   int offset =
      sizeof( int ) + // Number of variables.
      sizeof( int ) * count; // Offsets.
   int padding = alignpad( offset + size, 4 );
   c_add_str( codegen, "MEXP" );
   c_add_int( codegen, offset + size + padding );
   c_add_int( codegen, count );
   // Write offsets.
   list_iterate( &codegen->vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( ! var->hidden ) {
         c_add_int( codegen, offset );
         offset += t_full_name_length( var->name ) + 1;
      }
      list_next( &i );
   }
   // Write names.
   struct str str;
   str_init( &str );
   list_iterate( &codegen->vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( ! var->hidden ) {
         t_copy_name( var->name, true, &str );
         c_add_sized( codegen, str.value, str.length + 1 );
      }
      list_next( &i );
   }
   str_deinit( &str );
   while ( padding ) {
      c_add_byte( codegen, 0 );
      --padding;
   }
}

static void do_mstr( struct codegen* codegen ) {
   int count = 0;
   struct list_iter i;
   list_iterate( &codegen->scalars, &i );
   while ( ! list_end( &i ) ) {
      if ( mstr_var( list_data( &i ) ) ) {
         ++count;
      }
      list_next( &i );
   }
   if ( count == 0 ) {
      return;
   }
   c_add_str( codegen, "MSTR" );
   c_add_int( codegen, sizeof( int ) * count );
   list_iterate( &codegen->scalars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( mstr_var( var ) ) {
         c_add_int( codegen, var->index );
      }
      list_next( &i );
   }
}

inline static bool mstr_var( struct var* var ) {
   return ( var->desc == DESC_PRIMITIVEVAR && var->initial_has_str ) ||
      ( var->desc == DESC_REFVAR && var->ref->type == REF_FUNCTION );
}

static void do_astr( struct codegen* codegen ) {
   int count = 0;
   struct list_iter i;
   list_iterate( &codegen->arrays, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( astr_var( var ) ) {
         ++count;
      }
      list_next( &i );
   }
   if ( count == 0 ) {
      return;
   }
   c_add_str( codegen, "ASTR" );
   c_add_int( codegen, sizeof( int ) * count );
   list_iterate( &codegen->arrays, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( astr_var( var ) ) {
         c_add_int( codegen, var->index );
      }
      list_next( &i );
   }
}

inline static bool astr_var( struct var* var ) {
   return ( var->desc == DESC_ARRAY && ( var->initial_has_str ||
      ( var->ref && var->ref->type == REF_FUNCTION ) ) );
}

static void do_atag( struct codegen* codegen ) {
   struct list_iter i;
   list_iterate( &codegen->arrays, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->desc == DESC_STRUCTVAR ) {
         struct atag_writing writing;
         init_atag_writing_var( &writing, var );
         write_atag_chunk( codegen, &writing );
      }
      list_next( &i );
   }
   if ( codegen->shary.used ) {
      struct atag_writing writing;
      init_atag_writing_shary( codegen, &writing );
      write_atag_chunk( codegen, &writing );
   }
}

static void init_atag_writing_var( struct atag_writing* writing,
   struct var* var ) {
   init_atag_writing( writing, var->index );
   writing->var = var;
}

static void init_atag_writing_shary( struct codegen* codegen,
   struct atag_writing* writing ) {
   init_atag_writing( writing, codegen->shary.index );
   writing->shary = true;
}

static void init_atag_writing( struct atag_writing* writing, int index ) {
   writing->var = NULL;
   writing->value = NULL;
   writing->index = index;
   writing->base = 0;
   writing->shary = false;
}

static void iterate_atag( struct codegen* codegen,
   struct atag_writing* writing ) {
   if ( writing->shary ) {
      list_iterate( &codegen->shary.vars, &writing->iter );
      next_atag( writing );
   }
   else {
      writing->value = writing->var->value;
   }
}

static void next_atag( struct atag_writing* writing ) {
   writing->value = NULL;
   if ( writing->shary ) {
      while ( ! list_end( &writing->iter ) ) {
         struct var* var = list_data( &writing->iter );
         list_next( &writing->iter );
         if ( var->value ) {
            writing->value = var->value;
            writing->base = var->index;
            break;
         }
      }
   }
}

static void write_atag_chunk( struct codegen* codegen,
   struct atag_writing* writing ) {
   enum { CHUNK_VERSION = 0 };
   enum {
      TAG_INTEGER,
      TAG_STRING,
      TAG_FUNCTION
   };
   int count = 0;
   iterate_atag( codegen, writing );
   while ( writing->value ) {
      struct value* value = writing->value;
      while ( value ) {
         switch ( value->type ) {
         case VALUE_STRING:
         case VALUE_FUNCREF:
            count = writing->base + value->index + 1;
            break;
         case VALUE_EXPR:
         case VALUE_STRINGINITZ:
         case VALUE_ARRAYREF:
         case VALUE_STRUCTREF:
            break;
         default:
            UNREACHABLE();
            c_bail( codegen );
         }
         value = value->next;
      }
      next_atag( writing );
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
   c_add_int( codegen, writing->index );
   int written = 0;
   iterate_atag( codegen, writing );
   while ( writing->value ) {
      struct value* value = writing->value;
      while ( value ) {
         // Implicit values.
         if (
            value->type == VALUE_STRING ||
            value->type == VALUE_FUNCREF ) {
            while ( written < writing->base + value->index ) {
               c_add_byte( codegen, TAG_INTEGER );
               ++written;
            }
         }
         switch ( value->type ) {
         case VALUE_STRING:
            c_add_byte( codegen, TAG_STRING );
            ++written;
            break;
         case VALUE_FUNCREF:
            c_add_byte( codegen, TAG_FUNCTION );
            ++written;
            break;
         case VALUE_EXPR:
         case VALUE_STRINGINITZ:
         case VALUE_ARRAYREF:
         case VALUE_STRUCTREF:
            break;
         default:
            UNREACHABLE();
            c_bail( codegen );
         }
         value = value->next;
      }
      next_atag( writing );
   }
}

static void do_sary( struct codegen* codegen ) {
   // Scripts.
   struct list_iter i;
   list_iterate( &codegen->task->library_main->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      write_sary_chunk( codegen, "SARY", script->assigned_number,
         &script->vars );
      list_next( &i );
   }
   // Functions.
   list_iterate( &codegen->task->library_main->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      write_sary_chunk( codegen, "FARY", impl->index, &impl->vars );
      list_next( &i );
   }
}

static void write_sary_chunk( struct codegen* codegen, const char* chunk_name,
   int index, struct list* vars ) {
   int count = 0;
   struct list_iter i;
   list_iterate( vars, &i );
   while ( ! list_end( &i ) ) {
      count += ( int ) script_array( list_data( &i ) );
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   c_add_str( codegen, chunk_name );
   c_add_int( codegen,
      // Function-index/script-number.
      sizeof( short ) +
      // List of array sizes.
      sizeof( int ) * count );
   c_add_short( codegen, ( short ) index );
   list_iterate( vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( script_array( var ) ) {
         c_add_int( codegen, var->size );
      }
      list_next( &i );
   }
}

inline static bool script_array( struct var* var ) {
   return ( var->storage == STORAGE_LOCAL && ( var->dim ||
      ( ! var->ref && var->structure ) ) ); 
}

static void do_alib( struct codegen* codegen ) {
   c_add_str( codegen, "ALIB" );
   c_add_int( codegen, 0 );
}
