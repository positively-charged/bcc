#include <stdio.h>
#include <string.h>

#include "phase.h"

enum {
   AREA_VAR,
   AREA_MEMBER,
   AREA_FUNCRETURN,
};

struct ref_reading {
   struct ref* head;
   int storage;
   int storage_index;
};

struct spec_reading {
   struct path* path;
   struct pos pos;
   int type;
   int area;
};

struct params {
   struct param* node;
   struct param* tail;
   int min;
   int max;
};

struct multi_value_read {
   struct multi_value* multi_value;
   struct initial* tail;
};

struct script_reading {
   struct pos pos;
   struct expr* number;
   struct param* param;
   struct param* param_tail;
   struct pos param_pos;
   int num_param;
   int type;
   int flags;
   bool param_specified;
};

struct special_reading {
   struct param* param;
   struct param* param_tail;
   int return_spec;
   int min_param;
   int max_param;
   bool optional;
};

static bool is_dec_bcs( struct parse* parse );
static void read_visibility( struct parse* parse, struct dec* dec );
static void read_object( struct parse* parse, struct dec* dec );
static void read_qual( struct parse* parse, struct dec* dec );
static void read_auto( struct parse* parse, struct dec* dec );
static void read_storage( struct parse* parse, struct dec* dec );
static void read_extended_spec( struct parse* parse, struct dec* dec );
static void read_enum( struct parse* parse, struct dec* dec );
static bool is_enum_def( struct parse* parse );
static void read_enum_def( struct parse* parse, struct dec* dec );
static void read_enum_name( struct parse* parse, struct dec* dec,
   struct enumeration* enumeration );
static void read_enum_base_type( struct parse* parse,
   struct enumeration* enumeration );
static void read_enum_body( struct parse* parse, struct dec* dec,
   struct enumeration* enumeration );
static void read_enumerator( struct parse* parse,
   struct enumeration* enumeration );
static void read_struct( struct parse* parse, struct dec* dec );
static bool is_struct_def( struct parse* parse );
static void read_struct_def( struct parse* parse, struct dec* dec );
static void read_struct_name( struct parse* parse, struct dec* dec,
   struct structure* structure );
static void read_struct_body( struct parse* parse, struct dec* dec,
   struct structure* structure );
void read_struct_member( struct parse* parse, struct dec* parent_dec,
   struct structure* structure );
static struct structure_member* create_structure_member( struct dec* dec );
static bool is_spec( struct parse* parse );
static void init_spec_reading( struct spec_reading* spec, int area );
static void read_spec( struct parse* parse, struct spec_reading* spec );
static void missing_spec( struct parse* parse, struct spec_reading* spec );
static void read_after_spec( struct parse* parse, struct dec* dec );
static void init_ref( struct ref* ref, int type, struct pos* pos );
static void prepend_ref( struct ref_reading* reading, struct ref* part );
static void init_ref_reading( struct ref_reading* reading );
static void read_ref( struct parse* parse, struct ref_reading* reading );
static void read_struct_ref( struct parse* parse,
   struct ref_reading* reading );
static bool is_array_ref( struct parse* parse );
static void read_ref_storage( struct parse* parse,
   struct ref_reading* reading );
static void read_array_ref( struct parse* parse, struct ref_reading* reading );
static void read_ref_func( struct parse* parse, struct ref_reading* reading );
static void read_after_ref( struct parse* parse, struct dec* dec );
static void read_var( struct parse* parse, struct dec* dec );
static void read_instance_list( struct parse* parse, struct dec* dec );
static void read_instance( struct parse* parse, struct dec* dec );
static void read_storage_index( struct parse* parse, struct dec* dec );
static void read_name( struct parse* parse, struct dec* dec );
static bool is_name( struct parse* parse, struct dec* dec );
static void missing_name( struct parse* parse, struct dec* dec );
static void read_dim( struct parse* parse, struct dec* dec );
static void read_var_init( struct parse* parse, struct dec* dec );
static void read_imported_init( struct parse* parse, struct dec* dec );
static bool has_implicit_length_dim( struct dec* dec );
static void read_imported_multi_init( struct parse* parse, struct dim* dim );
static void read_init( struct parse* parse, struct dec* dec );
static void read_multi_init( struct parse* parse, struct dec* dec,
   struct multi_value_read* parent );
static void init_initial( struct initial* initial, bool multi );
static struct value* alloc_value( void );
static void read_single_init( struct parse* parse, struct dec* dec );
static void test_var( struct parse* parse, struct dec* dec );
static void test_storage( struct parse* parse, struct dec* dec );
static void add_var( struct parse* parse, struct dec* dec );
static struct var* alloc_var( struct dec* dec );
static void finish_type_alias( struct parse* parse, struct dec* dec );
static void read_auto_instance_list( struct parse* parse, struct dec* dec );
static void read_foreach_var( struct parse* parse, struct dec* dec );
static bool is_cast_spec( struct parse* parse, int spec );
static void read_func( struct parse* parse, struct dec* dec );
static struct func* alloc_func( struct parse* parse, struct dec* dec );
static void read_func_param_list( struct parse* parse, struct func* func );
static struct func_aspec* alloc_aspec_impl( void );
static void init_params( struct params* params );
static void read_param_list( struct parse* parse, struct params* params );
static void read_param( struct parse* parse, struct params* params );
static void read_param_spec( struct parse* parse, struct params* params,
   struct param* param );
static void read_param_ref( struct parse* parse, struct param* param );
static void read_param_name( struct parse* parse, struct param* param );
static void read_param_default_value( struct parse* parse,
   struct params* params, struct param* param );
static void append_param( struct params* params, struct param* param );
static void read_func_body( struct parse* parse, struct dec* dec,
   struct func* func );
static void init_script_reading( struct script_reading* reading,
   struct pos* pos );
static void read_script_number( struct parse* parse,
   struct script_reading* reading );
static void read_script_param_paren( struct parse* parse,
   struct script_reading* reading );
static void read_script_param_list( struct parse* parse,
   struct script_reading* reading );
static void read_script_param( struct parse* parse,
   struct script_reading* reading );
static void read_script_type( struct parse* parse,
   struct script_reading* reading );
static const char* get_script_article( int type );
static void read_script_flag( struct parse* parse,
   struct script_reading* reading );
static void read_script_after_flag( struct parse* parse,
   struct script_reading* reading );
static void read_script_body( struct parse* parse,
   struct script_reading* reading, struct script* script );
static struct script* add_script( struct parse* parse,
   struct script_reading* reading );
static void read_special( struct parse* parse );
static void init_special_reading( struct special_reading* reading );
static void read_special_param_dec( struct parse* parse,
   struct special_reading* reading );
static void read_special_param_list_minmax( struct parse* parse,
   struct special_reading* reading );
static void read_special_param_list( struct parse* parse,
   struct special_reading* reading );
static void read_special_param( struct parse* parse,
   struct special_reading* reading );
static void read_special_return_type( struct parse* parse,
   struct special_reading* reading );

bool p_is_dec( struct parse* parse ) {
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      switch ( parse->tk ) {
      case TK_INT:
      case TK_BOOL:
      case TK_STR:
      case TK_VOID:
      case TK_WORLD:
      case TK_GLOBAL:
      case TK_STATIC:
         return true;
      default:
         return false;
      }
   default:
      return is_dec_bcs( parse );
   }
}

bool is_dec_bcs( struct parse* parse ) {
   if ( parse->tk == TK_STATIC ) {
      // The `static` keyword is also used by static-assert.
      return ( p_peek( parse ) != TK_ASSERT );
   }
   else {
      switch ( parse->tk ) {
      case TK_INT:
      case TK_FIXED:
      case TK_BOOL:
      case TK_STR:
         // Make sure it is not a conversion call.
         return ( p_peek( parse ) != TK_PAREN_L );
      case TK_RAW:
      case TK_VOID:
      case TK_WORLD:
      case TK_GLOBAL:
      case TK_ENUM:
      case TK_STRUCT:
      case TK_FUNCTION:
      case TK_AUTO:
      case TK_TYPEDEF:
      case TK_PRIVATE:
      case TK_EXTERN:
      case TK_TYPENAME:
      case TK_MSGBUILD:
         return true;
      case TK_ID:
      case TK_UPMOST:
      case TK_NAMESPACE:
         return p_peek_type_path( parse );
      default:
         return false;
      }
   }
}

void p_init_dec( struct dec* dec ) {
   dec->area = DEC_TOP;
   dec->structure = NULL;
   dec->enumeration = NULL;
   dec->path = NULL;
   dec->ref = NULL;
   dec->name = NULL;
   dec->name_offset = NULL;
   dec->dim = NULL;
   dec->vars = NULL;
   dec->var = NULL;
   dec->member = NULL;
   dec->type_alias_object = NULL;
   dec->storage.type = STORAGE_LOCAL;
   dec->storage.specified = false;
   dec->storage_index.value = 0;
   dec->storage_index.specified = false;
   dec->initz.initial = NULL;
   dec->initz.specified = false;
   dec->spec = SPEC_NONE;
   dec->object = DECOBJ_UNDECIDED;
   dec->implicit_type_alias.name = NULL;
   dec->implicit_type_alias.specified = false;
   dec->private_visibility = false;
   dec->static_qual = false;
   dec->msgbuild = false;
   dec->type_alias = false;
   dec->semicolon_absent = false;
   dec->external = false;
   dec->force_local_scope = false;
   dec->anon = false;
}

void p_read_dec( struct parse* parse, struct dec* dec ) {
   dec->pos = parse->tk_pos;
   switch ( parse->tk ) {
   case TK_ENUM:
      read_enum( parse, dec );
      break;
   case TK_STRUCT:
      read_struct( parse, dec );
      break;
   default:
      read_visibility( parse, dec );
   }
}

bool p_is_local_dec( struct parse* parse ) {
   return ( p_is_dec( parse ) || parse->tk == TK_LET );
}

void p_read_local_dec( struct parse* parse, struct dec* dec ) {
   dec->force_local_scope = p_read_let( parse );
   p_read_dec( parse, dec );
}

// Returns whether `let` is specified.
bool p_read_let( struct parse* parse ) {
   if ( parse->tk == TK_LET ) {
      p_read_tk( parse );
      return true;
   }
   else {
      // For a namespace block qualified with `strict`, `let` is implied.
      return parse->ns_fragment->strict;
   }
}

