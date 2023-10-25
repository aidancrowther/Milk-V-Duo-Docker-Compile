FROM ubuntu:22.04

SHELL ["/bin/bash", "-c"]

RUN apt update -y && apt upgrade -y

RUN apt-get install -y\
                     wget \
                     git \
                     make \
                     build-essential

RUN cd /root && \
    git clone https://github.com/milkv-duo/duo-examples.git && \
    cd duo-examples && \
    source envsetup.sh

RUN echo "source /root/duo-examples/envsetup.sh" >> ~/.bashrc

RUN echo "cd /root/duo-examples" >> ~/.bashrc

RUN mkdir /buildroot
