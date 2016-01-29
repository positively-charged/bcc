#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "phase.h"

struct text_buffer {
   char* data;
   unsigned int used;
   unsigned int size;
};

static bool source_loading( struct parse* parse, struct request* request );
static void open_source_file( struct parse* parse, struct request* request );
static struct source* alloc_source( struct parse* parse );
static void reset_filepos( struct source* source );
static void read_token( struct parse* parse, struct token* token );
static void escape_ch( struct parse* parse, char*, struct str* text, bool );
static char read_ch( struct parse* parse );
static void read_initial_ch( struct parse* parse );
static struct str* temp_text( struct parse* parse );
static char* save_text( struct parse* parse );
static unsigned int calc_buffer_size( struct parse* parse );
static void append_ch( struct str* str, char ch );
static const struct token_info* get_token_info( enum tk tk );

void p_load_main_source( struct parse* parse ) {
   struct request request;
   p_init_request( &request, parse->task->options->source_file );
   p_load_source( parse, &request );
   if ( request.source ) {
      parse->main_source = request.source;
   }
   else {
      p_diag( parse, DIAG_ERR, "failed to load source file: %s",
         parse->task->options->source_file );
      p_bail( parse );
   }
}

struct source* p_load_included_source( struct parse* parse ) {
   struct request request;
   p_init_request( &request, parse->tk_text );
   p_load_source( parse, &request );
   if ( ! request.source ) {
      if ( request.err_loading ) {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "file already being loaded" );
         p_bail( parse );
      }
      else {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "failed to load file: %s", parse->tk_text );
         p_bail( parse );
      }
   }
   return request.source;
}

void p_init_request( struct request* request, const char* path ) {
   request->given_path = path;
   request->file = NULL;
   request->source = NULL;
   request->err_open = false;
   request->err_loaded_before = false;
   request->err_loading = false;
}

void p_load_source( struct parse* parse, struct request* request ) {
   struct file_query query;
   t_init_file_query( &query, ( parse->source ? parse->source->file : NULL ),
      request->given_path );
   t_find_file( parse->task, &query );
   if ( query.success ) {
      request->file = query.file;
      if ( ! source_loading( parse, request ) ) {
         open_source_file( parse, request );
      }
      else {
         request->err_loading = true;
      }
   }
   else {
      request->err_open = true;
   }
}

bool source_loading( struct parse* parse, struct request* request ) {
   struct source* source = parse->source;
   while ( source && request->file != source->file ) {
      source = source->prev;
   }
   return ( source != NULL );
}

void open_source_file( struct parse* parse, struct request* request ) {
   FILE* fh = fopen( request->file->full_path.value, "rb" );
   if ( ! fh ) {
      request->err_open = true;
      return;
   }
   // Create source.
   struct source* source = alloc_source( parse );
   source->file = request->file;
   source->file_entry_id = source->file->id;
   source->fh = fh;
   source->prev = parse->source;
   parse->source = source;
   parse->tk = TK_END;
   read_initial_ch( parse );
   request->source = source;
}

struct source* alloc_source( struct parse* parse ) {
   // Allocate.
   struct source* source;
   if ( parse->free_source ) {
      source = parse->free_source;
      parse->free_source = source->prev;
   }
   else {
      source = mem_alloc( sizeof( *source ) );
   }
   // Initialize with default values.
   source->file = NULL;
   source->fh = NULL;
   source->prev = NULL;
   reset_filepos( source );
   source->find_dirc = true;
   source->imported = false;
   source->ch = '\0';
   source->buffer_pos = SOURCE_BUFFER_SIZE;
   return source;
}

void reset_filepos( struct source* source ) {
   source->line = 1;
   source->column = 0;
}

void unload_source( struct parse* parse ) {
   struct source* source = parse->source;
   parse->source = source->prev;
   source->prev = parse->free_source;
   parse->free_source = source;
   fclose( source->fh );
}

void unload_source_all( struct parse* parse ) {
   while ( parse->source ) {
      unload_source( parse );
   }
}

