#include "phase.h"
#include "pcode.h"

#include <stdio.h>

struct nestedfunc_writing {
   struct func* nested_funcs;
   struct call* nested_calls;
   int temps_start;
   int temps_size;
};

static void write_script( struct codegen* codegen, struct script* script );
static void write_userfunc( struct codegen* codegen, struct func* func );
static void write_world_initz( struct codegen* codegen, struct var* var );
static void write_stringinitz( struct codegen* codegen, struct var* var,
   struct value* value, bool include_nul );
static void write_world_multi_initz( struct codegen* codegen,
   struct var* var );
static void nullify_array( struct codegen* codegen, int storage, int index,
   int start, int size );
static void add_default_params( struct codegen* codegen, struct func*,
   int count_param, bool reset_count_param );
void init_nestedfunc_writing( struct nestedfunc_writing* writing,
   struct func* nested_funcs, struct call* nested_calls, int temps_start );
static void write_nested_funcs( struct codegen* codegen,
   struct nestedfunc_writing* writing );
static void write_one_nestedfunc( struct codegen* codegen,
   struct nestedfunc_writing* writing, struct func* func );
static void patch_nestedfunc_addresses( struct codegen* codegen,
   struct func* func );

void c_write_user_code( struct codegen* codegen ) {
   // Scripts.
   list_iter_t i;
   list_iter_init( &i, &codegen->task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      write_script( codegen, list_data( &i ) );
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &codegen->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      write_userfunc( codegen, list_data( &i ) );
      list_next( &i );
   }
   // When utilizing the Little-E format, where instructions can be of
   // different size, add padding so any following data starts at an offset
   // that is multiple of 4.
   if ( codegen->task->library_main->format == FORMAT_LITTLE_E ) {
      int i = alignpad( c_tell( codegen ), 4 );
      while ( i ) {
         c_add_opc( codegen, PCD_TERMINATE );
         --i;
      }
   }
}

void write_script( struct codegen* codegen, struct script* script ) {
   script->offset = c_tell( codegen );
   c_add_block_visit( codegen );
   c_write_stmt( codegen, script->body );
   c_add_opc( codegen, PCD_TERMINATE );
   if ( script->nested_funcs ) {
      struct nestedfunc_writing writing;
      init_nestedfunc_writing( &writing, script->nested_funcs,
         script->nested_calls, script->size );
      write_nested_funcs( codegen, &writing );
      script->size += writing.temps_size;
   }
   c_pop_block_visit( codegen );
}

void write_userfunc( struct codegen* codegen, struct func* func ) {
   struct func_user* impl = func->impl;
   impl->obj_pos = c_tell( codegen );
   c_add_block_visit( codegen );
   add_default_params( codegen, func, func->max_param, true );
   c_write_block( codegen, impl->body, false );
   c_add_opc( codegen, PCD_RETURNVOID );
   if ( impl->nested_funcs ) {
      struct nestedfunc_writing writing;
      init_nestedfunc_writing( &writing, impl->nested_funcs,
         impl->nested_calls, impl->size );
      write_nested_funcs( codegen, &writing );
      impl->size += writing.temps_size;
   }
   c_pop_block_visit( codegen );
}

void c_visit_var( struct codegen* codegen, struct var* var ) {
   if ( var->storage == STORAGE_LOCAL ) {
      if ( var->value ) {
         c_push_expr( codegen, var->value->expr, false );
         c_update_indexed( codegen, var->storage, var->index, AOP_NONE );
      }
   }
   else if ( var->storage == STORAGE_WORLD ||
      var->storage == STORAGE_GLOBAL ) {
      if ( var->initial ) {
         if ( var->initial->multi ) {
            write_world_multi_initz( codegen, var );
         }
         else {
            write_world_initz( codegen, var );
         }
      }
   }
}

void write_world_initz( struct codegen* codegen, struct var* var ) {
   if ( var->value->string_initz ) {
      write_stringinitz( codegen, var, var->value, true );
   }
   else {
      c_push_expr( codegen, var->value->expr, false );
      c_update_indexed( codegen, var->storage, var->index, AOP_NONE );
   }
}

