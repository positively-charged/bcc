#include "b_main.h"

#define STR_ENCRYPTION_CONSTANT 157135
#define DEFAULT_SCRIPT_SIZE 20

static void do_aray( back_t* );
static void do_aini( back_t* );
static void do_aimp( back_t* );
static void do_astr( back_t* );
static void do_mstr( back_t* );
static void do_load( back_t* );
static void do_func( back_t* );
static void do_fnam( back_t* );
static void do_mini( back_t* );
static void do_mimp( back_t* );
static void do_mexp( back_t* );
static void do_sptr( back_t* );
static void do_sflg( back_t* );
static void do_svct( back_t* );
static void do_strl( back_t* );
static void output_string_content( back_t* );
static void output_string_content_encrypted( back_t*, int );
static int write_array_initials( back_t*, list_iterator_t* );
static void write_map_var_init( back_t*, list_t* );
static void write_map_array_init( back_t*, list_t* );
static int count_scalars_with_initial( list_t* );
static int count_funcs( list_t* );
static void write_funcs( back_t*, list_t*, bool );
static int count_func_names_length( list_t* );

void b_publish( ast_t* ast, options_t* options, const char* path ) {
   output_t out;
   o_init( &out, path );
   bool little_e = false;
   // The only difference I see between the Big-E and Little-E formats is that
   // the latter provides compression for the opcodes.
   if ( options->format == k_format_little_e ) {
      little_e = true;
   }

   back_t back = {
      .ast = ast, 
      .out = &out,
      .options = options,
      .little_e = little_e };
   b_alloc_code_block( &back );

   // Beginning of code.
   o_fill( &out, sizeof( char ) * 4 + sizeof( int ) );

   list_iterator_t i = list_iterate( &ast->module->funcs );
   while ( ! list_end( &i ) ) {
      ufunc_t* ufunc = list_idata( &i );
      ufunc->offset = o_pos( &out );
      b_write_function( &back, ufunc );
      list_next( &i );
   }

   i = list_iterate( &ast->module->scripts );
   while ( ! list_end( &i ) ) {
      script_t* script = list_idata( &i );
      script->offset = o_pos( &out );
      b_write_script( &back, script );
      list_next( &i );
   }

   // Write chunks.
   int chunk_pos = o_pos( &out );

   do_sptr( &back );
   do_sflg( &back );
   do_svct( &back );
   do_mini( &back );
   do_aray( &back );
   do_aini( &back );
   do_strl( &back );
   do_load( &back );
   do_func( &back );
   do_fnam( &back );
   do_aimp( &back );
   do_mimp( &back );
   // Library chunks.
   if ( ast->module->name.length ) {
      do_mexp( &back );
      do_mstr( &back );
      do_astr( &back );
   }

   // Header.
   o_seek_beg( &out );
   if ( options->format == k_format_little_e ) {
      o_str( &out, "ACSe" );
      o_int( &out, chunk_pos );
   }
   else if ( options->format == k_format_big_e ) {
      o_str( &out, "ACSE" );
      o_int( &out, chunk_pos );
   }
   o_deinit( &out );
}

void do_sptr( back_t* obj ) {
   list_t* scripts = &obj->ast->module->scripts;
   if ( ! list_size( scripts ) ) { return; }
   // NOTE: In the chunk-based format, there are two formats of the script
   // entry structure. From my observation, when the header of the object file
   // contains the name of a chunk-based format, it will use the below
   // structure. The other, smaller structure is used when the chunk-based
   // format name is found elsewhere in the object file. For now, the format
   // name is placed directly into the header, so use this structure.
   typedef struct {
      short num;
      short type;
      int offset;
      int num_param;
   } entry_t;
   o_str( obj->out, "SPTR" );
   o_int( obj->out, sizeof( entry_t ) * list_size( scripts ) );
   list_iterator_t i = list_iterate( scripts );
   while ( ! list_end( &i ) ) {
      script_t* script = list_idata( &i );
      entry_t entry = {
         ( short ) script->number->value,
         ( short ) script->type,
         script->offset,
         list_size( &script->params ) };
      o_sized( obj->out, &entry, sizeof( entry ) );
      list_next( &i );
   }
}

