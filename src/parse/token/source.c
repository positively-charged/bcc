#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "phase.h"

struct text_buffer {
   char* data;
   unsigned int used;
   unsigned int size;
};

static void append_file( struct library* lib, struct file_entry* file );
static bool source_loading( struct parse* parse, struct request* request );
static void open_source_file( struct parse* parse, struct request* request );
static struct source* alloc_source( struct parse* parse );
static void reset_filepos( struct source* source );
static void read_token( struct parse* parse, struct token* token );
static void escape_ch( struct parse* parse, char*, struct str* text, bool );
static char read_ch( struct parse* parse );
static char peek_ch( struct parse* parse );
static void read_initial_ch( struct parse* parse );
static struct str* temp_text( struct parse* parse );
static char* save_text( struct parse* parse );
static unsigned int calc_buffer_size( struct parse* parse );
static void append_ch( struct str* str, char ch );

void p_load_main_source( struct parse* parse ) {
   struct request request;
   p_init_request( &request, NULL, parse->task->options->source_file );
   p_load_source( parse, &request );
   if ( request.source ) {
      parse->lib->file = request.file;
      parse->lib->file_pos.id = request.file->id;
      append_file( parse->lib, request.file );
   }
   else {
      p_diag( parse, DIAG_ERR,
         "failed to load source file: %s (%s)",
         parse->task->options->source_file, strerror( errno ) );
      p_bail( parse );
   }
}

void p_load_imported_lib_source( struct parse* parse, struct import_dirc* dirc,
   struct file_entry* file ) {
   struct request request;
   p_init_request( &request, NULL, file->full_path.value );
   request.file = file;
   open_source_file( parse, &request );
   if ( request.source ) {
      parse->lib->file = file;
      parse->lib->file_pos.id = file->id;
      append_file( parse->lib, file );
      request.source->imported = true;
   }
   else {
      p_diag( parse, DIAG_POS_ERR, &dirc->pos,
         "failed to load library file: %s", dirc->file_path );
      p_bail( parse );
   }
}

void p_load_included_source( struct parse* parse, const char* file_path,
   struct pos* pos ) {
   struct request request;
   p_init_request( &request, parse->source->file, file_path );
   p_load_source( parse, &request );
   if ( request.source ) {
      append_file( parse->lib, request.file );
   }
   else {
      if ( request.err_loading ) {
         p_diag( parse, DIAG_POS_ERR, pos,
            "file already being loaded" );
         p_bail( parse );
      }
      else {
         p_diag( parse, DIAG_POS_ERR, pos,
            "failed to load file: %s", file_path );
         p_bail( parse );
      }
   }
}

void append_file( struct library* lib, struct file_entry* file ) {
   list_iter_t i;
   list_iter_init( &i, &lib->files );
   while ( ! list_end( &i ) ) {
      if ( list_data( &i ) == file ) {
         return;
      }
      list_next( &i );
   }
   list_append( &lib->files, file );
}

void p_init_request( struct request* request, struct file_entry* offset_file,
   const char* path ) {
   request->given_path = path;
   request->file = NULL;
   request->offset_file = offset_file;
   request->source = NULL;
   request->err_open = false;
   request->err_loaded_before = false;
   request->err_loading = false;
}

void p_load_source( struct parse* parse, struct request* request ) {
   struct file_query query;
   t_init_file_query( &query, request->offset_file, request->given_path );
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
   if ( parse->source ) {
      read_token( parse, token );
      if ( token->type == TK_END ) {
         bool reread = ( ! parse->source->imported );
         unload_source( parse );
         if ( parse->source && reread ) {
            p_read_source( parse, token );
         }
      }
   }
}

