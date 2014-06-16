#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdbool.h>

#if defined( _WIN32 ) || defined( _WIN64 )
#   define OS_WINDOWS 1
#else
#   define OS_WINDOWS 0
#endif

void* mem_alloc( size_t );
void* mem_realloc( void*, size_t );
void* mem_slot_alloc( size_t );
void mem_free( void* );
void mem_free_all( void );

#define ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#define STATIC_ASSERT( cond ) switch ( 0 ) { case 0: case cond: ; }

struct str {
   char* value;
   int length;
   int buffer_length;
};

void str_init( struct str* );
void str_deinit( struct str* );
void str_copy( struct str*, const char* value, int length );
void str_grow( struct str*, int length );
void str_append( struct str*, const char* cstr );
void str_append_sub( struct str*, const char* cstr, int length );
void str_append_number( struct str*, int number );
void str_clear( struct str* );

struct list_link {
   struct list_link* next;
   void* data;
};

struct list {
   struct list_link* head;
   struct list_link* tail;
   int size;
};

typedef struct list_link* list_iter_t;

#define list_iter_init( i, list ) ( ( *i ) = ( list )->head )
#define list_end( i ) ( *( i ) == NULL )
#define list_data( i ) ( ( *i )->data )
#define list_next( i ) ( ( *i ) = ( *i )->next )
#define list_size( list ) ( ( list )->size )
#define list_head( list ) ( ( list )->head->data )
#define list_tail( list ) ( ( list )->tail->data )

void list_init( struct list* );
void list_append( struct list*, void* );
void list_append_head( struct list*, void* );
void list_deinit( struct list* );

struct options {
   struct list includes;
   const char* source_file;
   const char* object_file;
   int tab_size;
   bool acc_err;
   bool one_column;
   bool help;
};

#if OS_WINDOWS

// NOTE: Volume information is not included. Maybe add it later.
struct fileid {
   int id_high;
   int id_low;
};

#else

#include <sys/types.h>

struct fileid {
   dev_t device;
   ino_t number;
};

#endif

bool c_read_fileid( struct fileid*, const char* path );
bool c_same_fileid( struct fileid*, struct fileid* );
bool c_read_full_path( const char* path, struct str* );
void c_extract_dirname( struct str* );

int alignpad( int size, int align_size );

#endif