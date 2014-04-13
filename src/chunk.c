#include <string.h>
#include <limits.h>

#include "task.h"

#define MAX_MAP_LOCATIONS 128
#define DEFAULT_SCRIPT_SIZE 20
#define STR_ENCRYPTION_CONSTANT 157135

#include <stdio.h>

struct shared {
   struct var* last[ 2 ];
   struct var* last_str[ 2 ];
   int left;
   bool needed;
   bool needed_str;
};

struct func_entry {
   char params;
   char size;
   char value;
   char padding;
   int offset;
};

static void alloc_index( struct task* );
static void determine_shared( struct shared*, struct var* );
static void do_sptr( struct task* );
static void do_sflg( struct task* );
static void do_strl( struct task* );
static void do_aray( struct task* );
static void do_aini( struct task* );
static void do_aini_shared_int( struct task* );
static void count_shared_value( struct value*, int, int* );
static void add_shared_value( struct task*, struct value*, int, int* );
static void do_aini_shared_str( struct task* );
static void do_aini_indexed( struct task*, struct value*, int );
static void do_mini( struct task* );
static void do_func( struct task* );
static void add_func( struct task*, struct func*, struct func_user* );
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
task->format = FORMAT_BIG_E;
   if ( task->format == FORMAT_LITTLE_E ) {
      task->compress = true;
   }
   // Reserve header.
   t_add_int( task, 0 );
   t_add_int( task, 0 );
   // Output body of scripts and functions.
   t_publish_scripts( task, &task->library_main->scripts );
   t_publish_funcs( task,  &task->library_main->funcs );
   // Getter and setter functions for interfacing with variables of a library.
   if ( task->library_main->visible ) {
      do_var_interface( task );
   }
   // When utilizing the Little-E format, where instructions can be of
   // different size, add padding so any following data starts at an offset
   // that is multiple of 4.
   if ( task->format == FORMAT_LITTLE_E ) {
      int i = alignpad( t_tell( task ), 4 );
      while ( i ) {
         t_add_opc( task, PC_TERMINATE );
         --i;
      }
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
   // Chunks for a library that is to be used through #include/#import.
   if ( task->library_main->visible ) {
      do_mstr( task );
      do_astr( task );
   }
   do_load( task );
   // NOTE: When a BEHAVIOR lump is below 32 bytes in size, the engine will not
   // process it. It will consider the lump as being of unknown format, even
   // though it appears to be valid, just empty. The problem is that the engine
   // considers an unknown format to be an error, and so will halt the execution
   // of script code. This appears to be a bug in the engine. In the meantime,
   // pad up to the minimum limit.
   int i = t_tell( task );
   if ( i < 32 ) {
      while ( i < 32 ) {
         t_add_byte( task, 0 );
         ++i;
      }
      chunk_pos = t_tell( task );
   }
   t_seek( task, 0 );
   if ( task->format == FORMAT_LITTLE_E ) {
      t_add_str( task, "ACSe" );
   }
   else {
      t_add_str( task, "ACSE" );
   }
   t_add_int( task, chunk_pos );
   t_flush( task );
}

