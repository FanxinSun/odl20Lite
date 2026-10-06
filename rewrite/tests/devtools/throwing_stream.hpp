#pragma once
// tests/devtools/throwing_stream.hpp — an output stream whose every write throws, to reach the LAST-RESORT handler of a tool ("an error the tool did not anticipate", exit 70) on purpose: the
// tools print only at the end of a run, so a stream that refuses to be written to makes any run that gets that far fail inside the try block, with a std::runtime_error that is not one the
// tool caught on the way.  (Without this the handler can be reached by no input at all -- a tool that catches what it can anticipate leaves it nothing to do but a bad_alloc -- and a handler no
// test reaches is a handler whose exit code could be anything.)
//
// libstdc++ catches the exception inside operator<<, sets badbit and rethrows the very same exception when `exceptions()` includes badbit, so the tool sees a std::runtime_error("boom").

#include <ostream>
#include <stdexcept>
#include <streambuf>

namespace odl::devtools_testing {

class ThrowingStream : public std::ostream {
public:
    ThrowingStream() : std::ostream(&buffer_) { exceptions(std::ios::badbit); }

private:
    class Buffer : public std::streambuf {
    protected:
        int_type overflow(int_type) override { throw std::runtime_error("boom"); }
        std::streamsize xsputn(const char*, std::streamsize) override { throw std::runtime_error("boom"); }
    };
    Buffer buffer_;
};

}  // namespace odl::devtools_testing
