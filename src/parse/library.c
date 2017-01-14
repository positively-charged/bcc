#include <string.h>

#include "phase.h"
#include "cache/cache.h"

enum pseudo_dirc {
   PSEUDODIRC_UNKNOWN,
   PSEUDODIRC_DEFINE,
   PSEUDODIRC_LIBDEFINE,
   PSEUDODIRC_INCLUDE,
   PSEUDODIRC_IMPORT,
   PSEUDODIRC_LIBRARY,
   PSEUDODIRC_ENCRYPTSTRINGS,
   PSEUDODIRC_WADAUTHOR,
   PSEUDODIRC_NOWADAUTHOR,
   PSEUDODIRC_NOCOMPACT,
   PSEUDODIRC_REGION,
   PSEUDODIRC_ENDREGION
};

struct ns_qual {
   bool strict;
};

static void read_module( struct parse* parse );
static void read_module_item( struct parse* parse );
static void read_module_item_acs( struct parse* parse );
static void read_namespace( struct parse* parse );
static void init_namespace_qualifier( struct ns_qual* qualifiers );
static void read_namespace_qualifier( struct parse* parse,
   struct ns_qual* qualifiers );
static void read_namespace_name( struct parse* parse );
static void read_namespace_path( struct parse* parse );
static void read_namespace_member_list( struct parse* parse );
static void read_namespace_member( struct parse* parse );
static void read_using( struct parse* parse, struct list* output,
   bool block_scope );
static struct using_dirc* alloc_using( struct pos* pos );
static void read_using_item( struct parse* parse, struct using_dirc* dirc );
static struct using_item* alloc_using_item( void );
static struct path* alloc_path( struct pos pos );
static void read_pseudo_dirc( struct parse* parse, bool first_object );
static enum tk determine_bcs_pseudo_dirc( struct parse* parse );
static void read_include( struct parse* parse );
static void read_define( struct parse* parse );
static void read_import( struct parse* parse, struct pos* pos );
static void read_library( struct parse* parse, struct pos* pos,
   bool first_object );
static void read_library_name( struct parse* parse, struct pos* pos );
static void test_library_name( struct parse* parse, struct pos* pos,
   const char* name, int length );
static void read_linklibrary( struct parse* parse, struct pos* pos );
static void add_library_link( struct parse* parse, const char* name,
   struct pos* pos );
static void read_imported_libs( struct parse* parse );
static void import_lib( struct parse* parse, struct import_dirc* dirc );
static struct library* get_previously_processed_lib( struct parse* parse,
   struct file_entry* file );
static struct library* find_lib( struct parse* parse,
   struct file_entry* file );
static void append_imported_lib( struct parse* parse, struct library* lib );
static void determine_needed_library_links( struct parse* parse );

void p_read_target_lib( struct parse* parse ) {
   read_module( parse );
   read_imported_libs( parse );
   list_append( &parse->task->libraries, parse->task->library_main );
   // Unbind namespaces. They will be rebinded in the semantic phase.
   list_iter_t i;
   list_iter_init( &i, &parse->task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->namespaces );
      while ( ! list_end( &k ) ) {
         struct ns* ns = list_data( &k );
         ns->name->object = NULL;
         list_next( &k );
      }
      list_next( &i );
   }
   determine_needed_library_links( parse );
}

void read_module( struct parse* parse ) {
   if ( parse->tk == TK_HASH ) {
      read_pseudo_dirc( parse, true );
   }
   while ( parse->tk != TK_END ) {
      read_module_item( parse );
   }
   p_confirm_ifdircs_closed( parse );
}

void read_module_item( struct parse* parse ) {
   switch ( parse->lang ) {
   case LANG_ACS:
   case LANG_ACS95:
      read_module_item_acs( parse );
      break;
   default:
      switch ( parse->tk ) {
      case TK_HASH:
         read_pseudo_dirc( parse, false );
         break;
      default:
         read_namespace_member( parse );
      }
      break;
   }
}

