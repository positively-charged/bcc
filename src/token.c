#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "task.h"

static bool get_full_path( const char*, struct str* );
static void get_dirname( struct str* );
static enum tk peek( struct task*, int );
static void read_token( struct task*, struct token* );
static void read_ch( struct task* );
static void add_line( struct module*, int );
static void escape_ch( struct task*, char*, char**, bool );
static const char* get_token_name( enum tk );
static void make_pos( struct task*, struct pos*, struct file**, int*, int* );

struct module* t_load_module( struct task* task, const char* path,
   struct paused* paused ) {
   struct module* module = NULL;
   // Try path directly.
   struct str file_path;
   str_init( &file_path );
   str_append( &file_path, path );
   struct file_identity identity;
   if ( c_read_identity( &identity, file_path.value ) ) {
      goto load_file;
   }
   // Try directory of current file.
   if ( task->module ) {
      str_copy( &file_path, task->module->file_path.value,
         task->module->file_path.length );
      get_dirname( &file_path );
      str_append( &file_path, "/" );
      str_append( &file_path, path );
      if ( c_read_identity( &identity, file_path.value ) ) {
         goto load_file;
      }
   }
   // Try user-specified directories.
   list_iter_t i;
   list_iter_init( &i, &task->options->includes );
   while ( ! list_end( &i ) ) {
      char* include = list_data( &i ); 
      str_clear( &file_path );
      str_append( &file_path, include );
      str_append( &file_path, "/" );
      str_append( &file_path, path );
      if ( c_read_identity( &identity, file_path.value ) ) {
         goto load_file;
      }
      list_next( &i );
   }
   // Error:
   goto finish;
   load_file:
   // File should be loaded only once.
   list_iter_init( &i, &task->loaded_modules );
   while ( ! list_end( &i ) ) {
      struct module* loaded = list_data( &i );
      if ( c_same_identity( &identity, &loaded->identity ) ) {
         module = loaded;
         goto finish;
      }
      list_next( &i );
   }
   // Load file.
   FILE* fh = fopen( file_path.value, "rb" );
   if ( ! fh ) {
      goto finish;
   }
   struct file* file = mem_alloc( sizeof( *file ) );
   str_init( &file->path );
   str_append( &file->path, path );
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
   module->identity = identity;
   module->file = file;
   str_init( &module->file_path );
   get_full_path( file_path.value, &module->file_path );
   list_init( &module->included );
   module->ch = ch;
   module->ch_start = ch;
   module->ch_save = save_ch;
   module->end = ch;
   module->first_column = ch;
   module->recover_ch = *ch;
   module->line = 1;
   module->lines.count_max = 128;
   module->lines.columns = mem_alloc(
      sizeof( int ) * module->lines.count_max );
   // Column of the first line.
   module->lines.columns[ 0 ] = 0;
   module->lines.count = 1;
   module->id = list_size( &task->loaded_modules );
   module->read = false;
   list_append( &task->loaded_modules, module );
   if ( paused ) {
      paused->module = task->module;
      paused->ch = task->ch;
      paused->column = task->column;
      paused->tk = task->tk;
   }
   task->module = module;
   // Start at a hidden character. This is done do simplify the implementation
   // of the read_ch() function.
   task->ch = ' ';
   task->column = -1;
   finish:
   str_deinit( &file_path );
   return module;
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

void t_resume_module( struct task* task, struct paused* paused ) {
   task->module = paused->module;
   task->ch = paused->ch;
   task->column = paused->column;
   task->tk = paused->tk;
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
      read_token( task, token );
   }
   task->tk = token->type;
   task->tk_text = token->text;
   task->tk_pos = token->pos;
   task->tk_length = token->length;
}

enum tk t_peek( struct task* task ) {
   return peek( task, 1 );
}

enum tk t_peek_2nd( struct task* task ) {
   return peek( task, 2 );
}

// NOTE: Make sure @pos is not more than ( TK_BUFFER_SIZE - 1 ).
enum tk peek( struct task* task, int pos ) {
   int i = 0;
   while ( true ) {
      // Peeked tokens begin at position 1.
      struct token* token = &task->tokens.buffer[ i + 1 ];
      if ( i == task->tokens.peeked ) {
         read_token( task, token );
         ++task->tokens.peeked;
      }
      ++i;
      if ( i == pos ) {
         return token->type;
      }
   }
}

