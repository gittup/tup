touch file1
touch file2
touch file3

../../../wrapper ../../../create_dep file1 file2
../../../wrapper ../../../create_dep file2 file3
../../../wrapper ../../../create_dep file3 file1
