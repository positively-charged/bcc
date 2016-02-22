#include "cache/serial/read.h"
#include "cache/cache.h"

struct serial {
   struct serial_r* r;
   struct cache* cache;
};

static void read_archive( struct serial* serial );
static void read_table( struct serial* serial );
static void read_entry( struct serial* serial );
static void read_dependency_list( struct serial* serial,
   struct cache_entry* entry );
static void read_dependency( struct serial* serial,
   struct cache_entry* entry );

void cache_read_archive( struct cache* cache, const char* contents ) {
   jmp_buf exit;
   if ( setjmp( exit ) == 0 ) {
      struct serial_r reading;
      srl_init_reading( &reading, &exit, contents );
      struct serial serial;
      serial.cache = cache;
      serial.r = &reading;
      read_archive( &serial );
   }
}

void read_archive( struct serial* serial ) {
   srl_rf( serial->r, F_ARCHIVE );
   read_table( serial );
   srl_rf( serial->r, F_END );
}

void read_table( struct serial* serial ) {
   while ( srl_peekf( serial->r ) == F_ENTRY ) {
      read_entry( serial );
   }
}

void read_entry( struct serial* serial ) {
   srl_rf( serial->r, F_ENTRY );
   struct cache_entry* entry = cache_create_entry( serial->cache,
      srl_rs( serial->r, F_STRFILEPATH ) );
   entry->id = srl_ri( serial->r, F_INTCACHEDFILEID );
   entry->mtime = srl_rt( serial->r, F_TIMEMTIME );
   read_dependency_list( serial, entry );
   srl_rf( serial->r, F_END );
}

void read_dependency_list( struct serial* serial, struct cache_entry* entry ) {
   while ( srl_peekf( serial->r ) == F_DEPENDENCY ) {
      read_dependency( serial, entry );
   }
}

void read_dependency( struct serial* serial, struct cache_entry* entry ) {
   srl_rf( serial->r, F_DEPENDENCY );
   struct cache_dependency* dep = cache_alloc_dependency(
      srl_rs( serial->r, F_STRFILEPATH ) );
   cache_append_dependency( entry, dep );
   srl_rf( serial->r, F_END );
}