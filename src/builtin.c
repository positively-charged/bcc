#include <stdint.h>

#include "task.h"
#include "codegen/pcode.h"

#define F_LATENT 0x8000u
#define F_ALL F_LATENT
#define F_ISSET( info, flag ) ( !! ( info & flag ) )
#define F_GETOPCODE( info ) ( info & ~F_ALL )

struct setup {
   struct task* task;
   struct func* func;
   struct param* param;
   struct param* param_tail;
   struct expr* empty_string_expr;
   const char* format;
   int lang;
};

static void init_setup( struct setup* setup, struct task* task, int lang );
static void setup_func( struct setup* setup, int entry );
static void setup_return_type( struct setup* setup );
static void setup_param_list( struct setup* setup );
static void setup_param_list_acs( struct setup* setup );
static void setup_param_list_bcs( struct setup* setup );
static void setup_default_value( struct setup* setup, struct param* param,
   int param_number );
static void setup_empty_string_default_value( struct setup* setup,
   struct param* param );
static void append_param( struct setup* setup, struct param* param );

struct {
   const char* name;
   const char* format;
} g_funcs[] = {
   // Format:
   // [<return-type>] [; <required-parameters> [; <optional-parameters> ]]
   { "delay", ";i" },
   { "random", "i;ii" },
   { "thingcount", "i;i;i" },
   { "tagwait", ";i" },
   { "polywait", ";i" },
   { "changefloor", ";is" },
   { "changeceiling", ";is" },
   { "lineside", "i" },
   { "scriptwait", ";i" },
   { "clearlinespecial", "" },
   { "playercount", "i" },
   { "gametype", "i" },
   { "gameskill", "i" },
   { "timer", "i" },
   { "sectorsound", ";si" },
   { "ambientsound", ";si" },
   { "soundsequence", ";s" },
   { "setlinetexture", ";iiis" },
   { "setlineblocking", ";ii" },
   { "setlinespecial", ";ii;rrrrr" },
   { "thingsound", ";isi" },
   { "activatorsound", ";si" },
   { "localambientsound", ";si" },
   { "setlinemonsterblocking", ";ii" },
   { "isnetworkgame", "b" },
   { "playerteam", "i" },
   { "playerhealth", "i" },
   { "playerarmorpoints", "i" },
   { "playerfrags", "i" },
   { "bluecount", "i" },
   { "blueteamcount", "i" },
   { "redcount", "i" },
   { "redteamcount", "i" },
   { "bluescore", "i" },
   { "blueteamscore", "i" },
   { "redscore", "i" },
   { "redteamscore", "i" },
   { "isoneflagctf", "b" },
   { "getinvasionwave", "i" },
   { "getinvasionstate", "i" },
   { "music_change", ";si" },
   { "consolecommand", ";s;ii" },
   { "singleplayer", "b" },
   { "fixedmul", "f;ff" },
   { "fixeddiv", "f;ff" },
   { "setgravity", ";f" },
   { "setaircontrol", ";f" },
   { "clearinventory", "" },
   { "giveinventory", ";si" },
   { "takeinventory", ";si" },
   { "checkinventory", "i;s" },
   { "spawn", "i;sfff;ii" },
   { "spawnspot", "i;si;ii" },
   { "setmusic", ";s;ii" },
   { "localsetmusic", ";s;ii" },
   { "setfont", ";s" },
   { "setthingspecial", ";ii;rrrrr" },
   { "fadeto", ";iiiff" },
   { "faderange", ";iiifiiiff" },
   { "cancelfade", "" },
   { "playmovie", "i;s" },
   { "setfloortrigger", ";iii;rrrrr" },
   { "setceilingtrigger", ";iii;rrrrr" },
   { "getactorx", "f;i" },
   { "getactory", "f;i" },
   { "getactorz", "f;i" },
   { "sin", "f;f" },
   { "cos", "f;f" },
   { "vectorangle", "f;ff" },
   { "checkweapon", "b;s" },
   { "setweapon", "b;s" },
   { "setmarineweapon", ";ii" },
   { "setactorproperty", ";iir" },
   { "getactorproperty", "r;ii" },
   { "playernumber", "i" },
   { "activatortid", "i" },
   { "setmarinesprite", ";is" },
   { "getscreenwidth", "i" },
   { "getscreenheight", "i" },
   { "thing_projectile2", ";iiiiiii" },
   { "strlen", "i;s" },
   { "sethudsize", ";iib" },
   { "getcvar", "i;s" },
   { "setresultvalue", ";i" },
   { "getlinerowoffset", "i" },
   { "getactorfloorz", "f;i" },
   { "getactorangle", "f;i" },
   { "getsectorfloorz", "f;iii" },
   { "getsectorceilingz", "f;iii" },
   { "getsigilpieces", "i" },
   { "getlevelinfo", "i;i" },
   { "changesky", ";ss" },
   { "playeringame", "b;i" },
   { "playerisbot", "b;i" },
   { "setcameratotexture", ";isi" },
   { "getammocapacity", "i;s" },
   { "setammocapacity", ";si" },
   { "setactorangle", ";if" },
   { "spawnprojectile", ";isiiiii" },
   { "getsectorlightlevel", "i;i" },
   { "getactorceilingz", "f;i" },
   { "setactorposition", "b;ifffb" },
   { "clearactorinventory", ";i" },
   { "giveactorinventory", ";isi" },
   { "takeactorinventory", ";isi" },
   { "checkactorinventory", "i;is" },
   { "thingcountname", "i;si" },
   { "spawnspotfacing", "i;si;i" },
   { "playerclass", "i;i" },
   { "getplayerinfo", "i;ii" },
   { "changelevel", ";sii;i" },
   { "sectordamage", ";iissi" },
   { "replacetextures", ";ss;i" },
   { "getactorpitch", "f;i" },
   { "setactorpitch", ";if" },
   { "setactorstate", "i;is;b" },
   { "thing_damage2", "i;iis" },
   { "useinventory", "i;s" },
   { "useactorinventory", "i;is" },
   { "checkactorceilingtexture", "b;is" },
   { "checkactorfloortexture", "b;is" },
   { "getactorlightlevel", "i;i" },
   { "setmugshotstate", ";s" },
   { "thingcountsector", "i;iii" },
   { "thingcountnamesector", "i;sii" },
   { "checkplayercamera", "i;i" },
   { "morphactor", "i;i;ssiiss" },
   { "unmorphactor", "i;i;i" },
   { "getplayerinput", "i;ii" },
   { "classifyactor", "i;i" },
   { "namedscriptwait", ";s" },
   // Format functions
   // -----------------------------------------------------------------------
   { "print", "" },
   { "printbold", "" },
   { "hudmessage", ";iiifff;fff" },
   { "hudmessagebold", ";iiifff;fff" },
   { "log", "" },
   { "strparam", "s" },
   // Internal functions (Must be last.)
   // -----------------------------------------------------------------------
   { "acs_executewait", ";i;rrrr" },
   { "acs_namedexecutewait", ";s;rrrr" },
};

