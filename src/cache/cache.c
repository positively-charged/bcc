#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#include "cache.h"

// Default lifetime: 24 hours.
enum { DEFAULT_LIFETIME = 24 };

// NOTE: The order of the fields is important. Add a new field only to the
// bottom of the enumeration.
enum {
   F_HEADER,
   F_ID,
   F_END,
};

struct restore_request {
   struct str* path;
   struct library* lib;
   enum {
      RESTORE_ARCHIVE,
      RESTORE_LIB,
   } type;
};

static void generate_header_id( struct str* id );
static void init_cache_entry_list( struct cache_entry_list* entries );
static void prepare_dir( struct cache* cache );
static void prepare_tempdir( struct cache* cache );
static void restore_archive( struct cache* cache );
static void append_archive_path( struct cache* cache, struct str* path );
static void init_restore_request( struct restore_request* request, int type,
   struct str* path );
static void restore( struct cache* cache, struct restore_request* request );
static void restore_file( struct cache* cache, struct restore_request* request,
   struct field_reader* reader );
static int generate_id( struct cache* cache );
static void append_entry_sorted( struct cache_entry_list* entries,
   struct cache_entry* entry );
static struct cache_entry* find_entry( struct cache* cache, const char* path );
static bool timestamp_entry( struct cache_entry* entry );
static bool fresh_entry( struct cache_entry* entry );
static bool restore_lib( struct cache* cache, struct cache_entry* entry );
static void append_entry_path( struct cache* cache, struct cache_entry* entry,
   struct str* path );
static void unlink_entry( struct cache_entry_list* entries,
   struct cache_entry* selected_entry );
static bool lifetime_enabled( struct cache* cache );
static void remove_outdated_entries( struct cache* cache );
static time_t lifetime_seconds( struct cache* cache );
static void save_cache( struct cache* cache );
static void save_lib( struct cache* cache, struct cache_entry* entry );
static void save_header( struct cache* cache, struct field_writer* writer );
static void save_archive( struct cache* cache );
static void print_entry( struct cache* cache, struct cache_entry* entry );
static void print_lifetime( struct cache* cache, struct cache_entry* entry );

void cache_init( struct cache* cache, struct task* task ) {
   cache->task = task;
   str_init( &cache->dir_path );
   str_init( &cache->header_id );
   generate_header_id( &cache->header_id );
   init_cache_entry_list( &cache->entries );
   init_cache_entry_list( &cache->removed_entries );
   cache->free_dependencies = NULL;
   cache->buffer = NULL;
   if ( task->options->cache.lifetime >= 0 ) {
      cache->lifetime = task->options->cache.lifetime;
   }
   else {
      cache->lifetime = DEFAULT_LIFETIME;
   }
}

// NOTE: The ID is only regenerated when this file is compiled. Compiling the
// other source files of the cache will not cause the ID to be regenerated.
void generate_header_id( struct str* id ) {
   char text[] = __DATE__ " " __TIME__;
   int i = 0;
   int length = 0;
   while ( text[ i ] != '\0' ) {
      if ( isalpha( text[ i ] ) ) {
         text[ length ] = toupper( text[ i ] );
         ++length;
      }
      else if ( isdigit( text[ i ] ) ) {
         text[ length ] = text[ i ];
         ++length;
      }
      ++i;
   }
   str_append_sub( id, text, length );
}

void init_cache_entry_list( struct cache_entry_list* entries ) {
   entries->head = NULL;
   entries->tail = NULL;
}

void cache_load( struct cache* cache ) {
   prepare_dir( cache );
   restore_archive( cache );
}

void prepare_dir( struct cache* cache ) {
   if ( cache->task->options->cache.dir_path ) {
      str_append( &cache->dir_path, cache->task->options->cache.dir_path );
      fs_strip_trailing_pathsep( &cache->dir_path );
   }
   else {
      prepare_tempdir( cache );
   }
}

void prepare_tempdir( struct cache* cache ) {
   // Use temporary directory of the system.
   str_append( &cache->dir_path, fs_get_tempdir() );
   str_append( &cache->dir_path, OS_PATHSEP );
   str_append( &cache->dir_path, "bcc_cache" );
   struct fs_query query;
   fs_init_query( &query, cache->dir_path.value );
   if ( ! fs_exists( &query ) ) {
      struct fs_result result;
      if ( ! fs_create_dir( cache->dir_path.value, &result ) ) {
         t_diag( cache->task, DIAG_ERR,
            "failed to create cache directory: %s (%s)",
            cache->dir_path.value, strerror( result.err ) );
         t_bail( cache->task );
      }
   }
}

void restore_archive( struct cache* cache ) {
   struct str path;
   str_init( &path );
   append_archive_path( cache, &path );
   struct restore_request request;
   init_restore_request( &request, RESTORE_ARCHIVE, &path );
   restore( cache, &request );
   str_deinit( &path );
}

