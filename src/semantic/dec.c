#include "phase.h"

#define SCRIPT_MIN_NUM 0
#define SCRIPT_MAX_NUM 999

struct multi_value_test {
   struct dim* dim;
   struct type* type;
   struct type_member* member;
   int count;
   bool constant;
   bool nested;
   bool has_string;
};

struct value_list {
   struct value* head;
   struct value* tail;
};

struct value_index_alloc {
   struct value* value;
   int index;
};

static void test_script_number( struct semantic* phase, struct script* script );
static void test_script_params( struct semantic* phase, struct script* script );
static void test_script_body( struct semantic* phase, struct script* script );
static void test_type_member( struct semantic* phase, struct type_member* );
static struct type* find_type( struct semantic* phase, struct path* );
static bool test_spec( struct semantic* phase, struct var* var );
static void test_name( struct semantic* phase, struct var* var );
static bool test_dim( struct semantic* phase, struct var* var );
static bool test_initz( struct semantic* phase, struct var* var );
static bool test_object_initz( struct semantic* phase, struct var* var );
static bool test_imported_object_initz( struct var* var );
static void test_init( struct semantic* phase, struct var*, bool, bool* );
static void init_multi_value_test( struct multi_value_test*, struct dim*,
   struct type*, bool constant, bool nested );
static bool test_multi_value( struct semantic* phase, struct multi_value_test*,
   struct multi_value* multi_value );
static bool test_multi_value_child( struct semantic* phase,
   struct multi_value_test* test, struct multi_value* multi_value,
   struct initial* initial );
static bool test_value( struct semantic* phase, struct multi_value_test* test,
   struct dim* dim, struct type* type, struct value* value );
static void calc_dim_size( struct dim*, struct type* );
static void test_user_func( struct semantic* phase, struct func* func );
static bool test_func_params( struct semantic* semantic, struct func* func );
static void test_builtin_func( struct semantic* phase, struct func* func );
static void test_func_body( struct semantic* phase, struct func* );
static void calc_type_size( struct type* );
static void make_value_list( struct value_list*, struct multi_value* );
static void alloc_value_index( struct value_index_alloc*, struct multi_value*,
   struct type*, struct dim* );
static void alloc_value_index_struct( struct value_index_alloc*,
   struct multi_value*, struct type* );
static void diag_dup_struct_member( struct task* task, struct name*, struct pos* );

void s_test_constant( struct semantic* phase, struct constant* constant ) {
   // Test name. Only applies in a local scope.
   if ( phase->depth ) {
      if ( ! constant->name->object ||
         constant->name->object->depth != phase->depth ) {
         s_bind_local_name( phase, constant->name, &constant->object );
      }
      else {
         struct str str;
         str_init( &str );
         t_copy_name( constant->name, false, &str );
         diag_dup( phase->task, str.value, &constant->object.pos, constant->name );
         s_bail( phase );
      }
   }
   // Test expression.
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, phase->undef_err, false );
   s_test_expr( phase, &expr, constant->value_node );
   if ( ! expr.undef_erred ) {
      if ( constant->value_node->folded ) {
         constant->value = constant->value_node->value;
         constant->object.resolved = true;
      }
      else {
         s_diag( phase, DIAG_POS_ERR, &constant->value_node->pos,
            "expression not constant" );
         s_bail( phase );
      }
   }
}

