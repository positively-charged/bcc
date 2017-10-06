#include <string.h>

#include "task.h"
#include "phase.h"

#define MAX_MAP_LOCATIONS 128
#define MAX_LIB_FUNCS 256

static void publish_acs95( struct codegen* codegen );
static void publish( struct codegen* codegen );
static void clarify_vars( struct codegen* codegen );
static void alloc_dim_counter_var( struct codegen* codegen );
static void clarify_funcs( struct codegen* codegen );
static void assign_func_indexes( struct codegen* codegen );
static void setup_shary( struct codegen* codegen );
static void setup_diminfo( struct codegen* codegen );
static int append_dim( struct codegen* codegen, struct dim* dim );
static bool same_dim( struct dim* dim, struct list_iter i );
static void setup_data( struct codegen* codegen );
static void patch_initz( struct codegen* codegen );
static void patch_initz_list( struct codegen* codegen, struct list* vars );
static void patch_value( struct codegen* codegen, struct value* value );
static void sort_vars( struct codegen* codegen );
static bool is_initz_zero( struct value* value );
static void assign_indexes( struct codegen* codegen );
static void create_assert_strings( struct codegen* codegen );
static void append_acs_strings( struct codegen* codegen );

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
   list_init( &codegen->used_strings );
   list_init( &codegen->vars );
   list_init( &codegen->scalars );
   list_init( &codegen->arrays );
   list_init( &codegen->imported_vars );
   list_init( &codegen->imported_scalars );
   list_init( &codegen->imported_arrays );
   list_init( &codegen->funcs );
   list_init( &codegen->shary.vars );
   list_init( &codegen->shary.dims );
   codegen->shary.index = 0;
   codegen->shary.dim_counter = 0;
   codegen->shary.size = 0;
   codegen->shary.diminfo_size = 0;
   codegen->shary.diminfo_offset = 0;
   codegen->shary.data_offset = 0;
   codegen->shary.dim_counter_var = false;
   codegen->shary.used = false;
   codegen->null_handler = NULL;
   codegen->object_size = 0;
   codegen->lang = task->library_main->lang;
   codegen->dummy_script_offset = 0;
}

void c_publish( struct codegen* codegen ) {
   switch ( codegen->task->library_main->lang ) {
   case LANG_ACS95:
      publish_acs95( codegen );
      break;
   default:
      publish( codegen );
      break;
   }
}

static void publish_acs95( struct codegen* codegen ) {
   // Reserve header.
   c_add_int( codegen, 0 );
   c_add_int( codegen, 0 );
   // Write scripts and strings.
   c_write_user_code_acs( codegen );
   int string_offset = c_tell( codegen );
   struct list_iter i;
   list_iterate( &codegen->used_strings, &i );
   while ( ! list_end( &i ) ) {
      struct indexed_string* string = list_data( &i );
      // Plus one for the NUL character.
      c_add_sized( codegen, string->value, string->length + 1 );
      list_next( &i );
   }
   int padding = alignpad( c_tell( codegen ), 4 );
   while ( padding ) {
      c_add_byte( codegen, 0 );
      --padding;
   }
   // Write script entries.
   int dir_offset = c_tell( codegen );
   c_add_int( codegen, list_size( &codegen->task->library_main->scripts ) );
   list_iterate( &codegen->task->library_main->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      int number = script->assigned_number + ( script->type * 1000 );
      c_add_int( codegen, number );
      c_add_int( codegen, script->offset );
      c_add_int( codegen, script->num_param );
      list_next( &i );
   }
   // Write string entries.
   c_add_int( codegen, list_size( &codegen->used_strings ) );
   list_iterate( &codegen->used_strings, &i );
   while ( ! list_end( &i ) ) {
      struct indexed_string* string = list_data( &i );
      c_add_int( codegen, string_offset );
      string_offset += string->length + 1;
      list_next( &i );
   }
   c_seek( codegen, 0 );
   c_add_sized( codegen, "ACS\0", 4 );
   c_add_int( codegen, dir_offset );
   c_flush( codegen );
}

