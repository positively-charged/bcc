#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "task.h"

enum { STRTABLE_MAXSIZE = 65536 };
enum {
   STRTABLE_STR,
   STRTABLE_SCRIPTNAME,
};

struct diag_msg {
   struct str text;
   struct include_history_entry* file;
   const char* filename;
   int line;
   int column;
   int flags;
   bool include_history;
};

static void init_str_table( struct str_table* table );
static void add_internal_file( struct task* task, const char* name );
static void init_diag_msg( struct task* task, struct diag_msg* msg, int flags,
   va_list* args );
static void print_diag( struct task* task, struct diag_msg* msg );
static void print_include_history( struct task* task, struct diag_msg* msg,
   FILE* stream );
static void print_parent_files( struct task* task, FILE* stream,
   struct include_history_entry* entry,
   struct include_history_entry* child_entry );
static void log_diag( struct task* task, struct diag_msg* msg );
static void open_logfile( struct task* task );
static void decode_pos( struct task* task, struct pos* pos,
   struct include_history_entry** file, int* line, int* column );
static const char* decode_filename( struct task* task,
   struct include_history_entry* entry );
static bool identify_file( struct task* task, struct file_query* query );
static bool identify_file_relative( struct task* task,
   struct file_query* query );
static struct file_entry* add_file( struct task* task,
   struct file_query* query );
static struct file_entry* create_file_entry( struct task* task,
   struct file_query* query );
static void link_file_entry( struct task* task, struct file_entry* entry );
static struct indexed_string* intern_string( struct task* task,
   struct str_table* table, const char* value, int length, bool copy_value );
static void init_ref( struct ref* ref, int type );
static bool is_object_in_ns( struct ns* ns, struct object* object );
static bool is_object_in_fragment( struct ns_fragment* fragment,
   struct object* object );

void t_init( struct task* task, struct options* options, jmp_buf* bail,
   struct str* compiler_dir ) {
   task->options = options;
   task->err_file = NULL;
   task->bail = bail;
   task->text_buffer = NULL;
   task->file_entries = NULL;
   init_str_table( &task->str_table );
   init_str_table( &task->script_name_table );
   task->empty_string = t_intern_string( task, "", 0 );
   task->library_main = NULL;
   list_init( &task->libraries );
   list_init( &task->namespaces );
   task->last_id = 0;
   task->compile_time = time( NULL );
   gbuf_init( &task->growing_buffer );
   list_init( &task->runtime_asserts );
   task->root_name = t_create_name();
   task->upmost_ns = t_alloc_ns( task->root_name );
   task->upmost_ns->name->object = &task->upmost_ns->object;
   list_append( &task->namespaces, task->upmost_ns );

   task->array_name = t_create_name();
   struct func_intern* impl = mem_alloc( sizeof( *impl ) );
   impl->id = INTERN_FUNC_ARRAY_LENGTH;
   struct func* func = t_alloc_func();
   func->object.resolved = true;
   func->type = FUNC_INTERNAL;
   func->impl = impl;
   func->return_spec = SPEC_INT;
   struct name* name = t_extend_name( task->array_name, ".length" );
   func->name = name;
   name->object = &func->object;

   struct func_format* format_impl = mem_alloc( sizeof( *format_impl ) );
   format_impl->opcode = 0;
   func = t_alloc_func();
   func->object.resolved = true;
   func->type = FUNC_FORMAT;
   func->impl = format_impl;
   task->append_func = func;
   func->name = t_extend_name( task->root_name, ".append" );

   // `str` functions.
   task->str_name = t_create_name();
   // Function: str.length()
   impl = mem_alloc( sizeof( *impl ) );
   impl->id = INTERN_FUNC_STR_LENGTH;
   func = t_alloc_func();
   func->object.resolved = true;
   func->type = FUNC_INTERNAL;
   func->impl = impl;
   func->return_spec = SPEC_INT;
   name = t_extend_name( task->str_name, ".length" );
   func->name = name;
   name->object = &func->object;

   // Dummy nodes.
   struct expr* expr = t_alloc_expr();
   expr->pos.id = INTERNALFILE_COMPILER;
   expr->pos.line = 0;
   expr->pos.column = 0;
   expr->spec = SPEC_RAW;
   struct literal* literal = t_alloc_literal();
   expr->root = &literal->node;
   expr->folded = true;
   task->dummy_expr = expr;
   task->raw0_expr = expr;

   str_init( &task->err_file_dir );
   str_copy( &task->err_file_dir, "", 0 );
   t_update_err_file_dir( task, options->source_file );
   // TODO: Make this better.
   task->blank_name = task->upmost_ns->body;

   list_init( &task->include_history );
   task->last_diag_file = NULL;
   add_internal_file( task, "<none>" );
   add_internal_file( task, "<compiler>" );
   add_internal_file( task, "<command-line>" );
   task->compiler_dir = compiler_dir;
   // Setup BCS-specific directories.
   str_init( &task->bcs_lib_dir );
   str_append( &task->bcs_lib_dir, compiler_dir->value );
   str_append( &task->bcs_lib_dir, OS_PATHSEP );
   str_append( &task->bcs_lib_dir, "lib" );
   // Setup ACS/ACS95-specific directories.
   str_init( &task->acs_lib_dir );
   str_append( &task->acs_lib_dir, compiler_dir->value );
   str_append( &task->acs_lib_dir, OS_PATHSEP );
   str_append( &task->acs_lib_dir, "lib/acs" );

   list_init( &task->structures );
}

