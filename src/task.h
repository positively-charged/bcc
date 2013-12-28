#ifndef TASK_H
#define TASK_H

#include <stdbool.h>
#include <setjmp.h>

#include "common.h"

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
   TK_INSERTION,
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
   TK_FIXED,
   TK_NAMESPACE,
   TK_USING,
   TK_GOTO
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
      NODE_INDEXED_STRING_USAGE,
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
      NODE_NAMESPACE,
      NODE_NAMESPACE_LINK,
      NODE_SHORTCUT,
      NODE_NAME_USAGE
   } type;
};

struct object {
   struct node node;
   short depth;
   bool resolved;
   struct pos pos;
   struct object* link;
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
};

struct type {
   struct object object;
   struct name* name;
   struct name* body;
   struct type_member* member;
   struct type_member* member_tail;
   struct type_member* member_curr;
   struct path* undef;
   struct pos pos;
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
   struct pos pos;
};

struct literal {
   struct node node;
   struct pos pos;
   struct type* type;
   int value;
};

struct unary {
   struct node node;
   enum {
      UOP_NONE,
      UOP_MINUS,
      UOP_LOG_NOT,
      UOP_BIT_NOT,
      UOP_PRE_INC,
      UOP_PRE_DEC,
      UOP_POST_INC,
      UOP_POST_DEC
   } op;
   struct node* operand;
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
      AOP_BIT_OR
   } op;
   struct node* lside;
   struct node* rside;
};

struct subscript {
   struct node node;
   struct node* lside;
   struct node* index;
};

struct access {
   struct node node;
   struct pos pos;
   struct node* lside;
   struct node* rside;
   char* name;
};

struct call {
   struct node node;
   struct pos pos;
   struct node* func_tree;
   struct func* func;
   struct list args;
};

struct expr {
   struct node node;
   struct node* root;
   int value;
   bool folded;
};

struct expr_link {
   struct node node;
   struct expr_link* next;
   struct expr* expr;
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
   struct expr* expr;
   struct pos pos;
};

struct block {
   struct node node;
   struct list stmts;
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
   int size;
   int size_str;
   int count;
   struct expr* count_expr;
   struct pos pos;
};

struct initial {
   struct initial* next;
   bool is_initz;
   bool tested;
};

struct value {
   struct initial initial;
   struct expr* expr;
   struct value* next;
   int index;
};

struct initz {
   struct initial initial;
   struct initial* body;
   struct initial* body_curr;
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
   int usage;
   int index;
   int index_str;
   int size;
   int size_str;
   int get;
   int set;
   int get_offset;
   int set_offset;
   enum {
      VAR_FLAG_INTERFACE_GET_SET = 0x1
   } flags;
   bool initial_zero;
   bool imported;
   bool hidden;
   bool shared;
};

struct param {
   struct object object;
   struct pos pos;
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
};

struct func_aspec {
   int id;
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
   struct expr* expr;
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
   bool imported;
};

