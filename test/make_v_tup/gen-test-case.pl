#! /usr/bin/perl

use strict;

my (@path_names);
my @sample_paths = ("usr", "src", "linux", "mozilla", "marf", "tup", "test", "drivers", "include", "sound");

if($#ARGV < 0) {
	&usage();
}

my $num_files = 100;
my $num_deps = 7;

while(@ARGV) {
	if($ARGV[0] eq "-n") {
		shift;
		if($#ARGV < 0) {
			&usage();
		}
		$num_files = $ARGV[0];
		shift;
	} elsif($ARGV[0] eq "-d") {
		shift;
		if($#ARGV < 0) {
			&usage();
		}
		$num_deps = $ARGV[0];
		shift;
	} else {
		print STDERR "Unknown argument: $ARGV[0]\n";
		shift;
	}
}

for(my $x=0; $x<$num_files; $x++) {
	$path_names[$x] = &generate_path($x);
}

mkdir "tmake";
mkdir "ttup";

open FILE, ">ttup/Tuprules.tup" or die "Can't open ttup/Tuprules.tup for write.\n";
print FILE "TEST_TOP = \$(TUP_CWD)\n";
close FILE;

open MAKEFILE, ">tmake/Makefile" or die "Can't open Makefile for write\n";
print MAKEFILE "all:\n";
print MAKEFILE "src :=\n";
print MAKEFILE "progs :=\n";

my $path_name = $path_names[0];
my @path_indexes = ();

for(my $x=0; $x<$num_files; $x++) {
	my $new_path_name = $path_names[$x];
	if($new_path_name ne $path_name) {
		&create_directory($path_name, @path_indexes);
		$path_name = $new_path_name;
		@path_indexes = ();
	}
	push @path_indexes, $x;
}
&create_directory($path_name, @path_indexes);

print MAKEFILE "progs := \$(sort \$(progs))\n";
print MAKEFILE "all: \$(progs)\n";
print MAKEFILE "objs := \$(src:.c=.o)\n";
print MAKEFILE "deps := \$(src:.c=.d)\n";
print MAKEFILE "-include \$(deps)\n";
print MAKEFILE "\$(progs): %: ; gcc -o \$@ \$^\n";
print MAKEFILE "%.o: %.c\n\tgcc -MMD -I. -c \$< -o \$@\n";
print MAKEFILE "clean: ; \@rm -rf \$(objs) \$(deps) \$(progs)\n";
print MAKEFILE ".PHONY: clean all\n";

close MAKEFILE;

sub usage
{
	print "Usage: $0 [-n num files] [-d num deps]\n";
	die;
}

sub generate_path
{
	use integer;
	my (@path_components);

	my $num_files_per_dir = 10;
	my $num_subdirs_per_dir = 5;

	my $num = $_[0];
	my $dirindex = $num / $num_files_per_dir;

	if ($dirindex == 0) {
		return "";
	}

	--$dirindex;

	while (1) {
		my $levelindex = $dirindex % $num_subdirs_per_dir;
		unshift(@path_components, $sample_paths[$levelindex]);
		$dirindex /= $num_subdirs_per_dir;
		if ($dirindex < 1) {
			return join("/", @path_components) . "/";
		} else {
			--$dirindex;
		}
	}
}

sub create_directory
{
	my $path_name = shift;
	my @path_indexes = @_;

	if($path_name ne "") {
		mkdir "tmake/$path_name";
		mkdir "ttup/$path_name";
	}
	system("cp ../testTupfile.tup ttup/$path_name/Tupfile");
	print MAKEFILE "progs += ${path_name}prog\n";

	my $first_file_in_directory = 1;
	foreach my $x (@path_indexes) {
		open FILE, ">ttup/$path_name$x.c" or die "Can't open ttup/$path_name$x.c for write\n";
		for(my $y=0; $y<$num_deps; $y++) {
			my $tmp = ($x + $y) % $num_files;
			print FILE "#include \"$path_names[$tmp]$tmp.h\"\n";
		}
		print FILE "void func_$x(void) {}\n";
		if($first_file_in_directory) {
			print FILE "int main(void) {return 0;}\n";
			$first_file_in_directory = 0;
		}
		close FILE;
		system("cp ttup/$path_name$x.c tmake/$path_name$x.c");

		open FILE, ">ttup/$path_name$x.h" or die "Can't open ttup/$path_name$x.h for write\n";
		print FILE "void func_$x(void);\n";
		close FILE;
		system("cp ttup/$path_name$x.h tmake/$path_name$x.h");

		print MAKEFILE "src += $path_name$x.c\n";
		print MAKEFILE "${path_name}prog: $path_name$x.o\n";
	}
}
