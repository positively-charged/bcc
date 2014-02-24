#include <stdio.h>
#include <setjmp.h>
#include <string.h>

#include "common.h"
#include "task.h"

// Memory
// NOTE: The functions below may be violating the strict-aliasing rule.
// ==========================================================================

struct alloc {
   struct alloc* next;
};

// Linked list of current allocations. The head is the most recent allocation.
// This way, a short-term allocation can be found and removed quicker.
static struct alloc* g_alloc = NULL;

static void unlink_alloc( struct alloc* );

void* mem_alloc( size_t size ) {
   return mem_realloc( NULL, size );
}

void* mem_realloc( void* block, size_t size ) {
   struct alloc* alloc = NULL;
   if ( block ) {
      alloc = ( struct alloc* ) block - 1;
      unlink_alloc( alloc );
   }
   // TODO: Bail on error.
   alloc = realloc( alloc, sizeof( *alloc ) + size );
   alloc->next = g_alloc;
   g_alloc = alloc;
   return alloc + 1;
}

void unlink_alloc( struct alloc* alloc ) {
   struct alloc* curr = g_alloc;
   struct alloc* prev = NULL;
   while ( curr != alloc ) {
      prev = curr;
      curr = curr->next;
   }
   if ( prev ) {
      prev->next = alloc->next;
   }
   else {
      g_alloc = alloc->next;
   }
}

void mem_free( void* block ) {
   struct alloc* alloc = ( struct alloc* ) block - 1;
   unlink_alloc( alloc );
   free( alloc );
}

void mem_free_all( void ) {
   while ( g_alloc ) {
      struct alloc* next = g_alloc->next;
      free( g_alloc );
      g_alloc = next;
   }
}

// Str
// ==========================================================================

void str_init( struct str* str ) {
   str->length = 0;
   str->buffer_length = 0;
   str->value = NULL;
}

void str_deinit( struct str* str ) {
   if ( str->value ) {
      mem_free( str->value );
   }
}

void str_copy( struct str* str, const char* value, int length ) {
   if ( str->buffer_length <= length ) {
      str_grow( str, length + 1 );
   }
   memcpy( str->value, value, length );
   str->value[ length ] = 0;
   str->length = length;
}

void str_grow( struct str* str, int length ) {
   str->value = mem_realloc( str->value, length );
   str->buffer_length = length;
}

void str_append( struct str* str, const char* value ) {
   int length = strlen( value );
   int new_length = str->length + length;
   if ( str->buffer_length <= new_length ) {
      str_grow( str, new_length + 1 );
   }
   memcpy( str->value + str->length, value, length );
   str->value[ new_length ] = 0;
   str->length = new_length;
}

void str_append_sub( struct str* str, const char* cstr, int length ) {
   if ( str->buffer_length - str->length - 1 < length ) {
      str_grow( str, str->buffer_length + length );
   }
   memcpy( str->value + str->length, cstr, length );
   str->length += length;
   str->value[ str->length ] = '\0';
}

void str_clear( struct str* str ) {
   str->length = 0;
   if ( str->value ) {
      str->value[ 0 ] = 0;
   }
}

// Singly linked list
// ==========================================================================

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
   ++list->size;
}

void list_append_head( struct list* list, void* data ) {
   struct list_link* link = mem_alloc( sizeof( *link ) );
   link->data = data;
   link->next = list->head;
   list->head = link;
   if ( ! list->tail ) {
      list->tail = link;
   }
   ++list->size;
}

void list_deinit( struct list* list ) {
   struct list_link* link = list->head;
   while ( link ) {
      struct list_link* next = link->next;
      mem_free( link );
      link = next;
   }
}

// Doubly linked list
// ==========================================================================

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

// Stack
// ==========================================================================

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

// File Identity
// ==========================================================================

#ifdef __WINDOWS__

#else

#include <sys/stat.h>

bool c_read_identity( struct file_identity* identity, const char* path ) {
   struct stat buff;
   if ( stat( path, &buff ) == -1 ) {
      return false;
   }
   identity->device = buff.st_dev;
   identity->number = buff.st_ino;
   return true;
}

bool c_same_identity( struct file_identity* first,
   struct file_identity* other ) {
   return (
      first->device == other->device &&
      first->number == other->number );
}

#endif