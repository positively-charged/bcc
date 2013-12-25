#include <limits.h>

#include "task.h"

#define MAX_MAP_LOCATIONS 128
#define DEFAULT_SCRIPT_SIZE 20
#define STR_ENCRYPTION_CONSTANT 157135

#include <stdio.h>

struct func_entry {
   char params;
   char size;
   char value;
   char padding;
   int offset;
};

static void alloc_index( struct task* );
static void alloc_string_index( struct task* );
static void do_sptr( struct task* );
static void do_sflg( struct task* );
static void do_strl( struct task* );
static void do_aray( struct task* );
static void do_aini( struct task* );
static void do_aini_shared_int( struct task* );
static void do_aini_shared_str( struct task* );
static void do_aini_basic_int( struct task*, struct var* );
static void do_aini_basic_str( struct task*, struct var* );
static void do_mini( struct task* );
static void do_func( struct task* );
static void add_getter_setter( struct task*, struct var* );
static void do_fnam( struct task* );
static void do_load( struct task* );
static void do_svct( struct task* );
static void do_mstr( struct task* );
static void do_astr( struct task* );
static void do_var_interface( struct task* );
static void add_getter( struct task*, struct var* );
static void add_setter( struct task*, struct var* );

void t_init_fields_chunk( struct task* task ) {
   task->shared_size = 0;
   task->shared_size_str = 0;
   task->compress = false;
   task->block_walk = NULL;
   task->block_walk_free = NULL;
}

void t_publish( struct task* task ) {
   alloc_index( task );
   alloc_string_index( task );
task->format = FORMAT_BIG_E;
   if ( task->format == FORMAT_LITTLE_E ) {
      task->compress = true;
   }
   // Reserve header.
   t_add_int( task, 0 );
   t_add_int( task, 0 );
   t_write_script_content( task );
   t_write_func_content( task );
   // Getter and setter functions for variables in a library.
   if ( task->module->name.value ) {
      do_var_interface( task );
   }
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
   // Chunks for a library.
   if ( task->module->name.value ) {
      do_mstr( task );
      do_astr( task );
   }
   t_seek( task, 0 );
   if ( task->format == FORMAT_LITTLE_E ) {
      t_add_str( task, "ACSe" );
      t_add_int( task, chunk_pos );
   }
   else {
      t_add_str( task, "ACSE" );
      t_add_int( task, chunk_pos );
   }
   t_flush( task );
}

