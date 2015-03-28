#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "phase.h"

struct request {
   const char* given_path;
   struct source* source;
   struct str path;
   struct fileid fileid;
   bool err_open;
   bool err_loading;
   bool err_loaded_before;
};

static void load_source( struct parse* phase, struct request* );
static void init_request( struct request*, const char* );
static bool find_source_file( struct parse* phase, struct request* request );
static bool source_loading( struct parse* phase, struct request* request );
static void open_source_file( struct parse* phase, struct request* request );
static struct file_entry* assign_file_entry( struct parse* phase,
   struct request* request );
static struct file_entry* create_file_entry( struct parse* phase,
   struct request* request );
static void link_file_entry( struct parse* phase, struct file_entry* entry );
static enum tk peek( struct parse* phase, int );
static void read_source( struct parse* phase, struct token* );
static void escape_ch( struct parse* phase, char*, char**, bool );
static char read_ch( struct parse* phase );

void p_load_main_source( struct parse* phase ) {
   struct request request;
   init_request( &request, phase->task->options->source_file );
   load_source( phase, &request );
   if ( request.source ) {
      phase->main_source = request.source;
      phase->task->library_main->file_pos.id = request.source->file->id;
   }
   else {
      p_diag( phase, DIAG_ERR, "failed to load source file: %s",
         phase->task->options->source_file );
      p_bail( phase );
   }
}

struct source* p_load_included_source( struct parse* phase ) {
   struct request request;
   init_request( &request, phase->tk_text );
   load_source( phase, &request );
   if ( ! request.source ) {
      if ( request.err_loading ) {
         p_diag( phase, DIAG_POS_ERR, &phase->tk_pos,
            "file already being loaded" );
         p_bail( phase );
      }
      else {
         p_diag( phase, DIAG_POS_ERR, &phase->tk_pos,
            "failed to load file: %s", phase->tk_text );
         p_bail( phase );
      }
   }
   return request.source;
}

void init_request( struct request* request, const char* path ) {
   request->given_path = path;
   request->source = NULL;
   str_init( &request->path );
   // NOTE: request->file_id NOT initialized.
   request->err_open = false;
   request->err_loaded_before = false;
   request->err_loading = false;
}

void load_source( struct parse* phase, struct request* request ) {
   if ( find_source_file( phase, request ) ) {
      if ( ! source_loading( phase, request ) ) {
         open_source_file( phase, request );
      }
      else {
         request->err_loading = true;
      }
   }
   else {
      request->err_open = true;
   }
   // Deinitialize request.
   str_deinit( &request->path );
}

bool find_source_file( struct parse* phase, struct request* request ) {
   // Try path directly.
   str_append( &request->path, request->given_path );
   if ( c_read_fileid( &request->fileid, request->path.value ) ) {
      return true;
   }
   // Try directory of current file.
   if ( phase->source ) {
      str_copy( &request->path, phase->source->file->full_path.value,
         phase->source->file->full_path.length );
      c_extract_dirname( &request->path );
      str_append( &request->path, "/" );
      str_append( &request->path, request->given_path );
      if ( c_read_fileid( &request->fileid, request->path.value ) ) {
         return true;
      }
   }
   // Try user-specified directories.
   list_iter_t i;
   list_iter_init( &i, &phase->task->options->includes );
   while ( ! list_end( &i ) ) {
      char* include = list_data( &i ); 
      str_clear( &request->path );
      str_append( &request->path, include );
      str_append( &request->path, "/" );
      str_append( &request->path, request->given_path );
      if ( c_read_fileid( &request->fileid, request->path.value ) ) {
         return true;
      }
      list_next( &i );
   }
   return false;
}

bool source_loading( struct parse* phase, struct request* request ) {
   struct source* source = phase->source;
   while ( source &&
      ! c_same_fileid( &request->fileid, &source->file->file_id ) ) {
      source = source->prev;
   }
   return ( source != NULL );
}

