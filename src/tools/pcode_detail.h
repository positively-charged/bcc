#ifndef PCODE_DETAIL_H
#define PCODE_DETAIL_H

#include "../b_opcode.h"

typedef struct {
   opcode_t opcode;
   const char* name;
   int num_args;
} pcode_entry_t;

extern pcode_entry_t pcodes[];
extern int pcode_count;

#endif