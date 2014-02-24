#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "task.h"

static void add_buffer( struct task* );
static void write_opc( struct task*, int );
static void write_arg( struct task*, int );
static void write_args( struct task* );
static void add_immediate( struct task*, int );
static void remove_immediate( struct task* );
static void push_immediate( struct task*, int );
static bool is_byte_value( int );

void t_init_fields_obj( struct task* task ) {
   task->buffer_head = NULL;
   task->buffer = NULL;
   add_buffer( task );
   task->opc = PC_NONE;
   task->opc_args = 0;
   task->immediate = NULL;
   task->immediate_tail = NULL;
   task->free_immediate = NULL;
   task->immediate_count = 0;
   task->push_immediate = false;
}

void add_buffer( struct task* task ) {
   if ( ! task->buffer || ! task->buffer->next ) {
      struct buffer* buffer = mem_alloc( sizeof( *buffer ) );
      buffer->next = NULL;
      buffer->used = 0;
      buffer->pos = 0;
      if ( task->buffer ) {
         task->buffer->next = buffer;
         task->buffer = buffer;
      }
      else {
         task->buffer_head = buffer;
         task->buffer = buffer;
      }
   }
   else {
      task->buffer = task->buffer->next;
   }
}

void t_add_sized( struct task* task, const void* data, int length ) {
   if ( BUFFER_SIZE - task->buffer->pos < length ) {
      add_buffer( task );
   }
   memcpy( task->buffer->data + task->buffer->pos, data, length );
   task->buffer->pos += length;
   if ( task->buffer->pos > task->buffer->used ) {
      task->buffer->used = task->buffer->pos;
   }
}

void t_add_byte( struct task* task, char value ) {
   t_add_sized( task, &value, sizeof( value ) );
}

void t_add_short( struct task* task, short value ) {
   t_add_sized( task, &value, sizeof( value ) );
}

void t_add_int( struct task* task, int value ) {
   t_add_sized( task, &value, sizeof( value ) );
}

void t_add_int_zero( struct task* task, int amount ) {
   while ( amount ) {
      t_add_int( task, 0 );
      --amount;
   }
}

void t_add_str( struct task* task, const char* value ) {
   t_add_sized( task, value, strlen( value ) );
}

