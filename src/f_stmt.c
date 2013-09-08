#include <string.h>

#include "common.h"
#include "detail.h"
#include "f_main.h"
#include "mem.h"

#define MAX_MODULE_NAME_LENGTH 8
#define DEFAULT_LOCAL_SIZE 20

typedef struct {
   node_t* node;
} p_cond_t;

typedef struct sweep_t {
   struct sweep_t* prev;
   struct sweep_t* next;
   nkey_t* names[ DEFAULT_LOCAL_SIZE ];
   int size;
} sweep_t;

static void load_primitives( front_t* );
static void module_read( front_t* );
static module_t* module_new( void );
static void module_set_name( front_t*, module_t*, token_t* );
static void read_dirc( front_t*, position_t* );
static void read_define( front_t*, position_t*, bool );
static void include_file( front_t*, token_t* );
static void read_import( front_t* );
static void read_stmt( front_t* );
static void read_jump( front_t* );
static void read_script_jump( front_t* );
static void read_return( front_t* );
static void read_expr_stmt( front_t* );
static void read_format_stmt( front_t* );
static void read_if( front_t* );
static void read_switch( front_t* );
static void read_case( front_t* );
static void read_while( front_t* );
static void read_do_while( front_t* );
static void read_for( front_t* );
static void skip_imported_block( front_t* );
static void skip_imported_semicolon( front_t* );
static void read_id_stmt( front_t* );
static bool is_name( token_t*, const char* );
static void update_flow_with_loop( p_block_t*, expr_t*, int );
static int get_storage_of_scope( front_t* );
static void add_sweep( front_t* );
static void sweep_key( front_t*, nkey_t* );
static nval_t* use_nval( front_t* );
static void reuse_nval( front_t*, nval_t* );
static p_block_t* block_find_parent( p_block_t*, int );
static void read_paltrans( front_t* );
static void read_palrange_rgb_field( front_t*, p_expr_t*, p_expr_t*,
   p_expr_t* );
static void alloc_offset( ast_t* );

void f_init( front_t* front, options_t* options, ntbl_t* name_table,
   file_table_t* file_table, ast_t* ast ) {
   front->options = options;
   front->name_table = name_table;
   front->ast = ast;
   front->case_stmt = NULL;
   front->expr = NULL;
   front->reading_script_number = false;
   front->importing = false;
   front->str_table.head = NULL;
   front->str_table.tail = NULL;
   front->str_table.sorted_head = NULL;
   front->str_table.size = 0;
   list_init( &front->undef_stack );
   front->depth = 0;
   front->sweep = NULL;
   front->undef_head = NULL;
   front->undef_tail = NULL;
   add_sweep( front );
   front->scope = NULL;
   list_init( &front->scopes );
   front->nval = NULL;
   list_init( &front->anon_types );
   front->anon_type = NULL;
   front->file_table = file_table;
   front->queue.pos = 0;
   front->queue.size = 0;
   front->module = module_new();
   load_primitives( front );
   front->params = NULL;
   front->dec_for = NULL;
   front->dec_type = NULL;
   front->func_type = k_func_user;
   front->block = NULL;
   front->fh = NULL;
   front->err_file = options->err_file;
   front->num_errs = 0;
   front->scope_size = 0;
   front->scope_size_high = 0;
}

void f_deinit( front_t* front ) {
   if ( front->fh ) {
      fclose( front->fh );
   }
   str_del( &front->module->name );
}

void f_bail( front_t* front ) {
   longjmp( front->bail, 1 );
}

typedef struct {
   const char* name;
   const char* return_type;
   int num_param;
   int min_param;
   int id;
} ifunc_temp_t;

static ifunc_temp_t ifunc_temp[] = {
   { "acs_executewait", NULL, 5, 1, k_ifunc_acs_execwait },
   { NULL },
};

static ifunc_temp_t ifunc_str_temp[] = {
   { "[]", "int", 1, 1, k_ifunc_str_subscript },
   { "length", "int", 0, 0, k_ifunc_str_length },
   { NULL },
};

