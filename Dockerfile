# Use debian for the base image
FROM debian:buster-slim

# Install necessary packages
RUN apt-get update && apt-get install -y \
    bash \
    git \
    make \
    g++ \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user
RUN useradd -m milkv && echo "milkv:milkv" | chpasswd && adduser milkv sudo

# Switch to the new user
USER milkv

# Set the working directory
WORKDIR /home/milkv

# Clone the repository and source the setup script
RUN git clone https://github.com/milkv-duo/duo-examples.git \
    && cd duo-examples \
    && source envsetup.sh

# Update .bashrc
RUN echo "source ~/duo-examples/envsetup.sh" >> ~/.bashrc \
    && echo "cd ~/duo-examples" >> ~/.bashrc

# Create a volume for buildroot
VOLUME /buildroot
