#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "task.h"

struct module_request {
   const char* path;
   bool load_once;
   struct module* module;
   bool err_open;
   bool err_loading;
   bool err_loaded_before;
};

struct macro {
   char* name;
   struct macro_param* params;
   struct token* token;
   struct macro* next;
   struct pos pos;
   bool func_like;
};

struct macro_param {
   char* name;
   struct macro_param* next;
};

struct macro_expan {
   struct macro* macro;
   struct macro_arg* args;
   struct token* token;
   struct token* token_arg;
   struct macro_expan* prev;
   struct pos pos;
};

struct macro_arg {
   struct macro_arg* next;
   struct token* tokens;
};

struct calc {
   jmp_buf bail;
   struct pos expr_pos;
   bool first_token;
};

static struct library* add_library( struct task* );
static void load_module( struct task* task, struct module_request* );
static void init_module_request( struct module_request*, const char* );
static bool get_full_path( const char*, struct str* );
static void get_dirname( struct str* );
static void prepare_source( struct task*, struct module* );
static enum tk peek( struct task*, int );
static void read_stream( struct task*, struct token* );
static void read_stream_token( struct task*, struct token* );
static void read_stream_expan( struct task*, struct token* );
static void read_source( struct task*, struct token* );
static void add_line( struct module*, int );
static void escape_ch( struct task*, char*, char**, char*, char**, bool );
static void make_pos( struct task*, struct pos*, const char**, int*, int* );
static void add_macro( struct macro_list*, struct macro* );
static void read_dirc( struct task*, struct token* );
static void read_known_dirc( struct task*, struct pos, struct token*,
   struct token* );
static void read_if( struct task*, struct pos*, struct token* );
static void find_elif( struct task*, struct pos*, char* );
static void find_elif_single( struct task*, struct token*, int* );
static int read_expr( struct task*, struct token* );
static int read_op( struct task* task, struct token*, struct calc* );
static int read_operand( struct task*, struct token*, struct calc* );
static int read_primary( struct task*, struct token*, struct calc* );
static int read_id( struct task*, struct token* );
static int read_number( struct task*, struct token* );
static void read_postfix( struct task*, struct token*, struct calc* );
static void read_elif( struct task*, struct pos*, struct token* );
static void read_endif( struct task*, struct pos* );
static void read_library( struct task* );
static void read_encrypt_str( struct task* );
static void read_include( struct task*, struct pos*, struct token* );
static void read_undef( struct task* );
static void read_error( struct task*, struct pos* );
static void read_line( struct task*, struct pos* );
static void read_pragma( struct task*, struct pos* );
static bool read_macro_expan( struct task*, struct macro*, struct pos );
static void read_macro_expan_args( struct task*, struct macro_expan* );
static void read_macro_token( struct task*, struct token* );
static void read_macro_token_body( struct task*, struct token* );
static void read_macro( struct task*, struct pos* );
static void read_macro_params( struct task*, struct macro* );
static struct macro* find_macro( struct macro_list*, const char* );
static bool is_same_macro( struct macro*, struct macro* );
static struct macro* remove_macro( struct macro_list*, const char* );
static struct macro* alloc_macro( struct task* );
static struct macro_param* alloc_macro_param( struct task* );
static void free_macro( struct task*, struct macro* );

void t_init_fields_token( struct task* task ) {
   task->tk = TK_END;
   task->tk_text = NULL;
   task->tk_length = 0;
   task->macros.head = NULL;
   task->macros.tail = NULL;
   task->in_dirc = false;
   task->macro_free = NULL;
   task->macro_param_free = NULL;
}

void t_load_main_module( struct task* task ) {
   task->library = add_library( task );
   task->library_main = task->library;
   struct module_request request;
   init_module_request( &request, task->options->source_file );
   load_module( task, &request );
   if ( ! request.module ) {
      t_diag( task, DIAG_ERR, "failed to load source file: %s",
         task->options->source_file );
      t_bail( task );
   }
   task->module_main = request.module;
}

struct library* add_library( struct task* task ) {
   struct library* lib = mem_alloc( sizeof( *lib ) );
   str_init( &lib->name );
   str_copy( &lib->name, "", 0 );
   list_init( &lib->modules );
   list_init( &lib->vars );
   list_init( &lib->funcs );
   list_init( &lib->scripts );
   list_init( &lib->dynamic );
   lib->visible = false;
   lib->imported = false;
   lib->encrypt_str = false;
   list_append( &task->libraries, lib );
   return lib;
}

void init_module_request( struct module_request* request, const char* path ) {
   request->path = path;
   request->load_once = false;
   request->module = NULL;
   request->err_open = false;
   request->err_loaded_before = false;
   request->err_loading = false;
}

void load_module( struct task* task, struct module_request* request ) {
   struct module* module = NULL;
   // Try path directly.
   struct str path;
   str_init( &path );
   str_append( &path, request->path );
   struct file_identity id;
   if ( c_read_identity( &id, path.value ) ) {
      goto load_file;
   }
   // Try directory of current file.
   if ( task->module ) {
      str_copy( &path, task->module->file_path.value,
         task->module->file_path.length );
      get_dirname( &path );
      str_append( &path, "/" );
      str_append( &path, request->path );
      if ( c_read_identity( &id, path.value ) ) {
         goto load_file;
      }
   }
   // Try user-specified directories.
   list_iter_t i;
   list_iter_init( &i, &task->options->includes );
   while ( ! list_end( &i ) ) {
      char* include = list_data( &i ); 
      str_clear( &path );
      str_append( &path, include );
      str_append( &path, "/" );
      str_append( &path, request->path );
      if ( c_read_identity( &id, path.value ) ) {
         goto load_file;
      }
      list_next( &i );
   }
   // Error:
   request->err_open = true;
   goto finish;
   load_file:
   // See if the file should be loaded once.
   list_iter_init( &i, &task->loaded_modules );
   while ( ! list_end( &i ) ) {
      struct module* loaded = list_data( &i );
      if ( c_same_identity( &id, &loaded->identity ) ) {
         if ( loaded->load_once ) {
            request->err_loaded_before = true;
            goto finish;
         }
         else {
            break;
         }
      }
      list_next( &i );
   }
   // The file should not be currently processing.
   struct module* prev_module = task->module;
   while ( prev_module ) {
      if ( c_same_identity( &prev_module->identity, &id ) ) {
         if ( request->load_once ) {
            prev_module->load_once = true;
            request->err_loaded_before = true;
         }
         else {
            request->err_loading = true;
         }
         goto finish;
      }
      prev_module = prev_module->prev;
   }
   // Load file.
   FILE* fh = fopen( path.value, "rb" );
   if ( ! fh ) {
      request->err_open = true;
      goto finish;
   }
   struct file* file = mem_alloc( sizeof( *file ) );
   str_init( &file->path );
   str_append( &file->path, request->path );
   fseek( fh, 0, SEEK_END );
   size_t size = ftell( fh );
   rewind( fh );
   char* save_ch = mem_alloc( size + 1 + 1 );
   char* ch = save_ch + 1;
   int read = fread( ch, sizeof( char ), size, fh );
   ch[ size ] = 0;
   fclose( fh );
   // Create module.
   module = mem_alloc( sizeof( *module ) );
   t_init_object( &module->object, NODE_MODULE );
   module->object.resolved = true;
   module->identity = id;
   module->file = file;
   str_init( &module->file_path );
   get_full_path( path.value, &module->file_path );
   list_init( &module->included );
   module->ch = ch;
   module->start = ch;
   module->save = save_ch;
   module->first_column = ch;
   module->line = 1;
   module->lines.count_max = 128;
   module->lines.columns = mem_alloc(
      sizeof( int ) * module->lines.count_max );
   // Column of the first line.
   module->lines.columns[ 0 ] = 0;
   module->lines.count = 1;
   module->id = list_size( &task->loaded_modules ) + 1;
   module->read = false;
   module->is_nl = true;
   module->is_nl_token = false;
   module->is_lib = false;
   module->load_once = request->load_once;
   module->prev = NULL;
   module->tab_space = 0;
   module->library = task->library;
   list_append( &task->loaded_modules, module );
   module->prev = task->module;
   task->module = module;
   task->tk = TK_END;
   request->module = module;
   prepare_source( task, module );
   finish:
   str_deinit( &path );
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

#include <dirent.h>
#include <sys/stat.h>

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

void count_lines( struct task* task, struct module* module ) {
   char* ch = module->ch;
   int column = 0;
   while ( ch[ 0 ] ) {
      // Linux newline.
      if ( ch[ 0 ] == '\n' ) {
         add_line( module, column + 1 );
         ++column;
         ++ch;
      }
      // Windows newline.
      else if ( ch[ 0 ] == '\r' && ch[ 1 ] == '\n' ) {
         add_line( module, column + 2 );
         column += 2;
         ch += 2;
      }
      // Line Concatenation.
      else if ( ch[ 0 ] == '\\' && ch[ 1 ] == '\n' ) {
         add_line( module, column );
         ch += 2;
      }
      else if ( ch[ 0 ] == '\\' && ch[ 1 ] == '\r' && ch[ 2 ] == '\n' ) {
         add_line( module, column );
         ch += 3;
      }
      else if ( ch[ 0 ] == '\t' ) {
         column += task->options->tab_size;
         ++ch;
      }
      else {
         ++column;
         ++ch;
      }
   }
}

void add_line( struct module* module, int column ) {
   if ( module->lines.count == module->lines.count_max ) {
      module->lines.count_max *= 2;
      module->lines.columns = mem_realloc( module->lines.columns,
         sizeof( int ) * module->lines.count_max );
   }
   module->lines.columns[ module->lines.count ] = column;
   ++module->lines.count;
}

void prepare_source( struct task* task, struct module* module ) {
   count_lines( task, module );
   char* ch = module->ch;
   char* cleaned_start = ch;
   char* cleaned = cleaned_start;
   char* space = NULL;
   int column = 0;

   state_start:
   while ( true ) {
      /*
      if ( *ch == '\n' ) {
         add_line( task->module, column );
      } */
      if ( ! *ch ) {
         goto state_finish;
      }
      else if ( *ch == '/' ) {
         ++ch;
         if ( *ch == '*' ) {
            ++ch;
            goto state_comment_m;
         }
         else {
            cleaned[ 0 ] = '/';
            cleaned[ 1 ] = *ch;
            cleaned += 2;
            ++ch;
         }
      }
      else if ( *ch == '\\' ) {
         ++ch;
         if ( *ch == '\n' ) {
            // add_line( task->module, 0 );
            ++ch;
         }
         else {
            cleaned[ 0 ] = '\\';
            cleaned[ 1 ] = *ch;
            cleaned += 2;
            ++ch;
         }
      }
      else if ( *ch == '"' ) {
         *cleaned = *ch;
         ++cleaned;
         ++ch;
         goto state_string;
      }
      else {
         *cleaned = *ch;
         ++cleaned;
         ++ch;
      }
      ++column;
   }

   state_string:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! *ch ) {
         struct pos pos;
         //pos.module = task->module->id;
         //pos.column = column;
         t_diag( task, DIAG_POS_ERR, &pos, "unterminated string" );
         t_bail( task );
      }
      else if ( *ch == '\\' ) {
         cleaned[ 0 ] = *ch;
         ++cleaned;
         ++ch;
         if ( *ch ) {
            *cleaned = *ch;
            ++cleaned;
            ++ch;
         }
      }
      else if ( *ch == '"' ) {
         ++ch;
         goto state_string_concat;
      }
      else {
         *cleaned = *ch;
         ++cleaned;
         ++ch;
      }
   }

   state_string_concat:
   // -----------------------------------------------------------------------
   space = ch;
   while ( isspace( *space ) ) {
      ++space;
   }
   if ( *space == '"' ) {
      ch = space + 1;
      goto state_string;
   }
   else {
      *cleaned = '"';
      ++cleaned;
      goto state_start;
   }

   state_comment_m:
   while ( true ) {
      if ( ! *ch ) {
         struct pos pos;
         // pos.module = task->module->id;
         // pos.column = column;
         t_diag( task, DIAG_POS_ERR, &pos, "unterminated comment" );
         t_bail( task );
      }
      else if ( *ch == '*' ) {
         ++ch;
         if ( *ch == '/' ) {
            ++ch;
            goto state_start;
         }
      }
      else {
         ++ch;
      }
   }

   state_finish:
   *cleaned = 0;
   module->ch = cleaned_start;
   // printf( "%s\n", module->ch );
}

