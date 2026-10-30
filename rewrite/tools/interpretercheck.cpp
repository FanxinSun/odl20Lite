// interpretercheck.cpp — refuse every script interpreter in the build, the tests and CI.
//
// Plan §5 constraint 11 (the user's directive of 2026-10-06) and L0 step 8's decision D8, which asked for a check that refuses Python, extended by the maintainer's ruling of 2026-10-11 (plan/subplan_L0/L0-8.md, "Ruled
// 2026-10-11, at the start of C12") to every script interpreter: Python, Perl, Ruby, JavaScript, PHP, Lua, Tcl, R and Julia.  Why: constraint 11 says "otherwise keep C++", and the user decides any language other than C
// or C++ before it is introduced, so a language the user has not decided on must not reach the build, the tests or CI unnoticed (Perl had reached a scratch witness once, in C5).
//
// WHAT STAYS: the shell, its text tools (sed, awk, grep) and CMake, which have been the build's glue since L0; GitHub's own actions (checkout, cache), which are the runner's machinery; outside rewrite/, the original's
// Python and the revival's converters; plotting in Python, which the user allows and which would get its own ruling if any were ever added (none is).
//
// THE THREE THINGS IT REFUSES, in the places and on the terms D8 set (the header says them again):
//   (1) NAMES   below the tree, outside the top-level data/ and the build trees (.git, build, build-*, CMakeFiles, _deps; links to directories are not followed): a file named by an extension of one of those languages
//               (.py .pyc .pyo .pyw .pyi .pyx .ipynb | .pl .pm | .rb .gemspec | .js .mjs .cjs .jsx | .php .phar | .lua | .tcl | .r .rmd | .jl) or by a conventional name (setup.cfg, pyproject.toml, Pipfile,
//               requirements*.txt, package.json, yarn.lock, composer.json, Gemfile, Rakefile, cpanfile, ...), and a directory called __pycache__, node_modules or site-packages.
//   (2) #!      any file whose first line is a #! that names anything but sh or bash.
//   (3) WORDS   in every CMakeLists.txt and *.cmake below the tree, every *.sh file and every file that begins with a #! naming sh or bash (but for oracle/capture.sh, by exact path: kWordScanException) and every file of the workflows directory: a program or package tool of those languages as a WHOLE WORD (ASCII letters and digits, so that
//               Python3_EXECUTABLE and ENABLE_PYTHON carry the word and "pythonic", "perlin" and "x.json" do not; compared case-insensitively, with the trailing digits removed: python3, lua5, node18), or a file extension
//               of one after a dot ("fetch.py", not "py_sum").  Comments are read like code: the tree's own history lives in PROVENANCE.md, not in the build files.
//
// THE ALLOWLIST (real_allowlist) is a list of exact lines, each with its reason: never a pattern.  An entry that accepts no line is itself refused, so that it cannot outlive the line it excuses.  The allowlist is THIS
// tree's: on any other tree its entries accept nothing and are refused, which is why `--no-allowlist` exists (the proofs on synthetic trees use it).
//
// THE PROOF (registered before the tool was compiled or run: C12_proof_registration.txt, item C, in the group's report files, with three dated amendments -- the third was written after the first draft of this file and before
// its first compilation): each of seven injections, made alone on a synthetic tree that passes, is refused on its own; the tool on the tree as it stood before the cleaning of item B refuses exactly the lines a scan
// written apart from it (an awk script) flags; on the tree after it, it passes.
//
//   interpretercheck [-h] [--quiet] [--no-allowlist] [--root ROOT] [--workflows WORKFLOWS]
//   exit 0 no script interpreter   1 refused (a violation, or a stale allowlist entry)   2 an argument error, or something that cannot be read   70 an error the tool did not anticipate

#include "interpretercheck.hpp"

#include <odl/devkit/fs.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <system_error>
#include <tuple>
#include <utility>

namespace dk = odl::devkit;
namespace fs = std::filesystem;
using odl::devkit::Streams;

