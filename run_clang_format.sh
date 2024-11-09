#!/bin/bash

# Find all .cc and .h files under the dali directory and format them with clang-format.
find dali -name "*.cc" -o -name "*.h" | while read -r file; do
    echo "Running: clang-format -i -style=GOOGLE \"$file\""
    clang-format -i -style=GOOGLE "$file"
done