void p_read_source( struct parse* parse, struct token* token ) {
   read_token( parse, token );
   if ( token->type == TK_END ) {
      bool reread = ( ! parse->source->imported );
      unload_source( parse );
      if ( parse->source && reread ) {
         p_read_source( parse, token );
      }
   }
}

void read_token( struct parse* parse, struct token* token ) {
   char ch = parse->source->ch;
   int line = 0;
   int column = 0;
   enum tk tk = TK_END;
   struct str* text = NULL;
   bool is_id = false;

   state_space:
   // -----------------------------------------------------------------------
   switch ( ch ) {
   case ' ':
   case '\t':
      goto spacetab;
   case '\n':
      goto newline;
   default:
      if ( isspace( ch ) ) {
         ch = read_ch( parse );
         goto state_space;
      }
      else {
         goto state_token;
      }
   }

   spacetab:
   // -----------------------------------------------------------------------
   if ( parse->read_flags & READF_SPACETAB ) {
      line = parse->source->line;
      column = parse->source->column;
      tk = ( ch == ' ' ? TK_SPACE : TK_TAB );
      ch = read_ch( parse );
      goto state_finish;
   }
   ch = read_ch( parse );
   goto state_space;

   newline:
   // -----------------------------------------------------------------------
   ch = read_ch( parse );
   if ( parse->read_flags & READF_NL ) {
      line = parse->source->line;
      column = parse->source->column;
      tk = TK_NL;
      goto state_finish;
   }
   goto state_space;

   state_token:
   // -----------------------------------------------------------------------
   line = parse->source->line;
   column = parse->source->column;
   // Identifier:
   if ( isalpha( ch ) || ch == '_' ) {
      goto id;
   }
   else if ( ch == '0' ) {
      ch = read_ch( parse );
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
         text = temp_text( parse );
         append_ch( text, '0' );
         append_ch( text, '.' );
         ch = read_ch( parse );
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
      ch = read_ch( parse );
      goto state_string;
   }
   else if ( ch == '\'' ) {
      text = temp_text( parse );
      ch = read_ch( parse );
      if ( ch == '\'' || ! ch ) {
         struct pos pos = {
            .line = parse->source->line,
            .column = parse->source->column,
            .id = parse->source->file->id
         };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "missing character in character literal" );
         p_bail( parse );
      }
      if ( ch == '\\' ) {
         if ( parse->read_flags & READF_ESCAPESEQ ) {
            ch = read_ch( parse );
            if ( ch == '\'' ) {
               append_ch( text, ch );
               ch = read_ch( parse );
            }
            else {
               escape_ch( parse, &ch, text, false );
            }
         }
         else {
            append_ch( text, ch );
            ch = read_ch( parse );
            append_ch( text, ch );
            ch = read_ch( parse );
         }
      }
      else  {
         append_ch( text, ch );
         ch = read_ch( parse );
      }
      if ( ch != '\'' ) {
         struct pos pos = { parse->source->line, column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "multiple characters in character literal" );
         p_bail( parse );
      }
      ch = read_ch( parse );
      tk = TK_LIT_CHAR;
      goto state_finish;
   }
   else if ( ch == '/' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_DIV;
         ch = read_ch( parse );
         goto state_finish;
      }
      else if ( ch == '/' ) {
         goto state_comment;
      }
      else if ( ch == '*' ) {
         ch = read_ch( parse );
         goto state_comment_m;
      }
      else {
         tk = TK_SLASH;
         goto state_finish;
      }
   }
   else if ( ch == '=' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_EQ;
         ch = read_ch( parse );
      }
      else {
         tk = TK_ASSIGN;
      }
      goto state_finish;
   }
   else if ( ch == '+' ) {
      ch = read_ch( parse );
      if ( ch == '+' ) {
         tk = TK_INC;
         ch = read_ch( parse );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_ADD;
         ch = read_ch( parse );
      }
      else {
         tk = TK_PLUS;
      }
      goto state_finish;
   }
   else if ( ch == '-' ) {
      ch = read_ch( parse );
      if ( ch == '-' ) {
         tk = TK_DEC;
         ch = read_ch( parse );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_SUB;
         ch = read_ch( parse );
      }
      else {
         tk = TK_MINUS;
      }
      goto state_finish;
   }
   else if ( ch == '<' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_LTE;
         ch = read_ch( parse );
      }
      else if ( ch == '<' ) {
         ch = read_ch( parse );
         if ( ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_L;
            ch = read_ch( parse );
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
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_GTE;
         ch = read_ch( parse );
      }
      else if ( ch == '>' ) {
         ch = read_ch( parse );
         if ( ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_R;
            ch = read_ch( parse );
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
      ch = read_ch( parse );
      if ( ch == '&' ) {
         tk = TK_LOG_AND;
         ch = read_ch( parse );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_AND;
         ch = read_ch( parse );
      }
      else {
         tk = TK_BIT_AND;
      }
      goto state_finish;
   }
   else if ( ch == '|' ) {
      ch = read_ch( parse );
      if ( ch == '|' ) {
         tk = TK_LOG_OR;
         ch = read_ch( parse );
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_OR;
         ch = read_ch( parse );
      }
      else {
         tk = TK_BIT_OR;
      }
      goto state_finish;
   }
   else if ( ch == '^' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_XOR;
         ch = read_ch( parse );
      }
      else {
         tk = TK_BIT_XOR;
      }
      goto state_finish;
   }
   else if ( ch == '!' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_NEQ;
         ch = read_ch( parse );
      }
      else {
         tk = TK_LOG_NOT;
      }
      goto state_finish;
   }
   else if ( ch == '*' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_MUL;
         ch = read_ch( parse );
      }
      else {
         tk = TK_STAR;
      }
      goto state_finish;
   }
   else if ( ch == '%' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_MOD;
         ch = read_ch( parse );
      }
      else {
         tk = TK_MOD;
      }
      goto state_finish;
   }
   else if ( ch == ':' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_COLON;
         ch = read_ch( parse );
      }
      else {
         tk = TK_COLON;
      }
      goto state_finish;
   }
   else if ( ch == '\\' ) {
      struct pos pos = { parse->source->line, column,
         parse->source->file->id };
      p_diag( parse, DIAG_POS_ERR, &pos,
         "`\\` not followed with newline character" );
      p_bail( parse );
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
            ch = read_ch( parse );
            goto state_finish;
         }
         else if ( ! singles[ i ] ) {
            struct pos pos = { parse->source->line, column,
               parse->source->file->id };
            p_diag( parse, DIAG_POS_ERR, &pos, "invalid character" );
            p_bail( parse );
         }
         else {
            i += 2;
         }
      }
   }

   id:
   // -----------------------------------------------------------------------
   {
      text = temp_text( parse );
      while ( isalnum( ch ) || ch == '_' ) {
         append_ch( text, tolower( ch ) );
         ch = read_ch( parse );
      }
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
         { "respawn", TK_RESPAWN },
         { "restart", TK_RESTART },
         { "return", TK_RETURN },
         { "script", TK_SCRIPT },
         { "special", TK_RESERVED },
         { "static", TK_STATIC },
         { "str", TK_STR },
         { "strcpy", TK_STRCPY },
         { "struct", TK_STRUCT },
         { "suspend", TK_SUSPEND },
         { "switch", TK_SWITCH },
         { "terminate", TK_TERMINATE },
         { "true", TK_TRUE },
         { "unloading", TK_UNLOADING },
         { "until", TK_UNTIL },
         { "void", TK_VOID },
         { "while", TK_WHILE },
         { "whitereturn", TK_WHITE_RETURN },
         { "world", TK_WORLD },
         // Terminator.
         { "\x7F", TK_END } };
      #define RESERVED_MAX ARRAY_SIZE( reserved )
      #define RESERVED_MID ( RESERVED_MAX / 2 )
      int i = 0;
      const char* id = text->value;
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
            text = NULL;
            break;
         }
         else {
            ++i;
         }
      }
      is_id = true;
      goto state_finish;
   }

   binary_literal:
   // -----------------------------------------------------------------------
   text = temp_text( parse );
   ch = read_ch( parse );
   while ( true ) {
      if ( ch == '0' || ch == '1' ) {
         append_ch( text, ch );
         ch = read_ch( parse );
      }
      // Underscores can be used to improve readability of a numeric literal
      // by grouping digits, and are ignored.
      else if ( ch == '_' ) {
         ch = read_ch( parse );
      }
      else if ( isalnum( ch ) ) {
         struct pos pos = { parse->source->line, parse->source->column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "invalid digit in binary literal" );
         p_bail( parse );
      }
      else if ( text->length == 0 ) {
         struct pos pos = { parse->source->line, column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "no digits found in binary literal" );
         p_bail( parse );
      }
      else {
         tk = TK_LIT_BINARY;
         goto state_finish;
      }
   }

   hex_literal:
   // -----------------------------------------------------------------------
   text = temp_text( parse );
   ch = read_ch( parse );
   while ( true ) {
      if ( isxdigit( ch ) ) {
         append_ch( text, ch );
         ch = read_ch( parse );
      }
      else if ( ch == '_' ) {
         ch = read_ch( parse );
      }
      else if ( isalnum( ch ) ) {
         struct pos pos = { parse->source->line, parse->source->column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "invalid digit in hexadecimal literal" );
         p_bail( parse );
      }
      else if ( text->length == 0 ) {
         struct pos pos = { parse->source->line, column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "no digits found in hexadecimal literal" );
         p_bail( parse );
      }
      else {
         tk = TK_LIT_HEX;
         goto state_finish;
      }
   }

   octal_literal:
   // -----------------------------------------------------------------------
   text = temp_text( parse );
   while ( true ) {
      if ( ch >= '0' && ch <= '7' ) {
         append_ch( text, ch );
         ch = read_ch( parse );
      }
      else if ( ch == '_' ) {
         ch = read_ch( parse );
      }
      else if ( isalnum( ch ) ) {
         struct pos pos = { parse->source->line, parse->source->column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "invalid digit in octal literal" );
         p_bail( parse );
      }
      else {
         // We consider the number zero to be a decimal literal.
         if ( text->length == 0 ) {
            append_ch( text, '0' );
            tk = TK_LIT_DECIMAL;
         }
         else {
            tk = TK_LIT_OCTAL;
         }
         goto state_finish;
      }
   }

   decimal_literal:
   // -----------------------------------------------------------------------
   text = temp_text( parse );
   while ( true ) {
      if ( isdigit( ch ) ) {
         append_ch( text, ch );
         ch = read_ch( parse );
      }
      else if ( ch == '_' ) {
         ch = read_ch( parse );
      }
      // Fixed-point number.
      else if ( ch == '.' ) {
         append_ch( text, ch );
         ch = read_ch( parse );
         goto fraction;
      }
      else if ( isalpha( ch ) ) {
         struct pos pos = { parse->source->line, parse->source->column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "invalid digit in octal literal" );
         p_bail( parse );
      }
      else {
         tk = TK_LIT_DECIMAL;
         goto state_finish;
      }
   }

   fraction:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( isdigit( ch ) ) {
         append_ch( text, ch );
         ch = read_ch( parse );
      }
      else if ( ch == '_' ) {
         ch = read_ch( parse );
      }
      else if ( isalpha( ch ) ) {
         struct pos pos = { parse->source->line, parse->source->column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "invalid digit in fractional part of fixed-point literal" );
         p_bail( parse );
      }
      else if ( text->value[ text->length - 1 ] == '.' ) {
         struct pos pos = { parse->source->line, column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "no digits found in fractional part of fixed-point literal" );
         p_bail( parse );
      }
      else {
         tk = TK_LIT_FIXED;
         goto state_finish;
      }
   }

   state_string:
   // -----------------------------------------------------------------------
   text = temp_text( parse );
   while ( true ) {
      if ( ! ch ) {
         struct pos pos = { line, column, parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos,
            "unterminated string" );
         p_bail( parse );
      }
      else if ( ch == '"' ) {
         ch = read_ch( parse );
         if ( parse->read_flags & READF_CONCATSTRINGS ) {
            while ( isspace( ch ) ) {
               ch = read_ch( parse );
            }
            // Next string.
            if ( ch == '"' ) {
               ch = read_ch( parse );
               continue;
            }
         }
         // Done.
         tk = TK_LIT_STRING;
         goto state_finish;
      }
      else if ( ch == '\\' ) {
         ch = read_ch( parse );
         if ( ch == '"' ) {
            append_ch( text, ch );
            ch = read_ch( parse );
         }
         // Color codes are not parsed.
         else if ( ch == 'c' || ch == 'C' ) {
            append_ch( text, '\\' );
            append_ch( text, ch );
            ch = read_ch( parse );
         }
         else {
            if ( parse->read_flags & READF_ESCAPESEQ ) {
               escape_ch( parse, &ch, text, true );
            }
            else {
               append_ch( text, '\\' );
               append_ch( text, ch );
               ch = read_ch( parse );
            }
         }
      }
      else {
         append_ch( text, ch );
         ch = read_ch( parse );
      }
   }

   state_comment:
   // -----------------------------------------------------------------------
   while ( ch && ch != '\n' ) {
      ch = read_ch( parse );
   }
   goto state_space;

   state_comment_m:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! ch ) {
         struct pos pos = { line, column, parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos, "unterminated comment" );
         p_bail( parse );
      }
      else if ( ch == '*' ) {
         ch = read_ch( parse );
         if ( ch == '/' ) {
            ch = read_ch( parse );
            goto state_space;
         }
      }
      else {
         ch = read_ch( parse );
      }
   }

   state_finish:
   // -----------------------------------------------------------------------
   token->type = tk;
   if ( text != NULL ) {
      token->text = save_text( parse );
      token->length = text->length;
   }
   else {
      const struct token_info* info = get_token_info( tk );
      token->text = info->shared_text;
      token->length = info->length;
   }
   token->pos.line = line;
   token->pos.column = column;
   token->pos.id = parse->source->file_entry_id;
   token->next = NULL;
   token->is_id = is_id;
}

