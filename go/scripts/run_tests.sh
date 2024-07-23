#!/bin/bash

# Generated by ChapGPT 3.5

# Navigate to the directory containing the test files
cd scripts

# Determine the operating system
OS="$(uname -s)"

# Choose the executable based on the OS
if [[ "$OS" == "Darwin" ]]; then
    # For macOS
    GRADER_EXEC="../grader_arm64-apple-darwin"
else
    # For other environments, including Windows
    GRADER_EXEC="../grader"
fi

# Loop through all the .in files in the directory
for testfile in *.in; do
    echo "Running test on $testfile"
    # Execute the command with the current test file as input
    $GRADER_EXEC ../engine < "$testfile"
    echo "Test $testfile completed"
    echo "------------------------------------------------------"
done
