#include <string.h>

#include "phase.h"

struct test {
   struct stmt_test* stmt_test;
   struct inline_asm* inline_asm;
   const char* format;
   const char* repeat;
};

static void test_name( struct semantic* semantic, struct test* test );
static struct mnemonic* find_mnemonic( struct task* task, const char* name );
static void test_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg );
static void test_label_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg );
static void test_var_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg );
static void test_func_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg );
static void test_expr_arg( struct semantic* semantic,
   struct inline_asm_arg* arg );

void p_test_inline_asm( struct semantic* semantic,
   struct stmt_test* stmt_test, struct inline_asm* inline_asm ) {
   struct test test = { .stmt_test = stmt_test, .inline_asm = inline_asm };
   test_name( semantic, &test );
   list_iter_t i;
   list_iter_init( &i, &inline_asm->args );
   while ( ! list_end( &i ) ) {
      test_arg( semantic, &test, list_data( &i ) );
      list_next( &i );
   }
   if ( *test.format && ! test.repeat ) {
      s_diag( semantic, DIAG_POS_ERR, &inline_asm->pos,
         "too little arguments for `%s` instruction",
         inline_asm->name );
      s_bail( semantic );
   }
}

void test_name( struct semantic* semantic, struct test* test ) {
   struct mnemonic* mnemonic = find_mnemonic( semantic->task,
      test->inline_asm->name );
   if ( ! mnemonic ) {
      s_diag( semantic, DIAG_POS_ERR, &test->inline_asm->pos,
         "`%s` instruction not found", test->inline_asm->name );
      s_bail( semantic );
   }
   test->format = mnemonic->args;
   test->inline_asm->opcode = mnemonic->opcode;
}

struct mnemonic* find_mnemonic( struct task* task, const char* name ) {
   struct mnemonic* mnemonic = task->mnemonics;
   while ( mnemonic && strcmp( mnemonic->name, name ) < 0 ) {
      mnemonic = mnemonic->next;
   }
   if ( strcmp( mnemonic->name, name ) == 0 ) {
      return mnemonic;
   }
   else {
      return NULL;
   }
}

void test_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg ) {
   if ( ! *test->format ) {
      s_diag( semantic, DIAG_POS_ERR, &test->inline_asm->pos,
         "too many arguments for `%s` instruction",
         test->inline_asm->name );
      s_bail( semantic );
   }
   // A `+` can appear at the start of a format substring. It states that the
   // current format substring should be reused for all subsequent arguments.
   if ( *test->format == '+' ) {
      test->repeat = test->format;
      ++test->format;
   }
   // Determine argument type. The syntax of the argument is used to identify
   // its type. Each type has a single representation syntax-wise.
   while ( true ) {
      bool match = false;
      bool unknown = false;
      switch ( *test->format ) {
      case 'n': match = ( arg->type == INLINE_ASM_ARG_NUMBER ); break;
      case 'l': match = ( arg->type == INLINE_ASM_ARG_ID ); break;
      case 'v': match = ( arg->type == INLINE_ASM_ARG_ID ); break;
      case 'a': match = ( arg->type == INLINE_ASM_ARG_ID ); break;
      case 'f': match = ( arg->type == INLINE_ASM_ARG_ID ); break;
      case 'e': match = ( arg->type == INLINE_ASM_ARG_EXPR ); break;
      default:
         unknown = true;
      }
      if ( match || unknown ) {
         break;
      }
      else {
         ++test->format;
      }
   }
   // Process the argument.
   switch ( *test->format ) {
   case 'l':
      test_label_arg( semantic, test, arg );
      break;
   case 'v':
   case 'a':
      test_var_arg( semantic, test, arg );
      break;
   case 'f':
      test_func_arg( semantic, test, arg );
      break;
   case 'e':
      test_expr_arg( semantic, arg );
      break;
   case 'n':
      break;
   default:
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "invalid argument" );
      s_bail( semantic );
   }
   // Find format string of next argument.
   if ( test->repeat ) {
      test->format = test->repeat;
   }
   else {
      while ( *test->format && *test->format != ',' ) {
         ++test->format;
      }
   }
}

