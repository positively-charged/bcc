#include "phase.h"
#include "cache/cache.h"

void p_init( struct parse* parse, struct task* task, struct cache* cache ) {
   parse->task = task;
   // NOTE: parse->queue not initialized.
   parse->token = NULL;
   parse->source = NULL;
   parse->free_source = NULL;
   parse->last_id = 0;
   str_init( &parse->temp_text );
   list_init( &parse->text_buffers );
   parse->read_flags = READF_CONCATSTRINGS | READF_ESCAPESEQ;
   parse->line_beginning = true;
   parse->concat_strings = false;
   parse->macro_head = NULL;
   parse->macro_free = NULL;
   parse->macro_param_free = NULL;
   parse->macro_expan = NULL;
   parse->macro_expan_free = NULL;
   parse->ifdirc_top = NULL;

   parse->line = 0;
   parse->column = 0;
   parse->predef_macro_expan = PREDEFMACROEXPAN_NONE;
   parse->prep_context = PREPCONTEXT_NONE;
   parse->cache = cache;
   parse->create_nltk = false;
   p_init_stream( parse );
   parse->ns = NULL;
   parse->local_vars = NULL;
   parse->lib = NULL;
   parse->main_lib_lines = 0;
   parse->included_lines = 0;
}

void p_run( struct parse* parse ) {
   struct library* lib = t_add_library( parse->task );
   parse->task->library_main = lib;
   parse->lib = lib;
   parse->ns = lib->upmost_ns;
   p_define_cmdline_macros( parse );
   p_load_main_source( parse );
   if ( parse->task->options->preprocess ) {
      p_preprocess( parse );
   }
   else {
      p_read_tk( parse );
      p_read_target_lib( parse );
   }

/*
   while ( parse->tk != TK_END ) {
      printf( "a %s %d\n", parse->tk_text, parse->tk_length );
      p_read_tk( parse );
   }
   p_bail( parse );
*/
/*
   struct tkque_iter iter;
   p_examine_token_queue( parse, &iter );
   p_next_tk( parse, &iter );
   p_next_tk( parse, &iter );
   p_next_tk( parse, &iter );
   p_next_tk( parse, &iter );
   p_next_tk( parse, &iter );
   p_next_tk( parse, &iter );
   p_next_tk( parse, &iter );
   printf( "%s\n", parse->token->text );
   printf( "%s == %d\n", iter.token->text, iter.token->type );
   while ( parse->tk != TK_END ) {
      printf( "a %s %d\n", parse->tk_text, parse->tk_length );
      p_read_tk( parse );
   }
   p_bail( parse );
return;
*/
   //printf( "%s\n", iter.token->text );
   //p_next_tk( parse, &iter );

   // alloc_string_indexes( parse );
}

void p_diag( struct parse* parse, int flags, ... ) {
   va_list args;
   va_start( args, flags );
   p_macro_trace_diag( parse );
   t_diag_args( parse->task, flags, &args );
   va_end( args );
}

void p_unexpect_diag( struct parse* parse ) {
   p_diag( parse, DIAG_POS_ERR | DIAG_SYNTAX, &parse->token->pos,
      "unexpected %s", p_get_token_name( parse->token->type ) );
}

void p_unexpect_item( struct parse* parse, struct pos* pos, enum tk tk ) {
   p_unexpect_name( parse, pos, p_get_token_name( tk ) );
}

void p_unexpect_name( struct parse* parse, struct pos* pos,
   const char* subject ) {
   p_diag( parse, DIAG_POS, ( pos ? pos : &parse->token->pos ),
      "expecting %s here, or", subject );
}

void p_unexpect_last( struct parse* parse, struct pos* pos, enum tk tk ) {
   p_unexpect_last_name( parse, pos, p_get_token_name( tk ) );
}

void p_unexpect_last_name( struct parse* parse, struct pos* pos,
   const char* subject ) {
   p_diag( parse, DIAG_POS, ( pos ? pos : &parse->token->pos ),
      "expecting %s here", subject );
}

void p_bail( struct parse* parse ) {
   p_deinit_tk( parse );
   t_bail( parse->task );
}