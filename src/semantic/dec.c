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
   struct var* var;
   struct ref* ref;
   struct structure* structure;
   struct structure_member* member;
   struct enumeration* enumeration;
   struct dim* dim;
   struct initz_test* parent;
   int spec;
   int count;
   bool constant;
   bool has_string;
};

struct initz_pres {
   struct str output;
   bool member_last;
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
   struct structure* structure, struct structure_member* member );
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
static void refnotinit_var( struct semantic* semantic, struct var* var );
static bool test_object_initz( struct semantic* semantic, struct var* var );
static bool test_imported_object_initz( struct var* var );
static void confirm_dim_length( struct semantic* semantic, struct var* var );
static void test_auto_var( struct semantic* semantic, struct var* var );
static void assign_inferred_type( struct var* var, struct type_info* type );
static void init_initz_test( struct initz_test* test,
   struct initz_test* parent, int spec, struct dim* dim,
   struct structure* structure, struct enumeration* enumeration,
   struct ref* ref, bool constant );
static void init_root_initz_test( struct initz_test* test, struct var* var );
static bool test_multi_value( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value );
static bool test_multi_value_array( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value );
static bool test_multi_value_array_child( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value,
   struct initial* initial );
static void refnotinit_array( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value );
static bool test_multi_value_struct( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value );
static bool test_multi_value_struct_child( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value,
   struct initial* initial, struct structure_member* member );
static void refnotinit_struct( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value );
static bool test_value( struct semantic* semantic, struct initz_test* test,
   struct type_info* type, struct value* value );
static bool test_scalar_initz( struct semantic* semantic,
   struct initz_test* test, struct type_info* type, struct value* value );
static void initz_mismatch_diag( struct semantic* semantic,
   struct initz_test* test, struct type_info* initz_type,
   struct type_info* type, struct pos* pos );
static bool test_string_initz( struct semantic* semantic, struct dim* dim,
   struct value* value );
static void init_initz_pres( struct initz_pres* pres );
static void present_parent( struct initz_pres* pres, struct initz_test* test );
static void present_ref_element( struct initz_pres* pres, struct dim* dim,
   struct structure_member* member );
static void refnotinit( struct semantic* semantic, struct initz_pres* pres,
   struct pos* pos );
static void calc_dim_size( struct dim*, struct structure* );
static bool test_func_paramlist( struct semantic* semantic, struct func* func );
static bool test_func_param( struct semantic* semantic,
   struct func* func, struct param* param );
static void default_value_mismatch( struct semantic* semantic,
   struct func* func, struct param* param, struct expr_test* expr );
static int get_param_number( struct func* func, struct param* target );
static void init_func_test( struct func_test* test, struct func* func,
   struct list* labels );
static void calc_param_size( struct param* param );
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

void s_test_constant( struct semantic* semantic, struct constant* constant ) {
   // Test name. Only applies in a local scope.
   if ( semantic->in_localscope ) {
      if ( constant->name->object != &constant->object ) {
         s_bind_name( semantic, constant->name, &constant->object );
      }
   }
   // Test expression.
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, constant->value_node );
   if ( ! expr.undef_erred ) {
      if ( constant->value_node->folded ) {
         constant->spec = constant->value_node->spec;
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
      s_init_expr_test( &expr, true, false );
      s_test_expr( semantic, &expr, enumerator->initz );
      if ( expr.undef_erred ) {
         return;
      }
      if ( ! enumerator->initz->folded ) {
         s_diag( semantic, DIAG_POS_ERR, &enumerator->initz->pos,
            "enumerator expression not constant" );
         s_bail( semantic );
      }
      // The type of an enumerator is `int`. Maybe, sometime later, we'll
      // allow the user to specify the type of an enumerator value.
      if ( enumerator->initz->spec != SPEC_INT &&
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
         test_member( semantic, type, member );
         if ( ! member->object.resolved ) {
            return false;
         }
      }
      member = member->next;
   }
   return true;
}

