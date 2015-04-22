#include "phase.h"

static void test_block_item( struct semantic* phase, struct stmt_test*, struct node* );
static void test_case( struct semantic* phase, struct stmt_test*, struct case_label* );
static void test_default_case( struct semantic* phase, struct stmt_test*,
   struct case_label* );
static void test_label( struct semantic* phase, struct stmt_test*, struct label* );
static void test_if( struct semantic* phase, struct stmt_test*, struct if_stmt* );
static void test_switch( struct semantic* phase, struct stmt_test*,
   struct switch_stmt* );
static void test_while( struct semantic* phase, struct stmt_test*, struct while_stmt* );
static void test_for( struct semantic* phase, struct stmt_test* test,
   struct for_stmt* );
static void test_jump( struct semantic* phase, struct stmt_test*, struct jump* );
static void test_break( struct semantic* phase, struct stmt_test* test,
   struct jump* stmt );
static void test_continue( struct semantic* phase, struct stmt_test* test,
   struct jump* stmt );
static void test_script_jump( struct semantic* phase, struct stmt_test*,
   struct script_jump* );
static void test_return( struct semantic* phase, struct stmt_test*,
   struct return_stmt* );
static void test_goto( struct semantic* phase, struct stmt_test*, struct goto_stmt* );
static void test_paltrans( struct semantic* phase, struct stmt_test*, struct paltrans* );
static void test_paltrans_arg( struct semantic* phase, struct expr* expr );
static void test_format_item( struct semantic* phase, struct stmt_test*,
   struct format_item* );
static void test_packed_expr( struct semantic* phase, struct stmt_test*,
   struct packed_expr*, struct pos* );
static void test_goto_in_format_block( struct semantic* phase, struct list* );
static void perform_import( struct semantic* semantic, struct import* import );
static struct object* follow_import_path( struct semantic* semantic,
   struct path* path );
static void import_all( struct semantic* semantic, struct pos* import_pos,
   struct region* region );
static void import_selected( struct semantic* semantic,
   struct import* import, struct region* region );
static void import_item( struct semantic* semantic, struct import_item* item,
   struct region* region );
static void alias_imported( struct semantic* semantic, char* name,
   struct pos* pos, struct object* object );

void s_init_stmt_test( struct stmt_test* test, struct stmt_test* parent ) {
   test->parent = parent;
   test->func = NULL;
   test->labels = NULL;
   test->format_block = NULL;
   test->case_head = NULL;
   test->case_default = NULL;
   test->jump_break = NULL;
   test->jump_continue = NULL;
   test->nested_funcs = NULL;
   test->returns = NULL;
   test->in_loop = false;
   test->in_switch = false;
   test->in_script = false;
   test->manual_scope = false;
}

void s_test_block( struct semantic* phase, struct stmt_test* test,
   struct block* block ) {
   if ( ! test->manual_scope ) {
      s_add_scope( phase );
   }
   list_iter_t i;
   list_iter_init( &i, &block->stmts );
   while ( ! list_end( &i ) ) {
      struct stmt_test nested;
      s_init_stmt_test( &nested, test );
      test_block_item( phase, &nested, list_data( &i ) );
      list_next( &i );
   }
   if ( ! test->manual_scope ) {
      s_pop_scope( phase );
   }
   if ( ! test->parent ) {
      test_goto_in_format_block( phase, test->labels );
   }
}

