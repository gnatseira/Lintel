package BatchParallel::jobsfile;

use Sys::Hostname;
die "module version mismatch" 
    unless $BatchParallel::common::interface_version < 2;

@ISA = qw/BatchParallel::common/;

sub usage {
    print <<END_OF_USAGE;
batch-parallel jobsfile [printcmd] [maxjobs=#] -- file output-dir
  printcmd: include command in output-dir/<line#>

  - The file should specify the command to execute 1 job/line.
  - Lines with a # at the front of them will be ignored (but counted
    in the line count)
  - The output directory will be created if it doesn't already exist.
  - Stdout and stderr for each job will be placed in output-dir/<line#>
  - Jobs that return an error, or fail to complete because the host crashes
    will have their output in output-dir/<line#>-hostname-pid
  - if a line# file already exists, the job will not be re-executed
  - line#'s count starting from 1
  - if maxjobs=# is specified, it will be used in setting the output file
    format to have the right number of leading digits (including leading 0's)
    so that adding jobs to the file won't cause the output files to re-sort. 
    If not specified, jobsfile will use max(3, ceiling(log_10(#lines))) digits.
END_OF_USAGE
}

# TODO: add an option [skip-queued=<seconds>] that will write a .queued file out
# when starting the jobs, and will skip queueing any jobs that have a .queued file
# more recent than <seconds>

sub new {
    my $class = shift;
    
    my $this = { };
    foreach my $arg (@_) {
	if ($arg eq 'help') {
	    usage();
	    exit(0);
	} elsif ($arg eq 'printcmd') {
	    $this->{printcommand} = 1;
        } elsif ($arg =~ /^maxjobs=(\d+)$/o) {
            $this->{maxjobs} = $1;
	} else {
	    usage();
	    die "unable to interpret argument '$arg'."
	}
    }
    return bless $this, $class;
}

sub find_things_to_build {
    my($this,@dirs) = @_;

    unless (@dirs == 2) {
	usage();
	die "incorrect number of arguments";
    }
    my ($joblist,$outdir) = @dirs;
    
    $|=1;
    print "Reading in input jobs...";
    open(JOBSLIST,$joblist) 
	or die "Can't open $joblist for read: $!";
    unless(-d $outdir) {
	mkdir($outdir,0755) or die "can't mkdir $outdir: $!";
    }
    my $linenum = 0;
    my @defines;
    my @possibles;
    my %define_subs;
    while(<JOBSLIST>) {
	++$linenum;
	if (/^#define/o) {
	    chomp;
	    die "Bad define on line $linenum"
		unless /^#define\s+(\w+)\s+(.+)$/o;
	    my($var, $repl) = ($1,$2);
	    $define_subs{$var} = $repl;
	    next;
	}
	next if /^#/o;
	s/[\r\n]$//o;
	while(my($var,$val) = each %define_subs) {
	    s/\b$var\b/$val/g;
	}
	push(@possibles,[$linenum,$_]);
    }
    close(JOBSLIST) or die "close failed: $!";
    print "done.\n";
    my $digits = 3; # default so that adding a few lines isn't likely to push the count over
                    # a power of 10 boundary.
    if (defined $this->{maxjobs}) {
        $digits = 1; # user specified, use their constant
    } else {
        $this->{maxjobs} = $linenum;
    }
    while((10 ** ($digits)-1) < $this->{maxjobs}) { 
	++$digits;
    }
    my $outfile_fmt = "%0${digits}d";
    
    print "checking for existing jobs...";
    my @jobs;
    for(my $i = 0; $i < @possibles; ++$i) {
	print "." if ($i % 100) == 0;
	my ($linenum,$cmd) = @{$possibles[$i]};
	my $outfile = sprintf("%s/$outfile_fmt",$outdir,$linenum);
	next if -f $outfile;
	push(@jobs,[$outfile,$cmd]);
    }
    return ($linenum,@jobs);
}

sub rebuild_thing_message {
    my($this,$thing_info) = @_;

    my($outfile,$cmd) = @$thing_info;

    print "should run $cmd with output to $outfile\n";
}

# you will be forked before this is called; you should call exit to
# complete the function, it is an error to return.

sub rebuild_thing_do { 
    my($this,$thing_info) = @_;

    my($outfile,$cmd) = @$thing_info;

    if (-f $outfile) {
        print "Skipping '$cmd'; $outfile already exists\n";
        exit(0);
    }
    $cmd = "($cmd)";
    my $tmpoutfile = "$outfile-" . hostname() . "-$$";
    if ($this->{printcommand}) {
	open(OUT,">$tmpoutfile") or die "Can't open $tmpoutfile for write: $!";
	print OUT "command: $cmd\n" or die "print failed: $!";
	close(OUT) or die "close failed: $!";
	$cmd .= ">>$tmpoutfile 2>&1";
    } else {
	$cmd .= " >$tmpoutfile 2>&1";
    }
    print "Running '$cmd'\n";
    my $ret = system($cmd);
    if ($ret == 0) {
        if (-f $outfile) {
            print "WARNING: $outfile appeared while we were running '$cmd'; leaving around $tmpoutfile\n";
        } else {
            rename($tmpoutfile, $outfile)
                or die "Can't rename $tmpoutfile to $outfile: $!";
        }
	exit(0);
    } else {
	rename($tmpoutfile, "$tmpoutfile-err");
    }
    warn "FAILED: $cmd\n";
    exit(1);
};

sub rebuild_thing_success {    
    my($this,$thing_info) = @_;

    return;
}

sub rebuild_thing_fail {
    my($this,$thing_info) = @_;

    return;
}
	    
1;

    
