#include <string.h>
#include <ctype.h>

#include "phase.h"

static enum tk get_uniformtoken_type( const char* text );
static enum tk get_idtoken_type( const char* text );
static bool is_id_token( const char* text );
static bool is_decimal_literal_token( const char* text );
static bool is_octal_literal_token( const char* text );
static bool is_hex_literal_token( const char* text );
static bool is_bin_literal_token( const char* text );

static const struct {
   char* text;
   enum tk type;
} g_uniforms[] = {
   { "[", TK_BRACKET_L },
   { "]", TK_BRACKET_R },
   { "(", TK_PAREN_L },
   { ")", TK_PAREN_R },
   { "{", TK_BRACE_L },
   { "}", TK_BRACE_R },
   { ".", TK_DOT },
   { ",", TK_COMMA },
   { ":", TK_COLON },
   { "++", TK_INC },
   { "--", TK_DEC },
   { ";", TK_SEMICOLON },
   { "=", TK_ASSIGN },
   { "+=", TK_ASSIGN_ADD },
   { "-=", TK_ASSIGN_SUB },
   { "*=", TK_ASSIGN_MUL },
   { "/=", TK_ASSIGN_DIV },
   { "%=", TK_ASSIGN_MOD },
   { "<<=", TK_ASSIGN_SHIFT_L },
   { ">>=", TK_ASSIGN_SHIFT_R },
   { "&=", TK_ASSIGN_BIT_AND },
   { "^=", TK_ASSIGN_BIT_XOR },
   { "|=", TK_ASSIGN_BIT_OR },
   { ":=", TK_ASSIGN_COLON },
   { "==", TK_EQ },
   { "!=", TK_NEQ },
   { "!", TK_LOG_NOT },
   { "&&", TK_LOG_AND },
   { "||", TK_LOG_OR },
   { "&", TK_BIT_AND },
   { "|", TK_BIT_OR },
   { "^", TK_BIT_XOR },
   { "~", TK_BIT_NOT },
   { "<", TK_LT },
   { "<=", TK_LTE },
   { ">", TK_GT },
   { ">=", TK_GTE },
   { "+", TK_PLUS },
   { "-", TK_MINUS },
   { "/", TK_SLASH },
   { "*", TK_STAR },
   { "%", TK_MOD },
   { "<<", TK_SHIFT_L },
   { ">>", TK_SHIFT_R },
   { "#", TK_HASH },
   { NULL, TK_END }
};
static const struct {
   char* text;
   enum tk type;
} g_reserved_ids[] = {
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
   // Maybe we'll add this as a type later.
   { "fixed", TK_RESERVED },
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
   { "net", TK_NET },
   { "null", TK_NULL },
   { "objcpy", TK_OBJCPY },
   { "open", TK_OPEN },
   { "pickup", TK_PICKUP },
   { "private", TK_PRIVATE },
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
   { "void", TK_VOID },
   { "while", TK_WHILE },
   { "whitereturn", TK_WHITE_RETURN },
   { "world", TK_WORLD },
   { "zbool", TK_ZBOOL },
   { "zfixed", TK_ZFIXED },
   { "zint", TK_ZINT },
   { "zraw", TK_ZRAW },
   { "zstr", TK_ZSTR },
   { NULL, TK_END }
};

// NOTE: This function does not identify all token types. It only identifies
// those tokens that can be produced when two preprocessing tokens are
// concatenated. We do this so we don't waste time and code for checking a
// token that is impossible to produce.
enum tk p_identify_token_type( const char* text ) {
   enum tk type = get_uniformtoken_type( text );
   if ( type != TK_NONE ) {
      return type;
   }
   if ( is_id_token( text ) ) {
      enum tk type = get_idtoken_type( text );
      if ( type == TK_NONE ) {
         return TK_ID;
      }
      else {
         return type;
      }
   }
   if ( is_decimal_literal_token( text ) ) {
      return TK_LIT_DECIMAL;
   }
   if ( is_octal_literal_token( text ) ) {
      return TK_LIT_OCTAL;
   }
   if ( is_hex_literal_token( text ) ) {
      return TK_LIT_HEX;
   }
   if ( is_bin_literal_token( text ) ) {
      return TK_LIT_BINARY;
   }
   if ( text[ 0 ] == '#' && text[ 1 ] == '#' && text[ 2 ] == 0 ) {
      return TK_PREP_HASHHASH;
   }
   return TK_NONE;
}

enum tk get_uniformtoken_type( const char* text ) {
   for ( int i = 0; g_uniforms[ i ].text; ++i ) {
      if ( strcmp( text, g_uniforms[ i ].text ) == 0 ) {
         return g_uniforms[ i ].type;
      }
   }
   return TK_NONE;
}

bool is_id_token( const char* text ) {
   if ( isalpha( *text ) || *text == '_' ) {
      ++text;
      while ( isalnum( *text ) || *text == '_' ) {
         ++text;
      }
      return ( *text == 0 );
   }
   else {
      return false;
   }
}

