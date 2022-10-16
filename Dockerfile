FROM gcc as builder
RUN apt update
RUN apt install -y exuberant-ctags
ADD . /src
WORKDIR /src
RUN make build

FROM scratch
COPY --from=builder /src/bin/http /
COPY --from=builder /src/http-templates /

CMD [ "/http" ]
