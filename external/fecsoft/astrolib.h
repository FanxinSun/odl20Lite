#ifndef ASTROLIB_H
#define ASTROLIB_H

/* Gaussian gravitational constant */
#define GAUSSK (0.01720209895)

/* AU to km and km to AU */
#define AU2KM (149597870.66)

/* speed of light in km/s and AU/day */
#define CKMS (299792.458)
#define CAUD (CKMS * 86400.0 / AU2KM)

/* Earth's gravitational constant */
#define MUC (2.0 * GAUSSK * GAUSSK / (CAUD * CAUD))

#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

// Built with CALCEPH (external/calceph) when that library is present, which
// lets the ephemeris also be a stock JPL binary, an INPOP file or a SPICE
// kernel rather than only FECsoft's own container. The Makefile defines this
// automatically if external/calceph/lib/libcalceph.a has been built.
#ifdef SGNL_USE_CALCEPH
#include "calceph.h"
#endif

// #include <arpa/inet.h> // For htonl function
// #include <winsock2.h>  // Equivalent header on Windows

//
// inline bool big_endian_test()
// {
//    union {
//         uint32_t i;
//         char c[4];
//     } bint = {0x01020304};

//     return bint.c[0] == 1;
// }

inline bool big_endian_test()
{
    union Foo {
        uint32_t i;
        char c[4];
    };
    
    constexpr Foo bint = {0x01020304};
    bool check = (bint.c[0] == 1);

    return check; 
}

// #define BIG_ENDIAN_TEST (htonl(1) == 1)
#define BIG_ENDIAN_TEST (big_endian_test())


/*********************************************************************
Project:  fecsoftc
Filename: astrolib.h
Author:   Joe Heafner
Purpose:  Library routines header file.
Thanks to Charles Gamble for extensive modifications.

Source: http://www.willbell.com/math/fundephcomp.htm
Fundamental Ephemeris Computations For Use With JPL Data, Paul J. Heafner
*********************************************************************/

class Fecsoft
{
  private:
    FILE *fpBinaryFile = NULL;

    /* Ephemeris header information */
    double SS[3] = {};
    double au = 0.0;
    double emrat = 0.0;
    short int ipt[3][12] = {};
    short int lpt[3] = {};

    /* Other data file specs not in header */
    long LengthOfFile = 0;
    long ncoeff = 0;
    long BlockLength = 0;
    int LengthOfHeader = 0;
    int NumBlocks = 0;
    bool bary = false;
    double db[1100] = {};
    double pvsun[6] = {};

    // pleph variables
    double fac = 0.0;
    int nemb = 1;

    // pleph & state variables
    int LList[12] = {};
    double pv[6][13] = {};

    // state variables
    long int nrl = 0;

#ifdef SGNL_USE_CALCEPH
    // Non-NULL when the ephemeris was opened through CALCEPH instead of the
    // FECsoft reader below; pleph() dispatches on it.
    t_calcephbin *calceph_eph = NULL;
#endif

  public:
    Fecsoft()
    {
        // SGNL_EPHEMERIS points at a different ephemeris without editing this
        // file. res/1980_2020 is FECsoft's own container; with CALCEPH built
        // in, the override may also be a JPL binary, an INPOP file or a SPICE
        // kernel (.bsp).
        const char *from_env = std::getenv("SGNL_EPHEMERIS");
        ephopn(from_env != NULL ? std::string(from_env)
                                : std::string("../res/1980_2020"));
    }

    // Explicitly prevent copy and move operations
    Fecsoft(const Fecsoft &copy_from) = delete;
    Fecsoft &operator=(const Fecsoft &copy_from) = delete;
    Fecsoft(Fecsoft &&) = delete;
    Fecsoft &operator=(Fecsoft &&) = delete;

    // Define our own destructor to close open file handle
    ~Fecsoft();

    /* main ephemeris functions */
  private:
    void ephopn(std::string FileName);

    //! True if the file is a FECsoft container, i.e. the packed header this
    //! reader understands rather than the layout JPL publishes. Used to decide
    //! whether to read the file directly or hand it to CALCEPH.
    static bool looks_like_fecsoft(const std::string &FileName);

  public:
    void interp(int buff, const double t[2], int ncf, int ncm, int na, int fl,
                double dumpv[3][2]);

    void pleph(const double jd[2], int targ, int cent, int ipv, double *rrd);
    void state(const double jed[2], double *nut);

    /* binary data functions */
    static void convert_little_endian(char *ptr, int len);
    static void reverse_bytes(char *ptr, int len);

    /* other misc functions */
    [[noreturn]] void errprt(int group, const char *message);

    static void SplitStateVector(const double r[6], double p[3], double v[3]);
    static void Uvector(const double a[3], double unita[3]);
    static double Vdot(const double a[3], const double b[3]);
    static double Vecmag(const double a[3]);

    /* currently unused functions */
    void Aberrate(const double p1[6], const double EBdot[3], double p2[6]);
    void LightTime(const double jed[2], int body, double sun_ssb[],
                   double earth_ssb[], double magE, double body_geo[],
                   double body_hel[], double &Ltime);
    void RayBend(const double ue[3], double magE, const double body_geo[6],
                 const double body_hel[6], double p1[6]);
};

#endif /* _ASTROLIB_ */
