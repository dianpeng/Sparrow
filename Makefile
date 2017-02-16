DEPENDEND=src/vm/util.c src/vm/object.c src/vm/list.c src/vm/map.c src/vm/vm.c src/vm/bc.c src/vm/gc.c src/vm/builtin.c src/vm/error.c src/vm/loader.c src/vm/parser.c src/vm/lexer.c
COVERAGE=-fprofile-arcs -ftest-coverage
SANITIZE=-fsanitize=address -fuse-ld=gold
map:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) src/vm/map_test.c -lm -o map-test
list:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) src/vm/list_test.c -lm -o list-test

bc:
	$(CC) -g3 -Wall -Werror src/util.c src/vm/bc.c src/vm/bc_test.c -o bc-test

object:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) src/vm/object_test.c  -lm -o object-test

parser:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) src/vm/parser_test.c -lm -o parser-test

vm:
	$(CC) -Wall -Werror -DSPARROW_DEFAULT_GC_THRESHOLD=1 -g3 $(DEPENDEND) src/vm/vm_test.c -lm -o vm-test

test:
	$(CC) -O3 -Wall -Werror -g3 $(DEPENDEND) src/vm/vm_test_driver.c -lm -o vm-test-driver

show_bc:
	$(CC) -O3 -Wall -Werror -g3 $(DEPENDEND) tools/show_bc.c -lm -o show_bc

.PHONY:clean_coverage

clean_coverage:
	rm -rf *.gcno
	rm -rf *.gcda
	rm -rf *.gcov
