#!/usr/bin/perl

my $filename = $ARGV[0];
open(my $fh, '<:encoding(UTF-8)', $filename);

while (my $line = <$fh> ) {
    chomp $line;
    if ($line =~ s/\\$//) {
	$line .= <$fh>;
	redo unless eof($fh);
    }
    if ($line =~ /DRAW_FUNCTION/) {
	if ($line =~ /\s*("[^"]*")\s*,\s*(\w*)\s*,\s*(\w*)\s*,\s*("[^"]*")/) {
	    printf "  {%s, %s, %s, %s, %s},\n", $1, $2, $3, $4;
	}
    }
}
