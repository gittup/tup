#! /bin/sh -e

cat << HERE
  <ul id="menu">
    <li class="menu-header">Site Map</li>
HERE
for i in $@; do
	text=`./gen_text.sh $i`
	echo "Text is: $text" 1>&2
	echo "    <li class=\"menu-item\"><a href=\"$i\">$text</a></li>"
done

cat << HERE
    <li class="menu-header">Additional Info</li>
    <li class="menu-item"><a href="build_system_rules_and_algorithms.pdf">Build System Rules<br>and Algorithms (PDF)</a></li>
    <li class="menu-item"><a href="http://groups.google.com/group/tup-users">tup-users mailing list</a></li>
  </ul>
HERE
