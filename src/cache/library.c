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
   F_DESC,
   F_DIM,
   F_DIMCOUNT,
   F_ELEMENTSIZE,
   F_END,
   F_ENUMERATION,
   F_ENUMERATOR,
   // 10
   F_EXPR,
   F_FILEMAP,
   F_FILEPATH,
   F_FUNC,
   F_HASREFMEMBER,
   F_ID,
   F_INDEX,
   F_INSTANCE,
   F_LATENT,
   F_LIB,
   // 20
   F_LINE,
   F_MAXPARAM,
   F_MINPARAM,
   F_MSGBUILD,
   F_NAME,
   F_NAMEPOS,
   F_NAMESPACE,
   F_NAMESPACEFRAGMENT,
   F_NULLABLE,
   F_OBJECT,
   // 30
   F_OFFSET,
   F_OPCODE,
   F_PARAM,
   F_PATH,
   F_POS,
   F_REF,
   F_RETURNSPEC,
   F_SCRIPTCALLABLE,
   F_SIZE,
   F_SPEC,
   // 40
   F_STORAGE,
   F_STORAGEINDEX,
   F_STRUCTURE,
   F_STRUCTUREMEMBER,
   F_TEXT,
   F_TYPE,
   F_TYPEALIAS,
   F_VALUE,
   F_VALUESTRING,
   F_VAR,
   // 50
   F_UNREACHABLE,
   F_UPMOST
};

struct saver {
   struct task* task;
   struct field_writer* w;
   struct library* lib;
   struct str string;
};

static void save_lib( struct saver* saver );
static void save_file_map( struct saver* saver );
static void save_namespace( struct saver* saver,
   struct ns_fragment* fragment );
static void save_namespace_member( struct saver* saver,
   struct object* object );
static void save_constant( struct saver* saver, struct constant* constant );
static void save_enumeration( struct saver* saver,
   struct enumeration* enumeration );
static void save_enumerator( struct saver* saver,
   struct enumerator* enumerator );
static void save_structure( struct saver* saver, struct structure* structure );
static void save_structure_member( struct saver* saver,
   struct structure_member* member );
static void save_spec( struct saver* saver, struct structure* structure,
   struct enumeration* enumeration, struct path* path, int spec );
static void save_path( struct saver* saver, struct path* path );
static void save_ref( struct saver* saver, struct ref* ref );
static void save_dim( struct saver* saver, struct dim* dim );
static void save_type_alias( struct saver* saver, struct type_alias* alias );
static void save_var( struct saver* saver, struct var* var );
static void save_func( struct saver* saver, struct func* func );
static void save_impl( struct saver* saver, struct func* func );
static void save_param_list( struct saver* saver, struct param* param );
static void save_const_expr( struct saver* saver, struct expr* expr );
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
   save_namespace( saver, saver->lib->upmost_ns_fragment );
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

void save_namespace( struct saver* saver, struct ns_fragment* fragment ) {
   WF( saver, F_NAMESPACEFRAGMENT );
   // Save namespace path.
   if ( fragment->path ) {
      struct ns_path* path = fragment->path;
      while ( path ) {
         WF( saver, F_NAMESPACE );
         WS( saver, F_TEXT, path->text );
         save_pos( saver, &path->pos );
         WF( saver, F_END );
         path = path->next;
      }
   }
   // Save members.
   list_iter_t i;
   list_iter_init( &i, &fragment->objects );
   while ( ! list_end( &i ) ) {
      save_namespace_member( saver, list_data( &i ) );
      list_next( &i );
   }
   WF( saver, F_END );
}

