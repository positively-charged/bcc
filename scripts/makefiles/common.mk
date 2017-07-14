OBJECTS=\
	$(BUILD_DIR)/builtin.o \
	$(BUILD_DIR)/common.o \
	$(BUILD_DIR)/gbuf.o \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/task.o \
	$(BUILD_DIR)/version.o \
	$(BUILD_DIR)/parse/asm.o \
	$(BUILD_DIR)/parse/dec.o \
	$(BUILD_DIR)/parse/expr.o \
	$(BUILD_DIR)/parse/library.o \
	$(BUILD_DIR)/parse/phase.o \
	$(BUILD_DIR)/parse/stmt.o \
	$(BUILD_DIR)/parse/token/dirc.o \
	$(BUILD_DIR)/parse/token/expr.o \
	$(BUILD_DIR)/parse/token/info.o \
	$(BUILD_DIR)/parse/token/output.o \
	$(BUILD_DIR)/parse/token/queue.o \
	$(BUILD_DIR)/parse/token/source.o \
	$(BUILD_DIR)/parse/token/stream.o \
	$(BUILD_DIR)/parse/token/user.o \
	$(BUILD_DIR)/semantic/asm.o \
	$(BUILD_DIR)/semantic/dec.o \
	$(BUILD_DIR)/semantic/expr.o \
	$(BUILD_DIR)/semantic/phase.o \
	$(BUILD_DIR)/semantic/stmt.o \
	$(BUILD_DIR)/semantic/type.o \
	$(BUILD_DIR)/codegen/asm.o \
	$(BUILD_DIR)/codegen/chunk.o \
	$(BUILD_DIR)/codegen/dec.o \
	$(BUILD_DIR)/codegen/expr.o \
	$(BUILD_DIR)/codegen/linear.o \
	$(BUILD_DIR)/codegen/obj.o \
	$(BUILD_DIR)/codegen/pcode.o \
	$(BUILD_DIR)/codegen/phase.o \
	$(BUILD_DIR)/codegen/stmt.o \
	$(BUILD_DIR)/cache/archive.o \
	$(BUILD_DIR)/cache/cache.o \
	$(BUILD_DIR)/cache/field.o \
	$(BUILD_DIR)/cache/library.o

# Compile executable.
$(BUILD_DIR)/$(EXE): $(OBJECTS)
	polink $(LIB) $(LINKFLAGS) -out:"$@" $^
ifdef STRIP_EXE
	strip $@
endif

# Compile: src/
$(BUILD_DIR)/builtin.o: \
	src/builtin.c
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/common.o: \
	src/common.c \
	src/common.h \
	src/task.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/gbuf.o: \
	src/gbuf.c \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/main.o: \
	src/main.c \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/parse/phase.h \
	src/task.h \
	src/semantic/phase.h \
	src/codegen/phase.h \
	src/codegen/linear.h \
	src/cache/cache.h \
	src/gbuf.h \
	src/cache/field.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/task.o: \
	src/task.c \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/version.o: \
	build/version.c
	$(CC) -c $(OPTIONS) -Fo $@ $<

# Compile: src/parse/
$(BUILD_DIR)/parse/asm.o: \
	src/parse/asm.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/phase.h \
	src/codegen/linear.h \
	src/codegen/pcode.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/dec.o: \
	src/parse/dec.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/expr.o: \
	src/parse/expr.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/library.o: \
	src/parse/library.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/cache/cache.h \
	src/cache/field.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/phase.o: \
	src/parse/phase.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/cache/cache.h \
	src/gbuf.h \
	src/cache/field.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/stmt.o: \
	src/parse/stmt.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<

# Compile: src/parse/token/
$(BUILD_DIR)/parse/token/dirc.o: \
	src/parse/token/dirc.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/token/expr.o: \
	src/parse/token/expr.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/token/info.o: \
	src/parse/token/info.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/token/output.o: \
	src/parse/token/output.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/token/queue.o: \
	src/parse/token/queue.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/token/source.o: \
	src/parse/token/source.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/token/stream.o: \
	src/parse/token/stream.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/parse/token/user.o: \
	src/parse/token/user.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<

# Compile: src/semantic/
$(BUILD_DIR)/semantic/asm.o: \
	src/semantic/asm.c \
	src/semantic/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/phase.h \
	src/codegen/linear.h \
	src/codegen/pcode.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/semantic/dec.o: \
	src/semantic/dec.c \
	src/semantic/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/semantic/expr.o: \
	src/semantic/expr.c \
	src/parse/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/phase.h \
	src/codegen/linear.h \
	src/semantic/phase.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/semantic/phase.o: \
	src/semantic/phase.c \
	src/semantic/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/semantic/stmt.o: \
	src/semantic/stmt.c \
	src/semantic/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/semantic/type.o: \
	src/semantic/type.c \
	src/semantic/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h
	$(CC) -c $(OPTIONS) -Fo $@ $<

# Compile: src/codegen/
$(BUILD_DIR)/codegen/asm.o: \
	src/codegen/asm.c \
	src/codegen/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/linear.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/codegen/chunk.o: \
	src/codegen/chunk.c \
	src/codegen/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/linear.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/codegen/dec.o: \
	src/codegen/dec.c \
	src/codegen/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/linear.h \
	src/codegen/pcode.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/codegen/expr.o: \
	src/codegen/expr.c \
	src/codegen/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/linear.h \
	src/codegen/pcode.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/codegen/linear.o: \
	src/codegen/linear.c \
	src/codegen/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/linear.h \
	src/codegen/pcode.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/codegen/obj.o: \
	src/codegen/obj.c \
	src/codegen/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/linear.h \
	src/codegen/pcode.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/codegen/pcode.o: \
	src/codegen/pcode.c \
	src/common.h \
	src/codegen/pcode.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/codegen/phase.o: \
	src/codegen/phase.c \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/phase.h \
	src/codegen/linear.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/codegen/stmt.o: \
	src/codegen/stmt.c \
	src/codegen/phase.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/codegen/linear.h \
	src/codegen/pcode.h
	$(CC) -c $(OPTIONS) -Fo $@ $<

# Compile: src/cache/
$(BUILD_DIR)/cache/archive.o: \
	src/cache/archive.c \
	src/cache/cache.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/gbuf.h \
	src/cache/field.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/cache/cache.o: \
	src/cache/cache.c \
	src/cache/cache.h \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/gbuf.h \
	src/cache/field.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/cache/field.o: \
	src/cache/field.c \
	src/gbuf.h \
	src/cache/field.h
	$(CC) -c $(OPTIONS) -Fo $@ $<
$(BUILD_DIR)/cache/library.o: \
	src/cache/library.c \
	src/task.h \
	src/common.h \
	src/gbuf.h \
	src/cache/cache.h \
	src/gbuf.h \
	src/cache/field.h
	$(CC) -c $(OPTIONS) -Fo $@ $<

show-objects:
	@echo $(OBJECTS)