void read_dirc( struct task* task, struct token* token ) {
   task->in_dirc = true;
   struct token name;
   read_stream( task, &name );
   // Null directive.
   if ( name.type == TK_NL ) {
      token->reread = true;
   }
   else {
      if ( name.is_id ) {
         read_known_dirc( task, token->pos, &name, token );
      }
      else {
         t_diag( task, DIAG_POS_ERR, &name.pos,
            "directive name not an identifier" );
         t_bail( task );
      }
   }
   task->in_dirc = false;
}

void read_known_dirc( struct task* task, struct pos pos, struct token* name,
   struct token* token ) {
   // #libdefine is an alias of #define.
   if ( strcmp( name->text, "define" ) == 0 ||
      strcmp( name->text, "libdefine" ) == 0 ) {
      read_macro( task, &pos );
      token->reread = true;
   }
   else if ( strcmp( name->text, "include" ) == 0 ||
      strcmp( name->text, "import" ) == 0 ) {
      read_include( task, &pos, name );
      token->reread = true;
   }
   else if ( strcmp( name->text, "if" ) == 0 ||
      strcmp( name->text, "ifdef" ) == 0 ||
      strcmp( name->text, "ifndef" ) == 0 ) {
      read_if( task, &pos, name );
      token->reread = true;
   }
   else if ( strcmp( name->text, "elif" ) == 0 ||
      strcmp( name->text, "else" ) == 0 ) {
      read_elif( task, &pos, name );
      token->reread = true;
   }
   else if ( strcmp( name->text, "endif" ) == 0 ) {
      read_endif( task, &pos );
      token->reread = true;
   }
   else if ( strcmp( name->text, "undef" ) == 0 ) {
      read_undef( task );
      token->reread = true;
   }
   else if ( strcmp( name->text, "error" ) == 0 ) {
      read_error( task, &pos );
      token->reread = true;
   }
   else if ( strcmp( name->text, "line" ) == 0 ) {
      read_line( task, &pos );
      token->reread = true;
   }
   else if ( strcmp( name->text, "pragma" ) == 0 ) {
      read_pragma( task, &pos );
      token->reread = true;
   }
   else if ( strcmp( name->text, "library" ) == 0 ) {
      read_library( task );
      // Send out an acknowledgement token. This is so a user doesn't start a
      // library in strange places, like in the middle of a structure.
      token->type = TK_LIB;
      token->pos = pos;
   }
   else if ( strcmp( name->text, "encryptstrings" ) == 0 ) {
      read_encrypt_str( task );
      token->reread = true;
   }
   else {
      t_diag( task, DIAG_POS_ERR, &pos, "unknown directive" );
      t_bail( task );
   }
}

void read_macro( struct task* task, struct pos* pos ) {
   struct macro* macro = alloc_macro( task );
   macro->name = NULL;
   macro->params = NULL;
   macro->token = NULL;
   macro->next = NULL;
   macro->func_like = false;
   macro->pos = *pos;
   // Name.
   struct token name;
   read_stream( task, &name );
   if ( name.is_id ) {
      if ( strcmp( name.text, "defined" ) != 0 ) {
         macro->name = name.text;
      }
      else {
         t_diag( task, DIAG_POS_ERR, &name.pos,
            "invalid macro name" );
         t_bail( task );
      }
   }
   else {
      t_diag( task, DIAG_POS_ERR, &name.pos,
         "macro name not an identifier" );
      t_bail( task );
   }
   // Parameter list.
   if ( *task->module->ch == '(' ) {
      read_macro_params( task, macro );
   }
   // Body.
   bool checked_sep = false;
   struct token* tail = NULL;
   while ( true ) {
      struct token token;
      read_stream( task, &token );
      if ( token.type == TK_NL || token.type == TK_END ) {
         break;
      }
      // For an object-like macro, there needs to be whitespace between the
      // name and the body of the macro.
      if ( ! checked_sep ) {
         if ( ! token.space_before ) {
            t_diag( task, DIAG_POS_ERR, &token.pos,
               "missing whitespace after name" );
            t_bail( task );
         }
         checked_sep = true;
      }
      struct token* macro_token = mem_alloc( sizeof( *macro_token ) );
      *macro_token = token;
      if ( tail ) {
         tail->next = macro_token;
      }
      else {
         macro->token = macro_token;
      }
      tail = macro_token;
   }
   // Cannot have two different macros with the same name.
   struct macro* prev_macro = find_macro( &task->macros, macro->name );
   if ( prev_macro ) {
      if ( is_same_macro( prev_macro, macro ) ) {
         prev_macro->pos = macro->pos;
         free_macro( task, macro );
      }
      else {
         t_diag( task, DIAG_POS_ERR, &name.pos,
            "macro `%s` redefined", macro->name );
         t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &prev_macro->pos,
            "macro previously defined here" );
         t_bail( task );
      }
   }
   else {
      add_macro( &task->macros, macro );
   }
}

void read_macro_params( struct task* task, struct macro* macro ) {
   struct token token;
   // Read `(` token.
   read_stream( task, &token );
   int count = 0;
   struct macro_param* head = NULL;
   struct macro_param* tail;
   while ( true ) {
      read_stream( task, &token );
      if ( token.type == TK_NL || token.type == TK_END ||
         ( count == 0 && token.type == TK_PAREN_R ) ) {
         break;
      }
      // Parameter.
      if ( token.is_id ) {
         // Check for a duplicate parameter.
         struct macro_param* param = head;
         while ( param ) {
            if ( strcmp( param->name, token.text ) == 0 ) {
               t_diag( task, DIAG_POS_ERR, &token.pos, "duplicate parameter" );
               t_bail( task );
            }
            param = param->next;
         }
         param = alloc_macro_param( task );
         param->name = token.text;
         param->next = NULL;
         if ( head ) {
            tail->next = param;
         }
         else {
            head = param;
         }
         tail = param;
         ++count;
      }
      else {
         t_diag( task, DIAG_POS_ERR, &token.pos, "invalid parameter" );
         t_bail( task );
      }
      // Next parameter.
      read_stream( task, &token );
      if ( token.type != TK_COMMA ) {
         break;
      }
   }
   if ( token.type == TK_PAREN_R ) {
      macro->params = head;
      macro->func_like = true;
   }
   else {
      t_diag( task, DIAG_POS_ERR, &token.pos, "missing `)`" );
      t_bail( task );
   }
}

struct macro* find_macro( struct macro_list* list, const char* name ) {
   struct macro* macro = list->head;
   while ( macro ) {
      int result = strcmp( macro->name, name );
      if ( result == 0 ) {
         break;
      }
      if ( result > 0 ) {
         macro = NULL;
         break;
      }
      macro = macro->next;
   }
   return macro;
}

