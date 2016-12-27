#include <string.h>

#include "phase.h"

static bool same_ref( struct ref* a, struct ref* b );
static bool same_ref_struct( struct ref_struct* a, struct ref_struct* b );
static bool same_ref_array( struct ref_array* a, struct ref_array* b );
static bool same_ref_func( struct ref_func* a, struct ref_func* b );
static bool same_spec( int a, int b );
static bool compatible_raw_spec( int spec );
static bool same_dim( struct dim* a, struct dim* b );
static void present_extended_spec( struct structure* structure,
   struct enumeration* enumeration, int spec, struct str* string );
static void present_spec( int spec, struct str* string );
static void present_ref( struct ref* ref, struct str* string );
static void present_dim( struct type_info* type, struct str* string );
static void present_param_list( struct param* param, struct str* string );
static bool is_array_ref_type( struct type_info* type );
static void subscript_array_type( struct semantic* semantic,
   struct type_info* type, struct type_info* element_type );
static struct ref* dup_ref( struct ref* ref );

void s_init_type_info( struct type_info* type, struct ref* ref,
   struct structure* structure, struct enumeration* enumeration,
   struct dim* dim, int spec, int storage ) {
   type->ref = ref;
   type->structure = structure;
   type->enumeration = enumeration;
   type->dim = dim;
   type->spec = spec;
   type->storage = storage;
   type->implicit_ref = false;
   type->builtin_func = false;
}

void s_init_type_info_array_ref( struct type_info* type, struct ref* ref,
   struct structure* structure, struct enumeration* enumeration,
   int dim_count, int spec ) {
   s_init_type_info( type, ref, structure, enumeration, NULL, spec,
      STORAGE_LOCAL );
   struct ref_array* array = &type->implicit_ref_part.array;
   array->ref.next = type->ref;
   array->ref.type = REF_ARRAY;
   array->ref.nullable = false;
   array->dim_count = dim_count;
   array->storage = STORAGE_MAP;
   type->ref = &array->ref;
   type->implicit_ref = true;
}

void s_init_type_info_func( struct type_info* type, struct ref* ref,
   struct structure* structure, struct enumeration* enumeration,
   struct param* params, int return_spec, int min_param, int max_param,
   bool msgbuild ) {
   s_init_type_info( type, ref, structure, enumeration, NULL, return_spec,
      STORAGE_LOCAL );
   // NOTE: At this time, I don't see where in the compiler a distinction needs
   // to be made between a function and a reference-to-function. So decay a
   // function into reference-to-function at all times.
   struct ref_func* func = &type->implicit_ref_part.func;
   func->ref.next = type->ref;
   func->ref.type = REF_FUNCTION;
   func->ref.nullable = false;
   func->params = params;
   func->min_param = min_param;
   func->max_param = max_param;
   func->msgbuild = msgbuild;
   type->ref = &func->ref;
   type->implicit_ref = true;
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
   struct ref* ref = &type->implicit_ref_part.ref;
   ref->next = NULL;
   ref->type = REF_NULL;
   ref->nullable = false;
   type->ref = ref;
   type->implicit_ref = true;
}

void s_decay( struct semantic* semantic, struct type_info* type ) {
   // Array type.
   if ( type->dim ) {
      struct ref_array* array = &type->implicit_ref_part.array;
      array->ref.next = type->ref;
      array->ref.type = REF_ARRAY;
      array->ref.nullable = false;
      array->dim_count = 0;
      array->storage = type->storage;
      struct dim* count_dim = type->dim;
      while ( count_dim ) {
         ++array->dim_count;
         count_dim = count_dim->next;
      }
      type->ref = &array->ref;
      type->dim = NULL;
      type->implicit_ref = true;
   }
   // Structure type.
   else if ( ! type->ref && type->structure ) {
      struct ref_struct* implicit_ref = &type->implicit_ref_part.structure;
      implicit_ref->ref.next = NULL;
      implicit_ref->ref.type = REF_STRUCTURE;
      implicit_ref->ref.nullable = false;
      implicit_ref->storage = type->storage;
      type->ref = &implicit_ref->ref;
      type->implicit_ref = true;
   }
   // Enumeration type.
   else if ( ! type->ref && type->enumeration ) {
      type->spec = s_spec( semantic, type->enumeration->base_type );
   }
}

bool s_same_type( struct type_info* a, struct type_info* b ) {
   if ( a->ref && a->ref->type == REF_NULL ) {
      return ( ! b->dim && b->ref );
   }
   else if ( b->ref && b->ref->type == REF_NULL ) {
      return ( ! a->dim && a->ref );
   }
   else {
      return same_ref( a->ref, b->ref ) &&
         ( a->structure == b->structure ) && (
            ( ( a->spec == SPEC_ENUM || b->spec == SPEC_ENUM ) &&
               a->enumeration == b->enumeration ) ||
            ( same_spec( a->spec, b->spec ) ) ) &&
         same_dim( a->dim, b->dim );
   }
}

