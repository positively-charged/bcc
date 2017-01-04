#include <string.h>
#include <ctype.h>

#include "phase.h"

static void read_peeked_token( struct parse* parse );
static void read_token( struct parse* parse );
static void read_token_bcs( struct parse* parse );
static void read_token_acs( struct parse* parse );
static struct token* push_token( struct parse* parse );

// Functions used by the parser.
// ==========================================================================

// Needs to:
//  - Skip whitespace, both horizontal space and newlines.
//  - Expand macros.
//  - Execute directives.
void p_read_tk( struct parse* parse ) {
   read_peeked_token( parse );
   struct token* token = parse->token;
   parse->tk = token->type;
   parse->tk_text = token->text;
   parse->tk_pos = token->pos;
   parse->tk_length = token->length;
}

void read_peeked_token( struct parse* parse ) {
   if ( parse->parser_tkque.size > 0 ) {
      parse->token = p_shift_entry( parse, &parse->parser_tkque );
   }
   else {
      read_token( parse );
   }
}

void read_token( struct parse* parse ) {
   if ( parse->lang == LANG_BCS ) {
      read_token_bcs( parse );
   }
   else {
      read_token_acs( parse );
   }
}

void read_token_bcs( struct parse* parse ) {
   top:
   p_read_stream( parse );
   switch ( parse->token->type ) {
   case TK_ID:
      if ( p_expand_macro( parse ) ) {
         goto top;
      }
      else {
         goto identifier;
      }
      break;
   case TK_HASH:
      if ( parse->source_entry->line_beginning ) {
         if ( p_read_dirc( parse ) ) {
            goto top;
         }
      }
      break;
   case TK_NL:
      if ( ! parse->create_nltk ) {
         goto top;
      }
      break;
   case TK_HORZSPACE:
      goto top;
   case TK_LIT_STRING:
      goto string;
   case TK_PROCESSEDHASH:
      parse->token->type = TK_HASH;
      break;
   default:
      break;
   }
   return;

   identifier:
   // -----------------------------------------------------------------------
   {
      char* text = parse->token->modifiable_text;
      if ( ( parse->token->length >= 2 &&
         ( islower( parse->token->text[ parse->token->length - 2 ] ) ||
            parse->token->text[ parse->token->length - 2 ] == '_' ) &&
         text[ parse->token->length - 1 ] == 'T' ) ||
         ( parse->token->length == 1 && parse->token->text[ 0 ] == 'T' ) ) {
         parse->token->type = TK_TYPENAME;
      }
      while ( *text ) {
         *text = tolower( *text );
         ++text;
      }
      // Type name.
      if ( parse->token->type == TK_TYPENAME ) {
         return;
      }
      // Reserved identifier.
      static const struct {
         const char* name;
         enum tk tk;
      } table[] = {
         { "assert", TK_ASSERT },
         { "auto", TK_AUTO },
         { "bool", TK_BOOL },
         { "break", TK_BREAK },
         { "case", TK_CASE },
         { "const", TK_CONST },
         { "continue", TK_CONTINUE },
         { "createtranslation", TK_PALTRANS },
         { "default", TK_DEFAULT },
         { "do", TK_DO },
         { "else", TK_ELSE },
         { "enum", TK_ENUM },
         { "extern", TK_EXTERN },
         { "false", TK_FALSE },
         { "fixed", TK_FIXED },
         { "for", TK_FOR },
         { "foreach", TK_FOREACH },
         { "function", TK_FUNCTION },
         { "global", TK_GLOBAL },
         { "goto", TK_GOTO },
         { "if", TK_IF },
         { "int", TK_INT },
         { "let", TK_LET },
         { "memcpy", TK_MEMCPY },
         { "msgbuild", TK_MSGBUILD },
         { "namespace", TK_NAMESPACE },
         { "null", TK_NULL },
         { "private", TK_PRIVATE },
         { "raw", TK_RAW },
         { "restart", TK_RESTART },
         { "return", TK_RETURN },
         { "script", TK_SCRIPT },
         { "special", TK_SPECIAL },
         { "static", TK_STATIC },
         { "str", TK_STR },
         { "strcpy", TK_STRCPY },
         { "strict", TK_STRICT },
         { "struct", TK_STRUCT },
         { "suspend", TK_SUSPEND },
         { "switch", TK_SWITCH },
         { "terminate", TK_TERMINATE },
         { "true", TK_TRUE },
         { "typedef", TK_TYPEDEF },
         { "until", TK_UNTIL },
         { "upmost", TK_UPMOST },
         { "using", TK_USING },
         { "void", TK_VOID },
         { "while", TK_WHILE },
         { "world", TK_WORLD },
         // Terminator.
         { "\x7F", TK_END }
      };
      int left = 0;
      int right = ARRAY_SIZE( table ) - 1;
      while ( left <= right ) {
         int middle = ( left + right ) / 2;
         int result = strcmp( parse->token->text, table[ middle ].name );
         if ( result > 0 ) {
            left = middle + 1;
         }
         else if ( result < 0 ) {
            right = middle - 1;
         }
         else {
            parse->token->type = table[ middle ].tk;
            return;
         }
      }
   }
   return;

   // Concatenate adjacent strings.
   string:
   // -----------------------------------------------------------------------
   {
      struct streamtk_iter iter;
      p_init_streamtk_iter( parse, &iter );
      p_next_stream( parse, &iter );
      while (
         iter.token->type == TK_HORZSPACE ||
         iter.token->type == TK_NL ) {
         p_next_stream( parse, &iter );
      }
      if ( iter.token->type == TK_LIT_STRING ) {
         struct str text;
         str_init( &text );
         str_append( &text, parse->token->text );
         struct pos pos = parse->token->pos;
         while ( true ) {
            p_init_streamtk_iter( parse, &iter );
            p_next_stream( parse, &iter );
            if ( 
               iter.token->type == TK_HORZSPACE ||
               iter.token->type == TK_NL ) {
               p_read_stream( parse );
            }
            else if ( iter.token->type == TK_LIT_STRING ) {
               p_read_stream( parse );
               str_append( &text, parse->token->text );
            }
            else {
               break;
            }
         }
         parse->token->pos = pos;
         parse->token->modifiable_text = t_intern_text( parse->task,
            text.value, text.length );
         parse->token->type = TK_LIT_STRING;
         parse->token->text = parse->token->modifiable_text;
         parse->token->length = text.length;
         str_deinit( &text );
      }
   }
}

