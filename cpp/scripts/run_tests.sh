#!/bin/bash

# Generated by ChapGPT 3.5

# Navigate to the directory containing the test files
cd scripts

# Loop through all the .in files in the directory
for testfile in *.in; do
    echo "Running teh==st on $testfile"
    # Execute the command with the current test file as input
    ../grader_arm64-apple-darwin ../engine < "$testfile"
    echo "Test $testfile completed"
    echo "------------------------------------------------------"
done
