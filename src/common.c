#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <errno.h>

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
// Allocation sizes for bulk allocation.
static struct {
   size_t size;
   size_t quantity;
} g_bulk_sizes[] = {
   { sizeof( struct func ), 128 },
   { sizeof( struct func_aspec ), 256 },
   { sizeof( struct list_link ), 256 },
   { sizeof( struct expr ), 256 },
   { sizeof( struct indexed_string_usage ), 256 },
   { sizeof( struct binary ), 256 },
   { sizeof( struct name_usage ), 256 },
   { sizeof( struct literal ), 256 },
   { sizeof( struct block ), 256 },
   { sizeof( struct name ), 256 },
   { sizeof( struct call ), 256 },
   { sizeof( struct constant ), 256 },
   { sizeof( struct param ), 256 }
};
// Bulk allocations.
static struct {
   struct {
      size_t size;
      size_t quantity;
      size_t left;
      char* block;
      struct free_block {
         struct free_block* next;
      }* free_block;
   } slots[ ARRAY_SIZE( g_bulk_sizes ) ];
   size_t slots_used;
} g_bulk;

static void unlink_alloc( struct alloc* );

void mem_init( void ) {
   g_bulk.slots_used = 0;
   size_t i = 0;
   while ( i < ARRAY_SIZE( g_bulk_sizes ) ) {
      // Find slot with specified allocation size.
      size_t k = 0;
      while ( k < g_bulk.slots_used &&
         g_bulk.slots[ k ].size != g_bulk_sizes[ i ].size ) {
         ++k;
      }
      // If slot doesn't exist, allocate one.
      if ( k == g_bulk.slots_used ) {
         g_bulk.slots[ k ].size = g_bulk_sizes[ i ].size;
         g_bulk.slots[ k ].quantity = g_bulk_sizes[ i ].quantity;
         g_bulk.slots[ k ].left = 0;
         g_bulk.slots[ k ].block = NULL;
         g_bulk.slots[ k ].free_block = NULL;
         ++g_bulk.slots_used;
      }
      else {
         // On duplicate allocation size, use higher quantity.
         if ( g_bulk.slots[ k ].quantity < g_bulk_sizes[ i ].quantity ) {
            g_bulk.slots[ k ].quantity = g_bulk_sizes[ i ].quantity;
         }
      }
      ++i;
   }
}

void* mem_alloc( size_t size ) {
   return mem_realloc( NULL, size );
}

void* mem_realloc( void* block, size_t size ) {
   struct alloc* alloc = NULL;
   if ( block ) {
      alloc = ( struct alloc* ) block - 1;
      unlink_alloc( alloc );
   }
   alloc = realloc( alloc, sizeof( *alloc ) + size );
   if ( ! alloc ) {
      mem_free_all();
      printf( "error: failed to allocate memory block of %zu bytes\n", size );
      exit( EXIT_FAILURE );
   }
   alloc->next = g_alloc;
   g_alloc = alloc;
   return alloc + 1;
}