void s_test_constant_set( struct semantic* phase, struct constant_set* set ) {
   int value = 0;
   // Find the next unresolved enumerator.
   struct constant* enumerator = set->head;
   while ( enumerator && enumerator->object.resolved ) {
      value = enumerator->value;
      enumerator = enumerator->next;
   }
   while ( enumerator ) {
      if ( phase->depth ) {
         if ( ! enumerator->name->object ||
            enumerator->name->object->depth != phase->depth ) {
            s_bind_local_name( phase, enumerator->name, &enumerator->object );
         }
         else {
            struct str str;
            str_init( &str );
            t_copy_name( enumerator->name, false, &str );
            diag_dup( phase->task, str.value, &enumerator->object.pos,
               enumerator->name );
            s_bail( phase );
         }
      }
      if ( enumerator->value_node ) {
         struct expr_test expr;
         s_init_expr_test( &expr, NULL, NULL, true, phase->undef_err, false );
         s_test_expr( phase, &expr, enumerator->value_node );
         if ( expr.undef_erred ) {
            return;
         }
         if ( ! enumerator->value_node->folded ) {
            s_diag( phase, DIAG_POS_ERR, &expr.pos,
               "enumerator expression not constant" );
            s_bail( phase );
         }
         value = enumerator->value_node->value;
      }
      enumerator->value = value;
      ++value;
      enumerator->object.resolved = true;
      enumerator = enumerator->next;
   }
   // Set is resolved when all of the constants in it are resolved.
   set->object.resolved = true;
}

void s_test_type( struct semantic* phase, struct type* type ) {
   // Name.
   if ( phase->depth ) {
      if ( type->name->object && type->name->object->depth == phase->depth ) {
         diag_dup_struct( phase->task, type->name, &type->object.pos );
         s_bail( phase );
      }
      s_bind_local_name( phase, type->name, &type->object );
   }
   // Members.
   struct type_member* member = type->member;
   while ( member ) {
      if ( ! member->object.resolved ) {
         test_type_member( phase, member );
         if ( ! member->object.resolved ) {
            return;
         }
      }
      member = member->next;
   }
   type->object.resolved = true;
}

void test_type_member( struct semantic* phase, struct type_member* member ) {
   // Type:
   if ( member->type_path ) {
      if ( ! member->type ) {
         member->type = find_type( phase, member->type_path );
      }
      if ( ! member->type->object.resolved ) {
         if ( phase->undef_err ) {
            struct path* path = member->type_path;
            while ( path->next ) {
               path = path->next;
            }
            s_diag( phase, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &path->pos,
               "struct `%s` undefined", path->text );
            s_bail( phase );
         }
         return;
      }
   }
   // Name:
   if ( phase->depth ) {
      if ( ! member->name->object ||
         member->name->object->depth != phase->depth ) {
         s_bind_local_name( phase, member->name, &member->object );
      }
      else {
         diag_dup_struct_member( phase->task, member->name, &member->object.pos );
         s_bail( phase );
      }
   }
   else {
      if ( member->name->object != &member->object ) {
         if ( ! member->name->object ) {
            member->name->object = &member->object;
         }
         else {
            diag_dup_struct_member( phase->task, member->name, &member->object.pos );
            s_bail( phase );
         }
      }
   }
   // Dimension.
   struct dim* dim = member->dim;
   // Skip to the next unresolved dimension.
   while ( dim && dim->size ) {
      dim = dim->next;
   }
   while ( dim ) {
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, true, phase->undef_err, false );
      s_test_expr( phase, &expr, dim->size_node );
      if ( expr.undef_erred ) {
         return;
      }
      if ( ! dim->size_node->folded ) {
         s_diag( phase, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
         "array size not a constant expression" );
         s_bail( phase );
      }
      if ( dim->size_node->value <= 0 ) {
         s_diag( phase, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &expr.pos,
            "array size must be greater than 0" );
         s_bail( phase );
      }
      dim->size = dim->size_node->value;
      dim = dim->next;
   }
   member->object.resolved = true;
}

