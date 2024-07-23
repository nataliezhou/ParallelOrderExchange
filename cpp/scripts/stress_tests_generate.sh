#!/bin/bash

# Navigate to the directory containing the test files
cd scripts

# Pass in the number of threads to generate. Usage: ./stress_tests_generate.sh 100

# Loop through all the .in files in the directory
echo "Generating stress testing files"
file="slow-buy-fast-sell.in"
> "$file"

num_threads=2
printf "$num_threads\n" > $file
for (( i=0; i<$num_threads; i++ )); do 
    echo "$i o" >> "$file"
done
# add orders
start_id=100
instrument_id=1
for (( i=0; i<500; i++ )); do 
    echo "0 S $start_id INST 501 30" >> "$file" 
    start_id=$((start_id+1))
    # echo "$i S $start_id BB$i 2705 20" >> "$file"
    # start_id=$((start_id+1))
done

echo "0 B $start_id INST 501 500" >> "$file" # the slow buy
start_id++
echo "0 S $start_id INST 501 500" >> "$file"
start_id++
# echo "0 S $start_id INST 501 500" >> "$file"

for (( i=0; i<$num_threads; i++ )); do 
    echo "$i o" >> "$file"
done
