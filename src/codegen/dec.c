#include "phase.h"
#include "pcode.h"

#include <stdio.h>

struct nestedfunc_writing {
   struct func* nested_funcs;
   struct call* nested_calls;
   int temps_start;
   int temps_size;
};

static void write_script( struct codegen* phase, struct script* script );
static void write_userfunc( struct codegen* phase, struct func* func );
static void write_world_initz( struct codegen* phase, struct var* var );
static void init_world_array( struct codegen* phase, struct var* );
static void add_default_params( struct codegen* phase, struct func*,
   int count_param, bool reset_count_param );
void init_nestedfunc_writing( struct nestedfunc_writing* writing,
   struct func* nested_funcs, struct call* nested_calls, int temps_start );
static void write_nested_funcs( struct codegen* phase,
   struct nestedfunc_writing* writing );
static void write_one_nestedfunc( struct codegen* phase,
   struct nestedfunc_writing* writing, struct func* func );
static void patch_nestedfunc_addresses( struct codegen* phase,
   struct func* func );

void c_write_user_code( struct codegen* phase ) {
   // Scripts.
   list_iter_t i;
   list_iter_init( &i, &phase->task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      write_script( phase, list_data( &i ) );
      list_next( &i );
   }
   // Functions.
   list_iter_init( &i, &phase->task->library_main->funcs );
   while ( ! list_end( &i ) ) {
      write_userfunc( phase, list_data( &i ) );
      list_next( &i );
   }
   // When utilizing the Little-E format, where instructions can be of
   // different size, add padding so any following data starts at an offset
   // that is multiple of 4.
   if ( phase->task->library_main->format == FORMAT_LITTLE_E ) {
      int i = alignpad( c_tell( phase ), 4 );
      while ( i ) {
         c_add_opc( phase, PCD_TERMINATE );
         --i;
      }
   }
}

void write_script( struct codegen* phase, struct script* script ) {
   script->offset = c_tell( phase );
   c_add_block_visit( phase );
   c_write_stmt( phase, script->body );
   c_add_opc( phase, PCD_TERMINATE );
   if ( script->nested_funcs ) {
      struct nestedfunc_writing writing;
      init_nestedfunc_writing( &writing, script->nested_funcs,
         script->nested_calls, script->size );
      write_nested_funcs( phase, &writing );
      script->size += writing.temps_size;
   }
   c_pop_block_visit( phase );
}

void write_userfunc( struct codegen* phase, struct func* func ) {
   struct func_user* impl = func->impl;
   impl->obj_pos = c_tell( phase );
   c_add_block_visit( phase );
   add_default_params( phase, func, func->max_param, true );
   c_write_block( phase, impl->body, false );
   c_add_opc( phase, PCD_RETURNVOID );
   if ( impl->nested_funcs ) {
      struct nestedfunc_writing writing;
      init_nestedfunc_writing( &writing, impl->nested_funcs,
         impl->nested_calls, impl->size );
      write_nested_funcs( phase, &writing );
      impl->size += writing.temps_size;
   }
   c_pop_block_visit( phase );
}

void c_visit_var( struct codegen* phase, struct var* var ) {
   if ( var->storage == STORAGE_LOCAL ) {
      if ( var->value ) {
         c_push_expr( phase, var->value->expr, false );
         c_update_indexed( phase, var->storage, var->index, AOP_NONE );
      }
   }
   else if ( var->storage == STORAGE_WORLD ||
      var->storage == STORAGE_GLOBAL ) {
      if ( var->initial && ! var->is_constant_init ) {
         if ( var->initial->multi ) {
            init_world_array( phase, var );
         }
         else {
            write_world_initz( phase, var );
         }
      }
   }
}

void write_world_initz( struct codegen* phase, struct var* var ) {
   if ( var->value->string_initz ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) var->value->expr->root;
      for ( int i = 0; i < usage->string->length; ++i ) {
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, i );
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, usage->string->value[ i ] );
         c_update_element( phase, var->storage, var->index, AOP_NONE );
      }
   }
   else {
      c_push_expr( phase, var->value->expr, false );
      c_update_indexed( phase, var->storage, var->index, AOP_NONE );
   }
}

