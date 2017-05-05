#include <stdio.h>
#include <string.h>

#include "common.h"
#include "gbuf.h"

static void add_segment( struct gbuf* buffer );
static struct gbuf_seg* alloc_segment( void );
static size_t count_bytes_used( struct gbuf* buffer );

void gbuf_init( struct gbuf* buffer ) {
   buffer->head_segment = NULL;
   buffer->segment = NULL;
   add_segment( buffer );
}

static void add_segment( struct gbuf* buffer ) {
   if ( ! buffer->segment || ! buffer->segment->next ) {
      struct gbuf_seg* segment = alloc_segment();
      if ( buffer->head_segment ) {
         buffer->segment->next = segment;
      }
      else {
         buffer->head_segment = segment;
      }
      buffer->segment = segment;
   }
   else {
      buffer->segment = buffer->segment->next;
   }
}

static struct gbuf_seg* alloc_segment( void ) {
   struct gbuf_seg* segment = mem_alloc( sizeof( *segment ) );
   segment->next = NULL;
   segment->used = 0;
   segment->pos = 0;
   return segment;
}

void gbuf_write( struct gbuf* buffer, const void* data, int length ) {
   if ( GBUF_SEGMENT_SIZE - buffer->segment->pos < length ) {
      add_segment( buffer );
   }
   memcpy( buffer->segment->data + buffer->segment->pos, data, length );
   buffer->segment->pos += length;
   if ( buffer->segment->pos > buffer->segment->used ) {
      buffer->segment->used = buffer->segment->pos;
   }
}

char* gbuf_alloc_block( struct gbuf* buffer ) {
   char* block = mem_alloc( count_bytes_used( buffer ) );
   char* pos = block;
   struct gbuf_seg* segment = buffer->head_segment;
   while ( segment ) {
      memcpy( pos, segment->data, segment->used );
      pos += segment->used;
      segment = segment->next;
   }
   return block;
}

static size_t count_bytes_used( struct gbuf* buffer ) {
   size_t total = 0;
   struct gbuf_seg* segment = buffer->head_segment;
   while ( segment ) {
      total += segment->used;
      segment = segment->next;
   }
   return total;
}

void gbuf_reset( struct gbuf* buffer ) {
   buffer->segment = buffer->head_segment;
   struct gbuf_seg* segment = buffer->segment;
   while ( segment ) {
      segment->pos = 0;
      segment->used = 0;
      segment = segment->next;
   }
}

bool gbuf_save( struct gbuf* buffer, const char* file_path ) {
   FILE* fh = fopen( file_path, "wb" );
   if ( ! fh ) {
      return false;
   }
   struct gbuf_seg* segment = buffer->head_segment;
   while ( segment ) {
      size_t num_written = fwrite( segment->data,
         sizeof( segment->data[ 0 ] ), segment->used, fh );
      if ( num_written != segment->used ) {
         break;
      }
      segment = segment->next;
   }
   fclose( fh );
   return ( segment == NULL );
}