void read_module_item_acs( struct parse* parse ) {
   if ( p_is_dec( parse ) || parse->tk == TK_FUNCTION ) {
      struct dec dec;
      p_init_dec( &dec );
      dec.name_offset = parse->ns->body;
      p_read_dec( parse, &dec );
   }
   else {
      switch ( parse->tk ) {
      case TK_SCRIPT:
         p_read_script( parse );
         break;
      case TK_HASH:
         read_pseudo_dirc( parse, false );
         break;
      case TK_SPECIAL:
         p_read_special_list( parse );
         break;
      default:
         p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
            "unexpected %s", p_present_token_temp( parse, parse->tk ) );
         p_bail( parse );
      }
   }
}

void read_namespace( struct parse* parse ) {
   struct ns_qual qualifiers;
   init_namespace_qualifier( &qualifiers );
   read_namespace_qualifier( parse, &qualifiers );
   struct ns_fragment* parent_fragment = parse->ns_fragment;
   p_test_tk( parse, TK_NAMESPACE );
   struct ns_fragment* fragment = t_alloc_ns_fragment();
   fragment->object.pos = parse->tk_pos;
   fragment->strict = qualifiers.strict;
   t_append_unresolved_namespace_object( parent_fragment, &fragment->object );
   list_append( &parent_fragment->objects, fragment );
   list_append( &parent_fragment->fragments, fragment );
   parse->ns_fragment = fragment;
   p_read_tk( parse );
   read_namespace_name( parse );
   p_test_tk( parse, TK_BRACE_L );
   p_read_tk( parse );
   read_namespace_member_list( parse );
   p_test_tk( parse, TK_BRACE_R );
   p_read_tk( parse );
   parse->ns_fragment = parent_fragment;
   parse->ns = parent_fragment->ns;
}

void init_namespace_qualifier( struct ns_qual* qualifiers ) {
   qualifiers->strict = false;
}

void read_namespace_qualifier( struct parse* parse,
   struct ns_qual* qualifiers ) {
   if ( parse->tk == TK_STRICT ) {
      qualifiers->strict = true;
      p_read_tk( parse );
   }
}

void read_namespace_name( struct parse* parse ) {
   if ( parse->tk == TK_ID ||
      // An unqualified, unnamed namespace does not do anything useful. Force
      // an unqualified namespace to have a name.
      ! parse->ns_fragment->strict ) {
      read_namespace_path( parse );
   }
   if ( parse->ns_fragment->path ) {
      struct ns_path* path = parse->ns_fragment->path;
      while ( path ) {
         struct name* name = t_extend_name( parse->ns->body, path->text );
         if ( ! name->object ) {
            struct ns* ns = t_alloc_ns( name );
            ns->object.pos = path->pos;
            ns->parent = parse->ns;
            list_append( &parse->lib->namespaces, ns );
            name->object = &ns->object;
         }
         parse->ns = ( struct ns* ) name->object;
         parse->ns_fragment->ns = parse->ns;
         path = path->next;
      }
   }
   else {
      // A nameless namespace fragment is a fragment of the parent namespace.
      parse->ns_fragment->ns = parse->ns;
   }
}

void read_namespace_path( struct parse* parse ) {
   if ( parse->tk != TK_ID ) {
      p_unexpect_diag( parse );
      p_unexpect_last_name( parse, NULL, "namespace name" );
      p_bail( parse );
   }
   struct ns_path* head = mem_alloc( sizeof( *head ) );
   struct ns_path* tail = head;
   head->next = NULL;
   head->text = parse->tk_text;
   head->pos = parse->tk_pos;
   p_read_tk( parse );
   while ( parse->tk == TK_DOT ) {
      p_read_tk( parse );
      p_test_tk( parse, TK_ID );
      struct ns_path* path = mem_alloc( sizeof( *head ) );
      path->next = NULL;
      path->text = parse->tk_text;
      path->pos = parse->tk_pos;
      tail->next = path;
      tail = path;
      p_read_tk( parse );
   }
   parse->ns_fragment->path = head;
}

void read_namespace_member_list( struct parse* parse ) {
   while ( parse->tk != TK_BRACE_R ) {
      read_namespace_member( parse );
   }
}

