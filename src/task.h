#ifndef TASK_H
#define TASK_H

#include <stdio.h>
#include <stdbool.h>
#include <setjmp.h>

#include "common.h"

#define MAX_LIB_NAME_LENGTH 8

// Token types.
enum tk {
   // 0
   TK_END,
   TK_RESERVED,
   TK_BRACKET_L,
   TK_BRACKET_R,
   TK_PAREN_L,
   TK_PAREN_R,
   TK_BRACE_L,
   TK_BRACE_R,
   TK_DOT,
   TK_INC,
   // 10
   TK_DEC,
   TK_ID,
   TK_COMMA,
   TK_COLON,
   TK_COLON_2,
   TK_SEMICOLON,
   TK_ASSIGN,
   TK_ASSIGN_ADD,
   TK_ASSIGN_SUB,
   TK_ASSIGN_MUL,
   // 20
   TK_ASSIGN_DIV,
   TK_ASSIGN_MOD,
   TK_ASSIGN_SHIFT_L,
   TK_ASSIGN_SHIFT_R,
   TK_ASSIGN_BIT_AND,
   TK_ASSIGN_BIT_XOR,
   TK_ASSIGN_BIT_OR,
   TK_EQ,
   TK_NEQ,
   TK_LOG_NOT,
   // 30
   TK_LOG_AND,
   TK_LOG_OR,
   TK_BIT_AND,
   TK_BIT_OR,
   TK_BIT_XOR,
   TK_BIT_NOT,
   TK_LT,
   TK_LTE,
   TK_GT,
   TK_GTE,
   // 40
   TK_PLUS,
   TK_MINUS,
   TK_SLASH,
   TK_STAR,
   TK_MOD,
   TK_SHIFT_L,
   TK_SHIFT_R,
   TK_ASSIGN_COLON,
   TK_BREAK,
   TK_CASE,
   // 50
   TK_CONST,
   TK_CONTINUE,
   TK_DEFAULT,
   TK_DO,
   TK_ELSE,
   TK_ENUM,
   TK_FOR,
   TK_IF,
   TK_INT,
   TK_RETURN,
   // 60
   TK_STATIC,
   TK_STR,
   TK_STRUCT,
   TK_SWITCH,
   TK_VOID,
   TK_WHILE,
   TK_BOOL,
   TK_LIT_DECIMAL,
   TK_LIT_OCTAL,
   TK_LIT_HEX,
   // 70
   TK_LIT_CHAR,
   TK_LIT_FIXED,
   TK_LIT_STRING,
   TK_HASH,    // #
   TK_PALTRANS,
   TK_GLOBAL,
   TK_SCRIPT,
   TK_UNTIL,
   TK_WORLD,
   TK_OPEN,
   // 80
   TK_RESPAWN,
   TK_DEATH,
   TK_ENTER,
   TK_PICKUP,
   TK_BLUE_RETURN,
   TK_RED_RETURN,
   TK_WHITE_RETURN,
   TK_LIGHTNING,
   TK_DISCONNECT,
   TK_UNLOADING,
   // 90
   TK_CLIENTSIDE,
   TK_NET,
   TK_RESTART,
   TK_SUSPEND,
   TK_TERMINATE,
   TK_FUNCTION,
   TK_IMPORT,
   TK_REGION,
   TK_UPMOST,
   TK_GOTO,
   // 100
   TK_TRUE,
   TK_FALSE,
   TK_EVENT,
   TK_NL,
   TK_LIB,
   TK_LIB_END,
   TK_LIT_BINARY,
   TK_TOTAL
};

struct pos {
   int line;
   int column;
   int id;
};

struct token {
   char* text;
   struct pos pos;
   enum tk type;
   int length;
};

struct source {
   char* text;
   char* left;
   char* save;
   struct source* prev;
   struct fileid fileid;
   struct str path;
   struct str full_path;
   int line;
   int column;
   int id;
   int active_id;
   bool find_dirc;
   bool load_once;
   bool imported;
   char ch;
};