void alloc_index( struct task* task ) {
   // Determine whether the shared arrays are needed.
   int left = MAX_MAP_LOCATIONS;
   bool space_int = false;
   bool space_str = false;
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP ) {
         if ( ! var->type->primitive ) {
            if ( var->type->size ) {
               space_int = true;
               if ( left ) { 
                  --left;
               }
            }
            if ( var->type->size_str ) {
               space_str = true;
               if ( left ) { 
                  --left;
               }
            }
         }
         else if ( var->type->is_str ) {
            if ( left ) {
               --left;
            }
            else {
               space_str = true;
            }
         }
         else {
            if ( left ) {
               --left;
            }
            else {
               space_int = true;
            }
         }
      }
      list_next( &i );
   }
   // Allocate indexes.
   int index = 0;
   if ( space_int ) {
      ++index;
   }
   if ( space_str ) {
      ++index;
   }
   // Shared:
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->type->primitive ) {
         var->shared = true;
         if ( var->size ) {
            var->index = task->shared_size;
            task->shared_size += var->size;
         }
         if ( var->size_str ) {
            var->index_str = task->shared_size_str;
            task->shared_size_str += var->size_str;
         }
      }
      list_next( &i );
   }
   // Arrays.
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive && var->dim ) {
         if ( index < MAX_MAP_LOCATIONS ) {
            var->index = index;
            ++index;
         }
         // When an index can no longer be allocated, combine any remaining
         // variables into an array.
         else if ( var->type->is_str ) {
            var->shared = true;
            var->index_str = task->shared_size_str;
            task->shared_size_str += var->size_str;
         }
         else  {
            var->shared = true;
            var->index = task->shared_size;
            task->shared_size += var->size;
         }
      }
      list_next( &i );
   }
   // Scalars:
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         ! var->dim ) {
         if ( index < MAX_MAP_LOCATIONS ) {
            var->index = index;
            ++index;
         }
         else if ( var->type->is_str ) {
            var->shared = true;
            var->index_str = task->shared_size_str;
            task->shared_size_str += var->size_str;
         }
         else  {
            var->shared = true;
            var->index = task->shared_size;
            task->shared_size += var->size;
         }
      }
      list_next( &i );
   }
   // Functions:
   int used = 0;
   list_iter_init( &i, &task->module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      if ( impl->usage ) {
         ++used;
      }
      list_next( &i );
   }
   // The only problem that prevents the usage of the Little-E format is the
   // instruction for calling a user function. In Little-E, the field that
   // stores the index of the function is a byte in size, allowing up to 256
   // functions.
   if ( used <= UCHAR_MAX ) {
      task->format = FORMAT_LITTLE_E;
   }
   // To interface with an imported variable, getter and setter functions are
   // used. These are only used by the user of the library. From inside the
   // library, the variables are interfaced with directly.
   index = 0;
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t i_var;
      list_iter_init( &i_var, &module->vars );
      while ( ! list_end( &i_var ) ) {
         struct var* var = list_data( &i_var );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            var->get = index;
            ++index;
            var->set = index;
            ++index;
            var->flags |= VAR_FLAG_INTERFACE_GET_SET;
         }
         list_next( &i_var );
      }
      list_next( &i );
   }
   // Imported functions:
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t i_func;
      list_iter_init( &i_func, &module->funcs );
      while ( ! list_end( &i_func ) ) {
         struct func* func = list_data( &i_func );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            impl->index += index;
            ++index;
         }
         list_next( &i_func );
      }
      list_next( &i );
   }
   // Getter and setter functions for interfacing with the global variables of
   // a library.
   if ( task->module->name.value ) {
      list_iter_init( &i, &task->module->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->hidden ) {
            var->get = index;
            ++index;
            var->set = index;
            ++index;
            var->flags |= VAR_FLAG_INTERFACE_GET_SET;
         }
         list_next( &i );
      }
   }
   // User functions:
   list_iter_init( &i, &task->module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      impl->index = index;
      ++index;
      list_next( &i );
   }
}

void alloc_string_index( struct task* task ) {
   int index = 0;
   struct indexed_string* string = task->str_table.head;
   while ( string ) {
      if ( string->usage ) {
         string->index = index;
         ++index;
      }
      string = string->next;
   }
}

