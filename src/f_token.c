#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "mem.h"
#include "f_main.h"
#include "detail.h"

#define FIRST_LINE 1

typedef struct source_t {
   file_t* file;
   char* data;
   // One character beyond the end of the token source.
   char* end;
   char* first_column;
   int line;
   // When a token is found, its source must be terminated with a NULL byte so
   // it can be used as a string. We can't just insert a NULL byte at the end
   // of the token source. That may delete an important character of the next
   // token. This member stores this character. This way, the NULL byte can be
   // used. When the next token is looked for, the replaced character is again
   // placed at its original position.
   char resume_char;
} source_t;

typedef struct {
   const char* name;
   tk_t tk;
} keyword_t;

static void init_token( token_t* );
static int add_file( front_t*, const char*, str_t* );
static token_t* deq_token( front_t* );
static void read_token( front_t*, source_t*, token_t* );
static char* char_escape( front_t*, char*, char* );
static char hex_value( char );
static token_t* push_token( front_t* );
static const keyword_t* find_keyword( const char* name, int length );

int tk_load_file( front_t* front, const char* user_path ) {
   int err = add_file( front, user_path, NULL );
   if ( err == k_tk_file_err_none ) {
      return err;
   }

   str_t path;
   str_init( &path );

   file_t* file = job_active_file( front->file_table );
   if ( file ) {
      int length = 0;
      int i = 0;
      const char* source_path = file->load_path.value;
      while ( source_path[ i ] ) {
         if ( source_path[ i ] == '/' || source_path[ i ] == '\\' ) {
            length = i + 1;
         }
         ++i;
      }
      if ( length ) {
         str_append_sub( &path, source_path, length );
         str_append( &path, user_path );
         err = add_file( front, user_path, &path );
         if ( err == k_tk_file_err_none ) {
            return err;
         }
         else if ( err == k_tk_file_err_dup ) {
            str_del( &path );
            return err;
         }
         str_clear( &path );
      }
   }

   list_iterator_t i = list_iterate( front->file_table->include_paths );
   while ( ! list_end( &i ) ) {
      const char* include = list_idata( &i );
      str_append( &path, include );
      str_append( &path, "/" );
      str_append( &path, user_path );
      err = add_file( front, user_path, &path );
      if ( err == k_tk_file_err_none || err == k_tk_file_err_dup ) {
         break;
      }
      str_clear( &path );
      list_next( &i );
   }
   if ( err != k_tk_file_err_none ) {
      str_del( &path );
   }
   return err;
}

int add_file( front_t* front, const char* user_path, str_t* load_path ) {
   const char* path = user_path;
   if ( load_path ) {
      path = load_path->value;
   }
   int err = k_tk_file_err_none;
   FILE* fh = fopen( path, "rb" );
   if ( ! fh ) {
      err = k_tk_file_err_open;
      goto finish;
   }

   // The file being loaded should not already be loading.
   active_file_t* active = front->file_table->active;
   while ( active ) {
      if ( strcmp( path, active->file->load_path.value ) == 0 ) {
         err = k_tk_file_err_dup;
         goto close_file;
      }
      active = active->prev;
   }

   fseek( fh, 0, SEEK_END );
   size_t size = ftell( fh );
   rewind( fh );
   char* data = mem_temp_alloc( size + 1 );
   fread( data, sizeof( char ), size, fh );
   data[ size ] = '\0';

   file_t* file = mem_alloc( sizeof( *file ) );
   file->next = NULL;
   if ( load_path ) {
      file->load_path = *load_path;
   }
   else {
      str_init( &file->load_path );
      str_append( &file->load_path, user_path );
   }
   str_init( &file->user_path );
   str_append( &file->user_path, user_path );
   file->imported = false;
   file_table_t* table = front->file_table;
   if ( table->file ) {
      table->file->next = file;
      table->file = file;
   }
   else {
      table->file_head = file;
      table->file = file;
   }

   active = mem_temp_alloc( sizeof( *active ) );
   active->next = NULL;
   active->prev = front->file_table->active;
   active->file = file;
   front->file_table->active = active;

   source_t* source = mem_temp_alloc( sizeof( *source ) );
   source->file = file;
   source->data = data;
   source->line = FIRST_LINE;
   source->resume_char = data[ 0 ];
   source->end = data;
   source->first_column = data;
   active->source = source;
   front->source = source;

   close_file:
   fclose( fh );

   finish:
   return err;
}