void open_source_file( struct parse* phase, struct request* request ) {
   FILE* fh = fopen( request->path.value, "rb" );
   if ( ! fh ) {
      request->err_open = true;
      return;
   }
   fseek( fh, 0, SEEK_END );
   size_t size = ftell( fh );
   rewind( fh );
   char* save = mem_alloc( size + 1 + 1 );
   char* text = save + 1;
   size_t num_read = fread( text, sizeof( char ), size, fh );
   text[ size ] = 0;
   fclose( fh );
   if ( num_read != size ) {
      // For now, a read-error will be the same as an open-error.
      request->err_open = true;
      return;
   }
   // Create source.
   struct source* source = mem_alloc( sizeof( *source ) );
   source->file = assign_file_entry( phase, request );
   source->text = text;
   source->left = text;
   source->save = save;
   source->save[ 0 ] = 0;
   source->prev = phase->source;
   source->line = 1;
   source->column = 0;
   source->find_dirc = true;
   source->imported = false;
   source->ch = ' ';
   request->source = source;
   phase->source = source;
   phase->tk = TK_END;
}

struct file_entry* assign_file_entry( struct parse* phase,
   struct request* request ) {
   struct file_entry* entry = phase->task->file_entries;
   while ( entry ) {
      if ( c_same_fileid( &request->fileid, &entry->file_id ) ) {
         return entry;
      }
      entry = entry->next;
   }
   return create_file_entry( phase, request );
}

struct file_entry* create_file_entry( struct parse* phase,
   struct request* request ) {
   struct file_entry* entry = mem_alloc( sizeof( *entry ) );
   entry->next = NULL;
   entry->file_id = request->fileid;
   str_init( &entry->path );
   str_append( &entry->path, request->given_path );
   str_init( &entry->full_path );
   c_read_full_path( request->path.value, &entry->full_path );
   entry->id = phase->last_id;
   ++phase->last_id;
   link_file_entry( phase, entry );
   return entry;
}

