#include "cache/serial/read.h"
#include "cache/cache.h"

struct serial {
   struct serial_r* r;
   struct task* task;
   struct library* library;
   char* memory_start;
   char* memory;
};

static void deserialize_library( struct serial* serial );
static bool deserialize_libraryobject( struct serial* serial );
static void deserialize_var( struct serial* serial );
static void deserialize_object( struct serial* serial,
   struct object* object, int node );
static void deserialize_spec( struct serial* serial, struct var* var );
static struct structure* deserialize_primitive_type( struct serial* serial,
   enum field field );
static struct path* deserialize_path( struct serial* serial );
static void deserialize_pos( struct serial* serial, struct pos* pos );
static struct dim* deserialize_dim( struct serial* serial );
static void deserialize_func( struct serial* serial );
static struct param* deserialize_param_list( struct serial* serial );
static struct param* deserialize_param( struct serial* serial );
static struct expr* deserialize_param_defaultvalue( struct serial* serial );
static void* deserialize_funcimpl( struct serial* serial );
static void* deserialize_aspec_impl( struct serial* serial );
static void* deserialize_ext_impl( struct serial* serial );
static void* deserialize_ded_impl( struct serial* serial );
static void* deserialize_format_impl( struct serial* serial );
static void* deserialize_user_impl( struct serial* serial );
static struct structure* deserialize_struct( struct serial* serial );
static void deserialize_structmember_list( struct serial* serial,
   struct structure* struct_ );
static struct structure_member* deserialize_structmember( struct serial* serial,
   struct structure* struct_ );
static struct structure* deserialize_type( struct serial* serial );
static struct path* deserialize_type_path( struct serial* serial );
static struct enumeration* deserialize_enum( struct serial* serial );
static struct enumerator* deserialize_enumerator( struct serial* serial );
static struct constant* deserialize_constant( struct serial* serial );
static struct expr* deserialize_expr( struct serial* serial );
static struct node* deserialize_expr_node( struct serial* serial );
static struct node* deserialize_literal( struct serial* serial );

struct library* cache_restore_module( struct task* task,
   const char* encoded_text ) {
   jmp_buf exit;
   if ( setjmp( exit ) == 0 ) {
      struct serial_r reading;
      srl_init_reading( &reading, &exit, encoded_text );
      struct serial serial;
      serial.r = &reading;
      serial.task = task;
      serial.library = t_add_library( task );
      deserialize_library( &serial );
      return serial.library;
   }
   return NULL;
}

void deserialize_library( struct serial* serial ) {
   srl_rf( serial->r, F_LIB );
   str_append( &serial->library->name, srl_rs( serial->r, F_STRNAME ) );
   srl_rf( serial->r, F_NAMEPOS );
   deserialize_pos( serial, &serial->library->name_pos );
   int memory_required = srl_ri( serial->r, F_INTOBJMEMTOTALSIZE );
   serial->memory_start = mem_alloc( memory_required );
   serial->memory = serial->memory_start;
   while ( deserialize_libraryobject( serial ) );
   srl_rf( serial->r, F_END );
}

bool deserialize_libraryobject( struct serial* serial ) {
   void* object;
   switch ( srl_peekf( serial->r ) ) {
   case F_VAR:
      deserialize_var( serial );
      break;
   case F_FUNC:
      deserialize_func( serial );
      break;
   case F_STRUCT:
      object = deserialize_struct( serial );
      //t_append_object( &serial->library->resolved_objects, object );
      break;
   case F_CONSTANT:
      object = deserialize_constant( serial );
      //t_append_object( &serial->library->resolved_objects, object );
      break;
   case F_ENUM:
      object = deserialize_enum( serial );
      //t_append_object( &serial->library->resolved_objects, object );
      break;
   default:
      return false;
   }
   return true;
}

void* alloc( struct serial* serial, int size ) {
   char* block = serial->memory;
   serial->memory += size;
   return block;
}

