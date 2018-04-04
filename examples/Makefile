all: test  test-list test-string test-hash test-set test-zset test-key test-stable test-perform test-perform-proto
test:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test test.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt
test-list:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test-list test_list.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt
test-string:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test-string test_string.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt
test-hash:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test-hash test_hash.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt
test-set:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test-set test_set.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt
test-zset:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test-zset test_zset.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt
test-key:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test-key test_key.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt
test-stable:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test-stable test_stable.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt
test-perform:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test-perform test_perform.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt
test-perform-proto:
	gcc -g -ggdb -std=c99 -pedantic -O0 -fPIC  -Wall -W -Wstrict-prototypes -Wwrite-strings  -o test-perform-proto test_perform_proto.c -I../ -L../  -L/usr/local/lib ../libxhiredis.a -lpthread  -lrt

clean:
	rm test test-list test-string test-hash test-set test-zset test-key test-stable test-perform test-perform-proto
