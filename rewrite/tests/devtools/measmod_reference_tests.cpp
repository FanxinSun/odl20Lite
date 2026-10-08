// tests/devtools/measmod_reference_tests.cpp — reference values for SPEC-measmod.md's acceptance tests, from the definition (plan L0 step 8, group C8): the calendar, the constants, the cosine and the power of
// ten in 60 digits, the geodetic iteration, the closed-form light time against its quadratics, every section against values computed apart by GNU bc, the guards shown firing, the header's layout, the
// registry and the position sections on synthetic trees, the command line, and the real tree's header.
// (ctests `measmod_reference.behaviour` and `measmod_reference.real_tree`; the old ctest name measmod.reference_header_matches_its_generator runs the tool itself.)
//
// The Python tool had no test of its own.  Every expectation here is DERIVED BEFORE THE CODE RAN: by hand where the answer is a whole number or a closed form, otherwise by GNU bc 1.07.1 from the DEFINITIONS
// (C8_registered/reference_values.bc, lighttime_values.bc, shapiro_el10_leo30.bc and registry_values.bc, in this group's report files), a host tool that names no part of the build.  None was taken from a run of the port.
// One thing was CORRECTED after the first run, and said here: the tolerances of the geodetic coordinates of the worked tree's reference point and marker (latitude, height) had been written as a relative 1e-13 and
// 1e-12, which ignored two facts of the generator that its own comment states -- the latitude iteration stops when two steps differ by less than 1e-17 rad (an error of that times the contraction e^2, 7e-20 rad)
// and the height is formed from the DOUBLE sine and cosine of the latitude (some 3e-9 m) -- and they are now the absolute 2e-19 rad and 5e-9 m those two facts give (the expectations, the bc values, were not touched).

#include <catch2/catch_test_macros.hpp>

#include "measmod_synthetic_files.hpp"
#include "throwing_stream.hpp"

#include <odl/devkit/decimal.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "measmod_reference.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace mr = odl::tools::measmod_reference;
using namespace odl::devkit;
using namespace odl::devtools_testing;

namespace {

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

// the double nearest to a decimal given in text, the way strtod reads it: correctly rounded
double nearest(const char* text) { return std::strtod(text, nullptr); }

bool close(double got, double want, double relative) { return std::fabs(got - want) <= relative * std::fabs(want); }

// |got - want| <= tolerance, with the numbers shown when it is not
bool within(double got, double want, double tolerance, const char* what) {
    const bool ok = std::fabs(got - want) <= tolerance;
    if (!ok) {
        std::ostringstream message;
        message << std::setprecision(17) << what << ": got " << got << ", want " << want << ", difference " << got - want << ", allowed " << tolerance;
        UNSCOPED_INFO(message.str());
    }
    return ok;
}

double value(const mr::Values& values, const std::string& key) {
    for (const auto& [name, v] : values) {
        if (name == key) return v;
    }
    FAIL("no value named " << key);
    return 0.0;
}

std::vector<std::string> names(const mr::Values& values) {
    std::vector<std::string> out;
    for (const auto& entry : values) out.push_back(entry.first);
    return out;
}

// the context the tool computes in
struct Sixty {
    LocalContext context{mr::kPrecision};
};

Decimal D(const char* text) { return Decimal::from_string(text); }

// ---- the synthetic tree ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct Tree {
    TempDir td{"odl-mr"};
    fs::path root = td.path();
    void put(const char* relative, const std::string& text) const {
        fs::create_directories((root / relative).parent_path());
        write_text(root / relative, text);
    }
    Tree(const std::string& slrf, const std::string& ecc, const std::string& horizons, const std::string& sp3) {
        put(mr::kSlrfRelative, slrf);
        put(mr::kEccRelative, ecc);
        put(mr::kHorizonsRelative, horizons);
        put(mr::kSp3Relative, sp3);
    }
    [[nodiscard]] Result run(const std::vector<std::string>& extra = {}) const {
        std::ostringstream out;
        std::ostringstream err;
        std::vector<std::string> args = {"--root", root.string()};
        args.insert(args.end(), extra.begin(), extra.end());
        const int code = mr::run(args, Streams{out, err});
        return Result{code, out.str(), err.str()};
    }
};

// the tree of the worked example: a station on the equator at longitude 0 with no velocity, an eccentricity of (1, 2, 3) m, the Horizons record (3, 4, 12) km and the SP3 record (3000, 4000, 12000) km
Tree worked_tree() {
    return Tree(slrf_text("0.6378137000000000E+07", "0.0000000000000000E+00", "0.0000000000000000E+00", "0.0000000000000000E+00", "0.0000000000000000E+00", "0.0000000000000000E+00"),
                ecc_text(ecc_row("00:000:00000", "1.0000", "2.0000", "3.0000", "70900513")), horizons_text(" X = 3.0 Y = 4.0 Z = 12.0\n"), sp3_text(sp3_record(3000.0, 4000.0, 12000.0)));
}

const char kUsage[] = "usage: measmod_reference [-h] [--check] [--header HEADER] [--root ROOT]\n";

}  // namespace

TEST_CASE("days_from_civil counts the days of the proleptic Gregorian calendar from 1970-01-01", "[measmod_reference][behaviour]") {
    CHECK(mr::days_from_civil(1970, 1, 1) == 0);
    CHECK(mr::days_from_civil(1970, 1, 2) == 1);
    CHECK(mr::days_from_civil(1969, 12, 31) == -1);
    CHECK(mr::days_from_civil(2000, 3, 1) == 11017);
    CHECK(mr::days_from_civil(2000, 2, 29) == 11016);
    CHECK(mr::days_from_civil(1900, 3, 1) == -25508);   // 1900 is no leap year
    CHECK(mr::days_from_civil(1900, 1, 1) == -25567);
    CHECK(mr::days_from_civil(2015, 1, 1) == 16436);    // 45 years of 365 days and 11 leap days
    CHECK(mr::days_from_civil(2026, 1, 1) == 20454);    // 56 years and 14 leap days
    CHECK(mr::days_from_civil(2024, 2, 29) == 19782);
    CHECK(mr::days_from_civil(2026, 1, 1) - mr::days_from_civil(2015, 1, 1) == 4018);   // what datetime.date(2026, 1, 1) - datetime.date(2015, 1, 1) is, in days
    CHECK(mr::days_from_civil(2100, 3, 1) - mr::days_from_civil(2100, 2, 28) == 1);     // 2100 is no leap year either
    CHECK(mr::days_from_civil(1, 1, 1) == -719162);
    CHECK(mr::days_from_civil(9999, 12, 31) == 2932896);
    CHECK(mr::days_from_civil(2026, 12, 1) - mr::days_from_civil(2026, 11, 1) == 30);
    CHECK(mr::days_from_civil(2026, 3, 1) - mr::days_from_civil(2026, 2, 1) == 28);
    // before the year 1 (the years of the era before the first one count back from the 400-year cycle that begins in year 0): `date -u -d 0000-MM-DD +%s` divided by 86400
    CHECK(mr::days_from_civil(0, 1, 1) == -719528);
    CHECK(mr::days_from_civil(0, 2, 29) == -719469);   // year 0 is a leap year
    CHECK(mr::days_from_civil(0, 3, 1) == -719468);
    CHECK(mr::days_from_civil(-1, 12, 31) - mr::days_from_civil(-1, 1, 1) == 364);
    CHECK(mr::days_from_civil(-399, 3, 1) - mr::days_from_civil(-400, 3, 1) == 365);
    CHECK(mr::days_from_civil(0, 3, 1) - mr::days_from_civil(-400, 3, 1) == 146097);   // the days of a whole 400-year cycle
}

TEST_CASE("PI and LN10: the constants as the Python wrote and computed them, whatever the caller's precision", "[measmod_reference][behaviour]") {
    // the 62 decimals the Python wrote are bc's 4 a(1)
    CHECK(mr::pi().to_string() == "3.14159265358979323846264338327950288419716939937510582097494459");
    CHECK(mr::pi().digits() == 63);
    // Decimal(10).ln() at 60 digits: bc's l(10) at 60 decimals is 2.302585092994045684017991454684364207601101488628772976033327, whose 60th decimal (7, followed by 900967...) is the 61st significant digit:
    // 60 significant digits round the ...03332 up to ...03333
    CHECK(mr::ln10().to_string() == "2.30258509299404568401799145468436420760110148862877297603333");
    // asked under another precision, the value is the same (it was computed at the import of the Python module, at 60 digits)
    LocalContext other(10);
    CHECK(mr::ln10().to_string() == "2.30258509299404568401799145468436420760110148862877297603333");
}

