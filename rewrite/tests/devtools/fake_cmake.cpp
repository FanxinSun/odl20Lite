// tests/devtools/fake_cmake.cpp — the stand-in for `cmake` that tools/reprocheck.cpp runs (plan L0 step 8, group C11).
//
// reprocheck runs two commands per build directory: `cmake -S ROOT -B OUT -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo` and `cmake --build OUT`.  This program stands in for both and "builds" whatever artefacts a
// recipe says, so that a test can make the two builds agree, differ in content, differ in size, leave an artefact out of one of them or fail, without a compiler.  It decides which build it is in by the
// directory's name: build-repro-a is side `a`, build-repro-bbbbbbbb is side `b`, anything else is `x`.  It is steered by environment variables:
//   ODL_FAKE_CMAKE_LOG     a file; every call appends a record to it: the arguments, the three variables reprocheck sets for the build, the working directory, the mode and the side, and whether the
//                          build directory existed before the call (a configure of a directory that still holds what an earlier run left is a defect of reprocheck) and what it held
//   ODL_FAKE_CMAKE_RECIPE  a file of lines  SIDES KIND REL ARG [CONTENT]  (blank lines and lines beginning with # are skipped), applied by `--build`:
//                            SIDES    `*` (every side) or a string of the sides it is for, such as `a` or `ab`
//                            file     REL ARG=octal mode CONTENT=the rest of the line, in which \n is a newline, \\ a backslash, @SIDE@ the side, @OUT@ the build directory and @STAMP@ the value of
//                                     SOURCE_DATE_EPOCH the call was given; a CONTENT of - is an empty file
//                            link     REL ARG=the target of a symbolic link
//                            dir      REL                      fifo      REL
//                          Parent directories of REL are made.
//   ODL_FAKE_CMAKE_FAIL    a comma-separated list of  configure:SIDE  and  build:SIDE : those calls print a line to each stream and fail with the status of ODL_FAKE_CMAKE_EXIT (default 7);
//                          ODL_FAKE_CMAKE_FAIL_OUT and ODL_FAKE_CMAKE_FAIL_ERR, when set, are written to standard output and standard error INSTEAD of that line (\n, \r, \\ and \xHH decoded)
//   `--configure` makes the directory and writes a CMakeCache.txt and a build.ninja in it, which are not artefacts.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::string env(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

bool exists(const std::string& path) {
    struct stat st{};
    return ::lstat(path.c_str(), &st) == 0;
}

void make_directories(const std::string& path) {
    std::string made;
    std::size_t from = 0;
    while (from <= path.size()) {
        const std::size_t slash = path.find('/', from);
        const std::size_t end = slash == std::string::npos ? path.size() : slash;
        made = path.substr(0, end);
        if (!made.empty()) ::mkdir(made.c_str(), 0755);
        if (slash == std::string::npos) break;
        from = slash + 1;
    }
}

std::string parent_of(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

std::string base_of(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// the names inside a directory, one level, sorted by the order readdir gave (the test looks for one name)
std::vector<std::string> names_in(const std::string& directory) {
    std::vector<std::string> names;
    if (DIR* d = ::opendir(directory.c_str())) {
        while (const dirent* entry = ::readdir(d)) {
            const std::string name = entry->d_name;
            if (name != "." && name != "..") names.push_back(name);
        }
        ::closedir(d);
    }
    return names;
}

int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string unescape_and_fill(const std::string& text, const std::string& side, const std::string& out, const std::string& stamp) {
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            ++i;
            if (text[i] == 'x' && i + 2 < text.size() && hex_digit(text[i + 1]) >= 0 && hex_digit(text[i + 2]) >= 0) {
                result += static_cast<char>(hex_digit(text[i + 1]) * 16 + hex_digit(text[i + 2]));
                i += 2;
            } else {
                result += text[i] == 'n' ? '\n' : (text[i] == 'r' ? '\r' : text[i]);
            }
        } else if (text.compare(i, 6, "@SIDE@") == 0) {
            result += side;
            i += 5;
        } else if (text.compare(i, 5, "@OUT@") == 0) {
            result += out;
            i += 4;
        } else if (text.compare(i, 7, "@STAMP@") == 0) {
            result += stamp;
            i += 6;
        } else {
            result += text[i];
        }
    }
    return result;
}

