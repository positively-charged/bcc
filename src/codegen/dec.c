#include "phase.h"
#include "pcode.h"

#include <stdio.h>

struct nestedfunc_writing {
   struct func* nested_funcs;
   struct call* nested_calls;
   int temps_start;
   int temps_size;
   int args_size;
   int local_size;
};

static void write_script( struct codegen* codegen, struct script* script );
static void write_userfunc( struct codegen* codegen, struct func* func );
static void init_func_record( struct func_record* record );
static void alloc_param_indexes( struct func_record* func,
   struct param* param );
static void visit_local_var( struct codegen* codegen, struct var* var );
static void visit_world_var( struct codegen* codegen, struct var* var );
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
static void assign_nested_call_ids( struct codegen* codegen,
   struct func* nested_funcs );
void init_nestedfunc_writing( struct nestedfunc_writing* writing,
   struct func* nested_funcs, struct call* nested_calls, int temps_start );
static void write_nested_funcs( struct codegen* codegen,
   struct nestedfunc_writing* writing );
static void calc_args_space_size( struct codegen* codegen,
   struct nestedfunc_writing* writing, struct func* func );
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
   struct func_record record;
   init_func_record( &record );
   codegen->func = &record;
   script->offset = c_tell( codegen );
   if ( script->nested_funcs ) {
      assign_nested_call_ids( codegen, script->nested_funcs );
   }
   alloc_param_indexes( &record, script->params );
   c_write_stmt( codegen, script->body );
   c_pcd( codegen, PCD_TERMINATE );
   script->size = record.size;
   codegen->func = NULL;
   if ( script->nested_funcs ) {
      struct nestedfunc_writing writing;
      init_nestedfunc_writing( &writing, script->nested_funcs,
         script->nested_calls, script->size );
      write_nested_funcs( codegen, &writing );
      script->size += writing.temps_size;
   }
   c_flush_pcode( codegen );
}

void write_userfunc( struct codegen* codegen, struct func* func ) {
   struct func_record record;
   init_func_record( &record );
   codegen->func = &record;
   struct func_user* impl = func->impl;
   impl->obj_pos = c_tell( codegen );
   if ( impl->nested_funcs ) {
      assign_nested_call_ids( codegen, impl->nested_funcs );
   }
   alloc_param_indexes( &record, func->params );
   add_default_params( codegen, func, func->max_param, true );
   c_write_block( codegen, impl->body );
   c_pcd( codegen, PCD_RETURNVOID );
   impl->size = record.size;
   codegen->func = NULL;
   if ( impl->nested_funcs ) {
      struct nestedfunc_writing writing;
      init_nestedfunc_writing( &writing, impl->nested_funcs,
         impl->nested_calls, impl->size );
      write_nested_funcs( codegen, &writing );
      impl->size += writing.temps_size;
   }
   c_flush_pcode( codegen );
}

void init_func_record( struct func_record* record ) {
   record->start_index = RESERVEDSCRIPTVAR_TOTAL;
   record->array_index = 0;
   record->size = 0;
   record->nested_func = false;
}

void alloc_param_indexes( struct func_record* func, struct param* param ) {
   while ( param ) {
      param->index = func->start_index;
      ++func->start_index;
      ++func->size;
      param = param->next;
   }
}

// Increases the space size of local variables by one, returning the index of
// the space slot.
int c_alloc_script_var( struct codegen* codegen ) {
   int index = codegen->local_record->index;
   ++codegen->local_record->index;
   ++codegen->local_record->func_size;
   if ( codegen->local_record->func_size > codegen->func->size ) {
      codegen->func->size = codegen->local_record->func_size;
   }
   return index;
}

// Decreases the space size of local variables by one.
void c_dealloc_last_script_var( struct codegen* codegen ) {
   --codegen->local_record->index;
   --codegen->local_record->func_size;
}

