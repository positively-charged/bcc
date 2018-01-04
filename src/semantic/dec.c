#include <string.h>
#include <limits.h>

#include "phase.h"

#define SCRIPT_MIN_NUM 0
#define SCRIPT_MAX_NUM 32767

struct enumeration_test {
   int value;
};

struct spec_test {
   struct object* object;
   struct object* type_alias;
   struct ref* ref;
   struct structure* structure;
   struct enumeration* enumeration;
   struct dim* dim;
   struct path* path;
   int spec;
   bool public_spec;
};

struct ref_test {
   struct ref* ref;
   int spec;
   bool need_public_spec;
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
   bool has_str;
};

struct scalar_initz_test {
   struct type_info initz_type;
   struct initz_test* initz_test;
   struct type_info* type;
   bool constant;
   bool has_str;
};

struct initz_pres {
   struct str output;
   bool member_last;
};

struct param_list_test {
   struct func* func;
   struct ref_func* func_ref;
   struct param* params;
   struct param* prev_param;
   bool need_public_spec;
};

struct builtin_aliases {
   struct {
      struct alias alias;
      struct magic_id magic_id;
   } name;
};

struct builtin_script_aliases {
   struct {
      struct alias alias;
      struct magic_id magic_id;
   } name;
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
   struct structure* structure, struct structure_member* member );
static bool test_member_name( struct semantic* semantic,
   struct structure* structure, struct structure_member* member );
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
static void init_spec_test( struct spec_test* test, struct path* path,
   struct object* object, struct ref* ref, struct structure* structure,
   struct enumeration* enumeration, struct dim* dim, int spec );
static void test_spec( struct semantic* semantic, struct spec_test* test );
static void find_name_spec( struct semantic* semantic,
   struct spec_test* test );
static void merge_type( struct semantic* semantic, struct spec_test* test,
   struct type_alias* alias );
static void merge_ref( struct semantic* semantic, struct spec_test* test,
   struct type_alias* alias );
static void merge_func_type( struct semantic* semantic, struct spec_test* test,
   struct func* func_alias );
static bool test_var_ref( struct semantic* semantic, struct var* var );
static void init_ref_test( struct ref_test* test, struct ref* ref, int spec,
   bool need_public_spec );
static bool test_ref( struct semantic* semantic, struct ref_test* test );
static bool test_ref_struct( struct semantic* semantic, struct ref_test* test,
   struct ref_struct* structure );
static bool test_ref_array( struct semantic* semantic, struct ref_test* test,
   struct ref_array* array );
static bool test_ref_func( struct semantic* semantic, struct ref_test* test,
   struct ref_func* func );
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
static void confirm_enum_default_initz( struct semantic* semantic,
   struct enumeration* enumeration, struct structure_member* member,
   int spec, struct pos* pos );
static struct enumeration* find_nondefaultinitz_enum(
   struct enumeration* enumeration, struct structure_member* member );
static const char* get_enum_default_initz_text(
   struct enumeration* enumeration );
static void confirm_dim_length( struct semantic* semantic, struct var* var );
static bool test_var_finish( struct semantic* semantic, struct var* var );
static bool test_external_var( struct semantic* semantic, struct var* var );
static void test_worldglobal_var( struct semantic* semantic, struct var* var );
static void describe_var( struct var* var );
static bool is_auto_var( struct var* var );
static void test_auto_var( struct semantic* semantic, struct var* var );
static void assign_inferred_type( struct semantic* semantic, struct var* var,
   struct type_info* type );
static void init_initz_test( struct initz_test* test,
   struct initz_test* parent, struct var* var, int spec, struct dim* dim,
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
static void test_special_func( struct semantic* semantic, struct func* func );
static bool test_special_func_param_list( struct semantic* semantic,
   struct func* func );
static void test_special_func_param( struct semantic* semantic,
   struct func* func, struct param* param );
static bool test_special_func_param_type( struct semantic* semantic,
   struct func* func, struct param* param );
static bool test_special_func_return_type( struct semantic* semantic,
   struct func* func );
static void test_func( struct semantic* semantic, struct func* func );
static bool test_func_return_spec( struct semantic* semantic,
   struct func* func );
static bool test_func_ref( struct semantic* semantic, struct func* func );
static bool test_func_name( struct semantic* semantic, struct func* func );
static bool test_func_after_name( struct semantic* semantic,
   struct func* func );
static void init_param_list_test( struct param_list_test* test,
   struct func* func, struct ref_func* func_ref, bool need_public_spec );
static bool test_param_list( struct semantic* semantic,
   struct param_list_test* test );
static void test_param( struct semantic* semantic,
   struct param_list_test* test, struct param* param );
static bool test_param_spec( struct semantic* semantic,
   struct param_list_test* test, struct param* param );
static bool test_param_ref( struct semantic* semantic,
   struct param_list_test* test, struct param* param );
static bool test_param_after_name( struct semantic* semantic,
   struct param_list_test* test, struct param* param );
static bool test_param_default_value( struct semantic* semantic,
   struct param_list_test* test, struct param* param );
static void default_value_mismatch( struct semantic* semantic,
   struct func* func, struct param* param, struct type_info* param_type,
   struct type_info* type, struct pos* pos );
static int get_param_number( struct func* func, struct param* target );
static bool test_external_func( struct semantic* semantic, struct func* func );
static void init_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases, struct func* func );
static void bind_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases );
static void init_func_test( struct func_test* test, struct func_test* parent,
   struct func* func, struct list* labels, struct list* funcscope_vars,
   struct script* script );
static void calc_param_size( struct param* param );
static int calc_size( struct dim* dim, struct structure* structure,
   struct ref* ref );
static void calc_struct_size( struct structure* structure );
static void make_value_list( struct value_list*, struct multi_value* );
static void alloc_value_index( struct value_index_alloc*, struct multi_value*,
   struct structure*, struct dim* );
static void alloc_value_index_struct( struct value_index_alloc*,
   struct multi_value*, struct structure* );
static void test_script_number( struct semantic* semantic,
   struct script* script );
static void test_nonzero_script_number( struct semantic* semantic,
   struct script* script );
static void test_script_param_list( struct semantic* semantic,
   struct script* script );
static void test_script_body( struct semantic* semantic,
   struct script* script );
static void init_builtin_script_aliases( struct semantic* semantic,
   struct builtin_script_aliases* aliases, struct script* script );
static void bind_builtin_script_aliases( struct semantic* semantic,
   struct builtin_script_aliases* aliases );

void s_test_constant( struct semantic* semantic, struct constant* constant ) {
   // Name.
   if ( semantic->in_localscope ) {
      s_bind_local_name( semantic, constant->name, &constant->object,
         constant->force_local_scope );
   }
   // Expression.
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, constant->value_node );
   if ( expr.undef_erred ) {
      return;
   }
   if ( ! constant->value_node->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &constant->value_node->pos,
         "expression not constant" );
      s_bail( semantic );
   }
   if ( s_describe_type( &expr.type ) != TYPEDESC_PRIMITIVE ) {
      s_diag( semantic, DIAG_POS_ERR, &constant->value_node->pos,
         "expression not of primitive type" );
      s_bail( semantic );
   }
   // Finish.
   constant->spec = constant->value_node->spec;
   constant->value = constant->value_node->value;
   constant->has_str = constant->value_node->has_str;
   constant->object.resolved = true;
}