bool is_same_macro( struct macro* a, struct macro* b ) {
   // Macros need to be of the same kind.
   if ( a->func_like != b->func_like ) {
      return false;
   }
   // Parameter list.
   if ( a->func_like ) {
      struct macro_param* param_a = a->params;
      struct macro_param* param_b = b->params;
      while ( param_a && param_b ) {
         if ( strcmp( param_a->name, param_b->name ) != 0 ) {
            return false;
         }
         param_a = param_a->next;
         param_b = param_b->next;
      }
      // Parameter count needs to be the same.
      if ( param_a || param_b ) {
         return false;
      }
   }
   // Body.
   struct token* token_a = a->token;
   struct token* token_b = b->token;
   while ( token_a && token_b ) {
      if ( token_a->type != token_b->type ) {
         return false;
      }
      // Whitespace needs to appear in the same position, although the amount
      // of whitespace at each position doesn't matter.
      if ( token_a->space_before != token_b->space_before ) {
         return false;
      }
      // Tokens that can have different values need to have the same value.
      if ( token_a->text ) {
         if ( strcmp( token_a->text, token_b->text ) != 0 ) {
            return false;
         }
      }
      token_a = token_a->next;
      token_b = token_b->next;
   }
   // Number of tokens need to be the same.
   if ( token_a || token_b ) {
      return false;
   }
   return true;
}

void add_macro( struct macro_list* list, struct macro* macro ) {
   struct macro* prev = NULL;
   struct macro* curr = list->head;
   while ( curr && strcmp( curr->name, macro->name ) < 0 ) {
      prev = curr;
      curr = curr->next;
   }
   if ( prev ) {
      macro->next = prev->next;
      prev->next = macro;
   }
   else {
      macro->next = list->head;
      list->head = macro;
   }
}

// Tries to find a macro with the given name and remove it from the list.
struct macro* remove_macro( struct macro_list* list, const char* name ) {
   struct macro* macro_prev = NULL;
   struct macro* macro = list->head;
   while ( macro ) {
      int result = strcmp( macro->name, name );
      if ( result == 0 ) {
         break;
      }
      else if ( result > 0 ) {
         macro = NULL;
         break;
      }
      else {
         macro_prev = macro;
         macro = macro->next;
      }
   }
   if ( macro ) {
      if ( macro_prev ) {
         macro_prev->next = macro->next;
      }
      else {
         list->head = macro->next;
      }
   }
   return macro;
}

struct macro* alloc_macro( struct task* task ) {
   struct macro* macro = task->macro_free;
   if ( macro ) {
      task->macro_free = macro->next;
   }
   else {
      macro = mem_alloc( sizeof( *macro ) );
   }
   return macro;
}

struct macro_param* alloc_macro_param( struct task* task ) {
   struct macro_param* param = task->macro_param_free;
   if ( param ) {
      task->macro_param_free = param->next;
   }
   else {
      param = mem_alloc( sizeof( *param ) );
   }
   return param;
}

void free_macro( struct task* task, struct macro* macro ) {
   // Reuse macro parameters.
   if ( macro->params ) {
      struct macro_param* param = macro->params;
      while ( param->next ) {
         param = param->next;
      }
      param->next = task->macro_param_free;
      task->macro_param_free = macro->params;
   }
   // Reuse macro.
   macro->next = task->macro_free;
   task->macro_free = macro;
}

void read_include( struct task* task, struct pos* pos,
   struct token* name ) {
   struct token file;
   read_stream( task, &file );
   if ( file.type != TK_LIT_STRING ) {
      t_diag( task, DIAG_POS_ERR, &file.pos,
         "file argument not a string" );
      t_bail( task );
   }
   struct token nl;
   read_stream( task, &nl );
   if ( nl.type != TK_NL ) {
      t_diag( task, DIAG_POS_ERR, pos,
         "#%s not terminated with newline character", name->text );
      t_bail( task );
   }
   struct module_request request;
   init_module_request( &request, file.text );
   // The difference between an #include and an #import is that the latter
   // configures the file to be loaded once. Any future attempt to load the
   // file, whether via #include or #import, will be ignored.
   if ( name->text[ 1 ] == 'm' ) {
      request.load_once = true;
   }
   load_module( task, &request );
   if ( ! request.module && ! request.err_loaded_before ) {
      if ( request.err_loading ) {
         t_diag( task, DIAG_POS_ERR, &file.pos,
            "file already being loaded" );
         t_bail( task );
      }
      else {
         t_diag( task, DIAG_POS_ERR, &file.pos,
            "failed to load file: %s", file.text );
         t_bail( task );
      }
   }
}

void read_if( struct task* task, struct pos* pos, struct token* name ) {
   struct token token;
   int value = 0;
   // #ifdef:
   if ( name->text[ 2 ] == 'd' ) {
      read_stream( task, &token );
      if ( token.type != TK_NL ) {
         if ( token.is_id ) {
            if ( find_macro( &task->macros, token.text ) ) {
               value = 1;
            }
            // Get newline character.
            read_stream( task, &token );
         }
         else {
            t_diag( task, DIAG_POS_ERR, &token.pos,
               "macro argument not an identifier" );
            t_bail( task );
         }
      }
      else {
         t_diag( task, DIAG_POS_ERR, &token.pos,
            "missing macro argument" );
         t_bail( task );
      }
   }
   // #ifndef:
   else if ( name->text[ 2 ] == 'n' ) {
      read_stream( task, &token );
      if ( token.type != TK_NL ) {
         if ( token.is_id ) {
            if ( ! find_macro( &task->macros, token.text ) ) {
               value = 1;
            }
            // Get newline character.
            read_stream( task, &token );
         }
         else {
            t_diag( task, DIAG_POS_ERR, &token.pos,
               "macro argument not an identifier" );
            t_bail( task );
         }
      }
      else {
         t_diag( task, DIAG_POS_ERR, &token.pos,
            "missing macro argument" );
         t_bail( task );
      }
   }
   // #if:
   else {
      value = read_expr( task, &token );
   }
   if ( token.type != TK_NL ) {
      t_diag( task, DIAG_POS_ERR, &token.pos,
         "missing newline character" );
      t_bail( task );
   }
   if ( value ) {
      ++task->ifdirc_depth;
   }
   else {
      find_elif( task, pos, name->text );
   }
}

void find_elif( struct task* task, struct pos* pos, char* name ) {
   int depth = 1;
   while ( depth ) {
      struct token token;
      read_stream( task, &token );
      if ( token.is_dirc_hash ) {
         read_stream( task, &token );
         if ( token.is_id ) {
            find_elif_single( task, &token, &depth );
         }
      }
      else {
         if ( token.type == TK_END ) {
            t_diag( task, DIAG_POS_ERR, pos,
               "unterminated #%s", name );
            t_bail( task );
         }
      }
   }
}

void find_elif_single( struct task* task, struct token* token, int* depth ) {
   if ( strcmp( token->text, "endif" ) == 0 ) {
      --*depth;
   }
   else if ( strcmp( token->text, "if" ) == 0 ||
      strcmp( token->text, "ifdef" ) == 0 ||
      strcmp( token->text, "ifndef" ) == 0 ) {
      ++*depth;
   }
   else if ( strcmp( token->text, "elif" ) == 0 ) {
      if ( *depth == 1 ) {
         if ( read_expr( task, token ) ) {
            if ( token->type == TK_NL ) {
               ++task->ifdirc_depth;
               *depth = 0;
            }
            else {
               t_diag( task, DIAG_POS_ERR, &token->pos,
                  "missing newline character" );
               t_bail( task );
            }
         }
      }
   }
   else if ( strcmp( token->text, "else" ) == 0 ) {
      if ( *depth == 1 ) {
         read_stream( task, token );
         if ( token->type == TK_NL ) {
            ++task->ifdirc_depth;
            *depth = 0;
         }
         else {
            t_diag( task, DIAG_POS_ERR, &token->pos,
               "missing newline character" );
            t_bail( task );
         }
      }
   }
}

int read_expr( struct task* task, struct token* token ) {
   read_stream_expan( task, token );
   if ( token->type == TK_NL ) {
      t_diag( task, DIAG_POS_ERR, &token->pos,
         "missing expression" );
      t_bail( task );
   }
   int value = 0;
   struct calc calc;
   calc.expr_pos = token->pos;
   calc.first_token = false;
   if ( setjmp( calc.bail ) == 0 ) {
      value = read_op( task, token, &calc );
   }
   else {
      t_diag( task, DIAG_POS_ERR, &calc.expr_pos,
         "invalid expression" );
      t_bail( task );
   }
   return value;
}

