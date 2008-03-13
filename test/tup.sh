#! /bin/sh
tupdir=$PWD
check_empty_tupdirs()
{
	if [ "$(ls -A $tupdir/.tup/create)" ]; then
		cd $tupdir
		echo "Files shouldn't exist: " .tup/{create,modify,delete}/* 1>&2
		exit 1
	fi
}

sym_check()
{
	f=$1
	shift
	if [ ! -f $f ]; then
		echo "Object file does not exist: $f" 1>&2
		exit 1
	fi
	while [ $# -gt 0 ]; do
		sym=$1
		if echo $sym | grep '^~' > /dev/null; then
			sym=`echo $sym | sed 's/^~//'`
			if nm $f | grep $sym > /dev/null; then
				echo "'$sym' shouldn't exist in '$f'" 1>&2
				exit 1
			fi
		else
			if nm $f | grep $sym > /dev/null; then
				:
			else
				echo "No symbol '$sym' in object '$f'" 1>&2
				exit 1
			fi
		fi
		shift
	done
}

check_not_exist()
{
	if [ -f $1 ]; then
		echo "File '$1' exists when it shouldn't" 1>&2
		exit 1
	fi
}

tup_object_exist()
{
	while [ $# -gt 0 ]; do
		sum=`echo -n $1 | sha1sum | awk '{print $1}'`
		if [ -f "$tupdir/.tup/object/${sum:0:2}/${sum:2}/.name" ]; then
			:
		else
			echo "Missing object $1 from .tup/object" 1>&2
			exit 1
		fi
		shift
	done
}

tup_object_no_exist()
{
	while [ $# -gt 0 ]; do
		sum=`echo -n $1 | sha1sum | awk '{print $1}'`
		if [ -f "$tupdir/.tup/object/${sum:0:2}/${sum:2}/.name" ]; then
			echo "Object $1 exists in .tup/object" 1>&2
			exit 1
		fi
		shift
	done
}

tup_dep_no_exist()
{
	sum=`echo -n $1 | sha1sum | awk '{print $1}'`
	dep=`echo -n $2 | sha1sum | awk '{print $1}'`
	if [ -f "$tupdir/.tup/object/${sum:0:2}/${sum:2}/$dep" ]; then
		echo "Dependency from $1 -> $2 exists when it shouldn't" 1>&2
		exit 1
	fi
}