char read_ch( struct parse* parse ) {
   struct source* source = parse->source;
   // Adjust the file position. The file position is adjusted based on the
   // previous character. (If we adjust the file position based on the new
   // character, the file position will refer to the next character.)
   if ( source->ch == '\n' ) {
      ++source->line;
      source->column = 0;
   }
   else if ( source->ch == '\t' ) {
      source->column += parse->task->options->tab_size -
         ( ( source->column + parse->task->options->tab_size ) %
         parse->task->options->tab_size );
   }
   else {
      ++source->column;
   }
   // Read character.
   enum { LOOKAHEAD_AMOUNT = 3 };
   enum { SAFE_AMOUNT = SOURCE_BUFFER_SIZE - LOOKAHEAD_AMOUNT };
   if ( source->buffer_pos >= SAFE_AMOUNT ) {
      int unread = SOURCE_BUFFER_SIZE - source->buffer_pos;
      memcpy( source->buffer, source->buffer + source->buffer_pos, unread );
      size_t count = fread( source->buffer, sizeof( source->buffer[ 0 ] ),
         SOURCE_BUFFER_SIZE - unread, source->fh );
      source->buffer[ count ] = '\0';
      source->buffer_pos = 0;
   }
   // Line concatenation.
   while ( source->buffer[ source->buffer_pos ] == '\\' ) {
      // Linux newline character.
      if ( source->buffer[ source->buffer_pos + 1 ] == '\n' ) {
         source->buffer_pos += 2;
         ++source->line;
         source->column = 0;
      }
      // Windows newline character.
      else if ( source->buffer[ source->buffer_pos + 1 ] == '\r' &&
         source->buffer[ source->buffer_pos + 2 ] == '\n' ) {
         source->buffer_pos += 3;
         ++source->line;
         source->column = 0;
      }
      else {
         break;
      }
   }
   // Process character.
   char ch = source->buffer[ source->buffer_pos ];
   if ( ch == '\r' && source->buffer[ source->buffer_pos + 1 ] == '\n' ) {
      // Replace the two-character Windows newline with a single-character
      // newline to simplify things.
      ch = '\n';
      source->buffer_pos += 2;
   }
   else {
      ++source->buffer_pos;
   }
   source->ch = ch;
   return ch;
}