int read_op( struct task* task, struct token* token, struct calc* calc ) {
   int mul = 0;
   int mul_lside = 0;
   int add = 0;
   int add_lside = 0;
   int shift = 0;
   int shift_lside = 0;
   int lt = 0;
   int lt_lside = 0;
   int eq = 0;
   int eq_lside = 0;
   int bit_and = 0;
   int bit_and_lside = 0;
   int bit_xor = 0;
   int bit_xor_lside = 0;
   int bit_or = 0;
   int bit_or_lside = 0;
   int log_and = 0;
   int log_and_lside = 0;
   int log_or = 0;
   int log_or_lside = 0;
   int operand;
   struct pos operand_pos;

   top:
   operand = read_operand( task, token, calc );

   mul:
   switch ( token->type ) {
   case TK_STAR:
   case TK_SLASH:
   case TK_MOD:
      mul = token->type;
      break;
   default:
      goto add;
   }
   mul_lside = operand;
   operand = read_operand( task, token, calc );
   if ( mul == TK_STAR ) {
      operand = ( mul_lside * operand );
   }
   else {
      // Check for division by zero.
      if ( ! operand ) {
         t_diag( task, DIAG_POS_ERR, &calc->expr_pos,
            "division by zero in expression" );
         t_bail( task );
      }
      if ( mul == TK_SLASH ) {
         operand = ( mul_lside / operand );
      }
      else {
         operand = ( mul_lside % operand );
      }
   }
   goto mul;

   add:
   switch ( add ) {
   case TK_PLUS:
      operand = ( add_lside + operand );
      break;
   case TK_MINUS:
      operand = ( add_lside - operand );
      break;
   }
   switch ( token->type ) {
   case TK_PLUS:
   case TK_MINUS:
      add = token->type;
      add_lside = operand;
      goto top;
   default:
      add = 0;
   }

   switch ( shift ) {
   case TK_SHIFT_L:
      operand = ( shift_lside << operand );
      break;
   case TK_SHIFT_R:
      operand = ( shift_lside >> operand );
      break;
   }
   switch ( token->type ) {
   case TK_SHIFT_L:
   case TK_SHIFT_R:
      shift = token->type;
      shift_lside = operand;
      goto top;
   default:
      shift = 0;
   }

   switch ( lt ) {
   case TK_LT:
      operand = ( lt_lside < operand );
      break;
   case TK_LTE:
      operand = ( lt_lside <= operand );
      break;
   case TK_GT:
      operand = ( lt_lside > operand );
      break;
   case TK_GTE:
      operand = ( lt_lside >= operand );
      break;
   }
   switch ( token->type ) {
   case TK_LT:
   case TK_LTE:
   case TK_GT:
   case TK_GTE:
      lt = token->type;
      lt_lside = operand;
      goto top;
   default:
      lt = 0;
   }

   switch ( eq ) {
   case TK_EQ:
      operand = ( eq_lside == operand );
      break;
   case TK_NEQ:
      operand = ( eq_lside != operand );
      break;
   }
   switch ( token->type ) {
   case TK_EQ:
   case TK_NEQ:
      eq = token->type;
      eq_lside = operand;
      goto top;
   default:
      eq = 0;
   }

   if ( bit_and ) {
      operand = ( bit_and_lside & operand );
      bit_and = 0;
   }
   if ( token->type == TK_BIT_AND ) {
      bit_and = token->type;
      bit_and_lside = operand;
      goto top;
   }

   if ( bit_xor ) {
      operand = ( bit_xor_lside ^ operand );
      bit_xor = 0;
   }
   if ( token->type == TK_BIT_XOR ) {
      bit_xor = token->type;
      bit_xor_lside = operand;
      goto top;
   }

   if ( bit_or ) {
      operand = ( bit_or_lside | operand );
      bit_or = 0;
   }
   if ( token->type == TK_BIT_OR ) {
      bit_or = token->type;
      bit_or_lside = operand;
      goto top;
   }

   if ( log_and ) {
      operand = ( log_and_lside && operand );
      log_and = 0;
   }
   if ( token->type == TK_LOG_AND ) {
      log_and = token->type;
      log_and_lside = operand;
      goto top;
   }

   if ( log_or ) {
      operand = ( log_or_lside || operand );
      log_or = 0;
   }
   if ( token->type == TK_LOG_OR ) {
      log_or = token->type;
      log_or_lside = operand;
      goto top;
   }

   switch ( token->type ) {
   case TK_ASSIGN:
   case TK_ASSIGN_ADD:
   case TK_ASSIGN_SUB:
   case TK_ASSIGN_MUL:
   case TK_ASSIGN_DIV:
   case TK_ASSIGN_MOD:
   case TK_ASSIGN_SHIFT_L:
   case TK_ASSIGN_SHIFT_R:
   case TK_ASSIGN_BIT_AND:
   case TK_ASSIGN_BIT_XOR:
   case TK_ASSIGN_BIT_OR:
      longjmp( calc->bail, 1 );
      break;
   default:
      break;
   }

   return operand;
}

int read_operand( struct task* task, struct token* token,
   struct calc* calc ) {
   // Don't read the first token because it is read elsewhere.
   if ( ! calc->first_token ) {
      calc->first_token = true;
   }
   else {
      read_stream_expan( task, token );
   }
   int op = 0;
   switch ( token->type ) {
   case TK_MINUS:
   case TK_PLUS:
   case TK_LOG_NOT:
   case TK_BIT_NOT:
      op = token->type;
      break;
   case TK_INC:
   case TK_DEC:
      longjmp( calc->bail, 1 );
      break;
   default:
      break;
   }
   if ( op ) {
      int operand = read_operand( task, token, calc );
      switch ( op ) {
      case TK_PLUS:
         return operand;
      case TK_MINUS:
         return ( - operand );
      case TK_LOG_NOT:
         return ( ! operand );
      default:
         return ( ~ operand );
      }
   }
   else {
      int operand = read_primary( task, token, calc );
      read_postfix( task, token, calc );
      return operand;
   }
}

int read_primary( struct task* task, struct token* token,
   struct calc* calc ) {
   if ( token->is_id ) {
      return read_id( task, token );
   }
   else if (
      token->type == TK_LIT_DECIMAL ||
      token->type == TK_LIT_OCTAL ||
      token->type == TK_LIT_HEX ||
      token->type == TK_LIT_CHAR ) {
      int value = read_number( task, token );
      read_stream_expan( task, token );
      return value;
   }
   else if ( token->type == TK_PAREN_L ) {
      int value = read_op( task, token, calc );
      if ( token->type != TK_PAREN_R ) {
         t_diag( task, DIAG_POS_ERR, &token->pos, "missing `)`" );
         t_bail( task );
      }
      read_stream_expan( task, token );
      return value;
   }
   else {
      longjmp( calc->bail, 1 );
      return 0;
   }
}

int read_number( struct task* task, struct token* token ) {
   switch ( token->type ) {
   case TK_LIT_DECIMAL:
      return strtol( token->text, NULL, 10 );
   case TK_LIT_OCTAL:
      return strtol( token->text, NULL, 8 );
   case TK_LIT_HEX:
      return strtol( token->text, NULL, 16 );
   case TK_LIT_CHAR:
      return token->text[ 0 ];
   default:
      return 0;
   }
}

int read_id( struct task* task, struct token* token ) {
   if ( strcmp( token->text, "defined" ) == 0 ) {
      bool paren = false;
      read_stream( task, token );
      if ( token->type == TK_PAREN_L ) {
         read_stream( task, token );
         paren = true;
      }
      if ( token->type != TK_ID ) {
         t_diag( task, DIAG_POS_ERR, &token->pos,
            "argument not an identifier" );
         t_bail( task );
      }
      int value = 0;
      if ( find_macro( &task->macros, token->text ) ) {
         value = 1;
      }
      if ( paren ) {
         read_stream( task, token );
         if ( token->type != TK_PAREN_R ) {
            t_diag( task, DIAG_POS_ERR, &token->pos, "missing `)`" );
            t_bail( task );
         }
      }
      read_stream_expan( task, token );
      return value;
   }
   else {
      read_stream_expan( task, token );
      return 0;
   }
}

void read_postfix( struct task* task, struct token* token,
   struct calc* calc ) {
   switch ( token->type ) {
   case TK_BRACKET_L:
   case TK_DOT:
   case TK_COLON_2:
   case TK_PAREN_L:
   case TK_INC:
   case TK_DEC:
      longjmp( calc->bail, 1 );
      break;
   default:
      break;
   }
}

void read_elif( struct task* task, struct pos* pos, struct token* name ) {
   if ( task->ifdirc_depth ) {
      while ( true ) {
         struct token token;
         read_stream( task, &token );
         if ( token.type == TK_END ) {
            t_diag( task, DIAG_POS_ERR, "unterminated #if" );
            t_bail( task );
         }
         if ( token.is_dirc_hash ) {
            read_stream( task, &token );
            if ( token.is_id && strcmp( token.text, "endif" ) == 0 ) {
               read_endif( task, pos );
               break;
            }
         }
      }
   }
   else {
      t_diag( task, DIAG_POS_ERR, &pos,
         "#%s without #if", name->text );
      t_bail( task );
   }
}

void read_endif( struct task* task, struct pos* pos ) {
   if ( task->ifdirc_depth ) {
      --task->ifdirc_depth;
      struct token token;
      read_stream( task, &token );
      if ( token.type != TK_NL ) {
         t_diag( task, DIAG_POS_ERR, &token.pos,
            "missing newline character" );
         t_bail( task );
      }
   }
   else {
      t_diag( task, DIAG_POS_ERR, pos, "#endif without #if" );
      t_bail( task );
   }
}

void read_undef( struct task* task ) {
   struct token token;
   read_stream( task, &token );
   if ( token.type == TK_NL ) {
      t_diag( task, DIAG_POS_ERR, &token.pos,
         "#undef missing argument" );
      t_bail( task );
   }
   if ( token.type != TK_ID ) {
      t_diag( task, DIAG_POS_ERR, &token.pos,
         "#undef argument not an identifier" );
      t_bail( task );
   }
   struct macro* macro = remove_macro( &task->macros, token.text );
   if ( macro ) {
      free_macro( task, macro );
   }
   read_stream( task, &token );
   if ( token.type != TK_NL ) {
      t_diag( task, DIAG_POS_ERR, &token.pos,
         "missing newline character" );
      t_bail( task );
   }
}

void read_error( struct task* task, struct pos* pos ) {
   struct str message;
   str_init( &message );
   while ( true ) {
      struct token token;
      read_stream( task, &token );
      if ( token.type == TK_NL ) {
         break;
      }
      if ( token.text ) {
         if ( message.length ) {
            str_append( &message, " " );
         }
         str_append( &message, token.text );
      }
   }
   t_diag( task, DIAG_POS_ERR, &pos, message.value );
   t_bail( task );
}

void read_line( struct task* task, struct pos* pos ) {
   struct token token;
   read_stream( task, &token );
   if ( token.type == TK_LIT_DECIMAL ) {
      task->line_pos.offset = strtol( token.text, NULL, 10 );
      if ( ! task->line_pos.offset ) {
         t_diag( task, DIAG_POS_ERR, &token.pos,
            "line-number argument is 0" );
         t_bail( task );
      }
      task->line_pos.column = task->module->ch - task->module->start;
      task->line_pos.file = NULL;
      read_stream( task, &token );
      if ( token.type == TK_LIT_STRING ) {
         task->line_pos.file = token.text;
         read_stream( task, &token );
      }
   }
   else {
      if ( token.type == TK_NL ) {
         t_diag( task, DIAG_POS_ERR, &token.pos,
            "missing line-number argument" );
      }
      else {
         t_diag( task, DIAG_POS_ERR, &token.pos,
            "line-number argument not numeric" );
      }
      t_bail( task );
   }
}

