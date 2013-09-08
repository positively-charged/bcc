#ifndef OUT_H
#define OUT_H

#include <stdio.h>
#include <stdbool.h>

typedef struct {
   FILE* fh;
} output_t;

bool o_init( output_t*, const char* path );
void o_deinit( output_t* );
void o_int( output_t*, int arg );
void o_byte( output_t* output, char arg );
// Outputs the contents of a string, but not the NULL byte.
void o_str( output_t*, const char* );
void o_sized( output_t*, void* value, int length );
int o_pos( output_t* );
void o_seek( output_t*, int offset );
void o_seek_beg( output_t* );
void o_seek_end( output_t* );
void o_fill( output_t*, int size );
void o_fill_align( output_t*, int size );

#endif