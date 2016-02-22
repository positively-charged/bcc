#include <string.h>
#include <time.h>

#include "parse/phase.h"
#include "semantic/phase.h"
#include "cache.h"

static void prepare_dir( struct cache* cache );
static void prepare_tempdir( struct cache* cache );
static void load_archive( struct cache* cache );
static struct cache_entry* alloc_entry( void );
static void append_entry( struct cache_entry_list* list,
   struct cache_entry* entry );
static struct cache_entry* create_module_entry( struct cache* cache,
   struct library* lib );
static struct cache_entry* find_entry( struct cache* cache, const char* path );
static bool timestamp_entry( struct cache_entry* entry );
static bool obtain_timstamp( struct str* file_path, time_t* timestamp );
static bool fresh_entry( struct cache_entry* entry );
static void load_entry_file( struct cache* cache, struct cache_entry* entry );
static void write_entryfile( struct cache* cache, struct cache_entry* entry,
   struct gbuf* buffer );
static void append_entryfile_path( struct cache* cache,
   struct cache_entry* entry, struct str* path );
static char* read_file_contents( const char* path );

static void unlink_entry( struct cache_entry_list* list,
   struct cache_entry* entry );
static void remove_outdated_entries( struct cache* cache );
static void write_cache( struct cache* cache );
static void write_entry( struct cache* cache, struct cache_entry* entry );
static void write_archive( struct cache* cache );
static void write_buffer( const char* path, struct gbuf* buffer );

static void print_cache( struct cache* cache );

static void init_cache_entry_list( struct cache_entry_list* list ) {
   list->head = NULL;
   list->tail = NULL;
}

void cache_init( struct cache* cache, struct task* task ) {
   cache->task = task;
   str_init( &cache->path );
   init_cache_entry_list( &cache->entries );
   init_cache_entry_list( &cache->removed_entries );
   cache->buffer = NULL;
   cache->id = 0;
}

void cache_load( struct cache* cache ) {
   prepare_dir( cache );
   load_archive( cache );
}

void prepare_dir( struct cache* cache ) {
   if ( cache->task->options->cache_path ) {
      str_append( &cache->path, cache->task->options->cache_path );
   }
   else {
      prepare_tempdir( cache );
   }
}

void prepare_tempdir( struct cache* cache ) {
   // Use temporary directory of the system.
   str_append( &cache->path, fs_get_tempdir() );
   str_append( &cache->path, "/bcc_cache" );
   struct fs_query query;
   fs_init_query( &query, cache->path.value );
   if ( ! fs_exists( &query ) ) {
      bool created = fs_create_dir( cache->path.value );
      if ( ! created ) {
         t_diag( cache->task, DIAG_ERR,
            "failed to create cache directory: %s",
            cache->path.value );
         t_bail( cache->task );
      }
   }
}

void load_archive( struct cache* cache ) {
   char* contents = read_file_contents( LIBCACHE_FILEPATH );
   if ( contents ) {
      cache_read_archive( cache, contents );
      mem_free( contents );
   }
}

// Inserts a module into the cache.
void cache_add( struct cache* cache, struct library* lib ) {
   struct cache_entry* entry = find_entry( cache, lib->file->full_path.value );
   if ( ! entry ) {
      entry = create_module_entry( cache, lib );
   }
   entry->lib = lib;
   entry->modified = true;
   entry->mtime = cache->task->compile_time;
   entry->head_dependency = NULL;
   // Module itself.
   struct cache_dependency* dep = cache_alloc_dependency(
      lib->file->full_path.value );
   cache_append_dependency( entry, dep );
   // Imported modules.
   list_iter_t i;
   list_iter_init( &i, &lib->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      dep = cache_alloc_dependency( lib->file->full_path.value );
      cache_append_dependency( entry, dep );
      list_next( &i );
   }
}

struct cache_entry* create_module_entry( struct cache* cache,
   struct library* lib ) {
   struct cache_entry* entry = cache_create_entry( cache,
      lib->file->full_path.value );
   return entry;
}

struct cache_entry* cache_create_entry( struct cache* cache,
   const char* path ) {
   int id = cache->id;
   ++cache->id;
   struct cache_entry* entry = alloc_entry();
   str_append( &entry->file_path, path );
   cache_entryfile_path( id, &entry->cache_file_path );
   entry->id = id;
   append_entry( &cache->entries, entry );
   return entry;
}

struct cache_entry* alloc_entry( void ) {
   struct cache_entry* entry = mem_alloc( sizeof( *entry ) );
   entry->next = NULL;
   entry->head_dependency = NULL;
   entry->dependency = NULL;
   str_init( &entry->file_path );
   str_init( &entry->cache_file_path );
   entry->lib = NULL;
   entry->mtime = 0u;
   entry->id = 0u;
   entry->modified = false;
   return entry;
}

