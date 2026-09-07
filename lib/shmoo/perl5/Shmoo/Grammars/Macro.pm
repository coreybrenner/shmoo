package Shmoo::Grammars::Macro;

use strict;
use warnings;

use parent 'Shmoo::Grammar';
use Scalar::Util qw{blessed};
use overload (
    '""'  => sub { my $text = (shift)->{grammar}; return "$text"   },
    'qr'  => sub { my $expr = (shift)->{grammar}; return qr{$expr} },
    '&{}' => sub {
        my $gram = shift;
        my @rslt;
        if (! @_) {
            die(__PACKAGE__."::overload(&{}): expected ([parser,] text); got nothing\n");
        } elsif (blessed($_[0]) && $_[0]->isa('Shmoo::Parser')) {
            # Temporarily interpose a different parser.
            my $pars = $gram->{parser};
            $gram->{parser} = shift;
            @rslt = (shift =~ m{$gram ^(?> (?&macro) )$}o);
            $gram->{parser} = $pars;
        } else {
            @rslt = (shift =~ m{$gram ^(?> (?&macro) )$}o);
        }
        return @rslt;
    },
);

use Shmoo::Parser::Skip;

use Data::Dumper;

sub new {
    my $what = shift;
    my $type = (ref($what) || $what);
    my $orig = (ref($what) && $what);
    my %conf = @_;

    my $override_rules = ($conf{override_rules} // {});

    sub encode ($$) {
        my ($not, $more) = @_;
        foreach my $char (split('', $_[0])) {
            if ((ord($char) < 0x20) || (ord($char) == 0x7F)) {
                $char = sprintf("\\x%02X", ord($char));
            } elsif ((ord($char) >= 0x20) && (ord($char) < 0x7F)) {
                if ($char !~ m{[a-zA-Z0-9_]}o) {
                    $char = ('\\' . $char);
                }
            } elsif (ord($char) > 0xFFFF) {
                $char = sprintf("\\U%08X", ord($char));
            } elsif (ord($char) > 0xFF) {
                $char = sprintf("\\u%04X", ord($char));
            } else {
                $char = sprintf("\\x%02X", ord($char));
            }
            $more .= $char;
        }
        return $not.$more;
    }

    my $argsep = ($conf{argsep} || ',');
    my %breaking_chars = (
        literal => qq{${argsep}"'`(){}[]<>$},
    );
    $argsep = encode($argsep);
    if (my $add_breaker = $conf{add_breaker}) {
        if (ref($add_breaker) eq 'HASH') {
            foreach my $name (sort(keys(%breaking_chars))) {
                my $more;
                next if (! ($more = $add_breaker->{$name}));
                if (ref($more) eq 'ARRAY') {
                    $more = join('', @{$more});
                } elsif (ref($more)) {
                    die(__PACKAGE__."::new: ->{add_breaker}{$key} = $more;"
                        . " expected STRING or ARRAY(of strings)\n");
                }
                $breaking_chars{$key} .= encode($breaking_chars{$key}.$more);
            }
        } else {
            my $more;
            if (ref($add_breaker) eq 'ARRAY') {
                $more = join('', @{$add_breaker});
            } elsif (! ref($add_breaker)) {
                $more = $add_breaker;
            } else {
                die(__PACKAGE__."::new: ->{add_breaker} = $add_breaker;"
                    . " expected STRING or ARRAY(of strings)\n");
            }
            $breaking_chars{literal} .= encode($breaking_chars{literal}.$more);
        }
    }
    # compile breaking chars regexes.
    foreach my $key ($sort(keys(%breaking_chars))) {
        my $val = $breaking_chars{$key};
        my @val = (
            encode('^', $val),
            encode('',  $val),
        );
        $breaking_chars{$key} = {
            accept => qr{$val[0]}o,     # Accepting: [^\\\(\)...]++
            reject => qr{$val[1]}o,     # Rejecting: [\\\(\)...]
        };
    }

    my $opener = ($conf{opener_token} || '$[');
    my $closer = ($conf{closer_token} || ']' );
    if ($override_rules->{word}) {
        # Ensure that ',' will break the overridden word rule at the top rung.
        # Each descent into a group should utilize the given word parser's native
        # view of what a word is.
        my $rule = $override_rules->{word}->new(add_breaker => { literal => (','.$closer) });
        if ($rule) {
            $override_rules->{word} = $rule;
        }
    }
    $opener = encode($opener);
    $closer = encode($closer);

    my $parser; # forward declaration.
    if (! ($parser = $conf{parser})) {
        $parser = Shmoo::Parser::Skip->new();
    }

    my $self = bless({ parser => $parser }, $type);

    sub toke {
        my $parser = $self->parser();
        if (! @_) {
            return $parser->curr_token();
        } else {
            my ($str, $pos, $typ, @args) = @_;
            return $parser->push_token($str, $pos, ref($parser)."::$typ", @args);
        }
    }
    sub drop () {
        my $parser = $self->parser();
        return $parser->drop_token();
    }
    sub text (@) {
        my $parser = $self->parser();
        my $toke = $parser->curr_token();
        if (defined($toke)) {
            return $toke->text(@_);
        }
    }
    sub span (;$) {
        my $parser = $self->parser();
        my $toke = $parser->curr_token();
        if (! defined($toke)) {
            return undef;
        } elsif (! @_) {
            return $toke->span();
        } else {
            return $toke->span($_[0]);
        }
    }
    sub open ($$) {
        my $parser = $self->parser();
        return $parser->open_delim(@_);
    }
    sub shut (;$) {
        my $parser = $self->parser();
        # shut() returns list of [1] of all active delimiter stack elements.
        if (! @_) {
            return map { $_->[1] } ($parser->open_delim());
        } else {
            return $parser->shut_delim($_[0]);
        }
    }
    sub rule ($) {
        my $name = shift;
        if (! ref($name)) {
            # A type to instantiate.
        }
    }
    my $grammar = qr{
        (?(DEFINE)
            (?<macro_argument>
                (?{ toke($_, pos, 'Argument') })
                (?: (??{ rule('argument') })
                  | (?> (?&macro_word) )                        (?{ text(drop) })
                    (?: (?> (?&macro_spaces) )
                        (?> (?&macro_word)                      (?{ text(drop) })
                        )?+
                    )*+
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_argsep>
                (?{ toke($_, pos, 'ArgSep') })
                (?: (??{ rule('argsep') })                      (?{ text(drop) })
                  | ($argsep)                                   (?{ text($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_arglist>
                (?{ toke($_, pos, 'ArgList') })
                (?: (??{ rule('arglist') })                     (?{ text(drop) })
                  | (?: (?> (?&macro_argument) )?+
                        (?: (?> (?&macro_argsep) )              (?{ text(drop) })
                            (?> (?&macro_spaces) )?+
                            (?> (?&macro_argument) )?+          (?{ text(drop) })
                        )*+
                    )
                  | (?{ drop })
                    (*FAIL)
                )
            )
            (?<macro_ingroup>
                (?: (?> (?&macro_spaces) )?+
                    (?: ([^\\\s\(\)\{\}\[\]\<\>\"\'\`\$]++)     (?{ text($^N) })
                      | (?> (?&macro_contin) )++
                      | (\\ [\\\h\(\)\{\}\[\]\<\>\"\'\`\$\#])   (?{ text($^N) })
                      | (?> (?&macro_groups) )                  (?{ text(drop) })
                      | (?> (?&macro_quoted) )                  (?{ text(drop) })
                      | (?> (?&macro) )                         (?{ text(drop) })
                      | ( (?> (?&macro_strays) ) )              (?{ text($^N) })
                    )
                )++
            )
            (?<macro_paren_group>
                (?=\()
                (?{ toke($_, pos, 'ParenGroup') })
                (?: (??{ rule('paren_group') })
                  | (\()                                        (?{ open($^N, q{\)}) })
                    (?> (?&macro_ingroup) )
                    (\))                                        (?{ shut($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_brace_group>
                (?=\{)
                (?{ toke($_, pos, 'BraceGroup') })
                (?: (??{ rule('brace_group') })
                  | (\{)                                        (?{ open($^N, q{\}}) })
                    (?> (?&macro_ingroup) )
                    (\})                                        (?{ shut($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_square_group>
                (?=\[)
                (?{ toke($_, pos, 'SquareGroup') })
                (?: (??{ rule('square_group') })
                  | (\[)                                        (?{ open($^N, q{\]}) })
                    (?> (?&macro_ingroup) )
                    (\])                                        (?{ shut($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_angle_group>
                (?=\<)
                (?{ toke($_, pos, 'AngleGroup') })
                (?: (??{ rule('angle_group') })
                  | (\<)                                        (?{ open($^N, q{\>}) })
                    (?> (?&macro_ingroup) )
                    (\>)                                        (?{ shut($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_groups>
                (?=[\(\{\[\<])
                (?: (?> (?&macro_paren_group) )
                  | (?> (?&macro_brace_group) )
                  | (?> (?&macro_square_group) )
                  | (?> (?&macro_angle_group) )
                )
            )
            (?<macro_single_quoted>
                (?=\')
                (?{ toke($_, pos, 'SingleQuoted') })
                (?: (??{ rule('single_quoted') })
                  | (\')                                        (?{ open($^N, $^N) })
                    (?: ([^\\\'\r\n]++)                         (?{ text($^N) })
                      | (\\ \')                                 (?{ text($^N) })
                      | (\\)                                    (?{ text($^N) })
                      | ( (?> (?&macro_newline) ) )             (?{ text($^N) })
                    )*+
                    (\')                                        (?{ shut($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_double_quoted>
                (?=\")
                (?{ toke($_, pos, 'DoubleQuoted') })
                (?: (??{ rule('double_quoted') })
                  | (\")                                        (?{ open($^N, $^N) })
                    (?: ([^\\\"\'\`\(\)\{\}\[\]\<\>\r\n\$]++)   (?{ text($^N) })
                      | (?> (?&macro_contin) )++
                      | (\\ [\"\'\`\$])                         (?{ text($^N) })
                      | (\\)                                    (?{ text($^N) })
                      | ( (?> (?&macro_newline) )++ )           (?{ text($^N) })
                      | (?> (?&macro_single_quoted) )           (?{ text(drop) })
                      | (?> (?&macro_system_quoted) )           (?{ text(drop) })
                      | ( (?> (?&macro_group) ) )               (?{ text($^N) })
                      | (?> (?&macro) )                         (?{ text(drop) })
                      | ([\\\'\`\(\)\{\}\[\]\<\>\$])            (?{ text($^N) })
                    )*+
                    (\")                                        (?{ shut($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_system_quoted>
                (?=\`)
                (?{ toke($_, pos, 'SystemQuoted') })
                (?: (??{ rule('system_quoted') })
                  | (\`)                                        (?{ open($N, $^N) })
                    (?: ([^\\\`\"\'\(\)\{\}\[\]\<\>\$]++)       (?{ text($^N) })
                      | (\\ [\\\`\"\'\(\)\{\}\[\]\<\>\$])       (?{ text($^N) })
                      | (\\)                                    (?{ text($^N) })
                      | (?> (?&macro_double_quoted) )           (?{ text(drop) })
                      | (?> (?&macro_single_quoted) )           (?{ text(drop) })
                      | (?> (?&macro_groups) )                  (?{ text($^N) })
                      | (?> (?&macro) )                         (?{ text(drop) })
                      | ([\\\"\'\(\)\{\}\[\]\<\>\$])            (?{ text($^N) })
                    )*+
                    (\`)                                        (?{ shut($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_quoted>
                (?=[\"\'\`])
                (?: (?> (?&macro_single_quoted) )
                  | (?> (?&macro_double_quoted) )
                  | (?> (?&macro_system_quoted) )
                )
            )
            (?<macro_not_delim>
                (?: (??{ rule('not_delim') })
                  | (??{
                        my $rslt = '';
                        foreach my $dlim (reverse(shut)) {
                            if (substr($_, pos, length($dlim)) eq $dlim) {
                                $rslt = '(*FAIL)';
                                last;
                            }
                        }
                        $rslt;
                    })
                )
            )
            (?<macro_literal>
                (?: ((??{ $breaking_chars{literal}{accept} }))      (?{ text($^N) })
                  | (?> (?&macro_contin) )
                  | (\\ (??{ $breaking_chars{literal}{reject} }))   (?{ text($^N) })
                  | (?!\s) (?> (?&macro_not_delim) ) (.)            (?{ text($^N) })
                )++
            )
            (?<macro_strays>
                [\\\(\)\{\}\[\]\<\>\"\'\`\$]
            )
            (?<macro_word>
                (?: (?=[^\\\s\,\#]) | (?=\\[^\r\n]) | (?=\\$) )
                (??{ toke($_, pos, 'Word') })
                (?: (??{ rule('word') })
                  | (?: (?> (?&macro_literal) )                 # macro_literal handles continuations amid \\
                      | (?> (?&macro_groups) )                  (?{ text(drop) })
                      | (?> (?&macro_quoted) )                  (?{ text(drop) })
                      | (?> (?&macro) )                         (?{ text(drop) })
                      | ((?> (?&macro_strays) ))                (?{ text($^N) })
                    )++
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_funcname>
                (?: (?=[^\s\,\]\#\\]) | (?=\\[^\r\n]) )
                (?{ toke($_, pos, 'FuncName') })
                (?: (??{ rule('funcname') })
                  | (?> (?&macro_word) )
                    (?{ text(drop) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_contin>
                \\ (?> (?&macro_newline) )
            )
            (?<macro_comment>
                (?: (??{ rule('comment') })
                  | \# [^\r\n]* (?: (?> (?&macro_newline) ) | (?!.) )
                )
            )
            (?<macro_newline>
                (?: \r\n | [\r\n] )                             (?{ span(1) })
            )
            (?<macro_spaces>
                (?: \h++
                  | (?> (?&macro_newline) )
                  | (?> (?&macro_contin) )
                  | (?> (?&macro_comment) )
                )++
            )
            (?<macro_opener>
                (?{ toke($_, pos, 'Opener') })
                (?: (??{ rule('opener') })
                  | (${opener})                                 (?{ text($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro_closer>
                (?{ toke($_, pos, 'Closer') })
                (?: (??{ rule('closer') })
                  | (${closer})                                 (?{ text($^N) })
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
            (?<macro>
                (?=\$\[)
                (??{ toke($_, pos, 'Macro') })
                (?: (?> (?&macro_opener) )
                    (?> (?&macro_spaces) )?+
                    (?> (?&macro_funcname) )
                    (?> (?&macro_spaces) )
                    (?> (?&macro_arglist) )?+
                    (?> (?&macro_closer) )
                  | (?{ drop })
                    (*FAIL)
                )
                (?{ done })
            )
        )
    }xo;

    my %self = (
        grammar => $grammar,
        parser  => $parser,
    );

    return bless(\%self, $type);
}

sub parser {
    return (shift)->{parser};
}

1;