void test_member( struct semantic* semantic, struct structure* structure,
   struct structure_member* member ) {
      if ( member->spec == SPEC_NAME ) {
         struct var_test test;
         init_var_test( &test, member->path );
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
   if ( member->structure && member->structure->has_ref_member ) {
      structure->has_ref_member = true;
   }
}

bool test_member_spec( struct semantic* semantic,
   struct structure_member* member ) {
   if ( member->spec == SPEC_VOID ) {
      s_diag( semantic, DIAG_POS_ERR, &member->object.pos,
         "struct-member of void type" );
      s_bail( semantic );
   }
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
   if ( member->structure ) {
      return member->structure->object.resolved;
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
      s_init_expr_test( &expr, true, false );
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
      if ( ! s_is_scalar( &return_type ) ) {
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
         s_init_expr_test( &expr, true, false );
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
   else {
      // References must always have a valid value.
      if ( var->ref || ( var->structure &&
         var->structure->has_ref_member ) ) {
         refnotinit_var( semantic, var );
         s_bail( semantic );
      }
   }
   // All dimensions of implicit length need to have a length.
   if ( var->dim ) {
      confirm_dim_length( semantic, var );
   }
   return true;
}

void refnotinit_var( struct semantic* semantic, struct var* var ) {
   if ( var->dim || ( var->structure && ! var->ref ) ) {
      struct initz_pres pres;
      init_initz_pres( &pres );
      struct str name;
      str_init( &name );
      t_copy_name( var->name, false, &name );
      str_append( &pres.output, name.value );
      str_deinit( &name );
      present_ref_element( &pres, var->dim, var->structure ?
         var->structure->member : NULL );
      refnotinit( semantic, &pres, &var->object.pos );
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "variable not initialized" );
      s_diag( semantic, DIAG_POS, &var->object.pos,
         "a reference-type variable must be initialized explicitly" );
   }
}

bool test_object_initz( struct semantic* semantic, struct var* var ) {
   struct initz_test test;
   init_root_initz_test( &test, var );
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
   }
   else {
      struct type_info type;
      s_init_type_info( &type, var->spec, var->ref, var->dim, var->structure,
         var->enumeration, NULL );
      bool resolved = test_value( semantic, &test, &type,
         ( struct value* ) var->initial );
      if ( ! resolved ) {
         return false;
      }
   }
   var->initial_has_str = test.has_string;
   return true;
}

void init_initz_test( struct initz_test* test, struct initz_test* parent,
   int spec, struct dim* dim, struct structure* structure,
   struct enumeration* enumeration, struct ref* ref, bool constant ) {
   test->var = NULL;
   test->ref = ref;
   test->structure = structure;
   test->member = NULL;
   test->enumeration = enumeration;
   test->dim = dim;
   test->parent = parent;
   test->spec = spec;
   test->count = 0;
   test->constant = constant;
   test->has_string = false;
   if ( structure ) {
      test->member = structure->member;
   }
}

void init_root_initz_test( struct initz_test* test, struct var* var ) {
   init_initz_test( test, NULL, var->spec, var->dim, var->structure,
      var->enumeration, var->ref, var->is_constant_init );
   test->var = var;
}

bool test_multi_value( struct semantic* semantic, struct initz_test* test,
   struct multi_value* multi_value ) {
   if ( test->dim ) {
      return test_multi_value_array( semantic, test, multi_value );
   }
   else if ( test->structure ) {
      return test_multi_value_struct( semantic, test, multi_value );
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &multi_value->pos,
         "too many brace initializers" );
      s_bail( semantic );
      return false;
   }
}

bool test_multi_value_array( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value ) {
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( ! initial->tested ) {
         bool resolved = test_multi_value_array_child( semantic,
            test, multi_value, initial );
         if ( ! resolved ) {
            return false;
         }
         initial->tested = true;
      }
      ++test->count;
      initial = initial->next;
   }
   // Update size of implicit dimension.
   if ( ! test->dim->size_node && test->count > test->dim->size ) {
      test->dim->size = test->count;
   }
   // All reference-type elements must be initialized.
   if ( test->count < test->dim->size ) {
      if ( test->ref || ( test->structure &&
         test->structure->has_ref_member ) ) {
         refnotinit_array( semantic, test, multi_value );
         s_bail( semantic );
      }
   }
   return true;
}