void read_visibility( struct parse* parse, struct dec* dec ) {
   // At this time, visibility can be specified only for variables, functions,
   // and unnamed enumerations.
   if ( parse->tk == TK_PRIVATE ) {
      dec->private_visibility = true;
      p_read_tk( parse );
      if ( parse->tk == TK_ENUM ) {
         read_enum( parse, dec );
         return;
      }
      if ( parse->tk == TK_STRUCT ) {
         read_struct( parse, dec );
         return;
      }
   }
   else if ( parse->tk == TK_EXTERN ) {
      if ( parse->lib->imported ) {
         p_skip_semicolon( parse );
         return;
      }
      else {
         dec->external = true;
         dec->storage.type = STORAGE_MAP;
         p_read_tk( parse );
      }
   }
   read_object( parse, dec );
}

void read_object( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_TYPEDEF ) {
      dec->type_alias = true;
      p_read_tk( parse );
   }
   if ( parse->tk == TK_FUNCTION ) {
      dec->object = DECOBJ_FUNC;
      p_read_tk( parse );
   }
   else {
      switch ( parse->lang ) {
      case LANG_ACS:
      case LANG_ACS95:
         dec->object = DECOBJ_VAR;
         break;
      default:
         break;
      }
   }
   if ( dec->type_alias ) {
      read_extended_spec( parse, dec );
      read_after_spec( parse, dec );
   }
   else {
      read_qual( parse, dec );
      if ( parse->tk == TK_AUTO ) {
         read_auto( parse, dec );
         read_after_ref( parse, dec );
      }
      else {
         read_storage( parse, dec );
         read_extended_spec( parse, dec );
         read_after_spec( parse, dec );
      }
   }
}

void read_qual( struct parse* parse, struct dec* dec ) {
   while (
      parse->tk == TK_STATIC ||
      parse->tk == TK_MSGBUILD ) {
      if ( ( parse->tk == TK_STATIC && dec->static_qual ) ||
         ( parse->tk == TK_MSGBUILD && dec->msgbuild ) ) {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "duplicate `%s` %squalifier", parse->tk_text,
            dec->object == DECOBJ_FUNC ? "function-" : "" );
         p_bail( parse );
      }
      switch ( parse->tk ) {
      case TK_STATIC:
         dec->static_qual = true;
         dec->static_qual_pos = parse->tk_pos;
         p_read_tk( parse );
         break;
      case TK_MSGBUILD:
         if (
            dec->object == DECOBJ_UNDECIDED ||
            dec->object == DECOBJ_FUNC ) {
            dec->msgbuild = true;
            dec->object = DECOBJ_FUNC;
            p_read_tk( parse );
         }
         break;
      default:
         break;
      }
   }
}

void read_auto( struct parse* parse, struct dec* dec ) {
   p_test_tk( parse, TK_AUTO );
   dec->spec = SPEC_AUTO;
   p_read_tk( parse );
   if ( parse->tk == TK_ENUM ) {
      dec->spec = SPEC_AUTOENUM;
      p_read_tk( parse );
   }
}

void read_storage( struct parse* parse, struct dec* dec ) {
   switch ( parse->tk ) {
   case TK_WORLD:
   case TK_GLOBAL:
      dec->storage.type = ( parse->tk == TK_WORLD ) ? STORAGE_WORLD :
         STORAGE_GLOBAL;
      dec->storage.pos = parse->tk_pos;
      dec->storage.specified = true;
      dec->object = DECOBJ_VAR;
      p_read_tk( parse );
      break;
   default:
      break;
   }
}

void read_extended_spec( struct parse* parse, struct dec* dec ) {
   if ( is_enum_def( parse ) ) {
      read_enum_def( parse, dec );
   }
   else if ( is_struct_def( parse ) ) {
      read_struct_def( parse, dec );
   }
   else {
      struct spec_reading spec;
      init_spec_reading( &spec, dec->object == DECOBJ_FUNC ?
         AREA_FUNCRETURN : dec->area == DEC_MEMBER ?
         AREA_MEMBER : AREA_VAR );
      read_spec( parse, &spec );
      dec->type_pos = spec.pos;
      dec->spec = spec.type;
      dec->path = spec.path;
   }
}

void read_enum( struct parse* parse, struct dec* dec ) {
   if ( is_enum_def( parse ) ) {
      read_enum_def( parse, dec );
      if ( parse->tk == TK_SEMICOLON ) {
         dec->enumeration->semicolon = true;
         p_read_tk( parse );
      }
      else {
         dec->semicolon_absent = true;
         read_after_spec( parse, dec );
      }
   }
   else {
      read_object( parse, dec );
   }
}

inline bool is_enum_def( struct parse* parse ) {
   return parse->tk == TK_ENUM && (
      p_peek( parse ) == TK_BRACE_L ||
      p_peek( parse ) == TK_COLON || ( ( p_peek( parse ) == TK_ID ||
         p_peek( parse ) == TK_TYPENAME ) && (
         p_peek_2nd( parse ) == TK_BRACE_L ||
         p_peek_2nd( parse ) == TK_COLON ) ) );
}

void read_enum_def( struct parse* parse, struct dec* dec ) {
   dec->type_pos = parse->tk_pos;
   p_test_tk( parse, TK_ENUM );
   p_read_tk( parse );
   struct enumeration* enumeration = t_alloc_enumeration();
   enumeration->object.pos = dec->type_pos;
   enumeration->hidden = dec->private_visibility;
   enumeration->force_local_scope = dec->force_local_scope;
   read_enum_name( parse, dec, enumeration );
   read_enum_base_type( parse, enumeration );
   read_enum_body( parse, dec, enumeration );
   dec->enumeration = enumeration;
   dec->spec = SPEC_ENUM;
   if ( dec->vars ) {
      list_append( dec->vars, enumeration );
   }
   else {
      p_add_unresolved( parse, &enumeration->object );
      list_append( &parse->ns_fragment->objects, enumeration );
      list_append( &parse->lib->objects, enumeration );
      if ( dec->private_visibility ) {
         list_append( &parse->lib->private_objects, enumeration );
      }
   }
   if ( dec->implicit_type_alias.specified ) {
      struct dec implicit_dec;
      p_init_dec( &implicit_dec );
      if ( dec->vars ) {
         implicit_dec.area = DEC_LOCAL;
         implicit_dec.vars = dec->vars;
      }
      else {
         implicit_dec.area = DEC_TOP;
      }
      implicit_dec.type_alias = true;
      implicit_dec.name = dec->implicit_type_alias.name;
      implicit_dec.name_pos = enumeration->object.pos;
      implicit_dec.enumeration = enumeration;
      implicit_dec.spec = SPEC_ENUM;
      implicit_dec.private_visibility = enumeration->hidden;
      finish_type_alias( parse, &implicit_dec );
   }
   STATIC_ASSERT( DEC_TOTAL == 4 );
   if ( dec->area == DEC_FOR ) {
      p_diag( parse, DIAG_POS_ERR, &dec->type_pos,
         "enum in for-loop initialization" );
      p_bail( parse );
   }
}

void read_enum_name( struct parse* parse, struct dec* dec,
   struct enumeration* enumeration ) {
   if ( parse->tk == TK_ID || parse->tk == TK_TYPENAME ) {
      enumeration->name = t_extend_name( parse->ns->body_enums,
         parse->tk_text );
      enumeration->body = t_extend_name( enumeration->name, "." );
      enumeration->object.pos = parse->tk_pos;
      if ( parse->tk == TK_TYPENAME ) {
         dec->implicit_type_alias.name =
            t_extend_name( parse->ns->body, parse->tk_text );
         dec->implicit_type_alias.specified = true;
      }
      p_read_tk( parse );
   }
   else {
      enumeration->body = parse->ns->body;
   }
}

void read_enum_base_type( struct parse* parse,
   struct enumeration* enumeration ) {
   if ( parse->tk == TK_COLON ) {
      p_read_tk( parse );
      switch ( parse->tk ) {
      case TK_INT:
         p_read_tk( parse );
         break;
      case TK_FIXED:
         enumeration->base_type = SPEC_FIXED;
         p_read_tk( parse );
         break;
      case TK_BOOL:
         enumeration->base_type = SPEC_BOOL;
         p_read_tk( parse );
         break;
      case TK_STR:
         enumeration->base_type = SPEC_STR;
         p_read_tk( parse );
         break;
      default:
         p_unexpect_diag( parse );
         p_unexpect_last_name( parse, NULL, "base type" );
         p_bail( parse );
      }
   }
}

void read_enum_body( struct parse* parse, struct dec* dec,
   struct enumeration* enumeration ) {
   p_test_tk( parse, TK_BRACE_L );
   p_read_tk( parse );
   while ( true ) {
      read_enumerator( parse, enumeration );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
         if ( parse->tk == TK_BRACE_R ) {
            break;
         }
      }
      else {
         break;
      }
   }
   dec->rbrace_pos = parse->tk_pos;
   p_test_tk( parse, TK_BRACE_R );
   p_read_tk( parse );
}

void read_enumerator( struct parse* parse, struct enumeration* enumeration ) {
   if ( parse->tk != TK_ID ) {
      p_unexpect_diag( parse );
      p_unexpect_last_name( parse, NULL, "enumerator" );
      p_bail( parse );
   }
   struct enumerator* enumerator = t_alloc_enumerator();
   enumerator->object.pos = parse->tk_pos;
   enumerator->name = t_extend_name( parse->ns->body, parse->tk_text );
   enumerator->enumeration = enumeration;
   p_read_tk( parse );
   if ( parse->tk == TK_ASSIGN ) {
      p_read_tk( parse );
      struct expr_reading value;
      p_init_expr_reading( &value, true, false, false, true );
      p_read_expr( parse, &value );
      enumerator->initz = value.output_node;
   }
   t_append_enumerator( enumeration, enumerator );
   if ( ! ( parse->tk == TK_COMMA || parse->tk == TK_BRACE_R ) ) {
      p_unexpect_diag( parse );
      p_unexpect_item( parse, NULL, TK_COMMA );
      p_unexpect_last( parse, NULL, TK_BRACE_R );
      p_bail( parse );
   }
}

void read_struct( struct parse* parse, struct dec* dec ) {
   if ( is_struct_def( parse ) ) {
      read_struct_def( parse, dec );
      if ( parse->tk == TK_SEMICOLON ) {
         if ( dec->structure->anon ) {
            p_diag( parse, DIAG_POS_ERR, &dec->structure->object.pos,
               "unnamed and unused struct" );
            p_diag( parse, DIAG_POS, &dec->structure->object.pos,
               "a struct must have a name or be used as an object type" );
            p_bail( parse );
         }
         p_read_tk( parse );
         dec->structure->semicolon = true;
      }
      else {
         dec->semicolon_absent = true;
         read_after_spec( parse, dec );
      }
   }
   else {
      read_object( parse, dec );
   }
}

