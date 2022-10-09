help: # Show the make targets
	@grep "^[^	]*:" Makefile

build: # build the binary (linux only)
	-mkdir -pv bin
	make -C src/

docker-build: # build the binary and docker image
	docker build -t glaudiston/http .

docker-run: # run a container  using the port 8080
	docker run -p 8080:8080 --name http glaudiston/http

check: build # prepare and run tests to ensure quality
	-killall http
	-mkdir -pv tests
	bin/http 1>tests/out.log 2>tests/err.log &
	curl http://localhost:8080
	-killall http

clean: # remove all generated files
	rm -fr bin/ tests/