void do_sptr( struct task* task ) {
   if ( ! list_size( &task->module->scripts ) ) {
      return;
   }
   struct {
      short number;
      short type;
      int offset;
      int num_param;
   } entry;
   t_add_str( task, "SPTR" );
   t_add_int( task, list_size( &task->module->scripts ) * sizeof( entry ) );
   list_iter_t i;
   list_iter_init( &i, &task->module->scripts );
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

void do_sflg( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->module->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->flags ) {
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   struct {
      short number;
      short flags;
   } entry;
   t_add_str( task, "SFLG" );
   t_add_int( task, sizeof( entry ) * count );
   list_iter_init( &i, &task->module->scripts );
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

void do_svct( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->module->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->size > DEFAULT_SCRIPT_SIZE ) {
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   struct {
      short number;
      short size;
   } entry;
   t_add_str( task, "SVCT" );
   t_add_int( task, sizeof( entry ) * count );
   list_iter_init( &i, &task->module->scripts );
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

void do_strl( struct task* task ) {
   int count = 0;
   int size = 0;
   struct indexed_string* string = task->str_table.head;
   while ( string ) {
      if ( string->usage ) {
         // Plus one for the NULL byte.
         size += string->length + 1;
         ++count;
      }
      string = string->next;
   }
   if ( ! count ) {
      return;
   }
   int offset = 
      // Number of strings, padded with a zero on each size.
      sizeof( int ) * 3 +
      // Number of string offsets.
      sizeof( int ) * count;
   int offset_initial = offset;
   const char* name = "STRL";
   if ( task->options->encrypt_str ) {
      name = "STRE";
   }
   t_add_str( task, name );
   t_add_int( task, offset + size );
   // Number of strings.
   t_add_int( task, 0 );
   t_add_int( task, count );
   t_add_int( task, 0 );
   // Offsets.
   string = task->str_table.head;
   while ( string ) {
      if ( string->usage ) {
         t_add_int( task, offset );
         // Plus one for the NULL byte.
         offset += string->length + 1;
      }
      string = string->next;
   }
   // Strings.
   string = task->str_table.head;
   offset = offset_initial;
   while ( string ) {
      if ( string->usage ) {
         if ( task->options->encrypt_str ) {
            int key = offset * STR_ENCRYPTION_CONSTANT;
            // Each character of the string is encoded, including the null
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
      string = string->next;
   }
}

void do_aray( struct task* task ) {
   int count = 0;
   // Arrays for the integer and string members of structs.
   if ( task->shared_size ) {
      ++count;
   }
   if ( task->shared_size_str ) {
      ++count;
   }
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->dim && ! var->shared ) {
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
   if ( task->shared_size ) {
      entry.number = 0;
      entry.size = task->shared_size;
      t_add_sized( task, &entry, sizeof( entry ) );
   }
   if ( task->shared_size_str ) {
      entry.number = 1;
      entry.size = task->shared_size_str;
      t_add_sized( task, &entry, sizeof( entry ) );
   }
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->dim && ! var->shared ) {
         entry.number = var->index;
         entry.size = var->size;
         if ( var->type->is_str ) {
            entry.size = var->size_str;
         }
         t_add_sized( task, &entry, sizeof( entry ) );
      }
      list_next( &i );
   }
}

void do_aini( struct task* task ) {
   do_aini_shared_int( task );
   do_aini_shared_str( task );
   // Arrays with basic types.
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->initial && var->dim &&
         ! var->shared ) {
         if ( var->value_str ) {
            do_aini_basic_str( task, var );
         }
         else {
            do_aini_basic_int( task, var );
         }
      }
      list_next( &i );
   }
}

void do_aini_shared_int( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   int count = 0;
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->shared && var->value ) {
         struct value* value = var->value;
         while ( value ) {
            // Skip any value that is zero because the engine will initialize
            // any unitialized element with a zero.
            if ( value->expr->value ) {
               count = var->index + value->index + 1;
            }
            value = value->next;
         }
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   t_add_str( task, "AINI" );
   t_add_int( task, sizeof( int ) + sizeof( int ) * count );
   t_add_int( task, SHARED_ARRAY );
   count = 0;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage != STORAGE_MAP || ! var->shared || ! var->value ) {
         goto next;
      }
      struct value* value = var->value;
      while ( value ) {
         if ( value->expr->value ) {
            int index = var->index + value->index;
            // Fill any skipped element with a zero.
            if ( count < index ) {
               t_add_int_zero( task, index - count );
               count = index;
            }
            t_add_int( task, value->expr->value );
            ++count;
         }
         value = value->next;
      }
      next:
      list_next( &i );
   }
}

