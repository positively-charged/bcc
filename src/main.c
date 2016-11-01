#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "task.h"
#include "parse/phase.h"
#include "semantic/phase.h"
#include "codegen/phase.h"
#include "cache/cache.h"

#define TAB_SIZE_MIN 1
#define TAB_SIZE_MAX 100

static void init_options( struct options* );
static bool read_options( struct options*, char** );
static void strip_rslash( char* );
static bool source_object_files_same( struct options* );
static void print_usage( char* );
static bool perform_action( struct options* options, jmp_buf* root_bail );
static void perform_task( struct task* task );
static void perform_selected_task( struct task* task, struct cache* cache );
static void print_cache( struct task* task, struct cache* cache );
static void clear_cache( struct task* task, struct cache* cache );
static void preprocess( struct task* task );
static void compile_mainlib( struct task* task, struct cache* cache );
static void print_acc_stats( struct task* task, struct parse* parse,
   struct codegen* codegen );
static void print_acc_stats_acs( struct task* task, struct parse* parse,
   struct codegen* codegen );
static void print_acc_stats_bcs( struct task* task, struct parse* parse,
   struct codegen* codegen );
static const char* get_script_type_label( int type );

int main( int argc, char* argv[] ) {
   int result = EXIT_FAILURE;
   mem_init();
   // When no options are given, show the help information.
   if ( argc == 1 ) {
      print_usage( argv[ 0 ] );
      result = EXIT_SUCCESS;
      goto deinit_mem;
   }
   struct options options;
   init_options( &options );
   if ( read_options( &options, argv ) ) {
      // No need to continue when help is requested.
      if ( options.help ) {
         print_usage( argv[ 0 ] );
         result = EXIT_SUCCESS;
         goto deinit_mem;
      }
   }
   else {
      goto deinit_mem;
   }
   // Add directory of compiler as search path.
   struct str compiler_dir;
   str_init( &compiler_dir );
   if ( c_read_full_path( argv[ 0 ], &compiler_dir ) ) {
      c_extract_dirname( &compiler_dir );
      list_append( &options.includes, compiler_dir.value );
   }
   // When no object file is explicitly specified, create the object file in
   // the directory of the source file, giving it the name that of the source
   // file, but with ".o" extension.
   struct str object_file;
   str_init( &object_file );
   if ( ! options.object_file ) {
      str_append( &object_file, options.source_file );
      int i = 0;
      int length = object_file.length;
      while ( object_file.value[ i ] ) {
         if ( object_file.value[ i ] == '.' ) {
            length = i;
         }
         ++i;
      }
      object_file.length = length;
      str_append( &object_file, ".o" );
      options.object_file = object_file.value;
   }
   // Don't overwrite the source file.
   if ( source_object_files_same( &options ) ) {
      printf( "error: trying to overwrite source file\n" );
      printf( "source file: %s\n", options.source_file );
      printf( "object file: %s\n", options.object_file );
      goto deinit_object_file;
   }
   jmp_buf bail;
   if ( setjmp( bail ) == 0 ) {
      if ( perform_action( &options, &bail ) ) {
         result = EXIT_SUCCESS;
      }
   }
   deinit_object_file:
   str_deinit( &compiler_dir );
   str_deinit( &object_file );
   deinit_mem:
   mem_free_all();
   return result;
}

void init_options( struct options* options ) {
   list_init( &options->includes );
   list_init( &options->defines );
   list_init( &options->library_links );
   options->source_file = NULL;
   options->object_file = NULL;
   // Default tab size for now is 4, since it's a common indentation size.
   options->tab_size = 4;
   options->lang = LANG_ACS;
   options->acc_err = false;
   options->acc_stats = false;
   options->one_column = false;
   options->help = false;
   options->preprocess = false;
   options->write_asserts = true;
   options->lang_specified = false;
   options->cache.dir_path = NULL;
   options->cache.lifetime = -1;
   options->cache.enable = false;
   options->cache.clear = false;
   options->cache.print = false;
}