void cache_entryfile_path( int id, struct str* path ) {
   str_append( path, "/tmp/bcc_cache/lib_" );
   char id_string[ 12 ];
   sprintf( id_string, "%d", id );
   str_append( path, id_string );
   str_append( path, ".o" );
}

void append_entry( struct cache_entry_list* list, struct cache_entry* entry ) {
   if ( list->head ) {
      list->tail->next = entry;
   }
   else {
      list->head = entry;
   }
   list->tail = entry;
}

struct cache_dependency* cache_alloc_dependency( const char* file_path ) {
   struct cache_dependency* dep = mem_alloc( sizeof( *dep ) );
   dep->next = NULL;
   str_init( &dep->file_path );
   str_append( &dep->file_path, file_path );
   dep->mtime = 0u;
   return dep;
}

void cache_append_dependency( struct cache_entry* entry,
   struct cache_dependency* dep ) {
   if ( entry->head_dependency ) {
      entry->dependency->next = dep;
   }
   else {
      entry->head_dependency = dep;
   }
   entry->dependency = dep;
}

// Begins by searches for the cache for the module with the specified file
// path. If a match is made, the cached contents of the module are parsed and
// a module is generated.
struct library* cache_get( struct cache* cache, struct file_entry* file ) {
   struct cache_entry* entry = find_entry( cache, file->full_path.value );
   // Entry must be present.
   if ( ! entry ) {
      return NULL;
   }
   // Load once the contents of a cached module.
   if ( ! entry->lib ) {
      timestamp_entry( entry );
      if ( fresh_entry( entry ) ) {
         load_entry_file( cache, entry );
         entry->lib->file = file;
      }
   }
   return entry->lib;
}

struct cache_entry* find_entry( struct cache* cache, const char* path ) {
   struct cache_entry* entry = cache->entries.head;
   while ( entry && strcmp( entry->file_path.value, path ) != 0 ) {
      entry = entry->next;
   }
   return entry;
}

bool timestamp_entry( struct cache_entry* entry ) {
   bool obtained = obtain_timstamp( &entry->cache_file_path, &entry->mtime );
   struct cache_dependency* dep = entry->head_dependency;
   while ( dep && obtained ) {
      obtained = obtain_timstamp( &dep->file_path, &dep->mtime );
      dep = dep->next;
   }
   return obtained;
}

bool obtain_timstamp( struct str* file_path, time_t* timestamp ) {
   struct fs_query query;
   fs_init_query( &query, file_path->value );
   if ( fs_exists( &query ) ) {
      *timestamp = fs_get_mtime( &query );
      return true;
   }
   return false;
}

bool fresh_entry( struct cache_entry* entry ) {
   struct cache_dependency* dep = entry->head_dependency;
   while ( dep && dep->mtime <= entry->mtime ) {
      dep = dep->next;
   }
   return ( dep == NULL );
}

void load_entry_file( struct cache* cache, struct cache_entry* entry ) {
   struct str path;
   str_init( &path );
   append_entryfile_path( cache, entry, &path );
   char* contents = read_file_contents( path.value );
   entry->lib = cache_restore_module( cache->task, contents );
}

char* read_file_contents( const char* path ) {
   FILE* fh = fopen( path, "r" );
   if ( ! fh ) {
      return NULL;
   }
   fseek( fh, 0, SEEK_END );
   size_t size = ftell( fh );
   fseek( fh, 0, SEEK_SET );
   char* contents = mem_alloc( size );
   fread( contents, size, 1, fh );
   fclose( fh );
   return contents;
}

// Moves every entry into the removed-entry list.
void cache_clear( struct cache* cache ) {
   while ( cache->entries.head ) {
      struct cache_entry* entry = cache->entries.head;
      unlink_entry( &cache->entries, entry );
      append_entry( &cache->removed_entries, entry );
   }
}

void unlink_entry( struct cache_entry_list* list, struct cache_entry* entry ) {
   if ( entry == list->head ) {
      list->head = entry->next;
      if ( ! list->head ) {
         list->tail = NULL;
      }
   }
   else {
      struct cache_entry* prev_entry = NULL;
      struct cache_entry* curr_entry = list->head;
      while ( curr_entry != entry ) {
         prev_entry = curr_entry;
         curr_entry = curr_entry->next;
      }
      prev_entry->next = entry->next;
      entry->next = NULL;
      if ( ! prev_entry->next ) {
         list->tail = NULL;
      }
   }
}

// Performs final operations of the cache:
// - Remove outdated entries.
// - Remove cache file of outdated entries.
// - Write valid entries into permanent storage.
void cache_close( struct cache* cache ) {
   remove_outdated_entries( cache );
   write_cache( cache );
}

