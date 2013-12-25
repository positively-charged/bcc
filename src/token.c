#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "task.h"

struct paused {
   struct file* file;
   char* ch;
   int line;
   char* column;
   char* first_column;
   char* end;
   char recover_ch;
};

static char* escape_ch( struct task*, char*, char* );
static char hex_value( char );
static const char* get_token_name( enum tk );

void t_init_fields_tk( struct task* task ) {
   task->tk = TK_END;
   task->tk_text = NULL;
   task->tk_length = 0;
   task->ch = NULL;
   task->column = NULL;
   task->first_column = NULL;
   task->end = NULL;
   task->recover_ch = 0;
   stack_init( &task->paused );
   list_init( &task->unloaded_files );
   task->peeked_length = 0;
}

bool t_load_file( struct task* task, const char* path ) {
   bool direct = false;
   struct list_link* include_path = task->options->includes.head;
   struct str load_path;
   str_init( &load_path );
   while ( true ) {
      if ( ! direct ) {
         direct = true;
         str_append( &load_path, path );
      }
      else if ( include_path ) {
         str_clear( &load_path );
         str_append( &load_path, include_path->data );
         str_append( &load_path, "/" );
         str_append( &load_path, path );
         include_path = include_path->next;
      }
      else {
         str_del( &load_path );
         return false;
      }
      FILE* fh = fopen( load_path.value, "rb" );
      if ( ! fh ) {
         continue;
      }
      if ( task->ch ) {
         struct paused* paused = mem_alloc( sizeof( *paused ) );
         paused->file = task->file;
         paused->ch = task->ch;
         paused->line = task->line;
         paused->column = task->column;
         paused->first_column = task->first_column;
         paused->end = task->end;
         paused->recover_ch = task->recover_ch;
         stack_push( &task->paused, paused );
      }
      struct file* file = mem_alloc( sizeof( *file ) );
      file->load_path = load_path;
      str_init( &file->path );
      str_append( &file->path, path );
      task->file = file;
      fseek( fh, 0, SEEK_END );
      size_t size = ftell( fh );
      rewind( fh );
      char* ch = mem_alloc( size + 1 + 1 );
      ch += 1;
      fread( ch, sizeof( char ), size, fh );
      ch[ size ] = 0;
      fclose( fh );
      task->ch = ch;
      task->text = ch;
      task->end = ch;
      task->first_column = ch;
      task->recover_ch = *ch;
      task->line = 1;
      return true;
   }
}

void t_unload_file( struct task* task, bool* more ) {
   list_append( &task->unloaded_files, task->file );
   struct paused* paused = stack_pop( &task->paused );
   if ( paused ) {
      task->file = paused->file;
      task->ch = paused->ch;
      task->line = paused->line;
      task->column = paused->column;
      task->first_column = paused->first_column;
      task->end = paused->end;
      task->recover_ch = paused->recover_ch;
      t_read_tk( task );
      *more = true;
   }
}