TEST_CASE("d_cos: the Taylor series in 60 digits agrees with bc to a few units of the last place", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const auto check = [](const char* x, const char* expected) {
        const Decimal got = mr::d_cos(D(x));
        const Decimal want = D(expected);
        INFO("cos(" << x << ") = " << got.to_string());
        // the series sums terms that are each rounded to 60 digits: it lands within a few units of the 60th digit of bc's value (which is itself truncated there)
        CHECK((got - want).abs() < D("1e-57"));
    };
    check("0", "1");
    check("0.5", ".877582561890372716116281582603829651991645197109744052997610");
    check("1", ".540302305868139717400936607442976603732310420617922227670097");
    check("2", "-.416146836547142386997568229500762189766000771075544890755149");
    check("3.5", "-.936456687290796337698657626671760463019957765781959251620988");
    check("-1.2", ".362357754476673577638373355623076020339947785576648626487749");
    check("0.3", ".955336489125606019642310227568049898244214082632037674517613");
    CHECK(mr::d_cos(D("0")) == D("1"));
    CHECK(mr::d_cos(D("0.7")) == mr::d_cos(D("-0.7")));   // even
}

TEST_CASE("d_pow10 is exp(x ln 10): bc's 10**x to the relative 1e-57 the 60-digit ln 10 allows", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const auto check = [](const char* x, const char* expected) {
        const Decimal got = mr::d_pow10(D(x));
        INFO("10 ** " << x << " = " << got.to_string());
        CHECK(((got - D(expected)).abs() / D(expected)) < D("1e-57"));
    };
    check("0.5", "3.162277660168379331998893544432718533719555139325216826857501");
    check("-1.25", ".056234132519034908039495103977648123146825104309869166408169");
    check("2.5", "316.227766016837933199889354443271853371955513932521682685749614");
    check("7.123", "13273944.577297395022918314728597964500922630901419459528895672806078");
    check("-0.3333333333333333333333333333333333", ".464158883361277889241007635091944693280645766115806115080401");
    // a zero argument is exactly one: exp(0)
    CHECK(mr::d_pow10(D("0")) == D("1"));
    CHECK(mr::d_pow10(D("0.000")).to_string() == "1");
}

TEST_CASE("geodetic: the iteration returns the latitude, longitude and height of a point of the WGS 84 ellipsoid", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    // (1) the points made from known coordinates by bc: latitude 45 deg, longitude 30 deg, height 1000 m; latitude -29 deg, longitude 115 deg, height 244.5 m
    const mr::Geodetic a = mr::geodetic(D("3912960.837423738339253198607099305738729825247809289339013489668306"), D("2259148.992815058787444282887762403037755770925948720556393606697618"),
                                        D("4488055.515647106364413313071507193991780579614782124643011784221435"));
    CHECK(std::fabs(a.latitude - 0.785398163397448309615660845819875721049292349843776455243736) < 2e-15);
    CHECK(std::fabs(a.longitude - 0.523598775598298873077107230546583814032861566562517636829157) < 2e-15);
    CHECK(std::fabs(a.height - 1000.0) < 1e-8);
    const mr::Geodetic b = mr::geodetic(D("-2359499.750766190533891891050448080288114180700684558558022899118055"), D("5059963.544458674330263017125294849645409697166494006720418659100060"),
                                        D("-3074019.736541987287201360461656487334962897436811928462862795492666"));
    CHECK(std::fabs(b.latitude - (-0.506145483078355577307870322861697686898432847677100382268185)) < 2e-15);
    CHECK(std::fabs(b.longitude - 2.007128639793479013462244383761904620459302671822984274511769) < 2e-15);   // atan2 puts the longitude of a negative x in (pi/2, pi]
    CHECK(std::fabs(b.height - 244.5) < 1e-8);
    // (2) the special points: the equator at longitude 0, the pole, the antipode of the first, a point in the third quadrant of the equator
    const mr::Geodetic equator = mr::geodetic(D("6378137"), D("0"), D("0"));
    CHECK(equator.latitude == 0.0);
    CHECK(equator.longitude == 0.0);
    CHECK(equator.height == 0.0);   // p cos(0) + 0 - a^2/N = a - a
    // the pole: z = b = a (1 - f) = 6356752.314245179497563967 (6378137 - 6378137 / 298.257223563, to the digits written), so the height is the rounding of that to 24 digits, nothing
    const mr::Geodetic pole = mr::geodetic(D("0"), D("0"), D("6356752.314245179497563967"));
    CHECK(pole.latitude == std::acos(-1.0) / 2);   // atan2(z, 0): the double nearest pi/2 is half the double nearest pi
    CHECK(pole.longitude == 0.0);                   // atan2(0, 0)
    CHECK(std::fabs(pole.height) < 1e-6);
    const mr::Geodetic west = mr::geodetic(D("-6378137"), D("0"), D("0"));
    CHECK(west.latitude == 0.0);
    CHECK(west.longitude == std::acos(-1.0));   // atan2(+0, -a) is +pi
    CHECK(west.height == 0.0);
    // the third quadrant of the equator: atan2(-y, -y) is -3 pi / 4, to an ulp of the double
    const mr::Geodetic third = mr::geodetic(D("-4510023.6"), D("-4510023.6"), D("0"));
    CHECK(std::fabs(third.longitude - (-2.356194490192345)) < 5e-16);
    CHECK(third.latitude == 0.0);
    // a point above the equator by 500 m: height 500 and latitude 0
    const mr::Geodetic above = mr::geodetic(D("6378637"), D("0"), D("0"));
    CHECK(above.latitude == 0.0);
    CHECK(std::fabs(above.height - 500.0) < 1e-8);
}

