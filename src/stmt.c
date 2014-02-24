#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "task.h"

#ifdef __WINDOWS__

#include <windows.h>

#else

#include <dirent.h>
#include <sys/stat.h>

#endif

#define MAX_MODULE_NAME_LENGTH 8

struct package_query {
   struct str path;
   struct str init_path;
   struct str name;
};

static void mark_published( struct module*, const char* );
static void init_module_body( struct task*, struct module*, struct name* );
static void read_module( struct task* );
static void bind_module_object( struct task*, struct name*, struct object* );
static void make_module_link( struct task*, struct module*, struct pos* );
static struct name* make_import_name( struct task*, char*, struct pos* );
static void get_filename( const char*, struct str* );
static void get_dirname( struct str* );
static void import_module( struct task*, struct import* );
static void import_module_filepath( struct task*, struct import* );
static struct package* alloc_package( void );
static void create_parent_packages( struct task*, struct str*, struct list* );
static struct package* add_packages( struct task*, struct list* );
static struct package* make_parent_packages( struct task*, struct str* );
static void make_imported_visible( struct task*, struct import* );
static void alias_imported_object( struct task*, char*, struct pos*,
   struct object* );
static void import_objects( struct task*, struct module*, struct import* );
static void import_package_objects( struct task*, struct import* );
static void read_import( struct task*, struct list* );
static struct import* alloc_import( void );
static void read_dirc( struct task*, struct pos* );
static void read_stmt( struct task*, struct stmt_read* );
static void read_palrange_rgb_field( struct task*, struct expr**,
   struct expr**, struct expr** );
static struct label* new_label( char*, struct pos );
static void append_path( struct str*, struct path* );
static void test_block_item( struct task*, struct stmt_test*, struct node* );
static void test_stmt( struct task*, struct stmt_test*, struct node* );
static void test_goto_in_format_block( struct task*, struct list* );
static void read_pcode( struct task*, struct stmt_read* );

void t_init( struct task* task, struct options* options, jmp_buf* bail ) {
   task->tk = TK_END;
   task->tk_text = NULL;
   task->tk_length = 0;
   stack_init( &task->paused );
   list_init( &task->unloaded_files );
   task->peeked_length = 0;
   t_init_fields_dec( task );
   task->options = options;
   task->module = NULL;
   task->module_importer = NULL;
   task->module_main = NULL;
   task->str_table.head = NULL;
   task->str_table.tail = NULL;
   task->str_table.size = 0;
   task->format = FORMAT_BIG_E;
   t_init_fields_chunk( task );
   t_init_fields_obj( task );
   str_init( &task->str );
   list_init( &task->loaded_modules );
   list_init( &task->scripts );
   // Start the ID from 1. 0 is used for the main module.
   task->import_id = 1;
   task->newline_tk = false;
   str_init( &task->tokens.text );
   task->tokens.peeked = 0;
   task->bail = bail;
   task->column = 0;
   task->ch = 0;
   task->err_file = NULL;
}

void get_filename( const char* path, struct str* str ) {
   const char* ch = path;
   const char* start = ch;
   while ( *ch ) {
      if ( *ch == '/' ) {
         start = ch + 1;
      }
      ++ch;
   }
   int length = 0;
   ch = start;
   while ( *ch && *ch != '.' ) {
      ++length;
      ++ch;
   }
   str_copy( str, start, length );
}

void get_dirname( struct str* path ) {
   while ( true ) {
      if ( path->length == 0 ) {
         break;
      }
      --path->length;
      char ch = path->value[ path->length ];
      path->value[ path->length ] = 0;
      if ( ch == '/' ) {
         break;
      }
   }
}

#if defined( _WIN32 ) || defined( _WIN64 )

#include <windows.h>

bool get_full_path( const char* path, struct str* str ) {
   const int max_path = MAX_PATH + 1;
   if ( str->buffer_length < max_path ) { 
      str_grow( str, max_path );
   }
   str->length = GetFullPathName( path, max_path, str->value, NULL );
   if ( GetFileAttributes( str->value ) != INVALID_FILE_ATTRIBUTES ) {
      int i = 0;
      while ( str->value[ i ] ) {
         if ( str->value[ i ] == '\\' ) {
            str->value[ i ] = '/';
         }
         ++i;
      }
      return true;
   }
   else {
      return false;
   }
}

#else

bool get_full_path( const char* path, struct str* str ) {
   str_grow( str, PATH_MAX + 1 );
   if ( realpath( path, str->value ) ) {
      str->length = strlen( str->value );
      return true;
   }
   else {
      return false;
   }
}

#endif

void t_read( struct task* task ) {
   struct str path;
   str_init( &path );
   if ( ! get_full_path( task->options->source_file, &path ) ) {
      load_failure:
      t_diag( task, DIAG_ERR, "failed to load module: %s",
         task->options->source_file );
      t_bail( task );
   }
   struct str filename;
   str_init( &filename );
   get_filename( path.value, &filename );

   struct module* module = t_load_module( task, path.value, filename.value );
   if ( ! module ) {
      goto load_failure;
   }
   module->ref_count = 1;
   t_set_module( task, module );
   task->module_main = module;
   get_dirname( &path );
   struct package* package = make_parent_packages( task, &path );
   struct name* name = task->root_name;
   if ( package ) {
      name = package->body;
   }
   name = t_make_name( task, filename.value, name );
   name->object = &module->object;
   module->body = t_make_name( task, ".", name );
   module->body_struct = t_make_name( task, "struct.", module->body );
   module->body_import = t_make_name( task, "import.", module->body );
   str_deinit( &filename );
   str_deinit( &path );
   t_read_tk( task );
   read_module( task );
   // Determine which modules to publish.
   const char* lump = "";
   if ( task->module_main->lump.length ) {
      lump = task->module_main->lump.value;
   }
   // Determine which modules to publish and which to load dynamically.
   mark_published( task->module_main, lump );
}

void init_module_body( struct task* task, struct module* module,
   struct name* name ) {
   module->body = t_make_name( task, ".", name );
   module->body_struct = t_make_name( task, "struct.", module->body );
   module->body_import = t_make_name( task, "import.", module->body );
}

void read_module( struct task* task ) {
   char* lump = NULL;
   int lump_length = 0;
   struct pos lump_pos; 
   if ( task->tk == TK_MODULE ) {
      t_read_tk( task );
      t_test_tk( task, TK_LIT_STRING );
      lump = task->tk_text;
      lump_length = task->tk_length;
      lump_pos = task->tk_pos;
      t_read_tk( task );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
   }
   else if ( task->tk == TK_HASH ) {
      struct pos pos = task->tk_pos;
      t_read_tk( task );
      if ( task->tk == TK_ID && strcmp( task->tk_text, "library" ) == 0 ) {
         t_read_tk( task );
         t_test_tk( task, TK_LIT_STRING );
         lump = task->tk_text;
         lump_length = task->tk_length;
         lump_pos = task->tk_pos;
         t_read_tk( task );
      }
      else {
         read_dirc( task, &pos );
      }
   }
   if ( lump ) {
      if ( ! lump_length ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &lump_pos,
            "lump name of module is blank" );
         t_bail( task );
      }
      else if ( lump_length > MAX_MODULE_NAME_LENGTH ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &lump_pos,
            "lump name of module too long" );
         t_diag( task, DIAG_FILE, &lump_pos,
            "lump name of module can be up to %d characters long",
            MAX_MODULE_NAME_LENGTH );
         t_bail( task );
      }
      lump[ lump_length ] = 0;
      str_append( &task->module->lump, lump );
      task->module->visible = true;
   }
   else {
      // Header required for a module that is imported through the #import
      // directive.
      if ( task->module->import_dirc ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "missing module name (module \"<name\"; or #library \"<name>\")" );
         t_bail( task );
      }
   }
   while ( true ) {
      if ( t_is_dec( task ) ) {
         struct dec dec;
         t_init_dec( &dec );
         dec.name_offset = task->module->body;
         t_read_dec( task, &dec );
      }
      else if ( task->tk == TK_SCRIPT ) {
         t_read_script( task );
      }
      else if ( task->tk == TK_IMPORT ) {
         read_import( task, NULL );
      }
      else if ( task->tk == TK_HASH ) {
         struct pos pos = task->tk_pos;
         t_read_tk( task );
         read_dirc( task, &pos );
      }
      else if ( task->tk == TK_SEMICOLON ) {
         t_read_tk( task );
      }
      else if ( task->tk == TK_END ) {
         break;
      }
      else {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "unexpected token" );
         t_bail( task );
      }
   }
   task->module->read = true;
   // Bind module objects:
   // -----------------------------------------------------------------------
   struct object* object = task->module->unresolved;
   while ( object ) {
      if ( object->node.type == NODE_CONSTANT ) {
         struct constant* constant = ( struct constant* ) object;
         bind_module_object( task, constant->name, &constant->object );
      }
      else if ( object->node.type == NODE_CONSTANT_SET ) {
         struct constant_set* set = ( struct constant_set* ) object;
         struct constant* constant = set->head;
         while ( constant ) {
            bind_module_object( task, constant->name, &constant->object );
            constant = constant->next;
         }
      }
      else if ( object->node.type == NODE_VAR ) {
         struct var* var = ( struct var* ) object;
         bind_module_object( task, var->name, &var->object );
      }
      else if ( object->node.type == NODE_FUNC ) {
         struct func* func = ( struct func* ) object;
         bind_module_object( task, func->name, &func->object );
      }
      else if ( object->node.type == NODE_TYPE ) {
         struct type* type = ( struct type* ) object;
         if ( type->name->object ) {
            diag_dup_struct( task, type->name, &type->object.pos );
            t_bail( task );
         }
         type->name->object = &type->object;
      }
      object = object->next;
   }
   // Import modules:
   // -----------------------------------------------------------------------
   list_iter_t i;
   list_iter_init( &i, &task->module->imports );
   while ( ! list_end( &i ) ) {
      struct import* stmt = list_data( &i );
      if ( ! stmt->self ) {
         if ( stmt->path_file ) {
            import_module_filepath( task, stmt );
         }
         else {
            import_module( task, stmt );
         }
      }
      if ( ! stmt->local ) {
         make_imported_visible( task, stmt );
      }
      list_next( &i );
   }
}

