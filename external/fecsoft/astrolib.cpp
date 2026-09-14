/*********************************************************************
Project:  fecsoftc
Filename: astrolib.c
Author:   Joe Heafner
Purpose:  Astronomical ephemeris library.
Thanks to Charles Gamble for extensive modifications.

Source: http://www.willbell.com/math/fundephcomp.htm
Fundamental Ephemeris Computations For Use With JPL Data, Paul J. Heafners
*********************************************************************/

/* Header Files *****************************************************/
#include "astrolib.h"

#include <cmath>

Fecsoft::~Fecsoft()
{
#ifdef SGNL_USE_CALCEPH
    if (calceph_eph != NULL) {
        calceph_close(calceph_eph);
        calceph_eph = NULL;
    }
#endif

    // NULL when the ephemeris was opened through CALCEPH rather than read here.
    if (fpBinaryFile != NULL && fclose(fpBinaryFile) != 0) {
        std::cerr << "Error, could not close ephemeris file." << std::endl;
    }
}

/*********************************************************************
Name:    looks_like_fecsoft
Purpose: Decide whether a file is in FECsoft's container rather than the
         layout JPL publishes, so ephopn() knows whether it can read the
         file itself or should hand it to CALCEPH.

         The check mirrors what ephopn() would do: the constant count is a
         short at offset 195, the header is then 317 + 14 * ncon bytes, and
         the coefficient records that follow are ncoeff doubles each, so the
         remaining bytes must divide exactly by that record size.
Inputs:  FileName - file to examine.
Outputs: None.
Returns: true if the file parses as a FECsoft container.
*********************************************************************/
bool Fecsoft::looks_like_fecsoft(const std::string &FileName)
{
    FILE *fp = fopen(FileName.c_str(), "rb");
    if (fp == NULL) {
        return false;
    }

    short ncon = 0;
    short hdr_ipt[3][12] = {};
    short numde = 0;
    short hdr_lpt[3] = {};
    bool ok = (fseek(fp, 195L, SEEK_SET) == 0) &&
              (fread(&ncon, sizeof(short), 1, fp) == 1);

    if (ok) {
        convert_little_endian(reinterpret_cast<char *>(&ncon), sizeof(short));
        ok = (ncon > 0 && ncon <= 400);
    }

    if (ok) {
        /* skip the constant names, SS[3], au and emrat */
        const long offset = 195L + 2L + 6L * ncon + 24L + 8L + 8L;
        ok = (fseek(fp, offset, SEEK_SET) == 0) &&
             (fread(hdr_ipt, sizeof(short), 36, fp) == 36) &&
             (fread(&numde, sizeof(short), 1, fp) == 1) &&
             (fread(hdr_lpt, sizeof(short), 3, fp) == 3);
    }

    long ncoeff_check = 0;
    if (ok) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 12; j++) {
                convert_little_endian(
                    reinterpret_cast<char *>(&hdr_ipt[i][j]), sizeof(short));
            }
        }
        for (int i = 0; i < 3; i++) {
            convert_little_endian(reinterpret_cast<char *>(&hdr_lpt[i]),
                                  sizeof(short));
        }

        for (int j = 0; j < 12; j++) {
            ncoeff_check += static_cast<long>(hdr_ipt[1][j]) *
                            static_cast<long>(hdr_ipt[2][j]) *
                            ((j == 11) ? 2L : 3L);
        }
        ncoeff_check += static_cast<long>(hdr_lpt[1]) *
                            static_cast<long>(hdr_lpt[2]) * 3L +
                        2L;
        ok = (ncoeff_check > 0);
    }

    if (ok) {
        ok = (fseek(fp, 0L, SEEK_END) == 0);
    }

    if (ok) {
        const long size = ftell(fp);
        const long header = 317L + 14L * ncon;
        const long record = ncoeff_check * 8L;
        ok = (size > header) && ((size - header) % record == 0);
    }

    fclose(fp);
    return ok;
}

/*********************************************************************
Name:    ephopn
Purpose: Function to open a binary ephemeris data file for access,
         read the header information, and store it in memory for
         later use.
Inputs:  FileName - Filename of file to be read in.
Outputs: None.
Returns: NULL on error.
         Open file pointer otherwise.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::ephopn(std::string FileName)
{
    /* Temporary data used when reading in binary file */
    short tmpShort;
    double tmpDouble;

    /* Make sure all variables read from the data file are global */
    int i, j;
    long curpos;

    char ttl[3][65] = {};
    char cnam[400][7] = {};

#ifdef SGNL_USE_CALCEPH
    // Anything that is not FECsoft's own container goes to CALCEPH, which
    // reads JPL DE binaries, INPOP files and SPICE kernels. A FECsoft file is
    // still read below, so res/1980_2020 behaves exactly as it always did.
    if (!looks_like_fecsoft(FileName)) {
        calceph_eph = calceph_open(FileName.c_str());
        if (calceph_eph == NULL) {
            std::cerr << "EPHOPN: " << FileName
                      << " is neither a FECsoft container nor an ephemeris "
                         "CALCEPH can read." << std::endl;
            std::exit(1);
        }
        calceph_prefetch(calceph_eph);
        return;
    }
