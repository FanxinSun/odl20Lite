// GENERATED from the characterisation probe's output — do not edit.
//
// What `ConventionalField::potential` and `::acceleration` returned BEFORE SPEC-gravity v1.4 added the second
// derivatives and the synthesis of a coefficient view (GRAV-R-066, GRAV-A-036): recorded as hexadecimal floats from the
// tree at commit dea899b, with the EGM2008 tide-free file, the conventional field at 2023-01-22 00:00 UTC (the secular
// terms extrapolated as the ranking test does), by a scratch probe whose full output (323 lines, 10 degree/order
// pairs at 8 positions, two by-degree sweeps and one truncation RMS) has sha256
// 2ed5042bd486fc71b95b1db0b372597fbfb216cfd20334b9031a5f408d71eeb8 and is kept with the L7 report.  These twelve are the
// rows the test compares, at three positions: on the equator, mid-latitude, and within 7.2 km of the polar axis.
#pragma once

namespace odl::gravity::baseline {

struct SynthesisRow {
    double x, y, z;      ///< metres, ITRS
    int n, m;
    double potential;    ///< m^2 s^-2
    double a[3];         ///< m s^-2
};

inline constexpr SynthesisRow kRows[] = {
    {0x1.9799e13333333p+22, 0x0p+0, 0x0p+0, 2, 0, 0x1.c79a78c23c557p+25, {-0x1.1e6e576baca03p+3, 0x0p+0, 0x0p+0}},
    {0x1.9799e13333333p+22, 0x0p+0, 0x0p+0, 36, 36, 0x1.c79add4a39c41p+25, {-0x1.1e6f06fcfaf07p+3, -0x1.c2f5ff103672bp-16, 0x1.bc6038df673d8p-16}},
    {0x1.9799e13333333p+22, 0x0p+0, 0x0p+0, 90, 90, 0x1.c79adda068e4ap+25, {-0x1.1e6f0fc5d78d4p+3, -0x1.907a548396981p-16, 0x1.75af3ae44493ap-16}},
    {0x1.9799e13333333p+22, 0x0p+0, 0x0p+0, 360, 360, 0x1.c79add9fe6432p+25, {-0x1.1e6f0face1148p+3, -0x1.94c39e38ed6dep-16, 0x1.7b6d1229f6998p-16}},
    {0x1.0059p+22, 0x1.7a6bp+21, 0x1.dc13p+21, 2, 0, 0x1.d2ae96aafdf25p+25, {-0x1.82c74292fe38dp+2, -0x1.1d7ab1235a1ddp+2, -0x1.684533794fe09p+2}},
    {0x1.0059p+22, 0x1.7a6bp+21, 0x1.dc13p+21, 36, 36, 0x1.d2aef2520c4a7p+25, {-0x1.82caefc7b2cf7p+2, -0x1.1d7e5083e267ep+2, -0x1.684689fdd202ap+2}},
    {0x1.0059p+22, 0x1.7a6bp+21, 0x1.dc13p+21, 90, 90, 0x1.d2aef09587fcp+25, {-0x1.82ca3fad35371p+2, -0x1.1d7eedea90ea9p+2, -0x1.6845b9eb2df35p+2}},
    {0x1.0059p+22, 0x1.7a6bp+21, 0x1.dc13p+21, 360, 360, 0x1.d2aef0a2c39fep+25, {-0x1.82ca89705000ap+2, -0x1.1d7ec96b74a59p+2, -0x1.684596aed256bp+2}},
    {0x1.f4p+9, -0x1.f4p+10, 0x1.b774p+22, 2, 0, 0x1.a6036bfcf8471p+25, {-0x1.1685d2ecd4bf3p-10, 0x1.1685d2ecd4bf3p-9, -0x1.ead8524444197p+2}},
    {0x1.f4p+9, -0x1.f4p+10, 0x1.b774p+22, 36, 36, 0x1.a603be883af02p+25, {-0x1.046399cbcea74p-10, 0x1.14a5dd786185cp-9, -0x1.eada232ff3487p+2}},
    {0x1.f4p+9, -0x1.f4p+10, 0x1.b774p+22, 90, 90, 0x1.a603be7f03e55p+25, {-0x1.0469b438008a8p-10, 0x1.14a94056a8f9cp-9, -0x1.eada2182cb17ep+2}},
    {0x1.f4p+9, -0x1.f4p+10, 0x1.b774p+22, 360, 360, 0x1.a603be7f03d59p+25, {-0x1.0469b492f13dap-10, 0x1.14a94023ddef4p-9, -0x1.eada2182c7ad2p+2}},
};

}  // namespace odl::gravity::baseline