namespace odl::tools::interpretercheck {

namespace {

constexpr int kOk = 0;
constexpr int kRefused = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;

struct Entry {
    const char* word;
    const char* language;
};

// programs and package tools, as base names (the trailing digits of a word are removed before it is looked up: python3, pip3, lua5, perl5, node18)
constexpr Entry kWords[] = {
    {"python", "Python"},   {"pip", "Python"},      {"pipx", "Python"},   {"pytest", "Python"}, {"virtualenv", "Python"}, {"venv", "Python"},      {"conda", "Python"},
    {"poetry", "Python"},   {"setuptools", "Python"}, {"pypy", "Python"}, {"ipython", "Python"}, {"jupyter", "Python"},
    {"perl", "Perl"},       {"cpan", "Perl"},       {"cpanm", "Perl"},
    {"ruby", "Ruby"},       {"jruby", "Ruby"},      {"irb", "Ruby"},      {"gem", "Ruby"},      {"bundler", "Ruby"},      {"rake", "Ruby"},
    {"node", "JavaScript"}, {"nodejs", "JavaScript"}, {"npm", "JavaScript"}, {"npx", "JavaScript"}, {"yarn", "JavaScript"}, {"pnpm", "JavaScript"}, {"deno", "JavaScript"}, {"bun", "JavaScript"},
    {"php", "PHP"},         {"composer", "PHP"},
    {"lua", "Lua"},         {"luajit", "Lua"},      {"luarocks", "Lua"},
    {"tcl", "Tcl"},         {"tclsh", "Tcl"},
    {"rscript", "R"},
    {"julia", "Julia"},
};

// file extensions (a word directly after a dot, whatever stands before the dot; compared as written, digits kept)
constexpr Entry kExtensions[] = {
    {"py", "Python"},       {"pyc", "Python"},      {"pyo", "Python"},    {"pyw", "Python"},    {"pyi", "Python"},        {"pyx", "Python"},      {"ipynb", "Python"},
    {"pl", "Perl"},         {"pm", "Perl"},
    {"rb", "Ruby"},         {"gemspec", "Ruby"},
    {"js", "JavaScript"},   {"mjs", "JavaScript"},  {"cjs", "JavaScript"}, {"jsx", "JavaScript"},
    {"php", "PHP"},         {"phar", "PHP"},
    {"lua", "Lua"},
    {"tcl", "Tcl"},
    {"r", "R"},             {"rmd", "R"},
    {"jl", "Julia"},
};

// conventional file names, lower-cased
constexpr Entry kNames[] = {
    {"setup.cfg", "Python"},  {"pyproject.toml", "Python"}, {"pipfile", "Python"},       {"pipfile.lock", "Python"}, {"package.json", "JavaScript"}, {"package-lock.json", "JavaScript"},
    {"yarn.lock", "JavaScript"}, {"composer.json", "PHP"},  {"composer.lock", "PHP"},    {"gemfile", "Ruby"},        {"gemfile.lock", "Ruby"},        {"rakefile", "Ruby"},
    {"cpanfile", "Perl"},
};

constexpr Entry kDirectories[] = {
    {"__pycache__", "Python"},
    {"node_modules", "JavaScript"},
    {"site-packages", "Python"},
};

template <std::size_t N>
const Entry* find_entry(const Entry (&table)[N], std::string_view word) {
    for (const Entry& e : table) {
        if (word == e.word) return &e;
    }
    return nullptr;
}

bool is_alnum(char c) noexcept { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string last_component(std::string_view path) {
    const std::size_t slash = path.find_last_of('/');
    return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

bool ends_with(std::string_view s, std::string_view suffix) { return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0; }

/// The directories nothing is looked at in: the top-level data/ (pinned inputs, not the tree's code), and the build trees.
bool skipped_directory(const std::string& relative, const std::string& name) {
    return relative == "data" || name == ".git" || name == "build" || name.rfind("build-", 0) == 0 || name == "CMakeFiles" || name == "_deps";
}

/// The ONE file whose words are not read, by exact path relative to the root, and why (the maintainer's ruling of 2026-10-31): a frozen record, outside the build, the tests and CI, closed to the Executor by the
/// handover.  Its name and its #! line are still looked at.  Any other file of that name is read.
constexpr const char* kWordScanException = "oracle/capture.sh";

/// The files read for words: every CMakeLists.txt and *.cmake, and every *.sh anywhere outside data/ and the build trees (a file beginning with a #! naming sh or bash is added by examine_file); but for the exception.
bool read_for_words(const std::string& relative) {
    const std::string name = last_component(relative);
    if (name == "CMakeLists.txt" || ends_with(name, ".cmake")) return true;
    return relative != kWordScanException && ends_with(name, ".sh");
}

std::string join(const std::vector<std::string>& parts, const char* separator) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out += separator;
        out += parts.at(i);
    }
    return out;
}

std::string trimmed(std::string_view text, std::size_t most) {
    const std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) return std::string();
    text.remove_prefix(first);
    return std::string(text.substr(0, most));
}

/// The first line of a file, up to 512 bytes, without its line end, if the file begins with "#!"; otherwise nullopt.
std::optional<std::string> first_line_if_shebang(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + file.string());
    char head[512];
    in.read(head, sizeof head);
    const std::size_t got = static_cast<std::size_t>(in.gcount());
    if (got < 2 || head[0] != '#' || head[1] != '!') return std::nullopt;
    std::string line(head, got);
    const std::size_t end = line.find('\n');
    if (end != std::string::npos) line.resize(end);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

}  // namespace

