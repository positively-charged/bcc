#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "task.h"

#define TAB_SIZE_MIN 1
#define TAB_SIZE_MAX 100

static void init_options( struct options* );
static bool read_options( struct options*, char** );

int main( int argc, char* argv[] ) {
   int result = EXIT_FAILURE;
   struct options options;
   init_options( &options );
   if ( ! read_options( &options, argv ) ) {
      goto deinit_mem;
   }
   jmp_buf bail;
   struct task task;
   t_init( &task, &options, &bail );
   if ( setjmp( bail ) == 0 ) {
      t_read( &task );
      t_test( &task );
      t_publish( &task );
      result = EXIT_SUCCESS;
   }
   if ( task.err_file ) {
      fclose( task.err_file );
   }
   deinit_mem:
   mem_free_all();
   return result;
}

void init_options( struct options* options ) {
   list_init( &options->includes );
   options->source_file = NULL;
   options->object_file = "object.o";
   // Default tab size will be 4 for now, since it's a common indentation size.
   options->tab_size = 4;
   options->encrypt_str = false;
   options->acc_err = false;
   options->one_column = false;
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
      if ( strcmp( option, "i" ) == 0 || strcmp( option, "I" ) == 0 ) {
         if ( *args ) {
            list_append( &options->includes, *args );
            ++args;
         }
         else {
            printf( "error: missing path for include-path option\n" );
            return false;
         }
      }
      else if ( strcmp( option, "tabsize" ) == 0 ) {
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
      else if ( strcmp( option, "onecolumn" ) == 0 ) {
         options->one_column = true;
      }
      else if ( strcmp( option, "accerrfile" ) == 0 ) {
         options->acc_err = true;
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