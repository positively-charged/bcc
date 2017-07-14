# bcc Makefile, for pomake.

EXE=bcc.exe
BUILD_DIR=build/x86
VERSION_FILE=$(BUILD_DIR)/version.c

CC = pocc
INCLUDE = \
   -I"$(PELLESC_DIR)\Include\Win" \
   -I"$(PELLESC_DIR)\Include" \
   -Isrc -Isrc/parse
PELLESC_DIR = C:\Program Files\PellesC

CCFLAGS = -Tx86-coff -Ot -Ze -std:C99
OPTIONS=$(CCFLAGS) $(INCLUDE)
LIB = \
   -LIBPATH:"$(PELLESC_DIR)\Lib\Win" \
   -LIBPATH:"$(PELLESC_DIR)\Lib"
LINKFLAGS = -subsystem:console -machine:x86 -debug -debugtype:coff

!include common.mk
