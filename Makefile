DEPS_DIR := deps
CFLAGS := -O2 -std=c99 -pedantic
INCLUDE := -Iinclude -I$(DEPS_DIR)/jsmn -I$(DEPS_DIR)/roar/include

.PHONY: clean examples shared static

static: lib/libjsmn-tea.a

shared: lib/libjsmn-tea.so

examples: bin/demo bin/benchmark

clean:
	@rm -rf bin lib

lib/libjsmn-tea.a: jsmn.o jsmn-tea.o roar.o
	@mkdir -p lib
	@$(AR) rc $@ $^
	@rm -f *.o

lib/libjsmn-tea.so: src/jsmn-tea.c include/jsmn-tea.h
	@mkdir -p lib
	@$(CC) -o $@ $(INCLUDE) $(CFLAGS) -fPIC -shared $<

jsmn-tea.o: src/jsmn-tea.c include/jsmn-tea.h
	@$(CC) -o $@ -c $(INCLUDE) $(CFLAGS) $<

jsmn.o: $(DEPS_DIR)/jsmn/jsmn.c $(DEPS_DIR)/jsmn/jsmn.h
	@$(CC) -o $@ -c $(CFLAGS) $<

roar.o: $(DEPS_DIR)/roar/include/roar.h
	@$(CC) -x c -o $@ -c $(CFLAGS) -DROAR_IMPLEMENTATION $<

bin/%: examples/%.c lib/libjsmn-tea.a
	@mkdir -p bin
	@$(CC) -o $@ $(INCLUDE) $(CFLAGS) $< lib/libjsmn-tea.a
