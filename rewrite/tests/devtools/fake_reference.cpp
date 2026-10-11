// tests/devtools/fake_reference.cpp — the stand-in for the "executables" that the stand-in compiler (tests/devtools/fake_gfortran.cpp) produces: the NRLMSISE-00 reference built from Fortran (plan L0 step 8, group C10).
//
// `msis_reference` runs two executables named ref_s (the Fortran as published) and ref_d (every REAL widened), feeds each the sweep on standard input and parses the lines it prints.  The stand-in compiler copies
// THIS program to those names; it behaves by its own name (argv[0] without the directory, upper-cased: REF_S, REF_D) and is steered by environment variables:
//   ODL_FAKE_REF_OUT_<NAME>    a file whose bytes are written to standard output (what the reference "computes")
//   ODL_FAKE_REF_STDIN_<NAME>  a file; the whole of standard input is written there (what the tool fed it)
//   ODL_FAKE_REF_EXIT_<NAME>   the exit status (default 0); a non-zero status writes no output
//   ODL_FAKE_REF_STDERR_<NAME> what is written to standard error when the status is not 0
// Standard input is always read to its end first, as the Fortran reads its sweep.

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

// Standard input to its end, with stdio.  The first version built the string from std::istreambuf_iterator<char>(std::cin): GCC 13.3, the compiler of GitHub's runner (Ubuntu 24.04), inlines that iterator's
// sbumpc() and -Wnull-dereference, an error under -Werror, takes std::cin's buffer for a null pointer it cannot rule out; GCC 13.4 does not warn (PROVENANCE.md section 41.22).  The bytes read are the same.
std::string read_all_of_stdin() {
    std::string all;
    std::array<char, 8192> block{};
    for (;;) {
        const std::size_t got = std::fread(block.data(), 1, block.size(), stdin);
        all.append(block.data(), got);
        if (got < block.size()) return all;   // the end of the input; a read error ends it too, as it ended the iterator's loop
    }
}

}  // namespace

int main(int, char** argv) {
    std::string name = argv[0];
    name = name.substr(name.find_last_of('/') == std::string::npos ? 0 : name.find_last_of('/') + 1);
    for (char& c : name) c = static_cast<char>(std::isalnum(static_cast<unsigned char>(c)) != 0 ? std::toupper(static_cast<unsigned char>(c)) : '_');
    const auto env = [&](const char* prefix) { return std::getenv((std::string(prefix) + name).c_str()); };
    const std::string input = read_all_of_stdin();
    if (const char* path = env("ODL_FAKE_REF_STDIN_")) {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        f << input;
    }
    if (const char* status = env("ODL_FAKE_REF_EXIT_")) {
        const int code = std::atoi(status);
        if (code != 0) {
            if (const char* message = env("ODL_FAKE_REF_STDERR_")) std::cerr << message << '\n';
            return code;
        }
    }
    if (const char* out = env("ODL_FAKE_REF_OUT_")) {
        std::ifstream in(out, std::ios::binary);
        std::cout << in.rdbuf();
        std::cout.flush();
    }
    return 0;
}
