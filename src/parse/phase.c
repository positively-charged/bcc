#include <string.h>

#include "phase.h"

static void make_main_lib( struct parse* parse );
static void read_dirc( struct parse* parse, struct pos* );
static void read_include( struct parse* parse, struct pos*, bool );
static void read_import( struct parse* parse, struct pos* pos );
static void load_imported_lib( struct parse* parse );
static struct library* find_lib( struct parse* parse,
   struct file_entry* file );
static void append_imported_lib( struct parse* parse, struct library* lib );
static void read_library( struct parse* parse, struct pos* );
static void read_library_name( struct parse* parse, struct pos* pos );
static void read_define( struct parse* parse );
static void link_usable_strings( struct parse* parse );
static void add_usable_string( struct indexed_string**,
   struct indexed_string**, struct indexed_string* );
static void alloc_string_indexes( struct parse* parse );

void p_init( struct parse* parse, struct task* task ) {
   parse->task = task;
   // NOTE: parse->queue not initialized.
   parse->peeked = 0;
   parse->tk = TK_END;
   parse->tk_text = "";
   parse->tk_length = 0;
   parse->source = NULL;
   parse->main_source = NULL;
   parse->free_source = NULL;
   parse->last_id = 0;
   str_init( &parse->temp_text );
   list_init( &parse->text_buffers );
   parse->read_flags = READF_CONCATSTRINGS | READF_ESCAPESEQ;
}

void p_read( struct parse* parse ) {
   make_main_lib( parse );
   p_load_main_source( parse );
   parse->task->library->file_pos.id = parse->source->file->id;
   parse->task->library->file = parse->main_source->file;
   p_read_tk( parse );

/*
   while ( parse->tk != TK_END ) {
      printf( "a %s %d\n", parse->tk_text, parse->tk_length );
      p_read_tk( parse );
   }
   p_bail( parse );
*/
   p_read_lib( parse );
   link_usable_strings( parse );
   alloc_string_indexes( parse );
}

void make_main_lib( struct parse* parse ) {
   parse->task->library = t_add_library( parse->task );
   parse->task->library_main = parse->task->library;
}

void p_read_lib( struct parse* parse ) {
   while ( true ) {
      if ( p_is_dec( parse ) ) {
         struct dec dec;
         p_init_dec( &dec );
         dec.name_offset = parse->task->body;
         p_read_dec( parse, &dec );
      }
      else if ( parse->tk == TK_SCRIPT ) {
         p_read_script( parse );
      }
      else if ( parse->tk == TK_SEMICOLON ) {
         p_read_tk( parse );
      }
      else if ( parse->tk == TK_HASH ) {
         struct pos pos = parse->tk_pos;
         p_read_tk( parse );
         read_dirc( parse, &pos );
      }
      else if ( parse->tk == TK_END ) {
         break;
      }
      else {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "unexpected %s", p_get_token_name( parse->tk ) );
         p_bail( parse );
      }
   }
   // Library must have the #library directive.
   if ( parse->task->library->imported &&
      ! parse->task->library->header ) {
      p_diag( parse, DIAG_ERR | DIAG_FILE, &parse->tk_pos,
         "#imported file missing #library directive" );
      p_bail( parse );
   }
}

void read_dirc( struct parse* parse, struct pos* pos ) {
   if ( parse->tk == TK_IMPORT ) {
      p_read_tk( parse );
      read_import( parse, pos );
   }
   else if ( strcmp( parse->tk_text, "include" ) == 0 ) {
      p_read_tk( parse );
      read_include( parse, pos, false );
   }
   else if ( strcmp( parse->tk_text, "define" ) == 0 ||
      strcmp( parse->tk_text, "libdefine" ) == 0 ) {
      read_define( parse );
   }
   else if ( strcmp( parse->tk_text, "library" ) == 0 ) {
      p_read_tk( parse );
      read_library( parse, pos );
   }
   else if ( strcmp( parse->tk_text, "encryptstrings" ) == 0 ) {
      parse->task->library->encrypt_str = true;
      p_read_tk( parse );
   }
   else if ( strcmp( parse->tk_text, "nocompact" ) == 0 ) {
      parse->task->library->format = FORMAT_BIG_E;
      p_read_tk( parse );
   }
   else if (
      // NOTE: Not sure what these two are.
      strcmp( parse->tk_text, "wadauthor" ) == 0 ||
      strcmp( parse->tk_text, "nowadauthor" ) == 0 ) {
      p_diag( parse, DIAG_POS_ERR, pos, "directive `%s` not supported",
         parse->tk_text );
      p_bail( parse );
   }
   else {
      p_diag( parse, DIAG_POS_ERR, pos,
         "unknown directive '%s'", parse->tk_text );
      p_bail( parse );
   }
}

