#ifndef TASK_H
#define TASK_H

#include <stdio.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdarg.h>

#include "common.h"
#include "gbuf.h"

#define MAX_LIB_NAME_LENGTH 8

struct file_entry {
   struct file_entry* next;
   struct fileid file_id;
   struct str path;
   struct str full_path;
   int id;
};

struct file_query {
   const char* given_path;
   struct str* path;
   struct fileid fileid;
   struct file_entry* file;
   struct file_entry* offset_file;
   bool success;
};

struct pos {
   int line;
   int column;
   int id;
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
      NODE_STRUCTURE,
      NODE_STRUCTURE_MEMBER,
      NODE_ENUMERATION,
      NODE_ENUMERATOR,
      // 30
      NODE_CONSTANT,
      NODE_RETURN,
      NODE_PARAM,
      NODE_PALTRANS,
      NODE_ALIAS,
      NODE_BOOLEAN,
      NODE_NAME_USAGE,
      NODE_SCRIPT,
      NODE_PACKED_EXPR,
      NODE_CONDITIONAL,
      NODE_STRCPY,
      // 40
      NODE_LOGICAL,
      NODE_INC,
      NODE_FIXED_LITERAL,
      NODE_CAST,
      NODE_INLINE_ASM,
      NODE_ASSERT,
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
   const char* text;
   struct node* object;
   struct pos pos;
   int lib_id;
};

struct path {
   struct path* next;
   const char* text;
   struct pos pos;
};

enum {
   SPEC_NONE,
   SPEC_ZRAW,
   SPEC_ZINT,
   SPEC_ZFIXED,
   SPEC_ZBOOL,
   SPEC_ZSTR,
   SPEC_ENUM,
   SPEC_STRUCT,
   SPEC_VOID
};

struct structure {
   struct object object;
   struct name* name;
   struct name* body;
   struct structure_member* member;
   struct structure_member* member_tail;
   int size;
   bool anon;
};

struct structure_member {
   struct object object;
   struct name* name;
   struct structure* structure;
   struct path* type_path;
   struct dim* dim;
   struct structure_member* next;
   int spec;
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

struct fixed_literal {
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
      UOP_BIT_NOT
   } op;
   struct node* operand;
   struct pos pos;
   int operand_spec;
};

struct inc {
   struct node node;
   struct node* operand;
   struct pos pos;
   bool post;
   bool dec;
   bool zfixed;
};

struct cast {
   struct node node;
   struct node* operand;
   struct pos pos;
   int spec;
};