void read_token( struct parse* parse, struct token* token ) {
   char ch = parse->source->ch;
   int line = 0;
   int column = 0;
   int length = 0;
   enum tk tk = TK_END;
   struct str* text = NULL;
   bool is_id = false;

   state_space:
   // -----------------------------------------------------------------------
   switch ( ch ) {
   case ' ':
   case '\t':
      goto space;
   case '\n':
      goto newline;
   default:
      goto state_token;
   }

   space:
   // -----------------------------------------------------------------------
   line = parse->source->line;
   column = parse->source->column;
   while ( ch == ' ' || ch == '\t' ) {
      ch = read_ch( parse );
   }
   length = parse->source->column - column;
   tk = TK_HORZSPACE;
   goto state_finish;

   newline:
   // -----------------------------------------------------------------------
   line = parse->source->line;
   column = parse->source->column;
   tk = TK_NL;
   ch = read_ch( parse );
   goto state_finish;

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
   else if ( ch == '\\' ) {
      struct pos pos = { parse->source->line, column,
         parse->source->file->id };
      p_diag( parse, DIAG_POS_ERR, &pos,
         "`\\` not followed with newline character" );
      p_bail( parse );
   }
   else if ( ch == '#' ) {
      tk = TK_HASH;
      ch = read_ch( parse );
      if ( ch == '#' ) {
         tk = TK_PREP_HASHHASH;
         ch = read_ch( parse );
      }
      goto state_finish;
   }
   else if ( ch == '.' ) {
      ch = read_ch( parse );
      if ( ch == '.' && peek_ch( parse ) == '.' ) {
         read_ch( parse );
         ch = read_ch( parse );
         tk = TK_ELLIPSIS;
      }
      else {
         tk = TK_DOT;
      }
      goto state_finish;
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
         '?', TK_QUESTION_MARK,
         ':', TK_COLON,
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
         { "assert", TK_ASSERT },
         { "auto", TK_AUTO },
         { "bluereturn", TK_BLUE_RETURN },
         { "bool", TK_BOOL },
         { "break", TK_BREAK },
         { "case", TK_CASE },
         { "cast", TK_CAST },
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
         { "extspec", TK_EXTSPEC },
         { "false", TK_FALSE },
         { "fixed", TK_FIXED },
         { "for", TK_FOR },
         { "foreach", TK_FOREACH },
         { "function", TK_FUNCTION },
         { "global", TK_GLOBAL },
         { "goto", TK_GOTO },
         { "if", TK_IF },
         { "import", TK_IMPORT },
         { "in", TK_IN },
         { "int", TK_INT },
         { "lightning", TK_LIGHTNING },
         { "msgbuild", TK_MSGBUILD },
         { "namespace", TK_NAMESPACE },
         { "net", TK_NET },
         { "null", TK_NULL },
         { "objcpy", TK_OBJCPY },
         { "open", TK_OPEN },
         { "pickup", TK_PICKUP },
         { "private", TK_PRIVATE },
         { "raw", TK_RAW },
         { "redreturn", TK_RED_RETURN },
         { "ref", TK_REF },
         { "respawn", TK_RESPAWN },
         { "restart", TK_RESTART },
         { "return", TK_RETURN },
         { "script", TK_SCRIPT },
         { "special", TK_SPECIAL },
         { "static", TK_STATIC },
         { "str", TK_STR },
         { "strcpy", TK_STRCPY },
         { "struct", TK_STRUCT },
         { "suspend", TK_SUSPEND },
         { "switch", TK_SWITCH },
         { "terminate", TK_TERMINATE },
         { "true", TK_TRUE },
         { "typedef", TK_TYPEDEF },
         { "unloading", TK_UNLOADING },
         { "until", TK_UNTIL },
         { "upmost", TK_UPMOST },
         { "using", TK_USING },
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
/*
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
*/
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
      const struct token_info* info = p_get_token_info( tk );
      token->text = info->shared_text;
      token->length = ( length > 0 ) ?
         length : info->length;
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
      ++parse->line;
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
      size_t count = fread( source->buffer + unread,
         sizeof( source->buffer[ 0 ] ), SOURCE_BUFFER_SIZE - unread,
         source->fh );
      source->buffer[ unread + count ] = '\n';
      source->buffer[ unread + count + 1 ] = '\0';
      source->buffer_pos = 0;
   }
   // Line concatenation.
   while ( source->buffer[ source->buffer_pos ] == '\\' ) {
      // Linux newline character.
      if ( source->buffer[ source->buffer_pos + 1 ] == '\n' ) {
         source->buffer_pos += 2;
         ++source->line;
         source->column = 0;
         ++parse->line;
      }
      // Windows newline character.
      else if ( source->buffer[ source->buffer_pos + 1 ] == '\r' &&
         source->buffer[ source->buffer_pos + 2 ] == '\n' ) {
         source->buffer_pos += 3;
         ++source->line;
         source->column = 0;
         ++parse->line;
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

char peek_ch( struct parse* parse ) {
   return parse->source->buffer[ parse->source->buffer_pos ];
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

void p_increment_pos( struct pos* pos, enum tk tk ) {
   switch ( tk ) {
   case TK_BRACE_R:
      ++pos->column;
      break;
   default:
      break;
   }
}

void p_deinit_tk( struct parse* parse ) {
   unload_source_all( parse );
}

#include <time.h>

void p_read_sourcepos_token( struct parse* parse, struct pos* pos ) {
   const char* file;
   int line;
   int column;
   t_decode_pos( parse->task, pos, &file, &line, &column );
   if ( parse->predef_macro_expan == PREDEFMACROEXPAN_LINE ) {
      struct str* text = temp_text( parse );
      str_grow( text, 11 );
      int length = snprintf( text->value, 11, "%d", line );
      static struct token token;
      token.type = TK_LIT_DECIMAL;
      token.text = text->value;
      token.length = length;
      token.pos = parse->token->pos;
      token.is_id = false;
      parse->token = &token;
   }
   else if ( parse->predef_macro_expan == PREDEFMACROEXPAN_FILE ) {
      static struct token token;
      token.type = TK_LIT_STRING;
      token.text = ( char* ) file;
      token.length = strlen( token.text );
      token.pos = parse->token->pos;
      token.is_id = false;
      parse->token = &token;
   }
   else if ( parse->predef_macro_expan == PREDEFMACROEXPAN_TIME ) {
      static char buffer[ 9 ];
      time_t timestamp;
      time( &timestamp );
      struct tm* info = localtime( &timestamp );
      int length = snprintf( buffer, 9, "%02d:%02d:%02d", info->tm_hour,
         info->tm_min, info->tm_sec );
      static struct token token;
      token.type = TK_LIT_STRING;
      token.text = buffer;
      token.length = strlen( buffer );
      token.pos = parse->token->pos;
      token.is_id = false;
      parse->token = &token;
   }
   else if ( parse->predef_macro_expan == PREDEFMACROEXPAN_DATE ) {
      time_t timestamp;
      time( &timestamp );
      struct tm* info = localtime( &timestamp );
      static char buffer[ 12 ];
      strftime( buffer, 12, "%b %e %Y", info );
      static struct token token;
      token.type = TK_LIT_STRING;
      token.text = buffer;
      token.length = strlen( buffer );
      token.pos = parse->token->pos;
      token.is_id = false;
      parse->token = &token;
   }
   else {
      UNREACHABLE();
   }
}