static void publish( struct codegen* codegen ) {
   switch ( codegen->lang ) {
   case LANG_ACS:
      append_acs_strings( codegen );
      break;
   default:
      // Reserve index 0 for the empty string.
      c_append_string( codegen, codegen->task->empty_string );
      break;
   }
   clarify_vars( codegen );
   clarify_funcs( codegen );
   assign_func_indexes( codegen );
   switch ( codegen->lang ) {
   case LANG_BCS:
      setup_shary( codegen );
      break;
   default:
      break;
   }
   sort_vars( codegen );
   assign_indexes( codegen );
   switch ( codegen->lang ) {
   case LANG_BCS:
      patch_initz( codegen );
      if ( codegen->task->options->write_asserts &&
         list_size( &codegen->task->runtime_asserts ) > 0 ) {
         create_assert_strings( codegen );
      }
      break;
   default:
      break;
   }
   c_write_chunk_obj( codegen );
}

static void clarify_vars( struct codegen* codegen ) {
   int count = 0;
   // Variables.
   struct list_iter i;
   list_iterate( &codegen->task->library_main->vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ! var->hidden ) {
         list_append( &codegen->vars, var );
         ++count;
      }
      list_next( &i );
   }
   // Imported variables.
   list_iterate( &codegen->task->library_main->dynamic, &i );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      struct list_iter k;
      list_iterate( &lib->vars, &k );
      while ( ! list_end( &k ) ) {
         struct var* var = list_data( &k );
         if ( var->storage == STORAGE_MAP && var->used ) {
            list_append( &codegen->imported_vars, var );
            ++count;
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // External variables.
   list_iterate( &codegen->task->library_main->external_vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->imported && var->used ) {
         list_append( &codegen->imported_vars, var );
         ++count;
      }
      list_next( &i );
   }
   // Take shared array into account.
   ++count;
   // Store in the shared array those private arrays and private
   // structure-variables whose address is taken.
   list_iterate( &codegen->task->library_main->vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && ( var->desc == DESC_ARRAY ||
         var->desc == DESC_STRUCTVAR ) && var->hidden && var->addr_taken ) {
         list_append( &codegen->shary.vars, var );
      }
      list_next( &i );
   }
   // Allocate dimension counter. It is better to use a variable than it is
   // to use an element of the shared array because it reduces the number of
   // instructions we need to use when dealing with array references.
   if ( list_size( &codegen->shary.vars ) > 0 && count < MAX_MAP_LOCATIONS ) {
      alloc_dim_counter_var( codegen );
      ++count;
   }
   // Hidden variables.
   list_iterate( &codegen->task->library_main->vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_MAP && var->hidden && ! var->addr_taken ) {
         if ( count < MAX_MAP_LOCATIONS ) {
            list_append( &codegen->vars, var );
            ++count;
         }
         // When an index is no longer available, store the hidden variable
         // in the shared array.
         else {
            list_append( &codegen->shary.vars, var );
         }
      }
      list_next( &i );
   }
   // Shared array is used and needs to be reserved.
   if ( list_size( &codegen->shary.vars ) > 1 ) {
      codegen->shary.used = true;
   }
   // When the shared array contains a single variable and the variable is not
   // used by a reference, give the index of the shared array to the variable.
   else if ( list_size( &codegen->shary.vars ) == 1 ) {
      struct var* var = list_head( &codegen->shary.vars );
      if ( var->addr_taken ) {
         codegen->shary.used = true;
      }
      else {
         list_append( &codegen->vars, var );
      }
   }
   // Shared array not needed.
   else {
      --count;
   }
   // Don't go over the variable limit.
   if ( count > MAX_MAP_LOCATIONS ) {
      t_diag( codegen->task, DIAG_ERR | DIAG_FILE,
         &codegen->task->library_main->file_pos,
         "library uses over maximum %d variables", MAX_MAP_LOCATIONS );
      t_bail( codegen->task );
   }
}

static void alloc_dim_counter_var( struct codegen* codegen ) {
   // TODO: Determine if we need the dimension counter.
   codegen->shary.dim_counter_var = true;
}