#endif

    if ((fpBinaryFile = fopen(FileName.c_str(), "rb")) == NULL) {
        std::cerr << "EPHOPN: Can't open binary data file: "
                  << std::strerror(errno) << std::endl;
        std::exit(1);
    }

    /* BEGINNING OF HEADER */
    /* 195 bytes */
    for (i = 0; i < 3; i++) {
        /* char TTL[3][65] */
        if (fread(ttl[i], sizeof(char), 65, fpBinaryFile) != 65) {
            std::cerr << "EPHOPN: Error reading binary data file: "
                      << std::strerror(errno) << std::endl;
            std::exit(1);
        }
        ttl[i][64] = '\0';
    }

    /* 2 bytes */
    if (fread(&tmpShort, sizeof(short), 1, fpBinaryFile) != 1) {
        std::cerr << "EPHOPN: Error reading binary data file: "
                  << std::strerror(errno) << std::endl;
        std::exit(1);
    }

    convert_little_endian(reinterpret_cast<char *>(&tmpShort), sizeof(short));
    int ncon = static_cast<int>(tmpShort);

    /* ncon*6 bytes */
    for (j = 0; j < ncon; ++j) {
        /* char CNAM[NCON][7] */
        if (fread(&cnam[j], sizeof(char), 6, fpBinaryFile) != 6) {
            std::cerr << "EPHOPN: Error reading binary data file: "
                      << std::strerror(errno) << std::endl;
            std::exit(1);
        }
        cnam[j][6] = '\0';
    }

    /* 24 bytes, 8 each */
    for (j = 0; j < 3; j++) {
        /* double SS[3] */
        if (fread(&tmpDouble, sizeof(double), 1, fpBinaryFile) != 1) {
            std::cerr << "EPHOPN: Error reading binary data file: "
                      << std::strerror(errno) << std::endl;
            std::exit(1);
        }
        convert_little_endian(reinterpret_cast<char *>(&tmpDouble),
                              sizeof(double));
        SS[j] = static_cast<double>(tmpDouble);
    }

    /* 16 bytes, 8 each */
    if (fread(&tmpDouble, sizeof(double), 1, fpBinaryFile) != 1) {
        std::cerr << "EPHOPN: Error reading binary data file: "
                  << std::strerror(errno) << std::endl;
        std::exit(1);
    }
    convert_little_endian(reinterpret_cast<char *>(&tmpDouble), sizeof(double));
    au = static_cast<double>(tmpDouble);

    if (fread(&tmpDouble, sizeof(double), 1, fpBinaryFile) != 1) {
        std::cerr << "EPHOPN: Error reading binary data file: "
                  << std::strerror(errno) << std::endl;
        std::exit(1);
    }
    convert_little_endian(reinterpret_cast<char *>(&tmpDouble), sizeof(double));
    emrat = static_cast<double>(tmpDouble);

    /* 72 bytes */
    for (i = 0; i < 3; i++) {
        /* short IPT[3][12] */
        for (j = 0; j < 12; j++) {
            if (fread(&tmpShort, sizeof(short), 1, fpBinaryFile) != 1) {
                std::cerr << "EPHOPN: Error reading binary data file: "
                          << std::strerror(errno) << std::endl;
                std::exit(1);
            }
            convert_little_endian(reinterpret_cast<char *>(&tmpShort),
                                  sizeof(short));
            ipt[i][j] = static_cast<short>(tmpShort);
        }
    }

    /* 2 bytes */
    if (fread(&tmpShort, sizeof(short), 1, fpBinaryFile) != 1) {
        std::cerr << "EPHOPN: Error reading binary data file: "
                  << std::strerror(errno) << std::endl;
        std::exit(1);
    }
    convert_little_endian(reinterpret_cast<char *>(&tmpShort), sizeof(short));
    // short int NUMDE = static_cast<short>(tmpShort); // unused

    /* 6 bytes, 2 each */
    for (i = 0; i < 3; i++) {
        if (fread(&tmpShort, sizeof(short), 1, fpBinaryFile) != 1) {
            std::cerr << "EPHOPN: Error reading binary data file: "
                      << std::strerror(errno) << std::endl;
            std::exit(1);
        }
        convert_little_endian(reinterpret_cast<char *>(&tmpShort),
                              sizeof(short));
        lpt[i] = static_cast<short>(tmpShort);
    }

    /* ncon*8 bytes */
    // double cval[400] = {}; // unused
    for (j = 0; j < ncon; j++) {
        if (fread(&tmpDouble, sizeof(double), 1, fpBinaryFile) != 1) {
            std::cerr << "EPHOPN: Error reading binary data file: "
                      << std::strerror(errno) << std::endl;
            std::exit(1);
        }
        convert_little_endian(reinterpret_cast<char *>(&tmpDouble),
                              sizeof(double));
        // cval[j] = static_cast<double>(tmpDouble);
    }
    /* END OF HEADER */

    /* This block used to be a separate function */
    /* but it didn't work correctly.             */
    if ((curpos = ftell(fpBinaryFile)) == -1L) {
        std::cerr << "EPHOPN: ftell() returned -1L" << std::endl;
        std::exit(1);
    }

    if (fseek(fpBinaryFile, 0L, SEEK_END) != 0) {
        std::cerr << "EPHOPN: fseek() failed." << std::endl;
        std::exit(1);
    }

    LengthOfFile = ftell(fpBinaryFile);

    if (fseek(fpBinaryFile, curpos, SEEK_SET) != 0) {
        std::cerr << "EPHOPN: fseek() failed." << std::endl;
        std::exit(1);
    }
    /* end block */

    LengthOfHeader = 317 + 14 * ncon;

    /* Length of block of coeffs = 8*NCOEFF bytes */
    /* calculate number of coeffs per block */
    ncoeff = 0L;

    for (j = 0; j < 12; j++) {
        if (j == 11)
            ncoeff = ncoeff +
                     static_cast<long>(ipt[1][j]) *
                         static_cast<long>(ipt[2][j]) * 2L;
        else
            ncoeff = ncoeff +
                     static_cast<long>(ipt[1][j]) *
                         static_cast<long>(ipt[2][j]) * 3L;
    }

    ncoeff = ncoeff +
             static_cast<long>(lpt[1]) * static_cast<long>(lpt[2]) * 3L + 2L;
    BlockLength = ncoeff * 8L;
    NumBlocks = static_cast<int>(
        (LengthOfFile - static_cast<long>(LengthOfHeader)) / BlockLength);
}

