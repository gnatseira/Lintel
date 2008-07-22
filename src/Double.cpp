/* -*-C++-*-
   (c) Copyright 2001-2005, Hewlett-Packard Development Company, LP

   See the file named COPYING for license details
*/

/** @file
    Double static variables
*/

#if __GNUC__ == 2
#define _ISOC9X_SOURCE 1
#endif

// TODO: see if we need to do something to support cygwin
#if defined(__i386__) && !defined(__CYGWIN__)
#include <fpu_control.h>
#endif

#include <math.h>
#include <Lintel/Double.hpp>
#include <Lintel/AssertBoost.hpp>

double Double::default_epsilon = 1e-12;

/*
 * this is how these used to be defined, before C9X was usable. Kept in case
 * we port to a system on which it isn't available.
 */

// The warning about dividing by zero is unavoidable, sorry
#ifndef NAN
#define NAN 0.0/0.0
#endif
#ifndef INFINITY
#define INFINITY 1.0/0.0
#endif


// This is here because icc doesn't correctly handle the direct
// assignment correctly; I don't know why and this problem is separate from
// the problem in the tested revision not optimizing correctly with -xB 
// (see com_hp_hpl_lintel.m4)
static double work_around_icc_bug() {
    return NAN;
}
const double Double::NaN = work_around_icc_bug();
const double Double::Inf = INFINITY;

void
Double::setFP64BitMode()
{
#if defined(__i386__) && !defined(__CYGWIN__)
    fpu_control_t cw;
    _FPU_GETCW(cw);
    INVARIANT(cw == _FPU_IEEE,
	      boost::format("Whoa, exiting mode for fpu not IEEE?! %d != %d\n")
	      % cw % _FPU_IEEE);
    cw &= ~_FPU_EXTENDED;
    cw |= _FPU_DOUBLE;
    _FPU_SETCW(cw);
#endif
}

void
Double::resetFPMode()
{
#if defined(__i386__) && !defined(__CYGWIN__)
    fpu_control_t cw = _FPU_IEEE;
    _FPU_SETCW(cw);
#endif
}

void
Double::selfCheck()
{
    INVARIANT(isnan(NaN),"nan isn't");
    INVARIANT(isinf(Inf),"inf isn't");
    INVARIANT(isinf(-Inf),"-inf isn't");
}