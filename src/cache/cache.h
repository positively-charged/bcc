#ifndef SRC_CACHE_CACHE_H
#define SRC_CACHE_CACHE_H

#include "task.h"
#include "gbuf.h"

#define LIBCACHE_FILEPATH "/tmp/bcc_cache/archive.o"

struct cache_entry {
   struct cache_entry* next;
   struct cache_dependency* head_dependency;
   struct cache_dependency* dependency;
   struct str file_path;
   struct str cache_file_path;
   struct library* lib;
   time_t mtime;
   unsigned int id;
   bool modified;
};

struct cache_dependency {
   struct cache_dependency* next;
   struct str file_path;
   time_t mtime;
};

struct cache_entry_list {
   struct cache_entry* head;
   struct cache_entry* tail;
};

struct cache {
   struct task* task;
   struct str path;
   struct cache_entry_list entries;
   struct cache_entry_list removed_entries;
   struct gbuf* buffer;
   int id;
};

void cache_init( struct cache* cache, struct task* task );
void cache_load( struct cache* cache );
void cache_entryfile_path( int id, struct str* path );
void cache_add( struct cache* cache, struct library* lib );
struct library* cache_get( struct cache* cache, struct file_entry* file );
void cache_close( struct cache* cache );
struct cache_entry* cache_create_entry( struct cache* cache,
   const char* path );
struct cache_dependency* cache_alloc_dependency( const char* file_path );
void cache_append_dependency( struct cache_entry* entry,
   struct cache_dependency* dep );
void cache_clear( struct cache* cache );
void cache_save_archive( struct cache* cache, struct gbuf* buffer );
void cache_restore_archive( struct cache* cache, const char* saved_data );
void cache_save_lib( struct task* task, struct library* lib,
   struct gbuf* buffer );
struct library* cache_restore_lib( struct task* task,
   const char* encoded_text );

// Debugging.
void cache_print_entry( struct cache_entry* entry );
void cache_print_table( struct cache_entry_list* list );
void cache_test( struct task* task, struct cache* cache );

#endif