void read_import( struct task* task, struct list* local ) {
   t_test_tk( task, TK_IMPORT );
   t_read_tk( task );
   while ( true ) {
      struct import* stmt = alloc_import();
      list_append( &task->module->imports, stmt );
      if ( local ) {
         list_append( local, stmt );
         stmt->local = true;
      }
      // Link with another module:
      if ( task->tk == TK_MODULE ) {
         t_read_tk( task );
         t_test_tk( task, TK_ASSIGN );
         t_read_tk( task );
         stmt->link = true;
         stmt->path = t_read_path( task );
         stmt->type = IMPORT_LINK;
      }
      else {
         stmt->path = t_read_path( task );
         stmt->type = IMPORT_DIRECT;
         // Alias to imported module:
         if ( ! stmt->path->next && task->tk == TK_ASSIGN ) {
            stmt->type = IMPORT_ALIAS;
            stmt->alias = stmt->path;
            stmt->path = NULL;
            t_read_tk( task );
            // Alias to the current module:
            if ( task->tk == TK_MODULE ) {
               stmt->type = IMPORT_SELF;
               stmt->self = true;
               t_read_tk( task );
            }
            else {
               stmt->path = t_read_path( task );
            }
         }
      }
      if ( task->tk == TK_COMMA ) {
         t_read_tk( task );
      }
      else if ( task->tk == TK_COLON ) {
         stmt->type = IMPORT_SELECTIVE;
         t_read_tk( task );
         struct import_item* tail = NULL;
         while ( true ) {
            struct import_item* item = mem_alloc( sizeof( *item ) );
            item->pos = task->tk_pos;
            item->next = NULL;
            item->name = NULL;
            item->alias = NULL;
            item->is_struct = false;
            item->is_link = false;
            if ( task->tk == TK_MODULE ) {
               item->is_link = true;
               t_read_tk( task );
               t_test_tk( task, TK_ASSIGN );
               t_read_tk( task );
               t_test_tk( task, TK_ID );
               item->name = task->tk_text;
               item->name_pos = task->tk_pos;
               t_read_tk( task );
            }
            else {
               if ( task->tk == TK_STRUCT ) {
                  item->is_struct = true;
                  t_read_tk( task );
               }
               t_test_tk( task, TK_ID );
               item->name = task->tk_text;
               item->name_pos = task->tk_pos;
               t_read_tk( task );
               if ( task->tk == TK_ASSIGN ) {
                  item->alias = item->name;
                  item->alias_pos = item->name_pos;
                  t_read_tk( task );
                  t_test_tk( task, TK_ID );
                  item->name = task->tk_text;
                  item->name_pos = task->tk_pos;
                  t_read_tk( task );
               }
            }
            if ( tail ) {
               tail->next = item;
            }
            else {
               stmt->items = item;
            }
            tail = item;
            if ( task->tk == TK_COMMA ) {
               t_read_tk( task );
            }
            else {
               break;
            }
         }
         break;
      }
      else {
         break;
      }
   }
   t_test_tk( task, TK_SEMICOLON );
   t_read_tk( task );
}

struct import* alloc_import( void ) {
   struct import* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_IMPORT;
   stmt->type = IMPORT_NONE;
   stmt->path = NULL;
   stmt->alias = NULL;
   stmt->items = NULL;
   stmt->package = NULL;
   stmt->package_last = NULL;
   stmt->next = NULL;
   stmt->self = false;
   stmt->link = false;
   stmt->local = false;
   stmt->from_package = false;
   stmt->path_file = false;
   list_init( &stmt->modules );
   return stmt;
}

struct path* t_read_path( struct task* task ) {
   struct path* head = NULL;
   struct path* tail;
   while ( true ) {
      t_test_tk( task, TK_ID );
      struct path* path = mem_alloc( sizeof( *path ) );
      path->next = NULL;
      path->text = task->tk_text;
      path->pos = task->tk_pos;
      if ( head ) {
         tail->next = path;
      }
      else {
         head = path;
      }
      tail = path;
      t_read_tk( task );
      if ( task->tk == TK_DOT ) {
         t_read_tk( task );
      }
      else {
         return head;
      }
   }
}

void bind_module_object( struct task* task, struct name* name,
   struct object* object ) {
   if ( name->object ) {
      struct str str;
      str_init( &str );
      t_copy_name( name, false, &str );
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &object->pos, "duplicate name `%s`", str.value );
      t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &name->object->pos,
         "name `%s` first used here", str.value );
      t_bail( task );
   }
   name->object = object;
}

void diag_missing_package_object( struct task* task, struct pos* pos,
   const char* package, const char* package_dir, const char* object ) {
   t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "`%s` not found in `%s` package", object, package );
   t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "using `%s` package found at: ", package );
   t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "%s", package_dir );
}

