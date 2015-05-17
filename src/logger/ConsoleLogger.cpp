#include "ConsoleLogger.h"
#include <iostream>
#include <sstream>
#include <vector>
#if defined(_WIN32)
#include <io.h>
#include <Windows.h>
#else
#include <unistd.h>
#endif


using namespace Log;

ConsoleLogger::ConsoleLogger(ILogger::Level level) : CommonLogger(level) {
}

void ConsoleLogger::doLogString(Level level, boost::posix_time::ptime time, const std::string& message) {
  std::vector<std::pair<std::string, std::string> > coloredStrings;
  {
    std::stringstream ss(message);
    char c;
    std::string color = "";
    std::string text =  "";
    ss.read(&c, 1);
    while (!ss.eof()) {
      if (c == ILogger::COLOR_DELIMETER) {
        coloredStrings.push_back(std::make_pair(color, text));
        color.clear();
        text.clear();
        color += COLOR_DELIMETER;
        ss.read(&c, 1);
        while (c != ILogger::COLOR_DELIMETER) {
          color += c;
          ss.read(&c, 1);
        }
        color += COLOR_DELIMETER;
      } else {
        text += c;
      }
      ss.read(&c, 1);
    }
    coloredStrings.push_back(std::make_pair(color, text));
    coloredStrings[0].first = coloredStrings[1].first;
    coloredStrings[0].second = boost::posix_time::to_simple_string(time) + ILogger::LEVEL_NAMES[level];
  }

  std::lock_guard<std::mutex> lock(mutex);
  for (size_t stringNumber = 0 ; stringNumber < coloredStrings.size(); ++stringNumber) {
    if (coloredStrings[stringNumber].second.empty()) continue;
    std::string color = coloredStrings[stringNumber].first;

    if (color == BLUE) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_BLUE);
#else
      std::cout << "\033[0;34m";
#endif      
    } else if (color == GREEN) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_GREEN);
#else
      std::cout << "\033[0;32m";
#endif
    } else if (color == RED) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_RED);
#else
      std::cout << "\033[0;31m";
#endif
    } else if (color == YELLOW) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_RED | FOREGROUND_GREEN);
#else
      std::cout << "\033[0;33m";
#endif
    } else if (color == WHITE) {
#ifdef _WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
      std::cout << "\033[0;37m";
#endif
    } else if (color == CYAN) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
      std::cout << "\033[0;36m";
#endif
    } else if (color == MAGENTA) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_RED | FOREGROUND_BLUE);
#else
      std::cout << "\033[0;35m";
#endif
    } else if (color == BRIGHT_BLUE) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#else
      std::cout << "\033[1;34m";
#endif      
    } else if (color == BRIGHT_GREEN) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
#else
      std::cout << "\033[1;32m";
#endif
    } else if (color == BRIGHT_RED) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_RED | FOREGROUND_INTENSITY);
#else
      std::cout << "\033[1;31m";
#endif
    } else if (color == BRIGHT_YELLOW) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
#else
      std::cout << "\033[1;33m";
#endif
    } else if (color == BRIGHT_WHITE) {
#ifdef _WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#else
      std::cout << "\033[1;37m";
#endif
    } else if (color == BRIGHT_CYAN) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#else
      std::cout << "\033[1;36m";
#endif
    } else if (color == BRIGHT_MAGENTA) {
#ifdef WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#else
      std::cout << "\033[1;35m";
#endif
    } else {
#ifdef _WIN32
      HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      SetConsoleTextAttribute(h_stdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
      std::cout << "\033[0m";
#endif
    }

    std::cout << coloredStrings[stringNumber].second;
  }
}
