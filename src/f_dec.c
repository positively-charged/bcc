#include "f_main.h"
#include "mem.h"
#include "detail.h"

#define SCRIPT_MIN_NUM 0
#define SCRIPT_MAX_NUM 999
#define SCRIPT_MAX_PARAMS 3
#define MAX_MODULE_LOCATIONS 128
#define MAX_WORLD_LOCATIONS 256
#define MAX_GLOBAL_LOCATIONS 64

typedef struct {
   type_t* type;
   nkey_t* name;
   dimension_t* dim_head;
   dimension_t* dim_last;
   dimension_t* dim_implicit;
   initial_t* initial;
   initial_t* initial_tail;
   position_t pos;
   position_t storage_pos;
   position_t storage_index_pos;
   position_t name_pos;
   int array_index;
   int array_jump;
   nval_t* func_def;
   int area;
   int storage;
   int storage_index;
   int index;
   position_t index_pos;
   bool initial_not_zero;
   bool initial_has_string;
   bool is_static;
   bool is_format_param;
} dec_t;

typedef struct {
   position_t pos;
   expr_t* number;
   int type;
   int flags;
} p_script_t;

typedef struct {
   dimension_t* dim;
   dimension_t* dim_prev;
   position_t pos;
   int num_initials;
} p_initz_t;

static void read_dec( front_t*, dec_t* );
static void read_storage( front_t*, dec_t* );
static void read_storage_index( front_t*, dec_t* );
static void read_type( front_t*, dec_t* );
static void read_name( front_t*, dec_t* );
static void read_dim( front_t*, dec_t* );
static void finish_var( front_t*, dec_t* );
static void show_prev_def( front_t*, nkey_t* );
static void read_dec_init( front_t*, dec_t* );
static void read_dec_initz( front_t*, dec_t*, p_initz_t* );
static void read_param_list( front_t*, p_params_t* );
static void calc_array_element_size( dimension_t*, type_t* );
static void read_init( front_t*, dec_t* );
static void add_initial( dec_t*, node_t* );
static void read_initz( front_t*, dec_t*, p_initz_t* );
static void read_initz_value( front_t*, dec_t*, p_initz_t* );
static void finish_param( front_t*, p_params_t*, dec_t* );
static var_t* new_var( front_t*, dec_t* );
static void init_params( p_params_t*, int, position_t* );
static void read_func( front_t*, dec_t* );
static void read_bfunc( front_t*, dec_t*, p_params_t* );
static void add_dimension( front_t*, dec_t*, p_expr_t*, position_t* );
static void p_dec_add_param( front_t*, dec_t*, dec_t* param );
static void p_add_global_var( front_t*, dec_t* );
static void read_ufunc( front_t*, dec_t*, p_params_t* );
static void add_module_var( front_t*, dec_t* );
static void read_script_number( front_t*, p_script_t* );
static script_t* find_script( module_t*, int );
static void read_script_type( front_t*, p_script_t*, p_params_t* );
static void read_script_flag( front_t*, p_script_t* );
static void script_add( front_t*, p_script_t*, p_params_t*, p_block_t* );
static void alloc_local_index( front_t*, dec_t* );
static void alloc_local_offset( front_t*, dec_t* );
static void var_initz_must_have_space_left( front_t*, dec_t*, p_initz_t*,
   position_t* );

nkey_t* p_get_unique_name( front_t* front ) {
   nkey_t* key = ntbl_goto( front->name_table,
      "!s", front->token.source, "!" );
   if ( key->value && key->value->depth == front->depth ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN,
         &front->token.pos, "name '%s' already used",
         front->token.source );
      node_t* node = key->value->node;
      position_t* pos = NULL;
      switch ( node->type ) {
      case k_node_constant: ;
         constant_t* constant = ( constant_t* ) node;
         pos = &constant->pos;
         break;
      case k_node_var: ;
         var_t* var = ( var_t* ) node;
         pos = &var->pos;
         break;
      case k_node_func: ;
         func_t* func = ( func_t* ) node;
         pos = &func->pos;
         break;
      default:
         break;
      }
      if ( pos ) {
         f_diag( front, DIAG_NONE | DIAG_LINE | DIAG_COLUMN, pos,
            "name previously used here" );
      }
      f_bail( front );
   }
   return key;
}