void load_primitives( front_t* front ) {
   static const char* primitive[] = {
      "int", "str", "bool", NULL };
   static int known[] = {
      k_type_int, k_type_str, k_type_bool };
   int i = 0;
   while ( primitive[ i ] ) {
      type_t* type = mem_alloc( sizeof( *type ) );
      type->node.type = k_node_type;
      type->size = 1;
      type->known = known[ i ];
      nkey_t* key = ntbl_goto( front->name_table, "!s", primitive[ i ], "!" );
      nval_t* value = mem_alloc( sizeof( *value ) );
      value->node = ( node_t* ) type;
      value->prev = NULL;
      value->depth = 0;
      key->value = value;
      type->name = key;
      ++i;
   }

   nkey_t* name = ntbl_goto( front->name_table, "!s", "str.", "!" );
   ifunc_temp_t* temp = ifunc_str_temp;
   while ( temp->name ) {
      ifunc_t* ifunc = mem_alloc( sizeof( *ifunc ) );
      ifunc->func.node.type = k_node_func;
      ifunc->func.type = k_func_internal;
      ifunc->func.return_type = NULL;
      ifunc->func.num_param = temp->num_param;
      ifunc->func.min_param = temp->num_param;
      if ( temp->return_type ) {
         nkey_t* key = ntbl_goto( front->name_table,
            "!s", temp->return_type, "!" );
         ifunc->func.return_type = ( type_t* ) key->value->node;
      }
      ifunc->id = temp->id;
      nkey_t* member = ntbl_goto( front->name_table,
         "!o", &name,
         "!s", temp->name, "!" );
      member->value = ( nval_t* ) ifunc;
      ++temp;
   }

   temp = ifunc_temp;
   while ( temp->name ) {
      ifunc_t* ifunc = mem_alloc( sizeof( *ifunc ) );
      ifunc->func.node.type = k_node_func;
      ifunc->func.type = k_func_internal;
      ifunc->func.return_type = NULL;
      ifunc->func.num_param = temp->num_param;
      ifunc->func.min_param = temp->min_param;
      ifunc->func.latent = false;
      ifunc->id = temp->id;
      nkey_t* key = ntbl_goto( front->name_table, "!s", temp->name, "!" );
      nval_t* value = mem_alloc( sizeof( *value ) );
      value->node = ( node_t* ) ifunc;
      value->prev = NULL;
      value->depth = 0;
      key->value = value;
      ++temp;
   }
}

tk_t tk( front_t* front ) {
   return front->token.type;
}

void drop( front_t* front ) {
   tk_read( front );
}

void skip( front_t* front, tk_t expected ) {
   test( front, expected );
   drop( front );
}

token_t* test( front_t* front, tk_t expected ) {
   if ( front->token.type == expected ) {
      return &front->token;
   }
   else {
      const char* found = "reached";
      const char* name = "end of input";
      if ( front->token.type == tk_end ) {
         if ( front->importing ) {
            name = "end of module";
         }
      }
      else {
         found = "found";
         name = tk_name( front->token.type );
      }
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
         "expecting %s but %s %s", tk_name( expected ), found, name );
      f_bail( front );
      return NULL;
   }
}

bool p_build_tree( front_t* front ) {
   bool success = false;
   if ( setjmp( front->bail ) == 0 ) {
      tk_read( front );
      module_read( front );
      if ( ! front->num_errs ) { 
         front->ast->module = front->module;
         front->ast->str_table = front->str_table;
         success = true;
      }
   }
   return success;
}

module_t* module_new( void ) {
   module_t* module = mem_alloc( sizeof( *module ) );
   list_init( &module->scripts );
   list_init( &module->funcs );
   list_init( &module->vars );
   list_init( &module->arrays );
   list_init( &module->hidden_vars );
   list_init( &module->hidden_arrays );
   list_init( &module->customs );
   list_init( &module->imports );
   str_init( &module->name );
   return module;
}

void module_read( front_t* front ) {
   bool got_header = false;
   if ( tk( front ) == tk_hash ) {
      position_t pos = front->token.pos;
      drop( front );
      if ( tk( front ) == tk_id && is_name( &front->token, "library" ) ) {
         drop( front );
         test( front, tk_lit_string );
         module_set_name( front, front->module, &front->token );
         got_header = true;
         drop( front );
      }
      else {
         read_dirc( front, &pos );
      }
   }
   // Header required for an imported module.
   if ( front->importing && ! got_header ) {
      f_diag( front, DIAG_ERR, NULL,
         "missing module name (#library \"<name>\") in imported module" );
   }
   while ( true ) {
      if ( tk( front ) == tk_end ) {
         file_t* file = job_active_file( front->file_table );
         if ( file->imported ) {
            tk_pop_file( front );
            break;
         }
         else if ( ! tk_pop_file( front ) ) {
            break;
         }
         drop( front );
      }
      else if ( p_is_dec( front ) ) {
         p_read_dec( front, k_dec_top );
      }
      else if ( tk( front ) == tk_script ) {
         p_read_script( front );
      }
      else if ( tk( front ) == tk_hash ) {
         position_t pos = front->token.pos;
         drop( front );
         read_dirc( front, &pos );
      }
      else {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
            "unexpected token: %s", front->token.source );
         drop( front );
      }
   }
   // Make sure no objects are left undefined.
   undef_t* undef = front->undef_head;
   if ( undef ) {
      while ( undef ) {
         const char* kind = undef->is_called ? "function" : "object";
         undef_usage_t* usage = undef->usage;
         while ( usage ) {
            char buff[ 100 ];
            f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &usage->pos,
               "unknown %s '%s'", kind,
               ntbl_save( undef->name, buff, 100 ) );
            usage = usage->next;
         }
         undef = undef->next;
      }
      f_bail( front );
   }
   alloc_offset( front->ast );
}

// Checks whether a directive name matches the specified string.
bool is_name( token_t* token, const char* name ) {
   return
      // The substrings must match.
      strncmp( name, token->source, token->length ) == 0 &&
      // And the name must not be a substring.
      name[ token->length ] == '\0';
}

void module_set_name( front_t* front, module_t* module, token_t* token ) {
   if ( ! front->token.length ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
         "module name is blank" );
   }
   else if ( front->token.length > MAX_MODULE_NAME_LENGTH ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
         "module name too long. name must be up to %d characters",
         MAX_MODULE_NAME_LENGTH );
   }
   else {
      str_append( &front->module->name, token->source );
   }
}