TEST_CASE("lt_closed_form: the roots of the two quadratics, to 60 digits, and exact where the answer is a whole number", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const Decimal c = Decimal(299792458);
    // (1) at rest: the distance over c on each leg.  A target at (3 c, 4 c, 0) metres from a station at the origin is 5 light seconds away: tau_u = tau_d = 5, exactly, whatever the event
    const mr::Vec zero = {Decimal(0), Decimal(0), Decimal(0)};
    const mr::Vec far = {Decimal(3) * c, Decimal(4) * c, Decimal(0)};
    for (const int event : {2, 1}) {
        const auto [up, down] = mr::lt_closed_form(event, zero, zero, far, zero);
        CHECK(up == Decimal(5));
        CHECK(down == Decimal(5));
    }
    // (2) with velocities: the roots satisfy their quadratics.  Event 2 (the target moves, the station moves): (c^2 - vr^2) tau^2 - 2 (D.vr) tau - D^2 = 0 for the up leg, D = r0 - s0, and
    //     (c^2 - vs^2) tau^2 + 2 (E.vs) tau - E^2 = 0 for the down leg, E = (r0 + vr tau_u) - (s0 + vs tau_u)
    const mr::Vec s0 = {D("4000000"), D("3100000"), D("3900000")};
    const mr::Vec vs = {D("-250"), D("380"), D("0")};
    const mr::Vec r0 = {D("9000000"), D("5000000"), D("6000000")};
    const mr::Vec vr = {D("-3000"), D("4500"), D("2800")};
    const auto dot = [](const mr::Vec& a, const mr::Vec& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    const auto minus = [](const mr::Vec& a, const mr::Vec& b) { return mr::Vec{a[0] - b[0], a[1] - b[1], a[2] - b[2]}; };
    const auto plus_scaled = [](const mr::Vec& a, const mr::Vec& b, const Decimal& k) { return mr::Vec{a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k}; };
    {
        const auto [tu, td] = mr::lt_closed_form(2, s0, vs, r0, vr);
        const mr::Vec d = minus(r0, s0);
        const Decimal up_residual = (c * c - dot(vr, vr)) * tu * tu - Decimal(2) * dot(d, vr) * tu - dot(d, d);
        CHECK(up_residual.abs() < D("1e-40"));
        const mr::Vec e = minus(plus_scaled(r0, vr, tu), plus_scaled(s0, vs, tu));
        const Decimal down_residual = (c * c - dot(vs, vs)) * td * td + Decimal(2) * dot(e, vs) * td - dot(e, e);
        CHECK(down_residual.abs() < D("1e-40"));
        // the roots taken are the positive ones (the product of a quadratic's two roots is -D^2 / k < 0: one of them is negative, and a time of flight is not)
        CHECK(tu > Decimal(0));
        CHECK(td > Decimal(0));
    }
    // event 1 (the tag is the bounce time): the up leg is solved with the STATION moving: (c^2 - vs^2) tau^2 - 2 (E.vs) tau - E^2 = 0 with E = r0 - s0
    {
        const auto [tu, td] = mr::lt_closed_form(1, s0, vs, r0, vr);
        const mr::Vec e = minus(r0, s0);
        const Decimal k = c * c - dot(vs, vs);
        CHECK(((k * tu * tu - Decimal(2) * dot(e, vs) * tu - dot(e, e))).abs() < D("1e-40"));
        CHECK(((k * td * td + Decimal(2) * dot(e, vs) * td - dot(e, e))).abs() < D("1e-40"));
    }
    // the two events differ (the target's velocity enters event 2's down leg), and a moving station makes the legs differ
    CHECK(mr::lt_closed_form(2, s0, vs, r0, vr).first != mr::lt_closed_form(1, s0, vs, r0, vr).first);
    // the overload for doubles reads each one exactly: whole numbers give the same
    const std::array<double, 3> ds0 = {4.0e6, 3.1e6, 3.9e6};
    const std::array<double, 3> dvs = {-250.0, 380.0, 0.0};
    const std::array<double, 3> dr0 = {9.0e6, 5.0e6, 6.0e6};
    const std::array<double, 3> dvr = {-3000.0, 4500.0, 2800.0};
    const auto from_doubles = mr::lt_closed_form(2, ds0, dvs, dr0, dvr);
    CHECK(from_doubles.first == mr::lt_closed_form(2, s0, vs, r0, vr).first);
    CHECK(from_doubles.second == mr::lt_closed_form(2, s0, vs, r0, vr).second);
}

TEST_CASE("the light-time section: both geometries, both events, against bc's closed forms to the double", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const mr::Values v = mr::lighttime_section();
    REQUIRE(v.size() == 52);
    // the inputs, in the order of the header: s0, vs, r0, vr, each x y z, for lageos and then leo
    CHECK(names(v)[0] == "lt_lageos_s0_x");
    CHECK(names(v)[11] == "lt_lageos_vr_z");
    CHECK(names(v)[12] == "lt_lageos_e2_tau_u_s");
    CHECK(names(v)[18] == "lt_lageos_e2_d_range_dz");
    CHECK(names(v)[19] == "lt_lageos_e1_tau_u_s");
    CHECK(names(v)[25] == "lt_lageos_e1_d_range_dz");
    CHECK(names(v)[26] == "lt_leo_s0_x");
    CHECK(names(v)[51] == "lt_leo_e1_d_range_dz");
    CHECK(value(v, "lt_lageos_s0_x") == 4.0e6);
    CHECK(value(v, "lt_lageos_vs_y") == 380.0);
    CHECK(value(v, "lt_lageos_r0_z") == 6.0e6);
    CHECK(value(v, "lt_lageos_vr_x") == -3000.0);
    CHECK(value(v, "lt_leo_s0_y") == -1.5e6);
    CHECK(value(v, "lt_leo_r0_y") == -0.7e6);
    // bc (lighttime_values.bc, 100 digits): the inputs are whole numbers, so the doubles of the section are the roundings of exact values and equal strtod's of bc's digits
    CHECK(value(v, "lt_lageos_e2_tau_u_s") == nearest(".0191675978029244667279449336334373018457646330208525782329065935318973747976299434859334195147080260"));
    CHECK(value(v, "lt_lageos_e2_tau_d_s") == nearest(".0191676095515480291231753657171950912673201508209757289210888476752073427746203096291502062666240066"));
    CHECK(value(v, "lt_lageos_e2_tof_s") == nearest(".0383352073544724958511202993506323931130847838418283071539954412071047175722502531150836257813320326"));
    CHECK(value(v, "lt_lageos_e2_range_m") == nearest("5746303.0203684934123010782980109444928969796551701957078376389201362051721903479862465385242688502836450654"));
    CHECK(value(v, "lt_lageos_e1_tau_u_s") == nearest(".0191675982663043448552996169353702674890486432605069097877691671512579351440687259570384943006218757"));
    CHECK(value(v, "lt_lageos_e1_tau_d_s") == nearest(".0191676100158889368085586617443315497782246595853573189218775034764927949006943385365173968055496242"));
    CHECK(value(v, "lt_lageos_e1_tof_s") == nearest(".0383352082821932816638582786797018172672733028458642287096466706277507300447630644935558911061714999"));
    CHECK(value(v, "lt_lageos_e1_range_m") == nearest("5746303.1594303407705472015645184012528113532089700161295695718495048971857069845660678228775497464622838771"));
    CHECK(value(v, "lt_leo_e2_tof_s") == nearest(".0091716780592600691791807997369500165485846965185216489914764821120757958321570710646445406419922158"));
    CHECK(value(v, "lt_leo_e2_range_m") == nearest("1374799.9546851229002383271897729994421204412952358238386839778107861171574142618882752318676718721957742182"));
    CHECK(value(v, "lt_leo_e1_tau_u_s") == nearest(".0045857540654028469900456580099150437107609456670322978772445512651146079165495198599684666865155158"));
    CHECK(value(v, "lt_leo_e1_tau_d_s") == nearest(".0045857422268062505497763334010243186874228258737822101401890573583239247625733479325033583489399866"));
    CHECK(value(v, "lt_leo_e1_tof_s") == nearest(".0091714962922090975398219914109393623981837715408145080174336086234385326791228677924718250354555024"));
    CHECK(value(v, "lt_leo_e1_range_m") == nearest("1374772.7084896258007124938437701997711521438029656143403035641905153170618937849097570811615625711070604496"));
    // the derivatives: bc's central difference of the same closed form at h = 1e-20, 100 digits (accurate to some 40 digits: the doubles agree)
    CHECK(value(v, "lt_lageos_e2_d_range_dx") == nearest(".87011547273035988565238617811834068462168837544763131548364594949868831477644889"));
    CHECK(value(v, "lt_leo_e1_d_range_dz") == nearest(".36369648372680825178432824477111461171980884926934883467760151717583778715891823"));
    // relations that need no oracle: the range is c times the time of flight over two, the time of flight is the two legs, the derivative of the range is the cosine of an angle (|d| <= 1.0001)
    for (const char* name : {"lageos", "leo"}) {
        for (const char* event : {"e2", "e1"}) {
            const std::string p = std::string("lt_") + name + "_" + event + "_";
            const double tof = value(v, p + "tof_s");
            CHECK(close(tof, value(v, p + "tau_u_s") + value(v, p + "tau_d_s"), 1e-15));
            CHECK(close(value(v, p + "range_m"), 299792458.0 * tof / 2, 1e-15));
            for (const char* axis : {"x", "y", "z"}) CHECK(std::fabs(value(v, p + "d_range_d" + axis)) < 1.0001);
        }
    }
}

TEST_CASE("the emission section: the closed form of MEAS-A-088 against bc, the light-time condition, and a unit direction", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const mr::Values v = mr::emission_section();
    CHECK(names(v) == std::vector<std::string>{"em_lageos_tau_s", "em_lageos_rho_m", "em_lageos_n_x", "em_lageos_n_y", "em_lageos_n_z", "em_leo_tau_s", "em_leo_rho_m", "em_leo_n_x", "em_leo_n_y", "em_leo_n_z"});
    CHECK(value(v, "em_lageos_tau_s") == nearest(".0191676104871351109737392825308623489213961958324951466237081059443235682071262731231169389964725587"));
    CHECK(value(v, "em_lageos_rho_m") == nearest("5746305.0619248122969200729610836844427990143404730762793918541555731736601450385359585637631885617022222846"));
    CHECK(value(v, "em_lageos_n_x") == nearest(".8701343644217552349580957872925191809479512289481064237569064508509416068023408245766392508669179121"));
    CHECK(value(v, "em_lageos_n_y") == nearest(".3306322454652978135084803366444609033561563654025611281555174418801782893709168789775206631473542328"));
    CHECK(value(v, "em_lageos_n_z") == nearest(".3654428903548721537091874622482503743065607373484989514777669401482981599405418527923826087933213441"));
    CHECK(value(v, "em_leo_tau_s") == nearest(".0045856513479712275386864400857383440730415467814094811443047517277089435598971236916373672616927270"));
    CHECK(value(v, "em_leo_n_z") == nearest(".3636941540418962566079991733380386704742899430443556280444280262772178356622205179005681597590641430"));
    for (const char* name : {"lageos", "leo"}) {
        const std::string p = std::string("em_") + name + "_";
        CHECK(close(value(v, p + "rho_m"), 299792458.0 * value(v, p + "tau_s"), 1e-15));   // rho = c tau
        const double nx = value(v, p + "n_x");
        const double ny = value(v, p + "n_y");
        const double nz = value(v, p + "n_z");
        CHECK(std::fabs(nx * nx + ny * ny + nz * nz - 1.0) < 4e-16);   // a unit direction
    }
}

TEST_CASE("the aberration section: A and D of MEAS-R-053 in four cases, against bc, unit vectors, inverse to each other; every number the test passes is the double it was", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const mr::Values v = mr::aberration_section();
    REQUIRE(v.size() == 48);
    CHECK(names(v)[0] == "ab_annual_n_x");
    CHECK(names(v)[1] == "ab_annual_beta_x");
    CHECK(names(v)[2] == "ab_annual_a_x");
    CHECK(names(v)[3] == "ab_annual_d_x");
    CHECK(names(v)[4] == "ab_annual_n_y");
    CHECK(names(v)[47] == "ab_diurnal_d_z");
    // the inputs are the doubles the test passes
    CHECK(value(v, "ab_annual_n_x") == 0.6);
    CHECK(value(v, "ab_annual_beta_y") == 9.9e-5);
    CHECK(value(v, "ab_oblique_n_y") == -0.6);
    CHECK(value(v, "ab_large_beta_z") == 5.0e-4);
    CHECK(value(v, "ab_diurnal_beta_y") == 1.55e-6);
    // the outputs against bc (lighttime_values.bc), to the 1e-14 the decimal inputs of bc (0.6 for the double nearest 0.6) allow
    CHECK(close(value(v, "ab_annual_a_x"), nearest(".5999999970596999927955298896944941811942107597043738444911983927098776161178635878"), 1e-14));
    CHECK(value(v, "ab_annual_a_y") == 9.9e-5);   // beta is along y and n.beta = 0: A = beta_y exactly in the 60 digits, and the double nearest it is the double beta
    CHECK(close(value(v, "ab_annual_a_z"), nearest(".7999999960795999903940398529259922415922810129391651259882645236131701548238181170"), 1e-14));
    CHECK(close(value(v, "ab_annual_d_x"), nearest(".5999999970596999927955298896944941811942107597043738444911983927098776161178635878"), 1e-14));
    CHECK(value(v, "ab_annual_d_y") == -9.9e-5);
    CHECK(close(value(v, "ab_oblique_a_x"), nearest(".4800271825373176734280302294437836692856063190864491936301829446404292554581649879"), 1e-14));
    CHECK(close(value(v, "ab_oblique_a_y"), nearest("-.5999964798439979143285418893047239918519578867183004642907061258007169799731999327"), 1e-14));
    CHECK(close(value(v, "ab_oblique_a_z"), nearest(".6399829124282114010770239183694813798402928526276054631555896257430511468693665763"), 1e-14));
    CHECK(close(value(v, "ab_oblique_d_x"), nearest(".4799728145370567076228199652811531336484280349508293188734982730050432316040109340"), 1e-14));
    CHECK(close(value(v, "ab_oblique_d_y"), nearest("-.6000035198439700810737317721153226600133042681025104806269927845435196858837943766"), 1e-14));
    CHECK(close(value(v, "ab_oblique_d_z"), nearest(".6400170884282878008058318691057241681751758324343129862812793944596532280996138093"), 1e-14));
    // unit vectors, all four cases, A and D alike, to the rounding of three doubles
    for (const char* name : {"annual", "oblique", "large", "diurnal"}) {
        for (const char* which : {"a", "d"}) {
            const std::string p = std::string("ab_") + name + "_" + which + "_";
            const double x = value(v, p + "x");
            const double y = value(v, p + "y");
            const double z = value(v, p + "z");
            INFO(p);
            CHECK(std::fabs(x * x + y * y + z * z - 1.0) < 4e-16);
        }
    }
    // the diurnal case: beta = 1.55e-6 along y with n = (0, 0.6, 0.8): n.beta = 9.3e-7, and A moves n toward beta by about beta (1 - n.beta ...): the y of A exceeds n_y, the y of D falls short
    CHECK(value(v, "ab_diurnal_a_y") > value(v, "ab_diurnal_n_y"));
    CHECK(value(v, "ab_diurnal_d_y") < value(v, "ab_diurnal_n_y"));
    // and the aberration's size: to the first order in beta the direction moves by the component of beta perpendicular to n, |A - n| = beta sin(theta), whose corrections are of the relative order beta
    // (7e-4 at most here): the bound of 2e-3 holds the size, the angle and the factor of the formula
    for (const char* name : {"annual", "oblique", "large", "diurnal"}) {
        const std::string n = std::string("ab_") + name + "_n_";
        const std::string a = std::string("ab_") + name + "_a_";
        const double nx = value(v, n + "x");
        const double ny = value(v, n + "y");
        const double nz = value(v, n + "z");
        const double dx = value(v, a + "x") - nx;
        const double dy = value(v, a + "y") - ny;
        const double dz = value(v, a + "z") - nz;
        const double bx = value(v, std::string("ab_") + name + "_beta_x");
        const double by = value(v, std::string("ab_") + name + "_beta_y");
        const double bz = value(v, std::string("ab_") + name + "_beta_z");
        const double shift = std::sqrt(dx * dx + dy * dy + dz * dz);
        const double beta = std::sqrt(bx * bx + by * by + bz * bz);
        const double cos_theta = (nx * bx + ny * by + nz * bz) / beta;
        const double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);
        INFO(name << ": |A - n| = " << shift << ", beta sin(theta) = " << beta * sin_theta);
        CHECK(std::fabs(shift - beta * sin_theta) <= 2e-3 * beta);
    }
}

