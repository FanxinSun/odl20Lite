// tests/devtools/fake_gfortran.cpp — a stand-in for the host's Fortran compiler, so that tools/msis_reference.cpp can be tested without gfortran (plan L0 step 8, group C10).
//
// `msis_reference` builds NRL's published Fortran twice with gfortran and runs both executables.  gfortran is a regeneration-only host program: it is not part of the build, the tests or CI, not declared in
// the manifest and not redistributed (the maintainer's ruling D9, ATMO-R-028: nothing may come to need it).  The tests put THIS program, built under the name `gfortran` into `fakebin/` beside the test
// executable, on the tool's PATH, and steer it with environment variables:
//   ODL_FAKE_GFORTRAN_LOG        a file; every call appends its arguments, one per line, between "--- call" and "--- end"
//   ODL_FAKE_GFORTRAN_VERSION    the first line of `--version` (default "GNU Fortran (Fake) 99.9.9 20990101"); two more lines follow it, as a real compiler prints
//   ODL_FAKE_GFORTRAN_CAPTURE    a directory; the sources of a compilation are copied there as <name>.s (the build as published) or <name>.d (with -freal-4-real-8)
//   ODL_FAKE_GFORTRAN_EXIT       the exit status of a COMPILATION (default 0); a non-zero status writes no executable.  `--version` always succeeds
//   ODL_FAKE_GFORTRAN_STDERR     what a failing compilation writes to standard error
//   ODL_FAKE_GFORTRAN_EXE_MODE   the permissions of the "executable" in octal (default 755)
// A compilation "produces" its `-o` file by copying the stand-in reference program (tests/devtools/fake_reference.cpp, built as `odl_fake_reference` into the same directory), which prints what the test
// prepared.  The compiler reads nothing of the Fortran: the tests look at the arguments the tool chose and at the sources it wrote.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (const char* log = std::getenv("ODL_FAKE_GFORTRAN_LOG")) {
        std::ofstream f(log, std::ios::app);
        f << "--- call\n";
        for (int i = 1; i < argc; ++i) f << argv[i] << '\n';
        f << "--- end\n";
    }
    if (argc == 2 && std::string(argv[1]) == "--version") {
        const char* v = std::getenv("ODL_FAKE_GFORTRAN_VERSION");
        std::cout << (v != nullptr ? v : "GNU Fortran (Fake) 99.9.9 20990101") << "\nCopyright (C) 2099 Nobody\nThis is free software; see the source for copying conditions.\n";
        return 0;
    }
    std::string exe;
    bool promoted = false;
    std::vector<std::string> sources;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            exe = argv[++i];
        } else if (a == "-freal-4-real-8") {
            promoted = true;
        } else if (!a.empty() && a.front() != '-') {
            sources.push_back(a);
        }
    }
    if (const char* status = std::getenv("ODL_FAKE_GFORTRAN_EXIT")) {
        const int code = std::atoi(status);
        if (code != 0) {
            if (const char* message = std::getenv("ODL_FAKE_GFORTRAN_STDERR")) std::cerr << message << '\n';
            return code;
        }
    }
    for (const std::string& s : sources) {
        if (!fs::exists(s)) {
            std::cerr << "f951: Error: Can't open file '" << s << "'\n";
            return 1;
        }
    }
    if (const char* capture = std::getenv("ODL_FAKE_GFORTRAN_CAPTURE")) {
        for (const std::string& s : sources) fs::copy_file(s, fs::path(capture) / (fs::path(s).filename().string() + (promoted ? ".d" : ".s")), fs::copy_options::overwrite_existing);
    }
    if (exe.empty()) {
        std::cerr << "gfortran: fatal error: no output file\n";
        return 1;
    }
    const fs::path reference = fs::read_symlink("/proc/self/exe").parent_path() / "odl_fake_reference";
    fs::copy_file(reference, exe, fs::copy_options::overwrite_existing);
    const char* mode = std::getenv("ODL_FAKE_GFORTRAN_EXE_MODE");
    fs::permissions(exe, static_cast<fs::perms>(std::strtol(mode != nullptr ? mode : "755", nullptr, 8)), fs::perm_options::replace);
    return 0;
}