void read_dirc( front_t* front, position_t* pos ) {
   test( front, tk_id );
   if ( is_name( &front->token, "define" ) ) {
      drop( front );
      read_define( front, pos, true );
   }
   else if ( is_name( &front->token, "libdefine" ) ) {
      drop( front );
      read_define( front, pos, false );
   }
   else if ( is_name( &front->token, "include" ) ) {
      drop( front );
      test( front, tk_lit_string );
      if ( ! front->importing ) {
         include_file( front, &front->token );
      }
      drop( front );
   }
   else if ( is_name( &front->token, "libinclude" ) ) {
      drop( front );
      test( front, tk_lit_string );
      include_file( front, &front->token );
      drop( front );
   }
   else if ( is_name( &front->token, "import" ) ) {
      test( front, tk_lit_string );
      // Modules imported by an imported module itself are not needed.
      if ( front->importing ) {
         drop( front );
         return;
      }
      include_file( front, &front->token );
      file_t* file = job_active_file( front->file_table );
      file->imported = true;
      module_t* parent = front->module;
      front->module = module_new();
      drop( front );
      front->importing = true;
      module_read( front );
      front->importing = false;
      list_append( &parent->imports, front->module );
      front->module = parent;
   }
   // The library header should not appear as an intermediate directive.
   else if ( is_name( &front->token, "library" ) ) {
      drop( front );
      drop( front );
      // Error.
   }
   // Switch to the Big-E format.
   else if ( is_name( &front->token, "nocompact" ) ) {
      drop( front );
      if ( ! front->importing ) {
         front->options->format = k_format_big_e;
      }
   }
   else if ( is_name( &front->token, "encryptstrings" ) ) {
      drop( front );
      if ( ! front->importing ) {
         front->options->encrypt_str = true;
      }
   }
   else if ( is_name( &front->token, "bfunc" ) ) {
      tk_read( front );
      test( front, tk_lit_string );
      if ( is_name( &front->token, "aspec" ) ) {
         front->func_type = k_func_aspec;
      }
      else if ( is_name( &front->token, "ext" ) ) {
         front->func_type = k_func_ext;
      }
      else if ( is_name( &front->token, "ded" ) ) {
         front->func_type = k_func_ded;
      }
      else if ( is_name( &front->token, "format" ) ) {
         front->func_type = k_func_format;
      }
      // Unknown function category.
      else {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
            "unknown builtin function type" );
         f_bail( front );
      }
      drop( front );
   }
   else if (
      // NOTE: Not sure what these two are. 
      is_name( &front->token, "wadauthor" ) ||
      is_name( &front->token, "nowadauthor" ) ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
         "directive not supported: %s", front->token.source );
      drop( front );
   }
   else {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
         "unknown directive: %s", front->token.source );
      drop( front );
   }
}

void read_define( front_t* front, position_t* pos, bool is_define ) {
   test( front, tk_id );
   nkey_t* key = p_get_unique_name( front );
   drop( front );
   p_expr_t p_expr;
   p_expr_init( &p_expr, k_expr_cond );
   p_expr_read( front, &p_expr );
   if ( ! front->importing || ! is_define ) {
      constant_t* constant = mem_alloc( sizeof( *constant ) );
      constant->node.type = k_node_constant;
      constant->expr = p_expr.expr;
      constant->value = p_expr.expr->value;
      constant->pos = *pos;
      p_use_key( front, key, ( node_t* ) constant, false );
   }
}

void include_file( front_t* front, token_t* token ) {
   int err = tk_load_file( front, token->source );
   if ( err == k_tk_file_err_open ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &token->pos,
         "failed to load file: %s", token->source );
      f_bail( front );
   }
   else if ( err == k_tk_file_err_dup ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &token->pos,
         "file already being processed: %s", token->source );
      f_bail( front );
   }
}

void read_stmt( front_t* front ) {
   switch ( tk( front ) ) {
   case tk_brace_l:
      p_new_scope( front );
      p_read_compound( front );
      p_pop_scope( front );
      break;
   case tk_if:
      read_if( front );
      break;
   case tk_switch:
      read_switch( front );
      break;
   case tk_case:
   case tk_default:
      read_case( front );
      break;
   case tk_while:
   case tk_until:
   case tk_do:
      read_while( front );
      break;
   case tk_for:
      read_for( front );
      break;
   case tk_break:
   case tk_continue:
      read_jump( front );
      break;
   case tk_return:
      read_return( front );
      break;
   case tk_paltrans:
      read_paltrans( front );
      break;
   case tk_semicolon:
      drop( front );
      break;
   default:
      read_expr_stmt( front );
      break;
   }
}

void p_read_compound( front_t* front ) {
   skip( front, tk_brace_l );
   bool done = false;
   while ( ! done ) {
      if ( p_is_dec( front ) ) {
         p_read_dec( front, k_dec_local );
      }
      else if ( tk( front ) == tk_brace_r ) {
         drop( front );
         done = true;
      }
      else {
         read_stmt( front );
      }
   }
}

