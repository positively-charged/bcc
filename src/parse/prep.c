#include <string.h>

#include "phase.h"

static void output_source( struct parse* parse, struct str* output );
static void output_token( struct parse* parse, struct str* output );
static void read_token( struct parse* parse );
static bool pseudo_dirc( struct parse* parse );
static void read_dirc( struct parse* parse );
static void read_known_dirc( struct parse* parse, struct pos* pos );
static void read_include( struct parse* parse );
static void read_error( struct parse* parse, struct pos* pos );
static void read_line( struct parse* parse );

void p_preprocess( struct parse* parse ) {
   p_load_main_source( parse );
   parse->read_flags = READF_NL | READF_SPACETAB;
   struct str output;
   str_init( &output );
   output_source( parse, &output );
   if ( output.length > 0 && output.value[ output.length - 1 ] != '\n' ) {
      str_append( &output, NEWLINE_CHAR );
   }
   printf( "%s", output.value );
   str_deinit( &output );
}

void output_source( struct parse* parse, struct str* output ) {
   while ( true ) {
      read_token( parse );
      if ( parse->tk != TK_END ) {
         output_token( parse, output );
      }
      else {
         break;
      }
   }
}

void output_token( struct parse* parse, struct str* output ) {
   switch ( parse->tk ) {
   case TK_NL:
      str_append( output, NEWLINE_CHAR );
      break;
   case TK_SPACE:
   case TK_TAB:
      str_append( output, " " );
      break;
   case TK_LIT_STRING:
      str_append( output, "\"" );
      str_append( output, parse->tk_text );
      str_append( output, "\"" );
      break;
   case TK_LIT_CHAR:
      str_append( output, "'" );
      str_append( output, parse->tk_text );
      str_append( output, "'" );
      break;
   default:
      str_append( output, parse->tk_text );
      break;
   }
}

void read_token( struct parse* parse ) {
   p_read_tk( parse );
   switch ( parse->tk ) {
   case TK_NL:
      parse->line_beginning = true;
      if ( ! ( parse->read_flags & READF_NL ) ) {
         read_token( parse );
      }
      break;
   case TK_SPACE:
   case TK_TAB:
      break;
   case TK_HASH:
      // A directive must appear at the beginning of a line.
      if ( parse->line_beginning ) {
         parse->line_beginning = false;
         if ( ! pseudo_dirc( parse ) ) {
            read_dirc( parse );
         }
      }
      break;
   default:
      parse->line_beginning = false;
      break;
   }
}

bool pseudo_dirc( struct parse* parse ) {
   static const char* table[] = {
      "library",
      "import",
      "nocompact",
      "encryptstrings",
      "wadauthor",
      "nowadauthor",
   };
   int flags = parse->read_flags;
   parse->read_flags ^= READF_SPACETAB;
   struct token* token = p_peek_tk( parse );
   parse->read_flags = flags;
   for ( int i = 0; i < ARRAY_SIZE( table ); ++i ) {
      if ( strcmp( table[ i ], token->text ) == 0 ) {
         return true;
      }
   }
   return false;
}

void read_dirc( struct parse* parse ) {
   int flags = parse->read_flags;
   parse->read_flags ^= READF_SPACETAB;
   struct pos pos = parse->tk_pos;
   p_test_tk( parse, TK_HASH );
   p_read_tk( parse );
   if ( parse->tk != TK_NL ) {
      read_known_dirc( parse, &pos );
   }
   parse->line_beginning = true;
   parse->read_flags = flags;
   read_token( parse );
}

void read_known_dirc( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_ID );
   if ( strcmp( parse->tk_text, "include" ) == 0 ) {
      read_include( parse );
   }
   else if ( strcmp( parse->tk_text, "error" ) == 0 ) {
      read_error( parse, pos );
   }
   else if ( strcmp( parse->tk_text, "line" ) == 0 ) {
      read_line( parse );
   }
   else {
      p_diag( parse, DIAG_POS_ERR, pos,
         "unknown directive" );
      p_bail( parse );
   }
}

void read_include( struct parse* parse ) {
   p_test_tk( parse, TK_ID );
   p_read_tk( parse );
   p_test_tk( parse, TK_LIT_STRING );
   const char* path = parse->tk_text;
   struct pos pos = parse->tk_pos;
   p_read_tk( parse );
   p_test_tk( parse, TK_NL );
   struct request request;
   p_init_request( &request, path );
   p_load_source( parse, &request );
   if ( ! request.source ) {
      if ( request.err_loading ) {
         p_diag( parse, DIAG_POS_ERR, &pos,
            "file already being loaded" );
         p_bail( parse );
      }
      else {
         p_diag( parse, DIAG_POS_ERR, &pos,
            "failed to load file: %s", path );
         p_bail( parse );
      }
   }
}

void read_error( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_ID );
   int flags = parse->read_flags;
   parse->read_flags |= READF_SPACETAB;
   p_read_tk( parse );
   struct str message;
   str_init( &message );
   while ( parse->tk != TK_NL ) {
      bool space_separator = false;
      while (
         parse->tk == TK_SPACE ||
         parse->tk == TK_TAB ) {
         space_separator = true;
         p_read_tk( parse );
      }
      if ( message.length > 0 && space_separator ) {
         str_append( &message, " " );
      }
      output_token( parse, &message );
      p_read_tk( parse );
   }
   p_diag( parse, DIAG_POS_ERR | DIAG_CUSTOM, pos, message.value );
   p_bail( parse );
}

void read_line( struct parse* parse ) {
   p_test_tk( parse, TK_ID );
   p_read_tk( parse );
   p_test_tk( parse, TK_LIT_DECIMAL );
   int line = strtol( parse->tk_text, NULL, 10 );
   if ( line == 0 ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "invalid line-number argument" );
      p_bail( parse );
   }
   parse->source->line = line;
   p_read_tk( parse );
   // Custom filename.
   if ( parse->tk == TK_LIT_STRING ) {
      parse->source->file_entry_id = t_add_altern_filename(
         parse->task, parse->tk_text );
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_NL );
}