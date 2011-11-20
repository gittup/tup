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

<div id="header">
  <img src="logo.png">
  <!-- fork me on github logo? -->
</div>
<div id="content">
HERE

cat menu.inc

if [ $example = "1" ]; then
cat examples.inc
else
echo "<h1>$text</h1>"
fi

cat $1

cat << HERE
<div style="clear:both;"></div>
</div>
<div id="footer">&copy; 2011 Mike Shal. All Rights Reserved.</div>
</body>
</html>
HERE
