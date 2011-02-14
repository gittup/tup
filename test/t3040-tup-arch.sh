#! /bin/sh -e

# Make sure @(TUP_ARCH) has a default value, and can be overridden.

. ./tup.sh
cat > Tupfile << HERE
: |> echo @(TUP_ARCH) |>
HERE
tup touch Tupfile
tup parse

# Could validate other cpu architectures here if desired - not really necessary.
if uname -s | grep Linux > /dev/null; then
	arch=`uname -m`
	case $arch in
	i686)
		arch=i386 ;;
	esac
	tup_object_exist . "echo $arch"
	tup_dep_exist @ TUP_ARCH 0 .
fi

varsetall TUP_ARCH=bar
tup parse
tup_object_exist . 'echo bar'
tup_dep_exist @ TUP_ARCH 0 .
tup_object_no_exist . 'echo x86_64'
tup_object_no_exist . 'echo i386'

eotup