void apply_recipe(const std::string& recipe_path, const std::string& side, const std::string& out, const std::string& stamp) {
    std::ifstream recipe(recipe_path);
    std::string line;
    while (std::getline(recipe, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream words(line);
        std::string sides;
        std::string kind;
        std::string rel;
        std::string arg;
        words >> sides >> kind >> rel;
        if (sides != "*" && sides.find(side) == std::string::npos) continue;
        const std::string path = out + "/" + rel;
        make_directories(parent_of(path));
        if (kind == "dir") {
            make_directories(path);
        } else if (kind == "fifo") {
            ::mkfifo(path.c_str(), 0644);
        } else if (kind == "link") {
            words >> arg;
            if (::symlink(arg.c_str(), path.c_str()) != 0) std::cerr << "fake cmake: cannot make the link " << path << '\n';
        } else if (kind == "file") {
            words >> arg;
            std::string content;
            std::getline(words, content);
            if (!content.empty() && content[0] == ' ') content.erase(0, 1);
            if (content == "-") content.clear();
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file << unescape_and_fill(content, side, out, stamp);
            file.close();
            ::chmod(path.c_str(), static_cast<mode_t>(std::strtol(arg.c_str(), nullptr, 8)));
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string mode;
    std::string out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-B" && i + 1 < args.size()) {
            mode = "configure";
            out = args[i + 1];
        } else if (args[i] == "--build" && i + 1 < args.size()) {
            mode = "build";
            out = args[i + 1];
        }
    }
    const std::string name = base_of(out);
    const std::string side = name == "build-repro-a" ? "a" : (name == "build-repro-bbbbbbbb" ? "b" : "x");
    const std::string stamp = env("SOURCE_DATE_EPOCH");

    if (const std::string log_path = env("ODL_FAKE_CMAKE_LOG"); !log_path.empty()) {
        std::ofstream log(log_path, std::ios::app);
        log << "--- call\n";
        for (const std::string& a : args) log << "arg " << a << '\n';
        for (const char* variable : {"SOURCE_DATE_EPOCH", "LC_ALL", "TZ"}) {
            const char* value = std::getenv(variable);
            log << "env " << variable << '=' << (value != nullptr ? value : "(unset)") << '\n';
        }
        char cwd[4096];
        log << "cwd " << (::getcwd(cwd, sizeof cwd) != nullptr ? cwd : "?") << '\n';
        log << "mode " << (mode.empty() ? "?" : mode) << '\n';
        log << "side " << side << '\n';
        log << "directory_existed_before " << (exists(out) ? "yes" : "no") << '\n';
        std::string held;
        for (const std::string& n : names_in(out)) held += (held.empty() ? "" : ",") + n;
        log << "directory_held " << held << '\n';
        log << "--- end\n";
    }

    const std::string what = mode + ":" + side;
    std::istringstream failing(env("ODL_FAKE_CMAKE_FAIL"));
    std::string item;
    while (std::getline(failing, item, ',')) {
        if (item == what) {
            const std::string status = env("ODL_FAKE_CMAKE_EXIT");
            const char* out_text = std::getenv("ODL_FAKE_CMAKE_FAIL_OUT");
            const char* err_text = std::getenv("ODL_FAKE_CMAKE_FAIL_ERR");
            if (out_text != nullptr) std::cout << unescape_and_fill(out_text, side, out, stamp);
            else std::cout << "fake cmake: standard output of " << what << '\n';
            if (err_text != nullptr) std::cerr << unescape_and_fill(err_text, side, out, stamp);
            else std::cerr << "fake cmake: standard error of " << what << '\n';
            return status.empty() ? 7 : std::atoi(status.c_str());
        }
    }

    if (mode == "configure") {
        make_directories(out);
        std::ofstream(out + "/CMakeCache.txt") << "# the stand-in's cache\n";
        std::ofstream(out + "/build.ninja") << "# the stand-in's build.ninja\n";
        std::cout << "-- fake configure of " << what << '\n';
        return 0;
    }
    if (mode == "build") {
        if (!exists(out)) {
            std::cerr << "fake cmake: " << out << " is not a build directory\n";
            return 1;
        }
        if (const std::string recipe = env("ODL_FAKE_CMAKE_RECIPE"); !recipe.empty()) apply_recipe(recipe, side, out, stamp);
        std::cout << "fake build of " << what << '\n';
        return 0;
    }
    std::cerr << "fake cmake: neither -B nor --build given\n";
    return 64;
}
