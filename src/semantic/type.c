#include <string.h>

#include "phase.h"

static void create_implicit_ref( struct type_info* type );
static bool same_ref( struct ref* a, struct ref* b );
static bool same_ref_struct( struct ref_struct* a, struct ref_struct* b );
static bool same_ref_array( struct ref_array* a, struct ref_array* b );
static bool same_ref_func( struct ref_func* a, struct ref_func* b );
static bool same_spec_primitive( int a, int b );
static bool compatible_raw_spec( int spec );
static bool same_dim( struct dim* a, struct dim* b );
static void present_extended_spec( struct structure* structure,
   struct enumeration* enumeration, int spec, struct str* string );
static void present_spec( int spec, struct str* string );
static void present_ref( struct ref* ref, struct str* string );
static void present_dim( struct type_info* type, struct str* string );
static void present_param_list( struct param* param, struct str* string );
static struct ref* dup_ref( struct ref* ref );
static void set_storage( struct type_info* type, int storage );

void s_init_type_info( struct type_info* type, struct ref* ref,
   struct structure* structure, struct enumeration* enumeration,
   struct dim* dim, int spec, int storage ) {
   type->ref = ref;
   type->structure = structure;
   type->enumeration = enumeration;
   type->dim = dim;
   type->spec = spec;
   type->storage = storage;
   type->builtin_func = false;
   if ( type->ref && type->ref->implicit ) {
      create_implicit_ref( type );
   }
}

static void create_implicit_ref( struct type_info* type ) {
   switch ( type->ref->type ) {
   case REF_ARRAY: {
         struct ref_array* array = ( struct ref_array* ) type->ref;
         type->implicit_ref.array = *array;
         type->ref = &type->implicit_ref.array.ref;
         type->ref->implicit = true;
      }
      break;
   case REF_STRUCTURE: {
         struct ref_struct* structure = ( struct ref_struct* ) type->ref;
         type->implicit_ref.structure = *structure;
         type->ref = &type->implicit_ref.structure.ref;
         type->ref->implicit = true;
      }
      break;
   case REF_FUNCTION: {
         struct ref_func* func = ( struct ref_func* ) type->ref;
         type->implicit_ref.func = *func;
         type->ref = &type->implicit_ref.func.ref;
         type->ref->implicit = true;
      }
      break;
   case REF_NULL: {
         type->implicit_ref.ref = *type->ref;
         type->ref = &type->implicit_ref.ref;
         type->ref->implicit = true;
      }
      break;
   default:
      UNREACHABLE();
   }
}

void s_init_type_info_copy( struct type_info* type,
   struct type_info* other_type ) {
   s_init_type_info( type, other_type->ref, other_type->structure,
      other_type->enumeration, other_type->dim, other_type->spec,
      other_type->storage );
}

void s_init_type_info_array_ref( struct type_info* type, struct ref* ref,
   struct structure* structure, struct enumeration* enumeration,
   int dim_count, int spec ) {
   s_init_type_info( type, ref, structure, enumeration, NULL, spec,
      STORAGE_LOCAL );
   struct ref_array* array = &type->implicit_ref.array;
   array->ref.next = type->ref;
   array->ref.type = REF_ARRAY;
   array->ref.nullable = false;
   array->ref.implicit = true;
   array->dim_count = dim_count;
   array->storage = STORAGE_MAP;
   type->ref = &array->ref;
}

void s_init_type_info_func( struct type_info* type, struct ref* ref,
   struct structure* structure, struct enumeration* enumeration,
   struct param* params, int return_spec, int min_param, int max_param,
   bool local ) {
   s_init_type_info( type, ref, structure, enumeration, NULL, return_spec,
      STORAGE_LOCAL );
   // NOTE: At this time, I don't see where in the compiler a distinction needs
   // to be made between a function and a reference-to-function. So decay a
   // function into reference-to-function at all times.
   struct ref_func* func = &type->implicit_ref.func;
   func->ref.next = type->ref;
   func->ref.type = REF_FUNCTION;
   func->ref.nullable = false;
   func->ref.implicit = true;
   func->params = params;
   func->min_param = min_param;
   func->max_param = max_param;
   func->local = local;
   type->ref = &func->ref;
}

