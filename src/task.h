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
   const char* lang_dir;
   bool success;
};

// Nonexistent files.
enum {
   INTERNALFILE_NONE,
   // Refers to the internals of the compiler.
   INTERNALFILE_COMPILER,
   // Refers to the command-line.
   INTERNALFILE_COMMANDLINE,
   INTERNALFILE_TOTAL,
};

// Keeps track of the include history of source files. Also used for
// alternative file names.
struct include_history_entry {
   // The parent source file that #includes/#imports the current file. Walking
   // up the chain gives us the include history for a file.
   struct include_history_entry* parent;
   // `file` is the file that was included or read. If an alternative name is
   // provided by the user, this field still points to a file. But this field
   // is NULL for an internal file.
   struct file_entry* file;
   // Alternative file name. Instead of using the file path of the source
   // file (`file`), this provides an alternative file path. It can be set by
   // the user via the #line preprocessor directive. Also used for internal
   // file names.
   const char* altern_name;
   // ID of the include history entry.
   int id;
   // The line number of where the file inclusion occurs. For the source file
   // of the main module, the line number is 0.
   int line;
   // States whether the file is #imported. If false, it means #included.
   bool imported;
};

struct text_buffer {
   struct text_buffer* prev;
   char* start;
   char* end;
   char* left;
};

// File position in a source file.
struct pos {
   int line;
   int column;
   // ID of an include history entry. The include history entry has the file
   // path and parent files.
   int id;
};

struct node {
   enum {
      // 0
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
      NODE_FUNC,
      NODE_ACCESS,
      NODE_PAREN,
      NODE_SUBSCRIPT,
      NODE_CASE,
      NODE_CASE_DEFAULT,
      NODE_SWITCH,
      NODE_BLOCK,
      // 20
      NODE_GOTO,
      NODE_GOTO_LABEL,
      NODE_VAR,
      NODE_ASSIGN,
      NODE_STRUCTURE,
      NODE_STRUCTURE_MEMBER,
      NODE_ENUMERATION,
      NODE_ENUMERATOR,
      NODE_CONSTANT,
      NODE_RETURN,
      // 30
      NODE_PARAM,
      NODE_PALTRANS,
      NODE_ALIAS,
      NODE_BOOLEAN,
      NODE_NAME_USAGE,
      NODE_SCRIPT,
      NODE_EXPR_STMT,
      NODE_CONDITIONAL,
      NODE_STRCPY,
      NODE_LOGICAL,
      // 40
      NODE_INC,
      NODE_FIXED_LITERAL,
      NODE_CAST,
      NODE_INLINE_ASM,
      NODE_ASSERT,
      NODE_TYPE_ALIAS,
      NODE_FOREACH,
      NODE_NULL,
      NODE_NAMESPACE,
      NODE_NAMESPACEFRAGMENT,
      // 50
      NODE_UPMOST,
      NODE_CURRENTNAMESPACE,
      NODE_USING,
      NODE_MEMCPY,
      NODE_CONVERSION,
      NODE_SURE,
      NODE_DO,
      NODE_COMPOUNDLITERAL,
      NODE_MAGICID,
      NODE_BUILDMSG,
      // 60
      NODE_QUALIFIEDNAMEUSAGE,
      NODE_LENGTHOF,
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
};

struct qualified_name_usage {
   struct node node;
   struct path* path;
   struct node* object;
};

struct path {
   struct path* next;
   const char* text;
   struct pos pos;
   bool upmost;
   bool current_ns;
   bool dot_separator;
};

enum {
   SPEC_NONE,
   SPEC_RAW,
   SPEC_INT,
   SPEC_FIXED,
   SPEC_BOOL,
   SPEC_STR,
   SPEC_ENUM,
   SPEC_STRUCT,
   SPEC_AUTO,
   SPEC_AUTOENUM,
   SPEC_VOID,
   SPEC_TOTAL,