void alloc_local_offset( front_t* front, dec_t* dec ) {
   dec->index = front->scope_size;
   front->scope->size += ( dec->dim_head ) ?
      dec->dim_head->size * dec->dim_head->element_size :
      dec->type->size;
   front->scope_size += ( dec->dim_head ) ?
      dec->dim_head->size * dec->dim_head->element_size :
      dec->type->size;
   if ( front->scope_size > front->scope_size_high ) {
      front->scope_size_high = front->scope_size;
   }
}

void alloc_local_index( front_t* front, dec_t* dec ) {
   dec->index = front->scope->size;
   ++front->scope->size;
}

bool p_is_dec( front_t* front ) {
   if ( tk( front ) == tk_id ) {
      nkey_t* key = ntbl_goto( front->name_table,
         "!s", front->token.source, "!" );
      return ( key->value && key->value->node->type == k_node_type );
   }
   else {
      switch ( tk( front ) ) {
      case tk_int:
      case tk_str:
      case tk_bool:
      case tk_void:
      case tk_function:
      case tk_world:
      case tk_global:
      case tk_static:
         return true;
      default:
         return false;
      }
   }
}

void p_read_dec( front_t* front, int area ) {
   dec_t dec = {
      .pos = front->token.pos,
      .storage = k_storage_none,
      .area = area };
   read_dec( front, &dec );
}

void read_dec( front_t* front, dec_t* dec ) {
   bool is_func = false;
   if ( tk( front ) == tk_function ) {
      is_func = true;
      drop( front );
   }
   read_storage( front, dec );
   read_type( front, dec );
   bool is_var = false;
   while ( true ) {
      read_storage_index( front, dec );
      read_name( front, dec );
      // Function.
      if ( ! is_var ) {
         if ( is_func || tk( front ) == tk_paran_l ) {
            read_func( front, dec );
            break;
         }
         is_var = true;
      }
      read_dim( front, dec );
      read_init( front, dec );
      if ( dec->area == k_dec_param ) {
         finish_param( front, front->params, dec );
         break;
      }
      else {
         finish_var( front, dec );
         if ( tk( front ) == tk_comma ) {
            drop( front );
         }
         else {
            skip( front, tk_semicolon );
            break;
         }
      }
   }
}

void read_storage( front_t* front, dec_t* dec ) {
   if ( tk( front ) == tk_global ) {
      dec->storage = k_storage_global;
      dec->storage_pos = front->token.pos;
      drop( front );
   }
   else if ( tk( front ) == tk_world ) {
      dec->storage = k_storage_world;
      dec->storage_pos = front->token.pos;
      drop( front );
   }
   else if ( tk( front ) == tk_static ) {
      dec->storage = k_storage_module;
      dec->storage_pos = front->token.pos;
      if ( dec->area == k_dec_for ) {
         f_diag( front,
            DIAG_ERR | DIAG_LINE | DIAG_COLUMN,
            &front->token.pos,
            "static variable in for loop initialization" );
      }
      else if ( ! front->depth ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN,
            &front->token.pos, "'static' used in top scope" );
      }
      else if ( dec->area == k_dec_param ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN,
            &front->token.pos, "'static' used in parameter" );
         f_bail( front );
      }
      drop( front );
   }
   else {
      if ( ! front->depth ) {
         dec->storage = k_storage_module;
      }
      else {
         dec->storage = k_storage_local;
      }
   }
}

