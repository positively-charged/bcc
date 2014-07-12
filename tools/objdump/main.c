/*

   Displays the contents of an ACSE or ACSe object file. The Hexen format is
   not yet supported.

*/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>

enum {
   PCD_NONE,
   PCD_TERMINATE,
   PCD_SUSPEND,
   PCD_PUSHNUMBER,
   PCD_LSPEC1,
   PCD_LSPEC2,
   PCD_LSPEC3,
   PCD_LSPEC4,
   PCD_LSPEC5,
   PCD_LSPEC1DIRECT,
   PCD_LSPEC2DIRECT,
   PCD_LSPEC3DIRECT,
   PCD_LSPEC4DIRECT,
   PCD_LSPEC5DIRECT,
   PCD_ADD,
   PCD_SUBTRACT,
   PCD_MULIPLY,
   PCD_DIVIDE,
   PCD_MODULUS,
   PCD_EQ,
   PCD_NE,
   PCD_LT,
   PCD_GT,
   PCD_LE,
   PCD_GE,
   PCD_ASSIGNSCRIPTVAR,
   PCD_ASSIGNMAPVAR,
   PCD_ASSIGNWORLDVAR,
   PCD_PUSHSCRIPTVAR,
   PCD_PUSHMAPVAR,
   PCD_PUSHWORLDVAR,
   PCD_ADDSCRIPTVAR,
   PCD_ADDMAPVAR,
   PCD_ADDWORLDVAR,
   PCD_SUBSCRIPTVAR,
   PCD_SUBMAPVAR,
   PCD_SUBWORLDVAR,
   PCD_MULSCRIPTVAR,
   PCD_MULMAPVAR,
   PCD_MULWORLDVAR,
   PCD_DIVSCRIPTVAR,
   PCD_DIVMAPVAR,
   PCD_DIVWORLDVAR,
   PCD_MODSCRIPTVAR,
   PCD_MODMAPVAR,
   PCD_MODWORLDVAR,
   PCD_INCSCRIPTVAR,
   PCD_INCMAPVAR,
   PCD_INCWORLDVAR,
   PCD_DECSCRIPTVAR,
   PCD_DECMAPVAR,
   PCD_DECWORLDVAR,
   PCD_GOTO,
   PCD_IFGOTO,
   PCD_DROP,
   PCD_DELAY,
   PCD_DELAYDIRECT,
   PCD_RANDOM,
   PCD_RANDOMDIRECT,
   PCD_THINGCOUNT,
   PCD_THINGCOUNTDIRECT,
   PCD_TAGWAIT,
   PCD_TAGWAITDIRECT,
   PCD_POLYWAIT,
   PCD_POLYWAITDIRECT,
   PCD_CHANGEFLOOR,
   PCD_CHANGEFLOORDIRECT,
   PCD_CHANGECEILING,
   PCD_CHANGECEILINGDIRECT,
   PCD_RESTART,
   PCD_ANDLOGICAL,
   PCD_ORLOGICAL,
   PCD_ANDBITWISE,
   PCD_ORBITWISE,
   PCD_EORBITWISE,
   PCD_NEGATELOGICAL,
   PCD_LSHIFT,
   PCD_RSHIFT,
   PCD_UNARYMINUS,
   PCD_IFNOTGOTO,
   PCD_LINESIDE,
   PCD_SCRIPTWAIT,
   PCD_SCRIPTWAITDIRECT,
   PCD_CLEARLINESPECIAL,
   PCD_CASEGOTO,
   PCD_BEGINPRINT,
   PCD_ENDPRINT,
   PCD_PRINTSTRING,
   PCD_PRINTNUMBER,
   PCD_PRINTCHARACTER,
   PCD_PLAYERCOUNT,
   PCD_GAMETYPE,
   PCD_GAMESKILL,
   PCD_TIMER,
   PCD_SECTORSOUND,
   PCD_AMBIENTSOUND,
   PCD_SOUNDSEQUENCE,
   PCD_SETLINETEXTURE,
   PCD_SETLINEBLOCKING,
   PCD_SETLINESPECIAL,
   PCD_THINGSOUND,
   PCD_ENDPRINTBOLD,
   PCD_ACTIVATORSOUND,
   PCD_LOCALAMBIENTSOUND,
   PCD_SETLINEMONSTERBLOCKING,
   PCD_PLAYERBLUESKULL,
   PCD_PLAYERREDSKULL,
   PCD_PLAYERYELLOWSKULL,
   PCD_PLAYERMASTERSKULL,
   PCD_PLAYERBLUECARD,
   PCD_PLAYERREDCARD,
   PCD_PLAYERYELLOWCARD,
   PCD_PLAYERMASTERCARD,
   PCD_PLAYERBLACKSKULL,
   PCD_PLAYERSILVERSKULL,
   PCD_PLAYERGOLDSKULL,
   PCD_PLAYERBLACKCARD,
   PCD_PLAYERSILVERCARD,
   PCD_ISMULTIPLAYER,
   PCD_PLAYERTEAM,
   PCD_PLAYERHEALTH,
   PCD_PLAYERARMORPOINTS,
   PCD_PLAYERFRAGS,
   PCD_PLAYEREXPERT,
   PCD_BLUETEAMCOUNT,
   PCD_REDTEAMCOUNT,
   PCD_BLUETEAMSCORE,
   PCD_REDTEAMSCORE,
   PCD_ISONEFLAGCTF,
   PCD_GETINVASIONWAVE,
   PCD_GETINVASIONSTATE,
   PCD_PRINTNAME,
   PCD_MUSICCHANGE,
   PCD_CONSOLECOMMANDDIRECT,
   PCD_CONSOLECOMMAND,
   PCD_SINGLEPLAYER,
   PCD_FIXEDMUL,
   PCD_FIXEDDIV,
   PCD_SETGRAVITY,
   PCD_SETGRAVITYDIRECT,
   PCD_SETAIRCONTROL,
   PCD_SETAIRCONTROLDIRECT,
   PCD_CLEARINVENTORY,
   PCD_GIVEINVENTORY,
   PCD_GIVEINVENTORYDIRECT,
   PCD_TAKEINVENTORY,
   PCD_TAKEINVENTORYDIRECT,
   PCD_CHECKINVENTORY,
   PCD_CHECKINVENTORYDIRECT,
   PCD_SPAWN,
   PCD_SPAWNDIRECT,
   PCD_SPAWNSPOT,
   PCD_SPAWNSPOTDIRECT,
   PCD_SETMUSIC,
   PCD_SETMUSICDIRECT,
   PCD_LOCALSETMUSIC,
   PCD_LOCALSETMUSICDIRECT,
   PCD_PRINTFIXED,
   PCD_PRINTLOCALIZED,
   PCD_MOREHUDMESSAGE,
   PCD_OPTHUDMESSAGE,
   PCD_ENDHUDMESSAGE,
   PCD_ENDHUDMESSAGEBOLD,
   PCD_SETSTYLE,
   PCD_SETSTYLEDIRECT,
   PCD_SETFONT,
   PCD_SETFONTDIRECT,
   PCD_PUSHBYTE,
   PCD_LSPEC1DIRECTB,
   PCD_LSPEC2DIRECTB,
   PCD_LSPEC3DIRECTB,
   PCD_LSPEC4DIRECTB,
   PCD_LSPEC5DIRECTB,
   PCD_DELAYDIRECTB,
   PCD_RANDOMDIRECTB,
   PCD_PUSHBYTES,
   PCD_PUSH2BYTES,
   PCD_PUSH3BYTES,
   PCD_PUSH4BYTES,
   PCD_PUSH5BYTES,
   PCD_SETTHINGSPECIAL,
   PCD_ASSIGNGLOBALVAR,
   PCD_PUSHGLOBALVAR,
   PCD_ADDGLOBALVAR,
   PCD_SUBGLOBALVAR,
   PCD_MULGLOBALVAR,
   PCD_DIVGLOBALVAR,
   PCD_MODGLOBALVAR,
   PCD_INCGLOBALVAR,
   PCD_DECGLOBALVAR,
   PCD_FADETO,
   PCD_FADERANGE,
   PCD_CANCELFADE,
   PCD_PLAYMOVIE,
   PCD_SETFLOORTRIGGER,
   PCD_SETCEILINGTRIGGER,
   PCD_GETACTORX,
   PCD_GETACTORY,
   PCD_GETACTORZ,
   PCD_STARTTRANSLATION,
   PCD_TRANSLATIONRANGE1,
   PCD_TRANSLATIONRANGE2,
   PCD_ENDTRANSLATION,
   PCD_CALL,
   PCD_CALLDISCARD,
   PCD_RETURNVOID,
   PCD_RETURNVAL,
   PCD_PUSHMAPARRAY,
   PCD_ASSIGNMAPARRAY,
   PCD_ADDMAPARRAY,
   PCD_SUBMAPARRAY,
   PCD_MULMAPARRAY,
   PCD_DIVMAPARRAY,
   PCD_MODMAPARRAY,
   PCD_INCMAPARRAY,
   PCD_DECMAPARRAY,
   PCD_DUP,
   PCD_SWAP,
   PCD_WRITETOINI,
   PCD_GETFROMINI,
   PCD_SIN,
   PCD_COS,
   PCD_VECTORANGLE,
   PCD_CHECKWEAPON,
   PCD_SETWEAPON,
   PCD_TAGSTRING,
   PCD_PUSHWORLDARRAY,
   PCD_ASSIGNWORLDARRAY,
   PCD_ADDWORLDARRAY,
   PCD_SUBWORLDARRAY,
   PCD_MULWORLDARRAY,
   PCD_DIVWORLDARRAY,
   PCD_MODWORLDARRAY,
   PCD_INCWORLDARRAY,
   PCD_DECWORLDARRAY,
   PCD_PUSHGLOBALARRAY,
   PCD_ASSIGNGLOBALARRAY,
   PCD_ADDGLOBALARRAY,
   PCD_SUBGLOBALARRAY,
   PCD_MULGLOBALARRAY,
   PCD_DIVGLOBALARRAY,
   PCD_MODGLOBALARRAY,
   PCD_INCGLOBALARRAY,
   PCD_DECGLOBALARRAY,
   PCD_SETMARINEWEAPON,
   PCD_SETACTORPROPERTY,
   PCD_GETACTORPROPERTY,
   PCD_PLAYERNUMBER,
   PCD_ACTIVATORTID,
   PCD_SETMARINESPRITE,
   PCD_GETSCREENWIDTH,
   PCD_GETSCREENHEIGHT,
   PCD_THINGPROJECTILE2,
   PCD_STRLEN,
   PCD_SETHUDSIZE,
   PCD_GETCVAR,
   PCD_CASEGOTOSORTED,
   PCD_SETRESULTVALUE,
   PCD_GETLINEROWOFFSET,
   PCD_GETACTORFLOORZ,
   PCD_GETACTORANGLE,
   PCD_GETSECTORFLOORZ,
   PCD_GETSECTORCEILINGZ,
   PCD_LSPEC5RESULT,
   PCD_GETSIGILPIECES,
   PCD_GETLEVELINFO,
   PCD_CHANGESKY,
   PCD_PLAYERINGAME,
   PCD_PLAYERISBOT,
   PCD_SETCAMERATOTEXTURE,
   PCD_ENDLOG,
   PCD_GETAMMOCAPACITY,
   PCD_SETAMMOCAPACITY,
   PCD_PRINTMAPCHARARRAY,
   PCD_PRINTWORLDCHARARRAY,
   PCD_PRINTGLOBALCHARARRAY,
   PCD_SETACTORANGLE,
   PCD_GRAPINPUT,
   PCD_SETMOUSEPOINTER,
   PCD_MOVEMOUSEPOINTER,
   PCD_SPAWNPROJECTILE,
   PCD_GETSECTORLIGHTLEVEL,
   PCD_GETACTORCEILINGZ,
   PCD_SETACTORPOSITION,
   PCD_CLEARACTORINVENTORY,
   PCD_GIVEACTORINVENTORY,
   PCD_TAKEACTORINVENTORY,
   PCD_CHECKACTORINVENTORY,
   PCD_THINGCOUNTNAME,
   PCD_SPAWNSPOTFACING,
   PCD_PLAYERCLASS,
   PCD_ANDSCRIPTVAR,
   PCD_ANDMAPVAR,
   PCD_ANDWORLDVAR,
   PCD_ANDGLOBALVAR,
   PCD_ANDMAPARRAY,
   PCD_ANDWORLDARRAY,
   PCD_ANDGLOBALARRAY,
   PCD_EORSCRIPTVAR,
   PCD_EORMAPVAR,
   PCD_EORWORLDVAR,
   PCD_EORGLOBALVAR,
   PCD_EORMAPARRAY,
   PCD_EORWORLDARRAY,
   PCD_EORGLOBALARRAY,
   PCD_ORSCRIPTVAR,
   PCD_ORMAPVAR,
   PCD_ORWORLDVAR,
   PCD_ORGLOBALVAR,
   PCD_ORMAPARRAY,
   PCD_ORWORLDARRAY,
   PCD_ORGLOBALARRAY,
   PCD_LSSCRIPTVAR,
   PCD_LSMAPVAR,
   PCD_LSWORLDVAR,
   PCD_LSGLOBALVAR,
   PCD_LSMAPARRAY,
   PCD_LSWORLDARRAY,
   PCD_LSGLOBALARRAY,
   PCD_RSSCRIPTVAR,
   PCD_RSMAPVAR,
   PCD_RSWORLDVAR,
   PCD_RSGLOBALVAR,
   PCD_RSMAPARRAY,
   PCD_RSWORLDARRAY,
   PCD_RSGLOBALARRAY,
   PCD_GETPLAYERINFO,
   PCD_CHANGELEVEL,
   PCD_SECTORDAMAGE,
   PCD_REPLACETEXTURES,
   PCD_NEGATEBINARY,
   PCD_GETACTORPITCH,
   PCD_SETACTORPITCH,
   PCD_PRINTBIND,
   PCD_SETACTORSTATE,
   PCD_THINGDAMAGE2,
   PCD_USEINVENTORY,
   PCD_USEACTORINVENTORY,
   PCD_CHECKACTORCEILINGTEXTURE,
   PCD_CHECKACTORFLOORTEXTURE,
   PCD_GETACTORLIGHTLEVEL,
   PCD_SETMUGSHOTSTATE,
   PCD_THINGCOUNTSECTOR,
   PCD_THINGCOUNTNAMESECTOR,
   PCD_CHECKPLAYERCAMERA,
   PCD_MORPHACTOR,
   PCD_UNMORPHACTOR,
   PCD_GETPLAYERINPUT,
   PCD_CLASSIFYACTOR,
   PCD_PRINTBINARY,
   PCD_PRINTHEX,
   PCD_CALLFUNC,
   PCD_SAVESTRING,
   PCD_PRINTMAPCHRANGE,
   PCD_PRINTWORLDCHRANGE,
   PCD_PRINTGLOBALCHRANGE,
   PCD_STRCPYTOMAPCHRANGE,
   PCD_STRCPYTOWORLDCHRANGE,
   PCD_STRCPYTOGLOBALCHRANGE,
   PCD_PUSHFUNCTION,
   PCD_CALLSTACK,
   PCD_SCRIPTWAITNAMED,
   PCD_TRANSLATIONRANGE3,
   PCD_GOTOSTACK,
   PCD_TOTAL
};