void read_token( struct task* task, struct token* token ) {
   char* save = task->module->ch_save;
   enum tk tk = TK_END;
   int column = 0;

   state_start:
   // -----------------------------------------------------------------------
   while ( isspace( task->ch ) ) {
      read_ch( task );
   }
   column = task->column;
   // Identifier.
   if ( isalpha( task->ch ) || task->ch == '_' ) {
      char* id = save;
      while ( isalnum( task->ch ) || task->ch == '_' ) {
         *save = tolower( task->ch );
         ++save;
         read_ch( task );
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
      goto state_finish;
   }
   else if ( task->ch == '0' ) {
      read_ch( task );
      // Hexadecimal.
      if ( task->ch == 'x' || task->ch == 'X' ) {
         read_ch( task );
         while (
            ( task->ch >= '0' && task->ch <= '9' ) ||
            ( task->ch >= 'a' && task->ch <= 'f' ) ||
            ( task->ch >= 'A' && task->ch <= 'F' ) ) {
            *save = task->ch;
            ++save;
            read_ch( task );
         }
         tk = TK_LIT_HEX;
      }
      // Fixed-point number.
      else if ( task->ch == '.' ) {
         save[ 0 ] = '0';
         save[ 1 ] = '.';
         save += 2;
         read_ch( task );
         goto state_fraction;
      }
      // Octal.
      else if ( task->ch >= '0' && task->ch <= '7' ) {
         while ( task->ch >= '0' && task->ch <= '7' ) {
            *save = task->ch;
            ++save;
            read_ch( task );
         }
         tk = TK_LIT_OCTAL;
         goto state_finish;
      }
      // Decimal zero.
      else {
         *save = '0';
         ++save;
         tk = TK_LIT_DECIMAL;
      }
      goto state_finish_number;
   }
   else if ( isdigit( task->ch ) ) {
      while ( isdigit( task->ch ) ) {
         *save = task->ch;
         ++save;
         read_ch( task );
      }
      // Fixed-point number.
      if ( task->ch == '.' ) {
         *save = task->ch;
         ++save;
         read_ch( task );
         goto state_fraction;
      }
      else {
         tk = TK_LIT_DECIMAL;
      }
      goto state_finish_number;
   }
   else if ( task->ch == '"' ) {
      read_ch( task );
      goto state_string;
   }
   else if ( task->ch == '\'' ) {
      read_ch( task );
      if ( task->ch == '\\' ) {
         read_ch( task );
         if ( task->ch == '\'' ) {
            *save = task->ch;
            ++save;
            read_ch( task );
         }
         else {
            escape_ch( task, save, &save, false );
         }
      }
      else if ( task->ch == '\'' || ! task->ch ) {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = column;
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "missing character in character literal" );
         t_bail( task );
      }
      else {
         save[ 0 ] = task->ch;
         ++save;
         read_ch( task );
      }
      if ( task->ch != '\'' ) {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = column;
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "multiple characters in character literal" );
         t_bail( task );
      }
      read_ch( task );
      tk = TK_LIT_CHAR;
      goto state_finish;
   }
   else if ( task->ch == '/' ) {
      read_ch( task );
      if ( task->ch == '=' ) {
         tk = TK_ASSIGN_DIV;
         read_ch( task );
         goto state_finish;
      }
      else if ( task->ch == '/' ) {
         goto state_comment;
      }
      else if ( task->ch == '*' ) {
         read_ch( task );
         goto state_comment_m;
      }
      else {
         tk = TK_SLASH;
         goto state_finish;
      }
   }
   else if ( task->ch == '=' ) {
      read_ch( task );
      if ( task->ch == '=' ) {
         tk = TK_EQ;
         read_ch( task );
      }
      else {
         tk = TK_ASSIGN;
      }
      goto state_finish;
   }
   else if ( task->ch == '+' ) {
      read_ch( task );
      if ( task->ch == '+' ) {
         tk = TK_INC;
         read_ch( task );
      }
      else if ( task->ch == '=' ) {
         tk = TK_ASSIGN_ADD;
         read_ch( task );
      }
      else {
         tk = TK_PLUS;
      }
      goto state_finish;
   }
   else if ( task->ch == '-' ) {
      read_ch( task );
      if ( task->ch == '-' ) {
         tk = TK_DEC;
         read_ch( task );
      }
      else if ( task->ch == '=' ) {
         tk = TK_ASSIGN_SUB;
         read_ch( task );
      }
      else {
         tk = TK_MINUS;
      }
      goto state_finish;
   }
   else if ( task->ch == '<' ) {
      read_ch( task );
      if ( task->ch == '=' ) {
         tk = TK_LTE;
         read_ch( task );
      }
      else if ( task->ch == '<' ) {
         read_ch( task );
         if ( task->ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_L;
            read_ch( task );
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
   else if ( task->ch == '>' ) {
      read_ch( task );
      if ( task->ch == '=' ) {
         tk = TK_GTE;
         read_ch( task );
      }
      else if ( task->ch == '>' ) {
         read_ch( task );
         if ( task->ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_R;
            read_ch( task );
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
   else if ( task->ch == '&' ) {
      read_ch( task );
      if ( task->ch == '&' ) {
         tk = TK_LOG_AND;
         read_ch( task );
      }
      else if ( task->ch == '=' ) {
         tk = TK_ASSIGN_BIT_AND;
         read_ch( task );
      }
      else {
         tk = TK_BIT_AND;
      }
      goto state_finish;
   }
   else if ( task->ch == '|' ) {
      read_ch( task );
      if ( task->ch == '|' ) {
         tk = TK_LOG_OR;
         read_ch( task );
      }
      else if ( task->ch == '=' ) {
         tk = TK_ASSIGN_BIT_OR;
         read_ch( task );
      }
      else {
         tk = TK_BIT_OR;
      }
      goto state_finish;
   }
   else if ( task->ch == '^' ) {
      read_ch( task );
      if ( task->ch == '=' ) {
         tk = TK_ASSIGN_BIT_XOR;
         read_ch( task );
      }
      else {
         tk = TK_BIT_XOR;
      }
      goto state_finish;
   }
   else if ( task->ch == '!' ) {
      read_ch( task );
      if ( task->ch == '=' ) {
         tk = TK_NEQ;
         read_ch( task );
      }
      else {
         tk = TK_LOG_NOT;
      }
      goto state_finish;
   }
   else if ( task->ch == '*' ) {
      read_ch( task );
      if ( task->ch == '=' ) {
         tk = TK_ASSIGN_MUL;
         read_ch( task );
      }
      else {
         tk = TK_STAR;
      }
      goto state_finish;
   }
   else if ( task->ch == '%' ) {
      read_ch( task );
      if ( task->ch == '=' ) {
         tk = TK_ASSIGN_MOD;
         read_ch( task );
      }
      else {
         tk = TK_MOD;
      }
      goto state_finish;
   }
   else if ( task->ch == ':' ) {
      read_ch( task );
      if ( task->ch == '=' ) {
         tk = TK_ASSIGN_COLON;
         read_ch( task );
      }
      else if ( task->ch == ':' ) {
         tk = TK_COLON2;
         read_ch( task );
      }
      else {
         tk = TK_COLON;
      }
      goto state_finish;
   }
   else if ( task->ch == '\\' ) {
      struct pos pos = { task->module->id, column };
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
         "`\\` not followed with newline character" );
      t_bail( task );
   }
   // End.
   else if ( ! task->ch ) {
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
         if ( singles[ i ] == task->ch ) {
            tk = singles[ i + 1 ];
            read_ch( task );
            goto state_finish;
         }
         else if ( ! singles[ i ] ) {
            struct pos pos = { task->module->id, column };
            t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "invalid character `%c`", task->ch );
            t_bail( task );
         }
         else {
            i += 2;
         }
      }
   }

   state_fraction:
   // -----------------------------------------------------------------------
   if ( ! isdigit( task->ch ) ) {
      struct pos pos;
      pos.module = task->module->id;
      pos.column = column;
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
         "fixed-point number missing fractional part" );
      t_bail( task );
   }
   while ( isdigit( task->ch ) ) {
      *save = task->ch;
      ++save;
      read_ch( task );
   }
   tk = TK_LIT_FIXED;
   goto state_finish_number;

   state_finish_number:
   // -----------------------------------------------------------------------
   // Numbers need to be separated from identifiers.
   if ( isalpha( task->ch ) ) {
      struct pos pos;
      pos.module = task->module->id;
      pos.column = column;
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
         "number combined with identifier" );
      t_bail( task );
   }
   goto state_finish;

   state_string:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! task->ch ) {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = column;
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "unterminated string" );
         t_bail( task );
      }
      else if ( task->ch == '"' ) {
         read_ch( task );
         goto state_string_concat;
      }
      else if ( task->ch == '\\' ) {
         read_ch( task );
         if ( task->ch == '"' ) {
            *save = task->ch;
            ++save;
            read_ch( task );
         }
         // Color codes are not parsed.
         else if ( task->ch == 'c' || task->ch == 'C' ) {
            save[ 0 ] = '\\';
            save[ 1 ] = task->ch;
            save += 2;
            read_ch( task );
         }
         else {
            escape_ch( task, save, &save, true );
         }
      }
      else {
         *save = task->ch;
         ++save;
         read_ch( task );
      }
   }

   state_string_concat:
   // -----------------------------------------------------------------------
   while ( isspace( task->ch ) ) {
      read_ch( task );
   }
   if ( task->ch == '"' ) {
      read_ch( task );
      goto state_string;
   }
   else {
      *save = 0;
      ++save;
      tk = TK_LIT_STRING;
      goto state_finish;
   }

   state_comment:
   // -----------------------------------------------------------------------
   while ( task->ch && task->ch != '\n' ) {
      read_ch( task );
   }
   goto state_start;

   state_comment_m:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! task->ch ) {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = column;
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "unterminated comment" );
         t_bail( task );
      }
      else if ( task->ch == '*' ) {
         read_ch( task );
         if ( task->ch == '/' ) {
            read_ch( task );
            goto state_start;
         }
      }
      else {
         read_ch( task );
      }
   }

   state_finish:
   // -----------------------------------------------------------------------
   token->type = tk;
   token->text = NULL;
   token->length = 0;
   if ( save != task->module->ch_save ) {
      token->text = task->module->ch_save;
      // Terminate the sequence when the last character is not NUL.
      if ( save[ -1 ] ) {
         token->length = save - task->module->ch_save;
         *save = 0;
         ++save;
      }
      else {
         // When the NUL character is manually added, don't consider it as part
         // of the length.
         token->length = save - task->module->ch_save - 1;
      }
      task->module->ch_save = save;
   }
   token->pos.module = task->module->id;
   token->pos.column = column;
}

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

