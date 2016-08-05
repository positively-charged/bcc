#include <string.h>

#include "task.h"
#include "cache.h"

// Save
// ==========================================================================

#define WF( saver, field ) \
   f_wf( saver->w, field )
#define WV( saver, field, value ) \
   f_wv( saver->w, field, value, sizeof( *( value ) ) )
#define WS( saver, field, value ) \
   f_ws( saver->w, field, value )
#define WN( saver, field, value ) \
   f_ws( saver->w, field, name_s( saver, value, false ) )
#define WFN( saver, field, value ) \
   f_ws( saver->w, field, name_s( saver, value, true ) )

enum {
   // 0
   F_BASETYPE,
   F_COLUMN,
   F_CONSTANT,
   F_DIM,
   F_DIMCOUNT,
   F_ELEMENTSIZE,
   F_END,
   F_ENUMERATION,
   F_ENUMERATOR,
   F_FILEMAP,
   // 10
   F_FILEPATH,
   F_FUNC,
   F_HASREFMEMBER,
   F_ID,
   F_INDEX,
   F_LATENT,
   F_LIB,
   F_LINE,
   F_MAXPARAM,
   F_MINPARAM,
   // 20
   F_MSGBUILD,
   F_NAME,
   F_NAMEPOS,
   F_NAMESPACE,
   F_OBJECT,
   F_OFFSET,
   F_OPCODE,
   F_PARAM,
   F_POS,
   F_REF,
   // 30
   F_RETURNSPEC,
   F_SCRIPTCALLABLE,
   F_SIZE,
   F_SPEC,
   F_STORAGE,
   F_STORAGEINDEX,
   F_STRUCTURE,
   F_STRUCTUREMEMBER,
   F_TYPE,
   F_VALUE,
   // 40
   F_VALUESTRING,
   F_VAR,
   F_UNREACHABLE
};

struct saver {
   struct task* task;
   struct field_writer* w;
   struct library* lib;
   struct str string;
};

static void save_lib( struct saver* saver );
static void save_file_map( struct saver* saver );
static void save_namespace( struct saver* saver, struct ns* ns );
static void save_namespace_member( struct saver* saver,
   struct object* object );
static void save_constant( struct saver* saver, struct constant* constant );
static void save_enum( struct saver* saver, struct enumeration* enumeration );
static void save_struct( struct saver* saver, struct structure* structure );
static void save_struct_member( struct saver* saver,
   struct structure_member* member );
static void save_ref( struct saver* saver, struct ref* ref );
static void save_dim( struct saver* saver, struct dim* dim );
static void save_var( struct saver* saver, struct var* var );
static void save_func( struct saver* saver, struct func* func );
static void save_impl( struct saver* saver, struct func* func );
static void save_param_list( struct saver* saver, struct param* param );
static void save_object( struct saver* saver, struct object* object );
static void save_pos( struct saver* saver, struct pos* pos );
static int map_file( struct saver* saver, int id );
static const char* name_s( struct saver* saver, struct name* name, bool full );

void cache_save_lib( struct task* task, struct field_writer* writer,
   struct library* lib ) {
   struct saver saver;
   saver.task = task;
   saver.w = writer;
   saver.lib = lib;
   str_init( &saver.string );
   save_lib( &saver );
   str_deinit( &saver.string );
}

void save_lib( struct saver* saver ) {
   WF( saver, F_LIB );
   save_file_map( saver );
   if ( saver->lib->header ) {
      WS( saver, F_NAME, saver->lib->name.value );
      save_pos( saver, &saver->lib->name_pos );
   }
   list_iter_t i;
   list_iter_init( &i, &saver->lib->namespaces );
   while ( ! list_end( &i ) ) {
      save_namespace( saver, list_data( &i ) );
      list_next( &i );
   }
   WF( saver, F_END );
}

