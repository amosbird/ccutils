#pragma once

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <functional>
#include <iostream>
#include <sstream>

struct PerfCall {
    /// pid of last started perf process
    pid_t pid;
    void startProfile(const std::string& name, std::string perfMode = "record") {
        std::string filename = name.find(".data") == std::string::npos ? (name + ".data") : name;

        // Launch profiler
        std::stringstream s;
        s << getpid();
        pid = fork();
        if (pid == 0) {
            auto fd = open("/dev/null", O_RDWR);
            dup2(fd, 1);
            dup2(fd, 2);
            exit(execl("/usr/bin/env", "env", "perf", perfMode.c_str(), "-o", filename.c_str(), "-p", s.str().c_str(),
                       nullptr));
        }
    }

    void endProfile() {
        // Kill profiler
        kill(pid, SIGINT);
        waitpid(pid, nullptr, 0);
    }

    void profile(const std::string& name, std::function<void()> body) {
        startProfile(name);
        // Run body
        body();
        endProfile();
    }

    void profile(std::function<void()> body) { profile("perf.data", body); }
};
