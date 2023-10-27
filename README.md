# Milk-V Duo Compile

A Docker container with the Milk-V Duo toolchain.

<p align="center">
  <img src="https://milkv.io/assets/images/duo-v1.2-9bf1d36ef7632ffba032796978cda903.png" width="300">
  <br>
  <i>Don't let your Milk go spoiled!</i>
</p>

## About Milk-V Duo

*From [the Milk-V website](https://milkv.io/duo)*

The Milk-V Duo is an ultra-compact embedded development platform based on the
Sophgo CV1800B chip. It can run Linux and RTOS, providing a reliable, low-cost,
and high-performance platform for professionals, industrial ODMs, AIoT
enthusiasts, DIY hobbyists, and creators.

## Why this Docker Image?

This Docker image is designed to provide a ready-to-use environment for
compiling code for the Milk-V Duo. It eliminates the need to manually install
and configure the necessary tools on your local machine.

## Running the Docker Image

There are two ways to run commands with this Docker image: directly from outside
the container or within the container. The method you choose depends on your
specific needs.

### Running Commands from Outside the Container

If you have a simple task or want to automate a process, you can run a command
from outside the container. For example, if you want to compile the
`hello-world` example in this repository, you can do so with the following
command:

```bash
docker run -it \
    -v $(pwd):/home/milkv/buildroot \
    ghcr.io/aidancrowther/milk-v-duo-docker-compile:latest \
    "cd examples/hello-world && make"
```

This command mounts your current directory to the `/home/milkv/buildroot`
directory in the Docker container and runs the `make` command in the
`hello-world` directory. This method is useful for automation and scripting
because it allows you to run commands without manually starting a Docker
container. Note, the `"` are needed for your command.

### Running Commands Within the Container

If your task is more complex or requires multiple commands, you might find it
easier to start the Docker container and run commands within it. To start a
Docker container, use the following command:

```bash
docker run -it \
    -v $(pwd):/home/milkv/buildroot \
    ghcr.io/aidancrowther/milk-v-duo-docker-compile:latest \
    /bin/bash
```

This command starts a Docker container and opens a bash shell. From here, you
can navigate to the `hello-world` directory and run the `make` command:

```bash
cd examples/hello-world
make
```

This method is useful when you need to run multiple commands or want to explore
the file system within the Docker container.

## Copying Binaries to the Milk-V Duo

The Milk-V Duo runs on Busybox Linux, which has many simpler and/or older
implementations of common services. As such, you may encounter an error when
following the tutorial from Milk to copy files to the board. If you observe an
error regarding `/usr/libexec/sftp-server` being missing, this means you are
using a new version of SCP that usses sftp on its backend. To resolve this
issue, simply add the -O flag to your command and it should resolve it.

## Building the Docker Image

Building the Docker image yourself can be beneficial for several reasons:

1. **Customization**: You might want to add additional tools or modify the
   existing setup to suit your specific needs. Building the image yourself
   allows you to make these customizations.

2. **Learning**: If you're new to Docker, going through the process of building
   an image can be a great learning experience.

3. **Security**: Although it's generally safe to use images from trusted
   sources, building the image yourself ensures that you know exactly what's in
   your image.

To build the Docker image, use the following command:

```bash
docker build -t ghcr.io/aidancrowther/milk-v-duo-docker-compile .
```

In this command, `ghcr.io/aidancrowther/milk-v-duo-docker-compile` is the tag for the
Docker image. The tag is a label for your image so that it can be referenced.
This tag is used in subsequent commands (like `docker run`). By using this tag,
you don't need to change any of the other commands in this README.

## Using GitHub Codespaces

If you don't have Docker installed on your machine, you can use GitHub
Codespaces. Codespaces sets up a cloud-hosted, containerized, and customizable
VS Code environment. This repository is already configured for use with
Codespaces.

### Setting Up Codespaces

1. Navigate to the main page of the repository.
2. Click the green `Code` button.
3. In the dropdown, select `Open with Codespaces`.
4. Select `+ New codespace`.

After a short while, your codespace will be ready and you'll be in a fully
featured VS Code workspace.

### Building and Running in Codespaces

You can build and run your code directly in the terminal in Codespaces, just
like you would on your local machine.

### Downloading Executables from Codespaces

If you need to download executables or any other files from Codespaces:

1. In the VS Code workspace, right-click the file you want to download.
2. Select `Download`.

The selected file will be downloaded to your local machine.
