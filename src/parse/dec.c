#include <stdio.h>
#include <string.h>

#include "phase.h"

#define MAX_WORLD_LOCATIONS 256
#define MAX_GLOBAL_LOCATIONS 64
#define SCRIPT_MAX_PARAMS 3

struct params {
   struct param* node;
   struct pos format_pos;
   int min;
   int max;
   bool script;
   bool format;
};

struct multi_value_read {
   struct multi_value* multi_value;
   struct initial* tail;
};

struct script_reading {
   struct script* script;
   struct pos param_pos;
};

static void init_params( struct params* );
static void read_params( struct parse* parse, struct params* );
static void read_qual( struct parse* parse, struct dec* );
static void read_storage( struct parse* parse, struct dec* );
static void read_type( struct parse* parse, struct dec* dec );
static void missing_type( struct parse* parse, struct dec* dec );
static void read_enum( struct parse* parse, struct dec* );
static void read_enum_def( struct parse* parse, struct dec* dec );
static void read_rbrace( struct parse* parse, struct dec* dec );
static void read_manifest_constant( struct parse* parse, struct dec* dec );
static struct constant* alloc_constant( void );
static void read_struct( struct parse* parse, struct dec* );
static void read_struct_def( struct parse* parse, struct dec* dec );
static void read_objects( struct parse* parse, struct dec* dec );
static void read_storage_index( struct parse* parse, struct dec* );
static void read_func( struct parse* parse, struct dec* );
static void read_bfunc( struct parse* parse, struct func* );
static void check_useless( struct parse* parse, struct dec* dec );
static void read_script_number( struct parse* parse, struct script* );
static void read_script_params( struct parse* parse, struct script*,
   struct script_reading* );
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
static void test_struct_member( struct parse* parse, struct dec* dec );
static void add_struct_member( struct parse* parse, struct dec* );
static void add_var( struct parse* parse, struct dec* );
static void test_var( struct parse* parse, struct dec* dec );
static void test_storage( struct parse* parse, struct dec* dec );
static const char* get_storage_name( int type );

bool p_is_dec( struct parse* parse ) {
   switch ( parse->tk ) {
   case TK_INT:
   case TK_STR:
   case TK_BOOL:
   case TK_VOID:
   case TK_WORLD:
   case TK_GLOBAL:
   case TK_STATIC:
   case TK_ENUM:
   case TK_STRUCT:
   case TK_FUNCTION:
      return true;
   default:
      return false;
   }
}

void p_init_dec( struct dec* dec ) {
   dec->area = DEC_TOP;
   dec->type = NULL;
   dec->type_make = NULL;
   dec->type_path = NULL;
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
   dec->type_void = false;
   dec->type_struct = false;
   dec->static_qual = false;
   dec->leave = false;
   dec->read_func = false;
}

void p_read_dec( struct parse* parse, struct dec* dec ) {
   dec->pos = parse->tk_pos;
   if ( parse->tk == TK_FUNCTION ) {
      dec->read_func = true;
      p_read_tk( parse );
   }
   read_qual( parse, dec );
   read_storage( parse, dec );
   read_type( parse, dec );
   if ( ! dec->leave ) {
      read_objects( parse, dec );
   }
   else {
      check_useless( parse, dec );
   }
}

