C_SOURCE_FILES := src/fs.c src/http_accept.c src/main.c
build:
	-mkdir bin
	gcc -O3 $(C_SOURCE_FILES) -o bin/http

clean:
	rm -fr bin/