void do_aini_shared_str( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   int count = 0;
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->shared && var->value_str ) {
         struct value* value = var->value_str;
         while ( value ) {
            int test = value->expr->value;
            if ( value->expr->root->type == NODE_INDEXED_STRING_USAGE ) {
               struct indexed_string_usage* usage =
                  ( struct indexed_string_usage* ) value->expr->root;
               test = usage->string->index;
            }
            if ( test ) {
               count = var->index_str + value->index + 1;
            }
            value = value->next;
         }
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   t_add_str( task, "AINI" );
   t_add_int( task, sizeof( int ) + sizeof( int ) * count );
   t_add_int( task, SHARED_ARRAY_STR );
   count = 0;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage != STORAGE_MAP || ! var->shared || ! var->value_str ) {
         goto next;
      }
      struct value* value = var->value_str;
      while ( value ) {
         int output = value->expr->value;
         if ( value->expr->root->type == NODE_INDEXED_STRING_USAGE ) {
            struct indexed_string_usage* usage =
               ( struct indexed_string_usage* ) value->expr->root;
            output = usage->string->index;
         }
         if ( output ) {
            int index = var->index_str + value->index;
            // Fill any skipped element with a zero.
            if ( count < index ) {
               t_add_int_zero( task, index - count );
               count = index;
            }
            t_add_int( task, output );
            ++count;
         }
         value = value->next;
      }
      next:
      list_next( &i );
   }
}

void do_aini_basic_int( struct task* task, struct var* var ) {
   int count = 0;
   struct value* value = var->value;
   while ( value ) {
      if ( value->expr->value ) {
         count = value->index + 1;
      }
      value = value->next;
   }
   if ( ! count ) {
      return;
   }
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

void do_aini_basic_str( struct task* task, struct var* var ) {
   int count = 0;
   struct value* value = var->value_str;
   while ( value ) {
      int test = value->expr->value;
      if ( value->expr->root->type == NODE_INDEXED_STRING_USAGE ) {
         struct indexed_string_usage* usage =
            ( struct indexed_string_usage* ) value->expr->root;
         test = usage->string->index;
      }
      if ( test ) {
         count = value->index + 1;
      }
      value = value->next;
   }
   if ( ! count ) {
      return;
   }
   t_add_str( task, "AINI" );
   t_add_int( task, sizeof( int ) + sizeof( int ) * count );
   t_add_int( task, var->index );
   count = 0;
   value = var->value_str;
   while ( value ) {
      int output = value->expr->value;
      if ( value->expr->root->type == NODE_INDEXED_STRING_USAGE ) {
         struct indexed_string_usage* usage =
            ( struct indexed_string_usage* ) value->expr->root;
         output = usage->string->index;
      }
      if ( output ) {
         if ( count < value->index ) {
            t_add_int_zero( task, value->index - count );
            count = value->index;
         }
         t_add_int( task, output );
         ++count;
      }
      value = value->next;
   }
}

void do_mini( struct task* task ) {
   struct var* first_var = NULL;
   int skipped = 0;
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim && ! var->shared ) {
         int test = 0;
         if ( var->initial ) {
            struct value* value = ( struct value* ) var->initial;
            if ( value->expr->root->type == NODE_INDEXED_STRING_USAGE ) {
               struct indexed_string_usage* usage =
                  ( struct indexed_string_usage* ) value->expr->root;
               test = usage->string->index;
            }
            else {
               test = value->expr->value;
            }
         }
         if ( test ) {
            if ( ! first_var ) {
               first_var = var;
            }
            else {
               count += skipped;
            }
            ++count;
            skipped = 0;
         }
         else {
            ++skipped;
         }
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   t_add_str( task, "MINI" );
   t_add_int( task, sizeof( int ) * ( count + 1 ) );
   // Index of the first variable in the sequence to initialize.
   t_add_int( task, first_var->index );
   list_iter_init( &i, &task->module->vars );
   bool started = false;
   while ( ! list_end( &i ) && count ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim && ! var->shared ) {
         int output = 0;
         if ( var->initial ) {
            struct value* value = ( struct value* ) var->initial;
            if ( value->expr->root->type == NODE_INDEXED_STRING_USAGE ) {
               struct indexed_string_usage* usage =
                  ( struct indexed_string_usage* ) value->expr->root;
               output = usage->string->index;
            }
            else {
               output = value->expr->value;
            }
         }
         if ( output || started ) {
            t_add_int( task, output );
            started = true;
            --count;
         }
      }
      list_next( &i );
   }
}

