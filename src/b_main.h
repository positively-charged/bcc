#ifndef B_MAIN_H
#define B_MAIN_H

#include "common.h"
#include "out.h"
#include "b_opcode.h"

typedef struct immediate_t {
   struct immediate_t* next;
   int value;
} immediate_t;

typedef struct b_code_t {
   struct b_code_t* next;
   char* start;
   char* current;
   int size;
} b_code_t;

typedef struct b_node_t {
   union {
      struct b_pos_t {
         struct b_pos_usage_t* usage;
         int fpos;
      } pos;
      struct b_pos_usage_t {
         struct b_pos_usage_t* next;
         struct b_pos_t* pos;
         int fpos;
      } pos_usage;
      struct b_code_seg_t {
         b_code_t* codes;
         int pos;
         int size;
      } code_seg;
      struct {
         int value;
      } immediate;
   } body;
   struct b_node_t* next;
   struct b_node_t* prev;
   enum {
      k_back_node_pos,
      k_back_node_pos_usage,
      k_back_node_code_seg,
      k_back_node_immediate,
      k_back_node_4byte_align,
   } type;
} b_node_t;

typedef struct b_pos_t b_pos_t;

typedef struct {
   ast_t* ast; 
   output_t* out;
   int num_script_flags;
   options_t* options;
   int func_offset;
   int script_offset;
   int num_funcs;
   bool is_lib;
   immediate_t* immediates;
   immediate_t* immediates_tail;
   immediate_t* immediates_free;
   b_code_t* codes_head;
   b_code_t* codes_free;
   b_code_t* codes;
   b_node_t* node_head;
   b_node_t* node_tail;
   b_node_t* node_free;
   b_node_t* node;
   int immediates_size;
   int code;
   int num_args;
   int main_script_pos;
   int static_size;
   bool little_e;
} back_t;

void b_publish( ast_t*, options_t*, const char* );
void b_write_script( back_t*, script_t* );
void b_write_function( back_t*, ufunc_t* );
void b_alloc_code_block( back_t* );
void b_pcode( back_t*, int opcode, ... );
b_pos_t* b_new_pos( back_t* );
void b_seek( back_t*, b_pos_t* );
void b_flush( back_t* );

#endif