void t_add_opc( struct task* task, int code ) {
   // Number-stacking instructions:
   // -----------------------------------------------------------------------
   {
      switch ( code ) {
      case PC_PUSH_NUMBER:
      case PC_PUSH_BYTE:
      case PC_PUSH_2BYTES:
      case PC_PUSH_3BYTES:
      case PC_PUSH_4BYTES:
      case PC_PUSH_5BYTES:
      case PC_PUSH_BYTES:
         task->push_immediate = true;
         goto finish;
      default:
         task->push_immediate = false;
         goto fold;
      }
   }
   // Constant folding:
   // -----------------------------------------------------------------------
   fold: {
      switch ( code ) {
      case PC_ADD:
      case PC_MUL:
         if ( task->immediate_count >= 2 ) {
            break;
         }
      default:
         goto direct;
      }
      struct immediate* second_last = task->immediate;
      struct immediate* last = second_last->next;
      while ( last->next ) {
         second_last = last;
         last = last->next;
      }
      int l = second_last->value;
      int r = last->value;
      last->next = task->free_immediate;
      task->free_immediate = last;
      --task->immediate_count;
      last = second_last;
      last->next = NULL;
      task->immediate_tail = last;
      switch ( code ) {
      case PC_ADD: last->value = l + r; break;
      case PC_MUL: last->value = l * r; break;
      default: break;
      }
      goto finish;
   }
   // Direct instructions:
   // -----------------------------------------------------------------------
   direct: {
      if ( ! task->immediate ) {
         goto other;
      }
      static const struct entry {
         int code;
         int count;
         int direct;
      } entries[] = {
         { PC_LSPEC1, 1, PC_LSPEC1_DIRECT },
         { PC_LSPEC2, 2, PC_LSPEC2_DIRECT },
         { PC_LSPEC3, 3, PC_LSPEC3_DIRECT },
         { PC_LSPEC4, 4, PC_LSPEC4_DIRECT },
         { PC_LSPEC5, 5, PC_LSPEC5_DIRECT },
         { PC_DELAY, 1, PC_DELAY_DIRECT },
         { PC_RANDOM, 2, PC_RANDOM_DIRECT },
         { PC_THING_COUNT, 2, PC_THING_COUNT_DIRECT },
         { PC_TAG_WAIT, 1, PC_TAG_WAIT_DIRECT },
         { PC_POLY_WAIT, 1, PC_POLY_WAIT_DIRECT },
         { PC_CHANGE_FLOOR, 2, PC_CHANGE_FLOOR_DIRECT },
         { PC_CHANGE_CEILING, 2, PC_CHANGE_CEILING_DIRECT },
         { PC_SCRIPT_WAIT, 1, PC_SCRIPT_WAIT_DIRECT },
         { PC_CONSOLE_COMMAND, 3, PC_CONSOLE_COMMAND_DIRECT },
         { PC_SET_GRAVITY, 1, PC_SET_GRAVITY_DIRECT },
         { PC_SET_AIR_CONTROL, 1, PC_SET_AIR_CONTROL_DIRECT },
         { PC_GIVE_INVENTORY, 2, PC_GIVE_INVENTORY_DIRECT },
         { PC_TAKE_INVENTORY, 2, PC_TAKE_INVENTORY_DIRECT },
         { PC_CHECK_INVENTORY, 1, PC_CHECK_INVENTORY_DIRECT },
         { PC_SPAWN, 6, PC_SPAWN_DIRECT },
         { PC_SPAWN_SPOT, 4, PC_SPAWN_SPOT_DIRECT },
         { PC_SET_MUSIC, 3, PC_SET_MUSIC_DIRECT },
         { PC_LOCAL_SET_MUSIC, 3, PC_LOCAL_SET_MUSIC },
         { PC_SET_FONT, 1, PC_SET_FONT_DIRECT },
         { PC_NONE } };
      const struct entry* entry = entries;
      while ( entry->code != code ) {
         if ( entry->code == PC_NONE ) {
            goto other;
         }
         ++entry;
      }
      if ( entry->count > task->immediate_count ) {
         goto other;
      }
      else if ( entry->count < task->immediate_count ) {
         push_immediate( task, task->immediate_count - entry->count );
      }
      int count = 0;
      struct immediate* immediate = task->immediate;
      while ( immediate && is_byte_value( immediate->value ) ) {
         immediate = immediate->next;
         ++count;
      }
      int direct = entry->direct;
      if ( task->compress && count == entry->count ) {
         switch ( code ) {
         case PC_LSPEC1: direct = PC_LSPEC1_DIRECT_B; break;
         case PC_LSPEC2: direct = PC_LSPEC2_DIRECT_B; break;
         case PC_LSPEC3: direct = PC_LSPEC3_DIRECT_B; break;
         case PC_LSPEC4: direct = PC_LSPEC4_DIRECT_B; break;
         case PC_LSPEC5: direct = PC_LSPEC5_DIRECT_B; break;
         case PC_DELAY: direct = PC_DELAY_DIRECT_B; break;
         case PC_RANDOM: direct = PC_RANDOM_DIRECT_B; break;
         default: break;
         }
      }
      write_opc( task, direct );
      // Some instructions have other arguments that need to be written
      // before outputting the queued immediates.
      switch ( code ) {
      case PC_LSPEC1:
      case PC_LSPEC2:
      case PC_LSPEC3:
      case PC_LSPEC4:
      case PC_LSPEC5:
         break;
      default:
         write_args( task );
      }
      goto finish;
   }
   // Other instructions:
   // -----------------------------------------------------------------------
   other: {
      if ( task->immediate_count ) {
         push_immediate( task, task->immediate_count );
      }
      write_opc( task, code );
   }
   finish: ;
}

void t_add_arg( struct task* task, int arg ) {
   if ( task->push_immediate ) {
      add_immediate( task, arg );
   }
   else {
      write_arg( task, arg );
      switch ( task->opc ) {
      case PC_LSPEC1_DIRECT:
      case PC_LSPEC2_DIRECT:
      case PC_LSPEC3_DIRECT:
      case PC_LSPEC4_DIRECT:
      case PC_LSPEC5_DIRECT:
      case PC_LSPEC1_DIRECT_B:
      case PC_LSPEC2_DIRECT_B:
      case PC_LSPEC3_DIRECT_B:
      case PC_LSPEC4_DIRECT_B:
      case PC_LSPEC5_DIRECT_B:
         if ( task->opc_args == 1 ) {
            write_args( task );
         }
         break;
      default:
         break;
      }
   }
}