bool tk_pop_file( front_t* front ) {
   active_file_t* active = front->file_table->active;
   active = active->prev;
   front->file_table->active = active;
   if ( active ) {
      active->next = NULL;
      front->source = active->source;
      return true;
   }
   else {
      return false;
   }
}

token_t* push_token( front_t* front ) {
   int pos = ( front->queue.pos + front->queue.size ) % TK_QUEUE_SIZE;
   if ( front->queue.size != TK_QUEUE_SIZE ) {
      ++front->queue.size;
   }
   init_token( &front->queue.tokens[ pos ] );
   return &front->queue.tokens[ pos ];  
}

token_t* deq_token( front_t* front ) {
   token_t* token = &front->queue.tokens[ front->queue.pos ];
   front->queue.pos = ( front->queue.pos + 1 ) % TK_QUEUE_SIZE;
   --front->queue.size;
   return token;
}

token_t* tk_peek( front_t* front, int pos ) {
   // Read tokens up to the requested position. 
   token_t* token = &front->queue.tokens[ front->queue.pos ];
   while ( pos + 1 ) {
      token = push_token( front );
      read_token( front, front->source, token );
      --pos;
   }
   return token;
}

void tk_read( front_t* front ) {
   if ( front->queue.size ) {
      front->token = *deq_token( front );
   }
   else {
      read_token( front, front->source, &front->token );
   }
}

// Only used in areas where a new line delimiter may appear.
// TODO: Check for tabs.
void read_char( source_t* source, char** ch ) {
   // TODO: Take care of testing for the platform-specific newline character.
   if ( **ch == '\n' ) {
      source->first_column = *ch + 1;
      source->line += 1;
   }
   *ch += 1;
}

