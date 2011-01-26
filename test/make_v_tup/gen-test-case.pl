#! /usr/bin/perl

use strict;

my ($num_files, $num_deps, @path_names, $x, $y, %dir_names, %mains, @paths, @pathcounts);
my @sample_paths = ("usr", "src", "linux", "mozilla", "marf", "tup", "test", "drivers", "include", "sound");

$paths[0] = "";

if($#ARGV < 0) {
	&usage();
}

$num_files = 100;
$num_deps = 7;

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

mkdir "tmake";
mkdir "ttup";
open FILE, ">ttup/Tuprules.tup" or die "Can't open ttup/Tuprules.tup for write.\n";
print FILE "TEST_TOP = \$(TUP_CWD)\n";
close FILE;
for($x=0; $x<$num_files; $x++) {
	$path_names[$x] = &generate_path($x);
	if($path_names[$x] ne "") {
		system("mkdir -p tmake/$path_names[$x]");
		system("mkdir -p ttup/$path_names[$x]");
	}
	if($dir_names{$path_names[$x]} != 1) {
		$dir_names{$path_names[$x]} = 1;
		$mains{$x} = 1;
		system("cp ../testTupfile.tup ttup/$path_names[$x]/Tupfile");
	}
}

open MAKEFILE, ">tmake/Makefile" or die "Can't open Makefile for write\n";

print MAKEFILE "all:\n";
print MAKEFILE "objs :=\n";
print MAKEFILE "progs :=\n";

for($x=0; $x<$num_files; $x++) {
	print MAKEFILE "objs += $path_names[$x]$x.o\n";
	print MAKEFILE "progs += $path_names[$x]prog\n";
	print MAKEFILE "$path_names[$x]prog: $path_names[$x]$x.o\n";
	open FILE, ">ttup/$path_names[$x]$x.c" or die "Can't open ttup/$path_names[$x]$x.c for write\n";
	for($y=0; $y<$num_deps; $y++) {
		my ($tmp);
		$tmp = ($x + $y) % $num_files;
		print FILE "#include \"$path_names[$tmp]$tmp.h\"\n";
	}
	print FILE "void func_$x(void) {}\n";
	if($mains{$x}) {
		print FILE "int main(void) {return 0;}\n";
	}
	close FILE;
	open FILE, ">ttup/$path_names[$x]$x.h" or die "Can't open ttup/$path_names[$x]$x.h for write\n";
	print FILE "void func_$x(void);\n";
	close FILE;
	system("cp ttup/$path_names[$x]$x.c tmake/$path_names[$x]$x.c");
	system("cp ttup/$path_names[$x]$x.h tmake/$path_names[$x]$x.h");
}

print MAKEFILE "progs := \$(sort \$(progs))\n";
print MAKEFILE "all: \$(progs)\n";
print MAKEFILE "deps := \$(objs:.o=.d)\n";
print MAKEFILE "-include \$(deps)\n";
print MAKEFILE "\$(progs): %: ; \$Qgcc -o \$@ \$^\n";
print MAKEFILE "%.o: %.c\n\t\$Qgcc -MMD -I. -c \$< -o \$@\n";
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
	my ($x, $num, $dir, $depth, $path);
	my ($num_files_per_dir);
	my ($num_subdirs_per_dir);

	$num_files_per_dir = 10;
	$num_subdirs_per_dir = 5;

	$num = $_[0];
	$dir = $num / $num_files_per_dir;
	$path = "";
	if($num < $num_files_per_dir) {
		# leave path empty
	} else {
		$depth = int(log($num / $num_files_per_dir) / log($num_subdirs_per_dir + 1)) + 1;
		for($x=$depth; $x>0; $x--) {
			$path .= $sample_paths[$pathcounts[$x]]."/";
		}
	}
	$pathcounts[0]++;
	if($pathcounts[0] >= $num_files_per_dir) {
		$pathcounts[1]++;
		$pathcounts[0] = 0;
	}
	for($x=1; $x<$depth+1; $x++) {
		if($pathcounts[$x] >= $num_subdirs_per_dir) {
			$pathcounts[$x] = 0;
			$pathcounts[$x+1]++;
		}
	}
	if($path =~ /^\//) {
		for($x=1; $x<$depth+1; $x++) {
			print "PC[$x]: $pathcounts[$x]\n";
		}
		die "[$num]: Path is: $path\n";
	}
	return $path;
}
