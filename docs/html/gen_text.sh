#! /bin/sh -e

for j in $(echo $1 | sed 's/\.html//; s/_/ /g; s/index/home/; s/^ex //'); do echo -n ${j:0:1} | tr "[:lower:]" "[:upper:]"; echo -n "${j:1}&nbsp;"; done | sed 's/And&nbsp/and\&nbsp/; s/Vs&nbsp/vs\&nbsp/; s/&nbsp;$//'