void read_namespace_member( struct parse* parse ) {
   if ( p_is_dec( parse ) ) {
      struct dec dec;
      p_init_dec( &dec );
      dec.name_offset = parse->ns->body;
      p_read_dec( parse, &dec );
   }
   else if ( parse->tk == TK_SCRIPT ) {
      p_read_script( parse );
   }
   else if (
      parse->tk == TK_STRICT ||
      parse->tk == TK_NAMESPACE ) {
      read_namespace( parse );
   }
   else if ( parse->tk == TK_USING ) {
      read_using( parse, &parse->ns_fragment->usings, false );
   }
   else if ( parse->tk == TK_SPECIAL ) {
      p_read_special_list( parse );
   }
   else if ( parse->tk == TK_SEMICOLON ) {
      p_read_tk( parse );
   }
   else {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "unexpected %s", p_present_token_temp( parse, parse->tk ) );
      p_bail( parse );
   }
}

void read_using( struct parse* parse, struct list* output,
   bool block_scope ) {
   struct using_dirc* dirc = alloc_using( &parse->tk_pos );
   dirc->force_local_scope = block_scope;
   p_test_tk( parse, TK_USING );
   p_read_tk( parse );
   dirc->path = p_read_path( parse );
   if ( parse->tk == TK_COLON ) {
      p_read_tk( parse );
      while ( true ) {
         read_using_item( parse, dirc );
         if ( parse->tk == TK_COMMA ) {
            p_read_tk( parse );
         }
         else {
            break;
         }
      }
      dirc->type = USING_SELECTION;
   }
   p_test_tk( parse, TK_SEMICOLON );
   p_read_tk( parse );
   list_append( output, dirc );
}

void p_read_local_using( struct parse* parse, struct list* output ) {
   read_using( parse, output, p_read_let( parse ) );
}

struct using_dirc* alloc_using( struct pos* pos ) {
   struct using_dirc* dirc = mem_alloc( sizeof( *dirc ) );
   dirc->node.type = NODE_USING;
   dirc->path = NULL;
   list_init( &dirc->items );
   dirc->pos = *pos;
   dirc->type = USING_ALL;
   dirc->force_local_scope = false;
   return dirc;
}

void read_using_item( struct parse* parse, struct using_dirc* dirc ) {
   struct using_item* item = alloc_using_item();
   item->name = parse->tk_text;
   item->pos = parse->tk_pos;
   bool type_alias = false;
   switch ( parse->tk ) {
   case TK_STRUCT:
      p_read_tk( parse );
      if ( parse->tk != TK_TYPENAME ) {
         p_test_tk( parse, TK_ID );
      }
      item->name = parse->tk_text;
      item->type = USINGITEM_STRUCT;
      p_read_tk( parse );
      break;
   case TK_ENUM:
      p_read_tk( parse );
      if ( parse->tk != TK_TYPENAME ) {
         p_test_tk( parse, TK_ID );
      }
      item->name = parse->tk_text;
      item->type = USINGITEM_ENUM;
      p_read_tk( parse );
      break;
   case TK_TYPENAME:
      type_alias = true;
      p_read_tk( parse );
      break;
   default:
      p_test_tk( parse, TK_ID );
      p_read_tk( parse );
   }
   item->usage_name = item->name;
   if ( parse->tk == TK_ASSIGN ) {
      p_read_tk( parse );
      if ( type_alias ) {
         p_test_tk( parse, TK_TYPENAME );
      }
      else if (
         item->type == USINGITEM_ENUM ||
         item->type == USINGITEM_STRUCT ) {
         if ( parse->tk != TK_TYPENAME ) {
            p_test_tk( parse, TK_ID );
         }
      }
      else {
         p_test_tk( parse, TK_ID );
      }
      item->name = parse->tk_text;
      p_read_tk( parse );
   }
   list_append( &dirc->items, item );
}

struct using_item* alloc_using_item( void ) {
   struct using_item* item = mem_alloc( sizeof( *item ) );
   item->name = NULL;
   item->usage_name = NULL;
   item->alias = NULL;
   t_init_pos_id( &item->pos, ALTERN_FILENAME_COMPILER );
   item->type = USINGITEM_OBJECT;
   return item;
}

