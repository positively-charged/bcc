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
   TK_STRCPY,
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
   TK_GOTO,
   TK_TRUE,
   TK_FALSE,
   // 100
   TK_EVENT,
   TK_NL,
   TK_LIB,
   TK_LIB_END,
   TK_LIT_BINARY,
   TK_QUESTION_MARK,
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

enum {
   TKF_NONE,
   TKF_KEYWORD = 0x1,
};

struct token_info {
   char* shared_text;
   unsigned int length;
   unsigned int flags;
};

enum { SOURCE_BUFFER_SIZE = 1024 };

struct source {
   struct file_entry* file;
   FILE* fh;
   struct source* prev;
   int line;
   int column;
   bool find_dirc;
   bool load_once;
   bool imported;
   char ch;
   // Plus one for the null character.
   char buffer[ SOURCE_BUFFER_SIZE + 1 ];
   int buffer_pos;
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
   struct pos name_pos;
   struct pos static_qual_pos;
   struct type* type;
   struct type* type_make;
   struct path* type_path;
   struct name* name;
   struct name* name_offset;
   struct dim* dim;
   struct list* vars;
   struct {
      struct pos pos;
      int type;
      bool specified;
   } storage;
   struct {
      struct pos pos;
      int value;
      bool specified;
   } storage_index;
   struct {
      struct initial* initial;
      struct pos pos;
      bool specified;
      bool has_str;
   } initz;
   bool type_void;
   bool type_struct;
   bool static_qual;
   bool leave;
   bool read_func;
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
   char* tk_text;
   int tk_length;
   struct source* source;
   struct source* main_source;
   struct source* free_source;
   int last_id;
   struct str temp_text;
   struct list text_buffers;
};

void p_init( struct parse* parse, struct task* task );
void p_read( struct parse* parse );
void p_diag( struct parse* parse, int flags, ... );
void p_bail( struct parse* parse );
void p_load_main_source( struct parse* parse );
void p_read_tk( struct parse* parse );
void p_test_tk( struct parse* parse, enum tk type );
void p_read_top_stmt( struct parse* parse, struct stmt_reading* reading,
   bool need_block );
struct format_item* p_read_format_item( struct parse* parse, bool colon );
void p_skip_block( struct parse* parse );
void p_init_expr_reading( struct expr_reading* reading, bool in_constant,
   bool skip_assign, bool skip_func_call, bool expect_expr );
void p_read_expr( struct parse* parse, struct expr_reading* reading );
bool p_is_dec( struct parse* parse );
void p_init_dec( struct dec* dec );
void p_read_dec( struct parse* parse, struct dec* dec );
void p_init_stmt_reading( struct stmt_reading* reading, struct list* labels );
enum tk p_peek( struct parse* parse );
const char* p_get_token_name( enum tk type );
int p_extract_literal_value( struct parse* parse );
struct source* p_load_included_source( struct parse* parse );
void p_read_lib( struct parse* parse );
void p_read_script( struct parse* parse );
void p_add_unresolved( struct library* lib, struct object* object );
struct path* p_read_path( struct parse* parse );
void p_increment_pos( struct pos* pos, enum tk tk );
void p_unexpect_diag( struct parse* parse );
void p_unexpect_item( struct parse* parse, struct pos* pos, enum tk tk );
void p_unexpect_name( struct parse* parse, struct pos* pos,
   const char* subject );
void p_unexpect_last( struct parse* parse, struct pos* pos, enum tk tk );
void p_unexpect_last_name( struct parse* parse, struct pos* pos,
   const char* subject );
void p_load_library( struct parse* parent );
void p_deinit_tk( struct parse* parse );

#endif