// Order of functions:
// - null handler
// - imported functions
// - functions
// - hidden functions
static void clarify_funcs( struct codegen* codegen ) {
   // Null handler.
   if ( codegen->lang == LANG_BCS &&
      codegen->task->library_main->uses_nullable_refs ) {
      struct func* func = t_alloc_func();
      func->impl = t_alloc_func_user();
      func->name = t_extend_name( codegen->task->root_name, "." );
      codegen->null_handler = func;
      list_append( &codegen->funcs, func );
   }
   // Imported functions.
   struct list_iter i;
   list_iterate( &codegen->task->library_main->dynamic, &i );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      struct list_iter k;
      list_iterate( &lib->funcs, &k );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         if ( impl->usage ) {
            list_append( &codegen->funcs, func );
         }
         list_next( &k );
      }
      list_next( &i );
   }
   // External functions.
   list_iterate( &codegen->task->library_main->external_funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      if ( func->imported && impl->usage ) {
         list_append( &codegen->funcs, list_data( &i ) );
      }
      list_next( &i );
   }
   // Functions.
   list_iterate( &codegen->task->library_main->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( ! func->hidden ) {
         list_append( &codegen->funcs, func );
      }
      list_next( &i );
   }
   // Hidden functions.
   list_iterate( &codegen->task->library_main->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( func->hidden ) {
         list_append( &codegen->funcs, func );
      }
      list_next( &i );
   }
   // In Little-E, the field of the function-call instruction that stores the
   // index of the function is a byte in size, allowing up to 256 different
   // functions to be called.
   // NOTE: Maybe automatically switch to the Big-E format?
   if ( codegen->task->library_main->format == FORMAT_LITTLE_E &&
      list_size( &codegen->funcs ) > MAX_LIB_FUNCS ) {
      t_diag( codegen->task, DIAG_ERR | DIAG_FILE,
         &codegen->task->library_main->file_pos,
         "library uses over maximum %d functions", MAX_LIB_FUNCS );
      t_diag( codegen->task, DIAG_FILE, &codegen->task->library_main->file_pos,
         "to use more functions, try using the #nocompact directive" );
      t_bail( codegen->task );
   }
}

static void assign_func_indexes( struct codegen* codegen ) {
   int index = 0;
   struct list_iter i;
   list_iterate( &codegen->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct func_user* impl = func->impl;
      impl->index = index;
      ++index;
      list_next( &i );
   }
}

// Shared-array layout:
// - null-element/dimension-counter
// - dimension-information
// - data
static void setup_shary( struct codegen* codegen ) {
   if ( codegen->shary.used ) {
      // null-element/dimension-counter
      ++codegen->shary.size;
      setup_diminfo( codegen );
      setup_data( codegen );
   }
}

// TODO (Optimization): Objects with the greater number of dimensions need to
// be output first. This way, objects with smaller number of dimensions can
// reuse the dimension-size values of the objects with the greater number of
// dimensions, since they might form a substring of the values. This eliminates
// duplicates.
static void setup_diminfo( struct codegen* codegen ) {
   codegen->shary.diminfo_offset = codegen->shary.size;
   // Variables.
   struct list_iter i;
   list_iterate( &codegen->task->library_main->vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->dim && var->addr_taken ) {
         var->diminfo_start = append_dim( codegen, var->dim );
      }
      list_next( &i );
   }
   // Structures.
   list_iterate( &codegen->task->structures, &i );
   while ( ! list_end( &i ) ) {
      struct structure* structure = list_data( &i );
      struct structure_member* member = structure->member;
      while ( member ) {
         if ( member->dim && member->addr_taken ) {
            member->diminfo_start = append_dim( codegen, member->dim );
         }
         member = member->next;
      }
      list_next( &i );
   }
   codegen->shary.size += codegen->shary.diminfo_size;
}

static int append_dim( struct codegen* codegen, struct dim* candidate_dim ) {
   int offset = codegen->shary.diminfo_offset;
   struct list_iter i;
   list_iterate( &codegen->shary.dims, &i );
   while ( ! list_end( &i ) ) {
      if ( same_dim( candidate_dim, i ) ) {
         return offset;
      }
      ++offset;
      list_next( &i );
   }
   struct dim* dim = candidate_dim;
   while ( dim ) {
      list_append( &codegen->shary.dims, dim );
      ++codegen->shary.diminfo_size;
      dim = dim->next;
   }
   return offset;
}

static bool same_dim( struct dim* dim, struct list_iter i ) {
   while ( dim != NULL && ! list_end( &i ) &&
      t_dim_size( dim ) == t_dim_size( list_data( &i ) ) ) {
      list_next( &i );
      dim = dim->next;
   }
   return ( dim == NULL && list_end( &i ) );
}

static void setup_data( struct codegen* codegen ) {
   codegen->shary.data_offset = codegen->shary.size;
   struct list_iter i;
   list_iterate( &codegen->shary.vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      var->index = codegen->shary.size;
      var->in_shared_array = true;
      codegen->shary.size += var->size;
      list_next( &i );
   }
}

static void patch_initz( struct codegen* codegen ) {
   patch_initz_list( codegen, &codegen->vars );
   patch_initz_list( codegen, &codegen->shary.vars );
}

static void patch_initz_list( struct codegen* codegen, struct list* vars ) {
   struct list_iter i;
   list_iterate( vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      struct value* value = var->value;
      while ( value ) {
         patch_value( codegen, value );
         value = value->next;
      }
      list_next( &i );
   }
}