void read_pragma( struct task* task, struct pos* pos ) {
   struct token token;
   read_stream( task, &token );
   bool processed = false;
   if ( token.is_id ) {
      if ( strcmp( token.text, "once" ) == 0 ) {
         task->module->load_once = true;
         read_stream( task, &token );
         processed = true;
      }
   }
   if ( processed ) {
      if ( token.type != TK_NL ) {
         t_diag( task, DIAG_POS_ERR, &token.pos,
            "missing newline character" );
         t_bail( task );
      }
   }
   else {
      t_diag( task, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
         "ignoring unknown #pragma" );
      while ( token.type != TK_NL ) {
         read_stream( task, &token );
      }
   }
}

void read_library( struct task* task ) {
   struct token name;
   read_stream( task, &name );
   if ( name.type != TK_LIT_STRING ) {
      t_diag( task, DIAG_POS_ERR, &name.pos,
         "library name not a string" );
      t_bail( task );
   }
   if ( ! name.length ) {
      t_diag( task, DIAG_POS_ERR, &name.pos,
         "library name is blank" );
      t_bail( task );
   }
   if ( name.length > MAX_MODULE_NAME_LENGTH ) {
      t_diag( task, DIAG_POS_ERR, &name.pos,
         "library name too long" );
      t_diag( task, DIAG_FILE, &name.pos,
         "library name can be up to %d characters long",
         MAX_MODULE_NAME_LENGTH );
      t_bail( task );
   }
   struct token nl;
   read_stream( task, &nl );
   if ( nl.type != TK_NL ) {
      t_diag( task, DIAG_POS_ERR, &nl.pos,
         "directive not terminated" );
      t_bail( task );
   }
   // Name of main library.
   if ( task->module == task->module_main ) {
      str_copy( &task->library_main->name, name.text, name.length );
      task->library_main->visible = true;
   }
   else {
      list_iter_t i;
      list_iter_init( &i, &task->libraries );
      struct library* lib = NULL;
      while ( ! list_end( &i ) ) {
         lib = list_data( &i );
         if ( strcmp( name.text, lib->name.value ) == 0 ) {
            break;
         }
         list_next( &i );
      }
      // New library.
      if ( list_end( &i ) ) {
         lib = add_library( task );
         str_copy( &lib->name, name.text, name.length );
         lib->visible = true;
         lib->imported = true;
         list_append( &task->library->dynamic, lib );
      }
      task->library = lib;
   }
}

void read_encrypt_str( struct task* task ) {
   struct token token;
   read_stream( task, &token );
   if ( token.type == TK_NL ) {
      task->library->encrypt_str = true;
   }
   else {
      t_diag( task, DIAG_POS_ERR, &token.pos,
         "missing newline character" );
      t_bail( task );
   }
}

void t_read_tk( struct task* task ) {
   struct token* token = NULL;
   if ( task->tokens.peeked ) {
      // When dequeuing, shift the queue elements. For now, this will suffice.
      // In the future, maybe use a circular buffer.
      int i = 0;
      while ( i < task->tokens.peeked ) {
         task->tokens.buffer[ i ] = task->tokens.buffer[ i + 1 ];
         ++i;
      }
      token = &task->tokens.buffer[ 0 ];
      --task->tokens.peeked;
   }
   else {
      token = &task->tokens.buffer[ 0 ];
      while ( true ) {
         read_stream_expan( task, token );
         if ( token->is_dirc_hash ) {
            read_dirc( task, token );
            if ( ! token->reread ) {
               break;
            }
         }
         else {
            break;
         }
      }
   }
   task->tk = token->type;
   task->tk_text = token->text;
   task->tk_pos = token->pos;
   task->tk_length = token->length;
}

enum tk t_peek( struct task* task ) {
   return peek( task, 1 );
}

// NOTE: Make sure @pos is not more than ( TK_BUFFER_SIZE - 1 ).
enum tk peek( struct task* task, int pos ) {
   int i = 0;
   while ( true ) {
      // Peeked tokens begin at position 1.
      struct token* token = &task->tokens.buffer[ i + 1 ];
      if ( i == task->tokens.peeked ) {
         read_stream_expan( task, token );
         ++task->tokens.peeked;
      }
      ++i;
      if ( i == pos ) {
         return token->type;
      }
   }
}

void read_stream( struct task* task, struct token* token ) {
   while ( true ) {
      token->reread = false;
      read_stream_token( task, token );
      if ( ! token->reread ) {
         break;
      }
   }
}

void read_stream_expan( struct task* task, struct token* token ) {
   while ( true ) {
      token->reread = false;
      read_stream_token( task, token );
      if ( ! token->reread ) {
         if ( token->is_id ) {
            struct macro* macro =
               find_macro( &task->macros, token->text );
            if ( macro ) {
               if ( read_macro_expan( task, macro, token->pos ) ) {
                  token->reread = true;
               }
            }
         }
         if ( ! token->reread ) {
            break;
         }
      }
   }
}

void read_stream_token( struct task* task, struct token* token ) {
   if ( task->macro_expan ) {
      read_macro_token( task, token );
      if ( token->type == TK_END ) {
         task->macro_expan = task->macro_expan->prev;
         token->reread = true;
      }
   }
   else {
      read_source( task, token );
      if ( token->type == TK_END ) {
         if ( task->in_dirc ) {
            token->type = TK_NL;
         }
         else {
            task->module = task->module->prev;
            if ( task->module ) {
               task->library = task->module->library;
               token->reread = true;
            }
         }
      }
   }
}

bool read_macro_expan( struct task* task, struct macro* macro,
   struct pos pos ) {
   // The macro should not already be undergoing expansion.
   struct macro_expan* expan = task->macro_expan;
   while ( expan ) {
      if ( expan->macro == macro ) {
         return false;
      }
      expan = expan->prev;
   }
   if ( macro->func_like ) {
      struct token next_token;
      read_stream( task, &next_token );
      if ( next_token.type != TK_PAREN_L ) {
         task->tokens.buffer[ task->tokens.peeked ] = next_token;
         ++task->tokens.peeked;
         return false;
      }
   }
   expan = mem_alloc( sizeof( *expan ) );
   expan->macro = macro;
   expan->args = NULL;
   expan->token = macro->token;
   expan->token_arg = NULL;
   expan->prev = task->macro_expan;
   expan->pos = pos;
   if ( macro->func_like ) {
      read_macro_expan_args( task, expan );
   }
   task->macro_expan = expan;
   return true;
}

void read_macro_expan_args( struct task* task, struct macro_expan* expan ) {
   int count = 0;
   struct macro_arg* head = NULL;
   struct macro_arg* tail;
   bool done = false;
   while ( ! done ) {
      // Read the tokens of each argument.
      bool add = false;
      struct token* token_head = NULL;
      struct token* token_tail;
      while ( true ) {
         struct token token;
         read_stream( task, &token );
         if ( token.type == TK_END ) {
            t_diag( task, DIAG_POS_ERR, &expan->pos,
               "unterminated macro expansion" );
            t_bail( task );
         }
         if ( token.type == TK_PAREN_R ) {
            if ( token_head || head ) {
               add = true;
            }
            done = true;
            break;
         }
         else if ( token.type == TK_COMMA ) {
            add = true;
            break;
         }
         else {
            struct token* part = mem_alloc( sizeof( *part ) );
            part[ 0 ] = token;
            part->pos = expan->pos;
            part->next = NULL;
            if ( token_head ) {
               token_tail->next = part;
            }
            else {
               token_head = part;
            }
            token_tail = part;
         }
      }
      // Add argument.
      if ( add ) {
         struct macro_arg* arg = mem_alloc( sizeof( *arg ) );
         arg->next = NULL;
         arg->tokens = token_head;
         if ( head ) {
            tail->next = arg;
         }
         else {
            head = arg;
         }
         tail = arg;
         ++count;
      }
   }
   // Number of arguments need to be correct.
   int i = 0;
   struct macro_param* param = expan->macro->params;
   while ( param ) {
      param = param->next;
      ++i;
   }
   if ( i != count ) {
      t_diag( task, DIAG_POS_ERR, &expan->pos,
         "incorrect number of arguments in macro expansion" );
      t_bail( task );
   }
   expan->args = head;
}

void read_macro_token( struct task* task, struct token* token ) {
   // Read token from an argument.
   if ( task->macro_expan->token_arg ) {
      token[ 0 ] = *task->macro_expan->token_arg;
      task->macro_expan->token_arg = task->macro_expan->token_arg->next;
   }
   // Read token from the macro body.
   else {
      read_macro_token_body( task, token );
   }
}

void read_macro_token_body( struct task* task, struct token* token ) {
   struct macro_expan* expan = task->macro_expan;
   if ( expan->token ) {
      struct token* body_token = expan->token;
      // See if the body token refers to a parameter.
      struct macro_arg* arg = NULL;
      if ( body_token->is_id ) {
         int skipped = 0;
         struct macro_param* param = expan->macro->params;
         while ( param && strcmp( param->name, body_token->text ) != 0 ) {
            param = param->next;
            ++skipped;
         }
         // Find the argument.
         if ( param ) {
            arg = expan->args;
            while ( skipped ) {
               arg = arg->next;
               --skipped;
            }
         }
      }
      // Read token from argument.
      if ( arg ) {
         expan->token_arg = arg->tokens;
         expan->token = body_token->next;
         read_macro_token( task, token );
      }
      // Read body token.
      else {
         *token = *body_token;
         expan->token = body_token->next;
      }
   }
   else {
      token->type = TK_END;
   }
}