void link_file_entry( struct parse* phase, struct file_entry* entry ) {
   struct task* task = phase->task;
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

void p_read_tk( struct parse* phase ) {
   struct token* token = NULL;
   if ( phase->peeked ) {
      // When dequeuing, shift the queue elements. For now, this will suffice.
      // In the future, maybe use a circular buffer.
      int i = 0;
      while ( i < phase->peeked ) {
         phase->queue[ i ] = phase->queue[ i + 1 ];
         ++i;
      }
      token = &phase->queue[ 0 ];
      --phase->peeked;
   }
   else {
      token = &phase->queue[ 0 ];
      read_source( phase, token );
      if ( token->type == TK_END ) {
         bool imported = phase->source->imported;
         if ( phase->source->prev ) {
            phase->source = phase->source->prev;
            if ( ! imported ) {
               p_read_tk( phase );
               return;
            }
         }
      }
   }
   phase->tk = token->type;
   phase->tk_text = token->text;
   phase->tk_pos = token->pos;
   phase->tk_length = token->length;
}

enum tk p_peek( struct parse* phase ) {
   return peek( phase, 1 );
}

// NOTE: Make sure @pos is not more than ( TK_BUFFER_SIZE - 1 ).
enum tk peek( struct parse* phase, int pos ) {
   int i = 0;
   while ( true ) {
      // Peeked tokens begin at position 1.
      struct token* token = &phase->queue[ i + 1 ];
      if ( i == phase->peeked ) {
         read_source( phase, token );
         ++phase->peeked;
      }
      ++i;
      if ( i == pos ) {
         return token->type;
      }
   }
}

void read_source( struct parse* phase, struct token* token ) {
   char ch = phase->source->ch;
   char* save = phase->source->save;
   int line = 0;
   int column = 0;
   enum tk tk = TK_END;

   state_space:
   // -----------------------------------------------------------------------
   while ( isspace( ch ) ) {
      ch = read_ch( phase );
   }

   state_token:
   // -----------------------------------------------------------------------
   line = phase->source->line;
   column = phase->source->column;
   // Identifier:
   if ( isalpha( ch ) || ch == '_' ) {
      goto id;
   }
   else if ( ch == '0' ) {
      ch = read_ch( phase );
      // Binary.
      if ( ch == 'b' || ch == 'B' ) {
         goto binary_literal;
      }
      // Hexadecimal.
      else if ( ch == 'x' || ch == 'X' ) {
         goto hex_literal;
      }
      // Fixed-point number.
      else if ( ch == '.' ) {
         save[ 0 ] = '0';
         save[ 1 ] = '.';
         save += 2;
         ch = read_ch( phase );
         goto fraction;
      }
      // Octal.
      else {
         goto octal_literal;
      }
   }
   else if ( isdigit( ch ) ) {
      goto decimal_literal;
   }
   else if ( ch == '"' ) {
      ch = read_ch( phase );
      goto state_string;
   }
   else if ( ch == '\'' ) {
      ch = read_ch( phase );
      if ( ch == '\'' || ! ch ) {
         struct pos pos = {
            .line = phase->source->line,
            .column = phase->source->column,
            .id = phase->source->file->id
         };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "missing character in character literal" );
         p_bail( phase );
      }
      if ( ch == '\\' ) {
         ch = read_ch( phase );
         if ( ch == '\'' ) {
            save[ 0 ] = ch;
            save[ 1 ] = 0;
            save += 2;
            ch = read_ch( phase );
         }
         else {
            escape_ch( phase, &ch, &save, false );
         }
      }
      else  {
         save[ 0 ] = ch;
         save[ 1 ] = 0;
         save += 2;
         ch = read_ch( phase );
      }
      if ( ch != '\'' ) {
         struct pos pos = { phase->source->line, column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "multiple characters in character literal" );
         p_bail( phase );
      }
      ch = read_ch( phase );
      tk = TK_LIT_CHAR;
      goto state_finish;
   }
   else if ( ch == '/' ) {
      ch = read_ch( phase );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_DIV;
         ch = read_ch( phase );
         goto state_finish;
      }
      else if ( ch == '/' ) {
         goto state_comment;
      }
      else if ( ch == '*' ) {
         ch = read_ch( phase );
         goto state_comment_m;
      }
      else {
         tk = TK_SLASH;
         goto state_finish;
      }
   }
   else if ( ch == '=' ) {
      ch = read_ch( phase );
      if ( ch == '=' ) {
         tk = TK_EQ;
         ch = read_ch( phase );
      }
      else {
         tk = TK_ASSIGN;
      }
      goto state_finish;
   }
   else if ( ch == '+' ) {
      ch = read_ch( phase );
      if ( ch == '+' ) {
         tk = TK_INC;
         ch = read_ch( phase );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_ADD;
         ch = read_ch( phase );
      }
      else {
         tk = TK_PLUS;
      }
      goto state_finish;
   }
   else if ( ch == '-' ) {
      ch = read_ch( phase );
      if ( ch == '-' ) {
         tk = TK_DEC;
         ch = read_ch( phase );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_SUB;
         ch = read_ch( phase );
      }
      else {
         tk = TK_MINUS;
      }
      goto state_finish;
   }
   else if ( ch == '<' ) {
      ch = read_ch( phase );
      if ( ch == '=' ) {
         tk = TK_LTE;
         ch = read_ch( phase );
      }
      else if ( ch == '<' ) {
         ch = read_ch( phase );
         if ( ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_L;
            ch = read_ch( phase );
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
      ch = read_ch( phase );
      if ( ch == '=' ) {
         tk = TK_GTE;
         ch = read_ch( phase );
      }
      else if ( ch == '>' ) {
         ch = read_ch( phase );
         if ( ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_R;
            ch = read_ch( phase );
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
      ch = read_ch( phase );
      if ( ch == '&' ) {
         tk = TK_LOG_AND;
         ch = read_ch( phase );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_AND;
         ch = read_ch( phase );
      }
      else {
         tk = TK_BIT_AND;
      }
      goto state_finish;
   }
   else if ( ch == '|' ) {
      ch = read_ch( phase );
      if ( ch == '|' ) {
         tk = TK_LOG_OR;
         ch = read_ch( phase );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_OR;
         ch = read_ch( phase );
      }
      else {
         tk = TK_BIT_OR;
      }
      goto state_finish;
   }
   else if ( ch == '^' ) {
      ch = read_ch( phase );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_XOR;
         ch = read_ch( phase );
      }
      else {
         tk = TK_BIT_XOR;
      }
      goto state_finish;
   }
   else if ( ch == '!' ) {
      ch = read_ch( phase );
      if ( ch == '=' ) {
         tk = TK_NEQ;
         ch = read_ch( phase );
      }
      else {
         tk = TK_LOG_NOT;
      }
      goto state_finish;
   }
   else if ( ch == '*' ) {
      ch = read_ch( phase );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_MUL;
         ch = read_ch( phase );
      }
      else {
         tk = TK_STAR;
      }
      goto state_finish;
   }
   else if ( ch == '%' ) {
      ch = read_ch( phase );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_MOD;
         ch = read_ch( phase );
      }
      else {
         tk = TK_MOD;
      }
      goto state_finish;
   }
   else if ( ch == ':' ) {
      ch = read_ch( phase );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_COLON;
         ch = read_ch( phase );
      }
      else if ( ch == ':' ) {
         tk = TK_COLON_2;
         ch = read_ch( phase );
      }
      else {
         tk = TK_COLON;
      }
      goto state_finish;
   }
   else if ( ch == '\\' ) {
      struct pos pos = { phase->source->line, column,
         phase->source->file->id };
      p_diag( phase, DIAG_POS_ERR, &pos,
         "`\\` not followed with newline character" );
      p_bail( phase );
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
         '?', TK_QUESTION_MARK,
         '#', TK_HASH,
         0 };
      int i = 0;
      while ( true ) {
         if ( singles[ i ] == ch ) {
            tk = singles[ i + 1 ];
            ch = read_ch( phase );
            goto state_finish;
         }
         else if ( ! singles[ i ] ) {
            struct pos pos = { phase->source->line, column,
               phase->source->file->id };
            p_diag( phase, DIAG_POS_ERR, &pos, "invalid character" );
            p_bail( phase );
         }
         else {
            i += 2;
         }
      }
   }

   id:
   // -----------------------------------------------------------------------
   {
      char* id = save;
      while ( isalnum( ch ) || ch == '_' ) {
         *save = tolower( ch );
         ++save;
         ch = read_ch( phase );
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
      if ( strcmp( id, reserved[ RESERVED_MID ].name ) >= 0 ) {
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

   binary_literal:
   // -----------------------------------------------------------------------
   ch = read_ch( phase );
   while ( true ) {
      if ( ch == '0' || ch == '1' ) {
         *save = ch;
         ++save;
         ch = read_ch( phase );
      }
      // Underscores can be used to improve readability of a numeric literal
      // by grouping digits, and are ignored.
      else if ( ch == '_' ) {
         ch = read_ch( phase );
      }
      else if ( isalnum( ch ) ) {
         struct pos pos = { phase->source->line, phase->source->column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "invalid digit in binary literal" );
         p_bail( phase );
      }
      else if ( save == phase->source->save ) {
         struct pos pos = { phase->source->line, column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "no digits found in binary literal" );
         p_bail( phase );
      }
      else {
         *save = 0;
         ++save;
         tk = TK_LIT_BINARY;
         goto state_finish;
      }
   }

   hex_literal:
   // -----------------------------------------------------------------------
   ch = read_ch( phase );
   while ( true ) {
      if ( isxdigit( ch ) ) {
         *save = ch;
         ++save;
         ch = read_ch( phase );
      }
      else if ( ch == '_' ) {
         ch = read_ch( phase );
      }
      else if ( isalnum( ch ) ) {
         struct pos pos = { phase->source->line, phase->source->column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "invalid digit in hexadecimal literal" );
         p_bail( phase );
      }
      else if ( save == phase->source->save ) {
         struct pos pos = { phase->source->line, column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "no digits found in hexadecimal literal" );
         p_bail( phase );
      }
      else {
         *save = 0;
         ++save;
         tk = TK_LIT_HEX;
         goto state_finish;
      }
   }

   octal_literal:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ch >= '0' && ch <= '7' ) {
         *save = ch;
         ++save;
         ch = read_ch( phase );
      }
      else if ( ch == '_' ) {
         ch = read_ch( phase );
      }
      else if ( isalnum( ch ) ) {
         struct pos pos = { phase->source->line, phase->source->column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "invalid digit in octal literal" );
         p_bail( phase );
      }
      else {
         // We consider the number zero to be a decimal literal.
         if ( save == phase->source->save ) {
            save[ 0 ] = '0';
            save[ 1 ] = 0;
            save += 2;
            tk = TK_LIT_DECIMAL;
         }
         else {
            *save = 0;
            ++save;
            tk = TK_LIT_OCTAL;
         }
         goto state_finish;
      }
   }

   decimal_literal:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( isdigit( ch ) ) {
         *save = ch;
         ++save;
         ch = read_ch( phase );
      }
      else if ( ch == '_' ) {
         ch = read_ch( phase );
      }
      // Fixed-point number.
      else if ( ch == '.' ) {
         *save = ch;
         ++save;
         ch = read_ch( phase );
         goto fraction;
      }
      else if ( isalpha( ch ) ) {
         struct pos pos = { phase->source->line, phase->source->column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "invalid digit in octal literal" );
         p_bail( phase );
      }
      else {
         *save = 0;
         ++save;
         tk = TK_LIT_DECIMAL;
         goto state_finish;
      }
   }

   fraction:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( isdigit( ch ) ) {
         *save = ch;
         ++save;
         ch = read_ch( phase );
      }
      else if ( ch == '_' ) {
         ch = read_ch( phase );
      }
      else if ( isalpha( ch ) ) {
         struct pos pos = { phase->source->line, phase->source->column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "invalid digit in fractional part of fixed-point literal" );
         p_bail( phase );
      }
      else if ( save[ -1 ] == '.' ) {
         struct pos pos = { phase->source->line, column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "no digits found in fractional part of fixed-point literal" );
         p_bail( phase );
      }
      else {
         *save = 0;
         ++save;
         tk = TK_LIT_FIXED;
         goto state_finish;
      }
   }

   state_string:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! ch ) {
         struct pos pos = { line, column, phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos,
            "unterminated string" );
         p_bail( phase );
      }
      else if ( ch == '"' ) {
         ch = read_ch( phase );
         goto state_string_concat;
      }
      else if ( ch == '\\' ) {
         ch = read_ch( phase );
         if ( ch == '"' ) {
            *save = ch;
            ++save;
            ch = read_ch( phase );
         }
         // Color codes are not parsed.
         else if ( ch == 'c' || ch == 'C' ) {
            save[ 0 ] = '\\';
            save[ 1 ] = ch;
            save += 2;
            ch = read_ch( phase );
         }
         else {
            escape_ch( phase, &ch, &save, true );
         }
      }
      else {
         *save = ch;
         ++save;
         ch = read_ch( phase );
      }
   }

   state_string_concat:
   // -----------------------------------------------------------------------
   while ( isspace( ch ) ) {
      ch = read_ch( phase );
   }
   // Next string.
   if ( ch == '"' ) {
      ch = read_ch( phase );
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
      ch = read_ch( phase );
   }
   goto state_space;

   state_comment_m:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! ch ) {
         struct pos pos = { line, column, phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos, "unterminated comment" );
         p_bail( phase );
      }
      else if ( ch == '*' ) {
         ch = read_ch( phase );
         if ( ch == '/' ) {
            ch = read_ch( phase );
            goto state_space;
         }
      }
      else {
         ch = read_ch( phase );
      }
   }

   state_finish:
   // -----------------------------------------------------------------------
   token->type = tk;
   if ( save != phase->source->save ) {
      token->text = phase->source->save;
      // Minus 1 so to not include the NUL character in the count.
      token->length = save - phase->source->save - 1;
      phase->source->save = save;
   }
   else {
      token->text = NULL;
      token->length = 0;
   }
   token->pos.line = line;
   token->pos.column = column;
   token->pos.id = phase->source->file->id;
   phase->source->ch = ch;
}