void append_archive_path( struct cache* cache, struct str* path ) {
   str_append( path, cache->dir_path.value );
   str_append( path, OS_PATHSEP );
   str_append( path, "archive.o" );
}

void init_restore_request( struct restore_request* request, int type,
   struct str* path ) {
   request->path = path;
   request->lib = NULL;
   request->type = type;
}

void restore( struct cache* cache, struct restore_request* request ) {
   struct file_contents contents;
   fs_get_file_contents( request->path->value, &contents );
   if ( ! contents.obtained ) {
      if ( contents.err != ENOENT ) {
         t_diag( cache->task, DIAG_ERR,
            "failed to read cache file: %s (%s)",
            request->path->value, strerror( contents.err ) );
         t_bail( cache->task );
      }
      return;
   }
   jmp_buf bail;
   struct field_reader reader;
   f_init_reader( &reader, &bail, contents.data );
   if ( setjmp( bail ) == 0 ) {
      restore_file( cache, request, &reader );
   }
   else {
      t_diag( cache->task, DIAG_NONE,
         "%s: internal error: unexpected field: expecting %d, but got %d",
         request->path->value, reader.expected_field, reader.field );
      t_bail( cache->task );
   }
   mem_free( contents.data );
}

void restore_file( struct cache* cache, struct restore_request* request,
   struct field_reader* reader ) {
   f_rf( reader, F_HEADER );
   const char* id = f_rs( reader, F_ID );
   if ( strcmp( id, cache->header_id.value ) != 0 ) {
      return;
   }
   f_rf( reader, F_END );
   switch ( request->type ) {
   case RESTORE_ARCHIVE:
      cache_restore_archive( cache, reader );
      break;
   case RESTORE_LIB:
      request->lib = cache_restore_lib( cache, reader );
      break;
   default:
      UNREACHABLE();
   }
}

// Saves a library in the cache.
void cache_add( struct cache* cache, struct library* lib ) {
   struct cache_entry* entry = find_entry( cache, lib->file->full_path.value );
   // Create new entry.
   if ( ! entry ) {
      entry = cache_alloc_entry();
      str_append( &entry->path, lib->file->full_path.value );
      entry->id = generate_id( cache );
      append_entry_sorted( &cache->entries, entry );
   }
   // Free previous dependencies.
   if ( entry->dependency ) {
      entry->dependency_tail->next = cache->free_dependencies;
      cache->free_dependencies = entry->dependency;
      entry->dependency = NULL;
   }
   // Update entry.
   list_iter_t i;
   list_iter_init( &i, &lib->files );
   while ( ! list_end( &i ) ) {
      struct file_entry* file = list_data( &i );
      struct cache_dependency* dep = cache_alloc_dependency( cache,
         file->full_path.value );
      cache_append_dependency( entry, dep );
      list_next( &i );
   }
   list_iter_init( &i, &lib->import_dircs );
   while ( ! list_end( &i ) ) {
      struct import_dirc* dirc = list_data( &i );
      struct file_query query;
      t_init_file_query( &query, cache->task->library_main->file,
         dirc->file_path );
      t_find_file( cache->task, &query );
      if ( ! query.file ) {
         t_diag( cache->task, DIAG_POS_ERR, &dirc->pos,
            "failed to generate cache dependency because library file could "
            "not be found" );
         t_bail( cache->task );
      }
      struct cache_dependency* dep = cache_alloc_dependency( cache,
         query.file->full_path.value );
      cache_append_dependency( entry, dep );
      list_next( &i );
   }
   entry->lib = lib;
   entry->compile_time = cache->task->compile_time;
   entry->modified = true;
}

struct cache_entry* cache_alloc_entry( void ) {
   struct cache_entry* entry = mem_alloc( sizeof( *entry ) );
   entry->next = NULL;
   entry->dependency = NULL;
   entry->dependency_tail = NULL;
   entry->lib = NULL;
   str_init( &entry->path );
   entry->compile_time = 0;
   entry->id = 0;
   entry->modified = false;
   return entry;
}

int generate_id( struct cache* cache ) {
   // Search for an unused ID between entries.
   int id = 0;
   struct cache_entry* entry = cache->entries.head;
   while ( entry && id == entry->id ) {
      ++id;
      entry = entry->next;
   }
   return id;
}

// If @prev_entry is NULL, @entry will be inserted at the end of the list.
void cache_append_entry( struct cache_entry_list* entries,
   struct cache_entry* entry ) {
   if ( entries->head ) {
      entries->tail->next = entry;
   }
   else {
      entries->head = entry;
   }
   entries->tail = entry;
}