void t_read_tk( struct task* task ) {
   *task->end = task->recover_ch;
   char* ch = task->ch;
   enum tk tk = TK_END;
   int length = 0;
   char* start;
   char* end;
   int line;
   int column;

   state_start: ;
   // -----------------------------------------------------------------------
   enum {
      MULTI_WHITESPACE,
      MULTI_LINE_COMMENT,
      MULTI_BLOCK_COMMENT,
      MULTI_STRING,
      MULTI_STRING_CONCAT
   } multi = MULTI_WHITESPACE;
   goto state_multi;

   state_text: ;
   // -----------------------------------------------------------------------
   start = ch;
   end = NULL;
   line = task->line;
   column = ch - task->first_column;
   // Identifier.
   if ( isalpha( *ch ) || *ch == '_' ) {
      char* id = start - 1;
      while ( isalnum( *ch ) || *ch == '_' ) {
         ch[ -1 ] = tolower( *ch );
         ++ch;
      }
      ch[ -1 ] = 0;
      length = ch - start;
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
         { "fixed", TK_FIXED },
         { "for", TK_FOR },
         { "function", TK_FUNCTION },
         { "global", TK_GLOBAL },
         { "goto", TK_GOTO },
         { "if", TK_IF },
         { "int", TK_INT },
         { "lightning", TK_LIGHTNING },
         { "namespace", TK_NAMESPACE },
         { "net", TK_NET },
         { "open", TK_OPEN },
         { "redreturn", TK_RED_RETURN },
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
         { "unloading", TK_UNLOADING },
         { "until", TK_UNTIL },
         { "using", TK_USING },
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
            start = id;
            goto state_finish;
         }
         // Reserved identifier.
         else if ( strncmp( reserved[ i ].name, id, ch - start ) == 0 &&
            reserved[ i ].name[ ch - start ] == 0 ) {
            tk = reserved[ i ].tk;
            start = id;
            goto state_finish;
         }
         else {
            i += 1;
         }
      }
   }
   else if ( *ch == '0' ) {
      ch += 1;
      // Hexadecimal.
      if ( *ch == 'x' || *ch == 'X' ) {
         do {
            ch += 1;
         } while (
            ( *ch >= '0' && *ch <= '9' ) ||
            ( *ch >= 'a' && *ch <= 'f' ) ||
            ( *ch >= 'A' && *ch <= 'F' ) );
         start += 2;
         tk = TK_LIT_HEX;
      }
      // Fixed-point number.
      else if ( *ch == '.' ) {
         ++ch;
         goto state_fraction;
      }
      else {
         while ( *ch >= '0' && *ch <= '7' ) {
            ++ch;
         }
         // Octal.
         if ( ch - 1 != start ) {
            tk = TK_LIT_OCTAL;
            start += 1;
         }
         // Decimal zero.
         else {
            tk = TK_LIT_DECIMAL;
         }
      }
      goto state_finish_number;
   }
   else if ( isdigit( *ch ) ) {
      do {
         ch += 1;
      } while ( isdigit( *ch ) );
      // Fixed-point number.
      if ( *ch == '.' ) {
         ++ch;
         goto state_fraction;
      }
      else {
         tk = TK_LIT_DECIMAL;
      }
      goto state_finish_number;
   }
   else if ( *ch == '"' ) {
      end = ch;
      ++ch;
      multi = MULTI_STRING;
      goto state_multi;
   }
   else if ( *ch == '\'' ) {
      ++ch;
      if ( *ch == '\\' ) {
         ch += 1;
         end = ch;
         ch = escape_ch( task, ch, end - 1 );
      }
      else if ( *ch == '\'' || ! *ch ) {
         struct pos pos = {
            task->file,
            task->line,
            start - task->first_column };
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "missing character in character literal" );
         bail();
      }
      else {
         ++ch;
      }
      if ( *ch != '\'' ) {
         struct pos pos = {
            task->file,
            task->line,
            start - task->first_column };
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "multiple characters in character literal" );
         bail();
      }
      ++start;
      ch += 1;
      tk = TK_LIT_CHAR;
      goto state_finish;
   }
   else if ( *ch == '/' ) {
      ch += 1;
      if ( *ch == '=' ) {
         tk = TK_ASSIGN_DIV;
         ch += 1;
         goto state_finish;
      }
      else if ( *ch == '/' ) {
         multi = MULTI_LINE_COMMENT;
         goto state_multi;
      }
      else if ( *ch == '*' ) {
         ch += 1;
         multi = MULTI_BLOCK_COMMENT;
         goto state_multi;
      }
      else {
         tk = TK_SLASH;
         goto state_finish;
      }
   }
   else if ( *ch == '=' ) {
      ch += 1;
      if ( *ch == '=' ) {
         tk = TK_EQ;
         ch += 1;
      }
      else {
         tk = TK_ASSIGN;
      }
      goto state_finish;
   }
   else if ( *ch == '+' ) {
      ch += 1;
      if ( *ch == '+' ) {
         tk = TK_INC;
         ch += 1;
      }
      else if ( *ch == '=' ) {
         tk = TK_ASSIGN_ADD;
         ch += 1;
      }
      else {
         tk = TK_PLUS;
      }
      goto state_finish;
   }
   else if ( *ch == '-' ) {
      ch += 1;
      if ( *ch == '-' ) {
         tk = TK_DEC;
         ch += 1;
      }
      else if ( *ch == '=' ) {
         tk = TK_ASSIGN_SUB;
         ch += 1;
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
         else if ( *ch == '<' ) {
            tk = TK_INSERTION;
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
      ch += 1;
      if ( *ch == '=' ) {
         tk = TK_GTE;
         ch += 1;
      }
      else if ( *ch == '>' ) {
         ch += 1;
         if ( *ch == '=' ) {
            tk = TK_ASSIGN_SHIFT_R;
            ch += 1;
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
      ch += 1;
      if ( *ch == '&' ) {
         tk = TK_LOG_AND;
         ch += 1;
      }
      else if ( *ch == '=' ) {
         tk = TK_ASSIGN_BIT_AND;
         ch += 1;
      }
      else {
         tk = TK_BIT_AND;
      }
      goto state_finish;
   }
   else if ( *ch == '|' ) {
      ch += 1;
      if ( *ch == '|' ) {
         tk = TK_LOG_OR;
         ch += 1;
      }
      else if ( *ch == '=' ) {
         tk = TK_ASSIGN_BIT_OR;
         ch += 1;
      }
      else {
         tk = TK_BIT_OR;
      }
      goto state_finish;
   }
   else if ( *ch == '^' ) {
      ch += 1;
      if ( *ch == '=' ) {
         tk = TK_ASSIGN_BIT_XOR;
         ch += 1;
      }
      else {
         tk = TK_BIT_XOR;
      }
      goto state_finish;
   }
   else if ( *ch == '!' ) {
      ch += 1;
      if ( *ch == '=' ) {
         tk = TK_NEQ;
         ch += 1;
      }
      else {
         tk = TK_LOG_NOT;
      }
      goto state_finish;
   }
   else if ( *ch == '*' ) {
      ch += 1;
      if ( *ch == '=' ) {
         tk = TK_ASSIGN_MUL;
         ch += 1;
      }
      else {
         tk = TK_STAR;
      }
      goto state_finish;
   }
   else if ( *ch == '%' ) {
      ch += 1;
      if ( *ch == '=' ) {
         tk = TK_ASSIGN_MOD;
         ch += 1;
      }
      else {
         tk = TK_MOD;
      }
      goto state_finish;
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
         ':', TK_COLON,
         '?', TK_QUESTION,
         '~', TK_BIT_NOT,
         '.', TK_DOT,
         '#', TK_HASH,
         0 };
      int i = 0;
      while ( true ) {
         if ( singles[ i ] == *ch ) {
            tk = singles[ i + 1 ];
            ch += 1;
            goto state_finish;
         }
         else if ( ! singles[ i ] ) {
            struct pos pos = { task->file, line, column };
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "invalid character '%c'", *ch );
            ++ch;
            goto state_start;
         }
         else {
            i += 2;
         }
      }
   }

   state_fraction:
   // -----------------------------------------------------------------------
   if ( ! isdigit( *ch ) ) {               
      struct pos pos = {
         task->file,
         task->line,
         ch - task->first_column };
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
         "fixed-point number missing fractional part" );
      bail();
   }
   while ( isdigit( *ch ) ) {
      ++ch;
   }
   tk = TK_LIT_FIXED;
   goto state_finish_number;

   state_finish_number:
   // -----------------------------------------------------------------------
   // Numbers need to be separated from identifiers.
   if ( isalpha( *ch ) ) {
      struct pos pos = {
         task->file,
         task->line,
         ch - task->first_column };
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
         "number combined with identifier" );
      bail();
   }
   goto state_finish;

   state_multi:
   // -----------------------------------------------------------------------
   while ( true ) {
      // TODO: Take care of testing for the platform-specific newline character.
      if ( *ch == '\n' ) {
         task->first_column = ch + 1;
         ++task->line;
         if ( multi == MULTI_LINE_COMMENT ) {
            multi = MULTI_WHITESPACE;
         }
      }
      switch ( multi ) {
      case MULTI_WHITESPACE:
         if ( ! isspace( *ch ) ) {
            goto state_text;
         }
         ++ch;
         break;
      case MULTI_LINE_COMMENT:
         if ( ! *ch ) {
            goto state_text;
         }
         ++ch;
         break;
      case MULTI_BLOCK_COMMENT:
         if ( ! *ch ) {
            struct pos pos = { task->file, line, column };
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "comment not terminated" );
            bail();
         }
         else if ( ch[ 0 ] == '*' && ch[ 1 ] == '/' ) {
            multi = MULTI_WHITESPACE;
            ch += 2;
         }
         else {
            ++ch;
         }
         break;
      case MULTI_STRING:
         if ( ! *ch ) {
            struct pos pos = { task->file, line, column };
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
               "string not terminated" );
            bail();
         }
         else if ( *ch == '"' ) {
            multi = MULTI_STRING_CONCAT;
            ++ch;
         }
         else if ( *ch == '\\' ) {
            ++ch;
            if ( *ch == '"' || *ch == '\\' ) {
               *end = *ch;
               ++end;
               ++ch;
            }
            // Color codes are not parsed.
            else if ( *ch == 'c' || *ch == 'C' ) {
               end[ 0 ] = '\\';
               end[ 1 ] = *ch;
               end += 2;
               ++ch;
            }
            else {
               ch = escape_ch( task, ch, end );
               ++end;
            }
         }
         else {
            *end = *ch;
            ++end;
            ++ch;
         }
         break;
      case MULTI_STRING_CONCAT:
         if ( isspace( *ch ) ) {
            ++ch;
         }
         else if ( *ch == '"' ) {
            line = task->line;
            column = ch - task->first_column;
            multi = MULTI_STRING;
            ++ch;
         }
         else {
            tk = TK_LIT_STRING;
            goto state_finish;
         }
      }
   }

   state_finish:
   // -----------------------------------------------------------------------
   task->ch = ch;
   task->tk = tk;
   task->tk_text = start;
   if ( ! end ) {
      end = ch;
   }
   // Terminate the token source with the NULL byte so it can be used as a
   // string. Save the character so it can be recovered for the next token.
   task->recover_ch = *end;
   task->end = end;
   *end = 0;
   task->tk_pos.file = task->file;
   task->tk_pos.line = line;
   task->tk_pos.column = column;
   task->tk_length = length;
   if ( ! length ) {
      task->tk_length = end - start;
   }
}