void read_qual( struct parse* parse, struct dec* dec ) {
   if ( parse->tk == TK_STATIC ) {
      dec->static_qual = true;
      dec->static_qual_pos = parse->tk_pos;
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

void read_type( struct parse* parse, struct dec* dec ) {
   dec->type_pos = parse->tk_pos;
   switch ( parse->tk ) {
   case TK_INT:
      dec->type = parse->task->type_int;
      p_read_tk( parse );
      break;
   case TK_STR:
      dec->type = parse->task->type_str;
      p_read_tk( parse );
      break;
   case TK_BOOL:
      dec->type = parse->task->type_bool;
      p_read_tk( parse );
      break;
   case TK_VOID:
      dec->type_void = true;
      p_read_tk( parse );
      break;
   case TK_ENUM:
      read_enum( parse, dec );
      break;
   case TK_STRUCT:
      read_struct( parse, dec );
      break;
   default:
      missing_type( parse, dec );
   }
}

void missing_type( struct parse* parse, struct dec* dec ) {
   const char* subject;
   if ( dec->read_func ) {
      subject = "function return-type";
   }
   else if ( dec->area == DEC_MEMBER ) {
      subject = "struct-member type";
   }
   else {
      subject = "object type";
   }
   p_unexpect_diag( parse );
   p_unexpect_last_name( parse, NULL, subject );
   p_bail( parse );
}

void read_enum( struct parse* parse, struct dec* dec ) {
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
   if ( parse->tk == TK_ID ) {
      p_unexpect_diag( parse );
      p_diag( parse, DIAG_POS, &parse->tk_pos,
         "naming an enum is not currently supported" );
      p_bail( parse );
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
   struct constant* head = NULL;
   struct constant* tail;
   while ( true ) {
      if ( parse->tk != TK_ID ) {
         p_unexpect_diag( parse );
         p_unexpect_name( parse, NULL, "an enumerator" );
         p_unexpect_last( parse, NULL, TK_BRACE_R );
         p_bail( parse );
      }
      struct constant* constant = alloc_constant();
      constant->object.pos = parse->tk_pos;
      constant->name = t_extend_name( parse->region->body, parse->tk_text );
      p_read_tk( parse );
      if ( parse->tk == TK_ASSIGN ) {
         p_read_tk( parse );
         struct expr_reading value;
         p_init_expr_reading( &value, true, false, false, true );
         p_read_expr( parse, &value );
         constant->value_node = value.output_node;
      }
      if ( head ) {
         tail->next = constant;
      }
      else {
         head = constant;
      }
      tail = constant;
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
   read_rbrace( parse, dec );
   struct constant_set* set = mem_alloc( sizeof( *set ) );
   t_init_object( &set->object, NODE_CONSTANT_SET );
   set->head = head;
   if ( dec->vars ) {
      list_append( dec->vars, set );
   }
   else {
      p_add_unresolved( parse->region, &set->object );
   }
   dec->type = parse->task->type_int;
   if ( parse->tk == TK_SEMICOLON ) {
      p_read_tk( parse );
      dec->leave = true;
   }
   if ( dec->area == DEC_MEMBER ) {
      p_diag( parse, DIAG_POS_ERR, &dec->type_pos,
         "enum inside struct" );
      p_bail( parse );
   }
}

void read_rbrace( struct parse* parse, struct dec* dec ) {
   struct pos rbrace_pos = parse->tk_pos;
   p_test_tk( parse, TK_BRACE_R );
   p_read_tk( parse );
   if (
      parse->tk != TK_SEMICOLON &&
      parse->tk != TK_ID &&
      parse->tk != TK_LIT_DECIMAL ) {
      p_unexpect_diag( parse );
      p_increment_pos( &rbrace_pos, TK_BRACE_R );
      p_unexpect_item( parse, &rbrace_pos, TK_SEMICOLON );
      p_unexpect_name( parse, NULL, "storage number" );
      p_unexpect_last_name( parse, NULL, "name of object" );
      p_bail( parse );
   }
}

void read_manifest_constant( struct parse* parse, struct dec* dec ) {
   p_test_tk( parse, TK_ID );
   struct constant* constant = alloc_constant();
   constant->object.pos = parse->tk_pos;
   constant->name = t_extend_name( parse->region->body, parse->tk_text );
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
      p_add_unresolved( parse->region, &constant->object );
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
   p_test_tk( parse, TK_STRUCT );
   p_read_tk( parse );
   dec->type_struct = true;
   if ( parse->tk == TK_BRACE_L ||
      ( parse->tk == TK_ID && p_peek( parse ) == TK_BRACE_L ) ) {
      read_struct_def( parse, dec );
   }
   else {
      dec->type_path = p_read_path( parse );
   }
}

void read_struct_def( struct parse* parse, struct dec* dec ) {
   struct name* name = NULL;
   struct pos name_pos = parse->tk_pos;
   bool name_specified = false;
   bool anon = false;
   if ( parse->tk == TK_ID ) {
      name = t_extend_name( parse->region->body_struct, parse->tk_text );
      name_specified = true;
      p_read_tk( parse );
   }
   // When no name is specified, make random name.
   else {
      name = t_create_name();
      anon = true;
   }
   struct type* type = t_create_type( parse->task, name );
   type->object.pos = dec->type_pos;
   type->anon = anon;
   // Members:
   p_test_tk( parse, TK_BRACE_L );
   p_read_tk( parse );
   if ( parse->tk == TK_BRACE_R ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "empty struct" );
      p_diag( parse, DIAG_POS, &parse->tk_pos,
         "a struct must have at least one member" );
      p_bail( parse );
   }
   while ( true ) {
      struct dec member;
      p_init_dec( &member );
      member.area = DEC_MEMBER;
      member.type_make = type;
      member.name_offset = type->body;
      member.vars = dec->vars;
      p_read_dec( parse, &member );
      if ( parse->tk == TK_BRACE_R ) {
         break;
      }
   }
   read_rbrace( parse, dec );
   if ( parse->tk == TK_SEMICOLON ) {
      p_read_tk( parse );
      dec->leave = true;
   }
   // Nested struct is in the same scope as the parent struct.
   if ( dec->vars ) {
      list_append( dec->vars, type );
   }
   else {
      p_add_unresolved( parse->region, &type->object );
   }
   dec->type = type;
   if ( dec->leave && type->anon ) {
      p_diag( parse, DIAG_POS_ERR, &dec->type_pos,
         "useless, unnamed struct" );
      p_diag( parse, DIAG_POS, &dec->type_pos,
         "an unnamed struct must be used as a part of an object" );
      p_bail( parse );
   }
   // NOTE: This comment is outdated. [04/06/2015]
   // Don't allow nesting of named structs for now. Maybe later, nested
   // struct support like in C++ will be added. Here is potential syntax
   // for specifying a nested struct, when creating a variable:
   // struct region.struct.my_struct var1;
   // struct upmost.struct.my_struct var2;
   if ( name_specified && dec->area == DEC_MEMBER ) {
      p_diag( parse, DIAG_POS_ERR, &dec->pos,
         "named, nested struct" );
      p_diag( parse, DIAG_POS, &dec->pos,
         "a nested struct must not have a name specified" );
      p_bail( parse );
   }
   STATIC_ASSERT( DEC_TOTAL == 4 );
   if ( dec->area == DEC_FOR ) {
      p_diag( parse, DIAG_POS_ERR, &dec->type_pos,
         "struct in for-loop initialization" );
      p_bail( parse );
   }
}

void read_objects( struct parse* parse, struct dec* dec ) {
   bool checked_func = false;
   while ( true ) {
      read_storage_index( parse, dec );
      read_name( parse, dec );
      if ( ! checked_func ) {
         if ( dec->read_func || parse->tk == TK_PAREN_L ) {
            read_func( parse, dec );
            return;
         }
         checked_func = false;
      }
      read_dim( parse, dec );
      read_init( parse, dec );
      if ( dec->area == DEC_MEMBER ) {
         test_struct_member( parse, dec );
         add_struct_member( parse, dec );
      }
      else {
         test_var( parse, dec );
         add_var( parse, dec );
      }
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

void test_struct_member( struct parse* parse, struct dec* dec ) {
   if ( dec->static_qual ) {
      p_diag( parse, DIAG_POS_ERR, &dec->static_qual_pos,
         "static struct-member" );
      p_bail( parse );
   }
   test_storage( parse, dec );
   if ( dec->type_void ) {
      p_diag( parse, DIAG_POS_ERR, &dec->type_pos,
         "struct-member of void type" );
      p_bail( parse );
   }
   if ( dec->initz.specified ) {
      p_diag( parse, DIAG_POS_ERR, &dec->initz.pos,
         "initializing a struct-member" );
      p_bail( parse );
   }
} 

void add_struct_member( struct parse* parse, struct dec* dec ) {
   struct type_member* member = mem_alloc( sizeof( *member ) );
   t_init_object( &member->object, NODE_TYPE_MEMBER );
   member->object.pos = dec->name_pos;
   member->name = dec->name;
   member->type = dec->type;
   member->type_path = dec->type_path;
   member->dim = dec->dim;
   member->next = NULL;
   member->offset = 0;
   member->size = 0;
   if ( dec->type_make->member ) {
      dec->type_make->member_tail->next = member;
   }
   else {
      dec->type_make->member = member;
   }
   dec->type_make->member_tail = member;
}

void test_var( struct parse* parse, struct dec* dec ) {
   if ( dec->static_qual &&
      ( dec->area == DEC_TOP || dec->area == DEC_FOR ) ) {
      p_diag( parse, DIAG_POS_ERR, &dec->static_qual_pos,
         "static variable" );
      p_diag( parse, DIAG_POS, &dec->static_qual_pos,
         "static variables are not allowed here" );
      p_bail( parse );
   }
   test_storage( parse, dec );
   // Variable must have a type.
   if ( dec->type_void ) {
      p_diag( parse, DIAG_POS_ERR, &dec->name_pos,
         "variable of void type" );
      p_bail( parse );
   }
   // Cannot place multi-value variable in scalar portion of storage, where
   // a slot can hold only a single value.
   // TODO: Come up with syntax to navigate the array portion of storage,
   // the struct being the map. This way, you can access the storage data
   // using a struct member instead of an array index.
   if ( dec->type_struct && ! dec->dim && (
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
   struct var* var = mem_alloc( sizeof( *var ) );
   t_init_object( &var->object, NODE_VAR );
   var->object.pos = dec->name_pos;
   var->name = dec->name;
   var->type = dec->type;
   var->type_path = dec->type_path;
   var->dim = dec->dim;
   var->initial = dec->initz.initial;
   var->value = NULL;
   var->next = NULL;
   var->storage = dec->storage.type;
   var->index = dec->storage_index.value;
   var->size = 0;
   var->initz_zero = false;
   var->hidden = false;
   var->used = false;
   var->initial_has_str = false;
   var->imported = parse->task->library->imported;
   var->is_constant_init =
      ( dec->static_qual || dec->area == DEC_TOP ) ? true : false;
   if ( dec->static_qual ) {
      var->hidden = true;
   }
   if ( dec->area == DEC_TOP ) {
      p_add_unresolved( parse->region, &var->object );
      list_append( &parse->task->library->vars, var );
   }
   else if ( dec->storage.type == STORAGE_MAP ) {
      list_append( &parse->task->library->vars, var );
      list_append( dec->vars, var );
   }
   else {
      list_append( dec->vars, var );
   }
}

void test_storage( struct parse* parse, struct dec* dec ) {
   if ( dec->area == DEC_MEMBER ) {
      if ( dec->storage.specified ) {
         p_diag( parse, DIAG_POS_ERR, &dec->storage.pos,
            "%s-storage specified for struct-member",
            get_storage_name( dec->storage.type ) );
         p_diag( parse, DIAG_POS, &dec->storage.pos,
            "storage of struct-members must not be explicitly specified",
            get_storage_name( dec->storage.type ) );
         p_bail( parse );
      }
      if ( dec->storage_index.specified ) {
         p_diag( parse, DIAG_POS_ERR, &dec->storage_index.pos,
            "storage-number specified for struct-member" );
         p_bail( parse );
      }
   }
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
   struct func* func = mem_slot_alloc( sizeof( *func ) );
   t_init_object( &func->object, NODE_FUNC );
   func->object.pos = dec->name_pos;
   func->type = FUNC_ASPEC;
   func->name = dec->name;
   func->params = NULL;
   func->return_type = dec->type;
   func->impl = NULL;
   func->min_param = 0;
   func->max_param = 0;
   func->hidden = false;
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
      impl->index = 0;
      impl->size = 0;
      impl->usage = 0;
      impl->obj_pos = 0;
      impl->return_pos = 0;
      impl->index_offset = 0;
      impl->nested = ( dec->area == DEC_LOCAL );
      impl->publish = false;
      func->impl = impl;
      // Only read the function body when it is needed.
      if ( ! parse->task->library->imported ) {
         struct stmt_reading body;
         p_init_stmt_reading( &body, &impl->labels );
         p_read_top_stmt( parse, &body, true );
         impl->body = body.block_node;
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
   if ( dec->static_qual ) {
      p_diag( parse, DIAG_POS_ERR, &dec->static_qual_pos,
         "static function" );
      p_diag( parse, DIAG_POS, &dec->static_qual_pos,
         "functions are not allowed to be static" );
      p_bail( parse );
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
   // At this time, returning a struct is not possible. Maybe later, this can
   // be added as part of variable assignment.
   if ( dec->type_struct ) {
      p_diag( parse, DIAG_POS_ERR, &dec->type_pos,
         "function return-type of struct type" );
      p_bail( parse );
   }
   if ( func->type == FUNC_FORMAT ) {
      if ( ! params.format ) {
         p_diag( parse, DIAG_POS_ERR, &params_pos,
            "parameter list missing format parameter" );
         p_bail( parse );
      }
   }
   else {
      if ( params.format ) {
         p_diag( parse, DIAG_POS_ERR, &params.format_pos,
            "format parameter outside format function" );
         p_bail( parse );
      }
   }
   if ( dec->area == DEC_TOP ) {
      p_add_unresolved( parse->region, &func->object );
      if ( func->type == FUNC_USER ) {
         list_append( &parse->task->library->funcs, func );
         list_append( &parse->region->items, func );
      }
   }
   else {
      list_append( dec->vars, func );
   }
}

void init_params( struct params* params ) {
   params->node = NULL;
   params->min = 0;
   params->max = 0;
   params->script = false;
   params->format = false;
} 

void read_params( struct parse* parse, struct params* params ) {
   if ( parse->tk == TK_VOID ) {
      p_read_tk( parse );
      return;
   }
   // First parameter of a function can be a format parameter.
   if ( parse->tk == TK_BRACE_L ) {
      params->format_pos = parse->tk_pos;
      params->format = true;
      ++params->min;
      ++params->max;
      p_read_tk( parse );
      p_test_tk( parse, TK_BRACE_R );
      p_read_tk( parse );
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         return;
      }
   }
   struct param* tail = NULL;
   while ( true ) {
      struct type* type = parse->task->type_int;
      if ( parse->tk == TK_STR ) {
         type = parse->task->type_str;
      }
      else if ( parse->tk == TK_BOOL ) {
         type = parse->task->type_bool;
      }
      else {
         p_test_tk( parse, TK_INT );
      }
      if ( params->script && type != parse->task->type_int ) {
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "script parameter not of `int` type" );
         p_bail( parse );
      }
      struct pos pos = parse->tk_pos;
      struct param* param = mem_slot_alloc( sizeof( *param ) );
      t_init_object( &param->object, NODE_PARAM );
      param->type = type;
      param->next = NULL;
      param->name = NULL;
      param->default_value = NULL;
      param->index = 0;
      param->obj_pos = 0;
      param->used = false;
      ++params->max;
      p_read_tk( parse );
      // Name not required for a parameter.
      if ( parse->tk == TK_ID ) {
         param->name = t_extend_name( parse->region->body, parse->tk_text );
         param->object.pos = parse->tk_pos;
         p_read_tk( parse );
      }
      if ( parse->tk == TK_ASSIGN ) {
         p_read_tk( parse );
         struct expr_reading value;
         p_init_expr_reading( &value, false, true, false, true );
         p_read_expr( parse, &value );
         param->default_value = value.output_node;
         if ( params->script ) {
            p_diag( parse, DIAG_POS_ERR, &pos,
               "default parameter in script" );
            p_bail( parse );
         }
      }
      else {
         if ( tail && tail->default_value ) {
            p_diag( parse, DIAG_POS_ERR, &pos,
               "parameter missing default value" );
            p_bail( parse );
         }
         ++params->min;
      }
      if ( tail ) {
         tail->next = param;
      }
      else {
         params->node = param;
      }
      tail = param;
      if ( parse->tk == TK_COMMA ) {
         p_read_tk( parse );
      }
      else {
         break;
      }
   }
   // Format parameter not allowed in a script parameter list.
   if ( params->script && params->format ) {
      p_diag( parse, DIAG_POS_ERR, &params->format_pos,
         "format parameter specified for script" );
      p_bail( parse );
   }
}

void read_bfunc( struct parse* parse, struct func* func ) {
   // Action special.
   if ( parse->tk == TK_ASSIGN ) {
      p_read_tk( parse );
      struct func_aspec* impl = mem_slot_alloc( sizeof( *impl ) );
      p_test_tk( parse, TK_LIT_DECIMAL );
      impl->id = p_extract_literal_value( parse );
      impl->script_callable = false;
      p_read_tk( parse );
      p_test_tk( parse, TK_COMMA );
      p_read_tk( parse );
      p_test_tk( parse, TK_LIT_DECIMAL );
      if ( p_extract_literal_value( parse ) ) {
         impl->script_callable = true;
      }
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

void check_useless( struct parse* parse, struct dec* dec ) {
   if ( dec->static_qual ) {
      p_diag( parse, DIAG_POS_ERR, &dec->static_qual_pos,
         "useless static-qualifier" );
      p_bail( parse );
   }
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
   script->num_param = 0;
   script->offset = 0;
   script->size = 0;
   script->publish = false;
   p_read_tk( parse );
   struct script_reading reading;
   read_script_number( parse, script );
   read_script_params( parse, script, &reading );
   read_script_type( parse, script, &reading );
   read_script_flag( parse, script );
   read_script_body( parse, script );
   list_append( &parse->task->library->scripts, script );
   list_append( &parse->region->items, script );
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

void read_script_params( struct parse* parse, struct script* script,
   struct script_reading* reading ) {
   reading->param_pos = parse->tk_pos;
   if ( parse->tk == TK_PAREN_L ) {
      p_read_tk( parse );
      if ( parse->tk == TK_PAREN_R ) {
         p_read_tk( parse );
      }
      else {
         struct params params;
         init_params( &params );
         params.script = true;
         read_params( parse, &params );
         script->params = params.node;
         script->num_param = params.max;
         p_test_tk( parse, TK_PAREN_R );
         p_read_tk( parse );
      }
   }
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
   p_read_top_stmt( parse, &body, false );
   script->body = body.node;
}