void read_jump( front_t* front ) {
   jump_t* jump = mem_alloc( sizeof( *jump ) );
   jump->node.type = k_node_jump;
   jump->next = NULL;
   jump->type = k_jump_break;
   jump->offset = 0;
   p_block_t* block = front->block;
   list_append( &block->stmt_list, jump );
   position_t pos = front->token.pos;
   if ( tk( front ) == tk_break ) {
      drop( front );
      skip( front, tk_semicolon );
      p_block_t* closest = block_find_parent( block, k_jump_break );
      if ( closest ) {
         jump->next = closest->jump_break;
         closest->jump_break = jump;
         if ( block->flow == k_flow_going ) {
            block->flow = k_flow_jump_stmt;
            block->jump_stmt.type = k_jump_break;
         }
      }
      else {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "break outside loop or switch" );
      }
   }
   else {
      skip( front, tk_continue );
      skip( front, tk_semicolon );
      p_block_t* closest = block_find_parent( block, k_jump_continue );
      if ( closest ) {
         jump->type = k_jump_continue;
         jump->next = closest->jump_continue;
         closest->jump_continue = jump;
         if ( block->flow == k_flow_going ) {
            block->flow = k_flow_dead;
            block->jump_stmt.type = k_jump_continue;
         }
      }
      else {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "continue outside loop" );
      }
   }
}

p_block_t* block_find_parent( p_block_t* block, int jump ) {
   while ( block ) {
      if ( ( jump == k_jump_break && block->is_break ) ||
         ( jump == k_jump_continue && block->is_continue ) ) {
         return block;
      }
      block = block->parent;
   }
   return NULL;
}

void read_return( front_t* front ) {
   p_block_t* block = front->block;
   position_t pos = front->token.pos;
   drop( front );
   if ( ! block->in_func ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos, 
         "return statement outside function" );
   }
   expr_t* expr = NULL;
   if ( tk( front ) == tk_semicolon ) {
      drop( front );
      if ( block->is_func_return ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "return statement missing return value" );
      }
   }
   else {
      p_expr_t p_expr;
      p_expr_init( &p_expr, k_expr_comma );
      p_expr_read( front, &p_expr );
      skip( front, tk_semicolon );
      expr = p_expr.expr;
      if ( ! block->is_func_return ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "returning value in void function" );
      }
   }
   return_t* node = mem_alloc( sizeof( *node ) );
   node->node.type = k_node_return;
   node->expr = expr;
   list_append( &block->stmt_list, node );
   if ( block->flow == k_flow_going ) {
      block->flow = k_flow_dead;
      block->jump_stmt.pos = pos;
      block->jump_stmt.type = k_jump_return;
   }
}

void read_expr_stmt( front_t* front ) {
   p_expr_t p_expr;
   p_expr_init( &p_expr, k_expr_comma );
   p_expr_read( front, &p_expr );
   skip( front, tk_semicolon );
   list_append( &front->block->stmt_list, p_expr.expr );
}

void read_format_stmt( front_t* front ) {
   while ( true ) {
      test( front, tk_id );
      position_t pos = front->token.pos;
      p_fcast_t fcast;
      p_fcast_init( front, &fcast, &front->token );
      drop( front );
      skip( front, tk_colon );
      p_expr_t p_expr;
      p_expr_init( &p_expr, k_expr_assign );
      p_expr_read( front, &p_expr );
      if ( front->block->flags & k_stmt_format_block ) {
         format_item_t* node = mem_alloc( sizeof( *node ) );
         node->node.type = k_node_format_item;
         node->cast = fcast.cast;
         node->expr = p_expr.expr;
         list_append( &front->block->stmt_list, node );
      }
      else {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "format item outside format block" );
      }
      if ( tk( front ) == tk_comma ) {
         drop( front );
      }
      else {
         break;
      }
   }
   skip( front, tk_semicolon );
}

void read_if( front_t* front ) {
   skip( front, tk_if );
   p_new_scope( front );

   skip( front, tk_paran_l );
   p_expr_t p_expr;
   p_expr_init( &p_expr, k_expr_comma );
   p_expr_read( front, &p_expr );
   skip( front, tk_paran_r );
   expr_t* cond = p_expr.expr;

   p_block_t body;
   p_block_init( &body, front->block, k_stmt_none );
   p_block_t* parent = front->block;
   front->block = &body;
   p_new_scope( front );
   read_stmt( front );
   p_pop_scope( front );
   front->block = parent;

   p_block_t else_body;
   p_block_init( &else_body, front->block, k_stmt_none );
   if ( tk( front ) == tk_else ) {
      drop( front );
      p_new_scope( front );
      parent = front->block;
      front->block = &else_body;
      read_stmt( front );
      front->block = parent;
      p_pop_scope( front );
   }

   if_t* node = mem_alloc( sizeof( *node ) );
   node->node.type = k_node_if;
   node->cond = ( node_t* ) cond;
   node->body = body.stmt_list;
   node->else_body = else_body.stmt_list;
   list_append( &parent->stmt_list, node );

   if ( parent->flow == k_flow_going ) {
      expr_t* expr = cond;
      if ( expr->is_constant ) {
         if ( expr->value ) {
            parent->flow = body.flow;
            parent->jump_stmt = body.jump_stmt;
         }
         else {
            parent->flow = else_body.flow;
            parent->jump_stmt = else_body.jump_stmt;
         }
      }
      else if ( body.flow == else_body.flow ) {
         parent->flow = body.flow;
      }
      else if ( body.flow == k_flow_jump_stmt ||
         else_body.flow == k_flow_jump_stmt ) {
         parent->flow = k_flow_jump_stmt;
      }
   }

   p_pop_scope( front );
}