std::vector<AllowedLine> real_allowlist() {
    return {
        {"CMakeLists.txt", "foreach(_odl_interpreter Python Python2 Python3 PythonInterp PythonLibs Perl)",
         "the guard: it names the interpreters whose find_package it REFUSES (a find_package of one is a defect of the tree that no scan of the tree's own files can see in a dependency)"},
        {"CMakeLists.txt", "foreach(_odl_variable Python3_EXECUTABLE Python_EXECUTABLE Python2_EXECUTABLE PYTHON_EXECUTABLE PERL_EXECUTABLE)",
         "the guard: it names the variables whose READ it makes a fatal error, the way a dependency's own CMake would ask for an interpreter"},
        {"CMakeLists.txt", "set(ENABLE_PYTHON     OFF CACHE BOOL \"\" FORCE)",
         "CALCEPH's own option, which switches ITS interpreter binding OFF: it is no use of one"},
    };
}

fs::path default_root() {
#ifdef ODL_TREE_ROOT
    return fs::path(ODL_TREE_ROOT);
#else
    return fs::current_path();
#endif
}

Settings default_settings() {
    Settings settings;
    settings.root = default_root();
    settings.allowlist = real_allowlist();
    return settings;
}

std::vector<Word> flagged_words(std::string_view line) {
    std::vector<Word> found;
    std::size_t i = 0;
    while (i < line.size()) {
        if (!is_alnum(line[i])) {
            ++i;
            continue;
        }
        const std::size_t begin = i;
        while (i < line.size() && is_alnum(line[i])) ++i;
        const std::string word = lower(line.substr(begin, i - begin));
        std::string base = word;
        while (!base.empty() && is_digit(base.back())) base.pop_back();
        const Entry* extension = begin >= 1 && line[begin - 1] == '.' ? find_entry(kExtensions, word) : nullptr;
        if (extension != nullptr) {
            found.push_back(Word{word, extension->language, "extension"});
        } else if (const Entry* e = find_entry(kWords, base)) {
            found.push_back(Word{word, e->language, "word"});
        }
    }
    return found;
}

