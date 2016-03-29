#include <string.h>
#include <limits.h>

#include "phase.h"

#define SCRIPT_MIN_NUM 0
#define SCRIPT_MAX_NUM 32767

struct enumeration_test {
   int value;
};

struct var_test {
   struct structure* structure;
   struct enumeration* enumeration;
   struct path* path;
   int spec;
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
static void init_var_test( struct var_test* test, struct path* path );
static void resolve_name_spec( struct semantic* semantic,
   struct var_test* test );
static bool test_name( struct semantic* semantic, struct var* var );
static bool test_dim( struct semantic* semantic, struct var* var );
static bool test_initz( struct semantic* semantic, struct var* var );
static bool test_object_initz( struct semantic* semantic, struct var* var );
static bool test_imported_object_initz( struct var* var );
static void test_auto_var( struct semantic* semantic, struct var* var );
static void assign_inferred_type( struct var* var, struct type_info* type );
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
static int calc_size( struct dim* dim, struct structure* structure,
   struct ref* ref );
static void calc_struct_size( struct structure* structure );
static void make_value_list( struct value_list*, struct multi_value* );
static void alloc_value_index( struct value_index_alloc*, struct multi_value*,
   struct structure*, struct dim* );
static void alloc_value_index_struct( struct value_index_alloc*,
   struct multi_value*, struct structure* );
static void test_script_number( struct semantic* semantic, struct script* script );
static void test_script_body( struct semantic* semantic, struct script* script );
static bool same_ref_implicit( struct ref* a, struct type_info* b );
static bool same_ref( struct ref* a, struct ref* b );
static bool same_ref_array( struct ref_array* a, struct ref_array* b );
static bool same_ref_func( struct ref_func* a, struct ref_func* b );
static bool compatible_zraw_spec( int spec );
static void present_ref( struct ref* ref, struct str* string );
static void present_spec( int spec, struct str* string );
static bool is_str_value_type( struct type_info* type );
static bool is_array_ref_type( struct type_info* type );
static void subscript_array_type( struct type_info* type,
   struct type_info* element_type );
static void take_type_snapshot( struct type_info* type,
   struct type_snapshot* snapshot );
static struct ref* dup_ref( struct ref* ref );

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
   if ( resolved ) {
      calc_struct_size( type );
      type->object.resolved = true;
   }
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
      if ( member->spec == SPEC_NAME ) {
         struct var_test test;
         init_var_test( &test, member->type_path );
         resolve_name_spec( semantic, &test );
         member->structure = test.structure;
         member->enumeration = test.enumeration;
         member->spec = test.spec;
      }
   if ( test_member_spec( semantic, member ) ) {
      test_member_name( semantic, member );
      bool resolved = test_member_dim( semantic, member );
      member->object.resolved = resolved;
   }
}

bool test_member_spec( struct semantic* semantic,
   struct structure_member* member ) {
/*
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
*/
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
   if ( var->spec == SPEC_AUTO ) {
      test_auto_var( semantic, var );
   }
   else {
      if ( var->spec == SPEC_NAME ) {
         struct var_test test;
         init_var_test( &test, var->type_path );
         resolve_name_spec( semantic, &test );
         var->structure = test.structure;
         var->enumeration = test.enumeration;
         var->spec = test.spec;
      }
      var->object.resolved = (
         test_ref( semantic, var ) &&
         test_spec( semantic, var ) &&
         test_name( semantic, var ) &&
         test_dim( semantic, var ) &&
         test_initz( semantic, var ) );
   }
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
      s_init_type_info( &return_type, var->spec, ref->next,
         NULL, NULL, NULL, NULL );
      if ( ! s_is_scalar_type( &return_type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &ref->pos,
            "invalid return-type in function reference" );
         s_bail( semantic );
      }
      struct ref_func* part = ( struct ref_func* ) ref;
   }
   else if ( ref->type == REF_STRUCTURE ) {
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

void init_var_test( struct var_test* test, struct path* path ) {
   test->structure = NULL;
   test->enumeration = NULL;
   test->path = path;
   test->spec = SPEC_NONE;
}

void resolve_name_spec( struct semantic* semantic, struct var_test* test ) {
   struct object* object = s_follow_path( semantic, test->path );
   switch ( object->node.type ) {
      struct path* path;
   case NODE_STRUCTURE:
      test->structure =
         ( struct structure* ) object;
      test->spec = SPEC_STRUCT;
      break;
   case NODE_ENUMERATION:
      test->enumeration =
         ( struct enumeration* ) object;
      test->spec = SPEC_ENUM;
      break;
   default: {
      path = s_last_path_part( test->path );
      s_diag( semantic, DIAG_POS_ERR, &path->pos,
         "object not a valid type" );
      s_bail( semantic ); }
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
      s_init_type_info( &type, spec, NULL, NULL, NULL, NULL, NULL );
      struct type_info value_type;
      s_init_type_info( &value_type, value->expr->spec,
         NULL, NULL, NULL, NULL, NULL );
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

void test_auto_var( struct semantic* semantic, struct var* var ) {
   // Infer type from initializer.
   if ( ! var->initial ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "auto-declaration missing initializer" );
      s_bail( semantic );
   }
   struct value* value = ( struct value* ) var->initial;
   struct type_info type;
   struct expr_test expr;
   s_init_expr_test( &expr, NULL, NULL, true, false );
   s_test_expr_type( semantic, &expr, &type, value->expr );
   if ( var->is_constant_init && ! value->expr->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "non-constant initializer" );
      s_bail( semantic );
   }
   // For now, keep an auto-declaration in local scope.
   if ( ! semantic->in_localscope ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "auto-declaration in non-local scope" );
      s_bail( semantic );
   }
   assign_inferred_type( var, &type );
   s_bind_name( semantic, var->name, &var->object );
   var->object.resolved = true;
}

