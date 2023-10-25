#!/bin/bash

# This file is used for when people enter a command for docker to run from the
# outside. This is because

# Source .bashrc
# source /home/milkv/.bashrc

echo "Test"
echo "$@"
echo "Test"
echo $SHELL

# Execute command passed into docker run
/bin/bash -c "source /home/milkv/duo-examples/envsetup.sh && $@"

exec "$@"