void read_switch( front_t* front ) {
   skip( front, tk_switch );

   skip( front, tk_paran_l );
   p_expr_t p_expr;
   p_expr_init( &p_expr, k_expr_comma );
   p_expr_read( front, &p_expr );
   skip( front, tk_paran_r );
   expr_t* cond = p_expr.expr;

   p_case_t case_stmt = { NULL };
   p_case_t* prev = front->case_stmt;
   front->case_stmt = &case_stmt;
   p_block_t body;
   p_block_init( &body, front->block, k_stmt_switch );
   p_block_t* parent = front->block;
   front->block = &body;
   p_new_scope( front );
   read_stmt( front );
   p_pop_scope( front );
   front->block = parent;
   front->case_stmt = prev;

   switch_t* node = mem_alloc( sizeof( *node ) );
   node->node.type = k_node_switch;
   node->cond = ( node_t* ) cond;
   node->head_case = case_stmt.head_case;
   node->head_case_sorted = case_stmt.head_case_sorted;
   node->default_case = case_stmt.default_case;
   node->body = body.stmt_list;
   node->case_count = case_stmt.count;
   node->jump_break = body.jump_break;
   list_append( &parent->stmt_list, node );
   if ( parent->flow == k_flow_dead ) {
      parent->flow = k_flow_dead;
   }
}

void read_case( front_t* front ) {
   position_t pos = front->token.pos;
   p_block_t* block = front->block;
   p_case_t* case_stmt = front->case_stmt;
   if ( ! ( block->flags & k_stmt_switch ) ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
         "case outside switch statement" );
   }
   else if ( ! ( block->flags & k_stmt_switch_direct ) ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
         "case in nested block" );
   }
   expr_t* expr = NULL;
   if ( tk( front ) == tk_case ) {
      drop( front );
      p_expr_t p_expr;
      p_expr_init( &p_expr, k_expr_comma );
      p_expr_read( front, &p_expr );
      expr = p_expr.expr;
   }
   else {
      skip( front, tk_default );
   }
   skip( front, tk_colon );
   if ( expr ) {
      // Case expression must be constant.
      if ( ! expr->is_constant ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &expr->pos,
            "case value not constant" );
         f_bail( front );
      }
      // No duplicate cases allowed.
      case_t* item = case_stmt->head_case;
      while ( item ) {
         if ( item->expr && item->expr->value == expr->value ) {
            f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
               "case with value %d duplicated", expr->value );
            f_diag( front, DIAG_NONE | DIAG_LINE | DIAG_COLUMN, &item->pos,
               "previous case with value found here" );
         }
         item = item->next;
      }
   }
   else {
      // There should only be a single default case.
      if ( case_stmt->default_case ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "default case duplicated" );
         f_diag( front, DIAG_NONE | DIAG_LINE | DIAG_COLUMN,
            &case_stmt->default_case->pos,
            "previous default case found here" );
         f_bail( front );
      }
   }
   case_t* item = mem_alloc( sizeof( *item ) );
   item->node.type = k_node_case;
   item->next = NULL;
   item->next_sorted = NULL;
   item->expr = expr;
   item->pos = pos;
   item->offset = 0;
   list_append( &block->stmt_list, item );
   ++case_stmt->count;
   if ( case_stmt->tail_case ) {
      case_stmt->tail_case->next = item;
      case_stmt->tail_case = item;
   }
   else {
      case_stmt->head_case = item;
      case_stmt->tail_case = item;
   }
   if ( expr ) {
      case_t* prev = NULL;
      case_t* curr = case_stmt->head_case_sorted;
      while ( curr && curr->expr->value < item->expr->value ) {
         prev = curr;
         curr = curr->next_sorted;
      }
      if ( prev ) {
         item->next_sorted = prev->next_sorted;
         prev->next_sorted = item;
      }
      else {
         item->next_sorted = case_stmt->head_case_sorted;
         case_stmt->head_case_sorted = item;
      }
   }
   else {
      case_stmt->default_case = item;
   }
   // TODO: Fix the flow for the switch statement.
   // Reset the flow for the next case.
   if ( block->flow != k_flow_jump_stmt ) {
      block->flow = k_flow_going;
   }
   // A case must be followed by at least one other statement.
   if ( tk( front ) != tk_brace_r ) {
      read_stmt( front );
   }
   else {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
         "case not followed with a statement" );
      f_bail( front );
   }
}