// Appends the entry into the list, sorted by ID.
void append_entry_sorted( struct cache_entry_list* entries,
   struct cache_entry* latest_entry ) {
   struct cache_entry* prev_entry = NULL;
   struct cache_entry* entry = entries->head;
   while ( entry && entry->id < latest_entry->id ) {
      prev_entry = entry;
      entry = entry->next;
   }
   if ( prev_entry ) {
      latest_entry->next = prev_entry->next;
      prev_entry->next = latest_entry;
   }
   else {
      latest_entry->next = entries->head;
      entries->head = latest_entry;
   }
   if ( ! entry ) {
      entries->tail = latest_entry;
   }
}

struct cache_dependency* cache_alloc_dependency( struct cache* cache,
   const char* path ) {
   struct cache_dependency* dep;
   if ( cache->free_dependencies ) {
      dep = cache->free_dependencies;
      cache->free_dependencies = dep->next;
      str_clear( &dep->path );
   }
   else {
      dep = mem_alloc( sizeof( *dep ) );
      str_init( &dep->path );
   }
   dep->next = NULL;
   str_append( &dep->path, path );
   dep->mtime = 0;
   return dep;
}

void cache_append_dependency( struct cache_entry* entry,
   struct cache_dependency* dep ) {
   if ( entry->dependency ) {
      entry->dependency_tail->next = dep;
   }
   else {
      entry->dependency = dep;
   }
   entry->dependency_tail = dep;
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
   // Load only once the contents of a cached library.
   if ( ! entry->lib ) {
      if ( timestamp_entry( entry ) && fresh_entry( entry ) &&
         restore_lib( cache, entry ) ) {
         entry->lib->file = file;
      }
   }
   return entry->lib;
}

struct cache_entry* find_entry( struct cache* cache, const char* path ) {
   struct cache_entry* entry = cache->entries.head;
   while ( entry && strcmp( entry->path.value, path ) != 0 ) {
      entry = entry->next;
   }
   return entry;
}

bool timestamp_entry( struct cache_entry* entry ) {
   struct cache_dependency* dep = entry->dependency;
   while ( dep ) {
      struct fs_query query;
      fs_init_query( &query, dep->path.value );
      struct fs_timestamp timestamp;
      if ( ! fs_get_mtime( &query, &timestamp ) ) {
         return false;
      }
      dep->mtime = timestamp.value;
      dep = dep->next;
   }
   return true;
}

bool fresh_entry( struct cache_entry* entry ) {
   struct cache_dependency* dep = entry->dependency;
   while ( dep && dep->mtime <= entry->compile_time ) {
      dep = dep->next;
   }
   return ( dep == NULL );
}

bool restore_lib( struct cache* cache, struct cache_entry* entry ) {
   struct str path;
   str_init( &path );
   append_entry_path( cache, entry, &path );
   struct restore_request request;
   init_restore_request( &request, RESTORE_LIB, &path );
   restore( cache, &request );
   entry->lib = request.lib;
   return ( entry->lib != NULL );
}

void append_entry_path( struct cache* cache, struct cache_entry* entry,
   struct str* path ) {
   str_append( path, cache->dir_path.value );
   str_append( path, OS_PATHSEP );
   str_append( path, "lib" );
   char id[ 12 ];
   snprintf( id, sizeof( id ), "%d", entry->id );
   str_append( path, id );
   str_append( path, ".o" );
}

// Moves every entry into the removed-entry list.
void cache_clear( struct cache* cache ) {
   while ( cache->entries.head ) {
      struct cache_entry* entry = cache->entries.head;
      unlink_entry( &cache->entries, entry );
      cache_append_entry( &cache->removed_entries, entry );
   }
}

void unlink_entry( struct cache_entry_list* entries,
   struct cache_entry* selected_entry ) {
   struct cache_entry* prev_entry = NULL;
   struct cache_entry* entry = entries->head;
   while ( entry != selected_entry ) {
      prev_entry = entry;
      entry = entry->next;
   }
   if ( prev_entry ) {
      prev_entry->next = selected_entry->next;
      if ( ! prev_entry->next ) {
         entries->tail = prev_entry;
      }
   }
   else {
      entries->head = selected_entry->next;
      if ( ! entries->head ) {
         entries->tail = NULL;
      }
   }
   selected_entry->next = NULL;
}

// Performs final operations of the cache:
// - Remove outdated entries.
// - Remove cache file of outdated entries.
// - Write valid entries into permanent storage.
void cache_close( struct cache* cache ) {
   if ( lifetime_enabled( cache ) ) {
      remove_outdated_entries( cache );
   }
   save_cache( cache );
}

bool lifetime_enabled( struct cache* cache ) {
   return ( cache->lifetime > 0 );
}