bool same_ref( struct ref* a, struct ref* b ) {
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

bool same_ref_struct( struct ref_struct* a, struct ref_struct* b ) {
   return ( a->storage == b->storage );
}

bool same_ref_array( struct ref_array* a, struct ref_array* b ) {
   return ( a->dim_count == b->dim_count && a->storage == b->storage );
}

bool same_ref_func( struct ref_func* a, struct ref_func* b ) {
   struct param* param_a = a->params;
   struct param* param_b = b->params;
   while ( param_a && param_b &&
      param_a->spec == param_b->spec &&
      same_ref( param_a->ref, param_b->ref ) ) {
      param_a = param_a->next;
      param_b = param_b->next;
   }
   return
      ( param_a == NULL && param_b == NULL ) &&
      ( a->msgbuild == b->msgbuild );
}

bool same_spec( int a, int b ) {
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

bool compatible_raw_spec( int spec ) {
   switch ( spec ) {
   case SPEC_RAW:
   case SPEC_INT:
   case SPEC_FIXED:
   case SPEC_BOOL:
   case SPEC_STR:
   case SPEC_ENUM:
      return true;
   }
   return false;
}

bool same_dim( struct dim* a, struct dim* b ) {
   while ( a && b && a->length == b->length ) {
      a = a->next;
      b = b->next;
   }
   return ( a == NULL && b == NULL );
}

bool s_instance_of( struct type_info* type, struct type_info* instance ) {
   // Reference.
   if ( ! type->dim && type->ref ) {
      if ( instance->ref && instance->ref->type == REF_NULL ) {
         return type->ref->nullable;
      }
      else if ( ! type->ref->nullable ) {
         return s_same_type( type, instance ) && ! instance->ref->nullable;
      }
      else {
         return s_same_type( type, instance );
      }
   }
   // Enumeration.
   else if ( type->spec == SPEC_ENUM ) {
      return ( instance->enumeration == type->enumeration );
   }
   else {
      return s_same_type( type, instance );
   }
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

void present_extended_spec( struct structure* structure,
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

void present_spec( int spec, struct str* string ) {
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

void present_ref( struct ref* ref, struct str* string ) {
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
         if ( func->msgbuild ) {
            str_append( string, " " );
            str_append( string, "msgbuild" );
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

void present_dim( struct type_info* type, struct str* string ) {
   struct dim* dim = type->dim;
   while ( dim ) {
      char text[ 11 + 2 ];
      snprintf( text, sizeof( text ), "[%d]", dim->length );
      str_append( string, text );
      dim = dim->next;
   }
}

void present_param_list( struct param* param, struct str* string ) {
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
   else if ( is_array_ref_type( type ) ) {
      s_init_type_info_scalar( &iter->key, s_spec( semantic, SPEC_INT ) );
      subscript_array_type( semantic, type, &iter->value );
      iter->available = true;
   }
   else {
      iter->available = false;
   }
}

bool s_is_str_value_type( struct type_info* type ) {
   return ( s_is_value_type( type ) && type->spec == SPEC_STR );
}

inline bool is_array_ref_type( struct type_info* type ) {
   return ( type->dim || ( type->ref && type->ref->type == REF_ARRAY ) );
}

void subscript_array_type( struct semantic* semantic, struct type_info* type,
   struct type_info* element_type ) {
   if ( type->dim ) {
      s_init_type_info( element_type, type->ref, type->structure,
         type->enumeration, type->dim->next, type->spec, type->storage );
      s_decay( semantic, element_type );
   }
   else if ( type->ref && type->ref->type == REF_ARRAY ) {
      s_init_type_info( element_type, type->ref, type->structure,
         type->enumeration, NULL, type->spec, STORAGE_LOCAL );
      struct ref_array* array = ( struct ref_array* ) type->ref;
      if ( array->dim_count > 1 ) {
         struct ref_array* implicit_array =
            &element_type->implicit_ref_part.array;
         implicit_array->ref.next = array->ref.next;
         implicit_array->ref.type = REF_ARRAY;
         implicit_array->ref.nullable = false;
         implicit_array->dim_count = array->dim_count - 1;
         implicit_array->storage = STORAGE_MAP;
         element_type->ref = &implicit_array->ref;
         element_type->implicit_ref = true;
      }
      else if ( type->ref->next ) {
         element_type->ref = element_type->ref->next;
      }
      else if ( type->structure ) {
         struct ref_struct* implicit_ref =
            &element_type->implicit_ref_part.structure;
         implicit_ref->ref.next = NULL;
         implicit_ref->ref.type = REF_STRUCTURE;
         implicit_ref->ref.nullable = false;
         implicit_ref->storage = STORAGE_MAP;
         element_type->ref = &implicit_ref->ref;
         element_type->structure = type->structure;
         element_type->spec = SPEC_STRUCT;
         element_type->implicit_ref = true;
      }
      else {
         element_type->ref = NULL;
         element_type->spec = s_spec( semantic, type->spec );
      }
   }
   else {
      UNREACHABLE();
   }
}

void s_take_type_snapshot( struct type_info* type,
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
   case REF_STRUCTURE: size = sizeof( struct ref_struct ); break;
   case REF_FUNCTION: size = sizeof( struct ref_func ); break;
   case REF_NULL: size = sizeof( struct ref ); break;
   default:
      UNREACHABLE()
      return NULL;
   }
   void* block = mem_alloc( size );
   memcpy( block, ref, size );
   return block;
}

bool s_is_onedim_int_array( struct type_info* type ) {
   return ( type->dim && ! type->dim->next && ! type->structure &&
      ! type->ref && ( type->spec == SPEC_INT || type->spec == SPEC_RAW ) );
}

bool s_is_int_value( struct type_info* type ) {
   struct type_info required_type;
   s_init_type_info_scalar( &required_type, SPEC_INT );
   return s_same_type( &required_type, type );
}

bool s_is_enumerator( struct type_info* type ) {
   return ( s_is_value_type( type ) && type->enumeration );
}

bool s_is_null( struct type_info* type ) {
   return ( s_is_ref_type( type ) && type->ref->type == REF_NULL );
}