void alloc_index( struct task* task ) {
   // Determine which variables need to be stored in a shared array. A shared
   // array is a single array whose space is used to store multiple pieces of
   // different data, one after another.
   STATIC_ASSERT( MAX_MAP_LOCATIONS >= 2 );
   struct shared shared;
   shared.last[ 0 ] = NULL;
   shared.last[ 1 ] = NULL;
   shared.last_str[ 0 ] = NULL;
   shared.last_str[ 1 ] = NULL;
   shared.left = MAX_MAP_LOCATIONS;
   shared.needed = false;
   shared.needed_str = false;
   // Arrays:
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         var->dim ) {
         determine_shared( &shared, var );
      }
      list_next( &i );
   }
   // Scalars:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         ! var->dim ) {
         determine_shared( &shared, var );
      }
      list_next( &i );
   }
   // Customs:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->type->primitive ) {
         determine_shared( &shared, var );
      }
      list_next( &i );
   }
   // Allocate indexes.
   int index = 0;
   if ( shared.needed ) {
      ++index;
   }
   if ( shared.needed_str ) {
      ++index;
   }
   // Arrays:
   // When allocating an index, arrays are given higher priority over scalars.
   // Why? The off-by-one error check provided by the engine will be better
   // utilized. If multiple arrays are combined into a single array, the check
   // won't be useful to the individual arrays.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         var->dim && ! var->shared && ! var->shared_str ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Scalars:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         ! var->dim && ! var->shared && ! var->shared_str ) {
         var->index = index;
         ++index;
      }
      list_next( &i );
   }
   // Customs:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->type->primitive ) {
         if ( ! var->shared ) {
            var->index = index;
            ++index;
         }
         if ( ! var->shared_str ) {
            var->index_str = index;
            ++index;
         }
      }
      list_next( &i );
   }
   // Any remaining variables are combined into an array.
   // Scalars:
   // Scalars are placed first to avoid being damaged by a buffer overflow.
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         ! var->dim ) {
         if ( var->shared ) {
            var->index = task->shared_size;
            task->shared_size += var->size;
         }
         else if ( var->shared_str ) {
            var->index_str = task->shared_size_str;
            task->shared_size_str += var->size_str;
         }
      }
      list_next( &i );
   }
   // Custom types:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->type->primitive ) {
         if ( var->shared ) {
            var->index = task->shared_size;
            task->shared_size += var->size;
         }
         if ( var->shared_str ) {
            var->index_str = task->shared_size_str;
            task->shared_size_str += var->size_str;
         }
      }
      list_next( &i );
   }
   // Arrays:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         var->dim ) {
         if ( var->shared ) {
            var->index = task->shared_size;
            task->shared_size += var->size;
         }
         else if ( var->shared_str ) {
            var->index_str = task->shared_size_str;
            task->shared_size_str += var->size_str;
         }
      }
      list_next( &i );
   }
   // Functions:
   int count = 0;
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      if ( impl->usage ) {
         ++count;
      }
      list_next( &i );
   }
   // The only problem that prevents the usage of the Little-E format is the
   // instruction for calling a user function. In Little-E, the field that
   // stores the index of the function is a byte in size, allowing up to 256
   // functions.
   if ( count <= UCHAR_MAX ) {
      task->format = FORMAT_LITTLE_E;
   }
   // To interface with an imported variable, getter and setter functions are
   // used. These are only used by the user of the library. From inside the
   // library, the variables are interfaced with directly.
   index = 0;
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            var->use_interface = true;
            var->get = index;
            ++index;
            var->set = index;
            ++index;
         }
         list_next( &k );
      }
      list_next( &i );
   }
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
            impl->index += index;
            ++index;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Getter and setter functions for interfacing with the global variables of
   // a library.
   if ( task->library_main->visible ) {
      list_iter_init( &i, &task->library_main->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->hidden ) {
            var->has_interface = true;
            var->get = index;
            ++index;
            var->set = index;
            ++index;
         }
         list_next( &i );
      }
   }
   // Visible functions:
   // Visible functions are given an index first, so they are present in the
   // FNAM chunk.
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
   // Hidden functions:
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( func->hidden ) {
         struct func_user* impl = func->impl;
         impl->index = index;
         ++index;
      }
      list_next( &i );
   }
}

void determine_shared( struct shared* shared, struct var* var ) {
   if ( var->type->size ) {
      // Use an index when one is available.
      if ( shared->left ) {
         --shared->left;
         shared->last[ 1 ] = shared->last[ 0 ];
         shared->last[ 0 ] = var;
      }
      // Once all indexes are allocated, insert the variable in the shared
      // array.
      else if ( shared->needed ) {
         var->shared = true;
      }
      // When no shared array exists, reuse the index of the last variable as
      // the index of the shared array.
      else if ( shared->last[ 0 ] ) {
         shared->last[ 0 ]->shared = true;
         shared->last[ 0 ] = shared->last[ 1 ];
         shared->needed = true;
         var->shared = true;
      }
      // When no variables are available, place one of the string variables
      // into the shared array and reuse the index.
      else if ( shared->needed_str ) {
         shared->last_str[ 0 ]->shared_str = true;
         shared->last[ 0 ] = var;
      }
      // When no string shared array exists, make one by combining two string
      // variables. Then reuse the index.
      else {
         shared->last_str[ 0 ]->shared_str = true;
         shared->last_str[ 1 ]->shared_str = true;
         shared->needed_str = true;
         shared->last[ 0 ] = var;
      }
   }
   if ( var->type->size_str ) {
      if ( shared->left ) {
         --shared->left;
         shared->last_str[ 1 ] = shared->last_str[ 0 ];
         shared->last_str[ 0 ] = var;
      }
      else if ( shared->needed_str ) {
         var->shared_str = true;
      }
      else if ( shared->last_str[ 0 ] ) {
         shared->last_str[ 0 ]->shared_str = true;
         shared->last_str[ 0 ] = shared->last_str[ 1 ];
         shared->needed_str = true;
         var->shared_str = true;
      }
      else if ( shared->needed ) {
         shared->last[ 0 ]->shared = true;
         shared->last_str[ 0 ] = var;
      }
      else {
         shared->last[ 0 ]->shared = true;
         shared->last[ 1 ]->shared = true;
         shared->needed = true;
         shared->last_str[ 0 ] = var;
      }
   }
}

