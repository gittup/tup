#! /bin/sh -e

# This is a helper script to generate tup-version.o at the same time as
# linking, so that the version is updated whenever we change anything that
# affects the tup binary. This used to live in the Tupfile, but to support
# Windows local builds we need to make it an explicit shell script.
CC=$1
CFLAGS=$2
LDFLAGS=$3
output_tup=$4
output_version=$5
files=$6
version=$7
if [ "$version" = "" ]; then
	# If we don't pass in a version, try to get one from git
	version=`git describe 2>/dev/null || true`
	if [ "$version" = "" ]; then
		# If we aren't using git, try to get one from the pathname (eg:
		# for a tarball release
		version=`echo "$PWD" | sed 's/.*\/tup-//'`
		if [ "$version" = "" ]; then
			# No other version source, use "unknown"
			version="unknown"
		fi
	fi
fi
(echo "#include \"tup/version.h\""; echo "const char tup_version[] = \"$version\";") | $CC -x c -c - -o $output_version $CFLAGS
$CC $files $output_version -o $output_tup $LDFLAGS
