# bcc Makefile.

EXE=bcc.exe
BUILD_DIR=build/x64
VERSION_FILE=$(BUILD_DIR)/version.c

CC = pocc
INCLUDE = \
   -I"$(PELLESC_DIR)\Include\Win" \
   -I"$(PELLESC_DIR)\Include" \
   -Isrc -Isrc/parse
PELLESC_DIR = C:\Program Files\PellesC

CCFLAGS = -Tx64-coff -Ot -Ze -std:C99
OPTIONS=$(CCFLAGS) $(INCLUDE)
LIB = \
   -LIBPATH:"$(PELLESC_DIR)\Lib\Win64" \
   -LIBPATH:"$(PELLESC_DIR)\Lib"
LINKFLAGS = -subsystem:console -machine:x64 -debug -debugtype:coff

include common.mk