void s_test_enumeration( struct semantic* semantic,
   struct enumeration* enumeration ) {
   if ( enumeration->name ) {
      if ( semantic->in_localscope ) {
         s_bind_local_name( semantic, enumeration->name, &enumeration->object,
            enumeration->force_local_scope );
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

static void test_enumerator( struct semantic* semantic,
   struct enumeration_test* test, struct enumeration* enumeration,
   struct enumerator* enumerator ) {
   // Test name.
   if ( semantic->in_localscope ) {
      s_bind_local_name( semantic, enumerator->name, &enumerator->object,
         enumeration->force_local_scope );
   }
   // Test initializer.
   if ( enumerator->initz ) {
      struct expr_test expr;
      s_init_expr_test_enumerator( &expr, enumeration );
      s_test_expr( semantic, &expr, enumerator->initz );
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
      if ( ! s_instance_of( &base_type, &expr.type ) ) {
         s_type_mismatch( semantic, "enumerator", &expr.type,
            "enumeration-base", &base_type, &enumerator->object.pos );
         s_bail( semantic );
      }
      test->value = enumerator->initz->value;
      enumerator->has_str = enumerator->initz->has_str;
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
   if ( enumerator->value == 0 ) {
      enumeration->default_initz = true;
   }
}

void s_test_struct( struct semantic* semantic, struct structure* structure ) {
   structure->object.resolved =
      test_struct_name( semantic, structure ) &&
      test_struct_body( semantic, structure );
   if ( structure->object.resolved ) {
      calc_struct_size( structure );
   }
}

static bool test_struct_name( struct semantic* semantic,
   struct structure* structure ) {
   if ( semantic->in_localscope ) {
      s_bind_local_name( semantic, structure->name, &structure->object,
         structure->force_local_scope );
   }
   return true;
}

static bool test_struct_body( struct semantic* semantic,
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

static void test_member( struct semantic* semantic,
   struct structure* structure, struct structure_member* member ) {
   member->object.resolved =
      test_member_spec( semantic, structure, member ) &&
      test_member_ref( semantic, structure, member ) &&
      test_member_name( semantic, structure, member ) &&
      test_member_dim( semantic, member );
   if ( member->object.resolved ) {
      if ( member->structure && member->structure->has_ref_member ) {
         structure->has_ref_member = true;
         if ( member->structure->has_mandatory_ref_member ) {
            structure->has_mandatory_ref_member = true;
         }
      }
   }
}

static bool test_member_spec( struct semantic* semantic,
   struct structure* structure, struct structure_member* member ) {
   struct spec_test test;
   init_spec_test( &test, member->path, &member->object, member->ref,
      member->structure, member->enumeration, member->dim, member->spec );
   test_spec( semantic, &test );
   member->ref = test.ref;
   member->structure = test.structure;
   member->enumeration = test.enumeration;
   member->dim = test.dim;
   member->spec = test.spec;
   // Void members not allowed.
   if ( member->spec == SPEC_VOID && ! member->ref ) {
      s_diag( semantic, DIAG_POS_ERR, &member->object.pos, member->dim ?
         "array struct member has void element type" :
         "struct member has void type" );
      s_bail( semantic );
   }
   // Every member of a public structure must have a public type.
   if ( ! semantic->in_localscope && ! structure->hidden &&
      ! test.public_spec ) {
      s_diag( semantic, DIAG_POS_ERR, &member->object.pos,
         "member of non-private struct has a private type" );
      s_bail( semantic );
   }
   if ( member->spec == SPEC_STRUCT ) {
      if ( member->structure->object.resolved || member->ref ) {
         return true;
      }
      else {
         if ( semantic->trigger_err ) {
            s_diag( semantic, DIAG_POS_ERR, &member->object.pos,
               "struct member has type that is unresolvable" );
            s_bail( semantic );
         }
         return false;
      }
   }
   else if ( member->spec == SPEC_ENUM ) {
      return member->enumeration->object.resolved;
   }
   else {
      return true;
   }
}

static bool test_member_ref( struct semantic* semantic,
   struct structure* structure, struct structure_member* member ) {
   struct ref_test test;
   init_ref_test( &test, member->ref, member->spec, ( ! structure->hidden ) );
   return test_ref( semantic, &test );
}

static bool test_member_name( struct semantic* semantic,
   struct structure* structure, struct structure_member* member ) {
   if ( semantic->in_localscope ) {
      s_bind_local_name( semantic, member->name, &member->object,
         structure->force_local_scope );
   }
   return true;
}

static bool test_member_dim( struct semantic* semantic,
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

static bool test_typedef_spec( struct semantic* semantic,
   struct type_alias* alias ) {
   struct spec_test test;
   init_spec_test( &test, alias->path, &alias->object, alias->ref,
      alias->structure, alias->enumeration, alias->dim, alias->spec );
   test_spec( semantic, &test );
   alias->ref = test.ref;
   alias->structure = test.structure;
   alias->enumeration = test.enumeration;
   alias->dim = test.dim;
   alias->spec = test.spec;
   // Type alias must have a valid type.
   if ( alias->dim && ! alias->ref && alias->spec == SPEC_VOID ) {
      s_diag( semantic, DIAG_POS_ERR, &alias->object.pos,
         "type alias of an array of void elements" );
      s_bail( semantic );
   }
   // Public type alias must have a public type.
   if ( ! semantic->in_localscope && ! alias->hidden && ! test.public_spec ) {
      s_diag( semantic, DIAG_POS_ERR, &alias->object.pos,
         "non-private type alias has a private type" );
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

static bool test_typedef_ref( struct semantic* semantic,
   struct type_alias* alias ) {
   struct ref_test test;
   init_ref_test( &test, alias->ref, alias->spec, false );
   return test_ref( semantic, &test );
}

static bool test_typedef_name( struct semantic* semantic,
   struct type_alias* alias ) {
   if ( semantic->in_localscope ) {
      s_bind_local_name( semantic, alias->name, &alias->object,
         alias->force_local_scope );
   }
   return true;
}

static bool test_typedef_dim( struct semantic* semantic,
   struct type_alias* alias ) {
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
   if ( is_auto_var( var ) ) {
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

static bool test_var_spec( struct semantic* semantic, struct var* var ) {
   struct spec_test test;
   init_spec_test( &test, var->type_path, &var->object, var->ref,
      var->structure, var->enumeration, var->dim, var->spec );
   test.object = &var->object;
   test_spec( semantic, &test );
   var->ref = test.ref;
   var->structure = test.structure;
   var->enumeration = test.enumeration;
   var->dim = test.dim;
   var->spec = test.spec;
   // Variable must be of a valid type.
   if ( var->spec == SPEC_VOID && ! var->ref ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos, var->dim ?
         "array has void element type" : "variable has void type" );
      s_bail( semantic );
   }
   // Public (visible to a user of a library) variable must have a public type.
   if ( ! semantic->in_localscope && ! var->hidden && ! test.public_spec ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "non-private variable has a private type" );
      s_bail( semantic );
   }
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

static void init_spec_test( struct spec_test* test, struct path* path,
   struct object* object, struct ref* ref, struct structure* structure,
   struct enumeration* enumeration, struct dim* dim, int spec ) {
   test->object = object;
   test->type_alias = NULL;
   test->ref = ref;
   test->structure = structure;
   test->enumeration = enumeration;
   test->dim = dim;
   test->path = path;
   test->spec = spec;
   test->public_spec = false;
}

static void test_spec( struct semantic* semantic, struct spec_test* test ) {
   // Find the specifier.
   if ( test->spec >= SPEC_NAME ) {
      find_name_spec( semantic, test );
   }
   // Determine the visibility of the specifier.
   switch ( test->spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
   case SPEC_BOOL:
   case SPEC_STR:
   case SPEC_VOID:
   case SPEC_AUTO:
   case SPEC_AUTOENUM:
      test->public_spec = true;
      break;
   case SPEC_ENUM:
      test->public_spec = ( ! test->enumeration->hidden );
      break;
   case SPEC_STRUCT:
      test->public_spec = ( ! test->structure->hidden );
      break;
   case SPEC_NAME:
      test->public_spec = ( ( test->type_alias->node.type == NODE_FUNC &&
         ! ( ( struct func* ) test->type_alias )->hidden ) ||
         ( test->type_alias->node.type == NODE_TYPE_ALIAS &&
         ! ( ( struct type_alias* ) test->type_alias )->hidden ) );
      break;
   default:
      S_UNREACHABLE( semantic );
   }
   // Expand type alias.
   if ( test->spec == SPEC_NAME ) {
      if ( test->type_alias->node.type == NODE_FUNC ) {
         merge_func_type( semantic, test,
            ( struct func* ) test->type_alias );
      }
      else {
         merge_type( semantic, test,
            ( struct type_alias* ) test->type_alias );
      }
   }
}

static void find_name_spec( struct semantic* semantic,
   struct spec_test* test ) {
   int decoded_spec = test->spec - SPEC_NAME;
   if ( decoded_spec == SPEC_ENUM ) {
      struct follower follower;
      s_init_follower( &follower, test->path, NODE_ENUMERATION );
      s_follow_path( semantic, &follower );
      test->enumeration = follower.result.enumeration;
      test->spec = SPEC_ENUM;
   }
   else if ( decoded_spec == SPEC_STRUCT ) {
      struct follower follower;
      s_init_follower( &follower, test->path, NODE_STRUCTURE );
      s_follow_path( semantic, &follower );
      test->structure = follower.result.structure;
      test->spec = SPEC_STRUCT;
   }
   else {
      struct follower follower;
      s_init_follower( &follower, test->path, NODE_NONE );
      s_follow_path( semantic, &follower );
      struct object* object = follower.result.object;
      if ( object->node.type == NODE_TYPE_ALIAS ||
         ( object->node.type == NODE_FUNC &&
         ( ( struct func* ) object )->type == FUNC_ALIAS ) ) {
         test->type_alias = object;
      }
      else {
         struct path* path = s_last_path_part( test->path );
         s_diag( semantic, DIAG_POS_ERR, &path->pos,
            "`%s` not a type synonym", path->text );
         s_bail( semantic );
      }
   }
}

struct path* s_last_path_part( struct path* path ) {
   while ( path->next ) {
      path = path->next;
   }
   return path;
}

static void merge_type( struct semantic* semantic, struct spec_test* test,
   struct type_alias* alias ) {
   test->spec = alias->spec;
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

static void merge_ref( struct semantic* semantic, struct spec_test* test,
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
            alias->enumeration, alias->dim, alias->spec, STORAGE_MAP );
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
      S_UNREACHABLE( semantic );
   }
}

static void merge_func_type( struct semantic* semantic, struct spec_test* test,
   struct func* alias ) {
   test->spec = alias->return_spec;
   test->structure = alias->structure;
   test->enumeration = alias->enumeration;
   struct ref_func* func = t_alloc_ref_func();
   struct path* path = s_last_path_part( test->path );
   func->ref.pos = path->pos;
   func->params = alias->params;
   func->min_param = alias->min_param;
   func->max_param = alias->max_param;
   if ( test->ref ) {
      struct ref* ref = test->ref;
      struct ref* prev_ref = NULL;
      while ( ref->next ) {
         prev_ref = ref; 
         ref = ref->next;
      }
      if ( ref->type == REF_STRUCTURE ) {
         func->ref.pos = ref->pos;
         func->ref.nullable = ref->nullable;
         if ( prev_ref ) {
            prev_ref->next = &func->ref;
         }
         else {
            test->ref = &func->ref;
         }
         mem_free( ref );
      }
      else if ( ref->type == REF_FUNCTION ) {
         s_diag( semantic, DIAG_POS_ERR, &ref->pos,
            "function returning a function" );
         s_bail( semantic );
      }
      else {
         ref->next = &func->ref;
      }
   }
   else if ( test->dim ) {
      s_diag( semantic, DIAG_POS_ERR, &test->object->pos,
         "array of functions" );
      s_bail( semantic );
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &test->object->pos,
         "variable of function type" );
      s_bail( semantic );
   }
}

static bool test_var_ref( struct semantic* semantic, struct var* var ) {
   struct ref_test test;
   init_ref_test( &test, var->ref, var->spec,
      ( ! semantic->in_localscope && ! var->hidden ) );
   return test_ref( semantic, &test );
}

static void init_ref_test( struct ref_test* test, struct ref* ref, int spec,
   bool need_public_spec ) {
   test->ref = ref;
   test->spec = spec;
   test->need_public_spec = need_public_spec;
}

static bool test_ref( struct semantic* semantic, struct ref_test* test ) {
   struct ref* ref = test->ref;
   while ( ref ) {
      bool resolved = false;
      switch ( ref->type ) {
      case REF_STRUCTURE:
         resolved = test_ref_struct( semantic, test,
            ( struct ref_struct* ) ref );
         break;
      case REF_ARRAY:
         resolved = test_ref_array( semantic, test,
            ( struct ref_array* ) ref );
         break;
      case REF_FUNCTION:
         resolved = test_ref_func( semantic, test,
            ( struct ref_func* ) ref );
         break;
      default:
         S_UNREACHABLE( semantic );
      }
      if ( ! resolved ) {
         return false;
      }
      ref = ref->next;
   }
   return true;
}

static bool test_ref_struct( struct semantic* semantic, struct ref_test* test,
   struct ref_struct* structure ) {
   if ( test->spec != SPEC_STRUCT ) {
      s_diag( semantic, DIAG_POS_ERR, &structure->ref.pos,
         "invalid reference type (expecting reference-to-struct)" );
      s_bail( semantic );
   }
   return true;
}

static bool test_ref_array( struct semantic* semantic, struct ref_test* test,
   struct ref_array* array ) {
   if ( test->spec == SPEC_VOID && ! array->ref.next ) {
      s_diag( semantic, DIAG_POS_ERR, &array->ref.pos,
         "array reference has void element type" );
      s_bail( semantic );
   }
   return true;
}

static bool test_ref_func( struct semantic* semantic, struct ref_test* test,
   struct ref_func* func ) {
   if ( test->spec == SPEC_STRUCT && ! func->ref.next ) {
      s_diag( semantic, DIAG_POS_ERR, &func->ref.pos,
         "function reference returning a struct (a struct can "
         "only be returned by reference)" );
      s_bail( semantic );
   }
   s_add_scope( semantic, true );
   struct param_list_test param_test;
   init_param_list_test( &param_test, NULL, func, test->need_public_spec );
   bool resolved = test_param_list( semantic, &param_test );
   s_pop_scope( semantic );
   return resolved;
}

static bool test_var_name( struct semantic* semantic, struct var* var ) {
   if ( ! var->anon ) {
      if ( semantic->in_localscope ) {
         s_bind_local_name( semantic, var->name, &var->object,
            var->force_local_scope );
      }
   }
   return true;
}

static bool test_var_dim( struct semantic* semantic, struct var* var ) {
   if ( var->dim ) {
      struct dim_test test;
      init_dim_test( &test, var->dim, false );
      test_dim_list( semantic, &test );
      if ( test.resolved && semantic->lang == LANG_ACS ) {
         struct dim* dim = var->dim;
         while ( dim ) {
            if ( dim->length_node && ( dim == var->dim &&
               ( var->storage == STORAGE_WORLD ||
               var->storage == STORAGE_GLOBAL ) ) ) {
               s_diag( semantic, DIAG_POS_ERR, &dim->pos,
                  "length specified for dimension of %s array",
                  t_get_storage_name( var->storage ) );
               s_bail( semantic );
            }
            if ( ! dim->length_node && ! ( dim == var->dim &&
               ( var->storage == STORAGE_WORLD ||
               var->storage == STORAGE_GLOBAL ) ) ) {
               s_diag( semantic, DIAG_POS_ERR, &dim->pos,
                  "dimension missing length" );
               s_bail( semantic );
            }
            dim = dim->next;
         }
      }
      return test.resolved;
   }
   else {
      return true;
   }
}

static void init_dim_test( struct dim_test* test, struct dim* dim,
   bool member ) {
   test->dim = dim;
   test->resolved = false;
   test->member = member;
}

static void test_dim_list( struct semantic* semantic, struct dim_test* test ) {
   struct dim* dim = test->dim;
   while ( dim ) {
      if ( ! test_dim( semantic, test, dim ) ) {
         return;
      }
      dim = dim->next;
   }
   test->resolved = true;
}

static bool test_dim( struct semantic* semantic, struct dim_test* test,
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

static bool test_dim_length( struct semantic* semantic, struct dim_test* test,
   struct dim* dim ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, dim->length_node );
   if ( expr.undef_erred ) {
      return false;
   }
   if ( ! s_is_int_value( &expr.type ) ) {
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
         "dimension length less than or equal to 0" );
      s_bail( semantic );
   }
   dim->length = dim->length_node->value;
   return true;
}

static bool test_var_initz( struct semantic* semantic, struct var* var ) {
   if ( ! var->imported ) {
      if ( var->initial ) {
         return test_object_initz( semantic, var );
      }
      else {
         // References must always have a valid value.
         if ( ( ( var->ref && ! var->ref->nullable ) || ( var->structure &&
            var->structure->has_mandatory_ref_member ) ) && ! var->external ) {
            refnotinit_var( semantic, var );
            s_bail( semantic );
         }
         if ( ! var->ref && ( var->spec == SPEC_ENUM ||
            var->spec == SPEC_STRUCT ) ) {
            confirm_enum_default_initz( semantic, var->enumeration,
               var->spec == SPEC_STRUCT ? var->structure->member : NULL,
               var->spec, &var->object.pos );
         }
      }
   }
   // All dimensions of implicit length need to have a length.
   if ( var->dim ) {
      confirm_dim_length( semantic, var );
   }
   return true;
}

static void refnotinit_var( struct semantic* semantic, struct var* var ) {
   if ( var->dim || ( var->structure && ! var->ref ) ) {
      struct initz_pres pres;
      init_initz_pres( &pres );
      struct str name;
      str_init( &name );
      t_copy_name( var->name, &name );
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

static bool test_object_initz( struct semantic* semantic, struct var* var ) {
   if ( var->anon && ! ( var->dim ||
      ( var->spec == SPEC_STRUCT && ! var->ref ) ) ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "compound literal not of array type or of structure type" );
      s_bail( semantic );
   }
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
         var->enumeration, var->dim, var->spec, var->storage );
      bool resolved = test_value( semantic, &test, &type,
         ( struct value* ) var->initial );
      if ( ! resolved ) {
         return false;
      }
   }
   var->initial_has_str = test.has_str;
   // Global and world variables cannot be defined.
   if ( var->storage == STORAGE_WORLD || var->storage == STORAGE_GLOBAL ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "%s variable declaration has an initializer",
         t_get_storage_name( var->storage ) );
      s_bail( semantic );
   }
   return true;
}

static void init_initz_test( struct initz_test* test,
   struct initz_test* parent, struct var* var, int spec, struct dim* dim,
   struct structure* structure, struct enumeration* enumeration,
   struct ref* ref, bool constant ) {
   test->var = var;
   test->ref = ref;
   test->structure = structure;
   test->member = NULL;
   test->enumeration = enumeration;
   test->dim = dim;
   test->parent = parent;
   test->spec = spec;
   test->count = 0;
   test->constant = constant;
   test->has_str = false;
}

static void init_root_initz_test( struct initz_test* test, struct var* var ) {
   init_initz_test( test, NULL, var, var->spec, var->dim, var->structure,
      var->enumeration, var->ref, var->is_constant_init );
}

static bool test_multi_value( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value ) {
   if ( test->dim ) {
      return test_multi_value_array( semantic, test, multi_value );
   }
   else if ( test->structure && ! test->ref ) {
      return test_multi_value_struct( semantic, test, multi_value );
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &multi_value->pos,
         "too many brace initializers" );
      s_bail( semantic );
      return false;
   }
}

static bool test_multi_value_array( struct semantic* semantic,
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
   if ( test->count < test->dim->length ) {
      // Every non-nullable reference element must be initialized.
      if ( ( test->ref && ! test->ref->nullable ) || ( test->structure &&
         test->structure->has_mandatory_ref_member ) ) {
         refnotinit_array( semantic, test, multi_value );
         s_bail( semantic );
      }
      // Every enum element must be initialized with a valid enumerator.
      if ( ! test->ref && ( test->spec == SPEC_ENUM ||
         test->spec == SPEC_STRUCT ) ) {
         confirm_enum_default_initz( semantic, test->enumeration,
            test->spec == SPEC_STRUCT ? test->structure->member : NULL,
            test->spec, &test->var->object.pos );
      }
   }
   return true;
}

static bool test_multi_value_array_child( struct semantic* semantic,
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
      init_initz_test( &nested_test, test, test->var, test->spec,
         test->dim->next, test->structure, test->enumeration, test->ref,
         test->constant );
      bool resolved = test_multi_value( semantic, &nested_test,
         ( struct multi_value* ) initial );
      if ( nested_test.has_str ) {
         test->has_str = true;
      }
      return resolved;
   }
   else {
      struct type_info type;
      s_init_type_info( &type, test->ref, test->structure,
         test->enumeration, test->dim->next, test->spec, test->var->storage );
      return test_value( semantic, test, &type,
         ( struct value* ) initial );
   }
}

static void refnotinit_array( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value ) {
   struct initz_pres pres;
   init_initz_pres( &pres );
   present_parent( &pres, test );
   if ( ! test->ref ) {
      present_ref_element( &pres, NULL, test->structure->member );
   }
   refnotinit( semantic, &pres, &multi_value->pos );
}

static bool test_multi_value_struct( struct semantic* semantic,
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
   while ( test->member ) {
      // Every non-nullable reference member must be initialized.
      if ( ( test->member->ref && ! test->member->ref->nullable ) || (
         test->member->structure &&
         test->member->structure->has_mandatory_ref_member ) ) {
         refnotinit_struct( semantic, test, multi_value );
         s_bail( semantic );
      }
      // Every enum member must be initialized with a valid enumerator.
      if ( ! test->member->ref && ( test->member->spec == SPEC_ENUM ||
         test->member->spec == SPEC_STRUCT ) ) {
         confirm_enum_default_initz( semantic, test->member->enumeration,
            test->member, test->member->spec, &test->var->object.pos );
      }
      test->member = test->member->next;
   }
   return true;
}

static bool test_multi_value_struct_child( struct semantic* semantic,
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
      init_initz_test( &nested_test, test, test->var, member->spec,
         member->dim, member->structure, member->enumeration, member->ref,
         test->constant );
      bool resolved = test_multi_value( semantic, &nested_test,
         ( struct multi_value* ) initial );
      if ( nested_test.has_str ) {
         test->has_str = true;
      }
      return resolved;
   }
   else {
      struct type_info type;
      s_init_type_info( &type, member->ref, member->structure,
         member->enumeration, member->dim, member->spec, test->var->storage );
      return test_value( semantic, test, &type,
         ( struct value* ) initial );
   }
}

static void refnotinit_struct( struct semantic* semantic,
   struct initz_test* test, struct multi_value* multi_value ) {
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

static bool test_value( struct semantic* semantic, struct initz_test* test,
   struct type_info* type, struct value* value ) {
   if ( s_is_scalar( type ) ) {
      struct scalar_initz_test scalar_test;
      init_scalar_initz_test( &scalar_test, test, type, test->constant );
      if ( ! test_scalar_initz( semantic, &scalar_test, value ) ) {
         return false;
      }
      if ( scalar_test.has_str ) {
         test->has_str = true;
      }
      return true;
   }
   else if ( semantic->lang == LANG_BCS && s_is_onedim_int_array( type ) ) {
      return test_string_initz( semantic, type->dim, value );
   }
   else {
      s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
         "missing brace initializer" );
      s_bail( semantic );
      return false;
   }
}

static void init_scalar_initz_test( struct scalar_initz_test* test,
   struct initz_test* initz_test, struct type_info* type, bool constant ) {
   test->initz_test = initz_test;
   test->type = type;
   test->constant = constant;
   test->has_str = false;
}

static void init_scalar_initz_test_auto( struct scalar_initz_test* test,
   bool constant ) {
   init_scalar_initz_test( test, NULL, NULL, constant );
}

static bool test_scalar_initz( struct semantic* semantic,
   struct scalar_initz_test* test, struct value* value ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, value->expr );
   s_init_type_info_copy( &test->initz_type, &expr.type );
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
      if ( impl->local ) {
         s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
            "local function used as an initializer" );
         s_bail( semantic );
      }
   }
   test->has_str = expr.has_str;
   switch ( s_describe_type( &expr.type ) ) {
   case TYPEDESC_ARRAYREF:
      value->more.arrayref.var = expr.var;
      value->more.arrayref.structure_member = expr.structure_member;
      value->more.arrayref.diminfo = expr.dim_depth;
      value->type = VALUE_ARRAYREF;
      break;
   case TYPEDESC_STRUCTREF:
      value->more.structref.var = expr.var;
      value->type = VALUE_STRUCTREF;
      break;
   case TYPEDESC_FUNCREF:
      value->more.funcref.func = expr.func;
      value->type = VALUE_FUNCREF;
      break;
   case TYPEDESC_NULLREF:
      break;
   case TYPEDESC_PRIMITIVE:
      if ( expr.has_str ) {
         // In ACS, one can add strings and numbers, so an invalid string index
         // is possible. Make sure we have a valid string.
         struct indexed_string* string = t_lookup_string( semantic->task,
            value->expr->value );
         if ( string ) {
            value->more.string.string = string;
            value->type = VALUE_STRING;
         }
      }
      break;
   default:
      S_UNREACHABLE( semantic );
   }
   // Note which variable is passed by reference.
   if ( s_is_ref_type( &test->initz_type ) ) {
      if ( expr.var ) {
         if ( ! expr.var->hidden ) {
            s_diag( semantic, DIAG_POS_ERR, &value->expr->pos,
               "non-private initializer (references only work with private "
               "map variables)" );
            s_bail( semantic );
         }
         expr.var->addr_taken = true;
      }
      if ( expr.structure_member ) {
         expr.structure_member->addr_taken = true;
      }
   }
   return true;
}