void test_block_item( struct semantic* phase, struct stmt_test* test,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_CONSTANT:
      s_test_constant( phase, ( struct constant* ) node );
      break;
   case NODE_CONSTANT_SET:
      s_test_constant_set( phase, ( struct constant_set* ) node );
      break;
   case NODE_VAR:
      s_test_local_var( phase, ( struct var* ) node );
      break;
   case NODE_TYPE:
      s_test_struct( phase, ( struct type* ) node );
      break;
   case NODE_FUNC:
      s_test_func( phase, ( struct func* ) node );
      break;
   case NODE_CASE:
      test_case( phase, test, ( struct case_label* ) node );
      break;
   case NODE_CASE_DEFAULT:
      test_default_case( phase, test, ( struct case_label* ) node );
      break;
   case NODE_GOTO_LABEL:
      test_label( phase, test, ( struct label* ) node );
      break;
   case NODE_IMPORT: {
      struct import_status status;
      s_import( phase, ( struct import* ) node, &status );
      break; }
   default:
      s_test_stmt( phase, test, node );
   }
}

void test_case( struct semantic* phase, struct stmt_test* test,
   struct case_label* label ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_switch ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( phase, DIAG_POS_ERR, &label->pos,
         "case outside switch statement" );
      s_bail( phase );
   }
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, false );
   s_test_expr( phase, &expr, label->number );
   if ( ! label->number->folded ) {
      s_diag( phase, DIAG_POS_ERR, &expr.pos,
         "case value not constant" );
      s_bail( phase );
   }
   struct case_label* prev = NULL;
   struct case_label* curr = target->case_head;
   while ( curr && curr->number->value < label->number->value ) {
      prev = curr;
      curr = curr->next;
   }
   if ( curr && curr->number->value == label->number->value ) {
      s_diag( phase, DIAG_POS_ERR, &label->pos, "duplicate case" );
      s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &curr->pos,
         "case with value %d found here", curr->number->value );
      s_bail( phase );
   }
   if ( prev ) {
      label->next = prev->next;
      prev->next = label;
   }
   else {
      label->next = target->case_head;
      target->case_head = label;
   }
}

void test_default_case( struct semantic* phase, struct stmt_test* test,
   struct case_label* label ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_switch ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( phase, DIAG_POS_ERR, &label->pos,
         "default outside switch statement" );
      s_bail( phase );
   }
   if ( target->case_default ) {
      s_diag( phase, DIAG_POS_ERR, &label->pos, "duplicate default case" );
      s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &target->case_default->pos,
         "default case found here" );
      s_bail( phase );
   }
   target->case_default = label;
}

void test_label( struct semantic* phase, struct stmt_test* test,
   struct label* label ) {
   // The label might be inside a format block. Find this block.
   struct stmt_test* target = test;
   while ( target && ! target->format_block ) {
      target = target->parent;
   }
   if ( target ) {
      label->format_block = target->format_block;
   }
}

void s_test_stmt( struct semantic* phase, struct stmt_test* test,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_BLOCK:
      s_test_block( phase, test, ( struct block* ) node );
      break;
   case NODE_IF:
      test_if( phase, test, ( struct if_stmt* ) node );
      break;
   case NODE_SWITCH:
      test_switch( phase, test, ( struct switch_stmt* ) node );
      break;
   case NODE_WHILE:
      test_while( phase, test, ( struct while_stmt* ) node );
      break;
   case NODE_FOR:
      test_for( phase, test, ( struct for_stmt* ) node );
      break;
   case NODE_JUMP:
      test_jump( phase, test, ( struct jump* ) node );
      break;
   case NODE_SCRIPT_JUMP:
      test_script_jump( phase, test, ( struct script_jump* ) node );
      break;
   case NODE_RETURN:
      test_return( phase, test, ( struct return_stmt* ) node );
      break;
   case NODE_GOTO:
      test_goto( phase, test, ( struct goto_stmt* ) node );
      break;
   case NODE_PALTRANS:
      test_paltrans( phase, test, ( struct paltrans* ) node );
      break;
   case NODE_FORMAT_ITEM:
      test_format_item( phase, test, ( struct format_item* ) node );
      break;
   case NODE_PACKED_EXPR:
      test_packed_expr( phase, test, ( struct packed_expr* ) node, NULL );
      break;
   default:
      // TODO: Internal compiler error.
      break;
   }
}

