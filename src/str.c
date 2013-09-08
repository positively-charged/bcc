#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "mem.h"

void str_init( str_t* str ) {
   str->value = NULL;
   str->length = 0;
   str->buff_length = 0;
}

void str_grow( str_t* str, int length ) {
   str->buff_length = length + 1;
   str->value = realloc( str->value, str->buff_length );
}

void str_append( str_t* str, const char* cstr ) {
   str_append_sub( str, cstr, strlen( cstr ) );
}

void str_append_sub( str_t* str, const char* cstr, int length ) {
   if ( str->buff_length - str->length - 1 < length ) {
      str_grow( str, str->buff_length + length );
   }
   memcpy( str->value + str->length, cstr, length );
   str->length += length;
   str->value[ str->length ] = '\0';
}

void str_clear( str_t* str ) {
   str->value[ 0 ] = '\0';
   str->length = 0;
}

void str_del( str_t* str ) {
   if ( str->value ) {
      free( str->value );
      str->value = NULL;
   }
}