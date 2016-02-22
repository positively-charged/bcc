#include "cache/serial/write.h"
#include "task.h"

struct serial {
   struct serialw* w;
   struct library* library;
   struct str string;
};

static void serialize_library( struct serial* serial );
static void serialize_libraryobject( struct serial* serial,
   struct object* object );
static void serialize_object( struct serial* serial, struct object* object );
static void serialize_var( struct serial* serial, struct var* var );
static void serialize_object( struct serial* serial, struct object* object );
static void serialize_spec( struct serial* serial, struct var* var );
static void serialize_dim( struct serial* serial, struct dim* dim );
static void serialize_path( struct serial* serial, struct path* path );
static void serialize_pos( struct serial* serial, struct pos* pos );
static void serialize_func( struct serial* serial, struct func* func );
static void serialize_param_list( struct serial* serial, struct param* param );
static void serialize_param( struct serial* serial, struct param* param );
static void serialize_funcimpl( struct serial* serial, struct func* func );
static void serialize_aspec_impl( struct serial* serial,
   struct func_aspec* impl );
static void serialize_ext_impl( struct serial* serial, struct func_ext* impl );
static void serialize_ded_impl( struct serial* serial, struct func_ded* impl );
static void serialize_format_impl( struct serial* serial,
   struct func_format* impl );
static void serialize_user_impl( struct serial* serial,
   struct func_user* impl );
static void serialize_struct( struct serial* serial, struct structure* struct_ );
static void serialize_structmember_list( struct serial* serial,
   struct structure* struct_ );
static void serialize_structmember( struct serial* serial,
   struct structure_member* member );
static void serialize_type( struct serial* serial, struct structure* type,
   struct path* path );
static void serialize_enum( struct serial* serial, struct constant_set* set );
static void serialize_constant( struct serial* serial,
   struct constant* constant );
static void serialize_expr( struct serial* serial, struct expr* expr );
static void serialize_expr_node( struct serial* serial, struct node* node );
static void serialize_literal( struct serial* serial, struct literal* literal );
const char* name_s( struct serial* serial, struct name* name );

void cache_dump_module( struct task* task, struct library* lib,
   struct gbuf* buffer ) {
   struct serialw writing;
   srl_init_writing( &writing, buffer );
   struct serial serial;
   serial.w = &writing;
   serial.library = lib;
   str_init( &serial.string );
   serialize_library( &serial );
   str_deinit( &serial.string );
}

void serialize_library( struct serial* serial ) {
   struct library* lib = serial->library;
   srlw_f( serial->w, F_LIB );
   srlw_s( serial->w, F_STRNAME, lib->name.value );
   srlw_f( serial->w, F_NAMEPOS );
   serialize_pos( serial, &lib->name_pos );
   int size =
      list_size( &lib->vars ) * sizeof( struct var );
   printf( "%d\n", size );
   srlw_i( serial->w, F_INTOBJMEMTOTALSIZE, size );
   list_iter_t i;
   list_iter_init( &i, &lib->objects );
   while ( ! list_end( &i ) ) {
      serialize_libraryobject( serial, list_data( &i ) );
      list_next( &i );
   }
   srlw_f( serial->w, F_END );
}

void serialize_libraryobject( struct serial* serial, struct object* object ) {
   switch ( object->node.type ) {
   case NODE_VAR:
      serialize_var( serial, ( struct var* ) object );
      break;
/*
   case NODE_FUNC:
      serialize_func( serial, ( struct func* ) object );
      break;
   case NODE_TYPE:
      serialize_struct( serial, ( struct type* ) object );
      break;
   case NODE_CONSTANT:
      serialize_constant( serial, ( struct constant* ) object );
      break;
   case NODE_CONSTANT_SET:
      serialize_enum( serial, ( struct constant_set* ) object );
      break;
*/
   default:
      break;
   }
}

void acknowledge( const char* file, int line, bool success ) {
   if ( ! success ) {
      printf( "%s:%d: internal warning: "
         "field acknowledgment failure\n", file, line );
   }
}

#include <stddef.h>

#define ACKNOWLEDGE_STRUCT( structure ) \
   { \
      typedef structure type; \
      acknowledge( __FILE__, __LINE__, 0 ==
#define ACKNOWLEDGE( field ) \
         offsetof( type, field ) && \
         offsetof( type, field ) + sizeof( ( ( type* ) 0 )->field ) ==
#define ACKNOWLEDGE_LAST( field ) \
         offsetof( type, field ) \
      ); \
   }

