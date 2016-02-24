#include <stdarg.h>

#include "phase.h"
#include "pcode.h"

static void add_pcd( struct codegen* codegen, int code, va_list* args );
static void add_pushbytes( struct codegen* codegen, va_list* args );
static void add_casegotosorted( struct codegen* codegen, va_list* args );

void c_pcd( struct codegen* codegen, int code, ... ) {
   va_list args;
   va_start( args, code );
   switch ( code ) {
   case PCD_PUSHBYTES:
      add_pushbytes( codegen, &args );
      break;
   case PCD_CASEGOTOSORTED:
      add_casegotosorted( codegen, &args );
      break;
   default:
      add_pcd( codegen, code, &args );
      break;
   }
   va_end( args );
}

void add_pushbytes( struct codegen* codegen, va_list* args ) {
   int count = va_arg( *args, int );
   if ( count > 0 ) {
      c_add_opc( codegen, PCD_PUSHBYTES );
      c_add_arg( codegen, count );
      for ( int i = 0; i < count; i ) {
         c_add_arg( codegen, va_arg( *args, int ) );
      }
   }
}

void add_casegotosorted( struct codegen* codegen, va_list* args ) {
   int count = va_arg( *args, int );
   if ( count > 0 ) {
      c_add_opc( codegen, PCD_CASEGOTOSORTED );
      c_add_arg( codegen, count );
      for ( int i = 0; i < count; i ) {
         c_add_arg( codegen, va_arg( *args, int ) );
         c_add_arg( codegen, va_arg( *args, int ) );
      }
   }
}

void add_pcd( struct codegen* codegen, int code, va_list* args ) {
   c_add_opc( codegen, code );
   struct pcode* pcode = c_get_pcode_info( code );
   for ( int i = 0; i < pcode->argc; i ) {
      c_add_arg( codegen, va_arg( *args, int ) );
   }
}
