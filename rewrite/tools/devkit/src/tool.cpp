#include <odl/devkit/tool.hpp>

#include <ostream>

namespace odl::devkit {

void die(Streams& io, std::string_view tool, int code, std::string_view message) {
    io.err << tool << ": " << message << '\n';
    throw Exit{code};
}

}  // namespace odl::devkit
