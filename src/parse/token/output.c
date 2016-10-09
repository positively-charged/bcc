#include "phase.h"

static void output_source( struct parse* parse, struct str* output );
static void output_token( struct parse* parse, struct str* output );

void p_preprocess( struct parse* parse ) {
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
      p_read_eoptiontk( parse );
      if ( parse->token->type != TK_END ) {
         output_token( parse, output );
      }
      else {
         break;
      }
   }
   p_confirm_ifdircs_closed( parse );
}

// TODO: Get the original text of the token.
void output_token( struct parse* parse, struct str* output ) {
   switch ( parse->token->type ) {
   case TK_NL:
      str_append( output, NEWLINE_CHAR );
      break;
   case TK_HORZSPACE:
      for ( int i = 0; i < parse->token->length; ++i ) {
         str_append( output, parse->token->text );
      }
      break;
   case TK_LIT_STRING:
      str_append( output, "\"" );
      for ( int i = 0; i < parse->token->length; ++i ) {
         if ( parse->token->text[ i ] == '"' ) {
            str_append( output, "\\" );
         }
         char ch[] = { parse->token->text[ i ], 0 };
         str_append( output, ch );
      }
      str_append( output, "\"" );
      break;
   case TK_LIT_CHAR:
      str_append( output, "'" );
      str_append( output, parse->token->text );
      str_append( output, "'" );
      break;
   case TK_LIT_OCTAL:
      str_append( output, "0o" );
      str_append( output, parse->token->text );
      break;
   case TK_LIT_HEX:
      str_append( output, "0x" );
      str_append( output, parse->token->text );
      break;
   case TK_LIT_BINARY:
      str_append( output, "0b" );
      str_append( output, parse->token->text );
      break;
   case TK_LIT_RADIX:
      for ( int i = 0; i < parse->token->length; ++i ) {
         if ( parse->token->text[ i ] == '_' ) {
            str_append( output, "r" );
         }
         else {
            char ch[] = { parse->token->text[ i ], 0 };
            str_append( output, ch );
         }
      }
      break;
   default:
      str_append( output, parse->token->text );
      break;
   }
}