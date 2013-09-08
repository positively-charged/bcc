#include <stdio.h>
#include <stdarg.h>

#include "ast_view.h"
#include "list.h"

static void inc_pad( ast_view_t* );
static void dec_pad( ast_view_t* );
static void print_base( ast_view_t*, const char*, va_list );
static void print( ast_view_t*, const char*, ... );
static void print_ln( ast_view_t*, const char*, ... );
static void do_node( ast_view_t*, node_t* );
static void do_jump( ast_view_t*, jump_t* );
static void do_literal( ast_view_t*, literal_t* );
static void do_while( ast_view_t*, while_t* );
static void do_cond( ast_view_t*, node_t* );
static void do_binary( ast_view_t*, binary_t* );
static void do_call( ast_view_t*, call_t* );
static void do_format_item( ast_view_t*, format_item_t* );
static void do_format_block( ast_view_t*, format_block_t* );
static void do_var( ast_view_t*, var_t* );
static void do_subscript( ast_view_t*, subscript_t* );
static void do_if( ast_view_t*, if_t* );
static void do_body( ast_view_t*, list_t* );
static void do_cond( ast_view_t*, node_t* );
static void do_script( ast_view_t*, script_t* );
static void do_switch( ast_view_t*, switch_t* );
static void do_case( ast_view_t*, case_t* );
static void do_member_access( ast_view_t*, member_access_t* );
static void do_for( ast_view_t*, for_t* );
static void do_ufunc_def( ast_view_t*, ufunc_t* );
static void do_return( ast_view_t*, return_t* );
static void do_name( ast_view_t*, name_t* );
static void do_expr( ast_view_t*, expr_t* );

void ast_view_init( ast_view_t* view, ast_t* ast ) {
   view->ast = ast;
   view->depth = 0;
}

void ast_view_show( ast_view_t* view ) {
   // Scripts.
   list_iterator_t i = list_iterate( &view->ast->module->scripts );
   while ( ! list_end( &i ) ) {
      do_script( view, list_idata( &i ) );
      list_next( &i );
   }

   // Functions.
   i = list_iterate( &view->ast->module->funcs );
   while ( ! list_end( &i ) ) {
      do_ufunc_def( view, list_idata( &i ) );
      list_next( &i );
   }

   // Variables.
   print_ln( view, "module-scalars: " );
   inc_pad( view );
   i = list_iterate( &view->ast->vars );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      do_var( view, var );
      list_next( &i );
   }
   dec_pad( view );
   print_ln( view, "module-hidden-scalars: " );
   inc_pad( view );
   i = list_iterate( &view->ast->module->hidden_vars );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      do_var( view, var );
      list_next( &i );
   }
   dec_pad( view );

   // Arrays.
   print_ln( view, "module-arrays: " );
   i = list_iterate( &view->ast->arrays );
   while ( ! list_end( &i ) ) {
      var_t* var = list_idata( &i );
      do_var( view, var );
      list_next( &i );
   }

   // Imports.
   i = list_iterate( &view->ast->module->imports );
   while ( ! list_end( &i ) ) {
      module_t* module = list_idata( &i );
      print_ln( view, "imported-module: %s", module->name.value );
      list_next( &i );
   }

   // String table.
   print_ln( view, "str-table:" );
   indexed_string_t* string = view->ast->str_table.head;
   while ( string ) {
      print_ln( view, "  [ %d ] \"%s\"", string->index, string->value );
      string = string->next;
   }
}

void ast_view_expr( node_t* node ) {
   ast_view_t view = { NULL, 0 };
   do_node( &view, node );
}

void inc_pad( ast_view_t* view ) {
   ++view->depth;
}

void dec_pad( ast_view_t* view ) {
   --view->depth;
}

void print_pad( ast_view_t* view ) {
   for ( int i = 0; i < view->depth; ++i ) {
      printf( "  " );
   }
}

void print( ast_view_t* view, const char* format, ... ) {
   va_list args;
   va_start( args, format );
   vprintf( format, args );
   va_end( args );
}