inline bool is_struct_def( struct parse* parse ) {
   return ( parse->tk == TK_STRUCT && ( p_peek( parse ) == TK_BRACE_L ||
      ( ( p_peek( parse ) == TK_ID || p_peek( parse ) == TK_TYPENAME ) &&
         p_peek_2nd( parse ) == TK_BRACE_L ) ) );
}

void read_struct_def( struct parse* parse, struct dec* dec ) {
   struct structure* structure = t_alloc_structure();
   structure->object.pos = parse->tk_pos;
   structure->force_local_scope = dec->force_local_scope;
   structure->hidden = dec->private_visibility;
   p_test_tk( parse, TK_STRUCT );
   p_read_tk( parse );
   read_struct_name( parse, dec, structure );
   read_struct_body( parse, dec, structure );
   dec->structure = structure;
   dec->spec = SPEC_STRUCT;
   // Nested struct is in the same scope as the parent struct.
   if ( dec->vars ) {
      list_append( dec->vars, structure );
   }
   else {
      p_add_unresolved( parse, &structure->object );
      list_append( &parse->ns_fragment->objects, structure );
      list_append( &parse->lib->objects, structure );
      if ( dec->private_visibility ) {
         list_append( &parse->lib->private_objects, structure );
      }
   }
   if ( dec->implicit_type_alias.specified ) {
      struct dec implicit_dec;
      p_init_dec( &implicit_dec );
      if ( dec->vars ) {
         implicit_dec.area = DEC_LOCAL;
         implicit_dec.vars = dec->vars;
      }
      else {
         implicit_dec.area = DEC_TOP;
      }
      implicit_dec.type_alias = true;
      implicit_dec.name = dec->implicit_type_alias.name;
      implicit_dec.name_pos = structure->object.pos;
      implicit_dec.structure = structure;
      implicit_dec.spec = SPEC_STRUCT;
      implicit_dec.private_visibility = structure->hidden;
      finish_type_alias( parse, &implicit_dec );
   }
}

void read_struct_name( struct parse* parse, struct dec* dec,
   struct structure* structure ) {
   if ( parse->tk == TK_ID || parse->tk == TK_TYPENAME ) {
      structure->name = t_extend_name( parse->ns->body_structs,
         parse->tk_text );
      structure->object.pos = parse->tk_pos;
      if ( parse->tk == TK_TYPENAME ) {
         dec->implicit_type_alias.name =
            t_extend_name( parse->ns->body, parse->tk_text );
         dec->implicit_type_alias.specified = true;
      }
      p_read_tk( parse );
   }
   // When no name is specified, make random name.
   else {
      structure->name = t_create_name();
      structure->anon = true;
   }
}

void read_struct_body( struct parse* parse, struct dec* dec,
   struct structure* structure ) {
   structure->body = t_extend_name( structure->name, "." );
   p_test_tk( parse, TK_BRACE_L );
   p_read_tk( parse );
   while ( true ) {
      read_struct_member( parse, dec, structure );
      if ( parse->tk == TK_BRACE_R ) {
         break;
      }
   }
   dec->rbrace_pos = parse->tk_pos;
   p_test_tk( parse, TK_BRACE_R );
   p_read_tk( parse );
}

void read_struct_member( struct parse* parse, struct dec* parent_dec,
   struct structure* structure ) {
   struct dec dec;
   p_init_dec( &dec );
   dec.area = DEC_MEMBER;
   dec.name_offset = structure->body;
   dec.vars = parent_dec->vars;
   // The private keyword applies to all of the structures declared in the
   // declaration, including nested structures.
   dec.private_visibility = parent_dec->private_visibility;
   read_extended_spec( parse, &dec );
   struct ref_reading ref;
   init_ref_reading( &ref );
   read_ref( parse, &ref );
   dec.ref = ref.head;
   // Instance list.
   while ( true ) {
      read_name( parse, &dec );
      read_dim( parse, &dec );
      struct structure_member* member = create_structure_member( &dec );
      t_append_structure_member( structure, member );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   if ( dec.ref ) {
      structure->has_ref_member = true;
      if ( ! dec.ref->nullable ) {
         structure->has_mandatory_ref_member = true;
      }
   }
}

struct structure_member* create_structure_member( struct dec* dec ) {
   struct structure_member* member = t_alloc_structure_member();
   member->object.pos = dec->name_pos;
   member->name = dec->name;
   member->ref = dec->ref;
   member->structure = dec->structure;
   member->enumeration = dec->enumeration;
   member->path = dec->path;
   member->dim = dec->dim;
   member->spec = dec->spec;
   member->original_spec = dec->spec;
   if ( dec->member ) {
      dec->member->next_instance = member;
   }
   else {
      member->head_instance = true;
   }
   dec->member = member;
   return member;
}

bool is_spec( struct parse* parse ) {
   switch ( parse->tk ) {
   case TK_RAW:
   case TK_INT:
   case TK_FIXED:
   case TK_BOOL:
   case TK_STR:
   case TK_ENUM:
   case TK_STRUCT:
   case TK_TYPENAME:
   case TK_VOID:
      return true;
   default:
      return false;
   }
}

void init_spec_reading( struct spec_reading* spec, int area ) {
   spec->path = NULL;
   spec->type = SPEC_NONE;
   spec->area = area;
}

void read_spec( struct parse* parse, struct spec_reading* spec ) {
   spec->pos = parse->tk_pos;
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      switch ( parse->tk ) {
      case TK_INT:
      case TK_BOOL:
      case TK_STR:
         spec->type = SPEC_RAW;
         p_read_tk( parse );
         break;
      case TK_VOID:
         spec->type = SPEC_VOID;
         p_read_tk( parse );
         break;
      default:
         missing_spec( parse, spec );
         p_bail( parse );
      }
      break;
   case LANG_BCS:
      switch ( parse->tk ) {
      case TK_RAW:
         spec->type = SPEC_RAW;
         p_read_tk( parse );
         break;
      case TK_INT:
         spec->type = SPEC_INT;
         p_read_tk( parse );
         break;
      case TK_FIXED:
         spec->type = SPEC_FIXED;
         p_read_tk( parse );
         break;
      case TK_BOOL:
         spec->type = SPEC_BOOL;
         p_read_tk( parse );
         break;
      case TK_STR:
         spec->type = SPEC_STR;
         p_read_tk( parse );
         break;
      case TK_VOID:
         spec->type = SPEC_VOID;
         p_read_tk( parse );
         break;
      case TK_ENUM:
         p_read_tk( parse );
         spec->type = SPEC_NAME + SPEC_ENUM;
         if ( p_peek_type_path( parse ) ) {
            spec->path = p_read_type_path( parse );
         }
         else {
            spec->path = p_read_path( parse );
         }
         break;
      case TK_STRUCT:
         p_read_tk( parse );
         spec->type = SPEC_NAME + SPEC_STRUCT;
         if ( p_peek_type_path( parse ) ) {
            spec->path = p_read_type_path( parse );
         }
         else {
            spec->path = p_read_path( parse );
         }
         break;
      case TK_ID:
      case TK_UPMOST:
      case TK_NAMESPACE:
      case TK_TYPENAME:
         spec->type = SPEC_NAME;
         spec->path = p_read_type_path( parse );
         break;
      default:
         missing_spec( parse, spec );
         p_bail( parse );
      }
   }
}

void missing_spec( struct parse* parse, struct spec_reading* spec ) {
   const char* subject;
   switch ( spec->area ) {
   case AREA_FUNCRETURN:
      subject = "function return type";
      break;
   case AREA_MEMBER:
      subject = "struct-member type";
      break;
   default:
      subject = "object type";
      break;
   }
   p_unexpect_diag( parse );
   p_unexpect_last_name( parse, NULL, subject );
}

void read_after_spec( struct parse* parse, struct dec* dec ) {
   struct ref_reading ref;
   init_ref_reading( &ref );
   read_ref( parse, &ref );
   dec->ref = ref.head;
   read_after_ref( parse, dec );
}

void init_ref_reading( struct ref_reading* reading ) {
   reading->head = NULL;
   reading->storage = STORAGE_MAP;
   reading->storage_index = 0;
}

void read_ref( struct parse* parse, struct ref_reading* reading ) {
   // Read structure reference.
   switch ( parse->tk ) {
   case TK_GLOBAL:
   case TK_WORLD:
   case TK_SCRIPT:
   case TK_BIT_AND:
   case TK_QUESTION_MARK:
      read_struct_ref( parse, reading );
      break;
   default:
      break;
   }
   // Read array and function references.
   while ( parse->tk == TK_BRACKET_L || parse->tk == TK_FUNCTION ) {
      if ( parse->tk == TK_BRACKET_L ) {
         if ( is_array_ref( parse ) ) {
            read_array_ref( parse, reading );
         }
         else {
            break;
         }
      }
      else {
         switch ( parse->tk ) {
         case TK_FUNCTION:
            read_ref_func( parse, reading );
            break;
         default:
            UNREACHABLE()
         }
      }
      if ( parse->tk == TK_BIT_AND ) {
         p_read_tk( parse );
      }
      else if ( parse->tk == TK_QUESTION_MARK ) {
         reading->head->nullable = true;
         p_read_tk( parse );
      }
      else {
         p_unexpect_diag( parse );
         p_unexpect_item( parse, NULL, TK_BIT_AND );
         p_unexpect_last( parse, NULL, TK_QUESTION_MARK );
         p_bail( parse );
      }
   }
}

void init_ref( struct ref* ref, int type, struct pos* pos ) {
   ref->next = NULL;
   ref->type = type;
   ref->pos = *pos;
   ref->nullable = false;
}

void prepend_ref( struct ref_reading* reading, struct ref* part ) {
   part->next = reading->head;
   reading->head = part;
}

void read_struct_ref( struct parse* parse, struct ref_reading* reading ) {
   bool nullable = false;
   if ( parse->tk == TK_QUESTION_MARK ) {
      nullable = true;
   }
   else {
      p_test_tk( parse, TK_BIT_AND );
   }
   struct ref_struct* part = mem_alloc( sizeof( *part ) );
   init_ref( &part->ref, REF_STRUCTURE, &parse->tk_pos );
   part->ref.nullable = nullable;
   part->storage = STORAGE_MAP;
   part->storage_index = 0;
   prepend_ref( reading, &part->ref );
   p_read_tk( parse );
}