void read_token_acs( struct parse* parse ) {
   top:
   p_read_stream( parse );
   switch ( parse->token->type ) {
   case TK_NL:
   case TK_HORZSPACE:
      goto top;
   default:
      break;
   }
}

enum tk p_peek( struct parse* parse ) {
   return p_peek_tk( parse )->type;
}

struct token* p_peek_tk( struct parse* parse ) {
   struct parsertk_iter iter;
   p_init_parsertk_iter( parse, &iter );
   p_next_tk( parse, &iter );
   return iter.token;
}

enum tk p_peek_2nd( struct parse* parse ) {
   struct parsertk_iter iter;
   p_init_parsertk_iter( parse, &iter );
   p_next_tk( parse, &iter );
   p_next_tk( parse, &iter );
   return iter.token->type;
}

void p_init_parsertk_iter( struct parse* parse, struct parsertk_iter* iter ) {
   iter->entry = parse->parser_tkque.head;
   iter->token = NULL;
}

void p_next_tk( struct parse* parse, struct parsertk_iter* iter ) {
   if ( iter->entry ) {
      struct queue_entry* entry = iter->entry;
      iter->token = entry->token;
      iter->entry = entry->next;
   }
   else {
      iter->token = push_token( parse );
   }
}

struct token* push_token( struct parse* parse ) {
   struct queue_entry* entry = p_push_entry( parse, &parse->parser_tkque );
   entry->token = p_alloc_token( parse );
   entry->token_allocated = true;
   read_token( parse );
   entry->token[ 0 ] = parse->token[ 0 ];
   return entry->token;
}

/*
void read_stream( struct parse* parse ) {
   // Sometimes, the stream allocates a token dynamically. When a new token is
   // read, the previous token is no longer needed, so deallocate it.
   if ( parse->token->stream_alloced ) {
      free_token( parse, parse->token );
   }
   p_read_stream( parse );
   parse->token = parse->stream_token;
}
*/