void do_var_interface( struct task* task ) {
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
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
      enum {
         VAR_ELEMENT,
         VAR_VALUE,
         VAR_DO_STR
      };
      t_add_opc( task, PC_PUSH_SCRIPT_VAR );
      t_add_arg( task, VAR_ELEMENT );
      // String member.
      if ( var->type->size_str ) {
         int jump = 0;
         if ( var->type->size ) {
            t_add_opc( task, PC_PUSH_SCRIPT_VAR );
            t_add_arg( task, VAR_DO_STR );
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
         t_add_arg( task, VAR_VALUE );
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
         t_add_arg( task, VAR_VALUE );
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
   if ( task->options->encrypt_str ) {
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
         if ( task->options->encrypt_str ) {
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
   // Padding.
   i = 0;
   while ( i < padding ) {
      t_add_byte( task, 0 );
      ++i;
   }
}

void do_aray( struct task* task ) {
   int count = 0;
   if ( task->shared_size ) {
      ++count;
   }
   if ( task->shared_size_str ) {
      ++count;
   }
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP ) {
         // struct variables.
         if ( ! var->type->primitive ) {
            if ( var->size && ! var->shared ) {
               ++count;
            }
            if ( var->size_str && ! var->shared_str ) {
               ++count;
            }
         }
         // Primitive arrays.
         else if ( var->dim ) {
            if ( ! var->shared && ! var->shared_str ) {
               ++count;
            }
         }
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
      entry.number = SHARED_ARRAY;
      entry.size = task->shared_size;
      t_add_sized( task, &entry, sizeof( entry ) );
   }
   if ( task->shared_size_str ) {
      entry.number = SHARED_ARRAY_STR;
      entry.size = task->shared_size_str;
      t_add_sized( task, &entry, sizeof( entry ) );
   }
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP ) {
         // struct variables.
         if ( ! var->type->primitive ) {
            if ( var->size && ! var->shared ) {
               entry.number = var->index;
               entry.size = var->size;
               t_add_sized( task, &entry, sizeof( entry ) );
            }
            if ( var->size_str && ! var->shared_str ) {
               entry.number = var->index_str;
               entry.size = var->size_str;
               t_add_sized( task, &entry, sizeof( entry ) );
            }
         }
         // Primitive arrays.
         else if ( var->dim ) {
            if ( ! var->shared && ! var->shared_str ) {
               entry.number = var->index;
               if ( var->type->is_str ) {
                  entry.size = var->size_str;
               }
               else {
                  entry.size = var->size;
               }
               t_add_sized( task, &entry, sizeof( entry ) );
            }
         }
      }
      list_next( &i );
   }
}

void do_aini( struct task* task ) {
   // Shared arrays.
   do_aini_shared_int( task );
   do_aini_shared_str( task );
   // Variables with dedicated index.
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->initial &&
         ! var->shared && ! var->shared_str ) {
         if ( ! var->type->primitive ) {
            if ( var->value ) {
               do_aini_indexed( task, var->value, var->index );
            }
            if ( var->value_str ) {
               do_aini_indexed( task, var->value_str, var->index_str );
            }
         }
         else if ( var->dim ) {
            do_aini_indexed( task, var->value, var->index );
         }
      }
      list_next( &i );
   }
}

void do_aini_shared_int( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->initial && var->shared ) {
         count_shared_value( var->value, var->index, &count );
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
   // Writing of initial values needs to follow the order of shared index
   // assignment, above. So initial values for scalar variables come first,
   // then for struct variables, and so on.
   // Scalars:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive && ! var->dim &&
         var->shared ) {
         add_shared_value( task, var->value, var->index, &count );
      }
      list_next( &i );
   }
   // Customs:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->type->primitive &&
         var->shared ) {
         add_shared_value( task, var->value, var->index, &count );
      }
      list_next( &i );
   }
   // Arrays:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive && var->dim &&
         var->shared ) {
         add_shared_value( task, var->value, var->index, &count );
      }
      list_next( &i );
   }
}

