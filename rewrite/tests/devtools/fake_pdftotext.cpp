// tests/devtools/fake_pdftotext.cpp — a stand-in for poppler's `pdftotext`, so that tools/tides_from_conventions.cpp can be tested without the host program (plan L0 step 8, group C10).
//
// `tides_from_conventions` runs  pdftotext -layout PDF -  and reads the text on standard output.  pdftotext is a regeneration-only host program (poppler, GPL: not part of the build, the tests or CI, not
// declared in the manifest, not redistributed — the maintainer's ruling D9).  The tests point the tool's `program` setting at THIS program, built under the name `pdftotext` into `fakebin/` beside the test
// executable.  It treats the "PDF" as a text file and writes its bytes to standard output unchanged, so that a test chooses what the tool reads by writing the file; the tool also hashes the same file.
// It is steered by environment variables:
//   ODL_FAKE_PDFTOTEXT_LOG     a file; every call appends its arguments, one per line, between "--- call" and "--- end"
//   ODL_FAKE_PDFTOTEXT_EXIT    the exit status (default 0).  A status other than 0 writes nothing to standard output
//   ODL_FAKE_PDFTOTEXT_STDERR  what is written to standard error when the status is not 0
//   ODL_FAKE_PDFTOTEXT_ONLY    when set, the non-zero status applies only to a call whose PDF path contains this text; the others succeed
// A file that cannot be opened is what poppler says about it, on standard error, with status 1.  It understands no option; the tests look at the arguments the tool chose.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    // pdftotext -layout PDF -  : the PDF is the argument before the final "-"
    const std::string pdf = argc >= 3 ? argv[argc - 2] : "";
    if (const char* log = std::getenv("ODL_FAKE_PDFTOTEXT_LOG")) {
        std::ofstream f(log, std::ios::app);
        f << "--- call\n";
        for (int i = 1; i < argc; ++i) f << argv[i] << '\n';
        f << "--- end\n";
    }
    const char* status = std::getenv("ODL_FAKE_PDFTOTEXT_EXIT");
    const char* only = std::getenv("ODL_FAKE_PDFTOTEXT_ONLY");
    const int code = status != nullptr && (only == nullptr || pdf.find(only) != std::string::npos) ? std::atoi(status) : 0;
    if (code != 0) {
        if (const char* message = std::getenv("ODL_FAKE_PDFTOTEXT_STDERR")) std::cerr << message << '\n';
        return code;
    }
    std::ifstream in(pdf, std::ios::binary);
    if (!in) {
        std::cerr << "Error: Couldn't open file '" << pdf << "'\n";
        return 1;
    }
    std::cout << in.rdbuf();
    std::cout.flush();
    return 0;
}