bool is_array_ref( struct parse* parse ) {
   struct parsertk_iter iter;
   p_init_parsertk_iter( parse, &iter );
   p_next_tk( parse, &iter );
   if ( parse->tk == TK_BRACKET_L && iter.token->type == TK_BRACKET_R ) {
      p_next_tk( parse, &iter );
      while ( true ) {
         if ( iter.token->type == TK_BRACKET_L ) {
            p_next_tk( parse, &iter );
            if ( iter.token->type == TK_BRACKET_R ) {
               p_next_tk( parse, &iter );
            }
            else {
               return false;
            }
         }
         else {
            break;
         }
      }
      if ( iter.token->type == TK_BIT_AND ||
         iter.token->type == TK_QUESTION_MARK ) {
         return true;
      }
   }
   return false;
}

void read_array_ref( struct parse* parse, struct ref_reading* reading ) {
   struct pos pos = parse->tk_pos;
   int count = 0;
   while ( parse->tk == TK_BRACKET_L ) {
      p_read_tk( parse );
      p_test_tk( parse, TK_BRACKET_R );
      p_read_tk( parse );
      ++count;
   }
   struct ref_array* part = mem_alloc( sizeof( *part ) );
   init_ref( &part->ref, REF_ARRAY, &pos );
   part->dim_count = count;
   part->storage = STORAGE_MAP;
   part->storage_index = 0;
   prepend_ref( reading, &part->ref );
}

void read_ref_func( struct parse* parse, struct ref_reading* reading ) {
   struct ref_func* part = t_alloc_ref_func();
   part->ref.pos = parse->tk_pos;
   p_test_tk( parse, TK_FUNCTION );
   p_read_tk( parse );
   // Read parameter list.
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   if ( parse->tk != TK_PAREN_R ) {
      struct params params;
      init_params( &params );
      read_param_list( parse, &params );
      part->params = params.node;
      part->min_param = params.min;
      part->max_param = params.max;
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   // Read function qualifier.
   if ( parse->tk == TK_MSGBUILD ) {
      part->msgbuild = true;
      p_read_tk( parse );
   }
   // Done.
   prepend_ref( reading, &part->ref );
}

void read_after_ref( struct parse* parse, struct dec* dec ) {
   if ( dec->object == DECOBJ_UNDECIDED ) {
      if ( is_name( parse, dec ) && p_peek( parse ) == TK_PAREN_L ) {
         dec->object = DECOBJ_FUNC;
      }
      else {
         dec->object = DECOBJ_VAR;
      }
   }
   if ( dec->object == DECOBJ_FUNC ) {
      read_func( parse, dec );
   }
   else {
      read_var( parse, dec );
   }
}

void read_var( struct parse* parse, struct dec* dec ) {
   if (
      dec->spec == SPEC_AUTO ||
      dec->spec == SPEC_AUTOENUM ) {
      read_auto_instance_list( parse, dec );
   }
   else {
      read_instance_list( parse, dec );
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
}

void read_instance_list( struct parse* parse, struct dec* dec ) {
   if ( dec->semicolon_absent && ! ( parse->tk == TK_ID ||
      parse->tk == TK_LIT_DECIMAL || dec->ref ) ) {
      p_unexpect_diag( parse );
      p_increment_pos( &dec->rbrace_pos, TK_BRACE_R );
      p_unexpect_name( parse, NULL,
         "continuation of variable declaration" );
      p_unexpect_last( parse, &dec->rbrace_pos, TK_SEMICOLON );
      p_bail( parse );
   }
   while ( true ) {
      read_instance( parse, dec );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
}

void read_instance( struct parse* parse, struct dec* dec ) {
   if ( dec->type_alias ) {
      read_name( parse, dec );
      read_dim( parse, dec );
      finish_type_alias( parse, dec );
   }
   else {
      read_storage_index( parse, dec );
      read_name( parse, dec );
      if (
         parse->lang == LANG_ACS ||
         parse->lang == LANG_BCS ) {
         read_dim( parse, dec );
      }
      read_var_init( parse, dec );
      test_var( parse, dec );
      add_var( parse, dec );
   }
}

void read_storage_index( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_LIT_DECIMAL ) {
      p_test_tk( parse, TK_LIT_DECIMAL );
      dec->storage_index.pos = parse->tk_pos;
      dec->storage_index.value = p_extract_literal_value( parse );
      dec->storage_index.specified = true;
      p_read_tk( parse );
      p_test_tk( parse, TK_COLON );
      p_read_tk( parse );
   }
   else {
      dec->storage_index.value = 0;
      dec->storage_index.specified = false;
   }
}

void read_name( struct parse* parse, struct dec* dec ) {
   if ( is_name( parse, dec ) ) {
      dec->name = t_extend_name( dec->name_offset, parse->tk_text );
      dec->name_pos = parse->tk_pos;
      p_read_tk( parse );
   }
   else {
      missing_name( parse, dec );
   }
}

inline bool is_name( struct parse* parse, struct dec* dec ) {
   return ( ( dec->type_alias && parse->tk == TK_TYPENAME ) ||
      ( ! dec->type_alias && parse->tk == TK_ID ) );
}

void missing_name( struct parse* parse, struct dec* dec ) {
   if ( dec->type_alias ) {
      p_unexpect_diag( parse );
      p_unexpect_last( parse, NULL, TK_TYPENAME );
      p_bail( parse );
   }
   else {
      const char* subject;
      if ( dec->object == DECOBJ_FUNC ) {
         subject = "function name";
      }
      else if ( dec->area == DEC_MEMBER ) {
         subject = "struct-member name";
      }
      else {
         subject = "variable name";
      }
      p_unexpect_diag( parse );
      p_unexpect_last_name( parse, NULL, subject );
      p_bail( parse );
   }
}

void read_dim( struct parse* parse, struct dec* dec ) {
   dec->dim = NULL;
   struct dim* tail = NULL;
   while ( parse->tk == TK_BRACKET_L ) {
      struct dim* dim = t_alloc_dim();
      dim->pos = parse->tk_pos;
      p_read_tk( parse );
      if ( parse->tk == TK_BRACKET_R ) {
         p_read_tk( parse );
      }
      else {
         struct expr_reading size;
         p_init_expr_reading( &size, false, false, false, true );
         p_read_expr( parse, &size );
         dim->length_node = size.output_node;
         p_test_tk( parse, TK_BRACKET_R );
         p_read_tk( parse );
      }
      if ( tail ) {
         tail->next = dim;
      }
      else {
         dec->dim = dim;
      }
      tail = dim;
   }
}

void read_var_init( struct parse* parse, struct dec* dec ) {
   dec->initz.pos = parse->tk_pos;
   dec->initz.initial = NULL;
   if ( parse->lib->imported ) {
      read_imported_init( parse, dec );
   }
   else {
      read_init( parse, dec );
   }
}

void read_imported_init( struct parse* parse, struct dec* dec ) {
   // Assume global and world variables cannot be initialized.
   if ( ! dec->storage.specified ) {
      if ( dec->dim ) {
         if ( parse->tk == TK_ASSIGN || has_implicit_length_dim( dec ) ) {
            p_test_tk( parse, TK_ASSIGN );
            p_read_tk( parse );
            read_imported_multi_init( parse, dec->dim );
         }
      }
      else {
         if ( parse->tk == TK_ASSIGN ) {
            // The initializer of an imported variable is not needed.
            while ( ! (
               parse->tk == TK_SEMICOLON ||
               parse->tk == TK_COMMA ||
               parse->tk == TK_END ) ) {
               p_read_tk( parse );
            }
         }
      }
   }
}

bool has_implicit_length_dim( struct dec* dec ) {
   struct dim* dim = dec->dim;
   while ( dim ) {
      if ( ! dim->length_node ) {
         return true;
      }
      dim = dim->next;
   }
   return false;
}

void read_imported_multi_init( struct parse* parse, struct dim* dim ) {
   p_test_tk( parse, TK_BRACE_L );
   p_read_tk( parse );
   int length = 0;
   while ( true ) {
      if ( dim->next ) {
         read_imported_multi_init( parse, dim->next );
      }
      else {
         if ( parse->tk == TK_BRACE_R ) {
            p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
               "empty brace initializer" );
            p_bail( parse );
         }
         // Structure element initializer.
         if ( parse->tk == TK_BRACE_L ) {
            p_skip_block( parse );
         }
         else {
            while ( ! (
               parse->tk == TK_COMMA ||
               parse->tk == TK_BRACE_R ||
               parse->tk == TK_END ) ) {
               p_read_tk( parse );
            }
         }
      }
      ++length;
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
         if ( parse->tk == TK_BRACE_R ) {
            break;
         }
      }
      else {
         break;
      }
   }
   p_test_tk( parse, TK_BRACE_R );
   p_read_tk( parse );
   if ( ! dim->length_node && length > dim->length ) {
      dim->length = length;
   }
}

void read_init( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_ASSIGN ) {
      dec->initz.specified = true;
      p_read_tk( parse );
      // Multi-value initializer.
      switch ( parse->lang ) {
      case LANG_ACS:
      case LANG_BCS:
         if ( parse->tk == TK_BRACE_L ) {
            read_multi_init( parse, dec, NULL );
            return;
         }
         break;
      default:
         break;
      }
      // Single-value initializer.
      read_single_init( parse, dec );
   }
}

void read_multi_init( struct parse* parse, struct dec* dec,
   struct multi_value_read* parent ) {
   p_test_tk( parse, TK_BRACE_L );
   struct multi_value* multi_value = mem_alloc( sizeof( *multi_value ) );
   init_initial( &multi_value->initial, true );
   multi_value->body = NULL;
   multi_value->pos = parse->tk_pos;
   multi_value->padding = 0;
   struct multi_value_read read;
   read.multi_value = multi_value;
   read.tail = NULL;
   p_read_tk( parse );
   if ( parse->tk == TK_BRACE_R ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "empty brace initializer" );
      p_bail( parse );
   }
   while ( true ) {
      if ( parse->tk == TK_BRACE_L ) { 
         read_multi_init( parse, dec, &read );
      }
      else {
         struct expr_reading expr;
         p_init_expr_reading( &expr, false, false, false, true );
         p_read_expr( parse, &expr );
         struct value* value = alloc_value();
         value->expr = expr.output_node;
         if ( read.tail ) {
            read.tail->next = &value->initial;
         }
         else {
            multi_value->body = &value->initial;
         }
         read.tail = &value->initial;
      }
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
         if ( parse->tk == TK_BRACE_R ) {
            p_read_tk( parse );
            break;
         }
      }
      else {
         p_test_tk( parse, TK_BRACE_R );
         p_read_tk( parse );
         break;
      }
   }
   // Attach multi-value initializer to parent.
   if ( parent ) {
      if ( parent->tail ) {
         parent->tail->next = &multi_value->initial;
      }
      else {
         parent->multi_value->body = &multi_value->initial;
      }
      parent->tail = &multi_value->initial;
   }
   else {
      dec->initz.initial = &multi_value->initial;
   }
}