void read_include( struct parse* parse, struct pos* pos, bool import ) {
   p_test_tk( parse, TK_LIT_STRING );
   p_load_included_source( parse );
   p_read_tk( parse );
}

void read_import( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_LIT_STRING );
   load_imported_lib( parse );
   p_read_tk( parse );
}

#include "semantic/phase.h"

void load_imported_lib( struct parse* parse ) {
   struct file_query query;
   t_init_file_query( &query, parse->source->file, parse->tk_text );
   t_find_file( parse->task, &query );
   if ( ! query.file ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "library not found: %s", parse->tk_text );
      p_bail( parse );
   }
   struct library* lib = find_lib( parse, query.file );
   if ( ! lib ) {
      struct source* source = p_load_included_source( parse );
      source->imported = true;
      struct library* parent_lib = parse->task->library;
      lib = t_add_library( parse->task );
      parse->task->library = lib;
      parse->task->library->imported = true;
      p_read_tk( parse );
      p_read_lib( parse );
      struct semantic semantic;
      s_init( &semantic, parse->task, lib );
      s_test( &semantic );
      parse->task->library = parent_lib;
      lib->file = query.file;
   }
   append_imported_lib( parse, lib );
}

struct library* find_lib( struct parse* parse, struct file_entry* file ) {
   list_iter_t i;
   list_iter_init( &i, &parse->task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib->file == file ) {
         return lib;
      }
      list_next( &i );
   }
   return NULL;
}

void append_imported_lib( struct parse* parse, struct library* lib ) {
   list_iter_t i;
   list_iter_init( &i, &parse->task->library->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* imported_lib = list_data( &i );
      if ( lib == imported_lib ) {
         p_diag( parse, DIAG_WARN | DIAG_POS, &parse->tk_pos,
            "duplicate import of library" );
         return;
      }
      list_next( &i );
   }
   list_append( &parse->task->library->dynamic, lib );
}

void read_library( struct parse* parse, struct pos* pos ) {
   parse->task->library->header = true;
   if ( parse->tk == TK_LIT_STRING ) {
      read_library_name( parse, pos );
   }
   else {
      parse->task->library->compiletime = true;
   }
}

void read_library_name( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_LIT_STRING );
   if ( ! parse->tk_length ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "library name is blank" );
      p_bail( parse );
   }
   int length = parse->tk_length;
   if ( length > MAX_LIB_NAME_LENGTH ) {
      p_diag( parse, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &parse->tk_pos, "library name too long" );
      p_diag( parse, DIAG_FILE, &parse->tk_pos,
         "library name can be up to %d characters long",
         MAX_LIB_NAME_LENGTH );
      length = MAX_LIB_NAME_LENGTH;
   }
   char name[ MAX_LIB_NAME_LENGTH + 1 ];
   memcpy( name, parse->tk_text, length );
   name[ length ] = 0;
   p_read_tk( parse );
   // Different #library directives in the same library must have the same
   // name.
   if ( parse->task->library->name.length &&
      strcmp( parse->task->library->name.value, name ) != 0 ) {
      p_diag( parse, DIAG_POS_ERR, pos, "library has multiple names" );
      p_diag( parse, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &parse->task->library->name_pos, "first name given here" );
      p_bail( parse );
   }
   // Each library must have a unique name.
   list_iter_t i;
   list_iter_init( &i, &parse->task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib != parse->task->library && strcmp( name, lib->name.value ) == 0 ) {
         p_diag( parse, DIAG_POS_ERR, pos,
            "duplicate library name" );
         p_diag( parse, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &lib->name_pos,
            "library name previously found here" );
         p_bail( parse );
      }
      list_next( &i );
   }
   str_copy( &parse->task->library->name, name, length );
   parse->task->library->importable = true;
   parse->task->library->name_pos = *pos;
}

