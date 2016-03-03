#include <limits.h>

#include "phase.h"

#define SCRIPT_MIN_NUM 0
#define SCRIPT_MAX_NUM 32767

struct enumeration_test {
   int value;
};

struct multi_value_test {
   struct dim* dim;
   struct structure* type;
   struct structure_member* member;
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

static void test_enumerator( struct semantic* semantic,
   struct enumeration_test* test, struct enumerator* enumerator );
static void enumerator_mismatch( struct semantic* semantic,
   struct enumerator* enumerator );
static void test_struct_name( struct semantic* semantic, struct structure* type );
static bool test_struct_body( struct semantic* semantic, struct structure* type );
static void test_member( struct semantic* semantic,
   struct structure_member* member );
static bool test_member_spec( struct semantic* semantic,
   struct structure_member* member );
static void test_member_name( struct semantic* semantic,
   struct structure_member* member );
static bool test_member_dim( struct semantic* semantic,
   struct structure_member* member );
static bool test_spec( struct semantic* semantic, struct var* var );
static void test_name( struct semantic* semantic, struct var* var );
static bool test_dim( struct semantic* semantic, struct var* var );
static bool test_initz( struct semantic* semantic, struct var* var );
static bool test_object_initz( struct semantic* semantic, struct var* var );
static bool test_imported_object_initz( struct var* var );
static void test_init( struct semantic* semantic, struct var*, bool, bool* );
static void init_multi_value_test( struct multi_value_test*, struct dim*,
   struct structure*, bool constant, bool nested );
static bool test_multi_value( struct semantic* semantic, struct multi_value_test*,
   struct multi_value* multi_value );
static bool test_multi_value_child( struct semantic* semantic,
   struct multi_value_test* test, struct multi_value* multi_value,
   struct initial* initial );
static bool test_value( struct semantic* semantic, struct multi_value_test* test,
   struct dim* dim, struct structure* type, struct value* value );
static void calc_dim_size( struct dim*, struct structure* );
static bool test_func_paramlist( struct semantic* semantic, struct func* func );
static bool test_func_param( struct semantic* semantic,
   struct func* func, struct param* param );
static void default_value_mismatch( struct semantic* semantic,
   struct func* func, struct param* param, struct expr_test* expr );
static int get_param_number( struct func* func, struct param* target );
static int calc_size( struct dim* dim, struct structure* structure );
static void calc_struct_size( struct structure* structure );
static void make_value_list( struct value_list*, struct multi_value* );
static void alloc_value_index( struct value_index_alloc*, struct multi_value*,
   struct structure*, struct dim* );
static void alloc_value_index_struct( struct value_index_alloc*,
   struct multi_value*, struct structure* );
static void test_script_number( struct semantic* semantic, struct script* script );
static void test_script_body( struct semantic* semantic, struct script* script );

void s_test_constant( struct semantic* semantic, struct constant* constant ) {
   // Test name. Only applies in a local scope.
   if ( semantic->in_localscope ) {
      if ( constant->name->object != &constant->object ) {
         s_bind_name( semantic, constant->name, &constant->object );
      }
   }
   // Test expression.
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, false );
   s_test_expr( semantic, &expr, constant->value_node );
   if ( ! expr.undef_erred ) {
      if ( constant->value_node->folded ) {
         constant->value = constant->value_node->value;
         constant->object.resolved = true;
      }
      else {
         s_diag( semantic, DIAG_POS_ERR, &constant->value_node->pos,
            "expression not constant" );
         s_bail( semantic );
      }
   }
}

void s_test_enumeration( struct semantic* semantic,
   struct enumeration* enumeration ) {
   struct enumeration_test test = { 0 };
   // Skip previously resolved enumerators.
   struct enumerator* enumerator = enumeration->head;
   while ( enumerator && enumerator->object.resolved ) {
      test.value = enumerator->value;
      enumerator = enumerator->next;
   }
   // Test unresolved enumerators.
   while ( enumerator ) {
      test_enumerator( semantic, &test, enumerator );
      if ( ! enumerator->object.resolved ) {
         return;
      }
      enumerator = enumerator->next;
   }
   enumeration->object.resolved = true;
}