void import_module( struct task* task, struct import* stmt ) {
   struct str file_path;
   str_init( &file_path );
   struct path* path = stmt->path;
   struct import_item* item = NULL;
   // Load packages:
   struct package* package = NULL;
   struct package* package_head = NULL;
   // See if the package is already loaded.
   struct name* name = task->module->body_import;
   while ( path ) {
      name = t_make_name( task, path->text, name );
      if ( name->object ) {
         if ( name->object->node.type == NODE_PACKAGE ) {
            package = ( struct package* ) name->object;
            name = package->body;
            path = path->next;
            if ( ! package_head ) {
               package_head = package;
            }
         }
         else {
            if ( path->next ) {
               t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &path->pos, "using module like a package" );
               t_bail( task );
            }
            list_append( &stmt->modules, ( struct module* ) name->object );
            goto finish;
         }
      }
      else {
         break;
      }
   }
   if ( package ) {
      str_append( &file_path, package->path.value );
   }
   // No need to create packages when all are loaded.
   if ( ! path ) {
      goto load_modules;
   }
   bool only_module = false;
   // Resume from an already-loaded package.
   if ( package ) {
      goto load_package_files;
   }
   // Find the first package, or the module.
   list_iter_t i;
   list_iter_init( &i, &task->options->includes );
   bool done_module_dir = false;
   while ( true ) {
      // Try the directory of the current module.
      if ( ! done_module_dir ) {
         done_module_dir = true;
         if ( get_full_path( task->module->file->path.value, &file_path ) ) {
            get_dirname( &file_path );
         }
         else {
            continue;
         }
      }
      // Try each include directory.
      else if ( ! list_end( &i ) ) {
         str_clear( &file_path );
         char* dir = list_data( &i );
         list_next( &i );
         if ( ! get_full_path( dir, &file_path ) ) {
            continue;
         }
      }
      else {
         break;
      }
      // Packages are considered first.
      int length = file_path.length;
      str_append( &file_path, "/" );
      str_append( &file_path, path->text );
      str_append( &file_path, "/__init__.acs" );
      struct stat file;
      if ( stat( file_path.value, &file ) != -1 &&
         S_ISREG( file.st_mode ) ) {
         file_path.length = length;
         file_path.value[ length ] = 0;
         goto load_package_files;
      }
      // Try loading module.
      if ( ! path->next ) {
         file_path.length = length;
         file_path.value[ length ] = 0;
         str_append( &file_path, "/" );
         str_append( &file_path, path->text );
         // When using the import statement, a module file must have an .acs
         // extension.
         str_append( &file_path, ".acs" );
         if ( stat( file_path.value, &file ) != -1 &&
            S_ISREG( file.st_mode ) ) {
            file_path.length = length;
            file_path.value[ length ] = 0;
            only_module = true;
            goto load_package_files;
         }
      }
   }
   // Error:
   if ( path->next ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &path->pos, "package `%s` not found", path->text );
   }
   else {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &path->pos, "module or package `%s` not found", path->text );
   }
   t_bail( task );
   // file_path should contain the directory of a package or module.
   load_package_files:
   // Create parent packages. These are packages that are not specified in the
   // import statement but are present on the file system. Only load these if
   // not resuming from an already loaded package.
   if ( ! package ) {
      struct str dir;
      str_init( &dir );
      str_copy( &dir, file_path.value, file_path.length );
      package = make_parent_packages( task, &dir );
      str_deinit( &dir );
   }
   if ( only_module ) {
      goto load_modules;
   }
   // Create packages.
   name = task->root_name;
   if ( package ) {
      name = package->body;
   }
   struct package* package_last = NULL;
   while ( path ) {
      name = t_make_name( task, path->text, name );
      // Re-use existing package.
      if ( name->object ) {
         if ( name->object->node.type == NODE_PACKAGE ) {
            package = ( struct package* ) name->object;
            str_append( &file_path, "/" );
            str_append( &file_path, path->text );
         }
         else {
            break;
         }
      }
      // Create new package.
      else {
         int length = file_path.length;
         str_append( &file_path, "/" );
         str_append( &file_path, path->text );
         int length_dir = file_path.length;
         str_append( &file_path, "/__init__.acs" );
         struct stat file;
         if ( stat( file_path.value, &file ) == -1 ||
            ! S_ISREG( file.st_mode ) ) {
            file_path.length = length;
            file_path.value[ length ] = 0;
            break;
         }
         package = alloc_package();
         package->object.pos = path->pos;
         str_append( &package->filename, path->text );
         str_append( &package->init_path, file_path.value );
         file_path.length = length_dir;
         file_path.value[ length_dir ] = 0;
         str_append( &package->path, file_path.value );
         package->name = name;
         package->name->object = &package->object;
         package->body = t_make_name( task, ".", name );
         if ( package_last ) {
            package_last->next = package;
         }
      }
      // Remember the left-most package.
      if ( ! package_head ) {
         package_head = package;
         name = t_make_name( task, path->text, task->module->body_import );
         if ( ! name->object ) {
            name->object = &package->object;
         }
      }
      package_last = package;
      name = package->body;
      path = path->next;
   }
   if ( path && path->next ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &path->pos, "package `%s` not found", path->text );
      t_bail( task );
   }
   // Load modules:
   load_modules:
   // Cannot import just the package.
   if ( ! path && ! stmt->items ) {
      path = stmt->path;
      while ( path->next ) {
         path = path->next;
      }
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &path->pos,
         "importing only package" );
      t_bail( task );
   }
   item = stmt->items;
   while ( true ) {
      struct module* module = NULL;
      char* module_name = NULL;
      struct pos pos;
      // Module:
      if ( path ) {
         module_name = path->text;
         pos = path->pos;
      }
      // Module from package:
      else {
         module_name = item->name;
         pos = item->name_pos;
      }
      name = task->root_name;
      if ( package ) {
         name = package->body;
      }
      name = t_make_name( task, module_name, name );
      // Re-use loaded module.
      if ( name->object ) {
         module = ( struct module* ) name->object;
         goto got_module;
      }
      // Create new module:
      int length = file_path.length;
      str_append( &file_path, "/" );
      str_append( &file_path, module_name );
      str_append( &file_path, ".acs" );
      struct stat file;
      if ( stat( file_path.value, &file ) == -1 ||
         ! S_ISREG( file.st_mode ) ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "module `%s` not found", module_name );
         t_bail( task );
      }
      module = t_load_module( task, file_path.value, module_name );
      if ( ! module ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "failed to load module" );
         t_bail( task );
      }
      module->body = t_make_name( task, ".", name );
      module->body_struct = t_make_name( task, "struct.", module->body );
      module->body_import = t_make_name( task, "import.", module->body );
      name->object = &module->object;
      file_path.length = length;
      file_path.value[ length ] = 0;
      // Remember the module so it does not need to be loaded again.
      if ( only_module ) {
         name = t_make_name( task, module_name, task->module->body_import );
         name->object = &module->object;
      }
      got_module:
      list_append( &stmt->modules, module );
      if ( path ) {
         break;
      }
      else {
         item = item->next;
         if ( ! item ) {
            break;
         }
      }
   }
   // Read modules:
   list_iter_init( &i, &stmt->modules );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      if ( ! module->read ) {
         module->read = true;
         // Read.
         struct module* parent = task->module;
         struct module* parent_parent = task->module_importer;
         t_set_module( task, module );
         task->module_importer = parent;
         t_read_tk( task );
         read_module( task );
         task->module = parent;
         task->module_importer = parent_parent;
      }
      // Add module into imported list:
      if ( module != task->module ) {
         list_iter_t i;
         list_iter_init( &i, &task->module->imported );
         while ( ! list_end( &i ) && module != list_data( &i ) ) {
            list_next( &i );
         }
         if ( list_end( &i ) ) {
            list_append( &task->module->imported, module );
         }
      }
      list_next( &i );
   }
   finish: ;
   stmt->package = package_head;
   if ( ! path ) {
      stmt->from_package = true;
   }
   str_deinit( &file_path );
}

struct package* alloc_package( void ) {
   struct package* package = mem_alloc( sizeof( *package ) );
   t_init_object( &package->object, NODE_PACKAGE );
   str_init( &package->path );
   str_init( &package->init_path );
   str_init( &package->filename );
   package->object.resolved = true;
   package->name = NULL;
   package->body = NULL;
   package->next = NULL;
   package->ref_count = 0;
   return package;
}

struct package* make_parent_packages( struct task* task, struct str* path ) {
   // Query the packages.
   struct list packages;
   list_init( &packages );
   struct package* package = NULL;
   while ( true ) {
      int length = path->length;
      str_append( path, "/__init__.acs" );
      struct stat file;
      if ( stat( path->value, &file ) == -1 ||
         ! S_ISREG( file.st_mode ) ) {
         break;
      }
      package = alloc_package();
      str_append( &package->init_path, path->value );
      path->length = length;
      path->value[ length ] = 0;
      str_append( &package->path, path->value );
      // Determine name of package.
      while ( length >= 0 && path->value[ length ] != '/' ) {
         --length;
      }
      str_append( &package->filename, path->value + length + 1 );
      path->length = length;
      path->value[ length ] = 0;
      list_append_head( &packages, package );
   }
   // Only add them if they aren't already.
   list_iter_t i;
   list_iter_init( &i, &packages );
   struct name* name = task->root_name;
   while ( ! list_end( &i ) ) {
      package = list_data( &i );
      name = t_make_name( task, package->filename.value, name );
      if ( name->object ) {
         // Name already used.
         if ( name->object->node.type != NODE_PACKAGE ) {
            printf( "dup\n" );
            t_bail( task );
         }
         // Make sure the packages have matching directories.
         struct package* other_package = ( struct package* ) name->object;
         if ( strcmp( package->init_path.value,
            other_package->init_path.value ) != 0 ) {
            struct pos pos = { task->module->id };
            t_diag( task, DIAG_ERR | DIAG_FILE, &pos,
               "two different root packages named `%s`",
               package->filename.value );
            t_diag( task, DIAG_FILE, &pos, "package #1: %s", package->path.value );
            t_diag( task, DIAG_FILE, &pos, "package #2: %s", other_package->path.value );
            t_bail( task );
         }
         // No need for the duplicate package.
         str_deinit( &package->filename );
         str_deinit( &package->path );
         str_deinit( &package->init_path );
         mem_free( package );
         package = ( struct package* ) name->object;
      }
      else {
         package->name = name;
         package->body = t_make_name( task, ".", name );
         name->object = &package->object;
      }
      name = package->body;
      list_next( &i );
   }
   list_deinit( &packages );
   return package;
}

void import_module_filepath( struct task* task, struct import* stmt ) {
   struct str module_name;
   str_init( &module_name );
   get_filename( stmt->path->text, &module_name );
   struct module* module = NULL;
   // Don't load an already loaded module.
   struct name* name = t_make_name( task, module_name.value,
      task->module->body_import );
   if ( name->object ) {
      module = ( struct module* ) name->object;
      goto finish;
   }
   // Try the path directly.
   struct str file_path;
   str_init( &file_path );
   str_append( &file_path, stmt->path->text );
   struct stat file;
   if ( stat( file_path.value, &file ) != -1 &&
      S_ISREG( file.st_mode ) ) {
      goto load_module;
   }
   // Try the directory of the current module.
   str_clear( &file_path );
   get_full_path( task->module->file->path.value, &file_path );
   get_dirname( &file_path );
   str_append( &file_path, "/" );
   str_append( &file_path, stmt->path->text );
   if ( stat( file_path.value, &file ) != -1 &&
      S_ISREG( file.st_mode ) ) {
      goto load_module;
   }
   // Try the include directories.
   list_iter_t i;
   list_iter_init( &i, &task->options->includes );
   while ( ! list_end( &i ) ) {
      char* include = list_data( &i ); 
      str_clear( &file_path );
      str_append( &file_path, include );
      str_append( &file_path, "/" );
      str_append( &file_path, stmt->path->text );
      if ( stat( file_path.value, &file ) != -1 &&
         S_ISREG( file.st_mode ) ) {
         goto load_module;
      }
      list_next( &i );
   }
   t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
      &stmt->path->pos, "module not found: %s", stmt->path->text );
   t_bail( task );
   load_module:
   // -----------------------------------------------------------------------
   name = t_make_name( task, module_name.value, task->root_name );
   if ( name->object ) {
      module = ( struct module* ) name->object;
      goto finish;
   }
   module = t_load_module( task, file_path.value, module_name.value );
   if ( ! module ) {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &stmt->path->pos, "failed to load module" );
      t_bail( task );
   }
   name->object = &module->object;
   init_module_body( task, module, name );
   name = t_make_name( task, module_name.value, task->module->body_import );
   name->object = &module->object;
   // Read module.
   module->read = true;
   struct module* parent = task->module;
   struct module* parent_parent = task->module_importer;
   t_set_module( task, module );
   task->module_importer = parent;
   t_read_tk( task );
   read_module( task );
   task->module = parent;
   task->module_importer = parent_parent;
   // Add module into imported list:
   if ( module != task->module ) {
      list_iter_t i;
      list_iter_init( &i, &task->module->imported );
      while ( ! list_end( &i ) && module != list_data( &i ) ) {
         list_next( &i );
      }
      if ( list_end( &i ) ) {
         list_append( &task->module->imported, module );
      }
   }
   name->object = &module->object;
   finish:
   // -----------------------------------------------------------------------
   list_append( &stmt->modules, module );
   str_deinit( &module_name );
}