void read_storage_index( front_t* front, dec_t* dec ) {
   if ( tk( front ) == tk_lit_decimal ) {
      position_t pos = front->token.pos;
      p_literal_t literal;
      p_literal_init( front, &literal, &front->token );
      dec->storage_index = literal.value;
      drop( front );
      skip( front, tk_colon );
      const char* name = "global";
      int max_loc = MAX_GLOBAL_LOCATIONS;
      switch ( dec->storage ) {
      case k_storage_world:
         name = "world";
         max_loc = MAX_WORLD_LOCATIONS;
         break;
      case k_storage_global:
         break;
      default:
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "index specified for %s storage", d_name_storage( dec->storage ) );
         f_bail( front );
      }
      if ( dec->storage_index < 0 || dec->storage_index >= max_loc ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN,
            &dec->storage_index_pos,
            "index for %s storage not between %d and %d",
            name, 0, max_loc - 1 );
         f_bail( front );
      }
   }
   else {
      // Index must be explicitly specified for these storages.
      if ( dec->storage == k_storage_global ||
         dec->storage == k_storage_world ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
            "missing index for %s storage", d_name_storage( dec->storage ) );
         f_bail( front );
      }
   }
}

void read_type( front_t* front, dec_t* dec ) {
   switch ( tk( front ) ) {
   case tk_int:
   case tk_str:
   case tk_bool:
      break;
   case tk_void:
      drop( front );
      return;
   default:
      goto not_type;
   }
   nkey_t* key = ntbl_goto( front->name_table,
      "!s", front->token.source, "!" );
   if ( ! key->value || key->value->node->type != k_node_type ) {
      goto not_type;
   }
   dec->type = ( type_t* ) key->value->node;
   drop( front );
   return;
   not_type:
   f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN,
      &front->token.pos, "expecting type in declaration" );
   f_bail( front );
}

void read_name( front_t* front, dec_t* dec ) {
   if ( tk( front ) != tk_id ) {
      // Parameters don't require a name.
      if ( dec->area != k_dec_param ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN,
            &front->token.pos, "missing name in declaration" );
         f_bail( front );
      }
      return;
   }
   dec->name = p_get_unique_name( front );
   dec->name_pos = front->token.pos;
   drop( front );
}

void read_dim( front_t* front, dec_t* dec ) {
   if ( tk( front ) != tk_bracket_l ) {
      dec->dim_last = NULL;
      dec->dim_head = NULL;
      dec->dim_implicit = NULL;
      return;
   }
   // At this time, an array can only appear at top scope.
   if ( dec->storage == k_storage_local ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &dec->pos,
         "array in local scope" );
      f_bail( front );
   }
   while ( tk( front ) == tk_bracket_l ) {
      position_t pos = front->token.pos;
      drop( front );
      expr_t* expr = NULL;
      // Implicit size.
      if ( tk( front ) == tk_bracket_r ) {
         // Only the first dimension can have an implicit size.
         if ( dec->dim_head ) {
            f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
               "implicit size in subsequent dimension" );
            f_bail( front );
         }
         drop( front );
      }
      else {
         p_expr_t p_expr;
         p_expr_init( &p_expr, k_expr_cond );
         p_expr_read( front, &p_expr );
         skip( front, tk_bracket_r );
         expr = p_expr.expr;
         if ( ! expr->is_constant ) {
            f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &expr->pos,
               "array size not constant expression" );
         }
         else if ( expr->value <= 0 ) {
            f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &expr->pos,
               "invalid array size" );
         }
      }
      dimension_t* dim = mem_alloc( sizeof( *dim ) );
      dim->size = 0;
      dim->element_size = 0;
      dim->next = NULL;
      if ( dec->dim_last ) {
         dec->dim_last->next = dim;
         dec->dim_last = dim;
      }
      else {
         dec->dim_head = dim;
         dec->dim_last = dim;
      }
      if ( expr ) {
         dim->size = expr->value;
         calc_array_element_size( dec->dim_head, dec->type );
      }
      else {
         dec->dim_implicit = dim;
      }
   }
}

void calc_array_element_size( dimension_t* dim, type_t* type ) {
   if ( dim->next ) {
      calc_array_element_size( dim->next, type );
      dim->element_size = dim->next->size * dim->next->element_size;
   }
   else {
      dim->element_size = type->size;
   }
}