// Removes from the cache:
// - Outdated entries, and their cache file.
// - Entries missing their cache file.
void remove_outdated_entries( struct cache* cache ) {
   time_t present_time = time( NULL );
   struct cache_entry* entry = cache->entries.head;
   while ( entry ) {
      struct cache_entry* next_entry = entry->next;
      enum { LIFETIME = 24 * 60 * 60 };
      if ( entry->mtime + LIFETIME < present_time ) {
         unlink_entry( &cache->entries, entry );
         append_entry( &cache->removed_entries, entry );
      }
      entry = next_entry;
   }
}

#include <unistd.h>

// Writes cache contents into permanent storage.
void write_cache( struct cache* cache ) {
   bool update_archive = false;
   // Remove files of removed entries.
   if ( cache->removed_entries.head ) {
      struct cache_entry* entry = cache->removed_entries.head;
      while ( entry ) {
         unlink( entry->cache_file_path.value );
         entry = entry->next;
      }
      update_archive = true;
   }
   // Write files of entries.
   struct cache_entry* entry = cache->entries.head;
   while ( entry ) {
      if ( entry->modified ) {
         write_entry( cache, entry );
         update_archive = true;
      }
      entry = entry->next;
   }
   // Write the file that manages the entries.
   if ( update_archive ) {
      write_archive( cache );
   }
}

void write_entry( struct cache* cache, struct cache_entry* entry ) {
   gbuf_reset( &cache->task->growing_buffer );
   cache_dump_module( cache->task, entry->lib, &cache->task->growing_buffer );
   struct str path;
   str_init( &path );
   append_entryfile_path( cache, entry, &path );
   write_buffer( path.value, &cache->task->growing_buffer );
   entry->modified = false;
}

void append_entryfile_path( struct cache* cache, struct cache_entry* entry,
   struct str* path ) {
   str_append( path, cache->path.value );
   str_append( path, "/" );
   str_append( path, "lib_" );
   char id_string[ 12 ];
   sprintf( id_string, "%d", entry->id );
   str_append( path, id_string );
   str_append( path, ".o" );
}

void write_archive( struct cache* cache ) {
   gbuf_reset( &cache->task->growing_buffer );
   cache_encode_archive( cache, &cache->task->growing_buffer );
   write_buffer( LIBCACHE_FILEPATH, &cache->task->growing_buffer ); 
}

void write_buffer( const char* path, struct gbuf* buffer ) {
   FILE* fh = fopen( path, "w" );
   if ( ! fh ) {
      return;
   }
   struct gbuf_seg* segment = buffer->head_segment;
   while ( segment ) {
      fwrite( segment->data, segment->used, 1, fh );
      segment = segment->next;
   }
   fclose( fh );
}

void print_cache( struct cache* cache ) {
   printf( "valid entries: \n" );
   cache_print_table( &cache->entries );
   printf( "------\n" );
   printf( "removed entries: \n" );
   cache_print_table( &cache->removed_entries );
   printf( "======\n" );
}

void cache_print_entry( struct cache_entry* entry ) {
   printf( "id=%d file_path=%s mtime=%zd\n", entry->id, entry->file_path.value, entry->mtime );
   if ( entry->head_dependency ) {
      struct cache_dependency* dep = entry->head_dependency;
      while ( dep ) {
         printf( "  dep file_path=%s mtime=%zd\n", dep->file_path.value, dep->mtime );
         dep = dep->next;
      }
   }
}

void cache_print_table( struct cache_entry_list* list ) {
   struct cache_entry* entry = list->head;
   while ( entry ) {
      cache_print_entry( entry );
      entry = entry->next;
   }
}

void cache_test( struct task* task, struct cache* cache ) {
   //struct cache cache;
   //cache_init( &cache, task );
   print_cache( cache );
   // cache_load( cache );
   struct parse parse;
   p_init( &parse, task, cache );
   p_read( &parse );
   struct semantic semantic;
   s_init( &semantic, task, task->library_main );
   s_test( &semantic );

/*
   struct gbuf buffer;
   gbuf_init( &buffer );
   cache_dump_module( task, task->library_main, &buffer );
   struct library* lib = cache_restore_module( task, gbuf_alloc_block( &buffer ) );
      printf( "%p\n", ( void* ) lib );
return;
*/
   //cache_add( &cache, task->library_main );
   //prune( &cache );
   //cache_close( &cache );
   //print_cache( &cache );
   //cache_dump( &cache );
   //cache_dump( &cache );
//cache->head_entry->lib = NULL;

      //struct library* lib = cache_get( &cache, task->library_main->file->full_path.value );
      //printf( "%p\n", ( void* ) task->library_main );
     // printf( "%p\n", ( void* ) lib );
// cache_print_table( &cache );
}