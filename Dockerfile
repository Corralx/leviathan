FROM ubuntu:bionic
ENV DEBIAN_FRONTEND=noninteractive
RUN yes | unminimize

RUN apt-get update && apt-get install -y apt-utils software-properties-common
RUN add-apt-repository ppa:ubuntu-toolchain-r/test

RUN apt-get update && apt-get install -y build-essential cmake
RUN apt-get update && apt-get install -y gcc-8 g++-8 gcc-9 g++-9
RUN apt-get update && apt-get install -y libboost-thread-dev

ENTRYPOINT ["/usr/bin/env", "--"]
