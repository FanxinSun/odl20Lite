#pragma once
// tests/devtools/penumbral_cancellation_recorded.hpp -- the FIRST RECORD of tools/penumbral_cancellation.cpp's output at its defaults (plan L0 step 8, group C10; the budgetcheck precedent of PROVENANCE section 41.11):
// 91 lines, 6634 bytes, sha256 682d83822d3d17b38b0a17ad60893678e598eaf66e1fdec5519720dce0e80188.
//
// What it is.  The Python tool's output was never kept (PROVENANCE section 25.10/25.11 quote figures from it, no run).  This is what the C++ tool printed when it was first run on the final text of group C10,
// before the maintainer's ruling of 2026-10-10; the six variants of the sensitivity measurement (fused multiply-add, correctly rounded trigonometry, three ulp perturbations: C10_sensitivity.txt) printed the SAME
// bytes.  It is a REGRESSION RECORD from now on and not a proof against the Python; the figures the records quote that it does not reproduce are listed in PROVENANCE section 25.10/25.11 (corrected) and in
// section 41.21.  tests/devtools/penumbral_cancellation_tests.cpp compares the tool's output with it byte for byte ([real_tree]).
//
// Regeneration recipe (only if the tool's output is meant to change, with a line in PROVENANCE.md saying why): build the tool (`cmake --build <build> --target penumbral_cancellation`), run it from a directory outside
// the tree (about 20 s in RelWithDebInfo, standard error empty, exit 0), replace the literal below with the standard output - byte for byte, the final newline included, nothing before the first `=` - and the three numbers
// (lines, bytes, sha256 of that output: `sha256sum`).  The test also hashes the literal, so a careless edit of it is caught before the tool is blamed.

#include <cstddef>