void assign_inferred_type( struct var* var, struct type_info* type ) {
   struct type_snapshot snapshot;
   take_type_snapshot( type, &snapshot );
   var->ref = snapshot.ref;
   var->structure = snapshot.structure;
   var->enumeration = snapshot.enumeration;
   var->dim = snapshot.dim;
   var->spec = snapshot.spec;
}

void s_test_local_var( struct semantic* semantic, struct var* var ) {
   s_test_var( semantic, var );
   s_calc_var_size( var );
   if ( var->initial ) {
      s_calc_var_value_index( var );
   }
}

void s_test_foreach_var( struct semantic* semantic,
   struct type_info* collection_type, struct var* var ) {
   bool resolved = false;
   if ( var->spec == SPEC_AUTO ) {
      assign_inferred_type( var, collection_type );
      resolved = true;
   }
   else {
      if ( var->spec == SPEC_NAME ) {
         struct var_test test;
         init_var_test( &test, var->type_path );
         resolve_name_spec( semantic, &test );
         var->structure = test.structure;
         var->enumeration = test.enumeration;
         var->spec = test.spec;
      }
      resolved =
         test_ref( semantic, var ) &&
         test_spec( semantic, var );
   }
   var->object.resolved = resolved;
   s_calc_var_size( var );
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
   var->size = calc_size( var->dim, var->structure, var->ref );
}

int calc_size( struct dim* dim, struct structure* structure,
   struct ref* ref ) {
   enum { PRIMITIVE_SIZE = 1 };
   // Array.
   if ( dim ) {
      dim->element_size = calc_size( dim->next, structure, ref );
      return ( dim->size * dim->element_size );
   }
   // Reference.
   else if ( ref ) {
      // Reference to an array is a fat pointer. It consists of two primitive
      // data units. The first data unit stores an offset to the first element
      // of the array. The second data unit stores an offset to the dimension
      // information of the array.
      if ( ref->type == REF_ARRAY ) {
         return PRIMITIVE_SIZE + PRIMITIVE_SIZE;
      }
      else {
         return PRIMITIVE_SIZE;
      }
   }
   // Structure.
   else if ( structure ) {
      return structure->size;
   }
   // Primitive.
   else {
      return PRIMITIVE_SIZE;
   }
}

