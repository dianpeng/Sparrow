DEPENDEND=-Isrc/. -DSPARROW_DEBUG \
					src/vm/object.c \
					src/vm/map.c \
					src/vm/list.c \
					src/vm/vm.c \
					src/vm/bc.c \
					src/vm/gc.c \
					src/vm/builtin.c \
					src/vm/error.c \
					src/vm/loader.c \
					src/vm/parser.c \
					src/vm/lexer.c \
					src/shared/util.c \
					src/shared/debug.c \
					src/compiler/ir.c \
					src/compiler/bc-ir-builder.c \
					src/compiler/ir-helper.c

COVERAGE=-fprofile-arcs -ftest-coverage

SANITIZE=-fsanitize=address -fuse-ld=gold

map:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) test/vm/map-test.c -lm -o map-test
list:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) test/vm/list-test.c -lm -o list-test

bc:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) test/vm/bc-test.c -lm -o bc-test

object:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) test/vm/object-test.c  -lm -o object-test

parser:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) test/vm/parser-test.c -lm -o parser-test

vm:
	$(CC) -Wall -Werror -DSPARROW_DEFAULT_GC_THRESHOLD=1 -g3 $(DEPENDEND) test/vm/vm-test.c -lm -o vm-test

test:
	$(CC) -O3 -Wall -Werror -g3 $(DEPENDEND) test/vm/vm-test-driver.c -lm -o vm-test-driver

show_bc:
	$(CC) -Wall -Werror -g3 $(DEPENDEND) tools/show-bc.c -lm -o show-bc

ir:
	$(CC) -Wall -Werror -g3 $(DEPENDEND) test/compiler/ir-test.c -lm -o ir-test

.PHONY:clean_coverage

clean_coverage:
	rm -rf *.gcno
	rm -rf *.gcda
	rm -rf *.gcov
