#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "phase.h"

enum { LINE_OFFSET = 1 };

struct text_buffer {
   struct text_buffer* prev;
   char* start;
   char* end;
   char* left;
};

static void append_file( struct library* lib, struct file_entry* file );
static bool source_loading( struct parse* parse, struct request* request );
static void open_source_file( struct parse* parse, struct request* request );
static struct source* alloc_source( struct parse* parse );
static void reset_filepos( struct source* source );
static void read_token_acs( struct parse* parse, struct token* token );
static struct text_buffer* get_text_buffer( struct parse* parse,
   int min_free_size );
static bool pop_source( struct parse* parse );
static void read_token( struct parse* parse, struct token* token );
static void escape_ch( struct parse* parse, char*, struct str* text, bool );
static char read_ch( struct parse* parse );
static char peek_ch( struct parse* parse );
static void read_initial_ch( struct parse* parse );
static struct str* temp_text( struct parse* parse );
static char* copy_text( struct parse* parse, struct str* text );
static void append_ch( struct str* str, char ch );

struct keyword_entry {
   const char* name;
   enum tk tk;
};

struct keyword_table {
   const struct keyword_entry* entries;
   int max;
   int mid;
};

// NOTE: Reserved identifiers must be listed in ascending order.

static const struct keyword_entry g_keywords_acs95[] = {
   { "break", TK_BREAK },
   { "case", TK_CASE },
   { "const", TK_CONST },
   { "continue", TK_CONTINUE },
   { "default", TK_DEFAULT },
   { "define", TK_DEFINE },
   { "do", TK_DO },
   { "else", TK_ELSE },
   { "for", TK_FOR },
   { "goto", TK_GOTO },
   { "if", TK_IF },
   { "include", TK_INCLUDE },
   { "int", TK_INT },
   { "open", TK_OPEN },
   { "print", TK_PRINT },
   { "printbold", TK_PRINTBOLD },
   { "restart", TK_RESTART },
   { "script", TK_SCRIPT },
   { "special", TK_SPECIAL },
   { "str", TK_STR },
   { "suspend", TK_SUSPEND },
   { "switch", TK_SWITCH },
   { "terminate", TK_TERMINATE },
   { "until", TK_UNTIL },
   { "void", TK_VOID },
   { "while", TK_WHILE },
   { "world", TK_WORLD },
   // Terminator.
   { "\x7F", TK_END }
};

static const struct keyword_entry g_keywords_bcs[] = {
   { "assert", TK_ASSERT },
   { "auto", TK_AUTO },
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
   { "extern", TK_EXTERN },
   { "false", TK_FALSE },
   { "fixed", TK_FIXED },
   { "for", TK_FOR },
   { "foreach", TK_FOREACH },
   { "function", TK_FUNCTION },
   { "global", TK_GLOBAL },
   { "goto", TK_GOTO },
   { "if", TK_IF },
   { "int", TK_INT },
   { "libdefine", TK_LIBDEFINE },
   { "lightning", TK_LIGHTNING },
   { "memcpy", TK_MEMCPY },
   { "msgbuild", TK_MSGBUILD },
   { "namespace", TK_NAMESPACE },
   { "net", TK_NET },
   { "null", TK_NULL },
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
   { "typesyn", TK_TYPESYN },
   { "unloading", TK_UNLOADING },
   { "until", TK_UNTIL },
   { "upmost", TK_UPMOST },
   { "using", TK_USING },
   { "void", TK_VOID },
   { "while", TK_WHILE },
   { "whitereturn", TK_WHITE_RETURN },
   { "world", TK_WORLD },
   // Terminator.
   { "\x7F", TK_END }
};

#define KEYWORDTABLE_ENTRY( keywords ) \
   keywords, \
   ARRAY_SIZE( keywords ), \
   ARRAY_SIZE( keywords ) / 2