struct path* p_read_path( struct parse* parse ) {
   // Head of path.
   struct path* path = alloc_path( parse->tk_pos );
   if ( parse->tk == TK_UPMOST ) {
      path->upmost = true;
      p_read_tk( parse );
   }
   else if ( parse->tk == TK_NAMESPACE ) {
      path->current_ns = true;
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_ID );
      path->text = parse->tk_text;
      p_read_tk( parse );
   }
   // Tail of path.
   struct path* head = path;
   struct path* tail = head;
   while ( parse->tk == TK_DOT && p_peek( parse ) == TK_ID ) {
      p_read_tk( parse );
      p_test_tk( parse, TK_ID );
      path = alloc_path( parse->tk_pos );
      path->text = parse->tk_text;
      tail->next = path;
      tail = path;
      p_read_tk( parse );
   }
   return head;
}

struct path* alloc_path( struct pos pos ) {
   struct path* path = mem_alloc( sizeof( *path ) );
   path->next = NULL;
   path->text = "";
   path->pos = pos;
   path->upmost = false;
   path->current_ns = false;
   return path;
}

bool p_peek_type_path( struct parse* parse ) {
   if ( parse->tk == TK_TYPENAME ) {
      return true;
   }
   else if (
      parse->tk == TK_ID ||
      parse->tk == TK_UPMOST ||
      parse->tk == TK_NAMESPACE ) {
      struct parsertk_iter iter;
      p_init_parsertk_iter( parse, &iter );
      p_next_tk( parse, &iter );
      while ( iter.token->type == TK_DOT ) {
         p_next_tk( parse, &iter );
         if ( iter.token->type == TK_TYPENAME ) {
            return true;
         }
         else if ( iter.token->type == TK_ID ) {
            p_next_tk( parse, &iter );
         }
         else {
            break;
         }
      }
      return false;
   }
   else {
      return false;
   }
}

struct path* p_read_type_path( struct parse* parse ) {
   if ( parse->tk == TK_TYPENAME ) {
      struct path* path = alloc_path( parse->tk_pos );
      path->text = parse->tk_text;
      p_read_tk( parse );
      return path;
   }
   else {
      // Head.
      struct path* path = alloc_path( parse->tk_pos );
      if ( parse->tk == TK_UPMOST ) {
         path->upmost = true;
         p_read_tk( parse );
      }
      else if ( parse->tk == TK_NAMESPACE ) {
         path->current_ns = true;
         p_read_tk( parse );
      }
      else {
         p_test_tk( parse, TK_ID );
         path->text = parse->tk_text;
         p_read_tk( parse );
      }
      // Middle.
      struct path* head = path;
      struct path* tail = head;
      while ( parse->tk == TK_DOT && p_peek( parse ) == TK_ID ) {
         p_read_tk( parse );
         p_test_tk( parse, TK_ID );
         path = alloc_path( parse->tk_pos );
         path->text = parse->tk_text;
         tail->next = path;
         tail = path;
         p_read_tk( parse );
      }
      // Tail.
      p_test_tk( parse, TK_DOT );
      p_read_tk( parse );
      p_test_tk( parse, TK_TYPENAME );
      path = alloc_path( parse->tk_pos );
      path->text = parse->tk_text;
      tail->next = path;
      p_read_tk( parse );
      return head;
   }
}