void read_while( front_t* front ) {
   bool is_do = false;
   int type = k_while_while;
   if ( tk( front ) == tk_while ) {
      drop( front );
   }
   else if ( tk( front ) == tk_until ) {
      type = k_while_until;
      drop( front );
   }
   else {
      skip( front, tk_do );
      is_do = true;
   }
   // Condition (while/until).
   expr_t* cond = NULL;
   if ( ! is_do ) {
      skip( front, tk_paran_l );
      p_expr_t p_expr;
      p_expr_init( &p_expr, k_expr_comma );
      p_expr_read( front, &p_expr );
      skip( front, tk_paran_r );
      cond = p_expr.expr;
   }
   p_block_t body;
   p_block_init( &body, front->block, k_stmt_loop );
   p_block_t* parent = front->block;
   front->block = &body;
   p_new_scope( front );
   read_stmt( front );
   p_pop_scope( front );
   front->block = parent;
   // Condition (do-while/do-until).
   if ( is_do ) {
      type = k_while_do_while;
      if ( tk( front ) == tk_until ) {
         type = k_while_do_until;
         drop( front );
      }
      else {
         skip( front, tk_while );
      }
      skip( front, tk_paran_l );
      p_expr_t p_expr;
      p_expr_init( &p_expr, k_expr_comma );
      p_expr_read( front, &p_expr );
      skip( front, tk_paran_r );
      skip( front, tk_semicolon );
      cond = p_expr.expr;
   }
   while_t* node = mem_alloc( sizeof( *node ) );
   node->node.type = k_node_while;
   node->type = type;
   node->cond = ( node_t* ) cond;
   node->body = body.stmt_list;
   node->jump_break = body.jump_break;
   node->jump_early = body.jump_continue;
   list_append( &parent->stmt_list, node );
   update_flow_with_loop( parent, cond, body.flow );
}

void update_flow_with_loop( p_block_t* block, expr_t* expr, int flow ) {
   if ( block->flow == k_flow_going ) {
      if ( ! expr || ( expr->is_constant && expr->value &&
         flow != k_flow_jump_stmt ) ) {
         block->flow = k_flow_dead;
      }
   }
}

void read_for( front_t* front ) {
   skip( front, tk_for );
   expr_t* init = NULL;
   expr_t* post = NULL;
   list_t decs;
   list_init( &decs );
   skip( front, tk_paran_l );
   p_new_scope( front );
   // Optional initialization.
   if ( tk( front ) != tk_semicolon ) {
      // Declaration.
      if ( p_is_dec( front ) ) {
         front->dec_for = &decs;
         p_read_dec( front, k_dec_for );
         front->dec_for = NULL;
      }
      // Expression.
      else {
         p_expr_t p_expr;
         p_expr_init( &p_expr, k_expr_comma );
         p_expr_read( front, &p_expr );
         init = p_expr.expr;
         skip( front, tk_semicolon );
      }
   }
   else {
      drop( front );
   }
   // Optional condition.
   node_t* cond = NULL;
   if ( tk( front ) != tk_semicolon ) {
      p_expr_t p_expr;
      p_expr_init( &p_expr, k_expr_comma );
      p_expr_read( front, &p_expr );
      skip( front, tk_semicolon );
      cond = ( node_t* ) p_expr.expr;
   }
   else {
      drop( front );
   }
   // Optional post-expression.
   if ( tk( front ) != tk_paran_r ) {
      p_expr_t p_expr;
      p_expr_init( &p_expr, k_expr_comma );
      p_expr_read( front, &p_expr );
      post = p_expr.expr;
   }
   skip( front, tk_paran_r );
   p_block_t body;
   p_block_init( &body, front->block, k_stmt_loop );
   p_block_t* parent = front->block;
   front->block = &body;
   read_stmt( front );
   front->block = parent;
   p_pop_scope( front );
   for_t* node = mem_alloc( sizeof( *node ) );
   node->node.type = k_node_for;
   node->init = decs;
   node->init_expr = init;
   node->cond = cond;
   node->post = post;
   node->body = body.stmt_list;
   node->jump_break = body.jump_break;
   node->jump_continue = body.jump_continue;
   list_append( &parent->stmt_list, node );
   update_flow_with_loop( parent, ( expr_t* ) node->cond, body.flow );
}

void skip_imported_block( front_t* front ) {
   // Skip to the opening brace.
   while ( true ) {
      if ( tk( front ) == tk_brace_l ) {
         drop( front );
         break;
      }
      // Error.
      else if ( tk( front ) == tk_end ) {
         //msg_unexpected_token( front->token );
         f_bail( front );
      }
      else {
         drop( front );
      }
   }
   int depth = 1;
   while ( depth ) {
      switch ( tk( front ) ) {
      case tk_brace_l:
         ++depth;
         break;
      case tk_brace_r:
         --depth;
         break;
      case tk_end:
         skip( front, tk_brace_r );
         break;
      default:
         break;
      }
      drop( front );
   }
}

void skip_imported_semicolon( front_t* front ) {
   bool done = false;
   while ( ! done ) {
      switch ( tk( front ) ) {
      case tk_semicolon:
         drop( front );
         done = true;
         break;
      case tk_end:
         skip( front, tk_semicolon );
         break;
      default:
         drop( front );
         break;
      }
   }
}

void add_sweep( front_t* front ) {
   sweep_t* sweep = mem_temp_alloc( sizeof( *sweep ) );
   sweep->next = NULL;
   sweep->prev = NULL;
   sweep->size = 0;
   if ( front->sweep ) {
      sweep->prev = front->sweep;
      front->sweep->next = sweep;
   }
   front->sweep = sweep;
}