static struct {
   const struct keyword_table acs95;
   const struct keyword_table bcs;
} g_keyword_tables = {
   { KEYWORDTABLE_ENTRY( g_keywords_acs95 ) },
   { KEYWORDTABLE_ENTRY( g_keywords_bcs ) }
};

void p_load_main_source( struct parse* parse ) {
   struct request request;
   p_init_request( &request, NULL, parse->task->options->source_file, false );
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
   p_init_request( &request, NULL, file->full_path.value, false );
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
   bool bcs_ext = false;
   if ( parse->lang == LANG_BCS ) {
      const char* ext = "";
      for ( int i = 0; file_path[ i ]; ++i ) {
         if ( file_path[ i ] == '.' ) {
            ext = file_path + i + 1;
         }
      }
      bcs_ext = ( strcasecmp( ext, "h" ) == 0 );
   }
   struct request request;
   p_init_request( &request, parse->source->file, file_path, bcs_ext );
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
   const char* path, bool bcs_ext ) {
   request->given_path = path;
   request->file = NULL;
   request->offset_file = offset_file;
   request->source = NULL;
   request->err_open = false;
   request->err_loaded_before = false;
   request->err_loading = false;
   request->bcs_ext = bcs_ext;
}

// TODO: Refactor.
void p_load_source( struct parse* parse, struct request* request ) {
   if ( request->bcs_ext ) {
      struct str path;
      str_init( &path );
      str_append( &path, request->given_path );
      str_append( &path, ".bcs" );
      struct file_query query;
      t_init_file_query( &query, request->offset_file, path.value );
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
      str_deinit( &path );
      if ( request->source || request->err_loading ) {
         return;
      }
   }
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
   source->line = LINE_OFFSET;
   source->column = 0;
}

void p_read_source( struct parse* parse, struct token* token ) {
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      read_token_acs( parse, token );
      break;
   default:
      read_token( parse, token );
   }
}

