#include <string.h>

#include "phase.h"
#include "codegen/phase.h"
#include "codegen/pcode.h"

struct test {
   struct stmt_test* stmt_test;
   struct inline_asm* inline_asm;
   const char* format;
   const char* repeat;
};

struct mnemonic {
   const char* name;
   int opcode;
};

static void test_name( struct semantic* semantic, struct test* test );
static const struct mnemonic* find_mnemonic( const char* name );
static void test_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg );
static void test_label_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg );
static void test_var_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg );
static void test_func_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg );
static void test_expr_arg( struct semantic* semantic,
   struct inline_asm_arg* arg );

void p_test_inline_asm( struct semantic* semantic,
   struct stmt_test* stmt_test, struct inline_asm* inline_asm ) {
   struct test test = { .stmt_test = stmt_test, .inline_asm = inline_asm };
   test_name( semantic, &test );
   list_iter_t i;
   list_iter_init( &i, &inline_asm->args );
   while ( ! list_end( &i ) ) {
      test_arg( semantic, &test, list_data( &i ) );
      list_next( &i );
   }
   if ( *test.format && ! test.repeat ) {
      s_diag( semantic, DIAG_POS_ERR, &inline_asm->pos,
         "too little arguments for `%s` instruction",
         inline_asm->name );
      s_bail( semantic );
   }
}

void test_name( struct semantic* semantic, struct test* test ) {
   const struct mnemonic* mnemonic = find_mnemonic( test->inline_asm->name );
   if ( ! mnemonic ) {
      s_diag( semantic, DIAG_POS_ERR, &test->inline_asm->pos,
         "`%s` instruction not found", test->inline_asm->name );
      s_bail( semantic );
   }
   struct pcode* instruction = c_get_pcode_info( mnemonic->opcode );
   test->format = instruction->args_format;
   test->inline_asm->opcode = mnemonic->opcode;
}

