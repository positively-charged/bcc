#include <string.h>

#include "phase.h"

static struct library* add_library( struct task* );
static void read_region_body( struct parse* phase );
static void read_dirc( struct parse* phase, struct pos* );
static void read_include( struct parse* phase, struct pos*, bool );
static void read_library( struct parse* phase, struct pos* );
static void read_define( struct parse* phase );
static void link_usable_strings( struct parse* phase );
static void add_usable_string( struct indexed_string**,
   struct indexed_string**, struct indexed_string* );
static void alloc_string_indexes( struct parse* phase );

void p_init( struct parse* phase, struct task* task ) {
   phase->task = task;
   phase->tokens.peeked = 0;
   phase->tk = TK_END;
   phase->tk_text = "";
   phase->tk_length = 0;
   phase->source = NULL;
   phase->main_source = NULL;
   phase->region_upmost = task->region_upmost;
   phase->region = phase->region_upmost;
   phase->options = task->options;
   list_init( &phase->loaded_sources );
   phase->last_id = 0;
}

void p_read( struct parse* phase ) {
   p_make_main_lib( phase );
   p_load_main_source( phase );
   p_read_tk( phase );
   p_read_lib( phase );
   link_usable_strings( phase );
   alloc_string_indexes( phase );
}

void p_make_main_lib( struct parse* phase ) {
   phase->task->library = add_library( phase->task );
   phase->task->library_main = phase->task->library;
}

struct library* add_library( struct task* task ) {
   struct library* lib = mem_alloc( sizeof( *lib ) );
   str_init( &lib->name );
   str_copy( &lib->name, "", 0 );
   list_init( &lib->vars );
   list_init( &lib->funcs );
   list_init( &lib->scripts );
   list_init( &lib->dynamic );
   lib->file_pos.line = 0;
   lib->file_pos.column = 0;
   lib->file_pos.id = 0;
   lib->id = list_size( &task->libraries );
   lib->format = FORMAT_LITTLE_E;
   lib->importable = false;
   lib->imported = false;
   if ( task->library_main ) {
      struct library* last_lib = list_tail( &task->libraries );
      lib->hidden_names = t_make_name( task, "a.", last_lib->hidden_names );
   }
   else {
      lib->hidden_names = t_make_name( task, "!hidden.", task->root_name );
   }
   lib->encrypt_str = false;
   list_append( &task->libraries, lib );
   return lib;
}

void p_read_lib( struct parse* phase ) {
   while ( true ) {
      if ( p_is_dec( phase ) ) {
         struct dec dec;
         p_init_dec( &dec );
         dec.name_offset = phase->region_upmost->body;
         p_read_dec( phase, &dec );
      }
      else if ( phase->tk == TK_SCRIPT ) {
         if ( phase->task->library->imported ) {
            p_skip_block( phase );
         }
         else {
            p_read_script( phase );
         }
      }
      else if ( phase->tk == TK_REGION ) {
         p_read_region( phase );
      }
      else if ( phase->tk == TK_IMPORT ) {
         p_read_import( phase, NULL );
      }
      else if ( phase->tk == TK_SEMICOLON ) {
         p_read_tk( phase );
      }
      else if ( phase->tk == TK_HASH ) {
         struct pos pos = phase->tk_pos;
         p_read_tk( phase );
         read_dirc( phase, &pos );
      }
      else if ( phase->tk == TK_END ) {
         break;
      }
      else {
         t_diag( phase->task, DIAG_POS_ERR, &phase->tk_pos,
            "unexpected %s", p_get_token_name( phase->tk ) );
         t_bail( phase->task );
      }
   }
   // Library must have the #library directive.
   if ( phase->task->library->imported && ! phase->task->library->name.length ) {
      t_diag( phase->task, DIAG_ERR | DIAG_FILE, &phase->tk_pos,
         "#imported file missing #library directive" );
      t_bail( phase->task );
   }
}

void p_read_region( struct parse* phase ) {
   p_test_tk( phase, TK_REGION );
   p_read_tk( phase );
   struct region* parent = phase->region;
   while ( true ) {
      p_test_tk( phase, TK_ID );
      struct name* name = t_make_name( phase->task, phase->tk_text,
         phase->region->body );
      if ( name->object ) {
         // It is assumed the object will always be a region.
         phase->region = ( struct region* ) name->object;
      }
      else {
         struct region* region = t_alloc_region( phase->task, name, false );
         region->object.pos = phase->tk_pos;
         region->object.resolved = true;
         name->object = &region->object;
         list_append( &phase->task->regions, region );
         phase->region = region;
      }
      p_read_tk( phase );
      if ( phase->tk == TK_COLON_2 ) {
         p_read_tk( phase );
      }
      else {
         break;
      }
   }
   p_test_tk( phase, TK_BRACE_L );
   p_read_tk( phase );
   read_region_body( phase );
   p_test_tk( phase, TK_BRACE_R );
   p_read_tk( phase );
   phase->region = parent;
}

