#ifndef TASK_H
#define TASK_H

#include <stdio.h>
#include <stdbool.h>
#include <setjmp.h>

#include "common.h"

#define MAX_MODULE_NAME_LENGTH 8

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
   TK_SEMICOLON,
   TK_ASSIGN,
   TK_ASSIGN_ADD,
   TK_ASSIGN_SUB,
   TK_ASSIGN_MUL,
   TK_ASSIGN_DIV,
   // 20
   TK_ASSIGN_MOD,
   TK_ASSIGN_SHIFT_L,
   TK_ASSIGN_SHIFT_R,
   TK_ASSIGN_BIT_AND,
   TK_ASSIGN_BIT_XOR,
   TK_ASSIGN_BIT_OR,
   TK_EQ,
   TK_NEQ,
   TK_LOG_NOT,
   TK_LOG_AND,
   // 30
   TK_LOG_OR,
   TK_BIT_AND,
   TK_BIT_OR,
   TK_BIT_XOR,
   TK_BIT_NOT,
   TK_LT,
   TK_LTE,
   TK_GT,
   TK_GTE,
   TK_PLUS,
   // 40
   TK_MINUS,
   TK_SLASH,
   TK_STAR,
   TK_MOD,
   TK_SHIFT_L,
   TK_SHIFT_R,
   TK_ASSIGN_COLON,
   TK_BREAK,
   TK_CASE,
   TK_CONST,
   // 50
   TK_CONTINUE,
   TK_DEFAULT,
   TK_DO,
   TK_ELSE,
   TK_ENUM,
   TK_FOR,
   TK_IF,
   TK_INT,
   TK_RETURN,
   TK_STATIC,
   // 60
   TK_STR,
   TK_STRUCT,
   TK_SWITCH,
   TK_VOID,
   TK_WHILE,
   TK_BOOL,
   TK_LIT_DECIMAL,
   TK_LIT_OCTAL,
   TK_LIT_HEX,
   TK_LIT_CHAR,
   // 70
   TK_LIT_FIXED,
   TK_LIT_STRING,
   TK_HASH,    // #
   TK_PALTRANS,
   TK_GLOBAL,
   TK_SCRIPT,
   TK_UNTIL,
   TK_WORLD,
   TK_OPEN,
   TK_RESPAWN,
   // 80
   TK_DEATH,
   TK_ENTER,
   TK_PICKUP,
   TK_BLUE_RETURN,
   TK_RED_RETURN,
   TK_WHITE_RETURN,
   TK_LIGHTNING,
   TK_DISCONNECT,
   TK_UNLOADING,
   TK_CLIENTSIDE,
   // 90
   TK_NET,
   TK_RESTART,
   TK_SUSPEND,
   TK_TERMINATE,
   TK_FUNCTION,
   TK_IMPORT,
   TK_REGION,
   TK_UPMOST,
   TK_GOTO,
   TK_TRUE,
   // 100
   TK_FALSE,
   TK_COLON2,
   TK_EVENT
};

struct pos {
   int module;
   int column;
};

struct token {
   char* text;
   struct pos pos;
   enum tk type;
   int length;
};