void serialize_var( struct serial* serial, struct var* var ) {
   srlw_f( serial->w, F_VAR );
   serialize_object( serial, &var->object );
   srlw_s( serial->w, F_STRNAME, name_s( serial, var->name ) );
   serialize_type( serial, var->structure, var->type_path );
   serialize_dim( serial, var->dim );
   srlw_i( serial->w, F_INTSTORAGE, var->storage );
   srlw_i( serial->w, F_INTINDEX, var->index );
   srlw_i( serial->w, F_INTSIZE, var->size );
   srlw_b( serial->w, F_BOOLINITZZERO, var->initz_zero );
   srlw_b( serial->w, F_BOOLHIDDEN, var->hidden );
   srlw_b( serial->w, F_BOOLIMPORTED, var->imported );
   srlw_b( serial->w, F_BOOLISCONSTANTINIT, var->is_constant_init );
   srlw_f( serial->w, F_END );
   #ifdef DEBUG
      ACKNOWLEDGE_STRUCT( struct var )
      ACKNOWLEDGE( object )
      ACKNOWLEDGE( name )
      ACKNOWLEDGE( type )
      ACKNOWLEDGE( type_path )
      ACKNOWLEDGE( dim )
      ACKNOWLEDGE( initial )
      ACKNOWLEDGE( value )
      ACKNOWLEDGE( next )
      ACKNOWLEDGE( index )
      ACKNOWLEDGE( size )
      ACKNOWLEDGE( initz_zero )
      ACKNOWLEDGE( hidden )
      ACKNOWLEDGE( used )
      ACKNOWLEDGE( initial_has_str )
      ACKNOWLEDGE( imported )
      ACKNOWLEDGE_LAST( is_constant_init )
   #endif
}

void serialize_object( struct serial* serial, struct object* object ) {
   srlw_f( serial->w, F_OBJECT );
   serialize_pos( serial, &object->pos );
   srlw_f( serial->w, F_END );
}

void serialize_spec( struct serial* serial, struct var* var ) {
   if ( var->type_path ) {
      srlw_f( serial->w, F_TYPEPATH );
      serialize_path( serial, var->type_path );
   }
   else {
      srlw_s( serial->w, F_STRTYPE, name_s( serial, var->structure->name ) );
   }
}

void serialize_dim( struct serial* serial, struct dim* dim ) {
   while ( dim ) {
      srlw_f( serial->w, F_DIM );
      srlw_i( serial->w, F_INTSIZE, dim->size );
      srlw_i( serial->w, F_INTELEMENTSIZE, dim->element_size );
      srlw_f( serial->w, F_END );
      dim = dim->next;
   }
}

void serialize_path( struct serial* serial, struct path* path ) {
   srlw_f( serial->w, F_PATH );
   srlw_s( serial->w, F_STRTEXT, path->text );
   serialize_pos( serial, &path->pos );
   srlw_f( serial->w, F_END );
}

void serialize_pos( struct serial* serial, struct pos* pos ) {
   srlw_f( serial->w, F_POS );
   srlw_i( serial->w, F_INTLINE, pos->line );
   srlw_i( serial->w, F_INTCOLUMN, pos->column );
   srlw_i( serial->w, F_INTID, pos->id );
   srlw_f( serial->w, F_END );
}

void serialize_func( struct serial* serial, struct func* func ) {
   srlw_f( serial->w, F_FUNC );
   serialize_object( serial, &func->object );
   srlw_i( serial->w, F_INTTYPE, func->type );
   srlw_s( serial->w, F_STRNAME, name_s( serial, func->name ) );
   if ( func->params ) {
      serialize_param_list( serial, func->params );
   }
   if ( func->return_type ) {
      srlw_s( serial->w, F_STRRETURNTYPE,
         name_s( serial, func->return_type->name ) );
   }
   serialize_funcimpl( serial, func );
   srlw_i( serial->w, F_INTMINPARAM, func->min_param );
   srlw_i( serial->w, F_INTMAXPARAM, func->max_param );
   srlw_f( serial->w, F_END );
}

void serialize_param_list( struct serial* serial, struct param* param ) {
   while ( param ) {
      serialize_param( serial, param );
      param = param->next;
   }
}

void serialize_param( struct serial* serial, struct param* param ) {
   srlw_f( serial->w, F_PARAM );
   serialize_object( serial, &param->object );
   srlw_s( serial->w, F_STRTYPE, name_s( serial, param->structure->name ) );
   if ( param->default_value ) {
      serialize_expr( serial, param->default_value );
   }
   srlw_f( serial->w, F_END );
}

void serialize_funcimpl( struct serial* serial, struct func* func ) {
   switch ( func->type ) {
   case FUNC_ASPEC:
      serialize_aspec_impl( serial, func->impl );
      break;
   case FUNC_EXT:
      serialize_ext_impl( serial, func->impl );
      break;
   case FUNC_DED:
      serialize_ded_impl( serial, func->impl );
      break;
   case FUNC_FORMAT:
      serialize_format_impl( serial, func->impl );
      break;
   case FUNC_USER:
      serialize_user_impl( serial, func->impl );
      break;
   default:
      break;
   }
}