struct node {
   enum {
      NODE_NONE,
      NODE_LITERAL,
      NODE_UNARY,
      NODE_BINARY,
      NODE_EXPR,
      NODE_INDEXED_STRING_USAGE,
      NODE_JUMP,
      NODE_SCRIPT_JUMP,
      NODE_IF,
      NODE_WHILE,
      // 10
      NODE_FOR,
      NODE_CALL,
      NODE_FORMAT_ITEM,
      NODE_FORMAT_BLOCK_USAGE,
      NODE_FUNC,
      NODE_ACCESS,
      NODE_PAREN,
      NODE_SUBSCRIPT,
      NODE_CASE,
      NODE_CASE_DEFAULT,
      // 20
      NODE_SWITCH,
      NODE_BLOCK,
      NODE_GOTO,
      NODE_GOTO_LABEL,
      NODE_VAR,
      NODE_ASSIGN,
      NODE_TYPE,
      NODE_TYPE_MEMBER,
      NODE_CONSTANT,
      NODE_CONSTANT_SET,
      // 30
      NODE_RETURN,
      NODE_PARAM,
      NODE_PALTRANS,
      NODE_ALIAS,
      NODE_BOOLEAN,
      NODE_IMPORT,
      NODE_NAME_USAGE,
      NODE_REGION,
      NODE_REGION_HOST,
      NODE_REGION_UPMOST,
      // 40
      NODE_SCRIPT,
      NODE_PACKED_EXPR
   } type;
};

struct object {
   struct node node;
   short depth;
   bool resolved;
   struct pos pos;
   struct object* next;
   struct object* next_scope;
};

struct name {
   struct name* parent;
   struct name* next;
   struct name* drop;
   struct object* object;
   char ch;
};

struct name_usage {
   struct node node;
   char* text;
   struct node* object;
   struct pos pos;
   int lib_id;
};

struct path {
   struct path* next;
   char* text;
   struct pos pos;
   bool is_region;
   bool is_upmost;
};

struct type {
   struct object object;
   struct name* name;
   struct name* body;
   struct type_member* member;
   struct type_member* member_tail;
   int size;
   bool primitive;
   bool is_str;
   bool anon;
};

struct type_member {
   struct object object;
   struct name* name;
   struct type* type;
   struct path* type_path;
   struct dim* dim;
   struct type_member* next;
   int offset;
   int size;
};

struct paren {
   struct node node;
   struct node* inside;
};

struct literal {
   struct node node;
   int value;
};

struct boolean {
   struct node node;
   int value;
};

struct unary {
   struct node node;
   enum {
      UOP_NONE,
      UOP_MINUS,
      UOP_PLUS,
      UOP_LOG_NOT,
      UOP_BIT_NOT,
      UOP_PRE_INC,
      UOP_PRE_DEC,
      UOP_POST_INC,
      UOP_POST_DEC
   } op;
   struct node* operand;
   struct pos pos;
};

struct binary {
   struct node node;
   enum {
      BOP_NONE,
      BOP_LOG_OR,
      BOP_LOG_AND,
      BOP_BIT_OR,
      BOP_BIT_XOR,
      BOP_BIT_AND,
      BOP_EQ,
      BOP_NEQ,
      BOP_LT,
      BOP_LTE,
      BOP_GT,
      BOP_GTE,
      BOP_SHIFT_L,
      BOP_SHIFT_R,
      BOP_ADD,
      BOP_SUB,
      BOP_MUL,
      BOP_DIV,
      BOP_MOD
   } op;
   struct node* lside;
   struct node* rside;
   struct pos pos;
};

struct assign {
   struct node node;
   enum {
      AOP_NONE,
      AOP_ADD,
      AOP_SUB,
      AOP_MUL,
      AOP_DIV,
      AOP_MOD,
      AOP_SHIFT_L,
      AOP_SHIFT_R,
      AOP_BIT_AND,
      AOP_BIT_XOR,
      AOP_BIT_OR,
      AOP_TOTAL
   } op;
   struct node* lside;
   struct node* rside;
   struct pos pos;
};

struct subscript {
   struct node node;
   struct node* lside;
   struct expr* index;
   struct pos pos;
};

struct access {
   struct node node;
   struct pos pos;
   struct node* lside;
   struct node* rside;
   char* name;
   bool is_region;
};

