package Lintel::ProcessManager;

use strict;
use warnings;
use vars qw/@EXPORT_OK/;

use Exporter 'import';
@EXPORT_OK = qw/maskCoredump/;

use Carp;
use POSIX qw(:sys_wait_h setpgid);
use Time::HiRes 'time';

# TODO: write better tests for this module (test-lintel-processmanager only covers a little of the
# functionality).

=pod

=head1 NAME

Lintel::ProcessManager - library for dealing with sub-processes

=head1 SYNOPSIS

    use Lintel::ProcessManager; # 'wcoredump'

    my $process_manager 
        = new Lintel::ProcessManager([debug => 0|1],
                                     [auto_kill_on_destroy => 0|1 (default)]);


    my $pid = $process_manager->fork(cmd => '...' | [...] | sub { ... },
                                     [setpgid => 0|1,]
                                     [stdout => '/path',]
                                     [stderr => '/path|STDOUT',]
                                     [exitfn => sub { my ($pid, $status) = @_; ... }]);

    my %pid_to_status = $process_manager->wait([timeout]);

    my %pid_to_status = $process_manager->waitAll();

    my $status = $process_manager->waitPid($pid);

    $process_manager->enableSignals([sub { my ($pm) = @_; 'should-call-wait' }]);

    my @pids = $process_manager->children();

    my $nchildren = $process_manager->nChildren();

    my $new_status = Lintel::ProcessManger::wcoredump($status); # mask out core dump status

=head1 FUNCTIONS
    
=head2 my $mgr = new Lintel::ProcessManager(%options)

Create a new process manager.  You can pass a list of options to
control how the process manager will behave.  Right now there are two
options you can specify: debug, and auto_kill_on_destroy.  The debug
option will turn on a few additional bits of output, and is really for
developers.  auto_kill_on_destroy tells the process manager what to do
when it is destroyed, but still have active sub-processes.  By default
the answer is it prints a warning and sends a term signal to all of
the sub-processes.  Setting the value to 1 disables the warning, and 0
disables the killing.

=cut

# TODO: since you can't sanely have two process managers in a single process
# (wait on one can finish children the other should process), we should have
# this return a reference to a global and print out a warning that people
# should just use the global.
sub new {
    my ($class, %opts) = @_;

    $opts{children} = {};
    $opts{in_signal_handler} = 0;

    $SIG{CHLD} = sub { };
    return bless \%opts, $class;
}

sub DESTROY {
    my ($this) = @_;

    if ($this->nChildren() > 0) {
	unless (defined $this->{auto_kill_on_destroy}) {
	    warn "Lintel::ProcessManager($$): deleting process manager with children still present:\n";
	    while (my ($k, $v) = each %{$this->{children}}) {
		my $command = 'unknown';
		if (ref $v->{cmd} eq 'ARRAY') {
		    $command = join(" ", @{$v->{cmd}});
		} elsif (ref $v->{cmd} eq 'CODE') {
		    $command = "Function: $v->{cmd}";
		}
		warn "  pid $k, command $command\n";
		warn "Automatically killing children";
	    }
	    $this->{auto_kill_on_destroy} = 1;
	}
	if ($this->{auto_kill_on_destroy}) {
	    foreach my $pid ($this->children()) {
		kill ('TERM', $pid) 
		    or warn "Unable to kill $pid process group: $!";
	    }
	}
    }
}

sub waitForFile {
    my ($this, $path, $pid) = @_;

    return unless defined $path;
    die "internal" unless defined $pid;
    print "Lintel::ProcessManager($$): Wait for file $path to be created\n" if $this->{debug};
    for(my $start = time; ! -f $path; ) {
	my $delay = time - $start;
	if ($delay >= 3) {
	    warn "Waited too long ($delay seconds) for fork to make $path";
	    die "Process $pid has exited; giving up."
		unless kill(0, $pid) >= 1;
	    die "Waited way too long; giving up."
		if $delay > 30;
	    select(undef,undef,undef,0.25); # sleep longer now.
	} else {
	    select(undef,undef,undef,0.01);
	}
    }
}

=pod

=head2 $mgr->fork(%options)

Create a new sub-process.  The options control what process is created
and how it operates.  Options include:

=over 4

=item cmd => '...' | [...] | sub { ... }

What command should be executed in the sub-process.  This option can
be either a single string, which is then passed to exec, an array
reference which is dereferenced and passed to exec, or a subrouting
which is executed, and if it returns, the process will exit(0).  The
choice between the string and array reference is to control whether
the command is run through a shell (string) or not (array ref).

=item setpgid => 0|1

Should the sub-process execute setpgid after forking.  If yes, this process will become a new
process group leader, and so can be killed as a group.  I.e. if the sub-process forks children, you
can run kill -1, $pid; and it will kill both the process and its children.  The default, 0 leaves
the sub process part of the parent's group.

=item stdout => 'path'

What path (if any) should stdout be redirected to for the sub process.

=item stderr => 'path|STDOUT'