struct options {
   const char* file;
   const char* view_chunk;
   bool list_chunks;
};

struct object {
   const char* data;
   int size;
   enum {
      FORMAT_UNKNOWN,
      FORMAT_BIG_E,
      FORMAT_LITTLE_E,
   } format;
   int chunk_offset;
   bool indirect_format;
   bool small_code;
};

struct header {
   char id[ 4 ];
   int offset;
};

struct chunk {
   char name[ 5 ];
   const char* data; 
   int size;
   enum {
      CHUNK_UNKNOWN,
      CHUNK_ARAY,
      CHUNK_AINI,
      CHUNK_AIMP,
      CHUNK_ASTR,
      CHUNK_MSTR,
      CHUNK_LOAD,
      CHUNK_FUNC,
      CHUNK_FNAM,
      CHUNK_MINI,
      CHUNK_MIMP,
      CHUNK_MEXP,
      CHUNK_SPTR,
      CHUNK_SFLG,
      CHUNK_SVCT,
      CHUNK_STRL,
      CHUNK_STRE,
   } type;
};

struct chunk_read {
   const char* data;
   int data_size;
   int pos;
};

static bool read_options( struct options*, int, char** );
static void init_object( struct object*, const char*, int );
static void init_chunk( struct chunk*, const char* );
static int get_chunk_type( const char* );
static void show_pcode( struct object*, int, int );
static void list_chunks( struct object* );
static bool show_chunk( struct object*, struct chunk*, bool );
static void show_aray( struct chunk* );
static void show_aini( struct chunk* );
static void show_aimp( struct chunk* );
static void show_astr_mstr( struct chunk* );
static void show_load( struct chunk* );
static void show_func( struct object*, struct chunk* );
static void show_fnam( struct chunk* );
static void show_mini( struct chunk* );
static void show_mimp( struct chunk* );
static void show_mexp( struct chunk* );
static void show_sptr( struct object*, struct chunk* );
static const char* get_script_type_name( int );
static void show_sflg( struct chunk* );
static void show_strl( struct chunk*, bool );
static void show_svct( struct chunk* );
static bool view_chunk( struct object*, const char* );
static void init_chunk_read( struct object*, struct chunk_read* );
static bool read_chunk( struct chunk_read*, struct chunk* );
static void view_all_chunks( struct object* );