void test_if( struct semantic* phase, struct stmt_test* test,
   struct if_stmt* stmt ) {
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, true );
   s_test_expr( phase, &expr, stmt->cond );
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   s_test_stmt( phase, &body, stmt->body );
   if ( stmt->else_body ) {
      s_init_stmt_test( &body, test );
      s_test_stmt( phase, &body, stmt->else_body );
   }
}

void test_switch( struct semantic* phase, struct stmt_test* test,
   struct switch_stmt* stmt ) {
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, true );
   s_test_expr( phase, &expr, stmt->cond );
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.in_switch = true;
   s_test_stmt( phase, &body, stmt->body );
   stmt->case_head = body.case_head;
   stmt->case_default = body.case_default;
   stmt->jump_break = body.jump_break;
}

void test_while( struct semantic* phase, struct stmt_test* test,
   struct while_stmt* stmt ) {
   if ( stmt->type == WHILE_WHILE || stmt->type == WHILE_UNTIL ) {
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, true, true );
      s_test_expr( phase, &expr, stmt->cond );
   }
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.in_loop = true;
   s_test_stmt( phase, &body, stmt->body );
   stmt->jump_break = body.jump_break;
   stmt->jump_continue = body.jump_continue;
   if ( stmt->type == WHILE_DO_WHILE || stmt->type == WHILE_DO_UNTIL ) {
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, true, true );
      s_test_expr( phase, &expr, stmt->cond );
   }
}

void test_for( struct semantic* phase, struct stmt_test* test,
   struct for_stmt* stmt ) {
   s_add_scope( phase );
   // Initialization.
   list_iter_t i;
   list_iter_init( &i, &stmt->init );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      if ( node->type == NODE_EXPR ) {
         struct expr_test expr;
         s_init_expr_test( &expr, NULL, NULL, false, false );
         s_test_expr( phase, &expr, ( struct expr* ) node );
      }
      else {
         s_test_local_var( phase, ( struct var* ) node );
      }
      list_next( &i );
   }
   // Condition.
   if ( stmt->cond ) {
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, true, true );
      s_test_expr( phase, &expr, stmt->cond );
   }
   // Post expressions.
   list_iter_init( &i, &stmt->post );
   while ( ! list_end( &i ) ) {
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, false, false );
      s_test_expr( phase, &expr, list_data( &i ) );
      list_next( &i );
   }
   struct stmt_test body;
   s_init_stmt_test( &body, test );
   body.in_loop = true;
   s_test_stmt( phase, &body, stmt->body );
   stmt->jump_break = body.jump_break;
   stmt->jump_continue = body.jump_continue;
   s_pop_scope( phase );
}

void test_jump( struct semantic* phase, struct stmt_test* test,
   struct jump* stmt ) {
   switch ( stmt->type ) {
   case JUMP_BREAK:
      test_break( phase, test, stmt );
      break;
   case JUMP_CONTINUE:
      test_continue( phase, test, stmt );
      break;
   default:
      // TODO: Internal compiler error.
      break;
   }
}

void test_break( struct semantic* phase, struct stmt_test* test,
   struct jump* stmt ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_loop && ! target->in_switch ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( phase, DIAG_POS_ERR, &stmt->pos,
         "break outside loop or switch" );
      s_bail( phase );
   }
   stmt->next = target->jump_break;
   target->jump_break = stmt;
   // Jumping out of a format block is not allowed.
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         s_diag( phase, DIAG_POS_ERR, &stmt->pos,
            "leaving format block with a break statement" );
         s_bail( phase );
      }
      target = target->parent;
   }
}

void test_continue( struct semantic* phase, struct stmt_test* test,
   struct jump* stmt ) {
   struct stmt_test* target = test;
   while ( target && ! target->in_loop ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( phase, DIAG_POS_ERR, &stmt->pos,
         "continue outside loop" );
      s_bail( phase );
   }
   stmt->next = target->jump_continue;
   target->jump_continue = stmt;
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         s_diag( phase, DIAG_POS_ERR, &stmt->pos,
            "leaving format block with a continue statement" );
         s_bail( phase );
      }
      target = target->parent;
   }
}