void save_namespace_member( struct saver* saver, struct object* object ) {
   switch ( object->node.type ) {
   case NODE_CONSTANT: {
         struct constant* constant =
            ( struct constant* ) object;
         if ( ! constant->hidden ) {
            save_constant( saver, constant );
         }
      }
      break;
   case NODE_ENUMERATION: {
         struct enumeration* enumeration =
            ( struct enumeration* ) object;
         if ( ! enumeration->hidden && enumeration->semicolon ) {
            save_enumeration( saver, enumeration );
         }
      }
      break;
   case NODE_STRUCTURE: {
         struct structure* structure =
            ( struct structure* ) object;
         if ( ! structure->hidden && structure->semicolon ) {
            save_structure( saver, structure );
         }
      }
      break;
   case NODE_TYPE_ALIAS: {
         struct type_alias* alias =
            ( struct type_alias* ) object;
         if ( ! alias->hidden && alias->head_instance ) {
            save_type_alias( saver, alias );
         }
      }
      break;
   case NODE_VAR: {
         struct var* var =
            ( struct var* ) object;
         if ( ! var->hidden && var->head_instance ) {
            save_var( saver, var );
         }
      }
      break;
   case NODE_FUNC: {
         struct func* func =
            ( struct func* ) object;
         if ( ! func->hidden ) {
            save_func( saver, func );
         }
      }
      break;
   case NODE_NAMESPACEFRAGMENT:
      save_namespace( saver,
         ( struct ns_fragment* ) object );
      break;
   default:
      UNREACHABLE();
   }
}

void save_constant( struct saver* saver, struct constant* constant ) {
   WF( saver, F_CONSTANT );
   save_object( saver, &constant->object );
   WN( saver, F_NAME, constant->name );
   WV( saver, F_SPEC, &constant->spec );
   struct indexed_string* string = NULL;
   if ( constant->has_str ) {
      string = t_lookup_string( saver->task, constant->value );
   }
   if ( string ) {
      WS( saver, F_VALUESTRING, string->value );
   }
   else {
      WV( saver, F_VALUE, &constant->value );
   }
   WF( saver, F_END );
}

void save_enumeration( struct saver* saver,
   struct enumeration* enumeration ) {
   WF( saver, F_ENUMERATION );
   save_object( saver, &enumeration->object );
   if ( enumeration->name ) {
      WN( saver, F_NAME, enumeration->name );
   }
   struct enumerator* enumerator = enumeration->head;
   while ( enumerator ) {
      save_enumerator( saver, enumerator );
      enumerator = enumerator->next;
   }
   WV( saver, F_BASETYPE, &enumeration->base_type );
   WF( saver, F_END );
}

void save_enumerator( struct saver* saver, struct enumerator* enumerator ) {
   WF( saver, F_ENUMERATOR );
   save_object( saver, &enumerator->object );
   WN( saver, F_NAME, enumerator->name );
   struct indexed_string* string = NULL;
   if ( enumerator->has_str ) {
      string = t_lookup_string( saver->task, enumerator->value );
   }
   if ( string ) {
      WS( saver, F_VALUESTRING, string->value );
   }
   else {
      WV( saver, F_VALUE, &enumerator->value );
   }
   WF( saver, F_END );
}

void save_structure( struct saver* saver, struct structure* structure ) {
   WF( saver, F_STRUCTURE );
   save_object( saver, &structure->object );
   if ( ! structure->anon ) {
      WN( saver, F_NAME, structure->name );
   }
   struct structure_member* member = structure->member;
   while ( member ) {
      if ( member->head_instance ) {
         save_structure_member( saver, member );
      }
      member = member->next;
   }
   WF( saver, F_END );
}

void save_structure_member( struct saver* saver,
   struct structure_member* member ) {
   WF( saver, F_STRUCTUREMEMBER );
   save_spec( saver,
      member->structure,
      member->enumeration,
      member->path,
      member->original_spec );
   save_ref( saver, member->ref );
   while ( member ) {
      WF( saver, F_INSTANCE );
      save_object( saver, &member->object );
      WN( saver, F_NAME, member->name );
      save_dim( saver, member->dim );
      member = member->next_instance;
   }
   WF( saver, F_END );
}

void save_spec( struct saver* saver, struct structure* structure,
   struct enumeration* enumeration, struct path* path, int spec ) {
   switch ( spec ) {
   case SPEC_ENUM:
      save_enumeration( saver, enumeration );
      break;
   case SPEC_STRUCT:
      save_structure( saver, structure );
      break;
   case SPEC_NAME:
   case SPEC_NAME + SPEC_ENUM:
   case SPEC_NAME + SPEC_STRUCT:
      save_path( saver, path );
      WV( saver, F_SPEC, &spec );
      break;
   default:
      WV( saver, F_SPEC, &spec );
   }
}