bool test_multi_value_array_child( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value,
   struct initial* initial ) {
   // Make sure there is an element to initialize.
   if ( ! ( test->dim && ( ! test->dim->size_node ||
      test->count < test->dim->size ) ) ) {
      s_diag( semantic, DIAG_POS_ERR, &multi_value->pos,
         "too many values in brace initializer" );
      s_bail( semantic );
   }
   if ( initial->multi ) {
      struct initz_test nested_test;
      init_initz_test( &nested_test, test, test->spec, test->dim->next,
         test->structure, test->enumeration, test->ref, test->constant );
      bool resolved = test_multi_value( semantic, &nested_test,
         ( struct multi_value* ) initial );
      if ( nested_test.has_string ) {
         test->has_string = true;
      }
      return resolved;
   }
   else {
      struct type_info type;
      s_init_type_info( &type, test->spec, test->ref, test->dim->next,
         test->structure, test->enumeration, NULL );
      return test_value( semantic, test, &type,
         ( struct value* ) initial );
   }
}

void refnotinit_array( struct semantic* semantic, struct initz_test* test,
   struct multi_value* multi_value ) {
   struct initz_pres pres;
   init_initz_pres( &pres );
   present_parent( &pres, test );
   if ( ! test->ref ) {
      present_ref_element( &pres, NULL, test->structure->member );
   }
   refnotinit( semantic, &pres, &multi_value->pos );
}

bool test_multi_value_struct( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value ) {
   struct initial* initial = multi_value->body;
   while ( initial ) {
      if ( ! initial->tested ) {
         bool resolved = test_multi_value_struct_child( semantic,
            test, multi_value, initial, test->member );
         if ( ! resolved ) {
            return false;
         }
         initial->tested = true;
      }
      test->member = test->member->next;
      initial = initial->next;
   }
   // Every reference-type member must be initialized.
   while ( test->member ) {
      if ( test->member->ref || ( test->member->structure &&
         test->member->structure->has_ref_member ) ) {
         refnotinit_struct( semantic, test, multi_value );
         s_bail( semantic );
      }
      test->member = test->member->next;
   }
   return true;
}

bool test_multi_value_struct_child( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value,
   struct initial* initial, struct structure_member* member ) {
   // Make sure there is a member to initialize.
   if ( ! member ) {
      s_diag( semantic, DIAG_POS_ERR, &multi_value->pos,
         "too many values in brace initializer" );
      s_bail( semantic );
   }
   if ( initial->multi ) {
      struct initz_test nested_test;
      init_initz_test( &nested_test, test, member->spec, member->dim,
         member->structure, member->enumeration, member->ref, test->constant );
      bool resolved = test_multi_value( semantic, &nested_test,
         ( struct multi_value* ) initial );
      if ( nested_test.has_string ) {
         test->has_string = true;
      }
      return resolved;
   }
   else {
      struct type_info type;
      s_init_type_info( &type, member->spec, member->ref, member->dim,
         member->structure, member->enumeration, NULL );
      return test_value( semantic, test, &type,
         ( struct value* ) initial );
   }
}

void refnotinit_struct( struct semantic* semantic, struct initz_test* test,
   struct multi_value* multi_value ) {
   struct initz_pres pres;
   init_initz_pres( &pres );
   present_parent( &pres, test );
   if ( test->member->ref ) {
      present_ref_element( &pres, test->member->dim, NULL );
   }
   else {
      present_ref_element( &pres, test->member->dim,
         test->member->structure->member );
   }
   refnotinit( semantic, &pres, &multi_value->pos );
}

bool test_value( struct semantic* semantic, struct initz_test* test,
   struct type_info* type, struct value* value ) {
   if ( s_is_scalar( type ) ) {
      return test_scalar_initz( semantic, test, type, value );
   }
   else if ( s_is_onedim_int_array( type ) ) {
      return test_string_initz( semantic, type->dim, value );
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "missing brace initializer" );
      s_bail( semantic );
      return false;
   }
}