void test_script_jump( struct semantic* phase, struct stmt_test* test,
   struct script_jump* stmt ) {
   static const char* names[] = { "terminate", "restart", "suspend" };
   STATIC_ASSERT( ARRAY_SIZE( names ) == SCRIPT_JUMP_TOTAL );
   struct stmt_test* target = test;
   while ( target && ! target->in_script ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( phase, DIAG_POS_ERR, &stmt->pos,
         "`%s` outside script", names[ stmt->type ] );
      s_bail( phase );
   }
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         s_diag( phase, DIAG_POS_ERR, &stmt->pos,
            "`%s` inside format block", names[ stmt->type ] );
         s_bail( phase );
      }
      target = target->parent;
   }
}

void test_return( struct semantic* phase, struct stmt_test* test,
   struct return_stmt* stmt ) {
   struct stmt_test* target = test;
   while ( target && ! target->func ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( phase, DIAG_POS_ERR, &stmt->pos,
         "return statement outside function" );
      s_bail( phase );
   }
   if ( stmt->return_value ) {
      struct pos pos;
      test_packed_expr( phase, test, stmt->return_value, &pos );
      if ( ! target->func->return_type ) {
         s_diag( phase, DIAG_POS_ERR, &pos,
            "returning value in void function" );
         s_bail( phase );
      }
   }
   else {
      if ( target->func->return_type ) {
         s_diag( phase, DIAG_POS_ERR, &stmt->pos,
            "missing return value" );
         s_bail( phase );
      }
   }
   struct stmt_test* finish = target;
   target = test;
   while ( target != finish ) {
      if ( target->format_block ) {
         s_diag( phase, DIAG_POS_ERR, &stmt->pos,
            "leaving format block with a return statement" );
         s_bail( phase );
      }
      target = target->parent;
   }
   stmt->next = phase->func_test->returns;
   phase->func_test->returns = stmt;
}

void test_goto( struct semantic* phase, struct stmt_test* test,
   struct goto_stmt* stmt ) {
   struct stmt_test* target = test;
   while ( target ) {
      if ( target->format_block ) {
         stmt->format_block = target->format_block;
         break;
      }
      target = target->parent;
   }
}

void test_paltrans( struct semantic* phase, struct stmt_test* test,
   struct paltrans* stmt ) {
   test_paltrans_arg( phase, stmt->number );
   struct palrange* range = stmt->ranges;
   while ( range ) {
      test_paltrans_arg( phase, range->begin );
      test_paltrans_arg( phase, range->end );
      if ( range->rgb ) {
         test_paltrans_arg( phase, range->value.rgb.red1 );
         test_paltrans_arg( phase, range->value.rgb.green1 );
         test_paltrans_arg( phase, range->value.rgb.blue1 );
         test_paltrans_arg( phase, range->value.rgb.red2 );
         test_paltrans_arg( phase, range->value.rgb.green2 );
         test_paltrans_arg( phase, range->value.rgb.blue2 );
      }
      else {
         test_paltrans_arg( phase, range->value.ent.begin );
         test_paltrans_arg( phase, range->value.ent.end );
      }
      range = range->next;
   }
}

void test_paltrans_arg( struct semantic* phase, struct expr* expr ) {
   struct expr_test arg;
   s_init_expr_test( &arg, NULL, NULL, true, false );
   s_test_expr( phase, &arg, expr );
}

void test_format_item( struct semantic* phase, struct stmt_test* test,
   struct format_item* item ) {
   s_test_formatitemlist_stmt( phase, test, item );
   struct stmt_test* target = test;
   while ( target && ! target->format_block ) {
      target = target->parent;
   }
   if ( ! target ) {
      s_diag( phase, DIAG_POS_ERR, &item->pos,
         "format item outside format block" );
      s_bail( phase );
   }
}

