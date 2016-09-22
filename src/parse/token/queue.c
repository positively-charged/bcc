#include "phase.h"

void p_init_token_queue( struct token_queue* queue, bool stream ) {
   queue->head = NULL;
   queue->tail = NULL;
   queue->prev_entry = NULL;
   queue->size = 0;
   queue->stream = stream;
}

struct queue_entry* p_push_entry( struct parse* parse,
   struct token_queue* queue ) {
   // Allocate an entry.
   struct queue_entry* entry;
   if ( parse->tkque_free_entry ) {
      entry = parse->tkque_free_entry;
      parse->tkque_free_entry = entry->next;
   }
   else {
      entry = mem_alloc( sizeof( *entry ) );
   }
   // Initialize necessary fields of the entry.
   entry->next = NULL;
   entry->token_allocated = false;
   // Append the entry.
   if ( queue->head ) {
      queue->tail->next = entry;
   }
   else {
      queue->head = entry;
   }
   queue->tail = entry;
   ++queue->size;
   return entry;
}

struct token* p_shift_entry( struct parse* parse,
   struct token_queue* queue ) {
   // Free previous entry.
   if ( queue->prev_entry ) {
      struct queue_entry* entry = queue->prev_entry;
      entry->next = parse->tkque_free_entry;
      parse->tkque_free_entry = entry;
      if ( entry->token_allocated ) {
         p_free_token( parse, entry->token );
      }
   }
   struct queue_entry* entry = queue->head;
   queue->head = entry->next;
   queue->prev_entry = entry;
   --queue->size;
   return entry->token;
}