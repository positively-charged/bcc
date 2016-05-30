#include <string.h>
#include <limits.h>

#include "phase.h"

#define SCRIPT_MIN_NUM 0
#define SCRIPT_MAX_NUM 32767

struct enumeration_test {
   int value;
};

struct name_spec_test {
   struct ref* ref;
   struct structure* structure;
   struct enumeration* enumeration;
   struct dim* dim;
   struct path* path;
   int spec;
};

struct ref_test {
   struct ref* ref;
   int spec;
};

struct dim_test {
   struct dim* dim;
   bool resolved;
   bool member;
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

struct scalar_initz_test {
   struct type_info initz_type;
   struct initz_test* initz_test;
   struct type_info* type;
   bool constant;
   bool has_string;
};

struct initz_pres {
   struct str output;
   bool member_last;
};

struct builtin_aliases {
   struct alias* append;
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
   struct enumeration_test* test, struct enumeration* enumeration,
   struct enumerator* enumerator );
static bool test_struct_name( struct semantic* semantic,
   struct structure* structure );
static bool test_struct_body( struct semantic* semantic,
   struct structure* structure );
static void test_member( struct semantic* semantic,
   struct structure* structure, struct structure_member* member );
static bool test_member_spec( struct semantic* semantic,
   struct structure* structure, struct structure_member* member );
static bool test_member_ref( struct semantic* semantic,
   struct structure_member* member );
static bool test_member_name( struct semantic* semantic,
   struct structure_member* member );
static bool test_member_dim( struct semantic* semantic,
   struct structure_member* member );
static bool test_typedef_spec( struct semantic* semantic,
   struct type_alias* alias );
static bool test_typedef_ref( struct semantic* semantic,
   struct type_alias* alias );
static bool test_typedef_name( struct semantic* semantic,
   struct type_alias* alias );
static bool test_typedef_dim( struct semantic* semantic,
   struct type_alias* alias );
static bool test_var_spec( struct semantic* semantic, struct var* var );
static void init_name_spec_test( struct name_spec_test* test,
   struct path* path, struct ref* ref, struct dim* dim );
static void test_name_spec( struct semantic* semantic,
   struct name_spec_test* test );
static void merge_type( struct semantic* semantic, struct name_spec_test* test,
   struct type_alias* alias );
static void merge_ref( struct semantic* semantic,
   struct name_spec_test* test, struct type_alias* alias );
static bool test_var_ref( struct semantic* semantic, struct var* var );
static void init_ref_test( struct ref_test* test, struct ref* ref, int spec );
static void test_ref( struct semantic* semantic, struct ref_test* test );
static void test_ref_part( struct semantic* semantic, struct ref_test* test,
   struct ref* ref );
static bool test_var_name( struct semantic* semantic, struct var* var );
static bool test_var_dim( struct semantic* semantic, struct var* var );
static void init_dim_test( struct dim_test* test, struct dim* dim,
   bool member );
static void test_dim_list( struct semantic* semantic, struct dim_test* test );
static bool test_dim( struct semantic* semantic, struct dim_test* test,
   struct dim* dim );
static bool test_dim_length( struct semantic* semantic, struct dim_test* test,
   struct dim* dim );
static bool test_var_initz( struct semantic* semantic, struct var* var );
static void refnotinit_var( struct semantic* semantic, struct var* var );
static bool test_object_initz( struct semantic* semantic, struct var* var );
static bool test_imported_object_initz( struct var* var );
static void confirm_dim_length( struct semantic* semantic, struct var* var );
static bool test_var_finish( struct semantic* semantic, struct var* var );
static void describe_var( struct var* var );
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
static void init_scalar_initz_test( struct scalar_initz_test* test,
   struct initz_test* initz_test, struct type_info* type, bool constant );
static void init_scalar_initz_test_auto( struct scalar_initz_test* test,
   bool constant );
static bool test_scalar_initz( struct semantic* semantic,
   struct scalar_initz_test* test, struct value* value );
static void initz_mismatch( struct semantic* semantic,
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
static bool test_func_qual( struct semantic* semantic, struct func* func );
static bool test_func_return_spec( struct semantic* semantic,
   struct func* func );
static bool test_func_name( struct semantic* semantic, struct func* func );
static bool test_func_after_name( struct semantic* semantic,
   struct func* func );
static bool test_param_list( struct semantic* semantic, struct func* func );
static void test_param( struct semantic* semantic, struct func* func,
   struct param* param );
static bool test_param_spec( struct semantic* semantic, struct param* param );
static bool test_param_name( struct semantic* semantic, struct param* param );
static bool test_param_ref( struct semantic* semantic, struct param* param );
static bool test_param_after_ref( struct semantic* semantic, struct func* func,
   struct param* param );
static bool test_param_default_value( struct semantic* semantic,
   struct func* func, struct param* param );
static void default_value_mismatch( struct semantic* semantic,
   struct func* func, struct param* param, struct type_info* param_type,
   struct type_info* type, struct pos* pos );
static int get_param_number( struct func* func, struct param* target );
static void init_builtin_aliases( struct semantic* semantic, struct func* func,
   struct builtin_aliases* aliases );
static void bind_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases );
static void deinit_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases );
static void init_func_test( struct func_test* test, struct func* func,
   struct list* labels, struct list* funcscope_vars );
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
   struct enumeration_test test = { -1 };
   // Skip previously resolved enumerators.
   struct enumerator* enumerator = enumeration->head;
   while ( enumerator && enumerator->object.resolved ) {
      test.value = enumerator->value;
      enumerator = enumerator->next;
   }
   // Test unresolved enumerators.
   while ( enumerator ) {
      test_enumerator( semantic, &test, enumeration, enumerator );
      if ( ! enumerator->object.resolved ) {
         return;
      }
      enumerator = enumerator->next;
   }
   enumeration->object.resolved = true;
}