What path (if any) should stderr be redirected to for the sub process.
If the special value STDOUT is specified, then stderr is set to be
stdout.

=item exitfn => sub { my ($pid, $status) = @_; ... }

What function should be called when the sub-process exits.  The
process will be given both the process id that exited and the status
of the exit.  It can take any desired action.  Exit functions will be
called during an execution of the wait() method.

=item max_mem_bytes => <nbytes>

This option uses BSD::Resource if it is available to set the core limit
for the process to be started.   If BSD::Resource is not available then
specifying this option will raise an error through die().

=back

=cut

sub fork {
    my ($this, %opts) = @_;

    confess "missing cmd option" unless defined $opts{cmd};

    my $cmd = $opts{cmd};
    $cmd = join(' ', @$cmd) if ref $cmd eq 'ARRAY';
    $cmd = 'FUNCTION' if ref $cmd eq 'CODE';

    if ($this->{debug}) {
	my $stdout = $opts{stdout} || 'STDOUT';
	my $stderr = $opts{stdout} || 'STDERR';
	print "Lintel::ProcessManager($$): Running '$cmd', stdout=$stdout, stderr=$stderr\n";
    }

    if (defined $opts{stdout} && -f $opts{stdout}) {
	unlink($opts{stdout}) || confess "Can't delete $opts{stdout}: $!";
    }
    if (defined $opts{stderr} && -f $opts{stderr} && $opts{stderr} ne 'STDOUT') {
	unlink($opts{stderr}) || confess "Can't delete $opts{stdout}: $!";
    }
    my $pid = fork;
    confess "fork failed: $!" unless defined $pid && $pid >= 0;
    print "Lintel::ProcessManager($$): post-fork $pid\n" if $this->{debug};

    if ($pid == 0) { 
	print "Lintel::ProcessManager($$): Child -- $cmd\n" if $this->{debug};
	$this->{children} = {};
	if ($opts{setpgid}) {
	    print "setpgid -- $$\n" if $this->{debug};
	    # Make us a separate process group so we can kill the entire
	    # set of children (if any) as a single group.
	    setpgid(0, 0) or confess "Can't setpgid: $!";
	}
	if (defined $opts{stdout}) {
	    open(STDOUT, ">$opts{stdout}")
		or die "Can't write to $opts{stdout}: $!";
	}
	if (defined $opts{stderr}) {
	    if ($opts{stderr} eq 'STDOUT') {
		open(STDERR, ">&STDOUT")
		    or confess "Can't dup stdout: $!";
	    } else {
		open(STDERR, ">$opts{stderr}")
		    or die "Can't write to $opts{stderr}: $!";
	    }
	}

        if (defined $opts{max_mem_bytes}) {
            eval q#use BSD::Resource;
                   setrlimit(RLIMIT_VMEM, $opts{max_mem_bytes}, RLIM_INFINITY)
                       || die "setrlimit failed: $!";#;
	    die "Can not support max_mem_bytes without BSD::Resource module: $@" if $@;
        }

	print STDERR "Lintel::ProcessManager($$): exec $cmd\n" if $this->{debug};

	if (ref $opts{cmd} eq 'ARRAY') {
	    exec @{$opts{cmd}};
	} elsif (ref $opts{cmd} eq 'CODE') {
	    my $fn = $opts{cmd};
	    &$fn;
	    exit(0);
	} else {
	    exec $opts{cmd};
	}

	confess "exec($cmd) failed: $!";
    }

    print "Lintel::ProcessManager($$): Parent ($pid)\n" if $this->{debug};
    $this->waitForFile($opts{stdout}, $pid);
    $this->waitForFile($opts{stderr}, $pid)
	unless defined $opts{stderr} && $opts{stderr} eq 'STDOUT';

    $this->{children}->{$pid} =	{
				 exitfn => $opts{exitfn},
				 cmd => $opts{cmd}
				};

    return $pid;
}

=pod

=head2 my $new_status = maskCoredump($status)

The maskCcoredump function will mask out the core dump status from the $status variable, returning
the revised status.  If possible, it attempts to use the POSIX::WCOREDUMP function, but since that
function is often not present, it has hardcoded the flag value for masking.

=cut

sub maskCoredump {
    confess "wcoredump called incorrectly; first argument should be defined"
        unless defined $_[0];
    my $ret = eval q!&POSIX::WCOREDUMP($_[0]);!;
    if ($@ || !defined $ret) { # Assume it was function not found.
        # TODO: use a binary module to get this out of C
        return $_[0] & ~0x80;
    } else {
        return $_[0] & ~$ret;
    }
}

=pod

=head2 my %pid_to_status = $mgr->wait([timeout])

Wait for children to exit up to timeout seconds.  Return a map from
process id to status for each child that exited.  If an exitfn was
specified for each pid, then the function will be called.  It is an
error to call this from within the process manager signal handler, or
without any children.

=cut

