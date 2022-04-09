C_SOURCE_FILES := main.c
BIN_TARGET_DIR ?= ../bin/http

build: # build the binary
	-mkdir -pv ../bin
	gcc -E main.c | grep "^# 1 " | cut -d\" -f2 | sort | uniq | ctags -L - --c-kinds=+p --fields=+iaS --extra=+q -I__wur,__attr_dealloc,__attr_dealloc_fclose,__attr_dealloc_free,__attribute_const__,__attribute_deprecated__,__attribute_malloc__,__attribute_pure__,__attribute_warn_unused_result__,__THROW,__THROWNL
	gcc -D_FORTIFY_SOURCE=2 -pipe -Wall -O3 -g $(C_SOURCE_FILES) -o $(BIN_TARGET_DIR)

clean:
	rm -fr bin/http