static void initz_mismatch( struct semantic* semantic, struct initz_test* test,
   struct type_info* initz_type, struct type_info* type, struct pos* pos ) {
   s_type_mismatch( semantic, "initializer", initz_type, test->dim ?
      "element" : ( test->member ? "struct-member" : "variable" ), type, pos );
}

static bool test_string_initz( struct semantic* semantic, struct dim* dim,
   struct value* value ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, value->expr );
   if ( expr.undef_erred ) {
      return false;
   }
   if ( ! ( value->expr->root->type == NODE_INDEXED_STRING_USAGE ||
      s_is_str_value_type( &expr.type ) ) ) {
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
   value->type = VALUE_STRINGINITZ;
   value->more.stringinitz.string = string;
   return true;
}

static void init_initz_pres( struct initz_pres* pres ) {
   str_init( &pres->output );
   pres->member_last = false;
}

static void present_parent( struct initz_pres* pres,
   struct initz_test* test ) {
   if ( test->parent ) {
      present_parent( pres, test->parent );
   }
   else {
      struct str name;
      str_init( &name );
      t_copy_name( test->var->name, &name );
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
      t_copy_name( test->member->name, &name );
      str_append( &pres->output, name.value );
      str_deinit( &name );
      pres->member_last = true;
   }
}