void t_print_name( struct name* name ) {
   struct str str;
   str_init( &str );
   t_copy_name( name, false, &str );
   printf( "%s\n", str.value );
}

void make_imported_visible( struct task* task, struct import* stmt ) {
   struct package* package = stmt->package;
   while ( package ) {
      ++package->ref_count;
      package = package->next;
   }
   list_iter_t i;
   list_iter_init( &i, &stmt->modules );
   struct module* module = NULL;
   while ( ! list_end( &i ) ) {
      module = list_data( &i );
      ++module->ref_count;
      list_next( &i );
   }
   // Create alias to the host module.
   if ( stmt->type == IMPORT_SELF ) {
      alias_imported_object( task, stmt->alias->text, &stmt->alias->pos,
         &task->module->object );
   }
   // Establish link with another module.
   else if ( stmt->type == IMPORT_LINK ) {
      make_module_link( task, module, &stmt->path->pos );
   }
   else if ( stmt->type == IMPORT_DIRECT ) {
      if ( stmt->package ) {
         // Alias to the first package is all that is needed. From the first
         // package, one can access the subpackages and modules.
         alias_imported_object( task, stmt->package->filename.value,
            &stmt->path->pos, &stmt->package->object );
      }
      else {
         struct path* path = stmt->path;
         while ( path->next ) {
            path = path->next;
         }
         alias_imported_object( task, module->name.value, &path->pos,
            &module->object );
      }
   }
   else if ( stmt->type == IMPORT_ALIAS ) {
      // A package cannot appear here because it is not possible to import a
      // package only. The error checked is performed during importing.
      alias_imported_object( task, stmt->alias->text, &stmt->alias->pos,
         &module->object );
   }
   else if ( stmt->type == IMPORT_SELECTIVE ) {
      if ( stmt->from_package ) {
         if ( stmt->alias ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &stmt->alias->pos, "alias specified for package" );
            t_bail( task );
         }
         import_package_objects( task, stmt );
      }
      else {
         // An alias to the imported module can be specified when doing a
         // selective import.
         if ( stmt->alias ) {
            alias_imported_object( task, stmt->alias->text,
               &stmt->alias->pos, &module->object );
         }
         import_objects( task, module, stmt );
      }
   }
}

void alias_imported_object( struct task* task, char* alias_name,
   struct pos* alias_name_pos, struct object* object ) {
   struct name* name = task->module->body;
   if ( object->node.type == NODE_TYPE ) {
      name = task->module->body_struct;
   }
   name = t_make_name( task, alias_name, name );
   if ( name->object ) {
      // Duplicate imports are allowed as long as both names refer to the
      // same object.
      bool valid = false;
      if ( name->object->node.type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) name->object;
         if ( object == alias->target ) {
            t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               alias_name_pos, "duplicate import name `%s`", alias_name );
            t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &alias->object.pos,
               "import name already used here", alias_name );
            valid = true;
         }
      }
      if ( ! valid ) {
         diag_dup( task, alias_name, alias_name_pos, name );
         t_bail( task );
      }
   }
   else {
      struct alias* alias = mem_alloc( sizeof( *alias ) );
      t_init_object( &alias->object, NODE_ALIAS );
      alias->object.pos = *alias_name_pos;
      alias->object.resolved = true;
      alias->target = object;
      if ( task->depth ) {
         t_use_local_name( task, name, &alias->object );
      }
      else {
         name->object = &alias->object;
      }
   }
}

void make_module_link( struct task* task, struct module* module,
   struct pos* pos ) {
   struct module_link* link = task->module->links;
   while ( link ) {
      if ( link->module == module ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
            "duplicate module link" );
         // t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &link->object.pos,
         //    "link to module first made here" );
         t_bail( task );
      }
      link = link->next;
   }
   // When searching for a name using module links, the link that is
   // parsed last will be the first link used in the search, then the
   // second-last, all the way to the first one parsed.
   link = mem_alloc( sizeof( *link ) );
   link->module = module;
   link->next = task->module->links;
   task->module->links = link;
}

void import_objects( struct task* task, struct module* module,
   struct import* stmt ) {
   struct import_item* item = stmt->items;
   while ( item ) {
      struct object* object = NULL;
      // Link item can only appear when importing modules.
      if ( item->is_link ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &item->pos,
            "making module link with object" );
         t_bail( task );
      }
      else if ( item->is_struct ) {
         struct name* name = t_make_name( task, item->name,
            module->body_struct );
         if ( ! name->object ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &item->pos,
               "struct `%s` not found in module", item->name );
            t_bail( task );
         }
         object = name->object;
      }
      else {
         struct name* name = t_make_name( task, item->name, module->body );
         object = t_get_module_object( task, module, name );
         if ( ! object ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &item->name_pos, "`%s` not found in module", item->name );
            t_bail( task );
         }
      }
      if ( item->alias ) {
         alias_imported_object( task, item->alias, &item->alias_pos, object );
      }
      else {
         alias_imported_object( task, item->name, &item->name_pos, object );
      }
      item = item->next;
   }
}

void import_package_objects( struct task* task, struct import* stmt ) {
   list_iter_t i;
   list_iter_init( &i, &stmt->modules );
   struct import_item* item = stmt->items;
   while ( item ) {
      struct module* module = list_data( &i );
      list_next( &i );
      if ( item->is_link ) {
         make_module_link( task, module, &item->name_pos );
      }
      else if ( item->is_struct ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &item->pos, "importing struct from package" );
         t_bail( task );
      }
      else {
         if ( item->alias ) {
            alias_imported_object( task, item->alias, &item->alias_pos,
               &module->object );
         }
         else {
            alias_imported_object( task, item->name, &item->name_pos,
               &module->object );
         }
      }
      item = item->next;
   }
}

// Gets the top-most object associated with the name, and only retrieves the
// object if it can be used by the current module.
struct object* t_get_module_object( struct task* task, struct module* module,
   struct name* name ) {
   struct object* object = name->object;
   if ( ! object ) {
      return NULL;
   }
   // Find the top-most object.
   while ( object && object->next_scope ) {
      object = object->next_scope;
   }
   if ( object->depth != 0 ) {
      return NULL;
   }
   if ( object->node.type == NODE_CONSTANT ) {
      struct constant* constant = ( struct constant* ) object;
      if ( ! constant->visible ) {
         return NULL;
      }
   }
   // Hidden variables and functions cannot be used.
   else if ( object->node.type == NODE_VAR ) {
      struct var* var = ( struct var* ) object;
      if ( var->hidden ) {
         if ( module != task->module ) {
            return NULL;
         }
      }
   }
   else if ( object->node.type == NODE_FUNC ) {
      struct func* func = ( struct func* ) object;
      if ( func->hidden ) {
         if ( module != task->module ) {
            return NULL;
         }
      }
   }
   // Modules must be visible.
   else if ( object->node.type == NODE_MODULE ) {
      struct module* module = ( struct module* ) object;
      if ( ! module->ref_count ) {
         return NULL;
      }
   }
   return object;
}

void mark_published( struct module* published, const char* lump ) {
   published->publish = true;
   list_iter_t i;
   list_iter_init( &i, &published->imported );
   while ( ! list_end( &i ) ) {
      struct module* module = list_data( &i );
      if ( ! module->publish && ! module->dynamic ) {
         if ( ! module->lump.length ) {
            mark_published( module, lump );
         }
         else if ( strcmp( module->lump.value, lump ) == 0 ) {
            mark_published( module, lump );
         }
         else {
            module->dynamic = true;
         }
      }
      list_next( &i );
   }
}

void append_path( struct str* str, struct path* path ) {
   while ( path ) {
      str_append( str, path->text );
      path = path->next;
      if ( path ) {
         str_append( str, "/" );
      }
   }
}

