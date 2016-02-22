#ifndef SRC_SERIAL_RESTORE_LIBRARY_H
#define SRC_SERIAL_RESTORE_LIBRARY_H

struct library_restore {
   struct cache* cache;
   jmp_buf exit;
   struct task* task;
   const char* data;
   int integer;
   const char* string;
   bool boolean;
   struct library* lib;
   struct region* region;
   struct name* name;
};

#endif