static void present_ref_element( struct initz_pres* pres, struct dim* dim,
   struct structure_member* member ) {
   if ( dim ) {
      while ( dim ) {
         str_append( &pres->output, "[0]" );
         dim = dim->next;
      }
      pres->member_last = false;
   }
   while ( member && ! ( member->ref && ! member->ref->nullable ) && ! (
      member->structure && member->structure->has_mandatory_ref_member ) ) {
      member = member->next;
   }
   if ( member ) {
      str_append( &pres->output, "." );
      struct str name;
      str_init( &name );
      t_copy_name( member->name, &name );
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

static void refnotinit( struct semantic* semantic, struct initz_pres* pres,
   struct pos* pos ) {
   s_diag( semantic, DIAG_POS_ERR, pos,
      "`%s` not initialized", pres->output.value );
   s_diag( semantic, DIAG_POS, pos,
      "a reference-type %s must be initialized with a valid reference value",
      pres->member_last ? "member" : "element" );
   str_deinit( &pres->output );
}

// Enumeration variables must contain valid enumerators. The engine implicitly
// initializes non-initialized variables with a 0. Non-zero initializers
// consume more space in the object file. For efficiency purposes, leverage the
// memset() performed by the engine by requiring the enumeration to have an
// enumerator with value 0.
static void confirm_enum_default_initz( struct semantic* semantic,
   struct enumeration* enumeration, struct structure_member* member,
   int spec, struct pos* pos ) {
   enumeration = find_nondefaultinitz_enum( enumeration, member );
   if ( enumeration ) {
      // TODO: Show which structure member or array element is affected. Show
      // the full member path like is done for references.
      s_diag( semantic, DIAG_POS_ERR, pos,
         "enum variable implicitly initialized, but the enum lacks "
         "a default initializer (an enumerator with value %s)",
         get_enum_default_initz_text( enumeration ) );
      s_bail( semantic );
   }
}

static struct enumeration* find_nondefaultinitz_enum(
   struct enumeration* enumeration, struct structure_member* member ) {
   if ( enumeration ) {
      if ( ! enumeration->default_initz ) {
         return enumeration;
      }
      return NULL;
   }
   else if ( member ) {
      while ( member ) {
         if ( member->spec == SPEC_ENUM ) {
            if ( ! member->enumeration->default_initz ) {
               return member->enumeration;
            }
         }
         else if ( member->spec == SPEC_STRUCT && ! member->ref ) {
            enumeration = find_nondefaultinitz_enum( NULL,
               member->structure->member );
            if ( enumeration ) {
               return enumeration;
            }
         }
         member = member->next;
      }
      return NULL;
   }
   else {
      return NULL;
   }
}

static const char* get_enum_default_initz_text(
   struct enumeration* enumeration ) {
   STATIC_ASSERT( SPEC_TOTAL == 11 );
   switch ( enumeration->base_type ) {
   case SPEC_FIXED: return "0.0";
   case SPEC_BOOL: return "`false`";
   case SPEC_STR: return "\"\"";
   default:
      break;
   }
   return "0";
}

static void confirm_dim_length( struct semantic* semantic, struct var* var ) {
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

static bool test_var_finish( struct semantic* semantic, struct var* var ) {
   describe_var( var );
   if ( semantic->func_test != semantic->topfunc_test &&
      ( var->desc == DESC_ARRAY || var->desc == DESC_STRUCTVAR ) &&
      var->storage == STORAGE_LOCAL ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "local %s declared inside nested function", var->desc == DESC_ARRAY ?
            "array" : "struct variable" );
      s_bail( semantic );
   }
   switch ( semantic->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      if ( semantic->in_localscope && (
         var->storage == STORAGE_WORLD ||
         var->storage == STORAGE_GLOBAL ) ) {
         s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
            "%s variable declared inside script",
            t_get_storage_name( var->storage ) );
         s_bail( semantic );
      }
      if ( var->dim && var->dim->next && ( var->storage == STORAGE_WORLD ||
         var->storage == STORAGE_GLOBAL ) ) {
         s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
            "multidimensional %s array",
            t_get_storage_name( var->storage ) );
         s_bail( semantic );
      }
      break;
   default:
      break;
   }
   if ( var->external ) {
      if ( ! test_external_var( semantic, var ) ) {
         return false;
      }
   }
   // Only map variables can be private.
   if ( var->hidden && var->storage != STORAGE_MAP ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "%s variable declared private", t_get_storage_name( var->storage ) );
      s_bail( semantic );
   }
   if ( var->storage == STORAGE_WORLD || var->storage == STORAGE_GLOBAL ) {
      test_worldglobal_var( semantic, var );
   }
   s_calc_var_size( var );
   if ( var->initial ) {
      s_calc_var_value_index( var );
   }
   return true;
}