void read_dirc( struct task* task, struct pos* pos ) {
   bool import = false;
   if ( task->tk == TK_IMPORT ) {
      import = true;
      t_read_tk( task );
   }
   else {
      t_test_tk( task, TK_ID );
      if ( strcmp( task->tk_text, "include" ) == 0 ) {
         import = true;
         t_read_tk( task );
      }
   }
   // In this compiler, #include and #import do the same thing, that being
   // establishing a link with another module.
   if ( import ) {
      t_test_tk( task, TK_LIT_STRING );
      struct path* path = mem_alloc( sizeof( *path ) );
      path->next = NULL;
      path->text = task->tk_text;
      path->pos = task->tk_pos;
      struct import* stmt = alloc_import();
      stmt->type = IMPORT_LINK;
      stmt->path = path;
      stmt->path_file = true;
      list_append( &task->module->imports, stmt );
      t_read_tk( task );
/*
      struct str path;
      str_init( &path );
      str_grow( &path, PATH_MAX + 1 );
      if ( ! realpath( task->tk_text, path.value ) ) {
         t_diag( task, DIAG_ERR, "failed to load module: %s", task->tk_text );
         t_bail( task );
      }
      path.length = strlen( path.value );
      struct str filename;
      str_init( &filename );
      get_filename( path.value, &filename );
      struct module* module = t_load_module( task, path.value, filename.value );
      if ( ! module ) {
         t_diag( task, DIAG_ERR, "failed to load module: %s", task->tk_text );
         t_bail( task );
      }
      struct module* parent = task->module;
      task->module = module;
      get_dirname( &path );
      struct list queries;
      list_init( &queries );
      find_parent_packages( &path, &queries );
      create_packages_name( module, &queries );
      free_package_queries( &queries );
      str_deinit( &path );
      str_deinit( &filename );
      list_deinit( &queries );
      t_read_tk( task );
      read_module( task );
      task->module = parent;
      make_module_link( task, module, pos );
      t_read_tk( task );*/
   }
   else if ( strcmp( task->tk_text, "define" ) == 0 ||
      strcmp( task->tk_text, "libdefine" ) == 0 ) {
      t_read_define( task );
   }
   else if ( strcmp( task->tk_text, "library" ) == 0 ) {
      t_read_tk( task );
      t_test_tk( task, TK_LIT_STRING );
      t_read_tk( task );
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
         "#library directive after other code" );
      t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
         "#library directive must appear at the very top, "
         "before any other code" );
      t_bail( task );
   }
   else if ( strcmp( task->tk_text, "encryptstrings" ) == 0 ) {
      t_read_tk( task );
      // This directive is only effective from the main module.
      if ( task->module == task->module_main ) {
         task->options->encrypt_str = true;
      }
   }
   else if ( strcmp( task->tk_text, "macro" ) == 0 ) {
      t_read_tk( task );
      t_test_tk( task, TK_ID );
      t_read_tk( task );
      t_test_tk( task, TK_LIT_DECIMAL );
      t_read_tk( task );
   }
   else if (
      // NOTE: Not sure what these two are. 
      strcmp( task->tk_text, "wadauthor" ) == 0 ||
      strcmp( task->tk_text, "nowadauthor" ) == 0 ||
      // For now, the output format will be selected automatically.
      strcmp( task->tk_text, "nocompact" ) == 0 ) {
      t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
         "directive `%s` not supported", task->tk_text );
      t_read_tk( task );
   }
   else {
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
         "unknown directive '%s'", task->tk_text );
      t_bail( task );
   }
}

void t_init_stmt_read( struct stmt_read* read ) {
   read->block = NULL;
   read->node = NULL;
   read->labels = NULL;
   read->depth = 0;
}

void t_read_block( struct task* task, struct stmt_read* read ) {
   t_test_tk( task, TK_BRACE_L );
   t_read_tk( task );
   ++read->depth;
   struct block* block = mem_alloc( sizeof( *block ) );
   block->node.type = NODE_BLOCK;
   list_init( &block->stmts );
   while ( true ) {
      if ( t_is_dec( task ) ) {
         struct dec dec;
         t_init_dec( &dec );
         dec.area = DEC_LOCAL;
         dec.name_offset = task->module->body;
         dec.vars = &block->stmts;
         t_read_dec( task, &dec );
      }
      else if ( task->tk == TK_CASE ) {
         struct case_label* label = mem_alloc( sizeof( *label ) );
         label->node.type = NODE_CASE;
         label->offset = 0;
         label->next = NULL;
         label->pos = task->tk_pos;
         t_read_tk( task );
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         label->expr = expr.node;
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
         list_append( &block->stmts, label );
      }
      else if ( task->tk == TK_DEFAULT ) {
         struct case_label* label = mem_alloc( sizeof( *label ) );
         label->node.type = NODE_CASE_DEFAULT;
         label->pos = task->tk_pos;
         label->offset = 0;
         t_read_tk( task );
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
         list_append( &block->stmts, label );
      }
      // goto label:
      else if ( task->tk == TK_ID && t_peek( task ) == TK_COLON ) {
         struct label* label = NULL;
         list_iter_t i;
         list_iter_init( &i, read->labels );
         while ( ! list_end( &i ) ) {
            struct label* prev = list_data( &i );
            if ( strcmp( prev->name, task->tk_text ) == 0 ) {
               label = prev;
               break;
            }
            list_next( &i );
         }
         if ( label ) {
            if ( label->defined ) {
               t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &task->tk_pos, "duplicate label '%s'", task->tk_text );
               t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                  "label '%s' is found here", task->tk_text );
               t_bail( task );
            }
            else {
               label->defined = true;
               label->pos = task->tk_pos;
            }
         }
         else {
            label = new_label( task->tk_text, task->tk_pos );
            label->defined = true;
            list_append( read->labels, label );
         }
         t_read_tk( task );
         t_read_tk( task );
         list_append( &block->stmts, label );
      }
      else if ( task->tk == TK_IMPORT ) {
         read_import( task, &block->stmts );
      }
      else if ( task->tk == TK_FORMAT ) {
         struct format_block* format_block = mem_alloc(
            sizeof( *format_block ) );
         t_init_object( &format_block->object, NODE_FORMAT_BLOCK );
         format_block->object.pos = task->tk_pos;
         t_read_tk( task );
         t_test_tk( task, TK_ID );
         format_block->name = t_make_name( task, task->tk_text,
            task->module->body );
         t_read_tk( task );
         t_read_block( task, read );
         format_block->block = read->block;
         list_append( &block->stmts, format_block );
      }
      else if ( task->tk == TK_BRACE_R ) {
         t_read_tk( task );
         break;
      }
      else {
         read_stmt( task, read );
         if ( read->node->type != NODE_NONE ) {
            list_append( &block->stmts, read->node );
         }
      }
   }
   read->node = ( struct node* ) block;
   read->block = block;
   --read->depth;
   if ( read->depth == 0 ) {
      // All goto statements need to refer to valid labels.
      list_iter_t i;
      list_iter_init( &i, read->labels );
      while ( ! list_end( &i ) ) {
         struct label* label = list_data( &i );
         if ( ! label->defined ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
               "label '%s' not found", label->name );
            t_bail( task );
         }
         list_next( &i );
      }
   }
}

