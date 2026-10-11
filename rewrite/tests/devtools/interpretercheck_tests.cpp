// tests/devtools/interpretercheck_tests.cpp — the script-interpreter check (plan L0 step 8, group C12; ci.sh's gate 14): its tables, each refusal it can make and every line it says, on synthetic trees that each fail in ONE way
// alone, and the real tree held to a second method written here apart from the tool.
// (ctests `interpretercheck.behaviour` and `interpretercheck.real_tree`; `interpreters.refused_in_build_tests_and_ci` runs the built tool on the real tree.)
//
// Every expectation is DERIVED BY HAND from the design registered before the tool existed (REPORT.files/C12_proof_registration.txt, item C and its three amendments), or comes from a SECOND METHOD written here apart from
// the tool: the lists of programs and extensions below are typed again from the registration (not read out of the tool), and the real tree is walked with POSIX readdir/lstat and scanned with std::regex.  None is
// taken from running the tool to see what it says.
//
// THE SEVEN INJECTIONS of the registration (C-1), each made alone on a synthetic tree that passes: a script file; the find_package of a script interpreter in a CMake file; the action that sets one up in a workflow; the
// program named in a shell script; a Perl line in a shell script; a Perl file; and an extensionless file whose first line is a #! naming one.  Then each of the nine languages is tried through the three doors.
//
// NOTE FOR THE READER OF THE TREE'S OWN SCAN: this file is a .cpp under tests/, which the check reads only by name and first line, not for words -- the words below are its inputs.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/tool.hpp>

#include "interpretercheck.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
namespace ic = odl::tools::interpretercheck;
using namespace odl::devkit;