char* escape_ch( struct task* task, char* ch, char* dest ) {
   switch ( *ch ) {
   case 'n':
      *dest = '\n';
      return ++ch;
   case '\\':
      *dest = '\\';
      return ++ch;
   case 't':
      *dest = '\t';
      return ++ch;
   case '0':
      *dest = '\0';
      return ++ch;
   }
   if ( *ch == 'x' || *ch == 'X' ) {
      ++ch;
      char hex = hex_value( *ch );
      if ( hex != -1 ) {
         ++ch;
         char hex2 = hex_value( *ch );
         if ( hex2 != -1 ) {
            hex = ( hex << 4 ) + hex2;
            ++ch;
         }
      }
      else {
         struct pos pos = {
            task->file,
            task->line,
            ch - task->first_column };
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &pos,
            "escape sequence missing hexadecimal digit" );
         bail();
      }
      *dest = hex;
   }
   // Else: unknown sequence. For now, don't print any message. The engine
   // might support the escape sequence we don't.
   return ch;
}

char hex_value( char ch ) {
   if ( ch >= '0' && ch <= '9' ) {
      return ch - '0';
   }
   else {
      ch = tolower( ch );
      if ( ch >= 'a' && ch <= 'f' ) {
         return ( ch - 'a' ) + 10;
      }
      else {
         return -1;
      }
   }
}