static struct {
   const char* name;
   int num_args;
} g_pcodes[] = {
   { "none", 0 },
   { "terminate", 0 },
   { "suspend", 0 },
   { "pushnumber", 1 },
   { "lspec1", 1 },
   { "lspec2", 1 },
   { "lspec3", 1 },
   { "lspec4", 1 },
   { "lspec5", 1 },
   { "lspec1direct", 2 },
   { "lspec2direct", 3 },
   { "lspec3direct", 4 },
   { "lspec4direct", 5 },
   { "lspec5direct", 6 },
   { "add", 0 },
   { "subtract", 0 },
   { "multiply", 0 },
   { "divide", 0 },
   { "modulus", 0 },
   { "eq", 0 },
   { "ne", 0 },
   { "lt", 0 },
   { "gt", 0 },
   { "le", 0 },
   { "ge", 0 },
   { "assignscriptvar", 1 },
   { "assignmapvar", 1 },
   { "assignworldvar", 1 },
   { "pushscriptvar", 1 },
   { "pushmapvar", 1 },
   { "pushworldvar", 1 },
   { "addscriptvar", 1 },
   { "addmapvar", 1 },
   { "addworldvar", 1 },
   { "subscriptvar", 1 },
   { "submapvar", 1 },
   { "subworldvar", 1 },
   { "mulscriptvar", 1 },
   { "mulmapvar", 1 },
   { "mulworldvar", 1 },
   { "divscriptvar", 1 },
   { "divmapvar", 1 },
   { "divworldvar", 1 },
   { "modscriptvar", 1 },
   { "modmapvar", 1 },
   { "modworldvar", 1 },
   { "incscriptvar", 1 },
   { "incmapvar", 1 },
   { "incworldvar", 1 },
   { "decscriptvar", 1 },
   { "decmapvar", 1 },
   { "decworldvar", 1 },
   { "goto", 1 },
   { "ifgoto", 1 },
   { "drop", 0 },
   { "delay", 0 },
   { "delaydirect", 1 },
   { "random", 0 },
   { "randomdirect", 2 },
   { "thingcount", 0 },
   { "thingcountdirect", 2 },
   { "tagwait", 0 },
   { "tagwaitdirect", 1 },
   { "polywait", 0 },
   { "polywaitdirect", 1 },
   { "changefloor", 0 },
   { "changefloordirect", 2 },
   { "changeceiling", 0 },
   { "changeceilingdirect", 2 },
   { "restart", 0 },
   { "andlogical", 0 },
   { "orlogical", 0 },
   { "andbitwise", 0 },
   { "orbitwise", 0 },
   { "eorbitwise", 0 },
   { "negatelogical", 0 },
   { "lshift", 0 },
   { "rshift", 0 },
   { "unaryminus", 0 },
   { "ifnotgoto", 1 },
   { "lineside", 0 },
   { "scriptwait", 0 },
   { "scriptwaitdirect", 1 },
   { "clearlinespecial", 0 },
   { "casegoto", 2 },
   { "beginprint", 0 },
   { "endprint", 0 },
   { "printstring", 0 },
   { "printnumber", 0 },
   { "printcharacter", 0 },
   { "playercount", 0 },
   { "gametype", 0 },
   { "gameskill", 0 },
   { "timer", 0 },
   { "sectorsound", 0 },
   { "ambientsound", 0 },
   { "soundsequence", 0 },
   { "setlinetexture", 0 },
   { "setlineblocking", 0 },
   { "setlinespecial", 0 },
   { "thingsound", 0 },
   { "endprintbold", 0 },
   { "activatorsound", 0 },
   { "ambientsound", 0 },
   { "setlinemonsterblocking", 0 },
   { "playerblueskull", 0 },
   { "playerredskull", 0 },
   { "playeryellowskull", 0 },
   { "playermasterskull", 0 },
   { "playerbluecard", 0 },
   { "playerredcard", 0 },
   { "playeryellowcard", 0 },
   { "playermastercard", 0 },
   { "playerblackskull", 0 },
   { "playersilverskull", 0 },
   { "playergoldskull", 0 },
   { "playerblackcard", 0 },
   { "playersilvercard", 0 },
   { "ismultiplayer", 0 },
   { "playerteam", 0 },
   { "playerhealth", 0 },
   { "playerarmorpoints", 0 },
   { "playerfrags", 0 },
   { "playerexpert", 0 },
   { "blueteamcount", 0 },
   { "redteamcount", 0 },
   { "blueteamscore", 0 },
   { "redteamscore", 0 },
   { "isoneflagctf", 0 },
   { "getinvasionwave", 0 },
   { "getinvastionstate", 0 },
   { "printname", 0 },
   { "musicchange", 0 },
   { "consolecommanddirect", 3 },
   { "consolecommand", 0 },
   { "singleplayer", 0 },
   { "fixedmul", 0 },
   { "fixeddiv", 0 },
   { "setgravity", 0 },
   { "setgravitydirect", 1 },
   { "setaircontrol", 0 },
   { "setaircontroldirect", 1 },
   { "clearinventory", 0 },
   { "giveinventory", 0 },
   { "giveinventorydirect", 2 },
   { "takeinventory", 0 },
   { "takeinventorydirect", 2 },
   { "checkinventory", 0 },
   { "checkinventorydirect", 1 },
   { "spawn", 0 },
   { "spawndirect", 6 },
   { "spawnspot", 0 },
   { "spawnspotdirect", 4 },
   { "setmusic", 0 },
   { "setmusicdirect", 3 },
   { "localsetmusic", 0 },
   { "localsetmusicdirect", 3 },
   { "printfixed", 0 },
   { "printlocalized", 0 },
   { "morehudmessage", 0 },
   { "opthudmessage", 0 },
   { "endhudmessage", 0 },
   { "endhudmessagebold", 0 },
   { "setstyle", 0 },
   { "setstyledirect", 0 },
   { "setfont", 0 },
   { "setfontdirect", 1 },
   { "pushbyte", 1 },
   { "lspec1directb", 2 },
   { "lspec2directb", 3 },
   { "lspec3directb", 4 },
   { "lspec4directb", 5 },
   { "lspec5directb", 6 },
   { "delaydirectb", 1 },
   { "randomdirectb", 2 },
   { "pushbytes", -1 },
   { "push2bytes", 2 },
   { "push3bytes", 3 },
   { "push4bytes", 4 },
   { "push5bytes", 5 },
   { "setthingspecial", 0 },
   { "assignglobalvar", 1 },
   { "pushglobalvar", 1 },
   { "addglobalvar", 1 },
   { "subglobalvar", 1 },
   { "mulglobalvar", 1 },
   { "divglobalvar", 1 },
   { "modglobalvar", 1 },
   { "incglobalvar", 1 },
   { "decglobalvar", 1 },
   { "fadeto", 0 },
   { "faderange", 0 },
   { "cancelfade", 0 },
   { "playmovie", 0 },
   { "setfloortrigger", 0 },
   { "setceilingtrigger", 0 },
   { "getactorx", 0 },
   { "getactory", 0 },
   { "getactorz", 0 },
   { "starttranslation", 0 },
   { "translationrange1", 0 },
   { "translationrange2", 0 },
   { "endtranslation", 0 },
   { "call", 1 },
   { "calldiscard", 1 },
   { "returnvoid", 0 },
   { "returnval", 0 },
   { "pushmaparray", 1 },
   { "assignmaparray", 1 },
   { "addmaparray", 1 },
   { "submaparray", 1 },
   { "mulmaparray", 1 },
   { "divmaparray", 1 },
   { "modmaparray", 1 },
   { "incmaparray", 1 },
   { "decmaparray", 1 },
   { "dup", 0 },
   { "swap", 0 },
   { "writetoini", 0 },
   { "getfromini", 0 },
   { "sin", 0 },
   { "cos", 0 },
   { "vectorangle", 0 },
   { "checkweapon", 0 },
   { "setweapon", 0 },
   { "tagstring", 0 },
   { "pushworldarray", 1 },
   { "assignworldarray", 1 },
   { "addworldarray", 1 },
   { "subworldarray", 1 },
   { "mulworldarray", 1 },
   { "divworldarray", 1 },
   { "modworldarray", 1 },
   { "incworldarray", 1 },
   { "decworldarray", 1 },
   { "pushglobalarray", 1 },
   { "assignglobalarray", 1 },
   { "addglobalarray", 1 },
   { "subglobalarray", 1 },
   { "mulglobalarray", 1 },
   { "divglobalarray", 1 },
   { "modglobalarray", 1 },
   { "incglobalarray", 1 },
   { "decglobalarray", 1 },
   { "setmarineweapon", 0 },
   { "setactorproperty", 0 },
   { "getactorproperty", 0 },
   { "playernumber", 0 },
   { "activatortid", 0 },
   { "setmarinesprite", 0 },
   { "getscreenwidth", 0 },
   { "getscreenheight", 0 },
   { "thingprojectile2", 0 },
   { "strlen", 0 },
   { "gethudsize", 0 },
   { "getcvar", 0 },
   { "casegotosorted", -1 },
   { "setresultvalue", 0 },
   { "getlinerowoffset", 0 },
   { "getactorfloorz", 0 },
   { "getactorangle", 0 },
   { "getsectorfloorz", 0 },
   { "getsectorceilingz", 0 },
   { "lspec5result", 1 },
   { "getsigilpieces", 0 },
   { "getlevelinfo", 0 },
   { "changesky", 0 },
   { "playeringame", 0 },
   { "playerisbot", 0 },
   { "setcameratotexture", 0 },
   { "endlog", 0 },
   { "getammocapacity", 0 },
   { "setammocapacity", 0 },
   { "printmapchararray", 0 },
   { "printworldchararray", 0 },
   { "printglobalchararray", 0 },
   { "setactorangle", 0 },
   { "grabinput", 0 },
   { "setmousepointer", 0 },
   { "movemousepointer", 0 },
   { "spawnprojectile", 0 },
   { "getsectorlightlevel", 0 },
   { "getactorceilingz", 0 },
   { "setactorposition", 0 },
   { "clearactorinventory", 0 },
   { "giveactorinventory", 0 },
   { "takeactorinventory", 0 },
   { "checkactorinventory", 0 },
   { "thingcountname", 0 },
   { "spawnspotfacing", 0 },
   { "playerclass", 0 },
   { "andscriptvar", 1 },
   { "andmapvar", 1 },
   { "andworldvar", 1 },
   { "andglobalvar", 1 },
   { "andmaparray", 1 },
   { "andworldarray", 1 },
   { "andglobalarray", 1 },
   { "eorscriptvar", 1 },
   { "eormapvar", 1 },
   { "eorworldvar", 1 },
   { "eorglobalvar", 1 },
   { "eormaparray", 1 },
   { "eorworldarray", 1 },
   { "eorglobalarray", 1 },
   { "orscriptvar", 1 },
   { "ormapvar", 1 },
   { "orworldvar", 1 },
   { "orglobalvar", 1 },
   { "ormaparray", 1 },
   { "orworldarray", 1 },
   { "orglobalarray", 1 },
   { "lsscriptvar", 1 },
   { "lsmapvar", 1 },
   { "lsworldvar", 1 },
   { "lsglobalvar", 1 },
   { "lsmaparray", 1 },
   { "lsworldarray", 1 },
   { "lsglobalarray", 1 },
   { "rsscriptvar", 1 },
   { "rsmapvar", 1 },
   { "rsworldvar", 1 },
   { "rsglobalvar", 1 },
   { "rsmaparray", 1 },
   { "rsworldarray", 1 },
   { "rsglobalarray", 1 },
   { "getplayerinfo", 0 },
   { "changelevel", 0 },
   { "sectordamage", 0 },
   { "replacetextures", 0 },
   { "negatebinary", 0 },
   { "getactorpitch", 0 },
   { "setactorpitch", 0 },
   { "printbind", 0 },
   { "setactorstate", 0 },
   { "thingdamage2", 0 },
   { "useinventory", 0 },
   { "useactorinventory", 0 },
   { "checkactorceilingtexture", 0 },
   { "checkactorfloortexture", 0 },
   { "getactorlightlevel", 0 },
   { "setmugshotstate", 0 },
   { "thingcountsector", 0 },
   { "thingcountnamesector", 0 },
   { "checkplayercamera", 0 },
   { "morphactor", 0 },
   { "unmorphactor", 0 },
   { "getplayerinput", 0 },
   { "classifyactor", 0 },
   { "printbinary", 0 },
   { "printhex", 0 },
   { "callfunc", 2 },
   { "savestring", 0 },
   { "printmapchrange", 0 },
   { "printworldchrange", 0 },
   { "printglobalchrange", 0 },
   { "strcpytomapchrange", 0 },
   { "strcpytoworldchrange", 0 },
   { "strcpytoglobalchrange", 0 },
   { "pushfunction", 1 },
   { "callstack", 0 },
   { "scriptwaitnamed", 0 },
   { "translationrange3", 0 },
   { "gotostack", 0 }
};

