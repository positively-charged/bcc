#ifndef LIB_LIST_H
#define LIB_LIST_H

#include <stdbool.h>

/*

   A linked list.

   TODO:

   - Implement remove() function.
   - Implement insert() function.
   - Maybe improve the iterator handling code.

*/

typedef struct list_node_t {
   struct list_node_t* next;
   void* data;
} list_node_t;

typedef struct {
   list_node_t* curr;
   list_node_t* prev;
   list_node_t* last;
} list_iterator_t;

typedef struct {
   list_node_t* last;
   int size;
} list_t;

void list_init( list_t* );
void list_append( list_t*, void* data );
void list_prepend( list_t*, void* data );
int list_size( list_t* );
// Deletes all the nodes from the list.
void list_clear( list_t* );
list_iterator_t list_iterate( list_t* );
// Checks whether the iterator has gone past the list. Do NOT use the iterator
// when it has reached this point.
bool list_end( list_iterator_t* );
// Don't call this function if the iterator is at the end of the list.
void list_next( list_iterator_t* );
void list_prev( list_iterator_t* );
// Removes the node in the iterator from the list. The iterator will be updated
// to reflect the change. Other iterators in use from the same list may NOT be
// valid after this operation.
void list_remove( list_iterator_t* );
// Retrieves the data pointer in the current node of the iterator.
void* list_idata( list_iterator_t* );
void* list_head( list_t* );
void* list_tail( list_t* );
// Removes an element from the front of the list, returning the removed
// element.
void* list_pop_head( list_t* );
void list_insert_before( list_t*, list_iterator_t*, void* data );

#endif