/*********************************************************************
Name:    interp
Purpose: This subroutine differentiates and interpolates a set of
         Chebyshev coefficients to give position and velocity.
Inputs:  buff - 1st location of array of Chebyshev coefficients.
         t    - t[0] is fractional time in interval covered by
                coefficients at which interpolation is wanted
                (0 <= t[0] <= 1). t[1] is length of whole
                interval in input time units.
         ncf  - Number of coefficients per component.
         ncm  - Number of components per set of coefficients.
         na   - Number of sets of coefficients in full array
                (i.e., # of sub-intervals in full interval).
         fl   - Integer flag:  =1 for positions only.
                               =2 for pos and vel.
Outputs: dumpv - Interpolated quantities requested. Dimension
         expected is pv[ncm][fl].
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::interp(int buff, const double t[2], int ncf, int ncm, int na,
                     int fl, double dumpv[3][2])
{
    int i, j, l, n, m;
    int bcoef, np, nv;
    double pc[18], vc[18], cbody[1200];
    double cbuf[15][3][8] = {};
    double twot, dna, dt1, temp, ll, tc, vfac;

    np = 2;
    nv = 3;
    twot = 0.0;
    pc[0] = 1.0;
    pc[1] = 0.0;
    vc[1] = 1.0;

    /*
      Entry point. Get correct sub-interval number for this set
      of coefficients and then get normalized chebyshev time
      within that subinterval.
    */

    dna = static_cast<double>(na);
    dt1 = floor(t[0]);
    temp = dna * t[0];
    ll = floor((temp - dt1) + 1.0);

    /* 'tc' is the normalized chebyshev time (-1 <= tc <= 1) */
    tc = 2.0 * (fmod(temp, 1.0) + dt1) - 1.0;

    /*
      Check to see whether chebyshev time has changed,
      and compute new polynomial values if it has.
      (The element pc[1] is the value of 'tc' and hence
      contains the value of 'tc' on the previous call.)
    */

    // if (tc != pc[1]) {
    //     np = 2;
    //     nv = 3;
    //     pc[1] = tc;
    //     twot = tc + tc;
    // }
    pc[1] = tc;
    twot = tc + tc;

    /*
      Be sure that at least 'ncf' polynomials have been evaluated
      and are stored in the array pc[].
    */

    if (np < ncf) {
        for (i = np; i < ncf; i++) {
            pc[i] = twot * pc[i - 1] - pc[i - 2];
        }
        np = ncf;
    }

    /* interpolate to get position for each component */

    /* number of coefficients for body */
    bcoef = ncf * na * ncm;

    /* stored body's coefficients in an array */
    n = buff;
    for (m = 0; m < bcoef; m++) {
        cbody[m] = db[n];
        n++;
    }

    /* fill the cbuf[][][] array */
    n = 0;
    /* loop for each sub-interval */
    for (l = 0; l < na; l++) {
        /* loop for each component */
        for (i = 0; i < ncm; i++) {
            /* loop for each set of coeffs */
            for (j = 0; j < ncf; j++) {
                cbuf[j][i][l] = cbody[n];
                n++;
            }
        }
    }

    for (i = 0; i < ncm; i++) {
        dumpv[i][0] = 0.0;
        for (j = ncf - 1; j >= 0; j--) {
            dumpv[i][0] =
                dumpv[i][0] + pc[j] * cbuf[j][i][(static_cast<int>(ll)) - 1];
        }
    }
    if (fl <= 1)
        return;

    /*
      If velocity interpolation is wanted, be sure enough
      derivative polynomials have been generated and stored.
    */

    vfac = (dna + dna) / t[1];
    vc[2] = twot + twot;
    if (nv < ncf) {
        for (i = nv; i < ncf; i++) {
            vc[i] = twot * vc[i - 1] + pc[i - 1] + pc[i - 1] - vc[i - 2];
        }
        nv = ncf;
    }

    /* interpolate to get velocity for each component */

    for (i = 0; i < ncm; i++) {
        dumpv[i][1] = 0.0;
        for (j = ncf - 1; j >= 1; j--) {
            dumpv[i][1] =
                dumpv[i][1] + vc[j] * cbuf[j][i][(static_cast<int>(ll)) - 1];
        }
        dumpv[i][1] = dumpv[i][1] * vfac;
    }
}