void save_path( struct saver* saver, struct path* path ) {
   WF( saver, F_PATH );
   while ( path ) {
      WS( saver, F_TEXT, path->text );
      save_pos( saver, &path->pos );
      WV( saver, F_UPMOST, &path->upmost );
      path = path->next;
   }
   WF( saver, F_END );
}

void save_ref( struct saver* saver, struct ref* ref ) {
   while ( ref ) {
      WF( saver, F_REF );
      save_pos( saver, &ref->pos );
      WV( saver, F_TYPE, &ref->type );
      switch ( ref->type ) {
      case REF_STRUCTURE: {
            struct ref_struct* structure = ( struct ref_struct* ) ref;
            WV( saver, F_STORAGE, &structure->storage );
            WV( saver, F_STORAGEINDEX, &structure->storage_index );
         }
         break;
      case REF_ARRAY: {
            struct ref_array* array = ( struct ref_array* ) ref;
            WV( saver, F_DIMCOUNT, &array->dim_count );
            WV( saver, F_STORAGE, &array->storage );
            WV( saver, F_STORAGEINDEX, &array->storage_index );
         }
         break;
      case REF_FUNCTION: {
            struct ref_func* func = ( struct ref_func* ) ref;
            save_param_list( saver, func->params );
            WV( saver, F_MINPARAM, &func->min_param );
            WV( saver, F_MAXPARAM, &func->max_param );
            WV( saver, F_MSGBUILD, &func->msgbuild );
         }
         break;
      default:
         UNREACHABLE();
      }
      WV( saver, F_NULLABLE, &ref->nullable );
      WF( saver, F_END );
      ref = ref->next;
   }
}

void save_dim( struct saver* saver, struct dim* dim ) {
   while ( dim ) {
      WF( saver, F_DIM );
      WV( saver, F_SIZE, &dim->length );
      save_pos( saver, &dim->pos ); 
      WF( saver, F_END );
      dim = dim->next;
   }
}

void save_type_alias( struct saver* saver, struct type_alias* alias ) {
   WF( saver, F_TYPEALIAS );
   save_spec( saver,
      alias->structure,
      alias->enumeration,
      alias->path,
      alias->original_spec );
   save_ref( saver, alias->ref );
   while ( alias ) {
      WF( saver, F_INSTANCE );
      save_object( saver, &alias->object );
      WN( saver, F_NAME, alias->name );
      save_dim( saver, alias->dim );
      alias = alias->next_instance;
   }
   WF( saver, F_END );
}

void save_var( struct saver* saver, struct var* var ) {
   WF( saver, F_VAR );
   save_spec( saver,
      var->structure,
      var->enumeration,
      var->type_path,
      var->original_spec );
   save_ref( saver, var->ref );
   WV( saver, F_STORAGE, &var->storage );
   while ( var ) {
      WF( saver, F_INSTANCE );
      save_object( saver, &var->object );
      WN( saver, F_NAME, var->name );
      save_dim( saver, var->dim );
      WV( saver, F_INDEX, &var->index );
      var = var->next_instance;
   }
   WF( saver, F_END );
}

void save_func( struct saver* saver, struct func* func ) {
   WF( saver, F_FUNC );
   save_object( saver, &func->object );
   WV( saver, F_TYPE, &func->type );
   WN( saver, F_NAME, func->name );
   save_param_list( saver, func->params );
   save_impl( saver, func );
   save_spec( saver,
      func->structure,
      func->enumeration,
      func->path,
      func->original_return_spec );
   save_ref( saver, func->ref );
   WV( saver, F_MINPARAM, &func->min_param );
   WV( saver, F_MAXPARAM, &func->max_param );
   WV( saver, F_MSGBUILD, &func->msgbuild );
   WF( saver, F_END );
}