const struct mnemonic* find_mnemonic( const char* name ) {
   STATIC_ASSERT( PCD_TOTAL == 383 );
   static const struct mnemonic table[] = {
      { "activatorsound", PCD_ACTIVATORSOUND },
      { "activatortid", PCD_ACTIVATORTID },
      { "add", PCD_ADD },
      { "addglobalarray", PCD_ADDGLOBALARRAY },
      { "addglobalvar", PCD_ADDGLOBALVAR },
      { "addmaparray", PCD_ADDMAPARRAY },
      { "addmapvar", PCD_ADDMAPVAR },
      { "addscriptarray", PCD_ADDSCRIPTARRAY },
      { "addscriptvar", PCD_ADDSCRIPTVAR },
      { "addworldarray", PCD_ADDWORLDARRAY },
      { "addworldvar", PCD_ADDWORLDVAR },
      { "ambientsound", PCD_AMBIENTSOUND },
      { "andbitwise", PCD_ANDBITWISE },
      { "andglobalarray", PCD_ANDGLOBALARRAY },
      { "andglobalvar", PCD_ANDGLOBALVAR },
      { "andlogical", PCD_ANDLOGICAL },
      { "andmaparray", PCD_ANDMAPARRAY },
      { "andmapvar", PCD_ANDMAPVAR },
      { "andscriptarray", PCD_ANDSCRIPTARRAY },
      { "andscriptvar", PCD_ANDSCRIPTVAR },
      { "andworldarray", PCD_ANDWORLDARRAY },
      { "andworldvar", PCD_ANDWORLDVAR },
      { "assignglobalarray", PCD_ASSIGNGLOBALARRAY },
      { "assignglobalvar", PCD_ASSIGNGLOBALVAR },
      { "assignmaparray", PCD_ASSIGNMAPARRAY },
      { "assignmapvar", PCD_ASSIGNMAPVAR },
      { "assignscriptarray", PCD_ASSIGNSCRIPTARRAY },
      { "assignscriptvar", PCD_ASSIGNSCRIPTVAR },
      { "assignworldarray", PCD_ASSIGNWORLDARRAY },
      { "assignworldvar", PCD_ASSIGNWORLDVAR },
      { "beginprint", PCD_BEGINPRINT },
      { "blueteamcount", PCD_BLUETEAMCOUNT },
      { "blueteamscore", PCD_BLUETEAMSCORE },
      { "call", PCD_CALL },
      { "calldiscard", PCD_CALLDISCARD },
      { "callfunc", PCD_CALLFUNC },
      { "callstack", PCD_CALLSTACK },
      { "cancelfade", PCD_CANCELFADE },
      { "casegoto", PCD_CASEGOTO },
      { "casegotosorted", PCD_CASEGOTOSORTED },
      { "changeceiling", PCD_CHANGECEILING },
      { "changeceilingdirect", PCD_CHANGECEILINGDIRECT },
      { "changefloor", PCD_CHANGEFLOOR },
      { "changefloordirect", PCD_CHANGEFLOORDIRECT },
      { "changelevel", PCD_CHANGELEVEL },
      { "changesky", PCD_CHANGESKY },
      { "checkactorceilingtexture", PCD_CHECKACTORCEILINGTEXTURE },
      { "checkactorfloortexture", PCD_CHECKACTORFLOORTEXTURE },
      { "checkactorinventory", PCD_CHECKACTORINVENTORY },
      { "checkinventory", PCD_CHECKINVENTORY },
      { "checkinventorydirect", PCD_CHECKINVENTORYDIRECT },
      { "checkplayercamera", PCD_CHECKPLAYERCAMERA },
      { "checkweapon", PCD_CHECKWEAPON },
      { "classifyactor", PCD_CLASSIFYACTOR },
      { "clearactorinventory", PCD_CLEARACTORINVENTORY },
      { "clearinventory", PCD_CLEARINVENTORY },
      { "clearlinespecial", PCD_CLEARLINESPECIAL },
      { "consolecommand", PCD_CONSOLECOMMAND },
      { "consolecommanddirect", PCD_CONSOLECOMMANDDIRECT },
      { "cos", PCD_COS },
      { "decglobalarray", PCD_DECGLOBALARRAY },
      { "decglobalvar", PCD_DECGLOBALVAR },
      { "decmaparray", PCD_DECMAPARRAY },
      { "decmapvar", PCD_DECMAPVAR },
      { "decscriptarray", PCD_DECSCRIPTARRAY },
      { "decscriptvar", PCD_DECSCRIPTVAR },
      { "decworldarray", PCD_DECWORLDARRAY },
      { "decworldvar", PCD_DECWORLDVAR },
      { "delay", PCD_DELAY },
      { "delaydirect", PCD_DELAYDIRECT },
      { "delaydirectb", PCD_DELAYDIRECTB },
      { "divglobalarray", PCD_DIVGLOBALARRAY },
      { "divglobalvar", PCD_DIVGLOBALVAR },
      { "divide", PCD_DIVIDE },
      { "divmaparray", PCD_DIVMAPARRAY },
      { "divmapvar", PCD_DIVMAPVAR },
      { "divscriptarray", PCD_DIVSCRIPTARRAY },
      { "divscriptvar", PCD_DIVSCRIPTVAR },
      { "divworldarray", PCD_DIVWORLDARRAY },
      { "divworldvar", PCD_DIVWORLDVAR },
      { "drop", PCD_DROP },
      { "dup", PCD_DUP },
      { "endhudmessage", PCD_ENDHUDMESSAGE },
      { "endhudmessagebold", PCD_ENDHUDMESSAGEBOLD },
      { "endlog", PCD_ENDLOG },
      { "endprint", PCD_ENDPRINT },
      { "endprintbold", PCD_ENDPRINTBOLD },
      { "endtranslation", PCD_ENDTRANSLATION },
      { "eorbitwise", PCD_EORBITWISE },
      { "eorglobalarray", PCD_EORGLOBALARRAY },
      { "eorglobalvar", PCD_EORGLOBALVAR },
      { "eormaparray", PCD_EORMAPARRAY },
      { "eormapvar", PCD_EORMAPVAR },
      { "eorscriptarray", PCD_EORSCRIPTARRAY },
      { "eorscriptvar", PCD_EORSCRIPTVAR },
      { "eorworldarray", PCD_EORWORLDARRAY },
      { "eorworldvar", PCD_EORWORLDVAR },
      { "eq", PCD_EQ },
      { "faderange", PCD_FADERANGE },
      { "fadeto", PCD_FADETO },
      { "fixeddiv", PCD_FIXEDDIV },
      { "fixedmul", PCD_FIXEDMUL },
      { "gameskill", PCD_GAMESKILL },
      { "gametype", PCD_GAMETYPE },
      { "ge", PCD_GE },
      { "getactorangle", PCD_GETACTORANGLE },
      { "getactorceilingz", PCD_GETACTORCEILINGZ },
      { "getactorfloorz", PCD_GETACTORFLOORZ },
      { "getactorlightlevel", PCD_GETACTORLIGHTLEVEL },
      { "getactorpitch", PCD_GETACTORPITCH },
      { "getactorproperty", PCD_GETACTORPROPERTY },
      { "getactorx", PCD_GETACTORX },
      { "getactory", PCD_GETACTORY },
      { "getactorz", PCD_GETACTORZ },
      { "getammocapacity", PCD_GETAMMOCAPACITY },
      { "getcvar", PCD_GETCVAR },
      { "getinvasionstate", PCD_GETINVASIONSTATE },
      { "getinvasionwave", PCD_GETINVASIONWAVE },
      { "getlevelinfo", PCD_GETLEVELINFO },
      { "getlinerowoffset", PCD_GETLINEROWOFFSET },
      { "getplayerinfo", PCD_GETPLAYERINFO },
      { "getplayerinput", PCD_GETPLAYERINPUT },
      { "getscreenheight", PCD_GETSCREENHEIGHT },
      { "getscreenwidth", PCD_GETSCREENWIDTH },
      { "getsectorceilingz", PCD_GETSECTORCEILINGZ },
      { "getsectorfloorz", PCD_GETSECTORFLOORZ },
      { "getsectorlightlevel", PCD_GETSECTORLIGHTLEVEL },
      { "getsigilpieces", PCD_GETSIGILPIECES },
      { "giveactorinventory", PCD_GIVEACTORINVENTORY },
      { "giveinventory", PCD_GIVEINVENTORY },
      { "giveinventorydirect", PCD_GIVEINVENTORYDIRECT },
      { "goto_", PCD_GOTO },
      { "gotostack", PCD_GOTOSTACK },
      { "gt", PCD_GT },
      { "ifgoto", PCD_IFGOTO },
      { "ifnotgoto", PCD_IFNOTGOTO },
      { "incglobalarray", PCD_INCGLOBALARRAY },
      { "incglobalvar", PCD_INCGLOBALVAR },
      { "incmaparray", PCD_INCMAPARRAY },
      { "incmapvar", PCD_INCMAPVAR },
      { "incscriptarray", PCD_INCSCRIPTARRAY },
      { "incscriptvar", PCD_INCSCRIPTVAR },
      { "incworldarray", PCD_INCWORLDARRAY },
      { "incworldvar", PCD_INCWORLDVAR },
      { "isoneflagctf", PCD_ISONEFLAGCTF },
      { "le", PCD_LE },
      { "lineside", PCD_LINESIDE },
      { "localambientsound", PCD_LOCALAMBIENTSOUND },
      { "localsetmusic", PCD_LOCALSETMUSIC },
      { "localsetmusicdirect", PCD_LOCALSETMUSICDIRECT },
      { "lsglobalarray", PCD_LSGLOBALARRAY },
      { "lsglobalvar", PCD_LSGLOBALVAR },
      { "lshift", PCD_LSHIFT },
      { "lsmaparray", PCD_LSMAPARRAY },
      { "lsmapvar", PCD_LSMAPVAR },
      { "lspec1", PCD_LSPEC1 },
      { "lspec1direct", PCD_LSPEC1DIRECT },
      { "lspec1directb", PCD_LSPEC1DIRECTB },
      { "lspec2", PCD_LSPEC2 },
      { "lspec2direct", PCD_LSPEC2DIRECT },
      { "lspec2directb", PCD_LSPEC2DIRECTB },
      { "lspec3", PCD_LSPEC3 },
      { "lspec3direct", PCD_LSPEC3DIRECT },
      { "lspec3directb", PCD_LSPEC3DIRECTB },
      { "lspec4", PCD_LSPEC4 },
      { "lspec4direct", PCD_LSPEC4DIRECT },
      { "lspec4directb", PCD_LSPEC4DIRECTB },
      { "lspec5", PCD_LSPEC5 },
      { "lspec5direct", PCD_LSPEC5DIRECT },
      { "lspec5directb", PCD_LSPEC5DIRECTB },
      { "lspec5ex", PCD_LSPEC5EX },
      { "lspec5exresult", PCD_LSPEC5EXRESULT },
      { "lspec5result", PCD_LSPEC5RESULT },
      { "lsscriptarray", PCD_LSSCRIPTARRAY },
      { "lsscriptvar", PCD_LSSCRIPTVAR },
      { "lsworldarray", PCD_LSWORLDARRAY },
      { "lsworldvar", PCD_LSWORLDVAR },
      { "lt", PCD_LT },
      { "modglobalarray", PCD_MODGLOBALARRAY },
      { "modglobalvar", PCD_MODGLOBALVAR },
      { "modmaparray", PCD_MODMAPARRAY },
      { "modmapvar", PCD_MODMAPVAR },
      { "modscriptarray", PCD_MODSCRIPTARRAY },
      { "modscriptvar", PCD_MODSCRIPTVAR },
      { "modulus", PCD_MODULUS },
      { "modworldarray", PCD_MODWORLDARRAY },
      { "modworldvar", PCD_MODWORLDVAR },
      { "morehudmessage", PCD_MOREHUDMESSAGE },
      { "morphactor", PCD_MORPHACTOR },
      { "mulglobalarray", PCD_MULGLOBALARRAY },
      { "mulglobalvar", PCD_MULGLOBALVAR },
      { "mulmaparray", PCD_MULMAPARRAY },
      { "mulmapvar", PCD_MULMAPVAR },
      { "mulscriptarray", PCD_MULSCRIPTARRAY },
      { "mulscriptvar", PCD_MULSCRIPTVAR },
      { "multiply", PCD_MULTIPLY },
      { "mulworldarray", PCD_MULWORLDARRAY },
      { "mulworldvar", PCD_MULWORLDVAR },
      { "musicchange", PCD_MUSICCHANGE },
      { "ne", PCD_NE },
      { "negatebinary", PCD_NEGATEBINARY },
      { "negatelogical", PCD_NEGATELOGICAL },
      { "nop", PCD_NONE },
      { "opthudmessage", PCD_OPTHUDMESSAGE },
      { "orbitwise", PCD_ORBITWISE },
      { "orglobalarray", PCD_ORGLOBALARRAY },
      { "orglobalvar", PCD_ORGLOBALVAR },
      { "orlogical", PCD_ORLOGICAL },
      { "ormaparray", PCD_ORMAPARRAY },
      { "ormapvar", PCD_ORMAPVAR },
      { "orscriptarray", PCD_ORSCRIPTARRAY },
      { "orscriptvar", PCD_ORSCRIPTVAR },
      { "orworldarray", PCD_ORWORLDARRAY },
      { "orworldvar", PCD_ORWORLDVAR },
      { "playerarmorpoints", PCD_PLAYERARMORPOINTS },
      { "playerclass", PCD_PLAYERCLASS },
      { "playercount", PCD_PLAYERCOUNT },
      { "playerfrags", PCD_PLAYERFRAGS },
      { "playerhealth", PCD_PLAYERHEALTH },
      { "playeringame", PCD_PLAYERINGAME },
      { "playerisbot", PCD_PLAYERISBOT },
      { "playernumber", PCD_PLAYERNUMBER },
      { "playerteam", PCD_PLAYERTEAM },
      { "playmovie", PCD_PLAYMOVIE },
      { "polywait", PCD_POLYWAIT },
      { "polywaitdirect", PCD_POLYWAITDIRECT },
      { "printbinary", PCD_PRINTBINARY },
      { "printbind", PCD_PRINTBIND },
      { "printcharacter", PCD_PRINTCHARACTER },
      { "printfixed", PCD_PRINTFIXED },
      { "printglobalchararray", PCD_PRINTGLOBALCHARARRAY },
      { "printglobalchrange", PCD_PRINTGLOBALCHRANGE },
      { "printhex", PCD_PRINTHEX },
      { "printlocalized", PCD_PRINTLOCALIZED },
      { "printmapchararray", PCD_PRINTMAPCHARARRAY },
      { "printmapchrange", PCD_PRINTMAPCHRANGE },
      { "printname", PCD_PRINTNAME },
      { "printnumber", PCD_PRINTNUMBER },
      { "printscriptchararray", PCD_PRINTSCRIPTCHARARRAY },
      { "printscriptchrange", PCD_PRINTSCRIPTCHRANGE },
      { "printstring", PCD_PRINTSTRING },
      { "printworldchararray", PCD_PRINTWORLDCHARARRAY },
      { "printworldchrange", PCD_PRINTWORLDCHRANGE },
      { "push2bytes", PCD_PUSH2BYTES },
      { "push3bytes", PCD_PUSH3BYTES },
      { "push4bytes", PCD_PUSH4BYTES },
      { "push5bytes", PCD_PUSH5BYTES },
      { "pushbyte", PCD_PUSHBYTE },
      { "pushbytes", PCD_PUSHBYTES },
      { "pushfunction", PCD_PUSHFUNCTION },
      { "pushglobalarray", PCD_PUSHGLOBALARRAY },
      { "pushglobalvar", PCD_PUSHGLOBALVAR },
      { "pushmaparray", PCD_PUSHMAPARRAY },
      { "pushmapvar", PCD_PUSHMAPVAR },
      { "pushnumber", PCD_PUSHNUMBER },
      { "pushscriptarray", PCD_PUSHSCRIPTARRAY },
      { "pushscriptvar", PCD_PUSHSCRIPTVAR },
      { "pushworldarray", PCD_PUSHWORLDARRAY },
      { "pushworldvar", PCD_PUSHWORLDVAR },
      { "random", PCD_RANDOM },
      { "randomdirect", PCD_RANDOMDIRECT },
      { "randomdirectb", PCD_RANDOMDIRECTB },
      { "redteamcount", PCD_REDTEAMCOUNT },
      { "redteamscore", PCD_REDTEAMSCORE },
      { "replacetextures", PCD_REPLACETEXTURES },
      { "restart_", PCD_RESTART },
      { "returnval", PCD_RETURNVAL },
      { "returnvoid", PCD_RETURNVOID },
      { "rsglobalarray", PCD_RSGLOBALARRAY },
      { "rsglobalvar", PCD_RSGLOBALVAR },
      { "rshift", PCD_RSHIFT },
      { "rsmaparray", PCD_RSMAPARRAY },
      { "rsmapvar", PCD_RSMAPVAR },
      { "rsscriptarray", PCD_RSSCRIPTARRAY },
      { "rsscriptvar", PCD_RSSCRIPTVAR },
      { "rsworldarray", PCD_RSWORLDARRAY },
      { "rsworldvar", PCD_RSWORLDVAR },
      { "savestring", PCD_SAVESTRING },
      { "scriptwait", PCD_SCRIPTWAIT },
      { "scriptwaitdirect", PCD_SCRIPTWAITDIRECT },
      { "scriptwaitnamed", PCD_SCRIPTWAITNAMED },
      { "sectordamage", PCD_SECTORDAMAGE },
      { "sectorsound", PCD_SECTORSOUND },
      { "setactorangle", PCD_SETACTORANGLE },
      { "setactorpitch", PCD_SETACTORPITCH },
      { "setactorposition", PCD_SETACTORPOSITION },
      { "setactorproperty", PCD_SETACTORPROPERTY },
      { "setactorstate", PCD_SETACTORSTATE },
      { "setaircontrol", PCD_SETAIRCONTROL },
      { "setaircontroldirect", PCD_SETAIRCONTROLDIRECT },
      { "setammocapacity", PCD_SETAMMOCAPACITY },
      { "setcameratotexture", PCD_SETCAMERATOTEXTURE },
      { "setceilingtrigger", PCD_SETCEILINGTRIGGER },
      { "setfloortrigger", PCD_SETFLOORTRIGGER },
      { "setfont", PCD_SETFONT },
      { "setfontdirect", PCD_SETFONTDIRECT },
      { "setgravity", PCD_SETGRAVITY },
      { "setgravitydirect", PCD_SETGRAVITYDIRECT },
      { "sethudsize", PCD_SETHUDSIZE },
      { "setlineblocking", PCD_SETLINEBLOCKING },
      { "setlinemonsterblocking", PCD_SETLINEMONSTERBLOCKING },
      { "setlinespecial", PCD_SETLINESPECIAL },
      { "setlinetexture", PCD_SETLINETEXTURE },
      { "setmarinesprite", PCD_SETMARINESPRITE },
      { "setmarineweapon", PCD_SETMARINEWEAPON },
      { "setmugshotstate", PCD_SETMUGSHOTSTATE },
      { "setmusic", PCD_SETMUSIC },
      { "setmusicdirect", PCD_SETMUSICDIRECT },
      { "setresultvalue", PCD_SETRESULTVALUE },
      { "setthingspecial", PCD_SETTHINGSPECIAL },
      { "setweapon", PCD_SETWEAPON },
      { "sin", PCD_SIN },
      { "singleplayer", PCD_SINGLEPLAYER },
      { "soundsequence", PCD_SOUNDSEQUENCE },
      { "spawn", PCD_SPAWN },
      { "spawndirect", PCD_SPAWNDIRECT },
      { "spawnprojectile", PCD_SPAWNPROJECTILE },
      { "spawnspot", PCD_SPAWNSPOT },
      { "spawnspotdirect", PCD_SPAWNSPOTDIRECT },
      { "spawnspotfacing", PCD_SPAWNSPOTFACING },
      { "starttranslation", PCD_STARTTRANSLATION },
      { "strcpytoglobalchrange", PCD_STRCPYTOGLOBALCHRANGE },
      { "strcpytomapchrange", PCD_STRCPYTOMAPCHRANGE },
      { "strcpytoscriptchrange", PCD_STRCPYTOSCRIPTCHRANGE },
      { "strcpytoworldchrange", PCD_STRCPYTOWORLDCHRANGE },
      { "strlen", PCD_STRLEN },
      { "subglobalarray", PCD_SUBGLOBALARRAY },
      { "subglobalvar", PCD_SUBGLOBALVAR },
      { "submaparray", PCD_SUBMAPARRAY },
      { "submapvar", PCD_SUBMAPVAR },
      { "subscriptarray", PCD_SUBSCRIPTARRAY },
      { "subscriptvar", PCD_SUBSCRIPTVAR },
      { "subtract", PCD_SUBTRACT },
      { "subworldarray", PCD_SUBWORLDARRAY },
      { "subworldvar", PCD_SUBWORLDVAR },
      { "suspend_", PCD_SUSPEND },
      { "swap", PCD_SWAP },
      { "tagstring", PCD_TAGSTRING },
      { "tagwait", PCD_TAGWAIT },
      { "tagwaitdirect", PCD_TAGWAITDIRECT },
      { "takeactorinventory", PCD_TAKEACTORINVENTORY },
      { "takeinventory", PCD_TAKEINVENTORY },
      { "takeinventorydirect", PCD_TAKEINVENTORYDIRECT },
      { "terminate_", PCD_TERMINATE },
      { "thingcount", PCD_THINGCOUNT },
      { "thingcountdirect", PCD_THINGCOUNTDIRECT },
      { "thingcountname", PCD_THINGCOUNTNAME },
      { "thingcountnamesector", PCD_THINGCOUNTNAMESECTOR },
      { "thingcountsector", PCD_THINGCOUNTSECTOR },
      { "thingdamage2", PCD_THINGDAMAGE2 },
      { "thingprojectile2", PCD_THINGPROJECTILE2 },
      { "thingsound", PCD_THINGSOUND },
      { "timer", PCD_TIMER },
      { "translationrange1", PCD_TRANSLATIONRANGE1 },
      { "translationrange2", PCD_TRANSLATIONRANGE2 },
      { "translationrange3", PCD_TRANSLATIONRANGE3 },
      { "unaryminus", PCD_UNARYMINUS },
      { "unmorphactor", PCD_UNMORPHACTOR },
      { "useactorinventory", PCD_USEACTORINVENTORY },
      { "useinventory", PCD_USEINVENTORY },
      { "vectorangle", PCD_VECTORANGLE },
   };
   // Use binary search to find mnemonic.
   int left = 0;
   int right = ARRAY_SIZE( table ) - 1;
   while ( left <= right ) {
      int middle = ( left + right ) / 2;
      int result = strcmp( name, table[ middle ].name );
      if ( result > 0 ) {
         left = middle + 1;
      }
      else if ( result < 0 ) {
         right = middle - 1;
      }
      else {
         return &table[ middle ];
      }
   }
   return NULL;
}