void init_initial( struct initial* initial, bool multi ) {
   initial->next = NULL;
   initial->multi = multi;
   initial->tested = false;
}

struct value* alloc_value( void ) {
   struct value* value = mem_alloc( sizeof( *value ) );
   init_initial( &value->initial, false );
   value->expr = NULL;
   value->var = NULL;
   value->func = NULL;
   value->next = NULL;
   value->type = VALUE_OTHER;
   value->index = 0;
   value->string_initz = false;
   return value;
}

void read_single_init( struct parse* parse, struct dec* dec ) {
   struct expr_reading expr;
   p_init_expr_reading( &expr, false, false, false, true );
   p_read_expr( parse, &expr );
   struct value* value = alloc_value();
   value->expr = expr.output_node;
   dec->initz.initial = &value->initial;
}

void test_var( struct parse* parse, struct dec* dec ) {
   if ( dec->static_qual && ! ( dec->area == DEC_LOCAL ||
      dec->area == DEC_FOR ) ) {
      p_diag( parse, DIAG_POS_ERR, &dec->name_pos,
         "static non-local variable" );
      p_diag( parse, DIAG_POS, &dec->name_pos,
         "only variables in script/function can be static-qualified" );
      p_bail( parse );
   }
   test_storage( parse, dec );
   // Cannot place multi-value variable in scalar portion of storage, where
   // a slot can hold only a single value.
   // TODO: Come up with syntax to navigate the array portion of storage,
   // the struct being the map. This way, you can access the storage data
   // using a struct member instead of an array index.
   if ( dec->spec == SPEC_STRUCT && ! dec->dim && (
      dec->storage.type == STORAGE_WORLD ||
      dec->storage.type == STORAGE_GLOBAL ) ) {
      p_diag( parse, DIAG_POS_ERR, &dec->name_pos,
         "variable of struct type in scalar portion of storage" );
      p_bail( parse );
   }
}

void test_storage( struct parse* parse, struct dec* dec ) {
   if ( ! dec->storage.specified && ! dec->external ) {
      // Variable found at namespace scope, or a static local variable, has map
      // storage.
      if ( dec->area == DEC_TOP || ( ( dec->area == DEC_LOCAL ||
         dec->area == DEC_FOR ) && dec->static_qual ) ) {
         dec->storage.type = STORAGE_MAP;
      }
      else {
         dec->storage.type = STORAGE_LOCAL;
      }
   }
   if ( dec->storage_index.specified ) {
      int max = parse->lang_limits->max_world_vars;
      if ( dec->storage.type != STORAGE_WORLD ) {
         if ( dec->storage.type == STORAGE_GLOBAL ) {
            max = parse->lang_limits->max_global_vars;
         }
         else  {
            p_diag( parse, DIAG_POS_ERR, &dec->storage_index.pos,
               "storage-number specified for %s-storage variable",
               t_get_storage_name( dec->storage.type ) );
            p_bail( parse );
         }
      }
      if ( dec->storage_index.value >= max ) {
         p_diag( parse, DIAG_POS_ERR, &dec->storage_index.pos,
            "storage number not between 0 and %d, inclusive", max - 1 );
         p_bail( parse );
      }
   }
   else {
      // Storage-number must be explicitly specified for world and global
      // storage variables.
      if ( dec->storage.type == STORAGE_WORLD ||
         dec->storage.type == STORAGE_GLOBAL ) {
         p_diag( parse, DIAG_POS_ERR, &dec->name_pos,
            "%s-storage variable missing storage-number",
            t_get_storage_name( dec->storage.type ) );
         p_bail( parse );
      }
   }
}

void add_var( struct parse* parse, struct dec* dec ) {
   struct var* var = alloc_var( dec );
   var->imported = parse->lib->imported;
   if ( dec->area == DEC_TOP ) {
      p_add_unresolved( parse, &var->object );
      list_append( &parse->lib->objects, var );
      list_append( &parse->ns_fragment->objects, var );
      if ( dec->external ) {
         list_append( &parse->lib->external_vars, var );
      }
      else {
         list_append( &parse->lib->vars, var );
         if ( var->hidden ) {
            list_append( &parse->lib->private_objects, var );
         }
      }
   }
   else if ( dec->area == DEC_LOCAL || dec->area == DEC_FOR ) {
      var->hidden = dec->static_qual;
      if ( dec->vars ) {
         list_append( dec->vars, var );
      }
      if ( dec->static_qual ) {
         var->hidden = true;
         list_append( &parse->lib->vars, var );
      }
      else {
         list_append( parse->local_vars, var );
      }
   }
   else {
      UNREACHABLE();
   }
   if ( dec->var ) {
      dec->var->next_instance = var;
   }
   else {
      var->head_instance = true;
   }
   dec->var = var;
}

struct var* alloc_var( struct dec* dec ) {
   struct var* var = t_alloc_var();
   var->object.pos = dec->name_pos;
   var->name = dec->name;
   var->ref = dec->ref;
   var->structure = dec->structure;
   var->enumeration = dec->enumeration;
   var->type_path = dec->path;
   var->dim = dec->dim;
   var->initial = dec->initz.initial;
   var->spec = dec->spec;
   var->original_spec = dec->spec;
   var->storage = dec->storage.type;
   var->index = dec->storage_index.value;
   var->hidden = dec->private_visibility;
   var->is_constant_init =
      ( dec->static_qual || dec->area == DEC_TOP ) ? true : false;
   var->force_local_scope = dec->force_local_scope;
   var->external = dec->external;
   var->anon = dec->anon;
   return var;
}

void finish_type_alias( struct parse* parse, struct dec* dec ) {
   struct type_alias* alias = t_alloc_type_alias();
   t_init_object( &alias->object, NODE_TYPE_ALIAS );
   alias->object.pos = dec->name_pos;
   alias->name = dec->name;
   alias->ref = dec->ref;
   alias->structure = dec->structure;
   alias->enumeration = dec->enumeration;
   alias->path = dec->path;
   alias->dim = dec->dim;
   alias->spec = dec->spec;
   alias->original_spec = dec->spec;
   alias->force_local_scope = dec->force_local_scope;
   alias->hidden = dec->private_visibility;
   if ( dec->area == DEC_TOP ) {
      p_add_unresolved( parse, &alias->object );
      list_append( &parse->ns_fragment->objects, alias );
      if ( dec->private_visibility ) {
         list_append( &parse->lib->private_objects, alias );
      }
   }
   else {
      list_append( dec->vars, alias );
   }
   if ( dec->type_alias_object ) {
      dec->type_alias_object->next_instance = alias;
   }
   else {
      alias->head_instance = true;
   }
   dec->type_alias_object = alias;
}

void read_auto_instance_list( struct parse* parse, struct dec* dec ) {
   while ( true ) {
      read_name( parse, dec );
      read_var_init( parse, dec );
      test_var( parse, dec );
      add_var( parse, dec );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
}

struct var* p_read_cond_var( struct parse* parse ) {
   struct dec dec;
   p_init_dec( &dec );
   dec.area = DEC_LOCAL;
   dec.name_offset = parse->ns->body;
   dec.force_local_scope = p_read_let( parse );
   if ( parse->tk == TK_AUTO ) {
      dec.spec = SPEC_AUTO;
      p_read_tk( parse );
      if ( parse->tk == TK_ENUM ) {
         dec.spec = SPEC_AUTOENUM;
         p_read_tk( parse );
      }
   }
   else {
      struct spec_reading spec;
      init_spec_reading( &spec, AREA_VAR );
      read_spec( parse, &spec );
      dec.type_pos = spec.pos;
      dec.spec = spec.type;
      dec.path = spec.path;
      struct ref_reading ref;
      init_ref_reading( &ref );
      read_ref( parse, &ref );
      dec.ref = ref.head;
   }
   read_name( parse, &dec );
   p_test_tk( parse, TK_ASSIGN );
   p_read_tk( parse );
   read_single_init( parse, &dec );
   return alloc_var( &dec );
}

void p_read_foreach_item( struct parse* parse, struct foreach_stmt* stmt ) {
   struct dec dec;
   p_init_dec( &dec );
   dec.name_offset = parse->ns->body;
   read_foreach_var( parse, &dec );
   stmt->value = alloc_var( &dec );
   if ( parse->tk == TK_COMMA ) {
      p_read_tk( parse );
      read_name( parse, &dec );
      stmt->key = stmt->value;
      stmt->value = alloc_var( &dec );
      p_test_tk( parse, TK_SEMICOLON );
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_SEMICOLON );
      p_read_tk( parse );
      if ( parse->tk == TK_AUTO || is_spec( parse ) || parse->tk == TK_LET ) {
         stmt->key = stmt->value;
         p_init_dec( &dec );
         dec.name_offset = parse->ns->body;
         read_foreach_var( parse, &dec );
         stmt->value = alloc_var( &dec );
         p_test_tk( parse, TK_SEMICOLON );
         p_read_tk( parse );
      }
   }
}

void read_foreach_var( struct parse* parse, struct dec* dec ) {
   dec->force_local_scope = p_read_let( parse );
   if ( parse->tk == TK_AUTO ) {
      dec->spec = SPEC_AUTO;
      p_read_tk( parse );
      if ( parse->tk == TK_ENUM ) {
         dec->spec = SPEC_AUTOENUM;
         p_read_tk( parse );
      }
   }
   else {
      struct spec_reading spec;
      init_spec_reading( &spec, AREA_VAR );
      read_spec( parse, &spec );
      dec->type_pos = spec.pos;
      dec->spec = spec.type;
      dec->path = spec.path;
      struct ref_reading ref;
      init_ref_reading( &ref );
      read_ref( parse, &ref );
      dec->ref = ref.head;
   }
   read_name( parse, dec );
}