void write_world_multi_initz( struct codegen* codegen, struct var* var ) {
   // Determine segment of the array to nullify.
   // -----------------------------------------------------------------------
   // The segment begins at the first element with a value of zero, or at the
   // first unitialized element. The segment ends at the element after the
   // last element with a value of zero, or at the element after the last
   // unintialized element. The end-point element is not nullified.
   int start = 0;
   int end = 0;
   struct value* value = var->value;
   while ( value ) {
      int index = 0;
      if ( value->string_initz ) {
         struct indexed_string_usage* usage =
            ( struct indexed_string_usage* ) value->expr->root;
         index = value->index + usage->string->length + 1;
      }
      else {
         index = value->index;
         bool zero = ( value->expr->folded && value->expr->value == 0 );
         bool lib_string = ( value->expr->has_str &&
            codegen->task->library_main->importable );
         if ( ! zero || lib_string ) {
            ++index;
         }
      }
      int next_index = ( value->next ? value->next->index : var->size );
      if ( index != next_index ) {
         if ( ! end ) {
            start = index;
         }
         end = next_index;
      }
      value = value->next;
   }
   if ( start != end ) {
      nullify_array( codegen, var->storage, var->index, start, end - start );
   }
   // Assign values to elements.
   // -----------------------------------------------------------------------
   value = var->value;
   while ( value ) {
      if ( value->string_initz ) {
         // Don't include the NUL character if the element to contain the
         // character has been nullified.
         struct indexed_string_usage* usage =
            ( struct indexed_string_usage* ) value->expr->root;
         int nul_index = value->index + usage->string->length;
         bool include_nul = ( ! ( nul_index >= start && nul_index < end ) );
         write_stringinitz( codegen, var, value, include_nul );
      }
      else {
         bool zero = ( value->expr->folded && value->expr->value == 0 );
         bool lib_string = ( value->expr->has_str &&
            codegen->task->library_main->importable );
         if ( ! zero || lib_string ) {
            c_add_opc( codegen, PCD_PUSHNUMBER );
            c_add_arg( codegen, value->index );
            c_push_expr( codegen, value->expr, false );
            c_update_element( codegen, var->storage, var->index, AOP_NONE );
         }
      }
      value = value->next;
   }
}

void write_stringinitz( struct codegen* codegen, struct var* var,
   struct value* value, bool include_nul ) {
   struct indexed_string_usage* usage =
      ( struct indexed_string_usage* ) value->expr->root;
   // Optimization: If the string is small enough, initialize each element
   // with a separate set of instructions.
   enum { UNROLL_LIMIT = 3 };
   if ( usage->string->length <= UNROLL_LIMIT ) {
      int length = usage->string->length;
      if ( include_nul ) {
         ++length;
      }
      int i = 0;
      while ( i < length ) {
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, value->index + i );
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, usage->string->value[ i ] );
         ++i;
      }
      while ( i ) {
         c_update_element( codegen, var->storage, var->index, AOP_NONE );
         --i;
      }
   }
   else {
      // Element index.
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, value->index );
      // Variable index.
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, var->index );
      // Element offset. Not used.
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
      // Number of characters to copy.
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, usage->string->length );
      // String index.
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, usage->string->index );
      // String offset. Not used.
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
      c_add_opc( codegen, var->storage == STORAGE_WORLD ?
         PCD_STRCPYTOWORLDCHRANGE : PCD_STRCPYTOGLOBALCHRANGE );
      c_add_opc( codegen, PCD_DROP );
      if ( include_nul ) {
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, value->index + usage->string->length );
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, 0 );
         c_update_element( codegen, var->storage, var->index, AOP_NONE );
      }
      usage->string->used = true;
   }
}

void nullify_array( struct codegen* codegen, int storage, int index,
   int start, int size ) {
   // Optimization: If the size of the sub-array is small enough, initialize
   // each element with a separate set of instructions.
   enum { UNROLL_LIMIT = 4 };
   if ( size <= UNROLL_LIMIT ) {
      int i = 0;
      while ( i < size ) {
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, start + i );
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, 0 );
         c_update_element( codegen, storage, index, AOP_NONE );
         ++i;
      }
   }
   else {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, start + size );
      int loop = c_tell( codegen );
      c_add_opc( codegen, PCD_CASEGOTO );
      c_add_arg( codegen, 0 );
      c_add_arg( codegen, 0 );
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 1 );
      c_add_opc( codegen, PCD_SUBTRACT );
      c_add_opc( codegen, PCD_DUP );
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
      c_update_element( codegen, storage, index, AOP_NONE );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, loop );
      int done = c_tell( codegen );
      c_seek( codegen, loop );
      c_add_opc( codegen, PCD_CASEGOTO );
      c_add_arg( codegen, start );
      c_add_arg( codegen, done );
      c_seek_end( codegen );
   }
}