void do_sflg( back_t* obj ) {
   list_t* scripts = &obj->ast->module->scripts;
   int count = 0;
   list_iterator_t i = list_iterate( scripts );
   while ( ! list_end( &i ) ) {
      script_t* script = list_idata( &i );
      if ( script->flags ) { ++count; }
      list_next( &i );
   }
   if ( ! count ) { return; }
   typedef struct {
      short num;
      short flags;
   } entry_t;
   o_str( obj->out, "SFLG" );
   o_int( obj->out, sizeof( entry_t ) * count );
   i = list_iterate( scripts );
   while ( ! list_end( &i ) ) {
      script_t* script = list_idata( &i );
      if ( script->flags ) {
         entry_t entry = {
            ( short ) script->number->value,
            ( short ) script->flags };
         o_sized( obj->out, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
}

void do_svct( back_t* obj ) {
   list_t* scripts = &obj->ast->module->scripts;
   int count = 0;
   list_iterator_t i = list_iterate( scripts );
   while ( ! list_end( &i ) ) {
      script_t* script = list_idata( &i );
      if ( script->size > DEFAULT_SCRIPT_SIZE ) { ++count; }
      list_next( &i );
   }
   if ( ! count ) { return; }
   typedef struct {
      short num;
      short size;
   } entry_t;
   o_str( obj->out, "SVCT" );
   o_int( obj->out, sizeof( entry_t ) * count );
   i = list_iterate( scripts );
   while ( ! list_end( &i ) ) {
      script_t* script = list_idata( &i );
      if ( script->size > DEFAULT_SCRIPT_SIZE ) {
         entry_t entry = {
            ( short ) script->number->value,
            ( short ) script->size };
         o_sized( obj->out, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
}

// Handles both the normal string table and the encrypted string table.
void do_strl( back_t* obj ) {
   if ( ! obj->ast->str_table.head ) { return; }
   const char* name = "STRL";
   if ( obj->options->encrypt_str ) {
      name = "STRE";
   }
   o_str( obj->out, name );
   // The chunk size will be written later.
   int size_pos = o_pos( obj->out );
   o_int( obj->out, 0 );

   // Number of strings.
   o_int( obj->out, 0 );
   o_int( obj->out, obj->ast->str_table.size );
   o_int( obj->out, 0 );

   // A string offset is relative to the start of the chunk content.
   int offset =
      // Number of strings, written above.
      sizeof( int ) * 3 +
      // Individual string offsets, written below.
      sizeof( int ) * obj->ast->str_table.size;
   int size = offset;
   int initial_offset = offset;

   // Offsets.
   indexed_string_t* string = obj->ast->str_table.head;
   while ( string ) {
      o_int( obj->out, offset );
      // Plus one for the NULL byte.
      offset += string->length + 1;
      size += string->length;
      string = string->next;
   }

   // String content.
   if ( obj->options->encrypt_str ) {
      output_string_content_encrypted( obj, initial_offset );
   }
   else {
      output_string_content( obj );
   }

   // NULL byte of each string.
   size += obj->ast->str_table.size;

   // Chunk size.
   o_seek( obj->out, size_pos );
   o_int( obj->out, size );
   o_seek_end( obj->out );
}

void output_string_content( back_t* obj ) {
   indexed_string_t* string = obj->ast->str_table.head;
   while ( string ) {
      o_sized( obj->out, string->value, string->length + 1 );
      string = string->next;
   }
}

void output_string_content_encrypted( back_t* obj, int offset ) {
   indexed_string_t* string = obj->ast->str_table.head;
   while ( string ) {
      int key = offset * STR_ENCRYPTION_CONSTANT;
      // Each character of the string is encrypted, including the NULL byte.
      for ( int i = 0; i <= string->length; ++i ) {
         char ch = ( char )
            ( ( ( int ) string->value[ i ] ) ^ ( key + i / 2 ) );
         o_sized( obj->out, &ch, sizeof( ch ) );
      }
      offset += string->length + 1;
      string = string->next;
   }
}

void do_aray( back_t* obj ) {
   list_t* arrays = &obj->ast->module->arrays;
   int count = list_size( &obj->ast->module->customs );
   list_iterator_t i = list_iterate( arrays );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      if ( var->storage == k_storage_module ) { ++count; }
      list_next( &i );
   }
   if ( ! count ) { return; }
   typedef struct {
      int number;
      int size;
   } entry_t;
   o_str( obj->out, "ARAY" );
   o_int( obj->out, sizeof( entry_t ) * count );
   i = list_iterate( arrays );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      if ( var->storage == k_storage_module ) {
         entry_t entry = { var->index, var->size };
         o_sized( obj->out, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
   i = list_iterate( &obj->ast->module->customs );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      if ( var->storage == k_storage_module ) {
         entry_t entry = { var->index, var->type->size };
         o_sized( obj->out, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
}

void do_aimp( back_t* obj ) {
   bool header_shown = false;
   int size_pos = 0;
   int size = 0;
   int count = 0;
   list_iterator_t i_imp = list_iterate( &obj->ast->module->imports );
   while ( ! list_end( &i_imp ) ) {
      module_t* imported = list_idata( &i_imp );
      list_iterator_t i = list_iterate( &imported->arrays );
      while ( ! list_end( &i ) ) {
         if ( ! header_shown ) {
            o_str( obj->out, "AIMP" );
            size_pos = o_pos( obj->out );
            o_int( obj->out, 0 );
            // Total arrays.
            o_int( obj->out, 0 );
            size += sizeof( int );
            header_shown = true;
         }
         var_t* var = list_idata( &i );
         o_int( obj->out, var->index );
         o_int( obj->out, var->size );
         str_t* name = ntbl_read( obj->ast->name_table, var->name );
         o_sized( obj->out, name->value, name->length + 1 );
         size += sizeof( int ) * 2 + name->length + 1;
         ++count;
         list_next( &i );
      }
      list_next( &i_imp );
   }
   if ( header_shown ) {
      o_seek( obj->out, size_pos );
      o_int( obj->out, size );
      o_int( obj->out, count );
      o_seek_end( obj->out );
   }
}

void do_aini( back_t* obj ) {
   write_map_array_init( obj, &obj->ast->module->arrays );
   write_map_array_init( obj, &obj->ast->module->hidden_arrays );
}

void write_map_array_init( back_t* obj, list_t* arrays ) {
   list_iterator_t i_array = list_iterate( arrays );
   while ( ! list_end( &i_array ) ) {
      var_t* var = list_idata( &i_array );
      if ( var->initial_not_zero ) {
         o_str( obj->out, "AINI" );
         int size_pos = o_pos( obj->out );
         int size = 0;
         o_int( obj->out, 0 );
         o_int( obj->out, var->index );
         size += sizeof( int );
         //list_iterator_t i = list_iterate( &var->initials );
        // size += write_array_initials( obj, &i );
         o_seek( obj->out, size_pos );
         o_int( obj->out, size );
         o_seek_end( obj->out );
      }
      list_next( &i_array );
   }
}

int write_array_initials( back_t* obj, list_iterator_t* i ) {
   int index = 0;
   while ( ! list_end( i ) ) {
      node_t* node = list_idata( i );
      if ( node->type == k_node_array_jump ) {
         array_jump_t* jump = ( array_jump_t* ) node;
         o_fill( obj->out, ( jump->index - index ) * sizeof( int ) );
         index = jump->index;
      }
      else {
         expr_t* expr = ( expr_t* ) node;
         o_int( obj->out, expr->value );
         ++index;
      }
      list_next( i );
   }
   return ( index * sizeof( int ) );
}

typedef struct {
   char num_param;
   char size;
   char has_return;
   char padding;
   int offset;
} func_entry_t;

void do_func( back_t* obj ) {
   module_t* module = obj->ast->module;
   // For now, all of the functions in the current module are written. It
   // doesn't matter if they are used in the module itself, not even if the
   // module is private.
   int count = list_size( &module->funcs );
   list_iterator_t i = list_iterate( &module->imports );
   while ( ! list_end( &i ) ) {
      module_t* imported = list_idata( &i );
      count += count_funcs( &imported->funcs );
      list_next( &i );
   }
   obj->num_funcs = count;
   if ( ! count ) { return; }
   o_str( obj->out, "FUNC" );
   o_int( obj->out, sizeof( func_entry_t ) * count );
   i = list_iterate( &module->imports );
   while ( ! list_end( &i ) ) {
      module_t* imported = list_idata( &i );
      write_funcs( obj, &imported->funcs, false );
      list_next( &i );
   }
   write_funcs( obj, &module->funcs, true );
}

int count_funcs( list_t* funcs ) {
   int count = 0;
   list_iterator_t i = list_iterate( funcs );
   while ( ! list_end( &i ) ) {
      ufunc_t* ufunc = list_idata( &i );
      if ( ufunc->usage ) { ++count; }
      list_next( &i );
   }
   return count;
}

void write_funcs( back_t* obj, list_t* funcs, bool ignore_usage ) {
   list_iterator_t i = list_iterate( funcs );
   while ( ! list_end( &i ) ) {
      ufunc_t* ufunc = list_idata( &i );
      if ( ufunc->usage || ignore_usage ) {
         int num_param = ufunc->func.num_param;
         if ( ufunc->func.min_param < ufunc->func.num_param ) {
            ++num_param;
         }
         func_entry_t entry = {
            ( char ) num_param,
            ( char ) ufunc->size,
            ( char ) ( ufunc->func.return_type != NULL ),
            // Always zero.
            0,
            ufunc->offset };
         o_sized( obj->out, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
}

void do_fnam( back_t* obj ) {
   if ( ! obj->num_funcs ) { return; }
   list_t* funcs = &obj->ast->module->funcs;
   o_str( obj->out, "FNAM" );
   int size_pos = o_pos( obj->out );
   int size = 0;
   int table_size = 0;
   o_int( obj->out, 0 );
   // Table size.
   o_int( obj->out, 0 );
   size += sizeof( int ) * ( obj->num_funcs + 1 );
   module_t* module = obj->ast->module;
   list_iterator_t i_imp = list_iterate( &module->imports );
   bool done = false;
   bool ignore_usage = false;
   while ( ! done ) {
      list_t* funcs = NULL;
      if ( ! list_end( &i_imp ) ) { 
         module_t* imported = list_idata( &i_imp );
         funcs = &imported->funcs;
         list_next( &i_imp );
      }
      else {
         funcs = &module->funcs;
         ignore_usage = true;
         done = true;
      }
      list_iterator_t i = list_iterate( funcs );
      while ( ! list_end( &i ) ) {
         ufunc_t* ufunc = list_idata( &i );
         if ( ufunc->usage || ignore_usage ) {
            o_int( obj->out, size );
            size += ( int ) ufunc->name->length + 1;
            ++table_size;
         }
         list_next( &i );
      }
   }
   i_imp = list_iterate( &module->imports );
   done = false;
   ignore_usage = false;
   while ( ! done ) {
      list_t* funcs = NULL;
      if ( ! list_end( &i_imp ) ) { 
         module_t* imported = list_idata( &i_imp );
         funcs = &imported->funcs;
         list_next( &i_imp );
      }
      else {
         funcs = &module->funcs;
         ignore_usage = true;
         done = true;
      }
      list_iterator_t i = list_iterate( funcs );
      while ( ! list_end( &i ) ) {
         ufunc_t* ufunc = list_idata( &i );
         if ( ufunc->usage || ignore_usage ) {
            str_t* name = ntbl_read( obj->ast->name_table, ufunc->name );
            o_sized( obj->out, name->value, name->length + 1 );
         }
         list_next( &i );
      }
   }
   o_seek( obj->out, size_pos );
   o_int( obj->out, size );
   o_int( obj->out, table_size );
   o_seek_end( obj->out );
}

int count_func_names_length( list_t* funcs ) {
   int length = 0;
   list_iterator_t i = list_iterate( funcs );
   while ( ! list_end( &i ) ) {
      ufunc_t* ufunc = list_idata( &i );
      if ( ufunc->usage ) { length += ufunc->name->length; }
      list_next( &i );
   }
   return length;
}

void do_mini( back_t* obj ) {
   // TODO: After the public scalars come the hidden scalars. So if all of the
   // public scalars are written, a second chunk is not necessary. Merge the
   // two MINI chunks.
   write_map_var_init( obj, &obj->ast->module->vars );
   write_map_var_init( obj, &obj->ast->module->hidden_vars );
}

void write_map_var_init( back_t* obj, list_t* scalars ) {
   int count = count_scalars_with_initial( scalars );
   if ( ! count ) { return; }
   o_str( obj->out, "MINI" );
   o_int( obj->out, sizeof( int ) * ( count + 1 ) );
   // Index of the first variable in the sequence to initialize.
   var_t* var = list_head( scalars );
   o_int( obj->out, var->index );
   list_iterator_t i = list_iterate( scalars );
   while ( ! list_end( &i ) ) {
      var = list_idata( &i );
      // All variables with initials appear first in the list.
      if ( ! var->initial ) { break; }
      //o_int( obj->out, var->initial->value );
      list_next( &i );
   }
}

int count_scalars_with_initial( list_t* list ) {
   int count = 0;
   list_iterator_t i = list_iterate( list );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      if ( ! var->initial ) { break; }
      ++count;
      list_next( &i );
   }
   return count;
}

void do_mexp( back_t* obj ) {
   list_t* arrays = &obj->ast->module->arrays;
   list_t* vars = &obj->ast->module->vars;
   int count = list_size( arrays ) + list_size( vars );
   if ( ! count ) { return; }
   o_str( obj->out, "MEXP" );
   int size_pos = o_pos( obj->out );
   int size = sizeof( int ) + sizeof( int ) * count;
   o_int( obj->out, 0 );
   o_int( obj->out, count );
   list_iterator_t i = list_iterate( arrays );
   // String offsets.
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      o_int( obj->out, size );
      size += ( int ) var->name->length + 1;
      list_next( &i );
   }
   i = list_iterate( vars );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      o_int( obj->out, size );
      size += ( int ) var->name->length + 1;
      list_next( &i );
   }
   // String content.
   i = list_iterate( arrays );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      str_t* name = ntbl_read( obj->ast->name_table, var->name );
      o_sized( obj->out, name->value, name->length + 1 );
      list_next( &i );
   }
   i = list_iterate( vars );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      str_t* name = ntbl_read( obj->ast->name_table, var->name );
      o_sized( obj->out, name->value, name->length + 1 );
      list_next( &i );
   }
   o_seek( obj->out, size_pos );
   o_int( obj->out, size );
   o_seek_end( obj->out );
}

void do_load( back_t* obj ) {
   list_t* imports = &obj->ast->module->imports;
   if ( ! list_size( imports ) ) { return; }
   o_str( obj->out, "LOAD" );
   int size_pos = o_pos( obj->out );
   o_int( obj->out, 0 );
   int size = 0;
   list_iterator_t i = list_iterate( imports );
   while ( ! list_end( &i ) ) {
      module_t* module = list_idata( &i );
      o_sized( obj->out, module->name.value, module->name.length + 1 );
      size += module->name.length + 1;
      list_next( &i );
   }
   o_seek( obj->out, size_pos );
   o_int( obj->out, size );
   o_seek_end( obj->out );
}

void do_mimp( back_t* obj ) {
   list_iterator_t i = list_iterate( &obj->ast->module->imports );
   if ( list_end( &i ) ) { return; }
   o_str( obj->out, "MIMP" );
   int size_pos = o_pos( obj->out );
   int size = 0;
   o_int( obj->out, 0 );
   while ( ! list_end( &i ) ) {
      module_t* module = list_idata( &i );
      list_iterator_t i_var = list_iterate( &module->vars );
      while ( ! list_end( &i_var ) ) {
         var_t* var = list_idata( &i_var );
         if ( var->usage ) {
            o_int( obj->out, var->index );
            str_t* name = ntbl_read( obj->ast->name_table, var->name );
            o_sized( obj->out, name->value, name->length + 1 );
            size += sizeof( int ) + name->length + 1;
         }
         list_next( &i_var );
      }
      list_next( &i );
   }
   o_seek( obj->out, size_pos );
   o_int( obj->out, size );
   o_seek_end( obj->out );
}

void do_mstr( back_t* obj ) {
   int count = 0;
   list_iterator_t i = list_iterate( &obj->ast->module->vars );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      if ( var->has_string ) { ++count; }
      list_next( &i );
   }
   if ( ! count ) { return; }
   o_str( obj->out, "MSTR" );
   o_int( obj->out, count * sizeof( int ) );
   i = list_iterate( &obj->ast->module->vars );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      if ( var->has_string ) {
         o_int( obj->out, var->index );
      }
      list_next( &i );
   }
}

void do_astr( back_t* obj ) {
   int count = 0;
   list_iterator_t i = list_iterate( &obj->ast->module->arrays );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      if ( var->has_string ) { ++count; }
      list_next( &i );
   }
   if ( ! count ) { return; }
   o_str( obj->out, "ASTR" );
   o_int( obj->out, count * sizeof( int ) );
   i = list_iterate( &obj->ast->module->arrays );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      if ( var->has_string ) {
         o_int( obj->out, var->index );
      }
      list_next( &i );
   }
}