#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "task.h"

struct source_request {
   const char* path;
   struct source* source;
   bool load_once;
   bool err_open;
   bool err_loading;
   bool err_loaded_before;
};

static void load_source( struct task* task, struct source_request* );
static void init_source_request( struct source_request*, const char* );
static enum tk peek( struct task*, int );
static void read_source( struct task*, struct token* );
static void escape_ch( struct task*, char*, char**, bool );
static char read_ch( struct task* );
static void diag_acc( struct task*, int, va_list* );
static void make_pos( struct task*, struct pos*, const char**, int*, int* );

void t_init_fields_token( struct task* task ) {
   task->tk = TK_END;
   task->tk_text = NULL;
   task->tk_length = 0;
   task->source = NULL;
   task->source_main = NULL;
   task->library = NULL;
   task->library_main = NULL;
}

void t_load_main_source( struct task* task ) {
   struct source_request request;
   init_source_request( &request, task->options->source_file );
   load_source( task, &request );
   if ( request.source ) {
      task->source_main = request.source;
      task->library_main->file_pos.id = request.source->id;
   }
   else {
      t_diag( task, DIAG_ERR, "failed to load source file: %s",
         task->options->source_file );
      t_bail( task );
   }
}

struct source* t_load_included_source( struct task* task ) {
   struct source_request request;
   init_source_request( &request, task->tk_text );
   load_source( task, &request );
   if ( ! request.source ) {
      if ( request.err_loading ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "file already being loaded" );
         t_bail( task );
      }
      else {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "failed to load file: %s", task->tk_text );
         t_bail( task );
      }
   }
   return request.source;
}

void init_source_request( struct source_request* request, const char* path ) {
   request->path = path;
   request->source = NULL;
   request->load_once = false;
   request->err_open = false;
   request->err_loaded_before = false;
   request->err_loading = false;
}