void sweep_key( front_t* front, nkey_t* key ) {
   sweep_t* sweep = front->sweep;
   if ( sweep->size == DEFAULT_LOCAL_SIZE ) {
      if ( sweep->next ) {
         sweep = sweep->next;
         front->sweep = sweep;
      }
      else {
         add_sweep( front );
         sweep = front->sweep;
      }
   }
   sweep->names[ sweep->size ] = key;
   ++sweep->size;
}

void p_new_scope( front_t* front ) {
   if ( ! front->depth ) {
      front->scope_size = 0;
      front->scope_size_high = 0;
   }
   ++front->depth;
   sweep_key( front, NULL );
   p_scope_t* scope = mem_temp_alloc( sizeof( *scope ) );
   scope->prev = front->scope;
   scope->size = 0;
   scope->size_high = 0;
   if ( scope->prev ) {
      scope->size = scope->prev->size;
   }
   else {
      front->depth_size = 0;
   }
   front->scope = scope;
   list_prepend( &front->scopes, scope );
}

void p_pop_scope( front_t* front ) {
   sweep_t* sweep = front->sweep;
   while ( true ) {
      if ( ! sweep->size ) {
         sweep = sweep->prev;
         front->sweep = sweep;
      }
      nkey_t* key = sweep->names[ --sweep->size ];
      // Found scope division marker.
      if ( ! key ) { break; }
      nval_t* prev = key->value->prev;
      key->value->prev = front->nval;
      front->nval = key->value;
      key->value = prev;
   }
   p_scope_t* prev = front->scope->prev;
   if ( prev ) {
      if ( front->scope->size > prev->size_high ) {
         prev->size_high = front->scope->size;
      }
      if ( front->scope->size_high > prev->size_high ) {
         prev->size_high = front->scope->size_high;
      }
      front->scope_size = prev->size;
   }
   front->scope = prev;
   list_pop_head( &front->scopes );
   --front->depth;
}

void p_use_key( front_t* front, nkey_t* key, node_t* node, bool scoped ) {
   nval_t* value = front->nval;
   if ( value ) {
      front->nval = value->prev;
   }
   else {
      value = mem_alloc( sizeof( *value ) );
   }
   value->prev = key->value;
   value->depth = 0;
   value->node = node;
   key->value = value;
   if ( scoped && front->depth ) {
      value->depth = front->depth;
      sweep_key( front, key );
   }
}

void p_attach_undef( front_t* front, undef_t* undef ) {
   if ( front->undef_tail ) {
      undef->prev = front->undef_tail;
      front->undef_tail->next = undef;
      front->undef_tail = undef;
   }
   else {
      front->undef_head = undef;
      front->undef_tail = undef;
   }
}

void p_detach_undef( front_t* front, undef_t* undef ) {
   if ( undef->next ) {
      undef->next->prev = undef->prev;
   }
   else {
      front->undef_tail = undef->prev;
   }
   if ( undef->prev ) {
      undef->prev->next = undef->next;
   }
   else {
      front->undef_head = undef->next;
   }
}

int p_remove_undef( front_t* front, nkey_t* key ) {
   // Make sure the symbol is undefined.
   key = ntbl_goto( front->name_table,
      "!a",
      "!o", &key,
      "!s", "?", "!" );
   if ( ! key || ! key->value ) { return 0; }
   int count = 0;
   undef_t* undef = ( undef_t* ) key->value->node;
   undef_usage_t* usage = undef->usage;
   while ( usage ) {
      --usage->expr->num_undef;
      // The expression can be tested when all objects have been define.
      if ( usage->expr->num_undef == 0 ) {
         //p_test_expr( front, usage->expr->node );
         //mem_free( usage->expr );
      }
      undef_usage_t* next = usage->next;
      // mem_free( usage );
      usage = next;
      ++count;
   }
   p_detach_undef( front, undef );
   //mem_free( undef );
   //key->value = NULL;
   return count;
}

// Allocation order:
// - public arrays
// - public scalars
// - hidden scalars
// - hidden arrays
// - imported scalars
// - imported arrays
void alloc_offset( ast_t* ast ) {
   int offset = 0;
   list_iterator_t i = list_iterate( &ast->vars );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      var->index = offset;
      offset += var->type->size;
      list_next( &i );
   }
   i = list_iterate( &ast->arrays );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      var->index = offset;
      offset += var->dim->size * var->dim->element_size;
      list_next( &i );
   }

/*
   // Custom types are arrays internally.
   i = list_iterate( &module->customs );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      var->index = index;
      ++index;
      list_next( &i );
   }*/
/*
   // Functions.
   i_imp = list_iterate( &module->imports );
   done = false;
   index = 0;
   while ( ! done ) {
      list_t* funcs = NULL;
      if ( ! list_end( &i_imp ) ) {
         module_t* imported = list_idata( &i_imp );
         funcs = &imported->funcs;
         list_next( &i_imp );
      }
      else {
         funcs = &module->funcs;
         done = true;
      }
      list_iterator_t i = list_iterate( funcs );
      while ( ! list_end( &i ) ) {
         ufunc_t* ufunc = list_idata( &i );
         ufunc->index = index;
         ++index;
         list_next( &i );
      }
   }*/
}