void test_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg ) {
   if ( ! *test->format ) {
      s_diag( semantic, DIAG_POS_ERR, &test->inline_asm->pos,
         "too many arguments for `%s` instruction",
         test->inline_asm->name );
      s_bail( semantic );
   }
   // A `+` can appear at the start of a format substring. It states that the
   // current format substring should be reused for all subsequent arguments.
   if ( *test->format == '+' ) {
      test->repeat = test->format;
      ++test->format;
   }
   // Determine argument type. The syntax of the argument is used to identify
   // its type. Each type has a single representation syntax-wise.
   while ( true ) {
      bool match = false;
      bool unknown = false;
      switch ( *test->format ) {
      case 'n': match = ( arg->type == INLINE_ASM_ARG_NUMBER ); break;
      case 'l': match = ( arg->type == INLINE_ASM_ARG_ID ); break;
      case 'v': match = ( arg->type == INLINE_ASM_ARG_ID ); break;
      case 'a': match = ( arg->type == INLINE_ASM_ARG_ID ); break;
      case 'f': match = ( arg->type == INLINE_ASM_ARG_ID ); break;
      case 'e': match = ( arg->type == INLINE_ASM_ARG_EXPR ); break;
      default:
         unknown = true;
      }
      if ( match || unknown ) {
         break;
      }
      else {
         switch ( *test->format ) {
         case 'v':
         case 'a':
         case 'f':
            ++test->format;
            ++test->format;
            break;
         default:
            ++test->format;
         }
      }
   }
   // Process the argument.
   switch ( *test->format ) {
   case 'l':
      test_label_arg( semantic, test, arg );
      break;
   case 'v':
   case 'a':
      test_var_arg( semantic, test, arg );
      break;
   case 'f':
      test_func_arg( semantic, test, arg );
      break;
   case 'e':
      test_expr_arg( semantic, arg );
      break;
   case 'n':
      break;
   default:
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "invalid argument" );
      s_bail( semantic );
   }
   // Find format string of next argument.
   if ( test->repeat ) {
      test->format = test->repeat;
   }
   else {
      while ( *test->format && *test->format != ',' ) {
         ++test->format;
      }
      if ( *test->format == ',' ) {
         ++test->format;
      }
   }
}

