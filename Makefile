build: # build the binary
	-mkdir -pv bin
	make -C src/

check: build # prepare and run tests to ensure quality
	-killall http
	-mkdir -pv tests
	bin/http 1>tests/out.log 2>tests/err.log &
	curl http://localhost:8080
	-killall http

clean:
	rm -fr bin/ tests/
