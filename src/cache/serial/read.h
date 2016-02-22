#ifndef SRC_SERIAL_READ_H
#define SRC_SERIAL_READ_H

#include <stdbool.h>
#include <setjmp.h>
#include <time.h>

#include "field.h"

struct serial_r {
   jmp_buf* exit;
   const char* data;
};

void srl_init_reading( struct serial_r* reading, jmp_buf* exit,
   const char* data );
enum field srl_peekf( struct serial_r* reading );
void srl_rf( struct serial_r* reading, enum field expected_field );
int srl_ri( struct serial_r* reading, enum field expected_field );
time_t srl_rt( struct serial_r* reading, enum field expected_field );
bool srl_rb( struct serial_r* reading, enum field expected_field );
const char* srl_rs( struct serial_r* reading,
   enum field expected_field );

#endif