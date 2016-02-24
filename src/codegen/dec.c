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
static void add_default_params( struct codegen* codegen, struct func* func,
    int count_param, bool reset_count_param );
static void write_default_init( struct codegen* codegen, struct func* func,
   struct param* start, struct param* end, int count_param );
static bool zero_default_value( struct param* param );
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
   c_pcd( codegen, PCD_TERMINATE );
   if ( script->nested_funcs ) {
      struct nestedfunc_writing writing;
      init_nestedfunc_writing( &writing, script->nested_funcs,
         script->nested_calls, script->size );
      write_nested_funcs( codegen, &writing );
      script->size += writing.temps_size;
   }
   c_pop_block_visit( codegen );
   c_flush_pcode( codegen );
}

void write_userfunc( struct codegen* codegen, struct func* func ) {
   struct func_user* impl = func->impl;
   impl->obj_pos = c_tell( codegen );
   c_add_block_visit( codegen );
   add_default_params( codegen, func, func->max_param, true );
   c_write_block( codegen, impl->body, false );
   c_pcd( codegen, PCD_RETURNVOID );
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
            c_pcd( codegen, PCD_PUSHNUMBER, value->index );
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
         c_pcd( codegen, PCD_PUSHNUMBER, value->index + i );
         c_pcd( codegen, PCD_PUSHNUMBER, usage->string->value[ i ] );
         ++i;
      }
      while ( i ) {
         c_update_element( codegen, var->storage, var->index, AOP_NONE );
         --i;
      }
   }
   else {
      // Element index.
      c_pcd( codegen, PCD_PUSHNUMBER, value->index );
      // Variable index.
      c_pcd( codegen, PCD_PUSHNUMBER, var->index );
      // Element offset. Not used.
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      // Number of characters to copy.
      c_pcd( codegen, PCD_PUSHNUMBER, usage->string->length );
      // String index.
      c_pcd( codegen, PCD_PUSHNUMBER, usage->string->index );
      // String offset. Not used.
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, var->storage == STORAGE_WORLD ?
         PCD_STRCPYTOWORLDCHRANGE : PCD_STRCPYTOGLOBALCHRANGE );
      c_pcd( codegen, PCD_DROP );
      if ( include_nul ) {
         c_pcd( codegen, PCD_PUSHNUMBER,
            value->index + usage->string->length );
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
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
         c_pcd( codegen, PCD_PUSHNUMBER, start + i );
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         c_update_element( codegen, storage, index, AOP_NONE );
         ++i;
      }
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, start + size - 1 );
      struct c_point* loop_point = c_create_point( codegen );
      c_append_node( codegen, &loop_point->node );
      c_pcd( codegen, PCD_DUP );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_update_element( codegen, storage, index, AOP_NONE );
      struct c_casejump* exit_jump = c_create_casejump( codegen, 0, NULL );
      c_append_node( codegen, &exit_jump->node );
      c_pcd( codegen, PCD_PUSHNUMBER, 1 );
      c_pcd( codegen, PCD_SUBTRACT );
      struct c_jump* loop_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &loop_jump->node );
      loop_jump->point = loop_point;
      struct c_point* exit_point = c_create_point( codegen );
      c_append_node( codegen, &exit_point->node );
      exit_jump->value = start - 1;
      exit_jump->point = exit_point;
   }
}

// Implementation detail: a hidden parameter is used to store the number of
// arguments passed to the function. This parameter is found after the last
// visible parameter. The index of the parameter is @count_param.
void add_default_params( struct codegen* codegen, struct func* func,
   int count_param, bool reset_count_param ) {
   // Find first default parameter.
   struct param* param = func->params;
   while ( param && ! param->default_value ) {
      param = param->next;
   }
   if ( ! param ) {
      return;
   }
   // Find the minimum range that contains all non-zero values.
   struct param* start = param;
   struct param* end = start;
   while ( param ) {
      if ( ! zero_default_value( param ) ) {
         end = param->next;
      }
      param = param->next;
   }
   if ( start != end ) {
      write_default_init( codegen, func, start, end, count_param );
   }
   // Reset count parameter.
   if ( reset_count_param ) {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, count_param );
   }
}

void write_default_init( struct codegen* codegen, struct func* func,
   struct param* start, struct param* end, int count_param ) {
   c_pcd( codegen, PCD_PUSHSCRIPTVAR, count_param );
   struct c_sortedcasejump* table = c_create_sortedcasejump( codegen );
   c_append_node( codegen, &table->node );
   c_pcd( codegen, PCD_DROP );
   struct c_point* exit_point = c_create_point( codegen );
   struct c_jump* exit_jump = c_create_jump( codegen, PCD_GOTO );
   c_append_node( codegen, &exit_jump->node );
   exit_jump->point = exit_point;
   int args_passed = func->min_param;
   struct param* param = start;
   while ( param != end ) {
      struct c_point* init_point = c_create_point( codegen );
      c_append_node( codegen, &init_point->node );
      if ( ! zero_default_value( param ) ) {
         c_push_expr( codegen, param->default_value, false );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, param->index );
      }
      struct c_casejump* entry = c_create_casejump( codegen,
         args_passed, init_point );
      c_append_casejump( table, entry );
      ++args_passed;
      param = param->next;
   }
   c_append_node( codegen, &exit_point->node );
}

bool zero_default_value( struct param* param ) {
   return ( param->default_value->folded && param->default_value->value == 0 );
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