void test_enumerator( struct semantic* semantic, struct enumeration_test* test,
   struct enumeration* enumeration, struct enumerator* enumerator ) {
   // Test name.
   if ( semantic->in_localscope ) {
      s_bind_name( semantic, enumerator->name, &enumerator->object );
   }
   // Test initializer.
   if ( enumerator->initz ) {
      struct type_info type;
      struct expr_test expr;
      s_init_expr_test_enumerator( &expr, enumeration );
      s_test_expr_type( semantic, &expr, &type, enumerator->initz );
      if ( expr.undef_erred ) {
         return;
      }
      if ( ! enumerator->initz->folded ) {
         s_diag( semantic, DIAG_POS_ERR, &enumerator->initz->pos,
            "enumerator expression not constant" );
         s_bail( semantic );
      }
      struct type_info base_type;
      s_init_type_info_scalar( &base_type, enumeration->base_type );
      if ( ! s_instance_of( &base_type, &type ) ) {
         s_type_mismatch( semantic, "enumerator", &type,
            "enumeration-base", &base_type, &enumerator->object.pos );
         s_bail( semantic );
      }
      test->value = enumerator->initz->value;
   }
   else {
      // Non-integer base types require an explicit initializer.
      if ( enumeration->base_type != SPEC_INT ) {
         s_diag( semantic, DIAG_POS_ERR, &enumerator->object.pos,
            "missing initializer for enumerator of non-integer type" );
         s_bail( semantic );
      }
      // Check for integer overflow.
      if ( test->value == INT_MAX ) {
         s_diag( semantic, DIAG_POS_ERR, &enumerator->object.pos,
            "enumerator value growing beyond the maximum" );
         s_diag( semantic, DIAG_POS, &enumerator->object.pos,
            "maximum automatically-generated value is %d", INT_MAX );
         s_bail( semantic );
      }
      ++test->value;
   }
   enumerator->value = test->value;
   enumerator->object.resolved = true;
}

void s_test_struct( struct semantic* semantic, struct structure* structure ) {
   structure->object.resolved =
      test_struct_name( semantic, structure ) &&
      test_struct_body( semantic, structure );
   if ( structure->object.resolved ) {
      calc_struct_size( structure );
   }
}

bool test_struct_name( struct semantic* semantic,
   struct structure* structure ) {
   if ( semantic->in_localscope ) {
      s_bind_name( semantic, structure->name, &structure->object );
   }
   return true;
}