int main( int argc, char* argv[] ) {
   int result = EXIT_FAILURE;
   struct options options;
   if ( ! read_options( &options, argc, argv ) ) {
      goto finish;
   }
   FILE* fh = fopen( options.file, "rb" );
   if ( ! fh ) {
      printf( "error: failed to open file: %s\n", options.file );
      goto finish;
   }
   fseek( fh, 0, SEEK_END );
   size_t size = ftell( fh );
   rewind( fh );
   char* data = malloc( sizeof( char ) * size );
   fread( data, sizeof( char ), size, fh );
   fclose( fh );
   struct object object;
   init_object( &object, data, size );
   const char* format = "ACSE";
   switch ( object.format ) {
   case FORMAT_BIG_E:
      break;
   case FORMAT_LITTLE_E:
      format = "ACSe";
      break;
   default:
      printf( "error: unsupported format\n" );
      goto deinit_data;
   }
   const char* indirect = "";
   if ( object.indirect_format ) {
      indirect = " (indirect)";
   }
   printf( "format: %s%s\n", format, indirect );
   if ( options.list_chunks ) {
      list_chunks( &object );
      result = EXIT_SUCCESS;
   }
   else if ( options.view_chunk ) {
      if ( view_chunk( &object, options.view_chunk ) ) {
         result = EXIT_SUCCESS;
      }
   }
   else {
      view_all_chunks( &object ); 
      result = EXIT_SUCCESS;
   }
   deinit_data:
   free( data );
   finish:
   return result;
}

