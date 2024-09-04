#!/bin/bash

# Define cache sizes in bytes
cache_sizes=(1024 2048 4096 8192)

# Define block sizes in bytes (e.g., 16, 32, 64, 128)
block_sizes=(1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192)

# Define the maximum number of set bits to consider
max_set_bits=13

# Define the replacement policies
replacement_policies=("LFU" "LRU")

# Path to your cache.h file
CACHE_H_PATH="cache.h"

# Function to update cache.h file
update_cache_h() {
    local set_bits=$1
    local associativity=$2
    local block_bits=$3
    local policy=$4

    # Update cache.h file
    sed -i "s/#define CACHE_SET_BITS .*/#define CACHE_SET_BITS $set_bits/" $CACHE_H_PATH
    sed -i "s/#define CACHE_LINES_PER_SET .*/#define CACHE_LINES_PER_SET $associativity/" $CACHE_H_PATH
    sed -i "s/#define CACHE_BLOCK_BITS .*/#define CACHE_BLOCK_BITS $block_bits/" $CACHE_H_PATH
    if [ "$policy" == "LFU" ]; then
        sed -i "s/#define CACHE_LFU .*/#define CACHE_LFU 1/" $CACHE_H_PATH
    else
        sed -i "s/#define CACHE_LFU .*/#define CACHE_LFU 0/" $CACHE_H_PATH
    fi
}

# Function to run the simulation
run_simulation() {
    local cache_size=$1
    local set_bits=$2
    local associativity=$3
    local block_bits=$4
    local policy=$5

    # Update the cache.h file
    update_cache_h $set_bits $associativity $block_bits $policy

    # Clean and make
    make clean
    make

    # Run the simulation and save results
    local output_file="dse_${cache_size}B_all_configs.txt"
    ./riscv -s -f -c -e -v ./code/ms3/input/vec_xprod.input >> $output_file

    # Log the configuration and results
    echo "Cache Size: ${cache_size}B, Set Bits: $set_bits, Associativity: $associativity, Block Bits: $block_bits, Policy: $policy" >> $output_file
}

# Iterate over cache sizes and configurations
for cache_size in "${cache_sizes[@]}"; do
    # Clear the output file for the current cache size
    > "dse_${cache_size}B.txt"

    for block_size in "${block_sizes[@]}"; do
        block_bits=$(echo "l($block_size)/l(2)" | bc -l | awk '{printf "%.0f", $1}')
        max_associativity=$((cache_size / block_size))
        for set_bits in $(seq 0 $max_set_bits); do
            E=$((cache_size / (block_size * (2**set_bits))))
            if (( E >= 1 && E <= max_associativity && (E & (E - 1)) == 0 )); then  # E must be a power of 2
                for policy in "${replacement_policies[@]}"; do
                    run_simulation $cache_size $set_bits $E $block_bits $policy
                done
            fi
        done
    done
done
