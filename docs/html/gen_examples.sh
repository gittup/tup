#! /bin/sh -e

for i in $@; do
	title=`cat $i | head -n 1`
	synopsis=`cat $i | head -n 2 | tail -n 1`
	echo "<a href=\"$i\">$title</a>"
	echo "$synopsis"
done