static bool test_external_var( struct semantic* semantic, struct var* var ) {
   if ( semantic->in_localscope ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "external variable declared in local scope" );
      s_bail( semantic );
   }
   if ( var->storage != STORAGE_MAP ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "non-map variable declared external" );
      s_bail( semantic );
   }
   if ( var->initial ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "external variable declaration has an initializer" );
      s_bail( semantic );
   }
   struct var* other_var = ( struct var* ) var->name->object;
   if ( ! other_var->external && ! other_var->object.resolved ) {
      return false;
   }
   var->imported = ( other_var->external == true );
   struct var* var_def = ( struct var* ) var->name->object;
   struct type_info type;
   struct type_info other_type;
   s_init_type_info( &type, var->ref, var->structure, var->enumeration,
      var->dim, var->spec, var->storage );
   s_init_type_info( &other_type, var_def->ref,
      var_def->structure,
      var_def->enumeration,
      var_def->dim,
      var_def->spec, var_def->storage );
   if ( ! s_same_type( &type, &other_type ) ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "external variable declaration different from %s",
         var_def->external ? "previous declaration" : "actual variable" );
      s_diag( semantic, DIAG_POS, &var_def->object.pos,
         "%s found here", var_def->external ? "previous declaration" :
         "actual variable" );
      s_bail( semantic );
   }
   if ( ! other_var->external && other_var->hidden ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "external variable declaration for a private variable" );
      s_diag( semantic, DIAG_POS, &other_var->object.pos,
         "private variable found here" );
      s_bail( semantic );
   }
   return true;
}

