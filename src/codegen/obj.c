#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "task.h"
#include "pcode.h"

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
   task->opc = PCD_NONE;
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
      case PCD_PUSHNUMBER:
      case PCD_PUSHBYTE:
      case PCD_PUSH2BYTES:
      case PCD_PUSH3BYTES:
      case PCD_PUSH4BYTES:
      case PCD_PUSH5BYTES:
      case PCD_PUSHBYTES:
         task->push_immediate = true;
         goto finish;
      default:
         task->push_immediate = false;
         goto fold;
      }
   }
   // Unary constant folding:
   // -----------------------------------------------------------------------
   fold: {
      switch ( code ) {
      case PCD_UNARYMINUS:
      case PCD_NEGATELOGICAL:
      case PCD_NEGATEBINARY:
         if ( task->immediate_count ) {
            break;
         }
         // FALLTHROUGH
      default:
         goto binary_fold;
      }
      switch ( code ) {
      case PCD_UNARYMINUS:
         task->immediate_tail->value = ( - task->immediate_tail->value );
         break;
      case PCD_NEGATELOGICAL:
         task->immediate_tail->value = ( ! task->immediate_tail->value );
         break;
      case PCD_NEGATEBINARY:
         task->immediate_tail->value = ( ~ task->immediate_tail->value );
         break;
      default:
         break;
      }
      goto finish;
   }
   // Binary constant folding:
   // -----------------------------------------------------------------------
   binary_fold: {
      switch ( code ) {
      // TODO: Implement support for the logical operators.
      case PCD_ORLOGICAL:
      case PCD_ANDLOGICAL:
      case PCD_ORBITWISE:
      case PCD_EORBITWISE:
      case PCD_ANDBITWISE:
      case PCD_EQ:
      case PCD_NE:
      case PCD_LT:
      case PCD_LE:
      case PCD_GT:
      case PCD_GE:
      case PCD_LSHIFT:
      case PCD_RSHIFT:
      case PCD_ADD:
      case PCD_SUBTRACT:
      case PCD_MULIPLY:
      case PCD_DIVIDE:
      case PCD_MODULUS:
         if ( task->immediate_count >= 2 ) {
            break;
         }
         // FALLTHROUGH
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
      // case PCD_ORLOGICAL: last->value = ( l || r ); break;
      // case PCD_ANDLOGICAL: last->value = ( l && r ); break;
      case PCD_ORBITWISE: last->value = l | r; break;
      case PCD_EORBITWISE: last->value = l ^ r; break;
      case PCD_ANDBITWISE: last->value = l & r; break;
      case PCD_EQ: last->value = ( l == r ); break;
      case PCD_NE: last->value = ( l != r ); break;
      case PCD_LT: last->value = ( l < r ); break;
      case PCD_LE: last->value = ( l <= r ); break;
      case PCD_GT: last->value = ( l > r ); break;
      case PCD_GE: last->value = ( l >= r ); break;
      case PCD_LSHIFT: last->value = l << r; break;
      case PCD_RSHIFT: last->value = l >> r; break;
      case PCD_ADD: last->value = l + r; break;
      case PCD_SUBTRACT: last->value = l - r; break;
      case PCD_MULIPLY: last->value = l * r; break;
      case PCD_DIVIDE: last->value = l / r; break;
      case PCD_MODULUS: last->value = l % r; break;
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
         { PCD_LSPEC1, 1, PCD_LSPEC1DIRECT },
         { PCD_LSPEC2, 2, PCD_LSPEC2DIRECT },
         { PCD_LSPEC3, 3, PCD_LSPEC3DIRECT },
         { PCD_LSPEC4, 4, PCD_LSPEC4DIRECT },
         { PCD_LSPEC5, 5, PCD_LSPEC5DIRECT },
         { PCD_DELAY, 1, PCD_DELAYDIRECT },
         { PCD_RANDOM, 2, PCD_RANDOMDIRECT },
         { PCD_THINGCOUNT, 2, PCD_THINGCOUNTDIRECT },
         { PCD_TAGWAIT, 1, PCD_TAGWAITDIRECT },
         { PCD_POLYWAIT, 1, PCD_POLYWAITDIRECT },
         { PCD_CHANGEFLOOR, 2, PCD_CHANGEFLOORDIRECT },
         { PCD_CHANGECEILING, 2, PCD_CHANGECEILINGDIRECT },
         { PCD_SCRIPTWAIT, 1, PCD_SCRIPTWAITDIRECT },
         { PCD_CONSOLECOMMAND, 3, PCD_CONSOLECOMMANDDIRECT },
         { PCD_SETGRAVITY, 1, PCD_SETGRAVITYDIRECT },
         { PCD_SETAIRCONTROL, 1, PCD_SETAIRCONTROLDIRECT },
         { PCD_GIVEINVENTORY, 2, PCD_GIVEINVENTORYDIRECT },
         { PCD_TAKEINVENTORY, 2, PCD_TAKEINVENTORYDIRECT },
         { PCD_CHECKINVENTORY, 1, PCD_CHECKINVENTORYDIRECT },
         { PCD_SPAWN, 6, PCD_SPAWNDIRECT },
         { PCD_SPAWNSPOT, 4, PCD_SPAWNSPOTDIRECT },
         { PCD_SETMUSIC, 3, PCD_SETMUSICDIRECT },
         { PCD_LOCALSETMUSIC, 3, PCD_LOCALSETMUSIC },
         { PCD_SETFONT, 1, PCD_SETFONTDIRECT },
         { PCD_NONE, 0, PCD_NONE } };
      const struct entry* entry = entries;
      while ( entry->code != code ) {
         if ( entry->code == PCD_NONE ) {
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
         case PCD_LSPEC1: direct = PCD_LSPEC1DIRECTB; break;
         case PCD_LSPEC2: direct = PCD_LSPEC2DIRECTB; break;
         case PCD_LSPEC3: direct = PCD_LSPEC3DIRECTB; break;
         case PCD_LSPEC4: direct = PCD_LSPEC4DIRECTB; break;
         case PCD_LSPEC5: direct = PCD_LSPEC5DIRECTB; break;
         case PCD_DELAY: direct = PCD_DELAYDIRECTB; break;
         case PCD_RANDOM: direct = PCD_RANDOMDIRECTB; break;
         default: break;
         }
      }
      write_opc( task, direct );
      // Some instructions have other arguments that need to be written
      // before outputting the queued immediates.
      switch ( code ) {
      case PCD_LSPEC1:
      case PCD_LSPEC2:
      case PCD_LSPEC3:
      case PCD_LSPEC4:
      case PCD_LSPEC5:
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
      case PCD_LSPEC1DIRECT:
      case PCD_LSPEC2DIRECT:
      case PCD_LSPEC3DIRECT:
      case PCD_LSPEC4DIRECT:
      case PCD_LSPEC5DIRECT:
      case PCD_LSPEC1DIRECTB:
      case PCD_LSPEC2DIRECTB:
      case PCD_LSPEC3DIRECTB:
      case PCD_LSPEC4DIRECTB:
      case PCD_LSPEC5DIRECTB:
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
   case PCD_LSPEC1:
   case PCD_LSPEC2:
   case PCD_LSPEC3:
   case PCD_LSPEC4:
   case PCD_LSPEC5:
   case PCD_LSPEC5RESULT:
   case PCD_LSPEC1DIRECT:
   case PCD_LSPEC2DIRECT:
   case PCD_LSPEC3DIRECT:
   case PCD_LSPEC4DIRECT:
   case PCD_LSPEC5DIRECT:
      if ( task->opc_args == 0 && task->compress ) {
         t_add_byte( task, arg );
      }
      else {
         t_add_int( task, arg );
      }
      break;
   case PCD_PUSHBYTE:
   case PCD_PUSH2BYTES:
   case PCD_PUSH3BYTES:
   case PCD_PUSH4BYTES:
   case PCD_PUSH5BYTES:
   case PCD_PUSHBYTES:
   case PCD_LSPEC1DIRECTB:
   case PCD_LSPEC2DIRECTB:
   case PCD_LSPEC3DIRECTB:
   case PCD_LSPEC4DIRECTB:
   case PCD_LSPEC5DIRECTB:
   case PCD_DELAYDIRECTB:
   case PCD_RANDOMDIRECTB:
      t_add_byte( task, arg );
      break;
   case PCD_PUSHSCRIPTVAR:
   case PCD_PUSHMAPVAR:
   case PCD_PUSHMAPARRAY:
   case PCD_PUSHWORLDVAR:
   case PCD_PUSHWORLDARRAY:
   case PCD_PUSHGLOBALVAR:
   case PCD_PUSHGLOBALARRAY:
   case PCD_ASSIGNSCRIPTVAR:
   case PCD_ASSIGNMAPVAR:
   case PCD_ASSIGNWORLDVAR:
   case PCD_ASSIGNGLOBALVAR:
   case PCD_ASSIGNMAPARRAY:
   case PCD_ASSIGNWORLDARRAY:
   case PCD_ASSIGNGLOBALARRAY:
   case PCD_ADDSCRIPTVAR:
   case PCD_ADDMAPVAR:
   case PCD_ADDWORLDVAR:
   case PCD_ADDGLOBALVAR:
   case PCD_ADDMAPARRAY:
   case PCD_ADDWORLDARRAY:
   case PCD_ADDGLOBALARRAY:
   case PCD_SUBSCRIPTVAR:
   case PCD_SUBMAPVAR:
   case PCD_SUBWORLDVAR:
   case PCD_SUBGLOBALVAR:
   case PCD_SUBMAPARRAY:
   case PCD_SUBWORLDARRAY:
   case PCD_SUBGLOBALARRAY:
   case PCD_MULSCRIPTVAR:
   case PCD_MULMAPVAR:
   case PCD_MULWORLDVAR:
   case PCD_MULGLOBALVAR:
   case PCD_MULMAPARRAY:
   case PCD_MULWORLDARRAY:
   case PCD_MULGLOBALARRAY:
   case PCD_DIVSCRIPTVAR:
   case PCD_DIVMAPVAR:
   case PCD_DIVWORLDVAR:
   case PCD_DIVGLOBALVAR:
   case PCD_DIVMAPARRAY:
   case PCD_DIVWORLDARRAY:
   case PCD_DIVGLOBALARRAY:
   case PCD_MODSCRIPTVAR:
   case PCD_MODMAPVAR:
   case PCD_MODWORLDVAR:
   case PCD_MODGLOBALVAR:
   case PCD_MODMAPARRAY:
   case PCD_MODWORLDARRAY:
   case PCD_MODGLOBALARRAY:
   case PCD_LSSCRIPTVAR:
   case PCD_LSMAPVAR:
   case PCD_LSWORLDVAR:
   case PCD_LSGLOBALVAR:
   case PCD_LSMAPARRAY:
   case PCD_LSWORLDARRAY:
   case PCD_LSGLOBALARRAY:
   case PCD_RSSCRIPTVAR:
   case PCD_RSMAPVAR:
   case PCD_RSWORLDVAR:
   case PCD_RSGLOBALVAR:
   case PCD_RSMAPARRAY:
   case PCD_RSWORLDARRAY:
   case PCD_RSGLOBALARRAY:
   case PCD_ANDSCRIPTVAR:
   case PCD_ANDMAPVAR:
   case PCD_ANDWORLDVAR:
   case PCD_ANDGLOBALVAR:
   case PCD_ANDMAPARRAY:
   case PCD_ANDWORLDARRAY:
   case PCD_ANDGLOBALARRAY:
   case PCD_EORSCRIPTVAR:
   case PCD_EORMAPVAR:
   case PCD_EORWORLDVAR:
   case PCD_EORGLOBALVAR:
   case PCD_EORMAPARRAY:
   case PCD_EORWORLDARRAY:
   case PCD_EORGLOBALARRAY:
   case PCD_ORSCRIPTVAR:
   case PCD_ORMAPVAR:
   case PCD_ORWORLDVAR:
   case PCD_ORGLOBALVAR:
   case PCD_ORMAPARRAY:
   case PCD_ORWORLDARRAY:
   case PCD_ORGLOBALARRAY:
   case PCD_INCSCRIPTVAR:
   case PCD_INCMAPVAR:
   case PCD_INCWORLDVAR:
   case PCD_INCGLOBALVAR:
   case PCD_INCMAPARRAY:
   case PCD_INCWORLDARRAY:
   case PCD_INCGLOBALARRAY:
   case PCD_DECSCRIPTVAR:
   case PCD_DECMAPVAR:
   case PCD_DECWORLDVAR:
   case PCD_DECGLOBALVAR:
   case PCD_DECMAPARRAY:
   case PCD_DECWORLDARRAY:
   case PCD_DECGLOBALARRAY:
      if ( task->compress ) {
         t_add_byte( task, arg );
      }
      else {
         t_add_int( task, arg );
      }
      break;
   case PCD_CALL:
   case PCD_CALLDISCARD:
      if ( task->compress ) {
         t_add_byte( task, arg );
      }
      else {
         t_add_int( task, arg );
      }
      break;
   case PCD_CALLFUNC:
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
   case PCD_CASEGOTOSORTED:
      // The arguments need to be 4-byte aligned.
      if ( task->opc_args == 0 ) {
         int i = t_tell( task ) % 4;
         if ( i ) {
            while ( i < 4 ) {
               t_add_byte( task, 0 );
               ++i;
            }
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
            int code = PCD_PUSHBYTES;
            switch ( i ) {
            case 1: code = PCD_PUSHBYTE; break;
            case 2: code = PCD_PUSH2BYTES; break;
            case 3: code = PCD_PUSH3BYTES; break;
            case 4: code = PCD_PUSH4BYTES; break;
            case 5: code = PCD_PUSH5BYTES; break;
            default: break;
            }
            write_opc( task, code );
            if ( code == PCD_PUSHBYTES ) {
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
               write_opc( task, PCD_PUSH4BYTES );
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
               write_opc( task, PCD_PUSHNUMBER );
               write_arg( task, immediate->value );
               immediate = immediate->next;
               --i;
            }
         }
      }
      else {
         write_opc( task, PCD_PUSHNUMBER );
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

void t_seek_end( struct task* task ) {
   while ( task->buffer->next ) {
      task->buffer = task->buffer->next;
   }
   task->buffer->pos = task->buffer->used;
}

void t_flush( struct task* task ) {
   // Don't overwrite the object file unless it's an ACS object file, or the
   // file doesn't exist. This is a basic check done to help prevent the user
   // from accidently killing their other files. Maybe add a command-line
   // argument to force the overwrite, like --force-object-overwrite?
   FILE* fh = fopen( task->options->object_file, "rb" );
   if ( fh ) {
      char id[ 4 ];
      bool proceed = false;
      if ( fread( id, 1, 4, fh ) == 4 &&
         id[ 0 ] == 'A' && id[ 1 ] == 'C' && id[ 2 ] == 'S' &&
         ( id[ 3 ] == '\0' || id[ 3 ] == 'E' || id[ 3 ] == 'e' ) ) {
         proceed = true;
      }
      fclose( fh );
      if ( ! proceed ) {
         t_diag( task, DIAG_ERR, "trying to overwrite unknown file: %s",
            task->options->object_file );
         t_bail( task );
      }
   }
   bool failure = false;
   fh = fopen( task->options->object_file, "wb" );
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

void t_align_4byte( struct task* task ) {
   int padding = alignpad( t_tell( task ), 4 );
   int i = 0;
   while ( i < padding ) {
      t_add_byte( task, 0 );
      ++i;
   }
}