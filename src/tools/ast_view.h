#ifndef AST_VIEW_H
#define AST_VIEW_H

#include "common.h"

typedef struct {
   ast_t* ast;
   int depth;
} ast_view_t;


void a_view_node( ast_view_t*, node_t* );
void ast_view_init( ast_view_t*, ast_t* );
void ast_view_show( ast_view_t* );
void ast_view_expr( node_t* );

#endif