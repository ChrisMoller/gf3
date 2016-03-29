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
	    printf "void %s (GdkEvent *event, sheet_s *sheet, gdouble px, gdouble py);\n", $2;
	}
    }
}