std::optional<std::string> shebang_interpreter(std::string_view first_line) {
    if (first_line.substr(0, 2) != "#!") return std::nullopt;
    std::vector<std::string> words;
    std::string current;
    for (const char c : first_line.substr(2)) {
        if (c == ' ' || c == '\t' || c == '\r') {
            if (!current.empty()) words.push_back(std::move(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) words.push_back(std::move(current));
    if (words.empty()) return std::string();
    const std::string program = last_component(words.at(0));
    if (program != "env") return program;
    for (std::size_t i = 1; i < words.size(); ++i) {
        const std::string& w = words.at(i);
        if (w.front() == '-' || w.find('=') != std::string::npos) continue;   // an option of env, or a variable it sets
        return last_component(w);
    }
    return std::string();
}

std::string shebang_language(std::string_view interpreter) {
    std::string letters;
    for (const char c : lower(interpreter)) {
        if (c < 'a' || c > 'z') break;
        letters += c;
    }
    if (letters == "python" || letters == "pypy" || letters == "ipython") return "Python";
    if (letters == "perl") return "Perl";
    if (letters == "ruby" || letters == "jruby" || letters == "irb") return "Ruby";
    if (letters == "node" || letters == "nodejs" || letters == "deno" || letters == "bun") return "JavaScript";
    if (letters == "php") return "PHP";
    if (letters == "lua" || letters == "luajit") return "Lua";
    if (letters == "tclsh" || letters == "wish" || letters == "tcl") return "Tcl";
    if (letters == "rscript") return "R";
    if (letters == "julia") return "Julia";
    return "other";
}

std::optional<Finding> refused_name(std::string_view relative_path, bool is_directory) {
    const std::string name = last_component(relative_path);
    const std::string low = lower(name);
    Finding f;
    f.file = std::string(relative_path);
    f.kind = is_directory ? "directory" : "name";
    if (is_directory) {
        if (const Entry* d = find_entry(kDirectories, low)) {
            f.language = d->language;
            f.what = name;
            return f;
        }
        return std::nullopt;
    }
    if (const Entry* n = find_entry(kNames, low)) {
        f.language = n->language;
        f.what = "name " + name;
        return f;
    }
    if (low.rfind("requirements", 0) == 0 && ends_with(low, ".txt")) {
        f.language = "Python";
        f.what = "name " + name;
        return f;
    }
    const std::size_t dot = low.find_last_of('.');
    if (dot != std::string::npos && dot + 1 != low.size()) {   // the part after the last dot, even for a name that is nothing but ".py"
        if (const Entry* x = find_entry(kExtensions, std::string_view(low).substr(dot + 1))) {
            f.language = x->language;
            f.what = "extension ." + low.substr(dot + 1);
            return f;
        }
    }
    return std::nullopt;
}

namespace {

struct Scan {
    const Settings& settings;
    Report& report;
    std::vector<bool> used;
};

void read_words(Scan& scan, const std::string& label, const fs::path& file) {
    const dk::Bytes bytes = dk::read_bytes(file);
    const std::string_view text = dk::as_text(bytes);
    std::size_t pos = 0;
    std::size_t line_number = 0;
    while (pos <= text.size()) {
        const std::size_t newline = text.find('\n', pos);
        std::string_view line = text.substr(pos, newline == std::string_view::npos ? std::string_view::npos : newline - pos);
        pos = newline == std::string_view::npos ? text.size() + 1 : newline + 1;
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const std::vector<Word> words = flagged_words(line);
        if (words.empty()) continue;
        bool accepted = false;
        for (std::size_t k = 0; k < scan.settings.allowlist.size() && !accepted; ++k) {
            const AllowedLine& entry = scan.settings.allowlist.at(k);
            if (entry.file == label && entry.text == line) {
                scan.used.at(k) = true;
                scan.report.accepted.push_back(Accepted{label, line_number, entry.reason});
                accepted = true;
            }
        }
        if (accepted) continue;
        std::vector<std::string> languages;
        std::vector<std::string> tokens;
        for (const Word& w : words) {
            if (std::find(languages.begin(), languages.end(), w.language) == languages.end()) languages.push_back(w.language);
            tokens.push_back(w.kind == "extension" ? "." + w.text : w.text);
        }
        Finding f;
        f.file = label;
        f.line = line_number;
        f.language = join(languages, ", ");
        f.kind = words.front().kind;
        f.what = join(tokens, ", ");
        f.text = trimmed(line, 200);
        scan.report.violations.push_back(std::move(f));
    }
}

void examine_file(Scan& scan, const std::string& relative, const fs::path& file, std::vector<std::pair<std::string, fs::path>>& for_words) {
    ++scan.report.files_walked;
    if (const std::optional<Finding> named = refused_name(relative, false)) {
        scan.report.violations.push_back(*named);
        return;   // one refusal for the file: the name says what it is
    }
    bool first_line_is_shebang = false;   // a #! line that names sh or bash: the file is a shell script whatever it is called
    if (const std::optional<std::string> first = first_line_if_shebang(file)) {
        const std::string interpreter = shebang_interpreter(*first).value_or(std::string());
        if (interpreter != "sh" && interpreter != "bash") {
            Finding f;
            f.file = relative;
            f.line = 1;
            f.kind = "shebang";
            f.language = interpreter.empty() ? std::string("other") : shebang_language(interpreter);
            f.what = interpreter.empty() ? std::string("no interpreter") : interpreter;
            f.text = trimmed(*first, 200);
            scan.report.violations.push_back(std::move(f));
            return;
        }
        first_line_is_shebang = true;
    }
    if (relative != kWordScanException && (read_for_words(relative) || first_line_is_shebang)) for_words.emplace_back(relative, file);
}

void walk(Scan& scan, const fs::path& directory, const std::string& relative, std::vector<std::pair<std::string, fs::path>>& for_words) {
    std::vector<fs::directory_entry> entries;
    std::error_code error;
    for (fs::directory_iterator it(directory, error); !error && it != fs::directory_iterator(); it.increment(error)) entries.push_back(*it);
    if (error) throw std::runtime_error("cannot read the directory " + directory.string() + ": " + error.message());
    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) { return a.path().filename().string() < b.path().filename().string(); });
    for (const fs::directory_entry& entry : entries) {
        const std::string name = entry.path().filename().string();
        const std::string below = relative.empty() ? name : relative + "/" + name;
        std::error_code ignored;
        if (!entry.is_symlink(ignored) && entry.is_directory(ignored)) {
            if (skipped_directory(below, name)) continue;
            if (const std::optional<Finding> named = refused_name(below, true)) {
                scan.report.violations.push_back(*named);
                continue;
            }
            walk(scan, entry.path(), below, for_words);
        } else if (entry.is_regular_file(ignored)) {   // a link to a regular file is that file
            examine_file(scan, below, entry.path(), for_words);
        }
    }
}

}  // namespace

Report check(const Settings& settings) {
    if (!fs::is_directory(settings.root)) throw std::runtime_error("the tree " + settings.root.string() + " is not a directory");
    Report report;
    Scan scan{settings, report, std::vector<bool>(settings.allowlist.size(), false)};
    std::vector<std::pair<std::string, fs::path>> for_words;
    walk(scan, settings.root, "", for_words);
    for (const auto& item : for_words) {
        ++report.build_files;
        read_words(scan, item.first, item.second);
    }
    fs::path workflows = settings.workflows;
    const std::string label = settings.workflows.empty() ? std::string(".github/workflows") : settings.workflows.generic_string();   // the files are named as the directory is: by its default place, or as given
    if (workflows.empty()) workflows = (settings.root / ".." / ".github" / "workflows").lexically_normal();
    if (fs::is_directory(workflows)) {
        report.workflows_found = true;
        std::vector<fs::path> files;
        for (const fs::directory_entry& entry : fs::directory_iterator(workflows)) {
            std::error_code ignored;
            if (entry.is_regular_file(ignored)) files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());
        for (const fs::path& file : files) {
            ++report.workflow_files;
            read_words(scan, label + "/" + file.filename().string(), file);
        }
    } else if (settings.workflows_explicit) {
        throw std::runtime_error("--workflows: " + workflows.string() + " is not a directory");
    }
    for (std::size_t k = 0; k < settings.allowlist.size(); ++k) {
        if (!scan.used.at(k)) report.stale.push_back(settings.allowlist.at(k));
    }
    std::stable_sort(report.violations.begin(), report.violations.end(), [](const Finding& a, const Finding& b) { return std::tie(a.file, a.line) < std::tie(b.file, b.line); });
    std::stable_sort(report.accepted.begin(), report.accepted.end(), [](const Accepted& a, const Accepted& b) { return std::tie(a.file, a.line) < std::tie(b.file, b.line); });
    return report;
}

namespace {

const char kUsageText[] = "usage: interpretercheck [-h] [--quiet] [--no-allowlist] [--root ROOT] [--workflows WORKFLOWS]\n";

const char kHelpText[] =
    "\n"
    "Refuse every script interpreter in the build, the tests and CI (plan section 5 constraint 11; ci.sh's gate 14): Python, Perl, Ruby, JavaScript, PHP, Lua, Tcl, R and Julia.  It refuses, below the tree and outside\n"
    "data/ and the build trees, a file named like a program of one of them (by extension or by its conventional name) and any file whose first line is a #! naming anything but sh or bash; and, in every CMakeLists.txt\n"
    "and *.cmake, every shell script (but oracle/capture.sh) and every file of the workflows directory, a program or package tool of one of them as a whole word, or a file extension of one.  A line that is a false positive is accepted only by an\n"
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

}  // namespace

int run_on(const Settings& given, const std::vector<std::string>& argv, Streams io) {
    try {
        Settings settings = given;
        bool quiet = false;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--root" || opt == "--workflows") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value = argv[++i];
                }
                if (value.empty()) return usage_error("argument " + opt + ": the path is empty");
                if (opt == "--root") {
                    settings.root = value;
                } else {
                    settings.workflows = value;
                    settings.workflows_explicit = true;
                }
            } else if (opt == "--quiet" && !has_value) {
                quiet = true;
            } else if (opt == "--no-allowlist" && !has_value) {
                settings.allowlist.clear();
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        try {
            const Report report = check(settings);
            std::set<std::string> files;
            for (const Finding& f : report.violations) {
                files.insert(f.file);
                if (f.kind == "directory") {
                    io.err << f.file << ": " << f.language << ": directory refused (" << f.what << ")\n";
                } else if (f.kind == "name") {
                    io.err << f.file << ": " << f.language << ": file name refused (" << f.what << ")\n";
                } else if (f.kind == "shebang") {
                    io.err << f.file << ":1: " << f.language << ": #! names " << f.what << " (only sh and bash are allowed) -- " << f.text << '\n';
                } else {
                    io.err << f.file << ':' << f.line << ": " << f.language << ": " << f.what << " -- " << f.text << '\n';
                }
            }
            for (const AllowedLine& s : report.stale) {
                io.err << kTool << ": stale allowlist entry, it accepts no line: " << s.file << ": " << s.text << " (" << s.reason << ")\n";
            }
            if (!report.violations.empty() || !report.stale.empty()) {
                std::vector<std::string> counts;
                if (!report.violations.empty()) counts.push_back(std::to_string(report.violations.size()) + " refusal(s) in " + std::to_string(files.size()) + " file(s)");
                if (!report.stale.empty()) counts.push_back(std::to_string(report.stale.size()) + (report.stale.size() == 1 ? " stale allowlist entry" : " stale allowlist entries"));
                io.err << kTool << ": REFUSED: " << join(counts, ", ") << " -- "
                       << (report.violations.empty() ? "an allowlist entry that accepts no line is a defect of the check itself" : "no script interpreter may reach the build, the tests or CI (plan section 5 constraint 11)") << '\n';
                return kRefused;
            }
            if (!report.workflows_found) {
                const fs::path where = settings.workflows.empty() ? (settings.root / ".." / ".github" / "workflows").lexically_normal() : settings.workflows;
                io.out << "note     no workflows directory at " << where.string() << ": the files of the workflows were NOT read\n";
            }
            io.out << "ok       no script interpreter in the build, the tests or CI (files looked at by name and first line: " << report.files_walked << "; build files read for words: " << report.build_files
                   << "; workflow files read for words: " << report.workflow_files << "; allowlisted lines: " << report.accepted.size() << ")\n";
            if (!quiet) {
                for (const Accepted& a : report.accepted) io.out << "allowed  " << a.file << ':' << a.line << "  " << a.reason << '\n';
            }
            return kOk;
        } catch (const std::runtime_error& exc) {   // dk::read_bytes, the directory walk, a missing tree: the path and the reason
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        }
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::interpretercheck

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::interpretercheck::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