void read_pseudo_dirc( struct parse* parse, bool first_object ) {
   struct pos pos = parse->tk_pos;
   p_test_tk( parse, TK_HASH );
   p_read_tk( parse );
   // Determine the directive to read.
   enum tk dirc = TK_NONE;
   switch ( parse->lang ) {
   case LANG_ACS:
      switch ( parse->tk ) {
      case TK_DEFINE:
      case TK_LIBDEFINE:
      case TK_INCLUDE:
      case TK_IMPORT:
      case TK_LIBRARY:
      case TK_ENCRYPTSTRINGS:
      case TK_NOCOMPACT:
      case TK_WADAUTHOR:
      case TK_NOWADAUTHOR:
      case TK_REGION:
      case TK_ENDREGION:
         dirc = parse->tk;
         break;
      default:
         break;
      }
      break;
   case LANG_ACS95:
      switch ( parse->tk ) {
      case TK_DEFINE:
      case TK_INCLUDE:
         dirc = parse->tk;
         break;
      default:
         break;
      }
      break;
   default:
      dirc = determine_bcs_pseudo_dirc( parse );
   }
   // Read directive.
   switch ( dirc ) {
   case TK_DEFINE:
   case TK_LIBDEFINE:
      read_define( parse );
      break;
   case TK_INCLUDE:
      read_include( parse );
      break;
   case TK_IMPORT:
      read_import( parse, &pos );
      break;
   case TK_LIBRARY:
      read_library( parse, &pos, first_object );
      break;
   case TK_LINKLIBRARY:
      read_linklibrary( parse, &pos );
      break;
   case TK_ENCRYPTSTRINGS:
      parse->lib->encrypt_str = true;
      p_read_tk( parse );
      break;
   case TK_NOCOMPACT:
      // NOTE: This restriction doesn't apply to our compiler, but keep it to
      // stay compatible with acc. 
      if ( parse->lang == LANG_ACS && (
         list_size( &parse->lib->scripts ) > 0 ||
         list_size( &parse->lib->funcs ) > 0 ) ) {
         p_diag( parse, DIAG_POS_ERR, &pos,
            "`%s` directive found after a script or a function",
            parse->tk_text );
         p_bail( parse );
      }
      parse->lib->format = FORMAT_BIG_E;
      p_read_tk( parse );
      break;
   case TK_WADAUTHOR:
   case TK_NOWADAUTHOR:
      parse->lib->wadauthor = ( dirc == TK_WADAUTHOR );
      p_read_tk( parse );
      break;
   case TK_REGION:
   case TK_ENDREGION:
      p_read_tk( parse );
      break;
   default:
      p_diag( parse, DIAG_POS_ERR, &pos,
         "unknown directive: %s", parse->tk_text );
      p_bail( parse );
   }
}

enum tk determine_bcs_pseudo_dirc( struct parse* parse ) {
   static const struct { const char* text; enum tk tk; } table[] = {
      { "define", TK_DEFINE },
      { "libdefine", TK_LIBDEFINE },
      { "include", TK_INCLUDE },
      { "import", TK_IMPORT },
      { "library", TK_LIBRARY },
      { "linklibrary", TK_LINKLIBRARY },
      { "encryptstrings", TK_ENCRYPTSTRINGS },
      { "nocompact", TK_NOCOMPACT },
      { "wadauthor", TK_WADAUTHOR },
      { "nowadauthor", TK_NOWADAUTHOR }
   };
   for ( int i = 0; i < ARRAY_SIZE( table ); ++i ) {
      if ( strcmp( parse->tk_text, table[ i ].text ) == 0 ) {
         return table[ i ].tk;
      }
   }
   return TK_NONE;
}

void read_include( struct parse* parse ) {
   p_test_tk( parse, parse->lang == LANG_BCS ? TK_ID : TK_INCLUDE );
   p_read_tk( parse );
   if ( parse->lib->imported ) {
      p_test_tk( parse, TK_LIT_STRING );
      p_read_tk( parse );
   }
   else {
      p_test_tk( parse, TK_LIT_STRING );
      p_load_included_source( parse, parse->tk_text, &parse->tk_pos );
      p_read_tk( parse );
   }
}

void read_define( struct parse* parse ) {
   bool hidden = ( parse->tk_text[ 0 ] == 'd' );
   p_read_tk( parse );
   // In BCS, true and false are keywords, but they are defined as constants in
   // zcommon.acs. To make the file work in BCS, ignore the #defines.
   if ( parse->lang == LANG_BCS ) {
      switch ( parse->tk ) {
      case TK_TRUE:
      case TK_FALSE:
         p_read_tk( parse );
         p_test_tk( parse, TK_LIT_DECIMAL );
         p_read_tk( parse );
         return;
      default:
         break;
      }
   }
   p_test_tk( parse, TK_ID );
   struct constant* constant = t_alloc_constant();
   constant->object.pos = parse->tk_pos;
   constant->name = t_extend_name( parse->ns->body, parse->tk_text );
   p_read_tk( parse );
   struct expr_reading value;
   p_init_expr_reading( &value, true, false, false, true );
   p_read_expr( parse, &value );
   constant->value_node = value.output_node;
   constant->hidden = hidden;
   p_add_unresolved( parse, &constant->object );
   list_append( &parse->lib->objects, constant );
   list_append( &parse->ns_fragment->objects, constant );
   if ( hidden ) {
      list_append( &parse->lib->private_objects, constant );
   }
}

