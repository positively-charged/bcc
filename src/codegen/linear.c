#include <stdarg.h>

#include "phase.h"
#include "pcode.h"
#include "linear.h"

static void* alloc_node( struct codegen* codegen, int type );
static void free_node( struct codegen* codegen, struct c_node* node );
static void init_node( struct c_node* node, int type );
static void create_pcode( struct codegen* codegen, int code, bool optimize );
static void add_arg( struct codegen* codegen,
   struct c_point* point, int value );
static void add_pushbytes( struct codegen* codegen, va_list* args );
static void add_casegotosorted( struct codegen* codegen, va_list* args );
static void add_fixed( struct codegen* codegen, int code, va_list* args );
static void write_node( struct codegen* codegen, struct c_node* node );
static void write_pcode( struct codegen* codegen, struct c_pcode* pcode );
static void write_point( struct codegen* codegen, struct c_point* point );
static void write_jump( struct codegen* codegen, struct c_jump* jump );
static void write_casejump( struct codegen* codegen, struct c_casejump* jump );
static void write_sortedcasejump( struct codegen* codegen,
   struct c_sortedcasejump* sorted_jump );

static void* alloc_node( struct codegen* codegen, int type ) {
   if ( codegen->free_nodes[ type ] ) {
      struct c_node* node = codegen->free_nodes[ type ];
      codegen->free_nodes[ type ] = node->next;
      return node;
   }
   else {
      static size_t sizes[] = {
         sizeof( struct c_point ),
         sizeof( struct c_jump ),
         sizeof( struct c_casejump ),
         sizeof( struct c_sortedcasejump ),
         sizeof( struct c_pcode ),
      };
      return mem_alloc( sizes[ type ] );
   }
}

void c_seek_node( struct codegen* codegen, struct c_node* node ) {
   codegen->node = node;
}

void c_append_node( struct codegen* codegen, struct c_node* node ) {
   if ( codegen->node ) {
      node->next = codegen->node->next;
      codegen->node->next = node;
      if ( ! node->next ) {
         codegen->node_tail = node;
      }
   }
   else {
      codegen->node_head = node;
      codegen->node_tail = node;
   }
   codegen->node = node;
}

static void free_node( struct codegen* codegen, struct c_node* node ) {
   node->next = codegen->free_nodes[ node->type ];
   codegen->free_nodes[ node->type ] = node;
   if ( node->type == C_NODE_PCODE ) {
      struct c_pcode* pcode = ( struct c_pcode* ) node;
      struct c_pcode_arg* arg = pcode->args;
      while ( arg ) {
         struct c_pcode_arg* next_arg = arg->next;
         arg->next = codegen->free_pcode_args;
         codegen->free_pcode_args = arg;
         arg = next_arg;
      }
   }
}

static void init_node( struct c_node* node, int type ) {
   node->next = NULL;
   node->type = type;
}

struct c_point* c_create_point( struct codegen* codegen ) {
   struct c_point* point = alloc_node( codegen, C_NODE_POINT );
   init_node( &point->node, C_NODE_POINT );
   point->obj_pos = 0;
   return point;
}

struct c_jump* c_create_jump( struct codegen* codegen, int opcode ) {
   struct c_jump* jump = alloc_node( codegen, C_NODE_JUMP );
   init_node( &jump->node, C_NODE_JUMP );
   jump->opcode = opcode;
   jump->point = NULL;
   jump->next = NULL;
   jump->obj_pos = 0;
   return jump;
}

struct c_casejump* c_create_casejump( struct codegen* codegen, int value,
   struct c_point* point ) {
   struct c_casejump* jump = alloc_node( codegen, C_NODE_CASEJUMP );
   init_node( &jump->node, C_NODE_CASEJUMP );
   jump->next = NULL;
   jump->point = point;
   jump->value = value;
   jump->obj_pos = 0;
   return jump;
}

struct c_sortedcasejump* c_create_sortedcasejump( struct codegen* codegen ) {
   struct c_sortedcasejump* jump = alloc_node( codegen,
      C_NODE_SORTEDCASEJUMP );
   init_node( &jump->node, C_NODE_SORTEDCASEJUMP );
   jump->head = NULL;
   jump->tail = NULL;
   jump->count = 0;
   jump->obj_pos = 0;
   return jump;
}

void c_append_casejump( struct c_sortedcasejump* sorted_jump,
   struct c_casejump* jump ) {
   if ( sorted_jump->head ) {
      sorted_jump->tail->next = jump;
   }
   else {
      sorted_jump->head = jump;
   }
   sorted_jump->tail = jump;
   ++sorted_jump->count;
}

void c_opc( struct codegen* codegen, int code ) {
   create_pcode( codegen, code, true );
}

void c_unoptimized_opc( struct codegen* codegen, int code ) {
   create_pcode( codegen, code, false );
}

static void create_pcode( struct codegen* codegen, int code, bool optimize ) {
   struct c_pcode* pcode = alloc_node( codegen, C_NODE_PCODE );
   init_node( &pcode->node, C_NODE_PCODE );
   pcode->code = code;
   pcode->args = NULL;
   pcode->obj_pos = 0;
   pcode->optimize = optimize;
   pcode->patch = false;
   c_append_node( codegen, &pcode->node );
   codegen->pcode = pcode;
   codegen->pcodearg_tail = NULL;
}

void c_arg( struct codegen* codegen, int value ) {
   add_arg( codegen, NULL, value );
}

void c_point_arg( struct codegen* codegen, struct c_point* point ) {
   add_arg( codegen, point, 0 );
   codegen->pcode->patch = true;
}

