#! /usr/bin/perl

use strict;

my (@dirs, @files, $dir, $dirbase, $file, %file_hash);

print "digraph g {\n";
@dirs = `find . -name "*.tupd" -type d`;
foreach $dir (@dirs) {
	chomp($dir);
	$dirbase = $dir;
	$dirbase =~ s/\.\/(.*)\.tupd/\1/;
	&add_file($dirbase);
	@files = `find $dir -type f`;
	foreach $file (@files) {
		my ($tmp);
		chomp($file);
		$tmp = $file;
		$tmp =~ s/$dir\///;
		&add_file($tmp);
		print &dotify($tmp) . " -> " . &dotify($dirbase) . "\n";
	}
}
print "}\n";

sub add_file
{
	my ($file);
	$file = $_[0];

	if($file_hash{$file} != 1) {
		$file_hash{$file} = 1;
		print &dotify($file) . " [label=\"$file\"];\n";
	}
}

sub dotify
{
	my ($tmp);
	$tmp = $_[0];
	$tmp =~ s#[./]#_#g;
	return $tmp;
}
