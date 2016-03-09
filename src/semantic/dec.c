#include <limits.h>

#include "phase.h"

#define SCRIPT_MIN_NUM 0
#define SCRIPT_MAX_NUM 32767

struct enumeration_test {
   int value;
};

struct initz_test {
   struct dim* dim;
   struct structure* structure;
   struct structure_member* member;
   int spec;
   int count;
   bool constant;
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
static bool test_ref( struct semantic* semantic, struct var* var );
static void test_ref_part( struct semantic* semantic,
   struct var* var, struct ref* ref );
static bool test_spec( struct semantic* semantic, struct var* var );
static void resolve_name_spec( struct semantic* semantic, struct var* var );
static bool test_name( struct semantic* semantic, struct var* var );
static bool test_dim( struct semantic* semantic, struct var* var );
static bool test_initz( struct semantic* semantic, struct var* var );
static bool test_object_initz( struct semantic* semantic, struct var* var );
static bool test_imported_object_initz( struct var* var );
static void init_initz_test( struct initz_test* test, int spec,
   struct dim* dim, struct structure* structure, bool constant );
static bool test_multi_value( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value );
static bool test_multi_value_child( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value,
   struct initial* initial );
static bool test_value( struct semantic* semantic, struct initz_test* test,
   int spec, struct dim* dim, struct value* value );
static bool is_scalar( int spec, struct dim* dim );
static void value_mismatch_diag( struct semantic* semantic,
   struct type_info* type, struct type_info* value_type, struct expr* expr,
   bool array, bool member );
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
static bool same_ref( struct ref* a, struct ref* b );
static bool same_ref_array( struct ref_array* a, struct ref_array* b );
static bool same_ref_func( struct ref_func* a, struct ref_func* b );
static bool compatible_zraw_spec( int spec );
// static void present_ref( struct type_info* type, struct str* string );
static void present_spec( struct type_info* type, struct str* string );

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
   if ( enumeration->name ) {
      if ( semantic->in_localscope ) {
         s_bind_name( semantic, enumeration->name, &enumeration->object );
      }
   }
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
      if ( enumerator->initz->spec != SPEC_ZINT &&
         enumerator->initz->spec != SPEC_ENUM ) {
         s_diag( semantic, DIAG_POS_ERR, &enumerator->object.pos,
            "enumerator initializer of non-integer value" );
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
   if ( var->spec == SPEC_NAME ) {
      resolve_name_spec( semantic, var );
   }
   var->object.resolved = (
      test_ref( semantic, var ) &&
      test_spec( semantic, var ) &&
      test_name( semantic, var ) &&
      test_dim( semantic, var ) &&
      test_initz( semantic, var ) );
}

bool test_ref( struct semantic* semantic, struct var* var ) {
   struct ref* ref = var->ref;
   while ( ref ) {
      test_ref_part( semantic, var, ref );
      ref = ref->next;
   }
   return true;
}

// A reference can be made only to an array, a structure, or a function.
void test_ref_part( struct semantic* semantic,
   struct var* var, struct ref* ref ) {
   if ( ref->type == REF_FUNCTION ) {
      struct type_info return_type;
      s_init_type_info( &return_type, var->spec, ref->next, NULL, NULL, NULL );
      if ( ! s_is_scalar_type( &return_type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &ref->pos,
            "invalid return-type in function reference" );
         s_bail( semantic );
      }
      struct ref_func* part = ( struct ref_func* ) ref;
   }
   else if ( ref->type == REF_VAR ) {
      if ( var->spec != SPEC_STRUCT ) {
         s_diag( semantic, DIAG_POS_ERR, &ref->pos,
            "invalid reference type" );
         s_diag( semantic, DIAG_POS, &ref->pos,
            "reference must be made to an array, a structure, "
            "or a function" );
         s_bail( semantic );
      }
   }
}

bool test_spec( struct semantic* semantic, struct var* var ) {
   // Locate specified type.
   // Test type.
   if ( var->spec == SPEC_STRUCT ) {
      if ( ! var->structure->object.resolved ) {
         return false;
      }
      // An array or a variable with a structure type cannot appear in local
      // storage because there's no standard or efficient way to allocate such
      // a variable.
      if ( var->storage == STORAGE_LOCAL && ! var->ref ) {
         s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
            "variable of struct type in local storage" );
         s_bail( semantic );
      }
   }
   return true;
}

