#!/bin/bash

# This file is used for when people enter a command for docker to run from the
# outside. This is because for non-interactive terminals, .bashrc isn't run, and
# the environment isn't set.

# Execute command passed into docker run
/bin/bash -c "source /home/milkv/duo-examples/envsetup.sh && $@"