static void unlink_alloc( struct alloc* alloc ) {
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
   size_t i = 0;
   while ( i < g_bulk.slots_used ) {
      if ( g_bulk.slots[ i ].size == size ) {
         // Reuse a previously allocated block.
         if ( g_bulk.slots[ i ].free_block ) {
            struct free_block* free_block = g_bulk.slots[ i ].free_block;
            g_bulk.slots[ i ].free_block = free_block->next;
            return free_block;
         }
         // When no more blocks are left, allocate a series of blocks in a
         // single allocation.
         if ( ! g_bulk.slots[ i ].left ) {
            g_bulk.slots[ i ].left = g_bulk.slots[ i ].quantity;
            g_bulk.slots[ i ].block = mem_alloc( g_bulk.slots[ i ].size *
               g_bulk.slots[ i ].quantity );
         }
         char* block = g_bulk.slots[ i ].block;
         g_bulk.slots[ i ].block += g_bulk.slots[ i ].size;
         --g_bulk.slots[ i ].left;
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
   size_t i = 0;
   while ( i < g_bulk.slots_used ) {
      if ( g_bulk.slots[ i ].size == size ) {
         struct free_block* free_block = block;
         free_block->next = g_bulk.slots[ i ].free_block;
         g_bulk.slots[ i ].free_block = free_block;
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

static void adjust_buffer( struct str* string, int length );

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
   adjust_buffer( str, length );
   memcpy( str->value, value, length );
   str->value[ length ] = 0;
   str->length = length;
}

// If necessary, increases the buffer size enough to fit a string of the
// specified length and the terminating null character. To select the size, an
// initial size is doubled until the size is appropriate.
static void adjust_buffer( struct str* string, int length ) {
   int required_length = length + 1;
   if ( string->buffer_length < required_length ) {
      int buffer_length = 1;
      while ( buffer_length < required_length ) {
         buffer_length <<= 1;
      }
      str_grow( string, buffer_length );
   }
}

void str_grow( struct str* string, int length ) {
   string->value = mem_realloc( string->value, length );
   string->buffer_length = length;
}

void str_append( struct str* str, const char* value ) {
   int length = strlen( value );
   int new_length = str->length + length;
   adjust_buffer( str, new_length );
   memcpy( str->value + str->length, value, length );
   str->value[ new_length ] = 0;
   str->length = new_length;
}

void str_append_sub( struct str* str, const char* cstr, int length ) {
   adjust_buffer( str, str->buffer_length + length );
   memcpy( str->value + str->length, cstr, length );
   str->length += length;
   str->value[ str->length ] = '\0';
}

void str_append_number( struct str* str, int number ) {
   char buffer[ 11 ];
   sprintf( buffer, "%d", number );
   str_append( str, buffer );
}

void str_append_format( struct str* str, const char* format, ... ) {
   va_list args;
   va_start( args, format );
   str_append_format_va( str, format, &args );
   va_end( args );
}

void str_append_format_va( struct str* str, const char* format,
   va_list* args ) {
   va_list args_copy;
   va_copy( args_copy, *args );
   int length = vsnprintf( NULL, 0, format, *args );
   if ( length >= 0 ) {
      if ( str->length + length >= str->buffer_length ) {
         str_grow( str, str->length + length + 1 );
      }
      vsnprintf( str->value + str->length, str->buffer_length - str->length,
         format, args_copy );
      str->length += length;
   }
   va_end( args_copy );
}

void str_clear( struct str* str ) {
   str->length = 0;
   if ( str->value ) {
      str->value[ 0 ] = 0;
   }
}

// Singly linked list
// ==========================================================================

static struct list_link* alloc_list_link( void* data );

void list_init( struct list* list ) {
   list->head = NULL;
   list->tail = NULL;
   list->size = 0;
}

int list_size( struct list* list ) {
   return list->size;
}

void* list_head( struct list* list ) {
   if ( list->head ) {
      return list->head->data;
   }
   else {
      return NULL;
   }
}

void* list_tail( struct list* list ) {
   if ( list->tail ) {
      return list->tail->data;
   }
   else {
      return NULL;
   }
}

void list_append( struct list* list, void* data ) {
   struct list_link* link = alloc_list_link( data );
   if ( list->head ) {
      list->tail->next = link;
   }
   else {
      list->head = link;
   }
   list->tail = link;
   ++list->size;
}

static struct list_link* alloc_list_link( void* data ) {
   struct list_link* link = mem_slot_alloc( sizeof( *link ) );
   link->data = data;
   link->next = NULL;
   return link;
}

void list_prepend( struct list* list, void* data ) {
   struct list_link* link = alloc_list_link( data );
   link->next = list->head;
   list->head = link;
   if ( ! list->tail ) {
      list->tail = link;
   }
   ++list->size;
}

void list_iterate( struct list* list, struct list_iter* iter ) {
   iter->prev = NULL;
   iter->link = list->head;
}

bool list_end( struct list_iter* iter ) {
   return ( iter->link == NULL );
}

void list_next( struct list_iter* iter ) {
   if ( iter->link ) {
      iter->prev = iter->link;
      iter->link = iter->link->next;
   }
}

void* list_data( struct list_iter* iter ) {
   if ( iter->link ) {
      return iter->link->data;
   }
   else {
      return NULL;
   }
}

void list_insert_after( struct list* list,
   struct list_iter* iter, void* data ) {
   if ( iter->link ) {
      struct list_link* link = alloc_list_link( data );
      link->next = iter->link->next;
      iter->link->next = link;
      if ( ! link->next ) {
         list->tail = link;
      }
      ++list->size;
   }
   else {
      list_append( list, data );
   }
}

void list_insert_before( struct list* list,
   struct list_iter* iter, void* data ) {
   if ( iter->prev ) {
      struct list_link* link = alloc_list_link( data );
      link->next = iter->link;
      iter->prev->next = link;
      if ( ! link->next ) {
         list->tail = link;
      }
      ++list->size;
   }
   else {
      list_prepend( list, data );
   }
}

void* list_replace( struct list* list,
   struct list_iter* iter, void* data ) {
   void* replaced_data = NULL;
   if ( iter->link ) {
      replaced_data = iter->link->data;
      iter->link->data = data;
   }
   return replaced_data;
}

void list_merge( struct list* receiver, struct list* giver ) {
   if ( giver->head ) {
      if ( receiver->head ) {
         receiver->tail->next = giver->head;
      }
      else {
         receiver->head = giver->head;
      }
      receiver->tail = giver->tail;
      receiver->size += giver->size;
      list_init( giver );
   }
}

void* list_shift( struct list* list ) {
   if ( list->head ) {
      void* data = list->head->data;
      struct list_link* next_link = list->head->next;
      mem_slot_free( list->head, sizeof( *list->head ) );
      list->head = next_link;
      if ( ! list->head ) {
         list->tail = NULL;
      }
      --list->size;
      return data;
   }
   else {
      return NULL;
   }
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

void fs_strip_trailing_pathsep( struct str* path ) {
   while ( path->length - 1 > 0 &&
      path->value[ path->length - 1 ] == '\\' ) {
      path->value[ path->length - 1 ] = '\0';
      --path->length;
   }
}

const char* fs_get_tempdir( void ) {
   static bool got_path = false;
   static CHAR path[ MAX_PATH + 1 ];
   if ( ! got_path ) {
      DWORD length = GetTempPathA( sizeof( path ), path );
      if ( length - 1 > 0 && path[ length - 1 ] == '\\' ) {
         path[ length - 1 ] = '\0';
      }
      got_path = true;
   }
   return path;
}

bool fs_create_dir( const char* path, struct fs_result* result ) {
   return ( CreateDirectory( path, NULL ) != 0 );
}

void fs_init_query( struct fs_query* query, const char* path ) {
   query->path = path;
   query->err = 0;
   query->attrs_obtained = false;
}

WIN32_FILE_ATTRIBUTE_DATA* get_file_attrs( struct fs_query* query ) {
   // Request file statistics once.
   if ( ! query->attrs_obtained ) {
      bool result = GetFileAttributesEx( query->path, GetFileExInfoStandard,
         &query->attrs );
      if ( ! result ) {
         query->err = GetLastError();
         return NULL;
      }
      query->attrs_obtained = true;
   }
   return &query->attrs;
}

bool fs_exists( struct fs_query* query ) {
   return ( get_file_attrs( query ) != NULL );
}

bool fs_is_dir( struct fs_query* query ) {
   WIN32_FILE_ATTRIBUTE_DATA* attrs = get_file_attrs( query );
   return ( attrs && ( attrs->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) );
}

#define UNIX_EPOCH 116444736000000000LL

bool fs_get_mtime( struct fs_query* query, struct fs_timestamp* timestamp ) {
   WIN32_FILE_ATTRIBUTE_DATA* attrs = get_file_attrs( query );
   if ( attrs ) {
      ULARGE_INTEGER mtime = { { attrs->ftLastWriteTime.dwLowDateTime,
         attrs->ftLastWriteTime.dwHighDateTime } };
      timestamp->value = ( mtime.QuadPart - UNIX_EPOCH ) / 10000000LL;
      return true;
   }
   else {
      return false;
   }
}

bool fs_delete_file( const char* path ) {
   return ( DeleteFileA( path ) == TRUE );
}

bool c_is_absolute_path( const char* path ) {
   return ( ( isalpha( path[ 0 ] ) && path[ 1 ] == ':' &&
      ( path[ 2 ] == '\\' || path[ 2 ] == '/' ) ) || path[ 0 ] == '\\' ||
      path[ 0 ] == '/' ); 
}

#else

#include <unistd.h>
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

void fs_strip_trailing_pathsep( struct str* path ) {
   while ( path->length - 1 > 0 &&
      path->value[ path->length - 1 ] == '/' ) {
      path->value[ path->length - 1 ] = '\0';
      --path->length;
   }
}

void fs_init_query( struct fs_query* query, const char* path ) {
   query->path = path;
   query->err = 0;
   query->stat_obtained = false;
}

struct stat* get_query_stat( struct fs_query* query ) {
   // Request file statistics once.
   if ( ! query->stat_obtained ) {
      if ( stat( query->path, &query->stat ) != 0 ) {
         query->err = errno;
         return NULL;
      }
      query->stat_obtained = true;
   }
   return &query->stat;
}

bool fs_exists( struct fs_query* query ) {
   return ( get_query_stat( query ) != NULL );
}

bool fs_is_dir( struct fs_query* query ) {
   struct stat* stat = get_query_stat( query );
   return ( stat && S_ISDIR( stat->st_mode ) );
}

bool fs_get_mtime( struct fs_query* query, struct fs_timestamp* timestamp ) {
   struct stat* stat = get_query_stat( query );
   if ( ! stat ) {
      return false;
   }
   timestamp->value = stat->st_mtime;
   return true;
}

bool fs_create_dir( const char* path, struct fs_result* result ) {
   static const mode_t mode =
      S_IRUSR | S_IWUSR | S_IXUSR |
      S_IRGRP | S_IXGRP |
      S_IROTH | S_IXOTH;
   if ( mkdir( path, mode ) == 0 ) {
      result->err = 0;
      return true;
   }
   else {
      result->err = errno;
      return false;
   }
}

const char* fs_get_tempdir( void ) {
   return "/tmp";
}

bool fs_delete_file( const char* path ) {
   return ( unlink( path ) == 0 );
}

bool c_is_absolute_path( const char* path ) {
   return ( path[ 0 ] == '/' ); 
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

const char* c_get_file_ext( const char* path ) {
   const char* string = strrchr( path, '.' );
   if ( string ) {
      return string + 1;
   }
   else {
      return "";
   }
}

void fs_get_file_contents( const char* path, struct file_contents* contents ) {
   FILE* fh = fopen( path, "rb" );
   if ( ! fh ) {
      contents->obtained = false;
      contents->err = errno;
      return;
   }
   fseek( fh, 0, SEEK_END );
   size_t size = ftell( fh );
   fseek( fh, 0, SEEK_SET );
   contents->data = mem_alloc( size );
   fread( contents->data, size, 1, fh );
   fclose( fh );
   contents->obtained = true;
   contents->err = 0;
}