void read_initial_ch( struct parse* parse ) {
   read_ch( parse );
   // The file position is adjusted based on the previous character. Initially,
   // there is no previous character, but the file position is still adjusted
   // when we read a character from read_ch(). We want the initial character to
   // retain the initial file position, so reset the file position.
   reset_filepos( parse->source );
}

void escape_ch( struct parse* parse, char* ch_out, struct str* text,
   bool in_string ) {
   char ch = *ch_out;
   if ( ! ch ) {
      empty: ;
      struct pos pos = { parse->source->line, parse->source->column,
         parse->source->file->id };
      p_diag( parse, DIAG_POS_ERR, &pos, "empty escape sequence" );
      p_bail( parse );
   }
   int slash = parse->source->column - 1;
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
         append_ch( text, singles[ i + 1 ] );
         ch = read_ch( parse );
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
         struct pos pos = { parse->source->line, parse->source->column,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos, "too many digits" );
         p_bail( parse );
      }
      buffer[ i ] = ch;
      ch = read_ch( parse );
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
         append_ch( text, '\\' );
         append_ch( text, '\\' );
      }
      else {
         append_ch( text, '\\' );
      }
      ch = read_ch( parse );
   }
   // Hexadecimal notation.
   else if ( ch == 'x' || ch == 'X' ) {
      ch = read_ch( parse );
      i = 0;
      while (
         ( ch >= '0' && ch <= '9' ) ||
         ( ch >= 'a' && ch <= 'f' ) ||
         ( ch >= 'A' && ch <= 'F' ) ) {
         if ( i == 2 ) {
            goto too_many_digits;
         }
         buffer[ i ] = ch;
         ch = read_ch( parse );
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
            p_bail( parse );
         }
         append_ch( text, '\\' );
         append_ch( text, ch );
         ch = read_ch( parse );
      }
      else {
         struct pos pos = { parse->source->line, slash,
            parse->source->file->id };
         p_diag( parse, DIAG_POS_ERR, &pos, "unknown escape sequence" );
         p_bail( parse );
      }
   }
   goto finish;

   save_ch:
   // -----------------------------------------------------------------------
   // Code needs to be a valid character.
   if ( code > 127 ) {
      struct pos pos = { parse->source->line, slash,
         parse->source->file->id };
      p_diag( parse, DIAG_POS_ERR, &pos, "invalid character `\\%s`", buffer );
      p_bail( parse );
   }
   // In a string context, the NUL character must not be escaped. Leave it
   // for the engine to process it.
   if ( code == 0 && in_string ) {
      append_ch( text, '\\' );
      append_ch( text, '0' );
   }
   else {
      append_ch( text, ( char ) code );
   }

   finish:
   // -----------------------------------------------------------------------
   *ch_out = ch;
}