bool read_options( struct options* options, char** argv ) {
   char** args = argv + 1;
   // Process the help option first.
   while ( *args ) {
      char* option = *args;
      if ( *option == '-' && strcmp( option + 1, "h" ) == 0 ) {
         options->help = true;
         return true;
      }
      ++args;
   }
   // Process rest of the options.
   args = argv + 1;
   while ( true ) {
      // Select option.
      const char* option_arg = *args;
      const char* option = option_arg;
      if ( option && *option == '-' ) {
         ++option;
         ++args;
      }
      else {
         break;
      }
      // Process option.
      if (
         strcmp( option, "i" ) == 0 ||
         strcmp( option, "I" ) == 0 ) {
         if ( *args ) {
            strip_rslash( *args );
            list_append( &options->includes, *args );
            ++args;
         }
         else {
            printf( "error: missing directory path in %s option\n",
               option_arg );
            return false;
         }
      }
      else if ( strcmp( option, "tab-size" ) == 0 ) {
         if ( *args ) {
            int size = atoi( *args );
            // Set some limits on the size.
            if ( size >= TAB_SIZE_MIN && size <= TAB_SIZE_MAX ) {
               options->tab_size = size;
               ++args;
            }
            else {
               printf( "error: tab size not between %d and %d\n",
                  TAB_SIZE_MIN, TAB_SIZE_MAX );
               return false;
            }
         }
         else {
            printf( "error: missing tab size\n" );
            return false;
         }
      }
      else if ( strcmp( option, "one-column" ) == 0 ) {
         options->one_column = true;
      }
      else if ( strcmp( option, "acc-err" ) == 0 ||
         // Deprecated name:
         strcmp( option, "acc-err-file" ) == 0 ) {
         options->acc_err = true;
      }
      else if ( strcmp( option, "acc-stats" ) == 0 ) {
         options->acc_stats = true;
      }
      else if ( strcmp( option, "cache" ) == 0 ) {
         options->cache.enable = true;
      }
      else if ( strcmp( option, "cache-dir" ) == 0 ) {
         if ( *args ) {
            options->cache.dir_path = *args;
            ++args;
         }
         else {
            printf( "error: missing directory path for %s option\n",
               option );
            return false;
         }
      }
      else if ( strcmp( option, "cache-lifetime" ) == 0 ) {
         if ( *args ) {
            options->cache.lifetime = atoi( *args );
            ++args;
         }
         else {
            printf( "error: missing duration argument for %s option\n",
               option );
            return false;
         }
      }
      else if ( strcmp( option, "cache-print" ) == 0 ) {
         options->cache.print = true;
      }
      else if ( strcmp( option, "cache-clear" ) == 0 ) {
         options->cache.clear = true;
      }
      else if ( strcmp( option, "E" ) == 0 ) {
         options->preprocess = true;
      }
      else if ( strcmp( option, "strip-asserts" ) == 0 ) {
         options->write_asserts = false;
      }
      else if ( strcmp( option, "D" ) == 0 ) {
         if ( *args ) {
            list_append( &options->defines, *args );
            ++args;
         }
         else {
            printf( "error: missing macro argument for %s option\n",
               option );
            return false;
         }
      }
      else if ( strcmp( option, "x" ) == 0 ) {
         options->lang_specified = true;
         if ( *args ) {
            if ( strcmp( *args, "acs" ) == 0 ) {
               options->lang = LANG_ACS;
               ++args;
            }
            else if ( strcmp( *args, "acs95" ) == 0 ) {
               options->lang = LANG_ACS95;
               ++args;
            }
            else if ( strcmp( *args, "bcs" ) == 0 ) {
               options->lang = LANG_BCS;
               ++args;
            }
            else {
               printf( "error: unsupported language: %s\n", *args );
               return false;
            }
         }
         else {
            printf( "error: missing language argument for %s option\n",
               option );
            return false;
         }
      }
      else if ( strcmp( option, "l" ) == 0 ) {
         if ( *args ) {
            list_append( &options->library_links, *args );
            ++args;
         }
         else {
            printf( "error: missing library link argument for %s option\n",
               option );
            return false;
         }
      }
      else {
         printf( "error: unknown option: %s\n", option );
         return false;
      }
   }
   if ( *args ) {
      options->source_file = *args;
      ++args;
   }
   else {
      printf( "error: missing file to compile\n" );
      return false;
   }
   // Output file.
   if ( *args ) {
      options->object_file = *args;
   }
   return true;
}

