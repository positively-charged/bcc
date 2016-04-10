#ifndef SRC_CACHE_CACHE_H
#define SRC_CACHE_CACHE_H

#include "task.h"
#include "gbuf.h"
#include "field.h"

struct cache_entry {
   struct cache_entry* next;
   struct cache_dependency* dependency;
   struct cache_dependency* dependency_tail;
   struct library* lib;
   struct str path;
   time_t compile_time;
   int id;
   bool modified;
};

struct cache_dependency {
   struct cache_dependency* next;
   struct str path;
   time_t mtime;
};

struct cache_entry_list {
   struct cache_entry* head;
   struct cache_entry* tail;
};

struct cache {
   struct task* task;
   struct str dir_path;
   struct str header_id;
   struct cache_entry_list entries;
   struct cache_entry_list removed_entries;
   struct cache_dependency* free_dependencies;
   struct gbuf* buffer;
   int lifetime;
};

void cache_init( struct cache* cache, struct task* task );
void cache_load( struct cache* cache );
void cache_add( struct cache* cache, struct library* lib );
struct library* cache_get( struct cache* cache, struct file_entry* file );
void cache_close( struct cache* cache );
struct cache_entry* cache_alloc_entry( void );
void cache_append_entry( struct cache_entry_list* entries,
   struct cache_entry* entry );
struct cache_dependency* cache_alloc_dependency( struct cache* cache,
   const char* path );
void cache_append_dependency( struct cache_entry* entry,
   struct cache_dependency* dep );
void cache_clear( struct cache* cache );
void cache_save_archive( struct cache* cache, struct field_writer* writer );
void cache_restore_archive( struct cache* cache,
   struct field_reader* reader );
void cache_save_lib( struct task* task, struct field_writer* writer,
   struct library* lib );
struct library* cache_restore_lib( struct cache* cache,
   struct field_reader* reader );
void cache_print( struct cache* cache );

#endif