static void test_worldglobal_var( struct semantic* semantic,
   struct var* var ) {
   // Global/world declarations must be consistent.
   struct var* prev_var = NULL;
   if ( var->storage == STORAGE_WORLD ) {
      if ( var->desc == DESC_ARRAY || var->desc == DESC_STRUCTVAR ) {
         if ( semantic->world_arrays[ var->index ] ) {
            prev_var = semantic->world_arrays[ var->index ];
         }
         else {
            semantic->world_arrays[ var->index ] = var;
         }
      }
      else {
         if ( semantic->world_vars[ var->index ] ) {
            prev_var = semantic->world_vars[ var->index ];
         }
         else {
            semantic->world_vars[ var->index ] = var;
         }
      }
   }
   else {
      if ( var->desc == DESC_ARRAY || var->desc == DESC_STRUCTVAR ) {
         if ( semantic->global_arrays[ var->index ] ) {
            prev_var = semantic->global_arrays[ var->index ];
         }
         else {
            semantic->global_arrays[ var->index ] = var;
         }
      }
      else {
         if ( semantic->global_vars[ var->index ] ) {
            prev_var = semantic->global_vars[ var->index ];
         }
         else {
            semantic->global_vars[ var->index ] = var;
         }
      }
   }
   if ( prev_var ) {
      struct type_info type;
      s_init_type_info( &type, var->ref, var->structure, var->enumeration,
         var->dim, var->spec, var->storage );
      struct type_info prev_type;
      s_init_type_info( &prev_type, prev_var->ref, prev_var->structure,
         prev_var->enumeration, prev_var->dim, prev_var->spec,
         prev_var->storage );
      if ( ! s_same_type( &type, &prev_type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
            "%s variable declaration different from previous declaration",
            t_get_storage_name( var->storage ) );
         s_diag( semantic, DIAG_POS, &prev_var->object.pos,
            "previous declaration found here" );
         s_bail( semantic );
      }
   }
}

static void describe_var( struct var* var ) {
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

static bool is_auto_var( struct var* var ) {
   return ( var->spec == SPEC_AUTO || var->spec == SPEC_AUTOENUM );
}

static void test_auto_var( struct semantic* semantic, struct var* var ) {
   // Infer type from initializer.
   if ( ! var->initial ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "auto-declaration missing initializer" );
      s_bail( semantic );
   }
   struct scalar_initz_test initz_test;
   init_scalar_initz_test_auto( &initz_test, var->is_constant_init );
   test_scalar_initz( semantic, &initz_test, ( struct value* ) var->initial );
   assign_inferred_type( semantic, var, &initz_test.initz_type );
   var->initial_has_str = initz_test.has_str;
   // For now, keep an auto-declaration in local scope.
   if ( ! semantic->in_localscope ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "auto-declaration in non-local scope" );
      s_bail( semantic );
   }
   s_bind_local_name( semantic, var->name, &var->object,
      var->force_local_scope );
   var->object.resolved = true;
   describe_var( var );
   s_calc_var_size( var );
}

static void assign_inferred_type( struct semantic* semantic, struct var* var,
   struct type_info* type ) {
   if ( var->spec == SPEC_AUTOENUM ) {
      if ( ! s_is_enumerator( type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
            "auto-enum variable initialized with a non-enumerator" );
         s_bail( semantic );
      }
      var->enumeration = type->enumeration;
      var->spec = SPEC_ENUM;
   }
   else {
      if ( s_is_null( type ) ) {
         s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
            "cannot deduce variable type from `null`" );
         s_bail( semantic );
      }
      if ( s_describe_type( type ) == TYPEDESC_PRIMITIVE ) {
         var->spec = type->spec;
      }
      else {
         struct type_snapshot snapshot;
         s_take_type_snapshot( type, &snapshot );
         var->ref = snapshot.ref;
         var->structure = snapshot.structure;
         var->enumeration = snapshot.enumeration;
         var->spec = snapshot.spec;
      }
   }
}

void s_test_local_var( struct semantic* semantic, struct var* var ) {
   s_test_var( semantic, var );
   if ( var->initial ) {
      s_calc_var_value_index( var );
   }
   if ( ! var->force_local_scope ) {
      list_append( semantic->func_test->funcscope_vars, var );
   }
}

void s_test_foreach_var( struct semantic* semantic,
   struct type_info* collection_type, struct var* var ) {
   bool resolved = false;
   if ( is_auto_var( var ) ) {
      assign_inferred_type( semantic, var, collection_type );
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
   if ( ! var->force_local_scope ) {
      s_diag( semantic, DIAG_POS_ERR, &var->object.pos,
         "non-local foreach variable (to make the variable local, add the "
         "`let` keyword before the variable type)" );
      s_bail( semantic );
   }
   s_bind_local_name( semantic, var->name, &var->object, true );
   var->object.resolved = resolved;
   s_calc_var_size( var );
}

void s_test_func( struct semantic* semantic, struct func* func ) {
   switch ( func->type ) {
   // Other builtin functions (dedicated functions, format functions, and
   // internal functions) are part of the compiler and assumed to be resolved,
   // so they should not appear here.
   case FUNC_ASPEC:
   case FUNC_EXT:
      test_special_func( semantic, func );
      break;
   case FUNC_USER:
   case FUNC_ALIAS:
      test_func( semantic, func );
      break;
   default:
      S_UNREACHABLE( semantic );
   }
}

// Tests the following function types:
//  - FUNC_ASPEC (action-specials)
//  - FUNC_EXT (extension functions)
// Most of the testing done by this function are just sanity checks.
static void test_special_func( struct semantic* semantic, struct func* func ) {
   // Special functions should not appear inside scripts or functions. Special
   // functions should not be hidden, although that could be added later.
   S_ASSERT( semantic, semantic->in_localscope == false );
   S_ASSERT( semantic, func->hidden == false );
   func->object.resolved =
      test_special_func_param_list( semantic, func ) &&
      test_special_func_return_type( semantic, func );
   // Special functions should be resolved in one go.
   S_ASSERT( semantic, func->object.resolved == true );
}

static bool test_special_func_param_list( struct semantic* semantic,
   struct func* func ) {
   struct param* param = func->params;
   while ( param ) {
      test_special_func_param( semantic, func, param );
      if ( ! param->object.resolved ) {
         return false;
      }
      param = param->next;
   }
   // Action-specials can take up to 5 arguments.
   // TODO: See if we need to do the same for extension functions.
   enum { MAX_ASPEC_PARAMS = 5 };
   if ( func->type == FUNC_ASPEC && func->max_param > MAX_ASPEC_PARAMS ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "too many parameters for action-special (maximum is %d)",
         MAX_ASPEC_PARAMS );
      s_bail( semantic );
   }
   return true;
}

static void test_special_func_param( struct semantic* semantic,
   struct func* func, struct param* param ) {
   S_ASSERT( semantic, param->name == NULL );
   S_ASSERT( semantic, param->default_value == NULL );
   param->object.resolved =
      test_special_func_param_type( semantic, func, param );
}

static bool test_special_func_param_type( struct semantic* semantic,
   struct func* func, struct param* param ) {
   S_ASSERT( semantic, param->ref == NULL );
   S_ASSERT( semantic, param->structure == NULL );
   S_ASSERT( semantic, param->enumeration == NULL );
   S_ASSERT( semantic, param->path == NULL );
   switch ( param->spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
   case SPEC_BOOL:
   case SPEC_STR:
      return true;
   default:
      S_UNREACHABLE( semantic );
   }
   return false;
}

static bool test_special_func_return_type( struct semantic* semantic,
   struct func* func ) {
   S_ASSERT( semantic, func->ref == NULL );
   S_ASSERT( semantic, func->structure == NULL );
   S_ASSERT( semantic, func->enumeration == NULL );
   S_ASSERT( semantic, func->path == NULL );
   switch ( func->return_spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
   case SPEC_BOOL:
   case SPEC_STR:
   case SPEC_VOID:
      return true;
   default:
      S_UNREACHABLE( semantic );
   }
   return false;
}

// Tests the following function types:
//  - FUNC_USER (user-defined functions)
//  - FUNC_ALIAS (function aliases created through typedef)
static void test_func( struct semantic* semantic, struct func* func ) {
   func->object.resolved =
      test_func_return_spec( semantic, func ) &&
      test_func_ref( semantic, func ) &&
      test_func_name( semantic, func ) &&
      test_func_after_name( semantic, func );
}

static bool test_func_return_spec( struct semantic* semantic,
   struct func* func ) {
   // Keep the interface of a function explicit. Nested functions are an
   // implementation detail of a function, so it doesn't matter there.
   if (
      func->return_spec == SPEC_AUTO ||
      func->return_spec == SPEC_AUTOENUM ) {
      struct func_user* impl = func->impl;
      if ( func->type == FUNC_USER && ! impl->nested ) {
         s_diag( semantic, DIAG_POS | DIAG_ERR, &func->object.pos,
            "return-type deduction only available for nested functions" );
         s_bail( semantic );
      }
   }
   struct spec_test test;
   init_spec_test( &test, func->path, &func->object, func->ref,
      func->structure, func->enumeration, NULL, func->return_spec );
   test_spec( semantic, &test );
   func->ref = test.ref;
   func->structure = test.structure;
   func->enumeration = test.enumeration;
   func->return_spec = test.spec;
   // Functions cannot return aggregate types. Aggregate types must be returned
   // by reference.
   if ( func->return_spec == SPEC_STRUCT && ! func->ref ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "function returning a struct by value" );
      s_diag( semantic, DIAG_POS, &func->object.pos,
         "a struct can only be returned by reference" );
      s_bail( semantic );
   }
   // Public function must have a public return type.
   if ( ! semantic->in_localscope && ! func->hidden && ! test.public_spec ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "non-private function has a private return type" );
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