void read_token( front_t* front, source_t* source, token_t* token ) {
   // Recover the character replaced by the NULL character.
   *source->end = source->resume_char;
   char* ch = source->data;
   tk_t token_type = tk_end;

   state_start:
   // -----------------------------------------------------------------------
   // Skip whitespace.
   while ( isspace( *ch ) ) {
      read_char( front->source, &ch );
   }

   char* start = ch;
   char* end = NULL;
   char* column = ch;
   char* first_column = source->first_column;
   int start_line = source->line;

   // Identifier.
   if ( isalpha( *ch ) || *ch == '_' ) {
      goto state_id;
   }
   // Zero can be a prefix to some other numeric notation.
   else if ( *ch == '0' ) {
      ++ch;
      goto state_zero;
   }
   // Decimal or fixed.
   else if ( *ch >= '1' && *ch <= '9' ) {
      ++ch;
      goto state_decimal;
   }
   else {
      switch ( *ch ) {
      case '"':
         ++ch;
         goto state_string;
      case '/':
         ++ch;
         goto state_slash;
      case '=':
         ++ch;
         goto state_assign;
      case '+':
         ++ch;
         goto state_add;
      case '-':
         ++ch;
         goto state_sub;
      case '<':
         ++ch;
         goto state_angleb_l;
      case '>':
         ++ch;
         goto state_angleb_r;
      case '&':
         ++ch;
         goto state_ampersand;
      case '|':
         ++ch;
         goto state_pipe;
      case '^':
         ++ch;
         goto state_carrot;
      case '!':
         ++ch;
         goto state_exclamation;
      case '*':
         ++ch;
         goto state_star;
      case '%':
         ++ch;
         goto state_percent;
      case '\'':
         ++ch;
         goto state_char;
      case ';':
         ++ch;
         token_type = tk_semicolon;
         goto state_finish;
      case ',':
         ++ch;
         token_type = tk_comma;
         goto state_finish;
      case '(':
         ++ch;
         token_type = tk_paran_l;
         goto state_finish;
      case ')':
         ++ch;
         token_type = tk_paran_r;
         goto state_finish;
      case '{':
         ++ch;
         token_type = tk_brace_l;
         goto state_finish;
      case '}':
         ++ch;
         token_type = tk_brace_r;
         goto state_finish;
      case '[':
         ++ch;
         token_type = tk_bracket_l;
         goto state_finish;
      case ']':
         ++ch;
         token_type = tk_bracket_r;
         goto state_finish;
      case ':':
         ++ch;
         token_type = tk_colon;
         goto state_finish;
      case '?':
         ++ch;
         token_type = tk_question;
         goto state_finish;
      case '~':
         ++ch;
         token_type = tk_bit_not;
         goto state_finish;
      case '.':
         ++ch;
         token_type = tk_dot;
         goto state_finish;
      case '#':
         ++ch;
         token_type = tk_hash;
         goto state_finish;
      // End.
      case '\0':
         token_type = tk_end;
         goto state_finish;
      default: ;
         position_t pos = {
            source->file,
            source->line,
            column - source->first_column };
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "invalid character '%c'", *ch );
         ++ch;
         goto state_start;
      }
   }

   state_id:
   // -----------------------------------------------------------------------
   while ( isalnum( *ch ) || *ch == '_' ) {
      *ch = tolower( *ch );
      ++ch;
   }
   // A keyword is a reserved identifier.
   const keyword_t* keyword = find_keyword( start, ch - start );
   if ( keyword ) {
      token_type = keyword->tk;
   }
   else {
      token_type = tk_id;
   }
   goto state_finish;

   state_zero:
   // -----------------------------------------------------------------------
   // Hexadecimal.
   if ( *ch == 'x' || *ch == 'X' ) {
      ++ch;
      goto state_hex;
   }
   // Octal.
   else if ( *ch >= '0' && *ch <= '7' ) {
      ++ch;
      goto state_octal;
   }
   // Decimal zero.
   else {
      goto state_decimal;
   }

   state_decimal:
   // -----------------------------------------------------------------------
   while ( *ch >= '0' && *ch <= '9' ) {
      ++ch;
   }
   // Fixed-point number.
   if ( *ch == '.' ) {
      ++ch;
      goto state_fixed;
   }
   else {
      token_type = tk_lit_decimal;
      goto state_finish;
   }

   state_fixed:
   // -----------------------------------------------------------------------
   while ( isdigit( *ch ) ) {
      ++ch;
   }
   token_type = tk_lit_fixed;
   goto state_finish;

   state_hex:
   // -----------------------------------------------------------------------
   while (
      ( *ch >= '0' && *ch <= '9' ) ||
      ( *ch >= 'a' && *ch <= 'f' ) ||
      ( *ch >= 'A' && *ch <= 'F' ) ) {
      ++ch;
   }
   // Prefix not needed.
   start += 2;
   token_type = tk_lit_hex;
   goto state_finish;

   state_octal:
   // -----------------------------------------------------------------------
   while ( *ch >= '0' && *ch <= '7' ) {
      ++ch;
   }
   // Prefix not needed.
   start += 1;
   token_type = tk_lit_octal;
   goto state_finish;

   state_char:
   // -----------------------------------------------------------------------
   if ( *ch == '\\' ) {
      ++ch;
      ch = char_escape( front, ch, start );
   }
   else if ( *ch == '\'' || ! *ch ) {
      position_t pos = {
         source->file,
         source->line,
         ch - source->first_column };
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
         "missing character in character literal" );
      f_bail( front );
   }
   else {
      ++ch;
   }
   // Only a single character should be present.
   if ( *ch != '\'' ) {
      position_t pos = {
         source->file,
         source->line,
         column - source->first_column };
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
         "multiple characters in character literal" );
      f_bail( front );
   }
   ++start;
   end = ch;
   ++ch;
   token_type = tk_lit_char;
   goto state_finish;

   state_string: ;
   // -----------------------------------------------------------------------
   end = ch;
   while ( true ) {
      // Search for the terminating quotation mark. Make sure the character is
      // not the NULL byte of the input. This may occur if a string is not
      // terminated.
      while ( true ) {
         // Unterminated string.
         if ( ! *ch ) {
            position_t pos = {
               source->file,
               start_line,
               column - source->first_column };
            f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
               "unterminated string" );
            f_bail( front );
         }
         else if ( *ch == '"' ) {
            ++ch;
            break;
         }
         // Expand escape sequence.
         else if ( *ch == '\\' ) {
            ++ch;
            if ( *ch == '"' || *ch == '\\' ) {
               *end = *ch;
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
               ch = char_escape( front, ch, ch );
            }
         }
         else {
            *end = *ch;
            read_char( front->source, &ch );
            ++end;
         }
      }
      // Concatenate with any subsequent strings.
      while ( isspace( *ch ) ) {
         read_char( front->source, &ch );
      }
      if ( *ch == '"' ) {
         start_line = source->line;
         column = ch;
         ++ch;
      }
      else {
         // The quotation marks are not needed.
         ++start;
         token_type = tk_lit_string;
         goto state_finish;
      }
   }

   state_slash:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_assign_div;
      goto state_finish;
   case '/':
      ++ch;
      goto state_comment_line;
   case '*':
      ++ch;
      goto state_comment_block;
   default:
      token_type = tk_slash;
      goto state_finish;
   }

   state_assign:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_eq;
      goto state_finish;
   default:
      token_type = tk_assign;
      goto state_finish;
   }

   state_add:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '+':
      ++ch;
      token_type = tk_increment;
      goto state_finish;
   case '=':
      ++ch;
      token_type = tk_assign_add;
      goto state_finish;
   default:
      token_type = tk_plus;
      goto state_finish;
   }

   state_sub:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '-':
      ++ch;
      token_type = tk_decrement;
      goto state_finish;
   case '=':
      ++ch;
      token_type = tk_assign_sub;
      goto state_finish;
   default:
      token_type = tk_minus;
      goto state_finish;
   }

   state_angleb_l:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_lte;
      goto state_finish;
   case '<':
      ++ch;
      goto state_shift_l;
   default:
      token_type = tk_lt;
      goto state_finish;
   }

   state_shift_l:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_assign_shift_l;
      goto state_finish;
   default:
      token_type = tk_shift_l;
      goto state_finish;
   }

   state_angleb_r:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_gte;
      goto state_finish;
   case '>':
      ++ch;
      goto state_shift_r;
   default:
      token_type = tk_gt;
      goto state_finish;
   }

   state_shift_r:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_assign_shift_r;
      goto state_finish;
   default:
      token_type = tk_shift_r;
      goto state_finish;
   }

   state_ampersand:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '&':
      ++ch;
      token_type = tk_log_and;
      goto state_finish;
   case '=':
      ++ch;
      token_type = tk_assign_bit_and;
      goto state_finish;
   default:
      token_type = tk_bit_and;
      goto state_finish;
   }

   state_pipe:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '|':
      ++ch;
      token_type = tk_log_or;
      goto state_finish;
   case '=':
      ++ch;
      token_type = tk_assign_bit_or;
      goto state_finish;
   default:
      token_type = tk_bit_or;
      goto state_finish;
   }

   state_carrot:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_assign_bit_xor;
      goto state_finish;
   default:
      token_type = tk_bit_xor;
      goto state_finish;
   }

   state_exclamation:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_neq;
      goto state_finish;
   default:
      token_type = tk_log_not;
      goto state_finish;
   }

   state_star:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_assign_mul;
      goto state_finish;
   default:
      token_type = tk_star;
      goto state_finish;
   }

   state_percent:
   // -----------------------------------------------------------------------
   switch ( *ch ) {
   case '=':
      ++ch;
      token_type = tk_assign_mod;
      goto state_finish;
   default:
      token_type = tk_mod;
      goto state_finish;
   }

   state_comment_line: ;
   // -----------------------------------------------------------------------
   char* old = source->first_column;
   while ( *ch ) {
      read_char( source, &ch );
      if ( source->first_column != old ) {
         break;
      }
   }
   goto state_start;

   state_comment_block:
   // -----------------------------------------------------------------------
   while ( true ) {
      // Unterminated comment.
      if ( ! *ch ) {
         position_t pos = {
            source->file,
            start_line,
            column - source->first_column };
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "unterminated comment" );
         f_bail( front );
      }
      // End tag.
      else if ( *ch == '*' && ch[ 1 ] == '/' ) {
         ch += 2;
         goto state_start;
      }
      else {
         read_char( source, &ch );
      }
   }

   state_finish:
   // -----------------------------------------------------------------------
   source->data = ch;
   if ( ! end ) {
      end = ch;
   }
   // Terminate the token source with the NULL byte so it can be used as a
   // string. Save the character so it can be recovered for the next token.
   source->resume_char = *end;
   source->end = end;
   *end = '\0';
   token->type = token_type;
   token->source = start;
   token->length = end - start;
   token->pos.line = start_line;
   token->pos.column = column - first_column;
   token->pos.file = source->file;
}