void save_file_map( struct saver* saver ) {
   WF( saver, F_FILEMAP );
   int size = list_size( &saver->lib->files );
   WV( saver, F_SIZE, &size );
   list_iter_t i;
   list_iter_init( &i, &saver->lib->files );
   while ( ! list_end( &i ) ) {
      struct file_entry* file = list_data( &i );
      WS( saver, F_FILEPATH, file->full_path.value );
      list_next( &i );
   }
   WF( saver, F_END );
}

void save_namespace( struct saver* saver, struct ns* ns ) {
   WF( saver, F_NAMESPACE );
   if ( ns != saver->task->upmost_ns ) {
      WFN( saver, F_NAME, ns->name );
   }
   list_iter_t i;
   list_iter_init( &i, &ns->fragments );
   while ( ! list_end( &i ) ) {
      struct ns_fragment* fragment = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &fragment->objects );
      while ( ! list_end( &k ) ) {
         save_namespace_member( saver, list_data( &k ) );
         list_next( &k );
      }
      list_next( &i );
   }
   WF( saver, F_END );
}

void save_namespace_member( struct saver* saver, struct object* object ) {
   switch ( object->node.type ) {
      struct var* var;
      struct func* func;
   case NODE_CONSTANT:
      save_constant( saver,
         ( struct constant* ) object );
      break;
   case NODE_ENUMERATION:
      save_enum( saver,
         ( struct enumeration* ) object );
      break;
   case NODE_STRUCTURE:
      save_struct( saver,
         ( struct structure* ) object );
      break;
   case NODE_VAR:
      var = ( struct var* ) object;
      if ( ! var->hidden ) {
         save_var( saver, var );
      }
      break;
   case NODE_FUNC:
      func = ( struct func* ) object;
      if ( ! func->hidden ) {
         save_func( saver, func );
      }
      break;
   default:
      break;
   }
}

void save_constant( struct saver* saver, struct constant* constant ) {
   WF( saver, F_CONSTANT );
   save_object( saver, &constant->object );
   WN( saver, F_NAME, constant->name );
   if ( constant->spec == SPEC_STR ) {
      struct indexed_string* string = t_lookup_string( saver->task,
         constant->value );
      WS( saver, F_VALUESTRING, string->value );
   }
   else {
      WV( saver, F_SPEC, &constant->spec );
      WV( saver, F_VALUE, &constant->value );
   }
   WF( saver, F_END );
}

void save_enum( struct saver* saver, struct enumeration* enumeration ) {
   WF( saver, F_ENUMERATION );
   save_object( saver, &enumeration->object );
   if ( enumeration->name ) {
      WN( saver, F_NAME, enumeration->name );
   }
   struct enumerator* enumerator = enumeration->head;
   while ( enumerator ) {
      WF( saver, F_ENUMERATOR );
      save_object( saver, &enumerator->object );
      WN( saver, F_NAME, enumerator->name );
      WV( saver, F_VALUE, &enumerator->value );
      WF( saver, F_END );
      enumerator = enumerator->next;
   }
   WV( saver, F_BASETYPE, &enumeration->base_type );
   WF( saver, F_END );
}

void save_struct( struct saver* saver, struct structure* structure ) {
   WF( saver, F_STRUCTURE );
   if ( ! structure->anon ) {
      WN( saver, F_NAME, structure->name );
   }
   struct structure_member* member = structure->member;
   while ( member ) {
      save_struct_member( saver, member );
      member = member->next;
   }
   WV( saver, F_SIZE, &structure->size );
   WV( saver, F_HASREFMEMBER, &structure->has_ref_member );
   WF( saver, F_END );
}

void save_struct_member( struct saver* saver,
   struct structure_member* member ) {
   WF( saver, F_STRUCTUREMEMBER );
   save_object( saver, &member->object );
   WN( saver, F_NAME, member->name );
   save_ref( saver, member->ref );
   save_dim( saver, member->dim );
   WV( saver, F_SPEC, &member->spec );
   WV( saver, F_OFFSET, &member->offset );
   WV( saver, F_SIZE, &member->size );
   WF( saver, F_END );
}

