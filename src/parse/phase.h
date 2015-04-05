#ifndef SRC_PARSE_PHASE_H
#define SRC_PARSE_PHASE_H

#include "task.h"

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
   TK_QUESTION_MARK,
   TK_STRCPY,
   TK_TOTAL
};

struct token {
   // The text contains the character content of the token. The text and the
   // length of the text are applicable only to a token that is an identifier,
   // a string, a character literal, or any of the numbers. For the rest, the
   // text will be NULL and the length will be zero.
   char* text;
   struct pos pos;
   enum tk type;
   int length;
};

struct source {
   struct file_entry* file;
   char* text;
   char* left;
   char* save;
   struct source* prev;
   int line;
   int column;
   bool find_dirc;
   bool load_once;
   bool imported;
   char ch;
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
   struct pos static_qual_pos;
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
   bool type_void;
   bool type_struct;
   bool initz_str;
   bool static_qual;
   bool leave;
   bool read_func;
   bool read_objects;
};

struct stmt_reading {
   struct list* labels;
   struct node* node;
   struct block* block_node;
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

#define TK_BUFFER_SIZE 5

struct parse {
   struct task* task;
   struct token queue[ TK_BUFFER_SIZE ];
   int peeked;
   enum tk tk;
   struct pos tk_pos;
   // The text contains the character content of the token. The text and the
   // length of the text are applicable only to a token that is an identifier,
   // a string, a character literal, or any of the numbers. For the rest, the
   // text will be NULL and the length will be zero.
   char* tk_text;
   int tk_length;
   struct source* source;
   struct source* main_source;
   struct region* region;
   int last_id;
};

void p_init( struct parse* phase, struct task* task );
void p_read( struct parse* phase );
void p_diag( struct parse* phase, int flags, ... );
void p_bail( struct parse* phase );
void p_load_main_source( struct parse* phase );
void p_read_tk( struct parse* phase );
void p_test_tk( struct parse* phase, enum tk type );
void p_read_top_stmt( struct parse* phase, struct stmt_reading* reading,
   bool need_block );
struct format_item* p_read_format_item( struct parse* phase, bool colon );
void p_skip_block( struct parse* phase );
void p_init_expr_reading( struct expr_reading* reading, bool in_constant,
   bool skip_assign, bool skip_func_call, bool expect_expr );
void p_read_expr( struct parse* phase, struct expr_reading* reading );
bool p_is_dec( struct parse* phase );
void p_init_dec( struct dec* dec );
void p_read_dec( struct parse* phase, struct dec* dec );
void p_init_stmt_reading( struct stmt_reading* reading, struct list* labels );
enum tk p_peek( struct parse* phase );
void p_read_region_body( struct parse* phase, bool is_brace );
const char* p_get_token_name( enum tk type );
void p_read_region( struct parse* phase );
void p_read_import( struct parse* phase, struct list* output );
int p_extract_literal_value( struct parse* phase );
struct source* p_load_included_source( struct parse* phase );
void p_read_lib( struct parse* phase );
void p_read_script( struct parse* parse );
void p_add_unresolved( struct region* region, struct object* object );
struct path* p_read_path( struct parse* phase );

#endif