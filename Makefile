C_SOURCE_FILES := fs.c http_accept.c main.c
build:
	gcc $(C_SOURCE_FILES) -o http-server