void remove_outdated_entries( struct cache* cache ) {
   struct cache_entry* entry = cache->entries.head;
   while ( entry ) {
      struct cache_entry* next_entry = entry->next;
      time_t expire_time = entry->compile_time + lifetime_seconds( cache );
      if ( cache->task->compile_time >= expire_time ) {
         unlink_entry( &cache->entries, entry );
         cache_append_entry( &cache->removed_entries, entry );
      }
      entry = next_entry;
   }
}

time_t lifetime_seconds( struct cache* cache ) {
   return ( 60 * 60 * cache->lifetime );
}

// Writes cache contents into permanent storage.
void save_cache( struct cache* cache ) {
   bool archive_updated = false;
   // Remove files of removed entries.
   struct cache_entry* entry = cache->removed_entries.head;
   while ( entry ) {
      struct str path;
      str_init( &path );
      append_entry_path( cache, entry, &path );
      // TODO: Check for file errors.
      fs_delete_file( path.value );
      archive_updated = true;
      str_deinit( &path );
      entry = entry->next;
   }
   // Write files of entries.
   entry = cache->entries.head;
   while ( entry ) {
      if ( entry->modified ) {
         save_lib( cache, entry );
         archive_updated = true;
      }
      entry = entry->next;
   }
   // Write the file that stores the entries.
   if ( archive_updated ) {
      save_archive( cache );
   }
}

void save_lib( struct cache* cache, struct cache_entry* entry ) {
   struct field_writer writer;
   gbuf_reset( &cache->task->growing_buffer );
   f_init_writer( &writer, &cache->task->growing_buffer );
   save_header( cache, &writer );
   cache_save_lib( cache->task, &writer, entry->lib );
   struct str path;
   str_init( &path );
   append_entry_path( cache, entry, &path );
   // TODO: Check for file errors.
   gbuf_save( &cache->task->growing_buffer, path.value );
   entry->modified = false;
   str_deinit( &path );
}

void save_header( struct cache* cache, struct field_writer* writer ) {
   f_wf( writer, F_HEADER );
   f_ws( writer, F_ID, cache->header_id.value );
   f_wf( writer, F_END );
}

void save_archive( struct cache* cache ) {
   struct field_writer writer;
   gbuf_reset( &cache->task->growing_buffer );
   f_init_writer( &writer, &cache->task->growing_buffer );
   save_header( cache, &writer );
   cache_save_archive( cache, &writer );
   struct str path;
   str_init( &path );
   append_archive_path( cache, &path );
   // TODO: Check for file errors.
   gbuf_save( &cache->task->growing_buffer, path.value );
   str_deinit( &path );
}

void cache_print( struct cache* cache ) {
   printf( "directory=%s\n", cache->dir_path.value );
   if ( lifetime_enabled( cache ) ) {
      printf( "lifetime=%dh\n", cache->lifetime );
   }
   else {
      printf( "lifetime=infinite\n" );
   }
   int total = 0;
   struct cache_entry* entry = cache->entries.head;
   while ( entry ) {
      ++total;
      entry = entry->next;
   }
   printf( "total-libraries=%d\n", total );
   entry = cache->entries.head;
   while ( entry ) {
      print_entry( cache, entry );
      entry = entry->next;
   }
}

void print_entry( struct cache* cache, struct cache_entry* entry ) {
   printf( "library=%s\n", entry->path.value );
   struct str path;
   str_init( &path );
   append_entry_path( cache, entry, &path );
   printf( "  cache-file=%s\n", path.value );
   str_deinit( &path );
   struct tm* tm_time = localtime( &entry->compile_time );
   char time_text[ 100 ];
   strftime( time_text, sizeof( time_text ), "%Y-%m-%d %H:%M:%S", tm_time );
   printf( "  compilation-time=%s\n", time_text );
   if ( lifetime_enabled( cache ) ) {
      print_lifetime( cache, entry );
   }
   printf( "  dependencies=\n" );
   struct cache_dependency* dep = entry->dependency;
   while ( dep ) {
      printf( "    file=%s\n", dep->path.value );
      dep = dep->next;
   }
}

void print_lifetime( struct cache* cache, struct cache_entry* entry ) {
   time_t expire_time = entry->compile_time + lifetime_seconds( cache );
   struct tm* tm_time = localtime( &expire_time );
   char time_text[ 100 ];
   strftime( time_text, sizeof( time_text ), "%Y-%m-%d %H:%M:%S", tm_time );
   printf( "  expiration-time=%s\n", time_text );
   time_t time_left = expire_time - cache->task->compile_time;
   if ( time_left > 0 ) {
      printf( "  time-left=%ldh,%ldm,%lds\n",
         ( long ) ( time_left / 60 / 60 ),
         ( long ) ( time_left / 60 % 60 ),
         ( long ) ( time_left % 60 ) );
   }
   else {
      printf( "  time-left=none (entry will be removed)\n" );
   }
}