struct func_intern {
   enum {
      INTERN_FUNC_ACS_EXECWAIT,
      INTERN_FUNC_TOTAL
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
      SCRIPT_TYPE_RETURN
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
};

struct shortcut {
   struct object object;
   struct path* path;
   struct name* alias;
   struct name* target;
   bool import_struct;
};

struct ns {
   struct object object;
   struct name* name;
   struct name* body;
   struct name* body_struct;
   // Contains functions and scripts.
   struct list items;
   struct ns_link* ns_link;
   struct object* unresolved;
   struct object* unresolved_tail;
};

struct ns_link {
   struct object object;
   struct path* path;
   struct ns* ns;
   struct ns_link* next;
};

struct constant {
   struct object object;
   struct name* name;
   struct expr* expr;
   struct constant* next;
   int value;
};

struct constant_set {
   struct object object;
   struct constant* head;
};

struct indexed_string {
   struct indexed_string* next;
   char* value;
   int length;
   int index;
   int usage;
};

struct indexed_string_usage {
   struct node node;
   struct indexed_string* string;
   struct pos pos;
};

struct str_table {
   struct indexed_string* head;
   struct indexed_string* tail;
   int size;
};

struct module {
   struct str name;
   struct list vars;
   struct list funcs;
   struct list scripts;
   struct list imports;
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
   struct pos storage_index_pos;
   struct pos type_pos;
   struct pos storage_pos;
   struct pos name_pos;
   const char* storage_name;
   struct type* type;
   struct type* new_type;
   struct path* type_path;
   struct name* name;
   struct name* name_offset;
   struct dim* dim;
   struct initial* initial;
   struct list* vars;
   int storage;
   int storage_index;
   bool type_needed;
   bool storage_given;
   bool initial_str;
};

struct stmt_read {
   struct block* block;
   struct node* node;
   struct list* labels;
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
   bool in_loop;
   bool in_switch;
   bool in_script;
   bool manual_scope;
};

struct read_expr {
   struct expr* node;
   struct stmt_read* stmt_read;
   bool has_str;
   bool skip_assign;
   bool skip_function_call;
};

struct expr_test {
   jmp_buf bail;
   struct stmt_test* stmt_test;
   struct pos pos;
   bool needed;
   bool has_string;
   bool undef_err;
   bool undef_erred;
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
   PC_GET_ACTOR_POSITION_Z,
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

struct task {
   struct options* options;
   enum tk tk;
   struct pos tk_pos;
   char* tk_text;
   int tk_length;
   struct file* file;
   char* text;
   char* ch;
   int line;
   char* column;
   char* first_column;
   char* end;
   char recover_ch;
   struct stack paused;
   struct list unloaded_files;
   int peeked_length;
   struct ns* ns;
   struct ns* ns_global;
   struct d_list nss;
   struct str_table str_table;
   struct type* type_int;
   struct type* type_str;
   struct type* type_bool;
   struct type* type_fixed;
   struct module* module;
   struct name* anon_name;
   struct str str;
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
   bool importing;
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

void t_init( struct task*, struct options* );
void t_init_fields_tk( struct task* );
void t_init_fields_dec( struct task* );
void t_init_fields_chunk( struct task* );
void t_init_fields_obj( struct task* );
void t_read_module( struct task* );
bool t_load_file( struct task*, const char* );
void t_unload_file( struct task*, bool* );
void t_read_tk( struct task* );
void t_test_tk( struct task*, enum tk );
char t_peek_usable_ch( struct task* );
void t_skip_past( struct task*, char );
void t_skip_past_tk( struct task*, enum tk );
bool t_coming_up_insertion_token( struct task* );
bool t_is_dec_start( struct task* );
void t_test( struct task* );
struct name* t_make_name( struct task*, const char*, struct name* );
void t_use_name( struct name*, struct object* );
void t_read_namespace( struct task* );
void t_read_script( struct task* );
void t_init_read_expr( struct read_expr* );
void t_read_expr( struct task*, struct read_expr* );
struct format_item* t_read_format_item( struct task*, enum tk sep );
void t_init_expr_test( struct expr_test* );
void t_test_expr( struct task*, struct expr_test*, struct expr* );
bool t_is_dec( struct task* );
void t_init_dec( struct dec* );
void t_read_dec( struct task*, struct dec* );
void t_read_enum( struct task* );
void t_init_stmt_read( struct stmt_read* );
void t_read_block( struct task*, struct stmt_read* );
void t_read_using( struct task*, struct node** local );
int t_read_literal( struct task* );
void t_add_scope( struct task* );
void t_pop_scope( struct task* );
void t_init_stmt_test( struct stmt_test*, struct stmt_test* parent );
void t_test_top_block( struct task*, struct stmt_test*, struct block* );
void t_test_block( struct task*, struct stmt_test*, struct block* );
void t_test_constant( struct task*, struct constant*, bool local,
   bool undef_err );
void t_test_constant_set( struct task*, struct constant_set*, bool local,
   bool undef_err );
void t_test_type( struct task*, struct type*, bool undef_err );
void t_read_define( struct task* );
void t_copy_name( struct name*, struct str*, char term );
void t_copy_full_name( struct name*, struct str* );
int t_full_name_length( struct name* );
void t_test_local_var( struct task*, struct var* );
void t_test_format_item( struct task*, struct format_item*, struct stmt_test*,
   struct name* name_offset );
void t_skip_block( struct task* );
void t_publish( struct task* );
void t_add_byte( struct task*, char );
void t_add_short( struct task*, short );
void t_add_int( struct task*, int );
void t_add_int_zero( struct task*, int );
// NOTE: Does not include the null character.
void t_add_str( struct task*, const char* );
void t_add_sized( struct task*, const void*, int size );
void t_add_opc( struct task*, int );
void t_add_arg( struct task*, int );
void t_seek( struct task*, int );
int t_tell( struct task* );
void t_flush( struct task* );
void t_skip_to_tk( struct task*, enum tk );
void t_test_ns_link( struct task*, struct ns_link*, bool undef_err );
void t_test_shortcut( struct task*, struct shortcut*, bool undef_err );
int t_get_script_number( struct script* );
void t_write_func_content( struct task* );
void t_write_script_content( struct task* );

#endif