void test_enumerator( struct semantic* semantic,
   struct enumeration_test* test, struct enumerator* enumerator ) {
   // Test name.
   if ( semantic->in_localscope ) {
      s_bind_name( semantic, enumerator->name, &enumerator->object );
   }
   // Test initializer.
   if ( enumerator->initz ) {
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, true, false );
      s_test_expr( semantic, &expr, enumerator->initz );
      if ( expr.undef_erred ) {
         return;
      }
      if ( ! enumerator->initz->folded ) {
         s_diag( semantic, DIAG_POS_ERR, &enumerator->initz->pos,
            "enumerator expression not constant" );
         s_bail( semantic );
      }
      // The type of an enumerator is `zint`. Maybe, sometime later, we'll
      // allow the user to specify the type of an enumerator value.
      if ( enumerator->initz->spec != SPEC_ZINT ) {
         enumerator_mismatch( semantic, enumerator );
         s_bail( semantic );
      }
      test->value = enumerator->initz->value;
   }
   else {
      // Check for integer overflow.
      if ( test->value == INT_MAX ) {
         s_diag( semantic, DIAG_POS_ERR, &enumerator->object.pos,
            "enumerator value growing beyond the maximum" );
         s_diag( semantic, DIAG_POS, &enumerator->object.pos,
            "maximum automatically-generated value is %d", INT_MAX );
         s_bail( semantic );
      }
      else {
         ++test->value;
      }
   }
   enumerator->value = test->value;
   enumerator->object.resolved = true;
}

void enumerator_mismatch( struct semantic* semantic,
   struct enumerator* enumerator ) {
   struct str type;
   str_init( &type );
   s_present_spec( enumerator->initz->spec, &type );
   struct str required_type;
   str_init( &required_type );
   s_present_spec( SPEC_ZINT, &required_type );
   s_diag( semantic, DIAG_POS_ERR, &enumerator->initz->pos,
      "enumerator-initializer type mismatch" );
   s_diag( semantic, DIAG_POS, &enumerator->initz->pos,
      "`%s` enumerator-initializer, but needs to be `%s`", type.value,
      required_type.value );
   str_deinit( &type );
   str_deinit( &required_type );
}

void s_test_struct( struct semantic* semantic, struct structure* type ) {
   test_struct_name( semantic, type );
   bool resolved = test_struct_body( semantic, type );
   type->object.resolved = resolved;
}

void test_struct_name( struct semantic* semantic, struct structure* type ) {
   if ( type->name->object != &type->object ) {
      s_bind_name( semantic, type->name, &type->object );
   }
}

bool test_struct_body( struct semantic* semantic, struct structure* type ) {
   struct structure_member* member = type->member;
   while ( member ) {
      if ( ! member->object.resolved ) {
         test_member( semantic, member );
         if ( ! member->object.resolved ) {
            return false;
         }
      }
      member = member->next;
   }
   return true;
}

void test_member( struct semantic* semantic, struct structure_member* member ) {
   if ( test_member_spec( semantic, member ) ) {
      test_member_name( semantic, member );
      bool resolved = test_member_dim( semantic, member );
      member->object.resolved = resolved;
   }
}

bool test_member_spec( struct semantic* semantic,
   struct structure_member* member ) {
   if ( member->type_path ) {
      if ( ! member->structure ) {
         struct object_search search;
         s_init_object_search( &search, member->type_path, true );
         s_find_object( semantic, &search );
         member->structure = search.struct_object;
      }
      if ( ! member->structure->object.resolved ) {
         if ( semantic->trigger_err ) {
            struct path* path = member->type_path;
            while ( path->next ) {
               path = path->next;
            }
            s_diag( semantic, DIAG_POS_ERR, &path->pos,
               "struct `%s` undefined", path->text );
            s_bail( semantic );
         }
         return false;
      }
   }
   return true;
}