void init_world_array( struct codegen* phase, struct var* var ) {
   // Nullify array.
   c_add_opc( phase, PCD_PUSHNUMBER );
   c_add_arg( phase, var->size - 1 );
   int loop = c_tell( phase );
   c_add_opc( phase, PCD_CASEGOTO );
   c_add_arg( phase, 0 );
   c_add_arg( phase, 0 );
   c_add_opc( phase, PCD_DUP );
   c_add_opc( phase, PCD_PUSHNUMBER );
   c_add_arg( phase, 0 );
   c_update_element( phase, var->storage, var->index, AOP_NONE );
   c_add_opc( phase, PCD_PUSHNUMBER );
   c_add_arg( phase, 1 );
   c_add_opc( phase, PCD_SUBTRACT );
   c_add_opc( phase, PCD_GOTO );
   c_add_arg( phase, loop );
   int done = c_tell( phase );
   c_seek( phase, loop );
   c_add_opc( phase, PCD_CASEGOTO );
   c_add_arg( phase, -1 );
   c_add_arg( phase, done );
   c_seek_end( phase );
   // Assign elements.
   struct value* value = var->value;
   while ( value ) {
      // Initialize an element only if the value is not 0, because the array
      // is already nullified. String values from libraries are an exception,
      // because they need to be tagged regardless of the value.
      if ( ( ! value->expr->folded || value->expr->value ) ||
         ( value->expr->has_str && phase->task->library_main->importable ) ) {
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, value->index );
         if ( value->expr->folded && ! value->expr->has_str ) { 
            c_add_opc( phase, PCD_PUSHNUMBER );
            c_add_arg( phase, value->expr->value );
         }
         else {
            c_push_expr( phase, value->expr, false );
         }
         c_update_element( phase, var->storage, var->index, AOP_NONE );
      }
      value = value->next;
   }
}