void strip_rslash( char* ch ) {
   int i = strlen( ch ) - 1;
   while ( i >= 0 && ( ch[ i ] == '/' || ch[ i ] == '\\' ) ) {
      ch[ i ] = 0;
      --i;
   }
}

bool source_object_files_same( struct options* options ) {
   struct fileid source;
   struct fileid object;
   if (
      ! c_read_fileid( &source, options->source_file ) ||
      ! c_read_fileid( &object, options->object_file ) ) {
      return false;
   }
   return c_same_fileid( &source, &object );
}

void print_usage( char* path ) {
   printf(
      "Usage: %s [options] <source-file> [object-file]\n"
      "Options: \n"
      "  -acc-err             On error, create an error file like one\n"
      "                       created by the acc compiler\n"
      "  -acc-stats           Show compilation statistics like those shown\n"
      "                       by the acc compiler\n"
      "  -h                   Show this help information\n"
      "  -i <directory>       Add a directory to search in for files\n"
      "  -I <directory>       Same as -i\n"
      "  -one-column          Start column position at 1. Default is 0\n"
      "  -tab-size <size>     Specify the width of the tab character\n"
      "  -strip-asserts       Do not include asserts in object file\n"
      "                       (asserts will not be executed at run-time)\n"
      "  -E                   Do preprocessing only\n"
      "  -D <name>            Create a macro with the specified name. The\n"
      "                       macro will have a value of 1\n"
      "  -x <language>        Specify the language of the source file. The\n"
      "                       language must be one of the following:\n"
      "                         acs\n"
      "                         acs95\n"
      "                         bcs\n"
      "  -l <library>         Creates a link to the specified library\n"
      "Cache options:\n"
      "  -cache               Enable caching of library files\n"
      "  -cache-dir           Store cache-related files in the specified\n"
      "    <directory>        directory\n"
      "                       (If one is not specified, a system-default\n"
      "                       temporary directory will be used)\n"
      "  -cache-lifetime      Keep a library cached for the specified\n"
      "    <duration>         duration, in hours\n"
      "  -cache-print         Show the contents of the cache\n"
      "  -cache-clear         Delete all cached library files\n",
      path );
}

bool perform_action( struct options* options, jmp_buf* root_bail ) {
   bool success = false;
   struct task task;
   t_init( &task, options, root_bail );
   jmp_buf bail;
   if ( setjmp( bail ) == 0 ) {
      task.bail = &bail;
      perform_task( &task );
      success = true;
   }
   task.bail = root_bail;
   t_deinit( &task );
   return success;
}

void perform_task( struct task* task ) {
   if ( task->options->cache.enable ) {
      struct cache cache;
      cache_init( &cache, task );
      cache_load( &cache );
      perform_selected_task( task, &cache );
      cache_close( &cache );
   }
   else {
      perform_selected_task( task, NULL );
   }
}

void perform_selected_task( struct task* task, struct cache* cache ) {
   if ( task->options->cache.print ) {
      print_cache( task, cache );
   }
   else if ( task->options->cache.clear ) {
      clear_cache( task, cache );
   }
   else if ( task->options->preprocess ) {
      preprocess( task );
   }
   else {
      compile_mainlib( task, cache );
   }
}

