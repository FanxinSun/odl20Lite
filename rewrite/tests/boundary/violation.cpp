// Compiled twice by tests/CMakeLists.txt: once with alpha's include path on the
// command line (must succeed) and once with only gamma's (must fail).  The
// include path is the enforcement mechanism, so this is a direct test of it.
#include <odl/alpha/alpha.hpp>
int use() { return odl::alpha::answer(); }