void load_source( struct task* task, struct source_request* request ) {
   // Try path directly.
   struct str path;
   str_init( &path );
   str_append( &path, request->path );
   struct fileid fileid;
   if ( c_read_fileid( &fileid, path.value ) ) {
      goto load_file;
   }
   // Try directory of current file.
   if ( task->source ) {
      str_copy( &path, task->source->full_path.value,
         task->source->full_path.length );
      c_extract_dirname( &path );
      str_append( &path, "/" );
      str_append( &path, request->path );
      if ( c_read_fileid( &fileid, path.value ) ) {
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
      if ( c_read_fileid( &fileid, path.value ) ) {
         goto load_file;
      }
      list_next( &i );
   }
   // Error:
   request->err_open = true;
   goto finish;
   load_file:
   // See if the file should be loaded once.
   list_iter_init( &i, &task->loaded_sources );
   while ( ! list_end( &i ) ) {
      struct source* source = list_data( &i );
      if ( c_same_fileid( &fileid, &source->fileid ) ) {
         if ( source->load_once ) {
            request->err_loaded_before = true;
            goto finish;
         }
         else {
            break;
         }
      }
      list_next( &i );
   }
   // The file should not currently be processing.
   struct source* source = task->source;
   while ( source ) {
      if ( c_same_fileid( &source->fileid, &fileid ) ) {
         if ( request->load_once ) {
            source->load_once = true;
            request->err_loaded_before = true;
         }
         else {
            request->err_loading = true;
         }
         goto finish;
      }
      source = source->prev;
   }
   // Load file.
   FILE* fh = fopen( path.value, "rb" );
   if ( ! fh ) {
      request->err_open = true;
      goto finish;
   }
   fseek( fh, 0, SEEK_END );
   size_t size = ftell( fh );
   rewind( fh );
   char* save = mem_alloc( size + 1 + 1 );
   char* text = save + 1;
   size_t num_read = fread( text, sizeof( char ), size, fh );
   fclose( fh );
   if ( num_read != size ) {
      // For now, a read-error will be the same as an open-error.
      request->err_open = true;
      goto finish;
   }
   text[ size ] = 0;
   // Create source.
   source = mem_alloc( sizeof( *source ) );
   source->text = text;
   source->left = text;
   source->save = save;
   source->save[ 0 ] = 0;
   source->prev = task->source;
   source->fileid = fileid;
   str_init( &source->path );
   str_append( &source->path, request->path );
   str_init( &source->full_path );
   c_read_full_path( path.value, &source->full_path );
   source->line = 1;
   source->column = 0;
   source->id = list_size( &task->loaded_sources ) + 1;
   source->active_id = source->id;
   source->find_dirc = true;
   source->load_once = request->load_once;
   source->imported = false;
   source->ch = ' ';
   list_append( &task->loaded_sources, source );
   task->source = source;
   task->tk = TK_END;
   request->source = source;
   finish:
   str_deinit( &path );
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
      read_source( task, token );
      if ( token->type == TK_END ) {
         bool imported = task->source->imported;
         if ( task->source->prev ) {
            task->source = task->source->prev;
            if ( ! imported ) {
               t_read_tk( task );
               return;
            }
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
         read_source( task, token );
         ++task->tokens.peeked;
      }
      ++i;
      if ( i == pos ) {
         return token->type;
      }
   }
}

void read_source( struct task* task, struct token* token ) {
   char ch = task->source->ch;
   char* save = task->source->save;
   int line = 0;
   int column = 0;
   enum tk tk = TK_END;

   state_space:
   // -----------------------------------------------------------------------
   while ( isspace( ch ) ) {
      ch = read_ch( task );
   }

   state_token:
   // -----------------------------------------------------------------------
   line = task->source->line;
   column = task->source->column;
   // Identifier:
   if ( isalpha( ch ) || ch == '_' ) {
      char* id = save;
      while ( isalnum( ch ) || ch == '_' ) {
         *save = tolower( ch );
         ++save;
         ch = read_ch( task );
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
      #define RESERVED_MAX ARRAY_SIZE( reserved )
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
   else if ( ch == '0' ) {
      ch = read_ch( task );
      // Hexadecimal.
      if ( ch == 'x' || ch == 'X' ) {
         char* start = save;
         ch = read_ch( task );
         while (
            ( ch >= '0' && ch <= '9' ) ||
            ( ch >= 'a' && ch <= 'f' ) ||
            ( ch >= 'A' && ch <= 'F' ) ) {
            *save = ch;
            ++save;
            ch = read_ch( task );
         }
         if ( save == start ) {
            struct pos pos = {
               task->source->line,
               task->source->column,
               task->source->active_id };
            t_diag( task, DIAG_POS_ERR, &pos,
               "missing digits" );
            t_bail( task );
         }
         *save = 0;
         ++save;
         tk = TK_LIT_HEX;
         goto state_finish_number;
      }
      // Octal.
      else if ( isdigit( ch ) ) {
         while ( isdigit( ch ) ) {
            if ( ch == '8' || ch == '9' ) {
               struct pos pos = {
                  task->source->line,
                  task->source->column,
                  task->source->active_id };
               t_diag( task, DIAG_POS_ERR, &pos,
                  "invalid digit in octal literal" );
               t_bail( task );
            }
            *save = ch;
            ++save;
            ch = read_ch( task );
         }
         *save = 0;
         ++save;
         tk = TK_LIT_OCTAL;
         goto state_finish_number;
      }
      // Fixed-point number.
      else if ( ch == '.' ) {
         save[ 0 ] = '0';
         save[ 1 ] = '.';
         save += 2;
         ch = read_ch( task );
         goto state_fraction;
      }
      // Decimal zero.
      else {
         save[ 0 ] = '0';
         save[ 1 ] = 0;
         save += 2;
         tk = TK_LIT_DECIMAL;
         goto state_finish_number;
      }
   }
   else if ( isdigit( ch ) ) {
      while ( isdigit( ch ) ) {
         *save = ch;
         ++save;
         ch = read_ch( task );
      }
      // Fixed-point number.
      if ( ch == '.' ) {
         *save = ch;
         ++save;
         ch = read_ch( task );
         goto state_fraction;
      }
      else {
         *save = 0;
         ++save;
         tk = TK_LIT_DECIMAL;
         goto state_finish_number;
      }
   }
   else if ( ch == '"' ) {
      ch = read_ch( task );
      goto state_string;
   }
   else if ( ch == '\'' ) {
      ch = read_ch( task );
      if ( ch == '\'' || ! ch ) {
         struct pos pos = { task->source->line, task->source->column,
            task->source->active_id };
         t_diag( task, DIAG_POS_ERR, &pos,
            "missing character in character literal" );
         t_bail( task );
      }
      if ( ch == '\\' ) {
         ch = read_ch( task );
         if ( ch == '\'' ) {
            save[ 0 ] = ch;
            save[ 1 ] = 0;
            save += 2;
            ch = read_ch( task );
         }
         else {
            escape_ch( task, &ch, &save, false );
         }
      }
      else  {
         save[ 0 ] = ch;
         save[ 1 ] = 0;
         save += 2;
         ch = read_ch( task );
      }
      if ( ch != '\'' ) {
         struct pos pos = { task->source->line, column,
            task->source->active_id };
         t_diag( task, DIAG_POS_ERR, &pos,
            "multiple characters in character literal" );
         t_bail( task );
      }
      ch = read_ch( task );
      tk = TK_LIT_CHAR;
      goto state_finish;
   }
   else if ( ch == '/' ) {
      ch = read_ch( task );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_DIV;
         ch = read_ch( task );
         goto state_finish;
      }
      else if ( ch == '/' ) {
         goto state_comment;
      }
      else if ( ch == '*' ) {
         ch = read_ch( task );
         goto state_comment_m;
      }
      else {
         tk = TK_SLASH;
         goto state_finish;
      }
   }
   else if ( ch == '=' ) {
      ch = read_ch( task );
      if ( ch == '=' ) {
         tk = TK_EQ;
         ch = read_ch( task );
      }
      else {
         tk = TK_ASSIGN;
      }
      goto state_finish;
   }
   else if ( ch == '+' ) {
      ch = read_ch( task );
      if ( ch == '+' ) {
         tk = TK_INC;
         ch = read_ch( task );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_ADD;
         ch = read_ch( task );
      }
      else {
         tk = TK_PLUS;
      }
      goto state_finish;
   }
   else if ( ch == '-' ) {
      ch = read_ch( task );
      if ( ch == '-' ) {
         tk = TK_DEC;
         ch = read_ch( task );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_SUB;
         ch = read_ch( task );
      }
      else {
         tk = TK_MINUS;
      }
      goto state_finish;
   }
   else if ( ch == '<' ) {
      ch = read_ch( task );
      if ( ch == '=' ) {
         tk = TK_LTE;
         ch = read_ch( task );
      }
      else if ( ch == '<' ) {
         ch = read_ch( task );
         if ( ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_L;
            ch = read_ch( task );
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
   else if ( ch == '>' ) {
      ch = read_ch( task );
      if ( ch == '=' ) {
         tk = TK_GTE;
         ch = read_ch( task );
      }
      else if ( ch == '>' ) {
         ch = read_ch( task );
         if ( ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_R;
            ch = read_ch( task );
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
   else if ( ch == '&' ) {
      ch = read_ch( task );
      if ( ch == '&' ) {
         tk = TK_LOG_AND;
         ch = read_ch( task );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_AND;
         ch = read_ch( task );
      }
      else {
         tk = TK_BIT_AND;
      }
      goto state_finish;
   }
   else if ( ch == '|' ) {
      ch = read_ch( task );
      if ( ch == '|' ) {
         tk = TK_LOG_OR;
         ch = read_ch( task );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_OR;
         ch = read_ch( task );
      }
      else {
         tk = TK_BIT_OR;
      }
      goto state_finish;
   }
   else if ( ch == '^' ) {
      ch = read_ch( task );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_XOR;
         ch = read_ch( task );
      }
      else {
         tk = TK_BIT_XOR;
      }
      goto state_finish;
   }
   else if ( ch == '!' ) {
      ch = read_ch( task );
      if ( ch == '=' ) {
         tk = TK_NEQ;
         ch = read_ch( task );
      }
      else {
         tk = TK_LOG_NOT;
      }
      goto state_finish;
   }
   else if ( ch == '*' ) {
      ch = read_ch( task );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_MUL;
         ch = read_ch( task );
      }
      else {
         tk = TK_STAR;
      }
      goto state_finish;
   }
   else if ( ch == '%' ) {
      ch = read_ch( task );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_MOD;
         ch = read_ch( task );
      }
      else {
         tk = TK_MOD;
      }
      goto state_finish;
   }
   else if ( ch == ':' ) {
      ch = read_ch( task );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_COLON;
         ch = read_ch( task );
      }
      else if ( ch == ':' ) {
         tk = TK_COLON_2;
         ch = read_ch( task );
      }
      else {
         tk = TK_COLON;
      }
      goto state_finish;
   }
   else if ( ch == '\\' ) {
      struct pos pos = { task->source->line, column,
         task->source->active_id };
      t_diag( task, DIAG_POS_ERR, &pos,
         "`\\` not followed with newline character" );
      t_bail( task );
   }
   // End.
   else if ( ! ch ) {
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
         if ( singles[ i ] == ch ) {
            tk = singles[ i + 1 ];
            ch = read_ch( task );
            goto state_finish;
         }
         else if ( ! singles[ i ] ) {
            struct pos pos = { task->source->line, column,
               task->source->active_id };
            t_diag( task, DIAG_POS_ERR, &pos, "invalid character" );
            t_bail( task );
         }
         else {
            i += 2;
         }
      }
   }

   state_finish_number:
   // -----------------------------------------------------------------------
   // Numbers need to be separated from identifiers.
   if ( isalpha( ch ) ) {
      struct pos pos = { task->source->line, column,
         task->source->active_id };
      t_diag( task, DIAG_POS_ERR, &pos, "number combined with identifier" );
      t_bail( task );
   }
   goto state_finish;

   state_fraction:
   // -----------------------------------------------------------------------
   if ( ! isdigit( ch ) ) {
      struct pos pos = { task->source->line, column,
         task->source->active_id };
      t_diag( task, DIAG_POS_ERR, &pos,
         "fixed-point number missing fractional part" );
      t_bail( task );
   }
   while ( isdigit( ch ) ) {
      *save = ch;
      ++save;
      ch = read_ch( task );
   }
   tk = TK_LIT_FIXED;
   goto state_finish_number;

   state_string:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! ch ) {
         struct pos pos = { line, column, task->source->active_id };
         t_diag( task, DIAG_POS_ERR, &pos,
            "unterminated string" );
         t_bail( task );
      }
      else if ( ch == '"' ) {
         ch = read_ch( task );
         goto state_string_concat;
      }
      else if ( ch == '\\' ) {
         ch = read_ch( task );
         if ( ch == '"' ) {
            *save = ch;
            ++save;
            ch = read_ch( task );
         }
         // Color codes are not parsed.
         else if ( ch == 'c' || ch == 'C' ) {
            save[ 0 ] = '\\';
            save[ 1 ] = ch;
            save += 2;
            ch = read_ch( task );
         }
         else {
            escape_ch( task, &ch, &save, true );
         }
      }
      else {
         *save = ch;
         ++save;
         ch = read_ch( task );
      }
   }

   state_string_concat:
   // -----------------------------------------------------------------------
   while ( isspace( ch ) ) {
      ch = read_ch( task );
   }
   // Next string.
   if ( ch == '"' ) {
      ch = read_ch( task );
      goto state_string;
   }
   // Done.
   else {
      *save = 0;
      ++save;
      tk = TK_LIT_STRING;
      goto state_finish;
   }

   state_comment:
   // -----------------------------------------------------------------------
   while ( ch && ch != '\n' ) {
      ch = read_ch( task );
   }
   goto state_space;

   state_comment_m:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! ch ) {
         struct pos pos = { line, column, task->source->active_id };
         t_diag( task, DIAG_POS_ERR, &pos, "unterminated comment" );
         t_bail( task );
      }
      else if ( ch == '*' ) {
         ch = read_ch( task );
         if ( ch == '/' ) {
            ch = read_ch( task );
            goto state_space;
         }
      }
      else {
         ch = read_ch( task );
      }
   }

   state_finish:
   // -----------------------------------------------------------------------
   token->type = tk;
   if ( save != task->source->save ) {
      token->text = task->source->save;
      // Minus 1 so to not include the NUL character in the count.
      token->length = save - task->source->save - 1;
      task->source->save = save;
   }
   else {
      token->text = NULL;
      token->length = 0;
   }
   token->pos.line = line;
   token->pos.column = column;
   token->pos.id = task->source->active_id;
   task->source->ch = ch;
}

