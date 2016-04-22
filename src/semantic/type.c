#include <string.h>

#include "phase.h"

static void init_type_info( struct type_info* type );
static bool same_ref_implicit( struct ref* a, struct type_info* b );
static bool same_ref( struct ref* a, struct ref* b );
static bool same_ref_array( struct ref_array* a, struct ref_array* b );
static bool same_ref_func( struct ref_func* a, struct ref_func* b );
static bool compatible_raw_spec( int spec );
static void present_ref( struct ref* ref, struct str* string,
   bool require_ampersand );
static void present_spec( int spec, struct str* string );
static bool is_array_ref_type( struct type_info* type );
static void subscript_array_type( struct type_info* type,
   struct type_info* element_type );
static struct ref* dup_ref( struct ref* ref );

void s_init_type_info( struct type_info* type, int spec, struct ref* ref,
   struct dim* dim, struct structure* structure,
   struct enumeration* enumeration, struct func* func ) {
   init_type_info( type );
   // Array type.
   if ( dim ) {
      // Implicitly convert to reference-to-array.
      struct ref_array* array = &type->implicit_ref_part.array;
      array->ref.next = ref;
      array->ref.type = REF_ARRAY;
      array->dim_count = 0;
      struct dim* count_dim = dim;
      while ( count_dim ) {
         ++array->dim_count;
         count_dim = count_dim->next;
      }
      type->ref = &array->ref;
      type->structure = structure;
      type->enumeration = enumeration;
      // type->dim = dim;
      type->spec = spec;
      type->implicit_ref = true;
   }
   // Reference type.
   else if ( ref ) {
      type->ref = ref;
      type->structure = structure;
      type->enumeration = enumeration;
      type->spec = spec;
   }
   // Structure type.
   else if ( structure ) {
      // Implicitly convert to reference-to-struct.
      struct ref_struct* implicit_ref = &type->implicit_ref_part.structure;
      implicit_ref->ref.next = NULL;
      implicit_ref->ref.type = REF_STRUCTURE;
      type->ref = &implicit_ref->ref;
      type->structure = structure;
      type->spec = SPEC_STRUCT;
      type->implicit_ref = true;
   }
   // Primitive type.
   else {
      type->enumeration = enumeration;
      type->spec = spec;
   }
}

void s_init_type_info_decayless( struct type_info* type, struct ref* ref,
   struct structure* structure, struct enumeration* enumeration,
   struct dim* dim, int spec ) {
   type->ref = ref;
   type->structure = structure;
   type->enumeration = enumeration;
   type->dim = dim;
   type->spec = spec;
   type->implicit_ref = false;
}

void init_type_info( struct type_info* type ) {
   type->ref = NULL;
   type->structure = NULL;
   type->enumeration = NULL;
   type->dim = NULL;
   type->spec = SPEC_NONE;
   type->implicit_ref = false;
}

void s_init_type_info_func( struct type_info* type, struct ref* ref,
   struct structure* structure, struct enumeration* enumeration,
   struct param* params, int return_spec, int min_param, int max_param,
   bool msgbuild ) {
   struct ref_func* part = &type->implicit_ref_part.func;
   part->ref.next = ref;
   part->ref.type = REF_FUNCTION;
   part->params = params;
   part->min_param = min_param;
   part->max_param = max_param;
   part->msgbuild = msgbuild;
   s_init_type_info( type, return_spec, &part->ref, NULL, structure,
      enumeration, NULL );
}

void s_init_type_info_scalar( struct type_info* type, int spec ) {
   s_init_type_info( type, spec, NULL, NULL, NULL, NULL, NULL );
}

bool s_same_type( struct type_info* a, struct type_info* b ) {
   // Reference.
   if ( ! same_ref( a->ref, b->ref ) ) {
      return false;
   }
   // Structure.
   if ( a->structure != b->structure ) {
      return false;
   }
   // Enumeration.
   if ( a->enumeration && a->enumeration != b->enumeration ) {
      return false;
   }
   // Specifier.
   if ( a->spec == SPEC_RAW ) {
      return compatible_raw_spec( b->spec );
   }
   else if ( b->spec == SPEC_RAW ) {
      return compatible_raw_spec( a->spec );
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
      if ( type->enumeration->name ) {
         struct str name;
         str_init( &name );
         t_copy_name( type->enumeration->name, false, &name );
         str_append( string, name.value );
         str_deinit( &name );
      }
      else {
         str_append( string, "anonymous-enum" );
      }
   }
   else if ( type->structure ) {
      if ( type->structure->anon ) {
         str_append( string, "anonymous-struct" );
      }
      else {
         struct str name;
         str_init( &name );
         t_copy_name( type->structure->name, true, &name );
         str_append( string, name.value );
         str_deinit( &name );
      }
   }
   else {
      present_spec( type->spec, string );
   }
   // Reference.
   present_ref( type->ref, string, false );
}

void present_ref( struct ref* ref, struct str* string,
   bool require_ampersand ) {
   if ( ref ) {
      present_ref( ref->next, string, true );
      if ( ref->type == REF_ARRAY ) {
         struct ref_array* part = ( struct ref_array* ) ref;
         for ( int i = 0; i < part->dim_count; ++i ) {
            str_append( string, "[]" );
         }
         if ( require_ampersand ) {
            str_append( string, "&" );
         }
      }
      else if ( ref->type == REF_STRUCTURE ) {
         str_append( string, "&" );
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
         if ( require_ampersand ) {
            str_append( string, "&" );
         }
      }
      else {
         UNREACHABLE()
      }
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

bool s_is_scalar( struct type_info* type ) {
   return ( ! type->dim && ( type->ref || ! type->structure ) );
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
   if ( s_is_str_value_type( type ) ) {
      s_init_type_info_scalar( &iter->key, SPEC_INT );
      s_init_type_info_scalar( &iter->value, SPEC_INT );
      iter->available = true;
   }
   else if ( is_array_ref_type( type ) ) {
      s_init_type_info_scalar( &iter->key, SPEC_INT );
      subscript_array_type( type, &iter->value );
      iter->available = true;
   }
}

inline bool s_is_str_value_type( struct type_info* type ) {
   return ( s_is_value_type( type ) && type->spec == SPEC_STR );
}

inline bool is_array_ref_type( struct type_info* type ) {
   return ( type->dim || ( type->ref && type->ref->type == REF_ARRAY ) );
}

void subscript_array_type( struct type_info* type,
   struct type_info* element_type ) {
   if ( type->dim ) {
      s_init_type_info( element_type, type->spec, type->ref, type->dim->next,
         type->structure, type->enumeration, NULL );
   }
   else if ( type->ref && type->ref->type == REF_ARRAY ) {
      s_init_type_info( element_type, type->spec, type->ref, NULL,
         type->structure, type->enumeration, NULL );
      struct ref_array* array = ( struct ref_array* ) type->ref;
      if ( array->dim_count > 1 ) {
         struct ref_array* implicit_array =
            &element_type->implicit_ref_part.array;
         implicit_array->ref.next = array->ref.next;
         implicit_array->ref.type = REF_ARRAY;
         implicit_array->dim_count = array->dim_count - 1;
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
         element_type->ref = &implicit_ref->ref;
         element_type->structure = type->structure;
         element_type->spec = SPEC_STRUCT;
         element_type->implicit_ref = true;
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