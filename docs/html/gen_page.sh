#! /bin/sh -e

text=`echo $1 | sed 's/\.html//' | sed 's/_/ /g' | sed 's/index/home/'`
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
<h1>$text</h1>
HERE

cat $1

cat << HERE
</td></tr></table>
</body>
</html>
HERE
