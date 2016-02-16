#include "phase.h"

static void output_source( struct parse* parse, struct str* output );
static void output_token( struct parse* parse, struct str* output );

void p_preprocess( struct parse* parse ) {
   p_load_main_source( parse );


/*
   while ( true ) {
      //printf( "peeked %d %s\n", p_peek_stream( parse )->type, p_peek_stream( parse )->text );
      //printf( "peeked 2nd %d %s\n", p_peek_stream_2nd( parse )->type, p_peek_stream_2nd( parse )->text );
      p_read_tk( parse );
      if ( parse->token->type == TK_END ) {
         break;
      }
      printf( "%d %d %s\n", parse->token->type, parse->token->length, parse->token->text );
   }
   p_bail( parse ); */

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
}

int line = 1;
int column = 0;

void output_token( struct parse* parse, struct str* output ) {
/*
   switch ( parse->token->type ) {
   case TK_NL:
   case TK_HORZSPACE:
      return;
   default:
      break;
   }
   if ( line > parse->token->pos.line ) {
         str_append( output, " " );
         ++column;
   }
   else {
      while ( line < parse->token->pos.line ) {
         str_append( output, NEWLINE_CHAR );
         column = 0;
         ++line;
      }
      while ( column < parse->token->pos.column ) {
         str_append( output, " " );
         ++column;
      }
   column += parse->token->length;
   } */
/*
   if ( ! parse->macro_expan ) {
      while ( parse->line ) {
         str_append( output, NEWLINE_CHAR );
         --parse->line;
      }
      while ( parse->column ) {
         str_append( output, " " );
         --parse->column;
      }
   }
*/
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
      str_append( output, parse->token->text );
      str_append( output, "\"" );
      break;
   case TK_LIT_CHAR:
      str_append( output, "'" );
      str_append( output, parse->token->text );
      str_append( output, "'" );
      break;
   //case TK_NL:
   //case TK_HORZSPACE:
   //   break;
   default:
      str_append( output, parse->token->text );
      break;
   }
}