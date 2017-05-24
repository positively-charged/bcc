#ifndef SRC_GBUF
#define SRC_GBUF

/*

   Growing buffer.

*/

#include <stdbool.h>

enum { GBUF_SEGMENT_SIZE = 65536 };

// Segment.
struct gbuf_seg {
   struct gbuf_seg* next;
   char data[ GBUF_SEGMENT_SIZE ];
   int used;
   int pos;
};

// Growing buffer.
struct gbuf {
   struct gbuf_seg* head_segment;
   struct gbuf_seg* segment;
};

void gbuf_init( struct gbuf* buffer );
void gbuf_write( struct gbuf* buffer, const void* data, int length );
char* gbuf_alloc_block( struct gbuf* buffer );
void gbuf_reset( struct gbuf* buffer );
bool gbuf_save( struct gbuf* buffer, const char* file_path );

#endif