sub wait {
    my ($this, $timeout) = @_;

    confess "Can't call wait from inside signal handler -- race condition"
	if $this->{in_signal_handler};

    my %exited;
    my $started = time;
    my $exit_count = 0;

    my $handle_exit = sub {
        my ($child, $status) = @_;

        # TODO: add a test where we exit with lots of different status; it's possible
        # we need to set $SIG{CHLD} = 'IGNORE' or something, but it seems to be working.
        print "Lintel::ProcessManager($$): exit($child) -> $status\n" if $this->{debug};

        $exited{$child} = $status;
        ++$exit_count;

        unless (exists $this->{children}->{$child}) {
            warn "Unexpected pid $child exited with status $status";
        } else {
            my $fn = $this->{children}->{$child}->{exitfn};
            delete $this->{children}->{$child}; # in case fn calls wait
            &$fn($child, $status) if defined $fn;
        }
    };

    while ($exit_count == 0) {
	confess "Can't call wait without any children" 
	    unless $this->nChildren() > 0;
	while ((my $child = waitpid(-1, WNOHANG)) > 0) {
            &$handle_exit($child, $?);
	}
	if ($exit_count == 0) {
            # Added 'No child processes' check in because in at least one case, we got stuck in an
            # infinite loop waiting for a child to exit when there was no sub-process.
	    if ($! eq 'No child processes') {
                # Added can_signal check because we had cases (debian etch 32bit user, 64bit
		# kernel) where 'no child processes' was returned and yet, there were children as
		# per the pstree
		my @can_signal;

		foreach my $child ($this->children()) {
		    push(@can_signal, $child) if kill 0, $child;
		}

                # Added this check because on openbsd in a VM we seemed to have a race between
                # the waitpid above and the can_signal test, but on openbsd we can not send a 0
                # signal to a zombie process.  However, if any zombies are there, then the call
                # to waitpid below should succeed.
                if (@can_signal == 0) {
                    my $child = waitpid(-1, WNOHANG);
                    if ($child > 0) {
                        &$handle_exit($child, $?);
                        push(@can_signal, $child);
                    }
                }

		if (@can_signal == 0) {
		    if ($this->{debug}) {
			print "Lintel::ProcessManager($$): pstree-begin\n";
			system("pstree -p $$") == 0
                            || system("ps jx"); # openbsd
			print "Lintel::ProcessManager($$): pstree-end\n";
		    }

		    confess "Lintel::ProcessManager($$): Internal error, wait returned 'no children', can not signal any children, yet child list is still: (" . join(", ", $this->children()) . "), and exit count is 0";
		}
	    }
	    last if defined $timeout && time >= $started + $timeout;
	    select(undef, undef, undef, 0.1);
	}
    }

    return %exited;
}

=pod

=head2 my %pid_to_status = $mgr->waitAll()

Wait for all children to exit, return their status.

=cut

sub waitAll {
    my ($this) = @_;

    my %ret;
    while ($this->nChildren() > 0) {
	my %some = $this->wait();
	map { $ret{$_} = $some{$_} } keys %some;
    }

    return %ret;
}

=pod

=head2 my $status = $mgr->waitPid($pid)

Wait for a specific process to exit, return its status.  Note: any other
process may exit while waiting for this pid, and the exit functions will be
called.  Similarly, if you specify an exit function when creating $pid, it will
be called before this function exits.  Will return undef if $pid isn't
currently a child.

=cut

sub waitPid {
    my ($this, $pid) = @_;

    unless (exists $this->{children}->{$pid}) {
	print "Missing child $pid; children: ", join(", ", $this->children()), "\n" if $this->{debug};
	return undef;
    }

    while (exists $this->{children}->{$pid}) {
	my %pid_to_status = $this->wait();
	return $pid_to_status{$pid} if defined $pid_to_status{$pid};
    }
    die "Internal error: had child $pid at start, but vanished without getting status.";
}

=pod

=head2 $mgr->enableSignals(sub { my ($mgr) = @_; ... })

Specify a function to call when a child exits.  This notifies the
application that it should call wait shortly.  It is an error to call
wait() within the handler.  The reason is that there is a race
condition between the signal handler being called and the data
structures being set up to handle the sub-process exiting.

=cut

sub enableSignals {
    my ($this, $fn) = @_;

    die "?" if defined $this->{signal_handler_fn};

    $this->{signal_handler_fn} = sub {
	confess "signal recursion" if $this->{in_signal_handler};
	++$this->{in_signal_handler};

	&$fn if defined $fn;

	--$this->{in_signal_handler};
	$SIG{CHLD} = $this->{signal_handler_fn};
    };

    $SIG{CHLD} = $this->{signal_handler_fn};
}

=pod

=head2 my @pids = $mgr->children()

Return an array of the process id's of all the sub-processes

=cut

sub children {
    my ($this) = @_;

    return keys %{$this->{children}};
}

=pod

=head2 my $count = $mgr->nChildren()

Return the number of currently active children, i.e. ones that were
started but have not yet exited or been reaped (have exited but wait() has
not been called).

=cut

sub nChildren {
    my ($this) = @_;

    return scalar $this->children();
}

1;

