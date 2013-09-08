#include "mem.h"
#include "list.h"

#include <stdio.h>

/*

   Implementation details:

   The next-node of the last-node refers to the root-node. This way we can
   refer both to the first and last nodes in the list. This makes the list
   circular, so special care needs to be taken when traversing this list.

*/

static list_node_t* new_node( void* );

void list_init( list_t* list ) {
   list->last = NULL;
   list->size = 0;
}

void list_append( list_t* list, void* data ) {
   list_node_t* node = new_node( data );
   if ( list->last ) {
      // Transfer the root node to the new node.
      node->next = list->last->next;
      // Link the new node with the last node. Even if the last node is the
      // root node, the next-node field of the root node will be updated
      // correctly. The new node will become the second node in the list.
      list->last->next = node;
      // The new node is now the last node.
      list->last = node;
   }
   else {
      // The first node is both the root and the last node. 
      node->next = node;
      list->last = node;
   }
   ++list->size;
}

void list_prepend( list_t* list, void* data ) {
   list_node_t* node = new_node( data );
   if ( list->last ) {
      // The old root node becomes the second node in the list.
      node->next = list->last->next;
      // The new node becomes the root node.
      list->last->next = node;
   }
   else {
      node->next = node;
      list->last = node;
   }
   ++list->size;
}

int list_size( list_t* list ) {
   return list->size;
}

void list_clear( list_t* list ) {
   if ( ! list->last ) {
      return;
   }

   list_node_t* node = list->last;
   while ( list->size ) {
      list_node_t* next_node = node->next;
      // free( node );
      node = next_node;
      --list->size;
   }

   list->last = 0;
}

list_iterator_t list_iterate( list_t* list ) {
   list_iterator_t i = { NULL, NULL, NULL };
   if ( list->last ) {
      i.curr = list->last->next;
      i.last = list->last;
   }
   return i;
}

bool list_end( list_iterator_t* iter ) {
   return ( ! iter->last ); 
}

void list_next( list_iterator_t* iter ) {
   iter->prev = iter->curr;
   if ( iter->curr == iter->last ) {
      iter->last = 0;
   }
   else {
      iter->curr = iter->curr->next;
   }
}

void* list_idata( list_iterator_t* i ) {
   return i->curr->data;
}

list_node_t* new_node( void* data ) {
   list_node_t* node = mem_alloc( sizeof( list_node_t ) );
   node->data = data;
   node->next = NULL;
   return node;
}

void* list_head( list_t* list ) {
   if ( list->last ) {
      return list->last->next->data;
   }
   else {
      return NULL;
   }
}

void* list_tail( list_t* list ) {
   if ( list->last ) {
      return list->last->data;
   }
   else {
      return NULL;
   }
}

// Removes an element from the front of the list, returning the removed
// element.
void* list_pop_head( list_t* list ) {
   if ( ! list->last ) {
      return NULL;
   }
   list_node_t* root = list->last->next;
   if ( root == list->last ) {
      list->last = NULL;
   }
   else {
      list->last->next = root->next;
   }
   void* data = root->data;
   --list->size;
   return data;
}

void list_insert_before( list_t* list, list_iterator_t* iter, void* data ) {
   list_node_t* node = new_node( data );
   node->next = iter->curr;
   // Add before root node.
   if ( ! iter->prev ) {
      list->last->next = node;
   }
   // Add before an intermediate node. It could be the last node.
   else {
      iter->prev->next = node;
   }
   ++list->size;
}

/*

void hold_remove( hold_group_t* group, hold_iterator_t* i ) {
   // Remove last node.
   if ( i->curr == group->last ) {
      hold_link_t* root = i->curr->next;
      // Move the root node back to the new last node.
      if ( i->prev != root ) {
         i->prev->next = root;
         group->last = i->prev;
      }
      // Reset the list when no more nodes are available.
      else {
         group->last = 0;
      }
   }
   // Remove root node.
   else if ( i->curr == group->last->next ) {
      group->last->next = group->last->next->next;
   }
   // Remove node in the middle.
   else {
      i->prev->next = i->curr->next;
   }

   // Update position and remove node.
   // delete pos->curr;
}
*/