static bool test_func_ref( struct semantic* semantic, struct func* func ) {
   struct ref_test test;
   init_ref_test( &test, func->ref, func->return_spec, ( ! func->hidden ) );
   return test_ref( semantic, &test );
}

static bool test_func_name( struct semantic* semantic, struct func* func ) {
   if ( semantic->in_localscope ) {
      if ( func->literal ) {
         func->object.depth = semantic->depth;
      }
      else {
         s_bind_local_name( semantic, func->name, &func->object,
            func->force_local_scope );
      }
   }
   return true;
}

static bool test_func_after_name( struct semantic* semantic,
   struct func* func ) {
   struct param_list_test param_test;
   init_param_list_test( &param_test, func, NULL,
      ( semantic->depth == 1 && ! func->hidden ) );
   bool resolved = test_param_list( semantic, &param_test );
   if ( resolved && func->external ) {
      resolved = test_external_func( semantic, func );
   }
   return resolved;
}

static void init_param_list_test( struct param_list_test* test,
   struct func* func, struct ref_func* func_ref, bool need_public_spec ) {
   test->func = func;
   test->func_ref = func_ref;
   if ( func_ref ) {
      test->params = func_ref->params;
   }
   else {
      test->params = func->params;
   }
   test->prev_param = NULL;
   test->need_public_spec = need_public_spec;
}

static bool test_param_list( struct semantic* semantic,
   struct param_list_test* test ) {
   // Parameter names must be unique.
   s_add_scope( semantic, true );
   struct param* param = test->params;
   while ( param ) {
      if ( param->name ) {
         s_bind_local_name( semantic, param->name, &param->object, true );
      }
      param = param->next;
   }
   s_pop_scope( semantic );
   // Test parameters.
   param = test->params;
   while ( param ) {
      // Only test a parameter if it has not already been tested. Note that
      // previous parameters are not visible to following parameters, so we do
      // not bind the names of previous parameters. 
      if ( ! param->object.resolved ) {
         test_param( semantic, test, param );
         if ( ! param->object.resolved ) {
            return false;
         }
      }
      test->prev_param = param;
      param = param->next;
   }
   return true;
}

static void test_param( struct semantic* semantic,
   struct param_list_test* test, struct param* param ) {
   param->object.resolved =
      test_param_spec( semantic, test, param ) &&
      test_param_ref( semantic, test, param ) &&
      test_param_after_name( semantic, test, param );
   if ( param->object.resolved ) {
      calc_param_size( param );
   }
}

static bool test_param_spec( struct semantic* semantic,
   struct param_list_test* test, struct param* param ) {
   struct spec_test spec_test;
   init_spec_test( &spec_test, param->path, &param->object, param->ref,
      param->structure, param->enumeration, NULL, param->spec );
   test_spec( semantic, &spec_test );
   param->ref = spec_test.ref;
   param->structure = spec_test.structure;
   param->enumeration = spec_test.enumeration;
   param->spec = spec_test.spec;
   // Parameter must have a valid type.
   if ( param->spec == SPEC_VOID && ! param->ref ) {
      s_diag( semantic, DIAG_POS_ERR, &param->object.pos,
         "void parameter" );
      s_bail( semantic );
   }
   else if ( param->spec == SPEC_STRUCT && ! param->ref ) {
      s_diag( semantic, DIAG_POS_ERR, &param->object.pos,
         "struct parameter (structs can only be passed by reference)" );
      s_bail( semantic );
   }
   // For a public function, every parameter type must be public.
   if ( test->need_public_spec && ! spec_test.public_spec ) {
      s_diag( semantic, DIAG_POS_ERR, &param->object.pos,
         "parameter of non-private function has a private type" );
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

static bool test_param_ref( struct semantic* semantic,
   struct param_list_test* test, struct param* param ) {
   struct ref_test ref_test;
   init_ref_test( &ref_test, param->ref, param->spec, test->need_public_spec );
   return test_ref( semantic, &ref_test );
}

static bool test_param_after_name( struct semantic* semantic,
   struct param_list_test* test, struct param* param ) {
   if ( param->default_value ) {
      if ( ! param->default_value_tested &&
         ! test_param_default_value( semantic, test, param ) ) {
         return false;
      }
   }
   else {
      if ( test->prev_param && test->prev_param->default_value ) {
         s_diag( semantic, DIAG_POS_ERR, &param->object.pos,
            "parameter missing default value" );
         s_bail( semantic );
      }
   }
   return true;
}

static bool test_param_default_value( struct semantic* semantic,
   struct param_list_test* test, struct param* param ) {
   if ( test->func && test->func->type == FUNC_ALIAS ) {
      s_diag( semantic, DIAG_POS_ERR, &param->default_value->pos,
         "default value specified for parameter of function alias" );
      s_bail( semantic );
   }
   if ( test->func_ref ) {
      s_diag( semantic, DIAG_POS_ERR, &param->default_value->pos,
         "default argument specified in a function reference" );
      s_bail( semantic );
   }
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, param->default_value );
   if ( expr.undef_erred ) {
      return false;
   }
   if ( ! param->default_value->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &param->default_value->pos,
         "non-constant default value" );
      s_bail( semantic );
   }
   struct type_info param_type;
   s_init_type_info( &param_type, param->ref, param->structure,
      param->enumeration, NULL, param->spec, STORAGE_LOCAL );
   if ( ! s_instance_of( &param_type, &expr.type ) ) {
      default_value_mismatch( semantic, test->func, param, &param_type,
         &expr.type, &param->default_value->pos );
      s_bail( semantic );
   }
   // Note which variable is passed by reference.
   if ( s_is_ref_type( &expr.type ) ) {
      if ( expr.var ) {
         if ( ! expr.var->hidden ) {
            s_diag( semantic, DIAG_POS_ERR, &param->default_value->pos,
               "non-private default argument (references only work with "
               "private map variables)" );
            s_bail( semantic );
         }
         expr.var->addr_taken = true;
      }
      if ( expr.structure_member ) {
         expr.structure_member->addr_taken = true;
      }
   }
   param->default_value_tested = true;
   return true;
}

static void default_value_mismatch( struct semantic* semantic,
   struct func* func, struct param* param, struct type_info* param_type,
   struct type_info* type, struct pos* pos ) {
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

static int get_param_number( struct func* func, struct param* target ) {
   // Start at the second parameter when in a format-function.
   int number = ( func->type == FUNC_FORMAT ) ? 2 : 1;
   struct param* param = func->params;
   while ( param != target ) {
      param = param->next;
      ++number;
   }
   return number;
}

static bool test_external_func( struct semantic* semantic,
   struct func* func ) {
   if ( semantic->in_localscope ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "external function declaration in local scope" );
      s_bail( semantic );
   }
   bool resolved = false;
   struct func* other_func = ( struct func* ) func->name->object;
   // External function declarations only apply to user-defined functions.
   if ( other_func->type != FUNC_USER ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "external function declaration refers to a function that is not "
         "user-defined" );
      s_diag( semantic, DIAG_POS | DIAG_NOTE, &other_func->object.pos,
         "the target function (the function the external function declaration "
         "is referring to) is found here" );
      s_bail( semantic );
   }
   if ( func == other_func ) {
      resolved = true;
   }
   else {
      if ( other_func->object.resolved ) {
         struct type_info type;
         s_init_type_info_func( &type, func->ref, func->structure,
            func->enumeration, func->params, func->return_spec,
            func->min_param, func->max_param, false );
         struct type_info other_type;
         s_init_type_info_func( &other_type, other_func->ref,
            other_func->structure, other_func->enumeration, other_func->params,
            other_func->return_spec, other_func->min_param,
            other_func->max_param, false );
         if ( ! s_same_type( &type, &other_type ) ) {
            s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
               "external function declaration different from %s",
               other_func->external ? "previous declaration" :
               "actual function" );
            s_diag( semantic, DIAG_POS, &other_func->object.pos,
               "%s found here", other_func->external ?
               "previous declaration" : "actual function" );
            s_bail( semantic );
         }
         resolved = true;
      }
   }
   struct func_user* impl = func->impl;
   if ( impl->body ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "external function declaration has a body" );
      s_bail( semantic );
   }
   if ( ! other_func->external && other_func->hidden ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "external function declaration for a private function" );
      s_diag( semantic, DIAG_POS, &other_func->object.pos,
         "private function found here" );
      s_bail( semantic );
   }
   func->imported = other_func->external;
   return resolved;
}

