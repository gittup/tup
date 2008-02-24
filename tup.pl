#! /usr/bin/perl

my (%stack, %visited, @list, $f, @circ, %count, %deps, %cmds);

@list = `ls .tup/attrib/`;
push(@list, `ls .tup/modify/`);
foreach (@list) {chomp;}

print "digraph G {\n";
print "tupdir [label=\".tup\"];\n";
@{$deps{".tup"}} = @list;
foreach $f (@list) {
	print STDERR "$f\n";
	&dfs($f);
}

$count{".tup"} = 1;
&rebuild(".tup");

system("rm -f .tup/attrib/* .tup/modify/*");
print "}\n";

sub dfs
{
	my ($cur, @dlist, $d, $name);
	$cur = $_[0];

	if($stack{$cur} == 1) {
		print STDERR "Circular dependency detected!\n";
		foreach (@circ) {
			print STDERR "  $_\n";
		}
		print STDERR "  $cur\n";
		die;
	}
	$count{$cur}++;
	if($visited{$cur} == 1) {
		return;
	}
	$name = `cat .tup/object/$cur/name`;
	chomp($name);
	print "tup$cur [label=\"$name\"];\n";
	if(-f ".tup/object/$cur/cmd") {
		$cmds{$cur} = `cat .tup/object/$cur/cmd`;
		chomp($cmds{$cur});
	}

	push(@circ, $cur);

	$stack{$cur} = 1;
	$visited{$cur} = 1;
	print STDERR "Visit: $cur\n";

	@dlist = `ls .tup/object/$cur 2>/dev/null`;
	foreach $d (@dlist) {
		chomp($d);
		if($d =~ /name/ || $d =~ /cmd/) {
			next;
		}
		push(@{$deps{$cur}}, $d);
		&dfs($d);
	}

	$stack{$cur} = 0;
	pop(@circ);
}

sub rebuild
{
	my ($f, $d);

	$f = $_[0];
	$count{$f}--;
	if($count{$f} == 0) {
		if(exists $cmds{$f}) {
			print STDERR "Execute $cmds{$f}\n";
			if(system("../../wrapper $cmds{$f}\n") != 0) {
				die "Failed to run $cmds{$f}\n";
			}
		} else {
			print STDERR "No cmd for $f\n";
		}
		foreach $d (@{$deps{$_[0]}}) {
			if($f eq ".tup") {
				print "tup$d -> tupdir [dir=back]\n";
			} else {
				print "tup$d -> tup$f [dir=back]\n";
			}
			&rebuild($d);
		}
	} elsif($count{$f} > 0) {
		print STDERR "Skipping $f - count >0\n";
	} else {
		die "Count $f < 0??\n";
	}
}