/*********************************************************************
Name:    pleph
Purpose: Function that reads the JPL planetary ephemeris and gives the
         position and velocity of the point 'targ' with respect to 'cent'.
Inputs:  jd   - JED at which interpolation is wanted.
         targ - Number of target point.
         cent - Number of center point.
         The numbering convention for 'targ' and 'cent' is:
         1 = Mercury           8 = Neptune
         2 = Venus             9 = Pluto
         3 = Earth            10 = Moon
         4 = Mars             11 = Sun
         5 = Jupiter          12 = SSB
         6 = Saturn           13 = EMB
         7 = Uranus           14 = Nutations  (if present)
         15 = Librations (if present)
         If nutations are wanted, set targ = 14. For librations,
         set targ = 15. 'cent' will be ignored on either call.
Outputs: rrd[]  - 6 element array containing the state vector of 'targ'
                  relative to 'cent'. The units are AU and AU/DAY. For
                  librations the units are RAD and RAD/DAY. For
                  nutations the first 4 elements of RRD[] are set to
                  nutations and rates, in RAD and RAD/DAY.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::pleph(const double jd[2], int targ, int cent, int ipv,
                    double *rrd)
{
    int i;
    int ncmp, lme;

    const double embf[2] = {-1.0, 1.0};
    const double ve[2] = {1.0 / (1.0 + emrat), emrat / (1.0 + emrat)};

    const int LLst[13] = {0, 1, 9, 3, 4, 5, 6, 7, 8, 9, 10, 10, 2};
    int L[2], tc[2];

#ifdef SGNL_USE_CALCEPH
    if (calceph_eph != NULL) {
        // Bodies 1..13 are requested by NAIF id, because a SPICE kernel is
        // segmented that way (Earth is 399 relative to the Earth-Moon
        // barycentre 3, not "3" relative to "12"). The table converts the
        // numbering this function documents above; as in the DE files
        // themselves, 1..9 are barycentres and 3 is the Earth itself.
        static const int to_naif[14] = {
            0,                                  /* unused, ids start at 1 */
            NAIFID_MERCURY_BARYCENTER,          /*  1 */
            NAIFID_VENUS_BARYCENTER,            /*  2 */
            NAIFID_EARTH,                       /*  3 */
            NAIFID_MARS_BARYCENTER,             /*  4 */
            NAIFID_JUPITER_BARYCENTER,          /*  5 */
            6, 7, 8, 9,                         /*  6..9 outer barycentres */
            NAIFID_MOON,                        /* 10 */
            NAIFID_SUN,                         /* 11 */
            NAIFID_SOLAR_SYSTEM_BARYCENTER,     /* 12 */
            NAIFID_EARTH_MOON_BARYCENTER        /* 13 */
        };

        // Despite what the comment above this function says - it is inherited
        // from stock FECsoft - state() returns km and km/sec, and callers in
        // src/Ephemeris.cpp use the result directly against GM values in
        // km^3/s^2. Ask CALCEPH for the same units. Kilometres also avoid
        // needing an AU constant, which SPICE kernels do not carry.
        double PV[6] = {};
        int unit;
        int ok;

        if (targ >= 14) {
            // Nutations and librations have no NAIF body id; ask for them the
            // old way, in radians. SPICE kernels do not carry them.
            unit = CALCEPH_UNIT_RAD + CALCEPH_UNIT_SEC;
            ok = calceph_compute_unit(calceph_eph, jd[0], jd[1], targ, cent,
                                      unit, PV);
        } else {
            unit = CALCEPH_UNIT_KM + CALCEPH_UNIT_SEC + CALCEPH_USE_NAIFID;
            ok = calceph_compute_unit(calceph_eph, jd[0], jd[1],
                                      to_naif[targ], to_naif[cent], unit, PV);
        }

        if (ok == 0) {
            std::cerr << "pleph: CALCEPH could not compute body " << targ
                      << " relative to " << cent << " at JD "
                      << (jd[0] + jd[1])
                      << " (date outside the ephemeris, or body absent)."
                      << std::endl;
            std::exit(1);
        }

        for (i = 0; i < 6; i++) {
            rrd[i] = PV[i];
        }
        return;
    }