void read_stmt( struct task* task, struct stmt_read* read ) {
   if ( task->tk == TK_BRACE_L ) {
      t_read_block( task, read );
   }
   else if ( task->tk == TK_IF ) {
      t_read_tk( task );
      struct if_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_IF;
      t_test_tk( task, TK_PAREN_L );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->expr = expr.node;
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      read_stmt( task, read );
      stmt->body = read->node;
      stmt->else_body = NULL;
      if ( task->tk == TK_ELSE ) {
         t_read_tk( task );
         read_stmt( task, read );
         stmt->else_body = read->node;
      }
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_SWITCH ) {
      t_read_tk( task );
      struct switch_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_SWITCH;
      t_test_tk( task, TK_PAREN_L );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->expr = expr.node;
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      read_stmt( task, read );
      stmt->body = read->node;
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_WHILE || task->tk == TK_UNTIL ) {
      struct while_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_WHILE;
      stmt->type = WHILE_WHILE;
      if ( task->tk == TK_WHILE ) {
         t_read_tk( task );
      }
      else {
         t_test_tk( task, TK_UNTIL );
         t_read_tk( task );
         stmt->type = WHILE_UNTIL;
      }
      t_test_tk( task, TK_PAREN_L );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->expr = expr.node;
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      read_stmt( task, read );
      stmt->body = read->node;
      stmt->jump_break = NULL;
      stmt->jump_continue = NULL;
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_DO ) {
      t_read_tk( task );
      struct while_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_WHILE;
      stmt->type = WHILE_DO_WHILE;
      read_stmt( task, read );
      stmt->body = read->node;
      stmt->jump_break = NULL;
      stmt->jump_continue = NULL;
      if ( task->tk == TK_WHILE ) {
         t_read_tk( task );
      }
      else {
         t_test_tk( task, TK_UNTIL );
         t_read_tk( task );
         stmt->type = WHILE_DO_UNTIL;
      }
      t_test_tk( task, TK_PAREN_L );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->expr = expr.node;
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_FOR ) {
      t_read_tk( task );
      struct for_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_FOR;
      stmt->init = NULL;
      list_init( &stmt->vars );
      stmt->expr = NULL;
      stmt->post = NULL;
      stmt->body = NULL;
      stmt->jump_break = NULL;
      stmt->jump_continue = NULL;
      t_test_tk( task, TK_PAREN_L );
      t_read_tk( task );
      // Optional initialization:
      if ( task->tk != TK_SEMICOLON ) {
         if ( t_is_dec( task ) ) {
            struct dec dec;
            t_init_dec( &dec );
            dec.area = DEC_FOR;
            dec.name_offset = task->module->body;
            dec.vars = &stmt->vars;
            t_read_dec( task, &dec );
         }
         else {
            struct expr_link* tail = NULL;
            while ( true ) {
               struct expr_link* link = mem_alloc( sizeof( *link ) );
               link->node.type = NODE_EXPR_LINK;
               link->next = NULL;
               struct read_expr expr;
               t_init_read_expr( &expr );
               t_read_expr( task, &expr );
               link->expr = expr.node;
               if ( tail ) {
                  tail->next = link;
               }
               else {
                  stmt->init = link;
               }
               tail = link;
               if ( task->tk == TK_COMMA ) {
                  t_read_tk( task );
               }
               else {
                  break;
               }
            }
            t_test_tk( task, TK_SEMICOLON );
            t_read_tk( task );
         }
      }
      else {
         t_read_tk( task );
      }
      // Optional condition:
      if ( task->tk != TK_SEMICOLON ) {
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         stmt->expr = expr.node;
         t_test_tk( task, TK_SEMICOLON );
         t_read_tk( task );
      }
      else {
         t_read_tk( task );
      }
      // Optional post-expression:
      if ( task->tk != TK_PAREN_R ) {
         struct expr_link* tail = NULL;
         while ( true ) {
            struct expr_link* link = mem_alloc( sizeof( *link ) );
            link->node.type = NODE_EXPR_LINK;
            link->next = NULL;
            struct read_expr expr;
            t_init_read_expr( &expr );
            t_read_expr( task, &expr );
            link->expr = expr.node;
            if ( tail ) {
               tail->next = link;
            }
            else {
               stmt->post = link;
            }
            tail = link;
            if ( task->tk == TK_COMMA ) {
               t_read_tk( task );
            }
            else {
               break;
            }
         }
      }
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      read_stmt( task, read );
      stmt->body = read->node;
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_BREAK || task->tk == TK_CONTINUE ) {
      struct jump* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_JUMP;
      stmt->type = JUMP_BREAK;
      stmt->next = NULL;
      stmt->pos = task->tk_pos;
      stmt->obj_pos = 0;
      if ( task->tk == TK_CONTINUE ) {
         stmt->type = JUMP_CONTINUE;
      }
      t_read_tk( task );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_TERMINATE || task->tk == TK_RESTART ||
      task->tk == TK_SUSPEND ) {
      struct script_jump* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_SCRIPT_JUMP;
      stmt->type = SCRIPT_JUMP_TERMINATE;
      stmt->pos = task->tk_pos;
      if ( task->tk == TK_RESTART ) {
         stmt->type = SCRIPT_JUMP_RESTART;
      }
      else if ( task->tk == TK_SUSPEND ) {
         stmt->type = SCRIPT_JUMP_SUSPEND;
      }
      t_read_tk( task );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_RETURN ) {
      struct return_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_RETURN;
      stmt->expr = NULL;
      stmt->expr_format = NULL;
      stmt->pos = task->tk_pos;
      t_read_tk( task );
      if ( task->tk != TK_SEMICOLON ) {
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         if ( task->tk == TK_ASSIGN_COLON ) {
            t_read_tk( task );
            t_read_block( task, read );
            struct format_expr* node = mem_alloc( sizeof( *node ) );
            node->node.type = NODE_FORMAT_EXPR;
            node->expr = expr.node;
            node->format_block = read->block;
            stmt->expr_format = node;
         }
         else {
            stmt->expr = expr.node;
            t_test_tk( task, TK_SEMICOLON );
         }
      }
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_GOTO ) {
      struct pos pos = task->tk_pos;
      t_read_tk( task );
      t_test_tk( task, TK_ID );
      struct label* label = NULL;
      list_iter_t i;
      list_iter_init( &i, read->labels );
      while ( ! list_end( &i ) ) {
         struct label* prev = list_data( &i );
         if ( strcmp( prev->name, task->tk_text ) == 0 ) {
            label = prev;
            break;
         }
         list_next( &i );
      }
      if ( ! label ) {
         label = new_label( task->tk_text, task->tk_pos );
         list_append( read->labels, label );
      }
      t_read_tk( task );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      struct goto_stmt* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_GOTO;
      stmt->label = label;
      stmt->next = label->stmts;
      label->stmts = stmt;
      stmt->obj_pos = 0;
      stmt->format_block = NULL;
      stmt->pos = pos;
      read->node = ( struct node* ) stmt;
   }
   else if ( task->tk == TK_PALTRANS ) {
      t_read_tk( task );
      struct paltrans* stmt = mem_alloc( sizeof( *stmt ) );
      stmt->node.type = NODE_PALTRANS;
      stmt->ranges = NULL;
      stmt->ranges_tail = NULL;
      t_test_tk( task, TK_PAREN_L );
      t_read_tk( task );
      struct read_expr expr;
      t_init_read_expr( &expr );
      t_read_expr( task, &expr );
      stmt->number = expr.node;
      while ( task->tk == TK_COMMA ) {
         struct palrange* range = mem_alloc( sizeof( *range ) );
         range->next = NULL;
         t_read_tk( task );
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         range->begin = expr.node;
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
         t_init_read_expr( &expr );
         expr.skip_assign = true;
         t_read_expr( task, &expr );
         range->end = expr.node;
         t_test_tk( task, TK_ASSIGN );
         t_read_tk( task );
         if ( task->tk == TK_BRACKET_L ) {
            read_palrange_rgb_field( task, &range->value.rgb.red1,
               &range->value.rgb.green1, &range->value.rgb.blue1 );
            t_test_tk( task, TK_COLON );
            t_read_tk( task );
            read_palrange_rgb_field( task, &range->value.rgb.red2,
               &range->value.rgb.green2, &range->value.rgb.blue2 );
            range->rgb = true;
         }
         else {
            t_init_read_expr( &expr );
            t_read_expr( task, &expr );
            range->value.ent.begin = expr.node;
            t_test_tk( task, TK_COLON );
            t_read_tk( task );
            t_init_read_expr( &expr );
            t_read_expr( task, &expr );
            range->value.ent.end = expr.node;
            range->rgb = false;
         }
         if ( stmt->ranges ) {
            stmt->ranges_tail->next = range;
            stmt->ranges_tail = range;
         }
         else {
            stmt->ranges = range;
            stmt->ranges_tail = range;
         }
      }
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
      read->node = ( struct node* ) stmt;
   }
   // Format item:
   else if ( task->tk == TK_ID && t_peek( task ) == TK_ASSIGN_COLON ) {
      read->node = ( struct node* )
         t_read_format_item( task, TK_ASSIGN_COLON );
      t_test_tk( task, TK_SEMICOLON );
      t_read_tk( task );
   }
   // Inline assembly.
   else if ( task->tk == TK_GT ) {
      read_pcode( task, read );
   }
   else if ( task->tk == TK_SEMICOLON ) {
      static struct node empty = { NODE_NONE };
      read->node = &empty;
      t_read_tk( task );
   }
   else {
      struct read_expr expr;
      t_init_read_expr( &expr );
      expr.stmt_read = read;
      t_read_expr( task, &expr );
      // Format block.
      if ( task->tk == TK_ASSIGN_COLON ) {
         t_read_tk( task );
         t_read_block( task, read );
         struct format_expr* node = mem_alloc( sizeof( *node ) );
         node->node.type = NODE_FORMAT_EXPR;
         node->expr = expr.node;
         node->format_block = read->block;
         read->node = &node->node;
      }
      else {
         t_test_tk( task, TK_SEMICOLON );
         t_read_tk( task );
         read->node = ( struct node* ) expr.node;
      }
   }
}

struct label* new_label( char* name, struct pos pos ) {
   struct label* label = mem_alloc( sizeof( *label ) );
   label->node.type = NODE_GOTO_LABEL;
   label->name = name;
   label->defined = false;
   label->pos = pos;
   label->stmts = NULL;
   label->obj_pos = 0;
   label->format_block = NULL;
   list_init( &label->users );
   return label;
}

void read_palrange_rgb_field( struct task* task, struct expr** r,
   struct expr** g, struct expr** b ) {
   t_test_tk( task, TK_BRACKET_L );
   t_read_tk( task );
   struct read_expr expr;
   t_init_read_expr( &expr );
   t_read_expr( task, &expr );
   *r = expr.node;
   t_test_tk( task, TK_COMMA );
   t_read_tk( task );
   t_init_read_expr( &expr );
   t_read_expr( task, &expr );
   *g = expr.node;
   t_test_tk( task, TK_COMMA );
   t_read_tk( task );
   t_init_read_expr( &expr );
   t_read_expr( task, &expr );
   *b = expr.node;
   t_test_tk( task, TK_BRACKET_R );
   t_read_tk( task ); 
}