void resolve_name_spec( struct semantic* semantic, struct var* var ) {
   struct object_search search;
   s_init_object_search( &search, var->type_path, false );
   s_find_object( semantic, &search );
   switch ( search.object->node.type ) {
   case NODE_STRUCTURE:
      var->structure =
         ( struct structure* ) search.object;
      var->spec = SPEC_STRUCT;
      break;
   case NODE_ENUMERATION:
      var->enumeration =
         ( struct enumeration* ) search.object;
      var->spec = SPEC_ENUM;
      break;
   default:
      s_diag( semantic, DIAG_POS_ERR, &search.path->pos,
         "`%s` is not a valid type",
         search.path->text );
      s_bail( semantic );
   }
}

bool test_name( struct semantic* semantic, struct var* var ) {
   // Bind name of local variable.
   if ( semantic->in_localscope ) {
      if ( var->name->object != &var->object ) {
         s_bind_name( semantic, var->name, &var->object );
      }
   }
   return true;
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
   struct initz_test test;
   init_initz_test( &test, var->spec, var->dim, var->structure,
      var->is_constant_init );
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
      bool resolved = test_value( semantic, &test, var->spec, var->dim,
         ( struct value* ) var->initial );
      if ( ! resolved ) {
         return false;
      }
   }
   var->initial_has_str = test.has_string;
   return true;
}

void init_initz_test( struct initz_test* test, int spec, struct dim* dim,
   struct structure* structure, bool constant ) {
   test->dim = dim;
   test->structure = structure;
   test->member = ( ! dim && structure ? structure->member : NULL );
   test->spec = spec;
   test->count = 0;
   test->constant = constant;
   test->has_string = false;
}

bool test_multi_value( struct semantic* semantic, struct initz_test* test,
   struct multi_value* multi_value ) {
   if ( ! ( test->dim || test->structure ) ) {
      s_diag( semantic, DIAG_POS_ERR, &multi_value->pos,
         "too many brace initializers" );
      s_bail( semantic );
   }
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( ! initial->tested ) {
         bool resolved = test_multi_value_child( semantic,
            test, multi_value, initial );
         if ( ! resolved ) {
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

bool test_multi_value_child( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value,
   struct initial* initial ) {
   // Make sure there is an object to initialize. 
   bool have_element = ( test->dim &&
      ( ! test->dim->size_node || test->count < test->dim->size ) );
   bool have_member = ( test->member != NULL );
   if ( ! ( have_element || have_member ) ) {
      s_diag( semantic, DIAG_POS_ERR, &multi_value->pos,
         "too many values in brace initializer" );
      s_bail( semantic );
   }
   if ( initial->multi ) {
      struct initz_test nested_test;
      init_initz_test( &nested_test,
         ( test->dim ? test->spec : test->member->spec ),
         ( test->dim ? test->dim->next : test->member->dim ),
         ( test->dim ? test->structure : test->member->structure ),
         test->constant );
      bool resolved = test_multi_value( semantic, &nested_test,
         ( struct multi_value* ) initial );
      if ( nested_test.has_string ) {
         test->has_string = true;
      }
      return resolved;
   }
   else {
      return test_value( semantic, test,
         ( test->dim ? test->spec : test->member->spec ),
         ( test->dim ? test->dim->next : test->member->dim ),
         ( struct value* ) initial );
   }
}

bool test_value( struct semantic* semantic, struct initz_test* test, int spec,
   struct dim* dim, struct value* value ) {
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
   // Only initialize a scalar object or an array of a single dimension
   // (using the string initializer).
   bool scalar = is_scalar( spec, dim );
   bool string_initz_element = ( dim && ! dim->next &&
      value->expr->spec == SPEC_ZSTR );
   if ( ! ( scalar || string_initz_element ) ) {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "missing brace initializer" );
      s_bail( semantic );
   }
   // String initializer.
   if ( string_initz_element ) {
      if ( ! ( spec == SPEC_ZINT || spec == SPEC_ZRAW ) ) {
         s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
            "string initializer specified for a non-integer array" );
         s_bail( semantic );
      }
      struct indexed_string* string = t_lookup_string( semantic->task,
         value->expr->value );
      if ( dim->size_node ) {
         if ( string->length >= dim->size ) {
            s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
               "string initializer too long" );
            s_bail( semantic );
         }
      }
      else {
         dim->size = string->length + 1;
      }
      value->string_initz = true;
   }
   // Scalar object.
   else {
      struct type_info type;
      s_init_type_info( &type, spec, NULL, NULL, NULL, NULL );
      struct type_info value_type;
      s_init_type_info( &value_type, value->expr->spec, NULL, NULL, NULL, NULL );
      if ( ! s_same_type( &type, &value_type ) ) {
         value_mismatch_diag( semantic, &type, &value_type, value->expr,
            ( test->dim != NULL ), ( test->member != NULL ) );
         s_bail( semantic );
      }
      if ( expr.has_string ) {
         test->has_string = true;
      }
   }
   return true;
}