bool read_options( struct options* options, int argc, char** argv ) {
   options->file = NULL;
   options->view_chunk = NULL;
   options->list_chunks = false;
   if ( argc > 1 ) {
      int i = 1;
      while ( argv[ i ] && argv[ i ][ 0 ] == '-' ) {
         switch ( argv[ i ][ 1 ] ) {
         case 'c':
            if ( argv[ i + 1 ] ) {
               options->view_chunk = argv[ i + 1 ];
               i += 2;
            }
            else {
               printf( "error: missing chunk to view\n" );
               return false;
            }
            break;
         case 'l':
            options->list_chunks = true;
            ++i;
            break;
         default:
            printf( "error: unknown option: %c\n", argv[ i ][ 1 ] );
            return false;
         }
      }
      if ( argv[ i ] ) {
         options->file = argv[ i ];
         return true;
      }
      else {
         printf( "error: missing object file\n" );
         return false;
      }
   }
   else {
      printf(
         "%s [options] <object-file>\n"
         "Options:\n"
         "  -c <chunk>    View selected chunk\n"
         "  -l            List chunks in object file\n",
         argv[ 0 ]
      );
      return false;
   }
}

void init_object( struct object* object, const char* data, int size ) {
   struct header header;
   memcpy( &header, data, sizeof( header ) );
   object->data = data;
   object->size = size;
   object->format = FORMAT_UNKNOWN;
   object->chunk_offset = header.offset;
   object->indirect_format = false;
   object->small_code = false;
   if ( memcmp( header.id, "ACSE", 4 ) == 0 ) {
      object->format = FORMAT_BIG_E;
   }
   else if ( memcmp( header.id, "ACSe", 4 ) == 0 ) {
      object->format = FORMAT_LITTLE_E;
   }
   else if ( memcmp( header.id, "ACS\0", 4 ) == 0 ) {
      const char* format = data + header.offset - 4;
      if ( memcmp( format, "ACSE", 4 ) == 0 ) {
         object->format = FORMAT_BIG_E;
      }
      else if ( memcmp( format, "ACSe", 4 ) == 0 ) {
         object->format = FORMAT_LITTLE_E;
      }
      if ( object->format != FORMAT_UNKNOWN ) {
         memcpy( &object->chunk_offset, format - sizeof( int ),
            sizeof( int ) );
         object->indirect_format = true;
      }
   }
   if ( object->format == FORMAT_LITTLE_E ) {
      object->small_code = true;
   }
}

void init_chunk( struct chunk* chunk, const char* data ) {
   memcpy( chunk->name, data, 4 );
   chunk->name[ 4 ] = 0;
   data += sizeof( int );
   memcpy( &chunk->size, data, sizeof( int ) );
   data += sizeof( int );
   chunk->data = data;
   chunk->type = get_chunk_type( chunk->name );
}

void list_chunks( struct object* object ) {
   const char* data = object->data + object->chunk_offset;
   while ( data < object->data + object->size ) {
      struct chunk chunk;
      init_chunk( &chunk, data );
      show_chunk( object, &chunk, false );
      data += sizeof( int ) * 2 + chunk.size;
   }
}

bool show_chunk( struct object* object, struct chunk* chunk, bool show_contents ) {
   printf( "-- %s (%d)\n", chunk->name, chunk->size );
   if ( show_contents ) {
      switch ( chunk->type ) {
      case CHUNK_ARAY:
         show_aray( chunk );
         break;
      case CHUNK_AINI:
         show_aini( chunk );
         break;
      case CHUNK_AIMP:
         show_aimp( chunk );
         break;
      case CHUNK_ASTR:
      case CHUNK_MSTR:
         show_astr_mstr( chunk );
         break;
      case CHUNK_LOAD:
         show_load( chunk );
         break;
      case CHUNK_FUNC:
         show_func( object, chunk );
         break;
      case CHUNK_FNAM:
         show_fnam( chunk );
         break;
      case CHUNK_MINI:
         show_mini( chunk );
         break;
      case CHUNK_MIMP:
         show_mimp( chunk );
         break;
      case CHUNK_MEXP:
         show_mexp( chunk );
         break;
      case CHUNK_SPTR:
         show_sptr( object, chunk );
         break;
      case CHUNK_SFLG:
         show_sflg( chunk );
         break;
      case CHUNK_SVCT:
         show_svct( chunk );
         break;
      case CHUNK_STRL:
         show_strl( chunk, false );
         break;
      case CHUNK_STRE:
         show_strl( chunk, true );
         break;
      default:
         printf( "chunk not supported\n" ); 
         break;
      }
   }
   return true;
}

void show_aray( struct chunk* chunk ) {
   int i = 0;
   while ( i < chunk->size ) {
      struct {
         int number;
         int size;
      } entry;
      memcpy( &entry, chunk->data + i, sizeof( entry ) );
      printf( "index=%d size=%d\n", entry.number, entry.size );
      i += sizeof( entry );
   }
}

void show_aini( struct chunk* chunk ) {
   int value = 0;
   memcpy( &value, chunk->data, sizeof( int ) );
   printf( "index=%d\n", value );
   int i = 0;
   int count = ( chunk->size - sizeof( int ) ) / sizeof( int );
   while ( i < count ) {
      memcpy( &value, chunk->data + ( sizeof( int ) + sizeof( int ) * i ),
         sizeof( int ) );
      printf( "[%d] = %d\n", i, value );
      ++i;
   }
}

void show_aimp( struct chunk* chunk ) {
   const char* data = chunk->data;
   int count = 0;
   memcpy( &count, data, sizeof( int ) );
   data += sizeof( int );
   printf( "total-imported=%d\n", count );
   int i = 0;
   while ( i < count ) {
      int index = 0;
      memcpy( &index, data, sizeof( int ) );
      data += sizeof( int );
      int size = 0;
      memcpy( &size, data, sizeof( int ) );
      data += sizeof( int );
      printf( "index=%d %s[%d]\n", index, data, size );
      while ( *data ) {
         ++data;
      }
      ++data;
      ++i;
   }
}

void show_astr_mstr( struct chunk* chunk ) {
   int count = chunk->size / sizeof( int );
   int i = 0;
   while ( i < count ) {
      int index = 0;
      memcpy( &index, chunk->data + sizeof( int ) * i, sizeof( int ) );
      printf( "tagged=%d\n", index );
      ++i;
   }
}

void show_load( struct chunk* chunk ) {
   int i = 0;
   while ( i < chunk->size ) {
      if ( chunk->data[ i ] ) {
         printf( "imported-module=%s\n", chunk->data + i );
         while ( chunk->data[ i ] ) {
            ++i;
         }
      }
      ++i;
   }
}

void show_func( struct object* object, struct chunk* chunk ) {
   struct {
      char num_param;
      char size;
      char has_return;
      char padding;
      int offset;
   } entry;
   int count = chunk->size / sizeof( entry );
   int i = 0;
   while ( i < count ) {
      memcpy( &entry, chunk->data + i * sizeof( entry ), sizeof( entry ) );
      printf( "index=%d params=%d size=%d has-return=%d offset=%d\n", i,
         ( int ) entry.num_param,
         ( int ) entry.size,
         ( int ) entry.has_return,
         entry.offset );
      if ( entry.offset ) {
         int offset = entry.offset;
         int end_offset = 0;
         // Use next function to calculate end position of code.
         int k = i + 1;
         while ( k < count && ! end_offset ) {
            memcpy( &entry, chunk->data + k * sizeof( entry ),
               sizeof( entry ) );
            end_offset = entry.offset;
            ++k;
         }
         // When no more functions follow, the next point in the object file is
         // the start of the chunk data. Use this.
         if ( ! end_offset ) {
            end_offset = object->chunk_offset;
         }
         show_pcode( object, offset, end_offset - offset );
      }
      else {
         printf( "(imported)\n" );
      }
      ++i;
   }
}

void show_fnam( struct chunk* chunk ) {
   const char* data = chunk->data;
   int count = 0;
   memcpy( &count, data, sizeof( int ) );
   data += sizeof( int );
   printf( "table-size=%d\n", count );
   for ( int i = 0; i < count; ++i ) {
      int offset = 0;
      memcpy( &offset, data, sizeof( int ) );
      data += sizeof( int );
      printf( "[%d] %s\n", i, chunk->data + offset );
   }
}

void show_mini( struct chunk* chunk ) {
   int index = 0;
   memcpy( &index, chunk->data, sizeof( int ) );
   printf( "first-var=%d\n", index );
   int count = chunk->size / sizeof( int ) - 1;
   int i = 0;
   while ( i < count ) {
      int value = 0;
      memcpy( &value, chunk->data + sizeof( int ) + sizeof( int ) * i,
         sizeof( int ) );
      printf( "index=%d value=%d\n", index, value );
      ++index;
      ++i;
   }
}

