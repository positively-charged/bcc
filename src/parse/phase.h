#ifndef SRC_PARSE_PHASE_H
#define SRC_PARSE_PHASE_H

#include "task.h"

struct cache;

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
   TK_GOTO,
   TK_TRUE,
   TK_FALSE,
   TK_EVENT,
   // 100
   TK_NL,
   TK_LIB,
   TK_LIB_END,
   TK_LIT_BINARY,
   TK_QUESTION_MARK,
   TK_SPACE,
   TK_TAB,
   TK_ELLIPSIS,
   TK_HORZSPACE,
   TK_PREP_HASHHASH,
   // 110
   TK_RAW,
   TK_FIXED,
   TK_CAST,
   TK_ASSERT,
   TK_REF,
   TK_AUTO,
   TK_TYPEDEF,
   TK_FOREACH,
   TK_PRIVATE,
   TK_OBJCPY,
   // 120
   TK_MSGBUILD,
   TK_EXTSPEC,
   TK_IN,
   TK_NULL,
   TK_SPECIAL,
   TK_NAMESPACE,
   TK_UPMOST,
   TK_USING,

   TK_TOTAL,

   // Pseudo tokens.
   TK_NONE,
   TK_STRINGIZE,
   TK_PLACEMARKER,
   TK_PROCESSEDHASH,
};

struct token {
   struct token* next;
   // The text contains the character content of the token. The text and the
   // length of the text are applicable only to a token that is an identifier,
   // a string, a character literal, or any of the numbers. For the rest, the
   // text will be NULL and the length will be zero.
   const char* text;
   struct pos pos;
   enum tk type;
   int length;
   bool is_id;
};

enum {
   PREPTK_NONE = 0x0,
   PREPTK_ACCEPTHORZSPACE = 0x1,
   PREPTK_ACCEPTNL = 0x2,
   PREPTK_EXPANDMACROS = 0x4,
};

enum {
   TKF_NONE,
   TKF_KEYWORD = 0x1,
};

struct token_info {
   const char* shared_text;
   unsigned int length;
   unsigned int flags;
};

struct macro {
   const char* name;
   struct macro* next;
   struct macro_param* param_head;
   struct macro_param* param_tail;
   struct token* body;
   struct token* body_tail;
   struct pos pos;
   int param_count;
   bool func_like;
   bool variadic;
};

struct macro_param {
   const char* name;
   struct macro_param* next;
};

struct queue_entry {
   struct queue_entry* next;
   struct token* token;
   bool token_allocated;
};

struct token_queue {
   struct queue_entry* head;
   struct queue_entry* tail;
   struct queue_entry* prev_entry;
   int size;
   bool stream;
};

struct parsertk_iter {
   struct queue_entry* entry;
   struct token* token;
};

struct streamtk_iter {
   struct queue_entry* entry;
   struct token* token;
};

enum { SOURCE_BUFFER_SIZE = 16384 };

struct source {
   struct file_entry* file;
   FILE* fh;
   struct source* prev;
   int file_entry_id;
   int line;
   int column;
   bool find_dirc;
   bool load_once;
   bool imported;
   char ch;
   // Plus one for the null character.
   char buffer[ SOURCE_BUFFER_SIZE + 2 ];
   int buffer_pos;
};

struct request {
   const char* given_path;
   struct file_entry* file;
   struct source* source;
   bool err_open;
   bool err_loading;
   bool err_loaded_before;
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
   struct structure* structure;
   struct enumeration* enumeration;
   struct path* path;
   struct ref* ref;
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
   int spec;
   bool private_visibility;
   bool static_qual;
   bool leave;
   bool read_func;
   bool msgbuild;
   bool extended_spec;
};

struct stmt_reading {
   struct list* labels;
   struct node* node;
   struct block* block_node;
   struct packed_expr* packed_expr;
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
   //struct token* queue[ TK_BUFFER_SIZE ];
   //struct token queue_source[ TK_BUFFER_SIZE ];
   struct token* token;
   struct token* token_free;
   struct token token_source;
   struct token token_peeked;
   enum tk tk;
   enum tk prev_tk;
   struct pos tk_pos;
   const char* tk_text;
   int tk_length;
   struct source* source;
   struct source* free_source;
   int last_id;
   struct str temp_text;
   struct list text_buffers;
   enum {
      READF_NONE = 0x0,
      READF_CONCATSTRINGS = 0x1,
      READF_ESCAPESEQ = 0x2,
      READF_NL = 0x4,
      READF_SPACETAB = 0x8,
   } read_flags;
   bool line_beginning;
   bool concat_strings;
   struct macro* macro_head;
   struct macro* macro_free;
   struct macro_param* macro_param_free;
   struct macro_expan* macro_expan;
   struct macro_expan* macro_expan_free;
   struct ifdirc* ifdirc_top;

