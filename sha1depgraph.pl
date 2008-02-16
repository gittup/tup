#! /usr/bin/perl

use strict;

my (@files, $file, %name_hash, $from, $to);

print "digraph g {\n";

@files = `ls .tup/*/tup.name`;
foreach $file (@files) {
	chomp($file);
	$from = $file;
	$from =~ s#\.tup/([0-9a-f]*)/tup\.name#\1#;
	open FILE, "$file" or die "Can't open $file\n";
	$name_hash{$from} = <FILE>;
	print "tup$from [label=\"$name_hash{$from}\"];\n";
	close FILE;
}

@files = `ls .tup/*/*`;
foreach $file (@files) {
	chomp($file);
	if($file =~ /\/tup\.name/) {
		next;
	}
	($from, $to) = $file =~ m#\.tup/([0-9a-f]*)/([0-9a-f]*)#;
	print "tup$to -> tup$from\n";
}

print "}\n";
