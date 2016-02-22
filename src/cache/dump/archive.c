#include "cache/serial/write.h"
#include "cache/cache.h"

struct serial {
   struct serialw* w;
   struct cache* cache;
};

static void write_archive( struct serial* serial );
static void write_table( struct serial* serial );
static void write_entry( struct serial* serial, struct cache_entry* entry );
static void write_dependency_list( struct serial* serial,
   struct cache_entry* entry );

void cache_encode_archive( struct cache* cache, struct gbuf* buffer ) {
   struct serialw writing;
   srl_init_writing( &writing, buffer );
   struct serial serial;
   serial.w = &writing;
   serial.cache = cache;
   write_archive( &serial );
}

void write_archive( struct serial* serial ) {
   srlw_f( serial->w, F_ARCHIVE );
   write_table( serial );
   srlw_f( serial->w, F_END );
}

void write_table( struct serial* serial ) {
   struct cache_entry* entry = serial->cache->entries.head;
   while ( entry ) {
      write_entry( serial, entry );
      entry = entry->next;
   }
}

void write_entry( struct serial* serial, struct cache_entry* entry ) {
   srlw_f( serial->w, F_ENTRY );
   srlw_s( serial->w, F_STRFILEPATH, entry->file_path.value );
   srlw_i( serial->w, F_INTCACHEDFILEID, entry->id );
   srlw_t( serial->w, F_TIMEMTIME, entry->mtime );
   write_dependency_list( serial, entry );
   srlw_f( serial->w, F_END );
}

void write_dependency_list( struct serial* serial,
   struct cache_entry* entry ) {
   struct cache_dependency* dep = entry->head_dependency;
   while ( dep ) {
      srlw_f( serial->w, F_DEPENDENCY );
      srlw_s( serial->w, F_STRFILEPATH, dep->file_path.value );
      srlw_f( serial->w, F_END );
      dep = dep->next;
   }
}