void test_label_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg ) {
   list_iter_t i;
   list_iter_init( &i, semantic->topfunc_test->labels );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      if ( strcmp( label->name, arg->value.id ) == 0 ) {
         arg->value.label = label;
         arg->type = INLINE_ASM_ARG_LABEL;
         return;
      }
      list_next( &i );
   }
   s_diag( semantic, DIAG_POS_ERR, &arg->pos,
      "label `%s` not found", arg->value.id );
   s_bail( semantic );
}

void test_var_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg ) {
   struct object_search search;
   s_init_object_search( &search, NODE_NONE, &arg->pos, arg->value.id );
   s_search_object( semantic, &search );
   struct object* object = search.object;
   if ( ! object ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "`%s` not found", arg->value.id );
      s_bail( semantic );
   }
   if ( object->node.type != NODE_VAR && object->node.type != NODE_PARAM ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "instruction argument not a variable" );
      s_bail( semantic );
   }
   struct var* var = NULL;
   struct param* param = NULL;
   struct structure* structure = NULL;
   struct dim* dim = NULL;
   int storage = STORAGE_LOCAL;
   if ( object->node.type == NODE_PARAM ) {
      param = ( struct param* ) object;
   }
   else {
      var = ( struct var* ) object;
      structure = var->structure;
      dim = var->dim;
      storage = var->storage;
   }
   // Check for array.
   if ( test->format[ 0 ] == 'a' ) {
      if ( ! dim && ! structure ) {
         s_diag( semantic, DIAG_POS_ERR, &arg->pos,
            "instruction argument not an array" );
         s_bail( semantic );
      }
   }
   else {
      if ( dim || structure ) {
         s_diag( semantic, DIAG_POS_ERR, &arg->pos,
            "instruction argument not a scalar" );
         s_bail( semantic );
      }
   }
   // Check storage.
   bool storage_correct = (
      ( test->format[ 1 ] == 's' && storage == STORAGE_LOCAL ) ||
      ( test->format[ 1 ] == 'm' && storage == STORAGE_MAP ) ||
      ( test->format[ 1 ] == 'w' && storage == STORAGE_WORLD ) ||
      ( test->format[ 1 ] == 'g' && storage == STORAGE_GLOBAL ) );
   if ( ! storage_correct ) {
      const char* name = "";
      switch ( test->format[ 1 ] ) {
      case 's': name = "script"; break;
      case 'm': name = "map"; break;
      case 'w': name = "world"; break;
      case 'g': name = "global"; break;
      default:
         UNREACHABLE();
      }
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "instruction argument not a %s %s", name,
         ( test->format[ 0 ] == 'a' ) ? "array" : "variable" );
      s_bail( semantic );
   }
   if ( param ) {
      arg->type = INLINE_ASM_ARG_PARAM;
      arg->value.param = param;
   }
   else {
      arg->type = INLINE_ASM_ARG_VAR;
      arg->value.var = var;
   }
}

void test_func_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg ) {
   struct object_search search;
   s_init_object_search( &search, NODE_NONE, &arg->pos, arg->value.id );
   s_search_object( semantic, &search );
   struct object* object = search.object;
   if ( ! object ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "`%s` not found", arg->value.id );
      s_bail( semantic );
   }
   if ( object->node.type != NODE_FUNC ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "instruction argument not a function" );
      s_bail( semantic );
   }
   struct func* func = ( struct func* ) object;
   if ( test->format[ 1 ] == 'e' ) {
      if ( func->type != FUNC_EXT ) {
         s_diag( semantic, DIAG_POS_ERR, &arg->pos,
            "instruction argument not an extension function" );
         s_bail( semantic );
      }
   }
   else {
      if ( func->type != FUNC_USER ) {
         s_diag( semantic, DIAG_POS_ERR, &arg->pos,
            "instruction argument not a user-created function" );
         s_bail( semantic );
      }
   }
   arg->type = INLINE_ASM_ARG_FUNC;
   arg->value.func = func;
}

void test_expr_arg( struct semantic* semantic, struct inline_asm_arg* arg ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, arg->value.expr );
   if ( ! arg->value.expr->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "instruction argument not constant" );
      s_bail( semantic );
   }
}