void read_source( struct task* task, struct token* token ) {
   struct module* module = task->module;
   char* ch = module->ch;
   char* save = module->save;
   int column = 0;
   enum tk tk = TK_END;
   bool is_id = false;
   bool space_before = false;

   state_space:
   // -----------------------------------------------------------------------
   if ( isspace( *ch ) ) {
      space_before = true;
   }
   while ( *ch && isspace( *ch ) ) {
      if ( *ch == '\n' ) {
         module->is_nl = true;
         if ( task->in_dirc ) {
            tk = TK_NL;
            column = ch - task->module->start;
            ++ch;
            goto state_finish;
         }
      }
      else if ( *ch == '\t' ) {
      //int column = task->column -
    //     task->module->lines.columns[ task->module->lines.count - 1 ];
  //    task->column += task->options->tab_size -
//         ( ( column + task->options->tab_size ) % task->options->tab_size );

         task->module->tab_space += task->options->tab_size - 1;
      }
      ++ch;
   }
   // Check for a directive.
   if ( module->is_nl ) {
      module->is_nl = false;
      if ( *ch == '#' ) {
         token->type = TK_HASH;
         token->is_dirc_hash = true;
         token->is_id = false;
         token->pos.module = module->id;
         token->pos.column = ( ch - task->module->start ) + task->module->tab_space;
         module->ch = ch + 1;
         return;
      }
   }

   state_token:
   // -----------------------------------------------------------------------
   column = ( ch - task->module->start ) + task->module->tab_space;
   // Identifier.
   if ( isalpha( *ch ) || *ch == '_' ) {
      char* id = save;
      while ( isalnum( *ch ) || *ch == '_' ) {
         *save = tolower( *ch );
         ++save;
         ++ch;
      }
      *save = 0;
      ++save;
      // NOTE: Reserved identifiers must be listed in ascending order.
      static const struct { const char* name; enum tk tk; }
      reserved[] = {
         { "bluereturn", TK_BLUE_RETURN },
         { "bool", TK_BOOL },
         { "break", TK_BREAK },
         { "case", TK_CASE },
         { "clientside", TK_CLIENTSIDE },
         { "const", TK_CONST },
         { "continue", TK_CONTINUE },
         { "createtranslation", TK_PALTRANS },
         { "death", TK_DEATH },
         { "default", TK_DEFAULT },
         { "disconnect", TK_DISCONNECT },
         { "do", TK_DO },
         { "else", TK_ELSE },
         { "enter", TK_ENTER },
         { "enum", TK_ENUM },
         { "event", TK_EVENT },
         { "false", TK_FALSE },
         // Maybe we'll add this as a type later.
         { "fixed", TK_RESERVED },
         { "for", TK_FOR },
         { "function", TK_FUNCTION },
         { "global", TK_GLOBAL },
         { "goto", TK_GOTO },
         { "if", TK_IF },
         { "import", TK_IMPORT },
         { "int", TK_INT },
         { "lightning", TK_LIGHTNING },
         { "net", TK_NET },
         { "open", TK_OPEN },
         { "pickup", TK_PICKUP },
         { "redreturn", TK_RED_RETURN },
         { "region", TK_REGION },
         { "respawn", TK_RESPAWN },
         { "restart", TK_RESTART },
         { "return", TK_RETURN },
         { "script", TK_SCRIPT },
         { "special", TK_RESERVED },
         { "static", TK_STATIC },
         { "str", TK_STR },
         { "struct", TK_STRUCT },
         { "suspend", TK_SUSPEND },
         { "switch", TK_SWITCH },
         { "terminate", TK_TERMINATE },
         { "true", TK_TRUE },
         { "unloading", TK_UNLOADING },
         { "until", TK_UNTIL },
         { "upmost", TK_UPMOST },
         { "void", TK_VOID },
         { "while", TK_WHILE },
         { "whitereturn", TK_WHITE_RETURN },
         { "world", TK_WORLD },
         // Terminator.
         { "\x7F", TK_END } };
      #define RESERVED_MAX ( sizeof( reserved ) / sizeof( reserved[ 0 ] ) )
      #define RESERVED_MID ( RESERVED_MAX / 2 )
      int i = 0;
      if ( *id >= *reserved[ RESERVED_MID ].name ) {
         i = RESERVED_MID;
      }
      while ( true ) {
         // Identifier.
         if ( reserved[ i ].name[ 0 ] > *id ) {
            tk = TK_ID;
            break;
         }
         // Reserved identifier.
         else if ( strcmp( reserved[ i ].name, id ) == 0 ) {
            tk = reserved[ i ].tk;
            break;
         }
         else {
            ++i;
         }
      }
      is_id = true;
      goto state_finish;
   }
   else if ( *ch == '0' ) {
      ++ch;
      // Hexadecimal.
      if ( *ch == 'x' || *ch == 'X' ) {
         ++ch;
         while (
            ( *ch >= '0' && *ch <= '9' ) ||
            ( *ch >= 'a' && *ch <= 'f' ) ||
            ( *ch >= 'A' && *ch <= 'F' ) ) {
            *save = *ch;
            ++save;
            ++ch;
         }
         *save = 0;
         ++save;
         tk = TK_LIT_HEX;
      }
      // Fixed-point number.
      else if ( *ch == '.' ) {
         save[ 0 ] = '0';
         save[ 1 ] = '.';
         save += 2;
         ++ch;
         goto state_fraction;
      }
      // Octal.
      else if ( *ch >= '0' && *ch <= '7' ) {
         while ( *ch >= '0' && *ch <= '7' ) {
            *save = *ch;
            ++save;
            ++ch;
         }
         *save = 0;
         ++save;
         tk = TK_LIT_OCTAL;
         goto state_finish;
      }
      // Decimal zero.
      else {
         save[ 0 ] = '0';
         save[ 1 ] = 0;
         save += 2;
         tk = TK_LIT_DECIMAL;
      }
      goto state_finish_number;
   }
   else if ( isdigit( *ch ) ) {
      while ( isdigit( *ch ) ) {
         *save = *ch;
         ++save;
         ++ch;
      }
      // Fixed-point number.
      if ( *ch == '.' ) {
         *save = *ch;
         ++save;
         ++ch;
         goto state_fraction;
      }
      else {
         *save = 0;
         ++save;
         tk = TK_LIT_DECIMAL;
      }
      goto state_finish_number;
   }
   else if ( *ch == '"' ) {
      ++ch;
      goto state_string;
   }
   else if ( *ch == '\'' ) {
      ++ch;
      if ( *ch == '\\' ) {
         ++ch;
         if ( *ch == '\'' ) {
            save[ 0 ] = *ch;
            save[ 1 ] = 0;
            save += 2;
            ++ch;
         }
         else {
            escape_ch( task, ch, &ch, save, &save, false );
         }
      }
      else if ( *ch == '\'' || ! *ch ) {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = column;
         t_diag( task, DIAG_POS_ERR, &pos,
            "missing character in character literal" );
         t_bail( task );
      }
      else {
         save[ 0 ] = *ch;
         save[ 1 ] = 0;
         save += 2;
         ++ch;
      }
      if ( *ch != '\'' ) {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = column;
         t_diag( task, DIAG_POS_ERR, &pos,
            "multiple characters in character literal" );
         t_bail( task );
      }
      ++ch;
      tk = TK_LIT_CHAR;
      goto state_finish;
   }
   else if ( *ch == '/' ) {
      ++ch;
      if ( *ch == '=' ) {
         tk = TK_ASSIGN_DIV;
         ++ch;
         goto state_finish;
      }
      else if ( *ch == '/' ) {
         goto state_comment;
      }
      else if ( *ch == '*' ) {
         ++ch;
         goto state_comment_m;
      }
      else {
         tk = TK_SLASH;
         goto state_finish;
      }
   }
   else if ( *ch == '=' ) {
      ++ch;
      if ( *ch == '=' ) {
         tk = TK_EQ;
         ++ch;
      }
      else {
         tk = TK_ASSIGN;
      }
      goto state_finish;
   }
   else if ( *ch == '+' ) {
      ++ch;
      if ( *ch == '+' ) {
         tk = TK_INC;
         ++ch;
      }
      else if ( *ch == '=' ) {
         tk = TK_ASSIGN_ADD;
         ++ch;
      }
      else {
         tk = TK_PLUS;
      }
      goto state_finish;
   }
   else if ( *ch == '-' ) {
      ++ch;
      if ( *ch == '-' ) {
         tk = TK_DEC;
         ++ch;
      }
      else if ( *ch == '=' ) {
         tk = TK_ASSIGN_SUB;
         ++ch;
      }
      else {
         tk = TK_MINUS;
      }
      goto state_finish;
   }
   else if ( *ch == '<' ) {
      ++ch;
      if ( *ch == '=' ) {
         tk = TK_LTE;
         ++ch;
      }
      else if ( *ch == '<' ) {
         ++ch;
         if ( *ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_L;
            ++ch;
         }
         else {
            tk = TK_SHIFT_L;
         }
      }
      else {
         tk = TK_LT;
      }
      goto state_finish;
   }
   else if ( *ch == '>' ) {
      ++ch;
      if ( *ch == '=' ) {
         tk = TK_GTE;
         ++ch;
      }
      else if ( *ch == '>' ) {
         ++ch;
         if ( *ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_R;
            ++ch;
            goto state_finish;
         }
         else {
            tk = TK_SHIFT_R;
            goto state_finish;
         }
      }
      else {
         tk = TK_GT;
      }
      goto state_finish;
   }
   else if ( *ch == '&' ) {
      ++ch;
      if ( *ch == '&' ) {
         tk = TK_LOG_AND;
         ++ch;
      }
      else if ( *ch == '=' ) {
         tk = TK_ASSIGN_BIT_AND;
         ++ch;
      }
      else {
         tk = TK_BIT_AND;
      }
      goto state_finish;
   }
   else if ( *ch == '|' ) {
      ++ch;
      if ( *ch == '|' ) {
         tk = TK_LOG_OR;
         ++ch;
      }
      else if ( *ch == '=' ) {
         tk = TK_ASSIGN_BIT_OR;
         ++ch;
      }
      else {
         tk = TK_BIT_OR;
      }
      goto state_finish;
   }
   else if ( *ch == '^' ) {
      ++ch;
      if ( *ch == '=' ) {
         tk = TK_ASSIGN_BIT_XOR;
         ++ch;
      }
      else {
         tk = TK_BIT_XOR;
      }
      goto state_finish;
   }
   else if ( *ch == '!' ) {
      ++ch;
      if ( *ch == '=' ) {
         tk = TK_NEQ;
         ++ch;
      }
      else {
         tk = TK_LOG_NOT;
      }
      goto state_finish;
   }
   else if ( *ch == '*' ) {
      ++ch;
      if ( *ch == '=' ) {
         tk = TK_ASSIGN_MUL;
         ++ch;
      }
      else {
         tk = TK_STAR;
      }
      goto state_finish;
   }
   else if ( *ch == '%' ) {
      ++ch;
      if ( *ch == '=' ) {
         tk = TK_ASSIGN_MOD;
         ++ch;
      }
      else {
         tk = TK_MOD;
      }
      goto state_finish;
   }
   else if ( *ch == ':' ) {
      ++ch;
      if ( *ch == '=' ) {
         tk = TK_ASSIGN_COLON;
         ++ch;
      }
      else if ( *ch == ':' ) {
         tk = TK_COLON_2;
         ++ch;
      }
      else {
         tk = TK_COLON;
      }
      goto state_finish;
   }
   else if ( *ch == '\\' ) {
      struct pos pos = { task->module->id, column };
      t_diag( task, DIAG_POS_ERR, &pos,
         "`\\` not followed with newline character" );
      t_bail( task );
   }
   // End.
   else if ( ! *ch ) {
      tk = TK_END;
      goto state_finish;
   }
   else {
      // Single character tokens.
      static const int singles[] = {
         ';', TK_SEMICOLON,
         ',', TK_COMMA,
         '(', TK_PAREN_L,
         ')', TK_PAREN_R,
         '{', TK_BRACE_L,
         '}', TK_BRACE_R,
         '[', TK_BRACKET_L,
         ']', TK_BRACKET_R,
         '~', TK_BIT_NOT,
         '.', TK_DOT,
         '#', TK_HASH,
         0 };
      int i = 0;
      while ( true ) {
         if ( singles[ i ] == *ch ) {
            tk = singles[ i + 1 ];
            ++ch;
            goto state_finish;
         }
         else if ( ! singles[ i ] ) {
            struct pos pos = { task->module->id, column };
            t_diag( task, DIAG_POS_ERR, &pos, "invalid character `%c`", *ch );
            t_bail( task );
         }
         else {
            i += 2;
         }
      }
   }

   state_fraction:
   // -----------------------------------------------------------------------
   if ( ! isdigit( *ch ) ) {
      struct pos pos;
      pos.module = task->module->id;
      pos.column = column;
      t_diag( task, DIAG_POS_ERR, &pos,
         "fixed-point number missing fractional part" );
      t_bail( task );
   }
   while ( isdigit( *ch ) ) {
      *save = *ch;
      ++save;
      ++ch;
   }
   *save = 0;
   ++save;
   tk = TK_LIT_FIXED;
   goto state_finish_number;

   state_finish_number:
   // -----------------------------------------------------------------------
   // Numbers need to be separated from identifiers.
   if ( isalpha( *ch ) ) {
      struct pos pos;
      pos.module = task->module->id;
      pos.column = column;
      t_diag( task, DIAG_POS_ERR, &pos, "number combined with identifier" );
      t_bail( task );
   }
   goto state_finish;

   state_string:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! *ch ) {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = column;
         t_diag( task, DIAG_POS_ERR, &pos, "unterminated string" );
         t_bail( task );
      }
      else if ( *ch == '"' ) {
         *save = 0;
         ++save;
         ++ch;
         tk = TK_LIT_STRING;
         goto state_finish;
      }
      else if ( *ch == '\\' ) {
         ++ch;
         if ( *ch == '"' ) {
            *save = *ch;
            ++save;
            ++ch;
         }
         // Color codes are not parsed.
         else if ( *ch == 'c' || *ch == 'C' ) {
            save[ 0 ] = '\\';
            save[ 1 ] = *ch;
            save += 2;
            ++ch;
         }
         else {
            escape_ch( task, ch, &ch, save, &save, true );
         }
      }
      else {
         *save = *ch;
         ++save;
         ++ch;
      }
   }

   state_comment:
   // -----------------------------------------------------------------------
   while ( *ch && *ch != '\n' ) {
      ++ch;
   }
   goto state_space;

   state_comment_m:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! *ch ) {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = column;
         t_diag( task, DIAG_POS_ERR, &pos, "unterminated comment" );
         t_bail( task );
      }
      else if ( *ch == '*' ) {
         ++ch;
         if ( *ch == '/' ) {
            ++ch;
            goto state_space;
         }
      }
      else {
         ++ch;
      }
   }

   state_finish:
   // -----------------------------------------------------------------------
   token->type = tk;
   token->text = NULL;
   token->length = 0;
   if ( save != module->save ) {
      token->text = module->save;
      token->length = save - task->module->save - 1;
      module->save = save;
   }
   token->pos.module = task->module->id;
   token->pos.column = column;
   token->next = NULL;
   token->is_dirc_hash = false;
   token->is_id = is_id;
   token->reread = false;
   token->space_before = space_before;
   module->ch = ch;
}