#endif

    /* Initialize jed[] for state() and set up component count */
    const double jed[2] = {jd[0], jd[1]};
    const double jdtot = jed[0] + jed[1];

    // A diverging trajectory arrives here with a non-finite epoch, and the
    // range test below lets NaN through: every comparison against it is false.
    // The crash then lands deep inside interp() with nothing to say what went
    // wrong. Name the real cause instead.
    if (!std::isfinite(jdtot)) {
        std::cerr << "pleph: epoch is not a finite number. The trajectory has\n"
                     "diverged before reaching the ephemeris - check the force\n"
                     "model, in particular whether the selected radiation\n"
                     "pressure model has the spacecraft properties it needs\n"
                     "(rp_model = 3, box-wing, needs per-face areas and optical\n"
                     "properties that the default spacecraft does not set)."
                  << std::endl;
        std::exit(1);
    }

    if (jdtot < SS[0] || jdtot > SS[1]) {
        std::cerr << "pleph: requested date not covered by ephemeris file."
                  << std::endl;
        std::exit(1);
    }

    /*
      pv[] is a zero-offset 6x13 matrix. The column number (0-12) specifies
      a body number. The row number (0-5) specifies a component of the
      body's state vector x,y,z,xdot,ydot,zdot in that order.
    */

    /* necessary for zero-offset arrays */
    targ = targ - 1;
    cent = cent - 1;
    // From here on, 'targ' and 'cent' are one less than their calling values.

    ncmp = 3 * ipv; /* total number of components */

    /* check for nutation call */
    if (targ == 13) {
        if (ipt[1][11] > 0) {
            LList[10] = ipv;
            state(jed, rrd);
            LList[10] = 0;
            return;
        } else {
            std::cerr << "pleph: no nutations on the ephemeris file."
                      << std::endl;
            std::exit(1);
        }
    }

    /* check for librations */
    if (targ == 14) {
        if (lpt[1] > 0) {
            LList[11] = ipv;
            state(jed, rrd);
            LList[11] = 0;
            for (i = 0; i < ncmp; i++) {
                rrd[i] = pv[i][10];
            }
            return;
        } else {
            std::cerr << "pleph: no librations on the ephemeris file."
                      << std::endl;
            std::exit(1);
        }
    }

    /* check for targ = cent */
    if (targ == cent) {
        for (i = 0; i < ncmp; i++) {
            rrd[i] = 0.0;
        }
        return;
    }

    /* force barycentric output by state() */
    bool bsav = bary;
    bary = true;

    /* set up proper entries in LList[] array for state() call */
    tc[0] = targ;
    tc[1] = cent;
    lme = 0;

    for (i = 0; i < 2; i++) {
        L[i] = LLst[tc[i]];
        if (L[i] < 10)
            LList[L[i]] = ipv;
        if (tc[i] == 2) {
            lme = 2;
            fac = -ve[0];
        } else if (tc[i] == 9) {
            lme = 9;
            fac = ve[1];
        } else if (tc[i] == 12) {
            nemb = i;
        }
    }

    if ((LList[9] == ipv) && (L[0] != L[1]))
        LList[2] = ipv - LList[2];

    /* make call to state() */
    state(jed, rrd);

    /* case: Earth-to-Moon */
    if ((targ == 9) && (cent == 2)) {
        for (i = 0; i < ncmp; i++) {
            rrd[i] = pv[i][9];
        }

        /* case: Moon-to-Earth */
    } else if ((targ == 2) && (cent == 9)) {
        for (i = 0; i < ncmp; i++) {
            rrd[i] = -pv[i][9];
        }

        /* case: EMB-to-Moon or -Earth */
    } else if ((targ == 12 || cent == 12) && LList[9] == ipv) {
        for (i = 0; i < ncmp; i++) {
            rrd[i] = pv[i][9] * fac * embf[nemb];
        }

        /* otherwise, get Earth or Moon vector and then get output vector */
    } else {
        for (i = 0; i < ncmp; i++) {
            pv[i][10] = pvsun[i];
            pv[i][12] = pv[i][2];
            if (lme > 0)
                pv[i][lme] = pv[i][2] + fac * pv[i][9];
            rrd[i] = pv[i][targ] - pv[i][cent];
        }
    }

    /* clear state() body array and restore barycenter flag */
    LList[2] = 0;
    LList[L[0]] = 0;
    LList[L[1]] = 0;
    bary = bsav;
}