void block_init( p_block_t* block ) {
   block->parent = NULL;
   list_init( &block->stmt_list );
   block->flow = k_flow_going;
   block->flags = 0;
   block->in_func = false;
   block->is_func_return = false;
   block->is_break = false;
   block->is_continue = false;
   block->jump_break = NULL;
   block->jump_continue = NULL;
}

void p_block_init( p_block_t* block, p_block_t* parent, int flags ) {
   block_init( block );
   if ( parent ) {
      block->parent = parent;
      block->flags = parent->flags | flags;
      block->in_func = parent->in_func;
      block->is_func_return = parent->is_func_return;
   }
   if ( flags == k_stmt_switch ) {
      block->flags |= k_stmt_switch_direct;
   }
   else {
      block->flags ^= k_stmt_switch_direct;
   }
   if ( flags == k_stmt_switch || flags == k_stmt_loop ) {
      block->is_break = true;
   }
   block->is_continue = ( flags & k_stmt_loop );
}

void p_block_init_ufunc( p_block_t* block, bool has_return ) {
   block_init( block );
   block->flags = k_stmt_function;
   block->in_func = true;
   block->is_func_return = has_return;
}

void p_block_init_script( p_block_t* block ) {
   block_init( block );
   block->flags = k_stmt_script;
}

void p_fcast_init( front_t* front, p_fcast_t* fcast, token_t* token ) {
   bool unknown = false;
   switch ( token->source[ 0 ] ) {
   case 'a': fcast->cast = k_fcast_array; break;
   case 'b': fcast->cast = k_fcast_binary; break;
   case 'c': fcast->cast = k_fcast_char; break;
   case 'd': // Same as 'i'
   case 'i': fcast->cast = k_fcast_decimal; break;
   case 'f': fcast->cast = k_fcast_fixed; break;
   case 'k': fcast->cast = k_fcast_key; break;
   case 'l': fcast->cast = k_fcast_local_string; break;
   case 'n': fcast->cast = k_fcast_name; break;
   case 's': fcast->cast = k_fcast_string; break;
   case 'x': fcast->cast = k_fcast_hex; break;
   default: unknown = true; break;
   }
   if ( unknown || token->length > 1 ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &token->pos,
         "unknown format cast: %s", token->source );
      f_bail( front );
   }
}

void read_paltrans( front_t* front ) {
   skip( front, tk_paltrans );
   skip( front, tk_paran_l );
   p_expr_t number;
   p_expr_init( &number, k_expr_cond );
   p_expr_read( front, &number );
   paltrans_t* trans = mem_alloc( sizeof( *trans ) );
   trans->node.type = k_node_paltrans;
   trans->number = number.expr;
   trans->ranges = NULL;
   trans->ranges_tail = NULL;
   while ( tk( front ) == tk_comma ) {
      drop( front );
      p_expr_t begin;
      p_expr_init( &begin, k_expr_cond );
      p_expr_read( front, &begin );
      skip( front, tk_colon );
      p_expr_t end;
      p_expr_init( &end, k_expr_cond );
      p_expr_read( front, &end );
      skip( front, tk_assign );
      palrange_t* range = mem_alloc( sizeof( *range ) );
      range->next = NULL;
      range->begin = begin.expr;
      range->end = end.expr;
      if ( tk( front ) == tk_bracket_l ) {
         p_expr_t red, green, blue;
         read_palrange_rgb_field( front, &red, &green, &blue );
         range->value.rgb.red1 = red.expr;
         range->value.rgb.green1 = green.expr;
         range->value.rgb.blue1 = blue.expr;
         skip( front, tk_colon );
         read_palrange_rgb_field( front, &red, &green, &blue );
         range->value.rgb.red2 = red.expr;
         range->value.rgb.green2 = green.expr;
         range->value.rgb.blue2 = blue.expr;
         range->rgb = true;
      }
      else {
         p_expr_t target_begin;
         p_expr_init( &target_begin, k_expr_cond );
         p_expr_read( front, &target_begin );
         skip( front, tk_colon );
         p_expr_t target_end;
         p_expr_init( &target_end, k_expr_cond );
         p_expr_read( front, &target_end );
         range->value.ent.begin = target_begin.expr;
         range->value.ent.end = target_end.expr;
         range->rgb = false;
      }
      if ( trans->ranges_tail ) {
         trans->ranges_tail->next = range;
      }
      else {
         trans->ranges = range;
      }
      trans->ranges_tail = range;
   }
   skip( front, tk_paran_r );
   skip( front, tk_semicolon );
   list_append( &front->block->stmt_list, trans );
}

void read_palrange_rgb_field( front_t* front, p_expr_t* red, p_expr_t* green,
   p_expr_t* blue ) {
   skip( front, tk_bracket_l );
   p_expr_init( red, k_expr_cond );
   p_expr_read( front, red );
   skip( front, tk_comma );
   p_expr_init( green, k_expr_cond );
   p_expr_read( front, green );
   skip( front, tk_comma );
   p_expr_init( blue, k_expr_cond );
   p_expr_read( front, blue );
   skip( front, tk_bracket_r );
}