void print_ln( ast_view_t* view, const char* format, ... ) {
   va_list args;
   va_start( args, format );
   print_pad( view );
   vprintf( format, args );
   printf( "\n" );
   va_end( args );
}

void do_if( ast_view_t* view, if_t* stmt ) {
   print_ln( view, "if" );
   inc_pad( view );
   do_cond( view, stmt->cond );
   dec_pad( view );
   do_body( view, &stmt->body );
   if ( list_size( &stmt->else_body ) ) {
      print_ln( view, "else" );
      do_body( view, &stmt->else_body );
   }
}

void do_cond( ast_view_t* view, node_t* node ) {
   print_ln( view, "condition" );
   inc_pad( view );
   do_node( view, node );
   dec_pad( view );
}

void do_body( ast_view_t* view, list_t* stmt_list ) {
   // Children.
   inc_pad( view );
   list_iterator_t i = list_iterate( stmt_list );
   while ( ! list_end( &i ) ) {
      do_node( view, list_idata( &i ) );
      list_next( &i );
   }
   dec_pad( view );
}

void a_view_node( ast_view_t* view, node_t* node  ) {
   do_node( view, node );
}

void do_node( ast_view_t* view, node_t* node ) {
   switch ( node->type ) {
      case k_node_while:
         do_while( view, ( while_t* ) node );
         break;
      case k_node_jump:
         do_jump( view, ( jump_t* ) node );
         break;
      case k_node_literal:
         do_literal( view, ( literal_t* ) node );
         break;
      case k_node_binary:
         do_binary( view, ( binary_t* ) node );
         break;
      case k_node_name:
         do_name( view, ( name_t* ) node );
         break;
      case k_node_expr:
         do_expr( view, ( expr_t* ) node );
         break;
      case k_node_call:
         do_call( view, ( call_t* ) node );
         break;
      case k_node_format_item:
         do_format_item( view, ( format_item_t* ) node );
         break;
      case k_node_format_block:
         do_format_block( view, ( format_block_t* ) node );
         break;
      case k_node_var:
         do_var( view, ( var_t* ) node );
         break;
      case k_node_subscript:
         do_subscript( view, ( subscript_t* ) node );
         break;
      case k_node_if:
         do_if( view, ( if_t* ) node );
         break;
      case k_node_switch:
         do_switch( view, ( switch_t* ) node );
         break;
      case k_node_case:
         do_case( view, ( case_t* ) node );
         break;
      case k_node_member_access:
         do_member_access( view, ( member_access_t* ) node );
         break;
      case k_node_for:
         do_for( view, ( for_t* ) node );
         break;
      case k_node_return:
         do_return( view, ( return_t* ) node );
         break;
      default:
         print_ln( view, "unknown-entity: %d", node->type );
         break;
   }
}

void do_expr( ast_view_t* view, expr_t* expr ) {
   print_ln( view, "expr: " );
   inc_pad( view );
   do_node( view, expr->root );
   dec_pad( view );
}

void do_name( ast_view_t* view, name_t* node ) {
   char name[ 100 ];
   if ( node->object->type == k_node_func ) {
      print_ln( view, "func: %s",
         ntbl_save( node->key, name, 100 ) );
   }
   else if ( node->object->type == k_node_var ) {
      print_ln( view, "var: %s length:%d",
         ntbl_save( node->key, name, 100 ), ( int ) node->key->length );
   }
   else if ( node->object->type == k_node_constant ) {
      constant_t* constant = ( constant_t* ) node->object;
      print_ln( view, "constant: name(%s) value(%d)",
         ntbl_save( node->key, name, 100 ), constant->value );
   }
   else if ( node->object->type == k_node_undef ) {
      print_ln( view, "undefined: %s",
         ntbl_save( node->key, name, 100 ) );
   }
}

void do_script( ast_view_t* view, script_t* script ) {
   print_ln( view, "script %d size(%d)", script->number->value,
      script->size );
   do_body( view, &script->body );
}

void do_jump( ast_view_t* view, jump_t* jump ) {
   switch ( jump->type ) {
      case k_jump_continue: print_ln( view, "continue" ); break;
      case k_jump_terminate: print_ln( view, "terminate" ); break;
      case k_jump_suspend: print_ln( view, "suspend" ); break;
      default: print_ln( view, "restart" ); break;
   }
}