void show_mimp( struct chunk* chunk ) {
   int i = 0;
   while ( i < chunk->size ) {
      int index = 0;
      memcpy( &index, chunk->data + i, sizeof( int ) );
      i += sizeof( int );
      printf( "index=%d name=%s\n", index, chunk->data + i );
      while ( chunk->data[ i ] ) {
         ++i;
      }
      ++i;
   }
}

void show_mexp( struct chunk* chunk ) {
   int count = 0;
   memcpy( &count, chunk->data, sizeof( int ) );
   printf( "table-size=%d\n", count );
   const char* data = chunk->data + sizeof( int );
   int i = 0;
   while ( i < count ) {
      int offset = 0;
      memcpy( &offset, data, sizeof( int ) );
      data += sizeof( int );
      printf( "[%d] offset=%d %s\n", i, offset, chunk->data + offset );
      ++i;
   }
}

void show_sptr( struct object* object, struct chunk* chunk ) {
   int size = 0;
   while ( size < chunk->size ) {
      int number = 0;
      int type = 0;
      int num_param = 0;
      int offset = 0;
      int end_offset = 0;
      if ( object->indirect_format ) {
         struct {
            short number;
            char type;
            char num_param;
            int offset;
         } entry;
         memcpy( &entry, chunk->data + size, sizeof( entry ) );
         size += sizeof( entry );
         number = ( int ) entry.number;
         type = ( int ) entry.type;
         num_param = ( int ) entry.num_param;
         offset = entry.offset;
         if ( size < chunk->size ) {
            memcpy( &entry, chunk->data + size, sizeof( entry ) );
            end_offset = entry.offset;
         }
      }
      else {
         struct {
            short number;
            short type;
            int offset;
            int num_param;
         } entry;
         memcpy( &entry, chunk->data + size, sizeof( entry ) );
         size += sizeof( entry );
         number = ( int ) entry.number;
         type = ( int ) entry.type;
         num_param = entry.num_param;
         offset = entry.offset;
         if ( size < chunk->size ) {
            memcpy( &entry, chunk->data + size, sizeof( entry ) );
            end_offset = entry.offset;
         }
      }
      // bcc code layout:
      //   <script-code>
      //   <function-code>
      //   <chunks>
      // When the last script is processed, there won't be another script
      // coming that could be used to determine the end of the code of the
      // last script. In this case, use the starting position of the first
      // function as the end position. If no functions are present, then use
      // the position of the first chunk.
      if ( ! end_offset ) {
         // Find functions.
         struct {
            char name[ 4 ];
            int size;
         } chunk;
         int i = object->chunk_offset;
         while ( i < object->size ) {
            memcpy( &chunk, object->data + i, sizeof( chunk ) );
            if (
               chunk.name[ 0 ] == 'F' &&
               chunk.name[ 1 ] == 'U' &&
               chunk.name[ 2 ] == 'N' &&
               chunk.name[ 3 ] == 'C' && chunk.size ) {
               break;
            }
            i += sizeof( chunk ) + chunk.size;
         }
         // Find first function.
         if ( i < object->size ) {
            int k = 0;
            while ( k < chunk.size && ! end_offset ) {
               memcpy( &end_offset,
                  object->data + i + sizeof( chunk ) + k + sizeof( int ),
                  sizeof( int ) );
               k += sizeof( int ) * 2;
            }
         }
         // When no function can be used, use position of first chunk.
         if ( ! end_offset ) {
            end_offset = object->chunk_offset;
         }
      }
      printf( "script=%d ", number );
      const char* name = get_script_type_name( type );
      if ( name ) {
         printf( "type=%s ", name );
      }
      else {
         printf( "type=unknown:%d ", type );
      }
      printf( "params=%d offset=%d\n", num_param, offset );
      show_pcode( object, offset, end_offset - offset );
   }
}

const char* get_script_type_name( int type ) {
   enum {
      TYPE_CLOSED,
      TYPE_OPEN,
      TYPE_RESPAWN,
      TYPE_DEATH,
      TYPE_ENTER,
      TYPE_PICKUP,
      TYPE_BLUERETURN,
      TYPE_REDRETURN,
      TYPE_WHITERETURN,
      TYPE_LIGHTNING = 12,
      TYPE_UNLOADING,
      TYPE_DISCONNECT,
      TYPE_RETURN,
      TYPE_EVENT
   };
   switch ( type ) {
   case TYPE_CLOSED: return "closed";
   case TYPE_OPEN: return "open";
   case TYPE_RESPAWN: return "respawn";
   case TYPE_DEATH: return "death";
   case TYPE_ENTER: return "enter";
   case TYPE_PICKUP: return "pickup";
   case TYPE_BLUERETURN: return "bluereturn";
   case TYPE_REDRETURN: return "redreturn";
   case TYPE_WHITERETURN: return "whitereturn";
   case TYPE_LIGHTNING: return "lightning";
   case TYPE_UNLOADING: return "unloading";
   case TYPE_DISCONNECT: return "disconnect";
   case TYPE_RETURN: return "return";
   case TYPE_EVENT: return "event";
   default: return NULL;
   }
}

