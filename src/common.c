#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"

typedef struct alloc_t {
   struct alloc_t* prev;
   struct alloc_t* next;
} alloc_t;

static alloc_t* g_alloc = NULL;
static alloc_t* g_alloc_tail = NULL;

void* mem_alloc( size_t size ) {
   void* block = malloc( sizeof( alloc_t ) + size );
   if ( g_alloc ) {
      alloc_t* alloc = block;
      alloc->next = NULL;
      alloc->prev = g_alloc_tail;
      g_alloc_tail->next = alloc;
      g_alloc_tail = alloc;
   }
   else {
      g_alloc = block;
      g_alloc->next = NULL;
      g_alloc->prev = NULL;
      g_alloc_tail = g_alloc;
   }
   return ( char* ) block + sizeof( alloc_t );
}

void* mem_realloc( void* block, size_t size ) {
   if ( block ) {
      block = realloc( ( alloc_t* ) block - 1, sizeof( alloc_t ) + size );
      alloc_t* alloc = block;
      if ( alloc->prev ) {
         alloc->prev->next = alloc;
      }
      else {
         g_alloc = alloc;
      }
      if ( alloc->next ) {
         alloc->next->prev = alloc;
      }
      else {
         g_alloc_tail = alloc;
      }
      return ( char* ) block + sizeof( alloc_t );
   }
   else {
      return mem_alloc( size );
   }
}

void mem_deinit( void ) {
   while ( g_alloc ) {
      alloc_t* next = g_alloc->next;
      free( g_alloc );
      g_alloc = next;
   }
}

int c_num_errs = 0;

void diag( int flags, ... ) {
   va_list args;
   va_start( args, flags );
   if ( flags & DIAG_FILE ) {
      struct pos* pos = va_arg( args, struct pos* );
      printf( "%s", pos->file->path.value );
      if ( flags & DIAG_LINE ) {
         printf( ":%d", pos->line );
         if ( flags & DIAG_COLUMN ) {
            printf( ":%d", pos->column );
         }
      }
      printf( ": " );
   }
   if ( flags & DIAG_ERR ) {
      printf( "error: " );
      c_num_errs += 1;
   }
   else if ( flags & DIAG_WARN ) {
      printf( "warning: " );
   }
   const char* format = va_arg( args, const char* );
   vprintf( format, args );
   printf( "\n" );
   va_end( args );
}

void str_init( struct str* str ) {
   str->value = NULL;
   str->length = 0;
   str->buff_length = 0;
}

void str_grow( struct str* str, int length ) {
   str->buff_length = length + 1;
   str->value = mem_realloc( str->value, str->buff_length );
}

void str_append( struct str* str, const char* cstr ) {
   str_append_sub( str, cstr, strlen( cstr ) );
}

void str_append_sub( struct str* str, const char* cstr, int length ) {
   if ( str->buff_length - str->length - 1 < length ) {
      str_grow( str, str->buff_length + length );
   }
   memcpy( str->value + str->length, cstr, length );
   str->length += length;
   str->value[ str->length ] = '\0';
}

void str_clear( struct str* str ) {
   str->value[ 0 ] = '\0';
   str->length = 0;
}

void str_del( struct str* str ) {
   if ( str->value ) {
      //mem_free( str->value );
      str->value = NULL;
   }
}

void list_init( struct list* list ) {
   list->head = NULL;
   list->tail = NULL;
   list->size = 0;
}

void list_append( struct list* list, void* data ) {
   struct list_link* link = mem_alloc( sizeof( *link ) );
   link->data = data;
   link->next = NULL;
   if ( list->head ) {
      list->tail->next = link;
   }
   else {
      list->head = link;
   }
   list->tail = link;
   list->size += 1;
}

void list_append_h( struct list* list, void* data ) {
   struct list_link* link = mem_alloc( sizeof( *link ) );
   link->data = data;
   link->next = list->head;
   list->head = link;
   if ( ! list->tail ) {
      list->tail = link;
   }
   list->size += 1;
}

void d_list_init( struct d_list* list ) {
   list->head = NULL;
   list->tail = NULL;
   list->size = 0;
}

void d_list_append( struct d_list* list, void* data ) {
   struct d_list_link* link = mem_alloc( sizeof( *link ) );
   link->data = data;
   link->next = NULL;
   link->prev = NULL;
   if ( list->head ) {
      link->prev = list->tail;
      list->tail->next = link;
      list->tail = link;
   }
   else {
      list->head = link;
      list->tail = link;
   }
   list->size += 1;
}

void stack_init( struct stack* entry ) {
   entry->prev = NULL;
   entry->value = NULL;
}

void stack_push( struct stack* entry, void* value ) {
   if ( entry->value ) {
      struct stack* prev = mem_alloc( sizeof( *prev ) );
      prev->prev = entry->prev;
      prev->value = entry->value;
      entry->prev = prev;
   }
   entry->value = value;
}

void* stack_pop( struct stack* entry ) {
   if ( entry->value ) {
      void* value = entry->value;
      if ( entry->prev ) {
         entry->value = entry->prev->value;
         entry->prev = entry->prev->prev;
      }
      else {
         entry->value = NULL;
      }
      return value;
   }
   else {
      return NULL;
   }
}