void print_cache( struct task* task, struct cache* cache ) {
   if ( cache ) {
      cache_print( cache );
   }
   else {
      t_diag( task, DIAG_ERR,
         "attempting to print cache, but cache is not enabled" );
      t_bail( task );
   }
}

void clear_cache( struct task* task, struct cache* cache ) {
   if ( cache ) {
      cache_clear( cache );
   }
   else {
      t_diag( task, DIAG_ERR,
         "attempting to clear cache, but cache is not enabled" );
      t_bail( task );
   }
}

void preprocess( struct task* task ) {
   struct parse parse;
   p_init( &parse, task, NULL );
   p_run( &parse );
}

void compile_mainlib( struct task* task, struct cache* cache ) {
   struct parse parse;
   p_init( &parse, task, cache );
   p_run( &parse );
   struct semantic semantic;
   s_init( &semantic, task );
   s_test( &semantic );
   struct codegen codegen;
   c_init( &codegen, task );
   c_publish( &codegen );
   if ( task->options->acc_stats ) {
      print_acc_stats( task, &parse, &codegen );
   }
}

void print_acc_stats( struct task* task, struct parse* parse,
   struct codegen* codegen ) {
   switch ( parse->lang ) {
   case LANG_ACS95:
      print_acc_stats_acs( task, parse, codegen );
      break;
   default:
      print_acc_stats_bcs( task, parse, codegen );
      break;
   }
}

void print_acc_stats_acs( struct task* task, struct parse* parse,
   struct codegen* codegen ) {
   t_diag( task, DIAG_NONE,
      "\"%s\":\n"
      "  %d line%s (%d included)\n"
      "  %d script%s"
      "",
      task->library_main->file->path.value,
      parse->main_lib_lines,
      parse->main_lib_lines == 1 ? "" : "s",
      parse->included_lines,
      list_size( &task->library_main->scripts ),
      list_size( &task->library_main->scripts ) == 1 ? "" : "s"
   );
   int closed_scripts = 0;
   int open_scripts = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      switch ( script->type ) {
      case SCRIPT_TYPE_CLOSED:
         ++closed_scripts;
         break;
      case SCRIPT_TYPE_OPEN:
         ++open_scripts;
         break;
      default:
         break;
      }
      list_next( &i );
   }
   if ( closed_scripts > 0 ) {
      t_diag( task, DIAG_NONE, "    %d closed", closed_scripts );
   }
   if ( open_scripts > 0 ) {
      t_diag( task, DIAG_NONE, "    %d open", open_scripts );
   }
   int map_vars = 0;
   int world_vars = 0;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      switch ( var->storage ) {
      case STORAGE_MAP:
         ++map_vars;
         break;
      case STORAGE_WORLD:
         ++world_vars;
         break;
      default:
         break;
      }
      list_next( &i );
   }
   t_diag( task, DIAG_NONE,
      "  %d world variable%s\n"
      "  %d map variable%s"
      "",
      world_vars, world_vars == 1 ? "" : "s",
      map_vars, map_vars == 1 ? "" : "s"
   );
   t_diag( task, DIAG_NONE,
      "  object \"%s\": %d bytes",
      task->options->object_file,
      codegen->object_size );
}