void save_ref( struct saver* saver, struct ref* ref ) {
   while ( ref ) {
      WF( saver, F_REF );
      save_pos( saver, &ref->pos );
      WV( saver, F_TYPE, &ref->type );
      switch ( ref->type ) {
         struct ref_struct* structure;
         struct ref_array* array;
         struct ref_func* func;
      case REF_STRUCTURE:
         structure = ( struct ref_struct* ) ref;
         WV( saver, F_STORAGE, &structure->storage );
         WV( saver, F_STORAGEINDEX, &structure->storage_index );
         break;
      case REF_ARRAY:
         array = ( struct ref_array* ) ref;
         WV( saver, F_DIMCOUNT, &array->dim_count );
         WV( saver, F_STORAGE, &array->storage );
         WV( saver, F_STORAGEINDEX, &array->storage_index );
         break;
      case REF_FUNCTION:
         func = ( struct ref_func* ) ref;
         save_param_list( saver, func->params );
         WV( saver, F_MINPARAM, &func->min_param );
         WV( saver, F_MAXPARAM, &func->max_param );
         WV( saver, F_MSGBUILD, &func->msgbuild );
         break;
      default:
         UNREACHABLE();
      }
      WF( saver, F_END );
      ref = ref->next;
   }
}

void save_dim( struct saver* saver, struct dim* dim ) {
   while ( dim ) {
      WF( saver, F_DIM );
      WV( saver, F_SIZE, &dim->length );
      WV( saver, F_ELEMENTSIZE, &dim->element_size );
      save_pos( saver, &dim->pos ); 
      WF( saver, F_END );
      dim = dim->next;
   }
}

void save_var( struct saver* saver, struct var* var ) {
   WF( saver, F_VAR );
   save_object( saver, &var->object );
   WN( saver, F_NAME, var->name );
   save_ref( saver, var->ref );
   save_dim( saver, var->dim );
   WV( saver, F_SPEC, &var->spec );
   WV( saver, F_STORAGE, &var->storage );
   WV( saver, F_INDEX, &var->index );
   WV( saver, F_SIZE, &var->size );
   WF( saver, F_END );
}

void save_func( struct saver* saver, struct func* func ) {
   WF( saver, F_FUNC );
   save_object( saver, &func->object );
   WV( saver, F_TYPE, &func->type );
   WN( saver, F_NAME, func->name );
   save_param_list( saver, func->params );
   save_impl( saver, func );
   WV( saver, F_RETURNSPEC, &func->return_spec );
   WV( saver, F_MINPARAM, &func->min_param );
   WV( saver, F_MAXPARAM, &func->max_param );
   WV( saver, F_MSGBUILD, &func->msgbuild );
   WF( saver, F_END );
}

void save_impl( struct saver* saver, struct func* func ) {
   switch ( func->type ) {
      struct func_aspec* aspec;
      struct func_ext* ext;
      struct func_ded* ded;
      struct func_format* format;
      struct func_user* user;
      struct func_intern* intern;
   case FUNC_ASPEC:
      aspec = func->impl;
      WV( saver, F_ID, &aspec->id );
      WV( saver, F_SCRIPTCALLABLE, &aspec->script_callable );
      break;
   case FUNC_EXT:
      ext = func->impl;
      WV( saver, F_ID, &ext->id );
      break;
   case FUNC_DED:
      ded = func->impl;
      WV( saver, F_OPCODE, &ded->opcode );
      WV( saver, F_LATENT, &ded->latent );
      break;
   case FUNC_FORMAT:
      format = func->impl;
      WV( saver, F_OPCODE, &format->opcode );
      break;
   case FUNC_USER:
      break;
   case FUNC_INTERNAL:
      intern = func->impl;
      WV( saver, F_ID, &intern->id );
      break;
   default:
      UNREACHABLE();
   }
}

void save_param_list( struct saver* saver, struct param* param ) {
   while ( param ) {
      WF( saver, F_PARAM );
      save_object( saver, &param->object );
      WV( saver, F_SPEC, &param->spec );
      WF( saver, F_END );
      param = param->next;
   }
}

