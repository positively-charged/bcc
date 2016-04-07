#include "cache.h"
#include "field.h"

// Save
// ==========================================================================

#define WF( saver, field ) \
   f_wf( saver->w, field )
#define WV( saver, field, value ) \
   f_wv( saver->w, field, value, sizeof( *( value ) ) )
#define WS( saver, field, value ) \
   f_ws( saver->w, field, value )

enum {
   F_ARCHIVE,
   F_CACHEDFILEID,
   F_DEPENDENCY,
   F_END,
   F_ENTRY,
   F_FILEPATH,
   F_MTIME,
};

struct saver {
   struct field_writer* w;
   struct cache* cache;
};

static void save_archive( struct saver* saver );
static void save_table( struct saver* saver );
static void save_entry( struct saver* saver, struct cache_entry* entry );
static void save_dependency_list( struct saver* saver,
   struct cache_entry* entry );

void cache_save_archive( struct cache* cache, struct gbuf* buffer ) {
   struct field_writer writer;
   f_init_writer( &writer, buffer );
   struct saver saver;
   saver.w = &writer;
   saver.cache = cache;
   save_archive( &saver );
}

void save_archive( struct saver* saver ) {
   WF( saver, F_ARCHIVE );
   save_table( saver );
   WF( saver, F_END );
}

void save_table( struct saver* saver ) {
   struct cache_entry* entry = saver->cache->entries.head;
   while ( entry ) {
      save_entry( saver, entry );
      entry = entry->next;
   }
}

void save_entry( struct saver* saver, struct cache_entry* entry ) {
   WF( saver, F_ENTRY );
   WS( saver, F_FILEPATH, entry->file_path.value );
   WV( saver, F_CACHEDFILEID, &entry->id );
   WV( saver, F_MTIME, &entry->mtime );
   save_dependency_list( saver, entry );
   WF( saver, F_END );
}

void save_dependency_list( struct saver* saver, struct cache_entry* entry ) {
   struct cache_dependency* dep = entry->head_dependency;
   while ( dep ) {
      WF( saver, F_DEPENDENCY );
      WS( saver, F_FILEPATH, dep->file_path.value );
      WF( saver, F_END );
      dep = dep->next;
   }
}

// Restore
// ==========================================================================

#define RF( restorer, field ) \
   f_rf( restorer->r, field )
#define RV( restorer, field, value ) \
   f_rv( restorer->r, field, value, sizeof( *( value ) ) )
#define RS( restorer, field ) \
   f_rs( restorer->r, field )

struct restorer {
   struct field_reader* r;
   struct cache* cache;
};

static void restore_archive( struct restorer* restorer );
static void restore_table( struct restorer* restorer );
static void restore_entry( struct restorer* restorer );
static void restore_dependency_list( struct restorer* restorer,
   struct cache_entry* entry );
static void restore_dependency( struct restorer* restorer,
   struct cache_entry* entry );

void cache_restore_archive( struct cache* cache, const char* saved_data ) {
   jmp_buf bail;
   if ( setjmp( bail ) == 0 ) {
      struct field_reader reader;
      f_init_reader( &reader, &bail, saved_data );
      struct restorer restorer;
      restorer.r = &reader;
      restorer.cache = cache;
      restore_archive( &restorer );
   }
}

void restore_archive( struct restorer* restorer ) {
   RF( restorer, F_ARCHIVE );
   restore_table( restorer );
   RF( restorer, F_END );
}

void restore_table( struct restorer* restorer ) {
   while ( f_peek( restorer->r ) == F_ENTRY ) {
      restore_entry( restorer );
   }
}

void restore_entry( struct restorer* restorer ) {
   RF( restorer, F_ENTRY );
   struct cache_entry* entry = cache_create_entry( restorer->cache,
      RS( restorer, F_FILEPATH ) );
   RV( restorer, F_CACHEDFILEID, &entry->id );
   RV( restorer, F_MTIME, &entry->mtime );
   restore_dependency_list( restorer, entry );
   RF( restorer, F_END );
}

void restore_dependency_list( struct restorer* restorer,
   struct cache_entry* entry ) {
   while ( f_peek( restorer->r ) == F_DEPENDENCY ) {
      restore_dependency( restorer, entry );
   }
}

void restore_dependency( struct restorer* restorer,
   struct cache_entry* entry ) {
   RF( restorer, F_DEPENDENCY );
   struct cache_dependency* dep = cache_alloc_dependency(
      RS( restorer, F_FILEPATH ) );
   cache_append_dependency( entry, dep );
   RF( restorer, F_END );
}