TEST_CASE("the Shapiro section: the term and its partials at three geometries, against bc; the two-method guard", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const mr::Values v = mr::shapiro_section();
    REQUIRE(v.size() == 19);
    CHECK(names(v)[0] == "shapiro_two_gm_over_c2_m");
    CHECK(names(v)[1] == "shapiro_zenith_r1_m");
    CHECK(names(v)[6] == "shapiro_zenith_d_r");
    CHECK(names(v)[7] == "shapiro_el10_r1_m");
    CHECK(names(v)[18] == "shapiro_leo30_d_r");
    // 2 GM / c^2 with TN36-1's GM
    CHECK(close(value(v, "shapiro_two_gm_over_c2_m"), nearest(".008870056071559441094742601296982725261023474462145796463162"), 4e-16));
    // the zenith geometry is derived by hand: the line of sight is radial, rho = r2 - r1 = 5891863 m, and the delay is k ln(r2 / r1)
    CHECK(value(v, "shapiro_zenith_r1_m") == 6378137.0);
    CHECK(value(v, "shapiro_zenith_r2_m") == 12270000.0);
    CHECK(value(v, "shapiro_zenith_rho_m") == 5891863.0);
    CHECK(close(value(v, "shapiro_zenith_delay_m"), nearest(".005803511021732268685617902052168334049646677533192478910251"), 4e-16));
    CHECK(close(value(v, "shapiro_zenith_d_rho"), nearest(".000000001056801483928104814816292996796637796417312880670294"), 4e-16));
    CHECK(close(value(v, "shapiro_zenith_d_r"), nearest("-.000000000333895528625786877184459150261778361941072093209349"), 4e-16));
    // MEAS-A-025 of SPEC-measmod states the three delays to 0.01 mm: "LAGEOS-like zenith 5.80 mm; 10 deg elevation 9.88 mm; a LEO case, 2.25 mm"
    CHECK(std::fabs(value(v, "shapiro_zenith_delay_m") - 5.80e-3) < 5e-6);
    CHECK(std::fabs(value(v, "shapiro_el10_delay_m") - 9.88e-3) < 5e-6);
    CHECK(std::fabs(value(v, "shapiro_leo30_delay_m") - 2.25e-3) < 5e-6);
    // the 10 degree and the LEO geometries by bc from the definition (shapiro_el10_leo30.bc, 60 decimals): the tool's rho is the double nearest the 60-digit one, and its sine of the angle the platform's, so the
    // inputs of the logarithm differ from bc's by an ulp or two
    CHECK(close(value(v, "shapiro_el10_rho_m"), nearest("9432796.303154551010150041519021572583732086231875462751819651198201"), 1e-15));
    CHECK(close(value(v, "shapiro_el10_delay_m"), nearest(".009883204994431897039992769417219782211963534582884221747073"), 4e-15));
    CHECK(close(value(v, "shapiro_leo30_rho_m"), nearest("1735312.304418282394050311938348172024102399322622419916905951087284"), 1e-15));
    CHECK(close(value(v, "shapiro_leo30_delay_m"), nearest(".002246238125552408885332469631270341923143598341745630402337"), 4e-15));
    // the second and third geometry: r1 is always the station's, the target is farther than the station (rho > 0), the delay is positive, d/d rho is positive and d/d r is negative
    for (const char* name : {"zenith", "el10", "leo30"}) {
        const std::string p = std::string("shapiro_") + name + "_";
        CHECK(value(v, p + "r1_m") == 6378137.0);
        CHECK(value(v, p + "rho_m") > 0.0);
        CHECK(value(v, p + "delay_m") > 0.0);
        CHECK(value(v, p + "d_rho") > 0.0);
        CHECK(value(v, p + "d_r") < 0.0);
    }
    CHECK(value(v, "shapiro_el10_r2_m") == 12270000.0);
    CHECK(value(v, "shapiro_leo30_r2_m") == 7400000.0);
    // the geometry at 10 degrees: rho = -r1 sin(e) + sqrt(r1^2 sin^2(e) + r2^2 - r1^2), the 60-digit value rounded to a double (hand-checked below to 1e-12 against the law of cosines: |r2|^2 = r1^2 + rho^2 + 2 r1 rho sin(e))
    {
        const double r1 = value(v, "shapiro_el10_r1_m");
        const double r2 = value(v, "shapiro_el10_r2_m");
        const double rho = value(v, "shapiro_el10_rho_m");
        const double sin_e = std::sin(10.0 * std::acos(-1.0) / 180.0);
        CHECK(close(r1 * r1 + rho * rho + 2 * r1 * rho * sin_e, r2 * r2, 1e-13));
    }
    {
        const double r1 = value(v, "shapiro_leo30_r1_m");
        const double r2 = value(v, "shapiro_leo30_r2_m");
        const double rho = value(v, "shapiro_leo30_rho_m");
        CHECK(close(r1 * r1 + rho * rho + 2 * r1 * rho * 0.5, r2 * r2, 1e-13));   // sin 30 deg = 1/2
    }
    // the two-method guard FIRES when the arithmetic has too few digits: at 25 digits the central difference of the 1e-20 step cannot agree with the closed form to 1e-30
    {
        LocalContext coarse(25);
        CHECK_THROWS_AS(mr::shapiro_section(), mr::GuardFailed);
        try {
            (void)mr::shapiro_section();
        } catch (const mr::GuardFailed& exc) {
            const std::string what = exc.what();
            CHECK(what.find("shapiro: the closed form ") == 0);
            CHECK(what.find(" and the central difference ") != std::string::npos);
            CHECK(what.find(" of the partials disagree at zenith") != std::string::npos);
        }
    }
}