static void add_arg( struct codegen* codegen, struct c_point* point,
   int value ) {
   struct c_pcode_arg* arg = codegen->free_pcode_args;
   if ( arg ) {
      codegen->free_pcode_args = arg->next;
   }
   else {
      arg = mem_alloc( sizeof( *arg ) );
   }
   arg->next = NULL;
   arg->point = point;
   arg->value = value;
   if ( codegen->pcodearg_tail ) {
      codegen->pcodearg_tail->next = arg;
   }
   else {
      codegen->pcode->args = arg;
   }
   codegen->pcodearg_tail = arg;
}

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
      add_fixed( codegen, code, &args );
      break;
   }
   va_end( args );
}

static void add_pushbytes( struct codegen* codegen, va_list* args ) {
   int count = va_arg( *args, int );
   if ( count > 0 ) {
      c_opc( codegen, PCD_PUSHBYTES );
      c_arg( codegen, count );
      for ( int i = 0; i < count; ++i ) {
         c_arg( codegen, va_arg( *args, int ) );
      }
   }
}

static void add_casegotosorted( struct codegen* codegen, va_list* args ) {
   int count = va_arg( *args, int );
   if ( count > 0 ) {
      c_opc( codegen, PCD_CASEGOTOSORTED );
      c_arg( codegen, count );
      for ( int i = 0; i < count; ++i ) {
         c_arg( codegen, va_arg( *args, int ) );
         c_arg( codegen, va_arg( *args, int ) );
      }
   }
}

static void add_fixed( struct codegen* codegen, int code, va_list* args ) {
   c_opc( codegen, code );
   struct pcode* pcode_info = c_get_pcode_info( code );
   for ( int i = 0; i < pcode_info->argc; ++i ) {
      c_arg( codegen, va_arg( *args, int ) );
   }
}

// ==========================================================================

void c_flush_pcode( struct codegen* codegen ) {
   struct c_node* node = codegen->node_head;
   while ( node ) {
      write_node( codegen, node );
      node = node->next;
   }
   // Patch address of jumps and delete nodes.
   node = codegen->node_head;
   while ( node ) {
      switch ( node->type ) {
         struct c_pcode* pcode;
      case C_NODE_JUMP:
      case C_NODE_CASEJUMP:
      case C_NODE_SORTEDCASEJUMP:
         write_node( codegen, node );
         break;
      case C_NODE_PCODE:
         pcode = ( struct c_pcode* ) node;
         if ( pcode->patch ) {
            write_pcode( codegen, pcode );
         }
         break;
      default:
         break;
       }
      // Free node.
      struct c_node* next_node = node->next; 
      free_node( codegen, node );
      node = next_node;
   }
   c_seek_end( codegen );
   codegen->node = NULL;
   codegen->node_head = NULL;
   codegen->node_tail = NULL;
}

static void write_node( struct codegen* codegen, struct c_node* node ) {
   switch ( node->type ) {
   case C_NODE_PCODE:
      write_pcode( codegen,
         ( struct c_pcode* ) node );
      break;
   case C_NODE_POINT:
      write_point( codegen,
         ( struct c_point* ) node );
      break;
   case C_NODE_JUMP:
      write_jump( codegen,
         ( struct c_jump* ) node );
      break;
   case C_NODE_CASEJUMP:
      write_casejump( codegen,
         ( struct c_casejump* ) node );
      break;
   case C_NODE_SORTEDCASEJUMP:
      write_sortedcasejump( codegen,
         ( struct c_sortedcasejump* ) node );
      break;
   default:
      break;
   }
}

static void write_pcode( struct codegen* codegen, struct c_pcode* pcode ) {
   if ( pcode->patch ) {
      if ( pcode->obj_pos ) {
         c_seek( codegen, pcode->obj_pos );
      }
      else {
         pcode->obj_pos = c_tell( codegen );
      }
   }
   if ( pcode->optimize ) {
      c_add_opc( codegen, pcode->code );
      struct c_pcode_arg* arg = pcode->args;
      while ( arg ) {
         c_add_arg( codegen, arg->value );
         arg = arg->next;
      }
   }
   else {
      c_write_opc( codegen, pcode->code );
      struct c_pcode_arg* arg = pcode->args;
      while ( arg ) {
         c_write_arg( codegen, arg->point ?
            arg->point->obj_pos : arg->value );
         arg = arg->next;
      }
   }
}

static void write_point( struct codegen* codegen, struct c_point* point ) {
   point->obj_pos = c_tell( codegen );
}

static void write_jump( struct codegen* codegen, struct c_jump* jump ) {
   if ( jump->obj_pos ) {
      c_seek( codegen, jump->obj_pos );
   }
   else {
      jump->obj_pos = c_tell( codegen );
   }
   c_add_opc( codegen, jump->opcode );
   c_add_arg( codegen, jump->point->obj_pos );
}

static void write_casejump( struct codegen* codegen,
   struct c_casejump* jump ) {
   if ( jump->obj_pos ) {
      c_seek( codegen, jump->obj_pos );
   }
   else {
      jump->obj_pos = c_tell( codegen );
   }
   c_add_opc( codegen, PCD_CASEGOTO );
   c_add_arg( codegen, jump->value );
   c_add_arg( codegen, jump->point->obj_pos );
}

static void write_sortedcasejump( struct codegen* codegen,
   struct c_sortedcasejump* sorted_jump ) {
   if ( sorted_jump->obj_pos ) {
      c_seek( codegen, sorted_jump->obj_pos );
   }
   else {
      sorted_jump->obj_pos = c_tell( codegen );
   }
   c_add_opc( codegen, PCD_CASEGOTOSORTED );
   c_add_arg( codegen, sorted_jump->count );
   struct c_casejump* jump = sorted_jump->head;
   while ( jump ) {
      c_add_arg( codegen, jump->value );
      c_add_arg( codegen, jump->point->obj_pos );
      jump = jump->next;
   }
}