// NOTE: The code here and the code that implements the dot operator in the
// expression subsystem, are very similar. Maybe find a way to merge them.
struct type* find_type( struct semantic* phase, struct path* path ) {
   // Find head of path.
   struct object* object = NULL;
   if ( path->is_upmost ) {
      object = &phase->task->region_upmost->object;
      path = path->next;
   }
   else if ( path->is_region ) {
      object = &phase->region->object;
      path = path->next;
   }
   // When no region is specified, search for the head in the current region.
   if ( ! object ) {
      struct name* name = phase->region->body;
      if ( ! path->next ) {
         name = phase->region->body_struct;
      }
      name = t_make_name( phase->task, path->text, name );
      if ( name->object ) {
         object = name->object;
         path = path->next;
      }
   }
   // When the head is not found in the current region, try linked regions.
   struct region_link* link = NULL;
   if ( ! object ) {
      link = phase->region->link;
      while ( link ) {
         struct name* name = link->region->body;
         if ( ! path->next ) {
            name = link->region->body_struct;
         }
         name = t_make_name( phase->task, path->text, name );
         link = link->next;
         if ( name->object ) {
            object = name->object;
            path = path->next;
            break;
         }
      }
   }
   // Error.
   if ( ! object ) {
      const char* msg = "`%s` not found";
      if ( ! path->next ) {
         msg = "struct `%s` not found";
      }
      s_diag( phase, DIAG_POS_ERR, &path->pos, msg, path->text );
      s_bail( phase );
   }
   // When using a region link, make sure no other object with the same name
   // can be found.
   if ( link ) {
      bool dup = false;
      while ( link ) {
         struct name* name = link->region->body;
         if ( ! path->next ) {
            name = link->region->body_struct;
         }
         name = t_make_name( phase->task, path->text, name );
         if ( name->object ) {
            const char* type = "object";
            if ( ! path->next ) {
               type = "struct";
            }
            if ( ! dup ) {
               s_diag( phase, DIAG_POS_ERR, &path->pos,
                  "%s `%s` found in multiple modules", type, path->text );
               s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &object->pos,
                  "%s found here", type );
               dup = true;
            }
            s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &name->object->pos, "%s found here", type );
         }
         link = link->next;
      }
      if ( dup ) {
         s_bail( phase );
      }
   }
   // Navigate rest of path.
   while ( true ) {
      // Follow a shortcut. It needs to refer to an object.
      while ( object->node.type == NODE_ALIAS ) {
         struct alias* alias = ( struct alias* ) object;
         object = alias->target;
      }
      if ( ! path ) {
         break;
      }
      if ( object->node.type == NODE_REGION ) {
         struct region* region = ( struct region* ) object;
         struct name* name = region->body;
         if ( ! path->next ) {
            name = region->body_struct;
         }
         name = t_make_name( phase->task, path->text, name );
         if ( name->object ) {
            object = name->object;
         }
         else {
            if ( path->next ) {
               s_diag( phase, DIAG_POS_ERR, &path->pos, "region `%s` not found",
                  path->text );
            }
            else {
               s_diag( phase, DIAG_POS_ERR, &path->pos, "struct `%s` not found",
                  path->text ); 
            }
            s_bail( phase );
         }
      }
      else {
         s_diag( phase, DIAG_POS_ERR, &path->pos,
            "accessing something not a region" );
         s_bail( phase );
      }
      path = path->next;
   }
   return ( struct type* ) object;
}

void s_test_var( struct semantic* phase, struct var* var ) {
   if ( test_spec( phase, var ) ) {
      test_name( phase, var );
      if ( test_dim( phase, var ) ) {
         var->object.resolved = test_initz( phase, var );
      }
   }
}

bool test_spec( struct semantic* phase, struct var* var ) {
   bool resolved = false;
   if ( var->type_path ) {
      if ( ! var->type ) {
         var->type = find_type( phase, var->type_path );
      }
      if ( ! var->type->object.resolved ) {
         return false;
      }
      // An array or a variable with a structure type cannot appear in local
      // storage because there's no standard or efficient way to allocate such
      // a variable.
      if ( var->storage == STORAGE_LOCAL ) {
         s_diag( phase, DIAG_POS_ERR, &var->object.pos,
            "variable of struct type in local storage" );
         s_bail( phase );
      }
   }
   return true;
}

