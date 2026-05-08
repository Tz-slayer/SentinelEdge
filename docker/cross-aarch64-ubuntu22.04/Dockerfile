FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        binutils-aarch64-linux-gnu \
        ca-certificates \
        cmake \
        file \
        g++-aarch64-linux-gnu \
        gcc-aarch64-linux-gnu \
        make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