void count_shared_value( struct value* value, int base, int* count ) {
   while ( value ) {
      // Skip any value that is zero because the engine will initialize
      // any unitialized element with a zero.
      if ( value->expr->value ) {
         int latest = base + value->index + 1;
         if ( latest > *count ) {
            *count = latest;
         }
      }
      value = value->next;
   }
}

void add_shared_value( struct task* task, struct value* value, int base,
   int* count ) {
   while ( value ) {
      int output = value->expr->value;
      if ( value->expr->root->type == NODE_INDEXED_STRING_USAGE ) {
         struct indexed_string_usage* usage =
            ( struct indexed_string_usage* ) value->expr->root;
         output = usage->string->index;
      }
      if ( output ) {
         int index = base + value->index;
         // Fill any skipped element with a zero.
         if ( *count < index ) {
            t_add_int_zero( task, index - *count );
            *count = index;
         }
         t_add_int( task, output );
         ++*count;
      }
      value = value->next;
   }
}

void do_aini_shared_str( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->value_str && var->shared_str ) {
         count_shared_value( var->value_str, var->index_str, &count );
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
   // Scalars:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive && ! var->dim &&
         var->shared_str ) {
         add_shared_value( task, var->value_str, var->index_str, &count );
      }
      list_next( &i );
   }
   // Customs:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->type->primitive &&
         var->shared_str ) {
         add_shared_value( task, var->value_str, var->index_str,
            &count );
      }
      list_next( &i );
   }
   // Arrays:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive && var->dim &&
         var->shared_str ) {
         add_shared_value( task, var->value_str, var->index_str,
            &count );
      }
      list_next( &i );
   }
}

void do_aini_indexed( struct task* task, struct value* head, int index ) {
   int count = 0;
   struct value* value = head;
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
   t_add_int( task, index );
   count = 0;
   value = head;
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

void do_mini( struct task* task ) {
   struct var* first_var = NULL;
   int skipped = 0;
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         ! var->dim && ! var->shared && ! var->shared_str ) {
         int nonzero = 0;
         if ( var->initial ) {
            struct value* value = ( struct value* ) var->initial;
            nonzero = value->expr->value;
         }
         if ( nonzero ) {
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
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive &&
         ! var->dim && ! var->shared && ! var->shared_str ) {
         int nonzero = 0;
         if ( var->initial ) {
            struct value* value = ( struct value* ) var->initial;
            nonzero = value->expr->value;
         }
         if ( nonzero ) {
            if ( var == first_var ) {
               skipped = 0;
            }
            while ( skipped ) {
               t_add_int( task, 0 );
               --skipped;
            }
            t_add_int( task, nonzero );
         }
         else {
            ++skipped;
         }
      }
      list_next( &i );
   }
}

void do_func( struct task* task ) {
   // Count number of function entries to output.
   // -----------------------------------------------------------------------
   int count = 0;
   // Variable interface functions:
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->has_interface ) {
         count += 2;
      }
      list_next( &i );
   }
   // Fuctions:
   count += list_size( &task->library_main->funcs );
   // Imported variable interface functions, and imported functions:
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            count += 2;
         }
         list_next( &k );
      }
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
   // Output entries:
   // -----------------------------------------------------------------------
   t_add_str( task, "FUNC" );
   t_add_int( task, sizeof( struct func_entry ) * count );
   // Imported variable interface functions:
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            add_getter_setter( task, var );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Imported functions:
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
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
   // Variable interface functions:
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->has_interface ) {
         add_getter_setter( task, var );
      }
      list_next( &i );
   }
   // Visible functions:
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( ! func->hidden ) {
         add_func( task, func, func->impl );
      }
      list_next( &i );
   }
   // Hidden function:
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( func->hidden ) {
         add_func( task, func, func->impl );
      }
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

void add_func( struct task* task, struct func* func, struct func_user* impl ) {
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
}

