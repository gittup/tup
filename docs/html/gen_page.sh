#! /bin/sh -e

example=0
if [ $1 = "-x" ]; then
	example=1
	shift
fi
text=`echo $1 | sed 's/\.html//; s/_/ /g; s/index/home/; s/^ex //'`
cat << HERE
<html>
<head>
	<title>tup | $text</title>
	<link rel="stylesheet" type="text/css" href="tup.css"/>
</head>
<body>
<table>
<tr><td valign="top">
HERE

cat menu.inc

cat << HERE
</td><td valign="top">
HERE

if [ $example = "1" ]; then
cat examples.inc
else
echo "<h1>$text</h1>"
fi

cat $1

cat << HERE
</td></tr></table>
</body>
</html>
HERE
