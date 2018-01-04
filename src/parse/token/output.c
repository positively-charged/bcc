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

static void output_source( struct parse* parse, struct str* output ) {
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
static void output_token( struct parse* parse, struct str* output ) {
   p_decorate_token( parse->token, output, true );
}

void p_decorate_token( struct token* token, struct str* string,
   bool expand_horzspace ) {
   // TODO: Make sure we encode every token, because I did not actually do that
   // before adding the static assert.
   STATIC_ASSERT( TK_TOTAL == 157 );
   switch ( token->type ) {
   case TK_NL:
      str_append( string, NEWLINE_CHAR );
      break;
   case TK_HORZSPACE:
      if ( expand_horzspace ) {
         for ( int i = 0; i < token->length; ++i ) {
            str_append( string, token->text );
         }
      }
      else {
         str_append( string, " " );
      }
      break;
   case TK_LIT_STRING:
      str_append( string, "\"" );
      for ( int i = 0; i < token->length; ++i ) {
         if ( token->text[ i ] == '"' ) {
            str_append( string, "\\" );
         }
         char ch[] = { token->text[ i ], 0 };
         str_append( string, ch );
      }
      str_append( string, "\"" );
      break;
   case TK_LIT_CHAR:
      str_append( string, "'" );
      str_append( string, token->text );
      str_append( string, "'" );
      break;
   case TK_LIT_OCTAL:
      str_append( string, "0o" );
      str_append( string, token->text );
      break;
   case TK_LIT_HEX:
      str_append( string, "0x" );
      str_append( string, token->text );
      break;
   case TK_LIT_BINARY:
      str_append( string, "0b" );
      str_append( string, token->text );
      break;
   case TK_LIT_RADIX:
      for ( int i = 0; i < token->length; ++i ) {
         if ( token->text[ i ] == '_' ) {
            str_append( string, "r" );
         }
         else {
            char ch[] = { token->text[ i ], 0 };
            str_append( string, ch );
         }
      }
      break;
   default:
      str_append( string, token->text );
   }
}