void escape_ch( struct task* task, char* save, char** save_update,
   bool in_string ) {
   if ( ! task->ch ) {
      empty: ;
      struct pos pos;
      pos.module = task->module->id;
      pos.column = task->column;
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
         "empty escape sequence" );
      t_bail( task );
   }
   int slash = task->column - 1;
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
      if ( singles[ i ] == task->ch ) {
         *save = singles[ i + 1 ];
         ++save;
         read_ch( task );
         goto finish;
      }
      i += 2;
   }
   // Octal notation.
   char buffer[ 4 ];
   int code = 0;
   i = 0;
   while ( task->ch >= '0' && task->ch <= '7' ) {
      if ( i == 3 ) {
         too_many_digits: ;
         struct pos pos;
         pos.module = task->module->id;
         pos.column = task->column;
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "too many digits" );
         t_bail( task );
      }
      buffer[ i ] = task->ch;
      read_ch( task );
      ++i;
   }
   if ( i ) {
      buffer[ i ] = 0;
      code = strtol( buffer, NULL, 8 );
      goto save_ch;
   }
   if ( task->ch == '\\' ) {
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
      read_ch( task );
   }
   // Hexadecimal notation.
   else if ( task->ch == 'x' || task->ch == 'X' ) {
      read_ch( task );
      i = 0;
      while (
         ( task->ch >= '0' && task->ch <= '9' ) ||
         ( task->ch >= 'a' && task->ch <= 'f' ) ||
         ( task->ch >= 'A' && task->ch <= 'F' ) ) {
         if ( i == 2 ) {
            goto too_many_digits;
         }
         buffer[ i ] = task->ch;
         read_ch( task );
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
         if ( task->ch == '\n' ) {
            t_bail( task );
         }
         save[ 0 ] = '\\';
         save[ 1 ] = task->ch;
         save += 2;
         read_ch( task );
      }
      else {
         struct pos pos;
         pos.module = task->module->id;
         pos.column = slash;
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "unknown escape sequence" );
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
      t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
         "invalid character `\\%s`", buffer );
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
   *save_update = save;
}