void print_acc_stats_bcs( struct task* task, struct parse* parse,
   struct codegen* codegen ) {
   // acc includes imported functions in the function count. This can cause
   // confusion. We, instead, have two counts: one for functions in the library
   // being compiled, and another for imported functions.
   int imported_funcs = 0;
   list_iter_t i;
   list_iter_init( &i, &task->library_main->dynamic );
   while ( ! list_end( &i ) ) {
      struct library* lib = list_data( &i );
      list_iter_t k;
      list_iter_init( &k, &lib->funcs );
      while ( ! list_end( &k ) ) {
         struct func* func = list_data( &k );
         struct func_user* impl = func->impl;
         imported_funcs += ( int ) ( impl->usage > 0 );
         list_next( &k );
      }
      list_next( &i );
   }
   t_diag( task, DIAG_NONE,
      "\"%s\":\n"
      "  %d line%s (%d included)\n"
      "  %d function%s (%d imported)\n"
      "  %d script%s"
      "",
      task->library_main->file->path.value,
      parse->main_lib_lines,
      parse->main_lib_lines == 1 ? "" : "s",
      parse->included_lines,
      list_size( &task->library_main->funcs ),
      list_size( &task->library_main->funcs ) == 1 ? "" : "s",
      imported_funcs,
      list_size( &task->library_main->scripts ),
      list_size( &task->library_main->scripts ) == 1 ? "" : "s"
   );
   int script_counts[ SCRIPT_TYPE_TOTAL ] = { 0 };
   list_iter_init( &i, &task->library_main->scripts );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      ++script_counts[ script->type ];
      list_next( &i );
   }
   for ( int i = 0; i < ARRAY_SIZE( script_counts ); ++i ) {
      if ( script_counts[ i ] > 0 ) {
         t_diag( task, DIAG_NONE, "    %d %s", script_counts[ i ],
            get_script_type_label( i ) );
      }
   }
   int map_vars = 0;
   int world_vars = 0;
   int world_arrays = 0;
   int global_vars = 0;
   int global_arrays = 0;
   list_iter_init( &i, &task->library_main->vars );
   while ( ! list_end( &i ) ) {
      struct var* var = list_data( &i );
      switch ( var->storage ) {
      case STORAGE_MAP:
         ++map_vars;
         break;
      case STORAGE_WORLD:
         if ( var->desc == DESC_ARRAY ) {
            ++world_arrays;
         }
         else {
            ++world_vars;
         }
         break;
      case STORAGE_GLOBAL:
         if ( var->desc == DESC_ARRAY ) {
            ++global_arrays;
         }
         else {
            ++global_vars;
         }
         break;
      default:
         break;
      }
      list_next( &i );
   }
   t_diag( task, DIAG_NONE,
      "  %d global variable%s\n"
      "  %d world variable%s\n"
      "  %d map variable%s\n"
      "  %d global array%s\n"
      "  %d world array%s"
      "",
      global_vars, global_vars == 1 ? "" : "s",
      world_vars, world_vars == 1 ? "" : "s",
      map_vars, map_vars == 1 ? "" : "s",
      global_arrays, global_arrays == 1 ? "" : "s",
      world_arrays, world_arrays == 1 ? "" : "s"
   );
   t_diag( task, DIAG_NONE,
      "  object \"%s\": %d bytes",
      task->options->object_file,
      codegen->object_size );
}

const char* get_script_type_label( int type ) {
   STATIC_ASSERT( SCRIPT_TYPE_NEXTFREENUMBER == SCRIPT_TYPE_REOPEN + 1 );
   switch ( type ) {
   case SCRIPT_TYPE_CLOSED: return "closed";
   case SCRIPT_TYPE_OPEN: return "open";
   case SCRIPT_TYPE_RESPAWN: return "respawn";
   case SCRIPT_TYPE_DEATH: return "death";
   case SCRIPT_TYPE_ENTER: return "enter";
   case SCRIPT_TYPE_PICKUP: return "pickup";
   case SCRIPT_TYPE_BLUERETURN: return "bluereturn";
   case SCRIPT_TYPE_REDRETURN: return "redreturn";
   case SCRIPT_TYPE_WHITERETURN: return "whitereturn";
   case SCRIPT_TYPE_LIGHTNING: return "lightning";
   case SCRIPT_TYPE_UNLOADING: return "unloading";
   case SCRIPT_TYPE_DISCONNECT: return "disconnect";
   case SCRIPT_TYPE_RETURN: return "return";
   case SCRIPT_TYPE_EVENT: return "event";
   case SCRIPT_TYPE_KILL: return "kill";
   case SCRIPT_TYPE_REOPEN: return "reopen";
   default: return "";
   }
}