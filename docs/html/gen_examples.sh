#! /bin/sh -e

for i in $@; do
	title=`./gen_text.sh $i`
	synopsis=`cat $i | head -n 1`
	echo "<a href=\"$i\">$title</a>"
	echo "$synopsis"
done