bool is_scalar( int spec, struct dim* dim ) {
   if ( ! dim ) {
      switch ( spec ) {
      case SPEC_ZRAW:
      case SPEC_ZINT:
      case SPEC_ZFIXED:
      case SPEC_ZBOOL:
      case SPEC_ZSTR:
      case SPEC_ENUM:
         return true;
      default:
         break;
      }
   }
   return false;
}

void value_mismatch_diag( struct semantic* semantic, struct type_info* type,
   struct type_info* value_type, struct expr* expr, bool array, bool member ) {
   struct str type_s;
   str_init( &type_s );
   s_present_type( type, &type_s );
   struct str value_type_s;
   str_init( &value_type_s );
   s_present_type( value_type, &value_type_s );
   const char* object =
      array  ? "element" :
      member ? "struct-member" : "variable";
   s_diag( semantic, DIAG_POS_ERR, &expr->pos,
      "initializer/%s type-mismatch", object );
   s_diag( semantic, DIAG_POS, &expr->pos,
      "`%s` initializer, but `%s` %s", value_type_s.value,
      type_s.value, object );
   str_deinit( &type_s );
   str_deinit( &value_type_s );
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
/*
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
*/
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
      script->named_script = ( script->number->spec == SPEC_ZSTR );
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

void s_init_type_info( struct type_info* type, int spec, struct ref* ref,
   struct dim* dim, struct structure* structure,
   struct enumeration* enumeration ) {
   type->ref = ref;
   type->structure = structure;
   type->enumeration = enumeration;
   type->dim = NULL;
   type->spec = spec;
   // An array and a structure decay into a reference.
   if ( dim ) {
      struct ref_array* part = &type->implicit_ref.array;
      part->ref.next = type->ref;
      part->ref.type = REF_ARRAY;
      part->dim_count = 0;
      while ( dim ) {
         ++part->dim_count;
         dim = dim->next;
      }
      type->ref = &part->ref;
   }
   else {
      if ( structure && ! ref ) {
         struct ref* part = &type->implicit_ref.var;
         part->next = NULL;
         part->type = REF_VAR;
         type->ref = part;
      }
   }
}

bool s_same_type( struct type_info* a, struct type_info* b ) {
   // Reference.
   bool same = same_ref( a->ref, b->ref );
   if ( ! same ) {
      return false;
   }
   // Structure.
   if ( a->structure != b->structure ) {
      return false;
   }
   // Enumeration.
   if ( a->enumeration != b->enumeration ) {
      return false;
   }
   // Specifier.
   if ( a->spec == SPEC_ZRAW ) {
      return compatible_zraw_spec( b->spec );
   }
   else if ( b->spec == SPEC_ZRAW ) {
      return compatible_zraw_spec( a->spec );
   }
   else {
      return ( a->spec == b->spec );
   }
}

bool same_ref( struct ref* a, struct ref* b ) {
   while ( a && b ) {
      if ( a->type != b->type ) {
         return false;
      }
      bool same = false;
      switch ( a->type ) {
      case REF_ARRAY:
         same = same_ref_array(
            ( struct ref_array* ) a,
            ( struct ref_array* ) b );
         break;
      case REF_FUNCTION:
         same = same_ref_func(
            ( struct ref_func* ) a,
            ( struct ref_func* ) b );
         break;
      case REF_VAR:
         same = true;
         break;
      default:
         break;
      }
      if ( ! same ) {
         return false;
      }
      a = a->next;
      b = b->next;
   }
   return ( a == NULL && b == NULL );
}

bool same_ref_array( struct ref_array* a, struct ref_array* b ) {
/*
   struct dim* dim_a = a->dim;
   struct dim* dim_b = b->dim;
   while ( dim_a && dim_b &&
      dim_a->size == dim_b->size ) {
      dim_a = dim_a->next;
      dim_b = dim_b->next;
   }
   return ( dim_a == NULL && dim_b == NULL );
*/
   return ( a->dim_count == b->dim_count );
}

bool same_ref_func( struct ref_func* a, struct ref_func* b ) {
   struct param* param_a = a->params;
   struct param* param_b = b->params;
   while ( param_a && param_b &&
      param_a->spec == param_b->spec ) {
      param_a = param_a->next;
      param_b = param_b->next;
   }
   return ( param_a == NULL && param_b == NULL );
}

bool compatible_zraw_spec( int spec ) {
   switch ( spec ) {
   case SPEC_ZRAW:
   case SPEC_ZINT:
   case SPEC_ZFIXED:
   case SPEC_ZBOOL:
   case SPEC_ZSTR:
      return true;
   }
   return false;
}

void s_present_type( struct type_info* type, struct str* string ) {
   if ( type->ref ) {
      switch ( type->ref->type ) {
      case REF_ARRAY:
         str_append( string, "array-reference" );
         break;
      case REF_FUNCTION:
         str_append( string, "function-reference" );
         break;
      default:
         str_append( string, "reference" );
         break;
      }
   }
   else {
      if ( type->ref ) {
         str_append( string, "reference" );
      }
      else {
         str_append( string, "`" );
         present_spec( type, string );
         str_append( string, "`" );
      }
   }
}

/*
void present_ref( struct type_info* type, struct str* string ) {
   struct ref* ref = type->ref;
   while ( ref ) {
      str_append( string, "ref" );
      if ( ref->type == REF_ARRAY ) {
         struct ref_array* part = ( struct ref_array* ) ref;
         for ( int i = 0; i < part->dim_count; ++i ) {
            str_append( string, "[]" );
         }
      }
      str_append( string, " " );
      ref = ref->next;
   }
} */

void present_spec( struct type_info* type, struct str* string ) {
   switch ( type->spec ) {
   case SPEC_ZRAW:
      str_append( string, "zraw" );
      break;
   case SPEC_ZINT:
      str_append( string, "zint" );
      break;
   case SPEC_ZFIXED:
      str_append( string, "zfixed" );
      break;
   case SPEC_ZBOOL:
      str_append( string, "zbool" );
      break;
   case SPEC_ZSTR:
      str_append( string, "zstr" );
      break;
   case SPEC_VOID:
      str_append( string, "void" );
   default:
      break;
   }
}

bool s_is_scalar_type( struct type_info* type ) {
   if ( ! type->dim ) {
      if ( type->ref ) {
         return true;
      }
      else {
         switch ( type->spec ) {
         case SPEC_ZRAW:
         case SPEC_ZINT:
         case SPEC_ZFIXED:
         case SPEC_ZBOOL:
         case SPEC_ZSTR:
         case SPEC_ENUM:
            return true;
         default:
            break;
         }
      }
   }
   return false;
}

bool s_is_value_type( struct type_info* type ) {
   return ( ! type->ref );
}