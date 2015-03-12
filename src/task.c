#include <string.h>
#include <stdarg.h>

#include "task.h"

enum {
   TYPE_INT,
   TYPE_STR,
   TYPE_BOOL,
   TYPE_VOID
};

static void init_str_table( struct str_table* table );
static struct type* get_type( struct task*, int type );
static struct name* new_name( void );
static void open_logfile( struct task* task );
static void diag_acc( struct task* task, int, va_list* );
static void decode_pos( struct task* task, struct pos* pos, const char** file,
   int* line, int* column );

void t_init( struct task* task, struct options* options, jmp_buf* bail ) {
   task->options = options;
   task->err_file = NULL;
   task->bail = bail;
   task->file_entries = NULL;
   init_str_table( &task->str_table );
   task->root_name = new_name();
   task->anon_name = t_make_name( task, "!anon.", task->root_name );
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
}

void init_str_table( struct str_table* table ) {
   table->head = NULL;
   table->head_sorted = NULL;
   table->head_usable = NULL;
   table->tail = NULL;
}

struct name* new_name( void ) {
   struct name* name = mem_slot_alloc( sizeof( *name ) );
   name->parent = NULL;
   name->next = NULL;
   name->drop = NULL;
   name->object = NULL;
   name->ch = 0;
   return name;
}

struct name* t_make_name( struct task* task, const char* ch,
   struct name* parent ) {
   struct name* name = parent->drop;
   while ( *ch ) {
      // Find the correct node to enter.
      struct name* prev = NULL;
      while ( name && name->ch < *ch ) {
         prev = name;
         name = name->next;
      }
      // Enter a new node if no node could be found.
      if ( ! name ) {
         name = new_name();
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
         struct name* smaller_name = new_name();
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
      region->body_struct = t_make_name( task, "struct::", name );
   }
   else {
      region->body = t_make_name( task, "::", name );
      region->body_struct = t_make_name( task, "::struct::", name );
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
      t_make_name( task, "int", task->region_upmost->body ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_int = type;
   type = t_create_type( task,
      t_make_name( task, "str", task->region_upmost->body ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   type->is_str = true;
   task->type_str = type;
   type = t_create_type( task,
      t_make_name( task, "bool", task->region_upmost->body ) );
   type->object.resolved = true;
   type->size = 1;
   type->primitive = true;
   task->type_bool = type;
}

struct type* t_create_type( struct task* task, struct name* name ) {
   struct type* type = mem_alloc( sizeof( *type ) );
   t_init_object( &type->object, NODE_TYPE );
   type->name = name;
   type->body = t_make_name( task, "::", name );
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
      struct name* name = t_make_name( task, list[ i ].name, type->body );
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
   if ( task->options->acc_err ) {
      diag_acc( task, flags, args );
   }
   else {
      if ( flags & DIAG_FILE ) {
         const char* file = NULL;
         int line = 0, column = 0;
         decode_pos( task, va_arg( args, struct pos* ), &file, &line,
            &column );
         printf( "%s", file );
         if ( flags & DIAG_LINE ) {
            printf( ":%d", line );
            if ( flags & DIAG_COLUMN ) {
               printf( ":%d", column );
            }
         }
         printf( ": " );
      }
      if ( flags & DIAG_ERR ) {
         printf( "error: " );
      }
      else if ( flags & DIAG_WARN ) {
         printf( "warning: " );
      }
      const char* format = va_arg( *args, const char* );
      vprintf( format, *args );
      printf( "\n" );
   }
}

// Line format: <file>:<line>: <message>
void diag_acc( struct task* task, int flags, va_list* args ) {
   if ( ! task->err_file ) {
      open_logfile( task );
   }
   if ( flags & DIAG_FILE ) {
      const char* file = NULL;
      int line = 0, column = 0;
      decode_pos( task, va_arg( *args, struct pos* ), &file, &line, &column );
      fprintf( task->err_file, "%s:", file );
      if ( flags & DIAG_LINE ) {
         // For some reason, DB2 decrements the line number by one. Add one to
         // make the number correct.
         fprintf( task->err_file, "%d:", line + 1 );
      }
   }
   fprintf( task->err_file, " " );
   if ( flags & DIAG_ERR ) {
      fprintf( task->err_file, "error: " );
   }
   else if ( flags & DIAG_WARN ) {
      fprintf( task->err_file, "warning: " );
   }
   const char* message = va_arg( *args, const char* );
   vfprintf( task->err_file, message, *args );
   fprintf( task->err_file, "\n" );
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