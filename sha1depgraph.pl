#! /usr/bin/perl

use strict;

my (@files, $file, %name_hash, $from, $to, %color_hash, @circ_list, %visited, %stack);

print "digraph g {\n";

@files = `ls .tup/object/*/name 2>/dev/null`;
foreach $file (@files) {
	chomp($file);
	$from = $file;
	$from =~ s#\.tup/object/([0-9a-f]*)/name#\1#;
	open FILE, "$file" or die "Can't open $file\n";
	$name_hash{$from} = <FILE>;
	chomp($name_hash{$from});
	close FILE;
}

@files = `ls .tup/object/*/*`;
foreach $file (@files) {
	chomp($file);
	if($file =~ /\/name/ || $file =~ /\/cmd/) {
		next;
	}
	($from, $to) = $file =~ m#\.tup/object/([0-9a-f]*)/([0-9a-f]*)#;
	print "tup$to -> tup$from [dir=back];\n";
}

@files = `ls .tup/attrib/* .tup/modify/* 2>/dev/null`;
foreach $file (@files) {
	chomp($file);
	($from) = $file =~ m#\.tup/....../([0-9a-f]*)#;
	%visited = ();
	%stack = ();
	@circ_list = ();
	&follow_chain($from);
}

foreach $from (keys %name_hash) {
	if($color_hash{$from}) {
		print "tup$from [label=\"$name_hash{$from}\" color=\"$color_hash{$from}\"];\n";
	} else {
		print "tup$from [label=\"$name_hash{$from}\"];\n";
	}
}

print "}\n";

sub follow_chain
{
	my ($f, @list, $dep);

	$f = $_[0];
	push(@circ_list, $f);

	if($stack{$f} == 1) {
		print STDERR "Error: Circular dependency detected:\n";
		foreach $f (@circ_list) {
			if(exists $name_hash{$f}) {
				print STDERR "  $name_hash{$f}\n";
			} else {
				print STDERR "  $f\n";
			}
		}
		die;
	}
	if($visited{$f} == 1) {
		return;
	}
	$visited{$f} = 1;
	$stack{$f} = 1;

	$color_hash{$f} = "red";
	@list = `ls .tup/object/$f/* 2>/dev/null`;
	foreach $dep (@list) {
		if($dep =~ /\/name/ || $dep =~ /\/cmd/) {
			next;
		}
		($dep) = $dep =~ m#\.tup/object/$f/([0-9a-f]*)#;
		&follow_chain($dep);
	}
	$stack{$f} = 0;
}