/*********************************************************************
Name:    state
Purpose: This subroutine reads and interpolates the JPL planetary
         ephemeris file.
Inputs:  jed[]   - 2 element array containing the JED epoch at which
                   interpolation is wanted. Any combination of
                   jed[0]+jed[1] which falls within the time span on
                   the file is a permissible epoch. For ease in
                   programming, the user may put the entire epoch in
                   jed[0] and set jed[1] = 0. For maximum accuracy,
                   set jed[0] = most recent midnight at or before
                   interpolation epoch and set jed[1] = fractional
                   part of a day elapsed between jed[0] and epoch.
                   As an alternative, it may prove convenient to set
                   jed[0] = some fixed epoch, such as start of
                   integration and jed[1] = elapsed interval between
                   interval between then and epoch.
         LList[] - 12 element array specifying what interpolation
                   is wanted for each of the bodies on the file.
                   LList[i] =0, no interpolation for body i,
                            =1, position only,
                            =2, position and velocity.
                   The designation of the astronomical bodies by i is:
                   i = 0: Mercury,
                     = 1: Venus,
                     = 2: Emb,
                     = 3: Mars,
                     = 4: Jupiter,
                     = 5: Saturn,
                     = 6: Uranus,
                     = 7: Neptune,
                     = 8: Pluto,
                     = 9: Moon (geocentric),
                     =10: Nutations in longitude and obliquity (if present),
                     =11: Lunar librations (if present).
Outputs: pv[]  - 6 x 13 array that will contain requested interpolated
                 quantities. the body specified by LList[i] will have its
                 state in the array starting at pv[0][i]. (On any given
                 call, only those words in pv[] which are affected by the
                 first 10 LList[] entries (and by LList[12] if librations
                 are on the file) are set. The rest of the pv[] array
                 is untouched.) the order of components starting in
                 pv[0][i] is: x,y,z,dx,dy,dz.

                 All output vectors are referenced to the earth mean
                 equator and equinox of epoch. The Moon state is always
                 geocentric; the other nine states are either heliocentric
                 or solar-system barycentric, depending on the setting of
                 common flags (see below).

                 Lunar librations, if in the data file, are put into
                 pv[k][11] if LList[11] is 1 or 2.

         nut[] - 4-word array that will contain nutations and rates,
                 depending on the setting of LList[10]. The order of
                 quantities in nut[] is:
                 dpsi     (nutation in longitude),
                 depsilon (nutation in obliquity),
                 dpsi dot,
                 depsilon dot.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
/***********************************************************************
*      Units are km and km/sec. Angle unit is always radians.          *
*                                                                      *
* Other important variables:                                           *
*                                                                      *
* bary Logical flag defining output center.                            *
*      only the 9 planets are affected.                                *
*                    bary = TRUE  = center is SSB,                     *
*                         = FALSE = center is Sun.                     *
*           default value = FALSE.                                     *
*                                                                      *
* pvsun[] 6-word array containing the barycentric position and         *
*         velocity of the Sun.                                         *
***********************************************************************/
void Fecsoft::state(const double jed[2], double *nut)
{
    /* Temporary data used when reading in binary file */
    double tmpDouble = 0.0;

    double jd[4] = {};
    double temp[6] = {};
    double dumpv[3][2] = {};

    double t[2] = {};
    t[1] = SS[2] * 86400.0;

    int buff, ncf, na;

    int i, j, k, m;
    long int nr;

    /* main entry point -- check epoch and read right record */
    double s, ipart, fpart;
    s = jed[0] - 0.5;
    fpart = std::modf(s, &ipart);
    jd[0] = ipart;
    jd[1] = fpart;
    fpart = std::modf(jed[1], &ipart);
    jd[2] = ipart;
    jd[3] = fpart;
    jd[0] = jd[0] + jd[2] + 0.5;
    jd[1] = jd[1] + jd[3];
    fpart = std::modf(jd[1], &ipart);
    jd[2] = ipart;
    jd[3] = fpart;
    jd[0] = jd[0] + jd[2];

    if (jd[3] < 0.0) {
        jd[0] -= 1.0;
        jd[3] += 1.0;
    }

    /* error return of epoch out of range */
    if ((jd[0] < SS[0]) || (jd[0] + jd[3]) > SS[1]) {
        std::cerr << "state: epoch out of range." << std::endl;
        std::exit(1);
    }

    /* 'nr' is the byte index of the first coefficient */
    nr = static_cast<long>(floor((jd[0] - SS[0]) / SS[2]));
    nr = LengthOfHeader + nr * BlockLength;
    /* use previous block if necessary */
    if (jd[0] >= SS[1])
        nr = nr - BlockLength;
    if (nr < 1L) {
        std::cerr << "state: block not present." << std::endl;
        std::exit(1);
    }

    /* calculate relative time in interval (0 <= t[0] <= 1) */
    t[0] = ((jd[0] -
             ((static_cast<double>(nr) - static_cast<double>(LengthOfHeader)) /
                  static_cast<double>(BlockLength) * SS[2] +
              SS[0])) +
            jd[3]) /
           SS[2];
    if (t[0] < 0.0)
        t[0] = t[0] + 1.0;

    /* read correct record if not in core */
    if (nr != nrl) {
        nrl = nr;
        if (fseek(fpBinaryFile, static_cast<long>(nr), 0) != 0) {
            std::cerr << "state: fseek() failed." << std::endl;
            std::exit(1);
        }
        k = 0;
        do {
            if (k < ncoeff) {
                if (fread(&tmpDouble, sizeof(double), 1, fpBinaryFile) == 0) {
                    std::cerr << "state: fread() failed." << std::endl;
                    std::exit(1);
                }
                convert_little_endian(reinterpret_cast<char *>(&tmpDouble),
                                      sizeof(double));
                db[k] = static_cast<double>(tmpDouble);
            }
            k++;
        } while (!feof(fpBinaryFile) && k <= ncoeff);
    }

    /* interpolate SSBARY Sun */
    buff = ipt[0][10] - 1; /* location of first coeff */
    ncf = ipt[1][10];      /* number of coeffs per component */
    na = ipt[2][10];       /* number of sets of coeffs per 32day interval */

    interp(buff, t, ncf, 3, na, 2, dumpv);

    k = 0;
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 3; i++) {
            pvsun[k] = dumpv[i][j];
            k++;
        }
    }

    /* check and interpolate whichever bodies are requested */
    for (i = 0; i < 10; i++) {
        if (LList[i] <= 0)
            continue;
        if (ipt[1][i] <= 0) {
            const char *err_str = "th body requested - not on file.\n";
            errprt(i, err_str);
        }
        buff = ipt[0][i] - 1; /* location of first coeff */
        ncf = ipt[1][i];      /* number of coeffs per component */
        na = ipt[2][i];       /* number of sets of coeffs per 32day interval */

        interp(buff, t, ncf, 3, na, LList[i], dumpv);

        /* need to re-map dumpv[1..3][1..2] --> temp[1..6] */
        k = 0;
        for (j = 0; j < 2; j++) {
            for (m = 0; m < 3; m++) {
                temp[k] = dumpv[m][j];
                k++;
            }
        }

        for (j = 0; j < (LList[i] * 3); j++) {
            if ((i <= 8) && (!bary)) {
                pv[j][i] = temp[j] - pvsun[j];
            } else {
                pv[j][i] = temp[j];
            }
        }
    }

    /* do nutations if requested and if on file */
    if ((LList[10] > 0) && (ipt[1][11] > 0)) {
        buff = ipt[0][11] - 1; /* location of first coeff */
        ncf = ipt[1][11];      /* number of coeffs per component */
        na = ipt[2][11];       /* number of sets of coeffs per 32day interval */

        interp(buff, t, ncf, 2, na, LList[10], dumpv);

        /* need to re-map dumpv(1:3,1:2) --> temp(1:6) */
        k = 0;
        for (j = 0; j < 2; j++) {
            for (m = 0; m < 2; m++) {
                nut[k] = dumpv[m][j];
                k++;
            }
        }
        nut[4] = 0.0;
        nut[5] = 0.0;
    }

    /* get librations if requested and if on file */
    if ((lpt[1] > 0) && (LList[11] > 0)) {
        buff = lpt[0] - 1; /* location of first coeff */
        ncf = lpt[1];      /* number of coeffs per component */
        na = lpt[2];       /* number of sets of coeffs per 32day interval */

        interp(buff, t, ncf, 3, na, LList[11], dumpv);

        pv[0][10] = dumpv[0][0];
        pv[1][10] = dumpv[1][0];
        pv[2][10] = dumpv[2][0];
        pv[3][10] = dumpv[0][1];
        pv[4][10] = dumpv[1][1];
        pv[5][10] = dumpv[2][1];
    }
}