bool p_is_paren_type( struct parse* parse ) {
   if ( parse->tk == TK_PAREN_L ) {
      switch ( p_peek( parse ) ) {
      case TK_RAW:
      case TK_ENUM:
      case TK_STRUCT:
      case TK_TYPENAME:
      case TK_VOID:
      case TK_AUTO:
      case TK_STATIC:
      case TK_FUNCTION:
      case TK_MSGBUILD:
         return true;
      case TK_INT:
      case TK_FIXED:
      case TK_BOOL:
      case TK_STR:
         {
            // Make sure we are not dealing with a conversion call.
            struct parsertk_iter iter;
            p_init_parsertk_iter( parse, &iter );
            p_next_tk( parse, &iter );
            p_next_tk( parse, &iter );
            if ( iter.token->type == TK_PAREN_L ) {
               p_next_tk( parse, &iter );
               switch ( iter.token->type ) {
               case TK_RAW:
               case TK_INT:
               case TK_FIXED:
               case TK_BOOL:
               case TK_STR:
               case TK_ENUM:
               case TK_STRUCT:
               case TK_TYPENAME:
               case TK_VOID:
               case TK_PAREN_R:
                  return true;
               default:
                  break;
               }
            }
            else {
               return true;
            }
         }
         break;
      default:
         break;
      }
   }
   return false;
}

void p_init_paren_reading( struct parse* parse,
   struct paren_reading* reading ) {
   reading->var = NULL;
   reading->func = NULL;
   t_init_pos_id( &reading->cast.pos, ALTERN_FILENAME_COMPILER );
   reading->cast.spec = SPEC_NONE;
}