void read_init( front_t* front, dec_t* dec ) {
   if ( tk( front ) != tk_assign ) {
      if ( dec->dim_implicit && ( ( dec->storage != k_storage_world &&
         dec->storage != k_storage_global ) || dec->dim_head->next ) ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN,
            &front->token.pos, "missing initialization" );
         f_bail( front );
      }
      return;
   }
   // At this time, there is no way to initialize an array at top scope with
   // world or global storage.
   if ( dec->storage == k_storage_world || dec->storage == k_storage_global ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
         "initialization of %s with %s storage", dec->dim_head ? "array" :
         "variable", d_name_storage( dec->storage ) );
      f_bail( front );
   }
   drop( front );
   if ( tk( front ) == tk_brace_l ) {
      read_initz( front, dec, NULL );
   }
   else {
      if ( dec->dim_head ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
            "array not initialized with initializer" );
         f_bail( front );
      }
      p_expr_t p_expr;
      p_expr_init( &p_expr, k_expr_assign );
      p_expr_read( front, &p_expr );
      if ( dec->storage == k_storage_module && ! p_expr.expr->is_constant ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &p_expr.expr->pos,
            "initial value not constant" );
         f_bail( front );
      }
      add_initial( dec, ( node_t* ) p_expr.expr );
      if ( p_expr.has_string ) {
         dec->initial_has_string = true;
      }
   }
}

void add_initial( dec_t* dec, node_t* value ) {
   initial_t* initial = mem_alloc( sizeof( *initial ) );
   initial->value = value;
   initial->next = NULL;
   if ( dec->initial ) {
      dec->initial_tail->next = initial;
      dec->initial_tail = initial;
   }
   else {
      dec->initial = initial;
      dec->initial_tail = initial;
   }
}

void read_initz( front_t* front, dec_t* dec, p_initz_t* parent ) {
   position_t pos = front->token.pos;
   skip( front, tk_brace_l );
   p_initz_t initz;
   initz.pos = pos;
   initz.num_initials = 0;
   if ( parent ) {
      if ( ! parent->dim->next ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "array does not have another dimension to initialize" );
         f_bail( front );
      }
      initz.dim = parent->dim->next;
      initz.dim_prev = parent->dim;
      parent->num_initials += 1;
      if ( parent->dim == dec->dim_implicit ) {
         dec->dim_implicit->size += 1;
      }
   }
   else {
      if ( ! dec->dim_head ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "initializer used on a scalar variable" );
         f_bail( front );
      }
      initz.dim = dec->dim_head;
      initz.dim_prev = NULL;
   }
   while ( tk( front ) != tk_brace_r ) {
      // Don't go over the dimension size. This does not apply to an implicit
      // dimension.
      if ( initz.dim != dec->dim_implicit &&
         initz.num_initials >= initz.dim->size ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
            "too many elements in initializer for dimension of size %d",
            initz.dim->size );
         f_bail( front );
      }
      if ( tk( front ) == tk_brace_l ) {
         read_initz( front, dec, &initz );
      }
      else {
         read_initz_value( front, dec, &initz );
      }
      if ( tk( front ) == tk_comma ) {
         drop( front );
      }
      else {
         break;
      }
   }
   skip( front, tk_brace_r );
   // There should be at least one value in the initializer.
   if ( ! initz.num_initials ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
         "initializer is empty" );
      f_bail( front );
   }
   // Skip the remaining, uninitialized elements.
   dec->array_jump += ( initz.dim->size - initz.num_initials ) *
      initz.dim->element_size;
}