void show_pcode( struct object* object, int offset, int code_size ) {
   const char* data_start = object->data + offset;
   const char* data = data_start;
   while ( data < data_start + code_size ) {
      // Show opcode.
      int opc = 0;
      int opc_pos = offset + ( data - data_start );
      if ( object->small_code ) {
         unsigned char ch = 0;
         memcpy( &ch, data, 1 );
         opc = ( int ) ch;
         ++data;
         if ( opc >= 240 ) {
            memcpy( &ch, data, 1 );
            opc += ( int ) ch;
            ++data;
         }
      }
      else {
         memcpy( &opc, data, sizeof( int ) );
         data += sizeof( int );
      }
      printf( "%08d> ", opc_pos );
      if ( opc < 0 || opc >= PCD_TOTAL ) {
         printf( "unknown pcode: %d\n", opc );
         break;
      }
      printf( "%s", g_pcodes[ opc ].name );
      // Show arguments.
      if (
         // One argument instructions, with argument being 1-byte/4-bytes.
         opc == PCD_LSPEC1 ||
         opc == PCD_LSPEC2 ||
         opc == PCD_LSPEC3 ||
         opc == PCD_LSPEC4 ||
         opc == PCD_LSPEC5 ||
         opc == PCD_ASSIGNSCRIPTVAR ||
         opc == PCD_ASSIGNMAPVAR ||
         opc == PCD_ASSIGNWORLDVAR ||
         opc == PCD_PUSHSCRIPTVAR ||
         opc == PCD_PUSHMAPVAR ||
         opc == PCD_PUSHWORLDVAR ||
         opc == PCD_ADDSCRIPTVAR ||
         opc == PCD_ADDMAPVAR ||
         opc == PCD_ADDWORLDVAR ||
         opc == PCD_SUBSCRIPTVAR ||
         opc == PCD_SUBMAPVAR ||
         opc == PCD_SUBWORLDVAR ||
         opc == PCD_MULSCRIPTVAR ||
         opc == PCD_MULMAPVAR ||
         opc == PCD_MULWORLDVAR ||
         opc == PCD_DIVSCRIPTVAR ||
         opc == PCD_DIVMAPVAR ||
         opc == PCD_DIVWORLDVAR ||
         opc == PCD_MODSCRIPTVAR ||
         opc == PCD_MODMAPVAR ||
         opc == PCD_MODWORLDVAR ||
         opc == PCD_INCSCRIPTVAR ||
         opc == PCD_INCMAPVAR ||
         opc == PCD_INCWORLDVAR ||
         opc == PCD_DECSCRIPTVAR ||
         opc == PCD_DECMAPVAR ||
         opc == PCD_DECWORLDVAR ||
         opc == PCD_ASSIGNGLOBALVAR ||
         opc == PCD_PUSHGLOBALVAR ||
         opc == PCD_ADDGLOBALVAR ||
         opc == PCD_SUBGLOBALVAR ||
         opc == PCD_MULGLOBALVAR ||
         opc == PCD_DIVGLOBALVAR ||
         opc == PCD_MODGLOBALVAR ||
         opc == PCD_INCGLOBALVAR ||
         opc == PCD_DECGLOBALVAR ||
         opc == PCD_CALL ||
         opc == PCD_CALLDISCARD ||
         opc == PCD_PUSHMAPARRAY ||
         opc == PCD_ASSIGNMAPARRAY ||
         opc == PCD_ADDMAPARRAY ||
         opc == PCD_SUBMAPARRAY ||
         opc == PCD_MULMAPARRAY ||
         opc == PCD_DIVMAPARRAY ||
         opc == PCD_MODMAPARRAY ||
         opc == PCD_INCMAPARRAY ||
         opc == PCD_DECMAPARRAY ||
         opc == PCD_PUSHWORLDARRAY ||
         opc == PCD_ASSIGNWORLDARRAY ||
         opc == PCD_ADDWORLDARRAY ||
         opc == PCD_SUBWORLDARRAY ||
         opc == PCD_MULWORLDARRAY ||
         opc == PCD_DIVWORLDARRAY ||
         opc == PCD_MODWORLDARRAY ||
         opc == PCD_INCWORLDARRAY ||
         opc == PCD_DECWORLDARRAY ||
         opc == PCD_PUSHGLOBALARRAY ||
         opc == PCD_ASSIGNGLOBALARRAY ||
         opc == PCD_ADDGLOBALARRAY ||
         opc == PCD_SUBGLOBALARRAY ||
         opc == PCD_MULGLOBALARRAY ||
         opc == PCD_DIVGLOBALARRAY ||
         opc == PCD_MODGLOBALARRAY ||
         opc == PCD_INCGLOBALARRAY ||
         opc == PCD_DECGLOBALARRAY ||
         opc == PCD_LSPEC5RESULT ||
         opc == PCD_ANDSCRIPTVAR ||
         opc == PCD_ANDMAPVAR ||
         opc == PCD_ANDGLOBALVAR ||
         opc == PCD_ANDMAPARRAY ||
         opc == PCD_ANDWORLDARRAY ||
         opc == PCD_ANDGLOBALARRAY ||
         opc == PCD_EORSCRIPTVAR ||
         opc == PCD_EORMAPVAR ||
         opc == PCD_EORWORLDVAR ||
         opc == PCD_EORGLOBALVAR ||
         opc == PCD_EORMAPARRAY ||
         opc == PCD_EORWORLDARRAY ||
         opc == PCD_EORGLOBALARRAY ||
         opc == PCD_ORSCRIPTVAR ||
         opc == PCD_ORMAPVAR ||
         opc == PCD_ORWORLDVAR ||
         opc == PCD_ORGLOBALVAR ||
         opc == PCD_ORMAPARRAY ||
         opc == PCD_ORWORLDARRAY ||
         opc == PCD_ORGLOBALARRAY ||
         opc == PCD_LSSCRIPTVAR ||
         opc == PCD_LSMAPVAR ||
         opc == PCD_LSWORLDVAR ||
         opc == PCD_LSGLOBALVAR ||
         opc == PCD_LSMAPARRAY ||
         opc == PCD_LSWORLDARRAY ||
         opc == PCD_LSGLOBALARRAY ||
         opc == PCD_RSSCRIPTVAR ||
         opc == PCD_RSMAPVAR ||
         opc == PCD_RSWORLDVAR ||
         opc == PCD_RSGLOBALVAR ||
         opc == PCD_RSMAPARRAY ||
         opc == PCD_RSWORLDARRAY ||
         opc == PCD_RSGLOBALARRAY ||
         opc == PCD_PUSHFUNCTION ) {
         int arg = 0;
         if ( object->small_code ) {
            char ch = 0;
            memcpy( &ch, data, sizeof( int ) );
            arg = ( int ) ch;
            ++data;
         }
         else {
            memcpy( &arg, data, sizeof( int ) );
            data += sizeof( int );
         }
         printf( " %d\n", arg );
      }
      else if ( opc == PCD_LSPEC1DIRECT ) {
         int id = 0;
         int arg = 0;
         if ( object->small_code ) {
            id = ( int ) ( unsigned char ) *data;
            ++data;
         }
         else {
            memcpy( &id, data, sizeof( int ) );
            data += sizeof( int );
         }
         memcpy( &arg, data, sizeof( arg ) );
         data += sizeof( int );
         printf( " %d %d\n", id, arg );
      }
      else if ( opc == PCD_LSPEC2DIRECT ) {
         int id = 0;
         int args[ 2 ];
         if ( object->small_code ) {
            id = ( int ) ( unsigned char ) *data;
            ++data;
         }
         else {
            memcpy( &id, data, sizeof( int ) );
            data += sizeof( int );
         }
         memcpy( args, data, sizeof( args ) );
         data += sizeof( args );
         printf( " %d %d %d\n",
            id,
            args[ 0 ],
            args[ 1 ] );
      }
      else if ( opc == PCD_LSPEC3DIRECT ) {
         int id = 0;
         int args[ 3 ];
         if ( object->small_code ) {
            id = ( int ) ( unsigned char ) *data;
            ++data;
         }
         else {
            memcpy( &id, data, sizeof( int ) );
            data += sizeof( int );
         }
         memcpy( args, data, sizeof( args ) );
         data += sizeof( args );
         printf( " %d %d %d %d\n",
            id,
            args[ 0 ],
            args[ 1 ],
            args[ 2 ] );
      }
      else if ( opc == PCD_LSPEC4DIRECT ) {
         int id = 0;
         int args[ 4 ];
         if ( object->small_code ) {
            id = ( int ) ( unsigned char ) *data;
            ++data;
         }
         else {
            memcpy( &id, data, sizeof( int ) );
            data += sizeof( int );
         }
         memcpy( args, data, sizeof( args ) );
         data += sizeof( args );
         printf( " %d %d %d %d %d\n",
            id,
            args[ 0 ],
            args[ 1 ],
            args[ 2 ],
            args[ 3 ] );
      }
      else if ( opc == PCD_LSPEC5DIRECT ) {
         int id = 0;
         int args[ 5 ];
         if ( object->small_code ) {
            id = ( int ) ( unsigned char ) *data;
            ++data;
         }
         else {
            memcpy( &id, data, sizeof( int ) );
            data += sizeof( int );
         }
         memcpy( args, data, sizeof( args ) );
         data += sizeof( args );
         printf( " %d %d %d %d %d %d\n",
            id,
            args[ 0 ],
            args[ 1 ],
            args[ 2 ],
            args[ 3 ],
            args[ 4 ] );
      }
      else if ( opc == PCD_LSPEC1DIRECTB ) {
         printf( " %hhd %hhd\n",
            ( unsigned char ) data[ 0 ],
            data[ 1 ] );
         data += 2;
      }
      else if ( opc == PCD_LSPEC2DIRECTB ) {
         printf( " %hhd %hhd %hhd\n",
            ( unsigned char ) data[ 0 ],
            data[ 1 ],
            data[ 2 ] );
         data += 3;
      }
      else if ( opc == PCD_LSPEC3DIRECTB ) {
         printf( " %hhd %hhd %hhd %hhd\n",
            ( unsigned char ) data[ 0 ],
            data[ 1 ],
            data[ 2 ],
            data[ 3 ] );
         data += 4;
      }
      else if ( opc == PCD_LSPEC4DIRECTB ) {
         printf( " %hhd %hhd %hhd %hhd %hhd\n",
            ( unsigned char ) data[ 0 ],
            data[ 1 ],
            data[ 2 ],
            data[ 3 ],
            data[ 4 ] );
         data += 5;
      }
      else if ( opc == PCD_LSPEC5DIRECTB ) {
         printf( " %hhd %hhd %hhd %hhd %hhd %hhd\n",
            ( unsigned char ) data[ 0 ],
            data[ 1 ],
            data[ 2 ],
            data[ 3 ],
            data[ 4 ],
            data[ 5 ] );
         data += 6;
      }
      else if (
         opc == PCD_PUSHBYTE ||
         opc == PCD_DELAYDIRECTB ) {
         printf( " %hhd\n", *data );
         ++data;
      }
      else if (
         opc == PCD_PUSH2BYTES ||
         opc == PCD_RANDOMDIRECTB ) {
         printf( " %hhd %hhd\n",
            data[ 0 ],
            data[ 1 ] );
         data += 2;
      }
      else if ( opc == PCD_PUSH3BYTES ) {
         printf( " %hhd %hhd %hhd\n",
            data[ 0 ],
            data[ 1 ],
            data[ 2 ] );
         data += 3;
      }
      else if ( opc == PCD_PUSH4BYTES ) {
         printf( " %hhd %hhd %hhd %hhd\n",
            data[ 0 ],
            data[ 1 ],
            data[ 2 ],
            data[ 3 ] );
         data += 4;
      }
      else if ( opc == PCD_PUSH5BYTES ) {
         printf( " %hhd %hhd %hhd %hhd %hhd\n",
            data[ 0 ],
            data[ 1 ],
            data[ 2 ],
            data[ 3 ],
            data[ 4 ] );
         data += 5;
      }
      else if ( opc == PCD_PUSHBYTES ) {
         int count = ( int ) ( unsigned char ) *data;
         printf( " count=%d", count );
         ++data;
         while ( count ) {
            printf( " %hhd", *data );
            ++data;
            --count;
         }
         printf( "\n" );
      }
      else if ( opc == PCD_CASEGOTOSORTED ) {
         // Count and cases are 4-byte aligned.
         int rem = ( offset + ( data - data_start ) ) % sizeof( int );
         if ( rem ) {
            data += sizeof( int ) - rem;
         }
         int count = 0;
         memcpy( &count, data, sizeof( int ) );
         data += sizeof( int );
         printf( " num-cases=%d\n", count );
         while ( count ) {
            int value = 0;
            memcpy( &value, data, sizeof( int ) );
            printf( "%08ld>   case %d: ",
               offset + ( data - data_start ),
               value );
            data += sizeof( int );
            memcpy( &value, data, sizeof( int ) );
            data += sizeof( int );
            printf( "%d\n", value );
            --count;
         }
      }
      else if ( opc == PCD_CALLFUNC ) {
         int num_args = 0;
         if ( object->small_code ) {
            char ch = 0;
            memcpy( &ch, data, sizeof( int ) );
            num_args = ( int ) ch;
            ++data;
         }
         else {
            memcpy( &num_args, data, sizeof( int ) );
            data += sizeof( int );
         }
         int index = 0;
         if ( object->small_code ) {
            short s = 0;
            memcpy( &s, data, sizeof( short ) );
            data += sizeof( short );
            index = ( int ) s;
         }
         else {
            memcpy( &index, data, sizeof( int ) );
            data += sizeof( int );
         }
         printf( " %d %d\n", num_args, index );
      }
      // For instructions that don't require any special handling of the
      // arguments, output arguments as integers.
      else if ( g_pcodes[ opc ].num_args ) {
         int count = g_pcodes[ opc ].num_args;
         while ( count ) {
            int arg = 0;
            memcpy( &arg, data, sizeof( int ) );
            data += sizeof( int );
            printf( " %d", arg );
            --count;
         }
         printf( "\n" );
      }
      // No arguments.
      else {
         printf( "\n" );
      }
   }
}