void test_name( struct semantic* phase, struct var* var ) {
   // Bind name of local variable.
   if ( phase->depth ) {
      if ( var->name->object && var->name->object->depth == phase->depth ) {
         struct str str;
         str_init( &str );
         t_copy_name( var->name, false, &str );
         diag_dup( phase->task, str.value, &var->object.pos, var->name );
         s_bail( phase );
      }
      s_bind_local_name( phase, var->name, &var->object );
   }
}

bool test_dim( struct semantic* phase, struct var* var ) {
   // No need to continue when the variable is not an array.
   if ( ! var->dim ) {
      return true;
   }
   // Find first untested dimension. A dimension is considered untested when it
   // has a size of zero.
   struct dim* dim = var->dim;
   while ( dim && dim->size ) {
      dim = dim->next;
   }
   while ( dim ) {
      if ( dim->size_node ) {
         struct expr_test expr;
         s_init_expr_test( &expr, NULL, NULL, true, phase->undef_err, false );
         s_test_expr( phase, &expr, dim->size_node );
         if ( expr.undef_erred ) {
            return false;
         }
         if ( ! dim->size_node->folded ) {
            s_diag( phase, DIAG_POS_ERR, &expr.pos,
               "dimension size not a constant expression" );
            s_bail( phase );
         }
         if ( dim->size_node->value <= 0 ) {
            s_diag( phase, DIAG_POS_ERR, &expr.pos,
               "dimension size less than or equal to 0" );
            s_bail( phase );
         }
         dim->size = dim->size_node->value;
      }
      else {
         // Only the first dimension can have an implicit size.
         if ( dim != var->dim ) {
            s_diag( phase, DIAG_POS_ERR, &dim->pos,
               "implicit size in subsequent dimension" );
            s_bail( phase );
         }
      }
      dim = dim->next;
   }
   if ( var->storage == STORAGE_LOCAL ) {
      s_diag( phase, DIAG_POS_ERR, &var->object.pos,
         "array in local storage" );
      s_bail( phase );
   }
   return true;
}

bool test_initz( struct semantic* phase, struct var* var ) {
   if ( var->initial ) {
      if ( var->imported ) {
         return test_imported_object_initz( var );
      }
      else {
         return test_object_initz( phase, var );
      }
   }
   return true;
}

bool test_object_initz( struct semantic* phase, struct var* var ) {
   struct multi_value_test test;
   init_multi_value_test( &test, var->dim, var->type, var->is_constant_init,
      false );
   if ( var->initial->multi ) {
      bool resolved = test_multi_value( phase, &test,
         ( struct multi_value* ) var->initial );
      if ( ! resolved ) {
         return false;
      }
      // Update size of implicit dimension.
      if ( var->dim && ! var->dim->size_node ) {
         var->dim->size = test.count;
      }
   }
   else {
      bool resolved = test_value( phase, &test, var->dim, var->type,
         ( struct value* ) var->initial );
      if ( ! resolved ) {
         return false;
      }
   }
   var->initial_has_str = test.has_string;
   return true;
}

bool test_imported_object_initz( struct var* var ) {
   if ( var->dim ) {
      // TODO: Add error checking.
      if ( ! var->dim->size_node && ! var->dim->size ) {
         struct multi_value* multi_value =
            ( struct multi_value* ) var->initial;
         struct initial* initial = multi_value->body;
         while ( initial ) {
            initial = initial->next;
            ++var->dim->size;
         }
      }
   }
   return true;
}

void init_multi_value_test( struct multi_value_test* test, struct dim* dim,
   struct type* type, bool constant, bool nested ) {
   test->dim = dim;
   test->type = type;
   test->member = ( ! dim ? type->member : NULL ); 
   test->count = 0;
   test->constant = constant;
   test->nested = nested;
   test->has_string = false;
}

bool test_multi_value( struct semantic* phase, struct multi_value_test* test,
   struct multi_value* multi_value ) {
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( ! initial->tested ) {
         if ( ! test_multi_value_child( phase, test, multi_value, initial ) ) {
            return false;
         }
         initial->tested = true;
      }
      if ( test->member ) {
         test->member = test->member->next;
      }
      ++test->count;
      initial = initial->next;
   }
   return true;
}

