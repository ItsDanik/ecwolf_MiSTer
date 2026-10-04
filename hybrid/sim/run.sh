#!/bin/sh
# Run the hybrid_host testbench in the simulation container
cd "$(dirname "$0")/.."
docker image inspect mister-hybrid-sim > /dev/null 2>&1 || docker build -t mister-hybrid-sim toolchain/sim
docker run --rm -v "$PWD":/src mister-hybrid-sim sh -c \
  "iverilog -g2012 -Wall -o /tmp/tb sim/tb_host.sv rtl/hybrid_host.sv && vvp -n /tmp/tb"