enum tk get_idtoken_type( const char* text ) {
   for ( int i = 0; g_reserved_ids[ i ].text; ++i ) {
      if ( strcmp( text, g_reserved_ids[ i ].text ) == 0 ) {
         return g_reserved_ids[ i ].type;
      }
   }
   return TK_NONE;
}

bool is_decimal_literal_token( const char* text ) {
   if ( *text == '0' ) {
      return ( *( text + 1 ) == 0 );
   }
   else if ( *text >= '1' && *text <= '9' ) {
      ++text;
      while ( *text >= '0' && *text <= '9' ) {
         ++text;
      }
      return ( *text == 0 );
   }
   else {
      return false;
   }
}

bool is_octal_literal_token( const char* text ) {
   if ( *text == '0' && *( text + 1 ) != 0 ) {
      while ( *text >= '0' && *text <= '7' ) {
         ++text;
      }
      return ( *text == 0 );
   }
   else {
      return false;
   }
}

bool is_hex_literal_token( const char* text ) {
   if ( text[ 0 ] == '0' &&
      ( text[ 1 ] == 'x' || text[ 1 ] == 'X' ) && text[ 2 ] != 0 ) {
      text += 2;
      while (
         ( *text >= '0' && *text <= '7' ) ||
         ( *text >= 'a' && *text <= 'f' ) ||
         ( *text >= 'A' && *text <= 'F' ) ) {
         ++text;
      }
      return ( *text == 0 );
   }
   else {
      return false;
   }
}

bool is_bin_literal_token( const char* text ) {
   if ( text[ 0 ] == '0' &&
      ( text[ 1 ] == 'b' || text[ 1 ] == 'B' ) && text[ 2 ] != 0 ) {
      text += 2;
      while ( *text == '0' || *text == '1' ) {
         ++text;
      }
      return ( *text == 0 );
   }
   else {
      return false;
   }
}

int p_identify_predef_macro( const char* text ) {
   static const struct {
      const char* text;
      int macro;
   } table[] = {
      { "__line__", PREDEFMACROEXPAN_LINE },
      { "__file__", PREDEFMACROEXPAN_FILE },
      { "__time__", PREDEFMACROEXPAN_TIME },
      { "__date__", PREDEFMACROEXPAN_DATE },
      { NULL, PREDEFMACROEXPAN_NONE }
   };
   int i = 0;
   while ( table[ i ].text != NULL &&
      strcmp( text, table[ i ].text ) != 0 ) {
      ++i;
   }
   return table[ i ].macro;
}

