## SGNL OPS 2017

The following project contains the SGNL Orbit Prediction Software (2017b)

## Status

The pre-2010 UCL Orbit Dynamics Library was rewritten in 2014 so that it 
can be compiled by freely available compilers e.g gcc, clang. In 2014, this
code library was renamed as the UCL SGNL Orbit Prediction Software (OPS).

Currently, OPS is being integrated with UCL's Solar and Radiation Pressure
modelling tool, SRP_TRR_5_0_7, which is a software that computes accelerations
on objects due the effects of solar and thermal re-radiation forcing on the
object surface. The integration of OPS with SRP_TRR_5_07 will be demonstrated
and tested using GPS-IIR and Jason-2 as example cases.

## Philosophy

All spaceraft/mission/analysis specific data is in the relevant folder in
/analyses .  @@@@ No spacecraft specific code in library @@@

## Build OPS

make
or 
make rebuild

## Licence

PolyForm Noncommercial 1.0.0 - research, teaching and personal use; commercial
use needs a separate licence. See `LICENSE`.

The vendored libraries under `external/` and the data under `res/` keep their
own terms, and the underlying work is UCL's. `NOTICE` says what is carved out
and records an open question about who is entitled to license the first-party
code at all; `PROVENANCE.md` traces every file to where it came from.