namespace {

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- helpers

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_with(const ic::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = ic::run_on(settings, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

std::string lowered(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string uppered(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// a synthetic tree that passes: rewrite/ with the files below, and the workflows directory beside it (.github/workflows), in a temporary directory of its own
//   CMakeLists.txt  cmake/Helper.cmake  tools/CMakeLists.txt  tools/ci.sh  tools/bootstrap.sh  tools/tool.cpp  README.md     = 7 files, of which 5 are read for words
//   .github/workflows/ci.yml                                                                                                  = 1 workflow file
class Tree {
public:
    Tree() : td_("odl-ic") {
        fs::create_directories(root());
        put("README.md", "# the synthetic tree\n");
        put("CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\nproject(synthetic CXX)\nadd_subdirectory(tools)\n");
        put("cmake/Helper.cmake", "function(helper)\nendfunction()\n");
        put("tools/CMakeLists.txt", "add_executable(tool tool.cpp)\n");
        put("tools/ci.sh", "#!/bin/sh\nset -eu\necho gates\n");
        put("tools/bootstrap.sh", "#!/usr/bin/env bash\nset -eu\ntools/ci.sh\n");
        put("tools/tool.cpp", "int main() { return 0; }\n");
        put_workflow("ci.yml",
                     "name: ci\non: [push]\njobs:\n  gates:\n    runs-on: ubuntu-24.04\n    steps:\n      - uses: actions/checkout@v4\n      - run: rewrite/tools/ci.sh\n");
    }
    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    [[nodiscard]] fs::path base() const { return td_.path(); }
    [[nodiscard]] fs::path root() const { return td_.path() / "rewrite"; }
    [[nodiscard]] fs::path workflows() const { return td_.path() / ".github" / "workflows"; }

    void put(const std::string& relative, const std::string& text) const {
        const fs::path p = root() / relative;
        fs::create_directories(p.parent_path());
        write_text(p, text);
    }
    void put_bytes(const std::string& relative, const std::string& bytes) const {
        const fs::path p = root() / relative;
        fs::create_directories(p.parent_path());
        write_bytes(p, as_bytes(bytes));
    }
    void put_workflow(const std::string& name, const std::string& text) const {
        const fs::path p = workflows() / name;
        fs::create_directories(p.parent_path());
        write_text(p, text);
    }
    void append(const std::string& relative, const std::string& text) const { put(relative, read_text(root() / relative) + text); }
    void append_workflow(const std::string& name, const std::string& text) const { put_workflow(name, read_text(workflows() / name) + text); }

    [[nodiscard]] ic::Settings settings() const {
        ic::Settings s;
        s.root = root();
        return s;
    }
    [[nodiscard]] Result run(const std::vector<std::string>& args = {}) const { return run_with(settings(), args); }
    [[nodiscard]] ic::Report report() const { return ic::check(settings()); }

private:
    TempDir td_;
};

// the output lines of the tool, derived by hand from its registered design
std::string ok_line(std::size_t files, std::size_t build, std::size_t workflow, std::size_t allowed) {
    return "ok       no script interpreter in the build, the tests or CI (files looked at by name and first line: " + std::to_string(files) + "; build files read for words: " + std::to_string(build) +
           "; workflow files read for words: " + std::to_string(workflow) + "; allowlisted lines: " + std::to_string(allowed) + ")\n";
}

std::string refused_line(std::size_t refusals, std::size_t files) {
    return "interpretercheck: REFUSED: " + std::to_string(refusals) + " refusal(s) in " + std::to_string(files) +
           " file(s) -- no script interpreter may reach the build, the tests or CI (plan section 5 constraint 11)\n";
}

// the same, when one allowlist entry is stale as well
std::string refused_and_stale_line(std::size_t refusals, std::size_t files, std::size_t stale) {
    return "interpretercheck: REFUSED: " + std::to_string(refusals) + " refusal(s) in " + std::to_string(files) + " file(s), " + std::to_string(stale) + (stale == 1 ? " stale allowlist entry" : " stale allowlist entries") +
           " -- no script interpreter may reach the build, the tests or CI (plan section 5 constraint 11)\n";
}

const char kUsage[] = "usage: interpretercheck [-h] [--quiet] [--no-allowlist] [--root ROOT] [--workflows WORKFLOWS]\n";

std::string names_of(const std::vector<ic::Word>& words) {
    std::string out;
    for (const ic::Word& w : words) out += (out.empty() ? "" : " ") + w.text;
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------- the registered lists, typed again here

struct Language {
    const char* name;
    std::vector<std::string> words;        // programs and package tools, by base name
    std::vector<std::string> extensions;   // file extensions
};

const std::vector<Language>& languages() {
    static const std::vector<Language> all = {
        {"Python", {"python", "pip", "pipx", "pytest", "virtualenv", "venv", "conda", "poetry", "setuptools", "pypy", "ipython", "jupyter"}, {"py", "pyc", "pyo", "pyw", "pyi", "pyx", "ipynb"}},
        {"Perl", {"perl", "cpan", "cpanm"}, {"pl", "pm"}},
        {"Ruby", {"ruby", "jruby", "irb", "gem", "bundler", "rake"}, {"rb", "gemspec"}},
        {"JavaScript", {"node", "nodejs", "npm", "npx", "yarn", "pnpm", "deno", "bun"}, {"js", "mjs", "cjs", "jsx"}},
        {"PHP", {"php", "composer"}, {"php", "phar"}},
        {"Lua", {"lua", "luajit", "luarocks"}, {"lua"}},
        {"Tcl", {"tcl", "tclsh"}, {"tcl"}},
        {"R", {"rscript"}, {"r", "rmd"}},
        {"Julia", {"julia"}, {"jl"}},
    };
    return all;
}

bool is_word_of(const Language& l, const std::string& w) { return std::find(l.words.begin(), l.words.end(), w) != l.words.end(); }

// the conventional file names of the registration, with their languages
const std::vector<std::pair<std::string, std::string>>& conventional_names() {
    static const std::vector<std::pair<std::string, std::string>> all = {
        {"setup.cfg", "Python"},    {"pyproject.toml", "Python"}, {"Pipfile", "Python"},       {"Pipfile.lock", "Python"},   {"requirements.txt", "Python"}, {"requirements-dev.txt", "Python"},
        {"package.json", "JavaScript"}, {"package-lock.json", "JavaScript"}, {"yarn.lock", "JavaScript"}, {"composer.json", "PHP"}, {"Gemfile", "Ruby"}, {"Gemfile.lock", "Ruby"},
        {"Rakefile", "Ruby"},       {"cpanfile", "Perl"},
    };
    return all;
}

}  // namespace

// ============================================================================================================================================================================== the tables

TEST_CASE("flagged_words: every program of the registered list, alone, in any case, with digits after it, and between separators", "[interpretercheck][behaviour]") {
    for (const Language& lang : languages()) {
        for (const std::string& w : lang.words) {
            INFO(lang.name << ": " << w);
            const auto one = [&](const std::string& line, const std::string& text) {
                INFO("line: " << line);
                const std::vector<ic::Word> found = ic::flagged_words(line);
                REQUIRE(found.size() == 1);
                CHECK(found.front().text == text);
                CHECK(found.front().language == lang.name);
                CHECK(found.front().kind == "word");
            };
            one(w, w);
            one(uppered(w), w);
            one(w + "3", w + "3");                  // python3, pip3, lua5, node18: the trailing digits are taken off to look the word up
            one(uppered(w) + "12", w + "12");
            one("x_" + w + "_y", w);                // an underscore separates words: ENABLE_PYTHON, Python3_EXECUTABLE
            one("-" + w + "-", w);
            one("/usr/bin/" + w, w);
            one("(" + w + ")", w);
            one("${" + uppered(w) + "}", w);
            // glued to a letter, a word is another word: not flagged
            CHECK(ic::flagged_words("q" + w).empty());
            CHECK(ic::flagged_words(w + "q").empty());
            CHECK(ic::flagged_words("3" + w).empty());   // leading digits are part of the word, too
            CHECK(ic::flagged_words(w + "3q").empty());
        }
    }
}

TEST_CASE("flagged_words: every extension of the registered list, after a dot and whatever stands before the dot", "[interpretercheck][behaviour]") {
    for (const Language& lang : languages()) {
        for (const std::string& e : lang.extensions) {
            INFO(lang.name << ": ." << e);
            const auto one = [&](const std::string& line, const std::string& text) {
                INFO("line: " << line);
                const std::vector<ic::Word> found = ic::flagged_words(line);
                REQUIRE(found.size() == 1);
                CHECK(found.front().text == text);
                CHECK(found.front().language == lang.name);
                CHECK(found.front().kind == "extension");
            };
            one("fetch." + e, e);
            one("." + e, e);                        // a line that begins with the dot
            one("*." + e, e);                       // a glob
            one("${name}." + e, e);                 // a name built from a variable
            one("'." + e + "'", e);                 // a quoted extension
            one("fetch." + uppered(e), e);
            one("a.b.c." + e, e);
            one("fetch." + e + "_x", e);            // the word ends at the underscore, as the scan of the registration reads it
            // no dot, no extension (and these are no program either, but for php, lua and tcl, which are both)
            const std::string bare = "fetch " + e;
            const std::vector<ic::Word> without = ic::flagged_words(bare);
            CHECK(without.size() == (is_word_of(lang, e) ? 1U : 0U));
            // the extension is compared whole, digits kept: ".e2" and ".eq" are other words
            const std::vector<ic::Word> digits = ic::flagged_words("fetch." + e + "2");
            CHECK(digits.size() == (is_word_of(lang, e) ? 1U : 0U));   // php2, lua2, tcl2 are the programs again; py2 is nothing
            if (is_word_of(lang, e)) CHECK(digits.front().kind == "word");
            CHECK(ic::flagged_words("fetch." + e + "q").empty());
        }
    }
}

TEST_CASE("flagged_words: the extensions that are also programs (php, lua, tcl) are said to be extensions after a dot and programs without one", "[interpretercheck][behaviour]") {
    for (const char* w : {"php", "lua", "tcl"}) {
        INFO(w);
        const std::vector<ic::Word> dotted = ic::flagged_words(std::string("x.") + w);
        REQUIRE(dotted.size() == 1);
        CHECK(dotted.front().kind == "extension");
        const std::vector<ic::Word> bare = ic::flagged_words(std::string("run ") + w);
        REQUIRE(bare.size() == 1);
        CHECK(bare.front().kind == "word");
    }
}

TEST_CASE("flagged_words: lines as they stand in build files -- what is flagged, in the order of the line, and what is not", "[interpretercheck][behaviour]") {
    const auto words = [](const std::string& line) { return names_of(ic::flagged_words(line)); };
    CHECK(words("find_package(Python3 REQUIRED)") == "python3");
    CHECK(words("set(PYTHON_EXECUTABLE /usr/bin/python3)") == "python python3");
    CHECK(words("ENABLE_PYTHON") == "python");
    CHECK(words("Python3_EXECUTABLE") == "python3");
    CHECK(words("      - uses: actions/setup-python@v5") == "python");
    CHECK(words("python3.13") == "python3");   // the 13 after the dot is no extension
    CHECK(words("python3 -m pip install foo.py && perl x") == "python3 pip py perl");
    CHECK(words("ruby.rb") == "ruby rb");
    CHECK(words("perl.pl.pm") == "perl pl pm");
    CHECK(words("python3-pip") == "python3 pip");
    CHECK(words("pip3") == "pip3");
    CHECK(words("PyThOn") == "python");
    CHECK(words("") == "");
    CHECK(words("   ") == "");
    CHECK(words("add_executable(tool tool.cpp)") == "");
    // a line holding several: one entry for each, with its language
    const std::vector<ic::Word> several = ic::flagged_words("perl x.js julia");
    REQUIRE(several.size() == 3);
    CHECK(several.at(0).language == "Perl");
    CHECK(several.at(1).language == "JavaScript");
    CHECK(several.at(1).kind == "extension");
    CHECK(several.at(2).language == "Julia");
    // near misses: other words, other extensions, no dot
    for (const char* line : {"pythonic", "perlin", "nodes", "pipeline", "rubyist", "venture", "npmx", "gems", "gemstone", "bunny", "bundle", "julian", "tclx", "phpx", "composers", "rakes", "yarns", "denote",
                             "denoise", "x.json", "x.sh", "x.cpp", "x.txt", "x.yml", "x.md", "x.cmake", "x.in", "x.hpp", "x.pdf", "x.rs", "x.pyd", "py_sum", "py", "x/py", "x-py", "x py", "e.g. here", "i.e. there",
                             "-r", "sort -r file", "x.", "...", "3.14", "1.0.5", "cmake", "add_subdirectory", "bash", "sh", "awk", "sed", "grep", "make", "ninja", "g++-13", "git", "tar", "curl", "ctest", "cargo",
                             "rustc", "go", "java", "javac", "gcc", "clang", "ccache"}) {
        INFO("line: " << line);
        CHECK(ic::flagged_words(line).empty());
    }
}

TEST_CASE("flagged_words: the edges of the alphanumeric ranges -- the neighbours of 0, 9, A, Z, a and z in the ASCII table", "[interpretercheck][behaviour]") {
    // '/' ':' '@' '[' '`' '{' stand next to the ranges and are separators; '0' '9' 'A' 'Z' 'a' 'z' are in them
    for (const char c : {'/', ':', '@', '[', '`', '{'}) {
        INFO("separator " << c);
        CHECK(ic::flagged_words(std::string("perl") + c).size() == 1);
        CHECK(ic::flagged_words(std::string(1, c) + "perl").size() == 1);
        CHECK(ic::flagged_words(std::string("perl") + c + "perl").size() == 2);
    }
    for (const char c : {'A', 'Z', 'a', 'z'}) {
        INFO("letter " << c);
        CHECK(ic::flagged_words(std::string("perl") + c).empty());
        CHECK(ic::flagged_words(std::string(1, c) + "perl").empty());
    }
    for (const char c : {'0', '9'}) {
        INFO("digit " << c);
        CHECK(ic::flagged_words(std::string("perl") + c).size() == 1);   // a digit after the word is taken off
        CHECK(ic::flagged_words(std::string(1, c) + "perl").empty());    // a digit before it is part of another word
    }
    // upper case reaches the lower case of every letter the lists use
    CHECK(names_of(ic::flagged_words("PIPX CONDA JULIA RAKE YARN BUNDLER LUAROCKS VIRTUALENV SETUPTOOLS RSCRIPT")) == "pipx conda julia rake yarn bundler luarocks virtualenv setuptools rscript");
    // bytes above 127 are separators, not part of a word
    CHECK(names_of(ic::flagged_words("python\xC3\xA9")) == "python");
    CHECK(ic::flagged_words("pyth\xC3\xB6n").empty());
    CHECK(names_of(ic::flagged_words(std::string("\xFF\xFE") + "perl" + std::string(1, '\0') + "x")) == "perl");
}

TEST_CASE("shebang_interpreter: the first word's last component, an env looked through, and what names none", "[interpretercheck][behaviour]") {
    const auto of = [](const std::string& line) -> std::string {
        const std::optional<std::string> r = ic::shebang_interpreter(line);
        return r.has_value() ? "<" + *r + ">" : "none";
    };
    CHECK(of("#!/bin/sh") == "<sh>");
    CHECK(of("#!/bin/bash") == "<bash>");
    CHECK(of("#! /bin/sh") == "<sh>");
    CHECK(of("#!\t/bin/sh") == "<sh>");
    CHECK(of("#!/bin/sh -e") == "<sh>");
    CHECK(of("#!/bin/bash -eu -o pipefail") == "<bash>");
    CHECK(of("#!sh") == "<sh>");
    CHECK(of("#!/bin/sh\r") == "<sh>");
    CHECK(of("#!/bin/sh\t-e") == "<sh>");              // a tab between the program and its argument separates them
    CHECK(of("#!/usr/bin/env\tbash") == "<bash>");
    CHECK(of("#!/usr/bin/env /bin/sh") == "<sh>");      // the word after env is a path, too: its last component
    CHECK(of("#!/usr/bin/env /usr/bin/python3") == "<python3>");
    CHECK(of("#!/usr/bin/env bash") == "<bash>");
    CHECK(of("#!/usr/bin/env sh") == "<sh>");
    CHECK(of("#!/usr/bin/env -S bash -e") == "<bash>");
    CHECK(of("#!/usr/bin/env -i -S perl") == "<perl>");
    CHECK(of("#!/usr/bin/env FOO=1 python3") == "<python3>");
    CHECK(of("#!/usr/bin/env  python3  -u") == "<python3>");
    CHECK(of("#!/usr/bin/env python3.13") == "<python3.13>");
    CHECK(of("#!/usr/bin/python3") == "<python3>");
    CHECK(of("#!/usr/local/bin/env bash") == "<bash>");
    CHECK(of("#!/usr/bin/awk -f") == "<awk>");
    CHECK(of("#!/usr/bin/env awk") == "<awk>");
    CHECK(of("#!/bin/sh/") == "<>");   // the last component of a path that ends in '/' is empty: it names nothing
    CHECK(of("#!/usr/bin/env") == "<>");
    CHECK(of("#!/usr/bin/env -S") == "<>");
    CHECK(of("#!/usr/bin/env A=1 B=2") == "<>");
    CHECK(of("#!") == "<>");
    CHECK(of("#!   ") == "<>");
    CHECK(of("#!\r") == "<>");
    CHECK(of("# !/bin/sh") == "none");
    CHECK(of("#/bin/sh") == "none");
    CHECK(of(" #!/bin/sh") == "none");
    CHECK(of("!#/bin/sh") == "none");
    CHECK(of("#") == "none");
    CHECK(of("") == "none");
    CHECK(of("echo hi") == "none");
}

TEST_CASE("shebang_language: the program's letters up to its first non-letter, case-insensitively", "[interpretercheck][behaviour]") {
    const std::vector<std::pair<std::string, std::string>> table = {
        {"python", "Python"}, {"python3", "Python"}, {"python3.13", "Python"}, {"Python3", "Python"}, {"pypy", "Python"}, {"pypy3", "Python"}, {"ipython", "Python"}, {"perl", "Perl"}, {"perl5.38.2", "Perl"},
        {"ruby", "Ruby"}, {"ruby3.2", "Ruby"}, {"jruby", "Ruby"}, {"irb", "Ruby"}, {"node", "JavaScript"}, {"nodejs", "JavaScript"}, {"node18", "JavaScript"}, {"deno", "JavaScript"}, {"bun", "JavaScript"},
        {"php", "PHP"}, {"php8.3", "PHP"}, {"lua", "Lua"}, {"lua5.4", "Lua"}, {"luajit", "Lua"}, {"tclsh", "Tcl"}, {"tclsh8.6", "Tcl"}, {"wish", "Tcl"}, {"wish8.6", "Tcl"}, {"tcl", "Tcl"}, {"Rscript", "R"},
        {"rscript", "R"}, {"julia", "Julia"}, {"sh", "other"}, {"bash", "other"}, {"awk", "other"}, {"env", "other"}, {"dash", "other"}, {"zsh", "other"}, {"", "other"}, {"3python", "other"}, {"pythonx", "other"},
        {"perlx", "other"}, {"lua-5", "Lua"}, {"pythonz", "other"}, {"pythonZ", "other"}, {"pythona", "other"}, {"pythonA", "other"}, {"apython", "other"}, {"zpython", "other"}};
    for (const auto& [program, language] : table) {
        INFO("program: " << program);
        CHECK(ic::shebang_language(program) == language);
    }
}

TEST_CASE("refused_name: every extension, in any case, and nothing but the last component", "[interpretercheck][behaviour]") {
    for (const Language& lang : languages()) {
        for (const std::string& e : lang.extensions) {
            INFO(lang.name << ": ." << e);
            for (const std::string& name : {"x." + e, "X." + uppered(e), "tools/deep/x." + e, "a.b." + e, "." + e}) {
                INFO("name: " << name);
                const std::optional<ic::Finding> f = ic::refused_name(name, false);
                REQUIRE(f.has_value());
                CHECK(f->file == name);
                CHECK(f->kind == "name");
                CHECK(f->language == lang.name);
                CHECK(f->what == "extension ." + e);
                CHECK(f->line == 0);
            }
            CHECK_FALSE(ic::refused_name("x." + e + ".", false).has_value());     // a trailing dot: no extension
            CHECK_FALSE(ic::refused_name("x." + e + "q", false).has_value());
            CHECK_FALSE(ic::refused_name("x." + e + ".bak", false).has_value());
            CHECK_FALSE(ic::refused_name("x" + e, false).has_value());            // no dot
            CHECK_FALSE(ic::refused_name("x." + e + "/readme", false).has_value());   // the extension is on a directory above, not on the file
            CHECK_FALSE(ic::refused_name("x." + e, true).has_value());            // a directory is refused by its own table only
        }
    }
    for (const char* name : {"x", "x.", "py", ".", "..", "x.json", "x.sh", "x.cpp", "x.hpp", "x.txt", "x.md", "x.cmake", "x.yml", "x.pdf", "x.tar.gz", ".gitignore", ".github", "CMakeLists.txt", "ci.sh", "README"}) {
        INFO("name: " << name);
        CHECK_FALSE(ic::refused_name(name, false).has_value());
    }
}

TEST_CASE("refused_name: the conventional names, in any case, and requirements*.txt", "[interpretercheck][behaviour]") {
    for (const auto& [name, language] : conventional_names()) {
        INFO("name: " << name);
        for (const std::string& variant : {name, lowered(name), uppered(name), "tools/" + name}) {
            INFO("variant: " << variant);
            const std::optional<ic::Finding> f = ic::refused_name(variant, false);
            REQUIRE(f.has_value());
            CHECK(f->kind == "name");
            CHECK(f->language == language);
            CHECK(f->what == "name " + variant.substr(variant.find_last_of('/') == std::string::npos ? 0 : variant.find_last_of('/') + 1));
        }
        CHECK_FALSE(ic::refused_name(name, true).has_value());   // a DIRECTORY of that name is nothing special
        CHECK_FALSE(ic::refused_name("my_" + name, false).has_value());
    }
    for (const char* name : {"requirementsx.txt", "requirements_all.txt", "Requirements.TXT", "requirements-3.txt"}) {
        INFO("name: " << name);
        const std::optional<ic::Finding> f = ic::refused_name(name, false);
        REQUIRE(f.has_value());
        CHECK(f->language == "Python");
    }
    for (const char* name : {"requirements", "requirements.md", "requirements.in", "my-requirements.txt", "requirement.txt", "xrequirements.txt", "setup.py.txt", "package.jsonx", "Pipfile.txt", "requirementstxt"}) {
        INFO("name: " << name);
        CHECK_FALSE(ic::refused_name(name, false).has_value());
    }
}

TEST_CASE("refused_name: the three directories, in any case -- and a file of that name is nothing special", "[interpretercheck][behaviour]") {
    for (const char* name : {"__pycache__", "node_modules", "site-packages"}) {
        INFO("name: " << name);
        for (const std::string& variant : {std::string(name), uppered(name), std::string("tools/") + name, std::string("a/b/") + name}) {
            INFO("variant: " << variant);
            const std::optional<ic::Finding> f = ic::refused_name(variant, true);
            REQUIRE(f.has_value());
            CHECK(f->kind == "directory");
            CHECK(f->file == variant);
            CHECK(f->what == variant.substr(variant.find_last_of('/') == std::string::npos ? 0 : variant.find_last_of('/') + 1));   // as it is written
            CHECK(f->language == (std::string(name) == "node_modules" ? "JavaScript" : "Python"));
            CHECK_FALSE(ic::refused_name(variant, false).has_value());
        }
        CHECK_FALSE(ic::refused_name(std::string("x") + name, true).has_value());
        CHECK_FALSE(ic::refused_name(std::string(name) + "x", true).has_value());
    }
    for (const char* name : {"modules", "tools", "build", "data", "pycache", "node", "packages", "site"}) CHECK_FALSE(ic::refused_name(name, true).has_value());
}

// ============================================================================================================================================================================== the seven injections

TEST_CASE("the clean synthetic tree passes and says what it covered -- 7 files by name and first line, 5 read for words, 1 workflow file", "[interpretercheck][behaviour]") {
    const Tree t;
    const Result r = t.run();
    CHECK(r.code == 0);
    CHECK(r.out == ok_line(7, 5, 1, 0));
    CHECK(r.err.empty());
    const ic::Report rep = t.report();
    CHECK(rep.violations.empty());
    CHECK(rep.accepted.empty());
    CHECK(rep.stale.empty());
    CHECK(rep.files_walked == 7);
    CHECK(rep.build_files == 5);
    CHECK(rep.workflow_files == 1);
    CHECK(rep.workflows_found);
    // --quiet prints the same when nothing is allowlisted, and the tool writes nothing
    const Result q = t.run({"--quiet"});
    CHECK(q.code == 0);
    CHECK(q.out == ok_line(7, 5, 1, 0));
    CHECK(q.err.empty());
}

TEST_CASE("each of the seven injections, made alone, is refused with exit 1, the file named, and for a word the line and the word", "[interpretercheck][behaviour]") {
    struct Injection {
        const char* what;
        std::function<void(const Tree&)> make;
        std::string expected;   // the lines on stderr before the summary
        std::size_t files;      // the files looked at by name and first line: the 7 of the clean tree, and the one an injection adds
    };
    const std::vector<Injection> injections = {
        {"a script file", [](const Tree& t) { t.put("tools/helper.py", "print(1)\n"); }, "tools/helper.py: Python: file name refused (extension .py)\n", 8},
        {"find_package of a script interpreter in a CMake file", [](const Tree& t) { t.append("CMakeLists.txt", "find_package(Python3 REQUIRED)\n"); },
         "CMakeLists.txt:4: Python: python3 -- find_package(Python3 REQUIRED)\n", 7},
        {"the setup action of a script interpreter in a workflow", [](const Tree& t) { t.append_workflow("ci.yml", "      - uses: actions/setup-python@v5\n"); },
         ".github/workflows/ci.yml:9: Python: python -- - uses: actions/setup-python@v5\n", 7},
        {"the program named in a shell script", [](const Tree& t) { t.append("tools/ci.sh", "python3 tools/gen.sh\n"); }, "tools/ci.sh:4: Python: python3 -- python3 tools/gen.sh\n", 7},
        {"a Perl line in a shell script", [](const Tree& t) { t.append("tools/ci.sh", "perl -pe 's/a/b/' data.txt\n"); }, "tools/ci.sh:4: Perl: perl -- perl -pe 's/a/b/' data.txt\n", 7},
        {"a Perl file", [](const Tree& t) { t.put("tools/gen.pl", "print 1;\n"); }, "tools/gen.pl: Perl: file name refused (extension .pl)\n", 8},
        {"an extensionless file whose first line is a #! naming a script interpreter", [](const Tree& t) { t.put("tools/runme", "#!/usr/bin/env python3\nprint(1)\n"); },
         "tools/runme:1: Python: #! names python3 (only sh and bash are allowed) -- #!/usr/bin/env python3\n", 8},
    };
    REQUIRE(injections.size() == 7);
    // what the Report says of the one finding of each: its kind and its line (a name has no line; a first-line marker is on line 1)
    const std::vector<std::pair<std::string, std::size_t>> findings = {{"name", 0}, {"word", 4}, {"word", 9}, {"word", 4}, {"word", 4}, {"name", 0}, {"shebang", 1}};
    std::size_t index = 0;
    for (const Injection& inj : injections) {
        INFO("injection: " << inj.what);
        const Tree t;
        REQUIRE(t.run().code == 0);   // the tree passes before it
        inj.make(t);
        const Result r = t.run();
        CHECK(r.code == 1);
        CHECK(r.out.empty());
        CHECK(r.err == inj.expected + refused_line(1, 1));
        const ic::Report rep = t.report();
        CHECK(rep.violations.size() == 1);
        REQUIRE(!rep.violations.empty());
        CHECK(rep.violations.front().kind == findings.at(index).first);
        CHECK(rep.violations.front().line == findings.at(index).second);
        ++index;
        CHECK(rep.files_walked == inj.files);
        CHECK(rep.build_files == 5);
        CHECK(rep.stale.empty());
    }
}

TEST_CASE("each of the nine languages, through each of the three doors: a word in a shell script, a file named like one, and a #! line", "[interpretercheck][behaviour]") {
    struct Door {
        const char* language;
        const char* line;        // appended to tools/ci.sh
        const char* word;        // what the tool says it found there
        const char* file;
        const char* extension;   // as the tool says it
        const char* shebang;
        const char* interpreter;
    };
    const Door doors[] = {
        {"Python", "python3 -V", "python3", "x.py", ".py", "#!/usr/bin/env python3", "python3"},
        {"Perl", "perl -e 1", "perl", "x.pl", ".pl", "#!/usr/bin/perl", "perl"},
        {"Ruby", "ruby -e 1", "ruby", "x.rb", ".rb", "#!/usr/bin/env ruby", "ruby"},
        {"JavaScript", "node x", "node", "x.js", ".js", "#!/usr/bin/env node", "node"},
        {"PHP", "php x", "php", "x.php", ".php", "#!/usr/bin/php", "php"},
        {"Lua", "lua5.4 x", "lua5", "x.lua", ".lua", "#!/usr/bin/env lua5.4", "lua5.4"},
        {"Tcl", "tclsh x", "tclsh", "x.tcl", ".tcl", "#!/usr/bin/tclsh", "tclsh"},
        {"R", "Rscript x", "rscript", "x.R", ".r", "#!/usr/bin/env Rscript", "Rscript"},
        {"Julia", "julia x", "julia", "x.jl", ".jl", "#!/usr/bin/env julia", "julia"},
    };
    for (const Door& d : doors) {
        INFO("language: " << d.language);
        {
            const Tree t;
            t.append("tools/ci.sh", std::string(d.line) + "\n");
            const Result r = t.run();
            CHECK(r.code == 1);
            CHECK(r.out.empty());
            CHECK(r.err == std::string("tools/ci.sh:4: ") + d.language + ": " + d.word + " -- " + d.line + "\n" + refused_line(1, 1));
        }
        {
            const Tree t;
            t.put(std::string("modules/m/") + d.file, "x\n");
            const Result r = t.run();
            CHECK(r.code == 1);
            CHECK(r.out.empty());
            CHECK(r.err == std::string("modules/m/") + d.file + ": " + d.language + ": file name refused (extension " + d.extension + ")\n" + refused_line(1, 1));
        }
        {
            const Tree t;
            t.put("modules/m/run", std::string(d.shebang) + "\n");
            const Result r = t.run();
            CHECK(r.code == 1);
            CHECK(r.out.empty());
            CHECK(r.err == std::string("modules/m/run:1: ") + d.language + ": #! names " + d.interpreter + " (only sh and bash are allowed) -- " + d.shebang + "\n" + refused_line(1, 1));
        }
    }
}

TEST_CASE("each conventional name, alone, is refused with its language; each of the three directories is refused once and not entered", "[interpretercheck][behaviour]") {
    for (const auto& [name, language] : conventional_names()) {
        INFO("name: " << name);
        const Tree t;
        t.put("modules/m/" + name, "x\n");
        const Result r = t.run();
        CHECK(r.code == 1);
        CHECK(r.err == "modules/m/" + name + ": " + language + ": file name refused (name " + name + ")\n" + refused_line(1, 1));
    }
    for (const char* dir : {"__pycache__", "node_modules", "site-packages"}) {
        INFO("directory: " << dir);
        const Tree t;
        t.put(std::string("modules/m/") + dir + "/a.pyc", "x");          // would be a refusal of its own if the directory were entered
        t.put(std::string("modules/m/") + dir + "/deep/b.js", "x");
        t.put(std::string("modules/m/") + dir + "/tool", "#!/usr/bin/perl\n");
        const Result r = t.run();
        CHECK(r.code == 1);
        CHECK(r.out.empty());
        CHECK(r.err == std::string("modules/m/") + dir + ": " + (std::string(dir) == "node_modules" ? "JavaScript" : "Python") + ": directory refused (" + dir + ")\n" + refused_line(1, 1));
    }
}

// ============================================================================================================================================================================== #! lines at tree level

TEST_CASE("first lines: the ones that pass and the ones that do not, in files of any name and any depth", "[interpretercheck][behaviour]") {
    struct Line {
        std::string first;
        bool refused;
        const char* language;
        const char* what;
    };
    const std::vector<Line> lines = {
        {"#!/bin/sh", false, "", ""},
        {"#!/bin/bash", false, "", ""},
        {"#!/usr/bin/env sh", false, "", ""},
        {"#!/usr/bin/env bash", false, "", ""},
        {"#! /bin/sh", false, "", ""},
        {"#!/bin/sh -e", false, "", ""},
        {"#!/usr/bin/env -S bash -eu", false, "", ""},
        {"#!/bin/sh\r", false, "", ""},
        {"# !/usr/bin/perl", false, "", ""},        // not a #! line
        {" #!/usr/bin/perl", false, "", ""},
        {"//#!/usr/bin/perl", false, "", ""},
        {"#!/usr/bin/python3", true, "Python", "python3"},
        {"#!/usr/bin/env python", true, "Python", "python"},
        {"#!/usr/bin/perl -w", true, "Perl", "perl"},
        {"#!/usr/bin/env ruby", true, "Ruby", "ruby"},
        {"#!/usr/bin/env node", true, "JavaScript", "node"},
        {"#!/usr/bin/php", true, "PHP", "php"},
        {"#!/usr/bin/lua5.4", true, "Lua", "lua5.4"},
        {"#!/usr/bin/tclsh", true, "Tcl", "tclsh"},
        {"#!/usr/bin/Rscript", true, "R", "Rscript"},
        {"#!/usr/bin/env julia", true, "Julia", "julia"},
        {"#!/usr/bin/awk -f", true, "other", "awk"},
        {"#!/usr/bin/env awk", true, "other", "awk"},
        {"#!/bin/dash", true, "other", "dash"},
        {"#!/bin/zsh", true, "other", "zsh"},
        {"#!/bin/ksh", true, "other", "ksh"},
        {"#!/usr/bin/env", true, "other", "no interpreter"},
        {"#!", true, "other", "no interpreter"},
        {"#!   ", true, "other", "no interpreter"},
    };
    for (const Line& l : lines) {
        INFO("first line: " << l.first);
        for (const char* where : {"tools/x1", "modules/m/deep/er/x1", "x1.txt", "x1.cpp", "x1.dat"}) {
            for (const char* eol : {"\n", "\r\n"}) {   // with a Unix line end and with a DOS one: the carriage return is no part of the line
                INFO("in: " << where << (std::string(eol) == "\n" ? " (LF)" : " (CR LF)"));
                const Tree t;
                t.put(where, l.first + eol + "second line" + eol);
                const Result r = t.run();
                if (l.refused) {
                    // the line is shown as it stands, without the carriage return of a DOS line end
                    CHECK(r.code == 1);
                    CHECK(r.out.empty());
                    CHECK(r.err == std::string(where) + ":1: " + l.language + ": #! names " + l.what + " (only sh and bash are allowed) -- " + l.first + "\n" + refused_line(1, 1));
                } else {
                    CHECK(r.code == 0);
                    CHECK(r.err.empty());
                }
            }
        }
    }
}

TEST_CASE("first lines: not the first line, an empty file, a single byte, no newline at all, a very long first line, a binary file", "[interpretercheck][behaviour]") {
    const Tree t;
    t.put("a/second_line", "\n#!/usr/bin/perl\n");
    t.put("a/empty", "");
    t.put("a/one_byte", "#");
    t.put("a/hash_only_then_bang", "#\n!/usr/bin/perl\n");
    t.put("a/no_newline_sh", "#!/bin/sh");
    t.put("a/long_sh", "#!/bin/sh" + std::string(600, ' ') + "perl\n");
    t.put_bytes("a/binary_sh", std::string("#!/bin/sh\n") + std::string(1, '\0') + "\x01\x02perl\xFF");
    const Result r = t.run();
    CHECK(r.code == 0);
    CHECK(r.err.empty());
    CHECK(r.out == ok_line(14, 5, 1, 0));   // 7 of the baseline and these 7
    // a binary file that begins with a #! naming a script interpreter is refused; and so is one whose first line is only cut by the 512 bytes the tool reads
    const Tree u;
    u.put_bytes("a/binary_perl", std::string("#!/usr/bin/perl\n") + std::string(1, '\0') + "\x01\x02\xFF");
    u.put("a/long_first_line", "#!" + std::string(600, 'a') + "\n");
    u.put("a/two_bytes", "#!");   // the shortest file that is a #! line: it names no interpreter
    const Result s = u.run();
    CHECK(s.code == 1);
    CHECK(s.err == "a/binary_perl:1: Perl: #! names perl (only sh and bash are allowed) -- #!/usr/bin/perl\n"
                   "a/long_first_line:1: other: #! names " + std::string(510, 'a') + " (only sh and bash are allowed) -- #!" + std::string(198, 'a') + "\n"
                   "a/two_bytes:1: other: #! names no interpreter (only sh and bash are allowed) -- #!\n" + refused_line(3, 3));
}

TEST_CASE("a name is refused before the first line is looked at: one refusal for the file", "[interpretercheck][behaviour]") {
    const Tree t;
    t.put("tools/gen.py", "#!/usr/bin/env python3\nprint(1)\n");
    t.put("tools/also.sh", "#!/usr/bin/perl\n");
    const Result r = t.run();
    CHECK(r.code == 1);
    CHECK(r.err == "tools/also.sh:1: Perl: #! names perl (only sh and bash are allowed) -- #!/usr/bin/perl\n"
                   "tools/gen.py: Python: file name refused (extension .py)\n" + refused_line(2, 2));
}

// ============================================================================================================================================================================== the walk

TEST_CASE("the walk: what is not looked at -- the top-level data, the version-control and build trees, at any depth", "[interpretercheck][behaviour]") {
    const Tree t;
    for (const char* dir : {"data", "data/deep/er", ".git/hooks", "build", "build-ci", "build-repro-a", "build-repro-bbbbbbbb", "build-", "tools/build", "modules/m/build-x", "tools/CMakeFiles",
                            "CMakeFiles/3.28", "_deps/catch2-src", "modules/_deps", "modules/m/.git"}) {
        INFO("directory: " << dir);
        t.put(std::string(dir) + "/x.py", "x");
        t.put(std::string(dir) + "/y.pl", "x");
        t.put(std::string(dir) + "/script", "#!/usr/bin/env python3\n");
        t.put(std::string(dir) + "/CMakeLists.txt", "find_package(Python3 REQUIRED)\n");
        t.put(std::string(dir) + "/helper.cmake", "perl\n");
        t.put(std::string(dir) + "/__pycache__/z.pyc", "x");
    }
    const Result r = t.run();
    CHECK(r.code == 0);
    CHECK(r.err.empty());
    CHECK(r.out == ok_line(7, 5, 1, 0));   // none of that was counted
}

TEST_CASE("the walk: what looks like those and is looked at -- a data below the top, names that only begin with build, other cases", "[interpretercheck][behaviour]") {
    for (const char* dir : {"modules/data", "tools/data", "builder", "buildx", "build_ci", "xbuild", "Build", "BUILD", "Data", "DATA", ".github", ".gitx", "git", "CMakeFilesX", "cmakefiles", "_deps2", "deps", "_dep", "buil"}) {
        INFO("directory: " << dir);
        const Tree t;
        t.put(std::string(dir) + "/x.py", "x");
        const Result r = t.run();
        CHECK(r.code == 1);
        CHECK(r.err == std::string(dir) + "/x.py: Python: file name refused (extension .py)\n" + refused_line(1, 1));
    }
    // a FILE called data at the top, and files named like the skipped directories, are files like any other
    const Tree u;
    u.put("data", "#!/usr/bin/perl\n");
    CHECK(u.run().code == 1);
    const Tree v;
    v.put("build", "#!/usr/bin/perl\n");
    CHECK(v.run().code == 1);
}

TEST_CASE("the walk: a link to a directory is not entered, a link to a file is that file, a link to nothing is nothing", "[interpretercheck][behaviour]") {
    const Tree t;
    fs::create_directories(t.base() / "outside");
    write_text(t.base() / "outside" / "x.py", "x");
    write_text(t.base() / "outside" / "script", "#!/usr/bin/perl\n");
    write_text(t.base() / "outside" / "clean.txt", "nothing\n");
    fs::create_directory_symlink("../outside", t.root() / "lnk");
    fs::create_directory_symlink("../../outside", t.root() / "tools" / "lnk2");
    fs::create_symlink("../nowhere", t.root() / "dangling.py");
    const Result r = t.run();
    CHECK(r.code == 0);          // not entered, and a dangling link is no file
    CHECK(r.out == ok_line(7, 5, 1, 0));
    // a link to a file with a script's #! is refused through the link, under the link's name; a link named like a script is refused by name though it leads to a clean file
    fs::create_symlink("../../outside/script", t.root() / "tools" / "runme");
    fs::create_symlink("../outside/clean.txt", t.root() / "fake.py");
    const Result s = t.run();
    CHECK(s.code == 1);
    CHECK(s.err == "fake.py: Python: file name refused (extension .py)\n"
                   "tools/runme:1: Perl: #! names perl (only sh and bash are allowed) -- #!/usr/bin/perl\n" + refused_line(2, 2));
    // a link to a build file is read through the link
    const Tree u;
    write_text(u.base() / "outside.cmake", "perl\n");
    fs::create_symlink("../outside.cmake", u.root() / "linked.cmake");
    const Result v = u.run();
    CHECK(v.code == 1);
    CHECK(v.err == "linked.cmake:1: Perl: perl -- perl\n" + refused_line(1, 1));
}

TEST_CASE("the walk: files are counted once each, depth does not matter, and the order of the findings is by file and then by line", "[interpretercheck][behaviour]") {
    const Tree t;
    t.put("tools/b.pl", "x");
    t.put("tools/a.py", "x");
    t.append("CMakeLists.txt", "# a comment with python3\nset(X 1)\nperl\n");
    t.append("cmake/Helper.cmake", "ruby\n");
    t.append_workflow("ci.yml", "      - run: node x\n");
    t.put("modules/a/b/c/d/e/deep.js", "x");
    const Result r = t.run();
    CHECK(r.code == 1);
    // '.' < 'C' < 'c' < 'm' < 't' in the byte order of the strings; within a file, by line
    CHECK(r.err == ".github/workflows/ci.yml:9: JavaScript: node -- - run: node x\n"
                   "CMakeLists.txt:4: Python: python3 -- # a comment with python3\n"
                   "CMakeLists.txt:6: Perl: perl -- perl\n"
                   "cmake/Helper.cmake:3: Ruby: ruby -- ruby\n"
                   "modules/a/b/c/d/e/deep.js: JavaScript: file name refused (extension .js)\n"
                   "tools/a.py: Python: file name refused (extension .py)\n"
                   "tools/b.pl: Perl: file name refused (extension .pl)\n" + refused_line(7, 6));
    CHECK(r.out.empty());
}

TEST_CASE("the walk: a directory that cannot be read and a file that cannot be opened are exit 2 with the path and the reason", "[interpretercheck][behaviour]") {
    if (::geteuid() == 0) SKIP("root opens every file");
    {
        const Tree t;
        t.put("modules/locked/x.txt", "x");
        REQUIRE(::chmod((t.root() / "modules/locked").c_str(), 0) == 0);
        const Result r = t.run();
        REQUIRE(::chmod((t.root() / "modules/locked").c_str(), 0755) == 0);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err.rfind("interpretercheck: cannot read the directory " + (t.root() / "modules/locked").string() + ": ", 0) == 0);
        CHECK(r.err.find("Permission denied") != std::string::npos);
    }
    {
        const Tree t;
        t.put("tools/locked", "#!/bin/sh\n");
        REQUIRE(::chmod((t.root() / "tools/locked").c_str(), 0) == 0);
        const Result r = t.run();
        REQUIRE(::chmod((t.root() / "tools/locked").c_str(), 0644) == 0);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == "interpretercheck: cannot open " + (t.root() / "tools/locked").string() + "\n");
    }
    {
        const Tree t;
        t.put("tools/locked.sh", "echo\n");
        REQUIRE(::chmod((t.root() / "tools/locked.sh").c_str(), 0) == 0);
        const Result r = t.run();
        REQUIRE(::chmod((t.root() / "tools/locked.sh").c_str(), 0644) == 0);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err.rfind("interpretercheck: ", 0) == 0);
        CHECK(r.err.find((t.root() / "tools/locked.sh").string()) != std::string::npos);
    }
    // Two that cannot be read: the one said is the first in the order of the NAMES, whatever order the directory lists them in (a file system lists them as it likes; the tool sorts them).
    {
        const Tree t;
        t.put("tools/a_locked", "#!/bin/sh\n");
        t.put("tools/z_locked", "#!/bin/sh\n");
        REQUIRE(::chmod((t.root() / "tools/a_locked").c_str(), 0) == 0);
        REQUIRE(::chmod((t.root() / "tools/z_locked").c_str(), 0) == 0);
        const Result r = t.run();
        REQUIRE(::chmod((t.root() / "tools/a_locked").c_str(), 0644) == 0);
        REQUIRE(::chmod((t.root() / "tools/z_locked").c_str(), 0644) == 0);
        CHECK(r.code == 2);
        CHECK(r.err == "interpretercheck: cannot open " + (t.root() / "tools/a_locked").string() + "\n");
    }
    {
        const Tree t;
        t.put("modules/a_dir/x", "x");
        t.put("modules/z_dir/x", "x");
        REQUIRE(::chmod((t.root() / "modules/a_dir").c_str(), 0) == 0);
        REQUIRE(::chmod((t.root() / "modules/z_dir").c_str(), 0) == 0);
        const Result r = t.run();
        REQUIRE(::chmod((t.root() / "modules/a_dir").c_str(), 0755) == 0);
        REQUIRE(::chmod((t.root() / "modules/z_dir").c_str(), 0755) == 0);
        CHECK(r.code == 2);
        CHECK(r.err.rfind("interpretercheck: cannot read the directory " + (t.root() / "modules/a_dir").string() + ": ", 0) == 0);
    }
    {
        const Tree t;
        t.put_workflow("a.yml", "name: a\n");
        t.put_workflow("z.yml", "name: z\n");
        REQUIRE(::chmod((t.workflows() / "a.yml").c_str(), 0) == 0);
        REQUIRE(::chmod((t.workflows() / "z.yml").c_str(), 0) == 0);
        const Result r = t.run();
        REQUIRE(::chmod((t.workflows() / "a.yml").c_str(), 0644) == 0);
        REQUIRE(::chmod((t.workflows() / "z.yml").c_str(), 0644) == 0);
        CHECK(r.code == 2);
        CHECK(r.err.rfind("interpretercheck: ", 0) == 0);
        CHECK(r.err.find((t.workflows() / "a.yml").filename().string()) != std::string::npos);
        CHECK(r.err.find("z.yml") == std::string::npos);
    }
}

// ============================================================================================================================================================================== the token scan

TEST_CASE("the token scan reads CMakeLists.txt and *.cmake at any depth and tools/*.sh -- and no other file", "[interpretercheck][behaviour]") {
    const std::vector<std::pair<std::string, bool>> places = {
        {"CMakeLists.txt", true},         {"a/b/c/CMakeLists.txt", true}, {"tools/CMakeLists.txt", true}, {"cmake/X.cmake", true},      {"a/b/c/deep.cmake", true},  {"FindFoo.cmake", true},
        {".cmake", true},                 {"tools/x.sh", true},           {"tools/ci.sh", true},          {"tools/.hidden.sh", true},    {".sh", false},              {"tools/sub/x.sh", false},
        {"modules/x.sh", false},          {"x.sh", false},                {"README.md", false},           {"tools/x.cpp", false},        {"tools/x.txt", false},      {"tools/x.SH", false},
        {"CMakeLists.txt.in", false},     {"cmakelists.txt", false},      {"CMakeLists.cmake.in", false}, {"x.cmake.in", false},         {"tools/sh", false},         {"tools/x.sh.bak", false},
        {"modules/m/foo.cpp", false},     {"doc/guide.md", false},        {"Makefile", false},            {"tools/Makefile", false},     {"tests/CMakeLists.txt", true},
        {"xcmake", false},                {"Findcmake", false},          {"xCMakeLists.txt", false},    {"tools/xsh", false},         {"tools/.sh.txt", false},
    };
    for (const auto& [place, is_read] : places) {
        INFO("place: " << place << (is_read ? " (read)" : " (not read)"));
        const Tree t;
        t.put(place, "run python3 here\n");
        const Result r = t.run();
        if (is_read) {
            CHECK(r.code == 1);
            CHECK(r.err == place + ":1: Python: python3 -- run python3 here\n" + refused_line(1, 1));
        } else {
            CHECK(r.code == 0);
            CHECK(r.err.empty());
        }
    }
}

TEST_CASE("the token scan: comments are read like code, case does not matter, every flagged line is its own finding", "[interpretercheck][behaviour]") {
    const Tree t;
    t.append("CMakeLists.txt",
             "# python3 in a comment\n"
             "set(a 1) # trailing PERL comment\n"
             "set(b 2)\n"
             "if(ENABLE_PYTHON)\n"
             "endif()\n"
             "set(c ${Python3_EXECUTABLE})\n");
    const Result r = t.run();
    CHECK(r.code == 1);
    CHECK(r.err == "CMakeLists.txt:4: Python: python3 -- # python3 in a comment\n"
                   "CMakeLists.txt:5: Perl: perl -- set(a 1) # trailing PERL comment\n"
                   "CMakeLists.txt:7: Python: python -- if(ENABLE_PYTHON)\n"
                   "CMakeLists.txt:9: Python: python3 -- set(c ${Python3_EXECUTABLE})\n" + refused_line(4, 1));
}

TEST_CASE("the token scan: one finding per line with every word of it and every language of them, in the order of the line", "[interpretercheck][behaviour]") {
    const Tree t;
    t.append("CMakeLists.txt", "python3 -m pip install foo.py && perl x\n");
    const Result r = t.run();
    CHECK(r.code == 1);
    CHECK(r.err == "CMakeLists.txt:4: Python, Perl: python3, pip, .py, perl -- python3 -m pip install foo.py && perl x\n" + refused_line(1, 1));
    const ic::Report rep = t.report();
    REQUIRE(rep.violations.size() == 1);
    CHECK(rep.violations.front().line == 4);
    CHECK(rep.violations.front().kind == "word");
    CHECK(rep.violations.front().language == "Python, Perl");
    // the kind of a line is the kind of its FIRST flagged word: an extension first, a program last
    const Tree u;
    u.append("CMakeLists.txt", "foo.py x.js python3\n");
    const Result s = u.run();
    CHECK(s.code == 1);
    CHECK(s.err == "CMakeLists.txt:4: Python, JavaScript: .py, .js, python3 -- foo.py x.js python3\n" + refused_line(1, 1));
    const ic::Report rep2 = u.report();
    REQUIRE(rep2.violations.size() == 1);
    CHECK(rep2.violations.front().kind == "extension");
    CHECK(rep2.violations.front().what == ".py, .js, python3");
}

TEST_CASE("the token scan: line numbers, CR LF line ends, a last line without a line end, an empty file, bytes that are no text, and long lines", "[interpretercheck][behaviour]") {
    {
        const Tree t;
        t.put("a.cmake", "x\r\n\r\npython3\r\n");
        const ic::Report rep = t.report();
        REQUIRE(rep.violations.size() == 1);
        CHECK(rep.violations.front().file == "a.cmake");
        CHECK(rep.violations.front().line == 3);
        CHECK(rep.violations.front().text == "python3");   // no carriage return
    }
    {
        const Tree t;
        t.put("a.cmake", "x\nperl");
        const ic::Report rep = t.report();
        REQUIRE(rep.violations.size() == 1);
        CHECK(rep.violations.front().line == 2);
    }
    {
        const Tree t;
        t.put("a.cmake", "");
        t.put("b.cmake", "\n\n\n");
        t.put("c.cmake", "\n");
        const Result r = t.run();
        CHECK(r.code == 0);
        CHECK(r.out == ok_line(10, 8, 1, 0));
    }
    {
        const Tree t;
        t.put_bytes("a.cmake", std::string("\xFF\xFE", 2) + "perl" + std::string(1, '\0') + "x\n");
        const ic::Report rep = t.report();
        REQUIRE(rep.violations.size() == 1);
        CHECK(rep.violations.front().line == 1);
        CHECK(rep.violations.front().language == "Perl");
    }
    {
        // the line is shown from its first non-blank character, and at most 200 characters of it
        const Tree t;
        t.put("a.cmake", "\t  python3 " + std::string(300, 'x') + "\n");
        const ic::Report rep = t.report();
        REQUIRE(rep.violations.size() == 1);
        CHECK(rep.violations.front().text == "python3 " + std::string(192, 'x'));
        CHECK(rep.violations.front().text.size() == 200);
    }
    {
        const Tree t;
        t.put("a.cmake", "   perl   \n");
        const ic::Report rep = t.report();
        REQUIRE(rep.violations.size() == 1);
        CHECK(rep.violations.front().text == "perl   ");   // the trailing blanks stay
    }
}

TEST_CASE("the workflows: every regular file directly in the directory, whatever its name; none below it; the default place and --workflows", "[interpretercheck][behaviour]") {
    {
        const Tree t;
        t.put_workflow("other.yaml", "run: python3 x\n");
        t.put_workflow("notes.txt", "x\n");
        t.put_workflow("noextension", "ruby\n");
        t.put_workflow("sub/nested.yml", "perl\n");   // below the directory: not read
        const Result r = t.run();
        CHECK(r.code == 1);
        CHECK(r.err == ".github/workflows/noextension:1: Ruby: ruby -- ruby\n"
                       ".github/workflows/other.yaml:1: Python: python3 -- run: python3 x\n" + refused_line(2, 2));
        CHECK(t.report().workflow_files == 4);
    }
    {
        // --workflows names another directory: the default one is then not read, and the files are labelled by the directory as given
        const Tree t;
        t.append_workflow("ci.yml", "      - run: python3 x\n");
        fs::create_directories(t.base() / "elsewhere");
        write_text(t.base() / "elsewhere" / "one.yml", "name: x\n");
        const Result clean = t.run({"--workflows", (t.base() / "elsewhere").string()});
        CHECK(clean.code == 0);
        CHECK(clean.out == ok_line(7, 5, 1, 0));
        write_text(t.base() / "elsewhere" / "two.yml", "x\nnode\n");
        const Result dirty = t.run({"--workflows=" + (t.base() / "elsewhere").string()});
        CHECK(dirty.code == 1);
        CHECK(dirty.err == (t.base() / "elsewhere").generic_string() + "/two.yml:2: JavaScript: node -- node\n" + refused_line(1, 1));
    }
    {
        // a directory called like a workflow file is not a file
        const Tree t;
        fs::create_directories(t.workflows() / "dir.yml");
        CHECK(t.report().workflow_files == 1);
    }
}

TEST_CASE("the workflows: no workflows directory beside the tree is said, not an error; an explicit one that is missing is exit 2", "[interpretercheck][behaviour]") {
    const Tree t;
    fs::remove_all(t.base() / ".github");
    const Result r = t.run();
    CHECK(r.code == 0);
    CHECK(r.out == "note     no workflows directory at " + (t.base() / ".github" / "workflows").string() + ": the files of the workflows were NOT read\n" + ok_line(7, 5, 0, 0));
    CHECK(r.err.empty());
    const ic::Report rep = t.report();
    CHECK_FALSE(rep.workflows_found);
    CHECK(rep.workflow_files == 0);
    // --quiet says it all the same
    CHECK(t.run({"--quiet"}).out == r.out);
    // refused things are refused whether or not the workflows were found
    t.put("tools/x.py", "x");
    CHECK(t.run().code == 1);
    // an explicit directory is required
    const Result missing = t.run({"--workflows", (t.base() / "none").string()});
    CHECK(missing.code == 2);
    CHECK(missing.out.empty());
    CHECK(missing.err == "interpretercheck: --workflows: " + (t.base() / "none").string() + " is not a directory\n");
    // a file is no directory
    const Result file = t.run({"--workflows", (t.root() / "README.md").string()});
    CHECK(file.code == 2);
    CHECK(file.err == "interpretercheck: --workflows: " + (t.root() / "README.md").string() + " is not a directory\n");
}

// ============================================================================================================================================================================== the allowlist

TEST_CASE("the allowlist: an exact line of an exact file is accepted, is said (not with --quiet), and counts", "[interpretercheck][behaviour]") {
    const Tree t;
    t.append("CMakeLists.txt", "find_package(Python3 REQUIRED)\n");
    ic::Settings s = t.settings();
    s.allowlist = {{"CMakeLists.txt", "find_package(Python3 REQUIRED)", "the guard names it in order to refuse it"}};
    const Result r = run_with(s, {});
    CHECK(r.code == 0);
    CHECK(r.out == ok_line(7, 5, 1, 1) + "allowed  CMakeLists.txt:4  the guard names it in order to refuse it\n");
    CHECK(r.err.empty());
    const Result q = run_with(s, {"--quiet"});
    CHECK(q.code == 0);
    CHECK(q.out == ok_line(7, 5, 1, 1));
    const ic::Report rep = ic::check(s);
    REQUIRE(rep.accepted.size() == 1);
    CHECK(rep.accepted.front().file == "CMakeLists.txt");
    CHECK(rep.accepted.front().line == 4);
    CHECK(rep.accepted.front().reason == "the guard names it in order to refuse it");
    CHECK(rep.violations.empty());
    CHECK(rep.stale.empty());
    // without the allowlist it is the refusal of the line, and no entry is stale
    const Result n = run_with(s, {"--no-allowlist"});
    CHECK(n.code == 1);
    CHECK(n.err == "CMakeLists.txt:4: Python: python3 -- find_package(Python3 REQUIRED)\n" + refused_line(1, 1));
}

TEST_CASE("the allowlist: the text must be the whole line, as it stands -- anything else is a refusal of the line and a stale entry", "[interpretercheck][behaviour]") {
    const std::string reason = "because";
    struct Variant {
        const char* what;
        std::string file;
        std::string text;
    };
    const std::vector<Variant> variants = {
        {"another file", "tools/CMakeLists.txt", "find_package(Python3 REQUIRED)"},
        {"a trailing blank", "CMakeLists.txt", "find_package(Python3 REQUIRED) "},
        {"a missing last character", "CMakeLists.txt", "find_package(Python3 REQUIRED"},
        {"a missing first character", "CMakeLists.txt", "ind_package(Python3 REQUIRED)"},
        {"another case", "CMakeLists.txt", "find_package(python3 REQUIRED)"},
        {"a leading blank the line does not have", "CMakeLists.txt", " find_package(Python3 REQUIRED)"},
        {"a part of it", "CMakeLists.txt", "Python3"},
        {"empty", "CMakeLists.txt", ""},
        {"the line and more", "CMakeLists.txt", "find_package(Python3 REQUIRED)\n"},
        {"a pattern", "CMakeLists.txt", "find_package(.*)"},
        {"a file that is not there", "nothing/CMakeLists.txt", "find_package(Python3 REQUIRED)"},
    };
    for (const Variant& v : variants) {
        INFO("variant: " << v.what);
        const Tree t;
        t.append("CMakeLists.txt", "find_package(Python3 REQUIRED)\n");
        ic::Settings s = t.settings();
        s.allowlist = {{v.file, v.text, reason}};
        const Result r = run_with(s, {});
        CHECK(r.code == 1);
        CHECK(r.out.empty());
        CHECK(r.err == "CMakeLists.txt:4: Python: python3 -- find_package(Python3 REQUIRED)\n"
                       "interpretercheck: stale allowlist entry, it accepts no line: " + v.file + ": " + v.text + " (" + reason + ")\n" + refused_and_stale_line(1, 1, 1));
    }
    // the line with leading blanks IS accepted by an entry with the blanks, and a line with a carriage return by an entry without it
    const Tree u;
    u.append("CMakeLists.txt", "  find_package(Python3 REQUIRED)\r\n");
    ic::Settings s = u.settings();
    s.allowlist = {{"CMakeLists.txt", "  find_package(Python3 REQUIRED)", reason}};
    const Result r = run_with(s, {"--quiet"});
    CHECK(r.code == 0);
    CHECK(r.out == ok_line(7, 5, 1, 1));
}

TEST_CASE("the allowlist: an entry that accepts nothing is itself refused, whatever else the tree is; two entries, one used", "[interpretercheck][behaviour]") {
    const Tree t;
    ic::Settings s = t.settings();
    s.allowlist = {{"CMakeLists.txt", "find_package(Python3 REQUIRED)", "gone"}};
    const Result r = run_with(s, {});
    CHECK(r.code == 1);
    CHECK(r.out.empty());
    CHECK(r.err == "interpretercheck: stale allowlist entry, it accepts no line: CMakeLists.txt: find_package(Python3 REQUIRED) (gone)\n"
                   "interpretercheck: REFUSED: 1 stale allowlist entry -- an allowlist entry that accepts no line is a defect of the check itself\n");
    // two stale entries
    s.allowlist.push_back({"tools/ci.sh", "perl", "also gone"});
    const Result two = run_with(s, {});
    CHECK(two.code == 1);
    CHECK(two.err == "interpretercheck: stale allowlist entry, it accepts no line: CMakeLists.txt: find_package(Python3 REQUIRED) (gone)\n"
                     "interpretercheck: stale allowlist entry, it accepts no line: tools/ci.sh: perl (also gone)\n"
                     "interpretercheck: REFUSED: 2 stale allowlist entries -- an allowlist entry that accepts no line is a defect of the check itself\n");
    // one used, one stale
    t.append("tools/ci.sh", "perl\n");
    const Result one = run_with(s, {});
    CHECK(one.code == 1);
    CHECK(one.err == "interpretercheck: stale allowlist entry, it accepts no line: CMakeLists.txt: find_package(Python3 REQUIRED) (gone)\n"
                     "interpretercheck: REFUSED: 1 stale allowlist entry -- an allowlist entry that accepts no line is a defect of the check itself\n");
    CHECK(ic::check(s).accepted.size() == 1);
}

TEST_CASE("the allowlist: two identical entries -- the first takes the line, the second accepts nothing and is stale", "[interpretercheck][behaviour]") {
    const Tree t;
    t.append("CMakeLists.txt", "perl\n");
    ic::Settings s = t.settings();
    s.allowlist = {{"CMakeLists.txt", "perl", "first"}, {"CMakeLists.txt", "perl", "second"}};
    const Result r = run_with(s, {});
    CHECK(r.code == 1);
    CHECK(r.err == "interpretercheck: stale allowlist entry, it accepts no line: CMakeLists.txt: perl (second)\n"
                   "interpretercheck: REFUSED: 1 stale allowlist entry -- an allowlist entry that accepts no line is a defect of the check itself\n");
    const ic::Report rep = ic::check(s);
    REQUIRE(rep.accepted.size() == 1);
    CHECK(rep.accepted.front().reason == "first");
    REQUIRE(rep.stale.size() == 1);
    CHECK(rep.stale.front().reason == "second");
}

TEST_CASE("the allowlist: a second line of another file does not use up an entry of the first", "[interpretercheck][behaviour]") {
    const Tree t;
    t.append("CMakeLists.txt", "perl\n");
    t.append("cmake/Helper.cmake", "perl\n");
    ic::Settings s = t.settings();
    s.allowlist = {{"cmake/Helper.cmake", "perl", "the helper's"}};
    const Result r = run_with(s, {});
    CHECK(r.code == 1);
    CHECK(r.err == "CMakeLists.txt:4: Perl: perl -- perl\n" + refused_line(1, 1));
    const ic::Report rep = ic::check(s);
    REQUIRE(rep.accepted.size() == 1);
    CHECK(rep.accepted.front().file == "cmake/Helper.cmake");
    CHECK(rep.accepted.front().line == 3);
    CHECK(rep.stale.empty());
}

TEST_CASE("the allowlist: the same line twice in a file is accepted twice by the one entry; an entry in a workflow file", "[interpretercheck][behaviour]") {
    const Tree t;
    t.append("CMakeLists.txt", "perl\nset(x 1)\nperl\n");
    t.append_workflow("ci.yml", "      - run: node x\n");
    ic::Settings s = t.settings();
    s.allowlist = {{"CMakeLists.txt", "perl", "twice"}, {".github/workflows/ci.yml", "      - run: node x", "the runner's own"}};
    const Result r = run_with(s, {});
    CHECK(r.code == 0);
    CHECK(r.out == ok_line(7, 5, 1, 3) + "allowed  .github/workflows/ci.yml:9  the runner's own\nallowed  CMakeLists.txt:4  twice\nallowed  CMakeLists.txt:6  twice\n");
    CHECK(r.err.empty());
}

TEST_CASE("the allowlist excuses a line, never a name or a #! line", "[interpretercheck][behaviour]") {
    const Tree t;
    t.put("tools/helper.py", "print(1)\n");
    t.put("tools/runme", "#!/usr/bin/perl\n");
    ic::Settings s = t.settings();
    s.allowlist = {{"tools/helper.py", "print(1)", "no"}, {"tools/runme", "#!/usr/bin/perl", "no"}};
    const Result r = run_with(s, {});
    CHECK(r.code == 1);
    CHECK(r.err == "tools/helper.py: Python: file name refused (extension .py)\n"
                   "tools/runme:1: Perl: #! names perl (only sh and bash are allowed) -- #!/usr/bin/perl\n"
                   "interpretercheck: stale allowlist entry, it accepts no line: tools/helper.py: print(1) (no)\n"
                   "interpretercheck: stale allowlist entry, it accepts no line: tools/runme: #!/usr/bin/perl (no)\n"
                   "interpretercheck: REFUSED: 2 refusal(s) in 2 file(s), 2 stale allowlist entries -- no script interpreter may reach the build, the tests or CI (plan section 5 constraint 11)\n");
}

// ============================================================================================================================================================================== the command line

TEST_CASE("run_on: -h whole, the refusals, --root and --workflows in both forms, --quiet and --no-allowlist", "[interpretercheck][behaviour]") {
    const char help[] =
        "\n"
        "Refuse every script interpreter in the build, the tests and CI (plan section 5 constraint 11; ci.sh's gate 14): Python, Perl, Ruby, JavaScript, PHP, Lua, Tcl, R and Julia.  It refuses, below the tree and outside\n"
        "data/ and the build trees, a file named like a program of one of them (by extension or by its conventional name) and any file whose first line is a #! naming anything but sh or bash; and, in every CMakeLists.txt\n"
        "and *.cmake, every tools/*.sh and every file of the workflows directory, a program or package tool of one of them as a whole word, or a file extension of one.  A line that is a false positive is accepted only by an\n"
        "exact-line entry of the tool's allowlist, with its reason; an entry that accepts nothing is refused.\n"
        "\n"
        "options:\n"
        "  -h, --help             show this help and exit\n"
        "  --quiet                print the counts and the verdict, not each accepted line\n"
        "  --no-allowlist         use no allowlist (the allowlist is this tree's: on any other tree its entries accept nothing and are refused)\n"
        "  --root ROOT            the tree to check (default: the tree this tool was built from)\n"
        "  --workflows WORKFLOWS  the workflows directory (default: ROOT/../.github/workflows; if that is absent the files of the workflows are not read, and the tool says so)\n"
        "\n"
        "exit codes: 0 no script interpreter   1 refused (a violation, or a stale allowlist entry)   2 an argument error, or something that cannot be read   70 an error the tool did not anticipate\n";
    const Tree t;
    for (const char* flag : {"-h", "--help"}) {
        const Result r = t.run({flag});
        CHECK(r.code == 0);
        CHECK(r.out == std::string(kUsage) + help);
        CHECK(r.err.empty());
    }
    CHECK(t.run({"-h", "--bogus"}).code == 0);   // -h wins over what follows it
    CHECK(t.run({"--bogus", "-h"}).code == 2);   // an earlier bad one is met first
    CHECK(t.run({"--quiet", "-h"}).code == 0);

    const auto refused = [&](const std::vector<std::string>& args, const std::string& what) {
        INFO("arguments: " << (args.empty() ? std::string("(none)") : args.front()));
        const Result r = t.run(args);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == std::string(kUsage) + "interpretercheck: error: " + what + "\n");
    };
    refused({"--bogus"}, "unrecognized arguments: --bogus");
    refused({"extra"}, "unrecognized arguments: extra");
    refused({"--quiet=1"}, "unrecognized arguments: --quiet=1");
    refused({"--no-allowlist=1"}, "unrecognized arguments: --no-allowlist=1");
    refused({"--quie"}, "unrecognized arguments: --quie");   // exact names only
    refused({"-q"}, "unrecognized arguments: -q");
    refused({"--root"}, "argument --root: expected one argument");
    refused({"--root", "--quiet"}, "argument --root: expected one argument");
    refused({"--workflows"}, "argument --workflows: expected one argument");
    refused({"--workflows", "--root", "x"}, "argument --workflows: expected one argument");
    refused({"--root", "---x"}, "argument --root: expected one argument");   // anything that begins with two hyphens is taken for the next option, three included
    refused({"--root", ""}, "argument --root: the path is empty");
    refused({"--root="}, "argument --root: the path is empty");
    refused({"--workflows", ""}, "argument --workflows: the path is empty");
    refused({"--workflows="}, "argument --workflows: the path is empty");
    refused({"--quiet", "--bogus", "--root"}, "unrecognized arguments: --bogus");   // the first fault met is the one said

    // the options in both forms and in any order
    const Tree g;
    ic::Settings elsewhere;
    elsewhere.root = g.base() / "nowhere";   // replaced by --root
    for (const std::vector<std::string>& args : std::vector<std::vector<std::string>>{{"--root", g.root().string()},
                                                                                       {"--root=" + g.root().string()},
                                                                                       {"--quiet", "--root", g.root().string(), "--quiet", "--no-allowlist"},
                                                                                       {"--root", g.root().string(), "--workflows", g.workflows().string()},
                                                                                       {"--workflows=" + g.workflows().string(), "--root=" + g.root().string()}}) {
        INFO("arguments: " << args.front());
        const Result r = run_with(elsewhere, args);
        CHECK(r.code == 0);
        CHECK(r.out == ok_line(7, 5, 1, 0));
        CHECK(r.err.empty());
    }
    // the root and the settings' workflows: the later --root wins
    const Result later = run_with(elsewhere, {"--root", (g.base() / "also_nowhere").string(), "--root", g.root().string()});
    CHECK(later.code == 0);
}

TEST_CASE("run_on: a root that is not a directory is exit 2 and says so; a relative root works", "[interpretercheck][behaviour]") {
    const Tree t;
    const Result missing = t.run({"--root", (t.base() / "none").string()});
    CHECK(missing.code == 2);
    CHECK(missing.out.empty());
    CHECK(missing.err == "interpretercheck: the tree " + (t.base() / "none").string() + " is not a directory\n");
    const Result file = t.run({"--root", (t.root() / "README.md").string()});
    CHECK(file.code == 2);
    CHECK(file.err == "interpretercheck: the tree " + (t.root() / "README.md").string() + " is not a directory\n");
    // a value that begins with ONE hyphen is a value (a directory may be called -x); only two hyphens make the next word an option
    const Result dash = t.run({"--root", "-x"});
    CHECK(dash.code == 2);
    CHECK(dash.err == "interpretercheck: the tree -x is not a directory\n");
    const Result dash_workflows = t.run({"--workflows", "-x"});
    CHECK(dash_workflows.code == 2);
    CHECK(dash_workflows.err == "interpretercheck: --workflows: -x is not a directory\n");
    // a relative root, the workflows then being looked for beside it
    const fs::path here = fs::current_path();
    fs::current_path(t.base());
    const Result rel = t.run({"--root", "rewrite"});
    fs::current_path(here);
    CHECK(rel.code == 0);
    CHECK(rel.out == ok_line(7, 5, 1, 0));
    // a root with a trailing slash
    const Result slash = t.run({"--root", t.root().string() + "/"});
    CHECK(slash.code == 0);
    CHECK(slash.out == ok_line(7, 5, 1, 0));
}

TEST_CASE("run_on: the tool writes nothing -- the tree is the same after a refusal as before", "[interpretercheck][behaviour]") {
    const Tree t;
    t.put("tools/gen.py", "print(1)\n");
    t.append("tools/ci.sh", "perl -e 1\n");
    const auto snapshot = [&]() {
        std::map<std::string, std::string> files;
        for (const fs::path& base : {t.root(), t.workflows()}) {
            for (const fs::directory_entry& e : fs::recursive_directory_iterator(base)) {
                if (e.is_regular_file()) files[e.path().string()] = read_text(e.path());
            }
        }
        return files;
    };
    const auto before = snapshot();
    CHECK(t.run().code == 1);
    CHECK(t.run({"--quiet"}).code == 1);
    CHECK(snapshot() == before);
}

TEST_CASE("run: the default settings are the tree the tool was built from, the allowlist is the real one, and --no-allowlist drops it", "[interpretercheck][behaviour]") {
#ifdef ODL_TREE_ROOT
    CHECK(ic::default_root() == fs::path(ODL_TREE_ROOT));
    const ic::Settings s = ic::default_settings();
    CHECK(s.root == fs::path(ODL_TREE_ROOT));
    CHECK(s.workflows.empty());
    CHECK_FALSE(s.workflows_explicit);
    REQUIRE(s.allowlist.size() == ic::real_allowlist().size());
    CHECK(s.allowlist.size() == 3);
    for (std::size_t i = 0; i < s.allowlist.size(); ++i) {
        CHECK(s.allowlist.at(i).file == ic::real_allowlist().at(i).file);
        CHECK(s.allowlist.at(i).text == ic::real_allowlist().at(i).text);
        CHECK_FALSE(s.allowlist.at(i).reason.empty());
    }
#else
    SKIP("the tree this tool was built from is not known");
#endif
}

TEST_CASE("run: on a tree that is not the real one the real allowlist is refused as stale -- which is what --no-allowlist is for", "[interpretercheck][behaviour]") {
    const Tree t;
    std::ostringstream out;
    std::ostringstream err;
    const int stale = ic::run({"--root", t.root().string()}, Streams{out, err});
    CHECK(stale == 1);
    CHECK(out.str().empty());
    CHECK(err.str().find("interpretercheck: stale allowlist entry, it accepts no line: CMakeLists.txt: ") != std::string::npos);
    CHECK(err.str().find("interpretercheck: REFUSED: 3 stale allowlist entries -- ") != std::string::npos);
    std::ostringstream out2;
    std::ostringstream err2;
    const int clean = ic::run({"--root", t.root().string(), "--no-allowlist"}, Streams{out2, err2});
    CHECK(clean == 0);
    CHECK(out2.str() == ok_line(7, 5, 1, 0));
    CHECK(err2.str().empty());
}

// ============================================================================================================================================================================== the real tree, against a second method

namespace {

// ---- a second method for the walk, written apart from the tool: POSIX readdir, lstat and stat

struct SecondWalk {
    std::vector<std::string> files;         // relative paths, '/' separated
    std::vector<std::string> directories;   // the names of the directories entered
};

bool skipped_second(const std::string& relative, const std::string& name) {
    if (relative == "data") return true;
    if (name == ".git" || name == "build" || name == "CMakeFiles" || name == "_deps") return true;
    return name.compare(0, 6, "build-") == 0;
}

void walk_second(const std::string& dir, const std::string& relative, SecondWalk& out) {
    DIR* d = ::opendir(dir.c_str());
    REQUIRE(d != nullptr);
    std::vector<std::string> names;
    while (const dirent* e = ::readdir(d)) {
        const std::string n = e->d_name;
        if (n != "." && n != "..") names.push_back(n);
    }
    ::closedir(d);
    std::sort(names.begin(), names.end());
    for (const std::string& n : names) {
        const std::string path = dir + "/" + n;
        const std::string rel = relative.empty() ? n : relative + "/" + n;
        struct stat link_status {};
        REQUIRE(::lstat(path.c_str(), &link_status) == 0);
        if (S_ISDIR(link_status.st_mode)) {
            if (skipped_second(rel, n)) continue;
            out.directories.push_back(n);
            walk_second(path, rel, out);
        } else if (S_ISREG(link_status.st_mode)) {
            out.files.push_back(rel);
        } else if (S_ISLNK(link_status.st_mode)) {
            struct stat target_status {};
            if (::stat(path.c_str(), &target_status) == 0 && S_ISREG(target_status.st_mode)) out.files.push_back(rel);
        }
    }
}

bool ends_with_second(const std::string& s, const std::string& suffix) { return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0; }

// the files the token scan reads: CMakeLists.txt and *.cmake anywhere, and *.sh directly in tools/
bool read_for_words_second(const std::string& relative) {
    const std::size_t slash = relative.find_last_of('/');
    const std::string name = slash == std::string::npos ? relative : relative.substr(slash + 1);
    if (name == "CMakeLists.txt" || ends_with_second(name, ".cmake")) return true;
    return relative.compare(0, 6, "tools/") == 0 && relative.find('/', 6) == std::string::npos && ends_with_second(name, ".sh");
}

std::set<std::string> second_words() {
    std::set<std::string> all;
    for (const Language& l : languages()) all.insert(l.words.begin(), l.words.end());
    return all;
}

std::set<std::string> second_extensions() {
    std::set<std::string> all;
    for (const Language& l : languages()) all.insert(l.extensions.begin(), l.extensions.end());
    return all;
}

// the flagged lines of a text by regular expression: (line number, word)
std::vector<std::pair<std::size_t, std::string>> second_scan(const std::string& text) {
    static const std::set<std::string> words = second_words();
    static const std::set<std::string> extensions = second_extensions();
    static const std::regex token("[A-Za-z0-9]+");
    std::vector<std::pair<std::size_t, std::string>> hits;
    std::istringstream in(text);
    std::string line;
    std::size_t number = 0;
    while (std::getline(in, line)) {
        ++number;
        for (std::sregex_iterator it(line.begin(), line.end(), token), end; it != end; ++it) {
            const std::string w = lowered(it->str());
            std::string base = w;
            while (!base.empty() && std::isdigit(static_cast<unsigned char>(base.back())) != 0) base.pop_back();
            const std::size_t at = static_cast<std::size_t>(it->position());
            const bool after_dot = at >= 1 && line[at - 1] == '.';
            if ((after_dot && extensions.count(w) != 0) || words.count(base) != 0) hits.emplace_back(number, w);
        }
    }
    return hits;
}

// the first line of a file if it begins with "#!", else nullopt -- read with stdio, up to the line end
std::optional<std::string> second_first_line(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    REQUIRE(f != nullptr);
    std::string line;
    int c = 0;
    while ((c = std::fgetc(f)) != EOF && c != '\n' && line.size() < 4096) line.push_back(static_cast<char>(c));
    std::fclose(f);
    if (line.compare(0, 2, "#!") != 0) return std::nullopt;
    return line;
}

}  // namespace

TEST_CASE("the real tree: nothing refused, the three allowlisted lines used and none stale -- and the counts, the lines and the first lines agree with a second method", "[interpretercheck][real_tree]") {
    const fs::path root = fs::path(ODL_TREE_ROOT);
    const ic::Report rep = ic::check(ic::default_settings());
    for (const ic::Finding& f : rep.violations) UNSCOPED_INFO("refused: " << f.file << ":" << f.line << " " << f.language << " " << f.what << " -- " << f.text);
    CHECK(rep.violations.empty());
    for (const ic::AllowedLine& s : rep.stale) UNSCOPED_INFO("stale: " << s.file << ": " << s.text);
    CHECK(rep.stale.empty());

    // second method 1: the walk
    SecondWalk walk;
    walk_second(root.string(), "", walk);
    CHECK(rep.files_walked == walk.files.size());
    std::size_t read_count = 0;
    for (const std::string& f : walk.files) {
        if (read_for_words_second(f)) ++read_count;
    }
    CHECK(rep.build_files == read_count);
    CHECK(read_count > 20);   // the tree has its CMake files and scripts; a walk that found none would pass the comparison above for the wrong reason
    CHECK(walk.files.size() > 200);

    // second method 2: the names -- no file with an extension of the lists or a conventional name, no directory of the three
    const std::set<std::string> extensions = second_extensions();
    for (const std::string& f : walk.files) {
        const std::size_t slash = f.find_last_of('/');
        const std::string name = lowered(slash == std::string::npos ? f : f.substr(slash + 1));
        const std::size_t dot = name.find_last_of('.');
        if (dot != std::string::npos && dot + 1 != name.size()) CHECK(extensions.count(name.substr(dot + 1)) == 0);
        for (const auto& entry : conventional_names()) CHECK(name != lowered(entry.first));
        CHECK_FALSE((name.compare(0, 12, "requirements") == 0 && ends_with_second(name, ".txt")));
    }
    for (const std::string& d : walk.directories) {
        CHECK(lowered(d) != "__pycache__");
        CHECK(lowered(d) != "node_modules");
        CHECK(lowered(d) != "site-packages");
    }

    // second method 3: the first lines -- every file that begins with "#!" names sh or bash
    std::size_t with_shebang = 0;
    for (const std::string& f : walk.files) {
        const std::optional<std::string> line = second_first_line(root.string() + "/" + f);
        if (!line.has_value()) continue;
        ++with_shebang;
        std::istringstream words(line->substr(2));
        std::string first;
        words >> first;
        const std::size_t slash = first.find_last_of('/');
        std::string program = slash == std::string::npos ? first : first.substr(slash + 1);
        if (program == "env") {
            std::string next;
            while (words >> next) {
                if (next[0] != '-' && next.find('=') == std::string::npos) break;
                next.clear();
            }
            const std::size_t s2 = next.find_last_of('/');
            program = s2 == std::string::npos ? next : next.substr(s2 + 1);
        }
        INFO("file: " << f << ", first line: " << *line);
        CHECK((program == "sh" || program == "bash"));
    }
    CHECK(with_shebang >= 2);   // tools/ci.sh and tools/bootstrap.sh at the least

    // second method 4: the words -- the lines the regular-expression scan flags in the files read for words are exactly the lines the tool accepted by the allowlist
    std::set<std::pair<std::string, std::size_t>> flagged;
    for (const std::string& f : walk.files) {
        if (!read_for_words_second(f)) continue;
        for (const auto& [number, word] : second_scan(read_text(root / f))) flagged.insert({f, number});
    }
    const fs::path workflows = (root / ".." / ".github" / "workflows").lexically_normal();
    std::size_t workflow_count = 0;
    if (fs::is_directory(workflows)) {
        for (const fs::directory_entry& e : fs::directory_iterator(workflows)) {
            if (!e.is_regular_file()) continue;
            ++workflow_count;
            for (const auto& [number, word] : second_scan(read_text(e.path()))) flagged.insert({".github/workflows/" + e.path().filename().string(), number});
        }
        CHECK(rep.workflows_found);
    } else {
        CHECK_FALSE(rep.workflows_found);
    }
    CHECK(rep.workflow_files == workflow_count);
    std::set<std::pair<std::string, std::size_t>> accepted;
    for (const ic::Accepted& a : rep.accepted) accepted.insert({a.file, a.line});
    CHECK(flagged == accepted);
    CHECK(rep.accepted.size() == 3);

    // the three entries stand on lines of CMakeLists.txt that are exactly their texts, each once
    const std::string cmake = read_text(root / "CMakeLists.txt");
    for (const ic::AllowedLine& entry : ic::real_allowlist()) {
        INFO("entry: " << entry.text);
        CHECK(entry.file == "CMakeLists.txt");
        std::size_t count = 0;
        std::istringstream in(cmake);
        std::string line;
        while (std::getline(in, line)) {
            if (line == entry.text) ++count;
        }
        CHECK(count == 1);
        CHECK(entry.reason.size() > 40);   // a reason, not a word
    }
}

TEST_CASE("the real tree: the tool run as ci.sh runs it, by the executable's own entry point -- with the workflows beside the tree or without them", "[interpretercheck][real_tree]") {
    // An export of rewrite/ alone has no .github beside it: then the tool says so, first, and still passes.  Either layout is the real tree's.
    const fs::path workflows = (fs::path(ODL_TREE_ROOT) / ".." / ".github" / "workflows").lexically_normal();
    const std::string note = fs::is_directory(workflows) ? std::string() : "note     no workflows directory at " + workflows.string() + ": the files of the workflows were NOT read\n";
    std::ostringstream out;
    std::ostringstream err;
    const int code = ic::run({"--quiet"}, Streams{out, err});
    INFO("stderr: " << err.str());
    CHECK(code == 0);
    CHECK(err.str().empty());
    const std::string line = out.str();
    CHECK(line.find(note + "ok       no script interpreter in the build, the tests or CI (files looked at by name and first line: ") == 0);
    CHECK(line.find("allowlisted lines: 3)\n") != std::string::npos);
    CHECK(line.find("allowed  ") == std::string::npos);   // --quiet
    std::ostringstream out2;
    std::ostringstream err2;
    CHECK(ic::run({}, Streams{out2, err2}) == 0);
    const std::string full = out2.str();   // one string: iterators of two temporaries are not a range
    CHECK(std::count(full.begin(), full.end(), '\n') == (note.empty() ? 4 : 5));   // the note if any, the ok line, the three allowed lines
    CHECK(full.find("allowed  CMakeLists.txt:") != std::string::npos);
    CHECK(err2.str().empty());
}

TEST_CASE("the real tree: ci.sh runs gate 14 as the last gate, the tool with --quiet, and the ctests are wired", "[interpretercheck][real_tree]") {
    const fs::path root = fs::path(ODL_TREE_ROOT);
    const std::string ci = read_text(root / "tools" / "ci.sh");
    // the gate's name, then the call, on the next line; after gate 13's call; and the last printf says 14
    const std::string gate = "gate \"no script interpreter in the build, the tests or CI\"\n\"$BUILD/tools/interpretercheck\" --quiet\n";
    const std::size_t at = ci.find(gate);
    REQUIRE(at != std::string::npos);
    CHECK(ci.find(gate, at + 1) == std::string::npos);
    const std::size_t thirteen = ci.find("gate \"build is reproducible\"\n\"$BUILD/tools/reprocheck\"\n");
    REQUIRE(thirteen != std::string::npos);
    CHECK(thirteen < at);
    CHECK(ci.find("of 14 gates passed") != std::string::npos);
    CHECK(ci.find("of 13 gates passed") == std::string::npos);
    CHECK(ci.find("interpretercheck", at + gate.size()) == std::string::npos);   // nothing after it names the tool again
    std::size_t printfs = 0;
    for (std::size_t p = ci.find("gates passed"); p != std::string::npos; p = ci.find("gates passed", p + 1)) ++printfs;
    CHECK(printfs == 1);
    // the ctests
    const std::string tests = read_text(root / "tests" / "CMakeLists.txt");
    CHECK(tests.find("add_test(NAME interpreters.refused_in_build_tests_and_ci\n  COMMAND interpretercheck --quiet)\n") != std::string::npos);
    for (const char* name : {"interpretercheck.behaviour", "interpretercheck.real_tree", "interpretercheck.help_exits_zero", "interpretercheck.help_text", "interpretercheck.refuses_an_argument",
                             "interpretercheck.refusal_text"}) {
        INFO("ctest: " << name);
        CHECK(tests.find(std::string("add_test(NAME ") + name + "\n") != std::string::npos);
    }
}