bool test_multi_value_child( struct semantic* phase, struct multi_value_test* test,
   struct multi_value* multi_value, struct initial* initial ) {
   bool capacity = ( ( test->dim && ( ! test->dim->size_node ||
      test->count < test->dim->size ) ) || test->member );
   if ( ! capacity ) {
      s_diag( phase, DIAG_POS_ERR, &multi_value->pos,
         "too many values in brace initializer" );
      s_bail( phase );
   }
   if ( initial->multi ) {
      // There needs to be an element or member to initialize.
      bool deeper = ( ( test->dim && ( test->dim->next ||
         ! test->type->primitive ) ) || ( test->member &&
         ( test->member->dim || ! test->member->type->primitive ) ) );
      if ( ! deeper ) {
         s_diag( phase, DIAG_POS_ERR, &multi_value->pos,
            "too many brace initializers" );
         s_bail( phase );
      }
      struct multi_value_test nested;
      init_multi_value_test( &nested,
         ( test->dim ? test->dim->next : test->member->dim ),
         ( test->dim ? test->type : test->member->type ),
         test->constant, true );
      bool resolved = test_multi_value( phase, &nested,
         ( struct multi_value* ) initial );
      if ( nested.has_string ) {
         test->has_string = true;
      }
      return resolved;
   }
   else {
      bool resolved = test_value( phase, test,
         ( test->dim ? test->dim->next : test->member->dim ),
         ( test->dim ? test->type : test->member->type ),
         ( struct value* ) initial );
      if ( ! resolved ) {
         return false;
      }
      // At this time, I know of no good way to initialize a string member.
      // The user will have to initialize the member manually, by using an
      // assignment operation.
      if ( test->member && test->member->type == phase->task->type_str ) {
         struct value* value = ( struct value* ) initial;
         s_diag( phase, DIAG_POS_ERR, &value->expr->pos,
            "initializing struct member of `str` type" );
         s_bail( phase );
      }
      return true;
   }
}

bool test_value( struct semantic* phase, struct multi_value_test* test,
   struct dim* dim, struct type* type, struct value* value ) {
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, phase->undef_err, false );
   s_test_expr( phase, &expr, value->expr );
   if ( expr.undef_erred ) {
      return false;
   }
   if ( test->constant && ! value->expr->folded ) {
      s_diag( phase, DIAG_POS_ERR, &expr.pos,
         "non-constant initializer" );
      s_bail( phase );
   }
   // Only initialize a primitive element or an array of a single dimension--
   // using the string initializer.
   if ( ! ( ! dim && type->primitive ) && ! ( dim && ! dim->next &&
      value->expr->root->type == NODE_INDEXED_STRING_USAGE &&
      type->primitive ) ) {
      s_diag( phase, DIAG_POS_ERR, &expr.pos,
         "missing %sbrace initializer", test->nested ? "another " : "" );
      s_bail( phase );
   }
   // String initializer.
   if ( dim ) {
      // Even though it doesn't matter what primitive type is specified, for
      // readability purposes, restrict the string initializer to an array of
      // `int` type.
      if ( type != phase->task->type_int ) {
         s_diag( phase, DIAG_POS_ERR, &expr.pos,
            "string initializer specified for a non-int array" );
         s_bail( phase );
      }
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) value->expr->root;
      if ( dim->size_node ) {
         if ( usage->string->length >= dim->size ) {
            s_diag( phase, DIAG_POS_ERR, &expr.pos,
               "string initializer too long" );
            s_bail( phase );
         }
      }
      else {
         dim->size = usage->string->length + 1;
      }
      value->string_initz = true;
   }
   if ( expr.has_string ) {
      test->has_string = true;
   }
   return true;
}