bool test_struct_body( struct semantic* semantic,
   struct structure* structure ) {
   struct structure_member* member = structure->member;
   while ( member ) {
      if ( ! member->object.resolved ) {
         test_member( semantic, structure, member );
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
   member->object.resolved =
      test_member_spec( semantic, structure, member ) &&
      test_member_ref( semantic, member ) &&
      test_member_name( semantic, member ) &&
      test_member_dim( semantic, member );
   if ( member->object.resolved ) {
      if ( member->structure && member->structure->has_ref_member ) {
         structure->has_ref_member = true;
      }
   }
}

bool test_member_spec( struct semantic* semantic, struct structure* structure,
   struct structure_member* member ) {
   if ( member->spec == SPEC_NAME ) {
      struct name_spec_test test;
      init_name_spec_test( &test, member->path, member->ref, member->dim );
      test_name_spec( semantic, &test );
      member->ref = test.ref;
      member->structure = test.structure;
      member->enumeration = test.enumeration;
      member->dim = test.dim;
      member->spec = test.spec;
   }
   if ( member->spec == SPEC_VOID &&
      ! ( member->ref && member->ref->type == REF_FUNCTION ) ) {
      s_diag( semantic, DIAG_POS_ERR, &member->object.pos,
         "struct-member of void type" );
      s_bail( semantic );
   }
   if ( member->spec == SPEC_STRUCT ) {
      return member->structure->object.resolved ||
         ( member->structure == structure && member->ref );
   }
   else if ( member->spec == SPEC_ENUM ) {
      return member->enumeration->object.resolved;
   }
   else {
      return true;
   }
}

bool test_member_ref( struct semantic* semantic,
   struct structure_member* member ) {
   struct ref_test test;
   init_ref_test( &test, member->ref, member->spec );
   test_ref( semantic, &test );
   return true;
}

bool test_member_name( struct semantic* semantic,
   struct structure_member* member ) {
   if ( semantic->in_localscope ) {
      s_bind_name( semantic, member->name, &member->object );
   }
   return true;
}

bool test_member_dim( struct semantic* semantic,
   struct structure_member* member ) {
   if ( member->dim ) {
      struct dim_test test;
      init_dim_test( &test, member->dim, true );
      test_dim_list( semantic, &test );
      return test.resolved;
   }
   else {
      return true;
   }
}

void s_test_type_alias( struct semantic* semantic, struct type_alias* alias ) {
   alias->object.resolved =
      test_typedef_spec( semantic, alias ) &&
      test_typedef_ref( semantic, alias ) &&
      test_typedef_name( semantic, alias ) &&
      test_typedef_dim( semantic, alias );
}

bool test_typedef_spec( struct semantic* semantic, struct type_alias* alias ) {
   if ( alias->spec == SPEC_NAME ) {
      struct name_spec_test test;
      init_name_spec_test( &test, alias->path, alias->ref, alias->dim );
      test_name_spec( semantic, &test );
      alias->ref = test.ref;
      alias->structure = test.structure;
      alias->enumeration = test.enumeration;
      alias->dim = test.dim;
      alias->spec = test.spec;
   }
   if ( alias->spec == SPEC_VOID &&
      ! ( alias->ref && alias->ref->type == REF_FUNCTION ) ) {
      s_diag( semantic, DIAG_POS_ERR, &alias->object.pos,
         "typedef of void type" );
      s_bail( semantic );
   }
   if ( alias->spec == SPEC_STRUCT ) {
      return alias->structure->object.resolved;
   }
   else if ( alias->spec == SPEC_ENUM ) {
      return alias->enumeration->object.resolved;
   }
   else {
      return true;
   }
}

bool test_typedef_ref( struct semantic* semantic, struct type_alias* alias ) {
   struct ref_test test;
   init_ref_test( &test, alias->ref, alias->spec );
   test_ref( semantic, &test );
   return true;
}

bool test_typedef_name( struct semantic* semantic, struct type_alias* alias ) {
   if ( semantic->in_localscope ) {
      s_bind_name( semantic, alias->name, &alias->object );
   }
   return true;
}

bool test_typedef_dim( struct semantic* semantic, struct type_alias* alias ) {
   // No need to continue when the variable is not an array.
   if ( alias->dim ) {
      struct dim_test test;
      init_dim_test( &test, alias->dim, false );
      test_dim_list( semantic, &test );
      return test.resolved;
   }
   else {
      return true;
   }
}

void s_test_var( struct semantic* semantic, struct var* var ) {
   if ( var->spec == SPEC_AUTO ) {
      test_auto_var( semantic, var );
   }
   else {
      var->object.resolved =
         test_var_spec( semantic, var ) &&
         test_var_ref( semantic, var ) &&
         test_var_name( semantic, var ) &&
         test_var_dim( semantic, var ) &&
         test_var_initz( semantic, var ) &&
         test_var_finish( semantic, var );
   }
}

bool test_var_spec( struct semantic* semantic, struct var* var ) {
   if ( var->spec == SPEC_NAME ) {
      struct name_spec_test test;
      init_name_spec_test( &test, var->type_path, var->ref, var->dim );
      test_name_spec( semantic, &test );
      var->ref = test.ref;
      var->structure = test.structure;
      var->enumeration = test.enumeration;
      var->dim = test.dim;
      var->spec = test.spec;
   }
   if ( var->spec == SPEC_VOID &&
      ! ( var->ref && var->ref->type == REF_FUNCTION ) ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "variable of void type" );
      s_bail( semantic );
   }
   var->spec = s_spec( semantic, var->spec );
   var->func_scope = s_func_scope_forced( semantic );
   if ( var->spec == SPEC_STRUCT ) {
      return var->structure->object.resolved;
   }
   else if ( var->spec == SPEC_ENUM ) {
      return var->enumeration->object.resolved;
   }
   else {
      return true;
   }
}

void init_name_spec_test( struct name_spec_test* test, struct path* path,
   struct ref* ref, struct dim* dim ) {
   test->ref = ref;
   test->structure = NULL;
   test->enumeration = NULL;
   test->dim = dim;
   test->path = path;
   test->spec = SPEC_NONE;
}

void test_name_spec( struct semantic* semantic, struct name_spec_test* test ) {
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
   case NODE_TYPE_ALIAS:
      merge_type( semantic, test,
         ( struct type_alias* ) object );
      break;
   default:
      path = s_last_path_part( test->path );
      s_diag( semantic, DIAG_POS_ERR, &path->pos,
         "object not a valid type" );
      s_bail( semantic );
   }
}