char read_ch( struct parse* phase ) {
   // Determine position of character.
   char* left = phase->source->left;
   if ( left[ -1 ] ) {
      if ( left[ -1 ] == '\n' ) {
         ++phase->source->line;
         phase->source->column = 0;
      }
      else if ( left[ -1 ] == '\t' ) {
         phase->source->column += phase->task->options->tab_size -
            ( ( phase->source->column + phase->task->options->tab_size ) %
            phase->task->options->tab_size );
      }
      else {
         ++phase->source->column;
      }
   }
   // Line concatenation.
   while ( left[ 0 ] == '\\' ) {
      // Linux.
      if ( left[ 1 ] == '\n' ) {
         ++phase->source->line;
         phase->source->column = 0;
         left += 2;
      }
      // Windows.
      else if ( left[ 1 ] == '\r' && left[ 2 ] == '\n' ) {
         ++phase->source->line;
         phase->source->column = 0;
         left += 3;
      }
      else {
         break;
      }
   }
   // Process character.
   if ( *left == '\n' ) {
      phase->source->left = left + 1;
      return '\n';
   }
   else if ( *left == '\r' && left[ 1 ] == '\n' ) {
      phase->source->left = left + 2;
      return '\n';
   }
   else {
      phase->source->left = left + 1;
      return *left;
   }
}