void save_object( struct saver* saver, struct object* object ) {
   WF( saver, F_OBJECT );
   save_pos( saver, &object->pos );
   WF( saver, F_END );
}

void save_pos( struct saver* saver, struct pos* pos ) {
   WF( saver, F_POS );
   WV( saver, F_LINE, &pos->line );
   WV( saver, F_COLUMN, &pos->column );
   int id = map_file( saver, pos->id );
   WV( saver, F_ID, &id );
   WF( saver, F_END );
}

int map_file( struct saver* saver, int id ) {
   list_iter_t i;
   list_iter_init( &i, &saver->lib->files );
   int map_id = 0;
   while ( ! list_end( &i ) ) {
      struct file_entry* file = list_data( &i );
      if ( file->id == id ) {
         return map_id;
      }
      ++map_id;
      list_next( &i );
   }
   return 0;
}

const char* name_s( struct saver* saver, struct name* name, bool full ) {
   str_clear( &saver->string );
   t_copy_name( name, full, &saver->string );
   return saver->string.value;
}

// Restore
// ==========================================================================

#define RF( restorer, field ) \
   f_rf( restorer->r, field )
#define RV( restorer, field, value ) \
   f_rv( restorer->r, field, value, sizeof( *( value ) ) )
#define RS( restorer, field ) \
   f_rs( restorer->r, field )

struct restorer {
   struct task* task;
   struct field_reader* r;
   struct library* lib;
   struct ns* ns;
   struct file_entry** file_map;
   int file_map_size;
};

static void restore_lib( struct restorer* restorer );
static void restore_file_map( struct restorer* restorer );
static void restore_namespace( struct restorer* restorer );
static void restore_namespace_member( struct restorer* restorer );
static void restore_constant( struct restorer* restorer );
static void restore_enum( struct restorer* restorer );
static void restore_struct( struct restorer* restorer );
static void restore_struct_member( struct restorer* restorer,
   struct structure* structure );
static struct ref* restore_ref( struct restorer* restorer );
static struct ref* restore_specific_ref( struct restorer* restorer, int type );
static struct dim* restore_dim( struct restorer* restorer );
static void restore_var( struct restorer* restorer );
static void restore_func( struct restorer* restorer );
static void restore_impl( struct restorer* restorer, struct func* func );
static struct param* restore_param_list( struct restorer* restorer );
static void restore_object( struct restorer* restorer, struct object* object,
   int node );
static void restore_pos( struct restorer* restorer, struct pos* pos );
static struct file_entry* map_id( struct restorer* restorer, int id );

struct library* cache_restore_lib( struct cache* cache,
   struct field_reader* reader ) {
   struct restorer restorer;
   restorer.task = cache->task;
   restorer.r = reader;
   restorer.lib = NULL;
   restorer.ns = NULL;
   restorer.file_map = NULL;
   restorer.file_map_size = 0;
   restore_lib( &restorer );
   return restorer.lib;
}

void restore_lib( struct restorer* restorer ) {
   RF( restorer, F_LIB );
   restorer->lib = t_add_library( restorer->task );
   restorer->ns = restorer->task->upmost_ns;
   restore_file_map( restorer );
   if ( f_peek( restorer->r ) == F_NAME ) {
      str_append( &restorer->lib->name, RS( restorer, F_NAME ) );
      restore_pos( restorer, &restorer->lib->name_pos );
   }
   while ( f_peek( restorer->r ) != F_END ) {
      restore_namespace( restorer );
   }
   RF( restorer, F_END );
   mem_free( restorer->file_map );
}