void read_initz_value( front_t* front, dec_t* dec, p_initz_t* initz ) {
   if ( initz->dim != dec->dim_last ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
         "missing another initializer" );
      f_bail( front );
   }
   p_expr_t p_expr;
   p_expr_init( &p_expr, k_expr_assign );
   p_expr_read( front, &p_expr );
   if ( ! p_expr.expr->is_constant ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &p_expr.expr->pos,
         "initial value not constant" );
      f_bail( front );
   }
   if ( dec->array_jump > dec->array_index ) {
      array_jump_t* jump = mem_alloc( sizeof( *jump ) );
      jump->node.type = k_node_array_jump;
      jump->index = dec->array_jump;
      add_initial( dec, ( node_t* ) jump );
      dec->array_index = dec->array_jump;
   }
   add_initial( dec, ( node_t* ) p_expr.expr );
   initz->num_initials += 1;
   if ( initz->dim == dec->dim_implicit ) {
      dec->dim_implicit->size += 1;
   }
   dec->array_index += 1;
   dec->array_jump += 1;
   if ( p_expr.expr->value ) {
      dec->initial_not_zero = true;
   }
   if ( p_expr.has_string ) {
      dec->initial_has_string = true;
   }
}

void finish_var( front_t* front, dec_t* dec ) {
   if ( dec->area == k_dec_top ) {
      var_t* var = new_var( front, dec );
      p_use_key( front, dec->name, ( node_t* ) var, false );
      if ( var->dim ) {
         list_append( &front->ast->arrays, var );
      }
      else {
         // Variables with initials appear first.
         if ( dec->initial ) {
            list_prepend( &front->ast->vars, var );
         }
         else {
            list_append( &front->ast->vars, var );
         }
      }
   }
   else if ( dec->area == k_dec_local ) {
      alloc_local_offset( front, dec );
      var_t* var = new_var( front, dec );
      p_use_key( front, dec->name, ( node_t* ) var, true );
      list_append( &front->block->stmt_list, var );
   }
   else if ( dec->area == k_dec_for ) {
      alloc_local_index( front, dec );
      var_t* var = new_var( front, dec );
      p_use_key( front, dec->name, ( node_t* ) var, true );
      // list_append( front->dec_for, var );
   }
}

var_t* new_var( front_t* front, dec_t* dec ) {
   var_t* var = mem_alloc( sizeof( *var ) );
   var->node.type = k_node_var;
   var->pos = dec->name_pos;
   var->type = dec->type;
   var->name = dec->name;
   var->dim = NULL;
   var->initial = NULL;
   var->storage = dec->storage;
   var->index = dec->index;
   if ( dec->dim_head ) {
      var->dim = dec->dim_head;
   }
   else if ( dec->initial ) {
      initial_t* initial = mem_alloc( sizeof( *initial ) );
      initial->next = NULL;
      initial->value = ( node_t* ) dec->initial;
      var->initial = initial;
   }
   if ( var->dim ) {
      var->size = var->dim->size * var->dim->element_size;
   }
   else {
      var->size = var->type->size;
   }
   return var;
}

void add_module_var( front_t* front, dec_t* dec ) {
   var_t* var = new_var( front, dec );
   module_t* module = front->module;
   p_use_key( front, dec->name, ( node_t* ) var, false );
   if ( dec->storage == k_storage_module ) {
      if ( var->dim ) {
         if ( dec->is_static ) {
            list_append( &front->ast->arrays, var );
         }
         else {
            list_append( &front->ast->arrays, var );
         }
      }
      else {
         list_t* list = &front->ast->vars;
         // Custom types are allocated as arrays.
         if ( dec->type->known == k_type_custom ) {
            list = &front->ast->vars;
         }
         // Variables with initials appear first.
         if ( dec->initial ) {
            list_prepend( list, var );
         }
         else {
            list_append( list, var );
         }
      }
   }
}