void merge_type( struct semantic* semantic, struct name_spec_test* test,
   struct type_alias* alias ) {
   test->spec = s_spec( semantic, alias->spec );
   test->structure = alias->structure;
   test->enumeration = alias->enumeration;
   if ( test->ref ) {
      merge_ref( semantic, test, alias );
   }
   else if ( test->dim ) {
      test->ref = alias->ref;
      if ( alias->dim ) {
         struct dim* dim = test->dim;
         while ( dim->next ) {
            dim = dim->next;
         }
         dim->next = alias->dim;
      }
   }
   else {
      test->ref = alias->ref;
      test->dim = alias->dim;
   }
}

void merge_ref( struct semantic* semantic, struct name_spec_test* test,
   struct type_alias* alias ) {
   struct ref* ref = test->ref;
   while ( ref->next ) {
      ref = ref->next;
   }
   if ( ref->type == REF_ARRAY ) {
      if ( alias->dim ) {
         struct ref_array* array = ( struct ref_array* ) ref;
         struct dim* dim = alias->dim;
         while ( dim ) {
            ++array->dim_count;
            dim = dim->next;
         }
         ref->next = alias->ref;
      }
      else if ( alias->ref && alias->ref->type == REF_ARRAY ) {
         struct ref_array* array = ( struct ref_array* ) ref;
         struct ref_array* alias_array = ( struct ref_array* ) alias->ref;
         array->dim_count += alias_array->dim_count;
         ref->next = alias->ref->next;
      }
      else {
         ref->next = alias->ref;
      }
   }
   else if ( ref->type == REF_FUNCTION ) {
      if ( alias->dim ) {
         s_diag( semantic, DIAG_POS_ERR, &ref->pos,
            "reference function returning an array" );
         s_bail( semantic );
      }
      else {
         ref->next = alias->ref;
      }
   }
   else if ( ref->type == REF_STRUCTURE ) {
      if ( alias->dim ) {
         struct type_info type;
         s_init_type_info( &type, alias->ref, alias->structure,
            alias->enumeration, alias->dim, alias->spec );
         struct type_snapshot snapshot;
         s_take_type_snapshot( &type, &snapshot );
         if ( ref == test->ref ) {
            test->ref = snapshot.ref;
         }
         else {
            struct ref* prev_ref = test->ref;
            while ( prev_ref->next != ref ) {
               prev_ref = prev_ref->next;
            }
            prev_ref->next = snapshot.ref;
         }
         mem_free( ref );
      }
      else if ( alias->ref ) {
         s_diag( semantic, DIAG_POS_ERR, &ref->pos,
            "reference to another reference" );
         s_bail( semantic );
      }
   }
   else {
      UNREACHABLE();
   }
}

bool test_var_ref( struct semantic* semantic, struct var* var ) {
   if ( var->ref ) {
      struct ref_test test;
      init_ref_test( &test, var->ref, var->spec );
      test_ref( semantic, &test );
   }
   return true;
}

void init_ref_test( struct ref_test* test, struct ref* ref, int spec ) {
   test->ref = ref;
   test->spec = spec;
}

void test_ref( struct semantic* semantic, struct ref_test* test ) {
   struct ref* ref = test->ref;
   while ( ref ) {
      test_ref_part( semantic, test, ref );
      ref = ref->next;
   }
}

void test_ref_part( struct semantic* semantic, struct ref_test* test,
   struct ref* ref ) {
   if ( ref->type == REF_FUNCTION ) {
      // TODO
   }
   else if ( ref->type == REF_STRUCTURE ) {
      if ( test->spec != SPEC_STRUCT ) {
         s_diag( semantic, DIAG_POS_ERR, &ref->pos,
            "invalid reference type" );
         s_diag( semantic, DIAG_POS, &ref->pos,
            "only array/structure/function reference type is valid" );
         s_bail( semantic );
      }
   }
   if ( semantic->lib->importable && ref->type != REF_FUNCTION ) {
      s_diag( semantic, DIAG_POS_ERR, &ref->pos,
         "invalid reference type" );
      s_diag( semantic, DIAG_POS, &ref->pos,
         "only function-reference type is valid in an importable library" );
      s_bail( semantic );
   }
}

bool test_var_name( struct semantic* semantic, struct var* var ) {
   if ( semantic->in_localscope ) {
      s_bind_name( semantic, var->name, &var->object );
   }
   return true;
}

