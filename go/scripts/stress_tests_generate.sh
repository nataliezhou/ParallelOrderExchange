#!/bin/bash

# Navigate to the directory containing the test files
cd scripts

# Pass in the number of threads to generate. Usage: ./stress_tests_generate.sh 100

# Loop through all the .in files in the directory
echo "Generating stress testing files"
file="forty-threads-3.in"
> "$file"

num_threads=25
printf "$num_threads\n" > $file
for (( i=0; i<$num_threads; i++ )); do 
    echo "$i o" >> "$file"
done

# add orders
start_id=100
instrument_id=1
for (( i=0; i<num_threads; i++ )); do 
    echo "$i B $start_id AA$i 2705 20" >> "$file" 
    start_id=$((start_id+1))
    echo "$i S $start_id BB$i 2705 20" >> "$file" 
    start_id=$((start_id+1))
done

for (( i=0; i<num_threads; i++ )); do 
    echo "$i S $start_id AA$i 2705 20" >> "$file" 
    start_id=$((start_id+1))
    echo "$i B $start_id BB$i 2705 20" >> "$file" 
    start_id=$((start_id+1))
done

for (( i=0; i<$num_threads; i++ )); do 
    echo "$i x" >> "$file"
done
