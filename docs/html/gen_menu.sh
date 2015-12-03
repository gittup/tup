#! /bin/sh -e

example=""
if [ $1 = "-x" ]; then
	shift
	example=$1
	shift
fi

luaexample=""
if [ $1 = "-l" ]; then
	shift
	luaexample=$1
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
		cat $example
	fi
	if [ "$i" = "lua_parser.html" ]; then
		cat $luaexample
	fi
done

cat << HERE
    <li class="menu-header">Additional Info</li>
    <li class="menu-item"><a href="build_system_rules_and_algorithms.pdf">Build System Rules<br>and Algorithms (PDF)</a></li>
    <li class="menu-item"><a href="http://groups.google.com/group/tup-users">tup-users mailing list</a></li>
  </ul>
HERE