/*********************************************************************
Name:    convert_little_endian
Purpose: Reverses the byte ordering of the given block of memory IF this
         machine is BIG ENDIAN. Used to convert the little-endian binary
         data read in from a file into the native endian format for this
         machine.
Inputs:  ptr - Pointer to block of memory to reverse.
         len - Length of block of memory to reverse.
Outputs: ptr - Byte-reversed block of memory.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::convert_little_endian(char *ptr, int len)
{
    if (BIG_ENDIAN_TEST) {
        reverse_bytes(ptr, len);
    }
}

/*********************************************************************
Name:    reverse_bytes
Purpose: Reverses the byte ordering of the given block of memory.
Inputs:  ptr - Pointer to block of memory to reverse.
         len - Length of block of memory to reverse.
Outputs: ptr - Byte-reversed block of memory.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::reverse_bytes(char *ptr, int len)
{

    int i;
    char *tmp;

    if ((tmp = reinterpret_cast<char *>(malloc(static_cast<size_t>(len)))) ==
        NULL) {
        std::cerr << "Memory allocation error (reverse_bytes: " << len << ")"
                  << std::endl;
        std::exit(1);
    }

    for (i = 0; i < len; i++) {
        tmp[i] = ptr[(len - 1) - i];
    }

    memcpy(ptr, tmp, static_cast<size_t>(len));
    free(tmp);
}

/*********************************************************************
Name:    errprt
Purpose: Function to print error messages.
Inputs:  group   - Error code number.
         message - Error message.
Outputs: None.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
[[noreturn]] void Fecsoft::errprt(int group, const char *message)
{
    std::cerr << std::endl << "ERROR #" << group << " " << message << std::endl;
    std::exit(1);
}

/*********************************************************************
Name:    SplitStateVector
Purpose: Subprogram to split a state vector into its position
         and velocity components.
Inputs:  pv[] - Zero-offset 6-d state vector.
Outputs: p[] - Zero-offset 3-d position vector.
         v[] - Zero-offset 3-d velocity vector.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::SplitStateVector(const double r[6], double p[3], double v[3])
{
    for (int i = 0; i < 3; ++i) {
        p[i] = r[i];
        v[i] = r[i + 3];
    }
}

/*********************************************************************
Name:    Uvector
Purpose: Unit vector subroutine.
Inputs:  a[] - Zero-offset column vector (3 rows by 1 column).
Outputs: unita[] - Zero-offset unit vector (3 rows by 1 column).
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::Uvector(const double a[3], double unita[3])
{
    double maga = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);

    if (maga > 0) {
        unita[0] = a[0] / maga;
        unita[1] = a[1] / maga;
        unita[2] = a[2] / maga;
    } else {
        unita[0] = 0.0;
        unita[1] = 0.0;
        unita[2] = 0.0;
    }
}

/*********************************************************************
Name:    Vdot
Purpose: Vector dot product function.
Inputs:  n   - Number of rows.
         a[] - Zero-offset column vector with N rows.
         b[] - Zero-offset column vector with N rows.
Outputs: adotb - Dot product of a and b.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
double Fecsoft::Vdot(const double a[3], const double b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/*********************************************************************
Name:    Vecmag
Purpose: Vector magnitude function.
Inputs:  a[] - Zero-offset column vector (3 rows by 1 column).
Outputs: None.
Returns: Magnitude of vector.
Status:  Finished.
Errors:  None known.
*********************************************************************/
double Fecsoft::Vecmag(const double a[3])
{
    return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

/*********************************************************************
Name:    Aberrate
Purpose: Function to correct an input vector for aberration.
Inputs:  p1    - Zero-offset geocentric state vector of object.
         EBdot - Zero-offset barycentric velocity of Earth.
Outputs: p2 - Zero-offset geocentric state vector of object corrected
              for aberration.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::Aberrate(const double p1[6], const double EBdot[3], double p2[6])
{
    double v[3], up[3], p[3], pdot[3], magP, magV2, p1dotv, beta;
    int i;

    /* Extract the pos. portion of the state vector */
    SplitStateVector(p1, p, pdot);
    magP = Vecmag(p);

    /* Need to make Ppos() a unit vector */
    Uvector(p, up);

    for (i = 0; i < 3; i++) {
        v[i] = EBdot[i] / CAUD;
    }

    p1dotv = Vdot(up, v);
    magV2 = Vdot(v, v);

    beta = 1.0 / std::sqrt(1.0 - magV2);

    for (i = 0; i < 3; i++) {
        p2[i] =
            ((up[i] / beta) + (1.0 + p1dotv / (1.0 + (1.0 / beta))) * v[i]) /
            (1.0 + p1dotv);
        /* Make p2[] a non-unit vector */
        p2[i] = magP * p2[i];
        p2[i + 3] = p1[i + 3];
    }

    return;
}

