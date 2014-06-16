#include <stdio.h>
#include <setjmp.h>
#include <string.h>

#include "common.h"
#include "task.h"

// Memory
// NOTE: The functions below may be violating the strict-aliasing rule.
// ==========================================================================

// Linked list of current allocations. The head is the most recent allocation.
// This way, a short-term allocation can be found and removed quicker.
static struct alloc {
   struct alloc* next;
}* g_alloc = NULL;
// Bulk allocations.
static struct {
   size_t alloc_size;
   int quantity;
   char* slot;
   int size;   
   struct free_slot {
      struct free_slot* next;
   }* free_slot;
} store[] = {
   { 8, 256 },  // struct func_aspec
   { 16, 256 }, // struct list_link
   { 24, 256 }, // struct expr
                // struct indexed_string_usage
   { 32, 256 }, // struct binary
                // struct name_usage
                // struct literal
                // struct block
   { 40, 256 }, // struct name
   { 56, 256 }, // struct call
   { 64, 256 }, // struct constant
   { 80, 256 }, // struct param
   { 88, 128 }, // struct func
   { 0 }
};

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

void* mem_slot_alloc( size_t size ) {
   int i = 0;
   while ( store[ i ].alloc_size ) {
      if ( store[ i ].alloc_size == size ) {
         // Reuse a previously allocated block.
         if ( store[ i ].free_slot ) {
            struct free_slot* slot = store[ i ].free_slot;
            store[ i ].free_slot = slot->next;
            return slot;
         }
         // Allocate a series of blocks in one allocation.
         if ( ! store[ i ].size ) {
            store[ i ].size = store[ i ].quantity;
            store[ i ].slot = mem_alloc( store[ i ].alloc_size *
               store[ i ].quantity );
         }
         char* block = store[ i ].slot;
         store[ i ].slot += store[ i ].alloc_size;
         store[ i ].size -= 1;
         return block;
      }
      ++i;
   }
   return mem_alloc( size );
}

void mem_free( void* block ) {
   struct alloc* alloc = ( struct alloc* ) block - 1;
   unlink_alloc( alloc );
   free( alloc );
}

void mem_slot_free( void* block, size_t size ) {
   int i = 0;
   while ( store[ i ].alloc_size ) {
      if ( store[ i ].alloc_size == size ) {
         struct free_slot* slot = block;
         slot->next = store[ i ].free_slot;
         store[ i ].free_slot = slot;
         return;
      }
      ++i;
   }
   mem_free( block );
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

void str_append_number( struct str* str, int number ) {
   char buffer[ 11 ];
   sprintf( buffer, "%d", number );
   str_append( str, buffer );
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
   struct list_link* link = mem_slot_alloc( sizeof( *link ) );
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
   struct list_link* link = mem_slot_alloc( sizeof( *link ) );
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
      mem_slot_free( link, sizeof( *link ) );
      link = next;
   }
}

// File Identity
// ==========================================================================

#if OS_WINDOWS

#include <windows.h>

bool c_read_fileid( struct fileid* fileid, const char* path ) {
   HANDLE fh = CreateFile( path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0,
      NULL );
   if ( fh != INVALID_HANDLE_VALUE ) {
      BY_HANDLE_FILE_INFORMATION detail;
      if ( GetFileInformationByHandle( fh, &detail ) ) {
         fileid->id_high = detail.nFileIndexHigh;
         fileid->id_low = detail.nFileIndexLow;
         CloseHandle( fh );
         return true;
      }
      else {
         CloseHandle( fh );
         return false;
      }
   }
   else {
      return false;
   }
}

bool c_same_fileid( struct fileid* a, struct fileid* b ) {
   return ( a->id_high == b->id_high && a->id_low == b->id_low );
}

#else

#include <sys/stat.h>

bool c_read_fileid( struct fileid* fileid, const char* path ) {
   struct stat buff;
   if ( stat( path, &buff ) == -1 ) {
      return false;
   }
   fileid->device = buff.st_dev;
   fileid->number = buff.st_ino;
   return true;
}

bool c_same_fileid( struct fileid* a, struct fileid* b ) {
   return ( a->device == b->device && a->number == b->number );
}

#endif

// Miscellaneous
// ==========================================================================

int alignpad( int size, int align_size ) {
   int i = size % align_size;
   if ( i ) {
      return align_size - i;
   }
   else {
      return 0;
   }
}

#if OS_WINDOWS

#include <windows.h>

bool c_read_full_path( const char* path, struct str* str ) {
   const int max_path = MAX_PATH + 1;
   if ( str->buffer_length < max_path ) { 
      str_grow( str, max_path );
   }
   str->length = GetFullPathName( path, max_path, str->value, NULL );
   if ( GetFileAttributes( str->value ) != INVALID_FILE_ATTRIBUTES ) {
      int i = 0;
      while ( str->value[ i ] ) {
         if ( str->value[ i ] == '\\' ) {
            str->value[ i ] = '/';
         }
         ++i;
      }
      return true;
   }
   else {
      return false;
   }
}

#else

#include <dirent.h>
#include <sys/stat.h>

#ifdef __APPLE__
#include <limits.h> // PATH_MAX on OS X is defined in limits.h
#elif defined __linux__
#include <linux/limits.h> // And on Linux it is in linux/limits.h
#endif

bool c_read_full_path( const char* path, struct str* str ) {
   str_grow( str, PATH_MAX + 1 );
   if ( realpath( path, str->value ) ) {
      str->length = strlen( str->value );
      return true;
   }
   else {
      return false;
   }
}

#endif

void c_extract_dirname( struct str* path ) {
   while ( true ) {
      if ( path->length == 0 ) {
         break;
      }
      --path->length;
      char ch = path->value[ path->length ];
      path->value[ path->length ] = 0;
      if ( ch == '/' || ch == '\\' ) {
         break;
      }
   }
}