void do_func( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      // Imported getter and setter functions:
      list_iter_t k;
      list_iter_init( &k, &module->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            count += 2;
         }
         list_next( &k );
      }
      // Imported functions:
      list_iter_init( &k, &module->funcs );
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
   // Getter and setter functions:
   if ( task->module->name.value ) {
      list_iter_init( &i, &task->module->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->flags & VAR_FLAG_INTERFACE_GET_SET ) {
            count += 2;
         }
         list_next( &i );
      }
   }
   // User fuctions:
   count += list_size( &task->module->funcs );
   if ( ! count ) {
      return;
   }
   t_add_str( task, "FUNC" );
   t_add_int( task, sizeof( struct func_entry ) * count );
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      // Imported getter and setter functions:
      list_iter_t k;
      list_iter_init( &k, &module->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            add_getter_setter( task, var );
         }
         list_next( &k );
      }
      list_iter_init( &k, &module->funcs );
      struct func_entry entry;
      entry.size = 0;
      entry.padding = 0;
      entry.offset = 0;
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            entry.params = ( char ) func->max_param;
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
   if ( task->module->name.value ) {
      list_iter_init( &i, &task->module->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->flags & VAR_FLAG_INTERFACE_GET_SET ) {
            add_getter_setter( task, var );
         }
         list_next( &i );
      }
   }
   // User functions:
   list_iter_init( &i, &task->module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      struct func_entry entry;
      entry.params = ( char ) func->max_param;
      // A hidden parameter used to store the number of real arguments
      // passed, is found after the last user parameter.
      if ( func->min_param != func->max_param ) {
         ++entry.params;
      }
      entry.size = ( char ) impl->size;
      entry.value = ( char ) ( func->value != NULL );
      entry.padding = 0;
      entry.offset = impl->obj_pos;
      t_add_sized( task, &entry, sizeof( entry ) );
      list_next( &i );
   }
}

void add_getter_setter( struct task* task, struct var* var ) {
   // Getter:
   struct func_entry entry;
   // Always zero.
   entry.padding = 0;
   if ( ! var->type->primitive ) {
      // Parameter 1: element
      // Parameter 2: accessing string member or not
      entry.params = 2;
   }
   else if ( var->dim ) {
      // Parameter 1: element
      entry.params = 1;
   }
   else {
      entry.params = 0;
   }
   entry.size = 0;
   entry.value = 1;
   entry.offset = var->get_offset;
   t_add_sized( task, &entry, sizeof( entry ) );
   // Setter:
   if ( ! var->type->primitive ) {
      // Parameter 1: element
      // Parameter 2: updating string member or not
      // Parameter 3: value
      entry.params = 3;
   }
   else if ( var->dim ) {
      // Parameter 1: element
      // Parameter 2: value
      entry.params = 2;
   }
   else {
      // Parameter 1: value
      entry.params = 1;
   }
   entry.size = 0;
   entry.value = 0;
   entry.offset = var->set_offset;
   t_add_sized( task, &entry, sizeof( entry ) );
}