const struct token_info* p_get_token_info( enum tk tk ) {
   // The table below contains information about the available tokens. The
   // order of the table corresponds to the order of the token enumeration.
   #define ENTRY( text, flags ) \
      { text, ARRAY_SIZE( text ) - 1, flags }
   #define BLANK ""
   static struct token_info table[] = {
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
      ENTRY( ":=", TKF_NONE ),
      ENTRY( "break", TKF_KEYWORD ),
      ENTRY( "case", TKF_KEYWORD ),

      // 50
      ENTRY( "const", TKF_KEYWORD ),
      ENTRY( "continue", TKF_KEYWORD ),
      ENTRY( "default", TKF_KEYWORD ),
      ENTRY( "do", TKF_KEYWORD ),
      ENTRY( "else", TKF_KEYWORD ),
      ENTRY( "enum", TKF_KEYWORD ),
      ENTRY( "for", TKF_KEYWORD ),
      ENTRY( "if", TKF_KEYWORD ),
      ENTRY( "int", TKF_KEYWORD ),
      ENTRY( "return", TKF_KEYWORD ),

      // 60
      ENTRY( "static", TKF_KEYWORD ),
      ENTRY( "str", TKF_KEYWORD ),
      ENTRY( "struct", TKF_KEYWORD ),
      ENTRY( "switch", TKF_KEYWORD ),
      ENTRY( "void", TKF_KEYWORD ),
      ENTRY( "while", TKF_KEYWORD ),
      ENTRY( "bool", TKF_KEYWORD ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),

      // 70
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( "#", TKF_NONE ),
      ENTRY( "createtranslation", TKF_KEYWORD ),
      ENTRY( "global", TKF_KEYWORD ),
      ENTRY( "script", TKF_KEYWORD ),
      ENTRY( "until", TKF_KEYWORD ),
      ENTRY( "world", TKF_KEYWORD ),
      ENTRY( "open", TKF_KEYWORD ),

      // 80
      ENTRY( "respawn", TKF_KEYWORD ),
      ENTRY( "death", TKF_KEYWORD ),
      ENTRY( "enter", TKF_KEYWORD ),
      ENTRY( "pickup", TKF_KEYWORD ),
      ENTRY( "bluereturn", TKF_KEYWORD ),
      ENTRY( "redreturn", TKF_KEYWORD ),
      ENTRY( "whitereturn", TKF_KEYWORD ),
      ENTRY( "lightning", TKF_KEYWORD ),
      ENTRY( "disconnect", TKF_KEYWORD ),
      ENTRY( "unloading", TKF_KEYWORD ),

      // 90
      ENTRY( "clientside", TKF_KEYWORD ),
      ENTRY( "net", TKF_KEYWORD ),
      ENTRY( "restart", TKF_KEYWORD ),
      ENTRY( "suspend", TKF_KEYWORD ),
      ENTRY( "terminate", TKF_KEYWORD ),
      ENTRY( "function", TKF_KEYWORD ),
      ENTRY( "import", TKF_KEYWORD ),
      ENTRY( "goto", TKF_KEYWORD ),
      ENTRY( "true", TKF_KEYWORD ),
      ENTRY( "false", TKF_KEYWORD ),

      // 100
      ENTRY( "event", TKF_KEYWORD ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( BLANK, TKF_NONE ),
      ENTRY( "?", TKF_KEYWORD ),
      ENTRY( " ", TKF_KEYWORD ),
      ENTRY( "\t", TKF_KEYWORD ),
      ENTRY( "...", TKF_KEYWORD ),
      ENTRY( " ", TKF_NONE ),

      // 110
      ENTRY( "##", TKF_NONE ),
      ENTRY( "zraw", TKF_KEYWORD ),
      ENTRY( "zint", TKF_KEYWORD ),
      ENTRY( "zfixed", TKF_KEYWORD ),
      ENTRY( "zbool", TKF_KEYWORD ),
      ENTRY( "zstr", TKF_KEYWORD ),
      ENTRY( "cast", TKF_KEYWORD ),
      ENTRY( "assert", TKF_KEYWORD ),
      ENTRY( "ref", TKF_KEYWORD ),
      ENTRY( "auto", TKF_KEYWORD ),

      // 120
      ENTRY( "typedef", TKF_KEYWORD ),
      ENTRY( "foreach", TKF_KEYWORD ),
      ENTRY( "private", TKF_KEYWORD ),
      ENTRY( "objcpy", TKF_KEYWORD ),
      ENTRY( "msgbuild", TKF_KEYWORD ),
      ENTRY( "extspec", TKF_KEYWORD ),
      ENTRY( "in", TKF_KEYWORD ),
      ENTRY( "null", TKF_KEYWORD ),
      ENTRY( "special", TKF_KEYWORD ),

      // Invalid entry.
      // This entry should not be reached when all tokens are acccounted for.
      ENTRY( BLANK, TKF_NONE )
   };
   #undef ENTRY
   if ( tk < ARRAY_SIZE( table ) - 1 ) {
      return &table[ tk ];
   }
   else {
      UNREACHABLE();
      return &table[ ARRAY_SIZE( table ) - 1 ];
   }
}

const char* p_get_token_name( enum tk tk ) {
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
      { TK_ASSIGN_COLON, "`:=`" },
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
      { TK_IMPORT, "`import`" },
      { TK_GOTO, "`goto`" },
      { TK_TRUE, "`true`" },
      { TK_FALSE, "`false`" },
      { TK_IMPORT, "`import`" },
      { TK_EVENT, "`event`" },
      { TK_LIT_OCTAL, "octal number" },
      { TK_LIT_DECIMAL, "decimal number" },
      { TK_LIT_HEX, "hexadecimal number" },
      { TK_LIT_BINARY, "binary number" },
      { TK_LIT_FIXED, "fixed-point number" },
      { TK_NL, "end-of-line" },
      { TK_END, "end-of-input" },
      { TK_LIB, "start-of-library" },
      { TK_LIB_END, "end-of-library" },
      { TK_QUESTION_MARK, "`?`" },
      { TK_ELLIPSIS, "`...`" },
      { TK_HORZSPACE, "horizontal space" },
      { TK_PREP_HASHHASH, "`##`" },
      { TK_STRCPY, "`strcpy`" },
      { TK_ZRAW, "`zraw`" },
      { TK_ZINT, "`zint`" },
      { TK_ZFIXED, "`zfixed`" },
      { TK_ZBOOL, "`zbool`" },
      { TK_ZSTR, "`zstr`" },
      { TK_CAST, "`cast`" },
      { TK_ASSERT, "`assert`" },
      { TK_REF, "`ref`" },
      { TK_AUTO, "`auto`" },
      { TK_TYPEDEF, "`typedef`" },
      { TK_FOREACH, "`foreach`" },
      { TK_PRIVATE, "`private`" },
      { TK_OBJCPY, "`objcpy`" },
      { TK_MSGBUILD, "`msgbuild`" },
      { TK_EXTSPEC, "`extspec`" },
      { TK_IN, "`in`" },
      { TK_NULL, "`null`" },
      { TK_SPECIAL, "`special`" }, };
   STATIC_ASSERT( TK_TOTAL == 129 );
   switch ( tk ) {
   case TK_LIT_STRING:
      return "string literal";
   case TK_LIT_CHAR:
      return "character literal";
   case TK_ID:
      return "identifier";
   default:
      for ( size_t i = 0; i < ARRAY_SIZE( names ); ++i ) {
         if ( names[ i ].tk == tk ) {
            return names[ i ].name;
         }
      }
      return "";
   }
}