void test_packed_expr( struct semantic* phase, struct stmt_test* test,
   struct packed_expr* packed, struct pos* expr_pos ) {
   // Test expression.
   struct expr_test expr_test;
   s_init_expr_test( &expr_test, test, packed->block, false, false );
   s_test_expr( phase, &expr_test, packed->expr );
   if ( expr_pos ) {
      *expr_pos = expr_test.pos;
   }
   // Test format block.
   if ( packed->block ) {
      struct stmt_test nested;
      s_init_stmt_test( &nested, test );
      nested.format_block = packed->block;
      s_test_block( phase, &nested, nested.format_block );
      if ( ! expr_test.format_block_usage ) {
         s_diag( phase, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &packed->block->pos, "unused format block" );
      }
   }
}

void test_goto_in_format_block( struct semantic* phase, struct list* labels ) {
   list_iter_t i;
   list_iter_init( &i, labels );
   while ( ! list_end( &i ) ) {
      struct label* label = list_data( &i );
      if ( label->format_block ) {
         if ( label->users ) {
            struct goto_stmt* stmt = label->users;
            while ( stmt ) {
               if ( stmt->format_block != label->format_block ) {
                  s_diag( phase, DIAG_POS_ERR, &stmt->pos,
                     "entering format block with a goto statement" );
                  s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                     "point of entry is here" ); 
                  s_bail( phase );
               }
               stmt = stmt->next;
            }
         }
         else {
            // If a label is unused, the user might have used the syntax of a
            // label to create a format-item.
            s_diag( phase, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &label->pos, "unused label in format block" );
         }
      }
      else {
         struct goto_stmt* stmt = label->users;
         while ( stmt ) {
            if ( stmt->format_block ) {
               s_diag( phase, DIAG_POS_ERR, &stmt->pos,
                  "leaving format block with a goto statement" );
               s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &label->pos,
                  "destination of goto statement is here" );
               s_bail( phase );
            }
            stmt = stmt->next;
         }
      }
      list_next( &i );
   }
}

void s_import( struct semantic* semantic, struct import* import,
   struct import_status* status ) {
   status->resolved = false;
   status->erred = false;
   while ( import ) {
      if ( ! import->resolved ) {
         perform_import( semantic, import );
         if ( import->resolved ) {
            status->resolved = true;
         }
         else {
            status->erred = true;
            break;
         }
      }
      import = import->next;
   }
}

void perform_import( struct semantic* semantic, struct import* import ) {
   struct object* object = follow_import_path( semantic, import->path );
   if ( ! object ) {
      return;
   }
   // Name.
   if ( import->alias ) {
      alias_imported( semantic, import->alias, &import->alias_pos, object );
   }
   else {
      if ( import->get_one ) {
         struct path* path = import->path;
         while ( path->next ) {
            path = path->next;
         }
         if ( path->text ) {
            alias_imported( semantic, path->text, &path->pos, object );
         }
      }
   }
   // Import objects.
   if ( import->get_all || import->get_selected ) {
      if ( object->node.type != NODE_REGION ) {
         s_diag( semantic, DIAG_POS_ERR, &import->pos,
            "imported object not a region" );
         s_bail( semantic );
      }
      struct region* region = ( struct region* ) object;
      if ( import->get_all ) {
         import_all( semantic, &import->pos, region );
      }
      else {
         import_selected( semantic, import, region );
      }
   }
   else {
      if ( ( import->path->is_region || import->path->is_upmost ) &&
         ! import->path->next && ! import->alias ) {
         s_diag( semantic, DIAG_WARN | DIAG_POS, &import->pos,
            "useless import" );
      }
   }
   import->resolved = true;
}

