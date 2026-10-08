#pragma once
// tests/devtools/measmod_fd_sizing_recorded.hpp -- the output the PYTHON tools/measmod_fd_sizing.py printed for `--scan --check` on this tree's data, as L6's Round 9 report recorded it before the diurnal-aberration
// section was added to the tool (plan L0 step 8, group C8): 106 lines, 9,872 bytes, sha256 c6389b08fcf6bfa3b39f50e5aaa2902cdea56f2391b8e1663a8e486f4eccdab5, from
//   ~/.claude/handover/2026-09-25-odl-rewrite-L6.REPORT.files/round9/fd_sizing_angle_scan_check.out
// (PROVENANCE section 39 ties it to the tree).  It is the ORACLE of tests/devtools/measmod_fd_sizing_tests.cpp: what the C++ port must print, byte for byte, for the sections that existed, and the lines from which the
// port's other modes are derived (the Python's own flags decide which lines each mode prints).  Not edited: the bytes are the recorded ones (ASCII, no tab, no trailing blank).

namespace odl::devtools_testing {

inline constexpr char kRecordedScanCheck[] = R"R1(--- range, LAGEOS-like: rho = 6.6e+06 m, F = 2.651e-14, nu = 5.59e-09
    h =     10   T = 4.42e-13   N = 5.59e-10   eps = 5.59e-10
    h =     30   T = 3.98e-12   N = 1.86e-10   eps = 1.90e-10
    h =    100   T = 4.42e-11   N = 5.59e-11   eps = 1.00e-10
    h =    300   T = 3.98e-10   N = 1.86e-11   eps = 4.16e-10
    h =   1000   T = 4.42e-09   N = 5.59e-12   eps = 4.42e-09
    h* = 86, best = 9.8e-11, B_pred = 4.42e-09, window [4.4e-10, 4.4e-08]
    frozen B_pred 4.42e-09, h* 86: reproduced
--- range, LEO-like: rho = 1.5e+06 m, F = 5.132e-13, nu = 2.79e-09
    h =     10   T = 8.55e-12   N = 2.79e-10   eps = 2.88e-10
    h =     30   T = 7.70e-11   N = 9.31e-11   eps = 1.70e-10
    h =    100   T = 8.55e-10   N = 2.79e-11   eps = 8.83e-10
    h =    300   T = 7.70e-09   N = 9.31e-12   eps = 7.71e-09
    h =   1000   T = 8.55e-08   N = 2.79e-12   eps = 8.55e-08
    h* = 25, best = 1.7e-10, B_pred = 8.55e-08, window [8.6e-09, 8.6e-07]
    frozen B_pred 8.55e-08, h* 25: reproduced
--- angle, LEO-like, |phi| <= 40 deg: rho = 1.2e+06 m, F = 2.575e-18, nu = 4.99e-15
    h =     10   T = 4.29e-17   N = 4.99e-16   eps = 5.42e-16
    h =     30   T = 3.86e-16   N = 1.66e-16   eps = 5.53e-16
    h =    100   T = 4.29e-15   N = 4.99e-17   eps = 4.34e-15
    h =    300   T = 3.86e-14   N = 1.66e-17   eps = 3.86e-14
    h =   1000   T = 4.29e-13   N = 4.99e-18   eps = 4.29e-13
    h* = 18, best = 4.2e-16, B_pred = 4.29e-13, window [4.3e-14, 4.3e-12]
    frozen B_pred 4.29e-13, h* 18: reproduced
--- if only machine round-off mattered: eps^(2/3) = 3.67e-11
--- series scan, |dec| <= 30 deg: max |f'''| RA 3.0750, Dec 2.2295 (rho = 1); analytic RA bound 2/cos^3 = 3.0792
    the frozen angle coefficient 2/(rho^3 cos^3 phi_max) bounds both: yes

=== THE AMENDED RANGE SIZING (2026-10-06, made after the first run's miss) ===
the chain's floor: 2^-45 rad = 2.8422e-14 rad, axis distance 5580552.4 m, one ulp = 1.5861e-07 m, three ulp = 4.7583e-07 m;  ZTD = 2.364942 m, lat -29.0465 deg, H 244.5 m, t = 37.15 C
the Decimal FCULa at the IERS FCUL_A printed case differs from the printed 3.800243667312344087 by -2.37e-16: ok
one ulp of the chain's angle at the station = 1.586e-07 m against the 1.59e-7 m predicted before it was measured: ok
--- LAGEOS-like: rho = 6.6e+06 m, elevation 52 deg, azimuth 105 deg, speed 5700 m/s, nu(frozen) = 5.588e-09 m
    Shapiro alone: sup |d3| = 3.503e-23 m^-2 = 1.32e-09 of F_geo;  atmosphere alone: 9.694e-20 = 3.66e-06 of F_geo
    h        F_geo        F_trop(sup)   F(h)=F_geo+1.01 F_trop   eps_rigid(h)      eps_real event 2      eps_real event 1
        10   2.65113e-14  9.6904e-20   2.65114e-14   5.5924e-10   4.3503e-09        8.1413e-09
        30   2.65114e-14  9.6905e-20   2.65115e-14   1.9024e-10   1.4539e-09        2.7176e-09
       100   2.65120e-14  9.6910e-20   2.65121e-14   1.0007e-10   4.7917e-10        8.5827e-10
       300   2.65136e-14  9.6924e-20   2.65137e-14   4.1633e-10   5.4270e-10        6.6907e-10
      1000   2.65192e-14  9.6973e-20   2.65193e-14   4.4255e-09   4.4634e-09        4.5013e-09
    rigid         : nu = 5.5879e-09 m   B_pred = 4.4255e-09   window [4.425e-10, 4.425e-08]
    real, event 2 : nu = 4.3498e-08 m   B_pred = 4.4634e-09   window [4.463e-10, 4.463e-08]
    real, event 1 : nu = 8.1408e-08 m   B_pred = 8.1413e-09   window [8.141e-10, 8.141e-08]
    |g . east| = 0.1593: nu_chain event 2 = 3.7910e-08 m, event 1 = 7.5821e-08 m
    closed-form two-way light time, across event 2: max over the cone of |y'''| / F_geo(h) = 0.999888  (the formula bounds the coupled geometry: yes)
    closed-form two-way light time, across event 1: max over the cone of |y'''| / F_geo(h) = 0.999888  (the formula bounds the coupled geometry: yes)
    closed-form two-way light time, along  event 2: max over the cone of |y'''| / F_geo(h) = 0.999888  (the formula bounds the coupled geometry: yes)
    closed-form two-way light time, along  event 1: max over the cone of |y'''| / F_geo(h) = 0.999888  (the formula bounds the coupled geometry: yes)
    frozen F_trop reproduced: ok
    the frozen amended eps(h), nu_chain and B_pred reproduced: ok
--- LEO-like: rho = 1.5e+06 m, elevation 15 deg, azimuth 0 deg, speed 7500 m/s, nu(frozen) = 2.794e-09 m
    Shapiro alone: sup |d3| = 8.009e-22 m^-2 = 1.56e-09 of F_geo;  atmosphere alone: 7.803e-16 = 1.52e-03 of F_geo
    h        F_geo        F_trop(sup)   F(h)=F_geo+1.01 F_trop   eps_rigid(h)      eps_real event 2      eps_real event 1
        10   5.13279e-13  7.7299e-16   5.14060e-13   2.8796e-10   2.3269e-08        4.6249e-08
        30   5.13293e-13  7.7314e-16   5.14074e-13   1.7024e-10   7.8305e-09        1.5491e-08
       100   5.13341e-13  7.7365e-16   5.14122e-13   8.8481e-10   3.1829e-09        5.4809e-09
       300   5.13478e-13  7.7512e-16   5.14261e-13   7.7232e-09   8.4892e-09        9.2553e-09
      1000   5.13957e-13  7.8028e-16   5.14746e-13   8.5794e-08   8.6024e-08        8.6253e-08
    rigid         : nu = 2.7940e-09 m   B_pred = 8.5794e-08   window [8.579e-09, 8.579e-07]
    real, event 2 : nu = 2.3260e-07 m   B_pred = 8.6024e-08   window [8.602e-09, 8.602e-07]
    real, event 1 : nu = 4.6241e-07 m   B_pred = 8.6253e-08   window [8.625e-09, 8.625e-07]
    |g . east| = 0.9659: nu_chain event 2 = 2.2981e-07 m, event 1 = 4.5961e-07 m
    closed-form two-way light time, across event 2: max over the cone of |y'''| / F_geo(h) = 0.999854  (the formula bounds the coupled geometry: yes)
    closed-form two-way light time, across event 1: max over the cone of |y'''| / F_geo(h) = 0.999854  (the formula bounds the coupled geometry: yes)
    closed-form two-way light time, along  event 2: max over the cone of |y'''| / F_geo(h) = 0.999854  (the formula bounds the coupled geometry: yes)
    closed-form two-way light time, along  event 1: max over the cone of |y'''| / F_geo(h) = 0.999854  (the formula bounds the coupled geometry: yes)
    frozen F_trop reproduced: ok
    the frozen amended eps(h), nu_chain and B_pred reproduced: ok

=== THE ANGLE SIZING (item 6: written and committed before the angle gate's first run) ===
nu_a = 3 [ulp(2 pi) + ulp(7.2e6 m)/rho] = 4.9928e-15 rad;  the chain's floor: ZERO (the observer, the orientation and the Earth's motion are evaluated at the observation epoch only)
the refraction of the first normal point's weather (976.7 hPa, 37.15 C, rh 0.12, 0.532 um): A = 2.484383e-04, B = -3.123796e-07 rad
    h        F_pure(h) (m^-3)    h^2 F_pure/6      nu_a/h       agreement tolerance
        10   2.57480e-18    4.2913e-17   4.9928e-16   1.0007e-15
        30   2.57504e-18    3.8626e-16   1.6643e-16   3.3494e-16
       100   2.57587e-18    4.2931e-15   4.9928e-17   1.0194e-16
       300   2.57824e-18    3.8674e-14   1.6643e-17   3.5370e-17
      1000   2.58656e-18    4.3109e-13   4.9928e-18   1.2073e-17
    geometric   : F_extra/rho^3 = 0.000262  F_a(h) = ['2.57496e-18', '2.57519e-18', '2.57602e-18', '2.57839e-18', '2.58671e-18']
                  eps(h) = ['5.4220e-16', '5.5271e-16', '4.3433e-15', '3.8693e-14', '4.3112e-13']   B_pred = 4.3112e-13  window [4.311e-14, 4.311e-12]
    astrometric : F_extra/rho^3 = 0.00143  F_a(h) = ['2.57563e-18', '2.57587e-18', '2.57670e-18', '2.57907e-18', '2.58739e-18']
                  eps(h) = ['5.4221e-16', '5.5281e-16', '4.3444e-15', '3.8703e-14', '4.3124e-13']   B_pred = 4.3124e-13  window [4.312e-14, 4.312e-12]
    refracted   : F_extra/rho^3 = 0.0974  F_a(h) = ['2.63117e-18', '2.63141e-18', '2.63224e-18', '2.63461e-18', '2.64292e-18']
                  eps(h) = ['5.4314e-16', '5.6114e-16', '4.4370e-15', '3.9536e-14', '4.4049e-13']   B_pred = 4.4049e-13  window [4.405e-14, 4.405e-12]
    ofdate      : F_extra/rho^3 = 0.00143  F_a(h) = ['2.57563e-18', '2.57587e-18', '2.57670e-18', '2.57907e-18', '2.58739e-18']
                  eps(h) = ['5.4221e-16', '5.5281e-16', '4.3444e-15', '3.8703e-14', '4.3124e-13']   B_pred = 4.3124e-13  window [4.312e-14, 4.312e-12]
the 60-digit eraRefco against ERFA's published case: 0.000 of ERFA's tolerances: ok
the frozen A and B of the first normal point's weather reproduced: ok
the frozen nu_a reproduced: ok
the frozen eps(h) and B_pred of the four groups reproduced from F_pure and the frozen F_extra: ok
the frozen agreement tolerances reproduced: ok
the frozen F_extra are 1.05 x the fine scan, rounded up to three digits: ok
the series guard against 60-digit third differences ... worst relative disagreement 3.06e-07: ok
--- geometric (check grid: 15 degrees, 6 lines of sight): sup |full - pure| = (2.4219e-04, 1.0354e-04) /rho^3;  sup |pure| = (4.3655, 2.6384), worst ratio to F_pure 0.97846;  sup |full| = (4.3655, 2.6384)
    below the frozen F_extra 0.000262 and within 15 % of the fine scan's 0.0002495: yes
    the pure geometric formula bounds the pure third derivatives of the scan: yes
    the full third derivative stays below F_a (the largest of the five sizes, 4.4698): yes
--- astrometric (check grid: 15 degrees, 6 lines of sight): sup |full - pure| = (1.3359e-03, 6.8928e-04) /rho^3;  sup |pure| = (4.3655, 2.6384), worst ratio to F_pure 0.97846;  sup |full| = (4.3663, 2.6389)
    below the frozen F_extra 0.00143 and within 15 % of the fine scan's 0.001358: yes
    the pure geometric formula bounds the pure third derivatives of the scan: yes
    the full third derivative stays below F_a (the largest of the five sizes, 4.4710): yes
--- refracted (check grid: 15 degrees, 9 lines of sight): sup |full - pure| = (2.4837e-04, 9.1824e-02) /rho^3;  sup |pure| = (4.3655, 2.6384), worst ratio to F_pure 0.97846;  sup |full| = (4.3655, 2.6384)
    below the frozen F_extra 0.0974 and within 15 % of the fine scan's 0.09272: yes
    the pure geometric formula bounds the pure third derivatives of the scan: yes
    the full third derivative stays below F_a (the largest of the five sizes, 4.5670): yes
ok       every frozen number reproduced
)R1";

}  // namespace odl::devtools_testing
