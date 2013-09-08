/*

   Receives the pcode instructions, performing final optimizations before
   outputting them to a file.

*/

#include <stdarg.h>
#include <string.h>

#include "b_main.h"
#include "detail.h"
#include "mem.h"

#define CODE_BLOCK_SIZE 4096

enum {
   k_add_byte,
   k_add_short,
   k_add_int,
};

typedef struct {
   int code;
   int direct;
   int direct_b;
} opcode_direct_t;

typedef struct b_pos_usage_t b_pos_usage_t;
typedef struct b_code_seg_t b_code_seg_t;

static b_node_t* add_node( back_t* );
static void add_code( back_t*, int, int );
static void push_immediate( back_t*, int );
static immediate_t* remove_immediate( back_t* );
static bool is_byte_value( int );
static bool is_range_byte( back_t*, int );
static void write_immediates( back_t*, int );
static void assign_var( back_t*, bool, int, int, int );
static const opcode_direct_t* get_direct_opcode( int );
static void add_opcode( back_t*, int );
static void add_arg( back_t*, int );
static void add_pos( back_t*, b_pos_t* );
static void add_pcode( back_t*, int, va_list* );
static void add_pseudo( back_t*, int, va_list* );
static void add_align4( back_t* );

void b_alloc_code_block( back_t* back ) {
   if ( back->codes && back->codes->next ) {
      b_code_t* codes = back->codes->next;
      codes->current = codes->start;
      codes->size = 0;
      back->codes = codes;
   }
   else if ( back->codes_free ) {
      b_code_t* codes = back->codes_free;
      codes->current = codes->start;
      codes->size = 0;
      back->codes = codes;
      back->codes_free = NULL;
   }
   else {
      char* block = mem_temp_alloc( sizeof( b_code_t ) + CODE_BLOCK_SIZE );
      b_code_t* codes = ( b_code_t* ) block;
      codes->next = NULL;
      codes->start = block + sizeof( *codes );
      codes->current = codes->start;
      codes->size = 0;
      if ( back->codes ) {
         back->codes->next = codes;
      }
      else {
         back->codes_head = codes;
      }
      back->codes = codes;
   }
}

b_node_t* add_node( back_t* back ) {
   b_node_t* node = NULL;
   if ( back->node_free ) {
      node = back->node_free;
      back->node_free = node->next;
   }
   else {
      node = mem_temp_alloc( sizeof( *node ) );
   }
   if ( back->node ) {
      node->next = back->node->next;
      back->node->next = node;
      back->node = node;
      if ( ! node->next ) {
         back->node_tail = node;
      }
   }
   else {
      node->next = NULL;
      back->node_head = node;
      back->node_tail = node;
      back->node = node;
   }
   return node;
}

void add_code( back_t* back, int type, int value ) {
   b_code_seg_t* code_seg = NULL;
   if ( back->node && back->node->type == k_back_node_code_seg ) {
      code_seg = &back->node->body.code_seg;
   }
   else {
      b_node_t* node = add_node( back );
      node->type = k_back_node_code_seg;
      code_seg = &node->body.code_seg;
      code_seg->codes = back->codes;
      code_seg->pos = back->codes->size;
      code_seg->size = 0;
   }
   char* ch = ( char* ) &value;
   int length = sizeof( value );
   short value_s = 0;
   char value_c = 0;
   switch ( type ) {
   case k_add_byte:
      value_c = ( char ) value;
      ch = &value_c;
      length = sizeof( value_c );
      break;
   case k_add_short:
      value_s = ( short ) value;
      ch = ( char* ) &value_s;
      length = sizeof( value_s );
      break;
   default:
      break;
   }
   b_code_t* codes = back->codes;
   while ( length ) {
      if ( codes->size == CODE_BLOCK_SIZE ) {
         b_alloc_code_block( back );
         codes = back->codes;
      }
      *codes->current = *ch;
      codes->current += 1;
      codes->size += 1;
      code_seg->size += 1;
      length -= 1;
      ch += 1;
   }
}