bool test_var_dim( struct semantic* semantic, struct var* var ) {
   // No need to continue when the variable is not an array.
   if ( var->dim ) {
      struct dim_test test;
      init_dim_test( &test, var->dim, false );
      test_dim_list( semantic, &test );
      return test.resolved;
   }
   else {
      return true;
   }
}

void init_dim_test( struct dim_test* test, struct dim* dim, bool member ) {
   test->dim = dim;
   test->resolved = false;
   test->member = member;
}

void test_dim_list( struct semantic* semantic, struct dim_test* test ) {
   struct dim* dim = test->dim;
   while ( dim ) {
      if ( ! test_dim( semantic, test, dim ) ) {
         return;
      }
      dim = dim->next;
   }
   test->resolved = true;
}

bool test_dim( struct semantic* semantic, struct dim_test* test,
   struct dim* dim ) {
   if ( test->member && ! dim->length_node ) {
      s_diag( semantic, DIAG_POS_ERR, &dim->pos,
         "dimension of implicit length in struct member" );
      s_bail( semantic );
   }
   if ( dim->length_node && dim->length == 0 ) {
      return test_dim_length( semantic, test, dim );
   }
   else {
      return true;
   }
}

bool test_dim_length( struct semantic* semantic, struct dim_test* test,
   struct dim* dim ) {
   struct type_info type;
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr_type( semantic, &expr, &type, dim->length_node );
   if ( expr.undef_erred ) {
      return false;
   }
   if ( ! s_is_int_value( &type ) ) {
      s_diag( semantic, DIAG_POS_ERR, &dim->length_node->pos,
         "dimension length of non-integer type" );
      s_bail( semantic );
   }
   if ( ! dim->length_node->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &dim->length_node->pos,
         "non-constant dimension length" );
      s_bail( semantic );
   }
   if ( dim->length_node->value <= 0 ) {
      s_diag( semantic, DIAG_POS_ERR, &dim->length_node->pos,
         "dimension length <= 0" );
      s_bail( semantic );
   }
   dim->length = dim->length_node->value;
   return true;
}

bool test_var_initz( struct semantic* semantic, struct var* var ) {
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
      s_init_type_info( &type, var->ref, var->structure,
         var->enumeration, var->dim, var->spec );
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
   // Update length of implicit dimension.
   if ( ! test->dim->length_node && test->count > test->dim->length ) {
      test->dim->length = test->count;
   }
   // All reference-type elements must be initialized.
   if ( test->count < test->dim->length ) {
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
   if ( ! ( test->dim && ( ! test->dim->length_node ||
      test->count < test->dim->length ) ) ) {
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
      s_init_type_info( &type, test->ref, test->structure,
         test->enumeration, test->dim->next, test->spec );
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
   test->member = test->structure->member;
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
      s_init_type_info( &type, member->ref, member->structure,
         member->enumeration, member->dim, member->spec );
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
      struct scalar_initz_test scalar_test;
      init_scalar_initz_test( &scalar_test, test, type, test->constant );
      if ( ! test_scalar_initz( semantic, &scalar_test, value ) ) {
         return false;
      }
      if ( scalar_test.has_string ) {
         test->has_string = true;
      }
      return true;
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

void init_scalar_initz_test( struct scalar_initz_test* test,
   struct initz_test* initz_test, struct type_info* type, bool constant ) {
   test->initz_test = initz_test;
   test->type = type;
   test->constant = constant;
   test->has_string = false;
}

void init_scalar_initz_test_auto( struct scalar_initz_test* test,
   bool constant ) {
   init_scalar_initz_test( test, NULL, NULL, constant );
}

bool test_scalar_initz( struct semantic* semantic,
   struct scalar_initz_test* test, struct value* value ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr_type( semantic, &expr, &test->initz_type, value->expr );
   if ( expr.undef_erred ) {
      return false;
   }
   if ( test->constant && ! value->expr->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "non-constant initializer" );
      s_bail( semantic );
   }
   // Perform type checking when testing an initializer for a variable with
   // known type information.
   if ( test->initz_test ) {
      if ( ! s_instance_of( test->type, &test->initz_type ) ) {
         initz_mismatch( semantic, test->initz_test, &test->initz_type,
            test->type, &value->expr->pos );
         s_bail( semantic );
      }
   }
   if ( expr.func && expr.func->type == FUNC_USER ) {
      struct func_user* impl = expr.func->impl;
      if ( impl->nested ) {
         s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
            "nested function used as an initializer" );
         s_bail( semantic );
      }
   }
   test->has_string = expr.has_string;
   value->var = expr.var;
   value->func = expr.func;
   if ( expr.has_string ) {
      value->type = VALUE_STRING;
   }
   else if ( expr.func ) {
      value->type = VALUE_FUNC;
   }
   if ( s_is_ref_type( &test->initz_type ) && expr.var ) {
      expr.var->addr_taken = true;
   }
   return true;
}

void initz_mismatch( struct semantic* semantic, struct initz_test* test,
   struct type_info* initz_type, struct type_info* type, struct pos* pos ) {
   s_type_mismatch( semantic, "initializer", initz_type, test->dim ?
      "element" : ( test->member ? "struct-member" : "variable" ), type, pos );
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
   if ( dim->length_node ) {
      if ( string->length >= dim->length ) {
         s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
            "string initializer too long" );
         s_bail( semantic );
      }
   }
   else {
      // Update length of implicit dimension.
      if ( string->length + 1 > dim->length ) {
         dim->length = string->length + 1;
      }
   }
   value->string_initz = true;
   value->type = VALUE_STRINGINITZ;
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
      if ( ! var->dim->length_node && ! var->dim->length ) {
         struct multi_value* multi_value =
            ( struct multi_value* ) var->initial;
         struct initial* initial = multi_value->body;
         while ( initial ) {
            initial = initial->next;
            ++var->dim->length;
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
      if ( dim->length == 0 ) {
         s_diag( semantic, DIAG_POS_ERR, &dim->pos,
            "missing initialization of dimension of implicit length" );
         s_bail( semantic );
      }
      dim = dim->next;
   }
}

bool test_var_finish( struct semantic* semantic, struct var* var ) {
   describe_var( var );
   if ( semantic->func_test != semantic->topfunc_test &&
      ( var->desc == DESC_ARRAY || var->desc == DESC_STRUCTVAR ) ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "%s declared inside nested function", var->desc == DESC_ARRAY ?
            "array" : "struct variable" );
      s_bail( semantic );
   }
   return true;
}

