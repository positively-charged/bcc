#include <stdio.h>
#include <string.h>

#include "out.h"

bool o_init( output_t* o, const char* path ) {
   o->fh = fopen( path, "wb" );
   return ( o->fh != NULL );
}

void o_deinit( output_t* o ) {
   fclose( o->fh );
}

void o_byte( output_t* output, char byte ) {
   fputc( byte, output->fh );
}

void o_int( output_t* o, int arg ) {
   fwrite( &arg, sizeof( arg ), 1, o->fh );
}

void o_sized( output_t* o, void* value, int length ) {
   fwrite( value, length, 1, o->fh );
}

void o_str( output_t* o, const char* str ) {
   fwrite( str, strlen( str ), 1, o->fh );
}

void o_seek( output_t* o, int offset ) {
   fseek( o->fh, offset, SEEK_SET );
}

void o_seek_beg( output_t* o ) {
   fseek( o->fh, 0, SEEK_SET );
}

void o_seek_end( output_t* o ) {
   fseek( o->fh, 0, SEEK_END );
}

int o_pos( output_t* o ) {
   return ftell( o->fh );
}

void o_fill( output_t* output, int size ) {
   for ( int i = 0; i < size; ++i ) {
      char ch = 0;
      fwrite( &ch, sizeof( ch ), 1, output->fh );
   }
}

void o_fill_align( output_t* output, int size ) {
   int rem = ftell( output->fh ) % size;
   if ( rem ) {
      o_fill( output, size - rem );
   }
}