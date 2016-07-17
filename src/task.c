#include <string.h>
#include <stdarg.h>

#include "task.h"

enum {
   TYPE_INT,
   TYPE_STR,
   TYPE_BOOL,
   TYPE_VOID
};

struct diag_msg {
   struct str text;
   const char* file;
   int line;
   int column;
   int flags;
};

static void init_str_table( struct str_table* table );
static struct type* get_type( struct task*, int type );
static void init_diag_msg( struct task* task, struct diag_msg* msg, int flags,
   va_list* args );
static void print_diag( struct task* task, struct diag_msg* msg );
static void log_diag( struct task* task, struct diag_msg* msg );
static void open_logfile( struct task* task );
static void decode_pos( struct task* task, struct pos* pos, const char** file,
   int* line, int* column );
static bool identify_file( struct task* task, struct file_query* query );
static struct file_entry* add_file( struct task* task,
   struct file_query* query );
static struct file_entry* create_file_entry( struct task* task,
   struct file_query* query );
static void link_file_entry( struct task* task, struct file_entry* entry );

void t_init( struct task* task, struct options* options, jmp_buf* bail ) {
   task->options = options;
   task->err_file = NULL;
   task->bail = bail;
   task->file_entries = NULL;
   init_str_table( &task->str_table );
   task->root_name = t_create_name();
   struct region* region = t_alloc_region( task, task->root_name, true );
   task->root_name->object = &region->object;
   task->region_upmost = region;
   t_init_types( task );
   t_init_type_members( task );
   task->library = NULL;
   task->library_main = NULL;
   list_init( &task->libraries );
   list_init( &task->regions );
   list_append( &task->regions, region );
   list_init( &task->scripts );
   task->last_id = 0;
}

void init_str_table( struct str_table* table ) {
   table->head = NULL;
   table->head_sorted = NULL;
   table->head_usable = NULL;
   table->tail = NULL;
}

struct name* t_create_name( void ) {
   struct name* name = mem_slot_alloc( sizeof( *name ) );
   name->parent = NULL;
   name->next = NULL;
   name->drop = NULL;
   name->object = NULL;
   name->ch = 0;
   return name;
}

struct name* t_extend_name( struct name* parent, const char* extension ) {
   struct name* name = parent->drop;
   const char* ch = extension;
   while ( *ch ) {
      // Find the correct node to enter.
      struct name* prev = NULL;
      while ( name && name->ch < *ch ) {
         prev = name;
         name = name->next;
      }
      // Enter a new node if no node could be found.
      if ( ! name ) {
         name = t_create_name();
         name->parent = parent;
         name->ch = *ch;
         if ( prev ) {
            prev->next = name;
         }
         else {
            parent->drop = name;
         }
      }
      // Enter a new node if no node with the same character exists in the
      // parent node.
      else if ( name->ch != *ch ) {
         struct name* smaller_name = t_create_name();
         smaller_name->next = name;
         smaller_name->parent = parent;
         smaller_name->ch = *ch;
         name = smaller_name;
         if ( prev ) {
            prev->next = name;
         }
         else {
            parent->drop = name;
         }
      }
      parent = name;
      name = name->drop;
      ++ch;
   }
   return parent;
}

void t_copy_name( struct name* start, bool full, struct str* str ) {
   int length = 0;
   char term = ':';
   if ( full ) {
      term = 0;
   }
   struct name* name = start;
   while ( name->ch && name->ch != term ) {
      name = name->parent;
      ++length;
   }
   if ( str->buffer_length < length + 1 ) {
      str_grow( str, length + 1 );
   }
   str->length = length;
   str->value[ length ] = 0;
   name = start;
   while ( length ) {
      --length;
      str->value[ length ] = name->ch;
      name = name->parent;
   }
}

int t_full_name_length( struct name* name ) {
   int length = 0;
   while ( name->ch ) {
      name = name->parent;
      ++length;
   }
   return length;
}

void t_init_object( struct object* object, int node_type ) {
   object->node.type = node_type;
   object->resolved = false;
   object->depth = 0;
   object->next = NULL;
   object->next_scope = NULL;
}

struct region* t_alloc_region( struct task* task, struct name* name,
   bool upmost ) {
   struct region* region = mem_alloc( sizeof( *region ) );
   t_init_object( &region->object, NODE_REGION );
   region->name = name;
   if ( upmost ) {
      region->body = name;
      region->body_struct = t_extend_name( name, "struct::" );
   }
   else {
      region->body = t_extend_name( name, "::" );
      region->body_struct = t_extend_name( name, "::struct::" );
   }
   region->link = NULL;
   region->unresolved = NULL;
   region->unresolved_tail = NULL;
   list_init( &region->imports );
   list_init( &region->items );
   return region;
}