static void patch_value( struct codegen* codegen, struct value* value ) {
   switch ( value->type ) {
   case VALUE_ARRAYREF:
      value->more.arrayref.offset =
         value->more.arrayref.var->index + value->expr->value;
      if ( value->more.arrayref.structure_member ) {
         value->more.arrayref.diminfo =
            value->more.arrayref.structure_member->diminfo_start +
            value->more.arrayref.diminfo;
      }
      else {
         value->more.arrayref.diminfo =
            value->more.arrayref.var->diminfo_start +
            value->more.arrayref.diminfo;
      }
      break;
   case VALUE_STRUCTREF:
      value->more.structref.offset = value->more.structref.var->index +
         value->expr->value;
      break;
   case VALUE_STRING:
      c_append_string( codegen, value->more.string.string );
      break;
   case VALUE_FUNCREF:
   case VALUE_STRINGINITZ:
   case VALUE_EXPR:
      break;
   default:
      UNREACHABLE();
      c_bail( codegen );
   }
} 

// The reason for sorting the variables is to reduce the size of the chunks.
// A chunk might need some of the variables but not all, so the variables are
// ordered in such a way that only those that are needed appear first. Now take
// all of the chunks into account and try to find the order that best reduces
// the size of as many chunks as possible. (It is extra work and added
// complexity for a small gain, but going the distance is what separates a
// professional grade product from a toy.)
//
// Sorting order:
// - arrays
// - scalars, with-no-value
// - scalars, with-value
// - scalars, with-value, hidden
// - scalars, with-no-value, hidden
// - arrays, hidden
static void sort_vars( struct codegen* codegen ) {
   struct list arrays;
   struct list zero_scalars;
   struct list nonzero_scalars;
   struct list zerohidden_scalars;
   struct list nonzerohidden_scalars;
   struct list hidden_arrays;
   list_init( &arrays );
   list_init( &zero_scalars );
   list_init( &nonzero_scalars );
   list_init( &zerohidden_scalars );
   list_init( &nonzerohidden_scalars );
   list_init( &hidden_arrays );
   while ( list_size( &codegen->vars ) > 0 ) {
      struct var* var = list_shift( &codegen->vars );
      // Arrays.
      if ( c_is_public_array( var ) ) {
         list_append( &arrays, var );
      }
      // Scalars, with-no-value.
      else if ( c_is_public_zero_scalar_var( var ) ) {
         list_append( &zero_scalars, var );
      }
      // Scalars, with-value.
      else if ( c_is_public_nonzero_scalar_var( var ) ) {
         list_append( &nonzero_scalars, var );
      }
      // Scalars, with-value, hidden.
      else if ( c_is_hidden_nonzero_scalar_var( var ) ) {
         list_append( &zerohidden_scalars, var );
      }
      // Scalars, with-no-value, hidden.
      else if ( c_is_hidden_zero_scalar_var( var ) ) {
         list_append( &nonzerohidden_scalars, var );
      }
      // Arrays, hidden.
      else if ( c_is_hidden_array( var ) ) {
         list_append( &hidden_arrays, var );
      }
      else {
         UNREACHABLE();
         c_bail( codegen );
      }
   }
   list_merge( &codegen->vars, &arrays );
   list_merge( &codegen->vars, &zero_scalars );
   list_merge( &codegen->vars, &nonzero_scalars );
   list_merge( &codegen->vars, &zerohidden_scalars );
   list_merge( &codegen->vars, &nonzerohidden_scalars );
   list_merge( &codegen->vars, &hidden_arrays );
   struct list_iter i;
   list_iterate( &codegen->vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( c_is_scalar_var( var ) ) {
         list_append( &codegen->scalars, var );
      }
      else {
         list_append( &codegen->arrays, var );
      }
      list_next( &i );
   }
   list_iterate( &codegen->imported_vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( c_is_scalar_var( var ) ) {
         list_append( &codegen->imported_scalars, var );
      }
      else {
         list_append( &codegen->imported_arrays, var );
      }
      list_next( &i );
   }
}

bool c_is_array( struct var* var ) {
   switch ( var->desc ) {
   case DESC_ARRAY:
   case DESC_STRUCTVAR:
      return true;
   case DESC_REFVAR:
      // An array reference consists of the offset to the first element of the
      // array and the offset to the dimension information, so we need two
      // scalar variables to store the information. However, it's not desirable
      // to waste two indexes, so use an array instead.
      return ( var->ref->type == REF_ARRAY );
   default:
      return false;
   }
}