void read_func( front_t* front, dec_t* dec ) {
   // No function allowed in parameter.
   if ( dec->area == k_dec_param ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &dec->pos,
         "parameter is a function" );
   }
    // Cannot specify storage for a function.
   else if ( dec->storage == k_storage_world ||
      dec->storage == k_storage_global ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &dec->storage_pos,
         "storage specified for function" );
      f_bail( front );
   }
   // The function name needs to be visible before any parameters, because a
   // parameter may have the same name as the function.
   p_use_key( front, dec->name, NULL, false );
   dec->func_def = dec->name->value;
   p_new_scope( front );
   skip( front, tk_paran_l );
   p_params_t params;
   init_params( &params, k_params_func, &front->token.pos );
   read_param_list( front, &params );
   skip( front, tk_paran_r );
   if ( tk( front ) == tk_assign ) {
      drop( front );
      read_bfunc( front, dec, &params );
   }
   else {
      read_ufunc( front, dec, &params );
   }
   p_pop_scope( front );
}

void init_params( p_params_t* params, int type, position_t* pos ) {
   params->pos = *pos;
   params->type = type;
   params->num_param = 0;
   params->min_param = 0;
   list_init( &params->var_list );
   params->default_given = false;
}

void read_param_list( front_t* front, p_params_t* params ) {
   if ( tk( front ) == tk_void ) {
      drop( front );
      return;
   }
   else if ( tk( front ) == tk_star ) {
      drop( front );
      if ( tk( front ) == tk_comma ) {
         drop( front );
      }
   }
   p_params_t* prev = front->params;
   front->params = params;
   while ( tk( front ) != tk_paran_r ) {
      p_read_dec( front, k_dec_param );
      if ( tk( front ) == tk_comma ) {
         drop( front );
      }
      else {
         break;
      }
   }
   front->params = prev;
}

void finish_param( front_t* front, p_params_t* params, dec_t* dec ) {
   if ( params->type == k_params_script ) {
      // Script parameter must be of 'int' type.
      if ( dec->type->known != k_type_int ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &dec->pos,
            "script parameter not of 'int' type" );
      }
      // Script cannot have a default parameter.
      if ( dec->initial ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &dec->pos,
            "default parameter in script" );
      }
   }
   else {
      if ( dec->initial ) {
         params->default_given = true;
      }
      else {
         // All default parameters must have a value.
         if ( params->default_given ) {
            f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &dec->pos,
               "parameter missing default value" );
            return;
         }
         ++params->min_param;
      }
   }
   if ( params->type != k_params_bfunc ) {
      alloc_local_index( front, dec );
      var_t* var = new_var( front, dec );
      if ( dec->name ) {
         p_use_key( front, dec->name, ( node_t* ) var, true );
      }
      param_t* param = mem_alloc( sizeof( *param ) );
      param->name = dec->name;
      param->var = var;
      param->default_value = NULL;
     // if ( dec->initial ) {
     //    param->default_value = dec->initial;
      //}
      list_append( &params->var_list, param );
   }
   ++params->num_param;
}

void read_bfunc( front_t* front, dec_t* dec, p_params_t* params ) {
   func_t* func = NULL;
   bool latent = false;
   if ( front->func_type == k_func_aspec ) {
      aspec_t* aspec = mem_alloc( sizeof( *aspec ) );
      test( front, tk_lit_decimal );
      p_literal_t literal;
      p_literal_init( front, &literal, &front->token );
      aspec->id = literal.value;
      drop( front );
      func = ( func_t* ) aspec;
   }
   else if ( front->func_type == k_func_ext ) {
      ext_func_t* ext = mem_alloc( sizeof( *ext ) );
      test( front, tk_lit_decimal );
      p_literal_t literal;
      p_literal_init( front, &literal, &front->token );
      ext->id = literal.value;
      drop( front );
      func = ( func_t* ) ext;
   }
   else if ( front->func_type == k_func_ded ) {
      ded_func_t* ded = mem_alloc( sizeof( *ded ) );
      test( front, tk_lit_decimal );
      p_literal_t literal;
      p_literal_init( front, &literal, &front->token );
      ded->opcode = literal.value;
      drop( front );
      skip( front, tk_comma );
      test( front, tk_lit_decimal );
      p_literal_init( front, &literal, &front->token );
      ded->opcode_constant = literal.value;
      drop( front );
      skip( front, tk_comma );
      test( front, tk_lit_decimal );
      p_literal_init( front, &literal, &front->token );
      latent = ( literal.value != 0 );
      drop( front );
      func = ( func_t* ) ded;
   }
   else {
      format_func_t* format = mem_alloc( sizeof( *format ) );
      test( front, tk_lit_decimal );
      p_literal_t literal;
      p_literal_init( front, &literal, &front->token );
      format->opcode = literal.value;
      drop( front );
      func = ( func_t* ) format;
   }
   skip( front, tk_semicolon );
   func->node.type = k_node_func;
   func->pos = dec->pos;
   func->return_type = dec->type;
   func->num_param = params->num_param;
   func->min_param = params->min_param;
   func->type = front->func_type;
   func->latent = latent;
   dec->func_def->node = ( node_t* ) func;
   p_remove_undef( front, dec->name );
}

