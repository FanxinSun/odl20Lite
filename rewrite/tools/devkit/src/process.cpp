#include <odl/devkit/process.hpp>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace odl::devkit {

namespace {

struct Pipe {
    int read_end = -1;
    int write_end = -1;
};

[[nodiscard]] Pipe make_pipe() {
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0) throw std::runtime_error(std::string("cannot create a pipe: ") + std::strerror(errno));
    return Pipe{fds[0], fds[1]};
}

void close_fd(int& fd) noexcept {
    if (fd >= 0) ::close(fd);
    fd = -1;
}

void set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) throw std::runtime_error("cannot make a pipe non-blocking");
}

}  // namespace

ProcessResult run_process(const std::vector<std::string>& argv, const ProcessOptions& options) {
    if (argv.empty()) throw std::runtime_error("run_process: no program given");

    std::vector<std::string> env_storage;
    for (char** e = environ; e != nullptr && *e != nullptr; ++e) {
        const std::string entry(*e);
        const std::size_t eq = entry.find('=');
        bool replaced = false;
        for (const auto& kv : options.env) {
            if (eq != std::string::npos && entry.compare(0, eq, kv.first) == 0 && eq == kv.first.size()) replaced = true;
        }
        if (!replaced) env_storage.push_back(entry);
    }
    for (const auto& kv : options.env) env_storage.push_back(kv.first + "=" + kv.second);
    std::vector<char*> envp;
    for (std::string& s : env_storage) envp.push_back(s.data());
    envp.push_back(nullptr);

    std::vector<std::string> args = argv;
    std::vector<char*> argp;
    for (std::string& s : args) argp.push_back(s.data());
    argp.push_back(nullptr);

    Pipe in = make_pipe();
    Pipe out = make_pipe();
    Pipe err = make_pipe();

    posix_spawn_file_actions_t actions;
    ::posix_spawn_file_actions_init(&actions);
    ::posix_spawn_file_actions_adddup2(&actions, in.read_end, 0);
    ::posix_spawn_file_actions_adddup2(&actions, out.write_end, 1);
    ::posix_spawn_file_actions_adddup2(&actions, err.write_end, 2);

    pid_t pid = 0;
    const int rc = ::posix_spawnp(&pid, argp[0], &actions, nullptr, argp.data(), envp.data());
    ::posix_spawn_file_actions_destroy(&actions);
    close_fd(in.read_end);
    close_fd(out.write_end);
    close_fd(err.write_end);
    if (rc != 0) {
        close_fd(in.write_end);
        close_fd(out.read_end);
        close_fd(err.read_end);
        throw std::runtime_error("cannot run '" + argv[0] + "': " + std::strerror(rc));
    }

    ProcessResult result;
    // A child that exits without reading its input must cost a short write, not the parent: SIGPIPE would end it.
    if (!options.input.empty()) std::signal(SIGPIPE, SIG_IGN);
    set_nonblocking(in.write_end);
    set_nonblocking(out.read_end);
    set_nonblocking(err.read_end);
    std::size_t written = 0;
    if (options.input.empty()) close_fd(in.write_end);
    while (in.write_end >= 0 || out.read_end >= 0 || err.read_end >= 0) {
        pollfd fds[3];
        nfds_t n = 0;
        int which[3];
        if (in.write_end >= 0) {
            fds[n] = pollfd{in.write_end, POLLOUT, 0};
            which[n++] = 0;
        }
        if (out.read_end >= 0) {
            fds[n] = pollfd{out.read_end, POLLIN, 0};
            which[n++] = 1;
        }
        if (err.read_end >= 0) {
            fds[n] = pollfd{err.read_end, POLLIN, 0};
            which[n++] = 2;
        }
        if (::poll(fds, n, -1) < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(std::string("poll failed: ") + std::strerror(errno));
        }
        for (nfds_t k = 0; k < n; ++k) {
            if (fds[k].revents == 0) continue;
            if (which[k] == 0) {
                const ssize_t w = ::write(in.write_end, options.input.data() + written, options.input.size() - written);
                if (w > 0) written += static_cast<std::size_t>(w);
                if (w < 0 && errno != EAGAIN && errno != EINTR) written = options.input.size();   // the child closed its stdin
                if (written >= options.input.size()) close_fd(in.write_end);
            } else {
                int& fd = which[k] == 1 ? out.read_end : err.read_end;
                std::string& sink = which[k] == 1 ? result.out : result.err;
                char buf[8192];
                const ssize_t r = ::read(fd, buf, sizeof buf);
                if (r > 0) {
                    sink.append(buf, static_cast<std::size_t>(r));
                } else if (r == 0 || (errno != EAGAIN && errno != EINTR)) {
                    close_fd(fd);
                }
            }
        }
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) throw std::runtime_error(std::string("waitpid failed: ") + std::strerror(errno));
    }
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }
    return result;
}

}  // namespace odl::devkit