void describe_var( struct var* var ) {
   // Array.
   if ( var->dim ) {
      var->desc = DESC_ARRAY;
   }
   // Reference/reference-element. 
   else if ( var->ref ) {
      var->desc = DESC_REFVAR;
   }
   // Structure/structure-element.
   else if ( var->structure ) {
      var->desc = DESC_STRUCTVAR;
   }
   // Initializer.
   else {
      var->desc = DESC_PRIMITIVEVAR;
   }
}

void test_auto_var( struct semantic* semantic, struct var* var ) {
   // Infer type from initializer.
   if ( ! var->initial ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "auto-declaration missing initializer" );
      s_bail( semantic );
   }
   struct scalar_initz_test initz_test;
   init_scalar_initz_test_auto( &initz_test, var->is_constant_init );
   test_scalar_initz( semantic, &initz_test, ( struct value* ) var->initial );
   assign_inferred_type( var, &initz_test.initz_type );
   var->func_scope = s_func_scope_forced( semantic );
   var->initial_has_str = initz_test.has_string;
   // For now, keep an auto-declaration in local scope.
   if ( ! semantic->in_localscope ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "auto-declaration in non-local scope" );
      s_bail( semantic );
   }
   s_bind_name( semantic, var->name, &var->object );
   var->object.resolved = true;
   describe_var( var );
}

void assign_inferred_type( struct var* var, struct type_info* type ) {
   struct type_snapshot snapshot;
   s_take_type_snapshot( type, &snapshot );
   var->ref = snapshot.ref;
   var->structure = snapshot.structure;
   var->enumeration = snapshot.enumeration;
   var->dim = snapshot.dim;
   var->spec = snapshot.spec;
   if ( var->enumeration ) {
      var->spec = SPEC_ENUM;
   }
}

void s_test_local_var( struct semantic* semantic, struct var* var ) {
   s_test_var( semantic, var );
   s_calc_var_size( var );
   if ( var->initial ) {
      s_calc_var_value_index( var );
   }
   if ( var->func_scope ) {
      list_append( semantic->func_test->funcscope_vars, var );
   }
}

void s_test_foreach_var( struct semantic* semantic,
   struct type_info* collection_type, struct var* var ) {
   bool resolved = false;
   if ( var->spec == SPEC_AUTO ) {
      assign_inferred_type( var, collection_type );
      describe_var( var );
      resolved = true;
   }
   else {
      resolved =
         test_var_spec( semantic, var ) &&
         test_var_ref( semantic, var ) &&
         test_var_finish( semantic, var );
   }
   // We don't want the user to access iterator information after the foreach
   // loop has ended, so make the iterator variables accessible only in the
   // body of the loop.
   s_bind_block_name( semantic, var->name, &var->object );
   var->constant = true;
   var->object.resolved = resolved;
   s_calc_var_size( var );
}

void s_test_func( struct semantic* semantic, struct func* func ) {
   func->object.resolved =
      test_func_qual( semantic, func ) &&
      test_func_return_spec( semantic, func ) &&
      test_func_name( semantic, func ) &&
      test_func_after_name( semantic, func );
}

