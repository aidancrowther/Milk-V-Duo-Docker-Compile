FROM ubuntu:22.04

SHELL ["/bin/bash", "-c"]

RUN apt update \
    && apt upgrade -y \
    && apt-get install -y \
    wget \
    git \
    make \
    build-essential

RUN useradd -m milkv && echo "milkv:milkv" | chpasswd && adduser milkv sudo

USER milkv

WORKDIR /home/milkv

RUN git clone https://github.com/milkv-duo/duo-examples.git \
    && cd duo-examples \
    && source envsetup.sh

RUN echo "source ~/duo-examples/envsetup.sh" >> ~/.bashrc

RUN echo "cd ~/duo-examples" >> ~/.bashrc

VOLUME /buildroot