bool test_scalar_initz( struct semantic* semantic, struct initz_test* test,
   struct type_info* type, struct value* value ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   struct type_info initz_type;
   s_test_expr_type( semantic, &expr, &initz_type, value->expr );
   if ( expr.undef_erred ) {
      return false;
   }
   if ( test->constant && ! value->expr->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "non-constant initializer" );
      s_bail( semantic );
   }
   if ( ! s_same_type( type, &initz_type ) ) {
      initz_mismatch_diag( semantic, test, &initz_type, type,
         &value->expr->pos );
      s_bail( semantic );
   }
   if ( expr.has_string ) {
      test->has_string = true;
   }
   return true;
}

void initz_mismatch_diag( struct semantic* semantic, struct initz_test* test,
   struct type_info* initz_type, struct type_info* type, struct pos* pos ) {
   struct str initz_type_s;
   str_init( &initz_type_s );
   s_present_type( initz_type, &initz_type_s );
   struct str type_s;
   str_init( &type_s );
   s_present_type( type, &type_s );
   const char* object =
      test->dim ? "element" :
      test->member ? "struct-member" : "variable";
   s_diag( semantic, DIAG_POS_ERR, pos,
      "initializer/%s type mismatch", object );
   s_diag( semantic, DIAG_POS, pos,
      "`%s` initializer, but `%s` %s", initz_type_s.value,
      type_s.value, object );
   str_deinit( &type_s );
   str_deinit( &initz_type_s );
}

bool test_string_initz( struct semantic* semantic, struct dim* dim,
   struct value* value ) {
   struct type_info type;
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr_type( semantic, &expr, &type, value->expr );
   if ( expr.undef_erred ) {
      return false;
   }
   if ( ! s_is_str_value_type( &type ) ) {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "missing brace initializer" );
      s_bail( semantic );
   }
   if ( ! value->expr->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "non-constant string initializer" );
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
   return true;
}

void init_initz_pres( struct initz_pres* pres ) {
   str_init( &pres->output );
   pres->member_last = false;
}

void present_parent( struct initz_pres* pres, struct initz_test* test ) {
   if ( test->parent ) {
      present_parent( pres, test->parent );
   }
   else {
      struct str name;
      str_init( &name );
      t_copy_name( test->var->name, false, &name );
      str_append( &pres->output, name.value );
      str_deinit( &name );
   }
   if ( test->dim ) {
      str_append( &pres->output, "[" );
      char number[ 11 ];
      sprintf( number, "%d", test->count );
      str_append( &pres->output, number );
      str_append( &pres->output, "]" );
      pres->member_last = false;
   }
   else if ( test->member ) {
      str_append( &pres->output, "." );
      struct str name;
      str_init( &name );
      t_copy_name( test->member->name, false, &name );
      str_append( &pres->output, name.value );
      str_deinit( &name );
      pres->member_last = true;
   }
}

void present_ref_element( struct initz_pres* pres, struct dim* dim,
   struct structure_member* member ) {
   if ( dim ) {
      while ( dim ) {
         str_append( &pres->output, "[0]" );
         dim = dim->next;
      }
      pres->member_last = false;
   }
   while ( member && ! ( member->ref || ( member->structure &&
      member->structure->has_ref_member ) ) ) {
      member = member->next;
   }
   if ( member ) {
      str_append( &pres->output, "." );
      struct str name;
      str_init( &name );
      t_copy_name( member->name, false, &name );
      str_append( &pres->output, name.value );
      str_deinit( &name );
      pres->member_last = true;
      if ( member->ref ) {
         present_ref_element( pres, member->dim, NULL );
      }
      else {
         present_ref_element( pres, member->dim,
            member->structure->member );
      }
   }
}

