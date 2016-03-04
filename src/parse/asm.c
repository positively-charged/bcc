#include <string.h>

#include "phase.h"

static struct mnemonic* alloc_mnemonic( void );
static void append_mnemonic( struct parse* parse, struct task* task,
   struct mnemonic* mnemonic );

void p_read_mnemonic( struct parse* parse ) {
   struct mnemonic* mnemonic = alloc_mnemonic();
   p_test_tk( parse, TK_ID );
   mnemonic->name = parse->tk_text;
   mnemonic->pos = parse->tk_pos;
   p_read_tk( parse );
   p_test_tk( parse, TK_LIT_DECIMAL );
   mnemonic->opcode = p_extract_literal_value( parse );
   p_read_tk( parse );
   p_test_tk( parse, TK_LIT_STRING );
   mnemonic->args = parse->tk_text;
   p_read_tk( parse );
   append_mnemonic( parse, parse->task, mnemonic );
}

struct mnemonic* alloc_mnemonic( void ) {
   struct mnemonic* mnemonic = mem_alloc( sizeof( *mnemonic ) );
   mnemonic->name = NULL;
   mnemonic->args = NULL;
   mnemonic->next = NULL;
   mnemonic->opcode = 0;
   return mnemonic;
}

void append_mnemonic( struct parse* parse, struct task* task,
   struct mnemonic* mnemonic ) {
   struct mnemonic* prev = NULL;
   struct mnemonic* curr = task->mnemonics;
   while ( curr && strcmp( curr->name, mnemonic->name ) <= 0 ) {
      prev = curr;
      curr = curr->next;
   }
   if ( prev ) {
      if ( strcmp( prev->name, mnemonic->name ) == 0 ) {
         p_diag( parse, DIAG_POS_ERR, &mnemonic->pos,
            "duplicate mnemonic" );
         p_diag( parse, DIAG_POS_ERR, &prev->pos,
            "mnemonic previously defined here" );
         p_bail( parse );
      }
      mnemonic->next = prev->next;
      prev->next = mnemonic;
   }
   else {
      mnemonic->next = task->mnemonics;
      task->mnemonics = mnemonic;
   }
}