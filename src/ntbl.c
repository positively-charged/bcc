#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "ntbl.h"
#include "mem.h"

static nkey_t* new_key( char, nkey_t*, nkey_t*, short );

void ntbl_init( ntbl_t* table ) {
   table->top = NULL;
   str_init( &table->saved );
}

void ntbl_deinit( ntbl_t* table ) {
   str_del( &table->saved );
}

nkey_t* ntbl_goto( ntbl_t* table, ... ) {
   va_list args;
   va_start( args, table );
   nkey_t* parent = NULL;
   nkey_t* node = table->top;
   bool kill = false;
   bool access = false;
   while ( true ) {
      const char* ch = "";
      int length = -1;
      while ( true ) {
         const char* control = va_arg( args, const char* );
         // Skip initial starting delimiter. It exists to help the user
         // differentiate between the control and data segments of the query.
         ++control;
         if ( *control == 's' ) {
            ch = va_arg( args, const char* );
            break;
         }
         else if ( *control == 'o' ) {
            parent = *va_arg( args, nkey_t** );
            node = parent->drop;
         }
         else if ( *control == 'l' ) {
            length = va_arg( args, int );
         }
         else if ( *control == 'n' ) {
            nkey_t** save = va_arg( args, nkey_t** );
            *save = parent;
         }
         else if ( *control == 'a' ) {
            access = true;
         }
         // End.
         else {
            kill = true;
            break;
         }
      }
      // Navigate string path.
      short i = 0;
      while ( *ch && length ) {
         // Find the correct node to enter.
         nkey_t* prev = NULL;
         while ( node && node->ch < *ch ) {
            prev = node;
            node = node->next;
         }
         // Don't enter a new node if the user wants to access an existing
         // node.
         if ( access && ( ! node || node->ch != *ch ) ) {
            parent = NULL;
            kill = true;
            break;
         }
         ++i;
         // Enter a new node if no node could be found.
         if ( ! node ) {
            node = new_key( *ch, NULL, parent, i );
            // Intermediate node.
            if ( prev ) {
               node->next = prev->next;
               prev->next = node;
            }
            // Top node.
            else if ( ! parent ) {
               table->top = node;
            }
            // Top node of parent.
            else {
               parent->drop = node;
            }
         }
         // Enter a new node if no node with the same character exists in the
         // current parent node.
         else if ( node->ch > *ch ) {
            node = new_key( *ch, node, parent, i );
            if ( prev ) {
               prev->next = node;
            }
            else if ( ! parent ) {
               table->top = node;
            }
            else {
               parent->drop = node;
            }
         }
         parent = node;
         node = node->drop;
         ++ch;
         --length;
      }
      if ( kill ) { break; }
   }
   va_end( args );
   return parent;
}

nkey_t* new_key( char ch, nkey_t* next, nkey_t* parent, short length ) {
   nkey_t* key = mem_alloc( sizeof( *key ) );
   key->parent = parent;
   key->next = next;
   key->drop = NULL;
   key->value = NULL;
   key->ch = ch;
   key->length = length;
   return key;
}

nkey_iter_t ntbl_iter_init( nkey_t* start ) {
   nkey_iter_t i = { start, start };
   return i;
}

bool ntbl_iter_get( nkey_iter_t* iter ) {
   nkey_t* key = iter->name;
   while ( true ) {
      if ( key->drop ) {
         key = key->drop;
      }
      else {
         while ( key ) {
            if ( key == iter->start ) {
               iter->name = NULL;
               return false;
            }

            if ( key->next ) {
               key = key->next;
               break;
            }
            else {
               key = key->parent;
            }
         }
      }
      if ( ! key ) {
         return false;
      }
      else if ( key->value ) {
         iter->name = key;
         return true;
      }
   }
}

// debug
// --------------------------------------------------------------------------

static void show_name_term( nkey_t*, nkey_t* );

void ntbl_show_key( const nkey_t* key ) {
   #define BUFFER_SIZE 128
   char buffer[ BUFFER_SIZE + 1 ];
   buffer[ BUFFER_SIZE ] = 0;
   char* ch = buffer + BUFFER_SIZE;
   int left = BUFFER_SIZE;
   while ( key ) {
      if ( left ) {
         --ch;
         *ch = key->ch;
         key = key->parent;
         --left;
      }
      else {
         ntbl_show_key( key );
         break;
      }
   }
   printf( "%s", ch );
}

void ntbl_show_all_used( ntbl_t* table, char initial_ch ) {
   nkey_t* key = table->top;
   if ( initial_ch ) {
      while ( key && key->ch != initial_ch ) {
         key = key->next;
      }
   }
   while ( key ) {
      if ( initial_ch && ! key->parent && key->ch != initial_ch ) {
         break;
      }
      if ( key->value ) {
         ntbl_show_key( key );
         printf( "\n" );
      }
      if ( key->drop ) {
         key = key->drop;
      }
      else {
         while ( key ) {
            if ( key->next ) {
               key = key->next;
               break;
            }
            else {
               key = key->parent;
            }
         }
      }
   }
}

void ntbl_show_iter( nkey_iter_t* iter ) {
   show_name_term( iter->name, iter->start );
}

void show_name_term( nkey_t* key, nkey_t* end ) {
   #define BUFFER_SIZE 128
   char buffer[ BUFFER_SIZE + 1 ];
   buffer[ BUFFER_SIZE ] = 0;
   char* ch = buffer + BUFFER_SIZE;
   int left = BUFFER_SIZE;
   while ( key && key != end ) {
      if ( left ) {
         --ch;
         *ch = key->ch;
         key = key->parent;
         --left;
      }
      else {
         show_name_term( key, end );
         break;
      }
   }
   printf( "%s", ch );
}

char* ntbl_save( nkey_t* key, char* buff, int buff_size ) {
   buff[ --buff_size ] = 0;
   while ( key && buff_size ) {
      buff[ --buff_size ] = key->ch;
      key = key->parent;
   }
   return &buff[ buff_size ];
}

str_t* ntbl_read( ntbl_t* table, nkey_t* key ) {
   str_t* saved = &table->saved;
   if ( saved->buff_length < key->length + 1 ) {
      str_grow( saved, key->length );
   }
   saved->length = key->length;
   char* ch = saved->value + key->length;
   *ch = 0;
   --ch;
   while ( key ) {
      *ch = key->ch;
      key = key->parent;
      --ch;
   }
   return saved;
}