struct {
   uint16_t info;
} g_deds[] = {
   { PCD_DELAY | F_LATENT },
   { PCD_RANDOM },
   { PCD_THINGCOUNT },
   { PCD_TAGWAIT | F_LATENT },
   { PCD_POLYWAIT | F_LATENT },
   { PCD_CHANGEFLOOR },
   { PCD_CHANGECEILING },
   { PCD_LINESIDE },
   { PCD_SCRIPTWAIT | F_LATENT },
   { PCD_CLEARLINESPECIAL },
   { PCD_PLAYERCOUNT },
   { PCD_GAMETYPE },
   { PCD_GAMESKILL },
   { PCD_TIMER },
   { PCD_SECTORSOUND },
   { PCD_AMBIENTSOUND },
   { PCD_SOUNDSEQUENCE },
   { PCD_SETLINETEXTURE },
   { PCD_SETLINEBLOCKING },
   { PCD_SETLINESPECIAL },
   { PCD_THINGSOUND },
   { PCD_ACTIVATORSOUND },
   { PCD_LOCALAMBIENTSOUND },
   { PCD_SETLINEMONSTERBLOCKING },
   { PCD_ISNETWORKGAME },
   { PCD_PLAYERTEAM },
   { PCD_PLAYERHEALTH },
   { PCD_PLAYERARMORPOINTS },
   { PCD_PLAYERFRAGS },
   { PCD_BLUETEAMCOUNT },
   { PCD_BLUETEAMCOUNT },
   { PCD_REDTEAMCOUNT },
   { PCD_REDTEAMCOUNT },
   { PCD_BLUETEAMSCORE },
   { PCD_BLUETEAMSCORE },
   { PCD_REDTEAMSCORE },
   { PCD_REDTEAMSCORE },
   { PCD_ISONEFLAGCTF },
   { PCD_GETINVASIONWAVE },
   { PCD_GETINVASIONSTATE },
   { PCD_MUSICCHANGE },
   { PCD_CONSOLECOMMAND },
   { PCD_SINGLEPLAYER },
   { PCD_FIXEDMUL },
   { PCD_FIXEDDIV },
   { PCD_SETGRAVITY },
   { PCD_SETAIRCONTROL },
   { PCD_CLEARINVENTORY },
   { PCD_GIVEINVENTORY },
   { PCD_TAKEINVENTORY },
   { PCD_CHECKINVENTORY },
   { PCD_SPAWN },
   { PCD_SPAWNSPOT },
   { PCD_SETMUSIC },
   { PCD_LOCALSETMUSIC },
   { PCD_SETFONT },
   { PCD_SETTHINGSPECIAL },
   { PCD_FADETO },
   { PCD_FADERANGE },
   { PCD_CANCELFADE },
   { PCD_PLAYMOVIE },
   { PCD_SETFLOORTRIGGER },
   { PCD_SETCEILINGTRIGGER },
   { PCD_GETACTORX },
   { PCD_GETACTORY },
   { PCD_GETACTORZ },
   { PCD_SIN },
   { PCD_COS },
   { PCD_VECTORANGLE },
   { PCD_CHECKWEAPON },
   { PCD_SETWEAPON },
   { PCD_SETMARINEWEAPON },
   { PCD_SETACTORPROPERTY },
   { PCD_GETACTORPROPERTY },
   { PCD_PLAYERNUMBER },
   { PCD_ACTIVATORTID },
   { PCD_SETMARINESPRITE },
   { PCD_GETSCREENWIDTH },
   { PCD_GETSCREENHEIGHT },
   { PCD_THINGPROJECTILE2 },
   { PCD_STRLEN },
   { PCD_SETHUDSIZE },
   { PCD_GETCVAR },
   { PCD_SETRESULTVALUE },
   { PCD_GETLINEROWOFFSET },
   { PCD_GETACTORFLOORZ },
   { PCD_GETACTORANGLE },
   { PCD_GETSECTORFLOORZ },
   { PCD_GETSECTORCEILINGZ },
   { PCD_GETSIGILPIECES },
   { PCD_GETLEVELINFO },
   { PCD_CHANGESKY },
   { PCD_PLAYERINGAME },
   { PCD_PLAYERISBOT },
   { PCD_SETCAMERATOTEXTURE },
   { PCD_GETAMMOCAPACITY },
   { PCD_SETAMMOCAPACITY },
   { PCD_SETACTORANGLE },
   { PCD_SPAWNPROJECTILE },
   { PCD_GETSECTORLIGHTLEVEL },
   { PCD_GETACTORCEILINGZ },
   { PCD_SETACTORPOSITION },
   { PCD_CLEARACTORINVENTORY },
   { PCD_GIVEACTORINVENTORY },
   { PCD_TAKEACTORINVENTORY },
   { PCD_CHECKACTORINVENTORY },
   { PCD_THINGCOUNTNAME },
   { PCD_SPAWNSPOTFACING },
   { PCD_PLAYERCLASS },
   { PCD_GETPLAYERINFO },
   { PCD_CHANGELEVEL },
   { PCD_SECTORDAMAGE },
   { PCD_REPLACETEXTURES },
   { PCD_GETACTORPITCH },
   { PCD_SETACTORPITCH },
   { PCD_SETACTORSTATE },
   { PCD_THINGDAMAGE2 },
   { PCD_USEINVENTORY },
   { PCD_USEACTORINVENTORY },
   { PCD_CHECKACTORCEILINGTEXTURE },
   { PCD_CHECKACTORFLOORTEXTURE },
   { PCD_GETACTORLIGHTLEVEL },
   { PCD_SETMUGSHOTSTATE },
   { PCD_THINGCOUNTSECTOR },
   { PCD_THINGCOUNTNAMESECTOR },
   { PCD_CHECKPLAYERCAMERA },
   { PCD_MORPHACTOR },
   { PCD_UNMORPHACTOR },
   { PCD_GETPLAYERINPUT },
   { PCD_CLASSIFYACTOR },
   { PCD_SCRIPTWAITNAMED | F_LATENT },
};