void do_fnam( struct task* task ) {
   int count = 0;
   int size = 0;
   list_iter_t i;
   // Imported variables:
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t i_var;
      list_iter_init( &i_var, &module->vars );
      while ( ! list_end( &i_var ) ) {
         struct var* var = list_data( &i_var );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            size += t_full_name_length( var->name ) + 1 + 4;
            size += t_full_name_length( var->name ) + 1 + 4;
            count += 2;
         }
         list_next( &i_var );
      }
      list_next( &i );
   }
   // Variables:
   if ( task->module->name.value ) {
      list_iter_init( &i, &task->module->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->hidden ) {
            size += t_full_name_length( var->name ) + 1 + 4;
            size += t_full_name_length( var->name ) + 1 + 4;
            count += 2;
         }
         list_next( &i );
      }
   }
   // Imported functions:
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t i_func;
      list_iter_init( &i_func, &module->funcs );
      while ( ! list_end( &i_func ) ) {
         struct func* func = list_data( &i_func );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            size += t_full_name_length( func->name ) + 1;
            ++count;
         }
         list_next( &i_func );
      }
      list_next( &i );
   }
   // Functions:
   list_iter_init( &i, &task->module->funcs );
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
   t_add_str( task, "FNAM" );
   t_add_int( task, offset + size );
   t_add_int( task, count );
   // Offsets:
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t i_var;
      list_iter_init( &i_var, &module->vars );
      while ( ! list_end( &i_var ) ) {
         struct var* var = list_data( &i_var );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            t_add_int( task, offset );
            offset += t_full_name_length( var->name ) + 1 + 4;
            t_add_int( task, offset );
            offset += t_full_name_length( var->name ) + 1 + 4;
         }
         list_next( &i_var );
      }
      list_next( &i );
   }
   if ( task->module->name.value ) {
      list_iter_init( &i, &task->module->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->hidden ) {
            t_add_int( task, offset );
            offset += t_full_name_length( var->name ) + 1 + 4;
            t_add_int( task, offset );
            offset += t_full_name_length( var->name ) + 1 + 4;
         }
         list_next( &i );
      }
   }
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t f;
      list_iter_init( &f, &module->funcs );
      while ( ! list_end( &f ) ) {
         struct func* func = list_data( &f );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            t_add_int( task, offset );
            offset += t_full_name_length( func->name ) + 1;
         }
         list_next( &f );
      }
      list_next( &i );
   }
   list_iter_init( &i, &task->module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      t_add_int( task, offset );
      offset += t_full_name_length( func->name ) + 1;
      list_next( &i );
   }
   // Names:
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t i_var;
      list_iter_init( &i_var, &module->vars );
      while ( ! list_end( &i_var ) ) {
         struct var* var = list_data( &i_var );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            t_copy_full_name( var->name, &task->str );
            t_add_str( task, task->str.value );
            t_add_str( task, "!get" );
            t_add_byte( task, 0 );
            t_add_str( task, task->str.value );
            t_add_str( task, "!set" );
            t_add_byte( task, 0 );
         }
         list_next( &i_var );
      }
      list_next( &i );
   }
   if ( task->module->name.value ) {
      list_iter_init( &i, &task->module->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->hidden ) {
            t_copy_full_name( var->name, &task->str );
            t_add_str( task, task->str.value );
            t_add_str( task, "!get" );
            t_add_byte( task, 0 );
            t_add_str( task, task->str.value );
            t_add_str( task, "!set" );
            t_add_byte( task, 0 );
         }
         list_next( &i );
      }
   }
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      list_iter_t f;
      list_iter_init( &f, &module->funcs );
      while ( ! list_end( &f ) ) {
         struct func* func = list_data( &f );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            t_copy_full_name( func->name, &task->str );
            t_add_str( task, task->str.value );
            t_add_byte( task, 0 );
         }
         list_next( &f );
      }
      list_next( &i );
   }
   list_iter_init( &i, &task->module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      t_copy_full_name( func->name, &task->str );
      t_add_str( task, task->str.value );
      t_add_byte( task, 0 );
      list_next( &i );
   }
}

void do_load( struct task* task ) {
   int size = 0;
   list_iter_t i;
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      size += module->name.length + 1;
      list_next( &i );
   }
   if ( ! size ) {
      return;
   }
   t_add_str( task, "LOAD" );
   t_add_int( task, size );
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      t_add_sized( task, module->name.value, module->name.length + 1 );
      list_next( &i );
   }
}