/*********************************************************************
Name:    LightTime
Purpose: Subprogram to compute the vectors body_geo() and body_hel()
         from a barycentric ephemeris of the object, Earth, and the Sun.
Inputs:  JED       - Desired time of reduction.
         Body      - Body number, 99 for stellar reduction.
         sun_ssb   - Barycentric state vector of Sun at JED.
         earth_ssb - Barycentric state vector of Earth at JED.
         magE      - Distance from sun to the Earth.
Outputs: body_geo - Geometric geocentric state vector of object.
         body_hel - Heliocentric state vector of object.
         Ltime    - Light time in days to the object.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::LightTime(const double jed[2], int body, double sun_ssb[],
                        double earth_ssb[], double magE, double body_geo[],
                        double body_hel[], double &Ltime)
{
    int i;
    double RQB[6], RSB[6], Q[3] = {}, P[3];
    double magP, trialtau, tau, xtime[2], RelTerm, TrueHelDist, TrueGeoDist;
    double magQ = 1.0;

    RelTerm = 0.0;
    TrueGeoDist = 0.0;

    xtime[0] = jed[0];
    xtime[1] = jed[1];

    if (body == 11) {
        // Sun's geocentric state vector
        P[0] = sun_ssb[0] - earth_ssb[0];
        P[1] = sun_ssb[1] - earth_ssb[1];
        P[2] = sun_ssb[2] - earth_ssb[2];

        TrueHelDist = 0.0;
    } else {
        /* Body's barycentric state vector */
        pleph(xtime, body, 12, 2, RQB);

        // Body's heliocentric state vector
        Q[0] = RQB[0] - sun_ssb[0];
        Q[1] = RQB[1] - sun_ssb[1];
        Q[2] = RQB[2] - sun_ssb[2];
        // Body's geocentric state vector
        P[0] = RQB[0] - earth_ssb[0];
        P[1] = RQB[1] - earth_ssb[1];
        P[2] = RQB[2] - earth_ssb[2];

        TrueHelDist = Vecmag(Q);
    }

    TrueGeoDist = Vecmag(P);

    tau = Ltime;
    do {
        trialtau = tau;
        xtime[1] = jed[1] - tau;
        /* Body's barycentric state vector */
        pleph(xtime, body, 12, 2, RQB);

        if (body == 11) {
            // Sun's geocentric state vector
            P[0] = RQB[0] - earth_ssb[0];
            P[1] = RQB[1] - earth_ssb[1];
            P[2] = RQB[2] - earth_ssb[2];
            magP = Vecmag(P);

            /* Calculate improved value of the light time */
            tau = magP / CAUD;
        } else {
            // Body's barycentric state vector
            pleph(xtime, 11, 12, 2, RSB);

            for (i = 0; i < 3; i++) {
            }
            // Body's heliocentric state vector
            Q[0] = RQB[0] - RSB[0];
            Q[1] = RQB[1] - RSB[1];
            Q[2] = RQB[2] - RSB[2];
            magQ = Vecmag(Q);
            // Body's geocentric state vector
            P[0] = RQB[0] - earth_ssb[0];
            P[1] = RQB[1] - earth_ssb[1];
            P[2] = RQB[2] - earth_ssb[2];
            magP = Vecmag(P);

            /* Calculate improved value of the light time */
            RelTerm =
                MUC * log((magE + magP + magQ) / fabs(magE - magP + magQ));
            tau = (magP + RelTerm) / CAUD;
        }
    } while (fabs(trialtau - tau) > 1e-14);

    Ltime = tau;

    TrueGeoDist /= magP;

    body_geo[0] = P[0] * TrueGeoDist;
    body_geo[1] = P[1] * TrueGeoDist;
    body_geo[2] = P[2] * TrueGeoDist;
    body_geo[3] = RQB[3] - earth_ssb[3];
    body_geo[4] = RQB[4] - earth_ssb[4];
    body_geo[5] = RQB[5] - earth_ssb[5];

    if (body == 11) {
        body_hel[0] = 0.0;
        body_hel[1] = 0.0;
        body_hel[2] = 0.0;
        body_hel[3] = 0.0;
        body_hel[4] = 0.0;
        body_hel[5] = 0.0;
    } else {
        TrueHelDist /= magQ;
        body_hel[0] = Q[0] * TrueHelDist;
        body_hel[1] = Q[1] * TrueHelDist;
        body_hel[2] = Q[2] * TrueHelDist;
        body_hel[3] = RQB[3] - RSB[3];
        body_hel[4] = RQB[4] - RSB[4];
        body_hel[5] = RQB[5] - RSB[5];
    }
}

/*********************************************************************
Name:    RayBend
Purpose: Function to correct an input vector for relativistic light
         deflection due to the Sun's gravity field.
Inputs:  ue[]        - Earth-sun unit vector.
         magE        - Distance from sun to the Earth.
         body_geo[]  - Zero-offset geocentric state vector of object.
         body_hel[]  - Zero-offset heliocentric state vector of object.
Outputs: p1[] - Zero-offset geocentric state vector of object
                corrected for light deflection.
Returns: Nothing.
Status:  Finished.
Errors:  None known.
*********************************************************************/
void Fecsoft::RayBend(const double ue[3], double magE, const double body_geo[6],
                      const double body_hel[6], double p1[6])
{
    double up[3], uq[3], P[3], Pdot[3], Q[3], Qdot[3];
    double magP, pdotq, edotp, qdote;
    int i;

    /* extract the pos. portions of the state vectors */
    SplitStateVector(body_geo, P, Pdot);
    SplitStateVector(body_hel, Q, Qdot);

    /* form unit vectors */
    Uvector(P, up);
    Uvector(Q, uq);

    /* form dot products and other quantities */
    // magE = Vecmag(E);
    magP = Vecmag(P);
    pdotq = Vdot(up, uq);
    edotp = Vdot(ue, up);
    qdote = Vdot(uq, ue);

    for (i = 0; i < 3; i++) {
        p1[i] = up[i] +
                (MUC / magE) * (pdotq * ue[i] - edotp * uq[i]) / (1.0 + qdote);
        /* make p1[] a non-unit vector */
        p1[i] = magP * p1[i];
        p1[i + 3] = body_geo[i + 3];
    }
}

/* End Of File - astrolib.c *****************************************/
