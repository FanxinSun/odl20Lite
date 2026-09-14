# UCL Space Geodesy and Navigation Laboratory Orbit Prediction Software
# Makefile for UCL SGNL OPS 2017a
#
# Santosh Bhattarai December 2014
#
# This makefile assumes src/main_*.cpp is a standalone utility to be named *
#
# All other files matching src/*.cpp are compiled to object files and added to
# a library, with which the utilities are linked.

LIBRARY	= libsgnlOPS.a

SRCDIR	= src
OBJDIR	= obj
BINDIR	= bin
LIBDIR	= lib
OUTDIR	= output

CXX 	= g++
CACHE   = 
LIBSGNL	= ${OBJDIR}/${LIBRARY}
# LIBEXT  = -lpng
LIBEXT  = $(shell pkg-config --libs libpng)

# -MMD -MP make the compiler emit a .d file listing the headers each object
# depends on, which is included below. Without it, editing a header recompiles
# nothing: objects keep the old struct layouts while the one changed .cpp gets
# the new one, and the mismatched build corrupts memory at run time rather
# than failing to compile.
FLAGS   = -std=gnu++14 -O3 -MMD -MP
INTELFL = -std=gnu++11 -O3
DEBUGFL = -g

# Optional CALCEPH support. When external/calceph has been built, the
# ephemeris reader can also take a stock JPL binary, an INPOP file or a SPICE
# kernel instead of only FECsoft's own container. Build it with:
#
#   cmake -S <calceph-source> -B build -DCMAKE_INSTALL_PREFIX=$(CURDIR)/external/calceph \
#         -DENABLE_FORTRAN=OFF -DBUILD_SHARED_LIBS=OFF
#   cmake --build build -j && cmake --install build
#
# Without it everything still builds; res/1980_2020 is read as before.
CALCEPHDIR = external/calceph
CALCEPHLIB = $(wildcard ${CALCEPHDIR}/lib/libcalceph.a)

ifneq (${CALCEPHLIB},)
FLAGS  += -DSGNL_USE_CALCEPH -I${CALCEPHDIR}/include
LIBEXT += ${CALCEPHLIB}
endif

# The NRLMSISE-00 reference implementation is C and is compiled straight into
# the library; drag = 3 in the config selects it.
CC      = gcc
CFLAGS  = -O3
NRLDIR  = external/nrlmsise00
NRLSRC  = $(sort $(wildcard ${NRLDIR}/*.c))
NRLOBJ  = $(patsubst ${NRLDIR}/%.c, ${OBJDIR}/%.o, ${NRLSRC})

ALLSRC	= $(sort $(wildcard ${SRCDIR}/*.cpp))

UTILSRC = $(sort $(wildcard ${SRCDIR}/main_*.cpp))
UTILS   = $(patsubst ${SRCDIR}/main_%.cpp, %, ${UTILSRC})

SOURCES	= $(filter-out ${UTILSRC}, ${ALLSRC})

OBJECTS	= $(patsubst ${SRCDIR}/%.cpp, ${OBJDIR}/%.o, ${SOURCES})

CLNMSG	= echo ; echo Cleaning up previous build:; echo ;
OBJMSG	= echo ; echo Compiling objects:; echo ;
BINMSG	= echo ; echo Compiling utilities:; echo ;

all: ${UTILS}

${OBJDIR}/%.o: ${SRCDIR}/%.cpp
	@${OBJMSG}
	$(eval OBJMSG =)
	$(eval PADSRC = $(shell printf "%-31s" "$<"))
	$(eval PADOBJ = $(shell printf "%-29s" "$@"))
	${CACHE} ${CXX} -c ${PADSRC} -o ${PADOBJ} ${FLAGS}

${OBJDIR}/%.o: ${NRLDIR}/%.c
	@${OBJMSG}
	$(eval OBJMSG =)
	$(eval PADSRC = $(shell printf "%-31s" "$<"))
	$(eval PADOBJ = $(shell printf "%-29s" "$@"))
	${CACHE} ${CC} -c ${PADSRC} -o ${PADOBJ} ${CFLAGS}

${LIBSGNL}: ${OBJECTS} ${NRLOBJ}
	@ar rcs $@ ${OBJECTS} ${NRLOBJ}

# Each utility depends on its own source as well as the library. Without the
# first prerequisite an edit to src/main_*.cpp does not relink, and a stale
# binary is silently kept - which is how a "rebuilt" tree can still run old code.
${BINDIR}/%: ${SRCDIR}/main_%.cpp ${LIBSGNL}
	@${BINMSG}
	$(eval BINMSG =)
	$(eval PADSRC = $(shell printf "%-32s" "$<"))
	$(eval PADOBJ = $(shell printf "%-23s" "$@"))
	${CXX} ${PADSRC} -o ${PADOBJ} ${LIBSGNL} ${LIBEXT} ${FLAGS}

.SECONDEXPANSION:

${UTILS}: ${BINDIR}/$$@

# Header dependencies recorded by -MMD, for both the library objects and the
# utilities. Missing on a clean tree, which is why this is -include.
-include $(OBJECTS:.o=.d)
-include $(patsubst ${BINDIR}/%, ${BINDIR}/%.d, $(addprefix ${BINDIR}/, ${UTILS}))

clean:
	@${CLNMSG}
	$(eval CLNMSG =)
	rm -f ${OBJDIR}/*
	rm -f ${LIBDIR}/*
	rm -f ${BINDIR}/*
	rm -f ${OUTDIR}/*

rebuild: clean all

debug: FLAGS += ${DEBUGFL}
debug: rebuild

clang: CXX = clang++
clang: rebuild

clangdebug: CXX = clang++
clangdebug: debug

hardmode: FLAGS += -Weverything -Wno-c++98-compat-pedantic -Wno-padded         \
                   -Wno-global-constructors -Wno-exit-time-destructors
hardmode: clang

intel: CXX = icpc
intel: CACHE =
intel: FLAGS = ${INTELFL}
intel: rebuild