void s_test_func_body( struct semantic* semantic, struct func* func ) {
   struct func_user* impl = func->impl;
   if ( ! impl->body ) {
      s_diag( semantic, DIAG_POS_ERR, &func->object.pos,
         "function missing body" );
      s_bail( semantic );
   }
   s_add_scope( semantic, true );
   struct builtin_aliases aliases;
   init_builtin_aliases( semantic, &aliases, func );
   if ( semantic->lib->lang == LANG_BCS ) {
      bind_builtin_aliases( semantic, &aliases );
   }
   struct param* param = func->params;
   while ( param ) {
      if ( param->name ) {
         s_bind_local_name( semantic, param->name, &param->object, true );
      }
      param = param->next;
   }
   struct func_test test;
   init_func_test( &test, semantic->func_test, func, &impl->labels,
      &impl->funcscope_vars, NULL );
   if ( ! impl->nested ) {
      semantic->topfunc_test = &test;
   }
   semantic->func_test = &test;
   s_test_func_block( semantic, func, impl->body );
   semantic->func_test = test.parent;
   impl->returns = test.returns;
   if ( ! impl->nested ) {
      impl->nested_funcs = test.nested_funcs;
      semantic->topfunc_test = NULL;
   }
   s_pop_scope( semantic );
   // If `auto` is still the specifier, it means no return statements were
   // encountered. The return type is then `void`.
   if ( func->return_spec == SPEC_AUTO ) {
      func->return_spec = SPEC_VOID;
   }
}

static void init_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases, struct func* func ) {
   s_init_magic_id( &aliases->name.magic_id, MAGICID_FUNCTION );
   struct alias* alias = &aliases->name.alias;
   s_init_alias( alias );
   alias->object.resolved = true;
   alias->target = &aliases->name.magic_id.object;
}

static void bind_builtin_aliases( struct semantic* semantic,
   struct builtin_aliases* aliases ) {
   struct name* name = t_extend_name( semantic->ns->body, "__function__" );
   s_bind_local_name( semantic, name, &aliases->name.alias.object, true );
}

void s_init_magic_id( struct magic_id* magic_id, int name ) {
   t_init_object( &magic_id->object, NODE_MAGICID );
   magic_id->object.resolved = true;
   magic_id->string = NULL;
   magic_id->name = name;
}

static void init_func_test( struct func_test* test, struct func_test* parent,
   struct func* func, struct list* labels, struct list* funcscope_vars,
   struct script* script ) {
   test->func = func;
   test->labels = labels;
   test->funcscope_vars = funcscope_vars;
   test->nested_funcs = NULL;
   test->returns = NULL;
   test->parent = parent;
   test->script = script;
   test->enclosing_buildmsg = NULL;
}

void s_test_nested_func( struct semantic* semantic, struct func* func ) {
   s_test_func( semantic, func );
   if ( func->type == FUNC_USER ) {
      s_test_func_body( semantic, func );
      struct func_user* impl = func->impl;
      if ( impl->local ) {
         impl->next_nested = semantic->topfunc_test->nested_funcs;
         semantic->topfunc_test->nested_funcs = func;
      }
   }
}

void s_test_script( struct semantic* semantic, struct script* script ) {
   if ( script->number ) {
      test_script_number( semantic, script );
   }
   test_script_param_list( semantic, script );
   if ( script->body ) {
      test_script_body( semantic, script );
   }
}

static void test_script_number( struct semantic* semantic,
   struct script* script ) {
   if ( script->number != semantic->task->raw0_expr ) {
      test_nonzero_script_number( semantic, script );
   }
}

static void test_nonzero_script_number( struct semantic* semantic,
   struct script* script ) {
   struct expr_test expr;
   s_init_expr_test( &expr, true, false );
   s_test_expr( semantic, &expr, script->number );
   if ( ! script->number->folded ) {
      s_diag( semantic, DIAG_POS_ERR, &script->number->pos,
         "script number not constant" );
      s_bail( semantic );
   }
   switch ( semantic->lang ) {
   case LANG_ACS:
   case LANG_BCS:
      script->named_script = ( semantic->strong_type ?
         script->number->spec == SPEC_STR :
         script->number->root->type == NODE_INDEXED_STRING_USAGE );
      break;
   default:
      break;
   }
   if ( script->named_script ) {
      struct indexed_string* string = t_lookup_string( semantic->task,
         script->number->value );
      if ( ! string ) {
         s_diag( semantic, DIAG_POS_ERR, &script->number->pos,
            "script name not a valid string" );
         s_bail( semantic );
      }
      if ( strcasecmp( string->value, "none" ) == 0 ) {
         s_diag( semantic, DIAG_POS_ERR, &script->number->pos,
            "\"%s\" is a reserved script name", string->value );
         s_bail( semantic );
      }
   }
   else {
      if ( script->number->value < SCRIPT_MIN_NUM ||
         script->number->value > SCRIPT_MAX_NUM ) {
         s_diag( semantic, DIAG_POS_ERR, &script->number->pos,
            "script number not between %d and %d", SCRIPT_MIN_NUM,
            SCRIPT_MAX_NUM );
         s_bail( semantic );
      }
      switch ( semantic->lang ) {
      case LANG_ACS:
      case LANG_BCS:
         if ( script->number->value == 0 ) {
            s_diag( semantic, DIAG_POS_ERR, &script->number->pos,
               "script number 0 not between `<<` and `>>`" );
            s_bail( semantic );
         }
         break;
      default:
         break;
      }
   }
}

static void test_script_param_list( struct semantic* semantic,
   struct script* script ) {
   struct param* param = script->params;
   while ( param ) {
      calc_param_size( param );
      param->object.resolved = true;
      param = param->next;
   }
}

static void test_script_body( struct semantic* semantic,
   struct script* script ) {
   s_add_scope( semantic, true );
   struct builtin_script_aliases aliases;
   init_builtin_script_aliases( semantic, &aliases, script );
   if ( semantic->lib->lang == LANG_BCS ) {
      bind_builtin_script_aliases( semantic, &aliases );
   }
   struct param* param = script->params;
   while ( param ) {
      if ( param->name ) {
         s_bind_local_name( semantic, param->name, &param->object, true );
      }
      param = param->next;
   }
   struct func_test test;
   init_func_test( &test, NULL, NULL, &script->labels,
      &script->funcscope_vars, script );
   semantic->topfunc_test = &test;
   semantic->func_test = semantic->topfunc_test;
   s_test_top_block( semantic, script->body );
   script->nested_funcs = test.nested_funcs;
   semantic->topfunc_test = NULL;
   semantic->func_test = NULL;
   s_pop_scope( semantic );
}

static void init_builtin_script_aliases( struct semantic* semantic,
   struct builtin_script_aliases* aliases, struct script* script ) {
   s_init_magic_id( &aliases->name.magic_id, MAGICID_SCRIPT );
   struct alias* alias = &aliases->name.alias;
   s_init_alias( alias );
   alias->object.resolved = true;
   alias->target = &aliases->name.magic_id.object;
}

static void bind_builtin_script_aliases( struct semantic* semantic,
   struct builtin_script_aliases* aliases ) {
   struct name* name = t_extend_name( semantic->ns->body, "__script__" );
   s_bind_local_name( semantic, name, &aliases->name.alias.object, true );
}

void s_calc_var_size( struct var* var ) {
   var->size = calc_size( var->dim, var->structure, var->ref );
}

static void calc_param_size( struct param* param ) {
   param->size = calc_size( NULL, NULL, param->ref );
}

static int calc_size( struct dim* dim, struct structure* structure,
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

static void calc_struct_size( struct structure* structure ) {
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

static void make_value_list( struct value_list* list,
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

static void alloc_value_index( struct value_index_alloc* alloc,
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
         if ( alloc->value->type == VALUE_STRINGINITZ ) {
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

static void alloc_value_index_struct( struct value_index_alloc* alloc,
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
         if ( alloc->value->type == VALUE_STRINGINITZ ) {
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