void read_pcode( struct task* task, struct stmt_read* read ) {
   t_read_tk( task );
   task->newline_tk = true;
   struct pcode* pcode = mem_alloc( sizeof( *pcode ) );
   pcode->node.type = NODE_PCODE;
   pcode->opcode = PC_NONE;
   list_init( &pcode->args );
   // Opcode.
   t_test_tk( task, TK_ID );
   t_read_tk( task );
   // pcode->opcode = expr.node;
   // Arguments.
   while ( true ) {
      // Path argument.
      if ( task->tk == TK_ID ) {
         t_read_tk( task );
      }
      // Literal argument.
      else if (
         task->tk == TK_LIT_DECIMAL ||
         task->tk == TK_LIT_STRING ) {
         t_read_tk( task );
      }
      // Macro.
      else if ( task->tk == TK_MOD ) {
         t_read_tk( task );
         t_test_tk( task, TK_ID );
         //if ( strcmp( task->tk_text, "offset" ) != 0 ) {
         //   printf( "bad macro\n" );
         //   t_bail( task );
         // }
         t_read_tk( task );
         t_test_tk( task, TK_PAREN_L );
         t_read_tk( task );
         t_test_tk( task, TK_ID );
/*
   struct label* label = NULL;
   list_iter_t i;
   list_iter_init( &i, read->labels );
   while ( ! list_end( &i ) ) {
      struct label* prev = list_data( &i );
      if ( strcmp( prev->name, task->tk_text ) == 0 ) {
         label = prev;
         break;
      }
      list_next( &i );
   }
   if ( ! label ) {
      label = new_label( task->tk_text, task->tk_pos );
      list_append( read->labels, label );
   }

         struct pcode_offset* macro = mem_alloc( sizeof( *macro ) );
         macro->node.type = NODE_PCODE_OFFSET;
         macro->label = label;
         macro->obj_pos = 0;
         list_append( &pcode->args, macro );
         list_append( &label->users, macro ); */
         t_read_tk( task );
         t_test_tk( task, TK_PAREN_R );
         t_read_tk( task );
      }
      else if ( task->tk == TK_PAREN_L ) {
         t_read_tk( task );
         struct read_expr expr;
         t_init_read_expr( &expr );
         t_read_expr( task, &expr );
         list_append( &pcode->args, expr.node );
         t_test_tk( task, TK_PAREN_R );
         t_read_tk( task );
      }
      else {
         break;
      }
      if ( task->tk == TK_COMMA ) {
         t_read_tk( task );
      }
      else {
         break;
      }
   }
   t_test_tk( task, TK_NL );
   t_read_tk( task );
   read->node = &pcode->node;
   task->newline_tk = false;
}

void t_skip_semicolon( struct task* task ) {
   while ( task->tk != TK_SEMICOLON ) {
      t_read_tk( task );
   }
   t_read_tk( task );
}

void t_skip_past_tk( struct task* task, enum tk needed ) {
   while ( true ) {
      if ( task->tk == needed ) {
         t_read_tk( task );
         break;
      }
      else if ( task->tk == TK_END ) {
         t_bail( task );
      }
      else {
         t_read_tk( task );
      }
   }
}

void t_skip_block( struct task* task ) {
   while ( true ) {
      if ( task->tk == TK_BRACE_L ) {
         t_read_tk( task );
         break;
      }
      else {
         t_read_tk( task );
      }
   }
   int depth = 1;
   while ( true ) {
      if ( task->tk == TK_BRACE_L ) {
         ++depth;
         t_read_tk( task );
      }
      else if ( task->tk == TK_BRACE_R ) {
         --depth;
         t_read_tk( task );
         if ( ! depth ) {
            break;
         }
      }
      else if ( task->tk == TK_END ) {
         t_bail( task );
      }
      else {
         t_read_tk( task );
      }
   }
}

void t_skip_to_tk( struct task* task, enum tk tk ) {
   while ( true ) {
      if ( task->tk == tk ) {
         break;
      }
      t_read_tk( task );
   }
}

void t_init_stmt_test( struct stmt_test* test, struct stmt_test* parent ) {
   test->parent = parent;
   test->func = NULL;
   test->labels = NULL;
   test->format_block = NULL;
   test->case_head = NULL;
   test->case_default = NULL;
   test->jump_break = NULL;
   test->jump_continue = NULL;
   test->imports = NULL;
   test->in_loop = false;
   test->in_switch = false;
   test->in_script = false;
   test->manual_scope = false;
}

void t_test_block( struct task* task, struct stmt_test* test,
   struct block* block ) {
   if ( ! test->manual_scope ) {
      t_add_scope( task );
   }
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      struct stmt_test nested;
      t_init_stmt_test( &nested, test );
      test_block_item( task, &nested, list_data( &i ) );
      list_next( &i );
   }
   if ( ! test->manual_scope ) {
      t_pop_scope( task );
   }
   if ( ! test->parent ) {
      test_goto_in_format_block( task, test->labels );
   }
   // Reduce usage of imported packages and modules. When an object is no
   // longer used, remove it from visibility.
   if ( test->imports ) {
      struct import* stmt = test->imports;
      while ( stmt ) {
         struct package* package = stmt->package;
         while ( package ) {
            --package->ref_count;
            if ( ! package->ref_count ) {
               package->name->object = NULL;
               break;
            }
            package = package->next;
         }
         list_iter_t i;
         list_iter_init( &i, &stmt->modules );
         while ( ! list_end( &i ) ) {
            struct module* module = list_data( &i );
            --module->ref_count;
            list_next( &i );
         }
         stmt = stmt->next;
      }
   }
}

void test_block_item( struct task* task, struct stmt_test* test,
   struct node* node ) {
   if ( node->type == NODE_CONSTANT ) {
      t_test_constant( task, ( struct constant* ) node, true );
   }
   else if ( node->type == NODE_CONSTANT_SET ) {
      t_test_constant_set( task, ( struct constant_set* ) node, true );
   }
   else if ( node->type == NODE_TYPE ) {
      t_test_type( task, ( struct type* ) node, true );
   }
   else if ( node->type == NODE_VAR ) {
      t_test_local_var( task, ( struct var* ) node );
   }
   else if ( node->type == NODE_CASE ) {
      struct case_label* label = ( struct case_label* ) node;
      struct stmt_test* target = test;
      while ( target && ! target->in_switch ) {
         target = target->parent;
      }
      if ( ! target ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "case outside switch statement" );
         t_bail( task );
      }
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, label->expr );
      if ( ! label->expr->folded ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "case value not constant" );
         t_bail( task );
      }
      struct case_label* prev = NULL;
      struct case_label* curr = target->case_head;
      while ( curr && curr->expr->value < label->expr->value ) {
         prev = curr;
         curr = curr->next;
      }
      if ( curr && curr->expr->value == label->expr->value ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "duplicate case" );
         t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &curr->pos,
            "case with value %d is found here", label->expr->value );
         t_bail( task );
      }
      if ( prev ) {
         label->next = prev->next;
         prev->next = label;
      }
      else {
         label->next = target->case_head;
         target->case_head = label;
      }
   }
   else if ( node->type == NODE_CASE_DEFAULT ) {
      struct case_label* label = ( struct case_label* ) node;
      struct stmt_test* target = test;
      while ( target && ! target->in_switch ) {
         target = target->parent;
      }
      if ( ! target ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "default outside switch statement" );
         t_bail( task );
      }
      else if ( target->case_default ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
            "duplicate default case" );
         t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &target->case_default->pos,
            "default case is found here" );
         t_bail( task );
      }
      target->case_default = label;
   }
   else if ( node->type == NODE_GOTO_LABEL ) {
      struct stmt_test* target = test;
      while ( target ) {
         if ( target->format_block ) {
            struct label* label = ( struct label* ) node;
            label->format_block = target->format_block;
            break;
         }
         target = target->parent;
      }
   }
   else if ( node->type == NODE_FORMAT_ITEM ) {
      t_test_format_item( task, ( struct format_item* ) node, test,
         task->module->body, NULL );
      struct stmt_test* target = test;
      while ( target && ! target->format_block ) {
         target = target->parent;
      }
      if ( ! target ) {
         struct format_item* item = ( struct format_item* ) node;
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &item->pos,
            "format item outside format block" );
         t_bail( task );
      }
   }
   else if ( node->type == NODE_IMPORT ) {
      struct import* stmt = ( struct import* ) node;
      stmt->next = test->parent->imports;
      test->parent->imports = stmt;
      make_imported_visible( task, stmt );
   }
   else if ( node->type == NODE_FORMAT_BLOCK ) {
      struct format_block* format_block = ( struct format_block* ) node;
      struct stmt_test stmt_test;
      t_init_stmt_test( &stmt_test, test );
      stmt_test.format_block = format_block->block;
      t_test_block( task, &stmt_test, format_block->block );
      format_block->object.resolved = true;
      // TODO: Check name is unique.
      t_use_local_name( task, format_block->name, &format_block->object );
   }
   else {
      test_stmt( task, test, node );
   }
}