void escape_ch( struct parse* phase, char* ch_out, char** save_out,
   bool in_string ) {
   char ch = *ch_out;
   char* save = *save_out;
   if ( ! ch ) {
      empty: ;
      struct pos pos = { phase->source->line, phase->source->column,
         phase->source->file->id };
      p_diag( phase, DIAG_POS_ERR, &pos, "empty escape sequence" );
      p_bail( phase );
   }
   int slash = phase->source->column - 1;
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
         ch = read_ch( phase );
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
         struct pos pos = { phase->source->line, phase->source->column,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos, "too many digits" );
         p_bail( phase );
      }
      buffer[ i ] = ch;
      ch = read_ch( phase );
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
      ch = read_ch( phase );
   }
   // Hexadecimal notation.
   else if ( ch == 'x' || ch == 'X' ) {
      ch = read_ch( phase );
      i = 0;
      while (
         ( ch >= '0' && ch <= '9' ) ||
         ( ch >= 'a' && ch <= 'f' ) ||
         ( ch >= 'A' && ch <= 'F' ) ) {
         if ( i == 2 ) {
            goto too_many_digits;
         }
         buffer[ i ] = ch;
         ch = read_ch( phase );
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
            p_bail( phase );
         }
         save[ 0 ] = '\\';
         save[ 1 ] = ch;
         save += 2;
         ch = read_ch( phase );
      }
      else {
         struct pos pos = { phase->source->line, slash,
            phase->source->file->id };
         p_diag( phase, DIAG_POS_ERR, &pos, "unknown escape sequence" );
         p_bail( phase );
      }
   }
   goto finish;

   save_ch:
   // -----------------------------------------------------------------------
   // Code needs to be a valid character.
   if ( code > 127 ) {
      struct pos pos = { phase->source->line, slash,
         phase->source->file->id };
      p_diag( phase, DIAG_POS_ERR, &pos, "invalid character `\\%s`", buffer );
      p_bail( phase );
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

void p_test_tk( struct parse* phase, enum tk expected ) {
   if ( phase->tk != expected ) {
      if ( phase->tk == TK_RESERVED ) {
         p_diag( phase, DIAG_POS_ERR, &phase->tk_pos,
            "`%s` is a reserved identifier that is not currently used",
            phase->tk_text );
      }
      else {
         p_diag( phase, DIAG_POS_ERR, &phase->tk_pos, 
            "unexpected %s", p_get_token_name( phase->tk ) );
         p_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &phase->tk_pos,
            "expecting %s here", p_get_token_name( expected ),
            p_get_token_name( phase->tk ) );
      }
      p_bail( phase );
   }
}

