#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdbool.h>

void* mem_alloc( size_t );
void* mem_realloc( void*, size_t );
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
void str_clear( struct str* );

enum {
   STORAGE_LOCAL,
   STORAGE_MAP,
   STORAGE_WORLD,
   STORAGE_GLOBAL
};

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
#define list_head( list ) ( ( list )->head->data )

void list_init( struct list* );
void list_append( struct list*, void* );
void list_append_head( struct list*, void* );
void list_deinit( struct list* );

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
   int tab_size;
   bool encrypt_str;
   bool acc_err;
   bool one_column;
};

extern int c_num_errs;

#ifdef __WINDOWS__

#else

#include <sys/types.h>

struct file_identity {
   dev_t device;
   ino_t number;
};

#endif

bool c_read_identity( struct file_identity*, const char* path );
bool c_same_identity( struct file_identity*, struct file_identity* );

struct file {
   struct file* prev;
   struct str path;
   struct str load_path;
   struct file_identity identity;
   bool one_copy;
};

#endif