void s_init_type_info_builtin_func( struct type_info* type ) {
   s_init_type_info( type, NULL, NULL, NULL, NULL, SPEC_NONE, STORAGE_LOCAL );
   type->builtin_func = true;
}

void s_init_type_info_scalar( struct type_info* type, int spec ) {
   s_init_type_info( type, NULL, NULL, NULL, NULL, spec, STORAGE_LOCAL );
}

void s_init_type_info_null( struct type_info* type ) {
   s_init_type_info( type, NULL, NULL, NULL, NULL, SPEC_NONE, STORAGE_LOCAL );
   struct ref* ref = &type->implicit_ref.ref;
   ref->next = NULL;
   ref->type = REF_NULL;
   ref->nullable = true;
   ref->implicit = true;
   type->ref = ref;
}

void s_decay( struct semantic* semantic, struct type_info* type ) {
   // Array type.
   if ( type->dim ) {
      struct ref_array* array = &type->implicit_ref.array;
      array->ref.next = type->ref;
      array->ref.type = REF_ARRAY;
      array->ref.nullable = false;
      array->ref.implicit = true;
      array->dim_count = 0;
      array->storage = type->storage;
      struct dim* count_dim = type->dim;
      while ( count_dim ) {
         ++array->dim_count;
         count_dim = count_dim->next;
      }
      type->ref = &array->ref;
      type->dim = NULL;
   }
   // Structure type.
   else if ( ! type->ref && type->structure ) {
      struct ref_struct* implicit_ref = &type->implicit_ref.structure;
      implicit_ref->ref.next = NULL;
      implicit_ref->ref.type = REF_STRUCTURE;
      implicit_ref->ref.nullable = false;
      implicit_ref->ref.implicit = true;
      implicit_ref->storage = type->storage;
      type->ref = &implicit_ref->ref;
   }
   // Enumeration type.
   else if ( ! type->ref && type->enumeration ) {
      type->spec = s_spec( semantic, type->enumeration->base_type );
   }
}

bool s_same_type( struct type_info* a, struct type_info* b ) {
   if ( s_is_null( a ) ) {
      return s_is_ref( b );
   }
   else if ( s_is_null( b ) ) {
      return s_is_ref( a );
   }
   else {
      switch ( s_describe_type( a ) ) {
      case TYPEDESC_PRIMITIVE:
         return ( s_describe_type( b ) == TYPEDESC_PRIMITIVE &&
            same_spec_primitive( a->spec, b->spec ) );
      default:
         return ( same_ref( a->ref, b->ref ) &&
            a->structure == b->structure &&
            a->enumeration == b->enumeration &&
            a->spec == b->spec &&
            same_dim( a->dim, b->dim ) );
      }
   }
}

