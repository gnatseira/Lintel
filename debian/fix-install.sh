#!/bin/sh
set -e
[ -d "$1" ]
TMP_USR=$1/usr

# Unclear whether lexgrog or doxygen is wrong, but lexgrog does not accept spaces 
# on the left hand side of the NAME entry.
find $TMP_USR/share/man/man3 -type f -print0 | xargs -0 perl -i -p -e 'if ($fix) { ($left, $right) = /^(.+)( \\-.+)$/; $left =~ s/ //go; $_ = "$left$right\n"; $fix = 0; }; $fix = 1 if /^.SH NAME/o;' 
chrpath -d $TMP_USR/bin/drawRandomLogNormal $TMP_USR/lib/libLintel.so.*.* $TMP_USR/lib/libLintelPThread.so.*.* $TMP_USR/lib/perl5/libLintelPerl.so