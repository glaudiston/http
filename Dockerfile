FROM gcc as builder
RUN apt update
RUN apt install -y exuberant-ctags
ADD . /src
WORKDIR /src
RUN make

FROM scratch
COPY --from=builder /src/bin/http /

CMD [ "/http" ]