   SPEC_NAME = SPEC_TOTAL
};

struct enumeration {
   struct object object;
   struct enumerator* head;
   struct enumerator* tail;
   struct name* name;
   struct name* body;
   int base_type;
   int num_enumerators;
   bool hidden;
   bool semicolon;
   bool force_local_scope;
   bool default_initz;
};

struct enumerator {
   struct object object;
   struct name* name;
   struct enumerator* next;
   // NOTE: The following field is not cached. It will be NULL for an
   // enumerator that is retrieved from a cached library.
   struct expr* initz;
   struct enumeration* enumeration;
   int value;
   bool has_str;
};

struct structure {
   struct object object;
   struct name* name;
   struct name* body;
   struct structure_member* member;
   struct structure_member* member_tail;
   int size;
   bool anon;
   bool has_ref_member;
   bool has_mandatory_ref_member;
   bool semicolon;
   bool force_local_scope;
   bool hidden;
};

struct structure_member {
   struct object object;
   struct name* name;
   struct ref* ref;
   struct structure* structure;
   struct enumeration* enumeration;
   struct path* path;
   struct dim* dim;
   struct structure_member* next;
   struct structure_member* next_instance;
   int spec;
   int original_spec;
   int offset;
   int size;
   int diminfo_start;
   bool head_instance;
   bool addr_taken;
};

struct ref {
   struct ref* next;
   struct pos pos;
   enum {
      REF_NULL,
      REF_STRUCTURE,
      REF_ARRAY,
      REF_FUNCTION
   } type;
   bool nullable;
   bool implicit;
};

struct ref_struct {
   struct ref ref;
   int storage;
   int storage_index;
};

struct ref_array {
   struct ref ref;
   int dim_count;
   int storage;
   int storage_index;
};

struct ref_func {
   struct ref ref;
   struct param* params;
   int min_param;
   int max_param;
   bool local;
};

struct type_alias {
   struct object object;
   struct ref* ref;
   struct structure* structure;
   struct enumeration* enumeration;
   struct path* path;
   struct dim* dim;
   struct name* name;
   struct type_alias* next_instance;
   int spec;
   int original_spec;
   bool head_instance;
   bool force_local_scope;
   bool hidden;
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

struct compound_literal {
   struct node node;
   struct var* var;
};

// List of magic identifiers:
// __NAMESPACE__ | Fully-qualified name of the current namespace.
// __FUNCTION__  | Name of the current function.
// __SCRIPT__    | Name or number of the current script.
struct magic_id {
   struct object object;
   struct indexed_string* string;
   enum {
      MAGICID_NAMESPACE,
      MAGICID_FUNCTION,
      MAGICID_SCRIPT,
   } name;
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
   bool fixed;
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
   enum {
      BINARYOPERAND_NONE,
      BINARYOPERAND_PRIMITIVERAW = SPEC_RAW,
      BINARYOPERAND_PRIMITIVEINT = SPEC_INT,
      BINARYOPERAND_PRIMITIVEFIXED = SPEC_FIXED,
      BINARYOPERAND_PRIMITIVEBOOL = SPEC_BOOL,
      BINARYOPERAND_PRIMITIVESTR = SPEC_STR,
      BINARYOPERAND_REF,
      BINARYOPERAND_REFFUNC
   } operand_type;
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
   enum {
      ASSIGNLSIDE_NONE,
      ASSIGNLSIDE_PRIMITIVE,
      ASSIGNLSIDE_PRIMITIVESTR,
      ASSIGNLSIDE_PRIMITIVEFIXED,
      ASSIGNLSIDE_REF,
      ASSIGNLSIDE_REFARRAY,
   } lside_type;
};

struct conditional {
   struct node node;
   struct pos pos;
   struct node* left;
   struct node* middle;
   struct node* right;
   struct ref* ref;
   int left_spec;
   int left_value;
   bool folded;
};

struct subscript {
   struct node node;
   struct node* lside;
   struct expr* index;
   struct pos pos;
   bool string;
};

struct access {
   struct node node;
   struct pos pos;
   struct node* lside;
   struct node* rside;
   const char* name;
   enum {
      ACCESS_STRUCTURE,
      ACCESS_NAMESPACE,
      ACCESS_ARRAY,
      ACCESS_STR
   } type;
};

struct call {
   struct node node;
   struct pos pos;
   struct node* operand;
   struct func* func;
   struct ref_func* ref_func;
   struct nested_call* nested_call;
   struct format_item* format_item;
   struct list args;
   bool constant;
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

struct memcpy_call {
   struct node node;
   struct expr* destination;
   struct expr* destination_offset;
   struct expr* destination_length;
   struct expr* source;
   struct expr* source_offset;
   enum {
      MEMCPY_ARRAY,
      MEMCPY_STRUCT,
   } type;
   bool array_cast;
};

struct lengthof {
   struct node node;
   struct expr* operand;
   int value;
};

struct conversion {
   struct node node;
   struct expr* expr;
   int spec;
   int spec_from;
   bool from_ref;
};

struct sure {
   struct node node;
   struct pos pos;
   struct node* operand;
   struct ref* ref;
   bool already_safe;
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
   struct expr* return_value;
   struct buildmsg* buildmsg;
   struct return_stmt* next;
   struct c_jump* epilogue_jump;
   struct pos pos;
};

struct block {
   struct node node;
   struct list stmts;
   struct pos pos;
};

struct cond {
   union {
      struct node* node;
      struct expr* expr;
      struct var* var;
   } u;
};

struct heavy_cond {
   struct expr* expr;
   struct var* var;
};

struct if_stmt {
   struct node node;
   struct heavy_cond cond;
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
   struct heavy_cond cond;
   // Cases are sorted by their value, in ascending order. Default case not
   // included in this list.
   struct case_label* case_head;
   struct case_label* case_default;
   struct jump* jump_break;
   struct node* body;
};

struct while_stmt {
   struct node node;
   struct cond cond;
   struct block* body;
   struct jump* jump_break;
   struct jump* jump_continue;
   bool until;
};

struct do_stmt {
   struct node node;
   struct expr* cond;
   struct block* body;
   struct jump* jump_break;
   struct jump* jump_continue;
   bool until;
};

struct for_stmt {
   struct node node;
   struct list init;
   struct list post;
   struct cond cond;
   struct node* body;
   struct jump* jump_break;
   struct jump* jump_continue;
};

struct foreach_stmt {
   struct node node;
   struct var* key;
   struct var* value;
   struct expr* collection;
   struct node* body;
   struct jump* jump_break;
   struct jump* jump_continue;
};

struct buildmsg {
   struct expr* expr;
   struct block* block;
   struct list usages;
};

struct buildmsg_usage {
   struct buildmsg* buildmsg;
   struct c_point* point;
};

struct buildmsg_stmt {
   struct node node;
   struct buildmsg* buildmsg;
};

struct expr_stmt {
   struct node node;
   struct list expr_list;
};

struct dim {
   struct dim* next;
   // NOTE: The following field is not cached. It will be NULL for a dimension
   // that is retrieved from a cached library.
   struct expr* length_node;
   int length;
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
   union {
      struct {
         struct indexed_string* string;
      } string;
      struct {
         struct indexed_string* string;
      } stringinitz;
      struct {
         struct func* func;
      } funcref;
      struct {
         struct var* var;
         struct structure_member* structure_member;
         int offset;  // Offset to array.
         int diminfo; // Offset to dimension information.
      } arrayref;
      struct {
         struct var* var;
         int offset;
      } structref;
   } more;
   enum {
      VALUE_EXPR,
      VALUE_STRING,
      VALUE_STRINGINITZ,
      VALUE_ARRAYREF,
      VALUE_STRUCTREF,
      VALUE_FUNCREF,
      VALUE_TOTAL,
   } type;
   int index;
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
   struct ref* ref;
   struct structure* structure;
   struct enumeration* enumeration;
   struct path* type_path;
   struct dim* dim;
   struct initial* initial;
   struct value* value;
   struct var* next_instance;
   int spec;
   int original_spec;
   int storage;
   int index;
   int size;
   int diminfo_start;
   enum {
      DESC_NONE,
      DESC_ARRAY,
      DESC_STRUCTVAR,
      DESC_REFVAR,
      DESC_PRIMITIVEVAR,
   } desc;
   bool initz_zero;
   bool hidden;
   bool used;
   bool modified;
   bool initial_has_str;
   bool imported;
   bool is_constant_init;
   bool addr_taken;
   bool in_shared_array;
   bool force_local_scope;
   bool constant;
   bool external;
   bool head_instance;
   bool anon;
};

struct param {
   struct object object;
   struct ref* ref;
   struct structure* structure;
   struct enumeration* enumeration;
   struct path* path;
   struct param* next;
   struct name* name;
   struct expr* default_value;
   int spec;
   int original_spec;
   int index;
   int size;
   int obj_pos;
   bool used;
   bool default_value_tested;
};

struct func {
   struct object object;
   enum {
      FUNC_ASPEC,
      FUNC_EXT,
      FUNC_DED,
      FUNC_FORMAT,
      FUNC_USER,
      FUNC_INTERNAL,
      FUNC_SAMPLE,
      FUNC_ALIAS
   } type;
   struct ref* ref;
   struct structure* structure;
   struct enumeration* enumeration;
   struct path* path;
   struct name* name;
   struct param* params;
   void* impl;
   int return_spec;
   int original_return_spec;
   int min_param;
   int max_param;
   bool hidden;
   bool imported;
   bool external;
   bool force_local_scope;
   bool literal;
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
   enum {
      FCAST_ARRAY,
      FCAST_BINARY,
      FCAST_CHAR,
      FCAST_DECIMAL,
      FCAST_FIXED,
      FCAST_RAW,
      FCAST_KEY,
      FCAST_LOCAL_STRING,
      FCAST_NAME,
      FCAST_STRING,
      FCAST_HEX,
      FCAST_BUILDMSG,
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

struct format_item_buildmsg {
   struct buildmsg_usage* usage;
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
   struct list vars;
   struct list funcscope_vars;
   int index;
   int size;
   int usage;
   int obj_pos;
   enum {
      RECURSIVE_UNDETERMINED,
      RECURSIVE_POSSIBLY
   } recursive;
   bool nested;
   bool local;
};

struct func_intern {
   enum {
      INTERN_FUNC_ACS_EXECWAIT,
      INTERN_FUNC_ACS_NAMEDEXECUTEWAIT,
      INTERN_FUNC_STANDALONE_TOTAL,

