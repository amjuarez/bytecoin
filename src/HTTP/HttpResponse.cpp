// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "HttpResponse.h"

#include <stdexcept>

namespace {

const char* getStatusString(cryptonote::HttpResponse::HTTP_STATUS status) {
  switch (status) {
  case cryptonote::HttpResponse::STATUS_200:
    return "200 OK";
  case cryptonote::HttpResponse::STATUS_404:
    return "404 Not Found";
  case cryptonote::HttpResponse::STATUS_500:
    return "500 Internal Server Error";
  default:
    throw std::runtime_error("Unknown HTTP status code is given");
  }

  return ""; //unaccessible
}


} //namespace

namespace cryptonote {

HttpResponse::HttpResponse() {
  status = STATUS_200;
  headers["Server"] = "Cryptonote-based HTTP server";
}

void HttpResponse::setStatus(HTTP_STATUS s) {
  status = s;
}

void HttpResponse::addHeader(const std::string& name, const std::string& value) {
  headers[name] = value;
}

void HttpResponse::setBody(const std::string& b) {
  body = b;
  if (!body.empty()) {
    headers["Content-Length"] = std::to_string(body.size());
  } else {
    headers.erase("Content-Length");
  }
}

std::ostream& HttpResponse::printHttpResponse(std::ostream& os) const {
  os << "HTTP/1.1 " << getStatusString(status) << "\r\n";

  for (auto pair: headers) {
    os << pair.first << ": " << pair.second << "\r\n";
  }
  os << "\r\n";

  if (!body.empty()) {
    os << body;
  }

  return os;
}

} //namespace cryptonote