void t_test_tk( struct task* task, enum tk expected ) {
   if ( task->tk != expected ) {
      if ( task->tk == TK_END ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "expecting %s but reached end of file",
            get_token_name( expected ) );
      }
      else if ( task->tk == TK_RESERVED ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "%s is a reserved identifier that is not currently used",
            task->tk_text );
      }
      else {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos, 
            "unexpected token" );
         diag( DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos, 
            "expecting %s before %s token", get_token_name( expected ),
            get_token_name( task->tk ) );
      }
      bail();
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
      { TK_QUESTION, "`?`" },
      { TK_INSERTION, "`<<<`" },
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
      { TK_NAMESPACE, "`namespace`" },
      { TK_USING, "`using`" },
      { TK_GOTO, "`goto`" },
      { TK_FIXED, "`fixed`" } };
   switch ( tk ) {
   case TK_LIT_OCTAL:
   case TK_LIT_DECIMAL:
   case TK_LIT_HEX:
   case TK_LIT_FIXED:
      return "number";
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

char t_peek_usable_ch( struct task* task ) {
   *task->end = task->recover_ch;
   char* ch = task->ch;
   while ( *ch && isspace( *ch ) ) {
      ch += 1;
   }
   char usable = *ch;
   *task->end = 0;
   return usable;
}

void t_skip_past( struct task* task, char target ) {
   char* ch = task->ch;
   *ch = task->recover_ch;
   while ( *ch ) {
      if ( *ch == '\n' ) {
         task->first_column = ch + 1;
         task->line += 1;
         ch += 1;
      }
      if ( *ch == target ) {
         ch += 1;
         t_read_tk( task );
         break;
      }
      else {
         ch += 1;
      }
   }
   task->ch = ch;
}

bool t_is_dec_start( struct task* task ) {
   bool result = false;
   char* ch = task->ch;
   *task->end = task->recover_ch;
   if ( task->tk == TK_DOT ) {
      goto read_name;
   }
   while ( true ) {
      while ( isspace( *ch ) ) {
         ch += 1;
      }
      if ( isalpha( *ch ) || *ch == '_' || isdigit( *ch ) ) {
         result = true;
         break;
      }
      if ( *ch != '.' ) {
         break;
      }
      ++ch;
      read_name:
      while ( isspace( *ch ) ) {
         ++ch;
      }
      char* start = ch;
      while ( isalpha( *ch ) || *ch == '_' ) {
         ++ch;
      }
      if ( ch == start ) {
         break;
      }
   }
   *task->end = 0;
   return result;
}

bool t_coming_up_insertion_token( struct task* task ) {
   if ( task->ch == task->end ) {
      if ( isspace( task->recover_ch ) ) {
         char* ch = task->ch + 1;
         while ( *ch && isspace( *ch ) ) {
            ++ch;
         }
         return
            ch[ 0 ] == '<' &&
            ch[ 1 ] == '<' &&
            ch[ 2 ] == '<';
      }
      else {
         return
            task->recover_ch == '<' &&
            task->ch[ 0 ] == '<' &&
            task->ch[ 1 ] == '<';
      }
   }
   else {
      char* ch = task->ch;
      while ( *ch && isspace( *ch ) ) {
         ++ch;
      }
      return
         ch[ 0 ] == '<' &&
         ch[ 1 ] == '<' &&
         ch[ 2 ] == '<';
   }
}