void p_read_paren_type( struct parse* parse, struct paren_reading* reading ) {
   p_test_tk( parse, TK_PAREN_L );
   struct dec dec;
   p_init_dec( &dec );
   dec.pos = parse->tk_pos;
   dec.name = parse->task->blank_name;
   dec.name_pos = parse->tk_pos;
   dec.private_visibility = true;
   dec.anon = true;
   if ( parse->local_vars ) {
      dec.area = DEC_LOCAL;
   }
   else {
      dec.area = DEC_TOP;
   }
   p_read_tk( parse );
   // Function keyword.
   if ( parse->tk == TK_FUNCTION ) {
      dec.object = DECOBJ_FUNC;
      p_read_tk( parse );
   }
   // Qualifier.
   read_qual( parse, &dec );
   // Specifier.
   if ( parse->tk == TK_AUTO ) {
      read_auto( parse, &dec );
   }
   else {
      struct spec_reading spec;
      init_spec_reading( &spec, AREA_VAR );
      read_spec( parse, &spec );
      dec.type_pos = spec.pos;
      dec.spec = spec.type;
      dec.path = spec.path;
   }
   // Reference.
   struct ref_reading ref;
   init_ref_reading( &ref );
   read_ref( parse, &ref );
   dec.ref = ref.head;
   struct func* func = NULL;
   if ( dec.object == DECOBJ_FUNC || dec.spec == SPEC_AUTO ||
      parse->tk == TK_PAREN_L ) {
      func = alloc_func( parse, &dec );
      func->literal = true;
      p_test_tk( parse, TK_PAREN_L );
      p_read_tk( parse );
      read_func_param_list( parse, func );
      p_test_tk( parse, TK_PAREN_R );
      p_read_tk( parse );
   }
   // Dimension, for array literals. If a reference type is specified, then
   // force it to be a part of an array literal, since a reference type is not
   // used in a cast or a structure variable literal.
   else if ( parse->tk == TK_BRACKET_L || dec.ref ) {
      p_test_tk( parse, TK_BRACKET_L );
      read_dim( parse, &dec );
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   // Function literal.
   if ( func ) {
      read_func_body( parse, &dec, func );
      if ( dec.area == DEC_TOP ) {
         p_add_unresolved( parse, &func->object );
         list_append( &parse->lib->funcs, func );
         list_append( &parse->ns_fragment->funcs, func );
      }
      else {
         if ( ! ( ( struct func_user* ) func->impl )->local ) {
            list_append( &parse->lib->funcs, func );
            func->hidden = true;
         }
      }
      reading->func = func;
   }
   // Compound literal.
   else if ( dec.static_qual || dec.dim || parse->tk == TK_BRACE_L ||
      ! is_cast_spec( parse, dec.spec ) ) {
      // Inializer.
      dec.initz.pos = parse->tk_pos;
      dec.initz.specified = true;
      // String-based initializer, for array literals.
      if ( dec.dim && parse->tk == TK_LIT_STRING ) {
         read_single_init( parse, &dec );
      }
      else {
         read_multi_init( parse, &dec, NULL );
      }
      // Variable.
      test_var( parse, &dec );
      add_var( parse, &dec );
      reading->var = dec.var;
   }
   // Cast.
   else {
      reading->cast.pos = dec.pos;
      reading->cast.spec = dec.spec;
   }
}

bool is_cast_spec( struct parse* parse, int spec ) {
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

void read_func( struct parse* parse, struct dec* dec ) {
   read_name( parse, dec );
   struct func* func = alloc_func( parse, dec );
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   read_func_param_list( parse, func );
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   if ( dec->type_alias ) {
      func->type = FUNC_ALIAS;
   }
   else if ( parse->tk == TK_SEMICOLON ) {
      func->impl = t_alloc_func_user();
      p_read_tk( parse );
   }
   else {
      read_func_body( parse, dec, func );
   }
   if ( dec->area == DEC_TOP ) {
      p_add_unresolved( parse, &func->object );
      list_append( &parse->ns_fragment->objects, func );
      list_append( &parse->lib->objects, func );
      if ( func->hidden ) {
         list_append( &parse->lib->private_objects, func );
      }
      if ( func->type == FUNC_USER ) {
         if ( func->external ) {
            list_append( &parse->lib->external_funcs, func );
         }
         else {
            list_append( &parse->lib->funcs, func );
            list_append( &parse->ns_fragment->funcs, func );
         }
      }
   }
   else {
      list_append( dec->vars, func );
      if ( func->type == FUNC_USER &&
         ! ( ( struct func_user* ) func->impl )->local ) {
         list_append( &parse->lib->funcs, func );
         func->hidden = true;
      }
   }
}

struct func* alloc_func( struct parse* parse, struct dec* dec ) {
   struct func* func = t_alloc_func();
   func->object.pos = dec->name_pos;
   func->type = FUNC_USER;
   func->ref = dec->ref;
   func->structure = dec->structure;
   func->enumeration = dec->enumeration;
   func->path = dec->path;
   func->name = dec->name;
   func->return_spec = dec->spec;
   func->original_return_spec = dec->spec;
   func->hidden = dec->private_visibility;
   func->msgbuild = dec->msgbuild;
   func->imported = parse->lib->imported;
   func->external = dec->external;
   func->force_local_scope = dec->force_local_scope;
   return func;
}

void read_func_param_list( struct parse* parse, struct func* func ) {
   // In BCS, the parameter list can be empty. The `void` keyword is optional.
   if ( ! ( parse->lang == LANG_BCS && parse->tk == TK_PAREN_R ) ) {
      struct params params;
      init_params( &params );
      read_param_list( parse, &params );
      func->params = params.node;
      func->min_param = params.min;
      func->max_param = params.max;
   }
}

void init_params( struct params* params ) {
   params->node = NULL;
   params->tail = NULL;
   params->min = 0;
   params->max = 0;
} 

void read_param_list( struct parse* parse, struct params* params ) {
   // The reason we peek is because the `void` keyword might be part of a
   // function-reference parameter. We don't want to leave prematurely.
   if ( parse->tk == TK_VOID && ! ( parse->lang == LANG_BCS &&
      p_peek( parse ) != TK_PAREN_R ) ) {
      p_read_tk( parse );
   }
   else {
      while ( true ) {
         read_param( parse, params );
         if ( parse->tk == TK_COMMA ) {
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
   }
}

void read_param( struct parse* parse, struct params* params ) {
   struct param* param = t_alloc_param();
   param->object.pos = parse->tk_pos;
   read_param_spec( parse, params, param );
   read_param_ref( parse, param );
   read_param_name( parse, param );
   read_param_default_value( parse, params, param );
   append_param( params, param );
}

void read_param_spec( struct parse* parse, struct params* params,
   struct param* param ) {
   switch ( parse->lang ) {
      struct spec_reading spec;
   case LANG_ACS:
      switch ( parse->tk ) {
      case TK_INT:
      case TK_BOOL:
      case TK_STR:
         param->spec = SPEC_RAW;
         param->original_spec = SPEC_RAW;
         p_read_tk( parse );
         break;
      default:
         p_unexpect_diag( parse );
         if ( params->node ) {
            p_unexpect_last_name( parse, NULL, "parameter type" );
         }
         else {
            p_unexpect_name( parse, NULL, "parameter type" );
            p_unexpect_last( parse, NULL, TK_VOID );
         }
         p_bail( parse );
      }
      break;
   default:
      init_spec_reading( &spec, AREA_VAR );
      read_spec( parse, &spec );
      param->path = spec.path;
      param->spec = spec.type;
      param->original_spec = spec.type;
   }
}

void read_param_ref( struct parse* parse, struct param* param ) {
   switch ( parse->lang ) {
      struct ref_reading ref;
   case LANG_BCS:
      init_ref_reading( &ref );
      read_ref( parse, &ref );
      param->ref = ref.head;
      break;
   default:
      break;
   }
}

void read_param_name( struct parse* parse, struct param* param ) {
   // In BCS, the name is optional.
   if ( ! ( parse->lang == LANG_BCS && parse->tk != TK_ID ) ) {
      if ( parse->tk != TK_ID ) {
         p_unexpect_diag( parse );
         p_unexpect_last_name( parse, NULL, "parameter name" );
         p_bail( parse );
      }
      param->name = t_extend_name( parse->ns->body, parse->tk_text );
      param->object.pos = parse->tk_pos;
      p_read_tk( parse );
   }
}

void read_param_default_value( struct parse* parse, struct params* params,
   struct param* param ) {
   switch ( parse->lang ) {
   case LANG_BCS:
      if ( parse->tk == TK_ASSIGN ) {
         p_read_tk( parse );
         struct expr_reading value;
         p_init_expr_reading( &value, false, true, false, true );
         p_read_expr( parse, &value );
         param->default_value = value.output_node;
      }
      break;
   default:
      break;
   }
}

void append_param( struct params* params, struct param* param ) {
   if ( params->tail ) {
      params->tail->next = param;
   }
   else {
      params->node = param;
   }
   params->tail = param;
   ++params->max;
   if ( ! param->default_value ) {
      ++params->min;
   }
}

void read_func_body( struct parse* parse, struct dec* dec,
   struct func* func ) {
   struct func_user* impl = t_alloc_func_user();
   impl->nested = ( dec->area == DEC_LOCAL );
   if ( ! ( dec->area == DEC_TOP || dec->static_qual ) ) {
      impl->local = true;
   }
   func->impl = impl;
   // Only read the function body when it is needed.
   if ( ! parse->lib->imported ) {
      p_read_func_body( parse, func );
   }
   else {
      p_skip_block( parse );
   }
}

void p_read_func_body( struct parse* parse, struct func* func ) {
   struct func_user* impl = func->impl;
   struct stmt_reading body;
   p_init_stmt_reading( &body, &impl->labels );
   // TODO: Remove `parse.local_vars` fields.
   struct list* prev_local_vars = parse->local_vars;
   parse->local_vars = &impl->vars;
   p_read_top_block( parse, &body, true );
   impl->body = body.block_node;
   parse->local_vars = prev_local_vars;
}

struct func_aspec* alloc_aspec_impl( void ) {
   struct func_aspec* impl = mem_slot_alloc( sizeof( *impl ) );
   impl->id = 0;
   impl->script_callable = false;
   return impl;
}

void p_read_script( struct parse* parse ) {
   struct script_reading reading;
   init_script_reading( &reading, &parse->tk_pos );
   p_test_tk( parse, TK_SCRIPT );
   p_read_tk( parse );
   read_script_number( parse, &reading );
   read_script_param_paren( parse, &reading );
   read_script_type( parse, &reading );
   if (
      parse->lang == LANG_ACS ||
      parse->lang == LANG_BCS ) {
      read_script_flag( parse, &reading );
   }
   read_script_after_flag( parse, &reading );
}

void init_script_reading( struct script_reading* reading, struct pos* pos ) {
   reading->pos = *pos;
   reading->number = NULL;
   reading->param = NULL;
   reading->param_tail = NULL;
   reading->num_param = 0;
   reading->type = SCRIPT_TYPE_CLOSED;
   reading->flags = 0;
   reading->param_specified = false;
}

void read_script_number( struct parse* parse,
   struct script_reading* reading ) {
   if ( ( parse->lang == LANG_ACS || parse->lang == LANG_BCS ) &&
      parse->tk == TK_SHIFT_L ) {
      p_read_tk( parse );
      // The token between the `<<` and `>>` tokens must be the digit `0`.
      if ( ! ( parse->tk == TK_LIT_DECIMAL && parse->tk_text[ 0 ] == '0' &&
         parse->tk_length == 1 ) ) {
         p_unexpect_diag( parse );
         p_unexpect_last_name( parse, NULL, "the digit `0`" );
         p_bail( parse );
      }
      reading->number = parse->task->raw0_expr;
      p_read_tk( parse );
      p_test_tk( parse, TK_SHIFT_R );
      p_read_tk( parse );
   }
   else if ( parse->lang == LANG_ACS && parse->tk == TK_LIT_STRING ) {
      struct indexed_string* string = t_intern_script_name( parse->task,
         parse->tk_text, parse->tk_length );
      struct indexed_string_usage* usage = t_alloc_indexed_string_usage();
      usage->string = string;
      struct expr* expr = t_alloc_expr();
      expr->pos = parse->tk_pos;
      expr->root = &usage->node;
      reading->number = expr;
      p_read_tk( parse );
   }
   else {
      // When reading the script number, the left parenthesis of the parameter
      // list can be mistaken for a function call. Don't read function calls.
      struct expr_reading number;
      p_init_expr_reading( &number, false, false, true, true );
      p_read_expr( parse, &number );
      reading->number = number.output_node;
   }
}

void read_script_param_paren( struct parse* parse,
   struct script_reading* reading ) {
   reading->param_pos = parse->tk_pos;
   if ( parse->tk == TK_PAREN_L ) {
      p_test_tk( parse, TK_PAREN_L );
      p_read_tk( parse );
      // In BCS, the parameter list is optional.
      if ( ! (
         parse->lang == LANG_BCS &&
         parse->tk == TK_PAREN_R ) ) {
         read_script_param_list( parse, reading );
      }
      p_test_tk( parse, TK_PAREN_R );
      p_read_tk( parse );
      reading->param_specified = true;
   }
}

void read_script_param_list( struct parse* parse,
   struct script_reading* reading ) {
   if ( parse->tk == TK_VOID ) {
      p_read_tk( parse );
   }
   else {
      while ( true ) {
         read_script_param( parse, reading );
         if ( parse->tk == TK_COMMA ) {
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
   }
}

void read_script_param( struct parse* parse, struct script_reading* reading ) {
   struct param* param = t_alloc_param();
   // Specifier.
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      switch ( parse->tk ) {
      case TK_INT:
         param->spec = SPEC_RAW;
         p_read_tk( parse );
         break;
      default:
         p_unexpect_diag( parse );
         if ( reading->param ) {
            p_unexpect_last( parse, &parse->tk_pos, TK_INT );
         }
         else {
            p_unexpect_item( parse, &parse->tk_pos, TK_INT );
            p_unexpect_last( parse, &parse->tk_pos, TK_VOID );
         }
         p_bail( parse );
      }
      break;
   default:
      // NOTE: TK_ID-based specifiers are not currently allowed. Maybe
      // implement support for that since a user can create an alias to an
      // `int` type via the `typedef` construct.
      switch ( parse->tk ) {
      case TK_RAW:
         param->spec = SPEC_RAW;
         p_read_tk( parse );
         break;
      case TK_INT:
         param->spec = SPEC_INT;
         p_read_tk( parse );
         break;
      default:
         p_unexpect_diag( parse );
         p_unexpect_item( parse, &parse->tk_pos, TK_INT );
         if ( reading->param ) {
            p_unexpect_last( parse, &parse->tk_pos, TK_RAW );
         }
         else {
            p_unexpect_item( parse, &parse->tk_pos, TK_RAW );
            p_unexpect_item( parse, &parse->tk_pos, TK_VOID );
            p_unexpect_last( parse, &parse->tk_pos, TK_PAREN_R );
         }
         p_bail( parse );
      }
   }
   // Name.
   // In BCS, like for functions, the parameter name is optional.
   if ( ! (
      parse->lang == LANG_BCS &&
      parse->tk != TK_ID ) ) {
      p_test_tk( parse, TK_ID );
      param->name = t_extend_name( parse->ns->body, parse->tk_text );
      param->object.pos = parse->tk_pos;
      p_read_tk( parse );
   }
   // Finish.
   if ( reading->param ) {
      reading->param_tail->next = param;
   }
   else {
      reading->param = param;
   }
   reading->param_tail = param;
   ++reading->num_param;
}

void read_script_type( struct parse* parse, struct script_reading* reading ) {
   enum tk tk = parse->tk;
   // In BCS, script types are context-sensitive keywords.
   if ( parse->lang == LANG_BCS && parse->tk == TK_ID ) {
      // The keywords are ordered based on potential usage, because a linear
      // search is used to find the keyword.
      static const struct {
         const char* text;
         enum tk tk;
      } table[] = {
         { "open", TK_OPEN },
         { "enter", TK_ENTER },
         { "respawn", TK_RESPAWN },
         { "disconnect", TK_DISCONNECT },
         { "death", TK_DEATH },
         { "unloading", TK_UNLOADING },
         { "event", TK_EVENT },
         { "kill", TK_KILL },
         { "lightning", TK_LIGHTNING },
         { "pickup", TK_PICKUP },
         { "bluereturn", TK_BLUE_RETURN },
         { "redreturn", TK_RED_RETURN },
         { "whitereturn", TK_WHITE_RETURN },
         { "reopen", TK_REOPEN },
      };
      int i = 0;
      while ( i < ARRAY_SIZE( table ) ) {
         if ( strcmp( parse->tk_text, table[ i ].text ) == 0 ) {
            tk = table[ i ].tk;
            break;
         }
         ++i;
      }
   }
   switch ( tk ) {
   case TK_OPEN: reading->type = SCRIPT_TYPE_OPEN; break;
   case TK_RESPAWN: reading->type = SCRIPT_TYPE_RESPAWN; break;
   case TK_DEATH: reading->type = SCRIPT_TYPE_DEATH; break;
   case TK_ENTER: reading->type = SCRIPT_TYPE_ENTER; break;
   case TK_PICKUP: reading->type = SCRIPT_TYPE_PICKUP; break;
   case TK_BLUE_RETURN: reading->type = SCRIPT_TYPE_BLUERETURN; break;
   case TK_RED_RETURN: reading->type = SCRIPT_TYPE_REDRETURN; break;
   case TK_WHITE_RETURN: reading->type = SCRIPT_TYPE_WHITERETURN; break;
   case TK_LIGHTNING: reading->type = SCRIPT_TYPE_LIGHTNING; break;
   case TK_DISCONNECT: reading->type = SCRIPT_TYPE_DISCONNECT; break;
   case TK_UNLOADING: reading->type = SCRIPT_TYPE_UNLOADING; break;
   case TK_RETURN: reading->type = SCRIPT_TYPE_RETURN; break;
   case TK_EVENT: reading->type = SCRIPT_TYPE_EVENT; break;
   case TK_KILL: reading->type = SCRIPT_TYPE_KILL; break;
   case TK_REOPEN: reading->type = SCRIPT_TYPE_REOPEN; break;
   default: break;
   }
   // Correct number of parameters need to be specified for a script type.
   if ( reading->type == SCRIPT_TYPE_CLOSED ) {
      if ( ( parse->lang == LANG_ACS || parse->lang == LANG_ACS95 ) &&
         ! reading->param_specified ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "script missing parameter list" );
         p_bail( parse );
      }
      if ( reading->num_param > parse->lang_limits->max_script_params ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "too many parameters in script" );
         p_diag( parse, DIAG_POS, &reading->param_pos,
            "a closed-script can have up to a maximum of %d parameters",
            parse->lang_limits->max_script_params );
         p_bail( parse );
      }
   }
   else if ( reading->type == SCRIPT_TYPE_DISCONNECT ) {
      // A disconnect script must have a single parameter. It is the number of
      // the player who exited the game.
      if ( reading->num_param < 1 ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "missing player-number parameter in disconnect-script" );
         p_bail( parse );
      }
      if ( reading->num_param > 1 ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "too many parameters in disconnect-script" );
         p_bail( parse );
 
      }
      p_read_tk( parse );
   }
   else if ( reading->type == SCRIPT_TYPE_EVENT ) {
      if ( reading->num_param != 3 ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "incorrect number of parameters in event-script" );
         p_diag( parse, DIAG_POS, &reading->param_pos,
            "an event-script must have exactly 3 parameters" );
         p_bail( parse );
      }
      p_read_tk( parse );
   }
   else {
      if ( parse->lang != LANG_BCS && reading->param_specified &&
         reading->num_param == 0 ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "parameter list specified for %s-script", parse->tk_text );
         p_bail( parse );
      }
      if ( reading->param_specified && reading->num_param != 0 ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "non-empty parameter list in %s-script", parse->tk_text );
         p_diag( parse, DIAG_POS, &reading->param_pos,
            "%s %s-script must have zero parameters",
            get_script_article( reading->type ), parse->tk_text );
         p_bail( parse );
      }
      p_read_tk( parse );
   }
}