struct ns* t_alloc_ns( struct name* name ) {
   struct ns* ns = mem_alloc( sizeof( *ns ) );
   t_init_object( &ns->object, NODE_NAMESPACE );
   ns->parent = NULL;
   ns->name = name;
   ns->body = t_extend_name( name, "." );
   ns->body_structs = t_extend_name( name, ".!s." );
   ns->body_enums = t_extend_name( name, ".!e." );
   ns->links = NULL;
   list_init( &ns->fragments );
   ns->hidden = false;
   ns->dot_separator = false;
   return ns;
}

struct ns_fragment* t_alloc_ns_fragment( void ) {
   struct ns_fragment* fragment = mem_alloc( sizeof( *fragment ) );
   t_init_object( &fragment->object, NODE_NAMESPACEFRAGMENT );
   fragment->ns = NULL;
   fragment->path = NULL;
   fragment->unresolved = NULL;
   fragment->unresolved_tail = NULL;
   list_init( &fragment->objects );
   list_init( &fragment->funcs );
   list_init( &fragment->scripts );
   list_init( &fragment->runnables );
   list_init( &fragment->fragments );
   list_init( &fragment->usings );
   fragment->strict = false;
   fragment->hidden = false;
   return fragment;
}

void t_append_unresolved_namespace_object( struct ns_fragment* fragment,
   struct object* object ) {
   if ( fragment->unresolved ) {
      fragment->unresolved_tail->next = object;
   }
   else {
      fragment->unresolved = object;
   }
   fragment->unresolved_tail = object;
}