void test_label_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg ) {
   list_iter_t i;
   list_iter_init( &i, semantic->topfunc_test->labels );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      if ( strcmp( label->name, arg->value.id ) == 0 ) {
         arg->value.label = label;
         arg->type = INLINE_ASM_ARG_LABEL;
         return;
      }
      list_next( &i );
   }
   s_diag( semantic, DIAG_POS_ERR, &arg->pos,
      "label `%s` not found", arg->value.id );
   s_bail( semantic );
}

void test_var_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg ) {
   struct object* object = s_search_object( semantic, arg->value.id );
   if ( ! object ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "`%s` not found", arg->value.id );
      s_bail( semantic );
   }
   if ( object->node.type != NODE_VAR && object->node.type != NODE_PARAM ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "instruction argument not a variable" );
      s_bail( semantic );
   }
   struct var* var = NULL;
   struct param* param = NULL;
   struct structure* structure = NULL;
   struct dim* dim = NULL;
   int storage = STORAGE_LOCAL;
   if ( object->node.type == NODE_PARAM ) {
      param = ( struct param* ) object;
   }
   else {
      var = ( struct var* ) object;
      structure = var->structure;
      dim = var->dim;
      storage = var->storage;
   }
   // Check for array.
   if ( *test->format == 'a' ) {
      if ( ! dim && ! structure ) {
         s_diag( semantic, DIAG_POS_ERR, &arg->pos,
            "instruction argument not an array" );
         s_bail( semantic );
      }
   }
   else {
      if ( dim || structure ) {
         s_diag( semantic, DIAG_POS_ERR, &arg->pos,
            "instruction argument not a scalar" );
         s_bail( semantic );
      }
   }
   // Check storage.
   bool storage_correct = (
      ( test->format[ 1 ] == 'l' && storage == STORAGE_LOCAL ) ||
      ( test->format[ 1 ] == 'm' && storage == STORAGE_MAP ) ||
      ( test->format[ 1 ] == 'w' && storage == STORAGE_WORLD ) ||
      ( test->format[ 1 ] == 'g' && storage == STORAGE_GLOBAL ) );
   if ( ! storage_correct ) {
      const char* name = "";
      switch ( test->format[ 1 ] ) {
      case 'l': name = "local"; break;
      case 'm': name = "map"; break;
      case 'w': name = "world"; break;
      case 'g': name = "global"; break;
      default:
         UNREACHABLE();
      }
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "instruction argument not a %s variable", name );
      s_bail( semantic );
   }
   if ( param ) {
      arg->type = INLINE_ASM_ARG_PARAM;
      arg->value.param = param;
   }
   else {
      arg->type = INLINE_ASM_ARG_VAR;
      arg->value.var = var;
   }
}

void test_func_arg( struct semantic* semantic, struct test* test,
   struct inline_asm_arg* arg ) {
   struct object* object = s_search_object( semantic, arg->value.id );
   if ( ! object ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "`%s` not found", arg->value.id );
      s_bail( semantic );
   }
   if ( object->node.type != NODE_FUNC ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "instruction argument not a function" );
      s_bail( semantic );
   }
   struct func* func = ( struct func* ) object;
   if ( test->format[ 1 ] == 'e' ) {
      if ( func->type != FUNC_EXT ) {
         s_diag( semantic, DIAG_POS_ERR, &arg->pos,
            "instruction argument not an extension function" );
         s_bail( semantic );
      }
   }
   else {
      if ( func->type != FUNC_USER ) {
         s_diag( semantic, DIAG_POS_ERR, &arg->pos,
            "instruction argument not a user-created function" );
         s_bail( semantic );
      }
   }
   arg->type = INLINE_ASM_ARG_FUNC;
   arg->value.func = func;
}

void test_expr_arg( struct semantic* semantic, struct inline_asm_arg* arg ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, arg->value.expr );
   if ( ! arg->value.expr->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &arg->pos,
         "instruction argument not constant" );
      s_bail( semantic );
   }
}