void restore_file_map( struct restorer* restorer ) {
   RF( restorer, F_FILEMAP );
   RV( restorer, F_SIZE, &restorer->file_map_size );
   restorer->file_map = mem_alloc( sizeof( *restorer->file_map ) *
      restorer->file_map_size );
   for ( int i = 0; i < restorer->file_map_size; ++i ) {
      struct file_query query;
      t_init_file_query( &query, NULL, RS( restorer, F_FILEPATH ) );
      t_find_file( restorer->task, &query );
      restorer->file_map[ i ] = query.file;
      list_append( &restorer->lib->files, query.file );
   }
   RF( restorer, F_END );
}

void restore_namespace( struct restorer* restorer ) {
   RF( restorer, F_NAMESPACE );
   if ( f_peek( restorer->r ) == F_NAME ) {
      struct name* name = t_extend_name( restorer->task->upmost_ns->body,
         RS( restorer, F_NAME ) );
      struct ns* ns = t_alloc_ns( name );
      list_append( &restorer->ns->objects, ns );
      restorer->ns = ns;
      ns->name->object = &ns->object;
   }
   while ( f_peek( restorer->r ) != F_END ) {
      restore_namespace_member( restorer );
   }
   RF( restorer, F_END );
}

void restore_namespace_member( struct restorer* restorer ) {
   switch ( f_peek( restorer->r ) ) {
   case F_CONSTANT:
      restore_constant( restorer );
      break;
   case F_ENUMERATION:
      restore_enum( restorer );
      break;
   case F_STRUCTURE:
      restore_struct( restorer );
      break;
   case F_VAR:
      restore_var( restorer );
      break;
   case F_FUNC:
      restore_func( restorer );
      break;
   default:
      UNREACHABLE();
      RF( restorer, F_UNREACHABLE );
   }
}

void restore_constant( struct restorer* restorer ) {
   RF( restorer, F_CONSTANT );
   struct constant* constant = t_alloc_constant();
   restore_object( restorer, &constant->object, NODE_CONSTANT );
   constant->name = t_extend_name( restorer->ns->body,
      RS( restorer, F_NAME ) );
   if ( f_peek( restorer->r ) == F_VALUESTRING ) {
      const char* value = RS( restorer, F_VALUESTRING );
      struct indexed_string* string = t_intern_string( restorer->task, value,
         strlen( value ) );
      constant->spec = SPEC_STR;
      constant->value = string->index;
   }
   else {
      RV( restorer, F_SPEC, &constant->spec );
      RV( restorer, F_VALUE, &constant->value );
   }
   RF( restorer, F_END );
   list_append( &restorer->ns->objects, constant );
   list_append( &restorer->lib->objects, constant );
}

void restore_enum( struct restorer* restorer ) {
   RF( restorer, F_ENUMERATION );
   struct enumeration* enumeration = t_alloc_enumeration();
   restore_object( restorer, &enumeration->object, NODE_ENUMERATION );
   if ( f_peek( restorer->r ) == F_NAME ) {
      enumeration->name = t_extend_name( restorer->ns->body,
         RS( restorer, F_NAME ) );
      enumeration->body = t_extend_name( enumeration->name, "." );
   }
   else {
      enumeration->body = restorer->ns->body;
   }
   while ( f_peek( restorer->r ) == F_ENUMERATOR ) {
      RF( restorer, F_ENUMERATOR );
      struct enumerator* enumerator = t_alloc_enumerator();
      restore_object( restorer, &enumerator->object, NODE_ENUMERATOR );
      enumerator->name = t_extend_name( restorer->ns->body,
         RS( restorer, F_NAME ) );
      enumerator->enumeration = enumeration;
      RV( restorer, F_VALUE, &enumerator->value );
      RF( restorer, F_END );
      t_append_enumerator( enumeration, enumerator );
   }
   RV( restorer, F_BASETYPE, &enumeration->base_type );
   RF( restorer, F_END );
   list_append( &restorer->ns->objects, enumeration );
   list_append( &restorer->lib->objects, enumeration );
}

