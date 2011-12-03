#! /bin/sh -e

example=0
if [ $1 = "-x" ]; then
	example=1
	shift
fi

cat << HERE
  <ul id="menu">
    <li class="menu-header">Site Map</li>
HERE
for i in $@; do
	text=`./gen_text.sh $i`
	echo "    <li class=\"menu-item\"><a href=\"$i\">$text</a></li>"
	if [ "$i" = "examples.html" ]; then
		if [ $example = "1" ]; then
			cat examples.inc
		fi
	fi
done

cat << HERE
    <li class="menu-header">Additional Info</li>
    <li class="menu-item"><a href="build_system_rules_and_algorithms.pdf">Build System Rules<br>and Algorithms (PDF)</a></li>
    <li class="menu-item"><a href="http://groups.google.com/group/tup-users">tup-users mailing list</a></li>
  </ul>
HERE