void add_default_params( struct codegen* phase, struct func* func,
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
      c_add_opc( phase, PCD_PUSHSCRIPTVAR );
      c_add_arg( phase, count_param );
      int jump = c_tell( phase );
      c_add_opc( phase, PCD_CASEGOTOSORTED );
      c_add_arg( phase, 0 );
      param = start;
      int i = 0;
      while ( param && i < num_cases ) {
         c_add_arg( phase, 0 );
         c_add_arg( phase, 0 );
         param = param->next;
         ++i;
      }
      c_add_opc( phase, PCD_DROP );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, 0 );
      param = start;
      while ( param ) {
         param->obj_pos = c_tell( phase );
         if ( ! param->default_value->folded || param->default_value->value ) {
            c_push_expr( phase, param->default_value, false );
            c_update_indexed( phase, STORAGE_LOCAL, param->index, AOP_NONE );
         }
         param = param->next;
      }
      int done = c_tell( phase );
      // Add case positions.
      c_seek( phase, jump );
      c_add_opc( phase, PCD_CASEGOTOSORTED );
      c_add_arg( phase, num_cases );
      num = func->min_param;
      param = start;
      while ( param && num_cases ) {
         c_add_arg( phase, num );
         c_add_arg( phase, param->obj_pos );
         param = param->next;
         --num_cases;
         ++num;
      }
      c_add_opc( phase, PCD_DROP );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, done );
      c_seek( phase, done );
   }
   if ( start ) {
      // Reset the parameter-count parameter.
      if ( reset_count_param ) {
         c_add_opc( phase, PCD_PUSHNUMBER );
         c_add_arg( phase, 0 );
         c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
         c_add_arg( phase, count_param );
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

void write_nested_funcs( struct codegen* phase,
   struct nestedfunc_writing* writing ) {
   phase->func_visit->nested_func = true;
   struct func* func = writing->nested_funcs;
   while ( func ) {
      write_one_nestedfunc( phase, writing, func );
      struct func_user* impl = func->impl;
      func = impl->next_nested;
   }
   // Insert address of function into each call.
   func = writing->nested_funcs;
   while ( func ) {
      struct func_user* impl = func->impl;
      patch_nestedfunc_addresses( phase, func );
      func = impl->next_nested;
   }
   c_seek_end( phase );
   phase->func_visit->nested_func = false;
}

void write_one_nestedfunc( struct codegen* phase,
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
   impl->obj_pos = c_tell( phase );

   // Assign arguments to temporary space.
   int temps_index = writing->temps_start;
   if ( func->min_param != func->max_param ) {
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, temps_index );
      ++temps_index;
   }
   struct param* param = func->params;
   while ( param ) {
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, temps_index );
      ++temps_index;
      param = param->next;
   }

   // The function uses variables of the script or function as its workspace.
   // Save the values of these variables onto the stack. They will be restored
   // at epilogue.
   int i = 0;
   while ( i < impl->size ) {
      c_add_opc( phase, PCD_PUSHSCRIPTVAR );
      c_add_arg( phase, impl->index_offset + i );
      ++i;
   }

   // Activate parameters.
   int temps_size = temps_index - writing->temps_start;
   param = func->params;
   while ( param ) {
      --temps_index;
      c_add_opc( phase, PCD_PUSHSCRIPTVAR );
      c_add_arg( phase, temps_index );
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, param->index );
      param = param->next;
   }
   if ( func->min_param != func->max_param ) {
      --temps_index;
      add_default_params( phase, func, temps_index, false );
   }

   // Body:
   // -----------------------------------------------------------------------
   int body_pos = c_tell( phase );
   c_write_block( phase, impl->body, true );
   if ( func->return_type ) {
      c_add_opc( phase, PCD_PUSHNUMBER );
      c_add_arg( phase, 0 );
   }

   // Epilogue:
   // -----------------------------------------------------------------------
   int epilogue_pos = c_tell( phase );

   // Temporarily save the return-value.
   int temps_return = 0;
   if ( impl->size > 2 && func->return_type ) {
      // Use the last temporary variable.
      temps_return = writing->temps_start;
      temps_size += ( ! temps_size ? 1 : 0 );
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, temps_return );
   }

   // Restore previous values of script variables.
   i = 0;
   while ( i < impl->size ) {
      if ( impl->size <= 2 && func->return_type ) {
         c_add_opc( phase, PCD_SWAP );
      }
      c_add_opc( phase, PCD_ASSIGNSCRIPTVAR );
      c_add_arg( phase, impl->index_offset + impl->size - i - 1 );
      ++i;
   }

   // Return-value.
   if ( func->return_type ) {
      if ( impl->size > 2 ) {
         c_add_opc( phase, PCD_PUSHSCRIPTVAR );
         c_add_arg( phase, temps_return );
      }
      c_add_opc( phase, PCD_SWAP );
   }

   // Output return table.
   impl->return_pos = c_tell( phase );
   c_add_opc( phase, PCD_CASEGOTOSORTED );
   c_add_arg( phase, num_entries );
   call = impl->nested_calls;
   while ( call ) {
      c_add_arg( phase, 0 );
      c_add_arg( phase, 0 );
      call = call->nested_call->next;
   }

   // Patch address of return-statements.
   struct return_stmt* stmt = impl->returns;
   while ( stmt ) {
      c_seek( phase, stmt->obj_pos );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, epilogue_pos );
      stmt = stmt->next;
   }
   c_seek_end( phase );

   if ( temps_size > writing->temps_size ) {
      writing->temps_size = temps_size;
   }
}

void patch_nestedfunc_addresses( struct codegen* phase, struct func* func ) {
   struct func_user* impl = func->impl; 
   // Correct calls to this function.
   int num_entries = 0;
   struct call* call = impl->nested_calls;
   while ( call ) {
      c_seek( phase, call->nested_call->enter_pos );
      c_add_opc( phase, PCD_GOTO );
      c_add_arg( phase, impl->obj_pos );
      call = call->nested_call->next;
      ++num_entries;
   }
   // Correct return-addresses in return-table.
   c_seek( phase, impl->return_pos );
   c_add_opc( phase, PCD_CASEGOTOSORTED );
   c_add_arg( phase, num_entries );
   call = impl->nested_calls;
   while ( call ) {
      c_add_arg( phase, call->nested_call->id );
      c_add_arg( phase, call->nested_call->leave_pos );
      call = call->nested_call->next;
   }
}