void calc_struct_size( struct structure* structure ) {
   struct structure_member* member = structure->member;
   while ( member ) {
      member->offset = structure->size;
      member->size = calc_size( member->dim, member->structure, member->ref );
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
   struct enumeration* enumeration, struct func* func ) {
   type->ref = ref;
   type->structure = structure;
   type->enumeration = enumeration;
   type->dim = dim;
   type->func = func;
   type->spec = spec;
   type->implicit_ref = false;
   if ( dim ) {
      struct ref_array* array = &type->implicit_ref_part.array;
      array->ref.next = ref;
      array->ref.type = REF_ARRAY;
      array->dim_count = 0;
      while ( dim ) {
         ++array->dim_count;
         dim = dim->next;
      }
      type->ref = &array->ref;
   }
   else if ( structure ) {
      struct ref_struct* structure = &type->implicit_ref_part.structure;
      structure->ref.next = ref;
      structure->ref.type = REF_STRUCTURE;
      type->ref = &structure->ref;
   }
}

void s_init_type_info_func( struct type_info* type, struct func* func ) {
   struct ref_func* part = &type->implicit_ref_part.func;
   part->ref.next = func->ref;
   part->ref.type = REF_FUNCTION;
   part->params = func->params;
   part->min_param = func->min_param;
   part->max_param = func->max_param;
   part->msgbuild = false;
   if ( func->type == FUNC_USER ) {
      struct func_user* impl = func->impl;
      part->msgbuild = impl->msgbuild;
   }
   s_init_type_info( type, func->return_spec, &part->ref, NULL, NULL, NULL,
      func );
}

void s_init_type_info_scalar( struct type_info* type, int spec ) {
   s_init_type_info( type, spec, NULL, NULL, NULL, NULL, NULL );
}

bool s_same_type( struct type_info* a, struct type_info* b ) {
   // Reference.
   bool same = false;
   if ( a->ref && ! b->ref ) {
      same = same_ref_implicit( a->ref, b );
   }
   else if ( ! a->ref && b->ref ) {
      same = same_ref_implicit( b->ref, a );
   }
   else {
      same = same_ref( a->ref, b->ref );
   }
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

bool same_ref_implicit( struct ref* a, struct type_info* b ) {
      return false;
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
      case REF_STRUCTURE:
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
   return
      ( param_a == NULL && param_b == NULL ) &&
      ( a->msgbuild == b->msgbuild );
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
/*
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
   } */
   // Specifier.
   if ( type->enumeration ) {
      str_append( string, "enum" );
      if ( type->enumeration->name ) {
         struct str name;
         str_init( &name );
         t_copy_name( type->enumeration->name, false, &name );
         str_append( string, " " );
         str_append( string, name.value );
         str_deinit( &name );
      }
   }
   else {
      present_spec( type->spec, string );
   }
   // Reference.
   present_ref( type->ref, string );
}

void present_ref( struct ref* ref, struct str* string ) {
   if ( ref ) {
      present_ref( ref->next, string );
      if ( ref->next ) {
         str_append( string, "&" );
      }
      if ( ref->type == REF_ARRAY ) {
         struct ref_array* part = ( struct ref_array* ) ref;
         for ( int i = 0; i < part->dim_count; ++i ) {
            str_append( string, "[]" );
         }
      }
      else if ( ref->type == REF_STRUCTURE ) {

      }
      else if ( ref->type == REF_FUNCTION ) {
         struct ref_func* func = ( struct ref_func* ) ref;
         str_append( string, " " );
         str_append( string, "function" );
         str_append( string, "(" );
         struct param* param = func->params;
         while ( param ) {
            present_spec( param->spec, string );
            param = param->next;
            if ( param ) {
               str_append( string, "," );
               str_append( string, " " );
            }
         }
         str_append( string, ")" );
         if ( func->msgbuild ) {
            str_append( string, " " );
            str_append( string, "msgbuild" );
         }
      }
      else {
         UNREACHABLE()
      }
   }
}

void present_spec( int spec, struct str* string ) {
   switch ( spec ) {
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

bool s_is_ref_type( struct type_info* type ) {
   return ( type->ref || type->dim || type->structure );
}

bool s_is_value_type( struct type_info* type ) {
   return ( ! s_is_ref_type( type ) );
}

// Initializes @type with the type of the key used by the iterable type. Right
// now, only an integer key is possible.
void s_iterate_type( struct type_info* type, struct type_iter* iter ) {
   if ( is_str_value_type( type ) ) {
      s_init_type_info_scalar( &iter->key, SPEC_ZINT );
      s_init_type_info_scalar( &iter->value, SPEC_ZINT );
      iter->available = true;
   }
   else if ( is_array_ref_type( type ) ) {
      s_init_type_info_scalar( &iter->key, SPEC_ZINT );
      subscript_array_type( type, &iter->value );
      iter->available = true;
   }
}

inline bool is_str_value_type( struct type_info* type ) {
   return ( s_is_value_type( type ) && type->spec == SPEC_ZSTR );
}

inline bool is_array_ref_type( struct type_info* type ) {
   return ( type->dim || ( type->ref && type->ref->type == REF_ARRAY ) );
}

void subscript_array_type( struct type_info* type,
   struct type_info* element_type ) {
   s_init_type_info( element_type, type->spec, NULL, NULL, type->structure,
      type->enumeration, NULL );
   if ( type->dim ) {
      element_type->dim = type->dim->next;
      element_type->ref = type->ref;
   }
   else if ( type->ref && type->ref->type == REF_ARRAY ) {
      struct ref_array* part = ( struct ref_array* ) type->ref;
      if ( part->dim_count > 1 ) {
         struct ref_array* implicit_part =
            &element_type->implicit_ref_part.array;
         implicit_part->ref.next = part->ref.next;
         implicit_part->ref.type = REF_ARRAY;
         implicit_part->dim_count = part->dim_count - 1;
         element_type->ref = &implicit_part->ref;
         element_type->implicit_ref = true;
      }
   }
}

void take_type_snapshot( struct type_info* type,
   struct type_snapshot* snapshot ) {
   if ( type->implicit_ref ) {
      snapshot->ref = dup_ref( type->ref );
   }
   else {
      snapshot->ref = type->ref;
   }
   snapshot->structure = type->structure;
   snapshot->enumeration = type->enumeration;
   snapshot->dim = type->dim;
   snapshot->spec = type->spec;
}

struct ref* dup_ref( struct ref* ref ) {
   size_t size = 0;
   switch ( ref->type ) {
   case REF_ARRAY: size = sizeof( struct ref_array ); break;
   default:
      UNREACHABLE()
      return NULL;
   }
   void* block = mem_alloc( size );
   memcpy( block, ref, size );
   return block;
}