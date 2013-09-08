#ifndef DETAIL_H
#define DETAIL_H

#include "f_tkid.h"
#include "b_opcode.h"
#include "common.h"

// Gets the name of a token.
const char* tk_name( tk_t );
const char* d_script_flag_name( int flag );
const char* d_name_jump( int jump );
const char* d_name_storage( int storage );
const char* d_name_script_type( int type );

#define PCODE_VARLENGTH -1

typedef struct {
   opcode_t opcode;
   const char* name;
   int num_args;
} pcode_entry_t;

extern pcode_entry_t g_pcode[];
extern int g_pcode_count;

#endif