char read_ch( struct task* task ) {
   // Determine position of character.
   char* left = task->source->left;
   if ( left[ -1 ] ) {
      if ( left[ -1 ] == '\n' ) {
         ++task->source->line;
         task->source->column = 0;
      }
      else if ( left[ -1 ] == '\t' ) {
         task->source->column += task->options->tab_size -
            ( ( task->source->column + task->options->tab_size ) %
            task->options->tab_size );
      }
      else {
         ++task->source->column;
      }
   }
   // Line concatenation.
   while ( left[ 0 ] == '\\' ) {
      // Linux.
      if ( left[ 1 ] == '\n' ) {
         ++task->source->line;
         task->source->column = 0;
         left += 2;
      }
      // Windows.
      else if ( left[ 1 ] == '\r' && left[ 2 ] == '\n' ) {
         ++task->source->line;
         task->source->column = 0;
         left += 3;
      }
      else {
         break;
      }
   }
   // Process character.
   if ( *left == '\n' ) {
      task->source->left = left + 1;
      return '\n';
   }
   else if ( *left == '\r' && left[ 1 ] == '\n' ) {
      task->source->left = left + 2;
      return '\n';
   }
   else {
      task->source->left = left + 1;
      return *left;
   }
}

void escape_ch( struct task* task, char* ch_out, char** save_out,
   bool in_string ) {
   char ch = *ch_out;
   char* save = *save_out;
   if ( ! ch ) {
      empty: ;
      struct pos pos = { task->source->line, task->source->column,
         task->source->active_id };
      t_diag( task, DIAG_POS_ERR, &pos, "empty escape sequence" );
      t_bail( task );
   }
   int slash = task->source->column - 1;
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
      if ( singles[ i ] == ch ) {
         *save = singles[ i + 1 ];
         ++save;
         ch = read_ch( task );
         goto finish;
      }
      i += 2;
   }
   // Octal notation.
   char buffer[ 4 ];
   int code = 0;
   i = 0;
   while ( ch >= '0' && ch <= '7' ) {
      if ( i == 3 ) {
         too_many_digits: ;
         struct pos pos = { task->source->line, task->source->column,
            task->source->active_id };
         t_diag( task, DIAG_POS_ERR, &pos, "too many digits" );
         t_bail( task );
      }
      buffer[ i ] = ch;
      ch = read_ch( task );
      ++i;
   }
   if ( i ) {
      buffer[ i ] = 0;
      code = strtol( buffer, NULL, 8 );
      goto save_ch;
   }
   if ( ch == '\\' ) {
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
      ch = read_ch( task );
   }
   // Hexadecimal notation.
   else if ( ch == 'x' || ch == 'X' ) {
      ch = read_ch( task );
      i = 0;
      while (
         ( ch >= '0' && ch <= '9' ) ||
         ( ch >= 'a' && ch <= 'f' ) ||
         ( ch >= 'A' && ch <= 'F' ) ) {
         if ( i == 2 ) {
            goto too_many_digits;
         }
         buffer[ i ] = ch;
         ch = read_ch( task );
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
         if ( ch == '\n' ) {
            t_bail( task );
         }
         save[ 0 ] = '\\';
         save[ 1 ] = ch;
         save += 2;
         ch = read_ch( task );
      }
      else {
         struct pos pos = { task->source->line, slash,
            task->source->active_id };
         t_diag( task, DIAG_POS_ERR, &pos, "unknown escape sequence" );
         t_bail( task );
      }
   }
   goto finish;

   save_ch:
   // -----------------------------------------------------------------------
   // Code needs to be a valid character.
   if ( code > 127 ) {
      struct pos pos = { task->source->line, slash,
         task->source->active_id };
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
   *ch_out = ch;
   *save_out = save;
}