void save_impl( struct saver* saver, struct func* func ) {
   switch ( func->type ) {
   case FUNC_ASPEC: {
         struct func_aspec* aspec = func->impl;
         WV( saver, F_ID, &aspec->id );
         WV( saver, F_SCRIPTCALLABLE, &aspec->script_callable );
      }
      break;
   case FUNC_EXT: {
         struct func_ext* ext = func->impl;
         WV( saver, F_ID, &ext->id );
      }
      break;
   case FUNC_DED: {
         struct func_ded* ded = func->impl;
         WV( saver, F_OPCODE, &ded->opcode );
         WV( saver, F_LATENT, &ded->latent );
      }
      break;
   case FUNC_FORMAT: {
         struct func_format* format = func->impl;
         WV( saver, F_OPCODE, &format->opcode );
      }
      break;
   case FUNC_USER:
   case FUNC_ALIAS:
      break;
   case FUNC_INTERNAL: {
         struct func_intern* intern = func->impl;
         WV( saver, F_ID, &intern->id );
      }
      break;
   default:
      UNREACHABLE();
   }
}

void save_param_list( struct saver* saver, struct param* param ) {
   while ( param ) {
      WF( saver, F_PARAM );
      save_object( saver, &param->object );
      save_spec( saver, NULL, NULL, param->path, param->original_spec );
      save_ref( saver, param->ref );
      if ( param->default_value ) {
         save_const_expr( saver, param->default_value );
      }
      WF( saver, F_END );
      param = param->next;
   }
}

void save_const_expr( struct saver* saver, struct expr* expr ) {
   WF( saver, F_EXPR );
   save_pos( saver, &expr->pos );
   WV( saver, F_SPEC, &expr->spec );
   struct indexed_string* string = NULL;
   if ( expr->has_str ) {
      string = t_lookup_string( saver->task, expr->value );
   }
   if ( string ) {
      WS( saver, F_VALUESTRING, string->value );
   }
   else {
      WV( saver, F_VALUE, &expr->value );
   }
   WF( saver, F_END );
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
   struct ns_fragment* ns_fragment;
   struct file_entry** file_map;
   int file_map_size;
};

struct restored_spec {
   struct enumeration* enumeration;
   struct structure* structure;
   struct path* path;
   int spec;
};

static void restore_lib( struct restorer* restorer );
static void restore_file_map( struct restorer* restorer );
static void restore_namespace( struct restorer* restorer, bool upmost );
static void restore_namespace_path( struct restorer* restorer );
static void setup_namespaces( struct restorer* restorer );
static void restore_namespace_member( struct restorer* restorer );
static void restore_constant( struct restorer* restorer );
static struct enumeration* restore_enumeration( struct restorer* restorer );
static void restore_enumerator( struct restorer* restorer,
   struct enumeration* enumeration );
static struct structure* restore_structure( struct restorer* restorer );
static void restore_structure_member( struct restorer* restorer,
   struct structure* structure );
static void restore_spec( struct restorer* restorer,
   struct restored_spec* spec );
static struct path* restore_path( struct restorer* restorer );
static struct ref* restore_ref( struct restorer* restorer );
static struct ref* restore_specific_ref( struct restorer* restorer, int type );
static struct dim* restore_dim( struct restorer* restorer );
static void restore_type_alias( struct restorer* restorer );
static void restore_var( struct restorer* restorer );
static void restore_func( struct restorer* restorer );
static void restore_impl( struct restorer* restorer, struct func* func );
static struct param* restore_param_list( struct restorer* restorer );
static struct expr* restore_const_expr( struct restorer* restorer );
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
   restorer.ns_fragment = NULL;
   restorer.file_map = NULL;
   restorer.file_map_size = 0;
   restore_lib( &restorer );
   mem_free( restorer.file_map );
   return restorer.lib;
}