TEST_CASE("the vapour, zenith and normal-point sections against bc", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const mr::Values vapour = mr::vapour_section();
    CHECK(names(vapour) == std::vector<std::string>{"vapour_a_t_c", "vapour_a_rh", "vapour_a_e_hpa", "vapour_b_t_c", "vapour_b_rh", "vapour_b_e_hpa", "vapour_c_t_c", "vapour_c_rh", "vapour_c_e_hpa",
                                                    "vapour_d_t_c", "vapour_d_rh", "vapour_d_e_hpa"});
    // the doubles the test passes, and Marini & Murray's e at each: bc 6.11 rh 10^(7.5 t / (237.3 + t)); rh = 0.3 and 0.8 are not exact decimals (the double nearest each is used by the tool), so 1e-15
    CHECK(value(vapour, "vapour_a_t_c") == 20.0);
    CHECK(value(vapour, "vapour_a_rh") == 0.5);
    CHECK(close(value(vapour, "vapour_a_e_hpa"), nearest("11.694678421549668047560831944542391723050616658086658683406266"), 1e-15));
    CHECK(value(vapour, "vapour_b_e_hpa") == 6.11);   // t = 0: 10**0 = 1 exactly, times 6.11 and rh = 1
    CHECK(value(vapour, "vapour_c_t_c") == -10.0);
    CHECK(close(value(vapour, "vapour_c_e_hpa"), nearest(".857436684143802990210633495535816036559964049892513580576832"), 1e-15));
    CHECK(close(value(vapour, "vapour_d_e_hpa"), nearest("44.992640343968540609785589624491278498780267133183529920192719"), 1e-15));
    // the zenith delay at the IERS FCUL_ZD_HPA prolog's inputs: bc, from the formulas of TN36 9.3-9.7 at 60 digits
    const mr::Values zenith = mr::zenith_section();
    CHECK(names(zenith) == std::vector<std::string>{"zenith_lat_rad", "zenith_height_m", "zenith_p_hpa", "zenith_e_hpa", "zenith_lambda_um", "zenith_hydrostatic_m", "zenith_wet_m", "zenith_total_m"});
    CHECK(value(zenith, "zenith_lat_rad") == nearest(".535321570465705095135255672772388688374439946528651955760667"));   // 30.67166667 deg in radians: the double nearest bc's 60 decimals
    CHECK(value(zenith, "zenith_height_m") == 2010.344);
    CHECK(value(zenith, "zenith_p_hpa") == 798.4188);
    CHECK(value(zenith, "zenith_e_hpa") == 14.322);
    CHECK(value(zenith, "zenith_lambda_um") == 0.532);
    // the tool takes the cosine of the DOUBLE latitude and bc of the exact one: the two differ by 1e-19 of the result, a fraction of the last place of a double
    CHECK(close(value(zenith, "zenith_hydrostatic_m"), nearest("1.932995972236289609193291737725103663423112665271880122196785"), 4e-16));
    CHECK(close(value(zenith, "zenith_wet_m"), nearest(".002233752731683583062974328922064137017868374651549211211100"), 4e-16));
    CHECK(close(value(zenith, "zenith_total_m"), nearest("1.935229724967973192256266066647167800440981039923429333407885"), 4e-16));
    // the total is the sum of the two in 60 digits, rounded once: the sum of the two doubles is within an ulp of it
    CHECK(close(value(zenith, "zenith_total_m"), value(zenith, "zenith_hydrostatic_m") + value(zenith, "zenith_wet_m"), 4e-16));
    // the normal point: 299792458 m/s times the time of flight 0.051212898595 s over two, exact in decimal: 7676620.375549898255
    const mr::Values np = mr::np_section();
    CHECK(names(np) == std::vector<std::string>{"np_first_tof_s", "np_first_observed_range_m"});
    CHECK(value(np, "np_first_tof_s") == 0.051212898595);
    CHECK(value(np, "np_first_observed_range_m") == nearest("7676620.375549898255"));
}

TEST_CASE("the registry section on a worked tree: a station on the equator, the marker, the reference point and the elapsed years", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const Tree t = worked_tree();
    const mr::Values v = mr::registry_section(t.root);
    CHECK(names(v) == std::vector<std::string>{"yarl_elapsed_years", "yarl_marker_x_m", "yarl_marker_y_m", "yarl_marker_z_m", "yarl_marker_lat_rad", "yarl_marker_lon_rad", "yarl_marker_h_m", "yarl_srp_x_m",
                                               "yarl_srp_y_m", "yarl_srp_z_m", "yarl_srp_lat_rad", "yarl_srp_lon_rad", "yarl_srp_h_m", "yarl_srp_minus_marker_m"});
    // (4018 days * 86400 + 7676.8005871 + 2) / (365.25 * 86400): bc 11.000927789204093467...
    CHECK(value(v, "yarl_elapsed_years") == nearest("11.000927789204093467183816259791619134534945623241311126321393"));
    CHECK(value(v, "yarl_elapsed_years") == 11.000927789204093);   // the value in the committed header
    // no velocity: the marker is the station's own position, on the equator at longitude 0 (the geodetic iteration is exact there)
    CHECK(value(v, "yarl_marker_x_m") == 6378137.0);
    CHECK(value(v, "yarl_marker_y_m") == 0.0);
    CHECK(value(v, "yarl_marker_z_m") == 0.0);
    CHECK(value(v, "yarl_marker_lat_rad") == 0.0);
    CHECK(value(v, "yarl_marker_lon_rad") == 0.0);
    CHECK(value(v, "yarl_marker_h_m") == 0.0);
    // the eccentricity (up 1, north 2, east 3) at latitude 0 and longitude 0: up is +x, east is +y, north is +z, so the reference point is (a + 1, 3, 2), and the distance sqrt(14)
    CHECK(value(v, "yarl_srp_x_m") == 6378138.0);
    CHECK(value(v, "yarl_srp_y_m") == 3.0);
    CHECK(value(v, "yarl_srp_z_m") == 2.0);
    CHECK(value(v, "yarl_srp_minus_marker_m") == nearest("3.741657386773941385583748732316549301756019807778726946303745"));
    // its geodetic coordinates, by bc's iteration run to convergence (registry_values.bc).  The tool's latitude comes from an iteration that stops when two successive values differ by less than 1e-17 rad, which
    // leaves an error of that times the contraction of the iteration (about e^2 = 0.0067), 7e-20 rad at most; its longitude is an atan2 of two doubles; its height carries the rounding of the DOUBLE sine and cosine
    // of the latitude (the generator's own note: "the trigonometry at the end is the platform's"), 1.1e-16 of terms of 6.4e6 m, some 3e-9 m at most
    CHECK(within(value(v, "yarl_srp_lat_rad"), nearest(".000000315684450752979436598068736027339966935932466719608923"), 2e-19, "srp latitude"));
    CHECK(close(value(v, "yarl_srp_lon_rad"), nearest(".000000470356709121028545410303890081925014042345838937556195"), 1e-15));
    CHECK(within(value(v, "yarl_srp_h_m"), nearest("1.000001021219514434537936483067718027594723116298488751443453"), 5e-9, "srp height"));
    // the same station with velocities (0.5, -0.25, 0.125) m/y: the marker moves by the elapsed years
    const Tree moving(slrf_text("0.6378137000000000E+07", "0.0000000000000000E+00", "0.0000000000000000E+00", "0.5000000000000000E+00", "-.2500000000000000E+00", "0.1250000000000000E+00"),
                      ecc_text(ecc_row("00:000:00000", "1.0000", "2.0000", "3.0000", "70900513")), horizons_text(" X = 3.0 Y = 4.0 Z = 12.0\n"), sp3_text(sp3_record(3000.0, 4000.0, 12000.0)));
    const mr::Values w = mr::registry_section(moving.root);
    CHECK(value(w, "yarl_marker_x_m") == nearest("6378142.500463894602046733591908129895809567267472811620655563160696"));
    CHECK(value(w, "yarl_marker_y_m") == nearest("-2.750231947301023366795954064947904783633736405810327781580348"));
    CHECK(value(w, "yarl_marker_z_m") == nearest("1.375115973650511683397977032473952391816868202905163890790174"));
    CHECK(within(value(w, "yarl_marker_lat_rad"), nearest(".000000217051211246561984307551241660657328835734341531440568"), 2e-19, "marker latitude"));
    CHECK(close(value(w, "yarl_marker_lon_rad"), nearest("-.000000431196378428488112945395479531400464205968686863412010"), 1e-15));
    CHECK(within(value(w, "yarl_marker_h_m"), nearest("5.500464636782368333536104731461112339906198762468817687259058"), 5e-9, "marker height"));
    // the rotation by the marker's own latitude and longitude keeps the length of the eccentricity vector (the double sine and cosine are orthogonal to 1e-16)
    CHECK(close(value(w, "yarl_srp_minus_marker_m"), std::sqrt(14.0), 1e-14));
}