void read_token_acs( struct parse* parse, struct token* token ) {
   char ch = parse->source->ch;
   int id = 0;
   int line = 0;
   int column = 0;
   enum tk tk = TK_END;
   char* text = NULL;
   int length = 0;

   whitespace:
   // -----------------------------------------------------------------------
   while ( isspace( ch ) ) {
      ch = read_ch( parse );
   }

   // The chain of if-statements is ordered based on a likelihood of a token
   // being used. Identifier tokens are one of the most common, so look for
   // them first.
   // -----------------------------------------------------------------------
   id = parse->source->file_entry_id;
   line = parse->source->line;
   column = parse->source->column;
   if ( isalpha( ch ) || ch == '_' ) {
      goto identifier;
   }
   else if ( ch == '(' ) {
      tk = TK_PAREN_L;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == ')' ) {
      tk = TK_PAREN_R;
      read_ch( parse );
      goto finish;
   }
   else if ( isdigit( ch ) ) {
      if ( ch == '0' ) {
         ch = read_ch( parse );
         if ( ch == 'x' || ch == 'X' ) {
            ch = read_ch( parse );
            goto hexadecimal;
         }
         else if ( ch == '.' ) {
            str_clear( &parse->temp_text );
            append_ch( &parse->temp_text, '0' );
            append_ch( &parse->temp_text, '.' );
            ch = read_ch( parse );
            goto fixedpoint;
         }
         else {
            goto zero;
         }
      }
      else {
         goto decimal;
      }
   }
   else if ( ch == ',' ) {
      tk = TK_COMMA;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == ';' ) {
      tk = TK_SEMICOLON;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == '"' ) {
      ch = read_ch( parse );
      goto string;
   }
   else if ( ch == ':' ) {
      tk = TK_COLON;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == '#' ) {
      tk = TK_HASH;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == '{' ) {
      tk = TK_BRACE_L;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == '}' ) {
      tk = TK_BRACE_R;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == '=' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_EQ;
         read_ch( parse );
         goto finish;
      }
      else {
         tk = TK_ASSIGN;
         goto finish;
      }
   }
   else if ( ch == '[' ) {
      tk = TK_BRACKET_L;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == ']' ) {
      tk = TK_BRACKET_R;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == '+' ) {
      ch = read_ch( parse );
      if ( ch == '+' ) {
         tk = TK_INC;
         read_ch( parse );
         goto finish;
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_ADD;
         read_ch( parse );
         goto finish;
      }
      else {
         tk = TK_PLUS;
         goto finish;
      }
   }
   else if ( ch == '-' ) {
      ch = read_ch( parse );
      if ( ch == '-' ) {
         tk = TK_DEC;
         read_ch( parse );
         goto finish;
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_SUB;
         read_ch( parse );
         goto finish;
      }
      else {
         tk = TK_MINUS;
         goto finish;
      }
   }
   else if ( ch == '!' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_NEQ;
         read_ch( parse );
         goto finish;
      }
      else {
         tk = TK_LOG_NOT;
         goto finish;
      }
   }
   else if ( ch == '&' ) {
      ch = read_ch( parse );
      if ( ch == '&' ) {
         tk = TK_LOG_AND;
         read_ch( parse );
         goto finish;
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_AND;
         read_ch( parse );
         goto finish;
      }
      else {
         tk = TK_BIT_AND;
         goto finish;
      }
   }
   else if ( ch == '<' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_LTE;
         read_ch( parse );
         goto finish;
      }
      else if ( ch == '<' ) {
         ch = read_ch( parse );
         if ( ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_L;
            read_ch( parse );
            goto finish;
         }
         else {
            tk = TK_SHIFT_L;
            goto finish;
         }
      }
      else {
         tk = TK_LT;
         goto finish;
      }
   }
   else if ( ch == '>' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_GTE;
         read_ch( parse );
         goto finish;
      }
      else if ( ch == '>' ) {
         ch = read_ch( parse );
         if ( ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_R;
            read_ch( parse );
            goto finish;
         }
         else {
            tk = TK_SHIFT_R;
            goto finish;
         }
      }
      else {
         tk = TK_GT;
         goto finish;
      }
   }
   else if ( ch == '|' ) {
      ch = read_ch( parse );
      if ( ch == '|' ) {
         tk = TK_LOG_OR;
         read_ch( parse );
         goto finish;
      }
      else if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_OR;
         read_ch( parse );
         goto finish;
      }
      else {
         tk = TK_BIT_OR;
         goto finish;
      }
   }
   else if ( ch == '*' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_MUL;
         read_ch( parse );
         goto finish;
      }
      else {
         tk = TK_STAR;
         goto finish;
      }
   }
   else if ( ch == '/' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_DIV;
         read_ch( parse );
         goto finish;
      }
      else if ( ch == '/' ) {
         goto comment;
      }
      else if ( ch == '*' ) {
         ch = read_ch( parse );
         goto multiline_comment;
      }
      else {
         tk = TK_SLASH;
         goto finish;
      }
   }
   else if ( ch == '%' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_MOD;
         read_ch( parse );
         goto finish;
      }
      else {
         tk = TK_MOD;
         goto finish;
      }
   }
   else if ( ch == '^' ) {
      ch = read_ch( parse );
      if ( ch == '=' ) {
         tk = TK_ASSIGN_BIT_XOR;
         read_ch( parse );
         goto finish;
      }
      else {
         tk = TK_BIT_XOR;
         goto finish;
      }
   }
   else if ( ch == '\'' ) {
      ch = read_ch( parse );
      goto character;
   }
   else if ( ch == '~' ) {
      tk = TK_BIT_NOT;
      read_ch( parse );
      goto finish;
   }
   else if ( ch == '\0' ) {
      if ( pop_source( parse ) ) {
         ch = parse->source->ch;
         goto whitespace;
      }
      else {
         tk = TK_END;
         goto finish;
      }
   }
   // Generated by acc, but not actually used.
   else if ( ch == '.' ) {
      tk = TK_DOT;
      read_ch( parse );
      goto finish;
   }
   else {
      struct pos pos;
      t_init_pos( &pos, parse->source->file->id, parse->source->line,
         column );
      p_diag( parse, DIAG_POS_ERR, &pos,
         "invalid character" );
      p_bail( parse );
   }

   identifier:
   // -----------------------------------------------------------------------
   {
      // NOTE: Reserved identifiers must be listed in ascending order.
      static const struct entry {
         const char* name;
         enum tk tk;
      } table[] = {
         { "acs_executewait", TK_ACSEXECUTEWAIT },
         { "acs_namedexecutewait", TK_ACSNAMEDEXECUTEWAIT },
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
         { "define", TK_DEFINE },
         { "disconnect", TK_DISCONNECT },
         { "do", TK_DO },
         { "else", TK_ELSE },
         { "encryptstrings", TK_ENCRYPTSTRINGS },
         { "endregion", TK_ENDREGION },
         { "enter", TK_ENTER },
         { "event", TK_EVENT },
         { "for", TK_FOR },
         { "function", TK_FUNCTION },
         { "global", TK_GLOBAL },
         { "goto", TK_GOTO },
         { "hudmessage", TK_HUDMESSAGE },
         { "hudmessagebold", TK_HUDMESSAGEBOLD },
         { "if", TK_IF },
         { "import", TK_IMPORT },
         { "include", TK_INCLUDE },
         { "int", TK_INT },
         { "libdefine", TK_LIBDEFINE },
         { "library", TK_LIBRARY },
         { "lightning", TK_LIGHTNING },
         { "log", TK_LOG },
         { "net", TK_NET },
         { "nocompact", TK_NOCOMPACT },
         { "nowadauthor", TK_NOWADAUTHOR },
         { "open", TK_OPEN },
         { "pickup", TK_PICKUP },
         { "redreturn", TK_RED_RETURN },
         { "region", TK_REGION },
         { "respawn", TK_RESPAWN },
         { "restart", TK_RESTART },
         { "return", TK_RETURN },
         { "script", TK_SCRIPT },
         { "special", TK_SPECIAL },
         { "static", TK_STATIC },
         { "str", TK_STR },
         { "strcpy", TK_STRCPY },
         { "strparam", TK_STRPARAM },
         { "suspend", TK_SUSPEND },
         { "switch", TK_SWITCH },
         { "terminate", TK_TERMINATE },
         { "unloading", TK_UNLOADING },
         { "until", TK_UNTIL },
         { "void", TK_VOID },
         { "wadauthor", TK_WADAUTHOR },
         { "while", TK_WHILE },
         { "whitereturn", TK_WHITE_RETURN },
         { "world", TK_WORLD }
      };
      enum { MAX_IDENTIFIER_LENGTH = 31 };
      struct text_buffer* text_buffer = get_text_buffer( parse,
         MAX_IDENTIFIER_LENGTH + 1 );
      text = text_buffer->left;
      char* copied_text = text;
      char* end = copied_text + MAX_IDENTIFIER_LENGTH;
      char* source_text = parse->source->buffer +
         parse->source->buffer_pos - 1;
      while ( true ) {
         if ( isalnum( *source_text ) || *source_text == '_' ) {
            if ( copied_text == end ) {
               struct pos pos;
               t_init_pos( &pos, parse->source->file->id, line, column );
               p_diag( parse, DIAG_POS_ERR, &pos,
                  "identifier too long (maximum length is %d)",
                  MAX_IDENTIFIER_LENGTH );
               p_bail( parse );
            }
            *copied_text = tolower( *source_text );
            ++copied_text;
            ++source_text;
         }
         // Read new data from the source file.
         else if ( *source_text == '\n' && source_text[ 1 ] == '\0' ) {
            size_t count = fread( parse->source->buffer,
               sizeof( parse->source->buffer[ 0 ] ), SOURCE_BUFFER_SIZE,
               parse->source->fh );
            parse->source->buffer[ count ] = '\n';
            parse->source->buffer[ count + 1 ] = '\0';
            parse->source->buffer_pos = 0;
            source_text = parse->source->buffer;
            if ( count == 0 ) {
               break;
            }
         }
         else {
            break;
         }
      }
      *copied_text = '\0';
      length = copied_text - text;
      // Update source buffer. The 1 added to `buffer_pos` is for the character
      // after the identifier, since we assume it is now read.
      parse->source->buffer_pos = source_text - parse->source->buffer + 1;
      parse->source->ch = *source_text;
      // Update text buffer.
      text_buffer->left = copied_text + 1;
      // Reserved identifier. Uses binary search.
      int left = 0;
      int right = ARRAY_SIZE( table ) - 1;
      while ( left <= right ) {
         int middle = ( left + right ) / 2;
         int result = strcmp( text, table[ middle ].name );
         if ( result > 0 ) {
            left = middle + 1;
         }
         else if ( result < 0 ) {
            right = middle - 1;
         }
         else {
            tk = table[ middle ].tk;
            goto finish;
         }
      }
      // Identifer.
      tk = TK_ID;
      goto finish;
   }

   hexadecimal:
   // -----------------------------------------------------------------------
   str_clear( &parse->temp_text );
   while ( true ) {
      if ( isxdigit( ch ) ) {
         append_ch( &parse->temp_text, ch );
         ch = read_ch( parse );
      }
      else if ( isalpha( ch ) ) {
         struct pos pos;
         t_init_pos( &pos, parse->source->file->id, parse->source->line,
            parse->source->column );
         p_diag( parse, DIAG_POS_ERR, &pos,
            "invalid digit in hexadecimal literal" );
         p_bail( parse );
      }
      else {
         if ( parse->temp_text.length == 0 ) {
            struct pos pos;
            t_init_pos( &pos, parse->source->file->id, parse->source->line,
               column );
            p_diag( parse, DIAG_POS_ERR, &pos,
               "no digits found in hexadecimal literal" );
            p_bail( parse );
         }
         tk = TK_LIT_HEX;
         text = parse->temp_text.value;
         length = parse->temp_text.length;
         goto finish;
      }
   }

   zero:
   // -----------------------------------------------------------------------
   while ( ch == '0' ) {
      ch = read_ch( parse );
   }
   if ( isdigit( ch ) ) {
      goto decimal;
   }
   else if ( ch == '.' ) {
      str_clear( &parse->temp_text );
      append_ch( &parse->temp_text, '0' );
      append_ch( &parse->temp_text, '.' );
      ch = read_ch( parse );
      goto fixedpoint;
   }
   else if ( ch == '_' ) {
      str_clear( &parse->temp_text );
      append_ch( &parse->temp_text, '0' );
      append_ch( &parse->temp_text, ch );
      ch = read_ch( parse );
      goto radix;
   }
   else {
      text = "0";
      length = 1;
      tk = TK_LIT_DECIMAL;
      goto finish;
   }

   decimal:
   // -----------------------------------------------------------------------
   str_clear( &parse->temp_text );
   while ( true ) {
      if ( isdigit( ch ) ) {
         append_ch( &parse->temp_text, ch );
         ch = read_ch( parse );
      }
      else if ( ch == '.' ) {
         append_ch( &parse->temp_text, ch );
         ch = read_ch( parse );
         goto fixedpoint;
      }
      else if ( ch == '_' ) {
         append_ch( &parse->temp_text, ch );
         ch = read_ch( parse );
         goto radix;
      }
      else if ( isalpha( ch ) ) {
         struct pos pos;
         t_init_pos( &pos, parse->source->file->id, parse->source->line,
            parse->source->column );
         p_diag( parse, DIAG_POS_ERR, &pos,
            "invalid digit in decimal literal" );
         p_bail( parse );
      }
      else {
         tk = TK_LIT_DECIMAL;
         text = parse->temp_text.value;
         length = parse->temp_text.length;
         goto finish;
      }
   }

   fixedpoint:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( isdigit( ch ) ) {
         append_ch( &parse->temp_text, ch );
         ch = read_ch( parse );
      }
      else if ( isalpha( ch ) ) {
         struct pos pos;
         t_init_pos( &pos, parse->source->file->id, parse->source->line,
            parse->source->column );
         p_diag( parse, DIAG_POS_ERR, &pos,
            "invalid digit in fractional part of fixed-point literal" );
         p_bail( parse );
      }
      else {
         tk = TK_LIT_FIXED;
         text = parse->temp_text.value;
         length = parse->temp_text.length;
         goto finish;
      }
   }

   radix:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( isdigit( ch ) || isalpha( ch ) ) {
         append_ch( &parse->temp_text, tolower( ch ) );
         ch = read_ch( parse );
      }
      else {
         text = parse->temp_text.value;
         length = parse->temp_text.length;
         tk = TK_LIT_RADIX;
         goto finish;
      }
   }

   string:
   // -----------------------------------------------------------------------
   {
      // Most strings will be small, so copy the characters directly into the
      // text buffer. For long strings, use an intermediate buffer.
      enum { SEGMENTLENGTH = 255 };
      enum { CUSHIONLENGTH = 1 };
      enum { SAFELENGTH = SEGMENTLENGTH - CUSHIONLENGTH };
      struct text_buffer* text_buffer = get_text_buffer( parse,
         SEGMENTLENGTH + 1 );
      text = text_buffer->left;
      char* copied_text = text;
      char* end = copied_text + SAFELENGTH;
      struct str* temp_text = NULL;
      while ( true ) {
         if ( copied_text >= end ) {
            if ( ! temp_text ) {
               temp_text = &parse->temp_text;
               str_clear( temp_text );
               str_append_sub( temp_text, text, copied_text - text );
            }
            else {
               temp_text->length += copied_text - text;
               str_grow( temp_text, temp_text->buffer_length * 2 );
            }
            copied_text = temp_text->value + temp_text->length;
            end = temp_text->value + temp_text->buffer_length -
               ( CUSHIONLENGTH + 1 );
         }
         else if ( ! ch ) {
            struct pos pos;
            t_init_pos( &pos, parse->source->file->id, line, column );
            p_diag( parse, DIAG_POS_ERR, &pos,
               "unterminated string" );
            p_bail( parse );
         }
         else if ( ch == '"' ) {
            read_ch( parse );
            tk = TK_LIT_STRING;
            *copied_text = '\0';
            if ( temp_text ) {
               temp_text->length = copied_text - temp_text->value;
               text = copy_text( parse, temp_text );
               length = temp_text->length;
            }
            else {
               length = copied_text - text;
               text_buffer->left = copied_text + 1;
            }
            goto finish;
         }
         else if ( ch == '\\' ) {
            *copied_text = ch;
            ++copied_text;
            ch = read_ch( parse );
            if ( ch ) {
               *copied_text = ch;
               ++copied_text;
               ch = read_ch( parse );
            }
         }
         else {
            *copied_text = ch;
            ++copied_text;
            ch = read_ch( parse );
         }
      }
   }

   character:
   // -----------------------------------------------------------------------
   str_clear( &parse->temp_text );
   if ( ch == '\'' || ! ch ) {
      struct pos pos;
      t_init_pos( &pos, parse->source->file->id, parse->source->line,
         parse->source->column );
      p_diag( parse, DIAG_POS_ERR, &pos,
         "missing character in character literal" );
      p_bail( parse );
   }
   if ( ch == '\\' ) {
      ch = read_ch( parse );
      if ( ch == '\'' ) {
         append_ch( &parse->temp_text, ch );
         ch = read_ch( parse );
      }
      else {
         escape_ch( parse, &ch, &parse->temp_text, false );
      }
   }
   else  {
      append_ch( &parse->temp_text, ch );
      ch = read_ch( parse );
   }
   if ( ch != '\'' ) {
      struct pos pos;
      t_init_pos( &pos, parse->source->file->id, parse->source->line, column );
      p_diag( parse, DIAG_POS_ERR, &pos,
         "multiple characters in character literal" );
      p_bail( parse );
   }
   read_ch( parse );
   tk = TK_LIT_CHAR;
   text = parse->temp_text.value;
   length = parse->temp_text.length;
   goto finish;

   comment:
   // -----------------------------------------------------------------------
   while ( ch != '\n' ) {
      ch = read_ch( parse );
   }
   goto whitespace;

   multiline_comment:
   // -----------------------------------------------------------------------
   while ( true ) {
      if ( ! ch ) {
         struct pos pos;
         t_init_pos( &pos, parse->source->file->id, line, column );
         p_diag( parse, DIAG_POS_ERR, &pos,
            "unterminated comment" );
         p_bail( parse );
      }
      else if ( ch == '*' ) {
         ch = read_ch( parse );
         if ( ch == '/' ) {
            ch = read_ch( parse );
            goto whitespace;
         }
      }
      else {
         ch = read_ch( parse );
      }
   }

   finish:
   // -----------------------------------------------------------------------
   token->type = tk;
   if ( text ) {
      token->text = text;
      token->length = length;
   }
   else {
      const struct token_info* info = p_get_token_info( tk );
      token->text = info->shared_text;
      token->length = ( length > 0 ) ?
         length : info->length;
   }
   token->pos.line = line;
   token->pos.column = column;
   token->pos.id = id;
   token->next = NULL;
   token->is_id = false;
}

