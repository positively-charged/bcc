#include "mem.h"

#define POINTER_SIZE sizeof( void* )
#define DEFAULT_REGION_SIZE 65536

typedef struct region_t {
   struct region_t* next;
   char* data;
   uint32_t used;
   uint32_t size;
} region_t;

typedef struct {
   region_t* region_head;
   region_t* region_free;
   region_t* region;
} pool_t;

static void init_pool( pool_t* );
static region_t* alloc_region( pool_t*, uint32_t );
static void* alloc_block( pool_t*, uint32_t );
static void free_regions( region_t* );

static pool_t g_pool;
static pool_t g_pool_temp;

void mem_init( void ) {
   init_pool( &g_pool );
   init_pool( &g_pool_temp );
}

void init_pool( pool_t* pool ) {
   pool->region_head = NULL;
   pool->region = NULL;
   alloc_region( pool, 0 );
}

region_t* alloc_region( pool_t* pool, uint32_t size ) {
   region_t* region = pool->region;
   if ( region && region->next && region->next->size > size ) {
      region = region->next;
      region->used = 0;
   }
   else {
      if ( size >= DEFAULT_REGION_SIZE ) {
         size *= 2;
      }
      else {
         size = DEFAULT_REGION_SIZE;
      }
      char* block = malloc( sizeof( region_t ) + size );
      region = ( region_t* ) block;
      region->data = block + sizeof( *region );
      region->used = 0;
      region->size = size;
      region->next = NULL;
      if ( pool->region ) {
         pool->region->next = region;
         pool->region = region;
      }
      else {
         pool->region_head = region;
         pool->region = region;
      }
   }
   return region;
}

void mem_deinit( void ) {
   free_regions( g_pool.region_head );
   free_regions( g_pool_temp.region_head );
}

void free_regions( region_t* region ) {
   while ( region ) {
      region_t* next = region->next;
      free( region );
      region = next;
   }
}

void* mem_alloc( uint32_t size ) {
   return alloc_block( &g_pool, size );
}

void* alloc_block( pool_t* pool, uint32_t size ) {
   // Align size.
   if ( size % POINTER_SIZE ) {
      size += POINTER_SIZE - size % POINTER_SIZE;
   }
   region_t* region = g_pool.region;
   if ( region->used + size > region->size ) {
      region = alloc_region( &g_pool, size );
   }
   void* block = region->data + region->used;
   region->used += size;
   return block;
}

void* mem_temp_alloc( uint32_t size ) {
   return alloc_block( &g_pool_temp, size );
}

void mem_temp_reset( void ) {
   g_pool_temp.region = g_pool.region_head;
   g_pool_temp.region->used = 0;
}