struct {
   uint16_t opcode;
} g_formats[] = {
   { PCD_ENDPRINT },
   { PCD_ENDPRINTBOLD },
   { PCD_ENDHUDMESSAGE },
   { PCD_ENDHUDMESSAGEBOLD },
   { PCD_ENDLOG },
   { PCD_SAVESTRING },
};

struct {
   uint16_t id;
} g_interns[] = {
   { INTERN_FUNC_ACS_EXECWAIT },
   { INTERN_FUNC_ACS_NAMEDEXECUTEWAIT },
};

enum {
   BOUND_DED = ARRAY_SIZE( g_deds ),
   BOUND_DED_ACS95 = 21,
   BOUND_FORMAT = BOUND_DED + ARRAY_SIZE( g_formats )
};

void t_create_builtins( struct task* task, int lang ) {
   struct setup setup;
   init_setup( &setup, task, lang );
   enum { TOTAL_IMPLS =
      ARRAY_SIZE( g_deds ) +
      ARRAY_SIZE( g_formats ) +
      ARRAY_SIZE( g_interns ) };
   if ( ARRAY_SIZE( g_funcs ) != TOTAL_IMPLS ) {
      t_diag( task, DIAG_INTERNAL | DIAG_ERR,
         "builtin function declarations (%zu) != implementations (%zu)",
         ARRAY_SIZE( g_funcs ), TOTAL_IMPLS );
      t_bail( task );
   }
   if ( lang == LANG_ACS95 ) {
      // Dedicated functions.
      for ( int entry = 0; entry < BOUND_DED_ACS95; ++entry ) {
         setup_func( &setup, entry );
      }
      // Format functions.
      setup_func( &setup, BOUND_DED );
      setup_func( &setup, BOUND_DED + 1 );
   }
   else {
      for ( int entry = 0; entry < ARRAY_SIZE( g_funcs ); ++entry ) {
         setup_func( &setup, entry );
      }
   }
}