void read_region_body( struct parse* phase ) {
   while ( true ) {
      if ( p_is_dec( phase ) ) {
         struct dec dec;
         p_init_dec( &dec );
         dec.name_offset = phase->region->body;
         p_read_dec( phase, &dec );
      }
      else if ( phase->tk == TK_SCRIPT ) {
         if ( ! phase->task->library->imported ) {
            p_read_script( phase );
         }
         else {
            p_skip_block( phase );
         }
      }
      else if ( phase->tk == TK_REGION ) {
         p_read_region( phase );
      }
      else if ( phase->tk == TK_IMPORT ) {
         p_read_import( phase, NULL );
      }
      else if ( phase->tk == TK_SEMICOLON ) {
         p_read_tk( phase );
      }
      else if ( phase->tk == TK_BRACE_R ) {
         break;
      }
      else {
         t_diag( phase->task, DIAG_POS_ERR, &phase->tk_pos,
            "unexpected %s", p_get_token_name( phase->tk ) );
         t_bail( phase->task );
      }
   }
}

void read_dirc( struct parse* phase, struct pos* pos ) {
   // Directives can only appear in the upmost region.
   if ( phase->region != phase->region_upmost ) {
      t_diag( phase->task, DIAG_POS_ERR, pos, "directive not in upmost region" );
      t_bail( phase->task );
   }
   if ( phase->tk == TK_IMPORT ) {
      p_read_tk( phase );
      if ( phase->source->imported ) {
         p_test_tk( phase, TK_LIT_STRING );
         p_read_tk( phase );
      }
      else {
         read_include( phase, pos, true );
      }
   }
   else if ( strcmp( phase->tk_text, "include" ) == 0 ) {
      p_read_tk( phase );
      if ( phase->source->imported ) {
         p_test_tk( phase, TK_LIT_STRING );
         p_read_tk( phase );
      }
      else {
         read_include( phase, pos, false );
      }
   }
   else if ( strcmp( phase->tk_text, "define" ) == 0 ||
      strcmp( phase->tk_text, "libdefine" ) == 0 ) {
      read_define( phase );
   }
   else if ( strcmp( phase->tk_text, "library" ) == 0 ) {
      p_read_tk( phase );
      read_library( phase, pos );
   }
   else if ( strcmp( phase->tk_text, "encryptstrings" ) == 0 ) {
      phase->task->library->encrypt_str = true;
      p_read_tk( phase );
   }
   else if ( strcmp( phase->tk_text, "nocompact" ) == 0 ) {
      phase->task->library->format = FORMAT_BIG_E;
      p_read_tk( phase );
   }
   else if (
      // NOTE: Not sure what these two are.
      strcmp( phase->tk_text, "wadauthor" ) == 0 ||
      strcmp( phase->tk_text, "nowadauthor" ) == 0 ) {
      t_diag( phase->task, DIAG_POS_ERR, pos, "directive `%s` not supported",
         phase->tk_text );
      t_bail( phase->task );
   }
   else {
      t_diag( phase->task, DIAG_POS_ERR, pos,
         "unknown directive '%s'", phase->tk_text );
      t_bail( phase->task );
   }
}

void read_include( struct parse* phase, struct pos* pos, bool import ) {
   p_test_tk( phase, TK_LIT_STRING );
   struct source* source = p_load_included_source( phase );
   p_read_tk( phase );
   if ( import ) {
      source->imported = true;
      struct library* parent_lib = phase->task->library;
      struct library* lib = add_library( phase->task );
      list_append( &parent_lib->dynamic, lib );
      phase->task->library = lib;
      phase->task->library->imported = true;
      p_read_lib( phase );
      p_read_tk( phase );
      phase->task->library = parent_lib;
   }
}

