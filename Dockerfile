FROM gcc as builder
RUN apt update
RUN apt install -y exuberant-ctags
ADD . /src
WORKDIR /src
RUN make build
RUN mkdir /static

FROM scratch
COPY --from=builder /src/bin/http /
COPY --from=builder /src/http-templates /http-templates
COPY --from=builder /static /static

CMD [ "/http", "8080", "/static" ]