void init_setup( struct setup* setup, struct task* task, int lang ) {
   setup->task = task;
   setup->func = NULL;
   setup->param = NULL;
   setup->param_tail = NULL;
   setup->empty_string_expr = NULL;
   setup->format = NULL;
   setup->lang = lang;
}

void setup_func( struct setup* setup, int entry ) {
   struct func* func = t_alloc_func();
   t_init_pos_id( &func->object.pos, ALTERN_FILENAME_COMPILER );
   func->object.resolved = true;
   func->name = t_extend_name( t_extend_name( setup->task->root_name, "." ),
      g_funcs[ entry ].name );
   func->name->object = &func->object;
   // Dedicated function.
   if ( entry < BOUND_DED ) {
      struct func_ded* impl = mem_alloc( sizeof( *impl ) );
      impl->opcode = F_GETOPCODE( g_deds[ entry ].info );
      impl->latent = F_ISSET( g_deds[ entry ].info, F_LATENT );
      func->type = FUNC_DED;
      func->impl = impl;
   }
   // Format function.
   else if ( entry < BOUND_FORMAT ) {
      int impl_entry = entry - BOUND_DED;
      struct func_format* impl = mem_alloc( sizeof( *impl ) );
      impl->opcode = g_formats[ impl_entry ].opcode;
      func->type = FUNC_FORMAT;
      func->impl = impl;
   }
   // Internal function.
   else {
      int impl_entry = entry - BOUND_FORMAT;
      struct func_intern* impl = mem_alloc( sizeof( *impl ) );
      impl->id = g_interns[ impl_entry ].id;
      func->type = FUNC_INTERNAL;
      func->impl = impl;
   }
   setup->func = func;
   setup->format = g_funcs[ entry ].format;
   setup_return_type( setup );
   if ( setup->format[ 0 ] == ';' ) {
      ++setup->format;
      setup_param_list( setup );
   }
}

