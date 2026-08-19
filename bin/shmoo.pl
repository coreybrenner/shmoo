#!/bin/sh
#! -*- perl -*- -w
# This is actually a Bourne Shell script which becomes Perl,
# if Perl was not executed explicitly.  Done this way so that
# the interpreter itself enforces '-w' and other options from
# from the line above.
eval 'exec perl -x -wS $0 ${1+"$@"}'
    if 0;

use strict;
use warnings;

use Cwd;
use File::Spec;

sub unique (@);
sub which (@);
sub islink ($);
sub probe_link_sequence ($);
sub get_interp_info ($);
sub get_script_info ($);

sub main (@) {
    my @argv = @_;

    my %interp = get_interp_info(\%ENV);
    my %script = get_script_info(\%ENV);

}
exit main(@ARGV)

use Inline C => <<'END_C';
void get_interp_argv (void) {
    PPCODE:
    {
        EXTEND(SP, PL_origargc);
        for (int i = 0; i < PL_origargc; ++i) {
            PUSHs(sv_2mortal(newSVpv(PL_origargv[i], 0)));
        }
        XSRETURN(PL_origargc);
    }
}
END_C

sub unique (@) {
    my %seen; grep { ! $seen{$_}++ } @_
}

sub which (@) {
    my %rslt;
    my @path = File::Spec->path();
    foreach my $find (@_) {
        my @look;
        if (File::Spec->file_name_is_absolute($find)) {
            if ((@_ == 1) && ! wantarray) {
                return $find;
            } elsif (-x $find) {
                $rslt{$find} = $find;
            } else {
                $rslt{$find} = undef;
            }
            next;
        } elsif (grep { $_ } File::Spec->splitdir((File::Spec->splitpath($find))[1])) {
            @look = (Cwd::cwd());
        } else {
            @look = @path;
        }
        $rslt{$find} = undef;
        foreach my $path (@look) {
            my $look = File::Spec->catfile($path, $find);
            if (-x $look) {
                $rslt{$find} = $look;
                last;
            }
        }
    }

    return %rslt;
}

if (($^O eq 'MSWin32') || ($^O eq 'cygwin') || ($^O eq 'msys')) {
    sub islink ($) {
        my $look = shift;
        if (-l $look) {
            return readlink($look);
        } elsif (! -f $look) {
            return undef;
        } elsif ($look =~ m{\.lnk$}io) {
        } 
    }
} else {
    sub islink ($) {
        my $look = shift;
        return ((-l $look) ? readlink($look) : undef);
    }
}

sub probe_link_sequence ($) {
    my $prog = shift;

    my @work = ($prog);
    while (@work) {
        my $file = shift(@work);
        push(@link, $file);
        my $link = islink($file);
        last if (! defined($link));
        if (! File::Spec->file_name_is_absolute($link)) {
            my ($vol, $dir, undef) = File::Spec->splitpath($file);
            $link = File::Spec->catpath($vol, $dir, $link);
        }
        unshift(@work, $link);
    }

    return @link;
}

sub get_interp_info ($) {
    my $env = shift;

    my $full = which($^X);
    my @link = probe_link_sequence($full);

    my @argv = get_interp_argv();
    my @work = @argv;
    my %opts;
    while (@work) {
        if ($work[0] eq '--') {
            shift(@work);
            last;
        } elsif ($work[0] !~ m{^\-}o) {
            last;
        }

        my $arg = shift(@work);
        for ($arg =~ s{^\-}{}o; $arg; ) {
            my $opt;
            if ($arg =~ m{^(?:[acfgh\?npsStTuUwWX]|V(?=[^\:])|d(?![t\:]))}o) {
                ($opt, $arg) = ($arg =~ m{^(.)(.*+)$}o);
                ++$opts{"-$opt"};
            } elsif ($arg =~ m{^0([0-3][0-7]{2}|[0-7]{1,2}|x[0-9a-f]{1,4})?+$}io) {
                if (! $1) {
                    if ($work[0] !~ m{^(?:[0-3][0-7]{2}|[0-7]{1,2}|x[0-9a-f]{1,4})$}io) {
                        die("$0: Could not match octal or hex digits for -0\n");
                    }
    
            } elsif ($arg =~ m{^[xVmMlIiFeEDC0]|d(?=[t\:])
                if ($arg =~ m{^
        }
        if ($arg =~ m{^\-[gsTtuUWXh\?vcw
        if ($arg =~ m{^\-
    }

    my $opts = ($env->{PERL5OPT} // '');
    my $libs = ($env->{PERL5LIB} // $env->{PERLLIB} // '');

    if (! $opts || ($opts =~ m{^\-[tT]}o)) {
    }



    return (
        environ => { %{$env} },
        workdir => Cwd::cwd(),
        arglist => \@argv,
        varname => '^X',
        vartext => $^X,
        program => $full,
        linkseq => \@link,
        tainted => ${^TAINT},
    );
}

sub get_script_info ($) {
    my $env = shift;

    return (
        environ => { %{$env} },
        workdir => 
}

