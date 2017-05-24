#ifndef SRC_CACHE_FIELD_H
#define SRC_CACHE_FIELD_H

#include <setjmp.h>

// Writer
// ==========================================================================

struct field_writer {
   struct gbuf* output;
};

void f_init_writer( struct field_writer* writer, struct gbuf* buffer );
void f_wf( struct field_writer* writer, char field );
void f_wv( struct field_writer* writer, char field, void* value,
   size_t value_length );
void f_ws( struct field_writer* writer, char field, const char* value );

// Reader
// ==========================================================================

struct field_reader {
   jmp_buf* bail;
   const char* data;
   enum {
      FIELDRERR_NONE,
      FIELDRERR_UNEXPECTEDFIELD,
   } err;
   char field;
   char expected_field;
};

void f_init_reader( struct field_reader* reader, jmp_buf* bail,
   const char* data );
void f_rf( struct field_reader* reader, char expected_field );
void f_rv( struct field_reader* reader, char field, void* value,
   size_t value_length );
const char* f_rs( struct field_reader* reader, char field );
char f_peek( struct field_reader* reader );

#endif
