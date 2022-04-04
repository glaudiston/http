C_SOURCE_FILES := src/logger.c src/fs.c src/http_accept.c src/main.c
build: # build the binary
	-mkdir bin
	gcc -O3 -g $(C_SOURCE_FILES) -o bin/http

check: build # prepare and run tests to ensure quality
	-killall http
	- mkdir tests
	bin/http 1>tests/out.log 2>tests/err.log &
	curl http://localhost:8080
	-killall http

clean:
	rm -fr bin/ tests/
