#! /bin/sh -e

menu=""
if [ $1 = "-m" ]; then
	shift
	menu=$1
	shift
fi
base=`basename $1`
text=`./gen_text.sh $base`
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

cat $menu
echo "<h1>$text</h1>"

cat $1

cat << HERE
<div style="clear:both;"></div>
</div>
<div id="footer">&copy; 2008-2024 Mike Shal. All Rights Reserved.</div>
HERE

cat << HERE
</body>
</html>
HERE