void write_opc( struct task* task, int code ) {
   if ( task->compress ) {
      // I don't know how the compression algorithm works exactly, but at this
      // time, the following works with all of the available opcodes as of this
      // writing. Also, now that the opcode is shrinked, the 4-byte arguments
      // that follow may no longer be 4-byte aligned.
      if ( code >= 240 ) {
         t_add_byte( task, ( char ) 240 );
         t_add_byte( task, ( char ) code - 240 );
      }
      else {
         t_add_byte( task, ( char ) code );
      }
   }
   else {
      t_add_int( task, code );
   }
   task->opc = code;
   task->opc_args = 0;
}

void write_arg( struct task* task, int arg ) {
   switch ( task->opc ) {
   case PC_LSPEC1:
   case PC_LSPEC2:
   case PC_LSPEC3:
   case PC_LSPEC4:
   case PC_LSPEC5:
   case PC_LSPEC5_RESULT:
   case PC_LSPEC1_DIRECT:
   case PC_LSPEC2_DIRECT:
   case PC_LSPEC3_DIRECT:
   case PC_LSPEC4_DIRECT:
   case PC_LSPEC5_DIRECT:
      if ( task->opc_args == 0 && task->compress ) {
         t_add_byte( task, arg );
      }
      else {
         t_add_int( task, arg );
      }
      break;
   case PC_PUSH_BYTE:
   case PC_PUSH_2BYTES:
   case PC_PUSH_3BYTES:
   case PC_PUSH_4BYTES:
   case PC_PUSH_5BYTES:
   case PC_PUSH_BYTES:
   case PC_LSPEC1_DIRECT_B:
   case PC_LSPEC2_DIRECT_B:
   case PC_LSPEC3_DIRECT_B:
   case PC_LSPEC4_DIRECT_B:
   case PC_LSPEC5_DIRECT_B:
   case PC_DELAY_DIRECT_B:
   case PC_RANDOM_DIRECT_B:
      t_add_byte( task, arg );
      break;
   case PC_PUSH_SCRIPT_VAR:
   case PC_PUSH_MAP_VAR:
   case PC_PUSH_MAP_ARRAY:
   case PC_PUSH_WORLD_VAR:
   case PC_PUSH_WORLD_ARRAY:
   case PC_PUSH_GLOBAL_VAR:
   case PC_PUSH_GLOBAL_ARRAY:
   case PC_ASSIGN_SCRIPT_VAR:
   case PC_ASSIGN_MAP_VAR:
   case PC_ASSIGN_WORLD_VAR:
   case PC_ASSIGN_GLOBAL_VAR:
   case PC_ASSIGN_MAP_ARRAY:
   case PC_ASSIGN_WORLD_ARRAY:
   case PC_ASSIGN_GLOBAL_ARRAY:
   case PC_ADD_SCRIPT_VAR:
   case PC_ADD_MAP_VAR:
   case PC_ADD_WORLD_VAR:
   case PC_ADD_GLOBAL_VAR:
   case PC_ADD_MAP_ARRAY:
   case PC_ADD_WORLD_ARRAY:
   case PC_ADD_GLOBAL_ARRAY:
   case PC_SUB_SCRIPT_VAR:
   case PC_SUB_MAP_VAR:
   case PC_SUB_WORLD_VAR:
   case PC_SUB_GLOBAL_VAR:
   case PC_SUB_MAP_ARRAY:
   case PC_SUB_WORLD_ARRAY:
   case PC_SUB_GLOBAL_ARRAY:
   case PC_MUL_SCRIPT_VAR:
   case PC_MUL_MAP_VAR:
   case PC_MUL_WORLD_VAR:
   case PC_MUL_GLOBAL_VAR:
   case PC_MUL_MAP_ARRAY:
   case PC_MUL_WORLD_ARRAY:
   case PC_MUL_GLOBAL_ARRAY:
   case PC_DIV_SCRIPT_VAR:
   case PC_DIV_MAP_VAR:
   case PC_DIV_WORLD_VAR:
   case PC_DIV_GLOBAL_VAR:
   case PC_DIV_MAP_ARRAY:
   case PC_DIV_WORLD_ARRAY:
   case PC_DIV_GLOBAL_ARRAY:
   case PC_MOD_SCRIPT_VAR:
   case PC_MOD_MAP_VAR:
   case PC_MOD_WORLD_VAR:
   case PC_MOD_GLOBAL_VAR:
   case PC_MOD_MAP_ARRAY:
   case PC_MOD_WORLD_ARRAY:
   case PC_MOD_GLOBAL_ARRAY:
   case PC_LS_SCRIPT_VAR:
   case PC_LS_MAP_VAR:
   case PC_LS_WORLD_VAR:
   case PC_LS_GLOBAL_VAR:
   case PC_LS_MAP_ARRAY:
   case PC_LS_WORLD_ARRAY:
   case PC_LS_GLOBAL_ARRAY:
   case PC_RS_SCRIPT_VAR:
   case PC_RS_MAP_VAR:
   case PC_RS_WORLD_VAR:
   case PC_RS_GLOBAL_VAR:
   case PC_RS_MAP_ARRAY:
   case PC_RS_WORLD_ARRAY:
   case PC_RS_GLOBAL_ARRAY:
   case PC_AND_SCRIPT_VAR:
   case PC_AND_MAP_VAR:
   case PC_AND_WORLD_VAR:
   case PC_AND_GLOBAL_VAR:
   case PC_AND_MAP_ARRAY:
   case PC_AND_WORLD_ARRAY:
   case PC_AND_GLOBAL_ARRAY:
   case PC_EOR_SCRIPT_VAR:
   case PC_EOR_MAP_VAR:
   case PC_EOR_WORLD_VAR:
   case PC_EOR_GLOBAL_VAR:
   case PC_EOR_MAP_ARRAY:
   case PC_EOR_WORLD_ARRAY:
   case PC_EOR_GLOBAL_ARRAY:
   case PC_OR_SCRIPT_VAR:
   case PC_OR_MAP_VAR:
   case PC_OR_WORLD_VAR:
   case PC_OR_GLOBAL_VAR:
   case PC_OR_MAP_ARRAY:
   case PC_OR_WORLD_ARRAY:
   case PC_OR_GLOBAL_ARRAY:
   case PC_INC_SCRIPT_VAR:
   case PC_INC_MAP_VAR:
   case PC_INC_WORLD_VAR:
   case PC_INC_GLOBAL_VAR:
   case PC_INC_MAP_ARRAY:
   case PC_INC_WORLD_ARRAY:
   case PC_INC_GLOBAL_ARRAY:
   case PC_DEC_SCRIPT_VAR:
   case PC_DEC_MAP_VAR:
   case PC_DEC_WORLD_VAR:
   case PC_DEC_GLOBAL_VAR:
   case PC_DEC_MAP_ARRAY:
   case PC_DEC_WORLD_ARRAY:
   case PC_DEC_GLOBAL_ARRAY:
      if ( task->compress ) {
         t_add_byte( task, arg );
      }
      else {
         t_add_int( task, arg );
      }
      break;
   case PC_CALL:
   case PC_CALL_DISCARD:
      if ( task->compress ) {
         t_add_byte( task, arg );
      }
      else {
         t_add_int( task, arg );
      }
      break;
   case PC_CALL_FUNC:
      if ( task->compress ) {
         // Argument-count field.
         if ( task->opc_args == 0 ) {
            t_add_byte( task, arg );
         }
         // Function-index field.
         else {
            t_add_short( task, arg );
         }
      }
      else {
         t_add_int( task, arg );
      }
      break;
   case PC_CASE_GOTO_SORTED:
      // The arguments need to be 4-byte aligned.
      if ( task->opc_args == 0 ) {
         int padding = 4 - ( t_tell( task ) % 4 );
         while ( padding ) {
            t_add_byte( task, 0 );
            --padding;
         }
      }
      t_add_int( task, arg );
      break;
   default:
      t_add_int( task, arg );
      break;
   }
   ++task->opc_args;
}