struct node {
   enum {
      NODE_NONE,
      NODE_LITERAL,
      NODE_UNARY,
      NODE_BINARY,
      NODE_EXPR,
      NODE_EXPR_LINK,
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
      NODE_MODULE,
      NODE_IMPORT,
      NODE_NAME_USAGE,
      NODE_REGION,
      NODE_REGION_HOST,
      NODE_REGION_UPMOST,
      // 40
      NODE_SCRIPT,
      NODE_PACKED_EXPR,
      NODE_INDEXED_STRING_USAGE,
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
   int size_str;
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
   int offset_str;
   int size;
   int size_str;
};

struct paren {
   struct node node;
   struct node* inside;
};

struct literal {
   struct node node;
   int value;
   bool is_bool;
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

struct expr_link {
   struct node node;
   struct expr_link* next;
   struct expr* expr;
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
   struct packed_expr* packed_expr;
   struct pos pos;
};

struct block {
   struct node node;
   struct list stmts;
   struct pos pos;
};

struct if_stmt {
   struct node node;
   struct expr* expr;
   struct node* body;
   struct node* else_body;
};

struct case_label {
   struct node node;
   int offset;
   struct expr* expr;
   struct case_label* next;
   struct pos pos;
};

struct switch_stmt {
   struct node node;
   struct expr* expr;
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
   struct expr* expr;
   struct node* body;
   struct jump* jump_break;
   struct jump* jump_continue;
};

struct for_stmt {
   struct node node;
   struct expr_link* init;
   struct list vars;
   struct expr* expr;
   struct expr_link* post;
   struct node* body;
   struct jump* jump_break;
   struct jump* jump_continue;
};

struct dim {
   struct dim* next;
   // Number of elements.
   struct expr* size_expr;
   int size;
   int element_size;
   int element_size_str;
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
};

struct multi_value {
   struct initial initial;
   struct initial* body;
   struct pos pos;
   int padding;
   int padding_str;
};

struct var {
   struct object object;
   struct name* name;
   struct type* type;
   struct path* type_path;
   struct dim* dim;
   struct initial* initial;
   struct value* value;
   struct value* value_str;
   int storage;
   int index;
   int index_str;
   int size;
   int size_str;
   int get;
   int set;
   int get_offset;
   int set_offset;
   bool initz_zero;
   bool hidden;
   bool shared;
   bool shared_str;
   bool state_accessed;
   bool state_modified;
   bool has_interface;
   bool use_interface;
   bool initial_has_str;
};

struct param {
   struct object object;
   struct type* type;
   struct param* next;
   struct name* name;
   struct expr* expr;
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
   struct type* value;
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
   struct expr* expr;
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
   struct goto_stmt* stmts;
   struct list users;
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
   struct block* body;
   struct list labels;
   int num_param;
   int offset;
   int size;
   bool tested;
   bool publish;
};

struct alias {
   struct object object;
   struct object* target;
};

struct constant {
   struct object object;
   struct name* name;
   struct expr* expr;
   struct constant* next;
   int value;
   bool visible;
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

struct module {
   struct object object;
   struct file* file;
   char* ch;
   char* ch_start;
   char* ch_save;
   int line;
   char* column;
   char* first_column;
   char* end;
   struct {
      int* columns;
      int count;
      int count_max;
   } lines;
   char recover_ch;
   struct file_identity identity;
   struct str file_path;
   struct list included;
   int id;
   bool read;
};

struct library {
   struct str name;
   struct list modules;
   struct list vars;
   struct list funcs;
   struct list scripts;
   // #included/#imported libraries.
   struct list dynamic;
   // Whether the objects of the library can be used by another library.
   bool visible;
   bool imported;
};

struct module_self {
   struct node node;
   struct pos pos;
};

struct import {
   struct node node;
   struct pos pos;
   // When NULL, use global region.
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
   struct stmt_read* stmt_read;
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

struct stmt_read {
   struct list* labels;
   struct block* block;
   struct node* node;
   int depth;
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

struct read_expr {
   struct expr* node;
   struct stmt_read* stmt_read;
   bool has_str;
   bool in_constant;
   bool skip_assign;
   bool skip_function_call;
};

struct expr_test {
   jmp_buf bail;
   struct stmt_test* stmt_test;
   struct block* format_block;
   struct format_block_usage* format_block_usage;
   struct pos pos;
   bool need_value;
   bool has_string;
   bool undef_err;
   bool undef_erred;
   bool accept_array;
};

#define OBJ_SEEK_END -1
#define BUFFER_SIZE 65536

struct buffer {
   struct buffer* next;
   char data[ BUFFER_SIZE ];
   int used;
   int pos;
};

#define SHARED_ARRAY 0
#define SHARED_ARRAY_STR 1

struct immediate {
   struct immediate* next;
   int value;
};

enum {
   PC_NONE,
   PC_TERMINATE,
   PC_SUSPEND,
   PC_PUSH_NUMBER,
   PC_LSPEC1,
   PC_LSPEC2,
   PC_LSPEC3,
   PC_LSPEC4,
   PC_LSPEC5,
   PC_LSPEC1_DIRECT,
   PC_LSPEC2_DIRECT,
   PC_LSPEC3_DIRECT,
   PC_LSPEC4_DIRECT,
   PC_LSPEC5_DIRECT,
   PC_ADD,
   PC_SUB,
   PC_MUL,
   PC_DIV,
   PC_MOD,
   PC_EQ,
   PC_NE,
   PC_LT,
   PC_GT,
   PC_LE,
   PC_GE,
   PC_ASSIGN_SCRIPT_VAR,
   PC_ASSIGN_MAP_VAR,
   PC_ASSIGN_WORLD_VAR,
   PC_PUSH_SCRIPT_VAR,
   PC_PUSH_MAP_VAR,
   PC_PUSH_WORLD_VAR,
   PC_ADD_SCRIPT_VAR,
   PC_ADD_MAP_VAR,
   PC_ADD_WORLD_VAR,
   PC_SUB_SCRIPT_VAR,
   PC_SUB_MAP_VAR,
   PC_SUB_WORLD_VAR,
   PC_MUL_SCRIPT_VAR,
   PC_MUL_MAP_VAR,
   PC_MUL_WORLD_VAR,
   PC_DIV_SCRIPT_VAR,
   PC_DIV_MAP_VAR,
   PC_DIV_WORLD_VAR,
   PC_MOD_SCRIPT_VAR,
   PC_MOD_MAP_VAR,
   PC_MOD_WORLD_VAR,
   PC_INC_SCRIPT_VAR,
   PC_INC_MAP_VAR,
   PC_INC_WORLD_VAR,
   PC_DEC_SCRIPT_VAR,
   PC_DEC_MAP_VAR,
   PC_DEC_WORLD_VAR,
   PC_GOTO,
   PC_IF_GOTO,
   PC_DROP,
   PC_DELAY,
   PC_DELAY_DIRECT,
   PC_RANDOM,
   PC_RANDOM_DIRECT,
   PC_THING_COUNT,
   PC_THING_COUNT_DIRECT,
   PC_TAG_WAIT,
   PC_TAG_WAIT_DIRECT,
   PC_POLY_WAIT,
   PC_POLY_WAIT_DIRECT,
   PC_CHANGE_FLOOR,
   PC_CHANGE_FLOOR_DIRECT,
   PC_CHANGE_CEILING,
   PC_CHANGE_CEILING_DIRECT,
   PC_RESTART,
   PC_AND_LOGICAL,
   PC_OR_LOGICAL,
   PC_AND_BITWISE,
   PC_OR_BITWISE,
   PC_EOR_BITWISE,
   PC_NEGATE_LOGICAL,
   PC_LSHIFT,
   PC_RSHIFT,
   PC_UNARY_MINUS,
   PC_IF_NOT_GOTO,
   PC_LINE_SIDE,
   PC_SCRIPT_WAIT,
   PC_SCRIPT_WAIT_DIRECT,
   PC_CLEAR_LINE_SPECIAL,
   PC_CASE_GOTO,
   PC_BEGIN_PRINT,
   PC_END_PRINT,
   PC_PRINT_STRING,
   PC_PRINT_NUMBER,
   PC_PRINT_CHARACTER,
   PC_PLAYER_COUNT,
   PC_GAME_TYPE,
   PC_GAME_SKILL,
   PC_TIMER,
   PC_SECTOR_SOUND,
   PC_AMBIENT_SOUND,
   PC_SOUND_SEQUENCE,
   PC_SET_LINE_TEXTURE,
   PC_SET_LINE_BLOCKING,
   PC_SET_LINE_SPECIAL,
   PC_THING_SOUND,
   PC_END_PRINT_BOLD,
   PC_ACTIVATOR_SOUND,
   PC_LOCAL_AMBIENT_SOUND,
   PC_SET_LINE_MONSTER_BLOCKING,
   PC_PLAYER_BLUE_SKULL,
   PC_PLAYER_RED_SKULL,
   PC_PLAYER_YELLOW_SKULL,
   PC_PLAYER_MASTER_SKULL,
   PC_PLAYER_BLUE_CARD,
   PC_PLAYER_RED_CARD,
   PC_PLAYER_YELLOW_CARD,
   PC_PLAYER_MASTER_CARD,
   PC_PLAYER_BLACK_SKULL,
   PC_PLAYER_SILVER_SKULL,
   PC_PLAYER_GOLD_SKULL,
   PC_PLAYER_BLACK_CARD,
   PC_PLAYER_SILVER_CARD,
   PC_IS_MULTIPLAYER,
   PC_PLAYER_TEAM,
   PC_PLAYER_HEALTH,
   PC_PLAYER_ARMOR_POINTS,
   PC_PLAYER_FRAGS,
   PC_PLAYER_EXPERT,
   PC_BLUE_TEAM_COUNT,
   PC_RED_TEAM_COUNT,
   PC_BLUE_TEAM_SCORE,
   PC_RED_TEAM_SCORE,
   PC_IS_ONE_FLAG_CTF,
   PC_GET_INVASION_WAVE,
   PC_GET_INVASION_STATE,
   PC_PRINT_NAME,
   PC_MUSIC_CHANGE,
   PC_CONSOLE_COMMAND_DIRECT,
   PC_CONSOLE_COMMAND,
   PC_SINGLE_PLAYER,
   PC_FIXED_MUL,
   PC_FIXED_DIV,
   PC_SET_GRAVITY,
   PC_SET_GRAVITY_DIRECT,
   PC_SET_AIR_CONTROL,
   PC_SET_AIR_CONTROL_DIRECT,
   PC_CLEAR_INVENTORY,
   PC_GIVE_INVENTORY,
   PC_GIVE_INVENTORY_DIRECT,
   PC_TAKE_INVENTORY,
   PC_TAKE_INVENTORY_DIRECT,
   PC_CHECK_INVENTORY,
   PC_CHECK_INVENTORY_DIRECT,
   PC_SPAWN,
   PC_SPAWN_DIRECT,
   PC_SPAWN_SPOT,
   PC_SPAWN_SPOT_DIRECT,
   PC_SET_MUSIC,
   PC_SET_MUSIC_DIRECT,
   PC_LOCAL_SET_MUSIC,
   PC_LOCAL_SET_MUSIC_DIRECT,
   PC_PRINT_FIXED,
   PC_PRINT_LOCALIZED,
   PC_MORE_HUD_MESSAGE,
   PC_OPT_HUD_MESSAGE,
   PC_END_HUD_MESSAGE,
   PC_END_HUD_MESSAGE_BOLD,
   PC_SET_STYLE,
   PC_SET_STYLE_DIRECT,
   PC_SET_FONT,
   PC_SET_FONT_DIRECT,
   PC_PUSH_BYTE,
   PC_LSPEC1_DIRECT_B,
   PC_LSPEC2_DIRECT_B,
   PC_LSPEC3_DIRECT_B,
   PC_LSPEC4_DIRECT_B,
   PC_LSPEC5_DIRECT_B,
   PC_DELAY_DIRECT_B,
   PC_RANDOM_DIRECT_B,
   PC_PUSH_BYTES,
   PC_PUSH_2BYTES,
   PC_PUSH_3BYTES,
   PC_PUSH_4BYTES,
   PC_PUSH_5BYTES,
   PC_SET_THING_SPECIAL,
   PC_ASSIGN_GLOBAL_VAR,
   PC_PUSH_GLOBAL_VAR,
   PC_ADD_GLOBAL_VAR,
   PC_SUB_GLOBAL_VAR,
   PC_MUL_GLOBAL_VAR,
   PC_DIV_GLOBAL_VAR,
   PC_MOD_GLOBAL_VAR,
   PC_INC_GLOBAL_VAR,
   PC_DEC_GLOBAL_VAR,
   PC_FADE_TO,
   PC_FADE_RANGE,
   PC_CANCEL_FADE,
   PC_PLAY_MOVIE,
   PC_SET_FLOOR_TRIGGER,
   PC_SET_CEILING_TRIGGER,
   PC_GET_ACTOR_X,
   PC_GET_ACTOR_Y,
   PC_GET_ACTOR_Z,
   PC_START_TRANSLATION,
   PC_TRANSLATION_RANGE1,
   PC_TRANSLATION_RANGE2,
   PC_END_TRANSLATION,
   PC_CALL,
   PC_CALL_DISCARD,
   PC_RETURN_VOID,
   PC_RETURN_VAL,
   PC_PUSH_MAP_ARRAY,
   PC_ASSIGN_MAP_ARRAY,
   PC_ADD_MAP_ARRAY,
   PC_SUB_MAP_ARRAY,
   PC_MUL_MAP_ARRAY,
   PC_DIV_MAP_ARRAY,
   PC_MOD_MAP_ARRAY,
   PC_INC_MAP_ARRAY,
   PC_DEC_MAP_ARRAY,
   PC_DUP,
   PC_SWAP,
   PC_WRITE_TO_INI,
   PC_GET_FROM_INI,
   PC_SIN,
   PC_COS,
   PC_VECTOR_ANGLE,
   PC_CHECK_WEAPON,
   PC_SET_WEAPON,
   PC_TAG_STRING,
   PC_PUSH_WORLD_ARRAY,
   PC_ASSIGN_WORLD_ARRAY,
   PC_ADD_WORLD_ARRAY,
   PC_SUB_WORLD_ARRAY,
   PC_MUL_WORLD_ARRAY,
   PC_DIV_WORLD_ARRAY,
   PC_MOD_WORLD_ARRAY,
   PC_INC_WORLD_ARRAY,
   PC_DEC_WORLD_ARRAY,
   PC_PUSH_GLOBAL_ARRAY,
   PC_ASSIGN_GLOBAL_ARRAY,
   PC_ADD_GLOBAL_ARRAY,
   PC_SUB_GLOBAL_ARRAY,
   PC_MUL_GLOBAL_ARRAY,
   PC_DIV_GLOBAL_ARRAY,
   PC_MOD_GLOBAL_ARRAY,
   PC_INC_GLOBAL_ARRAY,
   PC_DEC_GLOBAL_ARRAY,
   PC_SET_MARINE_WEAPON,
   PC_SET_ACTOR_PROPERTY,
   PC_GET_ACTOR_PROPERTY,
   PC_PLAYER_NUMBER,
   PC_ACTIVATOR_TID,
   PC_SET_MARINE_SPRITE,
   PC_GET_SCREEN_WIDTH,
   PC_GET_SCREEN_HEIGHT,
   PC_THING_PROJECTILE2,
   PC_STRLEN,
   PC_SET_HUD_SIZE,
   PC_GET_CVAR,
   PC_CASE_GOTO_SORTED,
   PC_SET_RESULT_VALUE,
   PC_GET_LINE_ROW_OFFSET,
   PC_GET_ACTOR_FLOOR_Z,
   PC_GET_ACTOR_ANGLE,
   PC_GET_SECTOR_FLOOR_Z,
   PC_GET_SECTOR_CEILING_Z,
   PC_LSPEC5_RESULT,
   PC_GET_SIGIL_PIECES,
   PC_GET_LEVEL_INFO,
   PC_CHANGE_SKY,
   PC_PLAYER_IN_GAME,
   PC_PLAYER_IS_BOT,
   PC_SET_CAMERA_TO_TEXTURE,
   PC_END_LOG,
   PC_GET_AMMO_CAPACITY,
   PC_SET_AMMO_CAPACITY,
   PC_PRINT_MAP_CHAR_ARRAY,
   PC_PRINT_WORLD_CHAR_ARRAY,
   PC_PRINT_GLOBAL_CHAR_ARRAY,
   PC_SET_ACTOR_ANGLE,
   PC_GRAP_INPUT,
   PC_SET_MOUSE_POINTER,
   PC_MOVE_MOUSE_POINTER,
   PC_SPAWN_PROJECTILE,
   PC_GET_SECTOR_LIGHT_LEVEL,
   PC_GET_ACTOR_CEILING_Z,
   PC_SET_ACTOR_POSITION,
   PC_CLEAR_ACTOR_INVENTORY,
   PC_GIVE_ACTOR_INVENTORY,
   PC_TAKE_ACTOR_INVENTORY,
   PC_CHECK_ACTOR_INVENTORY,
   PC_THING_COUNT_NAME,
   PC_SPAWN_SPOT_FACING,
   PC_PLAYER_CLASS,
   PC_AND_SCRIPT_VAR,
   PC_AND_MAP_VAR,
   PC_AND_WORLD_VAR,
   PC_AND_GLOBAL_VAR,
   PC_AND_MAP_ARRAY,
   PC_AND_WORLD_ARRAY,
   PC_AND_GLOBAL_ARRAY,
   PC_EOR_SCRIPT_VAR,
   PC_EOR_MAP_VAR,
   PC_EOR_WORLD_VAR,
   PC_EOR_GLOBAL_VAR,
   PC_EOR_MAP_ARRAY,
   PC_EOR_WORLD_ARRAY,
   PC_EOR_GLOBAL_ARRAY,
   PC_OR_SCRIPT_VAR,
   PC_OR_MAP_VAR,
   PC_OR_WORLD_VAR,
   PC_OR_GLOBAL_VAR,
   PC_OR_MAP_ARRAY,
   PC_OR_WORLD_ARRAY,
   PC_OR_GLOBAL_ARRAY,
   PC_LS_SCRIPT_VAR,
   PC_LS_MAP_VAR,
   PC_LS_WORLD_VAR,
   PC_LS_GLOBAL_VAR,
   PC_LS_MAP_ARRAY,
   PC_LS_WORLD_ARRAY,
   PC_LS_GLOBAL_ARRAY,
   PC_RS_SCRIPT_VAR,
   PC_RS_MAP_VAR,
   PC_RS_WORLD_VAR,
   PC_RS_GLOBAL_VAR,
   PC_RS_MAP_ARRAY,
   PC_RS_WORLD_ARRAY,
   PC_RS_GLOBAL_ARRAY,
   PC_GET_PLAYER_INFO,
   PC_CHANGE_LEVEL,
   PC_SECTOR_DAMAGE,
   PC_REPLACE_TEXTURES,
   PC_NEGATE_BINARY,
   PC_GET_ACTOR_PITCH,
   PC_SET_ACTOR_PITCH,
   PC_PRINT_BIND,
   PC_SET_ACTOR_STATE,
   PC_THING_DAMAGE2,
   PC_USE_INVENTORY,
   PC_USE_ACTOR_INVENTORY,
   PC_CHECK_ACTOR_CEILING_TEXTURE,
   PC_CHECK_ACTOR_FLOOR_TEXTURE,
   PC_GET_ACTOR_LIGHT_LEVEL,
   PC_SET_MUGSHOT_STATE,
   PC_THING_COUNT_SECTOR,
   PC_THING_COUNT_NAME_SECTOR,
   PC_CHECK_PLAYER_CAMERA,
   PC_MORPH_ACTOR,
   PC_UNMORPH_ACTOR,
   PC_GET_PLAYER_INPUT,
   PC_CLASSIFY_ACTOR,
   PC_PRINT_BINARY,
   PC_PRINT_HEX,
   PC_CALL_FUNC,
   PC_SAVE_STRING
};

#define TK_BUFFER_SIZE 5

struct task {
   struct options* options;
   FILE* err_file;
   jmp_buf* bail;
   int column;
   struct {
      struct token buffer[ TK_BUFFER_SIZE ];
      struct str text;
      int peeked;
   } tokens;
   char ch;
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
   struct region* region_global;
   // Module currently being worked on. Could be the main module, or an
   // imported module.
   struct module* module;
   // Module requested by the user to be compiled.
   struct module* module_main;
   struct library* library;
   struct library* library_main;
   struct list libraries;
   struct name* root_name;
   struct name* anon_name;
   struct list loaded_modules;
   struct list regions;
   struct list scripts;
   int depth;
   struct scope* scope;
   struct scope* free_scope;
   struct sweep* free_sweep;
   int shared_size;
   int shared_size_str;
   struct buffer* buffer_head;
   struct buffer* buffer;
   bool compress;
   bool in_func;
   enum {
      FORMAT_ZERO,
      FORMAT_BIG_E,
      FORMAT_LITTLE_E
   } format;
   int opc;
   int opc_args;
   struct immediate* immediate;
   struct immediate* immediate_tail;
   struct immediate* free_immediate;
   int immediate_count;
   bool push_immediate;
   struct block_walk* block_walk;
   struct block_walk* block_walk_free;
};

struct paused {
   struct module* module;
   char ch;
   int column;
   enum tk tk;
};

void t_init( struct task*, struct options*, jmp_buf* );
void t_init_fields_dec( struct task* );
void t_init_fields_chunk( struct task* );
void t_init_fields_obj( struct task* );
struct module* t_load_module( struct task*, const char*, struct paused* );
void t_resume_module( struct task*, struct paused* );
void t_init_object( struct object*, int );
void t_read_tk( struct task* );
void t_test_tk( struct task*, enum tk );
void t_read( struct task* );
void t_unload_file( struct task*, bool* );
void t_skip_past( struct task*, char );
void t_skip_past_tk( struct task*, enum tk );
void t_test( struct task* );
void t_read_block( struct task*, struct stmt_read* );
struct name* t_make_name( struct task*, const char*, struct name* );
void t_use_name( struct name*, struct object* );
void t_read_script( struct task* );
struct format_item* t_read_format_item( struct task*, bool colon );
int t_read_literal( struct task* );
void t_add_scope( struct task* );
void t_pop_scope( struct task* );
void t_test_constant( struct task*, struct constant*, bool undef_err );
void t_test_constant_set( struct task*, struct constant_set*, bool undef_err );
void t_test_type( struct task*, struct type*, bool undef_err );
void t_read_define( struct task* );
void t_copy_name( struct name*, bool, struct str* );
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
int t_tell( struct task* );
void t_flush( struct task* );
void t_skip_to_tk( struct task*, enum tk );
int t_get_script_number( struct script* );
void t_publish_scripts( struct task*, struct list* );
void t_publish_funcs( struct task*, struct list* );
void t_pad4align( struct task* );
void t_init_read_expr( struct read_expr* );
void t_read_expr( struct task*, struct read_expr* );
bool t_is_dec( struct task* );
void t_init_dec( struct dec* );
void t_read_dec( struct task*, struct dec* );
void t_init_expr_test( struct expr_test* );
void t_test_expr( struct task*, struct expr_test*, struct expr* );
void t_init_stmt_read( struct stmt_read* );
void t_init_stmt_test( struct stmt_test*, struct stmt_test* );
void t_test_top_block( struct task*, struct stmt_test*, struct block* );
void t_test_block( struct task*, struct stmt_test*, struct block* );
void t_test_format_item( struct task*, struct format_item*, struct stmt_test*,
   struct expr_test*, struct name* name_offset, struct block* );
struct object* t_get_region_object( struct task*, struct region*,
   struct name* );
void diag_dup( struct task*, const char* text, struct pos*, struct name* );
void diag_dup_struct( struct task*, struct name*, struct pos* );
struct path* t_read_path( struct task* );
void t_use_local_name( struct task*, struct name*, struct object* );
enum tk t_peek( struct task* );
enum tk t_peek_2nd( struct task* );
void t_read_region_body( struct task*, bool is_brace );
void t_read_dirc( struct task*, struct pos* );

void t_print_name( struct name* );

#define DIAG_NONE 0
#define DIAG_FILE 0x1
#define DIAG_LINE 0x2
#define DIAG_COLUMN 0x4
#define DIAG_WARN 0x8
#define DIAG_ERR 0x10
#define DIAG_POS_ERR DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN

void t_diag( struct task*, int flags, ... );
void t_bail( struct task* );
void t_read_region( struct task* );
void t_read_import( struct task*, struct list* );
void t_import( struct task*, struct import* );

#endif