void t_test_tk( struct task* task, enum tk expected ) {
   if ( task->tk != expected ) {
      if ( task->tk == TK_END ) {
         t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "need %s but reached end of file",
            get_token_name( expected ) );
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
            "need %s but got %s", get_token_name( expected ),
            get_token_name( task->tk ) );
      }
      t_bail( task );
   }
}

const char* get_token_name( enum tk tk ) {
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
      { TK_COLON2, "`::`" } };
   switch ( tk ) {
   case TK_LIT_STRING:
      return "string";
   case TK_LIT_CHAR:
      return "character literal";
   case TK_ID:
      return "identifier";
   default:
      for ( int i = 0; i < ARRAY_SIZE( names ); ++i ) {
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
         struct file* file = NULL;
         int line = 0, column = 0;
         make_pos( task, va_arg( args, struct pos* ), &file, &line, &column );
         printf( "%s", file->path.value );
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
      struct file* file = NULL;
      int line = 0, column = 0;
      make_pos( task, va_arg( *args, struct pos* ), &file, &line, &column );
      fprintf( task->err_file, "%s:", file->path.value );
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

void make_pos( struct task* task, struct pos* pos, struct file** file,
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
   *file = module->file;
   // Find the line that the column resides on.
   int k = 0;
   int k_last = 0;
   while ( k < module->lines.count &&
      pos->column >= module->lines.columns[ k ] ) {
      k_last = k;
      ++k;
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