static void init_str_table( struct str_table* table ) {
   table->head = NULL;
   table->tail = NULL;
   table->root = NULL;
   table->size = 0;
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

void t_copy_name( struct name* start, struct str* str ) {
   int length = 0;
   struct name* name = start;
   while ( name && ! ( name->ch == '\0' || name->ch == '.' ) ) {
      ++length;
      name = name->parent;
   }
   if ( str->buffer_length < length + 1 ) {
      str_grow( str, length + 1 );
   }
   str->length = length;
   str->value[ length ] = '\0';
   struct name* end = name;
   name = start;
   while ( name != end ) {
      --length;
      str->value[ length ] = name->ch;
      name = name->parent;
   }
}

void t_copy_full_name( struct name* start, const char* separator,
   struct str* str ) {
   int length = 0;
   struct name* name = start;
   const int separator_length = strlen( separator );
   while ( name && ! ( name->ch == '\0' || ( name->ch == '.' &&
      name->parent && name->parent->ch == '\0' ) ) ) {
      if ( name->ch == '.' ) {
         length += separator_length;
      }
      else {
         ++length;
      }
      name = name->parent;
   }
   if ( str->buffer_length < length + 1 ) {
      str_grow( str, length + 1 );
   }
   str->length = length;
   str->value[ length ] = '\0';
   struct name* end = name;
   name = start;
   while ( name != end ) {
      if ( name->ch == '.' ) {
         length -= separator_length;
         memcpy( str->value + length, separator, separator_length ); 
      }
      else {
         --length;
         str->value[ length ] = name->ch;
      }
      name = name->parent;
   }
}

int t_full_name_length( struct name* name, const char* separator ) {
   int length = 0;
   const int separator_length = strlen( separator );
   while ( name && ! ( name->ch == '\0' || ( name->ch == '.' &&
      name->parent && name->parent->ch == '\0' ) ) ) {
      if ( name->ch == '.' ) {
         length += separator_length;
      }
      else {
         ++length;
      }
      name = name->parent;
   }
   return length;
}

void t_init_object( struct object* object, int node_type ) {
   object->node.type = node_type;
   object->depth = 0;
   object->resolved = false;
   t_init_pos_id( &object->pos, INTERNALFILE_COMPILER );
   object->next = NULL;
   object->next_scope = NULL;
}

/*
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
      struct structure* type = get_type( task, list[ i ].type );
      struct name* name = t_extend_name( type->body, list[ i ].name );
      name->object = &func->object;
      func->name = name;
      func->params = NULL;
      func->return_spec = SPEC_ZINT;
      struct func_intern* impl = mem_alloc( sizeof( *impl ) );
      impl->id = list[ i ].id;
      func->impl = impl;
      func->min_param = list[ i ].param; 
      func->max_param = list[ i ].param;
      func->hidden = false;
   }
} */

static void add_internal_file( struct task* task, const char* name ) {
   struct include_history_entry* entry =
      t_reserve_include_history_entry( task );
   entry->altern_name = name;
}

// Allocates and keeps track of (appends to a list) an include history entry.
struct include_history_entry* t_reserve_include_history_entry(
   struct task* task ) {
   struct include_history_entry* entry = mem_alloc( sizeof( *entry ) );
   entry->parent = NULL;
   entry->altern_name = NULL;
   entry->file = NULL;
   entry->id = list_size( &task->include_history );
   entry->line = 0;
   entry->imported = false;
   list_append( &task->include_history, entry );
   return entry;
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
   // Let the user know that internal errors are bugs.
   if ( flags & DIAG_INTERNAL ) {
      t_diag( task, DIAG_NOTE,
         "an internal error is a bug in the compiler, please report it to the "
         "developer" );
   }
}

static void init_diag_msg( struct task* task, struct diag_msg* msg, int flags,
   va_list* args ) {
   str_init( &msg->text );
   msg->file = NULL;
   msg->filename = NULL;
   msg->line = 0;
   msg->column = 0;
   msg->flags = flags;
   msg->include_history = false;
   // Decode position.
   if ( flags & DIAG_FILE ) {
      struct pos* pos = va_arg( *args, struct pos* );
      decode_pos( task, pos, &msg->file, &msg->line, &msg->column );
      if ( msg->file != task->last_diag_file ) {
         task->last_diag_file = msg->file;
         if ( msg->file->parent ) {
            msg->include_history = true;
         }
      }
   }
   else if ( flags & DIAG_FILENAME ) {
      msg->filename = va_arg( *args, const char* );
      if ( flags & DIAG_LINE ) {
         msg->line = va_arg( *args, int );
      }
   }
   // Append message prefix.
   if ( flags & DIAG_SYNTAX ) {
      str_append( &msg->text, "syntax " );
   }
   else if ( flags & DIAG_INTERNAL ) {
      str_append( &msg->text, "internal " );
   }
   if ( flags & DIAG_ERR ) {
      str_append( &msg->text, "error: " );
   }
   else if ( flags & DIAG_WARN ) {
      str_append( &msg->text, "warning: " );
   }
   else if ( flags & DIAG_NOTE ) {
      str_append( &msg->text, "note: " );
   }
   // Append message.
   const char* format = va_arg( *args, const char* );
   str_append_format_va( &msg->text, format, args );
}

static void print_diag( struct task* task, struct diag_msg* msg ) {
   if ( msg->flags & DIAG_FILE || msg->flags & DIAG_FILENAME ) {
      if ( msg->flags & DIAG_FILENAME ) {
         printf( "%s:", msg->filename );
      }
      else {
         print_include_history( task, msg, stdout );
         printf( "%s:", decode_filename( task, msg->file ) );
      }
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

static void print_include_history( struct task* task, struct diag_msg* msg,
   FILE* stream ) {
   if ( msg->include_history ) {
      print_parent_files( task, stream, msg->file->parent, msg->file );
   }
}

static void print_parent_files( struct task* task, FILE* stream,
   struct include_history_entry* entry,
   struct include_history_entry* child_entry ) {
   if ( entry->parent ) {
      print_parent_files( task, stream, entry->parent, entry );
   }
   fprintf( stream, "In file #%s from %s:%d:\n",
      child_entry->imported ? "imported" : "included",
      decode_filename( task, entry ), child_entry->line );
}

// Line format: <file>:<line>: <message>
static void log_diag( struct task* task, struct diag_msg* msg ) {
   if ( msg->flags & DIAG_ERR ) {
      if ( ! task->err_file ) {
         open_logfile( task );
      }
      if ( msg->flags & DIAG_FILE || msg->flags & DIAG_FILENAME ) {
         if ( msg->flags & DIAG_FILENAME ) {
            fprintf( task->err_file, "%s:", msg->filename );
         }
         else {
            print_include_history( task, msg, task->err_file );
            fprintf( task->err_file, "%s:",
               decode_filename( task, msg->file ) );
         }
         if ( msg->flags & DIAG_LINE ) {
            fprintf( task->err_file, "%d:", msg->line );
         }
         fprintf( task->err_file, " " );
      }
      fprintf( task->err_file, "%s\n", msg->text.value );
   }
}

static void open_logfile( struct task* task ) {
   struct str path;
   str_init( &path );
   str_append( &path, task->err_file_dir.value );
   str_append( &path, "acs.err" );
   task->err_file = fopen( path.value, "w" );
   if ( ! task->err_file ) {
      printf( "error: failed to load error output file: %s\n", path.value );
      t_bail( task );
   }
   str_deinit( &path );
}

static void decode_pos( struct task* task, struct pos* pos,
   struct include_history_entry** file, int* line, int* column ) {
   *file = t_decode_include_history_entry( task, pos->id );
   *line = pos->line;
   *column = pos->column;
   if ( task->options->one_column ) {
      ++*column;
   }
}

struct include_history_entry* t_decode_include_history_entry(
   struct task* task, int id ) {
   struct list_iter i;
   list_iterate( &task->include_history, &i );
   while ( ! list_end( &i ) ) {
      struct include_history_entry* entry = list_data( &i );
      while ( entry->id == id ) {
         return entry;
      }
      list_next( &i );
   }
   return list_head( &task->include_history );
}

static const char* decode_filename( struct task* task,
   struct include_history_entry* entry ) {
   if ( entry->altern_name ) {
      return entry->altern_name;
   }
   else {
      return entry->file->path.value;
   }
}

void t_decode_pos( struct task* task, struct pos* pos, const char** file,
   int* line, int* column ) {
   struct include_history_entry* entry;
   decode_pos( task, pos, &entry, line, column );
   *file = decode_filename( task, entry );
}

const char* t_decode_pos_file( struct task* task, struct pos* pos ) {
   struct include_history_entry* file;
   int line;
   int column;
   decode_pos( task, pos, &file, &line, &column );
   return decode_filename( task, file );
}
 
void t_bail( struct task* task ) {
   longjmp( *task->bail, 1 );
}

void t_deinit( struct task* task ) {
   if ( task->err_file ) {
      fclose( task->err_file );
   }
}

bool t_same_pos( struct pos* a, struct pos* b ) {
   return (
      a->id == b->id &&
      a->line == b->line &&
      a->column == b->column );
}

void t_init_file_query( struct file_query* query, const char* lang_dir,
   struct file_entry* offset_file, const char* path ) {
   query->given_path = path;
   query->path = NULL;
   // NOTE: .fileid NOT initialized.
   query->file = NULL;
   query->offset_file = offset_file;
   query->lang_dir = lang_dir;
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

static bool identify_file( struct task* task, struct file_query* query ) {
   // Absolute path.
   if ( c_is_absolute_path( query->given_path ) ) {
      str_append( query->path, query->given_path );
      return c_read_fileid( &query->fileid, query->path->value );
   }
   // Relative path.
   else {
      return identify_file_relative( task, query );
   }
}

static bool identify_file_relative( struct task* task,
   struct file_query* query ) {
   // Try directory of current file.
   if ( query->offset_file ) {
      str_copy( query->path, query->offset_file->path.value,
         query->offset_file->path.length );
      c_extract_dirname( query->path );
      if ( query->path->length > 0 ) {
         str_append( query->path, OS_PATHSEP );
      }
      str_append( query->path, query->given_path );
      if ( c_read_fileid( &query->fileid, query->path->value ) ) {
         return true;
      }
   }
   // Try path directly.
   else {
      str_append( query->path, query->given_path );
      if ( c_read_fileid( &query->fileid, query->path->value ) ) {
         return true;
      }
   }
   // Try user-specified directories.
   struct list_iter i;
   list_iterate( &task->options->includes, &i );
   while ( ! list_end( &i ) ) {
      char* include = list_data( &i ); 
      str_clear( query->path );
      str_append( query->path, include );
      str_append( query->path, OS_PATHSEP );
      str_append( query->path, query->given_path );
      if ( c_read_fileid( &query->fileid, query->path->value ) ) {
         return true;
      }
      list_next( &i );
   }
   // Try language-specific directories.
   if ( query->lang_dir ) {
      str_clear( query->path );
      str_append( query->path, query->lang_dir );
      str_append( query->path, OS_PATHSEP );
      str_append( query->path, query->given_path );
      if ( c_read_fileid( &query->fileid, query->path->value ) ) {
         return true;
      }
   }
   return false;
}

static struct file_entry* add_file( struct task* task,
   struct file_query* query ) {
   struct file_entry* entry = task->file_entries;
   while ( entry ) {
      if ( c_same_fileid( &query->fileid, &entry->file_id ) ) {
         return entry;
      }
      entry = entry->next;
   }
   return create_file_entry( task, query );
}

static struct file_entry* create_file_entry( struct task* task,
   struct file_query* query ) {
   struct file_entry* entry = mem_alloc( sizeof( *entry ) );
   entry->next = NULL;
   entry->file_id = query->fileid;
   str_init( &entry->path );
   str_append( &entry->path, query->path->value );
   str_init( &entry->full_path );
   c_read_full_path( query->path->value, &entry->full_path );
   entry->id = task->last_id;
   ++task->last_id;
   link_file_entry( task, entry );
   return entry;
}

static void link_file_entry( struct task* task, struct file_entry* entry ) {
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
   list_init( &lib->objects );
   list_init( &lib->private_objects );
   list_init( &lib->files );
   list_init( &lib->import_dircs );
   list_init( &lib->dynamic );
   list_init( &lib->dynamic_acs );
   list_init( &lib->dynamic_bcs );
   list_init( &lib->links );
   list_init( &lib->external_vars );
   list_init( &lib->external_funcs );
   lib->upmost_ns_fragment = t_alloc_ns_fragment();
   lib->upmost_ns_fragment->ns = task->upmost_ns;
   list_append( &lib->upmost_ns_fragment->ns->fragments,
      lib->upmost_ns_fragment );
   // root_name->object = &lib->upmost_ns->object;
   lib->file_pos.line = 0;
   lib->file_pos.column = 0;
   lib->file_pos.id = 0;
   lib->id = list_size( &task->libraries );
   lib->format = FORMAT_LITTLE_E;
   lib->lang = LANG_BCS;
   lib->importable = false;
   lib->imported = false;
   lib->encrypt_str = false;
   lib->file = NULL;
   lib->header = false;
   lib->wadauthor = false;
   lib->uses_nullable_refs = false;
   return lib;
}

char* t_intern_text( struct task* task, const char* value, int length ) {
   struct text_buffer* buffer = t_get_text_buffer( task, length + 1 );
   memcpy( buffer->left, value, length + 1 );
   char* text = buffer->left;
   buffer->left += length + 1;
   return text;
}

struct text_buffer* t_get_text_buffer( struct task* task,
   int min_free_size ) {
   struct text_buffer* buffer = task->text_buffer;
   if ( ! buffer || buffer->end - buffer->left < min_free_size ) {
      buffer = mem_alloc( sizeof( *buffer ) );
      buffer->prev = task->text_buffer;
      enum { INITIAL_SIZE = 1 << 15 };
      unsigned int size = INITIAL_SIZE;
      while ( size < ( unsigned int ) min_free_size ) {
         size <<= 1;
      }
      buffer->start = mem_alloc( sizeof( char ) * size );
      buffer->end = buffer->start + size;
      buffer->left = buffer->start;
      task->text_buffer = buffer;
   }
   return buffer;
}

struct indexed_string* t_intern_string( struct task* task,
   const char* value, int length ) {
   return intern_string( task, &task->str_table, value, length, false );
}

struct indexed_string* t_intern_string_copy( struct task* task,
   const char* value, int length ) {
   return intern_string( task, &task->str_table, value, length, true );
}

static struct indexed_string* intern_string( struct task* task,
   struct str_table* table, const char* value, int length, bool copy_value ) {
   // Indexed strings are stored in a binary search tree.
   struct indexed_string* parent_lchild = NULL;
   struct indexed_string* parent_rchild = NULL;
   struct indexed_string* string = table->root;
   while ( string ) {
      int result = strcmp( value, string->value );
      if ( result < 0 ) {
         parent_lchild = string;
         parent_rchild = NULL;
         string = string->left;
      }
      else if ( result > 0 ) {
         parent_lchild = NULL;
         parent_rchild = string;
         string = string->right;
      }
      else {
         return string;
      }
   }
   // Allocate a new indexed-string when one isn't interned.
   string = mem_alloc( sizeof( *string ) );
   if ( copy_value ) {
      string->value = t_intern_text( task, value, length );
   }
   else {
      string->value = value;
   }
   string->length = length;
   string->index = table->size;
   string->index_runtime = -1;
   string->next = NULL;
   string->left = NULL;
   string->right = NULL;
   string->used = false;
   string->in_source_code = false;
   if ( table->head ) {
      table->tail->next = string;
   }
   else {
      table->head = string;
   }
   table->tail = string;
   if ( parent_lchild ) {
      parent_lchild->left = string;
   }
   else if ( parent_rchild ) {
      parent_rchild->right = string;
   }
   else {
      table->root = string;
   }
   ++table->size;
   return string;
}

struct indexed_string* t_intern_script_name( struct task* task,
   const char* value, int length ) {
   struct indexed_string* string = intern_string( task,
      &task->script_name_table, value, length, false );
   // Encode number of string table in index.
   if ( string->index < STRTABLE_MAXSIZE ) {
      string->index += STRTABLE_SCRIPTNAME * STRTABLE_MAXSIZE;
   }
   return string;
}

struct indexed_string* t_lookup_string( struct task* task, int index ) {
   struct str_table* table = index / STRTABLE_MAXSIZE ==
      STRTABLE_SCRIPTNAME ? &task->script_name_table : &task->str_table;
   struct indexed_string* string = table->head;
   while ( string ) {
      if ( string->index == index ) {
         return string;
      }
      string = string->next;
   }
   return NULL;
}

struct constant* t_alloc_constant( void ) {
   struct constant* constant = mem_slot_alloc( sizeof( *constant ) );
   t_init_object( &constant->object, NODE_CONSTANT );
   constant->name = NULL;
   constant->value_node = NULL;
   constant->spec = SPEC_NONE;
   constant->value = 0;
   constant->hidden = false;
   constant->has_str = false;
   constant->force_local_scope = false;
   return constant;
}

struct enumeration* t_alloc_enumeration( void ) {
   struct enumeration* enumeration = mem_alloc( sizeof( *enumeration ) );
   t_init_object( &enumeration->object, NODE_ENUMERATION );
   enumeration->head = NULL;
   enumeration->tail = NULL;
   enumeration->name = NULL;
   enumeration->body = NULL;
   enumeration->base_type = SPEC_INT;
   enumeration->num_enumerators = 0;
   enumeration->hidden = false;
   enumeration->semicolon = false;
   enumeration->force_local_scope = false;
   enumeration->default_initz = false;
   return enumeration;
}

struct enumerator* t_alloc_enumerator( void ) {
   struct enumerator* enumerator = mem_alloc( sizeof( *enumerator ) );
   t_init_object( &enumerator->object, NODE_ENUMERATOR );
   enumerator->name = NULL;
   enumerator->next = NULL;
   enumerator->initz = NULL;
   enumerator->enumeration = NULL;
   enumerator->value = 0;
   enumerator->has_str = false;
   return enumerator;
}

void t_append_enumerator( struct enumeration* enumeration,
   struct enumerator* enumerator ) {
   if ( enumeration->head ) {
      enumeration->tail->next = enumerator;
   }
   else {
      enumeration->head = enumerator;
   }
   enumeration->tail = enumerator;
   ++enumeration->num_enumerators;
}

struct structure* t_alloc_structure( void ) {
   struct structure* structure = mem_alloc( sizeof( *structure ) );
   t_init_object( &structure->object, NODE_STRUCTURE );
   structure->name = NULL;
   structure->body = NULL;
   structure->member = NULL;
   structure->member_tail = NULL;
   structure->size = 0;
   structure->anon = false;
   structure->has_ref_member = false;
   structure->has_mandatory_ref_member = false;
   structure->semicolon = false;
   structure->force_local_scope = false;
   structure->hidden = false;
   return structure;
}

struct structure_member* t_alloc_structure_member( void ) {
   struct structure_member* member = mem_alloc( sizeof( *member ) );
   t_init_object( &member->object, NODE_STRUCTURE_MEMBER );
   member->name = NULL;
   member->ref = NULL;
   member->structure = NULL;
   member->enumeration = NULL;
   member->path = NULL;
   member->dim = NULL;
   member->next = NULL;
   member->next_instance = NULL;
   member->spec = SPEC_NONE;
   member->original_spec = SPEC_NONE;
   member->offset = 0;
   member->size = 0;
   member->diminfo_start = 0;
   member->head_instance = false;
   member->addr_taken = false;
   return member;
}

void t_append_structure_member( struct structure* structure,
   struct structure_member* member ) {
   if ( structure->member ) {
      structure->member_tail->next = member;
   }
   else {
      structure->member = member;
   }
   structure->member_tail = member;
}

struct dim* t_alloc_dim( void ) {
   struct dim* dim = mem_alloc( sizeof( *dim ) );
   dim->next = NULL;
   dim->length_node = NULL;
   dim->length = 0;
   dim->element_size = 0;
   return dim;
}

struct var* t_alloc_var( void ) {
   struct var* var = mem_alloc( sizeof( *var ) );
   t_init_object( &var->object, NODE_VAR );
   var->name = NULL;
   var->ref = NULL;
   var->structure = NULL;
   var->enumeration = NULL;
   var->type_path = NULL;
   var->dim = NULL;
   var->initial = NULL;
   var->value = NULL;
   var->next_instance = NULL;
   var->spec = SPEC_NONE;
   var->original_spec = SPEC_NONE;
   var->storage = STORAGE_LOCAL;
   var->index = 0;
   var->size = 0;
   var->diminfo_start = 0;
   var->initz_zero = false;
   var->hidden = false;
   var->used = false;
   var->modified = false;
   var->initial_has_str = false;
   var->imported = false;
   var->is_constant_init = false;
   var->addr_taken = false;
   var->in_shared_array = false;
   var->force_local_scope = false;
   var->constant = false;
   var->external = false;
   var->head_instance = false;
   var->anon = false;
   return var;
}

struct func* t_alloc_func( void ) {
   struct func* func = mem_slot_alloc( sizeof( *func ) );
   t_init_object( &func->object, NODE_FUNC );
   func->type = FUNC_ASPEC;
   func->ref = NULL;
   func->structure = NULL;
   func->enumeration = NULL;
   func->path = NULL;
   func->name = NULL;
   func->params = NULL;
   func->impl = NULL;
   func->return_spec = SPEC_VOID;
   func->original_return_spec = SPEC_VOID;
   func->min_param = 0;
   func->max_param = 0;
   func->hidden = false;
   func->imported = false;
   func->external = false;
   func->force_local_scope = false;
   func->literal = false;
   return func;
}

struct func_user* t_alloc_func_user( void ) {
   struct func_user* impl = mem_alloc( sizeof( *impl ) );
   list_init( &impl->labels );
   impl->body = NULL;
   impl->next_nested = NULL;
   impl->nested_funcs = NULL;
   impl->nested_calls = NULL;
   impl->returns = NULL;
   impl->prologue_point = NULL;
   impl->return_table = NULL;
   list_init( &impl->vars );
   list_init( &impl->funcscope_vars );
   impl->index = 0;
   impl->size = 0;
   impl->usage = 0;
   impl->obj_pos = 0;
   impl->recursive = RECURSIVE_UNDETERMINED;
   impl->nested = false;
   impl->local = false;
   return impl;
}

struct param* t_alloc_param( void ) {
   struct param* param = mem_slot_alloc( sizeof( *param ) );
   t_init_object( &param->object, NODE_PARAM );
   param->ref = NULL;
   param->structure = NULL;
   param->enumeration = NULL;
   param->path = NULL;
   param->next = NULL;
   param->name = NULL;
   param->default_value = NULL;
   param->spec = SPEC_NONE;
   param->original_spec = SPEC_NONE;
   param->index = 0;
   param->size = 0;
   param->obj_pos = 0;
   param->used = false;
   param->default_value_tested = false;
   return param;
}

struct format_item* t_alloc_format_item( void ) {
   struct format_item* item = mem_alloc( sizeof( *item ) );
   item->cast = FCAST_DECIMAL;
   t_init_pos_id( &item->pos, INTERNALFILE_COMPILER );
   item->next = NULL;
   item->value = NULL;
   item->extra = NULL;
   return item;
}

struct call* t_alloc_call( void ) {
   struct call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_CALL;
   call->operand = NULL;
   call->func = NULL;
   call->ref_func = NULL;
   call->nested_call = NULL;
   call->format_item = NULL;
   list_init( &call->args );
   call->constant = false;
   return call;
}

int t_dim_size( struct dim* dim ) {
   return dim->length * dim->element_size;
}

const struct lang_limits* t_get_lang_limits( int lang ) {
   static const struct lang_limits acs =
      { MAX_WORLD_VARS, MAX_GLOBAL_VARS, 1000, 4, 32768, 32767, 31 };
   static const struct lang_limits acs95 =
      { 64, 0, 64, 3, 128, 255, 31 };
   static const struct lang_limits bcs =
      { MAX_WORLD_VARS, MAX_GLOBAL_VARS, 1000, 4, 32768, 32767, 32768 };
   switch ( lang ) {
   case LANG_ACS:
      return &acs;
   case LANG_ACS95:
      return &acs95;
   default:
      return &bcs;
   }
}

const char* t_get_storage_name( int storage ) {
   switch ( storage ) {
   case STORAGE_MAP: return "map";
   case STORAGE_WORLD: return "world";
   case STORAGE_GLOBAL: return "global";
   default: return "local";
   }
}

struct literal* t_alloc_literal( void ) {
   struct literal* literal = mem_slot_alloc( sizeof( *literal ) );
   literal->node.type = NODE_LITERAL;
   literal->value = 0;
   return literal;
}

struct expr* t_alloc_expr( void ) {
   struct expr* expr = mem_slot_alloc( sizeof( *expr ) );
   expr->node.type = NODE_EXPR;
   expr->root = NULL;
   expr->spec = SPEC_NONE;
   expr->value = 0;
   expr->folded = false;
   expr->has_str = false;
   return expr;
}

struct indexed_string_usage* t_alloc_indexed_string_usage( void ) {
   struct indexed_string_usage* usage = mem_slot_alloc( sizeof( *usage ) );
   usage->node.type = NODE_INDEXED_STRING_USAGE;
   usage->string = NULL;
   return usage;
}

void t_init_pos( struct pos* pos, int id, int line, int column ) {
   pos->id = id;
   pos->line = line;
   pos->column = column;
}

void t_init_pos_id( struct pos* pos, int id ) {
   t_init_pos( pos, id, 0, 0 );
}

static void init_ref( struct ref* ref, int type ) {
   ref->next = NULL;
   ref->type = type;
   ref->nullable = false;
   ref->implicit = false;
}

struct ref_func* t_alloc_ref_func( void ) {
   struct ref_func* func = mem_alloc( sizeof( *func ) );
   init_ref( &func->ref, REF_FUNCTION );
   func->params = NULL;
   func->min_param = 0;
   func->max_param = 0;
   func->local = false;
   return func;
}

struct type_alias* t_alloc_type_alias( void ) {
   struct type_alias* alias = mem_alloc( sizeof( *alias ) );
   t_init_object( &alias->object, NODE_TYPE_ALIAS );
   t_init_pos_id( &alias->object.pos, INTERNALFILE_COMPILER );
   alias->ref = NULL;
   alias->structure = NULL;
   alias->enumeration = NULL;
   alias->path = NULL;
   alias->dim = NULL;
   alias->name = NULL;
   alias->next_instance = NULL;
   alias->spec = SPEC_NONE;
   alias->original_spec = SPEC_NONE;
   alias->head_instance = false;
   alias->force_local_scope = false;
   alias->hidden = false;
   return alias;
}

void t_update_err_file_dir( struct task* task, const char* path ) {
   struct str* dir = &task->err_file_dir;
   str_copy( dir, path, strlen( path ) );
   while ( dir->length && dir->value[ dir->length - 1 ] != '/' &&
      dir->value[ dir->length - 1 ] != '\\' ) {
      dir->value[ dir->length - 1 ] = 0;
      --dir->length;
   }
}

const char* t_get_lang_lib_dir( struct task* task, int lang ) {
   switch ( lang ) {
   case LANG_BCS:
      return task->bcs_lib_dir.value;
   case LANG_ACS:
   case LANG_ACS95:
      return task->acs_lib_dir.value;
   default:
      return NULL;
   }
}

struct script* t_alloc_script( void ) {
   struct script* script = mem_alloc( sizeof( *script ) );
   script->node.type = NODE_SCRIPT;
   t_init_pos_id( &script->pos, INTERNALFILE_COMPILER );
   script->number = NULL;
   script->type = SCRIPT_TYPE_CLOSED;
   script->flags = SCRIPT_FLAG_NONE;
   script->params = NULL;
   script->body = NULL;
   script->nested_funcs = NULL;
   script->nested_calls = NULL;
   list_init( &script->labels );
   list_init( &script->vars );
   list_init( &script->funcscope_vars );
   script->assigned_number = 0;
   script->num_param = 0;
   script->offset = 0;
   script->size = 0;
   script->named_script = false;
   return script;
}

struct ns* t_find_ns_of_object( struct task* task, struct object* object ) {
   struct list_iter i;
   list_iterate( &task->namespaces, &i );
   while ( ! list_end( &i ) ) {
      struct ns* ns = list_data( &i );
      if ( is_object_in_ns( ns, object ) ) {
         return ns;
      }
      list_next( &i );
   }
   return NULL;
}

static bool is_object_in_ns( struct ns* ns, struct object* object ) {
   struct list_iter i;
   list_iterate( &ns->fragments, &i );
   while ( ! list_end( &i ) ) {
      if ( is_object_in_fragment( list_data( &i ), object ) ) {
         return true;
      }
      list_next( &i );
   }
   return false;
}

static bool is_object_in_fragment( struct ns_fragment* fragment,
   struct object* object ) {
   struct list_iter i;
   list_iterate( &fragment->objects, &i );
   while ( ! list_end( &i ) ) {
      if ( object == list_data( &i ) ) {
         return true;
      }
      list_next( &i );
   }
   return false;
}