b_pos_t* b_new_pos( back_t* back ) {
   if ( back->immediates_size ) {
      write_immediates( back, back->immediates_size );
   }
   b_node_t* node = add_node( back );
   node->type = k_back_node_pos;
   b_pos_t* pos = &node->body.pos;
   pos->usage = NULL;
   pos->fpos = 0;
   return pos;
}

void b_seek( back_t* back, b_pos_t* pos ) {
   if ( pos ) {
      back->node = ( b_node_t* ) pos;
   }
   else {
      back->node = back->node_tail;
   }
}

void b_flush( back_t* back ) {
   b_node_t* node = back->node_head;
   while ( node ) {
      if ( node->type == k_back_node_pos ) {
         b_pos_t* pos = &node->body.pos;
         pos->fpos = o_pos( back->out );
         b_pos_usage_t* usage = pos->usage;
         while ( usage ) {
            // Add position to earlier usages.
            if ( usage->fpos ) {
               o_seek( back->out, usage->fpos );
               o_int( back->out, pos->fpos );
            }
            usage = usage->next;
         }
         o_seek_end( back->out );
      }
      else if ( node->type == k_back_node_pos_usage ) {
         b_pos_usage_t* usage = &node->body.pos_usage;
         if ( usage->pos->fpos ) {
            o_int( back->out, usage->pos->fpos );
         }
         else {
            usage->fpos = o_pos( back->out );
            o_int( back->out, 0 );
         }
      }
      else if ( node->type == k_back_node_code_seg ) {
         b_code_seg_t* seg = &node->body.code_seg;
         b_code_t* codes = back->codes;
         int pos = seg->pos;
         int left = seg->size;
         while ( left ) {
            int size = CODE_BLOCK_SIZE - pos;
            if ( left <= size ) {
               size = left;
            }
            o_sized( back->out, codes->start + pos, size );
            left -= size;
            // Proceed to the next block.
            codes = codes->next;
            pos = 0;
         }
      }
      else if ( node->type == k_back_node_4byte_align ) {
         int padding = o_pos( back->out ) % 4;
         if ( padding ) {
            padding = 4 - padding;
         }
         o_fill( back->out, padding );
      }
      node = node->next;
   }
   back->node_free = back->node_head;
   back->node_head = NULL;
   back->node = NULL;
   back->codes = NULL;
   back->codes_free = back->codes_head;
   b_alloc_code_block( back );
}

void b_pcode( back_t* back, int code, ... ) {
   va_list args;
   va_start( args, code );
   if ( code == pc_push_number ) {
      push_immediate( back, va_arg( args, int ) );
   }
   else if ( code > pp_start ) {
      add_pseudo( back, code, &args );
   }
   else {
      add_pcode( back, code, &args );
   }
   va_end( args );
}

void add_pcode( back_t* back, int code, va_list* args ) {
   pcode_entry_t* entry = &g_pcode[ code ];
   const opcode_direct_t* d_code = get_direct_opcode( code );
   if ( d_code && back->immediates_size >= entry->num_args ) {
      if ( d_code->direct_b != pc_none && back->little_e &&
         is_range_byte( back, entry->num_args ) ) {
         code = d_code->direct_b;
      }
      else {
         code = d_code->direct;
      }
      bool is_lspec = ( d_code->code >= pc_lspec1 &&
         d_code->code <= pc_lspec5 );
      entry = &g_pcode[ code ];
      int num_immediates = back->immediates_size - entry->num_args;
      if ( is_lspec ) {
         num_immediates += 1;
      }
      write_immediates( back, num_immediates );
      add_opcode( back, code );
      if ( is_lspec ) {
         add_arg( back, va_arg( *args, int ) );
      }
      while ( back->immediates_size ) {
         immediate_t* immediate = remove_immediate( back );
         add_arg( back, immediate->value );
      }
   }
   else {
      write_immediates( back, back->immediates_size );
      add_opcode( back, code );
      int num_args = entry->num_args;
      if ( num_args == PCODE_VARLENGTH ) {
         num_args = va_arg( *args, int );
      }
      while ( num_args ) {
         add_arg( back, va_arg( *args, int ) );
         num_args -= 1;
      }
   }
}

