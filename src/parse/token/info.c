#include "phase.h"

// The table below contains information about the available tokens. The order
// of the table corresponds to the order of the token enumeration.
#define ENTRY( text, flags ) \
   { text, ARRAY_SIZE( text ) - 1, flags }
#define BLANK ""
static struct token_info g_table[] = {
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
   ENTRY( "break", TKF_KEYWORD ),
   ENTRY( "case", TKF_KEYWORD ),
   ENTRY( "const", TKF_KEYWORD ),
   // 50
   ENTRY( "continue", TKF_KEYWORD ),
   ENTRY( "default", TKF_KEYWORD ),
   ENTRY( "do", TKF_KEYWORD ),
   ENTRY( "else", TKF_KEYWORD ),
   ENTRY( "enum", TKF_KEYWORD ),
   ENTRY( "for", TKF_KEYWORD ),
   ENTRY( "if", TKF_KEYWORD ),
   ENTRY( "int", TKF_KEYWORD ),
   ENTRY( "return", TKF_KEYWORD ),
   ENTRY( "static", TKF_KEYWORD ),
   // 60
   ENTRY( "str", TKF_KEYWORD ),
   ENTRY( "struct", TKF_KEYWORD ),
   ENTRY( "switch", TKF_KEYWORD ),
   ENTRY( "void", TKF_KEYWORD ),
   ENTRY( "while", TKF_KEYWORD ),
   ENTRY( "bool", TKF_KEYWORD ),
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( BLANK, TKF_NONE ),
   // 70
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( "#", TKF_NONE ),
   ENTRY( "createtranslation", TKF_KEYWORD ),
   ENTRY( "global", TKF_KEYWORD ),
   ENTRY( "script", TKF_KEYWORD ),
   ENTRY( "until", TKF_KEYWORD ),
   ENTRY( "world", TKF_KEYWORD ),
   ENTRY( "open", TKF_KEYWORD ),
   ENTRY( "respawn", TKF_KEYWORD ),
   // 80
   ENTRY( "death", TKF_KEYWORD ),
   ENTRY( "enter", TKF_KEYWORD ),
   ENTRY( "pickup", TKF_KEYWORD ),
   ENTRY( "bluereturn", TKF_KEYWORD ),
   ENTRY( "redreturn", TKF_KEYWORD ),
   ENTRY( "whitereturn", TKF_KEYWORD ),
   ENTRY( "lightning", TKF_KEYWORD ),
   ENTRY( "disconnect", TKF_KEYWORD ),
   ENTRY( "unloading", TKF_KEYWORD ),
   ENTRY( "clientside", TKF_KEYWORD ),
   // 90
   ENTRY( "net", TKF_KEYWORD ),
   ENTRY( "restart", TKF_KEYWORD ),
   ENTRY( "suspend", TKF_KEYWORD ),
   ENTRY( "terminate", TKF_KEYWORD ),
   ENTRY( "function", TKF_KEYWORD ),
   ENTRY( "import", TKF_KEYWORD ),
   ENTRY( "goto", TKF_KEYWORD ),
   ENTRY( "true", TKF_KEYWORD ),
   ENTRY( "false", TKF_KEYWORD ),
   ENTRY( "event", TKF_KEYWORD ),
   // 100
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( "?", TKF_NONE ),
   ENTRY( " ", TKF_NONE ),
   ENTRY( "\t", TKF_NONE ),
   ENTRY( "...", TKF_NONE ),
   ENTRY( " ", TKF_NONE ),
   ENTRY( "##", TKF_NONE ),
   // 110
   ENTRY( "raw", TKF_KEYWORD ),
   ENTRY( "fixed", TKF_KEYWORD ),
   ENTRY( "assert", TKF_KEYWORD ),
   ENTRY( "auto", TKF_KEYWORD ),
   ENTRY( "typedef", TKF_KEYWORD ),
   ENTRY( "foreach", TKF_KEYWORD ),
   ENTRY( "private", TKF_KEYWORD ),
   ENTRY( "memcpy", TKF_KEYWORD ),
   ENTRY( "msgbuild", TKF_KEYWORD ),
   ENTRY( "null", TKF_KEYWORD ),
   // 120
   ENTRY( "special", TKF_KEYWORD ),
   ENTRY( "namespace", TKF_KEYWORD ),
   ENTRY( "upmost", TKF_KEYWORD ),
   ENTRY( "using", TKF_KEYWORD ),
   ENTRY( "include", TKF_KEYWORD ),
   ENTRY( "define", TKF_KEYWORD ),
   ENTRY( "libdefine", TKF_KEYWORD ),
   ENTRY( "print", TKF_KEYWORD ),
   ENTRY( "printbold", TKF_KEYWORD ),
   ENTRY( "wadauthor", TKF_KEYWORD ),
   // 130
   ENTRY( "nowadauthor", TKF_KEYWORD ),
   ENTRY( "nocompact", TKF_KEYWORD ),
   ENTRY( "library", TKF_KEYWORD ),
   ENTRY( "encryptstrings", TKF_KEYWORD ),
   ENTRY( "region", TKF_KEYWORD ),
   ENTRY( "endregion", TKF_KEYWORD ),
   ENTRY( "log", TKF_KEYWORD ),
   ENTRY( "hudmessage", TKF_KEYWORD ),
   ENTRY( "hudmessagebold", TKF_KEYWORD ),
   ENTRY( "strparam", TKF_KEYWORD ),
   // 140
   ENTRY( "extern", TKF_KEYWORD ),
   ENTRY( "acs_executewait", TKF_KEYWORD ),
   ENTRY( "acs_namedexecutewait", TKF_KEYWORD ),
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( BLANK, TKF_NONE ),
   ENTRY( "kill", TKF_KEYWORD ),
   ENTRY( "\\", TKF_NONE ),
   ENTRY( "reopen", TKF_KEYWORD ),
   ENTRY( "let", TKF_KEYWORD ),
   ENTRY( "strict", TKF_KEYWORD ),
};
#undef ENTRY