void write_args( struct task* task ) {
   while ( task->immediate_count ) {
      write_arg( task, task->immediate->value );
      remove_immediate( task );
   }
}

void add_immediate( struct task* task, int value ) {
   struct immediate* immediate = NULL;
   if ( task->free_immediate ) {
      immediate = task->free_immediate;
      task->free_immediate = immediate->next;
   }
   else {
      immediate = mem_alloc( sizeof( *immediate ) );
   }
   immediate->value = value;
   immediate->next = NULL;
   if ( task->immediate ) {
      task->immediate_tail->next = immediate;
   }
   else {
      task->immediate = immediate;
   }
   task->immediate_tail = immediate;
   ++task->immediate_count;
}

void remove_immediate( struct task* task ) {
   struct immediate* immediate = task->immediate;
   task->immediate = immediate->next;
   immediate->next = task->free_immediate;
   task->free_immediate = immediate;
   --task->immediate_count;
   if ( ! task->immediate_count ) {
      task->immediate = NULL;
   }
}

// NOTE: It is assumed that the count passed is always equal to, or is less
// than, the number of immediates in the queue.
void push_immediate( struct task* task, int count ) {
   int left = count;
   struct immediate* immediate = task->immediate;
   while ( left ) {
      int i = 0;
      struct immediate* temp = immediate;
      while ( i < left && is_byte_value( temp->value ) ) {
         temp = temp->next;
         ++i;
      }
      if ( i ) {
         left -= i;
         if ( task->compress ) {
            int code = PC_PUSH_BYTES;
            switch ( i ) {
            case 1: code = PC_PUSH_BYTE; break;
            case 2: code = PC_PUSH_2BYTES; break;
            case 3: code = PC_PUSH_3BYTES; break;
            case 4: code = PC_PUSH_4BYTES; break;
            case 5: code = PC_PUSH_5BYTES; break;
            default: break;
            }
            write_opc( task, code );
            if ( code == PC_PUSH_BYTES ) {
               write_arg( task, i );
            }
            while ( i ) {
               write_arg( task, immediate->value );
               immediate = immediate->next;
               --i;
            }
         }
         else {
            // Optimization: Pack four byte-values into a single 4-byte integer.
            // The instructions following this one will still be 4-byte aligned.
            while ( i >= 4 ) {
               write_opc( task, PC_PUSH_4BYTES );
               write_arg( task, immediate->value );
               immediate = immediate->next;
               write_arg( task, immediate->value );
               immediate = immediate->next;
               write_arg( task, immediate->value );
               immediate = immediate->next;
               write_arg( task, immediate->value );
               immediate = immediate->next;
               i -= 4;
            }
            while ( i ) {
               write_opc( task, PC_PUSH_NUMBER );
               write_arg( task, immediate->value );
               immediate = immediate->next;
               --i;
            }
         }
      }
      else {
         write_opc( task, PC_PUSH_NUMBER );
         write_arg( task, immediate->value );
         immediate = immediate->next;
         --left;
      }
   }
   left = count;
   while ( left ) {
      remove_immediate( task );
      --left;
   }
}

