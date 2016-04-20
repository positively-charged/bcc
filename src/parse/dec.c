#include <stdio.h>
#include <string.h>

#include "phase.h"

#define MAX_WORLD_LOCATIONS 256
#define MAX_GLOBAL_LOCATIONS 64
#define SCRIPT_MAX_PARAMS 4

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
   bool done;
};

struct multi_value_read {
   struct multi_value* multi_value;
   struct initial* tail;
};

struct script_reading {
   struct script* script;
   struct param* param;
   struct param* param_tail;
   struct pos param_pos;
   int num_param;
};

static bool peek_dec_beginning_with_id( struct parse* parse );
static void read_enum( struct parse* parse, struct dec* );
static void read_enum_def( struct parse* parse, struct dec* dec );
static struct enumerator* alloc_enumerator( void );
static void read_manifest_constant( struct parse* parse, struct dec* dec );
static struct constant* alloc_constant( void );
static void read_struct( struct parse* parse, struct dec* dec );
static void read_struct_name( struct parse* parse,
   struct structure* structure );
static void read_struct_body( struct parse* parse, struct dec* dec,
   struct structure* structure );
void read_struct_member( struct parse* parse, struct dec* parent_dec,
   struct structure* structure );
static struct structure_member* create_struct_member( struct parse* parse,
   struct dec* dec );
static void append_struct_member( struct structure* structure,
   struct structure_member* member );
static void read_var( struct parse* parse, struct dec* dec );
static void read_qual( struct parse* parse, struct dec* );
static void read_storage( struct parse* parse, struct dec* );
static void init_spec_reading( struct spec_reading* spec, int area );
static void read_spec( struct parse* parse, struct spec_reading* spec );
static void read_name_spec( struct parse* parse, struct spec_reading* spec );
static void missing_spec( struct parse* parse, struct spec_reading* spec );
static void read_extended_spec( struct parse* parse, struct dec* dec );
static void init_ref( struct ref* ref, int type, struct pos* pos );
static void prepend_ref( struct ref_reading* reading, struct ref* part );
static void init_ref_reading( struct ref_reading* reading );
static void read_ref( struct parse* parse, struct ref_reading* reading );
static void read_struct_ref( struct parse* parse,
   struct ref_reading* reading );
static void read_ref_storage( struct parse* parse,
   struct ref_reading* reading );
static void read_array_ref( struct parse* parse, struct ref_reading* reading );
static void read_func_ref( struct parse* parse, struct ref_reading* reading );
static void read_instance_list( struct parse* parse, struct dec* dec );
static void read_instance( struct parse* parse, struct dec* dec );
static void read_storage_index( struct parse* parse, struct dec* );
static void read_func( struct parse* parse, struct dec* );
static struct func* alloc_func( void );
static void read_func_qual( struct parse* parse, struct dec* dec );
static void read_bfunc( struct parse* parse, struct func* );
static struct func_aspec* alloc_aspec_impl( void );
static void check_useless( struct parse* parse, struct dec* dec );
static void init_params( struct params* );
static void read_params( struct parse* parse, struct params* );
static void read_param( struct parse* parse, struct params* params );
static void read_script_number( struct parse* parse, struct script* );
static void read_script_param_paren( struct parse* parse,
   struct script* script, struct script_reading* reading );
static void read_script_param_list( struct parse* parse,
   struct script_reading* reading );
static void read_script_param( struct parse* parse,
   struct script_reading* reading );
static void read_script_type( struct parse* parse, struct script*,
   struct script_reading* );
static const char* get_script_article( int type );
static void read_script_flag( struct parse* parse, struct script* );
static void read_script_body( struct parse* parse, struct script* );
static void read_name( struct parse* parse, struct dec* );
static void missing_name( struct parse* parse, struct dec* dec );
static void read_dim( struct parse* parse, struct dec* );
static void read_init( struct parse* parse, struct dec* );
static void init_initial( struct initial*, bool );
static struct value* alloc_value( void );
static void read_multi_init( struct parse* parse, struct dec*,
   struct multi_value_read* );
static void add_type_alias( struct parse* parse, struct dec* dec );
static void read_auto_instance_list( struct parse* parse, struct dec* dec );
static void add_var( struct parse* parse, struct dec* );
static struct var* alloc_var( struct dec* dec );
static void test_var( struct parse* parse, struct dec* dec );
static void test_storage( struct parse* parse, struct dec* dec );
static const char* get_storage_name( int type );
static void read_foreach_var( struct parse* parse, struct dec* dec );
static void read_special( struct parse* parse );

bool p_is_dec( struct parse* parse ) {
   if ( parse->tk == TK_ID || parse->tk == TK_UPMOST ) {
      return peek_dec_beginning_with_id( parse );
   }
   else if ( parse->tk == TK_STATIC ) {
      // The `static` keyword is also used by static-assert.
      return ( p_peek( parse ) != TK_ASSERT );
   }
   else {
      switch ( parse->tk ) {
      case TK_RAW:
      case TK_INT:
      case TK_FIXED:
      case TK_BOOL:
      case TK_STR:
      case TK_VOID:
      case TK_WORLD:
      case TK_GLOBAL:
      case TK_ENUM:
      case TK_STRUCT:
      case TK_FUNCTION:
      case TK_REF:
      case TK_AUTO:
      case TK_TYPEDEF:
      case TK_PRIVATE:
      case TK_EXTSPEC:
         return true;
      default:
         return false;
      }
   }
}

