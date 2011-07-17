#! /bin/sh -e

cat << HERE
<div id="menu">
<ul>
	<li><h2>examples</h2></li>
	<ul>
HERE
for i in $@; do
	title=`echo "$i" | sed 's/\.html//; s/_/ /g; s/^ex //'`
	echo "		<li><a href=\"$i\">$title</a></li>"
done

cat << HERE
	</ul>
</ul>
</div>
HERE