void read_ufunc( front_t* front, dec_t* dec, p_params_t* params ) {
   ufunc_t* ufunc = mem_alloc( sizeof( *ufunc ) );
   ufunc->func.node.type = k_node_func;
   ufunc->func.pos = dec->name_pos;
   ufunc->func.type = k_func_user;
   ufunc->func.return_type = dec->type;
   ufunc->func.num_param = params->num_param;
   ufunc->func.min_param = params->min_param;
   ufunc->func.latent = false;
   ufunc->name = dec->name;
   ufunc->params = params->var_list;
   ufunc->index = 0;
   ufunc->offset = 0;
   dec->func_def->node = ( node_t* ) ufunc;
   // Only functions can be tested late. Everything else needs to be declared
   // first before usage.
   ufunc->usage = p_remove_undef( front, dec->name );
   list_init( &ufunc->body );
   p_block_t body;
   p_block_init_ufunc( &body, ( dec->type != NULL ) );
   front->block = &body;
   p_read_compound( front );
   front->block = NULL;
   ufunc->size = front->scope->size_high - params->num_param;
   ufunc->body = body.stmt_list;
   list_append( &front->module->funcs, ufunc );
   if ( dec->type && body.flow == k_flow_going ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &dec->pos,
         "end of non-void function can be reached but return statement"
         "missing" );
   }
}

void p_read_script( front_t* front ) {
   // Don't read an imported script.
   if ( front->importing ) {
      // skip_imported_block( front );
      return;
   }
   p_script_t script = {
      .pos = front->token.pos,
      .type = k_script_type_closed };
   skip( front, tk_script );
   read_script_number( front, &script );
   p_new_scope( front );
   p_params_t params;
   init_params( &params, k_params_script, &front->token.pos );
   // The parameter list can be absent. This is the same as "void", meaning no
   // parameters.
   if ( tk( front ) == tk_paran_l ) {
      drop( front );
      read_param_list( front, &params );
      skip( front, tk_paran_r );
   }
   read_script_type( front, &script, &params );
   read_script_flag( front, &script );
   p_block_t body;
   p_block_init_script( &body );
   front->block = &body;
   p_read_compound( front );
   front->block = NULL;
   script_add( front, &script, &params, &body );
   p_pop_scope( front );
}

void read_script_number( front_t* front, p_script_t* script ) {
   position_t pos;
   int num = 0;
   if ( tk( front ) == tk_shift_l ) {
      drop( front );
      // The token between the << and >> tokens must be the digit zero.
      if ( front->token.type == tk_lit_decimal && front->token.length == 1 &&
         front->token.source[ 0 ] == '0' ) {
         pos = front->token.pos;
         drop( front );
      }
      else {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
            "missing script number 0" );
      }
      skip( front, tk_shift_r );
   }
   else {
      pos = front->token.pos;
      p_expr_t p_expr;
      p_expr_init( &p_expr, k_expr_cond );
      front->reading_script_number = true;
      p_expr_read( front, &p_expr );
      front->reading_script_number = false;
      expr_t* expr = p_expr.expr;
      script->number = expr;
      // Script number must be constant.
      if ( ! expr->is_constant ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &expr->pos,
            "script number not a constant expression" );
         return;
      }
      // Script number must be within a valid range.
      num = expr->value;
      if ( num < SCRIPT_MIN_NUM || num > SCRIPT_MAX_NUM ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &expr->pos,
            "script number not between %d and %d", SCRIPT_MIN_NUM,
            SCRIPT_MAX_NUM );
         return;
      }
      // Number zero is not handled here. It must use the special syntax.
      if ( num == 0 ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &expr->pos,
            "script number 0 not between << and >>" ); 
         return;
      }
   }
   // There should be no duplicate scripts in the same module.
   script_t* earlier = find_script( front->module, num );
   if ( earlier ) {
      f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &pos,
         "script number %d already used", num );
      f_diag( front, DIAG_ERR | DIAG_LINE, &earlier->pos,
         "first script to use number found here" );
   }
}