static bool same_ref( struct ref* a, struct ref* b ) {
   while ( a && b ) {
      if ( ! ( a->type == b->type ) ) {
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
      case REF_NULL:
         same = same_ref_struct(
            ( struct ref_struct* ) a,
            ( struct ref_struct* ) b );
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

static bool same_ref_struct( struct ref_struct* a, struct ref_struct* b ) {
   return ( a->storage == b->storage );
}

static bool same_ref_array( struct ref_array* a, struct ref_array* b ) {
   return ( a->dim_count == b->dim_count && a->storage == b->storage );
}

static bool same_ref_func( struct ref_func* a, struct ref_func* b ) {
   struct param* param_a = a->params;
   struct param* param_b = b->params;
   while ( param_a && param_b &&
      param_a->spec == param_b->spec &&
      same_ref( param_a->ref, param_b->ref ) ) {
      param_a = param_a->next;
      param_b = param_b->next;
   }
   return (
      param_a == NULL &&
      param_b == NULL &&
      a->local == b->local );
}

static bool same_spec_primitive( int a, int b ) {
   if ( a == SPEC_RAW ) {
      return compatible_raw_spec( b );
   }
   else if ( b == SPEC_RAW ) {
      return compatible_raw_spec( a );
   }
   else {
      return ( a == b );
   }
}

static bool compatible_raw_spec( int spec ) {
   switch ( spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
   case SPEC_BOOL:
   case SPEC_STR:
      return true;
   default:
      return false;
   }
}

static bool same_dim( struct dim* a, struct dim* b ) {
   while ( a && b && a->length == b->length ) {
      a = a->next;
      b = b->next;
   }
   return ( a == NULL && b == NULL );
}

bool s_common_type( struct type_info* a, struct type_info* b,
   struct type_info* result ) {
   if ( s_same_type( a, b ) ) {
      switch ( s_describe_type( a ) ) {
      case TYPEDESC_ARRAYREF:
      case TYPEDESC_STRUCTREF:
      case TYPEDESC_FUNCREF:
         s_init_type_info_copy( result, a );
         if ( b->ref->nullable && ! result->ref->nullable ) {
            create_implicit_ref( result );
            result->ref->nullable = true;
         }
         return true;
      case TYPEDESC_NULLREF:
         if ( s_is_null( b ) ) {
            s_init_type_info_null( result );
         }
         else {
            s_init_type_info_copy( result, b );
            if ( ! result->ref->nullable ) {
               create_implicit_ref( result );
               result->ref->nullable = true;
            }
         }
         return true;
      case TYPEDESC_ARRAY:
      case TYPEDESC_STRUCT:
      case TYPEDESC_ENUM:
         s_init_type_info_copy( result, a );
         return true;
      case TYPEDESC_PRIMITIVE:
         // Implicit `raw` cast: 
         // Mixing a `raw` value with a value of another type will implicitly
         // cast the other value to `raw`.
         if ( a->spec == SPEC_RAW || b->spec == SPEC_RAW ) {
            s_init_type_info_scalar( result, SPEC_RAW );
         }
         else {
            s_init_type_info_copy( result, a );
         }
         return true;
      default:
         break;
      }
   }
   return false;
}

bool s_instance_of( struct type_info* type, struct type_info* instance ) {
   bool valid = false;
   switch ( s_describe_type( type ) ) {
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
      if ( s_is_null( instance ) ) {
         valid = type->ref->nullable;
      }
      else if ( ! type->ref->nullable ) {
         valid = ( s_same_type( type, instance ) &&
            ! instance->ref->nullable );
      }
      else {
         valid = s_same_type( type, instance );
      }
      break;
   case TYPEDESC_ENUM:
      {
         struct type_info revealed_type;
         s_init_type_info_copy( &revealed_type, instance );
         s_reveal( &revealed_type );
         valid = s_same_type( type, &revealed_type );
      }
      break;
   case TYPEDESC_PRIMITIVE:
      valid = s_same_type( type, instance );
      break;
   default:
      break;
   }
   return valid;
}

void s_present_type( struct type_info* type, struct str* string ) {
   if ( type->builtin_func ) {
      str_append( string, "builtin-function" );
   }
   else {
      present_extended_spec( type->structure, type->enumeration, type->spec,
         string );
      present_ref( type->ref, string );
      present_dim( type, string );
   }
}

static void present_extended_spec( struct structure* structure,
   struct enumeration* enumeration, int spec, struct str* string ) {
   if ( spec == SPEC_ENUM ) {
      if ( enumeration->name ) {
         struct str name;
         str_init( &name );
         t_copy_name( enumeration->name, false, &name );
         str_append( string, "enum " );
         str_append( string, name.value );
         str_deinit( &name );
      }
      else {
         str_append( string, "anonymous-enum" );
      }
   }
   else if ( spec == SPEC_STRUCT ) {
      if ( structure->anon ) {
         str_append( string, "anonymous-struct" );
      }
      else {
         struct str name;
         str_init( &name );
         t_copy_name( structure->name, false, &name );
         str_append( string, "struct " );
         str_append( string, name.value );
         str_deinit( &name );
      }
   }
   else {
      present_spec( spec, string );
   }
}

static void present_spec( int spec, struct str* string ) {
   switch ( spec ) {
   case SPEC_RAW:
      str_append( string, "raw" );
      break;
   case SPEC_INT:
      str_append( string, "int" );
      break;
   case SPEC_FIXED:
      str_append( string, "fixed" );
      break;
   case SPEC_BOOL:
      str_append( string, "bool" );
      break;
   case SPEC_STR:
      str_append( string, "str" );
      break;
   case SPEC_VOID:
      str_append( string, "void" );
   default:
      break;
   }
}

static void present_ref( struct ref* ref, struct str* string ) {
   if ( ref ) {
      present_ref( ref->next, string );
      if ( ref->type == REF_ARRAY ) {
         struct ref_array* part = ( struct ref_array* ) ref;
         for ( int i = 0; i < part->dim_count; ++i ) {
            str_append( string, "[]" );
         }
         switch ( part->storage ) {
         case STORAGE_LOCAL: str_append( string, " local" ); break;
         case STORAGE_WORLD: str_append( string, " world" ); break;
         case STORAGE_GLOBAL: str_append( string, " global" ); break;
         default: break;
         }
         if ( ref->nullable ) {
            str_append( string, "?" );
         }
         else {
            str_append( string, "&" );
         }
      }
      else if ( ref->type == REF_STRUCTURE ) {
         struct ref_struct* structure = ( struct ref_struct* ) ref;
         switch ( structure->storage ) {
         case STORAGE_LOCAL: str_append( string, " local" ); break;
         case STORAGE_WORLD: str_append( string, " world" ); break;
         case STORAGE_GLOBAL: str_append( string, " global" ); break;
         default: break;
         }
         if ( ref->nullable ) {
            str_append( string, "?" );
         }
         else {
            str_append( string, "&" );
         }
      }
      else if ( ref->type == REF_FUNCTION ) {
         struct ref_func* func = ( struct ref_func* ) ref;
         str_append( string, " " );
         str_append( string, "function" );
         str_append( string, "(" );
         present_param_list( func->params, string );
         str_append( string, ")" );
         if ( func->local ) {
            str_append( string, " local" );
         }
         if ( ref->nullable ) {
            str_append( string, "?" );
         }
         else {
            str_append( string, "&" );
         }
      }
      else if ( ref->type == REF_NULL ) {
         str_append( string, "null" );
      }
      else {
         UNREACHABLE()
      }
   }
}

static void present_dim( struct type_info* type, struct str* string ) {
   struct dim* dim = type->dim;
   while ( dim ) {
      char text[ 11 + 2 ];
      snprintf( text, sizeof( text ), "[%d]", dim->length );
      str_append( string, text );
      dim = dim->next;
   }
}

static void present_param_list( struct param* param, struct str* string ) {
   while ( param ) {
      present_extended_spec( param->structure, param->enumeration, param->spec,
         string );
      present_ref( param->ref, string );
      param = param->next;
      if ( param ) {
         str_append( string, "," );
         str_append( string, " " );
      }
   }
}

bool s_is_scalar( struct type_info* type ) {
   return ( ! type->dim && ( type->ref || ! type->structure ) );
}

bool s_is_ref_type( struct type_info* type ) {
   return ( type->ref || type->dim || type->structure );
}

bool s_is_value_type( struct type_info* type ) {
   return ( ! s_is_ref_type( type ) );
}

bool s_is_primitive_type( struct type_info* type ) {
   return ( ! s_is_ref_type( type ) );
}

// Initializes @type with the type of the key used by the iterable type. Right
// now, only an integer key is possible.
void s_iterate_type( struct semantic* semantic, struct type_info* type,
   struct type_iter* iter ) {
   if ( s_is_str_value_type( type ) ) {
      s_init_type_info_scalar( &iter->key, s_spec( semantic, SPEC_INT ) );
      s_init_type_info_scalar( &iter->value, s_spec( semantic, SPEC_INT ) );
      iter->available = true;
   }
   else if ( s_is_array_ref( type ) ) {
      s_init_type_info_scalar( &iter->key, s_spec( semantic, SPEC_INT ) );
      s_subscript_array_ref( semantic, type, &iter->value );
      iter->available = true;
   }
   else {
      iter->available = false;
   }
}

bool s_is_str_value_type( struct type_info* type ) {
   return ( s_is_value_type( type ) && type->spec == SPEC_STR );
}

bool s_is_array_ref( struct type_info* type ) {
   return ( type->dim || ( type->ref && type->ref->type == REF_ARRAY ) );
}

enum subscript_result s_subscript_array_ref( struct semantic* semantic,
   struct type_info* type, struct type_info* element_type ) {
   struct ref_array* array = ( struct ref_array* ) type->ref;
   // Sub-array.
   if ( array->dim_count > 1 ) {
      s_init_type_info_array_ref( element_type, type->ref->next,
         type->structure, type->enumeration, array->dim_count - 1,
         type->spec );
      return SUBSCRIPTRESULT_SUBARRAY;
   }
   // Reference element.
   else if ( type->ref->next ) {
      s_init_type_info( element_type, type->ref->next, type->structure,
         type->enumeration, NULL, type->spec, STORAGE_LOCAL );
      return SUBSCRIPTRESULT_REF;
   }
   // Structure element.
   else if ( type->structure ) {
      s_init_type_info( element_type, NULL, type->structure, NULL, NULL,
         type->spec, array->storage );
      s_decay( semantic, element_type );
      return SUBSCRIPTRESULT_STRUCT;
   }
   // Primitive element.
   else {
      s_init_type_info( element_type, NULL, NULL, type->enumeration, NULL,
         s_spec( semantic, type->spec ), STORAGE_LOCAL );
      s_decay( semantic, element_type );
      return SUBSCRIPTRESULT_PRIMITIVE;
   }
}

// NOTE: This function only makes sure that the data referenced by @type will
// be valid throughout compilation; it does not create new instances of the
// data unless it is necessary. Be careful when editing the data returned by
// this function because multiple objects might be sharing the same instance.
// Maybe avoid this whole situation and just allocate new instances every time? 
void s_take_type_snapshot( struct type_info* type,
   struct type_snapshot* snapshot ) {
   s_take_fine_type_snapshot( type, snapshot, false );
}

void s_take_fine_type_snapshot( struct type_info* type,
   struct type_snapshot* snapshot, bool force_dup_ref ) {
   snapshot->ref = type->ref;
   if ( snapshot->ref && ( snapshot->ref->implicit || force_dup_ref ) ) {
      snapshot->ref = dup_ref( snapshot->ref );
   }
   snapshot->structure = type->structure;
   snapshot->enumeration = type->enumeration;
   snapshot->dim = type->dim;
   snapshot->spec = type->spec;
   snapshot->storage = type->storage;
}

static struct ref* dup_ref( struct ref* ref ) {
   size_t size = 0;
   switch ( ref->type ) {
   case REF_ARRAY: size = sizeof( struct ref_array ); break;
   case REF_STRUCTURE: size = sizeof( struct ref_struct ); break;
   case REF_FUNCTION: size = sizeof( struct ref_func ); break;
   case REF_NULL: size = sizeof( struct ref ); break;
   default:
      UNREACHABLE()
      return NULL;
   }
   struct ref* dup = mem_alloc( size );
   memcpy( dup, ref, size );
   dup->implicit = false;
   return dup;
}

bool s_is_onedim_int_array( struct type_info* type ) {
   return ( type->dim && ! type->dim->next && ! type->structure &&
      ! type->ref && ( type->spec == SPEC_INT || type->spec == SPEC_RAW ) );
}

bool s_is_onedim_int_array_ref( struct semantic* semantic,
   struct type_info* type ) {
   return ( s_describe_type( type ) == TYPEDESC_ARRAYREF &&
      ! type->ref->next && ! type->enumeration && type->spec == SPEC_INT );
}

bool s_is_int_value( struct type_info* type ) {
   struct type_info required_type;
   s_init_type_info_scalar( &required_type, SPEC_INT );
   return s_same_type( &required_type, type );
}

bool s_is_str_value( struct type_info* type ) {
   struct type_info required_type;
   s_init_type_info_scalar( &required_type, SPEC_STR );
   return s_same_type( &required_type, type );
}

bool s_is_enumerator( struct type_info* type ) {
   return ( s_is_value_type( type ) && type->enumeration );
}

bool s_is_null( struct type_info* type ) {
   return ( type->ref && type->ref->type == REF_NULL );
}

bool s_is_nullable( struct type_info* type ) {
   switch ( s_describe_type( type ) ) {
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
   case TYPEDESC_NULLREF:
      return type->ref->nullable;
   default:
      return false;
   }
}

bool s_is_struct( struct type_info* type ) {
   return ( ! type->dim && (
      ( type->ref && type->ref->type == REF_STRUCTURE ) ||
      ( ! type->ref && type->spec == SPEC_STRUCT ) ) );
}

enum type_description s_describe_type( struct type_info* type ) {
   if ( type->dim ) {
      return TYPEDESC_ARRAY;
   }
   else if ( type->ref ) {
      switch ( type->ref->type ) {
      case REF_STRUCTURE: return TYPEDESC_STRUCTREF;
      case REF_ARRAY: return TYPEDESC_ARRAYREF;
      case REF_FUNCTION: return TYPEDESC_FUNCREF;
      case REF_NULL: return TYPEDESC_NULLREF;
      default:
         UNREACHABLE();
         return TYPEDESC_NONE;
      }
   }
   else if ( type->structure ) {
      return TYPEDESC_STRUCT;
   }
   else if ( type->spec == SPEC_ENUM ) {
      return TYPEDESC_ENUM;
   }
   else {
      return TYPEDESC_PRIMITIVE;
   }
}

bool s_is_ref( struct type_info* type ) {
   switch ( s_describe_type( type ) ) {
   case TYPEDESC_ARRAYREF:
   case TYPEDESC_STRUCTREF:
   case TYPEDESC_FUNCREF:
   case TYPEDESC_NULLREF:
      return true;
   default:
      return false;
   }
}

bool s_is_void( struct type_info* type ) {
   return ( s_describe_type( type ) == TYPEDESC_PRIMITIVE &&
      type->spec == SPEC_VOID );
}

void s_reveal( struct type_info* type ) {
   switch ( s_describe_type( type ) ) {
   case TYPEDESC_PRIMITIVE:
      if ( type->enumeration ) {
         type->spec = SPEC_ENUM;
      }
      break;
   default:
      break;
   }
}

bool s_is_str( struct type_info* type ) {
   return ( s_describe_type( type ) == TYPEDESC_PRIMITIVE &&
      type->spec == SPEC_STR );
}

bool s_is_struct_ref( struct type_info* type ) {
   return ( s_describe_type( type ) == TYPEDESC_STRUCTREF );
}

bool s_same_storageignored_type( struct type_info* a, struct type_info* b ) {
   struct type_info temp_a;
   struct type_info temp_b;
   s_init_type_info_copy( &temp_a, a );
   s_init_type_info_copy( &temp_b, b );
   set_storage( &temp_a, STORAGE_MAP );
   set_storage( &temp_b, STORAGE_MAP );
   return s_same_type( &temp_a, &temp_b );
}

static void set_storage( struct type_info* type, int storage ) {
   if ( type->ref ) {
      if ( ! type->ref->implicit ) {
         create_implicit_ref( type );
      }
      switch ( type->ref->type ) {
      case REF_ARRAY:
         {
            struct ref_array* array = ( struct ref_array* ) type->ref;
            array->storage = storage;
         }
         break;
      case REF_STRUCTURE:
         {
            struct ref_struct* structure = ( struct ref_struct* ) type->ref;
            structure->storage = storage;
         }
         break;
      default:
         break;
      }
   }
}
