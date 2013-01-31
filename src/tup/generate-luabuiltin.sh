#!/bin/bash
echo 'const char tuplua_builtin[] = "\'
while IFS='' read line
do
	printf "%s\\\n\\\\\n" "$line"
done < builtin.lua
echo '";'

