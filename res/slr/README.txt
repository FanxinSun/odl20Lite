Satellite laser ranging for LightSail-2 (NORAD 44420, COSPAR 1903629, ILRS
target "lightsail2"), and the ILRS orbit prediction for the same days.

  lightsail2_201908.npt            normal points in CRD v1 format, August 2019
  lightsail2_201909.npt            the same for September 2019
  lightsail2.slrobs                the two converted by scripts/crd2obs.py
The ILRS prediction the stations pointed with - lightsail2_cpf_190831_7428.nxt,
generated 30 Aug 2019 - is half a megabyte and is not kept here;
scripts/validate_slr.sh fetches it, and res/configOPS_lightsail2.txt already
carries the initial state derived from it by scripts/cpf2eci.py.

Source: the EUROLAS Data Centre at DGFI-TUM, which mirrors the ILRS archive and
serves it over plain HTTPS with no credentials:

  https://edc.dgfi.tum.de/pub/slr/data/npt_crd/lightsail2/2019/
  https://edc.dgfi.tum.de/pub/slr/cpf_predicts/2019/lightsail2/

NASA's CDDIS holds the same data behind an Earthdata login.
scripts/validate_slr.sh re-fetches both if they are missing.

WHAT IS HERE. 25 normal points, 8 passes, 2 Aug to 20 Sep 2019, every one of
them from Yarragadee (MOBLAS-5, station 7090). That is the entire public laser
ranging record for this object. It is thin, and it is from a single station, so
the geometry repeats: expect a fit to lean on its a priori.

WHY THIS OBJECT. LightSail-2 is a 32 m^2 solar sail on a 5.035 kg CubeSat -
6.36 m^2/kg - and it is the only high area-to-mass object in the whole public
laser ranging archive. Everything else the ILRS tracks is a dense sphere or a
large spacecraft, because geodetic targets are deliberately built to make
non-gravitational forces small, which is the opposite of what is wanted for
testing a radiation pressure model. It had no GPS receiver, so laser ranging
was its primary orbit determination.

WHAT THE HEADERS SAY IS NOT APPLIED, and so has to be modelled: tropospheric
refraction and the offset between the retroreflector and the centre of mass.
The station system delay IS applied. See src/main_fit_orbit_to_slr.cpp.

The station coordinates are in res/slr_stations.txt, from SLRF2020.
