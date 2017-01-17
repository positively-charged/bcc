#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#if defined( _WIN32 ) || defined( _WIN64 )
#   define OS_WINDOWS 1
#else
#   define OS_WINDOWS 0
#endif

void mem_init( void );
void* mem_alloc( size_t );
void* mem_realloc( void*, size_t );
void* mem_slot_alloc( size_t );
void mem_free( void* );
void mem_free_all( void );

#define ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#define STATIC_ASSERT( cond ) switch ( 0 ) { case 0: case cond: ; }
#define UNREACHABLE() printf( \
   "%s:%d: internal error: unreachable code\n", \
   __FILE__, __LINE__ );

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
void str_append_format( struct str* str, const char* format, va_list* args );
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
void list_merge( struct list* receiver, struct list* giver );
void* list_shift( struct list* list );
void list_deinit( struct list* );

struct options {
   struct list includes;
   struct list defines;
   struct list library_links;
   const char* source_file;
   const char* object_file;
   int tab_size;
   int lang;
   bool acc_err;
   bool acc_stats;
   bool one_column;
   bool help;
   bool preprocess;
   bool write_asserts;
   bool lang_specified;
   struct {
      const char* dir_path;
      int lifetime;
      bool enable;
      bool print;
      bool clear;
   } cache;
};

#if OS_WINDOWS

#include <windows.h>
#include <time.h>

// NOTE: Volume information is not included. Maybe add it later.
struct fileid {
   int id_high;
   int id_low;
};

#define NEWLINE_CHAR "\r\n"
#define OS_PATHSEP "\\"

struct fs_query {
   WIN32_FILE_ATTRIBUTE_DATA attrs;
   const char* path;
   DWORD err;
   bool attrs_obtained;
};

struct fs_timestamp {
   time_t value;
};

#define strcasecmp _stricmp

#else

#include <sys/types.h>
#include <sys/stat.h>

struct fileid {
   dev_t device;
   ino_t number;
};

#define NEWLINE_CHAR "\n"
#define OS_PATHSEP "/"

struct fs_query {
   const char* path;
   struct stat stat;
   int err;
   bool stat_obtained;
};

struct fs_timestamp {
   time_t value;
};

#endif

struct file_contents {
   char* data;
   int err;
   bool obtained;
};

struct fs_result {
   int err;
};

bool c_read_fileid( struct fileid*, const char* path );
bool c_same_fileid( struct fileid*, struct fileid* );
bool c_read_full_path( const char* path, struct str* );
void c_extract_dirname( struct str* );

int alignpad( int size, int align_size );

void fs_init_query( struct fs_query* query, const char* path );
bool fs_exists( struct fs_query* query );
bool fs_is_dir( struct fs_query* query );
bool fs_get_mtime( struct fs_query* query, struct fs_timestamp* timestamp );
bool fs_create_dir( const char* path, struct fs_result* result );
const char* fs_get_tempdir( void );
void fs_get_file_contents( const char* path, struct file_contents* contents );
void fs_strip_trailing_pathsep( struct str* path );
bool fs_delete_file( const char* path );
bool c_is_absolute_path( const char* path );

#endif