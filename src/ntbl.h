#ifndef A_NTBL_H
#define A_NTBL_H

#include <stdbool.h>

#include "str.h"

/*

   The name-lookup-table uses the Trie data structure. 

*/

typedef struct nkey_t {
   struct nkey_t* parent;
   struct nkey_t* next;
   struct nkey_t* drop;
   struct nval_t* value;
   short length;
   char ch;
} nkey_t;

typedef struct {
   nkey_t* start;
   nkey_t* name;
} nkey_iter_t;

typedef struct {
   nkey_t* top;
   str_t saved;
} ntbl_t;

void ntbl_init( ntbl_t* );
void ntbl_deinit( ntbl_t* );
// Operations:
//   Action: Finish.
//     !
//   Action: Access existing names only, don't create new names. It will abort
//     the navigation if the name in the path string does not exist. This
//     action persists until the end of the function call.
//     !a
//   Action: Length of characters to navigate in string. This is used to go
//   through a a substring.
//     !l <int>
//   Action: Save current name in given pointer. 
//     !n <name_t**>
//   Action: Start going from this name. The name must be valid.
//     !o <name_t*>
//   Action: From the current name, go to the name specified by the string. 
//     !s <const char*>
// Returns the last name stopped on, or NULL on failure.
nkey_t* ntbl_goto( ntbl_t*, ... );
nkey_iter_t ntbl_iter_init( nkey_t* start );
bool ntbl_iter_get( nkey_iter_t* );
void ntbl_show_key( const nkey_t* );
void ntbl_show_iter( nkey_iter_t* );
void ntbl_show_all_used( ntbl_t*, char initial_ch );
char* ntbl_save( nkey_t*, char* buff, int buff_size );
str_t* ntbl_read( ntbl_t*, nkey_t* ); 

#endif