void t_init_types( struct task* task ) {
   struct type* type = t_create_type( task,
      t_extend_name( task->region_upmost->body, "int" ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_int = type;
   type = t_create_type( task,
      t_extend_name( task->region_upmost->body, "str" ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   type->is_str = true;
   task->type_str = type;
   type = t_create_type( task,
      t_extend_name( task->region_upmost->body, "bool" ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_bool = type;
}

struct type* t_create_type( struct task* task, struct name* name ) {
   struct type* type = mem_alloc( sizeof( *type ) );
   t_init_object( &type->object, NODE_TYPE );
   type->name = name;
   type->body = t_extend_name( name, "::" );
   type->member = NULL;
   type->member_tail = NULL;
   type->size = 0;
   type->primitive = false;
   type->is_str = false;
   type->anon = false;
   return type;
}

void t_init_type_members( struct task* task ) {
   static struct {
      const char* name;
      int param;
      int type;
      int value;
      int id;
   } list[] = {
      { "length", 0, TYPE_STR, TYPE_INT, INTERN_FUNC_STR_LENGTH },
      { "at", 1, TYPE_STR, TYPE_INT, INTERN_FUNC_STR_AT },
   };
   for ( size_t i = 0; i < ARRAY_SIZE( list ); ++i ) {
      struct func* func = mem_slot_alloc( sizeof( *func ) );
      t_init_object( &func->object, NODE_FUNC );
      func->object.resolved = true;
      func->type = FUNC_INTERNAL;
      struct type* type = get_type( task, list[ i ].type );
      struct name* name = t_extend_name( type->body, list[ i ].name );
      name->object = &func->object;
      func->name = name;
      func->params = NULL;
      func->return_type = get_type( task, list[ i ].value );
      struct func_intern* impl = mem_alloc( sizeof( *impl ) );
      impl->id = list[ i ].id;
      func->impl = impl;
      func->min_param = list[ i ].param; 
      func->max_param = list[ i ].param;
      func->hidden = false;
   }
}

struct type* get_type( struct task* task, int id ) {
   switch ( id ) {
   case TYPE_INT:
      return task->type_int;
   case TYPE_STR:
      return task->type_str;
   case TYPE_BOOL:
      return task->type_bool;
   default:
      return NULL;
   }
}

int t_get_script_number( struct script* script ) {
   if ( script->number ) {
      return script->number->value;
   }
   else {
      return 0;
   }
}

void t_diag( struct task* task, int flags, ... ) {
   va_list args;
   va_start( args, flags );
   t_diag_args( task, flags, &args );
   va_end( args );
}

void t_diag_args( struct task* task, int flags, va_list* args ) {
   struct diag_msg msg;
   init_diag_msg( task, &msg, flags, args );
   print_diag( task, &msg );
   if ( task->options->acc_err ) {
      log_diag( task, &msg );
   }
   // Deinitialize `msg`.
   str_deinit( &msg.text );
}

void init_diag_msg( struct task* task, struct diag_msg* msg, int flags,
   va_list* args ) {
   if ( flags & DIAG_FILE ) {
      struct pos* pos = va_arg( *args, struct pos* );
      decode_pos( task, pos, &msg->file, &msg->line, &msg->column );
   }
   else {
      msg->file = NULL;
      msg->line = 0;
      msg->column = 0;
   }
   str_init( &msg->text );
   if ( flags & DIAG_ERR ) {
      if ( flags & DIAG_SYNTAX ) {
         str_append( &msg->text, "syntax " );
      }
      str_append( &msg->text, "error: " );
   }
   else if ( flags & DIAG_WARN ) {
      str_append( &msg->text, "warning: " );
   }
   const char* format = va_arg( *args, const char* );
   str_append_format( &msg->text, format, args );
   msg->flags = flags;
}

void print_diag( struct task* task, struct diag_msg* msg ) {
   if ( msg->flags & DIAG_FILE ) {
      printf( "%s:", msg->file );
      if ( msg->flags & DIAG_LINE ) {
         printf( "%d:", msg->line );
         if ( msg->flags & DIAG_COLUMN ) {
            printf( "%d:", msg->column );
         }
      }
      printf( " " );
   }
   printf( "%s\n", msg->text.value );
}

// Line format: <file>:<line>: <message>
void log_diag( struct task* task, struct diag_msg* msg ) {
   if ( ! task->err_file ) {
      open_logfile( task );
   }
   if ( msg->flags & DIAG_FILE ) {
      fprintf( task->err_file, "%s:", msg->file );
      if ( msg->flags & DIAG_LINE ) {
         fprintf( task->err_file, "%d:", msg->line );
      }
      fprintf( task->err_file, " " );
   }
   fprintf( task->err_file, "%s\n", msg->text.value );
}

void open_logfile( struct task* task ) {
   struct str str;
   str_init( &str );
   str_copy( &str, task->options->source_file,
      strlen( task->options->source_file ) );
   while ( str.length && str.value[ str.length - 1 ] != '/' &&
      str.value[ str.length - 1 ] != '\\' ) {
      str.value[ str.length - 1 ] = 0;
      --str.length;
   }
   str_append( &str, "acs.err" );
   task->err_file = fopen( str.value, "w" );
   if ( ! task->err_file ) {
      printf( "error: failed to load error output file: %s\n", str.value );
      t_bail( task );
   }
   str_deinit( &str );
}

// TODO: Log message to file.
void t_intern_diag( struct task* task, const char* file, int line,
   const char* format, ... ) {
   va_list args;
   va_start( args, format );
   printf( "%s:%d: internal compiler error: ", file, line );
   vprintf( format, args );
   printf( "\n" );
   va_end( args );
}

void t_unhandlednode_diag( struct task* task, const char* file, int line,
   struct node* node ) {
   t_intern_diag( task, file, line,
      "unhandled node type: %d", node->type );
}

void decode_pos( struct task* task, struct pos* pos, const char** file,
   int* line, int* column ) {
   struct file_entry* entry = task->file_entries;
   while ( entry && entry->id != pos->id ) {
      entry = entry->next;
   }
   *file = entry->path.value;
   *line = pos->line;
   *column = pos->column;
   if ( task->options->one_column ) {
      ++*column;
   }
}
 
void t_bail( struct task* task ) {
   longjmp( *task->bail, 1 );
}

bool t_same_pos( struct pos* a, struct pos* b ) {
   return (
      a->id == b->id &&
      a->line == b->line &&
      a->column == b->column );
}

void t_init_file_query( struct file_query* query,
   struct file_entry* offset_file, const char* path ) {
   query->given_path = path;
   query->path = NULL;
   // NOTE: .fileid NOT initialized.
   query->file = NULL;
   query->offset_file = offset_file;
   query->success = false;
}

void t_find_file( struct task* task, struct file_query* query ) {
   struct str path;
   str_init( &path );
   query->path = &path;
   if ( identify_file( task, query ) ) {
      query->file = add_file( task, query );
      query->success = true;
   }
   str_deinit( &path );
}

bool identify_file( struct task* task, struct file_query* query ) {
   // Try path directly.
   str_append( query->path, query->given_path );
   if ( c_read_fileid( &query->fileid, query->path->value ) ) {
      return true;
   }
   // Try directory of current file.
   if ( query->offset_file ) {
      str_copy( query->path, query->offset_file->full_path.value,
         query->offset_file->full_path.length );
      c_extract_dirname( query->path );
      str_append( query->path, "/" );
      str_append( query->path, query->given_path );
      if ( c_read_fileid( &query->fileid, query->path->value ) ) {
         return true;
      }
   }
   // Try user-specified directories.
   list_iter_t i;
   list_iter_init( &i, &task->options->includes );
   while ( ! list_end( &i ) ) {
      char* include = list_data( &i ); 
      str_clear( query->path );
      str_append( query->path, include );
      str_append( query->path, "/" );
      str_append( query->path, query->given_path );
      if ( c_read_fileid( &query->fileid, query->path->value ) ) {
         return true;
      }
      list_next( &i );
   }
   return false;
}

struct file_entry* add_file( struct task* task, struct file_query* query ) {
   struct file_entry* entry = task->file_entries;
   while ( entry ) {
      if ( c_same_fileid( &query->fileid, &entry->file_id ) ) {
         return entry;
      }
      entry = entry->next;
   }
   return create_file_entry( task, query );
}

struct file_entry* create_file_entry( struct task* task,
   struct file_query* query ) {
   struct file_entry* entry = mem_alloc( sizeof( *entry ) );
   entry->next = NULL;
   entry->file_id = query->fileid;
   str_init( &entry->path );
   str_append( &entry->path, query->given_path );
   str_init( &entry->full_path );
   c_read_full_path( query->path->value, &entry->full_path );
   entry->id = task->last_id;
   ++task->last_id;
   link_file_entry( task, entry );
   return entry;
}

void link_file_entry( struct task* task, struct file_entry* entry ) {
   if ( task->file_entries ) {
      struct file_entry* prev = task->file_entries;
      while ( prev->next ) {
         prev = prev->next;
      }
      prev->next = entry;
   }
   else {
      task->file_entries = entry;
   }
}

struct library* t_add_library( struct task* task ) {
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
      lib->hidden_names = t_extend_name( last_lib->hidden_names, "a." );
   }
   else {
      lib->hidden_names = t_extend_name( task->root_name, "!hidden." );
   }
   lib->encrypt_str = false;
   list_append( &task->libraries, lib );
   return lib;
}

struct indexed_string* t_lookup_string( struct task* task, int index ) {
   struct indexed_string* string = task->str_table.head;
   while ( string && string->index != index ) {
      string = string->next;
   }
   return string;
}