void restore_struct( struct restorer* restorer ) {
   RF( restorer, F_STRUCTURE );
   struct structure* structure = t_alloc_structure();
   if ( f_peek( restorer->r ) == F_NAME ) {
      structure->name = t_extend_name( restorer->ns->body,
         RS( restorer, F_NAME ) );
   }
   else {
      structure->name = t_create_name();
      structure->anon = true;
   }
   structure->body = t_extend_name( structure->name, "." );
   while ( f_peek( restorer->r ) == F_STRUCTUREMEMBER ) {
      restore_struct_member( restorer, structure );
   }
   RV( restorer, F_SIZE, &structure->size );
   RV( restorer, F_HASREFMEMBER, &structure->has_ref_member );
   RF( restorer, F_END );
   list_append( &restorer->ns->objects, structure );
   list_append( &restorer->lib->objects, structure );
}

void restore_struct_member( struct restorer* restorer,
   struct structure* structure ) {
   RF( restorer, F_STRUCTUREMEMBER );
   struct structure_member* member = t_alloc_structure_member();
   restore_object( restorer, &member->object, NODE_STRUCTURE_MEMBER );
   member->name = t_extend_name( structure->body, RS( restorer, F_NAME ) );
   member->ref = restore_ref( restorer );
   member->dim = restore_dim( restorer );
   RV( restorer, F_SPEC, &member->spec );
   RV( restorer, F_OFFSET, &member->offset );
   RV( restorer, F_SIZE, &member->size );
   RF( restorer, F_END );
   t_append_structure_member( structure, member );
}


struct ref* restore_ref( struct restorer* restorer ) {
   struct ref* head = NULL;
   struct ref* tail;
   while ( f_peek( restorer->r ) == F_REF ) {
      RF( restorer, F_REF );
      struct pos pos;
      restore_pos( restorer, &pos );
      int type;
      RV( restorer, F_TYPE, &type );
      struct ref* ref = restore_specific_ref( restorer, type );
      ref->pos = pos;
      ref->type = type;
      RF( restorer, F_END );
      if ( head ) {
         tail->next = ref;
      }
      else {
         head = ref;
      }
      tail = ref;
   }
   return head;
}

struct ref* restore_specific_ref( struct restorer* restorer, int type ) {
   switch ( type ) {
      struct ref_struct* structure;
      struct ref_array* array;
      struct ref_func* func;
   case REF_STRUCTURE:
      structure = mem_alloc( sizeof( *structure ) );
      RV( restorer, F_STORAGE, &structure->storage );
      RV( restorer, F_STORAGEINDEX, &structure->storage_index );
      return &structure->ref;
   case REF_ARRAY:
      array = mem_alloc( sizeof( *array ) );
      RV( restorer, F_DIMCOUNT, &array->dim_count );
      RV( restorer, F_STORAGE, &array->storage );
      RV( restorer, F_STORAGEINDEX, &array->storage_index );
      return &array->ref;
   case REF_FUNCTION:
      func = mem_alloc( sizeof( *func ) );
      func->params = restore_param_list( restorer );
      RV( restorer, F_MINPARAM, &func->min_param );
      RV( restorer, F_MAXPARAM, &func->max_param );
      RV( restorer, F_MSGBUILD, &func->msgbuild );
      return &func->ref;
   default:
      UNREACHABLE();
      RF( restorer, F_UNREACHABLE );
      return NULL;
   }
}

