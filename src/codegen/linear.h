#ifndef SRC_CODEGEN_LINEAR_H
#define SRC_CODEGEN_LINEAR_H

struct c_node {
   struct c_node* next;
   enum {
      C_NODE_POINT,
      C_NODE_JUMP,
      C_NODE_CASEJUMP,
      C_NODE_SORTEDCASEJUMP,
      C_NODE_PCODE,
      C_NODE_TOTAL
   } type;
};

struct c_point {
   struct c_node node;
   int obj_pos;
};

struct c_jump {
   struct c_node node;
   struct c_point* point;
   struct c_jump* next;
   int opcode;
   int obj_pos;
};

struct c_casejump {
   struct c_node node;
   struct c_casejump* next;
   struct c_point* point;
   int value;
   int obj_pos;
};

struct c_sortedcasejump {
   struct c_node node;
   struct c_casejump* head;
   struct c_casejump* tail;
   int count;
   int obj_pos;
};

struct c_pcode {
   struct c_node node;
   int code;
   struct c_pcode_arg* args;
   int obj_pos;
   bool optimize;
   bool patch;
};

struct c_pcode_arg {
   struct c_pcode_arg* next;
   struct c_point* point;
   int value;
};

#endif