void add_pseudo( back_t* back, int code, va_list* args ) {
   write_immediates( back, back->immediates_size );
   switch ( code ) {
   case pp_arg:
      break;
   case pp_assign_var:
   case pp_assign_array: ;
      int storage = va_arg( *args, int );
      int index = va_arg( *args, int );
      int op = va_arg( *args, int );
      assign_var( back, ( code == pp_assign_array ), storage, index, op );
      break;
   case pp_opcode:
      add_opcode( back, va_arg( args, int ) );
      break;
   case pp_goto:
   case pp_if_goto:
   case pp_if_not_goto:
      if ( code == pp_if_goto ) {
         add_opcode( back, pc_if_goto );
      }
      else if ( code == pp_if_not_goto ) {
         add_opcode( back, pc_if_not_goto );
      }
      else {
         add_opcode( back, pc_goto );
      }
      // FALLTHROUGH.
   case pp_arg_pos:
      add_pos( back, va_arg( *args, b_pos_t* ) );
      break;
   default:
      break;
   }
}

void add_arg( back_t* back, int arg ) {
   switch ( back->code ) {
      int add_type;
   case pc_lspec1:
   case pc_lspec2:
   case pc_lspec3:
   case pc_lspec4:
   case pc_lspec5:
   case pc_lspec1_direct:
   case pc_lspec2_direct:
   case pc_lspec3_direct:
   case pc_lspec4_direct:
   case pc_lspec5_direct:
   case pc_lspec5_result:
      add_type = k_add_int;
      // In Little-E, the ID is compressed.
      if ( back->num_args == 0 && back->little_e ) {
         add_type = k_add_byte;
      }
      add_code( back, add_type, arg );
      break;
   case pc_lspec1_direct_b:
   case pc_lspec2_direct_b:
   case pc_lspec3_direct_b:
   case pc_lspec4_direct_b:
   case pc_lspec5_direct_b:
   case pc_push_byte:
   case pc_push2_bytes:
   case pc_push3_bytes:
   case pc_push4_bytes:
   case pc_push5_bytes:
   case pc_push_bytes:
      add_code( back, k_add_byte, arg );
      break;
   case pc_push_script_var:
   case pc_push_map_var:
   case pc_push_world_var:
   case pc_push_global_var:
   case pc_push_map_array:
   case pc_push_world_array:
   case pc_push_global_array:
      add_type = k_add_int;
      if ( back->little_e ) {
         add_type = k_add_byte;
      }
      add_code( back, add_type, arg );
      break;
   case pc_call:
   case pc_call_discard:
      add_type = k_add_int;
      if ( back->little_e ) {
         add_type = k_add_byte;
      }
      add_code( back, add_type, arg );
      break;
   case pc_call_func:
      add_type = k_add_int;
      if ( back->little_e ) {
         // Argument count.
         if ( back->num_args == 0 ) {
            add_type = k_add_byte;
         }
         // Function index.
         else {
            add_type = k_add_short;
         }
      }
      add_code( back, add_type, arg );
      break;
   case pc_case_goto_sorted:
      if ( back->num_args == 0 ) {
         add_align4( back );
      }
      add_code( back, k_add_int, arg );
      break;
   default:
      add_code( back, k_add_int, arg );
      break;
   }
   back->num_args += 1;
}