char* char_escape( front_t* front, char* ch, char* dest ) {
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
   // Character in hexadecimal encoding.
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
         position_t pos = {
            front->source->file,
            front->source->line,
            ch - front->source->first_column };
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "escape sequence missing hexadecimal digit" );
         f_bail( front );
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

void init_token( token_t* token ) {
   token->type = tk_end;
   token->source = NULL;
   token->length = 0;
}

char f_term_token( token_t* token ) {
   char last_ch = token->source[ token->length ];
   token->source[ token->length ] = 0;
   return last_ch;
}

void f_unterm_token( token_t* token, char ch ) {
   token->source[ token->length ] = ch;
}

// NOTE: Keywords must be listed in ascending order because a binary search
// is used to find a specific keyword.
static const keyword_t keywords[] = {
   { "bluereturn", tk_blue_return },
   { "bool", tk_bool },
   { "break", tk_break },
   { "case", tk_case },
   { "clientside", tk_clientside },
   { "const", tk_const },
   { "continue", tk_continue },
   // Not really a keyword but is given a specific token identifier to make it
   // easier for the parser to recognize.
   { "createtranslation", tk_paltrans },
   { "death", tk_death },
   { "default", tk_default },
   { "disconnect", tk_disconnect },
   { "do", tk_do },
   { "else", tk_else },
   { "enter", tk_enter },
   { "for", tk_for },
   { "function", tk_function },
   { "global", tk_global },
   { "if", tk_if },
   { "int", tk_int },
   { "lightning", tk_lightning },
   { "net", tk_net },
   { "open", tk_open },
   { "redreturn", tk_red_return },
   { "respawn", tk_respawn },
   { "restart", tk_restart },
   { "return", tk_return },
   { "script", tk_script },
   { "special", tk_special },
   { "static", tk_static },
   { "str", tk_str },
   { "suspend", tk_suspend },
   { "switch", tk_switch },
   { "terminate", tk_terminate },
   { "unloading", tk_unloading },
   { "until", tk_until },
   { "void", tk_void },
   { "while", tk_while },
   { "whitereturn", tk_white_return },
   { "world", tk_world }
};