   int line;
   int column;
   enum {
      PREDEFMACROEXPAN_NONE,
      PREDEFMACROEXPAN_LINE,
      PREDEFMACROEXPAN_FILE,
      PREDEFMACROEXPAN_TIME,
      PREDEFMACROEXPAN_DATE,
   } predef_macro_expan;
   enum {
      PREPCONTEXT_NONE,
      PREPCONTEXT_DEFINEBODY
   } prep_context;
   struct cache* cache;

   struct token* source_token;
   struct queue_entry* tkque_free_entry;
   struct token_queue tkque;
   struct token_queue parser_tkque;
   bool create_nltk;
   struct ns* ns;
   struct list* local_vars;
   struct library* lib;
};

void p_init( struct parse* parse, struct task* task, struct cache* cache );
void p_init_stream( struct parse* parse );
void p_read( struct parse* parse );
void p_preprocess( struct parse* parse );
void p_diag( struct parse* parse, int flags, ... );
void p_bail( struct parse* parse );
void p_load_main_source( struct parse* parse );
void p_load_imported_lib_source( struct parse* parse, struct import_dirc* dirc,
   struct file_entry* file );
void p_read_tk( struct parse* parse );
void p_read_preptk( struct parse* parse );
void p_read_expanpreptk( struct parse* parse );
void p_read_eoptiontk( struct parse* parse );
void p_read_stream( struct parse* parse );
void p_read_stream_custom( struct parse* parse, int options );
void p_test_tk( struct parse* parse, enum tk type );
void p_test_preptk( struct parse* parse, enum tk type );
void p_test_stream( struct parse* parse, enum tk type );
void p_read_top_stmt( struct parse* parse, struct stmt_reading* reading,
   bool need_block );
void p_skip_block( struct parse* parse );
void p_init_expr_reading( struct expr_reading* reading, bool in_constant,
   bool skip_assign, bool skip_func_call, bool expect_expr );
void p_read_expr( struct parse* parse, struct expr_reading* reading );
bool p_is_dec( struct parse* parse );
void p_init_dec( struct dec* dec );
void p_read_dec( struct parse* parse, struct dec* dec );
void p_init_stmt_reading( struct stmt_reading* reading, struct list* labels );
enum tk p_peek( struct parse* parse );
struct token* p_peek_tk( struct parse* parse );
struct token* p_peek_stream( struct parse* parse, bool nonwhitespace );
const char* p_get_token_name( enum tk type );
int p_extract_literal_value( struct parse* parse );
void p_load_included_source( struct parse* parse, const char* file_path,
   struct pos* pos );
void p_read_lib( struct parse* parse );
void p_read_script( struct parse* parse );
void p_add_unresolved( struct parse* parse, struct object* object );
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
void p_init_request( struct request* request, const char* path );
void p_load_source( struct parse* parse, struct request* request );
void p_read_source( struct parse* parse, struct token* token );
bool p_read_dirc( struct parse* parse );
void p_confirm_ifdircs_closed( struct parse* parse );
struct macro* p_find_macro( struct parse* parse, const char* name );
void p_macro_trace_diag( struct parse* parse );
bool p_read_dirc( struct parse* parse );
enum tk p_identify_token_type( const char* text );
int p_eval_prep_expr( struct parse* parse );
void p_read_sourcepos_token( struct parse* parse, struct pos* pos );
int p_identify_predef_macro( const char* text );
const struct token_info* p_get_token_info( enum tk tk );
struct token* p_alloc_token( struct parse* parse );
void p_free_token( struct parse* parse, struct token* token );
void p_init_parsertk_iter( struct parse* parse, struct parsertk_iter* iter );
void p_next_tk( struct parse* parse, struct parsertk_iter* iter );
void p_init_streamtk_iter( struct parse* parse, struct streamtk_iter* iter );
void p_next_stream( struct parse* parse, struct streamtk_iter* iter );
bool p_expand_macro( struct parse* parse );
void p_init_token_queue( struct token_queue* queue, bool stream );
struct queue_entry* p_push_entry( struct parse* parse,
   struct token_queue* queue );
struct token* p_shift_entry( struct parse* parse, struct token_queue* queue );
void p_fill_queue( struct parse* parse, struct token_queue* queue,
   int required_size );
void p_read_mnemonic( struct parse* parse );
void p_read_asm( struct parse* parse, struct stmt_reading* reading );
void p_read_foreach_item( struct parse* parse, struct foreach_stmt* stmt );
void p_read_special_list( struct parse* parse );
void p_skip_semicolon( struct parse* parse );
bool p_peek_path( struct parse* parse, struct parsertk_iter* iter );
void p_read_using( struct parse* parse, struct list* output );
void p_read_target_lib( struct parse* parse );
void p_clear_macros( struct parse* parse );
void p_define_imported_macro( struct parse* parse );
void p_define_cmdline_macros( struct parse* parse );
void p_read_func_body( struct parse* parse, struct func* func );

#endif