#include <string.h>

#include "gbuf.h"
#include "field.h"

// Writer
// ==========================================================================

void f_init_writer( struct field_writer* writer, struct gbuf* buffer ) {
   writer->output = buffer;
}

void f_wf( struct field_writer* writer, char field ) {
   gbuf_write( writer->output, &field, sizeof( field ) );
}

void f_wv( struct field_writer* writer, char field, void* value,
   size_t value_length ) {
   f_wf( writer, field );
   gbuf_write( writer->output, value, value_length );
}

void f_ws( struct field_writer* writer, char field, const char* value ) {
   f_wf( writer, field );
   gbuf_write( writer->output, value, strlen( value ) + 1 );
}

// Reader
// ==========================================================================

void f_init_reader( struct field_reader* reader, jmp_buf* bail,
   const char* data ) {
   reader->bail = bail;
   reader->data = data;
   reader->err = FIELDRERR_NONE;
   reader->field = 0;
   reader->expected_field = 0;
}

void f_rf( struct field_reader* reader, char expected_field ) {
   char field;
   memcpy( &field, reader->data, sizeof( field ) );
   reader->data += sizeof( field );
   if ( field != expected_field ) {
      reader->err = FIELDRERR_UNEXPECTEDFIELD;
      reader->field = field;
      reader->expected_field = expected_field;
      longjmp( *reader->bail, 1 );
   }
}

void f_rv( struct field_reader* reader, char field, void* value,
   size_t value_length ) {
   f_rf( reader, field );
   memcpy( value, reader->data, value_length );
   reader->data += value_length;
}


const char* f_rs( struct field_reader* reader, char field ) {
   f_rf( reader, field );
   const char* value = reader->data;
   while ( *reader->data ) {
      ++reader->data;
   }
   ++reader->data;
   return value;
}

char f_peek( struct field_reader* reader ) {
   char field;
   memcpy( &field, reader->data, sizeof( field ) );
   return field;
}