TEST_CASE("the registry section refuses what it cannot read: a missing estimate, one that is no number, no open-ended eccentricity or two, a short line, a file that is missing", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    const auto refusal = [](const std::string& slrf, const std::string& ecc) {
        const Tree t(slrf, ecc, horizons_text(" X = 3.0 Y = 4.0 Z = 12.0\n"), sp3_text(sp3_record(3000.0, 4000.0, 12000.0)));
        try {
            (void)mr::registry_section(t.root);
        } catch (const mr::MissingInput& exc) {
            return std::string(exc.what());
        }
        return std::string("(nothing was refused)");
    };
    const std::string good_slrf = slrf_text("0.6378137000000000E+07", "0.0", "0.0", "0.0", "0.0", "0.0");
    const std::string good_ecc = ecc_text(ecc_row("00:000:00000", "1.0000", "2.0000", "3.0000", "70900513"));
    CHECK(refusal(good_slrf, good_ecc) == "(nothing was refused)");
    // one of the six estimates missing, in turn
    for (const char* type : {"STAX", "STAY", "STAZ", "VELX", "VELY", "VELZ"}) {
        std::string slrf = good_slrf;
        const std::size_t at = slrf.find(std::string(type) + "   7090");
        REQUIRE(at != std::string::npos);
        slrf.replace(at, 4, "XXXX");
        CHECK(refusal(slrf, good_ecc) == std::string("SLRF2020 holds no ") + type + " estimate for 7090 A 1");
    }
    // an estimate of another type for the same pad is more than the six
    {
        std::string slrf = good_slrf;
        slrf.insert(slrf.find("-SOLUTION/ESTIMATE"), estimate_line(9, "ROTX", "7090", "0.0"));
        CHECK(refusal(slrf, good_ecc) == "SLRF2020 holds estimates of other types for 7090 A 1");
    }
    // a value that is no number
    CHECK(refusal(slrf_text("0.6378137000000000E+07", "abc", "0.0", "0.0", "0.0", "0.0"), good_ecc) == "the estimate of Yarragadee: 'abc' is not a number");
    // a line of the block with too few fields (a pad, a point and a solution are read first)
    {
        std::string slrf = good_slrf;
        slrf.insert(slrf.find("-SOLUTION/ESTIMATE"), "     9 STAX\n");
        CHECK(refusal(slrf, good_ecc) == "SOLUTION/ESTIMATE: a line has 2 fields, field 2 is wanted");
    }
    // no open-ended eccentricity row for SOD 70900513, and two
    CHECK(refusal(good_slrf, ecc_text(ecc_row("99:365:86399", "1.0", "2.0", "3.0", "70900513"))) == "the eccentricity file holds 0 open-ended rows of SOD 70900513, one is wanted");
    CHECK(refusal(good_slrf, ecc_text(ecc_row("00:000:00000", "1.0", "2.0", "3.0", "70900513") + ecc_row("00:000:00000", "1.5", "2.5", "3.5", "70900513"))) ==
          "the eccentricity file holds 2 open-ended rows of SOD 70900513, one is wanted");
    // another SOD's open-ended row does not count, and a row of the SOD whose span is closed does not either
    CHECK(refusal(good_slrf, ecc_text(ecc_row("00:000:00000", "1.0", "2.0", "3.0", "70900513") + ecc_row("00:000:00000", "9.0", "9.0", "9.0", "70900999") + ecc_row("14:365:86399", "7.0", "7.0", "7.0", "70900513"))) ==
          "(nothing was refused)");
    // an eccentricity that is no number; a row of the SOD that stops short of its sixth field (its last field is the SOD, so the test reaches for field 5, the end of the span, and there is none); a blank line in the block
    CHECK(refusal(good_slrf, ecc_text(ecc_row("00:000:00000", "x", "2.0", "3.0", "70900513"))) == "the eccentricity: 'x' is not a number");
    CHECK(refusal(good_slrf, ecc_text(" 7090  A    1 L 70900513\n")) == "SITE/ECCENTRICITY: a line has 5 fields, field 5 is wanted");
    CHECK(refusal(good_slrf, ecc_text("\n")) == "SITE/ECCENTRICITY: a line has 0 fields, field 0 is wanted");
    // a row of the SOD that has the span (field 5) and ends with the SOD, with the eccentricity fields (7, 8 and 9) missing, one more of them in each: 7, 8 and 9 fields (the fields 7 and 8 of the longer rows are numbers
    // by luck of the layout: the SOD is a number)
    CHECK(refusal(good_slrf, ecc_text(" 7090  A    1 L 14:080:00000 00:000:00000 70900513\n")) == "SITE/ECCENTRICITY: a line has 7 fields, field 7 is wanted");
    CHECK(refusal(good_slrf, ecc_text(" 7090  A    1 L 14:080:00000 00:000:00000 1.0000 70900513\n")) == "SITE/ECCENTRICITY: a line has 8 fields, field 8 is wanted");
    CHECK(refusal(good_slrf, ecc_text(" 7090  A    1 L 14:080:00000 00:000:00000 1.0000 2.0000 70900513\n")) == "SITE/ECCENTRICITY: a line has 9 fields, field 9 is wanted");
    // a line of the estimate block that has the pad, the point and the solution but not the estimate's value (field 8): the six fields of the estimate's own columns before it are not all there
    {
        std::string slrf = good_slrf;
        slrf.insert(slrf.find("-SOLUTION/ESTIMATE"), "     9 STAX 7090  A    1 R 15:001:00000\n");
        CHECK(refusal(slrf, good_ecc) == "SOLUTION/ESTIMATE: a line has 7 fields, field 8 is wanted");
    }
    // a file that is missing
    {
        const Tree t = worked_tree();
        fs::remove(t.root / mr::kEccRelative);
        CHECK_THROWS_AS(mr::registry_section(t.root), mr::MissingInput);
        try {
            (void)mr::registry_section(t.root);
        } catch (const mr::MissingInput& exc) {
            CHECK(std::string(exc.what()).find(mr::kEccRelative) != std::string::npos);
        }
    }
}