void s_test_local_var( struct semantic* phase, struct var* var ) {
   s_test_var( phase, var );
   s_calc_var_size( var );
   if ( var->initial ) {
      s_calc_var_value_index( var );
   }
}

void s_test_func( struct semantic* phase, struct func* func ) {
   if ( func->type == FUNC_USER ) {
      test_user_func( phase, func );
   }
   else {
      test_builtin_func( phase, func );
   }
}

void test_user_func( struct semantic* semantic, struct func* func ) {
   struct func_user* impl = func->impl;
   // Test name of nested function.
   if ( impl->nested ) {
      if ( func->name->object && func->name->object->depth == semantic->depth ) {
         struct str str;
         str_init( &str );
         t_copy_name( func->name, false, &str );
         diag_dup( semantic->task, str.value, &func->object.pos, func->name );
         s_bail( semantic );
      }
      s_bind_local_name( semantic, func->name, &func->object );
   }
   s_add_scope( semantic );
   if ( test_func_params( semantic, func ) ) {
      func->object.resolved = true;
      // Test body of nested function.
      if ( impl->nested ) {
         impl->next_nested = semantic->topfunc_test->nested_funcs;
         semantic->topfunc_test->nested_funcs = func;
         s_test_func_body( semantic, func );
      }
   }
   s_pop_scope( semantic );
}

bool test_func_params( struct semantic* semantic, struct func* func ) {
   // Skip tested parameters.
   struct param* param = func->params;
   while ( param && param->object.resolved ) {
      param = param->next;
   }
   while ( param ) {
      if ( param->default_value ) {
         struct expr_test expr;
         s_init_expr_test( &expr, NULL, NULL, true, semantic->undef_err,
            false );
         s_test_expr( semantic, &expr, param->default_value );
         if ( expr.undef_erred ) {
            return false;
         }
      }
      // Any previous parameter is visible inside the expression of a
      // default parameter.
      if ( param->name ) {
         if ( param->name->object &&
            param->name->object->depth == semantic->depth ) {
            struct str str;
            str_init( &str );
            t_copy_name( param->name, false, &str );
            diag_dup( semantic->task, str.value, &param->object.pos,
               param->name );
            s_bail( semantic );
         }
         s_bind_local_name( semantic, param->name, &param->object );
      }
      param->object.resolved = true;
      param = param->next;
   }
   return true;
}

void test_builtin_func( struct semantic* phase, struct func* func ) {
   // Default arguments:
   struct param* param = func->params;
   while ( param && param->object.resolved ) {
      param = param->next;
   }
   while ( param ) {
      if ( param->default_value ) {
         struct expr_test expr;
         s_init_expr_test( &expr, NULL, NULL, true, phase->undef_err, false );
         s_test_expr( phase, &expr, param->default_value );
         if ( expr.undef_erred ) {
            return;
         }
         // NOTE: For now, for a built-in function, a previous parameter
         // is not visible to a following parameter.
      }
      param->object.resolved = true;
      param = param->next;
   }
   func->object.resolved = true;
}

void s_test_func_body( struct semantic* phase, struct func* func ) {
   struct func_user* impl = func->impl;
   if ( ! impl->nested ) {
      s_add_scope( phase );
      struct param* param = func->params;
      while ( param ) {
         if ( param->name ) {
            s_bind_local_name( phase, param->name, ( struct object* ) param );
         }
         param = param->next;
      }
   }
   struct stmt_test test;
   s_init_stmt_test( &test, NULL );
   test.func = func;
   test.manual_scope = true;
   test.labels = &impl->labels;
   if ( ! impl->nested ) {
      phase->topfunc_test = &test;
   }
   struct stmt_test* prev = phase->func_test;
   phase->func_test = &test;
   s_test_block( phase, &test, impl->body );
   phase->func_test = prev;
   impl->returns = test.returns;
   if ( ! impl->nested ) {
      impl->nested_funcs = test.nested_funcs;
      phase->topfunc_test = NULL;
   }
   if ( ! impl->nested ) {
      s_pop_scope( phase );
   }
}