bool c_is_public_array( struct var* var ) {
   return ( c_is_array( var ) && ! var->hidden );
}

bool c_is_hidden_array( struct var* var ) {
   return ( c_is_array( var ) && var->hidden );
}

bool c_is_scalar_var( struct var* var ) {
   switch ( var->desc ) {
   case DESC_PRIMITIVEVAR:
      return true;
   case DESC_REFVAR:
      return ( var->ref->type == REF_STRUCTURE ||
         var->ref->type == REF_FUNCTION );
   default:
      return false;
   }
}

static bool is_initz_zero( struct value* value ) {
   if ( value ) {
      STATIC_ASSERT( VALUE_TOTAL == 6 );
      switch ( value->type ) {
      case VALUE_EXPR:
         return ( value->expr->value == 0 );
      case VALUE_STRING:
         return ( value->more.string.string->index_runtime == 0 );
      case VALUE_FUNCREF:
         {
            struct func_user* impl = value->more.funcref.func->impl;
            return ( impl->index == 0 );
         }
         break;
      case VALUE_STRUCTREF:
         return false;
      default:
         break;
      }
   }
   return true;
}

bool c_is_zero_scalar_var( struct var* var ) {
   return ( c_is_scalar_var( var ) && is_initz_zero( var->value ) );
}

bool c_is_nonzero_scalar_var( struct var* var ) {
   return ( c_is_scalar_var( var ) && ! is_initz_zero( var->value ) );
}

bool c_is_public_zero_scalar_var( struct var* var ) {
   return ( c_is_zero_scalar_var( var ) && ! var->hidden );
}

bool c_is_public_nonzero_scalar_var( struct var* var ) {
   return ( c_is_nonzero_scalar_var( var ) && ! var->hidden );
}

bool c_is_hidden_zero_scalar_var( struct var* var ) {
   return ( c_is_zero_scalar_var( var ) && var->hidden );
}

bool c_is_hidden_nonzero_scalar_var( struct var* var ) {
   return ( c_is_nonzero_scalar_var( var ) && var->hidden );
}

static void assign_indexes( struct codegen* codegen ) {
   // NOTE: The indexes need to be assigned in the same order as the variables
   // are allocated.
   int index = 0;
   // Variables.
   struct list_iter i;
   list_iterate( &codegen->vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      var->index = index;
      ++index;
      list_next( &i );
   }
   // Imported variables.
   list_iterate( &codegen->imported_vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      var->index = index;
      ++index;
      list_next( &i );
   }
   // Shared array.
   if ( codegen->shary.used ) {
      codegen->shary.index = index;
      ++index;
      if ( codegen->shary.dim_counter_var ) {
         codegen->shary.dim_counter = index;
         ++index;
      }
   }
}

static void create_assert_strings( struct codegen* codegen ) {
   struct list_iter i;
   list_iterate( &codegen->task->runtime_asserts, &i );
   while ( ! list_end( &i ) ) {
      struct assert* assert = list_data( &i );
      // Create file string.
      const char* file = t_decode_pos_file( codegen->task, &assert->pos );
      struct indexed_string* string = t_intern_string( codegen->task,
         ( char* ) file, strlen( file ) );
      string->used = true;
      assert->file = string;
      list_next( &i );
   }
   // Create standard message-prefix string.
   static const char* message_prefix = "assertion failure";
   codegen->assert_prefix = t_intern_string( codegen->task,
      ( char* ) message_prefix, strlen( message_prefix ) );
   codegen->assert_prefix->used = true;
}

void c_append_string( struct codegen* codegen,
   struct indexed_string* string ) {
   // Allocate the index that the game engine will use for finding the string.
   if ( ! ( string->index_runtime >= 0 ) ) {
      string->index_runtime = codegen->runtime_index;
      ++codegen->runtime_index;
      list_append( &codegen->used_strings, string );
   }
}

static void append_acs_strings( struct codegen* codegen ) {
   struct indexed_string* string = codegen->task->str_table.head;
   while ( string ) {
      if ( string->in_source_code ) {
         c_append_string( codegen, string );
      }
      string = string->next;
   }
}

void c_diag( struct codegen* codegen, int flags, ... ) {
   va_list args;
   va_start( args, flags );
   t_diag_args( codegen->task, flags, &args );
   va_end( args );
}

void c_bail( struct codegen* codegen ) {
   t_bail( codegen->task );
}