void add_default_params( struct codegen* codegen, struct func* func,
   int count_param, bool reset_count_param ) {
   // Find first default parameter.
   struct param* param = func->params;
   while ( param && ! param->default_value ) {
      param = param->next;
   }
   int num = 0;
   int num_cases = 0;
   struct param* start = param;
   while ( param ) {
      ++num;
      if ( ! param->default_value->folded || param->default_value->value ) {
         num_cases = num;
      }
      param = param->next;
   }
   if ( num_cases ) {
      // A hidden parameter is used to store the number of arguments passed to
      // the function. This parameter is found after the last visible parameter.
      c_add_opc( codegen, PCD_PUSHSCRIPTVAR );
      c_add_arg( codegen, count_param );
      int jump = c_tell( codegen );
      c_add_opc( codegen, PCD_CASEGOTOSORTED );
      c_add_arg( codegen, 0 );
      param = start;
      int i = 0;
      while ( param && i < num_cases ) {
         c_add_arg( codegen, 0 );
         c_add_arg( codegen, 0 );
         param = param->next;
         ++i;
      }
      c_add_opc( codegen, PCD_DROP );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, 0 );
      param = start;
      while ( param ) {
         param->obj_pos = c_tell( codegen );
         if ( ! param->default_value->folded || param->default_value->value ) {
            c_push_expr( codegen, param->default_value, false );
            c_update_indexed( codegen, STORAGE_LOCAL, param->index, AOP_NONE );
         }
         param = param->next;
      }
      int done = c_tell( codegen );
      // Add case positions.
      c_seek( codegen, jump );
      c_add_opc( codegen, PCD_CASEGOTOSORTED );
      c_add_arg( codegen, num_cases );
      num = func->min_param;
      param = start;
      while ( param && num_cases ) {
         c_add_arg( codegen, num );
         c_add_arg( codegen, param->obj_pos );
         param = param->next;
         --num_cases;
         ++num;
      }
      c_add_opc( codegen, PCD_DROP );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, done );
      c_seek( codegen, done );
   }
   if ( start ) {
      // Reset the parameter-count parameter.
      if ( reset_count_param ) {
         c_add_opc( codegen, PCD_PUSHNUMBER );
         c_add_arg( codegen, 0 );
         c_add_opc( codegen, PCD_ASSIGNSCRIPTVAR );
         c_add_arg( codegen, count_param );
      }
   }
}

void init_nestedfunc_writing( struct nestedfunc_writing* writing,
   struct func* nested_funcs, struct call* nested_calls, int temps_start ) {
   writing->nested_funcs = nested_funcs;
   writing->nested_calls = nested_calls;
   writing->temps_start = temps_start;
   writing->temps_size = 0;
}

void write_nested_funcs( struct codegen* codegen,
   struct nestedfunc_writing* writing ) {
   codegen->func_visit->nested_func = true;
   struct func* func = writing->nested_funcs;
   while ( func ) {
      write_one_nestedfunc( codegen, writing, func );
      struct func_user* impl = func->impl;
      func = impl->next_nested;
   }
   // Insert address of function into each call.
   func = writing->nested_funcs;
   while ( func ) {
      struct func_user* impl = func->impl;
      patch_nestedfunc_addresses( codegen, func );
      func = impl->next_nested;
   }
   c_seek_end( codegen );
   codegen->func_visit->nested_func = false;
}

