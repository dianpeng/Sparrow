DEPENDEND=src/util.c src/fe/object.c src/fe/list.c src/fe/map.c src/fe/vm.c src/fe/bc.c src/fe/gc.c src/fe/builtin.c src/fe/error.c src/fe/sparrow.c src/fe/parser.c src/fe/lexer.c
COVERAGE=-fprofile-arcs -ftest-coverage
SANITIZE=-fsanitize=address -fuse-ld=gold
map:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) src/fe/map_test.c -lm -o map-test
list:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) src/fe/list_test.c -lm -o list-test

bc:
	$(CC) -g3 -Wall -Werror src/util.c src/fe/bc.c src/fe/bc_test.c -o bc-test

object:
	$(CC) -g3 -Wall -Werror $(DEPENDEND) src/fe/object_test.c  -lm -o object-test

parser:
	$(CC) -g3 $(DEPENDEND) src/fe/parser_test.c -lm -o parser-test

vm:
	$(CC) -O3 -DSPARROW_DEFAULT_GC_THRESHOLD=1 -g3 $(DEPENDEND) src/fe/vm_test.c -lm -o vm_test

test:
	$(CC) -O3 -g3 $(DEPENDEND) src/fe/vm_test_driver.c -lm -o vm_test_driver

.PHONY:clean_coverage

clean_coverage:
	rm -rf *.gcno
	rm -rf *.gcda
	rm -rf *.gcov
