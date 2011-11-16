#! /bin/sh -e

cat << HERE
<ul id="menu">
  <li class="menu-header">examples</li>
HERE
for i in $@; do
	title=`echo "$i" | sed 's/\.html//; s/_/ /g; s/^ex //'`
	echo "  <li class=\"menu-item\"><a href=\"$i\">$title</a></li>"
done

cat << HERE
</ul>
HERE
