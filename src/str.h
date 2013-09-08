#ifndef STR_H
#define STR_H

typedef struct {
   char* value;
   int length;
   int buff_length;
} str_t;

void str_init( str_t* );
void str_grow( str_t*, int length );
void str_append( str_t*, const char* cstr );
void str_append_sub( str_t*, const char* cstr, int length );
void str_clear( str_t* );
void str_del( str_t* );

#endif