bool is_byte_value( int value ) {
   return ( ( unsigned int ) value <= 255 );
}

int t_tell( struct task* task ) {
   // Make sure to flush any queued immediates before acquiring the position.
   if ( task->immediate_count ) {
      push_immediate( task, task->immediate_count );
   }
   int pos = 0;
   struct buffer* buffer = task->buffer_head;
   while ( true ) {
      if ( buffer == task->buffer ) {
         pos += buffer->pos;
         break;
      }
      else if ( buffer->next ) {
         pos += BUFFER_SIZE;
         buffer = buffer->next;
      }
      else {
         pos += buffer->pos;
         break;
      }
   }
   return pos;
}

void t_seek( struct task* task, int pos ) {
   if ( pos == OBJ_SEEK_END ) {
      while ( task->buffer->next ) {
         task->buffer = task->buffer->next;
      }
      task->buffer->pos = task->buffer->used;
   }
   else {
      struct buffer* buffer = task->buffer_head;
      while ( buffer ) {
         if ( pos < BUFFER_SIZE ) {
            task->buffer = buffer;
            task->buffer->pos = pos;
            break;
         }
         pos -= BUFFER_SIZE;
         buffer = buffer->next;
      }
   }
}

void t_flush( struct task* task ) {
   bool failure = false;
   FILE* fh = fopen( task->options->object_file, "wb" );
   if ( fh ) {
      struct buffer* buffer = task->buffer_head;
      while ( buffer ) {
         int written = fwrite( buffer->data, 1, buffer->used, fh );
         if ( written != buffer->used ) {
            failure = true;
            break;
         }
         buffer = buffer->next;
      }
      fclose( fh );
   }
   else {
      failure = true;
   }
   if ( failure ) {
      t_diag( task, DIAG_ERR, "failed to write object file: %s",
         task->options->object_file );
      t_bail( task );
   }
}