void read_import( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, parse->lang == LANG_BCS ? TK_ID : TK_IMPORT );
   p_read_tk( parse );
   p_test_tk( parse, TK_LIT_STRING );
   struct import_dirc* dirc = mem_alloc( sizeof( *dirc ) );
   dirc->file_path = parse->tk_text;
   dirc->pos = parse->tk_pos;
   list_append( &parse->lib->import_dircs, dirc ); 
   p_read_tk( parse );
}

void read_library( struct parse* parse, struct pos* pos, bool first_object ) {
   p_test_tk( parse, parse->lang == LANG_BCS ? TK_ID : TK_LIBRARY );
   p_read_tk( parse );
   parse->lib->header = true;
   parse->lib->importable = true;
   if ( parse->tk == TK_LIT_STRING ) {
      read_library_name( parse, pos );
   }
   else {
      parse->lib->name_pos = *pos;
   }
   // In ACS, the #library header must be the first object in the module. In
   // BCS, it can appear anywhere.
   if ( parse->lang == LANG_ACS && ! first_object ) {
      p_diag( parse, DIAG_POS_ERR, &parse->tk_pos,
         "`library` directive not at the very top" );
      p_bail( parse );
   }
}

void read_library_name( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_LIT_STRING );
   test_library_name( parse, &parse->tk_pos, parse->tk_text,
      parse->tk_length );
   // Different #library directives in the same library must have the same
   // name.
   if ( parse->lib->name.length != 0 &&
      strcmp( parse->lib->name.value, parse->tk_text ) != 0 ) {
      p_diag( parse, DIAG_POS_ERR, pos,
         "library has multiple names" );
      p_diag( parse, DIAG_POS, &parse->lib->name_pos,
         "first name found here" );
      p_bail( parse );
   }
   // Each library must have a unique name.
   list_iter_t i;
   list_iter_init( &i, &parse->task->libraries );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      if ( lib != parse->lib &&
         strcmp( parse->tk_text, lib->name.value ) == 0 ) {
         p_diag( parse, DIAG_POS_ERR, pos,
            "duplicate library name" );
         p_diag( parse, DIAG_POS, &lib->name_pos,
            "library name previously found here" );
         p_bail( parse );
      }
      list_next( &i );
   }
   str_append( &parse->lib->name, parse->tk_text );
   parse->lib->name_pos = *pos;
   p_read_tk( parse );
}

void test_library_name( struct parse* parse, struct pos* pos,
   const char* name, int length ) {
   if ( length == 0 ) {
      p_diag( parse, DIAG_POS_ERR, pos,
         "library name is blank" );
      p_bail( parse );
   }
   if ( length > MAX_LIB_NAME_LENGTH ) {
      p_diag( parse, DIAG_WARN | DIAG_POS, pos,
         "library name too long" );
      p_diag( parse, DIAG_FILE, pos,
         "library name can be up to %d characters long",
         MAX_LIB_NAME_LENGTH );
   }
}

void read_linklibrary( struct parse* parse, struct pos* pos ) {
   p_test_tk( parse, TK_ID );
   p_read_tk( parse );
   p_test_tk( parse, TK_LIT_STRING );
   add_library_link( parse, parse->tk_text, pos );
   p_read_tk( parse );
}

void add_library_link( struct parse* parse, const char* name,
   struct pos* pos ) {
   test_library_name( parse, pos, name, strlen( name ) );
   struct library_link* link = NULL;
   list_iter_t i;
   list_iter_init( &i, &parse->lib->links );
   while ( ! list_end( &i ) ) {
      struct library_link* other_link = list_data( &i );
      if ( strcmp( other_link->name, name ) == 0 ) {
         link = other_link;
         break;
      }
      list_next( &i );
   }
   if ( link ) {
      p_diag( parse, DIAG_POS | DIAG_WARN, pos,
         "duplicate library link" );
      p_diag( parse, DIAG_POS, &link->pos,
         "library link previously found here" );
   }
   else {
      link = mem_alloc( sizeof( *link ) );
      link->name = name;
      link->needed = false;
      memcpy( &link->pos, pos, sizeof( *pos ) );
      list_append( &parse->lib->links, link );
   }
}

