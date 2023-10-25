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

SHELL ["/bin/bash", "-c"]

# Set the working directory
WORKDIR /home/milkv

# Clone the repository and source the setup script
RUN git clone https://github.com/milkv-duo/duo-examples.git \
    && cd duo-examples \
    && ./envsetup.sh

# Update .bashrc
RUN echo ". ~/duo-examples/envsetup.sh" >> ~/.bashrc \
    && echo "cd /home/milkv/buildroot" >> ~/.bashrc

# Set the working directory for the user
WORKDIR /home/milkv/buildroot

# Add entrypoint script
COPY entrypoint.sh /entrypoint.sh

# This entrypoint is just for Docker run commands that pass a command in, rather
# than creating an interactive terminal.
ENTRYPOINT ["/entrypoint.sh"]