void do_literal( ast_view_t* view, literal_t* literal ) {
   print_ln( view, "%d", literal->value );
}

void do_while( ast_view_t* view, while_t* loop ) {
   switch ( loop->type ) {
      case k_while_while: print_ln( view, "while" ); break;
      case k_while_until: print_ln( view, "until" ); break;
      case k_while_do_while: print_ln( view, "do-while" ); break;
      case k_while_do_until: print_ln( view, "do-until" ); break;
   }

   inc_pad( view );
   do_cond( view, loop->cond );
   dec_pad( view );
   do_body( view, &loop->body );
}

void do_switch( ast_view_t* view, switch_t* stmt ) {
   print_ln( view, "switch" );
/*
   n_case_t* item = stmt->head_case;
   while ( item ) {
      if ( item->expr ) {
         print_ln( view, "case %d:", item->expr->value );
      }
      else {
         print_ln( view, "default:" );
      }
      // do_body( view, &c_stmt->body );
      item = item->next;
   }*/
   do_body( view, &stmt->body );
}

void do_case( ast_view_t* view, case_t* item ) {
   if ( item->expr ) {
      print_ln( view, "case %d:", item->expr->value );
   }
   else {
      print_ln( view, "default:" );
   }
}

void do_binary( ast_view_t* view, binary_t* binary ) {
   // These values are ordered in the same order as the bop_ enums.
   const char* names[] = {
      //"=",
      //"+=",
      ////"-=",
      //"*=",
      //"/=",
      //"%=",
      //"<<=",
      //">>=",
      //"&=",
      //"^=",
     // "|=",
      "||",
      "&&",
      "|",
      "^",
      "&",
      "==",
      "!=",
      "<",
      "<=",
      ">",
      ">=",
      "<<",
      ">>",
      "+",
      "-",
      "*",
      "/",
      "%"
   };

   int total = sizeof( names ) / sizeof( names[ 0 ] );
   if ( binary->op < total ) {
      print_ln( view, names[ binary->op ] );
   }
   else {
      print_ln( view, "unknown-binary-op: %d", binary->op );
   }

   inc_pad( view );
   do_node( view, binary->lside );
   do_node( view, binary->rside );
   dec_pad( view );
}

void do_call( ast_view_t* view, call_t* call ) {
   print_ln( view, "call" );
   inc_pad( view );
   do_node( view, call->func );
   list_iterator_t i = list_iterate( &call->args );
   if ( ! list_end( &i ) ) {
      print_ln( view, "args: " );
      inc_pad( view );
      while ( ! list_end( &i ) ) {
         do_node( view, list_idata( &i ) );
         list_next( &i );
      }
      dec_pad( view );
   }
   else {
      print_ln( view, "args: (none)" );
   }
   dec_pad( view );
}

void do_format_item( ast_view_t* view, format_item_t* item ) {
   print_ln( view, "format-item" );
   inc_pad( view );
   const char* names[] = {
      "array",
      "binary",
      "char",
      "decimal",
      "fixed",
      "key",
      "local_string",
      "name",
      "string",
      "hex"
   };
   print_ln( view, "cast: %s", names[ item->cast ] );
   do_node( view, item->expr->root );
   dec_pad( view );
}

void do_format_block( ast_view_t* view, format_block_t* block ) {
   print_ln( view, "format-block" );
   do_body( view, &block->body );
}

