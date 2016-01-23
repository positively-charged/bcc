#include "phase.h"

static void output_source( struct parse* parse, struct str* output );
static void output_token( struct parse* parse, struct str* output );

void p_preprocess( struct parse* parse ) {
   p_load_main_source( parse );
   parse->read_flags = READF_WHITESPACE;
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
      p_read_tk( parse );
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