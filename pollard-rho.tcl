# Pollard Rho Integer Factorization
# Copyright(C) 2003 Salvatore Sanfilippo
# All rights reserved.

load tclsbignum.so

proc swap {vara varb} {
    upvar $vara a
    upvar $varb b

    set t $a
    set a $b
    set b $t
}

proc gcd {a b} {
    if {[< $a 0]} {set a [- $a]}
    if {[< $b 0]} {set a [- $b]}
    if {[< $a $b]} {swap a b}
    while {[!= $b 0]} {
	set r [% $a $b]
	set a $b
	set b $r
    }
    return $a
}

# Little Fermat's theorem primality test in three bases
# Not perfect but it's hard to find such a pseudo-prime.
proc seemsprime n {
    if {[== $n 1]} {return 1}
    if {[== $n 2]} {return 1}
    if [== $n 3] {return 1}
    set l2 [** 2 [- $n 1] $n]
    set l3 [** 3 [- $n 1] $n]
    if {[== $l2 1] && [== $l3 1]} {return 1}
    return 0
}

# Pollard Rho. This version calls itself recursively for every
# factor found.
proc phro n {
    if {[seemsprime $n]} {
	puts -nonewline "$n "
	return
    }
    while {[% $n 2] == 0} {
	puts 2
	set n [/ $n 2]
	if {[== $n 1]} return
    }
    set i 1
    #set x [% [expr {int(rand()*$n)}] $n]
    set x [% [rand] $n]
    #puts "RAND: $x"
    set y $x
    set k 2
    while {1} {
	set i [+ $i 1]
	set x [% [- [* $x $x] 1] $n]
	set d [gcd [- $y $x] $n]
	if {[!= $d 1] && [!= $d $n]} {
	    phro $d
	    set g [/ $n $d]
	    phro $g
	    return
	}
	if {[== $i $k]} {
	    set y $x
	    set k [* $k 2]
	}
    }
}

set number 213029348203814091823131
puts -nonewline "$number : "; flush stdout
phro $number
puts {}