TEST_CASE("the position section: the first Horizons record after $$SOE and the first P record of the SP3, as text, in metres and kilometres", "[measmod_reference][behaviour]") {
    const Sixty sixty;
    // a record of (3, 4, 12) km has the norm 13 km exactly; the SP3's (3000, 4000, 12000) km has 13000
    const Tree t = worked_tree();
    const mr::Values v = mr::position_section(t.root);
    CHECK(names(v) == std::vector<std::string>{"position_horizons_x_km", "position_horizons_x_m", "position_horizons_y_km", "position_horizons_y_m", "position_horizons_z_km", "position_horizons_z_m",
                                               "position_horizons_norm_km", "position_sp3_x_km", "position_sp3_x_m", "position_sp3_y_km", "position_sp3_y_m", "position_sp3_z_km", "position_sp3_z_m",
                                               "position_sp3_norm_km"});
    CHECK(value(v, "position_horizons_x_km") == 3.0);
    CHECK(value(v, "position_horizons_x_m") == 3000.0);
    CHECK(value(v, "position_horizons_y_km") == 4.0);
    CHECK(value(v, "position_horizons_y_m") == 4000.0);
    CHECK(value(v, "position_horizons_z_km") == 12.0);
    CHECK(value(v, "position_horizons_z_m") == 12000.0);
    CHECK(value(v, "position_horizons_norm_km") == 13.0);
    CHECK(value(v, "position_sp3_x_km") == 3000.0);
    CHECK(value(v, "position_sp3_x_m") == 3000000.0);
    CHECK(value(v, "position_sp3_z_m") == 12000000.0);
    CHECK(value(v, "position_sp3_norm_km") == 13000.0);
    // the Horizons record: the FIRST match after the first $$SOE; an earlier "X =" is not read; a number may stand against its label, carry a sign and an exponent; the text between the numbers is white space only
    const auto horizons_of = [](const std::string& body) {
        const Tree tree(slrf_text("0.6378137000000000E+07", "0.0", "0.0", "0.0", "0.0", "0.0"), ecc_text(ecc_row("00:000:00000", "1.0", "2.0", "3.0", "70900513")), body, sp3_text(sp3_record(3000.0, 4000.0, 12000.0)));
        return mr::position_section(tree.root);
    };
    {
        const mr::Values first = horizons_of(horizons_text(" X = 3.0 Y =-4.0 Z =12.0\n X = 9.0 Y = 9.0 Z = 9.0\n"));
        CHECK(value(first, "position_horizons_x_km") == 3.0);
        CHECK(value(first, "position_horizons_y_km") == -4.0);
        CHECK(value(first, "position_horizons_z_km") == 12.0);
        CHECK(value(first, "position_horizons_norm_km") == 13.0);
        const mr::Values exponent = horizons_of(horizons_text(" X =+1.5E+01 Y =\t-2.0E-01   Z =     1.0E+01 \n"));
        CHECK(value(exponent, "position_horizons_x_km") == 15.0);
        CHECK(value(exponent, "position_horizons_y_km") == -0.2);
        CHECK(value(exponent, "position_horizons_z_km") == 10.0);
        // a label inside another word counts (the pattern has no word boundary): VX = ... Y = ... Z = ... is a record
        const mr::Values inside = horizons_of(horizons_text(" VX = 3.0 Y = 4.0 Z = 12.0\n"));
        CHECK(value(inside, "position_horizons_x_km") == 3.0);
        // the first X = that does not begin a whole record is passed over for the next
        const mr::Values passed = horizons_of(horizons_text(" X = 7.0 W = 8.0\n X = 3.0 Y = 4.0 Z = 12.0\n"));
        CHECK(value(passed, "position_horizons_x_km") == 3.0);
        CHECK(value(passed, "position_horizons_y_km") == 4.0);
    }
    // refused: no $$SOE, no record after it (a record BEFORE $$SOE does not count), a record whose number is no number, a record cut short
    const auto refused = [&](const std::string& horizons) {
        const Tree tree(slrf_text("0.6378137000000000E+07", "0.0", "0.0", "0.0", "0.0", "0.0"), ecc_text(ecc_row("00:000:00000", "1.0", "2.0", "3.0", "70900513")), horizons, sp3_text(sp3_record(3000.0, 4000.0, 12000.0)));
        try {
            (void)mr::position_section(tree.root);
        } catch (const mr::MissingInput& exc) {
            return std::string(exc.what());
        }
        return std::string("(nothing was refused)");
    };
    CHECK(refused(" X = 3.0 Y = 4.0 Z = 12.0\n").find(": no $$SOE marker") != std::string::npos);
    CHECK(refused("$$SOE\nnothing here\n$$EOE\n").find(": no first Horizons record after $$SOE") != std::string::npos);
    CHECK(refused(" X = 3.0 Y = 4.0 Z = 12.0\n$$SOE\nnothing\n").find(": no first Horizons record after $$SOE") != std::string::npos);
    CHECK(refused("$$SOE\n X = 3.0 Y = 4.0\n").find(": no first Horizons record after $$SOE") != std::string::npos);
    CHECK(refused("$$SOE\n X = 3.0Y = 4.0 Z = 12.0\n").find(": no first Horizons record after $$SOE") != std::string::npos);   // no white space between the number and the next label
    CHECK(refused("$$SOE\n X = --3.0 Y = 4.0 Z = 12.0\n") == "the first Horizons X: '--3.0' is not a number");
    // a token is any run of -+0-9.E: "..." is the third number of a record, and no number
    CHECK(refused("$$SOE\n X = 3.0 Y = 4.0 Z = ... \n") == "the first Horizons Z: '...' is not a number");
    CHECK(refused("$$SOE\n X = 3.0 Y = 4.0 Z = e5\n").find(": no first Horizons record after $$SOE") != std::string::npos);   // a lower-case e is no character of the token
    // the SP3: the first line that BEGINS with P (headers begin with #, +, % or *); fields are the 14 columns after the 4 of the record's name; refused when there is none or when it is cut short or not numeric
    {
        const std::string sp3 = sp3_text("*  2026  1  3  0  0  0.00000000\nPL51" + pad_left("1.000000", 14) + pad_left("2.000000", 14) + pad_left("2.000000", 14) + "\n" + sp3_record(7.0, 8.0, 9.0));
        const Tree tree(slrf_text("0.6378137000000000E+07", "0.0", "0.0", "0.0", "0.0", "0.0"), ecc_text(ecc_row("00:000:00000", "1.0", "2.0", "3.0", "70900513")), horizons_text(" X = 3.0 Y = 4.0 Z = 12.0\n"), sp3);
        const mr::Values first = mr::position_section(tree.root);
        CHECK(value(first, "position_sp3_x_km") == 1.0);
        CHECK(value(first, "position_sp3_y_km") == 2.0);
        CHECK(value(first, "position_sp3_z_km") == 2.0);
        CHECK(value(first, "position_sp3_norm_km") == 3.0);   // sqrt(1 + 4 + 4)
    }
    {
        const Tree tree(slrf_text("0.6378137000000000E+07", "0.0", "0.0", "0.0", "0.0", "0.0"), ecc_text(ecc_row("00:000:00000", "1.0", "2.0", "3.0", "70900513")), horizons_text(" X = 3.0 Y = 4.0 Z = 12.0\n"),
                        sp3_text("*  2026  1  3  0  0  0.00000000\n"));
        try {
            (void)mr::position_section(tree.root);
            FAIL("nothing was refused");
        } catch (const mr::MissingInput& exc) {
            CHECK(std::string(exc.what()).find(": no P record") != std::string::npos);
        }
        const Tree cut(slrf_text("0.6378137000000000E+07", "0.0", "0.0", "0.0", "0.0", "0.0"), ecc_text(ecc_row("00:000:00000", "1.0", "2.0", "3.0", "70900513")), horizons_text(" X = 3.0 Y = 4.0 Z = 12.0\n"),
                       sp3_text("PL51   1.0\n"));
        CHECK_THROWS_AS(mr::position_section(cut.root), mr::MissingInput);
        const Tree text(slrf_text("0.6378137000000000E+07", "0.0", "0.0", "0.0", "0.0", "0.0"), ecc_text(ecc_row("00:000:00000", "1.0", "2.0", "3.0", "70900513")), horizons_text(" X = 3.0 Y = 4.0 Z = 12.0\n"),
                        sp3_text("PL51" + pad_left("abc", 14) + pad_left("2.000000", 14) + pad_left("2.000000", 14) + "\n"));
        try {
            (void)mr::position_section(text.root);
            FAIL("nothing was refused");
        } catch (const mr::MissingInput& exc) {
            CHECK(std::string(exc.what()) == "an SP3 coordinate: '" + std::string(11, ' ') + "abc' is not a number");   // columns 4 to 17 of the record: eleven blanks and the three letters
        }
    }
}

TEST_CASE("the generator's other guards fire when the arithmetic has too few digits: A(D(n)) = n, the light-time condition", "[measmod_reference][behaviour]") {
    {
        LocalContext coarse(25);
        try {
            (void)mr::aberration_section();
            FAIL("nothing was refused");
        } catch (const mr::GuardFailed& exc) {
            const std::string what = exc.what();
            CHECK(what.find("aberration: A(D(n)) is ") == 0);
            CHECK(what.find(" in the case annual") != std::string::npos);
            // the first component to fail is the first one: n[0] = 0.6, as the double it is (the second component of the case is 0, and has its own residual)
            CHECK(what.find(", not 0.59999999999999997779553950749686919152736663818359375 in the case annual") != std::string::npos);
        }
        try {
            (void)mr::emission_section();
            FAIL("nothing was refused");
        } catch (const mr::GuardFailed& exc) {
            CHECK(std::string(exc.what()) == "emission: the light-time condition fails in the case lageos");
        }
    }
    // at 60 digits both hold
    const Sixty sixty;
    CHECK_NOTHROW(mr::aberration_section());
    CHECK_NOTHROW(mr::emission_section());
    CHECK_NOTHROW(mr::shapiro_section());
}

TEST_CASE("render lays the header out: two comment lines, the pragma, the namespace, each section under its name, and the values as Python's repr prints them", "[measmod_reference][behaviour]") {
    const mr::Sections sections = {{"alpha", {{"a_one", 1.0}, {"a_two", 0.1}}}, {"beta", {{"b_x", 1e-5}, {"b_y", -250.0}, {"b_z", 1e16}, {"b_w", 123456789.123456789}}}, {"empty", {}}};
    CHECK(mr::render(sections) ==
          "// GENERATED by tools/measmod_reference.cpp \xE2\x80\x94 do not edit; `measmod_reference --check` verifies it.\n"
          "// Reference values computed independently of the code under test (see the generator's header).\n"
          "#pragma once\n"
          "\n"
          "namespace odl::measmod::ref {\n"
          "\n"
          "// section: alpha\n"
          "inline constexpr double a_one = 1.0;\n"
          "inline constexpr double a_two = 0.1;\n"
          "\n"
          "// section: beta\n"
          "inline constexpr double b_x = 1e-05;\n"
          "inline constexpr double b_y = -250.0;\n"
          "inline constexpr double b_z = 1e+16;\n"
          "inline constexpr double b_w = 123456789.12345679;\n"
          "\n"
          "// section: empty\n"
          "\n"
          "}  // namespace odl::measmod::ref\n");
    CHECK(mr::render({}) ==
          "// GENERATED by tools/measmod_reference.cpp \xE2\x80\x94 do not edit; `measmod_reference --check` verifies it.\n"
          "// Reference values computed independently of the code under test (see the generator's header).\n"
          "#pragma once\n"
          "\n"
          "namespace odl::measmod::ref {\n"
          "\n"
          "}  // namespace odl::measmod::ref\n");
}