struct str* temp_text( struct parse* parse ) {
   str_clear( &parse->temp_text );
   return &parse->temp_text;
}

char* save_text( struct parse* parse ) {
   // Find buffer with enough free space to fit the text.
   struct text_buffer* buffer = NULL;
   list_iter_t i;
   list_iter_init( &i, &parse->text_buffers );
   while ( ! list_end( &i ) ) {
      struct text_buffer* candidate_buffer = list_data( &i );
      if ( candidate_buffer->size - candidate_buffer->used >=
         parse->temp_text.length + 1 ) {
         buffer = candidate_buffer;
         break;
      }
      list_next( &i );
   }
   // Create a new buffer when no suitable buffer could be found.
   if ( ! buffer ) {
      buffer = mem_alloc( sizeof( *buffer ) );
      buffer->used = 0u;
      buffer->size = calc_buffer_size( parse );
      buffer->data = mem_alloc( sizeof( char ) * buffer->size );
      list_append( &parse->text_buffers, buffer );
   }
   // Save text into buffer.
   memcpy( buffer->data + buffer->used, parse->temp_text.value,
      parse->temp_text.length + 1 );
   char* text = buffer->data + buffer->used;
   buffer->used += parse->temp_text.length + 1;
   return text;
}

// The size calculated should be at least twice as big as the text to be
// stored. This is an attempt at reducing buffer allocations.
// NOTE: Integer overflow is possible in this function, although for an
// overflow to happen would require a really long piece of text.
unsigned int calc_buffer_size( struct parse* parse ) {
   enum { INITIAL_SIZE = 4096 };
   unsigned int size = INITIAL_SIZE;
   unsigned int required_size = ( parse->temp_text.length + 1 ) * 2;
   while ( size < required_size ) {
      size <<= 1;
   }
   return size;
}