const opcode_direct_t* get_direct_opcode( int code ) {
   static const opcode_direct_t codes[] = {
      { pc_lspec1, pc_lspec1_direct, pc_lspec1_direct_b },
      { pc_lspec2, pc_lspec2_direct, pc_lspec2_direct_b },
      { pc_lspec3, pc_lspec3_direct, pc_lspec3_direct_b },
      { pc_lspec4, pc_lspec4_direct, pc_lspec4_direct_b },
      { pc_lspec5, pc_lspec5_direct, pc_lspec5_direct_b },
      { pc_delay, pc_delay_direct, pc_delay_direct_b },
      { pc_random, pc_random_direct, pc_random_direct_b },
      { pc_thing_count, pc_thing_count_direct, pc_none },
      { pc_tag_wait, pc_tag_wait_direct, pc_none },
      { pc_poly_wait, pc_poly_wait_direct, pc_none },
      { pc_change_floor, pc_change_floor_direct, pc_none },
      { pc_change_ceiling, pc_change_ceiling_direct, pc_none },
      { pc_script_wait, pc_script_wait_direct, pc_none },
      { pc_console_command, pc_console_command_direct, pc_none },
      { pc_set_gravity, pc_set_gravity_direct, pc_none },
      { pc_set_air_control, pc_set_air_control_direct, pc_none },
      { pc_give_inventory, pc_give_inventory_direct, pc_none },
      { pc_take_inventory, pc_take_inventory_direct, pc_none },
      { pc_check_inventory, pc_check_inventory_direct, pc_none },
      { pc_spawn, pc_spawn_direct, pc_none },
      { pc_spawn_spot, pc_spawn_spot_direct, pc_none },
      { pc_set_music, pc_set_music_direct, pc_none },
      { pc_local_set_music, pc_local_set_music_direct, pc_none },
      { pc_set_font, pc_set_font_direct, pc_none } };
   for ( int i = 0; i < sizeof( codes ) / sizeof( codes[ 0 ] ); ++i ) {
      if ( codes[ i ].code == code ) { return &codes[ i ]; }
   }
   return NULL;
}

void assign_var( back_t* back, bool array, int storage, int index, int op ) {
   static int v_code[] = {
      pc_assign_script_var, pc_assign_map_var, pc_assign_world_var,
         pc_assign_global_var,
      pc_add_script_var, pc_add_map_var, pc_add_world_var, pc_add_global_var,
      pc_sub_script_var, pc_sub_map_var, pc_sub_world_var, pc_sub_global_var,
      pc_mul_script_var, pc_mul_map_var, pc_mul_world_var, pc_mul_global_var,
      pc_div_script_var, pc_div_map_var, pc_div_world_var, pc_div_global_var,
      pc_mod_script_var, pc_mod_map_var, pc_mod_world_var, pc_mod_global_var,
      pc_ls_script_var, pc_ls_map_var, pc_ls_world_var, pc_ls_global_var,
      pc_rs_script_var, pc_rs_map_var, pc_rs_world_var, pc_rs_global_var,
      pc_and_script_var, pc_and_map_var, pc_and_world_var, pc_and_global_var,
      pc_eor_script_var, pc_eor_map_var, pc_eor_world_var, pc_eor_global_var,
      pc_or_script_var, pc_or_map_var, pc_or_world_var, pc_or_global_var };
   static int a_code[] = {
      pc_assign_map_array, pc_assign_world_array, pc_assign_global_array,
      pc_add_map_array, pc_add_world_array, pc_add_global_array,
      pc_sub_map_var, pc_sub_world_array, pc_sub_global_array,
      pc_mul_map_array, pc_mul_world_array, pc_mul_global_array,
      pc_div_map_array, pc_div_world_array, pc_div_global_array,
      pc_mod_map_array, pc_mod_world_array, pc_mod_global_array,
      pc_ls_map_array, pc_ls_world_array, pc_ls_global_array,
      pc_rs_map_array, pc_rs_world_array, pc_rs_global_array,
      pc_and_map_array, pc_and_world_array, pc_and_global_array,
      pc_eor_map_array, pc_eor_world_array, pc_eor_global_array,
      pc_or_map_array, pc_or_world_array, pc_or_global_array };
   int pos = 0;
   switch ( storage ) {
   case k_storage_module: pos = 1; break;
   case k_storage_world: pos = 2; break;
   case k_storage_global: pos = 3; break;
   default: break;
   }
   int code = 0;
   if ( array ) {
      code = a_code[ op * 3 + pos - 1 ];
   }
   else {
      code = v_code[ op * 4 + pos ];
   }
   add_opcode( back, code );
   int add_type = k_add_int;
   if ( back->little_e ) {
      add_type = k_add_byte;
   }
   add_code( back, add_type, index );
}

