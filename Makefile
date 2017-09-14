CFLAGS := -O2 -Iinclude -std=c99 -pedantic

.PHONY: clean

lib: lib/libjsmn-tea.a

example: bin/demo bin/benchmark

clean:
	@rm -rf bin lib *.o include/jsmn.h

include/jsmn.h: jsmn/jsmn.h
	@cp -u $< $@

lib/libjsmn-tea.a: jsmn.o jsmn-tea.o
	@mkdir -p lib
	@$(AR) rc $@ $^
	@rm *.o

%.o: src/%.c include/%.h include/jsmn.h
	@$(CC) -o $@ -c $(CFLAGS) $<

jsmn.o: jsmn/jsmn.c include/jsmn.h
	@$(CC) -o $@ -c $(CFLAGS) $<

bin/%: example/%.c lib/libjsmn-tea.a
	@mkdir -p bin
	@$(CC) -o $@ $(CFLAGS) $^