void show_sflg( struct chunk* chunk ) {
   int pos = 0;
   while ( pos < chunk->size ) {
      enum {
         FLAG_NET = 1,
         FLAG_CLIENTSIDE = 2
      };
      struct {
         short number;
         short flags;
      } entry;
      memcpy( &entry, chunk->data + pos, sizeof( entry ) );
      pos += sizeof( entry );
      printf( "script=%hd flags=", entry.number );
      if ( entry.flags & FLAG_NET ) {
         printf( "net" );
      }
      if ( entry.flags & FLAG_CLIENTSIDE ) {
         if ( entry.flags ^ FLAG_CLIENTSIDE ) {
            printf( "|" );
         }
         printf( "clientside" );
      }
      printf( "\n" );
   }
}

void show_svct( struct chunk* chunk ) {
   int pos = 0;
   while ( pos < chunk->size ) {
      struct {
         short number;
         short size;
      } entry;
      memcpy( &entry, chunk->data + pos, sizeof( chunk ) );
      pos += sizeof( entry );
      printf( "script=%hd new-size=%hd\n", entry.number, entry.size );
   }
}

void show_strl( struct chunk* chunk, bool is_encoded ) {
   const char* data = chunk->data;
   data += sizeof( int );
   int count = 0;
   memcpy( &count, data, sizeof( int ) );
   data += sizeof( int );
   data += sizeof( int );
   printf( "table-size=%d\n", count );
   for ( int i = 0; i < count; ++i ) {
      int offset = 0;
      memcpy( &offset, data, sizeof( int ) );
      data += sizeof( int );
      printf( "[%d] offset=%d", i, offset );
      printf( " " );
      printf( "\"" );
      const char* ch = chunk->data + offset;
      int k = 0;
      while ( true ) {
         char decoded = *ch;
         if ( is_encoded ) {
            decoded = decoded ^ ( offset * 157135 + k / 2 );
            ++k;
         }
         if ( ! decoded ) {
            break;
         }
         // Make the output of some characters more pretty.
         if ( decoded == '"' ) {
            printf( "\\\"" );
         }
         else if ( decoded == '\r' ) {
            printf( "\\r" );
         }
         else if ( decoded == '\n' ) {
            printf( "\\n" );
         }
         else {
            printf( "%c", decoded );
         }
         ++ch;
      }
      printf( "\"" );
      printf( "\n" );
   }
}

int get_chunk_type( const char* name ) {
   char buff[ 5 ];
   memcpy( buff, name, 4 );
   for ( int i = 0; i < 4; ++i ) {
      buff[ i ] = toupper( buff[ i ] );
   }
   buff[ 4 ] = 0;
   static const struct {
      const char* name;
      int type;
   } supported[] = {
      { "ARAY", CHUNK_ARAY },
      { "AINI", CHUNK_AINI },
      { "AIMP", CHUNK_AIMP },
      { "ASTR", CHUNK_ASTR },
      { "MSTR", CHUNK_MSTR },
      { "LOAD", CHUNK_LOAD },
      { "FUNC", CHUNK_FUNC },
      { "FNAM", CHUNK_FNAM },
      { "MINI", CHUNK_MINI },
      { "MIMP", CHUNK_MIMP },
      { "MEXP", CHUNK_MEXP },
      { "SPTR", CHUNK_SPTR },
      { "SFLG", CHUNK_SFLG },
      { "SVCT", CHUNK_SVCT },
      { "STRL", CHUNK_STRL },
      { "STRE", CHUNK_STRE },
      { "", CHUNK_UNKNOWN }
   };
   int i = 0;
   while ( supported[ i ].type != CHUNK_UNKNOWN &&
      strcmp( buff, supported[ i ].name ) != 0 ) {
      ++i;
   }
   return supported[ i ].type;
}

bool view_chunk( struct object* object, const char* name ) {
   int type = get_chunk_type( name );
   if ( type == CHUNK_UNKNOWN ) {
      printf( "error: unsupported chunk: %s\n", name );
      return false;
   }
   struct chunk chunk;
   struct chunk_read read;
   init_chunk_read( object, &read );
   bool found = false;
   while ( read_chunk( &read, &chunk ) ) {
      if ( chunk.type == type ) {
         show_chunk( object, &chunk, true );
         found = true;
      }
   }
   if ( found ) {
      return true;
   }
   else {
      printf( "error: `%s` chunk not found\n", name );
      return false;
   }
}

void init_chunk_read( struct object* object, struct chunk_read* read ) {
   read->data = object->data;
   read->data_size = object->size;
   read->pos = object->chunk_offset;
}

bool read_chunk( struct chunk_read* read, struct chunk* chunk ) {
   if ( read->pos < read->data_size ) {
      init_chunk( chunk, read->data + read->pos );
      read->pos += sizeof( int ) * 2 + chunk->size;
      return true;
   }
   else {
      return false;
   }
}

void view_all_chunks( struct object* object ) {
   struct chunk chunk;
   struct chunk_read read;
   init_chunk_read( object, &read );
   while ( read_chunk( &read, &chunk ) ) {
      show_chunk( object, &chunk, true );
   }
}