      INTERN_FUNC_STR_LENGTH = INTERN_FUNC_STANDALONE_TOTAL,
      INTERN_FUNC_ARRAY_LENGTH,
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
      struct {
         struct expr* red;
         struct expr* green;
         struct expr* blue;
      } colorisation;
      struct {
         struct expr* amount;
         struct expr* red;
         struct expr* green;
         struct expr* blue;
      } tint;
   } value;
   bool rgb;
   bool saturated;
   bool colorisation;
   bool tint;
};

struct paltrans {
   struct node node;
   struct expr* number;
   struct palrange* ranges;
   struct palrange* ranges_tail;
};

struct label {
   struct node node;
   struct pos pos;
   const char* name;
   struct buildmsg* buildmsg;
   struct c_point* point;
   // goto statements that use this label.
   struct list users;
};

struct goto_stmt {
   struct node node;
   int obj_pos;
   struct label* label;
   const char* label_name;
   struct pos pos;
   struct pos label_name_pos;
   struct buildmsg* buildmsg;
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
      SCRIPT_TYPE_KILL,
      SCRIPT_TYPE_REOPEN,
      SCRIPT_TYPE_NEXTFREENUMBER,
      SCRIPT_TYPE_TOTAL = SCRIPT_TYPE_NEXTFREENUMBER
   } type;
   enum {
      SCRIPT_FLAG_NONE = 0x0,
      SCRIPT_FLAG_NET = 0x1,
      SCRIPT_FLAG_CLIENTSIDE = 0x2
   } flags;
   struct param* params;
   struct block* body;
   struct func* nested_funcs;
   struct call* nested_calls;
   struct list labels;
   struct list vars;
   struct list funcscope_vars;
   int assigned_number;
   int num_param;
   int offset;
   int size;
   bool named_script;
};

struct alias {
   struct object object;
   struct object* target;
};

struct constant {
   struct object object;
   struct name* name;
   // NOTE: The following field is not cached. It will be NULL for a constant
   // that is retrieved from a cached library.
   struct expr* value_node;
   int spec;
   int value;
   bool hidden;
   bool has_str;
   bool force_local_scope;
};

struct indexed_string {
   struct indexed_string* next;
   struct indexed_string* left;
   struct indexed_string* right;
   const char* value;
   int length;
   int index;
   int index_runtime;
   bool used;
   bool in_source_code;
};

struct indexed_string_usage {
   struct node node;
   struct indexed_string* string;
};

struct str_table {
   struct indexed_string* head;
   struct indexed_string* tail;
   struct indexed_string* root;
   int size;
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
      INLINE_ASM_ARG_STRING,
      INLINE_ASM_ARG_EXPR,
      INLINE_ASM_ARG_LABEL,
      INLINE_ASM_ARG_VAR,
      INLINE_ASM_ARG_PARAM,
      INLINE_ASM_ARG_FUNC
   } type;
   union {
      int number;
      const char* id;
      struct indexed_string* string;
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
   struct expr* message;
   struct pos pos;
   struct indexed_string* file;
   bool is_static;
};

struct ns {
   struct object object;
   struct ns* parent;
   struct name* name;
   struct name* body;
   struct name* body_structs;
   struct name* body_enums;
   struct ns_link* links;
   struct list fragments;
   bool hidden;
   bool dot_separator;
};

struct ns_link {
   struct ns_link* next;
   struct ns* ns;
   struct pos pos;
};

struct ns_fragment {
   struct object object;
   struct ns* ns;
   struct ns_path* path;
   struct object* unresolved;
   struct object* unresolved_tail;
   struct list objects;
   struct list funcs;
   struct list scripts;
   // Contains functions, scripts, and namespace fragments.
   struct list runnables;
   struct list fragments;
   struct list usings;
   // Enables: strong typing; block scoping of local objects.
   bool strict;
   bool hidden;
};

struct ns_path {
   struct ns_path* next;
   const char* text;
   struct pos pos;
   bool dot_separator;
};

struct using_dirc {
   struct node node;
   struct path* path;
   struct list items;
   struct pos pos;
   enum {
      USING_ALL,
      USING_SELECTION
   } type;
   bool force_local_scope;
};

struct using_item {
   const char* name;
   const char* usage_name;
   struct alias* alias;
   struct pos pos;
   enum {
      USINGITEM_OBJECT,
      USINGITEM_STRUCT,
      USINGITEM_ENUM,
   } type;
};

struct import_dirc {
   const char* file_path;
   struct library* lib;
   struct pos pos;
};

struct library {
   struct str name;
   struct pos name_pos;
   struct list vars;
   struct list funcs;
   struct list scripts;
   struct list objects;
   struct list private_objects;
   // #included/#imported libraries.
   struct list import_dircs;
   struct list dynamic;
   struct list dynamic_acs;
   struct list dynamic_bcs;
   struct list links;
   struct list files;
   struct list external_vars;
   struct list external_funcs;
   struct ns_fragment* upmost_ns_fragment;
   struct file_entry* file;
   struct pos file_pos;
   int id;
   enum {
      FORMAT_ZERO,
      FORMAT_BIG_E,
      FORMAT_LITTLE_E
   } format;
   enum {
      LANG_ACS,
      LANG_ACS95,
      LANG_BCS
   } lang;
   bool importable;
   bool imported;
   bool encrypt_str;
   bool header;
   // Only applies to main module.
   bool wadauthor;
   bool uses_nullable_refs;
};

struct library_link {
   const char* name;
   struct pos pos;
   bool needed;
};

enum { MAX_WORLD_VARS = 256 };
enum { MAX_GLOBAL_VARS = 64 };

struct lang_limits {
   int max_world_vars;
   int max_global_vars;
   int max_scripts;
   int max_script_params;
   int max_strings;
   int max_string_length;
   int max_id_length;
};

struct task {
   struct options* options;
   FILE* err_file;
   jmp_buf* bail;
   struct text_buffer* text_buffer;
   struct file_entry* file_entries;
   struct str_table str_table;
   struct str_table script_name_table;
   struct indexed_string* empty_string;
   struct library* library_main;
   // Imported libraries come first, followed by the main library.
   struct list libraries;
   struct list namespaces;
   int last_id;
   time_t compile_time;
   struct gbuf growing_buffer;
   struct list runtime_asserts;
   struct name* root_name;
   struct name* array_name;
   struct name* str_name;
   struct name* blank_name;
   struct func* append_func;
   // Both `dummy_expr` and `raw0_expr` refer to the same node.
   struct expr* dummy_expr;
   struct expr* raw0_expr;
   struct ns* upmost_ns;
   struct str err_file_dir;
   struct list include_history;
   struct str* compiler_dir;
   struct str bcs_lib_dir;
   struct str acs_lib_dir;
   // The file printed in the last diagnostic.
   struct include_history_entry* last_diag_file;
   // All structs found during compilation, including local structs and structs
   // in imported libraries.
   struct list structures;
};

#define DIAG_NONE 0x0
#define DIAG_FILE 0x1
#define DIAG_LINE 0x2
#define DIAG_COLUMN 0x4
#define DIAG_WARN 0x8
#define DIAG_ERR 0x10
#define DIAG_SYNTAX 0x20
#define DIAG_INTERNAL 0x40
#define DIAG_NOTE 0x80
#define DIAG_FILENAME 0x100
#define DIAG_POS DIAG_FILE | DIAG_LINE | DIAG_COLUMN
#define DIAG_POS_ERR DIAG_POS | DIAG_ERR

#define NAMESEPARATOR_COLONCOLON "::"
#define NAMESEPARATOR_DOT "."
#define NAMESEPARATOR_INTERNAL NAMESEPARATOR_DOT 

void t_init( struct task* task, struct options* options, jmp_buf* bail,
   struct str* compiler_dir );
void t_copy_name( struct name* start, struct str* str );
void t_copy_full_name( struct name* start, const char* separator,
   struct str* str );
int t_full_name_length( struct name* name, const char* separator );
void t_print_name( struct name* );
void t_diag( struct task*, int flags, ... );
void t_diag_args( struct task* task, int flags, va_list* args );
void t_bail( struct task* );
void t_deinit( struct task* task );
bool t_same_pos( struct pos*, struct pos* );
void t_decode_pos( struct task* task, struct pos* pos, const char** file,
   int* line, int* column );
const char* t_decode_pos_file( struct task* task, struct pos* pos );
void t_init_object( struct object* object, int node_type );
void t_init_file_query( struct file_query* query, const char* lang_dir,
   struct file_entry* offset_file, const char* path );
void t_find_file( struct task* task, struct file_query* query );
struct library* t_add_library( struct task* task );
struct name* t_create_name( void );
struct name* t_extend_name( struct name* parent, const char* extension );
struct indexed_string* t_intern_string( struct task* task,
   const char* value, int length );
struct indexed_string* t_intern_string_copy( struct task* task,
   const char* value, int length );
struct indexed_string* t_lookup_string( struct task* task, int index );
struct ns* t_alloc_ns( struct name* name );
void t_append_unresolved_namespace_object( struct ns_fragment* fragment,
   struct object* object );
struct constant* t_alloc_constant( void );
struct enumeration* t_alloc_enumeration( void );
struct enumerator* t_alloc_enumerator( void );
void t_append_enumerator( struct enumeration* enumeration,
   struct enumerator* enumerator );
struct structure* t_alloc_structure( void );
struct structure_member* t_alloc_structure_member( void );
void t_append_structure_member( struct structure* structure,
   struct structure_member* member );
struct dim* t_alloc_dim( void );
struct var* t_alloc_var( void );
struct func* t_alloc_func( void );
struct func_user* t_alloc_func_user( void );
struct param* t_alloc_param( void );
struct format_item* t_alloc_format_item( void );
struct call* t_alloc_call( void );
int t_dim_size( struct dim* dim );
const struct lang_limits* t_get_lang_limits( int lang );
const char* t_get_storage_name( int storage );
struct literal* t_alloc_literal( void );
struct expr* t_alloc_expr( void );
void t_create_builtins( struct task* task, int lang );
struct indexed_string_usage* t_alloc_indexed_string_usage( void );
void t_init_pos( struct pos* pos, int id, int line, int column );
void t_init_pos_id( struct pos* pos, int id );
struct indexed_string* t_intern_script_name( struct task* task,
   const char* value, int length );
struct ref_func* t_alloc_ref_func( void );
struct ns_fragment* t_alloc_ns_fragment( void );
struct type_alias* t_alloc_type_alias( void );
char* t_intern_text( struct task* task, const char* value, int length );
struct text_buffer* t_get_text_buffer( struct task* task, int min_free_size );
void t_update_err_file_dir( struct task* task, const char* path );
struct include_history_entry* t_reserve_include_history_entry(
   struct task* task );
struct include_history_entry* t_decode_include_history_entry(
   struct task* task, int id );
const char* t_get_lang_lib_dir( struct task* task, int lang );
struct script* t_alloc_script( void );
struct ns* t_find_ns_of_object( struct task* task, struct object* object );

#endif