struct call {
   struct node node;
   struct pos pos;
   struct node* operand;
   struct func* func;
   struct list args;
};

struct expr {
   struct node node;
   struct node* root;
   struct pos pos;
   int value;
   bool folded;
   bool has_str;
};

struct packed_expr {
   struct node node;
   struct expr* expr;
   struct block* block;
};

struct jump {
   struct node node;
   enum {
      JUMP_BREAK,
      JUMP_CONTINUE
   } type;
   struct jump* next;
   struct pos pos;
   int obj_pos;
};

struct script_jump {
   struct node node;
   enum {
      SCRIPT_JUMP_TERMINATE,
      SCRIPT_JUMP_RESTART,
      SCRIPT_JUMP_SUSPEND,
      SCRIPT_JUMP_TOTAL,
   } type;
   struct pos pos;
};

struct return_stmt {
   struct node node;
   struct packed_expr* return_value;
   struct pos pos;
};

struct block {
   struct node node;
   struct list stmts;
   struct pos pos;
};

struct if_stmt {
   struct node node;
   struct expr* cond;
   struct node* body;
   struct node* else_body;
};

struct case_label {
   struct node node;
   int offset;
   struct expr* number;
   struct case_label* next;
   struct pos pos;
};

struct switch_stmt {
   struct node node;
   struct expr* cond;
   // Cases are sorted by their value, in ascending order. Default case not
   // included in this list.
   struct case_label* case_head;
   struct case_label* case_default;
   struct jump* jump_break;
   struct node* body;
};

struct while_stmt {
   struct node node;
   enum {
      WHILE_WHILE,
      WHILE_UNTIL,
      WHILE_DO_WHILE,
      WHILE_DO_UNTIL
   } type;
   struct expr* cond;
   struct node* body;
   struct jump* jump_break;
   struct jump* jump_continue;
};

struct for_stmt {
   struct node node;
   struct list init;
   struct list post;
   struct expr* cond;
   struct node* body;
   struct jump* jump_break;
   struct jump* jump_continue;
};

struct dim {
   struct dim* next;
   // Number of elements.
   struct expr* size_node;
   int size;
   int element_size;
   struct pos pos;
};

struct initial {
   struct initial* next;
   bool multi;
   bool tested;
};

struct value {
   struct initial initial;
   struct expr* expr;
   struct value* next;
   int index;
   bool string_initz;
};

struct multi_value {
   struct initial initial;
   struct initial* body;
   struct pos pos;
   int padding;
};

enum {
   STORAGE_LOCAL,
   STORAGE_MAP,
   STORAGE_WORLD,
   STORAGE_GLOBAL
};

struct var {
   struct object object;
   struct name* name;
   struct type* type;
   struct path* type_path;
   struct dim* dim;
   struct initial* initial;
   struct value* value;
   struct var* next;
   int storage;
   int index;
   int size;
   bool initz_zero;
   bool hidden;
   bool used;
   bool initial_has_str;
   bool imported;
   bool is_constant_init;
};

struct param {
   struct object object;
   struct type* type;
   struct param* next;
   struct name* name;
   struct expr* default_value;
   int index;
   int obj_pos;
};

struct func {
   struct object object;
   enum {
      FUNC_ASPEC,
      FUNC_EXT,
      FUNC_DED,
      FUNC_FORMAT,
      FUNC_USER,
      FUNC_INTERNAL
   } type;
   struct name* name;
   struct param* params;
   struct type* return_type;
   void* impl;
   int min_param;
   int max_param;
   bool hidden;
};

struct func_aspec {
   int id;
   bool script_callable;
};

// An extension function doesn't have a unique pcode reserved for it, but is
// instead called using the PC_CALL_FUNC pcode.
struct func_ext {
   int id;
};

// A function with an opcode allocated to it.
struct func_ded {
   int opcode;
   bool latent;
};