/*
void read_ch( struct task* task ) {
   // Calculate column position of the new character. Use the previous
   // character to calculate the position.
   if ( task->ch == '\t' ) {
      int column = task->column -
         task->module->lines.columns[ task->module->lines.count - 1 ];
      task->column += task->options->tab_size -
         ( ( column + task->options->tab_size ) % task->options->tab_size );
   }
   else {
      ++task->column;
      // Create a new line.
      if ( task->ch == '\n' ) {
         add_line( task->module, task->column );
      }
   }
   // Concatenate lines.
   char* ch = task->module->ch;
   while ( ch[ 0 ] == '\\' ) {
      // Linux.
      if ( ch[ 1 ] == '\n' ) {
         add_line( task->module, task->column );
         ch += 2;
      }
      // Windows.
      else if ( ch[ 1 ] == '\r' && ch[ 2 ] == '\n' ) {
         add_line( task->module, task->column );
         ch += 3;
      }
      else {
         break;
      }
   }
   task->module->ch = ch + 1;
   task->ch = *ch;
}*/

void escape_ch( struct task* task, char* ch, char** ch_update, char* save,
   char** save_update, bool in_string ) {
   if ( ! *ch ) {
      empty: ;
      struct pos pos;
      pos.module = task->module->id;
      pos.column = task->module->tab_space;
      t_diag( task, DIAG_POS_ERR, &pos, "empty escape sequence" );
      t_bail( task );
   }
   int slash = task->module->tab_space - 1;
   static const char singles[] = {
      'a', '\a',
      'b', '\b',
      'f', '\f',
      'n', '\n',
      'r', '\r',
      't', '\t',
      'v', '\v',
      0
   };
   int i = 0;
   while ( singles[ i ] ) {
      if ( singles[ i ] == *ch ) {
         *save = singles[ i + 1 ];
         ++save;
         ++ch;
         goto finish;
      }
      i += 2;
   }
   // Octal notation.
   char buffer[ 4 ];
   int code = 0;
   i = 0;
   while ( *ch >= '0' && *ch <= '7' ) {
      if ( i == 3 ) {
         too_many_digits: ;
         struct pos pos;
         pos.module = task->module->id;
         pos.column = task->module->tab_space;
         t_diag( task, DIAG_POS_ERR, &pos, "too many digits" );
         t_bail( task );
      }
      buffer[ i ] = *ch;
      ++ch;
      ++i;
   }
   if ( i ) {
      buffer[ i ] = 0;
      code = strtol( buffer, NULL, 8 );
      goto save_ch;
   }
   if ( *ch == '\\' ) {
      // In a string context, like the NUL character, the backslash character
      // must not be escaped.
      if ( in_string ) {
         save[ 0 ] = '\\';
         save[ 1 ] = '\\';
         save += 2;
      }
      else {
         save[ 0 ] = '\\';
         save += 1;
      }
      ++ch;
   }
   // Hexadecimal notation.
   else if ( *ch == 'x' || *ch == 'X' ) {
      ++ch;
      i = 0;
      while (
         ( *ch >= '0' && *ch <= '9' ) ||
         ( *ch >= 'a' && *ch <= 'f' ) ||
         ( *ch >= 'A' && *ch <= 'F' ) ) {
         if ( i == 2 ) {
            goto too_many_digits;
         }
         buffer[ i ] = *ch;
         ++ch;
         ++i;
      }
      if ( ! i ) {
         goto empty;
      }
      buffer[ i ] = 0;
      code = strtol( buffer, NULL, 16 );
      goto save_ch;
   }
   else {
      // In a string context, when encountering an unknown escape sequence,
      // leave it for the engine to process.
      if ( in_string ) {
         // TODO: Merge this code and the code above. Both handle the newline
         // character.
         if ( *ch == '\n' ) {
            t_bail( task );
         }
         save[ 0 ] = '\\';
         save[ 1 ] = *ch;
         save += 2;
         ++ch;
      }
      else {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = slash;
         t_diag( task, DIAG_POS_ERR, &pos, "unknown escape sequence" );
         t_bail( task );
      }
   }
   goto finish;

   save_ch:
   // -----------------------------------------------------------------------
   // Code needs to be a valid character.
   if ( code > 127 ) {
      struct pos pos;
      pos.module = task->module->id;
      pos.column = slash;
      t_diag( task, DIAG_POS_ERR, &pos, "invalid character `\\%s`", buffer );
      t_bail( task );
   }
   // In a string context, the NUL character must not be escaped. Leave it
   // for the engine to process it.
   if ( code == 0 && in_string ) {
      save[ 0 ] = '\\';
      save[ 1 ] = '0';
      save += 2;
   }
   else {
      *save = ( char ) code;
      ++save;
   }

   finish:
   // -----------------------------------------------------------------------
   *ch_update = ch;
   *save_update = save;
}

