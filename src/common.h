#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdbool.h>

void* mem_alloc( size_t );
void* mem_realloc( void*, size_t );
void mem_deinit( void );

#define ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#define STATIC_ASSERT( cond ) switch ( 0 ) { case 0: case cond: ; }

struct str {
   char* value;
   int length;
   int buff_length;
};

void str_init( struct str* );
void str_grow( struct str*, int length );
void str_append( struct str*, const char* cstr );
void str_append_sub( struct str*, const char* cstr, int length );
void str_clear( struct str* );
void str_del( struct str* );

enum {
   STORAGE_LOCAL,
   STORAGE_MAP,
   STORAGE_WORLD,
   STORAGE_GLOBAL
};

struct file {
   struct str path;
   struct str load_path;
};

struct pos {
   struct file* file;
   int line;
   int column;
};

#define DIAG_NONE 0
#define DIAG_FILE 0x1
#define DIAG_LINE 0x2
#define DIAG_COLUMN 0x4
#define DIAG_WARN 0x8
#define DIAG_ERR 0x10

void diag( int flags, ... );
void bail( void );

// Linked list.
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

void list_init( struct list* );
void list_append( struct list*, void* );
void list_append_h( struct list*, void* );

// Doubly-linked list.
struct d_list_link {
   struct d_list_link* next;
   struct d_list_link* prev;
   void* data;
};

struct d_list {
   struct d_list_link* head;
   struct d_list_link* tail;
   int size;
};

typedef struct d_list_link* d_list_iter_t;

void d_list_init( struct d_list* );
void d_list_append( struct d_list*, void* );

#define d_list_iter_init_head( i, list ) ( ( *i ) = ( list )->head )
#define d_list_iter_init_tail( i, list ) ( ( *i ) = ( list )->tail )
#define d_list_end( i ) ( *i == NULL )
#define d_list_data( i ) ( *i )->data
#define d_list_next( i ) ( ( *i ) = ( *i )->next )
#define d_list_prev( i ) ( ( *i ) = ( *i )->prev )

struct stack {
   struct stack* prev;
   void* value;
};

void stack_init( struct stack* );
void stack_push( struct stack*, void* );
void* stack_pop( struct stack* );

struct options {
   struct list includes;
   const char* source_file;
   const char* object_file;
   bool encrypt_str;
   bool err_file;
};

extern int c_num_errs;

#endif