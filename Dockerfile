FROM ubuntu:22.04

RUN apt update
RUN apt install -y rsync build-essential vim pahole && apt clean

ENTRYPOINT [ "/bin/bash" ]