void c_visit_var( struct codegen* codegen, struct var* var ) {
   switch ( var->storage ) {
   case STORAGE_LOCAL:
      visit_local_var( codegen, var );
      break;
   case STORAGE_WORLD:
   case STORAGE_GLOBAL:
      visit_world_var( codegen, var );
      break;
   default:
      break;
   }
}

void visit_local_var( struct codegen* codegen, struct var* var ) {
   if ( var->dim ) {
      var->index = codegen->func->array_index;
      ++codegen->func->array_index;
   }
   else {
      var->index = c_alloc_script_var( codegen );
      if ( var->ref && var->ref->type == REF_ARRAY ) {
         c_alloc_script_var( codegen );
      }
   }
   if ( var->value ) {
      c_push_expr( codegen, var->value->expr );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, var->index );
   }
}

void visit_world_var( struct codegen* codegen, struct var* var ) {
   if ( var->initial ) {
      if ( var->initial->multi ) {
         write_world_multi_initz( codegen, var );
      }
      else {
         write_world_initz( codegen, var );
      }
   }
}

void write_world_initz( struct codegen* codegen, struct var* var ) {
   if ( var->value->string_initz ) {
      write_stringinitz( codegen, var, var->value, true );
   }
   else {
      c_push_expr( codegen, var->value->expr );
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
            c_push_expr( codegen, value->expr );
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
         c_push_expr( codegen, param->default_value );
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

void assign_nested_call_ids( struct codegen* codegen,
   struct func* nested_funcs ) {
   struct func* nested_func = nested_funcs;
   while ( nested_func ) {
      struct func_user* nested_impl = nested_func->impl;
      struct call* call = nested_impl->nested_calls;
      int id = 0;
      while ( call ) {
         call->nested_call->id = id;
         ++id;
         call = call->nested_call->next;
      }
      nested_func = nested_impl->next_nested;
   }
}

void init_nestedfunc_writing( struct nestedfunc_writing* writing,
   struct func* nested_funcs, struct call* nested_calls, int temps_start ) {
   writing->nested_funcs = nested_funcs;
   writing->nested_calls = nested_calls;
   writing->temps_start = temps_start;
   writing->temps_size = 0;
   writing->args_size = 0;
   writing->local_size = 0;
}

void write_nested_funcs( struct codegen* codegen,
   struct nestedfunc_writing* writing ) {
   // Determine the number of script variables to reserve for the
   // argument-holding zone.
   struct func* func = writing->nested_funcs;
   while ( func ) {
      calc_args_space_size( codegen, writing, func );
      struct func_user* impl = func->impl;
      func = impl->next_nested;
   }
   // Write the nested functions.
   func = writing->nested_funcs;
   while ( func ) {
      write_one_nestedfunc( codegen, writing, func );
      struct func_user* impl = func->impl;
      func = impl->next_nested;
   }
   writing->temps_size = writing->args_size + writing->local_size;
   // Insert address of function into each call.
   func = writing->nested_funcs;
   while ( func ) {
      patch_nestedfunc_addresses( codegen, func );
      struct func_user* impl = func->impl;
      func = impl->next_nested;
   }
}

void calc_args_space_size( struct codegen* codegen,
   struct nestedfunc_writing* writing, struct func* func ) {
   struct func_record record;
   init_func_record( &record );
   record.start_index = writing->temps_start;
   alloc_param_indexes( &record, func->params );
   int args_size = record.size +
      ( func->min_param != func->max_param ? 1 : 0 );
   if ( args_size > writing->args_size ) {
      writing->args_size = args_size;
   }
}

void write_one_nestedfunc( struct codegen* codegen,
   struct nestedfunc_writing* writing, struct func* func ) {
   struct func_user* impl = func->impl;

   // Prologue (begin):
   // -----------------------------------------------------------------------
   struct c_point* prologue_point = c_create_point( codegen );
   c_append_node( codegen, &prologue_point->node );
   impl->prologue_point = prologue_point;

   int start_index = writing->temps_start + writing->args_size +
      impl->index_offset;
   struct func_record record;
   init_func_record( &record );
   record.start_index = start_index;
   record.nested_func = true;
   codegen->func = &record;
   alloc_param_indexes( &record, func->params );
   // At this point, we need to save the script variables used as the workspace
   // of the function. To do this, we need to know the size of the function.
   // Write the function body to determine the size. Then return to complete
   // the task of saving the workspace.

   // Body:
   // -----------------------------------------------------------------------
   c_write_block( codegen, impl->body );
   if ( func->return_spec != SPEC_VOID ) {
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
   }
   impl->size = record.size;

   // Prologue (finish):
   // -----------------------------------------------------------------------
   c_seek_node( codegen, &prologue_point->node );

   // Assign arguments to temporary space.
   int index = writing->temps_start;
   if ( func->min_param != func->max_param ) {
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, index );
      ++index;
   }
   struct param* param = func->params;
   while ( param ) {
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, index );
      ++index;
      param = param->next;
   }

   // The function uses variables of the script or function as its workspace.
   // Save the values of these variables onto the stack. They will be restored
   // at epilogue.
   int i = 0;
   while ( i < impl->size ) {
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, impl->index_offset + i );
      ++i;
   }

   // Assign arguments to parameters.
   param = func->params;
   while ( param ) {
      --index;
      c_pcd( codegen, PCD_PUSHSCRIPTVAR, index );
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, param->index );
      param = param->next;
   }
   if ( func->min_param != func->max_param ) {
      --index;
      add_default_params( codegen, func, index, false );
   }
   c_seek_node( codegen, codegen->node_tail );

   // Epilogue:
   // -----------------------------------------------------------------------
   struct c_point* epilogue_point = c_create_point( codegen );
   c_append_node( codegen, &epilogue_point->node );

   // Temporarily save the return value.
   int return_var = start_index;
   if ( func->return_spec != SPEC_VOID && impl->size > 0 ) {
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, return_var );
   }

   // Restore previous values of script variables.
   index = start_index + impl->size - 1;
   i = ( func->return_spec != SPEC_VOID && impl->size > 0 ) ? 1 : 0;
   while ( i < impl->size ) {
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, index );
      --index;
      ++i;
   }

   // Push return value into stack.
   if ( func->return_spec != SPEC_VOID ) {
      if ( impl->size > 0 ) {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, return_var );
         c_pcd( codegen, PCD_SWAP );
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, return_var );
      }
      c_pcd( codegen, PCD_SWAP );
   }

   // Return-table.
   struct c_sortedcasejump* return_table = c_create_sortedcasejump( codegen );
   c_append_node( codegen, &return_table->node );
   impl->return_table = return_table;
 
   // Patch address of return-statements.
   struct return_stmt* stmt = impl->returns;
   while ( stmt ) {
      stmt->epilogue_jump->point = epilogue_point;
      stmt = stmt->next;
   }

   // Keep track of the maximum number of script variables required to execute
   // the bodies of all the nested functions.
   int local_size = impl->index_offset + impl->size;
   if ( local_size > writing->local_size ) {
      writing->local_size = local_size;
   }

   codegen->func = NULL;
}

void patch_nestedfunc_addresses( struct codegen* codegen, struct func* func ) {
   struct func_user* impl = func->impl;
   // Correct calls to function.
   struct call* call = impl->nested_calls;
   while ( call ) {
      call->nested_call->prologue_jump->point = impl->prologue_point;
      call = call->nested_call->next;
   }
   // Populate return-table.
   call = impl->nested_calls;
   while ( call ) {
      struct c_casejump* entry = c_create_casejump( codegen,
         call->nested_call->id, call->nested_call->return_point );
      c_append_casejump( impl->return_table, entry );
      call = call->nested_call->next;
   }
}

void c_visit_nested_func( struct codegen* codegen, struct func* func ) {
   if ( func->type == FUNC_USER ) {
      if ( codegen->func->nested_func ) {
         struct func_user* impl = func->impl;
         impl->index_offset = codegen->local_record->index -
            codegen->func->start_index;
      }
   }
}