struct object* follow_import_path( struct semantic* semantic,
   struct path* path ) {
   struct object* object = &semantic->task->region_upmost->object;
   if ( path->is_upmost ) {
      path = path->next;
   }
   else if ( path->is_region ) {
      object = &semantic->region->object;
      path = path->next;
   }
   while ( path ) {
      if ( object->node.type != NODE_REGION ) {
         s_diag( semantic, DIAG_POS_ERR, &path->pos,
            "`%s` not a region", path->text );
         s_bail( semantic );
      }
      struct regobjget result = s_get_regionobject( semantic,
         ( struct region* ) object, path->text, false );
      object = result.object;
      if ( ! object ) {
         if ( semantic->trigger_err ) {
            s_diag( semantic, DIAG_POS_ERR, &path->pos,
               "`%s` not found", path->text );
            s_bail( semantic );
         }
         return NULL;
      }
      path = path->next;
   }
   return object;
}

void import_all( struct semantic* semantic, struct pos* import_pos,
   struct region* region ) {
   if ( region == semantic->region ) {
      s_diag( semantic, DIAG_POS_ERR, import_pos,
         "region importing self" );
      s_bail( semantic );
   }
   struct region_link* link = semantic->region->link;
   while ( link && link->region != region ) {
      link = link->next;
   }
   // Duplicate links are allowed in the source code.
   if ( link ) {
      s_diag( semantic, DIAG_WARN | DIAG_POS,
         import_pos, "duplicate import" );
      s_diag( semantic, DIAG_POS, &link->pos,
         "import already made here" );
   }
   else {
      link = mem_alloc( sizeof( *link ) );
      link->next = semantic->region->link;
      link->region = region;
      link->pos = *import_pos;
      semantic->region->link = link;
   }
}

void import_selected( struct semantic* semantic, struct import* import,
   struct region* region ) {
   struct import_item* item = import->items;
   while ( item ) {
      import_item( semantic, item, region );
      item = item->next;
   }
}

void import_item( struct semantic* semantic, struct import_item* item,
   struct region* region ) {
   struct regobjget result = s_get_regionobject( semantic, region,
      item->name, item->get_struct );
   struct object* object = result.object;
   if ( ! object ) {
      s_diag( semantic, DIAG_POS_ERR, &item->name_pos,
         "%s`%s` not found in region", ( item->get_struct ? "struct " : "" ),
         item->name );
      s_bail( semantic );
   }
   // Name.
   if ( item->alias ) {
      alias_imported( semantic, item->alias, &item->alias_pos, object );
   }
   else {
      alias_imported( semantic, item->name, &item->name_pos, object );
   }
   // Import object.
   if ( item->get_all ) {
      if ( object->node.type != NODE_REGION ) {
         s_diag( semantic, DIAG_POS_ERR, &item->name_pos,
            "selected object not a region" );
         s_bail( semantic );
      }
      import_all( semantic, &item->name_pos, ( struct region* ) object );
   }
}

void alias_imported( struct semantic* semantic, char* alias_name,
   struct pos* alias_pos, struct object* object ) {
   struct name* name = t_make_name( semantic->task, alias_name,
      object->node.type == NODE_TYPE ? semantic->region->body_struct :
         semantic->region->body );
   // Duplicate imports are allowed as long as both names refer to the
   // same object.
   if ( name->object && name->object->node.type == NODE_ALIAS ) {
      struct alias* alias = ( struct alias* ) name->object;
      if ( object == alias->target ) {
         s_diag( semantic, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            alias_pos, "duplicate import name `%s`", alias_name );
         s_diag( semantic, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &alias->object.pos, "import name already used here" );
         return;
      }
   }
   struct alias* alias = mem_alloc( sizeof( *alias ) );
   t_init_object( &alias->object, NODE_ALIAS );
   alias->object.pos = *alias_pos;
   alias->object.resolved = true;
   alias->target = object;
   s_bind_name( semantic, name, &alias->object );
}