void read_define( struct parse* parse ) {
   bool hidden = false;
   if ( parse->tk_text[ 0 ] == 'd' && parse->task->library->imported ) {
      hidden = true;
   }
   p_read_tk( parse );
   p_test_tk( parse, TK_ID );
   struct constant* constant = mem_alloc( sizeof( *constant ) );
   t_init_object( &constant->object, NODE_CONSTANT );
   constant->object.pos = parse->tk_pos;
   if ( hidden ) {
      constant->name = t_extend_name( parse->task->library->hidden_names,
         parse->tk_text );
   }
   else {
      constant->name = t_extend_name( parse->task->body,
            parse->tk_text );
   }
   p_read_tk( parse );
   struct expr_reading value;
   p_init_expr_reading( &value, true, false, false, true );
   p_read_expr( parse, &value );
   constant->value_node = value.output_node;
   constant->value = 0;
   constant->hidden = hidden;
   constant->lib_id = parse->task->library->id;
   p_add_unresolved( parse->task->library, &constant->object );
}

void p_add_unresolved( struct library* lib, struct object* object ) {
   if ( lib->unresolved ) {
      lib->unresolved_tail->next = object;
   }
   else {
      lib->unresolved = object;
   }
   lib->unresolved_tail = object;
}

void link_usable_strings( struct parse* parse ) {
   // Link together the strings that have the potential to be used. To reduce
   // the number of unused indexes that have to be published, we want used
   // strings to appear first. This is done to try and prevent the case where
   // you have a string with index, say, 20 that is used, and all strings
   // before are not, but you still have to publish the other 20 indexes
   // because it is required by the format of the STRL chunk.
   struct indexed_string* head = NULL;
   struct indexed_string* tail;
   // Strings in main file of the library appear first.
   struct indexed_string* string = parse->task->str_table.head;
   while ( string ) {
      if ( ! string->imported && string->in_main_file ) {
         add_usable_string( &head, &tail, string );
      }
      string = string->next;
   }
   // Strings part of the library but found in a secondary file appear next.
   string = parse->task->str_table.head;
   while ( string ) {
      if ( ! string->imported && ! string->in_main_file ) {
         add_usable_string( &head, &tail, string );
      }
      string = string->next;
   }
   // Strings in an imported library follow. Only a string found in a constant
   // is useful.
   string = parse->task->str_table.head;
   while ( string ) {
      if ( string->imported && string->in_constant ) {
         add_usable_string( &head, &tail, string );
      }
      string = string->next;
   }
   parse->task->str_table.head_usable = head;
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

void alloc_string_indexes( struct parse* parse ) {
   int index = 0;
   struct indexed_string* string = parse->task->str_table.head_usable;
   while ( string ) {
      string->index = index;
      ++index;
      string = string->next_usable;
   }
}

void p_diag( struct parse* parse, int flags, ... ) {
   va_list args;
   va_start( args, flags );
   t_diag_args( parse->task, flags, &args );
   va_end( args );
}

void p_unexpect_diag( struct parse* parse ) {
   p_diag( parse, DIAG_POS_ERR | DIAG_SYNTAX, &parse->tk_pos,
      "unexpected %s", p_get_token_name( parse->tk ) );
}

void p_unexpect_item( struct parse* parse, struct pos* pos, enum tk tk ) {
   p_unexpect_name( parse, pos, p_get_token_name( tk ) );
}

void p_unexpect_name( struct parse* parse, struct pos* pos,
   const char* subject ) {
   p_diag( parse, DIAG_POS, ( pos ? pos : &parse->tk_pos ),
      "expecting %s here, or", subject );
}

void p_unexpect_last( struct parse* parse, struct pos* pos, enum tk tk ) {
   p_unexpect_last_name( parse, pos, p_get_token_name( tk ) );
}

void p_unexpect_last_name( struct parse* parse, struct pos* pos,
   const char* subject ) {
   p_diag( parse, DIAG_POS, ( pos ? pos : &parse->tk_pos ),
      "expecting %s here", subject );
}

void p_bail( struct parse* parse ) {
   p_deinit_tk( parse );
   t_bail( parse->task );
}