bool test_func_qual( struct semantic* semantic, struct func* func ) {
   if ( func->msgbuild && func->type != FUNC_USER ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "message-building qualifier specified for non-user function" );
      s_bail( semantic );
   }
   return true;
}

bool test_func_return_spec( struct semantic* semantic, struct func* func ) {
   if ( func->return_spec == SPEC_NAME ) {
      struct name_spec_test test;
      init_name_spec_test( &test, func->path, func->ref, NULL );
      test_name_spec( semantic, &test );
      func->ref = test.ref;
      func->structure = test.structure;
      func->enumeration = test.enumeration;
      func->return_spec = test.spec;
   }
   if ( func->return_spec == SPEC_STRUCT && ! func->ref ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "function returning a struct by value" );
      s_diag( semantic, DIAG_POS, &func->object.pos,
         "a struct can only be returned by reference" );
      s_bail( semantic );
   }
   if ( func->return_spec == SPEC_STRUCT ) {
      return func->structure->object.resolved;
   }
   else if ( func->return_spec == SPEC_ENUM ) {
      return func->enumeration->object.resolved;
   }
   else {
      return true;
   }
}

bool test_func_name( struct semantic* semantic, struct func* func ) {
   if ( func->name && semantic->in_localscope ) {
      s_bind_name( semantic, func->name, &func->object );
   }
   return true;
}

bool test_func_after_name( struct semantic* semantic, struct func* func ) {
   s_add_scope( semantic, true );
   bool resolved = test_param_list( semantic, func );
   s_pop_scope( semantic );
   return resolved;
}

bool test_param_list( struct semantic* semantic, struct func* func ) {
   struct param* param = func->params;
   while ( param ) {
      if ( param->object.resolved ) {
         if ( param->name ) {
            s_bind_name( semantic, param->name, &param->object );
         }
      }
      else {
         test_param( semantic, func, param );
         if ( ! param->object.resolved ) {
            return false;
         }
      }
      param = param->next;
   }
   // Action-specials can take up to 5 arguments.
   enum { MAX_ASPEC_PARAMS = 5 };
   if ( func->type == FUNC_ASPEC && func->max_param > MAX_ASPEC_PARAMS ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "action-special has more than %d parameters", MAX_ASPEC_PARAMS );
      s_bail( semantic );
   }
   return true;
}

void test_param( struct semantic* semantic, struct func* func,
   struct param* param ) {
   param->object.resolved =
      test_param_spec( semantic, param ) &&
      test_param_ref( semantic, param ) &&
      test_param_name( semantic, param ) &&
      test_param_after_ref( semantic, func, param );
   if ( param->object.resolved ) {
      calc_param_size( param );
   }
}

bool test_param_spec( struct semantic* semantic, struct param* param ) {
   if ( param->spec == SPEC_NAME ) {
      struct name_spec_test test;
      init_name_spec_test( &test, param->path, param->ref, NULL );
      test_name_spec( semantic, &test );
      param->ref = test.ref;
      param->structure = test.structure;
      param->enumeration = test.enumeration;
      param->spec = test.spec;
   }
   if ( param->spec == SPEC_VOID &&
      ! ( param->ref && param->ref->type == REF_FUNCTION ) ) {
      s_diag( semantic, DIAG_POS_ERR, &param->object.pos,
         "parameter of void type" );
      s_bail( semantic );
   }
   if ( param->spec == SPEC_STRUCT ) {
      return param->structure->object.resolved;
   }
   else if ( param->spec == SPEC_ENUM ) {
      return param->enumeration->object.resolved;
   }
   else {
      return true;
   }
}

bool test_param_ref( struct semantic* semantic, struct param* param ) {
   struct ref_test test;
   init_ref_test( &test, param->ref, param->spec );
   test_ref( semantic, &test );
   return true;
}

bool test_param_name( struct semantic* semantic, struct param* param ) {
   if ( param->name ) {
      s_bind_name( semantic, param->name, &param->object );
   }
   return true;
}

bool test_param_after_ref( struct semantic* semantic, struct func* func,
   struct param* param ) {
   if ( param->default_value ) {
      return test_param_default_value( semantic, func, param );
   }
   else {
      return true;
   }
}

bool test_param_default_value( struct semantic* semantic, struct func* func,
   struct param* param ) {
   struct type_info type;
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr_type( semantic, &expr, &type, param->default_value );
   if ( expr.undef_erred ) {
      return false;
   }
   struct type_info param_type;
   s_init_type_info( &param_type, param->ref, param->structure,
      param->enumeration, NULL, param->spec );
   if ( ! s_instance_of( &param_type, &type ) ) {
      default_value_mismatch( semantic, func, param, &param_type, &type,
         &param->default_value->pos );
      s_bail( semantic );
   }
   if ( s_is_ref_type( &type ) && expr.var ) {
      expr.var->addr_taken = true;
   }
   if ( expr.func && expr.func->type == FUNC_USER ) {
      struct func_user* impl = expr.func->impl;
      if ( impl->nested ) {
         s_diag( semantic, DIAG_POS_ERR, &param->default_value->pos,
            "nested function used as a default value" );
         s_bail( semantic );
      }
   }
   return true;
}

