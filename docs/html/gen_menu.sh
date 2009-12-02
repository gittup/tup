#! /bin/sh -e

cat << HERE
<div id="menu">
<table>
<tr><td align=center bgcolor="#efefef"><img src="logo.png"></td></tr>
<tr><td>
<ul>
	<li><h2>Site Map</h2></li>
HERE
for i in $@; do
	text=`echo $i | sed 's/\.html//' | sed 's/_/\&nbsp;/g' | sed 's/index/home/'`
	echo "	<li><a href=\"$i\">$text</a></li>"
done

cat << HERE
	<li><h2>Additional Info</h2></li>
	<li><a href="build_system_rules_and_algorithms.pdf">Build System Rules and Algorithms (PDF)</a></li>
	<li><a href="http://groups.google.com/group/tup-users">tup-users mailing list</a></li>
</ul>
</td></tr>
</table>
</div>
HERE