void p_create_cmdline_library_links( struct parse* parse ) {
   list_iter_t i;
   list_iter_init( &i, &parse->task->options->library_links );
   while ( ! list_end( &i ) ) {
      struct pos pos;
      t_init_pos_id( &pos, ALTERN_FILENAME_COMMANDLINE );
      add_library_link( parse, list_data( &i ), &pos );
      list_next( &i );
   }
}

void p_add_unresolved( struct parse* parse, struct object* object ) {
   t_append_unresolved_namespace_object( parse->ns_fragment, object );
}

void read_imported_libs( struct parse* parse ) {
   list_iter_t i;
   list_iter_init( &i, &parse->lib->import_dircs );
   while ( ! list_end( &i ) ) {
      import_lib( parse, list_data( &i ) );
      list_next( &i );
   }
}

void import_lib( struct parse* parse, struct import_dirc* dirc ) {
   struct file_query query;
   t_init_file_query( &query, parse->task->library_main->file,
      dirc->file_path );
   t_find_file( parse->task, &query );
   if ( ! query.file ) {
      p_diag( parse, DIAG_POS_ERR, &dirc->pos,
         "library not found: %s", dirc->file_path );
      p_bail( parse );
   }
   // See if the library is already loaded.
   struct library* lib = NULL;
   list_iter_t i;
   list_iter_init( &i, &parse->task->libraries );
   while ( ! list_end( &i ) ) {
      lib = list_data( &i );
      if ( lib->file == query.file ) {
         goto have_lib;
      }
      list_next( &i );
   }
   // Load library from cache.
   if ( parse->cache ) {
      lib = cache_get( parse->cache, query.file );
      if ( lib ) {
         goto have_lib;
      }
   }
   // Read library from source file.
   lib = t_add_library( parse->task );
   lib->lang = p_determine_lang_from_file_path( query.file->full_path.value );
   list_append( &parse->task->libraries, lib );
   if ( lib->lang == LANG_BCS && parse->lib->lang == LANG_ACS ) {
      p_diag( parse, DIAG_POS_ERR, &dirc->pos,
         "importing BCS library into ACS library" );
      p_bail( parse );
   }
   parse->lang = lib->lang;
   parse->lang_limits = t_get_lang_limits( lib->lang );
   parse->lib = lib;
   p_load_imported_lib_source( parse, dirc, query.file );
   parse->ns_fragment = lib->upmost_ns_fragment;
   parse->ns = parse->ns_fragment->ns;
   p_clear_macros( parse );
   p_define_imported_macro( parse );
   p_read_tk( parse );
   read_module( parse );
   parse->lib = parse->task->library_main;
   // An imported library must have a #library directive.
   if ( ! lib->header ) {
      p_diag( parse, DIAG_POS_ERR, &dirc->pos,
         "imported library missing #library directive" );
      p_bail( parse );
   }
   if ( parse->cache ) {
      cache_add( parse->cache, lib );
   }
   have_lib:
   lib->imported = true;
   // Append library.
   list_iter_init( &i, &parse->task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* another_lib = list_data( &i );
      if ( lib == another_lib ) {
         p_diag( parse, DIAG_WARN | DIAG_POS, &dirc->pos,
            "duplicate import of library" );
         return;
      }
      list_next( &i );
   }
   list_append( &parse->task->library_main->dynamic, lib );
   if ( lib->lang == LANG_ACS ) {
      list_append( &parse->task->library_main->dynamic_acs, lib );
   }
   else {
      list_append( &parse->task->library_main->dynamic_bcs, lib );
   }
}

void determine_needed_library_links( struct parse* parse ) {
   list_iter_t i;
   list_iter_init( &i, &parse->task->library_main->links );
   while ( ! list_end( &i ) ) {
      struct library_link* link = list_data( &i );
      // Ignore a self-referential link.
      if ( strcmp( link->name, parse->task->library_main->name.value ) != 0 ) {
         // If a library is #imported, a link for it is not necessary.
         list_iter_t k;
         list_iter_init( &k, &parse->task->library_main->dynamic );
         while ( ! list_end( &k ) ) {
            struct library* lib = list_data( &k );
            if ( strcmp( link->name, lib->name.value ) == 0 ) {
               break;
            }
            list_next( &k );
         }
         if ( list_end( &k ) ) {
            link->needed = true;
         }
      }
      list_next( &i );
   }
}