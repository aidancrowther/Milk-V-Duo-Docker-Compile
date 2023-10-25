# MilkVDuoCompile

A Docker container with the Milk V Duo toolchain.

## About Milk V Duo

The Milk V Duo is an ultra-compact embedded development platform based on the
Sophgo CV1800B chip. It can run Linux and RTOS, providing a reliable, low-cost,
and high-performance platform for professionals, industrial ODMs, AIoT
enthusiasts, DIY hobbyists, and creators.

## Why this Docker Image?

This Docker image is designed to provide a ready-to-use environment for
compiling code for the Milk V Duo. It eliminates the need to manually install
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
    -v $(pwd)/buildroot:/buildroot \
    ghcr.io/angelonfira/milkvduocompile:latest \
    make -C /buildroot/examples/hello-world
```

This command mounts your local `buildroot` directory to the `/buildroot`
directory in the Docker container and runs the `make` command in the
`hello-world` directory. This method is useful for automation and scripting
because it allows you to run commands without manually starting a Docker
container.

### Running Commands Within the Container

If your task is more complex or requires multiple commands, you might find it
easier to start the Docker container and run commands within it. To start a
Docker container, use the following command:

```bash
docker run -it \
    -v $(pwd)/buildroot:/buildroot \
    ghcr.io/angelonfira/milkvduocompile:latest \
    /bin/bash
```

This command starts a Docker container and opens a bash shell. From here, you
can navigate to the `hello-world` directory and run the `make` command:

```bash
cd /buildroot/examples/hello-world
make
```

This method is useful when you need to run multiple commands or want to explore
the file system within the Docker container.



## FAQ

**Q: What is Milk V Duo?** A: The Milk V Duo is an ultra-compact embedded
development platform based on the Sophgo CV1800B chip.

**Q: Why do I need this Docker image?** A: This Docker image provides a
ready-to-use environment for compiling code for the Milk V Duo, eliminating the
need to manually install and configure necessary tools on your local machine.

**Q: How do I build and run this Docker image?** A: Instructions for building
and running this Docker image are provided above in this README.
