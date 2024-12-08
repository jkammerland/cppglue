#!/bin/bash

function show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  --help                 Show this help message"
    echo "  --install <path>         Install LLVM to specified path"
}

if [ "$1" = "--help" ]; then
    show_help
    exit 0
elif [ "$1" = "--install" ]; then
    if [ -z "$2" ]; then
        echo "Error: --install requires a path argument"
        exit 1
    fi
    
    echo "Installing LLVM to $2..."
    mkdir -p "$2"
    cp -r /usr/local/* "$2/"
    echo "Installation complete on host. Please add the path to your PATH environment variable."
else
    echo "No argument provided."
    exec "$@"
fi