void s_test_script( struct semantic* phase, struct script* script ) {
   test_script_number( phase, script );
   test_script_params( phase, script );
   test_script_body( phase, script );
}

void test_script_number( struct semantic* phase, struct script* script ) {
   if ( script->number ) {
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, true, true, false );
      s_test_expr( phase, &expr, script->number );
      if ( ! script->number->folded ) {
         s_diag( phase, DIAG_POS_ERR, &expr.pos,
            "script number not a constant expression" );
         s_bail( phase );
      }
      if ( script->number->value < SCRIPT_MIN_NUM ||
         script->number->value > SCRIPT_MAX_NUM ) {
         s_diag( phase, DIAG_POS_ERR, &expr.pos,
            "script number not between %d and %d", SCRIPT_MIN_NUM,
            SCRIPT_MAX_NUM );
         s_bail( phase );
      }
      if ( script->number->value == 0 ) {
         s_diag( phase, DIAG_POS_ERR, &expr.pos,
            "script number 0 not between << and >>" );
         s_bail( phase );
      }
   }
}

void test_script_params( struct semantic* phase, struct script* script ) {
   struct param* param = script->params;
   while ( param ) {
      if ( param->name && param->name->object &&
         param->name->object->node.type == NODE_PARAM ) {
         struct str str;
         str_init( &str );
         t_copy_name( param->name, false, &str );
         diag_dup( phase->task, str.value, &param->object.pos, param->name );
         s_bail( phase );
      }
      param->object.resolved = true;
      param = param->next;
   }
}

void test_script_body( struct semantic* phase, struct script* script ) {
   s_add_scope( phase );
   struct param* param = script->params;
   while ( param ) {
      s_bind_local_name( phase, param->name, &param->object );
      param = param->next;
   }
   struct stmt_test test;
   s_init_stmt_test( &test, NULL );
   test.in_script = true;
   test.manual_scope = true;
   test.labels = &script->labels;
   phase->topfunc_test = &test;
   phase->func_test = phase->topfunc_test;
   s_test_stmt( phase, &test, script->body );
   script->nested_funcs = test.nested_funcs;
   phase->topfunc_test = NULL;
   phase->func_test = NULL;
   s_pop_scope( phase );
}

void s_calc_var_size( struct var* var ) {
   // Calculate the size of the variable elements.
   if ( var->dim ) {
      calc_dim_size( var->dim, var->type );
   }
   else {
      // Only calculate the size of the type if it hasn't been already.
      if ( ! var->type->size ) {
         calc_type_size( var->type );
      }
   }
   // Calculate the size of the variable.
   if ( var->dim ) {
      var->size = var->dim->size * var->dim->element_size;
   }
   else {
      var->size = var->type->size;
   }
}

void calc_dim_size( struct dim* dim, struct type* type ) {
   if ( dim->next ) {
      calc_dim_size( dim->next, type );
      dim->element_size = dim->next->size * dim->next->element_size;
   }
   else {
      // Calculate the size of the element type.
      if ( ! type->size ) {
         calc_type_size( type );
      }
      dim->element_size = type->size;
   }
}

void calc_type_size( struct type* type ) {
   int offset = 0;
   struct type_member* member = type->member;
   while ( member ) {
      if ( member->dim ) {
         calc_dim_size( member->dim, member->type );
         if ( member->dim->element_size ) {
            int size = member->dim->size * member->dim->element_size;
            type->size += size;
            member->offset = offset;
            offset += size;
         }
      }
      else if ( ! member->type->primitive ) {
         // Calculate the size of the type if it hasn't been already.
         if ( ! member->type->size ) {
            calc_type_size( member->type );
         }
         if ( member->type->size ) {
            type->size += member->type->size;
            member->offset = offset;
            offset += member->type->size;
         }
      }
      else {
         member->size = member->type->size;
         member->offset = offset;
         offset += member->type->size;
         type->size += member->size;
      }
      member = member->next;
   }
}