void read_library( struct parse* phase, struct pos* pos ) {
   p_test_tk( phase, TK_LIT_STRING );
   if ( ! phase->tk_length ) {
      t_diag( phase->task, DIAG_POS_ERR, &phase->tk_pos,
         "library name is blank" );
      t_bail( phase->task );
   }
   int length = phase->tk_length;
   if ( length > MAX_LIB_NAME_LENGTH ) {
      t_diag( phase->task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &phase->tk_pos, "library name too long" );
      t_diag( phase->task, DIAG_FILE, &phase->tk_pos,
         "library name can be up to %d characters long",
         MAX_LIB_NAME_LENGTH );
      length = MAX_LIB_NAME_LENGTH;
   }
   char name[ MAX_LIB_NAME_LENGTH + 1 ];
   memcpy( name, phase->tk_text, length );
   name[ length ] = 0;
   p_read_tk( phase );
   // Different #library directives in the same library must have the same
   // name.
   if ( phase->task->library->name.length &&
      strcmp( phase->task->library->name.value, name ) != 0 ) {
      t_diag( phase->task, DIAG_POS_ERR, pos, "library has multiple names" );
      t_diag( phase->task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &phase->task->library->name_pos, "first name given here" );
      t_bail( phase->task );
   }
   // Each library must have a unique name.
   list_iter_t i;
   list_iter_init( &i, &phase->task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib != phase->task->library && strcmp( name, lib->name.value ) == 0 ) {
         t_diag( phase->task, DIAG_POS_ERR, pos,
            "duplicate library name" );
         t_diag( phase->task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &lib->name_pos,
            "library name previously found here" );
         t_bail( phase->task );
      }
      list_next( &i );
   }
   str_copy( &phase->task->library->name, name, length );
   phase->task->library->importable = true;
   phase->task->library->name_pos = *pos;
}

void read_define( struct parse* phase ) {
   bool hidden = false;
   if ( phase->tk_text[ 0 ] == 'd' && phase->task->library->imported ) {
      hidden = true;
   }
   p_read_tk( phase );
   p_test_tk( phase, TK_ID );
   struct constant* constant = mem_alloc( sizeof( *constant ) );
   t_init_object( &constant->object, NODE_CONSTANT );
   constant->object.pos = phase->tk_pos;
   if ( hidden ) {
      constant->name = t_make_name( phase->task, phase->tk_text,
         phase->task->library->hidden_names );
   }
   else {
      constant->name = t_make_name( phase->task, phase->tk_text,
         phase->region_upmost->body );
   }
   p_read_tk( phase );
   struct expr_reading value;
   p_init_expr_reading( &value, true, false, false, true );
   p_read_expr( phase, &value );
   constant->value_node = value.output_node;
   constant->value = 0;
   constant->hidden = hidden;
   constant->lib_id = phase->task->library->id;
   p_add_unresolved( phase->region, &constant->object );
}

void p_add_unresolved( struct region* region, struct object* object ) {
   if ( region->unresolved ) {
      region->unresolved_tail->next = object;
   }
   else {
      region->unresolved = object;
   }
   region->unresolved_tail = object;
}

void link_usable_strings( struct parse* phase ) {
   // Link together the strings that have the potential to be used. To reduce
   // the number of unused indexes that have to be published, we want used
   // strings to appear first. This is done to try and prevent the case where
   // you have a string with index, say, 20 that is used, and all strings
   // before are not, but you still have to publish the other 20 indexes
   // because it is required by the format of the STRL chunk.
   struct indexed_string* head = NULL;
   struct indexed_string* tail;
   // Strings in main file of the library appear first.
   struct indexed_string* string = phase->task->str_table.head;
   while ( string ) {
      if ( ! string->imported && string->in_main_file ) {
         add_usable_string( &head, &tail, string );
      }
      string = string->next;
   }
   // Strings part of the library but found in a secondary file appear next.
   string = phase->task->str_table.head;
   while ( string ) {
      if ( ! string->imported && ! string->in_main_file ) {
         add_usable_string( &head, &tail, string );
      }
      string = string->next;
   }
   // Strings in an imported library follow. Only a string found in a constant
   // is useful.
   string = phase->task->str_table.head;
   while ( string ) {
      if ( string->imported && string->in_constant ) {
         add_usable_string( &head, &tail, string );
      }
      string = string->next;
   }
   phase->task->str_table.head_usable = head;
}

void add_usable_string( struct indexed_string** head,
   struct indexed_string** tail, struct indexed_string* string ) {
   if ( *head ) {
      ( *tail )->next_usable = string;
   }
   else {
      *head = string;
   }
   *tail = string;
}

void alloc_string_indexes( struct parse* phase ) {
   int index = 0;
   struct indexed_string* string = phase->task->str_table.head_usable;
   while ( string ) {
      string->index = index;
      ++index;
      string = string->next_usable;
   }
}

void p_diag( struct parse* phase, int flags, ... ) {
   va_list args;
   va_start( args, flags );
   t_diag_args( phase->task, flags, &args );
   va_end( args );
}

void p_bail( struct parse* phase ) {
   t_bail( phase->task );
}