bool peek_dec_beginning_with_id( struct parse* parse ) {
   // When an identifier is allowed to start a declaration, the situation
   // becomes ambiguous because an identifier can also start an expression.
   // To disambiguate the situation, we look ahead of the variable type.
   struct parsertk_iter iter;
   p_init_parsertk_iter( parse, &iter );
   p_next_tk( parse, &iter );
   if ( ! p_peek_path( parse, &iter ) ) {
      return false;
   }
   // The variable type should be followed by one of these:
   if (
      // - variable name
      iter.token->type == TK_ID ||
      // - storage index
      iter.token->type == TK_LIT_DECIMAL ||
      // - Structure reference. NOTE: In this context, the ampersand can also
      // indicate a bitwise-and operation. However, it doesn't look like such a
      // bitwise-and operation can be used in any meaningful way in real code,
      // so a declaration takes precedence.
      iter.token->type == TK_BIT_AND ||
      // - Function reference.
      iter.token->type == TK_FUNCTION
   ) {
      return true;
   }
   else if (
      // - Array reference.
      iter.token->type == TK_BRACKET_L ) {
      p_next_tk( parse, &iter );
      return ( iter.token->type == TK_BRACKET_R );
   }
   else {
      return false;
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
   dec->storage.type = STORAGE_LOCAL;
   dec->storage.specified = false;
   dec->storage_index.value = 0;
   dec->storage_index.specified = false;
   dec->initz.initial = NULL;
   dec->initz.specified = false;
   dec->initz.has_str = false;
   dec->spec = SPEC_NONE;
   dec->private_visibility = false;
   dec->static_qual = false;
   dec->typedef_qual = false;
   dec->leave = false;
   dec->read_func = false;
   dec->msgbuild = false;
   dec->extended_spec = false;
}

void p_read_dec( struct parse* parse, struct dec* dec ) {
   dec->pos = parse->tk_pos;
   if ( parse->tk == TK_STRUCT ) {
      read_struct( parse, dec );
   }
   else if ( parse->tk == TK_ENUM ) {
      read_enum( parse, dec );
   }
   else {
      // At this time, visibility can be specified only for variables and
      // functions.
      if ( parse->tk == TK_PRIVATE ) {
         dec->private_visibility = true;
         p_read_tk( parse );
      }
      if ( parse->tk == TK_FUNCTION ) {
         read_func( parse, dec );
      }
      else {
         read_var( parse, dec );
      }
   }
}

void read_enum( struct parse* parse, struct dec* dec ) {
   dec->type_pos = parse->tk_pos;
   p_test_tk( parse, TK_ENUM );
   p_read_tk( parse );
   if ( ( parse->tk == TK_ID && p_peek( parse ) == TK_BRACE_L ) ||
      parse->tk == TK_BRACE_L ) {
      read_enum_def( parse, dec );
   }
   else if ( parse->tk == TK_ID ) {
      read_manifest_constant( parse, dec );
   }
   else {
      p_unexpect_diag( parse );
      p_unexpect_item( parse, NULL, TK_BRACE_L );
      p_unexpect_last_name( parse, NULL, "name of constant" );
      p_bail( parse );
   }
   STATIC_ASSERT( DEC_TOTAL == 4 );
   if ( dec->area == DEC_FOR ) {
      p_diag( parse, DIAG_POS_ERR, &dec->type_pos,
         "enum in for-loop initialization" );
      p_bail( parse );
   }
}

void read_enum_def( struct parse* parse, struct dec* dec ) {
   struct name* name = NULL;
   struct name* name_offset = parse->ns->body;
   struct pos name_pos = parse->tk_pos;
   if ( parse->tk == TK_ID ) {
      name = t_extend_name( parse->ns->body, parse->tk_text );
      name_offset = t_extend_name( name, "." );
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_BRACE_L );
   p_read_tk( parse );
   if ( parse->tk == TK_BRACE_R ) {
      p_diag( parse, DIAG_POS_ERR, &dec->type_pos,
         "empty enum" );
      p_diag( parse, DIAG_POS, &dec->type_pos,
         "an enum must have at least one enumerator" );
      p_bail( parse );
   }
   struct enumerator* head = NULL;
   struct enumerator* tail;
   while ( true ) {
      if ( parse->tk != TK_ID ) {
         p_unexpect_diag( parse );
         p_unexpect_name( parse, NULL, "an enumerator" );
         p_unexpect_last( parse, NULL, TK_BRACE_R );
         p_bail( parse );
      }
      struct enumerator* enumerator = alloc_enumerator();
      enumerator->object.pos = parse->tk_pos;
      enumerator->name = t_extend_name( name_offset, parse->tk_text );
      p_read_tk( parse );
      if ( parse->tk == TK_ASSIGN ) {
         p_read_tk( parse );
         struct expr_reading value;
         p_init_expr_reading( &value, true, false, false, true );
         p_read_expr( parse, &value );
         enumerator->initz = value.output_node;
      }
      if ( head ) {
         tail->next = enumerator;
      }
      else {
         head = enumerator;
      }
      tail = enumerator;
      if ( parse->tk != TK_COMMA && parse->tk != TK_BRACE_R ) {
         p_unexpect_diag( parse );
         p_unexpect_item( parse, NULL, TK_COMMA );
         p_unexpect_last( parse, NULL, TK_BRACE_R );
         p_bail( parse );
      }
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
   struct enumeration* set = mem_alloc( sizeof( *set ) );
   t_init_object( &set->object, NODE_ENUMERATION );
   set->object.pos = name_pos;
   set->head = head;
   set->name = name;
   set->hidden = dec->private_visibility;
   // TODO: Clean up.
   struct enumerator* enumerator = set->head;
   while ( enumerator ) {
      enumerator->enumeration = set;
      enumerator = enumerator->next;
   }
   if ( dec->vars ) {
      list_append( dec->vars, set );
   }
   else {
      p_add_unresolved( parse, &set->object );
      list_append( &parse->ns->objects, set );
      list_append( &parse->lib->objects, set );
   }
   dec->spec = SPEC_ENUM;
   dec->enumeration = set;
/*
   if ( dec->area == DEC_MEMBER ) {
      p_diag( parse, DIAG_POS_ERR, &dec->type_pos,
         "enum inside struct" );
      p_bail( parse );
   }
*/
}

struct enumerator* alloc_enumerator( void ) {
   struct enumerator* enumerator = mem_alloc( sizeof( *enumerator ) );
   t_init_object( &enumerator->object, NODE_ENUMERATOR );
   enumerator->name = NULL;
   enumerator->next = NULL;
   enumerator->initz = NULL;
   enumerator->value = 0;
   return enumerator;
}

void read_manifest_constant( struct parse* parse, struct dec* dec ) {
   p_test_tk( parse, TK_ID );
   struct constant* constant = alloc_constant();
   constant->object.pos = parse->tk_pos;
   constant->name = t_extend_name( parse->ns->body, parse->tk_text );
   constant->hidden = dec->private_visibility;
   p_read_tk( parse );
   p_test_tk( parse, TK_ASSIGN );
   p_read_tk( parse );
   struct expr_reading value;
   p_init_expr_reading( &value, true, false, false, true );
   p_read_expr( parse, &value );
   constant->value_node = value.output_node;
   if ( dec->vars ) {
      list_append( dec->vars, constant );
   }
   else {
      p_add_unresolved( parse, &constant->object );
      list_append( &parse->ns->objects, constant );
      list_append( &parse->lib->objects, constant );
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   dec->leave = true;
}

struct constant* alloc_constant( void ) {
   struct constant* constant = mem_slot_alloc( sizeof( *constant ) );
   t_init_object( &constant->object, NODE_CONSTANT );
   constant->name = NULL;
   constant->next = NULL;
   constant->value = 0;
   constant->value_node = NULL;
   constant->hidden = false;
   constant->lib_id = 0;
   return constant;
}

void read_struct( struct parse* parse, struct dec* dec ) {
   struct structure* structure = t_alloc_structure();
   structure->object.pos = parse->tk_pos;
   p_test_tk( parse, TK_STRUCT );
   p_read_tk( parse );
   read_struct_name( parse, structure );
   read_struct_body( parse, dec, structure );
   dec->structure = structure;
   dec->spec = SPEC_STRUCT;
   // Nested struct is in the same scope as the parent struct.
   if ( dec->vars ) {
      list_append( dec->vars, structure );
   }
   else {
      p_add_unresolved( parse, &structure->object );
      list_append( &parse->ns->objects, structure );
      list_append( &parse->lib->objects, structure );
   }
   if ( structure->anon && ! dec->extended_spec ) {
      p_diag( parse, DIAG_POS_ERR, &structure->object.pos,
         "unnamed and unused struct" );
      p_diag( parse, DIAG_POS, &structure->object.pos,
         "a struct must have a name or be used as an object type" );
      p_bail( parse );
   }
   // NOTE: This comment is outdated. [04/06/2015]
   // Don't allow nesting of named structs for now. Maybe later, nested
   // struct support like in C++ will be added. Here is potential syntax
   // for specifying a nested struct, when creating a variable:
   // struct region.struct.my_struct var1;
   // struct upmost.struct.my_struct var2;
   if ( ! structure->anon && dec->area == DEC_MEMBER ) {
      p_diag( parse, DIAG_POS_ERR, &structure->object.pos,
         "named, nested struct" );
      p_diag( parse, DIAG_POS, &structure->object.pos,
         "a nested struct must not have a name specified" );
      p_bail( parse );
   }
}

void read_struct_name( struct parse* parse, struct structure* structure ) {
   if ( parse->tk == TK_ID ) {
      structure->name = t_extend_name( parse->ns->body, parse->tk_text );
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
         p_read_tk( parse );
         break;
      }
   }
}

void read_struct_member( struct parse* parse, struct dec* parent_dec,
   struct structure* structure ) {
   struct dec dec;
   p_init_dec( &dec );
   dec.area = DEC_MEMBER;
   dec.name_offset = structure->body;
   dec.vars = parent_dec->vars;
   read_extended_spec( parse, &dec );
   struct ref_reading ref;
   init_ref_reading( &ref );
   read_ref( parse, &ref );
   dec.ref = ref.head;
   // Instance list.
   while ( true ) {
      read_name( parse, &dec );
      read_dim( parse, &dec );
      struct structure_member* member = create_struct_member( parse, &dec );
      append_struct_member( structure, member );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
   if ( dec.ref ) {
      structure->has_ref_member = true;
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
}

struct structure_member* create_struct_member( struct parse* parse,
   struct dec* dec ) {
   struct structure_member* member = t_alloc_structure_member();
   member->object.pos = dec->name_pos;
   member->name = dec->name;
   member->ref = dec->ref;
   member->structure = dec->structure;
   member->enumeration = dec->enumeration;
   member->path = dec->path;
   member->dim = dec->dim;
   member->spec = dec->spec;
   return member;
}

void append_struct_member( struct structure* structure,
   struct structure_member* member ) {
   if ( structure->member ) {
      structure->member_tail->next = member;
   }
   else {
      structure->member = member;
   }
   structure->member_tail = member;
}

void read_var( struct parse* parse, struct dec* dec ) {
   read_qual( parse, dec );
   if ( parse->tk == TK_AUTO ) {
      dec->spec = SPEC_AUTO;
      p_read_tk( parse );
      read_auto_instance_list( parse, dec );
   }
   else {
      read_storage( parse, dec );
      read_extended_spec( parse, dec );
      struct ref_reading ref;
      init_ref_reading( &ref );
      read_ref( parse, &ref );
      dec->ref = ref.head;
      read_instance_list( parse, dec );
      // check_useless( parse, dec );
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
}

void read_qual( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_STATIC ) {
      dec->static_qual = true;
      dec->static_qual_pos = parse->tk_pos;
      p_read_tk( parse );
   }
   else if ( parse->tk == TK_TYPEDEF ) {
      dec->typedef_qual = true;
      p_read_tk( parse );
   }
}

void read_storage( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_WORLD || parse->tk == TK_GLOBAL ) {
      dec->storage.type = ( parse->tk == TK_WORLD ?
         STORAGE_WORLD : STORAGE_GLOBAL );
      dec->storage.pos = parse->tk_pos;
      dec->storage.specified = true;
      p_read_tk( parse );
   }
}

void init_spec_reading( struct spec_reading* spec, int area ) {
   spec->path = NULL;
   spec->type = SPEC_NONE;
   spec->area = area;
}

void read_spec( struct parse* parse, struct spec_reading* spec ) {
   spec->pos = parse->tk_pos;
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
   case TK_ID:
   case TK_UPMOST:
      read_name_spec( parse, spec );
      break;
   default:
      missing_spec( parse, spec );
      p_bail( parse );
   }
}

void read_name_spec( struct parse* parse, struct spec_reading* spec ) {
   spec->path = p_read_path( parse );
   spec->type = SPEC_NAME;
}

void missing_spec( struct parse* parse, struct spec_reading* spec ) {
   const char* subject;
   switch ( spec->area ) {
   case AREA_FUNCRETURN:
      subject = "function return-type";
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

void read_extended_spec( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_EXTSPEC ) {
      p_read_tk( parse );
      dec->extended_spec = true;
      if ( parse->tk == TK_STRUCT ) {
         read_struct( parse, dec );
      }
      else {
         read_enum( parse, dec );
      }
   }
   else {
      struct spec_reading spec;
      init_spec_reading( &spec, dec->read_func ?
         AREA_FUNCRETURN : dec->area == DEC_MEMBER ?
         AREA_MEMBER : AREA_VAR );
      read_spec( parse, &spec );
      dec->type_pos = spec.pos;
      dec->spec = spec.type;
      dec->path = spec.path;
   }
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
      read_struct_ref( parse, reading );
      break;
   default:
      break;
   }
   // Read array and function references.
   while ( parse->tk == TK_BRACKET_L || parse->tk == TK_FUNCTION ) {
      switch ( parse->tk ) {
      case TK_BRACKET_L:
         read_array_ref( parse, reading );
         break;
      case TK_FUNCTION:
         read_func_ref( parse, reading );
         break;
      default:
         UNREACHABLE()
      }
      if ( parse->tk == TK_BIT_AND ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
}

void init_ref( struct ref* ref, int type, struct pos* pos ) {
   ref->next = NULL;
   ref->type = type;
   ref->pos = *pos;
}

void prepend_ref( struct ref_reading* reading, struct ref* part ) {
   part->next = reading->head;
   reading->head = part;
}

void read_struct_ref( struct parse* parse, struct ref_reading* reading ) {
   read_ref_storage( parse, reading );
   p_test_tk( parse, TK_BIT_AND );
   struct ref_struct* part = mem_alloc( sizeof( *part ) );
   init_ref( &part->ref, REF_STRUCTURE, &parse->tk_pos );
   part->storage = reading->storage;
   part->storage_index = reading->storage_index;
   prepend_ref( reading, &part->ref );
   p_read_tk( parse );
}

void read_ref_storage( struct parse* parse, struct ref_reading* reading ) {
   // Storage.
   int storage = STORAGE_MAP;
   switch ( parse->tk ) {
   case TK_GLOBAL:
      storage = STORAGE_GLOBAL;
      p_read_tk( parse );
      break;
   case TK_WORLD:
      storage = STORAGE_WORLD;
      p_read_tk( parse );
      break;
   case TK_SCRIPT:
      storage = STORAGE_LOCAL;
      p_read_tk( parse );
      break;
   default:
      break;
   }
   // Storage index. (Optional)
   int storage_index = 0;
   if ( ( storage == STORAGE_GLOBAL || storage == STORAGE_WORLD ) &&
      parse->tk == TK_COLON ) {
      p_read_tk( parse );
      p_test_tk( parse, TK_LIT_DECIMAL );
      storage_index = p_extract_literal_value( parse );
      p_read_tk( parse );
   }
   reading->storage = storage;
   reading->storage_index = storage_index;
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
   read_ref_storage( parse, reading );
   struct ref_array* part = mem_alloc( sizeof( *part ) );
   init_ref( &part->ref, REF_ARRAY, &pos );
   part->dim_count = count;
   part->storage = reading->storage;
   part->storage_index = reading->storage_index;
   prepend_ref( reading, &part->ref );
}

void read_func_ref( struct parse* parse, struct ref_reading* reading ) {
   struct ref_func* part = mem_alloc( sizeof( *part ) );
   init_ref( &part->ref, REF_FUNCTION, &parse->tk_pos );
   part->params = NULL;
   part->min_param = 0;
   part->max_param = 0;
   part->msgbuild = false;
   p_test_tk( parse, TK_FUNCTION );
   p_read_tk( parse );
   // Read parameter list.
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   if ( parse->tk != TK_PAREN_R ) {
      struct params params;
      init_params( &params );
      read_params( parse, &params );
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

void read_instance_list( struct parse* parse, struct dec* dec ) {
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
   read_storage_index( parse, dec );
   read_name( parse, dec );
   read_dim( parse, dec );
   read_init( parse, dec );
   if ( dec->typedef_qual ) {
      add_type_alias( parse, dec );
   }
   else {
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
}

void read_name( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_ID ) {
      dec->name = t_extend_name( dec->name_offset, parse->tk_text );
      dec->name_pos = parse->tk_pos;
      p_read_tk( parse );
   }
   else {
      missing_name( parse, dec );
   }
}

void missing_name( struct parse* parse, struct dec* dec ) {
   const char* subject;
   if ( dec->read_func ) {
      subject = "function name";
   }
   else if ( dec->area == DEC_MEMBER ) {
      subject = "struct-member name";
   }
   else {
      subject = "object name";
   }
   p_unexpect_diag( parse );
   p_unexpect_last_name( parse, NULL, subject );
   p_bail( parse );
}

void read_dim( struct parse* parse, struct dec* dec ) {
   dec->dim = NULL;
   struct dim* tail = NULL;
   while ( parse->tk == TK_BRACKET_L ) {
      struct dim* dim = mem_alloc( sizeof( *dim ) );
      dim->next = NULL;
      dim->size_node = NULL;
      dim->size = 0;
      dim->element_size = 0;
      dim->pos = parse->tk_pos;
      p_read_tk( parse );
      if ( parse->tk == TK_BRACKET_R ) {
         p_read_tk( parse );
      }
      else {
         struct expr_reading size;
         p_init_expr_reading( &size, false, false, false, true );
         p_read_expr( parse, &size );
         dim->size_node = size.output_node;
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

void read_init( struct parse* parse, struct dec* dec ) {
   dec->initz.pos = parse->tk_pos;
   dec->initz.initial = NULL;
   if ( parse->tk == TK_ASSIGN ) {
      dec->initz.specified = true;
      p_read_tk( parse );
      if ( parse->tk == TK_BRACE_L ) {
         read_multi_init( parse, dec, NULL );
      }
      else {
         struct expr_reading expr;
         p_init_expr_reading( &expr, false, false, false, true );
         p_read_expr( parse, &expr );
         struct value* value = alloc_value();
         value->expr = expr.output_node;
         dec->initz.initial = &value->initial;
         dec->initz.has_str = expr.has_str;
      }
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
      p_diag( parse, DIAG_POS, &dec->type_pos,
         "a brace initializer must initialize at least one element" );
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
   value->next = NULL;
   value->index = 0;
   value->string_initz = false;
   return value;
}

void add_type_alias( struct parse* parse, struct dec* dec ) {
   if ( dec->initz.specified ) {
      p_diag( parse, DIAG_POS_ERR, &dec->initz.pos,
         "initializing a typedef" );
      p_bail( parse );
   }
   struct type_alias* alias = mem_alloc( sizeof( *alias ) );
   t_init_object( &alias->object, NODE_TYPE_ALIAS );
   alias->object.pos = dec->name_pos;
   alias->name = dec->name;
   alias->ref = dec->ref;
   alias->structure = dec->structure;
   alias->enumeration = dec->enumeration;
   alias->path = dec->path;
   alias->dim = dec->dim;
   alias->spec = dec->spec;
   if ( dec->area == DEC_TOP ) {
      p_add_unresolved( parse, &alias->object );
   }
   else {
      list_append( dec->vars, alias );
   }
}

void read_auto_instance_list( struct parse* parse, struct dec* dec ) {
   while ( true ) {
      read_name( parse, dec );
      read_init( parse, dec );
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

void test_var( struct parse* parse, struct dec* dec ) {
   if ( dec->private_visibility && dec->area != DEC_TOP ) {
      p_diag( parse, DIAG_POS_ERR, &dec->name_pos,
         "private non-namespace variable" );
      p_diag( parse, DIAG_POS, &dec->name_pos,
         "only variables outside script/function can be made private" );
      p_bail( parse );
   }
   if ( dec->static_qual && dec->area != DEC_LOCAL ) {
      p_diag( parse, DIAG_POS_ERR, &dec->name_pos,
         "static non-local variable" );
      p_diag( parse, DIAG_POS, &dec->name_pos,
         "only variables in script/function can be static-qualified" );
      p_bail( parse );
   }
   test_storage( parse, dec );
   // Variable must have a type.
   if ( dec->spec == SPEC_VOID ) {
      p_diag( parse, DIAG_POS_ERR, &dec->name_pos,
         "variable of void type" );
      p_bail( parse );
   }
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
   // At this time, there is no good way to initialize a variable having
   // world or global storage at runtime.
   if ( dec->initz.specified && ( dec->storage.type == STORAGE_WORLD ||
      dec->storage.type == STORAGE_GLOBAL ) && ( dec->area == DEC_TOP ||
      dec->static_qual ) ) {
      p_diag( parse, DIAG_POS_ERR, &dec->initz.pos,
         "initialization of %s-storage variable",
         get_storage_name( dec->storage.type ) );
      p_diag( parse, DIAG_POS, &dec->initz.pos,
         "%s-storage variables must not be initialized in this scope",
         get_storage_name( dec->storage.type ) );
      p_bail( parse );
   }
   // Initializer needs to be present when the size of the initial dimension
   // is implicit. Global and world arrays are an exception, unless they are
   // multi-dimensional.
   if ( ! dec->initz.specified ) {
      if ( dec->dim && ! dec->dim->size_node && ( (
         dec->storage.type != STORAGE_WORLD &&
         dec->storage.type != STORAGE_GLOBAL ) || dec->dim->next ) ) {
         p_diag( parse, DIAG_POS_ERR, &dec->initz.pos,
            "missing initialization of implicit dimension" );
         p_bail( parse );
      }
   }
}

void add_var( struct parse* parse, struct dec* dec ) {
   struct var* var = alloc_var( dec );
   var->imported = parse->lib->imported;
   if ( dec->area == DEC_TOP ) {
      var->hidden = dec->private_visibility;
      p_add_unresolved( parse, &var->object );
      list_append( &parse->lib->vars, var );
      list_append( &parse->lib->objects, var );
      list_append( &parse->ns->objects, var );
      if ( var->hidden ) {
         list_append( &parse->ns->private_objects, var );
      }
   }
   else if ( dec->storage.type == STORAGE_MAP ) {
      var->hidden = ( dec->static_qual == true );
      list_append( &parse->lib->vars, var );
      list_append( &parse->lib->objects, var );
      list_append( dec->vars, var );
   }
   else {
      list_append( dec->vars, var );
      list_append( parse->local_vars, var );
   }
}

struct var* alloc_var( struct dec* dec ) {
   struct var* var = mem_alloc( sizeof( *var ) );
   t_init_object( &var->object, NODE_VAR );
   var->object.pos = dec->name_pos;
   var->name = dec->name;
   var->ref = dec->ref;
   var->structure = dec->structure;
   var->enumeration = dec->enumeration;
   var->type_path = dec->path;
   var->dim = dec->dim;
   var->initial = dec->initz.initial;
   var->value = NULL;
   var->next = NULL;
   var->spec = dec->spec;
   var->storage = dec->storage.type;
   var->index = dec->storage_index.value;
   var->size = 0;
   var->diminfo_start = 0;
   var->initz_zero = false;
   var->hidden = false;
   var->used = false;
   var->initial_has_str = false;
   var->imported = false;
   var->is_constant_init =
      ( dec->static_qual || dec->area == DEC_TOP ) ? true : false;
   var->addr_taken = false;
   return var;
}

void test_storage( struct parse* parse, struct dec* dec ) {
   if ( ! dec->storage.specified ) {
      // Variable found at region scope, or a static local variable, has map
      // storage.
      if ( dec->area == DEC_TOP ||
         ( dec->area == DEC_LOCAL && dec->static_qual ) ) {
         dec->storage.type = STORAGE_MAP;
      }
      else {
         dec->storage.type = STORAGE_LOCAL;
      }
   }
   if ( dec->storage_index.specified ) {
      int max = MAX_WORLD_LOCATIONS;
      if ( dec->storage.type != STORAGE_WORLD ) {
         if ( dec->storage.type == STORAGE_GLOBAL ) {
            max = MAX_GLOBAL_LOCATIONS;
         }
         else  {
            p_diag( parse, DIAG_POS_ERR, &dec->storage_index.pos,
               "storage-number specified for %s-storage variable",
               get_storage_name( dec->storage.type ) );
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
            get_storage_name( dec->storage.type ) );
         p_bail( parse );
      }
   }
}

const char* get_storage_name( int type ) {
   switch ( type ) {
   case STORAGE_MAP: return "map";
   case STORAGE_WORLD: return "world";
   case STORAGE_GLOBAL: return "global";
   default: return "local";
   }
}

void read_func( struct parse* parse, struct dec* dec ) {
   p_test_tk( parse, TK_FUNCTION );
   p_read_tk( parse );
   read_func_qual( parse, dec );
   read_extended_spec( parse, dec );
   struct ref_reading ref;
   init_ref_reading( &ref );
   read_ref( parse, &ref );
   dec->ref = ref.head;
   read_name( parse, dec );
   struct func* func = mem_slot_alloc( sizeof( *func ) );
   t_init_object( &func->object, NODE_FUNC );
   func->object.pos = dec->name_pos;
   func->type = FUNC_ASPEC;
   func->ref = dec->ref;
   func->structure = dec->structure;
   func->enumeration = dec->enumeration;
   func->path = dec->path;
   func->name = dec->name;
   func->params = NULL;
   func->impl = NULL;
   func->return_spec = dec->spec;
   func->min_param = 0;
   func->max_param = 0;
   func->hidden = dec->private_visibility;
   func->msgbuild = dec->msgbuild;
   // Parameter list:
   p_test_tk( parse, TK_PAREN_L );
   struct pos params_pos = parse->tk_pos;
   p_read_tk( parse );
   struct params params;
   init_params( &params );
   if ( parse->tk != TK_PAREN_R ) {
      read_params( parse, &params );
      func->params = params.node;
      func->min_param = params.min;
      func->max_param = params.max;
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
   // Body:
   if ( parse->tk == TK_BRACE_L ) {
      func->type = FUNC_USER;
      struct func_user* impl = mem_alloc( sizeof( *impl ) );
      list_init( &impl->labels );
      impl->body = NULL;
      impl->next_nested = NULL;
      impl->nested_funcs = NULL;
      impl->nested_calls = NULL;
      impl->returns = NULL;
      impl->prologue_point = NULL;
      impl->return_table = NULL;
      list_init( &impl->vars );
      impl->index = 0;
      impl->size = 0;
      impl->usage = 0;
      impl->obj_pos = 0;
      impl->index_offset = 0;
      impl->nested = ( dec->area == DEC_LOCAL );
      func->impl = impl;
      // Only read the function body when it is needed.
      if ( ! parse->lib->imported ) {
         struct stmt_reading body;
         p_init_stmt_reading( &body, &impl->labels );
         parse->local_vars = &impl->vars;
         p_read_top_stmt( parse, &body, true );
         impl->body = body.block_node;
         parse->local_vars = NULL;
      }
      else {
         p_skip_block( parse );
      }
   }
   else {
      read_bfunc( parse, func );
      p_test_tk( parse, TK_SEMICOLON );
      p_read_tk( parse );
   }
   if ( dec->storage.specified ) {
      p_diag( parse, DIAG_POS_ERR, &dec->storage.pos,
         "%s-storage specified for function",
         get_storage_name( dec->storage.type ) );
      p_bail( parse );
   }
   if ( dec->storage_index.specified ) {
      p_diag( parse, DIAG_POS_ERR, &dec->storage_index.pos,
         "storage-number specified for function" );
      p_bail( parse );
   }
   if ( dec->area == DEC_TOP ) {
      p_add_unresolved( parse, &func->object );
      list_append( &parse->ns->objects, func );
      list_append( &parse->lib->objects, func );
      if ( func->type == FUNC_USER ) {
         list_append( &parse->lib->funcs, func );
         list_append( &parse->ns->funcs, func );
      }
   }
   else {
      list_append( dec->vars, func );
   }
}

struct func* alloc_func( void ) {
   struct func* func = mem_slot_alloc( sizeof( *func ) );
   t_init_object( &func->object, NODE_FUNC );
   func->type = FUNC_ASPEC;
   func->ref = NULL;
   func->structure = NULL;
   func->enumeration = NULL;
   func->name = NULL;
   func->params = NULL;
   func->impl = NULL;
   func->return_spec = SPEC_VOID;
   func->min_param = 0;
   func->max_param = 0;
   func->hidden = false;
   return func;
}

void read_func_qual( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_MSGBUILD ) {
      dec->msgbuild = true;
      p_read_tk( parse );
   }
}

void init_params( struct params* params ) {
   params->node = NULL;
   params->tail = NULL;
   params->min = 0;
   params->max = 0;
   params->done = false;
} 

void read_params( struct parse* parse, struct params* params ) {
   if ( parse->tk == TK_VOID && p_peek( parse ) == TK_PAREN_R ) {
      p_read_tk( parse );
   }
   else {
      while ( ! params->done ) {
         read_param( parse, params );
      }
   }
}

void read_param( struct parse* parse, struct params* params ) {
   struct pos pos = parse->tk_pos;
   struct param* param = t_alloc_param();
   param->object.pos = pos;
   // Specifier.
   struct spec_reading spec;
   init_spec_reading( &spec, AREA_VAR );
   read_spec( parse, &spec );
   param->path = spec.path;
   param->spec = spec.type;
   // Reference.
   struct ref_reading ref;
   init_ref_reading( &ref );
   read_ref( parse, &ref );
   param->ref = ref.head;
   // Name not required for a parameter.
   if ( parse->tk == TK_ID ) {
      param->name = t_extend_name( parse->ns->body, parse->tk_text );
      param->object.pos = parse->tk_pos;
      p_read_tk( parse );
   }
   // Default value.
   if ( parse->tk == TK_ASSIGN ) {
      p_read_tk( parse );
      struct expr_reading value;
      p_init_expr_reading( &value, false, true, false, true );
      p_read_expr( parse, &value );
      param->default_value = value.output_node;
   }
   else {
      if ( params->tail && params->tail->default_value ) {
         p_diag( parse, DIAG_POS_ERR, &pos,
            "parameter missing default value" );
         p_bail( parse );
      }
      ++params->min;
   }
   if ( params->tail ) {
      params->tail->next = param;
   }
   else {
      params->node = param;
   }
   params->tail = param;
   ++params->max;
   if ( parse->tk == TK_COMMA ) {
      p_read_tk( parse );
   }
   else {
      params->done = true;
   }
}

void read_bfunc( struct parse* parse, struct func* func ) {
   // Action special.
   if ( parse->tk == TK_ASSIGN ) {
      p_read_tk( parse );
      struct func_aspec* impl = alloc_aspec_impl();
      p_test_tk( parse, TK_LIT_DECIMAL );
      impl->id = p_extract_literal_value( parse );
      p_read_tk( parse );
      p_test_tk( parse, TK_COMMA );
      p_read_tk( parse );
      p_test_tk( parse, TK_LIT_DECIMAL );
      impl->script_callable = ( p_extract_literal_value( parse ) != 0 );
      func->impl = impl;
      p_read_tk( parse );
   }
   // Extension function.
   else if ( parse->tk == TK_ASSIGN_SUB ) {
      p_read_tk( parse );
      func->type = FUNC_EXT;
      struct func_ext* impl = mem_alloc( sizeof( *impl ) );
      p_test_tk( parse, TK_LIT_DECIMAL );
      impl->id = p_extract_literal_value( parse );
      func->impl = impl;
      p_read_tk( parse );
   }
   // Dedicated function.
   else if ( parse->tk == TK_ASSIGN_ADD ) {
      p_read_tk( parse );
      func->type = FUNC_DED;
      struct func_ded* impl = mem_alloc( sizeof( *impl ) );
      p_test_tk( parse, TK_LIT_DECIMAL );
      impl->opcode = p_extract_literal_value( parse );
      p_read_tk( parse );
      p_test_tk( parse, TK_COMMA );
      p_read_tk( parse );
      p_test_tk( parse, TK_LIT_DECIMAL );
      impl->latent = false;
      if ( parse->tk_text[ 0 ] != '0' ) {
         impl->latent = true;
      }
      p_read_tk( parse );
      func->impl = impl;
   }
   // Format function.
   else if ( parse->tk == TK_ASSIGN_MUL ) {
      p_read_tk( parse );
      func->type = FUNC_FORMAT;
      struct func_format* impl = mem_alloc( sizeof( *impl ) );
      p_test_tk( parse, TK_LIT_DECIMAL );
      impl->opcode = p_extract_literal_value( parse );
      func->impl = impl;
      p_read_tk( parse );
   }
   // Internal function.
   else {
      p_test_tk( parse, TK_ASSIGN_DIV );
      p_read_tk( parse );
      func->type = FUNC_INTERNAL;
      struct func_intern* impl = mem_alloc( sizeof( *impl ) );
      p_test_tk( parse, TK_LIT_DECIMAL );
      struct pos pos = parse->tk_pos;
      impl->id = p_extract_literal_value( parse );
      p_read_tk( parse );
      if ( impl->id >= INTERN_FUNC_STANDALONE_TOTAL ) {
         p_diag( parse, DIAG_POS_ERR, &pos,
            "no internal function with ID of %d", impl->id );
         p_bail( parse );
      }
      func->impl = impl;
   }
}

struct func_aspec* alloc_aspec_impl( void ) {
   struct func_aspec* impl = mem_slot_alloc( sizeof( *impl ) );
   impl->id = 0;
   impl->script_callable = false;
   return impl;
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
   }
   else if ( parse->tk == TK_SEMICOLON ) {
      p_read_tk( parse );
      stmt->key = stmt->value;
      p_init_dec( &dec );
      dec.name_offset = parse->ns->body;
      read_foreach_var( parse, &dec );
      stmt->value = alloc_var( &dec );
   }
}

void read_foreach_var( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_AUTO ) {
      dec->spec = SPEC_AUTO;
      p_read_tk( parse );
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

void check_useless( struct parse* parse, struct dec* dec ) {
   if ( dec->storage.specified ) {
      p_diag( parse, DIAG_POS_ERR, &dec->storage.pos,
         "useless %s-storage specifier",
         get_storage_name( dec->storage.type ) );
      p_bail( parse );
   }
}

void p_read_script( struct parse* parse ) {
   p_test_tk( parse, TK_SCRIPT );
   struct script* script = mem_alloc( sizeof( *script ) );
   script->node.type = NODE_SCRIPT;
   script->pos = parse->tk_pos;
   script->number = NULL;
   script->type = SCRIPT_TYPE_CLOSED;
   script->flags = 0;
   script->params = NULL;
   script->body = NULL;
   script->nested_funcs = NULL;
   script->nested_calls = NULL;
   list_init( &script->labels );
   list_init( &script->vars );
   script->assigned_number = 0;
   script->num_param = 0;
   script->offset = 0;
   script->size = 0;
   script->named_script = false;
   p_read_tk( parse );
   struct script_reading reading;
   reading.param = NULL;
   reading.param_tail = NULL;
   reading.num_param = 0;
   read_script_number( parse, script );
   read_script_param_paren( parse, script, &reading );
   read_script_type( parse, script, &reading );
   read_script_flag( parse, script );
   read_script_body( parse, script );
   list_append( &parse->lib->scripts, script );
   list_append( &parse->ns->scripts, script );
}

void read_script_number( struct parse* parse, struct script* script ) {
   if ( parse->tk == TK_SHIFT_L ) {
      p_read_tk( parse );
      // The token between the `<<` and `>>` tokens must be the digit `0`.
      if ( parse->tk == TK_LIT_DECIMAL && parse->tk_text[ 0 ] == '0' &&
         parse->tk_length == 1 ) {
         p_read_tk( parse );
         p_test_tk( parse, TK_SHIFT_R );
         p_read_tk( parse );
      }
      else {
         p_unexpect_diag( parse );
         p_unexpect_last_name( parse, NULL, "the digit `0`" );
         p_bail( parse );
      }
   }
   else {
      // When reading the script number, the left parenthesis of the parameter
      // list can be mistaken for a function call. Don't read function calls.
      struct expr_reading number;
      p_init_expr_reading( &number, false, false, true, true );
      p_read_expr( parse, &number );
      script->number = number.output_node;
   }
}

void read_script_param_paren( struct parse* parse, struct script* script,
   struct script_reading* reading ) {
   reading->param_pos = parse->tk_pos;
   if ( parse->tk == TK_PAREN_L ) {
      p_read_tk( parse );
      if ( parse->tk != TK_PAREN_R ) {
         read_script_param_list( parse, reading );
         script->params = reading->param;
         script->num_param = reading->num_param;
      }
      p_test_tk( parse, TK_PAREN_R );
      p_read_tk( parse );
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
   // NOTE: TK_ID-based specifiers are not currently allowed. Maybe implement
   // support for that since a user can create an alias to an `int` type via
   // the `typedef` construct.
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
      p_unexpect_item( parse, &parse->tk_pos, TK_RAW );
      p_unexpect_last( parse, &parse->tk_pos, TK_PAREN_R );
      p_bail( parse );
   }
   // Name.
   // Like for functions, the parameter name is optional.
   if ( parse->tk == TK_ID ) {
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

void read_script_type( struct parse* parse, struct script* script,
   struct script_reading* reading ) {
   switch ( parse->tk ) {
   case TK_OPEN: script->type = SCRIPT_TYPE_OPEN; break;
   case TK_RESPAWN: script->type = SCRIPT_TYPE_RESPAWN; break;
   case TK_DEATH: script->type = SCRIPT_TYPE_DEATH; break;
   case TK_ENTER: script->type = SCRIPT_TYPE_ENTER; break;
   case TK_PICKUP: script->type = SCRIPT_TYPE_PICKUP; break;
   case TK_BLUE_RETURN: script->type = SCRIPT_TYPE_BLUERETURN; break;
   case TK_RED_RETURN: script->type = SCRIPT_TYPE_REDRETURN; break;
   case TK_WHITE_RETURN: script->type = SCRIPT_TYPE_WHITERETURN; break;
   case TK_LIGHTNING: script->type = SCRIPT_TYPE_LIGHTNING; break;
   case TK_DISCONNECT: script->type = SCRIPT_TYPE_DISCONNECT; break;
   case TK_UNLOADING: script->type = SCRIPT_TYPE_UNLOADING; break;
   case TK_RETURN: script->type = SCRIPT_TYPE_RETURN; break;
   case TK_EVENT: script->type = SCRIPT_TYPE_EVENT; break;
   default: break;
   }
   // Correct number of parameters need to be specified for a script type.
   if ( script->type == SCRIPT_TYPE_CLOSED ) {
      if ( script->num_param > SCRIPT_MAX_PARAMS ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "too many parameters in script" );
         p_diag( parse, DIAG_POS, &reading->param_pos,
            "a closed-script can have up to a maximum of %d parameters",
            SCRIPT_MAX_PARAMS );
         p_bail( parse );
      }
   }
   else if ( script->type == SCRIPT_TYPE_DISCONNECT ) {
      // A disconnect script must have a single parameter. It is the number of
      // the player who exited the game.
      if ( script->num_param < 1 ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "missing player-number parameter in disconnect-script" );
         p_bail( parse );
      }
      if ( script->num_param > 1 ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "too many parameters in disconnect-script" );
         p_bail( parse );
 
      }
      p_read_tk( parse );
   }
   else if ( script->type == SCRIPT_TYPE_EVENT ) {
      if ( script->num_param != 3 ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "incorrect number of parameters in event-script" );
         p_diag( parse, DIAG_POS, &reading->param_pos,
            "an event-script must have exactly 3 parameters" );
         p_bail( parse );
      }
      p_read_tk( parse );
   }
   else {
      if ( script->num_param ) {
         p_diag( parse, DIAG_POS_ERR, &reading->param_pos,
            "non-empty parameter-list in %s-script", parse->tk_text );
         p_diag( parse, DIAG_POS, &reading->param_pos,
            "%s %s-script must have zero parameters",
            get_script_article( script->type ), parse->tk_text );
         p_bail( parse );
      }
      p_read_tk( parse );
   }
}

const char* get_script_article( int type ) {
   STATIC_ASSERT( SCRIPT_TYPE_NEXTFREENUMBER == SCRIPT_TYPE_EVENT + 1 );
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

void read_script_flag( struct parse* parse, struct script* script ) {
   while ( true ) {
      int flag = SCRIPT_FLAG_NET;
      if ( parse->tk != TK_NET ) {
         if ( parse->tk == TK_CLIENTSIDE ) {
            flag = SCRIPT_FLAG_CLIENTSIDE;
         }
         else {
            break;
         }
      }
      if ( ! ( script->flags & flag ) ) {
         script->flags |= flag;
         p_read_tk( parse );
      }
      else {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "duplicate %s script-flag", parse->tk_text );
         p_bail( parse );
      }
   }
}

void read_script_body( struct parse* parse, struct script* script ) {
   struct stmt_reading body;
   p_init_stmt_reading( &body, &script->labels );
   parse->local_vars = &script->vars;
   p_read_top_stmt( parse, &body, false );
   script->body = body.node;
   parse->local_vars = NULL;
}

void p_read_special_list( struct parse* parse ) {
   p_test_tk( parse, TK_SPECIAL );
   p_read_tk( parse );
   if ( parse->lib->imported ) {
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
   bool minus = false;
   if ( parse->tk == TK_MINUS ) {
      p_read_tk( parse );
      minus = true;
   }
   // Special-number/function-index.
   struct func* func = alloc_func();
   func->return_spec = SPEC_INT;
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
   p_test_tk( parse, TK_PAREN_L );
   p_read_tk( parse );
   // Parameter count, in two formats:
   // 1. Maximum parameters
   // 2. Minimum parameters , maximum-parameters
   p_test_tk( parse, TK_LIT_DECIMAL );
   func->max_param = p_extract_literal_value( parse );
   func->min_param = func->max_param;
   p_read_tk( parse );
   if ( parse->tk == TK_COMMA ) {
      p_read_tk( parse );
      p_test_tk( parse, TK_LIT_DECIMAL );
      func->max_param = p_extract_literal_value( parse );
      p_read_tk( parse );
   }
   p_test_tk( parse, TK_PAREN_R );
   p_read_tk( parse );
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
   }
   p_add_unresolved( parse, &func->object );
   list_append( &parse->ns->objects, func );
   list_append( &parse->lib->objects, func );
}