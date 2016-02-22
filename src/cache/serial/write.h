#ifndef SRC_SERIAL_WRITE_H
#define SRC_SERIAL_WRITE_H

#include <stdio.h>
#include <stdbool.h>
#include <setjmp.h>
#include <time.h>

#include "field.h"

struct serialw {
   struct gbuf* output;
};

void srl_init_writing( struct serialw* writing, struct gbuf* buffer );
void srlw_f( struct serialw* writing, enum field field );
void srlw_i( struct serialw* writing, enum field field, int value );
void srlw_b( struct serialw* writing, enum field field, bool value );
void srlw_s( struct serialw* writing, enum field field,
   const char* value );
void srlw_t( struct serialw* writing, enum field field, time_t value );
void srlw_v( struct serialw* writing, enum field field, void* buffer );

#endif