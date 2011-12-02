#! /bin/sh -e
for j in `echo $1 | sed 's/\.html//; s/_/ /g; s/index/home/; s/^ex //'`; do echo $j | awk 'BEGIN{ORS="&nbsp;"} {print toupper(substr($0, 1, 1)) substr($0, 2)}'; done | sed 's/And&nbsp/and\&nbsp/; s/Vs&nbsp/vs\&nbsp/; s/&nbsp;$//'
