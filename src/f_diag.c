#include <stdio.h>
#include <stdarg.h>

#include "f_main.h"

#define SEVERITY 0x7

//static void show_scope( front_t*, FILE*, position_t* );
static void show_message( front_t*, FILE*, int, position_t*, va_list* );
static void show_message_acs( FILE*, int, position_t*, va_list* );

void f_diag( front_t* front, int flags, position_t* pos, const char* format,
   ... ) {
   va_list args;
   va_start( args, format );
   position_t default_pos = { NULL, 0, 0 };
   if ( ! pos ) {
      // For those messages that don't accept a position argument, display the
      // latest file.
      default_pos.file = job_active_file( front->file_table );
      pos = &default_pos;
   }
   if ( front->err_file ) {
      // TODO: Report error on failure to open.
      if ( ! front->fh ) {
         front->fh = fopen( "acs.err", "w" );
      }
      va_list file_args;
      va_copy( file_args, args );
      //show_scope( msg, msg->fh, pos );
      // show_message_acs( front->fh, id, pos, &file_args );
      va_end( file_args );
   }
   //show_scope( msg, stdout, pos );
   if ( pos->file ) {
      fprintf( stdout, "%s:", pos->file->user_path.value );
   }
   if ( flags & DIAG_LINE ) {
      fprintf( stdout, "%d:", pos->line );
      // Showing the column without showing the line doesn't make sense, so
      // only show the column if the line is shown.
      if ( flags & DIAG_COLUMN ) {
         fprintf( stdout, "%d:", pos->column );
      }
   }
   switch ( flags & SEVERITY ) {
   case DIAG_ERR:
      fprintf( stdout, " error: " );
      ++front->num_errs;
      break;
   case DIAG_WARNING:
      fprintf( stdout, " warning: " );
      break;
   case DIAG_NOTICE:
      fprintf( stdout, " notice: " );
      break;
   default:
      fprintf( stdout, " " );
      break;
   }
   vfprintf( stdout, format, args );
   fprintf( stdout, "\n" );
   va_end( args );
   //msg->scope->shown = true;
}


/*
typedef struct {
   enum {
      k_scope_global,
      k_scope_script,
      k_scope_func
   } type;
   int number;
   struct nkey_t* name;
   bool shown;
} scope_t;
void show_scope( front_t* msg, FILE* stream, position_t* pos ) {
   if ( msg->scope->shown ) { return; }
   switch ( msg->scope->type ) {
   case k_scope_script:
      fprintf( stream, "%s: in script %d:\n",
         pos->file->user_path.value,
         msg->scope->number );
      break;
   case k_scope_func: {
      char name[ 128 ];
      fprintf( stream, "%s: in function '%s':\n",
         pos->file->user_path.value,
         a_ntbl_save( msg->scope->name, name, 128 ) );
   }  break;
   default:
      break;
   }
}

void show_message( front_t* msg, FILE* stream, int id, position_t* pos,
   va_list* args ) {
   const message_t* message = &message_list[ id ];
   fprintf( stream, "%s:", pos->file->user_path.value );
   if ( message->show_line ) {
      fprintf( stream, "%d:", pos->line );
      // Showing the column without showing the line doesn't make sense, so
      // only show the column if the line is shown.
      if ( message->show_column ) {
         fprintf( stream, "%d:", pos->column );
      }
   }
   switch ( message->type ) {
   case k_message_error:
      fprintf( stream, " error: " );
      break;
   case k_message_warning:
      fprintf( stream, " warning: " );
      break;
   case k_message_notice:
      fprintf( stream, " notice: " );
      break;
   default:
      fprintf( stream, " " );
      break;
   }
   vfprintf( stream, message->content, *args );
   fprintf( stream, "\n" );
}

// Format: <file>:<line>: <message>
void show_message_acs( FILE* stream, int id, position_t* pos,
   va_list* args ) {
   const message_t* message = &message_list[ id ];
   fprintf( stream, "%s:", pos->file->user_path.value );
   if ( message->show_line ) {
      fprintf( stream, "%d:", pos->line );
   }
   switch ( message->type ) {
   case k_message_error:
      fprintf( stream, " error: " );
      break;
   case k_message_warning:
      fprintf( stream, " warning: " );
      break;
   case k_message_notice:
      fprintf( stream, " notice: " );
      break;
   default:
      fprintf( stream, " " );
      break;
   }
   vfprintf( stream, message->content, *args );
   fprintf( stream, "\n" );
}*/