void p_test_tk( struct parse* parse, enum tk expected ) {
   if ( parse->tk != expected ) {
      if ( parse->tk == TK_RESERVED ) {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "`%s` is a reserved identifier that is not currently used",
            parse->tk_text );
      }
      else {
         p_diag( parse, DIAG_POS_ERR | DIAG_SYNTAX, &parse->tk_pos, 
            "unexpected %s", p_present_token_temp( parse, parse->tk ) );
         p_diag( parse, DIAG_POS, &parse->tk_pos,
            "expecting %s here", p_present_token_temp( parse, expected ) );
      }
      p_bail( parse );
   }
}

void p_skip_block( struct parse* parse ) {
   while ( parse->tk != TK_END && parse->tk != TK_BRACE_L ) {
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_BRACE_L );
   p_read_tk( parse );
   int depth = 0;
   while ( true ) {
      if ( parse->tk == TK_BRACE_L ) {
         ++depth;
         p_read_tk( parse );
      }
      else if ( parse->tk == TK_BRACE_R ) {
         if ( depth ) {
            --depth;
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
      else if ( parse->tk == TK_LIB_END ) {
         break;
      }
      else if ( parse->tk == TK_END ) {
         break;
      }
      else {
         p_read_tk( parse );
      }
   }
   p_test_tk( parse, TK_BRACE_R );
   p_read_tk( parse );
}

void p_skip_semicolon( struct parse* parse ) {
   while (
      parse->tk != TK_END &&
      parse->tk != TK_SEMICOLON ) {
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
}

// Functions used by the preprocessor.
// ==========================================================================

void p_read_preptk( struct parse* parse ) {
   top:
   p_read_stream( parse );
   if ( parse->token->type == TK_HORZSPACE ) {
      goto top;
   }
}

void p_read_expanpreptk( struct parse* parse ) {
   top:
   p_read_stream( parse );
   switch ( parse->token->type ) {
   case TK_ID:
      if ( p_expand_macro( parse ) ) {
         goto top;
      }
      break;
   case TK_HORZSPACE:
      goto top;
   default:
      break;
   }
}

void p_read_eoptiontk( struct parse* parse ) {
   top:
   p_read_stream( parse );
   switch ( parse->token->type ) {
   case TK_ID:
      if ( p_expand_macro( parse ) ) {
         goto top;
      }
      break;
   case TK_HASH:
      if ( parse->source_entry->line_beginning ) {
         p_read_dirc( parse );
      }
      break;
   default:
      break;
   }
}

void p_test_preptk( struct parse* parse, enum tk expected ) {
   if ( parse->token->type != expected ) {
      p_diag( parse, DIAG_POS_ERR | DIAG_SYNTAX, &parse->token->pos, 
         "unexpected %s", p_present_token_temp( parse, parse->token->type ) );
      p_diag( parse, DIAG_POS, &parse->token->pos,
         "expecting %s here", p_present_token_temp( parse, expected ) );
      p_bail( parse );
   }
}

// ==========================================================================

/*
   // Concatenate adjacent strings.
   if ( parse->token->type == TK_LIT_STRING ) {
      struct str string;
      str_init( &string );
      str_append( &string, parse->token->text );
      while ( p_peek_tk( parse )->type == TK_LIT_STRING ) {
         readtk_peeked( parse );
         str_append( &string, parse->token->text );
printf( "t %d\n", parse->token->type );
         
      }
      if ( string.length ) {
   printf( "string: %s\n", string.value );
      }
   }*/

/*
   // -----------------------------------------------------------------------
   done:
   // Certain identifiers can appear only in a specific context.
   if ( parse->token->is_id &&
      strcmp( parse->token->text, "__va_args__" ) == 0 &&
      ! parse->prep_context != PREPCONTEXT_DEFINEBODY ) {
      p_diag( parse, DIAG_POS_ERR, &parse->token->pos,
         "`%s` outside function-like macro expansion",
         parse->token->text );
      p_bail( parse );
   }
*/