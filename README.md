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

**None is granted.** `LICENSE` is a bare copyright notice. The first-party code
originates with UCL and no rights statement ever existed in the original tree,
so nobody has established who may license it - and rather than offer a grant
that may not be the offeror's to make, none is offered. A PolyForm
Noncommercial licence was offered for a day in September 2026 and withdrawn.

The repository stays public and GitHub's own terms still permit viewing and
forking within GitHub; what is withheld is use, modification and distribution
beyond that. The practical cost is that nobody can rely on a licence here to
run this and reproduce the results in `REVIVAL.md`.

The vendored libraries under `external/` and the data under `res/` are not
affected and keep their own terms. `NOTICE` lists them; `PROVENANCE.md` traces
every file to where it came from.
