#pragma once

#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ccutils {

inline pid_t perf_pid = 0;

class Perf {
public:
    Perf(const std::string& name = "perf.data", std::string perfMode = "record") {
        std::string filename = name.find(".data") == std::string::npos ? (name + ".data") : name;
        std::stringstream s;
        s << getpid();
        perf_pid = fork();
        if (perf_pid == 0) {
            signal(SIGHUP, SIG_IGN);
            auto fd = open("/dev/null", O_RDWR);
            dup2(fd, 0);
            dup2(fd, 1);
            dup2(fd, 2);
            exit(execl("/usr/bin/env", "env", "perf", perfMode.c_str(), "-o", filename.c_str(),
                "-p", s.str().c_str(), nullptr));
        }
    }

    ~Perf() {
        kill(perf_pid, SIGINT);
        waitpid(perf_pid, nullptr, 0);
    }
};
}