struct dim* restore_dim( struct restorer* restorer ) {
   struct dim* head = NULL;
   struct dim* tail;
   while ( f_peek( restorer->r ) == F_DIM ) {
      struct dim* dim = t_alloc_dim();
      RF( restorer, F_DIM );
      RV( restorer, F_SIZE, &dim->length );
      RV( restorer, F_ELEMENTSIZE, &dim->element_size );
      restore_pos( restorer, &dim->pos );
      RF( restorer, F_END );
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

void restore_var( struct restorer* restorer ) {
   RF( restorer, F_VAR );
   struct var* var = t_alloc_var();
   restore_object( restorer, &var->object, NODE_VAR );
   var->name = t_extend_name( restorer->ns->body, RS( restorer, F_NAME ) );
   var->ref = restore_ref( restorer );
   var->dim = restore_dim( restorer );
   RV( restorer, F_SPEC, &var->spec );
   RV( restorer, F_STORAGE, &var->storage );
   RV( restorer, F_INDEX, &var->index );
   RV( restorer, F_SIZE, &var->size );
   RF( restorer, F_END );
   list_append( &restorer->lib->objects, var );
   list_append( &restorer->ns->objects, var );
}

void restore_func( struct restorer* restorer ) {
   RF( restorer, F_FUNC );
   struct func* func = t_alloc_func();
   restore_object( restorer, &func->object, NODE_FUNC );
   RV( restorer, F_TYPE, &func->type );
   func->name = t_extend_name( restorer->ns->body, RS( restorer, F_NAME ) );
   func->params = restore_param_list( restorer );
   restore_impl( restorer, func );
   RV( restorer, F_RETURNSPEC, &func->return_spec );
   RV( restorer, F_MINPARAM, &func->min_param );
   RV( restorer, F_MAXPARAM, &func->max_param );
   RV( restorer, F_MSGBUILD, &func->msgbuild );
   RF( restorer, F_END );
   list_append( &restorer->ns->objects, func );
   list_append( &restorer->lib->objects, func );
}

void restore_impl( struct restorer* restorer, struct func* func ) {
   switch ( func->type ) {
      struct func_aspec* aspec;
      struct func_ext* ext;
      struct func_ded* ded;
      struct func_format* format;
      struct func_user* user;
      struct func_intern* intern;
   case FUNC_ASPEC:
      aspec = mem_slot_alloc( sizeof( *aspec ) );
      RV( restorer, F_ID, &aspec->id );
      RV( restorer, F_SCRIPTCALLABLE, &aspec->script_callable );
      func->impl = aspec;
      break;
   case FUNC_EXT:
      ext = mem_alloc( sizeof( *ext ) );
      RV( restorer, F_ID, &ext->id );
      func->impl = ext;
      break;
   case FUNC_DED:
      ded = mem_alloc( sizeof( *ded ) );
      RV( restorer, F_OPCODE, &ded->opcode );
      RV( restorer, F_LATENT, &ded->latent );
      func->impl = ded;
      break;
   case FUNC_FORMAT:
      format = mem_alloc( sizeof( *format ) );
      RV( restorer, F_OPCODE, &format->opcode );
      func->impl = format;
      break;
   case FUNC_USER:
      user = t_alloc_func_user();
      func->impl = user;
      break;
   case FUNC_INTERNAL:
      intern = mem_alloc( sizeof( *intern ) );
      RV( restorer, F_ID, &intern->id );
      func->impl = intern;
      break;
   default:
      UNREACHABLE();
   }
}

struct param* restore_param_list( struct restorer* restorer ) {
   struct param* head = NULL;
   struct param* tail;
   while ( f_peek( restorer->r ) == F_PARAM ) {
      RF( restorer, F_PARAM );
      struct param* param = t_alloc_param();
      restore_object( restorer, &param->object, NODE_PARAM );
      RV( restorer, F_SPEC, &param->spec );
      RF( restorer, F_END );
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

void restore_object( struct restorer* restorer, struct object* object,
   int node ) {
   RF( restorer, F_OBJECT );
   t_init_object( object, node );
   object->resolved = true;
   restore_pos( restorer, &object->pos );
   RF( restorer, F_END );
}

void restore_pos( struct restorer* restorer, struct pos* pos ) {
   RF( restorer, F_POS );
   RV( restorer, F_LINE, &pos->line );
   RV( restorer, F_COLUMN, &pos->column );
   RV( restorer, F_ID, &pos->id );
   struct file_entry* file = map_id( restorer, pos->id );
   pos->id = file->id;
   RF( restorer, F_END );
}

struct file_entry* map_id( struct restorer* restorer, int id ) {
   if ( id < restorer->file_map_size ) {
      return restorer->file_map[ id ];
   }
   return NULL;
}