const struct token_info* p_get_token_info( enum tk tk ) {
   STATIC_ASSERT( TK_TOTAL == 150 );
   return &g_table[ tk ];
}

void p_present_token( struct str* str, enum tk tk ) {
   STATIC_ASSERT( TK_TOTAL == 150 );
   switch ( tk ) {
   case TK_ID:
      str_append( str,
         "identifier" );
      break;
   case TK_LIT_DECIMAL:
      str_append( str,
         "decimal number" );
      break;
   case TK_LIT_OCTAL:
      str_append( str,
         "octal number" );
      break;
   case TK_LIT_HEX:
      str_append( str,
         "hexadecimal number" );
      break;
   case TK_LIT_FIXED:
      str_append( str,
         "fixed-point number" );
      break;
   case TK_LIT_STRING:
      str_append( str,
         "string literal" );
      break;
   case TK_LIT_CHAR:
      str_append( str,
         "character literal" );
      break;
   case TK_NL:
      str_append( str,
         "end-of-line" );
      break;
   case TK_END:
      str_append( str,
         "end-of-input" );
      break;
   case TK_LIB:
      str_append( str,
         "start-of-library" );
      break;
   case TK_LIB_END:
      str_append( str,
         "end-of-library" );
      break;
   case TK_LIT_BINARY:
      str_append( str,
         "binary number" );
      break;
   case TK_HORZSPACE:
      str_append( str,
         "horizontal space" );
      break;
   case TK_LIT_RADIX:
      str_append( str,
         "radix number" );
      break;
   case TK_TYPENAME:
      str_append( str,
         "type name (an identifier that ends with either "
         "\"<lowercase-letter>T\" or \"_T\")" );
      break;
   default:
      str_append( str, "`" );
      str_append( str, g_table[ tk ].shared_text );
      str_append( str, "`" );
      if ( g_table[ tk ].flags & TKF_KEYWORD ) {
         str_append( str, " " );
         str_append( str, "keyword" );
      }
   }
}

const char* p_present_token_temp( struct parse* parse, enum tk tk ) {
   str_clear( &parse->token_presentation );
   p_present_token( &parse->token_presentation, tk );
   return parse->token_presentation.value;
}