void restore_lib( struct restorer* restorer ) {
   RF( restorer, F_LIB );
   restorer->lib = t_add_library( restorer->task );
   restorer->ns_fragment = restorer->lib->upmost_ns_fragment;
   restorer->ns = restorer->ns_fragment->ns;
   restore_file_map( restorer );
   if ( f_peek( restorer->r ) == F_NAME ) {
      str_append( &restorer->lib->name, RS( restorer, F_NAME ) );
      restore_pos( restorer, &restorer->lib->name_pos );
   }
   restore_namespace( restorer, true );
   RF( restorer, F_END );
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

void restore_namespace( struct restorer* restorer, bool upmost ) {
   RF( restorer, F_NAMESPACEFRAGMENT );
   struct ns_fragment* parent_fragment = restorer->ns_fragment;
   struct ns* parent_ns = restorer->ns;
   if ( ! upmost ) {
      restorer->ns_fragment = t_alloc_ns_fragment();
      t_append_unresolved_namespace_object( parent_fragment,
         &restorer->ns_fragment->object );
      list_append( &parent_fragment->objects, restorer->ns_fragment );
   }
   restore_namespace_path( restorer );
   setup_namespaces( restorer );
   // Restore namespace members.
   while ( f_peek( restorer->r ) != F_END ) {
      restore_namespace_member( restorer );
   }
   restorer->ns_fragment = parent_fragment;
   restorer->ns = parent_ns;
   RF( restorer, F_END );
}

void restore_namespace_path( struct restorer* restorer ) {
   // Restore namespace path.
   struct ns_path* head = NULL;
   struct ns_path* tail;
   while ( f_peek( restorer->r ) == F_NAMESPACE ) {
      RF( restorer, F_NAMESPACE );
      struct ns_path* path = mem_alloc( sizeof( *path ) );
      const char* value = RS( restorer, F_TEXT );
      path->next = NULL;
      path->text = t_intern_text( restorer->task, value, strlen( value ) );
      restore_pos( restorer, &path->pos );
      if ( head ) {
         tail->next = path;
      }
      else {
         head = path;
      }
      tail = path;
      RF( restorer, F_END );
   }
   restorer->ns_fragment->path = head;
}

void setup_namespaces( struct restorer* restorer ) {
   if ( restorer->ns_fragment->path ) {
      struct ns_path* path = restorer->ns_fragment->path;
      while ( path ) {
         struct name* name = t_extend_name( restorer->ns->body, path->text );
         if ( ! name->object ) {
            struct ns* ns = t_alloc_ns( name );
            ns->object.pos = path->pos;
            ns->parent = restorer->ns;
            list_append( &restorer->lib->namespaces, ns );
            name->object = &ns->object;
         }
         restorer->ns = ( struct ns* ) name->object;
         path = path->next;
      }
   }
   else {
      restorer->ns = restorer->task->upmost_ns;
   }
   restorer->ns_fragment->ns = restorer->ns;
}

void restore_namespace_member( struct restorer* restorer ) {
   switch ( f_peek( restorer->r ) ) {
   case F_CONSTANT:
      restore_constant( restorer );
      break;
   case F_ENUMERATION: {
         struct enumeration* enumeration = restore_enumeration( restorer );
         enumeration->semicolon = true;
      }
      break;
   case F_STRUCTURE: {
         struct structure* structure = restore_structure( restorer );
         structure->semicolon = true;
      }
      break;
   case F_TYPEALIAS:
      restore_type_alias( restorer );
      break;
   case F_VAR:
      restore_var( restorer );
      break;
   case F_FUNC:
      restore_func( restorer );
      break;
   case F_NAMESPACEFRAGMENT:
      restore_namespace( restorer, false );
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
   constant->name = t_extend_name( restorer->ns_fragment->ns->body,
      RS( restorer, F_NAME ) );
   RV( restorer, F_SPEC, &constant->spec );
   if ( f_peek( restorer->r ) == F_VALUESTRING ) {
      const char* value = RS( restorer, F_VALUESTRING );
      int length = strlen( value );
      struct indexed_string* string = t_intern_string( restorer->task,
         t_intern_text( restorer->task, value, length ), length );
      constant->value = string->index;
      constant->has_str = true;
   }
   else {
      RV( restorer, F_VALUE, &constant->value );
   }
   RF( restorer, F_END );
   list_append( &restorer->ns_fragment->objects, constant );
   list_append( &restorer->lib->objects, constant );
   constant->object.resolved = true;
}

struct enumeration* restore_enumeration( struct restorer* restorer ) {
   RF( restorer, F_ENUMERATION );
   struct enumeration* enumeration = t_alloc_enumeration();
   restore_object( restorer, &enumeration->object, NODE_ENUMERATION );
   if ( f_peek( restorer->r ) == F_NAME ) {
      enumeration->name = t_extend_name( restorer->ns_fragment->ns->body_enums,
         RS( restorer, F_NAME ) );
      enumeration->body = t_extend_name( enumeration->name, "." );
   }
   else {
      enumeration->body = restorer->ns_fragment->ns->body;
   }
   while ( f_peek( restorer->r ) == F_ENUMERATOR ) {
      restore_enumerator( restorer, enumeration );
   }
   RV( restorer, F_BASETYPE, &enumeration->base_type );
   RF( restorer, F_END );
   list_append( &restorer->ns_fragment->objects, enumeration );
   list_append( &restorer->lib->objects, enumeration );
   enumeration->object.resolved = true;
   return enumeration;
}

void restore_enumerator( struct restorer* restorer,
   struct enumeration* enumeration ) {
   RF( restorer, F_ENUMERATOR );
   struct enumerator* enumerator = t_alloc_enumerator();
   restore_object( restorer, &enumerator->object, NODE_ENUMERATOR );
   enumerator->name = t_extend_name( restorer->ns_fragment->ns->body,
      RS( restorer, F_NAME ) );
   enumerator->enumeration = enumeration;
   if ( f_peek( restorer->r ) == F_VALUESTRING ) {
      const char* value = RS( restorer, F_VALUESTRING );
      int length = strlen( value );
      struct indexed_string* string = t_intern_string( restorer->task,
         t_intern_text( restorer->task, value, length ), length );
      enumerator->value = string->index;
      enumerator->has_str = true;
   }
   else {
      RV( restorer, F_VALUE, &enumerator->value );
   }
   RF( restorer, F_END );
   t_append_enumerator( enumeration, enumerator );
   enumerator->object.resolved = true;
}

struct structure* restore_structure( struct restorer* restorer ) {
   RF( restorer, F_STRUCTURE );
   struct structure* structure = t_alloc_structure();
   restore_object( restorer, &structure->object, NODE_STRUCTURE );
   if ( f_peek( restorer->r ) == F_NAME ) {
      structure->name = t_extend_name( restorer->ns_fragment->ns->body_structs,
         RS( restorer, F_NAME ) );
   }
   else {
      structure->name = t_create_name();
      structure->anon = true;
   }
   structure->body = t_extend_name( structure->name, "." );
   while ( f_peek( restorer->r ) == F_STRUCTUREMEMBER ) {
      restore_structure_member( restorer, structure );
   }
   RF( restorer, F_END );
   list_append( &restorer->ns_fragment->objects, structure );
   list_append( &restorer->lib->objects, structure );
   t_append_unresolved_namespace_object( restorer->ns_fragment,
      &structure->object );
   return structure;
}

void restore_structure_member( struct restorer* restorer,
   struct structure* structure ) {
   RF( restorer, F_STRUCTUREMEMBER );
   struct restored_spec spec;
   restore_spec( restorer, &spec );
   struct ref* ref = restore_ref( restorer );
   while ( f_peek( restorer->r ) == F_INSTANCE ) {
      RF( restorer, F_INSTANCE );
      struct structure_member* member = t_alloc_structure_member();
      restore_object( restorer, &member->object, NODE_STRUCTURE_MEMBER );
      member->name = t_extend_name( structure->body, RS( restorer, F_NAME ) );
      member->enumeration = spec.enumeration;
      member->structure = spec.structure;
      member->path = spec.path;
      member->ref = ref;
      member->dim = restore_dim( restorer );
      member->spec = spec.spec;
      t_append_structure_member( structure, member );
   }
   RF( restorer, F_END );
}

void restore_spec( struct restorer* restorer, struct restored_spec* spec ) {
   spec->enumeration = NULL;
   spec->structure = NULL;
   spec->path = NULL;
   switch ( f_peek( restorer->r ) ) {
   case F_ENUMERATION:
      spec->enumeration = restore_enumeration( restorer );
      spec->spec = SPEC_ENUM;
      break;
   case F_STRUCTURE:
      spec->structure = restore_structure( restorer );
      spec->spec = SPEC_STRUCT;
      break;
   case F_PATH:
      spec->path = restore_path( restorer );
      RV( restorer, F_SPEC, &spec->spec );
      break;
   default:
      RV( restorer, F_SPEC, &spec->spec );
   }
}

struct path* restore_path( struct restorer* restorer ) {
   struct path* head = NULL;
   struct path* tail;
   if ( f_peek( restorer->r ) == F_PATH ) {
      RF( restorer, F_PATH );
      while ( f_peek( restorer->r ) != F_END ) {
         struct path* path = mem_alloc( sizeof( *path ) );
         path->next = NULL;
         const char* value = RS( restorer, F_TEXT );
         path->text = t_intern_text( restorer->task, value, strlen( value ) );
         restore_pos( restorer, &path->pos );
         RV( restorer, F_UPMOST, &path->upmost );
         if ( head ) {
            tail->next = path;
         }
         else {
            head = path;
         }
         tail = path;
      }
      RF( restorer, F_END );
   }
   return head;
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
      ref->next = NULL;
      ref->pos = pos;
      ref->type = type;
      RV( restorer, F_NULLABLE, &ref->nullable );
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
   case REF_STRUCTURE: {
         struct ref_struct* structure = mem_alloc( sizeof( *structure ) );
         RV( restorer, F_STORAGE, &structure->storage );
         RV( restorer, F_STORAGEINDEX, &structure->storage_index );
         return &structure->ref;
      }
   case REF_ARRAY: {
         struct ref_array* array = mem_alloc( sizeof( *array ) );
         RV( restorer, F_DIMCOUNT, &array->dim_count );
         RV( restorer, F_STORAGE, &array->storage );
         RV( restorer, F_STORAGEINDEX, &array->storage_index );
         return &array->ref;
      }
   case REF_FUNCTION: {
         struct ref_func* func = mem_alloc( sizeof( *func ) );
         func->params = restore_param_list( restorer );
         RV( restorer, F_MINPARAM, &func->min_param );
         RV( restorer, F_MAXPARAM, &func->max_param );
         RV( restorer, F_MSGBUILD, &func->msgbuild );
         return &func->ref;
      }
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

void restore_type_alias( struct restorer* restorer ) {
   RF( restorer, F_TYPEALIAS );
   struct restored_spec spec;
   restore_spec( restorer, &spec );
   struct ref* ref = restore_ref( restorer );
   while ( f_peek( restorer->r ) == F_INSTANCE ) {
      RF( restorer, F_INSTANCE );
      struct type_alias* alias = t_alloc_type_alias();
      restore_object( restorer, &alias->object, NODE_TYPE_ALIAS );
      alias->name = t_extend_name( restorer->ns_fragment->ns->body,
         RS( restorer, F_NAME ) );
      alias->ref = ref;
      alias->enumeration = spec.enumeration;
      alias->structure = spec.structure;
      alias->dim = restore_dim( restorer );
      alias->path = spec.path;
      alias->original_spec = spec.spec;
      alias->spec = alias->original_spec;
      list_append( &restorer->ns_fragment->objects, alias );
      t_append_unresolved_namespace_object( restorer->ns_fragment,
         &alias->object );
   }
   RF( restorer, F_END );
}

void restore_var( struct restorer* restorer ) {
   RF( restorer, F_VAR );
   struct restored_spec spec;
   restore_spec( restorer, &spec );
   struct ref* ref = restore_ref( restorer );
   int storage;
   RV( restorer, F_STORAGE, &storage );
   while ( f_peek( restorer->r ) == F_INSTANCE ) {
      RF( restorer, F_INSTANCE );
      struct var* var = t_alloc_var();
      restore_object( restorer, &var->object, NODE_VAR );
      var->name = t_extend_name( restorer->ns_fragment->ns->body,
         RS( restorer, F_NAME ) );
      var->ref = ref;
      var->enumeration = spec.enumeration;
      var->structure = spec.structure;
      var->dim = restore_dim( restorer );
      var->type_path = spec.path;
      var->original_spec = spec.spec;
      var->spec = var->original_spec;
      var->storage = storage;
      RV( restorer, F_INDEX, &var->index );
      var->imported = true;
      list_append( &restorer->lib->vars, var );
      list_append( &restorer->lib->objects, var );
      list_append( &restorer->ns_fragment->objects, var );
      t_append_unresolved_namespace_object( restorer->ns_fragment,
         &var->object );
   }
   RF( restorer, F_END );
}

void restore_func( struct restorer* restorer ) {
   RF( restorer, F_FUNC );
   struct func* func = t_alloc_func();
   restore_object( restorer, &func->object, NODE_FUNC );
   RV( restorer, F_TYPE, &func->type );
   func->name = t_extend_name( restorer->ns_fragment->ns->body,
      RS( restorer, F_NAME ) );
   func->params = restore_param_list( restorer );
   restore_impl( restorer, func );
   struct restored_spec spec;
   restore_spec( restorer, &spec );
   func->enumeration = spec.enumeration;
   func->structure = spec.structure;
   func->path = spec.path;
   func->return_spec = spec.spec;
   func->original_return_spec = spec.spec;
   func->ref = restore_ref( restorer );
   RV( restorer, F_MINPARAM, &func->min_param );
   RV( restorer, F_MAXPARAM, &func->max_param );
   RV( restorer, F_MSGBUILD, &func->msgbuild );
   func->imported = true;
   RF( restorer, F_END );
   list_append( &restorer->lib->objects, func );
   list_append( &restorer->ns_fragment->objects, func );
   if ( func->type == FUNC_USER ) {
      list_append( &restorer->lib->funcs, func );
      list_append( &restorer->ns_fragment->funcs, func );
   }
   t_append_unresolved_namespace_object( restorer->ns_fragment,
      &func->object );
}

void restore_impl( struct restorer* restorer, struct func* func ) {
   switch ( func->type ) {
   case FUNC_ASPEC: {
         struct func_aspec* aspec = mem_slot_alloc( sizeof( *aspec ) );
         RV( restorer, F_ID, &aspec->id );
         RV( restorer, F_SCRIPTCALLABLE, &aspec->script_callable );
         func->impl = aspec;
      }
      break;
   case FUNC_EXT: {
         struct func_ext* ext = mem_alloc( sizeof( *ext ) );
         RV( restorer, F_ID, &ext->id );
         func->impl = ext;
      }
      break;
   case FUNC_DED: {
         struct func_ded* ded = mem_alloc( sizeof( *ded ) );
         RV( restorer, F_OPCODE, &ded->opcode );
         RV( restorer, F_LATENT, &ded->latent );
         func->impl = ded;
      }
      break;
   case FUNC_FORMAT: {
         struct func_format* format = mem_alloc( sizeof( *format ) );
         RV( restorer, F_OPCODE, &format->opcode );
         func->impl = format;
      }
      break;
   case FUNC_USER:
   case FUNC_ALIAS:
      func->impl = t_alloc_func_user();
      break;
   case FUNC_INTERNAL: {
         struct func_intern* intern = mem_alloc( sizeof( *intern ) );
         RV( restorer, F_ID, &intern->id );
         func->impl = intern;
      }
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
      struct restored_spec spec;
      restore_spec( restorer, &spec );
      param->path = spec.path;
      param->spec = spec.spec;
      param->original_spec = spec.spec;
      param->ref = restore_ref( restorer );
      if ( f_peek( restorer->r ) == F_EXPR ) {
         param->default_value = restore_const_expr( restorer );
         param->default_value_tested = true;
      }
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

struct expr* restore_const_expr( struct restorer* restorer ) {
   RF( restorer, F_EXPR );
   struct expr* expr = t_alloc_expr();
   restore_pos( restorer, &expr->pos );
   RV( restorer, F_SPEC, &expr->spec );
   if ( f_peek( restorer->r ) == F_VALUESTRING ) {
      const char* value = RS( restorer, F_VALUESTRING );
      int length = strlen( value );
      value = t_intern_text( restorer->task, value, length );
      struct indexed_string* string = t_intern_string( restorer->task,
         value, length );
      struct indexed_string_usage* usage = t_alloc_indexed_string_usage();
      usage->string = string;
      expr->root = &usage->node;
      expr->has_str = true;
   }
   else {
      struct literal* literal = t_alloc_literal();
      RV( restorer, F_VALUE, &literal->value );
      expr->root = &literal->node;
   }
   expr->folded = true;
   RF( restorer, F_END );
   return expr;
}

void restore_object( struct restorer* restorer, struct object* object,
   int node ) {
   RF( restorer, F_OBJECT );
   t_init_object( object, node );
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