void do_var( ast_view_t* view, var_t* var ) {
   const char* types[] = {
      "void",
      "int",
      "str",
      "bool",
      "char",
      "fixed"
   };
   const char* assign = "";
   if ( var->initial ) {
      assign = "=";
   }
   const char* storages[] = {
      "none",
      "local",
      "module",
      "world",
      "global"
   };
   char n[ 100 ];
   print( view, "var: name(%s)", ntbl_save( var->name, n, 100 ) );
   int size = var->type->size;
   if ( var->dim ) {
      print( view, " " );
      dimension_t* dim = var->dim;
      size = dim->element_size * dim->size;
      while ( dim ) {
         print( view, "[%d]", dim->size );
         dim = dim->next;
      }
   }
   char t[ 100 ];
   print( view, " size(%d) type(%s) storage(%s) index(%d) %s",
      size,
      ntbl_save( var->type->name, t, 100 ),
      storages[ var->storage ],
      var->index,
      assign );
   print( view, "\n" );
   if ( var->initial ) {
      inc_pad( view );
      do_node( view, var->initial->value );
      dec_pad( view );
/*
   list_iterator_t i = list_iterate( &array->initials );
   while ( ! list_end( &i ) ) {
      node_t* node = list_idata( &i );
      if ( node->type == k_node_array_jump ) {
         n_dimension_mark_t* mark = ( n_dimension_mark_t* ) node;
         if ( mark->type == k_dimension_mark_enter ) {
            print_ln( view, "{" );
            inc_pad( view );
         }
         else {
            dec_pad( view );
            print_ln( view, "}" );
         }
      }
      else {
         do_node( view, node );
      }
      list_next( &i );
   }
*/
   }
}

void do_subscript( ast_view_t* view, subscript_t* subscript ) {
   print_ln( view, "subscript:" );
   inc_pad( view );
   do_node( view, subscript->operand );
   do_node( view, subscript->index->root );
   dec_pad( view );
}

void do_member_access( ast_view_t* view, member_access_t* access ) {
   print_ln( view, "." );
   inc_pad( view );
   do_node( view, access->object );
   print( view, "member: " );
   ntbl_show_key( access->name );
   printf( "\n" );
   dec_pad( view );
}

void do_for( ast_view_t* view, for_t* stmt ) {
   print_ln( view, "for" );
   inc_pad( view );

   print_ln( view, "init" );
   list_iterator_t i = list_iterate( &stmt->init );
   inc_pad( view );
   while ( ! list_end( &i ) ) {
      do_node( view, list_idata( &i ) );
      list_next( &i );
   }
   dec_pad( view );

   if ( stmt->cond ) {
      do_cond( view, stmt->cond );
   }
   else {
      print_ln( view, "condition: (none)" );
   }

   print_ln( view, "post" );
   inc_pad( view );
   if ( stmt->post ) {
      do_expr( view, stmt->post );
   }
   dec_pad( view );

   print_ln( view, "body:" );
   do_body( view, &stmt->body );
   dec_pad( view );
}

void do_ufunc_def( ast_view_t* view, ufunc_t* ufunc ) {
   const char* ret = "false";
   if ( ufunc->func.return_type != NULL ) {
      ret = "true";
   }

   char name[ 100 ];
   print_ln( view,
      "function: name(%s) index(%d) size(%d) has-return(%s) num-param(%d) "
      "min-param(%d)",
      ntbl_save( ufunc->name, name, 100 ), ufunc->index, 
      ufunc->size, ret,
      ufunc->func.num_param,
      ufunc->func.min_param );

   inc_pad( view );
   list_iterator_t i = list_iterate( &ufunc->params );
   while ( ! list_end( &i ) ) {
      param_t* param = list_idata( &i );
      char name[ 100 ];
      char type[ 100 ];
      print_pad( view );
      print( view, "param: %s type(%s) index(%d)",
         ntbl_save( param->name, name, 100 ),
         ntbl_save( param->var->type->name, type, 100 ),
         param->var->index );
      if ( param->default_value ) {
         print( view, " = \n" );
         inc_pad( view );
         do_node( view, &param->default_value->node );
         dec_pad( view );
      }
      else {
         print( view, "\n" );
      }
      list_next( &i );
   }
   print_ln( view, "body: " );
   do_body( view, &ufunc->body );
   dec_pad( view );
}

void do_return( ast_view_t* view, return_t* stmt ) {
   if ( stmt->expr ) {
      print_ln( view, "return = " );
      inc_pad( view );
      do_node( view, &stmt->expr->node );
      dec_pad( view );
   }
   else {
      print_ln( view, "return = (void)" );
   }
}