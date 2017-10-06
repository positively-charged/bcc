#include "phase.h"
#include "pcode.h"

#include <stdio.h>
#include <string.h>

struct nestedfunc_writing {
   struct func* nested_funcs;
   struct call* nested_calls;
   int temps_start;
   int temps_size;
   int args_size;
   int local_size;
};

static void write_null_handler( struct codegen* codegen );
static void write_script( struct codegen* codegen, struct script* script );
static void write_func( struct codegen* codegen, struct func* func );
static void init_func_record( struct func_record* record, struct func* func );
static void alloc_param_indexes( struct func_record* func,
   struct param* param );
static void alloc_funcscopevars_indexes( struct codegen* codegen,
   struct func_record* func, struct list* vars );
static void visit_local_var( struct codegen* codegen, struct var* var );
static void visit_world_var( struct codegen* codegen, struct var* var );
static void write_multi_initz( struct codegen* codegen, struct var* var );
static bool is_zero_local_value( struct codegen* codegen,
   struct value* value );
static bool is_nonzero_local_value( struct codegen* codegen,
   struct value* value );
static int get_local_value_size( struct codegen* codegen,
   struct value* value );
static void write_local_value( struct codegen* codegen, struct var* var,
   struct value* value, int zeroed_segment_start, int zeroed_segment_end );
static void write_string_initz( struct codegen* codegen, struct var* var,
   struct value* value, bool include_nul );
static void nullify_array( struct codegen* codegen, int storage, int index,
   int start, int size );
static void write_multi_initz_acs( struct codegen* codegen, struct var* var );
static void assign_nested_func_indexes( struct codegen* codegen,
   struct func* nested_funcs );
static void assign_nested_call_ids( struct codegen* codegen,
   struct func* nested_funcs );
static void init_nestedfunc_writing( struct nestedfunc_writing* writing,
   struct func* nested_funcs, struct call* nested_calls, int temps_start );
static void write_nested_funcs( struct codegen* codegen,
   struct nestedfunc_writing* writing );
static void write_one_nestedfunc( struct codegen* codegen,
   struct nestedfunc_writing* writing, struct func* func );
static void patch_nestedfunc_addresses( struct codegen* codegen,
   struct func* func );