TEST_CASE("the tool writes the header, checks it, and says what it did", "[measmod_reference][behaviour]") {
    const Tree t = worked_tree();
    const fs::path header = t.root / "out" / "header.hpp";
    // --header PATH writes it (making the directory), printing "wrote    PATH"
    const Result wrote = t.run({"--header", header.string()});
    CHECK(wrote.code == 0);
    CHECK(wrote.out == "wrote    " + header.string() + "\n");
    CHECK(wrote.err.empty());
    const std::string text = read_text(header);
    CHECK(text.rfind("// GENERATED by tools/measmod_reference.cpp \xE2\x80\x94 do not edit; `measmod_reference --check` verifies it.\n", 0) == 0);
    CHECK(text.find("// section: registry\n") != std::string::npos);
    CHECK(text.find("// section: emission\n") != std::string::npos);
    CHECK(text.find("inline constexpr double yarl_marker_x_m = 6378137.0;\n") != std::string::npos);
    CHECK(text.find("inline constexpr double position_horizons_norm_km = 13.0;\n") != std::string::npos);
    CHECK(text.find("inline constexpr double position_sp3_norm_km = 13000.0;\n") != std::string::npos);
    // 204 lines: 6 at the head, 2 for each of the 9 sections, 179 values, 1 at the foot
    CHECK(std::count(text.begin(), text.end(), '\n') == 204);
    // it is what render() gives for all_sections()
    {
        Sixty sixty;
        CHECK(text == mr::render(mr::all_sections(t.root)));
    }
    // --check on it: ok; on a stale copy: FAILED to the error stream, exit 1; on a missing file: FAILED; the default header path is under the root
    const Result ok = t.run({"--check", "--header", header.string()});
    CHECK(ok.code == 0);
    CHECK(ok.out == "ok       " + header.string() + " matches the generator\n");
    CHECK(ok.err.empty());
    write_text(header, text + "// stale\n");
    const Result stale = t.run({"--check", "--header", header.string()});
    CHECK(stale.code == 1);
    CHECK(stale.out.empty());
    CHECK(stale.err == "FAILED   " + header.string() + " differs from what the generator writes\n");
    fs::remove(header);
    const Result missing = t.run({"--check", "--header", header.string()});
    CHECK(missing.code == 1);
    CHECK(missing.err == "FAILED   " + header.string() + " differs from what the generator writes\n");
    // text read back is compared as Python's read_text() hands it back: a CR LF file equals its LF twin
    std::string crlf;
    for (const char ch : text) {
        if (ch == '\n') crlf += '\r';
        crlf += ch;
    }
    write_text(header, crlf);
    CHECK(t.run({"--check", "--header", header.string()}).code == 0);
    // the default header is ROOT/modules/measmod/tests/measmod_reference.hpp
    const Result defaulted = t.run({});
    CHECK(defaulted.code == 0);
    CHECK(defaulted.out == "wrote    " + (t.root / mr::kHeaderRelative).string() + "\n");
    CHECK(read_text(t.root / mr::kHeaderRelative) == text);
    CHECK(t.run({"--check"}).code == 0);
    // a header that is not UTF-8 is refused, exit 2, naming it; a directory in its place; a path that cannot be written
    write_text(header, std::string("\xFF\xFE not utf-8\n"));
    const Result bytes = t.run({"--check", "--header", header.string()});
    CHECK(bytes.code == 2);
    CHECK(bytes.err.find(header.string()) != std::string::npos);
    const Result unwritable = t.run({"--header", (t.root / mr::kSlrfRelative / "x" / "header.hpp").string()});
    CHECK(unwritable.code == 2);
    CHECK(unwritable.err.find("measmod_reference: ") == 0);
}

TEST_CASE("an input that is missing is refused, exit 2, with what to do; nothing is written", "[measmod_reference][behaviour]") {
    for (const char* relative : {mr::kSlrfRelative, mr::kEccRelative, mr::kHorizonsRelative, mr::kSp3Relative}) {
        const Tree copy = worked_tree();
        fs::remove(copy.root / relative);
        const fs::path header = copy.root / "h.hpp";
        const Result r = copy.run({"--header", header.string()});
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err.find("measmod_reference: ") == 0);
        CHECK(r.err.find(relative) != std::string::npos);
        CHECK(r.err.find(" (run `tools/bootstrap.sh`)\n") != std::string::npos);
        CHECK_FALSE(fs::exists(header));
        // and --check says the same, not "FAILED"
        const Result c = copy.run({"--check", "--header", header.string()});
        CHECK(c.code == 2);
        CHECK(c.err.find("FAILED") == std::string::npos);
    }
}

TEST_CASE("the command line: --check, --header PATH and --header=PATH, --root DIR and --root=DIR, -h; everything else is refused, exit 2", "[measmod_reference][behaviour]") {
    const Tree t = worked_tree();
    const auto run_args = [](const std::vector<std::string>& args) {
        std::ostringstream out;
        std::ostringstream err;
        const int code = mr::run(args, Streams{out, err});
        return Result{code, out.str(), err.str()};
    };
    const std::string help =
        std::string(kUsage) +
        "\n"
        "Reference values for SPEC-measmod.md's acceptance tests, from the DEFINITION, in 60-digit decimal arithmetic: the header modules/measmod/tests/measmod_reference.hpp.  --check regenerates it and fails\n"
        "if the committed one differs, so that a reference value cannot drift away from its generator.\n"
        "\n"
        "options:\n"
        "  -h, --help       show this help and exit\n"
        "  --check          fail (exit 1) unless the committed header is what the generator writes\n"
        "  --header HEADER  the header to write or to check (default: ROOT/modules/measmod/tests/measmod_reference.hpp)\n"
        "  --root ROOT      the tree whose data/ holds the inputs (default: the tree this tool was built from)\n"
        "\n"
        "exit codes: 0 written / the committed header matches   1 --check found a difference, or a guard of the generator refused\n"
        "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected\n";
    for (const char* flag : {"-h", "--help"}) {
        const Result r = run_args({flag});
        CHECK(r.code == 0);
        CHECK(r.out == help);
        CHECK(r.err.empty());
    }
    const fs::path header = t.root / "h.hpp";
    CHECK(run_args({"--root=" + t.root.string(), "--header=" + header.string()}).code == 0);
    CHECK(run_args({"--root", t.root.string(), "--check", "--header", header.string()}).code == 0);
    CHECK(run_args({"-h", "--frobnicate"}).code == 0);
    CHECK(run_args({"--frobnicate", "-h"}).code == 2);
    const Result unknown = run_args({"--frobnicate"});
    CHECK(unknown.code == 2);
    CHECK(unknown.err == std::string(kUsage) + "measmod_reference: error: unrecognized arguments: --frobnicate\n");
    CHECK(unknown.out.empty());
    CHECK(run_args({"stray"}).code == 2);
    CHECK(run_args({"--check=1"}).code == 2);
    for (const char* option : {"--header", "--root"}) {
        const Result bare = run_args({option});
        CHECK(bare.code == 2);
        CHECK(bare.err == std::string(kUsage) + "measmod_reference: error: argument " + option + ": expected one argument\n");
        // an argument that looks like an option is not the value of this one: the refusal says so (a root or a header taken from it would be refused in other words, or written under a stray name)
        for (const char* option_like : {"--check", "---x", "--other"}) {
            const Result r = run_args({option, option_like});
            CHECK(r.code == 2);
            CHECK(r.err == std::string(kUsage) + "measmod_reference: error: argument " + option + ": expected one argument\n");
            CHECK(r.out.empty());
        }
    }
    CHECK(mr::default_settings().root == mr::default_root());
}

TEST_CASE("an error the tool did not anticipate is reported with exit status 70", "[measmod_reference][behaviour]") {
    const Tree t = worked_tree();
    odl::devtools_testing::ThrowingStream out;
    std::ostringstream err;
    CHECK(mr::run({"--root", t.root.string(), "--header", (t.root / "h.hpp").string()}, Streams{out, err}) == 70);
    CHECK(err.str() == "measmod_reference: internal error: boom\n");
}

TEST_CASE("on the real tree the header is what the generator writes, and each section has the values the specification's rows use", "[measmod_reference][real_tree]") {
    Sixty sixty;
    const mr::Sections sections = mr::all_sections(mr::default_root());
    REQUIRE(sections.size() == 9);
    const std::vector<std::pair<std::string, std::size_t>> shape = {{"registry", 14}, {"shapiro", 19}, {"vapour", 12}, {"zenith", 8}, {"np", 2}, {"lighttime", 52}, {"position", 14}, {"aberration", 48}, {"emission", 10}};
    for (std::size_t i = 0; i < shape.size(); ++i) {
        CHECK(sections[i].first == shape[i].first);
        CHECK(sections[i].second.size() == shape[i].second);
    }
    // the header of 204 lines: 6 lines at the head, 2 per section, 179 values, 1 at the foot
    const std::string text = mr::render(sections);
    CHECK(std::count(text.begin(), text.end(), '\n') == 204);
    // Yarragadee's marker and reference point at the first normal point: the elapsed years of the specification, MEAS-A-010
    const mr::Values& registry = sections[0].second;
    CHECK(value(registry, "yarl_elapsed_years") == 11.000927789204093);
    // the real tool, in the way the ctest runs it: the committed header matches
    std::ostringstream out;
    std::ostringstream err;
    CHECK(mr::run({"--check"}, Streams{out, err}) == 0);
    CHECK(err.str().empty());
    CHECK(out.str() == "ok       " + (mr::default_root() / mr::kHeaderRelative).string() + " matches the generator\n");
}