void do_fnam( struct task* task ) {
   int count = 0;
   int size = 0;
   list_iter_t i;
   // Imported-variable interface functions:
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            size += t_full_name_length( var->name ) + 1 + 4;
            size += t_full_name_length( var->name ) + 1 + 4;
            count += 2;
         }
         list_next( &k );
      }
      list_next( &i );
   }
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
            size += t_full_name_length( func->name ) + 1;
            ++count;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Variables:
   if ( task->library_main->visible ) {
      list_iter_init( &i, &task->library_main->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->hidden ) {
            int length = t_full_name_length( var->name );
            size += ( length + 1 + 4 ) * 2;
            count += 2;
         }
         list_next( &i );
      }
   }
   // Functions:
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( ! func->hidden ) {
         size += t_full_name_length( func->name ) + 1;
         ++count;
      }
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
   // Imported variable interface functions.
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            t_add_int( task, offset );
            offset += t_full_name_length( var->name ) + 1 + 4;
            t_add_int( task, offset );
            offset += t_full_name_length( var->name ) + 1 + 4;
         }
         list_next( &k );
      }
      list_next( &i );
   }
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
   // Variable interface functions.
   if ( task->library_main->visible ) {
      list_iter_init( &i, &task->library_main->vars );
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
   // Functions.
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( ! func->hidden ) {
         t_add_int( task, offset );
         offset += t_full_name_length( func->name ) + 1;
      }
      list_next( &i );
   }
   // Names:
   // -----------------------------------------------------------------------
   // Imported variable interface functions.
   struct str str;
   str_init( &str );
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->vars );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->usage ) {
            t_copy_name( var->name, true, &str );
            t_add_str( task, str.value );
            t_add_str( task, "!get" );
            t_add_byte( task, 0 );
            t_add_str( task, str.value );
            t_add_str( task, "!set" );
            t_add_byte( task, 0 );
         }
         list_next( &k );
      }
      list_next( &i );
   }
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
            t_copy_name( func->name, true, &str );
            t_add_str( task, str.value );
            t_add_byte( task, 0 );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // Variable interface functions.
   if ( task->library_main->visible ) {
      list_iter_init( &i, &task->library_main->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && ! var->hidden ) {
            t_copy_name( var->name, true, &str );
            t_add_str( task, str.value );
            t_add_str( task, "!get" );
            t_add_byte( task, 0 );
            t_add_str( task, str.value );
            t_add_str( task, "!set" );
            t_add_byte( task, 0 );
         }
         list_next( &i );
      }
   }
   // Functions.
   list_iter_init( &i, &task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( ! func->hidden ) {
         t_copy_name( func->name, true, &str );
         t_add_str( task, str.value );
         t_add_byte( task, 0 );
      }
      list_next( &i );
   }
   // Padding:
   // -----------------------------------------------------------------------
   for ( int i = 0; i < padding; ++i ) {
      t_add_byte( task, 0 );
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
      t_add_str( task, "LOAD" );
      t_add_int( task, size );
      list_iter_init( &i, &task->library_main->dynamic );
      while ( ! list_end( &i ) ) {
         struct library* lib = list_data( &i );
         t_add_sized( task, lib->name.value, lib->name.length + 1 );
         list_next( &i );
      }
   }
}

/*
void determine_module_used( struct module* module ) {
   if ( list_size( &module->scripts ) ) {
      module->used = true;
      return;
   }
   // Functions.
   list_iter_t i;
   list_iter_init( &i, &module->funcs );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      if ( impl->usage ) {
         module->used = true;
         return;
      }
      list_next( &i );
   }
   // Variables.
   list_iter_init( &i, &module->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->usage ) {
         module->used = true;
         return;
      }
      list_next( &i );
   }
}*/

void do_mstr( struct task* task ) {
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive && ! var->dim &&
         var->initial_has_str && ! var->shared && ! var->shared_str ) {
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
         if ( var->storage == STORAGE_MAP && var->type->primitive &&
            ! var->dim && var->initial_has_str && ! var->shared &&
            ! var->shared_str ) {
            t_add_int( task, var->index );
         }
         list_next( &i );
      }
   }
}

void do_astr( struct task* task ) {
   int count = 0;
   if ( task->shared_size_str ) {
      ++count;
   }
   list_iter_t i;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->type->primitive && var->dim &&
         var->initial_has_str && ! var->shared && ! var->shared_str ) {
         ++count;
      }
      list_next( &i );
   }
   if ( count ) {
      t_add_str( task, "ASTR" );
      t_add_int( task, sizeof( int ) * count );
      if ( task->shared_size_str ) {
         t_add_int( task, SHARED_ARRAY_STR );
      }
      list_iter_init( &i, &task->library_main->vars );
      while ( ! list_end( &i ) ) {
         struct var* var = list_data( &i );
         if ( var->storage == STORAGE_MAP && var->type->primitive &&
            var->dim && var->initial_has_str && ! var->shared &&
            ! var->shared_str ) {
            t_add_int( task, var->index );
         }
         list_next( &i );
      }
   }
}