void c_write_user_code( struct codegen* codegen ) {
   if ( codegen->null_handler ) {
      write_null_handler( codegen );
   }
   // Scripts.
   struct list_iter i;
   list_iterate( &codegen->task->library_main->scripts, &i );
   while ( ! list_end( &i ) ) {
      write_script( codegen, list_data( &i ) );
      list_next( &i );
   }
   // Functions.
   list_iterate( &codegen->task->library_main->funcs, &i );
   while ( ! list_end( &i ) ) {
      write_func( codegen, list_data( &i ) );
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
   // Dummy script, for compatibility with older WAD editors. acc generates a
   // separate body per script. Since the bodies are all the same, generate a
   // single body, then have each script entry refer to the same body. (Not
   // sure if this will work though, but I don't see why it wouldn't.)
   if ( codegen->task->library_main->wadauthor ) {
      codegen->dummy_script_offset = c_tell( codegen );
      c_add_opc( codegen, PCD_TERMINATE );
      if ( codegen->compress ) {
         c_add_opc( codegen, PCD_NONE );
         c_add_opc( codegen, PCD_NONE );
         c_add_opc( codegen, PCD_NONE );
      }
   }
}

static void write_null_handler( struct codegen* codegen ) {
   #define NULLDEREF_MSG "\\cgerror: script attempting to use null reference"
   struct indexed_string* string = t_intern_string( codegen->task,
      NULLDEREF_MSG, strlen( NULLDEREF_MSG ) );
   string->used = true;
   c_append_string( codegen, string );
   struct func_user* impl = codegen->null_handler->impl;
   impl->obj_pos = c_tell( codegen );
   c_add_opc( codegen, PCD_BEGINPRINT );
   c_add_opc( codegen, PCD_PUSHNUMBER );
   c_add_arg( codegen, string->index_runtime );
   c_add_opc( codegen, PCD_PRINTSTRING );
   c_add_opc( codegen, PCD_ENDLOG );
   c_add_opc( codegen, PCD_TERMINATE );
}

void c_write_user_code_acs( struct codegen* codegen ) {
   // Scripts.
   struct list_iter i;
   list_iterate( &codegen->task->library_main->scripts, &i );
   while ( ! list_end( &i ) ) {
      write_script( codegen, list_data( &i ) );
      list_next( &i );
   }
}

static void write_script( struct codegen* codegen, struct script* script ) {
   struct func_record record;
   init_func_record( &record, NULL );
   codegen->func = &record;
   script->offset = c_tell( codegen );
   if ( script->nested_funcs ) {
      assign_nested_func_indexes( codegen, script->nested_funcs );
      assign_nested_call_ids( codegen, script->nested_funcs );
   }
   alloc_param_indexes( &record, script->params );
   alloc_funcscopevars_indexes( codegen, &record, &script->funcscope_vars );
   c_write_block( codegen, script->body );
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

static void write_func( struct codegen* codegen, struct func* func ) {
   struct func_record record;
   init_func_record( &record, func );
   codegen->func = &record;
   struct func_user* impl = func->impl;
   impl->obj_pos = c_tell( codegen );
   if ( impl->nested_funcs ) {
      assign_nested_func_indexes( codegen, impl->nested_funcs );
      assign_nested_call_ids( codegen, impl->nested_funcs );
   }
   alloc_param_indexes( &record, func->params );
   alloc_funcscopevars_indexes( codegen, &record, &impl->funcscope_vars );
   c_write_block( codegen, impl->body );
   if ( func->return_spec == SPEC_VOID && ! func->ref ) {
      c_pcd( codegen, PCD_RETURNVOID );
   }
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

static void init_func_record( struct func_record* record, struct func* func ) {
   record->func = func;
   record->start_index = 0;
   record->array_index = 0;
   record->size = 0;
   record->nested_func = false;
}

static void alloc_param_indexes( struct func_record* func,
   struct param* param ) {
   while ( param ) {
      param->index = func->start_index;
      func->start_index += param->size;
      func->size += param->size;
      param = param->next;
   }
}

static void alloc_funcscopevars_indexes( struct codegen* codegen,
   struct func_record* func, struct list* vars ) {
   struct list_iter i;
   list_iterate( vars, &i );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      if ( var->storage == STORAGE_LOCAL ) {
         switch ( var->desc ) {
         case DESC_ARRAY:
         case DESC_STRUCTVAR:
            var->index = func->array_index;
            ++func->array_index;
            break;
         case DESC_REFVAR:
         case DESC_PRIMITIVEVAR:
            var->index = func->start_index;
            func->start_index += var->size;
            func->size += var->size;
            break;
         default:
            C_UNREACHABLE( codegen );
         }
      }
      list_next( &i );
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

static void visit_local_var( struct codegen* codegen, struct var* var ) {
   switch ( var->desc ) {
   case DESC_ARRAY:
   case DESC_STRUCTVAR:
      if ( var->force_local_scope ) {
         var->index = codegen->func->array_index;
         ++codegen->func->array_index;
      }
      if ( var->initial ) {
         if ( codegen->lang == LANG_ACS ) {
            write_multi_initz_acs( codegen, var );
         }
         else {
            write_multi_initz( codegen, var );
         }
      }
      break;
   case DESC_REFVAR:
   case DESC_PRIMITIVEVAR:
      if ( var->force_local_scope ) {
         var->index = c_alloc_script_var( codegen );
         if ( var->ref && var->ref->type == REF_ARRAY ) {
            c_alloc_script_var( codegen );
         }
      }
      if ( var->value ) {
         c_init_local_var( codegen, var );
      }
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void visit_world_var( struct codegen* codegen, struct var* var ) {
   if ( var->initial ) {
      if ( var->initial->multi ) {
         write_multi_initz( codegen, var );
      }
      else if ( var->value->type == VALUE_STRINGINITZ ) {
         write_string_initz( codegen, var, var->value, true );
      }
      else {
         c_push_expr( codegen, var->value->expr );
         c_update_indexed( codegen, var->storage, var->index, AOP_NONE );
      }
   }
}

static void write_multi_initz( struct codegen* codegen, struct var* var ) {
   // Determine segment of the array to nullify.
   // -----------------------------------------------------------------------
   // The segment begins at the first element with a value of zero, or at the
   // first uninitialized element. The segment ends at the element after the
   // last element with a value of zero, or at the element after the last
   // uninitialized element. The end-point element is not nullified.
   int zeroed_segment_start = 0;
   int zeroed_segment_end = 0;
   struct value* value = var->value;
   while ( value ) {
      int value_end = value->index;
      if ( is_nonzero_local_value( codegen, value ) ) {
         value_end += get_local_value_size( codegen, value );
      }
      int next_value_start = 0;
      if ( value->next ) {
         next_value_start = value->next->index;
      }
      else {
         next_value_start = var->size;
      }
      if ( value_end < next_value_start ) {
         if ( zeroed_segment_end == 0 ) {
            zeroed_segment_start = value_end;
         }
         zeroed_segment_end = next_value_start;
      }
      value = value->next;
   }
   if ( zeroed_segment_start < zeroed_segment_end ) {
      nullify_array( codegen, var->storage, var->index, zeroed_segment_start,
         zeroed_segment_end - zeroed_segment_start );
   }
   // Assign values to elements.
   // -----------------------------------------------------------------------
   value = var->value;
   while ( value ) {
      if ( is_nonzero_local_value( codegen, value ) ) {
         write_local_value( codegen, var, value, zeroed_segment_start,
            zeroed_segment_end );
      }
      value = value->next;
   }
}

static bool is_zero_local_value( struct codegen* codegen,
   struct value* value ) {
   switch ( value->type ) {
   case VALUE_EXPR:
      return ( value->expr->folded && value->expr->value == 0 );
   case VALUE_STRING:
      return ( value->more.string.string->length == 0 &&
         // In a library, a string literal will be tagged, so assume that the
         // resulting string index will not be 0.
         ! codegen->task->library_main->importable );
   case VALUE_STRINGINITZ:
      return ( value->more.stringinitz.string->length == 0 );
   case VALUE_ARRAYREF:
   case VALUE_STRUCTREF:
      return false;
   case VALUE_FUNCREF:
      if ( value->more.funcref.func ) {
         struct func_user* impl = value->more.funcref.func->impl;
         return ( impl->index == 0 &&
            // In a library, a function reference will be tagged, so assume
            // that the resulting function index will not be 0.
            ! codegen->task->library_main->importable );
      }
      break;
   default:
      C_UNREACHABLE( codegen );
   }
   return false;
}

static bool is_nonzero_local_value( struct codegen* codegen,
   struct value* value ) {
   return ( ! is_zero_local_value( codegen, value ) );
}

static int get_local_value_size( struct codegen* codegen,
   struct value* value ) {
   switch ( value->type ) {
   case VALUE_EXPR:
   case VALUE_STRING:
   case VALUE_STRUCTREF:
   case VALUE_FUNCREF:
      return 1;
   case VALUE_STRINGINITZ:
      // NOTE: Plus one for the NUL character.
      return value->more.stringinitz.string->length + 1;
   case VALUE_ARRAYREF:
      // Offset to the array and offset to the dimension information.
      return 2;
   default:
      C_UNREACHABLE( codegen );
      return 0;
   }
}

static void write_local_value( struct codegen* codegen, struct var* var,
   struct value* value, int zeroed_segment_start, int zeroed_segment_end ) {
   switch ( value->type ) {
   case VALUE_EXPR:
   case VALUE_STRING:
   case VALUE_STRUCTREF:
   case VALUE_FUNCREF:
      c_pcd( codegen, PCD_PUSHNUMBER, value->index );
      c_push_expr( codegen, value->expr );
      c_update_element( codegen, var->storage, var->index, AOP_NONE );
      break;
   case VALUE_STRINGINITZ:
      {
         // Optimization: Do not include the NUL character if the element that
         // will contain the NUL character has been zeroed already.
         int nul_index = value->index + value->more.stringinitz.string->length;
         bool include_nul = ( ! ( nul_index >= zeroed_segment_start &&
            nul_index < zeroed_segment_end ) );
         write_string_initz( codegen, var, value, include_nul );
      }
      break;
   case VALUE_ARRAYREF:
      c_pcd( codegen, PCD_PUSHNUMBER, value->index );
      c_push_expr( codegen, value->expr );
      c_update_element( codegen, var->storage, var->index, AOP_NONE );
      c_pcd( codegen, PCD_PUSHNUMBER, value->index + 1 );
      c_push_dimtrack( codegen );
      c_update_element( codegen, var->storage, var->index, AOP_NONE );
      break;
   default:
      C_UNREACHABLE( codegen );
   }
}

static void write_string_initz( struct codegen* codegen, struct var* var,
   struct value* value, bool nul_element ) {
   struct indexed_string_usage* usage =
      ( struct indexed_string_usage* ) value->expr->root;
   // Optimization: If the string is small enough, initialize each element
   // with a separate set of instructions.
   enum { UNROLL_LIMIT = 3 };
   if ( usage->string->length <= UNROLL_LIMIT ) {
      int length = usage->string->length;
      if ( nul_element ) {
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
      if ( nul_element ) {
         c_pcd( codegen, PCD_PUSHNUMBER,
            value->index + usage->string->length );
      }
      c_pcd( codegen, PCD_PUSHNUMBER, value->index );
      c_pcd( codegen, PCD_PUSHNUMBER, var->index );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_pcd( codegen, PCD_PUSHNUMBER, usage->string->length );
      c_push_string( codegen, usage->string );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      int opcode = PCD_STRCPYTOSCRIPTCHRANGE;
      switch ( var->storage ) {
      case STORAGE_GLOBAL:
         opcode = PCD_STRCPYTOGLOBALCHRANGE;
         break;
      case STORAGE_WORLD:
         opcode = PCD_STRCPYTOWORLDCHRANGE;
         break;
      case STORAGE_LOCAL:
         break;
      default:
         C_UNREACHABLE( codegen );
      }
      c_pcd( codegen, opcode );
      if ( nul_element ) {
         // Instead of dropping the result of StrCpy() and then pushing a zero,
         // convert the result of StrCpy() to zero and use that. The result of
         // StrCpy() is 1 for a successful copy, so just negate it.
         c_pcd( codegen, PCD_NEGATELOGICAL );
         c_update_element( codegen, var->storage, var->index, AOP_NONE );
      }
      else {
         c_pcd( codegen, PCD_DROP );
      }
      usage->string->used = true;
   }
}

static void nullify_array( struct codegen* codegen, int storage, int index,
   int start, int size ) {
   // Optimization: If the size of the sub-array is small enough, initialize
   // each element with a separate set of instructions.
   enum { UNROLL_LIMIT = 4 };
   if ( size <= UNROLL_LIMIT ) {
      int i = 0;
      while ( i < size ) {
         c_pcd( codegen, PCD_PUSHNUMBER, start + i );
         c_pcd( codegen, PCD_PUSHNUMBER, 0 );
         ++i;
      }
      i = 0;
      while ( i < size ) {
         c_update_element( codegen, storage, index, AOP_NONE );
         ++i;
      }
   }
   else {
      c_pcd( codegen, PCD_PUSHNUMBER, start );
      struct c_point* loop_point = c_create_point( codegen );
      c_append_node( codegen, &loop_point->node );
      c_pcd( codegen, PCD_DUP );
      c_pcd( codegen, PCD_PUSHNUMBER, 0 );
      c_update_element( codegen, storage, index, AOP_NONE );
      struct c_casejump* exit_jump = c_create_casejump( codegen, 0, NULL );
      c_append_node( codegen, &exit_jump->node );
      c_pcd( codegen, PCD_PUSHNUMBER, 1 );
      c_pcd( codegen, PCD_ADD );
      struct c_jump* loop_jump = c_create_jump( codegen, PCD_GOTO );
      c_append_node( codegen, &loop_jump->node );
      loop_jump->point = loop_point;
      struct c_point* exit_point = c_create_point( codegen );
      c_append_node( codegen, &exit_point->node );
      exit_jump->value = start + size - 1;
      exit_jump->point = exit_point;
   }
}

static void write_multi_initz_acs( struct codegen* codegen, struct var* var ) {
   struct value* value = var->value;
   while ( value ) {
      c_pcd( codegen, PCD_PUSHNUMBER, value->index );
      c_pcd( codegen, PCD_PUSHNUMBER, value->expr->value );
      c_update_element( codegen, var->storage, var->index, AOP_NONE );
      value = value->next;
   }
}

static void assign_nested_func_indexes( struct codegen* codegen,
   struct func* nested_funcs ) {
   struct func* func = nested_funcs;
   // Start index at 1 because 0 is reserved for the null value.
   int index = 1;
   while ( func ) {
      struct func_user* impl = func->impl;
      impl->index = index;
      ++index;
      func = impl->next_nested;
   }
}

static void assign_nested_call_ids( struct codegen* codegen,
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

static void init_nestedfunc_writing( struct nestedfunc_writing* writing,
   struct func* nested_funcs, struct call* nested_calls, int temps_start ) {
   writing->nested_funcs = nested_funcs;
   writing->nested_calls = nested_calls;
   writing->temps_start = temps_start;
   writing->temps_size = 0;
   writing->args_size = 0;
   writing->local_size = 0;
}

static void write_nested_funcs( struct codegen* codegen,
   struct nestedfunc_writing* writing ) {
   // Determine the size of the argument-holding area.
   struct func* func = writing->nested_funcs;
   while ( func ) {
      struct func_user* impl = func->impl;
      if ( impl->recursive == RECURSIVE_POSSIBLY ) {
         int args_size = c_total_param_size( func );
         if ( args_size > writing->args_size ) {
            writing->args_size = args_size;
         }
         // The argument-holding area should have at least one variable, to
         // temporarily hold the return-value.
         if ( func->return_spec != SPEC_VOID ) {
            if ( ! writing->args_size ) {
               writing->args_size = 1;
            }
         }
      }
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

static void write_one_nestedfunc( struct codegen* codegen,
   struct nestedfunc_writing* writing, struct func* func ) {
   struct func_user* impl = func->impl;
   // Prologue (part #1):
   // -----------------------------------------------------------------------
   struct c_point* prologue_point = c_create_point( codegen );
   c_append_node( codegen, &prologue_point->node );
   impl->prologue_point = prologue_point;
   int start_index = writing->temps_start + writing->args_size +
      writing->local_size;
   struct func_record record;
   init_func_record( &record, func );
   record.start_index = start_index;
   record.nested_func = true;
   alloc_param_indexes( &record, func->params );
   alloc_funcscopevars_indexes( codegen, &record, &impl->funcscope_vars );
   codegen->func = &record;
   // Body:
   // -----------------------------------------------------------------------
   c_write_block( codegen, impl->body );
   impl->size = record.size;
   // Prologue (part #2):
   // -----------------------------------------------------------------------
   c_seek_node( codegen, &prologue_point->node );
   int total_param_size = c_total_param_size( func );
   if ( impl->recursive == RECURSIVE_POSSIBLY ) {
      // Assign arguments to temporary space.
      int i = 0;
      while ( i < total_param_size ) {
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, writing->temps_start + i );
         ++i;
      }
      // Push local variables of the function onto the stack. They will be
      // restored at epilogue.
      i = 0;
      while ( i < impl->size ) {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, start_index + i );
         ++i;
      }
   }
   // Assign arguments to parameters.
   int param_index = start_index + total_param_size - 1;
   int i = 0;
   while ( i < total_param_size ) {
      if ( impl->recursive == RECURSIVE_POSSIBLY ) {
         c_pcd( codegen, PCD_PUSHSCRIPTVAR, writing->temps_start + i );
      }
      c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, param_index );
      --param_index;
      ++i;
   }
   c_seek_node( codegen, codegen->node_tail );
   // Epilogue:
   // -----------------------------------------------------------------------
   struct c_point* epilogue_point = c_create_point( codegen );
   c_append_node( codegen, &epilogue_point->node );
   // Temporarily save the return-value so we can restore the variables.
   int return_var = writing->temps_start;
   if ( func->return_spec != SPEC_VOID ) {
      if ( impl->recursive == RECURSIVE_POSSIBLY ) {
         if ( impl->size > 0 ) {
            c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, return_var );
         }
      }
   }
   // Restore previous values of variables.
   if ( impl->recursive == RECURSIVE_POSSIBLY ) {
      int index = start_index + impl->size - 1;
      int i = 0;
      while ( i < impl->size ) {
         c_pcd( codegen, PCD_ASSIGNSCRIPTVAR, index );
         --index;
         ++i;
      }
   }
   // Push return-value onto the stack.
   if ( func->return_spec != SPEC_VOID || func->ref ) {
      if ( impl->recursive == RECURSIVE_POSSIBLY ) {
         if ( impl->size > 0 ) {
            c_pcd( codegen, PCD_PUSHSCRIPTVAR, return_var );
         }
      }
      c_pcd( codegen, PCD_SWAP );
   }
   // Add return-table.
   struct c_sortedcasejump* return_table = c_create_sortedcasejump( codegen );
   c_append_node( codegen, &return_table->node );
   impl->return_table = return_table;
   // Patch address of return-statements.
   struct return_stmt* stmt = impl->returns;
   while ( stmt ) {
      stmt->epilogue_jump->point = epilogue_point;
      stmt = stmt->next;
   }
   writing->local_size += impl->size;
   codegen->func = NULL;
}

static void patch_nestedfunc_addresses( struct codegen* codegen,
   struct func* func ) {
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