void deserialize_var( struct serial* serial ) {
   srl_rf( serial->r, F_VAR );
   struct var* var = alloc( serial, sizeof( *var ) );
   deserialize_object( serial, &var->object, NODE_VAR );
   var->name = t_extend_name( serial->task->root_name,
      srl_rs( serial->r, F_STRNAME ) );
   var->structure = deserialize_type( serial );
   var->type_path = deserialize_type_path( serial );
   var->dim = deserialize_dim( serial );
   var->storage = srl_ri( serial->r, F_INTSTORAGE );
   var->index = srl_ri( serial->r, F_INTINDEX );
   var->size = srl_ri( serial->r, F_INTSIZE );
   var->initz_zero = srl_rb( serial->r, F_BOOLINITZZERO );
   var->hidden = srl_rb( serial->r, F_BOOLHIDDEN );
   var->used = false;
   var->imported = srl_rb( serial->r, F_BOOLIMPORTED );
   var->is_constant_init = srl_rb( serial->r, F_BOOLISCONSTANTINIT );
   srl_rf( serial->r, F_END );
   list_append( &serial->library->vars, var );
   list_append( &serial->library->objects, var );
}

void deserialize_object( struct serial* serial,
   struct object* object, int node ) {
   srl_rf( serial->r, F_OBJECT );
   t_init_object( object, node );
   deserialize_pos( serial, &object->pos );
   object->resolved = true;
   srl_rf( serial->r, F_END );
}

struct structure* deserialize_primitive_type( struct serial* serial,
   enum field field ) {
   struct name* name = t_extend_name( serial->task->root_name,
      srl_rs( serial->r, field ) );
   if ( name->object && name->object->node.type == NODE_STRUCTURE ) {
      return ( struct structure* ) name->object;
   }
   else {
      return NULL;
   }
}

struct path* deserialize_path( struct serial* serial ) {
   srl_rf( serial->r, F_PATH );
   struct path* path = mem_alloc( sizeof( *path ) );
   path->next = NULL;
   //path->text = srl_rs( serial->r, F_STRTEXT );
   deserialize_pos( serial, &path->pos ); 
   return path;
}

void deserialize_pos( struct serial* serial, struct pos* pos ) {
   srl_rf( serial->r, F_POS );
   pos->line = srl_ri( serial->r, F_INTLINE );
   pos->column = srl_ri( serial->r, F_INTCOLUMN );
   pos->id = srl_ri( serial->r, F_INTID );
   srl_rf( serial->r, F_END );
}

struct dim* deserialize_dim( struct serial* serial ) {
   struct dim* head = NULL;
   struct dim* tail;
   while ( srl_peekf( serial->r ) == F_DIM ) {
      srl_rf( serial->r, F_DIM );
      struct dim* dim = mem_alloc( sizeof( *dim ) );
      dim->next = NULL;
      dim->size = srl_ri( serial->r, F_INTSIZE );
      dim->element_size = srl_ri( serial->r, F_INTELEMENTSIZE );
      srl_rf( serial->r, F_END );
      if ( head ) {
         tail->next = dim;
      }
      else {
         head = dim;
      }
      tail = dim;
   }
   return head;
}

void deserialize_func( struct serial* serial ) {
   srl_rf( serial->r, F_FUNC );
   struct func* func = mem_slot_alloc( sizeof( *func ) );
   deserialize_object( serial, &func->object, NODE_FUNC );
   func->type = srl_ri( serial->r, F_INTTYPE );
   func->name = t_extend_name( serial->task->root_name,
      srl_rs( serial->r, F_STRNAME ) );
   func->params = deserialize_param_list( serial );
   if ( srl_peekf( serial->r ) == F_STRRETURNTYPE ) {
      func->return_type = deserialize_primitive_type( serial,
         F_STRRETURNTYPE );
   }
   else {
      func->return_type = NULL;
   }
   func->impl = deserialize_funcimpl( serial );
   func->min_param = srl_ri( serial->r, F_INTMINPARAM );
   func->max_param = srl_ri( serial->r, F_INTMAXPARAM );
   func->hidden = false;
   srl_rf( serial->r, F_END );
   //t_append_object( &serial->library->resolved_objects, &func->object );
   if ( func->type == FUNC_USER ) {
      list_append( &serial->library->funcs, func );
   }
}

struct param* deserialize_param_list( struct serial* serial ) {
   struct param* head = NULL;
   struct param* tail;
   while ( srl_peekf( serial->r ) == F_PARAM ) {
      struct param* param = deserialize_param( serial );
      if ( head ) {
         tail->next = param;
      }
      else {
         head = param;
      }
      tail = param;
   }
   return head;
}