void test_stmt( struct task* task, struct stmt_test* test,
   struct node* node ) {
   if ( node->type == NODE_BLOCK ) {
      t_test_block( task, test, ( struct block* ) node );
   }
   else if ( node->type == NODE_IF ) {
      struct if_stmt* stmt = ( struct if_stmt* ) node;
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, stmt->expr );
      struct stmt_test body;
      t_init_stmt_test( &body, test );
      test_stmt( task, &body, stmt->body );
      if ( stmt->else_body ) {
         t_init_stmt_test( &body, test );
         test_stmt( task, &body, stmt->else_body );
      }
   }
   else if ( node->type == NODE_SWITCH ) {
      struct switch_stmt* stmt = ( struct switch_stmt* ) node;
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, stmt->expr );
      struct stmt_test body;
      t_init_stmt_test( &body, test );
      body.in_switch = true;
      test_stmt( task, &body, stmt->body );
      stmt->case_head = body.case_head;
      stmt->case_default = body.case_default;
      stmt->jump_break = body.jump_break;
   }
   else if ( node->type == NODE_WHILE ) {
      struct while_stmt* stmt = ( struct while_stmt* ) node;
      if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, stmt->expr );
      }
      struct stmt_test body;
      t_init_stmt_test( &body, test );
      body.in_loop = true;
      test_stmt( task, &body, stmt->body );
      stmt->jump_break = body.jump_break;
      stmt->jump_continue = body.jump_continue;
      if ( stmt->type == WHILE_DO_WHILE || stmt->type == WHILE_DO_UNTIL ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, stmt->expr );
      }
   }
   else if ( node->type == NODE_FOR ) {
      struct for_stmt* stmt = ( struct for_stmt* ) node;
      t_add_scope( task );
      if ( stmt->init ) {
         struct expr_link* link = stmt->init;
         while ( link ) {
            struct expr_test expr;
            t_init_expr_test( &expr );
            expr.undef_err = true;
            expr.needed = false;
            t_test_expr( task, &expr, link->expr );
            link = link->next;
         }
      }
      else if ( list_size( &stmt->vars ) ) {
         list_iter_t i;
         list_iter_init( &i, &stmt->vars );
         while ( ! list_end( &i ) ) {
            struct node* node = list_data( &i );
            if ( node->type == NODE_VAR ) {
               t_test_local_var( task, ( struct var* ) node );
            }
            list_next( &i );
         }
      }
      if ( stmt->expr ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, stmt->expr );
      }
      if ( stmt->post ) {
         struct expr_link* link = stmt->post;
         while ( link ) {
            struct expr_test expr;
            t_init_expr_test( &expr );
            expr.undef_err = true;
            expr.needed = false;
            t_test_expr( task, &expr, link->expr );
            link = link->next;
         }
      }
      struct stmt_test body;
      t_init_stmt_test( &body, test );
      body.in_loop = true;
      test_stmt( task, &body, stmt->body );
      stmt->jump_break = body.jump_break;
      stmt->jump_continue = body.jump_continue;
      t_pop_scope( task );
   }
   else if ( node->type == NODE_JUMP ) {
      struct jump* stmt = ( struct jump* ) node;
      if ( stmt->type == JUMP_BREAK ) {
         struct stmt_test* target = test;
         while ( target && ! target->in_loop && ! target->in_switch ) {
            target = target->parent;
         }
         if ( ! target ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
               "break outside loop or switch" );
            t_bail( task );
         }
         stmt->next = target->jump_break;
         target->jump_break = stmt;
         // Jumping out of a format block is not allowed.
         struct stmt_test* finish = target;
         target = test;
         while ( target != finish ) {
            if ( target->format_block ) {
               t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &stmt->pos,
                  "leaving format block with a break statement" );
               t_bail( task );
            }
            target = target->parent;
         }
      }
      else {
         struct stmt_test* target = test;
         while ( target && ! target->in_loop ) {
            target = target->parent;
         }
         if ( ! target ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
               "continue outside loop" );
            t_bail( task );
         }
         stmt->next = target->jump_continue;
         target->jump_continue = stmt;
         struct stmt_test* finish = target;
         target = test;
         while ( target != finish ) {
            if ( target->format_block ) {
               t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &stmt->pos,
                  "leaving format block with a continue statement" );
               t_bail( task );
            }
            target = target->parent;
         }
      }
   }
   else if ( node->type == NODE_SCRIPT_JUMP ) {
      static const char* names[] = { "terminate", "restart", "suspend" };
      STATIC_ASSERT( ARRAY_SIZE( names ) == SCRIPT_JUMP_TOTAL );
      struct stmt_test* target = test;
      while ( target && ! target->in_script ) {
         target = target->parent;
      }
      if ( ! target ) {
         struct script_jump* stmt = ( struct script_jump* ) node;
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
            "%s statement outside script", names[ stmt->type ] );
         t_bail( task );
      }
      struct stmt_test* finish = target;
      target = test;
      while ( target != finish ) {
         if ( target->format_block ) {
            struct script_jump* stmt = ( struct script_jump* ) node;
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &stmt->pos,
               "%s statement inside format block", names[ stmt->type ] );
            t_bail( task );
         }
         target = target->parent;
      }
   }
   else if ( node->type == NODE_RETURN ) {
      struct return_stmt* stmt = ( struct return_stmt* ) node;
      struct stmt_test* target = test;
      while ( target && ! target->func ) {
         target = target->parent;
      }
      if ( ! target ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos, 
            "return statement outside function" );
         t_bail( task );
      }
      if ( stmt->expr ) {
         struct expr_test expr;
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, stmt->expr );
         if ( ! target->func->value ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
               "returning value in void function" );
            t_bail( task );
         }
      }
      else if ( stmt->expr_format ) {
         struct expr_test expr_test;
         t_init_expr_test( &expr_test );
         expr_test.stmt_test = test;
         expr_test.undef_err = true;
         expr_test.needed = false;
         expr_test.format_block = stmt->expr_format->format_block;
         t_test_expr( task, &expr_test, stmt->expr_format->expr );
      }
      else if ( target->func->value ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &stmt->pos,
            "missing return value" );
         t_bail( task );
      }
      struct stmt_test* finish = target;
      target = test;
      while ( target != finish ) {
         if ( target->format_block ) {
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &stmt->pos,
               "leaving format block with a return statement" );
            t_bail( task );
         }
         target = target->parent;
      }
   }
   else if ( node->type == NODE_GOTO ) {
      struct stmt_test* target = test;
      while ( target ) {
         if ( target->format_block ) {
            struct goto_stmt* stmt = ( struct goto_stmt* ) node;
            stmt->format_block = target->format_block;
            break;
         }
         target = target->parent;
      }
   }
   else if ( node->type == NODE_PALTRANS ) {
      struct paltrans* stmt = ( struct paltrans* ) node;
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.undef_err = true;
      t_test_expr( task, &expr, stmt->number );
      struct palrange* range = stmt->ranges;
      while ( range ) {
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->begin );
         t_init_expr_test( &expr );
         expr.undef_err = true;
         t_test_expr( task, &expr, range->end );
         if ( range->rgb ) {
            t_init_expr_test( &expr );
            expr.undef_err = true;
            t_test_expr( task, &expr, range->value.rgb.red1 );
            t_init_expr_test( &expr );
            expr.undef_err = true;
            t_test_expr( task, &expr, range->value.rgb.green1 );
            t_init_expr_test( &expr );
            expr.undef_err = true;
            t_test_expr( task, &expr, range->value.rgb.blue1 );
            t_init_expr_test( &expr );
            expr.undef_err = true;
            t_test_expr( task, &expr, range->value.rgb.red2 );
            t_init_expr_test( &expr );
            expr.undef_err = true;
            t_test_expr( task, &expr, range->value.rgb.green2 );
            t_init_expr_test( &expr );
            expr.undef_err = true;
            t_test_expr( task, &expr, range->value.rgb.blue2 );
         }
         else {
            t_init_expr_test( &expr );
            expr.undef_err = true;
            t_test_expr( task, &expr, range->value.ent.begin );
            t_init_expr_test( &expr );
            expr.undef_err = true;
            t_test_expr( task, &expr, range->value.ent.end );
         }
         range = range->next;
      }
   }
   else if ( node->type == NODE_EXPR ) {
      struct expr_test expr;
      t_init_expr_test( &expr );
      expr.stmt_test = test;
      expr.undef_err = true;
      expr.needed = false;
      t_test_expr( task, &expr, ( struct expr* ) node );
   }
   else if ( node->type == NODE_FORMAT_EXPR ) {
      struct format_expr* format_expr = ( struct format_expr* ) node;
      struct expr_test expr_test;
      t_init_expr_test( &expr_test );
      expr_test.stmt_test = test;
      expr_test.undef_err = true;
      expr_test.needed = false;
      expr_test.format_block = format_expr->format_block;
      t_test_expr( task, &expr_test, format_expr->expr );
   }
   else if ( node->type == NODE_PCODE ) {
      struct pcode* pcode = ( struct pcode* ) node;
      struct expr_test expr_test;
      t_init_expr_test( &expr_test );
      expr_test.undef_err = true;
      expr_test.needed = true;
      t_test_expr( task, &expr_test, pcode->opcode );
      if ( ! pcode->opcode->folded ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr_test.pos,
            "instruction opcode not constant" );
         t_bail( task );
      }
      list_iter_t i;
      list_iter_init( &i, &pcode->args );
      while ( ! list_end( &i ) ) {
         struct node* node = list_data( &i );
         if ( node->type == NODE_PCODE_OFFSET ) {

         }
         else {
            struct expr* arg = list_data( &i );
            t_init_expr_test( &expr_test );
            expr_test.undef_err = true;
            expr_test.needed = true;
            t_test_expr( task, &expr_test, arg );
            if ( ! arg->folded ) {
               t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &expr_test.pos, "instruction argument not constant" );
               t_bail( task );
            }
         }
         list_next( &i );
      }
   }
}

void test_goto_in_format_block( struct task* task, struct list* labels ) {
   list_iter_t i;
   list_iter_init( &i, labels );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      if ( label->format_block ) {
         struct goto_stmt* stmt = label->stmts;
         while ( stmt ) {
            if ( stmt->format_block != label->format_block ) {
               t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &stmt->pos,
                  "entering format block with a goto statement" );
               t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                  "point of entry is here" ); 
               t_bail( task );
            }
            stmt = stmt->next;
         }
      }
      else {
         struct goto_stmt* stmt = label->stmts;
         while ( stmt ) {
            if ( stmt->format_block ) {
               t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
                  &stmt->pos,
                  "leaving format block with a goto statement" );
               t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                  "destination of goto statement is here" );
               t_bail( task );
            }
            stmt = stmt->next;
         }
      }
      list_next( &i );
   }
}