struct format_item {
   struct node node;
   enum {
      FCAST_ARRAY,
      FCAST_BINARY,
      FCAST_CHAR,
      FCAST_DECIMAL,
      FCAST_FIXED,
      FCAST_KEY,
      FCAST_LOCAL_STRING,
      FCAST_NAME,
      FCAST_STRING,
      FCAST_HEX,
      FCAST_TOTAL
   } cast;
   struct pos pos;
   struct format_item* next;
   struct expr* value;
   void* extra;
};

struct format_item_array {
   struct expr* offset;
   struct expr* length;
};

struct format_block_usage {
   struct node node;
   struct block* block;
   struct format_block_usage* next;
   struct pos pos;
   int obj_pos;
};

// Format functions are dedicated functions and have their first parameter
// consisting of a list of format items.
struct func_format {
   int opcode;
};

// Functions created by the user.
struct func_user {
   struct list labels;
   struct block* body;
   int index;
   int size;
   int usage;
   int obj_pos;
   bool publish;
};

struct func_intern {
   enum {
      INTERN_FUNC_ACS_EXECWAIT,
      INTERN_FUNC_STANDALONE_TOTAL,
      INTERN_FUNC_STR_LENGTH = INTERN_FUNC_STANDALONE_TOTAL,
      INTERN_FUNC_STR_AT,
      INTERN_FUNC_MEMBER_TOTAL
   } id;
};

struct palrange {
   struct palrange* next;
   struct expr* begin;
   struct expr* end;
   union {
      struct {
         struct expr* begin;
         struct expr* end;
      } ent;
      struct {
         struct expr* red1;
         struct expr* green1;
         struct expr* blue1;
         struct expr* red2;
         struct expr* green2;
         struct expr* blue2;
      } rgb;
   } value;
   bool rgb;
};

struct paltrans {
   struct node node;
   struct expr* number;
   struct palrange* ranges;
   struct palrange* ranges_tail;
};

struct label {
   struct node node;
   int obj_pos;
   struct pos pos;
   char* name;
   struct goto_stmt* users;
   struct block* format_block;
   bool defined;
};

struct goto_stmt {
   struct node node;
   int obj_pos;
   struct label* label;
   struct goto_stmt* next;
   struct block* format_block;
   struct pos pos;
};

struct script {
   struct node node;
   struct pos pos;
   struct expr* number;
   enum {
      SCRIPT_TYPE_CLOSED,
      SCRIPT_TYPE_OPEN,
      SCRIPT_TYPE_RESPAWN,
      SCRIPT_TYPE_DEATH,
      SCRIPT_TYPE_ENTER,
      SCRIPT_TYPE_PICKUP,
      SCRIPT_TYPE_BLUE_RETURN,
      SCRIPT_TYPE_RED_RETURN,
      SCRIPT_TYPE_WHITE_RETURN,
      SCRIPT_TYPE_LIGHTNING = 12,
      SCRIPT_TYPE_UNLOADING,
      SCRIPT_TYPE_DISCONNECT,
      SCRIPT_TYPE_RETURN,
      SCRIPT_TYPE_EVENT
   } type;
   enum {
      SCRIPT_FLAG_NET = 0x1,
      SCRIPT_FLAG_CLIENTSIDE = 0x2
   } flags;
   struct param* params;
   struct node* body;
   struct list labels;
   int num_param;
   int offset;
   int size;
   bool publish;
};

struct alias {
   struct object object;
   struct object* target;
};

struct constant {
   struct object object;
   struct name* name;
   struct expr* value_node;
   struct constant* next;
   int value;
   int lib_id;
   bool hidden;
};

struct constant_set {
   struct object object;
   struct constant* head;
};

struct indexed_string {
   struct indexed_string* next;
   struct indexed_string* next_sorted;
   struct indexed_string* next_usable;
   char* value;
   int length;
   int index;
   bool in_constant;
   bool in_main_file;
   bool imported;
   bool used;
};

struct indexed_string_usage {
   struct node node;
   struct indexed_string* string;
};

struct str_table {
   struct indexed_string* head;
   struct indexed_string* head_sorted;
   struct indexed_string* head_usable;
   struct indexed_string* tail;
};