void test_member_name( struct semantic* semantic,
   struct structure_member* member ) {
   if ( member->name->object != &member->object ) {
     s_bind_name( semantic, member->name, &member->object );
   }
}

bool test_member_dim( struct semantic* semantic,
   struct structure_member* member ) {
   if ( ! member->dim ) {
      return true;
   }
   struct dim* dim = member->dim;
   while ( dim && dim->size ) {
      dim = dim->next;
   }
   while ( dim ) {
      // Dimension with implicit size not allowed in struct.
      if ( ! dim->size_node ) {
         s_diag( semantic, DIAG_POS_ERR, &dim->pos,
            "dimension of implicit size in struct member" );
         s_bail( semantic );
      }
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, true, false );
      s_test_expr( semantic, &expr, dim->size_node );
      if ( expr.undef_erred ) {
         return false;
      }
      if ( ! dim->size_node->folded ) {
         s_diag( semantic, DIAG_POS_ERR, &dim->size_node->pos,
         "array size not a constant expression" );
         s_bail( semantic );
      }
      if ( dim->size_node->value <= 0 ) {
         s_diag( semantic, DIAG_POS_ERR, &dim->size_node->pos,
            "array size must be greater than 0" );
         s_bail( semantic );
      }
      dim->size = dim->size_node->value;
      dim = dim->next;
   }
   return true;
}

void s_test_var( struct semantic* semantic, struct var* var ) {
   if ( test_spec( semantic, var ) ) {
      test_name( semantic, var );
      if ( test_dim( semantic, var ) ) {
         var->object.resolved = test_initz( semantic, var );
      }
   }
}

bool test_spec( struct semantic* semantic, struct var* var ) {
   bool resolved = false;
   if ( var->type_path ) {
      if ( ! var->structure ) {
         struct object_search search;
         s_init_object_search( &search, var->type_path, true );
         s_find_object( semantic, &search );
         var->structure = search.struct_object;
      }
      if ( ! var->structure->object.resolved ) {
         return false;
      }
      // An array or a variable with a structure type cannot appear in local
      // storage because there's no standard or efficient way to allocate such
      // a variable.
      if ( var->storage == STORAGE_LOCAL ) {
         s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
            "variable of struct type in local storage" );
         s_bail( semantic );
      }
   }
   return true;
}

void test_name( struct semantic* semantic, struct var* var ) {
   // Bind name of local variable.
   if ( semantic->in_localscope ) {
      if ( var->name->object != &var->object ) {
         s_bind_name( semantic, var->name, &var->object );
      }
   }
}

bool test_dim( struct semantic* semantic, struct var* var ) {
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
         s_init_expr_test( &expr, NULL, NULL, true, false );
         s_test_expr( semantic, &expr, dim->size_node );
         if ( expr.undef_erred ) {
            return false;
         }
         if ( ! dim->size_node->folded ) {
            s_diag( semantic, DIAG_POS_ERR, &dim->size_node->pos,
               "dimension size not a constant expression" );
            s_bail( semantic );
         }
         if ( dim->size_node->value <= 0 ) {
            s_diag( semantic, DIAG_POS_ERR, &dim->size_node->pos,
               "dimension size less than or equal to 0" );
            s_bail( semantic );
         }
         dim->size = dim->size_node->value;
      }
      else {
         // Only the first dimension can have an implicit size.
         if ( dim != var->dim ) {
            s_diag( semantic, DIAG_POS_ERR, &dim->pos,
               "implicit size in subsequent dimension" );
            s_bail( semantic );
         }
      }
      dim = dim->next;
   }
   if ( var->storage == STORAGE_LOCAL ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "array in local storage" );
      s_bail( semantic );
   }
   return true;
}

bool test_initz( struct semantic* semantic, struct var* var ) {
   if ( var->initial ) {
      if ( var->imported ) {
         return test_imported_object_initz( var );
      }
      else {
         return test_object_initz( semantic, var );
      }
   }
   return true;
}