inline struct text_buffer* get_text_buffer( struct parse* parse,
   int min_free_size ) {
   struct text_buffer* buffer = parse->text_buffer;
   if ( ! buffer || buffer->end - buffer->left < min_free_size ) {
      buffer = mem_alloc( sizeof( *buffer ) );
      buffer->prev = parse->text_buffer;
      enum { INITIAL_SIZE = 1 << 15 };
      unsigned int size = INITIAL_SIZE;
      while ( size < min_free_size ) {
         size <<= 1;
      }
      buffer->start = mem_alloc( sizeof( char ) * size );
      buffer->end = buffer->start + size;
      buffer->left = buffer->start;
      parse->text_buffer = buffer;
   }
   return buffer;
}

bool pop_source( struct parse* parse ) {
   if ( parse->source->prev ) {
      struct source* source = parse->source;
      parse->included_lines += source->line - LINE_OFFSET;
      parse->source = source->prev;
      source->prev = parse->free_source;
      parse->free_source = source;
      fclose( source->fh );
      return ( ! source->imported );
   }
   else {
      // NOTE: The main source must remain in the stack. Only free up resources
      // used by the main source.
      if ( ! parse->main_source_deinited ) {
         parse->main_lib_lines = parse->source->line - LINE_OFFSET;
         fclose( parse->source->fh );
         parse->main_source_deinited = true;
      }
      return false;
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
   else if ( isdigit( ch ) ) {
      if ( ch == '0' ) {
         ch = read_ch( parse );
         switch ( ch ) {
         case 'b':
         case 'B':
            goto binary_literal;
         case 'x':
         case 'X':
            goto hex_literal;
         case 'o':
         case 'O':
            ch = read_ch( parse );
            goto octal_literal;
         case '.':
            text = temp_text( parse );
            append_ch( text, '0' );
            append_ch( text, '.' );
            ch = read_ch( parse );
            goto fraction;
         default:
            goto zero;
         }
      }
      else {
         goto decimal_literal;
      }
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
      if ( pop_source( parse ) ) {
         ch = parse->source->ch;
         goto state_space;
      }
      else {
         tk = TK_END;
         goto state_finish;
      }
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
      int length = 0;
      text = temp_text( parse );
      while ( isalnum( ch ) || ch == '_' ) {
         append_ch( text, tolower( ch ) );
         ch = read_ch( parse );
         ++length;
      }
      if ( parse->lang == LANG_ACS95 &&
         length > parse->lang_limits->max_id_length ) {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "identifier too long (its length is %d, but maximum length is %d)",
            length, parse->lang_limits->max_id_length );
         p_bail( parse );
      }
      const struct keyword_table* table = &g_keyword_tables.bcs;
      switch ( parse->lang ) {
      case LANG_ACS95:
         table = &g_keyword_tables.acs95;
         break;
      default:
         break;
      }
      int i = 0;
      const char* id = text->value;
      if ( strcmp( id, table->entries[ table->mid ].name ) >= 0 ) {
         i = table->mid;
      }
      while ( true ) {
         // Identifier.
         if ( table->entries[ i ].name[ 0 ] > *id ) {
            tk = TK_ID;
            break;
         }
         // Reserved identifier.
         else if ( strcmp( table->entries[ i ].name, id ) == 0 ) {
            tk = table->entries[ i ].tk;
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
         if ( ! ( ch == '0' || ch == '1' ) ) {
            struct pos pos;
            t_init_pos( &pos, parse->source->file->id, parse->source->line,
               parse->source->column );
            p_diag( parse, DIAG_POS_ERR, &pos,
               "missing binary digit after digit separator" );
            p_bail( parse );
         }
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
         if ( ! isxdigit( ch ) ) {
            struct pos pos;
            t_init_pos( &pos, parse->source->file->id, parse->source->line,
               parse->source->column );
            p_diag( parse, DIAG_POS_ERR, &pos,
               "missing hexadecimal digit after digit separator" );
            p_bail( parse );
         }
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
         if ( ! ( ch >= '0' && ch <= '7' ) ) {
            struct pos pos;
            t_init_pos( &pos, parse->source->file->id, parse->source->line,
               parse->source->column );
            p_diag( parse, DIAG_POS_ERR, &pos,
               "missing octal digit after digit separator" );
            p_bail( parse );
         }
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

   zero:
   // -----------------------------------------------------------------------
   while ( ch == '0' || ( ch == '_' && peek_ch( parse ) == '0' ) ) {
      ch = read_ch( parse );
   }
   if ( isdigit( ch ) || ch == '_' ) {
      goto decimal_literal;
   }
   else if ( ch == '.' ) {
      text = temp_text( parse );
      append_ch( text, '0' );
      append_ch( text, '.' );
      ch = read_ch( parse );
      goto fraction;
   }
   else {
      text = temp_text( parse );
      str_append( text, "0" );
      tk = TK_LIT_DECIMAL;
      goto state_finish;
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
         if ( ! isdigit( ch ) ) {
            struct pos pos;
            t_init_pos( &pos, parse->source->file->id, parse->source->line,
               parse->source->column );
            p_diag( parse, DIAG_POS_ERR, &pos,
               "missing decimal digit after digit separator" );
            p_bail( parse );
         }
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
            "invalid digit in decimal literal" );
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
      token->text = copy_text( parse, text );
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
   if ( parse->lang == LANG_BCS ) {
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

char* copy_text( struct parse* parse, struct str* str ) {
   struct text_buffer* buffer = get_text_buffer( parse, str->length + 1 );
   memcpy( buffer->left, str->value, str->length + 1 );
   char* text = buffer->left;
   buffer->left += str->length + 1;
   return text;
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
   while ( true ) {
      pop_source( parse );
      if ( ! parse->source->prev ) {
         break;
      }
   }
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