void setup_return_type( struct setup* setup ) {
   int spec = SPEC_VOID;
   if ( ! (
      setup->format[ 0 ] == '\0' ||
      setup->format[ 0 ] == ';' ) ) { 
      switch ( setup->format[ 0 ] ) {
      case 'i': spec = SPEC_INT; break;
      case 'r': spec = SPEC_RAW; break;
      case 'f': spec = SPEC_FIXED; break;
      case 'b': spec = SPEC_BOOL; break;
      case 's': spec = SPEC_STR; break;
      default:
         t_diag( setup->task, DIAG_INTERNAL | DIAG_ERR,
            "invalid builtin function return type `%c`", setup->format[ 0 ] );
         t_bail( setup->task );
      }
      switch ( setup->lang ) {
      case LANG_ACS:
      case LANG_ACS95:
         if ( spec != SPEC_VOID ) {
            spec = SPEC_RAW;
         }
         break;
      default:
         break;
      }
      ++setup->format;
   }
   setup->func->return_spec = spec;
}

void setup_param_list( struct setup* setup ) {
   switch ( setup->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      setup_param_list_acs( setup );
      break;
   default:
      setup_param_list_bcs( setup );
   }
}

void setup_param_list_acs( struct setup* setup ) {
   // In ACS and ACS95, we're just interested in the parameter count.
   bool optional = false;
   while ( setup->format[ 0 ] ) {
      if ( setup->format[ 0 ] == ';' ) {
         optional = true;
      }
      else {
         if ( ! optional ) {
            ++setup->func->min_param;
         }
         ++setup->func->max_param;
      }
      ++setup->format;
   }
}

void setup_param_list_bcs( struct setup* setup ) {
   const char* format = setup->format;
   bool optional = false;
   while ( format[ 0 ] ) {
      int spec = SPEC_NONE;
      switch ( format[ 0 ] ) {
      case 'i': spec = SPEC_INT; break;
      case 'r': spec = SPEC_RAW; break;
      case 'f': spec = SPEC_FIXED; break;
      case 'b': spec = SPEC_BOOL; break;
      case 's': spec = SPEC_STR; break;
      case ';':
         optional = true;
         ++format;
         continue;
      default:
         t_diag( setup->task, DIAG_INTERNAL | DIAG_ERR,
            "invalid builtin function parameter `%c`", format[ 0 ] );
         t_bail( setup->task );
      }
      struct param* param = t_alloc_param();
      param->spec = spec;
      if ( optional ) {
         setup_default_value( setup, param, setup->func->max_param );
      }
      else {
         ++setup->func->min_param;
      }
      append_param( setup, param );
      ++setup->func->max_param;
      ++format;
   }
   setup->func->params = setup->param;
   setup->param = NULL;
   setup->param_tail = NULL;
}

void setup_default_value( struct setup* setup, struct param* param,
   int param_number ) {
   switch ( setup->func->type ) {
      struct func_ded* ded;
   case FUNC_DED:
      ded = setup->func->impl;
      switch ( ded->opcode ) {
      case PCD_MORPHACTOR:
         switch ( param_number ) {
         case 1: case 2: case 5: case 6:
            setup_empty_string_default_value( setup, param );
            return;
         }
      }
   default:
      break;
   }
   param->default_value = setup->task->dummy_expr;
}

void setup_empty_string_default_value( struct setup* setup,
   struct param* param ) {
   if ( ! setup->empty_string_expr ) {
      struct indexed_string* string = t_intern_string( setup->task, "", 0 );
      struct indexed_string_usage* usage = t_alloc_indexed_string_usage();
      usage->string = string;
      struct expr* expr = t_alloc_expr();
      t_init_pos_id( &expr->pos, ALTERN_FILENAME_COMPILER );
      expr->root = &usage->node;
      expr->spec = SPEC_STR;
      expr->value = string->index;
      expr->folded = true;
      expr->has_str = true;
      setup->empty_string_expr = expr;
   }
   param->default_value = setup->empty_string_expr;
}

void append_param( struct setup* setup, struct param* param ) {
   if ( setup->param ) {
      setup->param_tail->next = param;
   }
   else {
      setup->param = param;
   }
   setup->param_tail = param;
}