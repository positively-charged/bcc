#ifndef MEM_H
#define MEM_H

#include <stdlib.h>
#include <stdint.h>

typedef struct {
   struct region_t* region;
   int size;
} mem_marker_t;

void mem_init( void );
void mem_deinit( void );
// Allocates a block of memory that cannot be freed. Should be used for data
// that should remain alive until the end of compilation.
void* mem_alloc( uint32_t size );
// Temporary memory used by a single phase of the compiler.
void* mem_temp_alloc( uint32_t size );
void mem_temp_reset( void );

#endif