namespace odl::devtools_testing {

inline constexpr std::size_t kRecordedPenumbralLines = 91;
inline constexpr std::size_t kRecordedPenumbralBytes = 6634;
inline constexpr char kRecordedPenumbralSha256[] = "682d83822d3d17b38b0a17ad60893678e598eaf66e1fdec5519720dce0e80188";

inline constexpr char kRecordedPenumbralDefaultRun[] = R"R1(=== SHDW-R-031: the ONE stated passage ===
LEO, r=7331km, circular, beta=0 (shadow axis in the orbital plane)

[baseline]
    duration 12.02 s   net -1.5572e-04 s   abs 1.1423e-01 s   peak running 5.7193e-02 s   swing 367.3x   cancels 99.86%

=== SHDW-R-033, family 1: orbital-plane tilt toward the eclipse cutoff ===
[beta/a_e = 0.0000]
    duration 12.02 s   net -1.5079e-04 s   abs 1.1423e-01 s   peak running 5.7191e-02 s   swing 379.3x   cancels 99.87%
[beta/a_e = 0.3000]
    duration 12.87 s   net -1.8718e-04 s   abs 1.2233e-01 s   peak running 6.1260e-02 s   swing 327.3x   cancels 99.85%
[beta/a_e = 0.6000]
    duration 16.39 s   net -3.8659e-04 s   abs 1.5581e-01 s   peak running 7.8099e-02 s   swing 202.0x   cancels 99.75%
[beta/a_e = 0.8000]
    duration 23.48 s   net -1.1346e-03 s   abs 2.2319e-01 s   peak running 1.1216e-01 s   swing 98.9x   cancels 99.49%
[beta/a_e = 0.9000]
    duration 33.83 s   net -3.3971e-03 s   abs 3.2152e-01 s   peak running 1.6246e-01 s   swing 47.8x   cancels 98.94%
[beta/a_e = 0.9500]
    duration 48.46 s   net -9.9779e-03 s   abs 4.6057e-01 s   peak running 2.3528e-01 s   swing 23.6x   cancels 97.83%
[beta/a_e = 0.9700]
    duration 63.01 s   net -2.1886e-02 s   abs 5.9869e-01 s   peak running 3.1029e-01 s   swing 14.2x   cancels 96.34%
[beta/a_e = 0.9800]
    duration 77.63 s   net -4.0781e-02 s   abs 7.3742e-01 s   peak running 3.8910e-01 s   swing 9.5x   cancels 94.47%
[beta/a_e = 0.9900]
    duration 112.20 s   net -1.2057e-01 s   abs 1.0634e+00 s   peak running 5.9198e-01 s   swing 4.9x   cancels 88.66%
[beta/a_e = 0.9950]
    duration 179.21 s   net -4.1782e-01 s   abs 1.6498e+00 s   peak running 1.0338e+00 s   swing 2.5x   cancels 74.67%
[beta/a_e = 0.9980]
    duration 200.17 s   net -8.6560e-01 s   abs 2.4964e+00 s   peak running 1.6810e+00 s   swing 1.9x   cancels 65.33%
[beta/a_e = 0.9995]
    duration 175.59 s   net +7.1533e-01 s   abs 1.3827e+00 s   peak running 7.1533e-01 s   swing 1.0x   cancels 48.27%

=== SHDW-R-033, family 2: eccentricity, eclipse AT an apse (should be UNCHANGED, exactly) ===
[e = 0.00, at periapsis]
    duration 12.02 s   net -1.5572e-04 s   abs 1.1423e-01 s   peak running 5.7193e-02 s   swing 367.3x   cancels 99.86%
[e = 0.30, at periapsis]
    duration 10.10 s   net -1.1963e-04 s   abs 9.5958e-02 s   peak running 4.8039e-02 s   swing 401.6x   cancels 99.88%
[e = 0.50, at periapsis]
    duration 9.40 s   net -1.0299e-04 s   abs 8.9317e-02 s   peak running 4.4710e-02 s   swing 434.1x   cancels 99.88%
[e = 0.70, at periapsis]
    duration 8.85 s   net -9.1053e-05 s   abs 8.4105e-02 s   peak running 4.2098e-02 s   swing 462.3x   cancels 99.89%
[e = 0.85, at periapsis]
    duration 8.50 s   net -8.3997e-05 s   abs 8.0800e-02 s   peak running 4.0442e-02 s   swing 481.5x   cancels 99.90%

=== SHDW-R-033, family 3: eccentricity, eclipse OFF an apse (genuine radial velocity) ===
[e = 0.50, 45 deg from periapsis]
    duration 7.72 s   net -1.2410e-04 s   abs 7.3408e-02 s   peak running 3.6763e-02 s   swing 296.2x   cancels 99.83%
[e = 0.50, 90 deg from periapsis]
    duration 14.26 s   net -5.7624e-04 s   abs 1.3551e-01 s   peak running 6.8039e-02 s   swing 118.1x   cancels 99.57%
[e = 0.85, 45 deg from periapsis]
    duration 6.66 s   net -1.1072e-04 s   abs 6.3262e-02 s   peak running 3.1684e-02 s   swing 286.2x   cancels 99.82%
[e = 0.85, 90 deg from periapsis]
    duration 16.29 s   net -9.7744e-04 s   abs 1.5482e-01 s   peak running 7.7898e-02 s   swing 79.7x   cancels 99.37%

=== resolution check on the two most extreme rows above (SHDW's own rule 5 diagnostic) ===
(kept modest deliberately: the point is to show the figure does not move across a
 refinement, which two steps already demonstrate; the original investigation went
 up to N=4801/nphi=1800 and is recorded in PROVENANCE.md SS25.11 for the full range)
  beta/a_e = 0.995:
[    N_samples=401 nphi=200]
    duration 179.21 s   net -4.1781e-01 s   abs 1.6499e+00 s   peak running 1.0338e+00 s   swing 2.5x   cancels 74.68%
[    N_samples=801 nphi=400]
    duration 179.21 s   net -4.1782e-01 s   abs 1.6498e+00 s   peak running 1.0338e+00 s   swing 2.5x   cancels 74.67%
  beta/a_e = 0.9995:
[    N_samples=401 nphi=200]
    duration 175.59 s   net +7.1432e-01 s   abs 1.3836e+00 s   peak running 7.1432e-01 s   swing 1.0x   cancels 48.37%
[    N_samples=801 nphi=400]
    duration 175.59 s   net +7.1483e-01 s   abs 1.3832e+00 s   peak running 7.1483e-01 s   swing 1.0x   cancels 48.32%

=== resolution check on the BASELINE row too (manager's finding, 2026-09-22) ===
this is the ROW THE EXTREME-ROW CHECK ABOVE DID NOT COVER, and it is the most
resolution-sensitive of any row in this file: cancel is 99.9%, so net is a ~0.1%
residual of two integrals each ~abs in size, and a tiny relative shift in either
integral is a large relative shift in their difference. swing = abs/net inherits
that sensitivity directly. abs and cancel_pct do NOT share it -- watch them hold
still while swing moves.
[  N_samples=201 nphi=100]
    duration 12.02 s   net -1.5565e-04 s   abs 1.1427e-01 s   peak running 5.7215e-02 s   swing 367.6x   cancels 99.86%
[  N_samples=401 nphi=200]
    duration 12.02 s   net -1.5427e-04 s   abs 1.1424e-01 s   peak running 5.7197e-02 s   swing 370.8x   cancels 99.86%
[  N_samples=801 nphi=400]
    duration 12.02 s   net -1.5389e-04 s   abs 1.1423e-01 s   peak running 5.7193e-02 s   swing 371.6x   cancels 99.87%
[  N_samples=1601 nphi=800]
    duration 12.02 s   net -1.5222e-04 s   abs 1.1423e-01 s   peak running 5.7192e-02 s   swing 375.7x   cancels 99.87%
[  N_samples=3201 nphi=1600]
    duration 12.02 s   net -1.5227e-04 s   abs 1.1423e-01 s   peak running 5.7192e-02 s   swing 375.6x   cancels 99.87%
  across these 5 resolutions: swing spans 367.6x to 375.7x (2.2% spread); net spans 2.2% spread; abs spans only 0.04% spread -- confirming net (and hence swing) is the sensitive one, not abs.
  the FINEST two resolutions agree to 0.03%, well inside the spread of the coarser pairs -- that is convergence, not noise. Converged estimate: ~376x. This is closer to the ORIGINAL figure this tool was built to check (376x) than to this tool's own default-resolution output (367x) -- the default was under-resolved, not the earlier figure wrong.

=== SUMMARY (the figures SPEC-shadow.md and PROVENANCE.md quote) ===
  baseline (beta=0):        cancels 99.86%, net -1.5572e-04 s, abs 1.1423e-01 s (stable to 4 figures), swing ~376x (from the finest-resolution pair above, NOT from this call's own default-resolution run of 367x -- see the baseline resolution check for why the two differ and which one to trust)
)R1";

}  // namespace odl::devtools_testing