void s_calc_var_value_index( struct var* var ) {
   if ( var->initial->multi ) {
      struct value_list list = {
         .head = NULL,
         .tail = NULL
      };
      make_value_list( &list, ( struct multi_value* ) var->initial );
      var->value = list.head;
      struct value_index_alloc alloc = {
         .value = list.head,
         .index = 0
      };
      if ( var->dim ) {
         alloc_value_index( &alloc,
            ( struct multi_value* ) var->initial,
            var->type, var->dim );
      }
      else {
         alloc_value_index_struct( &alloc,
            ( struct multi_value* ) var->initial, var->type );
      }
   }
   else {
      var->value = ( struct value* ) var->initial;
   }
}

void make_value_list( struct value_list* list,
   struct multi_value* multi_value ) {
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( initial->multi ) {
         make_value_list( list, ( struct multi_value* ) initial );
      }
      else {
         struct value* value = ( struct value* ) initial;
         if ( list->head ) {
            list->tail->next = value;
         }
         else {
            list->head = value;
         }
         list->tail = value;
      }
      initial = initial->next;
   }
}

void alloc_value_index( struct value_index_alloc* alloc,
   struct multi_value* multi_value, struct type* type, struct dim* dim ) {
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( initial->multi ) {
         if ( dim->next ) {
            int index = alloc->index;
            alloc_value_index( alloc,
               ( struct multi_value* ) initial, type, dim->next );
            // Skip elements not specified.
            int used = alloc->index - index;
            alloc->index += ( dim->next->size *
               dim->next->element_size ) - used;
         }
         else {
            int index = alloc->index;
            alloc_value_index_struct( alloc,
               ( struct multi_value* ) initial, type );
            // Skip members not specified.
            int used = alloc->index - index;
            alloc->index += type->size - used;
         }
      }
      else {
         alloc->value->index = alloc->index;
         if ( alloc->value->string_initz ) {
            alloc->index += dim->next->size;
         }
         else {
            ++alloc->index;
         }
         alloc->value = alloc->value->next;
      }
      initial = initial->next;
   }
}

void alloc_value_index_struct( struct value_index_alloc* alloc,
   struct multi_value* multi_value, struct type* type ) {
   struct type_member* member = type->member;
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( initial->multi ) {
         if ( member->dim ) {
            int index = alloc->index;
            alloc_value_index( alloc,
               ( struct multi_value* ) initial,
               member->type, member->dim );
            // Skip elements not specified.
            int used = alloc->index - index;
            alloc->index += ( member->dim->size *
               member->dim->element_size ) - used;
         }
         else {
            int index = alloc->index;
            alloc_value_index_struct( alloc,
               ( struct multi_value* ) initial, member->type );
            // Skip members not specified.
            int used = alloc->index - index;
            alloc->index += member->type->size - used;
         }
      }
      else {
         alloc->value->index = alloc->index;
         if ( alloc->value->string_initz ) {
            alloc->index += member->dim->size;
         }
         else {
            ++alloc->index;
         }
         alloc->value = alloc->value->next;
      }
      member = member->next;
      initial = initial->next;
   }
}

void diag_dup( struct task* task, const char* text, struct pos* pos,
   struct name* prev ) {
   t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "duplicate name `%s`", text );
   t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &prev->object->pos,
      "name already used here" );
}

void diag_dup_struct( struct task* task, struct name* name,
   struct pos* pos ) {
   struct str str;
   str_init( &str );
   t_copy_name( name, false, &str );
   t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "duplicate struct `%s`", str.value );
   t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &name->object->pos,
      "struct already found here" );
}

void diag_dup_struct_member( struct task* task, struct name* name,
   struct pos* pos ) {
   struct str str;
   str_init( &str );
   t_copy_name( name, false, &str );
   t_diag( task, DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, pos,
      "duplicate struct member `%s`", str.value );
   t_diag( task, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &name->object->pos,
      "struct member already found here", str.value );
}