struct binary {
   struct node node;
   enum {
      BOP_NONE,
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
   int lside_spec;
   int value;
   bool folded;
};

struct logical {
   struct node node;
   enum {
      LOP_OR,
      LOP_AND
   } op;
   struct node* lside;
   struct node* rside;
   struct pos pos;
   int lside_spec;
   int rside_spec;
   int value;
   bool folded;
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

struct conditional {
   struct node node;
   struct pos pos;
   struct node* left;
   struct node* middle;
   struct node* right;
   int left_value;
   bool folded;
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
   const char* name;
};

struct call {
   struct node node;
   struct pos pos;
   struct node* operand;
   struct func* func;
   struct nested_call* nested_call;
   struct list args;
};

struct nested_call {
   struct call* next;
   struct c_jump* prologue_jump;
   struct c_point* return_point;
   int id;
};

// I would like to make this a function.
struct strcpy_call {
   struct node node;
   struct expr* array;
   struct expr* array_offset;
   struct expr* array_length;
   struct expr* string;
   struct expr* offset;
};

struct expr {
   struct node node;
   struct node* root;
   struct pos pos;
   int spec;
   // The value is handled based on the expression type. For numeric types,
   // it'll contain the numeric result of the expression. For the string type,
   // it'll contain the string index, that can be used to lookup the string.
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
   struct c_jump* point_jump;
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
   struct return_stmt* next;
   struct c_jump* epilogue_jump;
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
   struct c_point* point;
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

#define DBG_LASTMEMBER char __last[];

struct var {
   struct object object;
   struct name* name;
   struct structure* structure;
   struct path* type_path;
   struct dim* dim;
   struct initial* initial;
   struct value* value;
   struct var* next;
   int spec;
   int storage;
   int index;
   int size;
   bool initz_zero;
   bool hidden;
   bool used;
   bool initial_has_str;
   bool imported;
   bool is_constant_init;
   DBG_LASTMEMBER
};

struct param {
   struct object object;
   struct param* next;
   struct name* name;
   struct expr* default_value;
   int spec;
   int index;
   int obj_pos;
   bool used;
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
   void* impl;
   int return_spec;
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
   struct c_point* point;
   struct pos pos;
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
   struct func* next_nested;
   struct func* nested_funcs;
   struct call* nested_calls;
   struct return_stmt* returns;
   struct c_point* prologue_point;
   struct c_sortedcasejump* return_table;
   int index;
   int size;
   int usage;
   int obj_pos;
   int index_offset;
   bool nested;
   bool publish;
   bool hidden;
};

struct func_intern {
   enum {
      INTERN_FUNC_ACS_EXECWAIT,
      INTERN_FUNC_ACS_NAMEDEXECUTEWAIT,
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
   struct c_point* point;
   struct pos pos;
   const char* name;
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
      SCRIPT_TYPE_BLUERETURN,
      SCRIPT_TYPE_REDRETURN,
      SCRIPT_TYPE_WHITERETURN,
      SCRIPT_TYPE_LIGHTNING = 12,
      SCRIPT_TYPE_UNLOADING,
      SCRIPT_TYPE_DISCONNECT,
      SCRIPT_TYPE_RETURN,
      SCRIPT_TYPE_EVENT,
      SCRIPT_TYPE_NEXTFREENUMBER
   } type;
   enum {
      SCRIPT_FLAG_NET = 0x1,
      SCRIPT_FLAG_CLIENTSIDE = 0x2
   } flags;
   struct param* params;
   struct node* body;
   struct func* nested_funcs;
   struct call* nested_calls;
   struct list labels;
   int assigned_number;
   int num_param;
   int offset;
   int size;
   bool publish;
   bool named_script;
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

struct enumerator {
   struct object object;
   struct name* name;
   struct enumerator* next;
   struct expr* initz;
   int value;
};

struct enumeration {
   struct object object;
   struct enumerator* head;
};

struct indexed_string {
   struct indexed_string* next;
   struct indexed_string* next_sorted;
   struct indexed_string* next_usable;
   const char* value;
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
   int size;
};

/*
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
   struct list items;
}; */

struct mnemonic {
   const char* name;
   const char* args;
   struct mnemonic* next;
   struct pos pos;
   int opcode;
};

struct inline_asm {
   struct node node;
   struct pos pos;
   const char* name;
   struct inline_asm* next;
   struct list args;
   int opcode;
   int obj_pos;
};

struct inline_asm_arg {
   enum {
      INLINE_ASM_ARG_NUMBER,
      INLINE_ASM_ARG_ID,
      INLINE_ASM_ARG_EXPR,
      INLINE_ASM_ARG_LABEL,
      INLINE_ASM_ARG_VAR,
      INLINE_ASM_ARG_PARAM,
      INLINE_ASM_ARG_FUNC
   } type;
   union {
      int number;
      const char* id;
      struct expr* expr;
      struct label* label;
      struct var* var;
      struct param* param;
      struct func* func;
   } value;
   struct pos pos;
};

struct assert {
   struct node node;
   struct assert* next;
   struct expr* cond;
   struct pos pos;
   const char* custom_message;
   struct indexed_string* file;
   struct indexed_string* message;
   bool is_static;
};

struct library {
   struct str name;
   struct pos name_pos;
   struct list vars;
   struct list funcs;
   struct list scripts;
   struct list objects;
   // #included/#imported libraries.
   struct list dynamic;
   struct name* hidden_names;
   struct object* unresolved;
   struct object* unresolved_tail;
   struct file_entry* file;
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
   bool compiletime;
   bool header;
};

struct task {
   struct options* options;
   FILE* err_file;
   jmp_buf* bail;
   struct file_entry* file_entries;
   struct str_table str_table;
   struct name* root_name;
   struct name* body;
   struct name* body_struct;
   struct structure* type_int;
   struct structure* type_str;
   struct structure* type_bool;
   struct library* library;
   struct library* library_main;
   struct list libraries;
   // List of alternative filenames. Each entry is a string.
   struct list altern_filenames;
   int last_id;
   time_t compile_time;
   struct gbuf growing_buffer;
   struct mnemonic* mnemonics;
   struct list runtime_asserts;
};

#define DIAG_NONE 0
#define DIAG_FILE 0x1
#define DIAG_LINE 0x2
#define DIAG_COLUMN 0x4
#define DIAG_WARN 0x8
#define DIAG_ERR 0x10
#define DIAG_SYNTAX 0x20
#define DIAG_CUSTOM 0x40
#define DIAG_POS DIAG_FILE | DIAG_LINE | DIAG_COLUMN
#define DIAG_POS_ERR DIAG_POS | DIAG_ERR

void t_init( struct task*, struct options*, jmp_buf* );
void t_copy_name( struct name*, bool full, struct str* buffer );
int t_full_name_length( struct name* );
int t_get_script_number( struct script* );
void t_print_name( struct name* );
void t_diag( struct task*, int flags, ... );
void t_diag_args( struct task* task, int flags, va_list* args );
void t_intern_diag( struct task* task, const char* file, int line,
   const char* format, ... );
void t_unhandlednode_diag( struct task*, const char* file, int line,
   struct node* node );
void t_bail( struct task* );
bool t_same_pos( struct pos*, struct pos* );
void t_decode_pos( struct task* task, struct pos* pos, const char** file,
   int* line, int* column );
const char* t_decode_pos_file( struct task* task, struct pos* pos );
void t_init_object( struct object* object, int node_type );
void t_init_types( struct task* task );
// void t_init_type_members( struct task* );
struct structure* t_create_structure( struct task* task, struct name* name );
void t_init_file_query( struct file_query* query,
   struct file_entry* offset_file, const char* path );
void t_find_file( struct task* task, struct file_query* query );
struct library* t_add_library( struct task* task );
struct name* t_create_name( void );
struct name* t_extend_name( struct name* parent, const char* extension );
struct indexed_string* t_intern_string( struct task* task,
   const char* value, int length );
struct indexed_string* t_intern_string_get_status( struct task* task,
   const char* value, int length, bool* first_time );
struct indexed_string* t_lookup_string( struct task* task, int index );
int t_add_altern_filename( struct task* task, const char* filename );

#endif