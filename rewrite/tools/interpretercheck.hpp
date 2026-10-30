#pragma once
// tools/interpretercheck.hpp — refuse every script interpreter in the build, the tests and CI (plan §5 constraint 11; plan L0 step 8, decision D8 as extended by the maintainer's ruling of 2026-10-11; ci.sh's gate 14).
//
// `run` is the whole tool: `interpretercheck [--root DIR] [--workflows DIR] [--quiet] [--no-allowlist]` (the allowlist is this tree's own: --no-allowlist is for any other tree).  Constraint 11 says that what is not C or C++ is decided by the user before it is introduced, so a language the user has not decided on
// must not reach the build, the tests or CI unnoticed.  The languages that count are Python, Perl, Ruby, JavaScript, PHP, Lua, Tcl, R and Julia; the shell, its text tools and CMake have been the build's glue since L0
// and GitHub's own actions are the runner's machinery.  Three things are refused:
//   (1) NAMES   a file below the tree, outside data/ and the build trees, named like a program of one of those languages by its extension or by its conventional name (the table in the .cpp), and a directory called
//               __pycache__, node_modules or site-packages;
//   (2) #!      a file whose first line is a #! that names anything but sh or bash (an `env` is looked through);
//   (3) WORDS   in every CMakeLists.txt and *.cmake of the tree, every shell script (but oracle/capture.sh) and every file of the workflows directory: a program or package tool of one of those languages named as a whole word (ASCII letters and
//               digits, compared case-insensitively with the trailing digits removed), or a file extension of one after a dot.  Comments are read like code.
// A line that is a false positive is accepted ONLY by an entry of the allowlist that gives the file and the line's exact text, with its reason -- never by a pattern -- and an entry that accepts nothing is itself refused.
// Exit 0 clean; 1 refused; 2 an argument error or something that cannot be read; 70 an error the tool did not anticipate.

#include <odl/devkit/tool.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odl::tools::interpretercheck {

inline constexpr char kTool[] = "interpretercheck";

/// A line of a build file that holds a word of the list and is accepted all the same: the file (relative to the tree, '/' separators; a workflow file as ".github/workflows/NAME"), the line's exact text and why.
struct AllowedLine {
    std::string file;
    std::string text;
    std::string reason;
};

/// What is refused.  `kind` is "name" (a file), "directory", "shebang", "word" or "extension"; `line` is 0 for a name or a directory, 1 for a shebang.
struct Finding {
    std::string file;
    std::size_t line = 0;
    std::string language;
    std::string kind;
    std::string what;
    std::string text;
};

/// A line that the allowlist accepted.
struct Accepted {
    std::string file;
    std::size_t line = 0;
    std::string reason;
};

struct Report {
    std::vector<Finding> violations;       // sorted by file, then line
    std::vector<Accepted> accepted;        // sorted by file, then line
    std::vector<AllowedLine> stale;        // the entries of the allowlist that accepted nothing
    std::size_t files_walked = 0;          // files looked at for their name and their first line
    std::size_t build_files = 0;           // CMakeLists.txt, *.cmake and every shell script read for words
    std::size_t workflow_files = 0;        // files of the workflows directory read for words
    bool workflows_found = false;
};

struct Settings {
    std::filesystem::path root;            // the tree (rewrite/)
    std::filesystem::path workflows;       // the workflows directory; empty means <root>/../.github/workflows, which may be absent
    bool workflows_explicit = false;       // given by the caller: then it must exist
    std::vector<AllowedLine> allowlist;
};

/// The three lines of this tree that hold a word of the list and are no use of it.
[[nodiscard]] std::vector<AllowedLine> real_allowlist();
[[nodiscard]] std::filesystem::path default_root();
[[nodiscard]] Settings default_settings();

/// A word of the list found in a line, in the order of the line: the lower-cased word as it stands in the line, its language, and "word" or "extension".
struct Word {
    std::string text;
    std::string language;
    std::string kind;
};
[[nodiscard]] std::vector<Word> flagged_words(std::string_view line);

/// The interpreter a first line names: nullopt if the line is no #! line, an empty string if it names none, otherwise the last path component of the program (of the first word that is neither an option nor an
/// assignment after `env`, if the program is env).
[[nodiscard]] std::optional<std::string> shebang_interpreter(std::string_view first_line);

/// The language a program name (python3.13, perl5, tclsh8.6, ...) belongs to, or "other".
[[nodiscard]] std::string shebang_language(std::string_view interpreter);

/// Whether a file or a directory is refused by its name alone (path relative to the tree, last component only is looked at): the finding, or nullopt.
[[nodiscard]] std::optional<Finding> refused_name(std::string_view relative_path, bool is_directory);

/// The whole check.  Throws std::runtime_error when the tree or a file in it cannot be read.
[[nodiscard]] Report check(const Settings& settings);

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::interpretercheck