bool test_object_initz( struct semantic* semantic, struct var* var ) {
   struct multi_value_test test;
   init_multi_value_test( &test, var->dim, var->structure, var->is_constant_init,
      false );
   if ( var->initial->multi ) {
      if ( ! ( var->dim || var->structure ) ) {
         struct multi_value* multi_value =
            ( struct multi_value* ) var->initial;
         s_diag( semantic, DIAG_POS_ERR, &multi_value->pos,
            "using brace initializer on scalar variable" );
         s_bail( semantic );
      }
      bool resolved = test_multi_value( semantic, &test,
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
      bool resolved = test_value( semantic, &test, var->dim, var->structure,
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
   struct structure* type, bool constant, bool nested ) {
   test->dim = dim;
   test->type = type;
   test->member = ( ! dim ? type->member : NULL ); 
   test->count = 0;
   test->constant = constant;
   test->nested = nested;
   test->has_string = false;
}

bool test_multi_value( struct semantic* semantic, struct multi_value_test* test,
   struct multi_value* multi_value ) {
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( ! initial->tested ) {
         if ( ! test_multi_value_child( semantic, test, multi_value, initial ) ) {
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

bool test_multi_value_child( struct semantic* semantic, struct multi_value_test* test,
   struct multi_value* multi_value, struct initial* initial ) {
   bool capacity = ( ( test->dim && ( ! test->dim->size_node ||
      test->count < test->dim->size ) ) || test->member );
   if ( ! capacity ) {
      s_diag( semantic, DIAG_POS_ERR, &multi_value->pos,
         "too many values in brace initializer" );
      s_bail( semantic );
   }
   if ( initial->multi ) {
      // There needs to be an element or member to initialize.
      bool deeper = ( ( test->dim && ( test->dim->next ||
         test->type ) ) || ( test->member &&
         ( test->member->dim || test->member->structure ) ) );
      if ( ! deeper ) {
         s_diag( semantic, DIAG_POS_ERR, &multi_value->pos,
            "too many brace initializers" );
         s_bail( semantic );
      }
      struct multi_value_test nested;
      init_multi_value_test( &nested,
         ( test->dim ? test->dim->next : test->member->dim ),
         ( test->dim ? test->type : test->member->structure ),
         test->constant, true );
      bool resolved = test_multi_value( semantic, &nested,
         ( struct multi_value* ) initial );
      if ( nested.has_string ) {
         test->has_string = true;
      }
      return resolved;
   }
   else {
      return test_value( semantic, test,
         ( test->dim ? test->dim->next : test->member->dim ),
         ( test->dim ? test->type : test->member->structure ),
         ( struct value* ) initial );
   }
}

bool test_value( struct semantic* semantic, struct multi_value_test* test,
   struct dim* dim, struct structure* type, struct value* value ) {
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, false );
   s_test_expr( semantic, &expr, value->expr );
   if ( expr.undef_erred ) {
      return false;
   }
   if ( test->constant && ! value->expr->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "non-constant initializer" );
      s_bail( semantic );
   }
   // Only initialize a primitive element or an array of a single dimension--
   // using the string initializer.
   if ( ! ( ! dim && ! type ) && ! ( dim && ! dim->next &&
      value->expr->root->type == NODE_INDEXED_STRING_USAGE && ! type ) ) {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "missing %sbrace initializer", test->nested ? "another " : "" );
      s_bail( semantic );
   }
   // String initializer.
   if ( dim ) {
      // Even though it doesn't matter what primitive type is specified, for
      // readability purposes, restrict the string initializer to an array of
      // `int` type.
      if ( type != semantic->task->type_int ) {
         s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
            "string initializer specified for a non-int array" );
         s_bail( semantic );
      }
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) value->expr->root;
      if ( dim->size_node ) {
         if ( usage->string->length >= dim->size ) {
            s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
               "string initializer too long" );
            s_bail( semantic );
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

void s_test_local_var( struct semantic* semantic, struct var* var ) {
   s_test_var( semantic, var );
   s_calc_var_size( var );
   if ( var->initial ) {
      s_calc_var_value_index( var );
   }
}

void s_test_func( struct semantic* semantic, struct func* func ) {
   // Test name of nested function.
   bool in_localscope = semantic->in_localscope;
   if ( in_localscope ) {
      if ( func->name->object != &func->object ) {
         s_bind_name( semantic, func->name, &func->object );
      }
   }
   s_add_scope( semantic );
   if ( test_func_paramlist( semantic, func ) ) {
      func->object.resolved = true;
      // Test body of a nested user-function.
      if ( in_localscope && func->type == FUNC_USER ) {
         struct func_user* impl = func->impl;
         impl->next_nested = semantic->topfunc_test->nested_funcs;
         semantic->topfunc_test->nested_funcs = func;
         s_test_func_body( semantic, func );
      }
   }
   s_pop_scope( semantic );
}

bool test_func_paramlist( struct semantic* semantic, struct func* func ) {
   struct param* param = func->params;
   while ( param && param->object.resolved ) {
      param = param->next;
   }
   while ( param ) {
      if ( ! test_func_param( semantic, func, param ) ) {
         return false;
      }
      param->object.resolved = true;
      param = param->next;
   }
   // Action-specials can have up to 5 arguments.
   enum { MAX_ASPEC_PARAMS = 5 };
   if ( func->type == FUNC_ASPEC && func->max_param > MAX_ASPEC_PARAMS ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "action-special has more than %d parameters", MAX_ASPEC_PARAMS );
      s_bail( semantic );
   }
   return true;
}

bool test_func_param( struct semantic* semantic,
   struct func* func, struct param* param ) {
   if ( param->default_value ) {
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, true, false );
      s_test_expr( semantic, &expr, param->default_value );
      if ( expr.undef_erred ) {
         return false;
      }
      if ( param->spec != SPEC_ZRAW &&
         param->default_value->spec != SPEC_ZRAW ) {
         if ( param->default_value->spec != param->spec ) {
            default_value_mismatch( semantic, func, param, &expr );
            s_bail( semantic );
         }
      }
   }
   // Any previous parameter is visible inside the expression of a default
   // parameter. At this time, this only works for user-functions.
   if ( param->name && param->name->object != &param->object ) {
      s_bind_name( semantic, param->name, &param->object );
   }
   return true;
}

void default_value_mismatch( struct semantic* semantic, struct func* func,
   struct param* param, struct expr_test* expr ) {
   struct str type;
   str_init( &type );
   s_present_spec( param->default_value->spec, &type );
   struct str param_type;
   str_init( &param_type );
   s_present_spec( param->spec, &param_type );
   s_diag( semantic, DIAG_POS_ERR, &param->default_value->pos,
      "default-value/parameter type mismatch (in parameter %d)",
      get_param_number( func, param ) );
   s_diag( semantic, DIAG_POS, &param->default_value->pos,
      "`%s` default-value, but `%s` parameter", type.value,
      param_type.value );
   str_deinit( &type );
   str_deinit( &param_type );
}

int get_param_number( struct func* func, struct param* target ) {
   // Start at the second parameter when in a format-function.
   int number = ( func->type == FUNC_FORMAT ) ? 2 : 1;
   struct param* param = func->params;
   while ( param != target ) {
      param = param->next;
      ++number;
   }
   return number;
}

void s_test_func_body( struct semantic* semantic, struct func* func ) {
   struct func_user* impl = func->impl;
   if ( ! impl->nested ) {
      s_add_scope( semantic );
      struct param* param = func->params;
      while ( param ) {
         if ( param->name ) {
            s_bind_name( semantic, param->name, ( struct object* ) param );
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
      semantic->topfunc_test = &test;
   }
   struct stmt_test* prev = semantic->func_test;
   semantic->func_test = &test;
   s_test_block( semantic, &test, impl->body );
   semantic->func_test = prev;
   impl->returns = test.returns;
   if ( ! impl->nested ) {
      impl->nested_funcs = test.nested_funcs;
      semantic->topfunc_test = NULL;
   }
   if ( ! impl->nested ) {
      s_pop_scope( semantic );
   }
}

void s_test_script( struct semantic* semantic, struct script* script ) {
   test_script_number( semantic, script );
   test_script_body( semantic, script );
}

void test_script_number( struct semantic* semantic, struct script* script ) {
   if ( script->number ) {
      struct expr_test expr;
      s_init_expr_test( &expr, NULL, NULL, true, false );
      s_test_expr( semantic, &expr, script->number );
      if ( ! script->number->folded ) {
         s_diag( semantic, DIAG_POS_ERR, &script->number->pos,
            "script number not a constant expression" );
         s_bail( semantic );
      }
      script->named_script = ( script->number->structure ==
         semantic->task->type_str );
      if ( ! script->named_script ) {
         if ( script->number->value < SCRIPT_MIN_NUM ||
            script->number->value > SCRIPT_MAX_NUM ) {
            s_diag( semantic, DIAG_POS_ERR, &script->number->pos,
               "script number not between %d and %d", SCRIPT_MIN_NUM,
               SCRIPT_MAX_NUM );
            s_bail( semantic );
         }
         if ( script->number->value == 0 ) {
            s_diag( semantic, DIAG_POS_ERR, &script->number->pos,
               "script number 0 not between `<<` and `>>`" );
            s_bail( semantic );
         }
      }
   }
}

void test_script_body( struct semantic* semantic, struct script* script ) {
   s_add_scope( semantic );
   struct param* param = script->params;
   while ( param ) {
      if ( param->name ) {
         s_bind_name( semantic, param->name, &param->object );
      }
      param->object.resolved = true;
      param = param->next;
   }
   struct stmt_test test;
   s_init_stmt_test( &test, NULL );
   test.in_script = true;
   test.manual_scope = true;
   test.labels = &script->labels;
   semantic->topfunc_test = &test;
   semantic->func_test = semantic->topfunc_test;
   s_test_stmt( semantic, &test, script->body );
   script->nested_funcs = test.nested_funcs;
   semantic->topfunc_test = NULL;
   semantic->func_test = NULL;
   s_pop_scope( semantic );
}

void s_calc_var_size( struct var* var ) {
   var->size = calc_size( var->dim, var->structure );
}

int calc_size( struct dim* dim, struct structure* structure ) {
   // Array.
   if ( dim ) {
      dim->element_size = calc_size( dim->next, structure );
      return ( dim->size * dim->element_size );
   }
   // Array element.
   else {
      if ( structure ) {
         if ( structure->size == 0 ) {
            calc_struct_size( structure );
         }
         return structure->size;
      }
      else {
         // TODO: Calculate size of references.
         enum { PRIMITIVE_SIZE = 1 };
         return PRIMITIVE_SIZE;
      }
   }
}

void calc_struct_size( struct structure* structure ) {
   struct structure_member* member = structure->member;
   while ( member ) {
      member->offset = structure->size;
      member->size = calc_size( member->dim, member->structure );
      structure->size += member->size;
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
            var->structure, var->dim );
      }
      else {
         alloc_value_index_struct( &alloc,
            ( struct multi_value* ) var->initial, var->structure );
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
   struct multi_value* multi_value, struct structure* type, struct dim* dim ) {
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
   struct multi_value* multi_value, struct structure* type ) {
   struct structure_member* member = type->member;
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( initial->multi ) {
         if ( member->dim ) {
            int index = alloc->index;
            alloc_value_index( alloc,
               ( struct multi_value* ) initial,
               member->structure, member->dim );
            // Skip elements not specified.
            int used = alloc->index - index;
            alloc->index += ( member->dim->size *
               member->dim->element_size ) - used;
         }
         else {
            int index = alloc->index;
            alloc_value_index_struct( alloc,
               ( struct multi_value* ) initial, member->structure );
            // Skip members not specified.
            int used = alloc->index - index;
            alloc->index += member->structure->size - used;
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