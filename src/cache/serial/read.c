#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "read.h"

// static void read_v( struct reading* reading, enum field field, void* buffer );

void srl_init_reading( struct serial_r* reading, jmp_buf* exit,
   const char* data ) {
   reading->exit = exit;
   reading->data = data;
}

void srl_rf( struct serial_r* reading, enum field expected_field ) {
   enum field field;
   memcpy( &field, reading->data, sizeof( field ) );
   reading->data += sizeof( field );
   if ( field != expected_field ) {
      printf( "unexpected field: %d\n", field );
      printf( "expecting: %d\n", expected_field );
      longjmp( *reading->exit, 1 );
   }
}

enum field srl_peekf( struct serial_r* reading ) {
   enum field field;
   memcpy( &field, reading->data, sizeof( field ) );
   return field;
}

int srl_ri( struct serial_r* reading, enum field expected_field ) {
   int integer;
   srl_rf( reading, expected_field );
   memcpy( &integer, reading->data, sizeof( integer ) );
   reading->data += sizeof( integer );
   return integer;
}

time_t srl_rt( struct serial_r* reading, enum field expected_field ) {
   time_t value;
   srl_rf( reading, expected_field );
   memcpy( &value, reading->data, sizeof( value ) );
   reading->data += sizeof( value );
   return value;
}

bool srl_rb( struct serial_r* reading, enum field expected_field ) {
   bool boolean;
   srl_rf( reading, expected_field );
   memcpy( &boolean, reading->data, sizeof( boolean ) );
   reading->data += sizeof( boolean );
   return boolean;
}

const char* srl_rs( struct serial_r* reading, enum field expected_field ) {
   srl_rf( reading, expected_field );
   const char* string = reading->data;
   while ( *reading->data ) {
      ++reading->data;
   }
   ++reading->data;
   return string;
}

/*
// Reads various fields.
void read_v( struct reading* reading, enum field field, void* buffer ) {
   read_f( reading, field );
   size_t size = 0;
   if ( field > F_STARTTIME && field < F_ENDTIME ) {
      size = sizeof( time_t );
   }
   else if ( field > F_STARTSIZE && field < F_ENDSIZE ) {
      size = sizeof( size_t );
   }
   memcpy( buffer, reading->data, size );
   reading->data += size;
}
*/