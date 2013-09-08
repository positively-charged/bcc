#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "mem.h"
#include "common.h"
#include "f_main.h"
#include "b_main.h"
#include "tools/ast_view.h"

static bool read_options( options_t*, char** );

int main( int argc, char* argv[] ) {
   int result = EXIT_FAILURE;

   mem_init();

   options_t options;
   job_init_options( &options );
   if ( ! read_options( &options, argv ) ) {
      goto deinit_mem;
   }

   file_table_t file_table;
   job_init_file_table( &file_table, &options.include_paths,
      options.source_file );

   ntbl_t name_table;
   ntbl_init( &name_table );

   ast_t ast;
   job_init_ast( &ast, &name_table );

   front_t front;
   f_init( &front, &options, &name_table, &file_table, &ast );
   if ( tk_load_file( &front, options.source_file ) != k_tk_file_err_none ) {
      printf( "error: failed to open source file: %s\n",
         options.source_file );
      goto deinit_file_table;
   }

   if ( p_build_tree( &front ) ) {
      //mem_temp_reset();
      ast_view_t view;
      ast_view_init( &view, &ast );
      ast_view_show( &view );
      //b_publish( &ast, &options, options.object_file );
      result = EXIT_SUCCESS;
   }

   deinit:
   ntbl_deinit( &name_table );
   f_deinit( &front );

   deinit_file_table:
   job_deinit_file_table( &file_table );

   deinit_mem:
   mem_deinit();

   return result;
}

bool read_options( options_t* options, char** args ) {
   // Program path not needed.
   ++args;
   while ( true ) {
      const char* option = *args;
      if ( option && *option == '-' ) {
         ++option;
         ++args;
      }
      else {
         break;
      }
      // For now, only look at the first character of the option.
      switch ( *option ) {
      case 'i':
      case 'I':
         if ( *args ) {
            list_append( &options->include_paths, *args );
            ++args;
         }
         else {
            printf( "error: missing path for include-path option\n" );
            return false;
         }
         break;
      case 'e':
         options->err_file = true;
         break;
      default:
         printf( "error: unknown option: %c\n", *option );
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