struct param* deserialize_param( struct serial* serial ) {
   srl_rf( serial->r, F_PARAM );
   struct param* param = mem_slot_alloc( sizeof( *param ) );
   deserialize_object( serial, &param->object, NODE_PARAM );
   param->structure = deserialize_primitive_type( serial, F_STRTYPE );
   param->default_value = deserialize_param_defaultvalue( serial );
   param->next = NULL;
   param->used = false;
   srl_rf( serial->r, F_END );
   return param;
}

struct expr* deserialize_param_defaultvalue( struct serial* serial ) {
   if ( srl_peekf( serial->r ) == F_EXPR ) {
      return deserialize_expr( serial );
   }
   return NULL;
}

void* deserialize_funcimpl( struct serial* serial ) {
   switch ( srl_peekf( serial->r ) ) {
   case F_ASPECFUNCIMPL:
      return deserialize_aspec_impl( serial );
   case F_EXTFUNCIMPL:
      return deserialize_ext_impl( serial );
   case F_DEDFUNCIMPL:
      return deserialize_ded_impl( serial );
   case F_FORMATFUNCIMPL:
      return deserialize_format_impl( serial );
   case F_USERFUNCIMPL:
      return deserialize_user_impl( serial );
   default:
      return NULL;
   }
}

void* deserialize_aspec_impl( struct serial* serial ) {
   srl_rf( serial->r, F_ASPECFUNCIMPL );
   struct func_aspec* impl = mem_slot_alloc( sizeof( *impl ) );
   impl->id = srl_ri( serial->r, F_INTID );
   impl->script_callable = srl_rb( serial->r, F_BOOLSCRIPTCALLABLE );
   srl_rf( serial->r, F_END );
   return impl;
}

void* deserialize_ext_impl( struct serial* serial ) {
   struct func_ext* impl = mem_alloc( sizeof( *impl ) );
   srl_rf( serial->r, F_EXTFUNCIMPL );
   impl->id = srl_ri( serial->r, F_INTID );
   srl_rf( serial->r, F_END );
   return impl;
}

void* deserialize_ded_impl( struct serial* serial ) {
   struct func_ded* impl = mem_alloc( sizeof( *impl ) );
   srl_rf( serial->r, F_DEDFUNCIMPL );
   impl->opcode = srl_ri( serial->r, F_INTOPCODE );
   impl->latent = srl_ri( serial->r, F_BOOLLATENT );
   srl_rf( serial->r, F_END );
   return impl;
}

void* deserialize_format_impl( struct serial* serial ) {
   struct func_format* impl = mem_alloc( sizeof( *impl ) );
   srl_rf( serial->r, F_FORMATFUNCIMPL );
   impl->opcode = srl_ri( serial->r, F_INTOPCODE );
   srl_rf( serial->r, F_END );
   return impl;
}

void* deserialize_user_impl( struct serial* serial ) {
/*
   srl_rf( serial->r, F_USERFUNCIMPL );
   struct func_user* impl = t_alloc_user_funcimpl();
   srl_rf( serial->r, F_END );
   return impl;
*/
return NULL;
}

struct structure* deserialize_struct( struct serial* serial ) {
   srl_rf( serial->r, F_STRUCT );
   struct name* name;
   bool anon = false;
   if ( srl_peekf( serial->r ) == F_STRNAME ) {
      name = t_extend_name( serial->task->root_name,
         srl_rs( serial->r, F_STRNAME ) );
   }
   else {
      name = t_create_name();
      anon = true;
   }
   struct structure* struct_ = t_create_structure( serial->task, name );
   struct_->anon = anon;
   deserialize_structmember_list( serial, struct_ );
   struct_->size = srl_ri( serial->r, F_INTSIZE );
   srl_rf( serial->r, F_END );
   return struct_;
}

void deserialize_structmember_list( struct serial* serial,
   struct structure* struct_ ) {
   struct structure_member* head = NULL;
   struct structure_member* tail = NULL;
   while ( srl_peekf( serial->r ) == F_STRUCTMEMBER ) {
      struct structure_member* member = deserialize_structmember( serial, struct_ );
      if ( head ) {
         tail->next = member;
      }
      else {
         head = member;
      }
      tail = member;
   }
   struct_->member = head;
   struct_->member_tail = tail;
}

