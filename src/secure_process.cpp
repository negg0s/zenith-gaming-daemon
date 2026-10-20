#include "zenith/secure_process.hpp"
#include <thread>

#ifdef __linux__
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <csignal>
#include <poll.h>
#endif

namespace zenith {

bool SecureProcess::is_binary_approved(std::string_view path) {
    for (const auto& approved : APPROVED_BINARIES) {
        if (path == approved) return true;
    }
    return false;
}

SubprocessResult SecureProcess::execute(const std::string& binary_path,
                                        const std::vector<std::string>& args,
                                        int timeout_ms,
                                        bool capture_stdout) {
    SubprocessResult result;

    if (!is_binary_approved(binary_path)) {
        ZENITH_LOG_ERROR("Security policy violation: rejected non-whitelisted binary execution: " + binary_path);
        return result;
    }

    if (!fs::exists(binary_path)) {
        ZENITH_LOG_DEBUG("Whitelisted binary not found on filesystem: " + binary_path);
        return result;
    }

#ifdef __linux__
    int pipe_fd[2] = { -1, -1 };
    if (capture_stdout) {
        if (::pipe2(pipe_fd, O_CLOEXEC) != 0) {
            ZENITH_LOG_ERROR("Failed to create pipe for subprocess execution");
            return result;
        }
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        if (capture_stdout) {
            ::close(pipe_fd[0]);
            ::close(pipe_fd[1]);
        }
        ZENITH_LOG_ERROR("Failed to fork subprocess: " + binary_path);
        return result;
    }

    if (pid == 0) {
        // Child process
        // Redirect stdin to /dev/null
        int devnull = ::open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            if (!capture_stdout) {
                ::dup2(devnull, STDOUT_FILENO);
            }
            ::dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) ::close(devnull);
        }

        if (capture_stdout) {
            ::dup2(pipe_fd[1], STDOUT_FILENO);
            ::close(pipe_fd[0]);
            ::close(pipe_fd[1]);
        }

        // Prepare execve arguments
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(binary_path.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        // Sanitize environment
        char* const envp[] = {
            const_cast<char*>("PATH=/usr/bin:/bin"),
            const_cast<char*>("LC_ALL=C"),
            nullptr
        };

        ::execve(binary_path.c_str(), argv.data(), envp);
        ::_exit(127);
    }

    // Parent process
    result.executed = true;
    if (capture_stdout) {
        ::close(pipe_fd[1]);
    }

    auto start_time = std::chrono::steady_clock::now();
    int status = 0;
    bool finished = false;

    while (true) {
        pid_t res = ::waitpid(pid, &status, WNOHANG);
        if (res == pid) {
            finished = true;
            if (WIFEXITED(status)) {
                result.exit_code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                result.exit_code = 128 + WTERMSIG(status);
            }
            break;
        } else if (res < 0) {
            break;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        if (elapsed > timeout_ms) {
            result.timed_out = true;
            ::kill(pid, SIGKILL);
            ::waitpid(pid, &status, 0);
            ZENITH_LOG_WARN("Subprocess execution timed out: " + binary_path);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (capture_stdout) {
        char buf[512];
        ssize_t bytes_read = 0;
        while ((bytes_read = ::read(pipe_fd[0], buf, sizeof(buf) - 1)) > 0) {
            buf[bytes_read] = '\0';
            result.stdout_output.append(buf, bytes_read);
        }
        ::close(pipe_fd[0]);
    }

#else
    (void)args;
    (void)timeout_ms;
    (void)capture_stdout;
    result.executed = true;
    result.exit_code = 0;
#endif

    return result;
}

} // namespace zenith