void default_value_mismatch( struct semantic* semantic, struct func* func,
   struct param* param, struct type_info* param_type, struct type_info* type,
   struct pos* pos ) {
   struct str type_s;
   str_init( &type_s );
   s_present_type( type, &type_s );
   struct str param_type_s;
   str_init( &param_type_s );
   s_present_type( param_type, &param_type_s );
   s_diag( semantic, DIAG_POS_ERR, pos,
      "default-value type (`%s`) different from parameter type (`%s`) "
      "(in parameter %d)", type_s.value, param_type_s.value,
      get_param_number( func, param ) );
   str_deinit( &type_s );
   str_deinit( &param_type_s );
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
   struct builtin_aliases builtin_aliases;
   init_builtin_aliases( semantic, func, &builtin_aliases );
   struct func_user* impl = func->impl;
   s_add_scope( semantic, true );
   struct param* param = func->params;
   while ( param ) {
      if ( param->name ) {
         s_bind_name( semantic, param->name, &param->object );
      }
      param = param->next;
   }
   struct func_test test;
   init_func_test( &test, func, &impl->labels, &impl->funcscope_vars );
   if ( ! impl->nested ) {
      semantic->topfunc_test = &test;
   }
   struct func_test* prev = semantic->func_test;
   semantic->func_test = &test;
   bind_builtin_aliases( semantic, &builtin_aliases );
   s_test_body( semantic, &impl->body->node );
   semantic->func_test = prev;
   impl->returns = test.returns;
   if ( ! impl->nested ) {
      impl->nested_funcs = test.nested_funcs;
      semantic->topfunc_test = NULL;
   }
   s_pop_scope( semantic );
   deinit_builtin_aliases( semantic, &builtin_aliases );
}

// THOUGHT: Instead of creating an alias for each builtin object, maybe making
// a link to a builtin namespace would be better.
void init_builtin_aliases( struct semantic* semantic, struct func* func,
   struct builtin_aliases* aliases ) {
   if ( func->msgbuild ) {
      struct func_user* impl = func->impl;
      struct alias* alias = s_alloc_alias();
      alias->object.pos = impl->body->pos;
      alias->object.resolved = true;
      alias->target = &semantic->task->append_func->object;
      alias->implicit = true;
      aliases->append = alias;
   }
   else {
      aliases->append = NULL;
   }
}

void bind_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases ) {
   if ( aliases->append ) {
      struct name* name = t_extend_name( semantic->ns->body, "append" );
      s_bind_name( semantic, name, &aliases->append->object );
   }
}

void deinit_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases ) {
   if ( aliases->append ) {
      mem_free( aliases->append );
   }
}

void init_func_test( struct func_test* test, struct func* func,
   struct list* labels, struct list* funcscope_vars ) {
   test->func = func;
   test->labels = labels;
   test->funcscope_vars = funcscope_vars;
   test->nested_funcs = NULL;
   test->returns = NULL;
   test->script = ( func == NULL );
}

void s_test_nested_func( struct semantic* semantic, struct func* func ) {
   s_test_func( semantic, func );
   if ( func->type == FUNC_USER ) {
      s_test_func_body( semantic, func );
      struct func_user* impl = func->impl;
      impl->next_nested = semantic->topfunc_test->nested_funcs;
      semantic->topfunc_test->nested_funcs = func;
   }
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
            "script number not constant" );
         s_bail( semantic );
      }
      if ( semantic->lib->type_mode == TYPEMODE_STRONG ) {
         script->named_script = ( script->number->spec == SPEC_STR );
      }
      else {
         script->named_script =
            ( script->number->root->type == NODE_INDEXED_STRING_USAGE );
      }
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
   s_add_scope( semantic, true );
   struct param* param = script->params;
   while ( param ) {
      if ( param->name ) {
         s_bind_name( semantic, param->name, &param->object );
      }
      param->object.resolved = true;
      param = param->next;
   }
   struct func_test test;
   init_func_test( &test, NULL, &script->labels, &script->funcscope_vars );
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
      return ( dim->length * dim->element_size );
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
            alloc->index += ( dim->next->length *
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
            alloc->index += dim->next->length;
         }
         else {
            alloc->index += dim->element_size;
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
            alloc->index += ( member->dim->length *
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
            alloc->index += member->dim->length;
         }
         else {
            alloc->index += member->size;
         }
         alloc->value = alloc->value->next;
      }
      member = member->next;
      initial = initial->next;
   }
}