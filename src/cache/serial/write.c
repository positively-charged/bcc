#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include "cache/cache.h"
#include "write.h"
#include "common.h"

void srl_init_writing( struct serialw* writing, struct gbuf* buffer ) {
   writing->output = buffer;
}

void srlw_f( struct serialw* writing, enum field field ) {
   struct {
      enum field field;
   } entry = { field };
   gbuf_write( writing->output, &entry, sizeof( entry ) );
}

void srlw_i( struct serialw* writing, enum field field, int value ) {
   struct {
      enum field field;
      int value;
   } entry = { field, value };
   gbuf_write( writing->output, &entry, sizeof( entry ) );
}

void srlw_b( struct serialw* writing, enum field field, bool value ) {
   srlw_f( writing, field );
   gbuf_write( writing->output, &value, sizeof( value ) );
}

void srlw_s( struct serialw* writing, enum field field, const char* value ) {
   gbuf_write( writing->output, &field, sizeof( field ) );
   gbuf_write( writing->output, value, strlen( value ) + 1 );
}

void srlw_t( struct serialw* writing, enum field field, time_t value ) {
   srlw_f( writing, field );
   gbuf_write( writing->output, &value, sizeof( value ) );
}

void srlw_v( struct serialw* writing, enum field field, void* buffer ) {
   gbuf_write( writing->output, &field, sizeof( field ) );
   size_t size = 0;
   if ( field > F_STARTTIME && field < F_ENDTIME ) {
      size = sizeof( time_t );
   }
   else if ( field > F_STARTSIZE && field < F_ENDSIZE ) {
      size = sizeof( size_t );
   }
   gbuf_write( writing->output, buffer, size );
}