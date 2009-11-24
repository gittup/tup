#! /usr/bin/perl
$num = 3;
$cur = 2;
$tup = 2 + ($num * 3 * 3) + 3 + 3;
&gen_data("initial", $cur, $cur + $tup, 1);
$cur += 4;
&gen_data("c_file", $cur, $cur + $tup, $num);
$cur += $num * 3 + 1;
&gen_data("h_file", $cur, $cur + $tup, $num);
$cur += $num * 3 + 1;
&gen_data("nothing", $cur, $cur + $tup, $num);

sub gen_data
{
	@arr = (1, 10, 100, 1000, 10000, 100000);
	open OUTPUT, ">plot-$_[0].dat" or die "Can't open $_[0].data for write!";
	foreach $val (@arr) {
		open FILE, "out-$val.txt" or die "Can't open out-$val.txt!";
		for($x=1; $x<$_[1]; $x++) {
			$tmp = <FILE>;
		}
		@vals = ();
		for($x=0; $x<$_[3]; $x++) {
			$tmp = <FILE>;
			$tmp =~ s/real //;
			push @vals, $tmp;
			$tmp = <FILE>;
			$tmp = <FILE>;
		}
		@sort_vals = sort @vals;
		# Take the median
		$val1 = $sort_vals[$#sort_vals / 2];
		close FILE;

		open FILE, "out-$val.txt" or die "Can't open out-$val.txt!";
		for($x=1; $x<$_[2]; $x++) {
			$tmp = <FILE>;
		}
		@vals = ();
		for($x=0; $x<$_[3]; $x++) {
			$tmp = <FILE>;
			$tmp =~ s/real //;
			push @vals, $tmp;
			$tmp = <FILE>;
			$tmp = <FILE>;
		}
		@sort_vals = sort @vals;
		# Take the median
		$val2 = $sort_vals[$#sort_vals / 2];
		close FILE;
		chomp($val1);
		chomp($val2);
		print OUTPUT "$val $val1 $val2\n";
	}
	close OUTPUT;
}