void serialize_aspec_impl( struct serial* serial, struct func_aspec* impl ) {
   srlw_f( serial->w, F_ASPECFUNCIMPL );
   srlw_i( serial->w, F_INTID, impl->id );
   srlw_b( serial->w, F_BOOLSCRIPTCALLABLE, impl->script_callable );
   srlw_f( serial->w, F_END );
}

void serialize_ext_impl( struct serial* serial, struct func_ext* impl ) {
   srlw_f( serial->w, F_EXTFUNCIMPL );
   srlw_i( serial->w, F_INTID, impl->id );
   srlw_f( serial->w, F_END );
}

void serialize_ded_impl( struct serial* serial, struct func_ded* impl ) {
   srlw_f( serial->w, F_DEDFUNCIMPL );
   srlw_i( serial->w, F_INTOPCODE, impl->opcode );
   srlw_b( serial->w, F_BOOLLATENT, impl->latent );
   srlw_f( serial->w, F_END );
}

void serialize_format_impl( struct serial* serial, struct func_format* impl ) {
   srlw_f( serial->w, F_FORMATFUNCIMPL );
   srlw_i( serial->w, F_INTOPCODE, impl->opcode );
   srlw_f( serial->w, F_END );
}

void serialize_user_impl( struct serial* serial, struct func_user* impl ) {
   srlw_f( serial->w, F_USERFUNCIMPL );
   srlw_f( serial->w, F_END );
}

void serialize_struct( struct serial* serial, struct structure* struct_ ) {
   srlw_f( serial->w, F_STRUCT );
   if ( ! struct_->anon ) {
      srlw_s( serial->w, F_STRNAME, name_s( serial, struct_->name ) );
   }
   serialize_structmember_list( serial, struct_ );
   srlw_i( serial->w, F_INTSIZE, struct_->size );
   srlw_f( serial->w, F_END );
}

void serialize_structmember_list( struct serial* serial,
   struct structure* struct_ ) {
   struct structure_member* member = struct_->member;
   while ( member ) {
      serialize_structmember( serial, member );
      member = member->next;
   }
}

void serialize_structmember( struct serial* serial,
   struct structure_member* member ) {
   srlw_f( serial->w, F_STRUCTMEMBER );
   serialize_object( serial, &member->object );
   srlw_s( serial->w, F_STRNAME, name_s( serial, member->name ) );
   serialize_type( serial, member->structure, member->type_path );
   serialize_dim( serial, member->dim );
   srlw_i( serial->w, F_INTOFFSET, member->offset );
   srlw_i( serial->w, F_INTSIZE, member->size );
   srlw_f( serial->w, F_END );
}

void serialize_type( struct serial* serial, struct structure* type,
   struct path* path ) {
   if ( path ) {
      srlw_f( serial->w, F_TYPEPATH );
      serialize_path( serial, path );
   }
   else {
      srlw_f( serial->w, F_TYPE );
      if ( type->primitive ) {
         srlw_s( serial->w, F_STRTYPE, name_s( serial, type->name ) );
      }
      else {
         serialize_struct( serial, type );
      }
   }
}

void serialize_enum( struct serial* serial, struct constant_set* set ) {
   srlw_f( serial->w, F_ENUM );
   struct constant* enumerator = set->head;
   while ( enumerator ) {
      serialize_constant( serial, enumerator );
      enumerator = enumerator->next;
   }
   srlw_f( serial->w, F_END );
}

void serialize_constant( struct serial* serial, struct constant* constant ) {
   srlw_f( serial->w, F_CONSTANT );
   serialize_object( serial, &constant->object );
   srlw_s( serial->w, F_STRNAME, name_s( serial, constant->name) );
   srlw_i( serial->w, F_INTVALUE, constant->value );
   srlw_f( serial->w, F_END );
}

void serialize_expr( struct serial* serial, struct expr* expr ) {
   srlw_f( serial->w, F_EXPR );
   serialize_expr_node( serial, expr->root );
   srlw_i( serial->w, F_INTVALUE, expr->value );
   srlw_f( serial->w, F_END );
}

void serialize_expr_node( struct serial* serial, struct node* node ) {
   switch ( node->type ) {
   case NODE_LITERAL:
      serialize_literal( serial, ( struct literal* ) node );
      break;
   default:
      break;
   }
}

void serialize_literal( struct serial* serial, struct literal* literal ) {
   srlw_f( serial->w, F_LITERAL );
   srlw_i( serial->w, F_INTVALUE, literal->value );
   srlw_f( serial->w, F_END );
}

const char* name_s( struct serial* serial, struct name* name ) {
   str_clear( &serial->string );
   t_copy_name( name, false, &serial->string );
   return serial->string.value;
}