void do_mstr( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim && var->value_str &&
         ! var->shared ) {
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   t_add_str( task, "MSTR" );
   t_add_int( task, count * sizeof( int ) );
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->dim && var->value_str &&
         ! var->shared ) {
         t_add_int( task, var->index );
      }
      list_next( &i );
   }
}

void do_astr( struct task* task ) {
   int count = 0;
   if ( task->shared_size_str ) {
      ++count;
   }
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->dim && var->value_str &&
         ! var->shared ) {
         ++count;
      }
      list_next( &i );
   }
   if ( ! count ) {
      return;
   }
   t_add_str( task, "ASTR" );
   t_add_int( task, count * sizeof( int ) );
   if ( task->shared_size_str ) {
      t_add_int( task, SHARED_ARRAY_STR );
   }
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->dim && var->value_str &&
         ! var->shared ) {
         t_add_int( task, var->index );
      }
      list_next( &i );
   }
}

void do_var_interface( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->module->vars );
   while ( ! list_end( &i ) ){
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->hidden ) {
         var->get_offset = t_tell( task );
         add_getter( task, var );
         var->set_offset = t_tell( task );
         add_setter( task, var );
      }
      list_next( &i );
   }
}

void add_getter( struct task* task, struct var* var ) {
   if ( ! var->type->primitive ) {
      t_add_opc( task, PC_PUSH_SCRIPT_VAR );
      t_add_arg( task, 0 );  // Element.
      // String member.
      if ( var->type->size_str ) {
         int jump = 0;
         if ( var->type->size ) {
            t_add_opc( task, PC_PUSH_SCRIPT_VAR );
            t_add_arg( task, 1 );  // String or not.
            jump = t_tell( task );
            t_add_opc( task, PC_IF_NOT_GOTO );
            t_add_arg( task, 0 );
         }
         // Optimization: Skip adding an offset of 0 because it won't have any
         // effect on the index.
         if ( var->index_str ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, var->index_str );
            t_add_opc( task, PC_ADD );
         }
         t_add_opc( task, PC_PUSH_MAP_ARRAY );
         t_add_arg( task, SHARED_ARRAY_STR );
         t_add_opc( task, PC_RETURN_VAL );
         if ( jump ) {
            int next = t_tell( task );
            t_seek( task, jump );
            t_add_opc( task, PC_IF_NOT_GOTO );
            t_add_arg( task, next );
            t_seek( task, OBJ_SEEK_END );
         }
      }
      // Integer member.
      if ( var->type->size ) {
         if ( var->index ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, var->index );
            t_add_opc( task, PC_ADD );
         }
         t_add_opc( task, PC_PUSH_MAP_ARRAY );
         t_add_arg( task, SHARED_ARRAY );
         t_add_opc( task, PC_RETURN_VAL );
      }
   }
   else if ( var->dim ) {
      // Element position.
      t_add_opc( task, PC_PUSH_SCRIPT_VAR );
      t_add_arg( task, 0 );
      int index = var->index;
      if ( var->shared ) {
         // Variable position.
         t_add_opc( task, PC_PUSH_NUMBER );
         if ( var->type->is_str ) {
            t_add_arg( task, var->index_str );
            index = SHARED_ARRAY_STR;
         }
         else {
            t_add_arg( task, var->index );
            index = SHARED_ARRAY;
         }
         t_add_opc( task, PC_ADD );
      }
      t_add_opc( task, PC_PUSH_MAP_ARRAY );
      t_add_arg( task, index );
      t_add_opc( task, PC_RETURN_VAL );
   }
   else {
      if ( var->shared ) {
         t_add_opc( task, PC_PUSH_NUMBER );
         if ( var->type->is_str ) {
            t_add_arg( task, var->index_str );
            t_add_opc( task, PC_PUSH_MAP_ARRAY );
            t_add_arg( task, SHARED_ARRAY_STR );
         }
         else {
            t_add_arg( task, var->index );
            t_add_opc( task, PC_PUSH_MAP_ARRAY );
            t_add_arg( task, SHARED_ARRAY );
         }
      }
      else {
         t_add_opc( task, PC_PUSH_MAP_VAR );
         t_add_arg( task, var->index );
      }
      t_add_opc( task, PC_RETURN_VAL );
   }
}