const char* p_get_token_name( enum tk tk ) {
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
      { TK_LIT_BINARY, "binary number" },
      { TK_LIT_FIXED, "fixed-point number" },
      { TK_NL, "newline character" },
      { TK_END, "end-of-input" },
      { TK_LIB, "start-of-library" },
      { TK_LIB_END, "end-of-library" },
      { TK_QUESTION_MARK, "`?`" },
      { TK_COLON_2, "`::`" } };
   STATIC_ASSERT( TK_TOTAL == 108 );
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

void p_skip_block( struct parse* phase ) {
   while ( phase->tk != TK_END && phase->tk != TK_BRACE_L ) {
      p_read_tk( phase );
   }
   p_test_tk( phase, TK_BRACE_L );
   p_read_tk( phase );
   int depth = 0;
   while ( true ) {
      if ( phase->tk == TK_BRACE_L ) {
         ++depth;
         p_read_tk( phase );
      }
      else if ( phase->tk == TK_BRACE_R ) {
         if ( depth ) {
            --depth;
            p_read_tk( phase );
         }
         else {
            break;
         }
      }
      else if ( phase->tk == TK_LIB_END ) {
         break;
      }
      else if ( phase->tk == TK_END ) {
         break;
      }
      else {
         p_read_tk( phase );
      }
   }
   p_test_tk( phase, TK_BRACE_R );
   p_read_tk( phase );
}