// tests/devtools/fake_curl.cpp — a stand-in for the host's `curl`, so that the download path of tools/fetch.cpp can be tested without a network.
//
// `fetch` spawns `curl`, found on PATH, for every download.  The tests put THIS program, built under the name `curl`, first on PATH and steer it
// with environment variables:
//   ODL_FAKE_CURL_LOG     a file; every call appends its arguments, one per line, between "--- call" and "--- end"
//   ODL_FAKE_CURL_BODY    a file whose bytes are written to the `--output` path: what a download that succeeded delivers
//   ODL_FAKE_CURL_EXIT    the exit status (default 0).  A status other than 0 writes no output file, as `curl --fail` does for an HTTP error
//   ODL_FAKE_CURL_STDERR  what is written to standard error when the status is not 0
//   ODL_FAKE_CURL_ONLY_URL  when set, the non-zero status applies only to a URL (the last argument) that contains this text; the others succeed
// It understands no other option and retries nothing: the tests look at the arguments `fetch` chose, not at what curl does with them.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string output;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--output") output = argv[i + 1];
    }
    if (const char* log = std::getenv("ODL_FAKE_CURL_LOG")) {
        std::ofstream f(log, std::ios::app);
        f << "--- call\n";
        for (int i = 1; i < argc; ++i) f << argv[i] << '\n';
        f << "--- end\n";
    }
    const char* status = std::getenv("ODL_FAKE_CURL_EXIT");
    const char* only = std::getenv("ODL_FAKE_CURL_ONLY_URL");
    const std::string url = argc > 1 ? argv[argc - 1] : "";
    const int code = status != nullptr && (only == nullptr || url.find(only) != std::string::npos) ? std::atoi(status) : 0;
    if (code != 0) {
        if (const char* message = std::getenv("ODL_FAKE_CURL_STDERR")) std::cerr << message << '\n';
        return code;
    }
    const char* body = std::getenv("ODL_FAKE_CURL_BODY");
    if (body != nullptr && !output.empty()) {
        std::ifstream in(body, std::ios::binary);
        std::ofstream out(output, std::ios::binary | std::ios::trunc);
        out << in.rdbuf();
    }
    return 0;
}