struct structure_member* deserialize_structmember( struct serial* serial,
   struct structure* struct_ ) {
   srl_rf( serial->r, F_STRUCTMEMBER );
   struct structure_member* member = mem_alloc( sizeof( *member ) );
   deserialize_object( serial, &member->object, NODE_STRUCTURE_MEMBER );
   member->next = NULL;
   member->name = t_extend_name( struct_->body,
      srl_rs( serial->r, F_STRNAME ) );
   member->structure = deserialize_type( serial );
   member->type_path = deserialize_type_path( serial );
   member->dim = deserialize_dim( serial );
   member->offset = srl_ri( serial->r, F_INTOFFSET );
   member->size = srl_ri( serial->r, F_INTSIZE );
   srl_rf( serial->r, F_END );
   return member;
}

struct structure* deserialize_type( struct serial* serial ) {
   if ( srl_peekf( serial->r ) == F_TYPE ) {
      srl_rf( serial->r, F_TYPE );
      if ( srl_peekf( serial->r ) == F_STRUCT ) {
         return deserialize_struct( serial );
      }
      else {
         return deserialize_primitive_type( serial, F_STRTYPE );
      }
   }
   return NULL;
}

struct path* deserialize_type_path( struct serial* serial ) {
   if ( srl_peekf( serial->r ) == F_TYPEPATH ) {
      srl_rf( serial->r, F_TYPEPATH );
      return deserialize_path( serial ); 
   }
   return NULL;
}

struct enumeration* deserialize_enum( struct serial* serial ) {
   srl_rf( serial->r, F_ENUM );
   struct enumerator* head = NULL;
   struct enumerator* tail;
   while ( srl_peekf( serial->r ) == F_CONSTANT ) {
      struct enumerator* enumerator = deserialize_enumerator( serial );
      if ( head ) {
         tail->next = enumerator;
      }
      else {
         head = enumerator;
      }
      tail = enumerator;
   }
   srl_rf( serial->r, F_END );
   struct enumeration* enum_ = mem_alloc( sizeof( *enum_ ) );
   t_init_object( &enum_->object, NODE_ENUMERATION );
   enum_->head = head;
   return enum_;
}

struct enumerator* deserialize_enumerator( struct serial* serial ) {
   srl_rf( serial->r, F_CONSTANT );
   struct enumerator* enumerator = mem_slot_alloc( sizeof( *enumerator ) );
   deserialize_object( serial, &enumerator->object, NODE_ENUMERATOR );
   enumerator->name = t_extend_name( serial->task->root_name,
      srl_rs( serial->r, F_STRNAME ) );
   enumerator->next = NULL;
   enumerator->value = srl_ri( serial->r, F_INTVALUE );
   enumerator->initz = NULL;
   srl_rf( serial->r, F_END );
   return enumerator;
}

struct constant* deserialize_constant( struct serial* serial ) {
   srl_rf( serial->r, F_CONSTANT );
   struct constant* constant = mem_slot_alloc( sizeof( *constant ) );
   deserialize_object( serial, &constant->object, NODE_CONSTANT );
   constant->name = t_extend_name( serial->task->root_name,
      srl_rs( serial->r, F_STRNAME ) );
   constant->next = NULL;
   constant->value = srl_ri( serial->r, F_INTVALUE );
   constant->value_node = NULL;
   srl_rf( serial->r, F_END );
   return constant;
}

struct expr* deserialize_expr( struct serial* serial ) {
   srl_rf( serial->r, F_EXPR );
   struct expr* expr = mem_alloc( sizeof( *expr ) );
   expr->node.type = NODE_EXPR;
   expr->root = deserialize_expr_node( serial );
   expr->value = srl_ri( serial->r, F_INTVALUE );
   srl_rf( serial->r, F_END );
   return expr;
}

struct node* deserialize_expr_node( struct serial* serial ) {
   switch ( srl_peekf( serial->r ) ) {
   case F_LITERAL:
      return deserialize_literal( serial );
   default:
      return NULL;
   }
}

struct node* deserialize_literal( struct serial* serial ) {
/*
   srl_rf( serial->r, F_LITERAL );
   struct literal* literal = t_alloc_literal();
   literal->value = srl_ri( serial->r, F_INTVALUE );
   srl_rf( serial->r, F_END );
   return &literal->node;
*/
   return NULL;
}