void add_setter( struct task* task, struct var* var ) {
   if ( ! var->type->primitive ) {
      t_add_opc( task, PC_PUSH_SCRIPT_VAR );
      t_add_arg( task, 0 );
      // String member.
      if ( var->type->size_str ) {
         int jump = 0;
         if ( var->type->size ) {
            t_add_opc( task, PC_PUSH_SCRIPT_VAR );
            t_add_arg( task, 1 );
            jump = t_tell( task );
            t_add_opc( task, PC_IF_NOT_GOTO );
            t_add_arg( task, 0 );
         }
         if ( var->index_str ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, var->index_str );
            t_add_opc( task, PC_ADD );
         }
         t_add_opc( task, PC_PUSH_SCRIPT_VAR );
         t_add_arg( task, 2 );  // Value.
         t_add_opc( task, PC_ASSIGN_MAP_ARRAY );
         t_add_arg( task, SHARED_ARRAY_STR );
         t_add_opc( task, PC_RETURN_VOID );
         if ( jump ) {
            int next = t_tell( task );
            t_seek( task, jump );
            t_add_opc( task, PC_IF_NOT_GOTO );
            t_add_arg( task, next );
            t_seek( task, OBJ_SEEK_END );
         }
      }
      // Integer member.
      if ( var->type->size ) {
         if ( var->index ) {
            t_add_opc( task, PC_PUSH_NUMBER );
            t_add_arg( task, var->index );
            t_add_opc( task, PC_ADD );
         }
         t_add_opc( task, PC_PUSH_SCRIPT_VAR );
         t_add_arg( task, 2 );  // Value.
         t_add_opc( task, PC_ASSIGN_MAP_ARRAY );
         t_add_arg( task, SHARED_ARRAY );
         t_add_opc( task, PC_RETURN_VOID );
      }
   }
   else if ( var->dim ) {
      // Element position.
      t_add_opc( task, PC_PUSH_SCRIPT_VAR );
      t_add_arg( task, 0 );
      int index = var->index;
      if ( var->shared ) {
         // Variable position.
         t_add_opc( task, PC_PUSH_NUMBER );
         if ( var->type->is_str ) {
            t_add_arg( task, var->index_str );
            index = SHARED_ARRAY_STR;
         }
         else {
            t_add_arg( task, var->index );
            index = SHARED_ARRAY;
         }
         t_add_opc( task, PC_ADD );
      }
      // Value.
      t_add_opc( task, PC_PUSH_SCRIPT_VAR );
      t_add_arg( task, 1 );
      t_add_opc( task, PC_ASSIGN_MAP_ARRAY );
      t_add_arg( task, index );
      t_add_opc( task, PC_RETURN_VOID );
   }
   else {
      if ( var->shared ) {
         // Variable position.
         t_add_opc( task, PC_PUSH_NUMBER );
         if ( var->type->is_str ) {
            t_add_arg( task, var->index_str );
         }
         else {
            t_add_arg( task, var->index );
         }
         // Value.
         t_add_opc( task, PC_PUSH_SCRIPT_VAR );
         t_add_arg( task, 0 );
         t_add_opc( task, PC_ASSIGN_MAP_ARRAY );
         if ( var->type->is_str ) {
            t_add_arg( task, SHARED_ARRAY_STR );
         }
         else {
            t_add_arg( task, SHARED_ARRAY );
         }
      }
      else {
         // Value.
         t_add_opc( task, PC_PUSH_SCRIPT_VAR );
         t_add_arg( task, 0 );
         t_add_opc( task, PC_ASSIGN_MAP_VAR );
         t_add_arg( task, var->index );
      }
      t_add_opc( task, PC_RETURN_VOID );
   }
}