const char* get_script_article( int type ) {
   STATIC_ASSERT( SCRIPT_TYPE_NEXTFREENUMBER == SCRIPT_TYPE_REOPEN + 1 );
   switch ( type ) {
   case SCRIPT_TYPE_OPEN:
   case SCRIPT_TYPE_ENTER:
   case SCRIPT_TYPE_UNLOADING:
   case SCRIPT_TYPE_EVENT:
      return "an";
   default:
      return "a";
   }
}

void read_script_flag( struct parse* parse, struct script_reading* reading ) {
   // In ACS, the flags must appear in a specific order.
   if ( parse->lang == LANG_ACS ) {
      if ( parse->tk == TK_NET ) {
         reading->flags |= SCRIPT_FLAG_NET;
         p_read_tk( parse );
      }
      if ( parse->tk == TK_CLIENTSIDE ) {
         reading->flags |= SCRIPT_FLAG_CLIENTSIDE;
         p_read_tk( parse );
      }
   }
   else {
      while ( parse->tk == TK_ID ) {
         int flag = SCRIPT_FLAG_NET;
         // In BCS, script flags are context-sensitive keywords.
         if ( strcmp( parse->tk_text, "net" ) != 0 ) {
            if ( strcmp( parse->tk_text, "clientside" ) == 0 ) {
               flag = SCRIPT_FLAG_CLIENTSIDE;
            }
            else {
               break;
            }
         }
         if ( ! ( reading->flags & flag ) ) {
            reading->flags |= flag;
            p_read_tk( parse );
         }
         else {
            p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
               "duplicate %s script-flag", parse->tk_text );
            p_bail( parse );
         }
      }
   }
}

void read_script_after_flag( struct parse* parse,
   struct script_reading* reading ) {
   struct script* script = add_script( parse, reading );
   // NOTE: A script can be of the form: script 1 open <statement>. In this
   // form, the body of the script is not a block, but we are skipping a block
   // in the following code. This will cause the next available block to be
   // skipped. This means a function could be skipped, because the next block
   // might be the body of a function. Warn about this form of a script.
   if ( parse->tk != TK_BRACE_L ) {
      p_diag( parse, DIAG_POS | DIAG_WARN, &reading->pos,
         "script missing braces around its body" );
   }
   if ( parse->lib->imported ) {
      p_skip_block( parse );
   }
   else {
      read_script_body( parse, reading, script );
   }
}

void read_script_body( struct parse* parse, struct script_reading* reading,
   struct script* script ) {
   struct stmt_reading body;
   p_init_stmt_reading( &body, &script->labels );
   parse->local_vars = &script->vars;
   p_read_top_block( parse, &body, false );
   script->body = body.block_node;
   parse->local_vars = NULL;
}

struct script* add_script( struct parse* parse,
   struct script_reading* reading ) {
   struct script* script = mem_alloc( sizeof( *script ) );
   script->node.type = NODE_SCRIPT;
   script->pos = reading->pos;
   script->number = reading->number;
   script->type = reading->type;
   script->flags = reading->flags;
   script->params = reading->param;
   script->body = NULL;
   script->nested_funcs = NULL;
   script->nested_calls = NULL;
   list_init( &script->labels );
   list_init( &script->vars );
   list_init( &script->funcscope_vars );
   script->assigned_number = 0;
   script->num_param = reading->num_param;
   script->offset = 0;
   script->size = 0;
   script->named_script = false;
   list_append( &parse->lib->scripts, script );
   list_append( &parse->lib->objects, script );
   list_append( &parse->ns_fragment->scripts, script );
   return script;
}

void p_read_special_list( struct parse* parse ) {
   p_test_tk( parse, TK_SPECIAL );
   p_read_tk( parse );
   if ( parse->lang == LANG_ACS && parse->lib->imported ) {
      p_skip_semicolon( parse );
   }
   else {
      while ( true ) {
         read_special( parse );
         if ( parse->tk == TK_COMMA ) {
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
      p_test_tk( parse, TK_SEMICOLON );
      p_read_tk( parse );
   }
}

void read_special( struct parse* parse ) {
   struct special_reading reading;
   init_special_reading( &reading );
   bool minus = false;
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_BCS:
      if ( parse->tk == TK_MINUS ) {
         p_read_tk( parse );
         minus = true;
      }
      break;
   default:
      break;
   }
   // Special-number/function-index.
   struct func* func = t_alloc_func();
   p_test_tk( parse, TK_LIT_DECIMAL );
   int id = p_extract_literal_value( parse );
   p_read_tk( parse );
   p_test_tk( parse, TK_COLON );
   p_read_tk( parse );
   // Name.
   p_test_tk( parse, TK_ID );
   func->object.pos = parse->tk_pos;
   func->name = t_extend_name( parse->ns->body, parse->tk_text );
   p_read_tk( parse );
   // Parameters.
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   read_special_param_dec( parse, &reading );
   func->min_param = reading.min_param;
   func->max_param = reading.max_param;
   func->params = reading.param;
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   // Return type.
   int return_spec = SPEC_RAW;
   switch ( parse->lang ) {
   case LANG_BCS:
      if ( parse->tk == TK_COLON ) {
         read_special_return_type( parse, &reading );
         return_spec = reading.return_spec;
      }
      break;
   default:
      break;
   }
   func->return_spec = return_spec;
   func->original_return_spec = return_spec;
   // Done.
   if ( minus ) {
      struct func_ext* impl = mem_alloc( sizeof( *impl ) );
      impl->id = id;
      func->type = FUNC_EXT;
      func->impl = impl;
   }
   else {
      struct func_aspec* impl = alloc_aspec_impl();
      impl->id = id;
      impl->script_callable = true;
      func->type = FUNC_ASPEC;
      func->impl = impl;
      if ( parse->tk == TK_COLON ) {
         p_read_tk( parse );
         p_test_tk( parse, TK_LIT_DECIMAL );
         impl->script_callable = ( p_extract_literal_value( parse ) != 0 );
         func->impl = impl;
         p_read_tk( parse );
      }
   }
   p_add_unresolved( parse, &func->object );
   list_append( &parse->ns_fragment->objects, func );
   list_append( &parse->lib->objects, func );
}

void init_special_reading( struct special_reading* reading ) { 
   reading->param = NULL;
   reading->param_tail = NULL;
   reading->return_spec = SPEC_NONE;
   reading->min_param = 0;
   reading->max_param = 0;
   reading->optional = false;
}

void read_special_param_dec( struct parse* parse,
   struct special_reading* reading ) {
   if ( parse->lang == LANG_ACS || parse->lang == LANG_ACS95 ||
      parse->tk == TK_LIT_DECIMAL ) {
      read_special_param_list_minmax( parse, reading );
   }
   else {
      if ( parse->tk != TK_PAREN_R ) {
         read_special_param_list( parse, reading );
      }
   }
}

void read_special_param_list_minmax( struct parse* parse,
   struct special_reading* reading ) {
   // Two formats:
   // 1. <maximum-parameters>
   // 2. <minimum-parameters> , <maximum-parameters>
   p_test_tk( parse, TK_LIT_DECIMAL );
   reading->max_param = p_extract_literal_value( parse );
   reading->min_param = reading->max_param;
   p_read_tk( parse );
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_BCS:
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
         p_test_tk( parse, TK_LIT_DECIMAL );
         reading->max_param = p_extract_literal_value( parse );
         p_read_tk( parse );
      }
      break;
   default:
      break;
   }
}

void read_special_param_list( struct parse* parse,
   struct special_reading* reading ) {
   // Required parameters.
   if ( parse->tk != TK_SEMICOLON ) {
      while ( true ) {
         read_special_param( parse, reading );
         if ( parse->tk == TK_COMMA ) {
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
   }
   // Optional parameters.
   if ( parse->tk == TK_SEMICOLON ) {
      reading->optional = true;
      p_read_tk( parse );
      while ( true ) {
         read_special_param( parse, reading );
         if ( parse->tk == TK_COMMA ) {
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
   }
}

void read_special_param( struct parse* parse,
   struct special_reading* reading ) {
   struct param* param = t_alloc_param();
   param->object.pos = parse->tk_pos;
   int spec = SPEC_RAW;
   switch ( parse->tk ) {
   case TK_RAW: break;
   case TK_INT: spec = SPEC_INT; break;
   case TK_FIXED: spec = SPEC_FIXED; break;
   case TK_BOOL: spec = SPEC_BOOL; break;
   case TK_STR: spec = SPEC_STR; break;
   default:
      p_unexpect_diag( parse );
      p_unexpect_last_name( parse, NULL, "parameter type" );
      p_bail( parse );
   }
   param->spec = spec;
   param->original_spec = spec;
   p_read_tk( parse );
   if ( reading->param ) {
      reading->param_tail->next = param;
   }
   else {
      reading->param = param;
   }
   reading->param_tail = param;
   if ( reading->optional ) {
      ++reading->max_param;
   }
   else {
      ++reading->min_param;
      ++reading->max_param;
   }
}

void read_special_return_type( struct parse* parse,
   struct special_reading* reading ) {
   p_test_tk( parse, TK_COLON );
   p_read_tk( parse );
   switch ( parse->tk ) {
   case TK_RAW: reading->return_spec = SPEC_RAW; break;
   case TK_INT: reading->return_spec = SPEC_INT; break;
   case TK_FIXED: reading->return_spec = SPEC_FIXED; break;
   case TK_BOOL: reading->return_spec = SPEC_BOOL; break;
   case TK_STR: reading->return_spec = SPEC_STR; break;
   case TK_VOID: reading->return_spec = SPEC_VOID; break;
   default:
      p_unexpect_diag( parse );
      p_unexpect_last_name( parse, NULL, "return type" );
      p_bail( parse );
   }
   p_read_tk( parse );
}