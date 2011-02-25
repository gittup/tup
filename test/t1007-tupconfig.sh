#! /bin/sh -e

# Test out ~/.tupconfig
check()
{
	if tup config | grep "$1.*$2" > /dev/null; then
		:
	else
		echo "Error: Expected config value $1 to be set to $2" 1>&2
		exit 1
	fi
}

. ./tup.sh
# Override HOME so we can control ~/.tupconfig
export HOME=`pwd`
re_init
check num_jobs 1
check keep_going 0

echo 'num_jobs = 2' > .tupconfig
re_init
check num_jobs 2
check keep_going 0

cat > .tupconfig << HERE
num_jobs = 3
keep_going = 1
HERE
re_init
check num_jobs 3
check keep_going 1

eotup