script_t* find_script( module_t* module, int num ) {
   list_iterator_t i = list_iterate( &module->scripts );
   while ( ! list_end( &i ) ) {
      script_t* script = list_idata( &i );
      if ( script->number->value == num ) { return script; }
      list_next( &i );
   }
   return NULL;
}

void read_script_type( front_t* front, p_script_t* script,
   p_params_t* params ) {
   switch ( tk( front ) ) {
   case tk_open: script->type = k_script_type_open; break;
   case tk_respawn: script->type = k_script_type_respawn; break;
   case tk_death: script->type = k_script_type_death; break;
   case tk_enter: script->type = k_script_type_enter; break;
   case tk_pickup: script->type = k_script_type_pickup; break;
   case tk_blue_return: script->type = k_script_type_blue_return; break;
   case tk_red_return: script->type = k_script_type_red_return; break;
   case tk_white_return: script->type = k_script_type_white_return; break;
   case tk_lightning: script->type = k_script_type_lightning; break;
   case tk_disconnect: script->type = k_script_type_disconnect; break;
   case tk_unloading: script->type = k_script_type_unloading; break;
   case tk_return: script->type = k_script_type_return; break;
   default: break; // Closed script.
   }
   switch ( script->type ) {
   case k_script_type_closed:
      // Don't go over the maximum number of parameters.
      if ( params->num_param > SCRIPT_MAX_PARAMS ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &params->pos,
            "script has over maximum %d parameters", SCRIPT_MAX_PARAMS );
      }
      break;
   case k_script_type_disconnect:
      drop( front );
      // A disconnect script must have a single parameter. It is the number of
      // the player who disconnected from the server.
      if ( params->num_param != 1 ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &script->pos,
            "disconnect script missing player-number parameter" );
      }
      break;
   default:
      drop( front );
      // All other scripts must have zero parameters.
      if ( params->num_param != 0 ) {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &params->pos,
            "parameter list specified for %s script",
            d_name_script_type( script->type ) );
      }
      break;
   }
}

void read_script_flag( front_t* front, p_script_t* script ) {
   // Order of flags does not matter.
   while ( true ) {
      int flag = k_script_flag_net;
      switch ( tk( front ) ) {
      case tk_clientside: flag = k_script_flag_clientside; break;
      case tk_net: break;
      default: return;
      }
      // A flag should only be set once.
      if ( ! ( script->flags & flag ) ) {
         script->flags |= flag;
      }
      else {
         f_diag( front, DIAG_ERR | DIAG_LINE | DIAG_COLUMN, &front->token.pos,
            "%s flag already set", d_script_flag_name( flag ) );
      }
      drop( front );
   }
}

void script_add( front_t* front, p_script_t* p_script, p_params_t* params,
   p_block_t* body ) {
   script_t* script = mem_alloc( sizeof( *script ) );
   script->node.type = k_node_script;
   script->number = p_script->number;
   script->type = p_script->type;
   script->flags = p_script->flags;
   script->params = params->var_list;
   script->body = body->stmt_list;
   script->size = front->scope->size_high;
   script->pos = p_script->pos;
   list_append( &front->module->scripts, script );
}