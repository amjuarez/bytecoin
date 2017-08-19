// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Process.h"

#include <cstdlib>
#include <sstream>
#include <stdexcept>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace Tests {

  void Process::startChild(const std::string& executablePath, const std::vector<std::string>& args) {
   
#if defined WIN32
    std::stringstream ss;
    ss << "start /MIN " << executablePath;

    for (const auto& arg: args) {
      ss << " \"" << arg << "\"";
    }

    auto cmdline = ss.str();
    system(cmdline.c_str());

#else
    std::vector<const char*> cargs;
    cargs.push_back(executablePath.c_str());
    for (const auto& arg : args) {
      cargs.push_back(arg.c_str());
    }

    cargs.push_back(nullptr);

    auto pid = fork();

    if (pid == 0) {
      if (execv(executablePath.c_str(), (char**)&cargs[0]) == -1) {
        printf("Failed to start %s: %d\n", executablePath.c_str(), errno);
        exit(404);
      }
    } else if (pid > 0) {
      m_pid = pid;
    } else if (pid < 0) {
      throw std::runtime_error("fork() failed");
    }
#endif

  }

  void Process::wait() {
#ifndef _WIN32
    if (m_pid == 0) {
      return;
    }

    int status;
    waitpid(m_pid, &status, 0);
    m_pid = 0;
#endif
  }

}