void add_opcode( back_t* back, int code ) {
   if ( back->little_e ) {
      // I don't know how the compression algorithm works, but at this time,
      // this works for all of pcodes available at this time. Also, now that
      // the pcode is shrinked, the 4-byte arguments following the pcode may no
      // longer be aligned to their natural boundary.
      if ( code >= 240 ) {
         add_code( back, k_add_byte, 240 );
         add_code( back, k_add_byte, code - 240 );
      }
      else {
         add_code( back, k_add_byte, code );
      }
   }
   else {
      add_code( back, k_add_int, code );
   }
   back->code = code;
   back->num_args = 0;
}

void add_align4( back_t* back ) {
   b_node_t* node = add_node( back );
   node->type = k_back_node_4byte_align;
}

void add_pos( back_t* back, b_pos_t* pos ) {
   b_node_t* node = add_node( back );
   node->type = k_back_node_pos_usage;
   b_pos_usage_t* usage = &node->body.pos_usage;
   usage->pos = pos;
   usage->fpos = 0;
   usage->next = pos->usage;
   pos->usage = usage;
   ++back->num_args;
}

void push_immediate( back_t* back, int value ) {
   immediate_t* immediate = NULL;
   if ( back->immediates_free ) {
      immediate = back->immediates_free;
      back->immediates_free = immediate->next;
   }
   else {
      immediate = mem_temp_alloc( sizeof( *immediate ) );
   }
   immediate->value = value;
   immediate->next = NULL;
   if ( back->immediates_tail ) {
      back->immediates_tail->next = immediate;
   }
   else {
      back->immediates = immediate;
   }
   back->immediates_tail = immediate;
   ++back->immediates_size;
}

immediate_t* remove_immediate( back_t* back ) {
   immediate_t* immediate = back->immediates;
   --back->immediates_size;
   back->immediates = immediate->next;
   if ( ! back->immediates_size ) {
      back->immediates_tail = NULL;
   }
   immediate->next = back->immediates_free;
   back->immediates_free = immediate;
   return immediate;
}

bool is_byte_value( int value ) {
   return ( value <= 127 && value >= -128 );
}

bool is_range_byte( back_t* back, int length ) {
   immediate_t* immediate = back->immediates;
   while ( immediate && length ) {
      if ( ! is_byte_value( immediate->value ) ) { return false; }
      immediate = immediate->next;
      --length;
   }
   return true;
}

void write_immediates( back_t* back, int left ) {
   immediate_t* immediate = back->immediates;
   int left_remove = left;
   while ( immediate && left > 0 ) {
      int count = 0;
      immediate_t* test = immediate;
      while ( is_byte_value( test->value ) && count < left ) {
         ++count;
         test = test->next;
         if ( ! test ) { break; }
      }
      // Byte values.
      if ( count ) {
         left -= count;
         if ( back->little_e ) {
            int code = pc_push_bytes;
            switch ( count ) {
            case 1: code = pc_push_byte; break;
            case 2: code = pc_push2_bytes; break;
            case 3: code = pc_push3_bytes; break;
            case 4: code = pc_push4_bytes; break;
            case 5: code = pc_push5_bytes; break;
            default: break;
            }
            add_opcode( back, code );
            if ( code == pc_push_bytes ) {
               add_code( back, k_add_byte, count );
            }
            while ( count ) {
               add_code( back, k_add_byte, immediate->value );
               immediate = immediate->next;
               --count;
            }
         }
         else {
            // Optimization: Pack four byte-values into a single 4-byte
            // integer. The instructions following this one will still be
            // 4-byte aligned.
            while ( count >= sizeof( int ) ) {
               add_opcode( back, pc_push4_bytes );
               int end = count - sizeof( int );
               while ( count > end ) {
                  add_code( back, k_add_byte, immediate->value );
                  immediate = immediate->next;
                  --count;
               }
            }
            while ( count ) {
               add_opcode( back, pc_push_number );
               add_code( back, k_add_int, immediate->value );
               immediate = immediate->next;
               --count;
            }
         }
      }
      // Integer values.
      else {
         while ( left && ! is_byte_value( immediate->value ) ) {
            add_opcode( back, pc_push_number );
            add_code( back, k_add_int, immediate->value );
            immediate = immediate->next;
            --left;
         }
      }
   }
   // Empty queue.
   while ( back->immediates_size && left_remove ) {
      remove_immediate( back );
      --left_remove;
   }
}