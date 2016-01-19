// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "LoggerMessage.h"

namespace Logging {

LoggerMessage::LoggerMessage(ILogger& logger, const std::string& category, Level level, const std::string& color)
  : std::ostream(this)
  , std::streambuf()
  , logger(logger)
  , category(category)
  , logLevel(level)
  , message(color)
  , timestamp(boost::posix_time::microsec_clock::local_time())
  , gotText(false) {
}

LoggerMessage::~LoggerMessage() {
  if (gotText) {
    (*this) << std::endl;
  }
}

#ifndef __linux__
LoggerMessage::LoggerMessage(LoggerMessage&& other)
  : std::ostream(std::move(other))
  , std::streambuf(std::move(other))
  , category(other.category)
  , logLevel(other.logLevel)
  , logger(other.logger)
  , message(other.message)
  , timestamp(boost::posix_time::microsec_clock::local_time())
  , gotText(false) {
  this->set_rdbuf(this);
}
#else
LoggerMessage::LoggerMessage(LoggerMessage&& other)
  : std::ostream(nullptr)
  , std::streambuf()
  , category(other.category)
  , logLevel(other.logLevel)
  , logger(other.logger)
  , message(other.message)
  , timestamp(boost::posix_time::microsec_clock::local_time())
  , gotText(false) {
  if (this != &other) {
    _M_tie = nullptr;
    _M_streambuf = nullptr;

    //ios_base swap
    std::swap(_M_streambuf_state, other._M_streambuf_state);
    std::swap(_M_exception, other._M_exception);
    std::swap(_M_flags, other._M_flags);
    std::swap(_M_precision, other._M_precision);
    std::swap(_M_width, other._M_width);

    std::swap(_M_callbacks, other._M_callbacks);
    std::swap(_M_ios_locale, other._M_ios_locale);
    //ios_base swap

    //streambuf swap
    char *_Pfirst = pbase();
    char *_Pnext = pptr();
    char *_Pend = epptr();
    char *_Gfirst = eback();
    char *_Gnext = gptr();
    char *_Gend = egptr();

    setp(other.pbase(), other.epptr());
    other.setp(_Pfirst, _Pend);

    setg(other.eback(), other.gptr(), other.egptr());
    other.setg(_Gfirst, _Gnext, _Gend);

    std::swap(_M_buf_locale, other._M_buf_locale);
    //streambuf swap

    std::swap(_M_fill, other._M_fill);
    std::swap(_M_tie, other._M_tie);
  }
  _M_streambuf = this;
}
#endif

int LoggerMessage::sync() {
  logger(category, logLevel, timestamp, message);
  gotText = false;
  message = DEFAULT;
  return 0;
}

int LoggerMessage::overflow(int c) {
  gotText = true;
  message += static_cast<char>(c);
  return 0;
}

}