void refnotinit( struct semantic* semantic, struct initz_pres* pres,
   struct pos* pos ) {
   s_diag( semantic, DIAG_POS_ERR, pos,
      "`%s` not initialized", pres->output.value );
   s_diag( semantic, DIAG_POS, pos,
      "a reference-type %s must be initialized with a valid reference value",
      pres->member_last ? "member" : "element" );
   str_deinit( &pres->output );
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

void confirm_dim_length( struct semantic* semantic, struct var* var ) {
   struct dim* dim = var->dim;
   // The first dimension of a world array or a global array does not need to
   // have a concrete length.
   switch ( var->storage ) {
   case STORAGE_WORLD:
   case STORAGE_GLOBAL:
      dim = dim->next;
      break;
   default:
      break;
   }
   while ( dim ) {
      if ( dim->size == 0 ) {
         s_diag( semantic, DIAG_POS_ERR, &dim->pos,
            "missing initialization of dimension of implicit length" );
         s_bail( semantic );
      }
      dim = dim->next;
   }
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
   s_init_expr_test( &expr, true, false );
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
   s_take_type_snapshot( type, &snapshot );
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
   // Test return type.
   if ( func->return_spec == SPEC_NAME ) {
      struct var_test test;
      init_var_test( &test, func->path );
      resolve_name_spec( semantic, &test );
      func->structure = test.structure;
      func->enumeration = test.enumeration;
      func->return_spec = test.spec;
   }
   if ( func->return_spec == SPEC_STRUCT && ! func->ref ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "function returning a struct" );
      s_diag( semantic, DIAG_POS, &func->object.pos,
         "a struct can only be returned by reference" );
      s_bail( semantic );
   }
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
   if ( param->spec == SPEC_NAME ) {
      struct var_test test;
      init_var_test( &test, param->path );
      resolve_name_spec( semantic, &test );
      param->structure = test.structure;
      param->enumeration = test.enumeration;
      param->spec = test.spec;
   }
   if ( param->default_value ) {
      struct expr_test expr;
      s_init_expr_test( &expr, true, false );
      s_test_expr( semantic, &expr, param->default_value );
      if ( expr.undef_erred ) {
         return false;
      }
      if ( param->spec != SPEC_RAW &&
         param->default_value->spec != SPEC_RAW ) {
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
   calc_param_size( param );
   return true;
}

void default_value_mismatch( struct semantic* semantic, struct func* func,
   struct param* param, struct expr_test* expr ) {
   s_diag( semantic, DIAG_POS, &param->default_value->pos, "mismatch" );
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
   struct func_test test;
   init_func_test( &test, func, &impl->labels );
   if ( ! impl->nested ) {
      semantic->topfunc_test = &test;
   }
   struct func_test* prev = semantic->func_test;
   semantic->func_test = &test;
   if ( func->msgbuild ) {
      struct name* name = t_extend_name( semantic->ns->body, "append" );
      semantic->task->append_func->name = name;
      s_bind_name( semantic, name, &semantic->task->append_func->object );
   }
   s_test_body( semantic, &impl->body->node );
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

void init_func_test( struct func_test* test, struct func* func,
   struct list* labels ) {
   test->func = func;
   test->labels = labels;
   test->nested_funcs = NULL;
   test->returns = NULL;
   test->script = ( func == NULL );
}

void s_test_script( struct semantic* semantic, struct script* script ) {
   test_script_number( semantic, script );
   test_script_body( semantic, script );
}

void test_script_number( struct semantic* semantic, struct script* script ) {
   if ( script->number ) {
      struct expr_test expr;
      s_init_expr_test( &expr, true, false );
      s_test_expr( semantic, &expr, script->number );
      if ( ! script->number->folded ) {
         s_diag( semantic, DIAG_POS_ERR, &script->number->pos,
            "script number not a constant expression" );
         s_bail( semantic );
      }
      script->named_script = ( script->number->spec == SPEC_STR );
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
   struct func_test test;
   init_func_test( &test, NULL, &script->labels );
   semantic->topfunc_test = &test;
   semantic->func_test = semantic->topfunc_test;
   s_test_body( semantic, script->body );
   script->nested_funcs = test.nested_funcs;
   semantic->topfunc_test = NULL;
   semantic->func_test = NULL;
   s_pop_scope( semantic );
}

void s_calc_var_size( struct var* var ) {
   var->size = calc_size( var->dim, var->structure, var->ref );
}

void calc_param_size( struct param* param ) {
   param->size = calc_size( NULL, NULL, param->ref );
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