void t_test_tk( struct task* task, enum tk expected ) {
   if ( task->tk != expected ) {
      if ( task->tk == TK_RESERVED ) {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos,
            "`%s` is a reserved identifier that is not currently used",
            task->tk_text );
      }
      else {
         t_diag( task, DIAG_POS_ERR, &task->tk_pos, 
            "unexpected %s", t_get_token_name( task->tk ) );
         t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "expecting %s here", t_get_token_name( expected ),
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
      { TK_END, "end-of-input" },
      { TK_LIB, "start-of-library" },
      { TK_LIB_END, "end-of-library" },
      { TK_COLON_2, "`::`" } };
   STATIC_ASSERT( TK_TOTAL == 106 );
   switch ( tk ) {
   case TK_LIT_STRING:
      return "string literal";
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
      if ( task->source_main ) {
         str_copy( &str, task->source_main->path.value,
            task->source_main->path.length );
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
   // Path of source file.
   struct source* source = NULL;
   list_iter_t i;
   list_iter_init( &i, &task->loaded_sources );
   while ( ! list_end( &i ) ) {
      source = list_data( &i );
      if ( source->id == pos->id ) {
         break;
      }
      list_next( &i );
   }
   *file = source->path.value;
   *line = pos->line;
   *column = pos->column;
   if ( task->options->one_column ) {
      ++*column;
   }
}
 
void t_bail( struct task* task ) {
   longjmp( *task->bail, 1 );
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
      else if ( task->tk == TK_LIB_END ) {
         break;
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

bool t_same_pos( struct pos* a, struct pos* b ) {
   return (
      a->id == b->id &&
      a->line == b->line &&
      a->column == b->column );
}