// Returns -1 if the name is smaller than the keyword, 0 if the keyword and the
// names match, or 1 if the name is greater than the keyword.
int keyword_cmp( const char* name, int length, const char* keyword ) {
   int i = 0;
   while ( i < length ) {
      if ( name[ i ] < keyword[ i ] ) {
         return -1;
      }
      else if ( name[ i ] > keyword[ i ] ) {
         return 1;
      }
      else {
         ++i;
      }
   }
   // The substrings might match, but the full keyword needs to be matched
   // successfully.
   if ( keyword[ length ] == 0 ) {
      return 0;
   }
   else {
      return -1;
   }
}

// Searches for a keyword using a binary search.
const keyword_t* find_keyword( const char* name, int length ) {
   int end = ( sizeof keywords / sizeof keywords[ 0 ] ) - 1;
   int beg = 0;
   while ( beg <= end ) {
      // Middle element.
      int mid = beg + ( ( end - beg ) / 2 );
      // Check middle element.
      int result = strncmp( keywords[ mid ].name, name, length );
      if ( result == 0 && keywords[ mid ].name[ length ] ) {
         result = 1;
      }
      // Match.
      if ( result == 0 ) {
         return &keywords[ mid ];
      }
      // Lower side.
      else if ( result > 0 ) {
         end = mid - 1;
      }
      // Higher side.
      else {
         beg = mid + 1;
      }
   }
   return 0;
}