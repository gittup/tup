#! /bin/sh -e

analytics=0
if [ $1 = "-a" ]; then
	analytics=1
	shift
fi
example=0
if [ $1 = "-x" ]; then
	example=1
	shift
fi
text=`./gen_text.sh $1`
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

if [ $example = "1" ]; then
cat menu-examples.inc
else
cat menu.inc
echo "<h1>$text</h1>"
fi

cat $1

cat << HERE
<div style="clear:both;"></div>
</div>
<div id="footer">&copy; 2011 Mike Shal. All Rights Reserved.</div>
HERE

if [ $analytics = "1" ]; then
cat << HERE
<script type="text/javascript">
	var _gaq = _gaq || [];
	_gaq.push(['_setAccount', 'UA-171695-12']);
	_gaq.push(['_trackPageview']);

	(function() {
		var ga = document.createElement('script'); ga.type = 'text/javascript'; ga.async = true;
		ga.src = ('https:' == document.location.protocol ? 'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';
		var s = document.getElementsByTagName('script')[0]; s.parentNode.insertBefore(ga, s);
	})();
</script>
HERE
fi

cat << HERE
</body>
</html>
HERE