void append_ch( struct str* str, char ch ) {
   char segment[ 2 ] = { ch, '\0' };
   str_append( str, segment );
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
      { TK_ELLIPSIS, "`...`" },
      { TK_STRCPY, "`strcpy`" } };
   STATIC_ASSERT( TK_TOTAL == 109 );
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

void p_increment_pos( struct pos* pos, enum tk tk ) {
   switch ( tk ) {
   case TK_BRACE_R:
      ++pos->column;
      break;
   default:
      break;
   }
}

void p_skip_block( struct parse* parse ) {
   while ( parse->tk != TK_END && parse->tk != TK_BRACE_L ) {
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_BRACE_L );
   p_read_tk( parse );
   int depth = 0;
   while ( true ) {
      if ( parse->tk == TK_BRACE_L ) {
         ++depth;
         p_read_tk( parse );
      }
      else if ( parse->tk == TK_BRACE_R ) {
         if ( depth ) {
            --depth;
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
      else if ( parse->tk == TK_LIB_END ) {
         break;
      }
      else if ( parse->tk == TK_END ) {
         break;
      }
      else {
         p_read_tk( parse );
      }
   }
   p_test_tk( parse, TK_BRACE_R );
   p_read_tk( parse );
}

const struct token_info* get_token_info( enum tk tk ) {
   // The table below contains information about the available tokens. The
   // order of the table corresponds to the order of the token enumeration.
   #define ENTRY( text, flags ) \
      { text, ARRAY_SIZE( text ) - 1, flags }
   #define BLANK ""
   static struct token_info table[] = {
      // 0
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( "[", TKF_NONE ),
      ENTRY( "]", TKF_NONE ),
      ENTRY( "(", TKF_NONE ),
      ENTRY( ")", TKF_NONE ),
      ENTRY( "{", TKF_NONE ),
      ENTRY( "}", TKF_NONE ),
      ENTRY( ".", TKF_NONE ),
      ENTRY( "++", TKF_NONE ),

      // 10
      ENTRY( "--", TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( ",", TKF_NONE ),
      ENTRY( ":", TKF_NONE ),
      ENTRY( "strcpy", TKF_KEYWORD ),
      ENTRY( ";", TKF_NONE ),
      ENTRY( "=", TKF_NONE ),
      ENTRY( "+=", TKF_NONE ),
      ENTRY( "-=", TKF_NONE ),
      ENTRY( "*=", TKF_NONE ),

      // 20
      ENTRY( "/=", TKF_NONE ),
      ENTRY( "%=", TKF_NONE ),
      ENTRY( "<<=", TKF_NONE ),
      ENTRY( ">>=", TKF_NONE ),
      ENTRY( "&=", TKF_NONE ),
      ENTRY( "^=", TKF_NONE ),
      ENTRY( "|=", TKF_NONE ),
      ENTRY( "==", TKF_NONE ),
      ENTRY( "!=", TKF_NONE ),
      ENTRY( "!", TKF_NONE ),

      // 30
      ENTRY( "&&", TKF_NONE ),
      ENTRY( "||", TKF_NONE ),
      ENTRY( "&", TKF_NONE ),
      ENTRY( "|", TKF_NONE ),
      ENTRY( "^", TKF_NONE ),
      ENTRY( "~", TKF_NONE ),
      ENTRY( "<", TKF_NONE ),
      ENTRY( "<=", TKF_NONE ),
      ENTRY( ">", TKF_NONE ),
      ENTRY( ">=", TKF_NONE ),

      // 40
      ENTRY( "+", TKF_NONE ),
      ENTRY( "-", TKF_NONE ),
      ENTRY( "/", TKF_NONE ),
      ENTRY( "*", TKF_NONE ),
      ENTRY( "%", TKF_NONE ),
      ENTRY( "<<", TKF_NONE ),
      ENTRY( ">>", TKF_NONE ),
      ENTRY( ":=", TKF_NONE ),
      ENTRY( "break", TKF_KEYWORD ),
      ENTRY( "case", TKF_KEYWORD ),

      // 50
      ENTRY( "const", TKF_KEYWORD ),
      ENTRY( "continue", TKF_KEYWORD ),
      ENTRY( "default", TKF_KEYWORD ),
      ENTRY( "do", TKF_KEYWORD ),
      ENTRY( "else", TKF_KEYWORD ),
      ENTRY( "enum", TKF_KEYWORD ),
      ENTRY( "for", TKF_KEYWORD ),
      ENTRY( "if", TKF_KEYWORD ),
      ENTRY( "int", TKF_KEYWORD ),
      ENTRY( "return", TKF_KEYWORD ),

      // 60
      ENTRY( "static", TKF_KEYWORD ),
      ENTRY( "str", TKF_KEYWORD ),
      ENTRY( "struct", TKF_KEYWORD ),
      ENTRY( "switch", TKF_KEYWORD ),
      ENTRY( "void", TKF_KEYWORD ),
      ENTRY( "while", TKF_KEYWORD ),
      ENTRY( "bool", TKF_KEYWORD ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),

      // 70
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( "#", TKF_NONE ),
      ENTRY( "createtranslation", TKF_KEYWORD ),
      ENTRY( "global", TKF_KEYWORD ),
      ENTRY( "script", TKF_KEYWORD ),
      ENTRY( "until", TKF_KEYWORD ),
      ENTRY( "world", TKF_KEYWORD ),
      ENTRY( "open", TKF_KEYWORD ),

      // 80
      ENTRY( "respawn", TKF_KEYWORD ),
      ENTRY( "death", TKF_KEYWORD ),
      ENTRY( "enter", TKF_KEYWORD ),
      ENTRY( "pickup", TKF_KEYWORD ),
      ENTRY( "bluereturn", TKF_KEYWORD ),
      ENTRY( "redreturn", TKF_KEYWORD ),
      ENTRY( "whitereturn", TKF_KEYWORD ),
      ENTRY( "lightning", TKF_KEYWORD ),
      ENTRY( "disconnect", TKF_KEYWORD ),
      ENTRY( "unloading", TKF_KEYWORD ),

      // 90
      ENTRY( "clientside", TKF_KEYWORD ),
      ENTRY( "net", TKF_KEYWORD ),
      ENTRY( "restart", TKF_KEYWORD ),
      ENTRY( "suspend", TKF_KEYWORD ),
      ENTRY( "terminate", TKF_KEYWORD ),
      ENTRY( "function", TKF_KEYWORD ),
      ENTRY( "import", TKF_KEYWORD ),
      ENTRY( "goto", TKF_KEYWORD ),
      ENTRY( "true", TKF_KEYWORD ),
      ENTRY( "false", TKF_KEYWORD ),

      // 100
      ENTRY( "event", TKF_KEYWORD ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( "?", TKF_KEYWORD ),
      ENTRY( " ", TKF_KEYWORD ),
      ENTRY( "\t", TKF_KEYWORD ),

      // Invalid entry.
      // This entry should not be reached when all tokens are acccounted for.
      ENTRY( BLANK, TKF_NONE )
   };
   #undef ENTRY
   if ( tk < ARRAY_SIZE( table ) - 1 ) {
      return &table[ tk ];
   }
   else {
      UNREACHABLE();
      return &table[ ARRAY_SIZE( table ) - 1 ];
   }
}

void p_deinit_tk( struct parse* parse ) {
   unload_source_all( parse );
}