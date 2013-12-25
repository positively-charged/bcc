#include <stdio.h>
#include <setjmp.h>

#include "task.h"

static jmp_buf g_bail;

static void init_options( struct options* );
static bool read_options( struct options*, char** );

int main( int argc, char* argv[] ) {
   int result = EXIT_FAILURE;
   struct options options;
   init_options( &options );
   if ( ! read_options( &options, argv ) ) {
      goto deinit_mem;
   }
   struct task task;
   t_init( &task, &options );
   if ( ! t_load_file( &task, options.source_file ) ) {
      printf( "error: failed to load source file: %s\n",
         options.source_file );
      goto deinit_mem;
   }
   if ( setjmp( g_bail ) == 0 ) {
      t_read_tk( &task );
      t_read_module( &task );
      t_test( &task );
      t_publish( &task );
      result = EXIT_SUCCESS;
   }
   deinit_mem:
   mem_deinit();
   return result;
}

void init_options( struct options* options ) {
   list_init( &options->includes );
   options->source_file = NULL;
   options->object_file = "object.o";
   options->encrypt_str = false;
   options->err_file = false;
}

bool read_options( struct options* options, char** args ) {
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
            list_append( &options->includes, *args );
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

void bail( void ) {
   longjmp( g_bail, 1 );
}