void write_one_nestedfunc( struct codegen* codegen,
   struct nestedfunc_writing* writing, struct func* func ) {
   struct func_user* impl = func->impl;

   // Count number of times function is called.
   int num_entries = 0;
   struct call* call = impl->nested_calls;
   while ( call ) {
      ++num_entries;
      struct nested_call* nested = call->nested_call;
      call = nested->next;
   }
   // Don't write the function when it isn't used.
   if ( ! num_entries ) {
      return;
   }

   // Prologue:
   // -----------------------------------------------------------------------
   impl->obj_pos = c_tell( codegen );

   // Assign arguments to temporary space.
   int temps_index = writing->temps_start;
   if ( func->min_param != func->max_param ) {
      c_add_opc( codegen, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( codegen, temps_index );
      ++temps_index;
   }
   struct param* param = func->params;
   while ( param ) {
      c_add_opc( codegen, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( codegen, temps_index );
      ++temps_index;
      param = param->next;
   }

   // The function uses variables of the script or function as its workspace.
   // Save the values of these variables onto the stack. They will be restored
   // at epilogue.
   int i = 0;
   while ( i < impl->size ) {
      c_add_opc( codegen, PCD_PUSHSCRIPTVAR );
      c_add_arg( codegen, impl->index_offset + i );
      ++i;
   }

   // Activate parameters.
   int temps_size = temps_index - writing->temps_start;
   param = func->params;
   while ( param ) {
      --temps_index;
      c_add_opc( codegen, PCD_PUSHSCRIPTVAR );
      c_add_arg( codegen, temps_index );
      c_add_opc( codegen, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( codegen, param->index );
      param = param->next;
   }
   if ( func->min_param != func->max_param ) {
      --temps_index;
      add_default_params( codegen, func, temps_index, false );
   }

   // Body:
   // -----------------------------------------------------------------------
   int body_pos = c_tell( codegen );
   c_write_block( codegen, impl->body, true );
   if ( func->return_type ) {
      c_add_opc( codegen, PCD_PUSHNUMBER );
      c_add_arg( codegen, 0 );
   }

   // Epilogue:
   // -----------------------------------------------------------------------
   int epilogue_pos = c_tell( codegen );

   // Temporarily save the return-value.
   int temps_return = 0;
   if ( impl->size > 2 && func->return_type ) {
      // Use the last temporary variable.
      temps_return = writing->temps_start;
      temps_size += ( ! temps_size ? 1 : 0 );
      c_add_opc( codegen, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( codegen, temps_return );
   }

   // Restore previous values of script variables.
   i = 0;
   while ( i < impl->size ) {
      if ( impl->size <= 2 && func->return_type ) {
         c_add_opc( codegen, PCD_SWAP );
      }
      c_add_opc( codegen, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( codegen, impl->index_offset + impl->size - i - 1 );
      ++i;
   }

   // Return-value.
   if ( func->return_type ) {
      if ( impl->size > 2 ) {
         c_add_opc( codegen, PCD_PUSHSCRIPTVAR );
         c_add_arg( codegen, temps_return );
      }
      c_add_opc( codegen, PCD_SWAP );
   }

   // Output return table.
   impl->return_pos = c_tell( codegen );
   c_add_opc( codegen, PCD_CASEGOTOSORTED );
   c_add_arg( codegen, num_entries );
   call = impl->nested_calls;
   while ( call ) {
      c_add_arg( codegen, 0 );
      c_add_arg( codegen, 0 );
      call = call->nested_call->next;
   }

   // Patch address of return-statements.
   struct return_stmt* stmt = impl->returns;
   while ( stmt ) {
      c_seek( codegen, stmt->obj_pos );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, epilogue_pos );
      stmt = stmt->next;
   }
   c_seek_end( codegen );

   if ( temps_size > writing->temps_size ) {
      writing->temps_size = temps_size;
   }
}

void patch_nestedfunc_addresses( struct codegen* codegen, struct func* func ) {
   struct func_user* impl = func->impl; 
   // Correct calls to this function.
   int num_entries = 0;
   struct call* call = impl->nested_calls;
   while ( call ) {
      c_seek( codegen, call->nested_call->enter_pos );
      c_add_opc( codegen, PCD_GOTO );
      c_add_arg( codegen, impl->obj_pos );
      call = call->nested_call->next;
      ++num_entries;
   }
   // Correct return-addresses in return-table.
   c_seek( codegen, impl->return_pos );
   c_add_opc( codegen, PCD_CASEGOTOSORTED );
   c_add_arg( codegen, num_entries );
   call = impl->nested_calls;
   while ( call ) {
      c_add_arg( codegen, call->nested_call->id );
      c_add_arg( codegen, call->nested_call->leave_pos );
      call = call->nested_call->next;
   }
}