void t_test_tk( struct task* task, enum tk expected ) {
   if ( task->tk != expected ) {
      if ( task->tk == TK_END ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "need %s but reached end of file",
            t_get_token_name( expected ) );
      }
      else if ( task->tk == TK_RESERVED ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "`%s` is a reserved identifier that is not currently used",
            task->tk_text );
      }
      else {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos, 
            "unexpected token" );
         t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos, 
            "need %s but got %s", t_get_token_name( expected ),
            t_get_token_name( task->tk ) );
      }
      t_bail( task );
   }
}

const char* t_get_token_name( enum tk tk ) {
   static const struct { enum tk tk; const char* name; } names[] = {
      { TK_BRACKET_L, "`[`" },
      { TK_BRACKET_R, "`]`" },
      { TK_PAREN_L, "`(`" },
      { TK_PAREN_R, "`)`" },
      { TK_BRACE_L, "`{`" },
      { TK_BRACE_R, "`}`" },
      { TK_DOT, "`.`" },
      { TK_INC, "`++`" },
      { TK_DEC, "`--`" },
      { TK_COMMA, "`,`" },
      { TK_COLON, "`:`" },
      { TK_SEMICOLON, "`;`" },
      { TK_ASSIGN, "`=`" },
      { TK_ASSIGN_ADD, "`+=`" },
      { TK_ASSIGN_SUB, "`-=`" },
      { TK_ASSIGN_MUL, "`*=`" },
      { TK_ASSIGN_DIV, "`/=`" },
      { TK_ASSIGN_MOD, "`%=`" },
      { TK_ASSIGN_SHIFT_L, "`<<=`" },
      { TK_ASSIGN_SHIFT_R, "`>>=`" },
      { TK_ASSIGN_BIT_AND, "`&=`" },
      { TK_ASSIGN_BIT_XOR, "`^=`" },
      { TK_ASSIGN_BIT_OR, "`|=`" },
      { TK_ASSIGN_COLON, "`:=`" },
      { TK_EQ, "`==`" },
      { TK_NEQ, "`!=`" },
      { TK_LOG_NOT, "`!`" },
      { TK_LOG_AND, "`&&`" },
      { TK_LOG_OR, "`||`" },
      { TK_BIT_AND, "`&`" },
      { TK_BIT_OR, "`|`" },
      { TK_BIT_XOR, "`^`" },
      { TK_BIT_NOT, "`~`" },
      { TK_LT, "`<`" },
      { TK_LTE, "`<=`" },
      { TK_GT, "`>`" },
      { TK_GTE, "`>=`" },
      { TK_PLUS, "`+`" },
      { TK_MINUS, "`-`" },
      { TK_SLASH, "`/`" },
      { TK_STAR, "`*`" },
      { TK_MOD, "`%`" },
      { TK_SHIFT_L, "`<<`" },
      { TK_SHIFT_R, "`>>`" },
      { TK_HASH, "`#`" },
      { TK_BREAK, "`break`" },
      { TK_CASE, "`case`" },
      { TK_CONST, "`const`" },
      { TK_CONTINUE, "`continue`" },
      { TK_DEFAULT, "`default`" },
      { TK_DO, "`do`" },
      { TK_ELSE, "`else`" },
      { TK_ENUM, "`enum`" },
      { TK_FOR, "`for`" },
      { TK_IF, "`if`" },
      { TK_INT, "`int`" },
      { TK_RETURN, "`return`" },
      { TK_STATIC, "`static`" },
      { TK_STR, "`str`" },
      { TK_STRUCT, "`struct`" },
      { TK_SWITCH, "`switch`" },
      { TK_VOID, "`void`" },
      { TK_WHILE, "`while`" },
      { TK_BOOL, "`bool`" },
      { TK_PALTRANS, "`createtranslation`" },
      { TK_GLOBAL, "`global`" },
      { TK_SCRIPT, "`script`" },
      { TK_UNTIL, "`until`" },
      { TK_WORLD, "`world`" },
      { TK_OPEN, "`open`" },
      { TK_RESPAWN, "`respawn`" },
      { TK_DEATH, "`death`" },
      { TK_ENTER, "`enter`" },
      { TK_PICKUP, "`pickup`" },
      { TK_BLUE_RETURN, "`bluereturn`" },
      { TK_RED_RETURN, "`redreturn`" },
      { TK_WHITE_RETURN, "`whitereturn`" },
      { TK_LIGHTNING, "`lightning`" },
      { TK_DISCONNECT, "`disconnect`" },
      { TK_UNLOADING, "`unloading`" },
      { TK_CLIENTSIDE, "`clientside`" },
      { TK_NET, "`net`" },
      { TK_RESTART, "`restart`" },
      { TK_SUSPEND, "`suspend`" },
      { TK_TERMINATE, "`terminate`" },
      { TK_FUNCTION, "`function`" },
      { TK_IMPORT, "`import`" },
      { TK_GOTO, "`goto`" },
      { TK_TRUE, "`true`" },
      { TK_FALSE, "`false`" },
      { TK_IMPORT, "`import`" },
      { TK_REGION, "`region`" },
      { TK_UPMOST, "`upmost`" },
      { TK_EVENT, "`event`" },
      { TK_LIT_OCTAL, "octal number" },
      { TK_LIT_DECIMAL, "decimal number" },
      { TK_LIT_HEX, "hexadecimal number" },
      { TK_LIT_FIXED, "fixed-point number" },
      { TK_NL, "newline character" },
      { TK_LIB, "start-of-library" },
      { TK_COLON_2, "`::`" } };
   STATIC_ASSERT( TK_TOTAL == 105 );
   switch ( tk ) {
   case TK_LIT_STRING:
      return "string";
   case TK_LIT_CHAR:
      return "character literal";
   case TK_ID:
      return "identifier";
   default:
      for ( size_t i = 0; i < ARRAY_SIZE( names ); ++i ) {
         if ( names[ i ].tk == tk ) {
            return names[ i ].name;
         }
      }
      return "";
   }
}

static void diag_acc( struct task*, int, va_list* );

void t_diag( struct task* task, int flags, ... ) {
   va_list args;
   va_start( args, flags );
   if ( task->options->acc_err ) {
      diag_acc( task, flags, &args );
   }
   else {
      if ( flags & DIAG_FILE ) {
         const char* file = NULL;
         int line = 0, column = 0;
         make_pos( task, va_arg( args, struct pos* ), &file, &line,
            &column );
         printf( "%s", file );
         if ( flags & DIAG_LINE ) {
            printf( ":%d", line );
            if ( flags & DIAG_COLUMN ) {
               // One-based column.
               if ( task->options->one_column ) {
                  ++column;
               }
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
      const char* format = va_arg( args, const char* );
      vprintf( format, args );
      printf( "\n" );
   }
   va_end( args );
}

// Line format: <file>:<line>: <message>
void diag_acc( struct task* task, int flags, va_list* args ) {
   if ( ! task->err_file ) {
      struct str str;
      str_init( &str );
      if ( task->module_main ) {
         str_copy( &str, task->module_main->file->path.value,
            task->module_main->file->path.length );
         while ( str.length && str.value[ str.length - 1 ] != '/' &&
            str.value[ str.length - 1 ] != '\\' ) {
            str.value[ str.length - 1 ] = 0;
            --str.length;
         }
      }
      str_append( &str, "acs.err" );
      task->err_file = fopen( str.value, "w" );
      if ( ! task->err_file ) {
         printf( "error: failed to load error output file: %s\n", str.value );
         t_bail( task );
      }
      str_deinit( &str );
   }
   if ( flags & DIAG_FILE ) {
      const char* file = NULL;
      int line = 0, column = 0;
      make_pos( task, va_arg( *args, struct pos* ), &file, &line, &column );
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

void make_pos( struct task* task, struct pos* pos, const char** file,
   int* line, int* column ) {
   // Find the module.
   struct module* module = NULL;
   list_iter_t i;
   list_iter_init( &i, &task->loaded_modules );
   while ( ! list_end( &i ) ) {
      module = list_data( &i );
      if ( module->id == pos->module ) {
         break;
      }
      list_next( &i );
   }
   // #line directive -- file
   if ( task->line_pos.file ) {
      *file = task->line_pos.file;
      if ( ! **file ) {
         *file = "(unnamed)";
      }
   }
   else {
      *file = module->file->path.value;
   }
   // Find the line that the column resides on.
   int k = 0;
   int k_last = 0;
   while ( k < module->lines.count &&
      pos->column >= module->lines.columns[ k ] ) {
      k_last = k;
      ++k;
   }
   // #line directive -- line number
   if ( task->line_pos.offset ) {
      k = task->line_pos.offset;
      int i = k_last;
      while ( task->line_pos.column < module->lines.columns[ i ] ) {
         ++k;
         --i;
      }
   }
   *line = k;
   *column = pos->column - module->lines.columns[ k_last ];
}
 
void t_bail( struct task* task ) {
   longjmp( *task->bail, 1 );
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
   while ( task->tk != TK_END && task->tk != TK_BRACE_L ) {
      t_read_tk( task );
   }
   t_test_tk( task, TK_BRACE_L );
   t_read_tk( task );
   int depth = 0;
   while ( true ) {
      if ( task->tk == TK_BRACE_L ) {
         ++depth;
         t_read_tk( task );
      }
      else if ( task->tk == TK_BRACE_R ) {
         if ( depth ) {
            --depth;
            t_read_tk( task );
         }
         else {
            break;
         }
      }
      else if ( task->tk == TK_END ) {
         break;
      }
      else {
         t_read_tk( task );
      }
   }
   t_test_tk( task, TK_BRACE_R );
   t_read_tk( task );
}

void t_skip_to_tk( struct task* task, enum tk tk ) {
   while ( true ) {
      if ( task->tk == tk ) {
         break;
      }
      t_read_tk( task );
   }
}