struct region {
   struct object object;
   struct name* name;
   struct name* body;
   struct name* body_struct;
   struct region_link {
      struct region_link* next;
      struct region* region;
      struct pos pos;
   }* link;
   struct object* unresolved;
   struct object* unresolved_tail;
   struct list imports;
   struct list items;
};

struct library {
   struct str name;
   struct pos name_pos;
   struct list vars;
   struct list funcs;
   struct list scripts;
   // #included/#imported libraries.
   struct list dynamic;
   struct name* hidden_names;
   struct pos file_pos;
   int id;
   enum {
      FORMAT_ZERO,
      FORMAT_BIG_E,
      FORMAT_LITTLE_E
   } format;
   bool importable;
   bool imported;
   bool encrypt_str;
};

struct import {
   struct node node;
   struct pos pos;
   // When NULL, use upmost region.
   struct path* path;
   // When NULL, it means make region link.
   struct import_item* item;
   struct import* next;
};

struct import_item {
   struct import_item* next;
   char* name;
   char* alias;
   struct pos pos;
   struct pos name_pos;
   struct pos alias_pos;
   bool is_struct;
   bool is_link;
};

struct dec {
   enum {
      DEC_TOP,
      DEC_LOCAL,
      DEC_FOR,
      DEC_MEMBER,
      DEC_TOTAL
   } area;
   struct pos pos;
   struct pos type_pos;
   struct pos storage_pos;
   struct pos name_pos;
   const char* storage_name;
   struct type* type;
   struct type* type_make;
   struct path* type_path;
   struct name* name;
   struct name* name_offset;
   struct dim* dim;
   struct initial* initial;
   struct list* vars;
   int storage;
   int storage_index;
   bool type_needed;
   bool type_void;
   bool type_struct;
   bool initz_str;
   bool is_static;
   bool leave;
};

struct stmt_reading {
   struct list* labels;
   struct node* node;
   struct block* block_node;
};

struct stmt_test {
   struct stmt_test* parent;
   struct func* func;
   struct list* labels;
   struct block* format_block;
   struct case_label* case_head;
   struct case_label* case_default;
   struct jump* jump_break;
   struct jump* jump_continue;
   struct import2* import;
   bool in_loop;
   bool in_switch;
   bool in_script;
   bool manual_scope;
};

struct expr_reading {
   struct node* node;
   struct expr* output_node;
   struct pos pos;
   bool has_str;
   bool in_constant;
   bool skip_assign;
   bool skip_call;
   bool expect_expr;
};

struct expr_test {
   jmp_buf bail;
   struct stmt_test* stmt_test;
   struct block* format_block;
   struct format_block_usage* format_block_usage;
   struct pos pos;
   bool result_required;
   bool has_string;
   bool undef_err;
   bool undef_erred;
   bool accept_array;
   bool suggest_paren_assign;
};

#define BUFFER_SIZE 65536

struct buffer {
   struct buffer* next;
   char data[ BUFFER_SIZE ];
   int used;
   int pos;
};

struct immediate {
   struct immediate* next;
   int value;
};

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

#define TK_BUFFER_SIZE 5

struct task {
   struct options* options;
   FILE* err_file;
   jmp_buf* bail;
   struct {
      struct token buffer[ TK_BUFFER_SIZE ];
      struct str text;
      int peeked;
   } tokens;
   enum tk tk;
   struct pos tk_pos;
   // The text contains the character content of the token. The text and the
   // length of the text are applicable only to a token that is an identifier,
   // a string, a character literal, or any of the numbers. For the rest, the
   // text will be NULL and the length will be zero.
   char* tk_text;
   int tk_length;
   struct str_table str_table;
   struct type* type_int;
   struct type* type_str;
   struct type* type_bool;
   struct region* region;
   struct region* region_upmost;
   struct source* source;
   struct source* source_main;
   struct library* library;
   struct library* library_main;
   struct list libraries;
   struct name* root_name;
   struct name* anon_name;
   struct list loaded_sources;
   struct list regions;
   struct list scripts;
   int depth;
   struct scope* scope;
   struct scope* free_scope;
   struct sweep* free_sweep;
   struct buffer* buffer_head;
   struct buffer* buffer;
   bool compress;
   bool in_func;
   int opc;
   int opc_args;
   struct immediate* immediate;
   struct immediate* immediate_tail;
   struct immediate* free_immediate;
   int immediate_count;
   bool push_immediate;
   struct block_visit* block_visit;
   struct block_visit* block_visit_free;
};

#define DIAG_NONE 0
#define DIAG_FILE 0x1
#define DIAG_LINE 0x2
#define DIAG_COLUMN 0x4
#define DIAG_WARN 0x8
#define DIAG_ERR 0x10
#define DIAG_POS_ERR DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN

void t_init( struct task*, struct options*, jmp_buf* );
void t_init_fields_token( struct task* );
void t_init_fields_dec( struct task* );
void t_init_fields_chunk( struct task* );
void t_init_fields_obj( struct task* );
void t_load_main_source( struct task* );
void t_read_tk( struct task* );
void t_test_tk( struct task*, enum tk );
void t_read( struct task* );
void t_test( struct task* );
void t_read_top_stmt( struct task*, struct stmt_reading*,
   bool need_block );
struct name* t_make_name( struct task*, const char*, struct name* );
struct format_item* t_read_format_item( struct task*, bool colon );
void t_add_scope( struct task* );
void t_pop_scope( struct task* );
void t_test_constant( struct task*, struct constant*, bool undef_err );
void t_test_constant_set( struct task*, struct constant_set*, bool undef_err );
void t_test_type( struct task*, struct type*, bool undef_err );
void t_copy_name( struct name*, bool full, struct str* buffer );
int t_full_name_length( struct name* );
void t_test_local_var( struct task*, struct var* );
void t_skip_block( struct task* );
void t_publish( struct task* );
void t_add_byte( struct task*, char );
void t_add_short( struct task*, short );
void t_add_int( struct task*, int );
void t_add_int_zero( struct task*, int );
// NOTE: Does not include the NUL character.
void t_add_str( struct task*, const char* );
void t_add_sized( struct task*, const void*, int size );
void t_add_opc( struct task*, int );
void t_add_arg( struct task*, int );
void t_seek( struct task*, int );
void t_seek_end( struct task* );
int t_tell( struct task* );
void t_flush( struct task* );
int t_get_script_number( struct script* );
void t_publish_usercode( struct task* );
void t_init_expr_reading( struct expr_reading*, bool in_constant,
   bool skip_assign, bool skip_func_call, bool expect_expr );
void t_read_expr( struct task*, struct expr_reading* );
bool t_is_dec( struct task* );
void t_init_dec( struct dec* );
void t_read_dec( struct task*, struct dec* );
void t_init_expr_test( struct expr_test* test, struct stmt_test* stmt_test,
   struct block* format_block, bool result_required, bool undef_err,
   bool suggest_paren_assign );
void t_test_expr( struct task*, struct expr_test*, struct expr* );
void t_init_stmt_reading( struct stmt_reading*, struct list* labels );
void t_init_stmt_test( struct stmt_test*, struct stmt_test* );
void t_test_top_block( struct task*, struct stmt_test*, struct block* );
void t_test_stmt( struct task*, struct stmt_test*, struct node* );
void t_test_block( struct task*, struct stmt_test*, struct block* );
void t_test_format_item( struct task*, struct format_item*, struct stmt_test*,
   struct expr_test*, struct block* );
struct object* t_get_region_object( struct task*, struct region*,
   struct name* );
void diag_dup( struct task*, const char* text, struct pos*, struct name* );
void diag_dup_struct( struct task*, struct name*, struct pos* );
enum tk t_peek( struct task* );
void t_read_region_body( struct task*, bool is_brace );
const char* t_get_token_name( enum tk );
void t_print_name( struct name* );
void t_diag( struct task*, int flags, ... );
void t_bail( struct task* );
void t_read_region( struct task* );
void t_read_import( struct task*, struct list* );
void t_import( struct task*, struct import* );
int t_extract_literal_value( struct task* );
void t_align_4byte( struct task* );
struct source* t_load_included_source( struct task* );
void t_make_main_lib( struct task* );
void t_read_lib( struct task* );
bool t_same_pos( struct pos*, struct pos* );

#endif