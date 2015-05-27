// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <boost/optional.hpp>
#include <boost/foreach.hpp>
#include <functional>

#include "misc_log_ex.h"
#include "storages/portable_storage_template_helper.h"
#include "serialization/enableable.h"
#include "serialization/keyvalue_serialization_overloads.h"

namespace CryptoNote {

class HttpClient;
  
namespace JsonRpc {

const int errParseError = -32700;
const int errInvalidRequest = -32600;
const int errMethodNotFound = -32601;
const int errInvalidParams = -32602;
const int errInternalError = -32603;

class JsonRpcError: public std::exception {
public:
  JsonRpcError();
  JsonRpcError(int c);
  JsonRpcError(int c, const std::string& msg);

#ifdef _MSC_VER
  virtual const char* what() const override {
#else
  virtual const char* what() const noexcept override {
#endif
    return message.c_str();
  }

  int code;
  std::string message;
};

typedef boost::optional<epee::serialization::storage_entry> OptionalId;

class JsonRpcRequest {
public:

  bool parseRequest(const std::string& requestBody) {
    if (!psReq.load_from_json(requestBody)) {
      throw JsonRpcError(errParseError);
    }

    OptionalId::value_type idValue;
    if (psReq.get_value("id", idValue, nullptr)) {
      id = idValue;
    }

    if (!psReq.get_value("method", method, nullptr)) {
      throw JsonRpcError(errInvalidRequest);
    }

    return true;
  }

  template <typename T>
  bool loadParams(T& v) const {
    return epee::serialization::kv_unserialize(v, 
      const_cast<epee::serialization::portable_storage&>(psReq), nullptr, "params");
  }

  template <typename T>
  bool setParams(const T& v) {
    return epee::serialization::kv_serialize(v, psReq, nullptr, "params");
  }

  const std::string& getMethod() const {
    return method;
  }

  void setMethod(const std::string& m) {
    method = m;
  }

  const OptionalId& getId() const {
    return id;
  }

  std::string getBody() {
    std::string reqBody;
    psReq.set_value("jsonrpc", std::string("2.0"), nullptr);
    psReq.set_value("method", method, nullptr);
    psReq.dump_as_json(reqBody);
    return reqBody;
  }

private:

  epee::serialization::portable_storage psReq;
  OptionalId id;
  std::string method;
};


class JsonRpcResponse {
public:

  void parse(const std::string& resonseBody) {
    if (!psResp.load_from_json(resonseBody)) {
      throw JsonRpcError(errParseError);
    }
  }

  void setId(const OptionalId& id) {
    if (id.is_initialized()) {
      psResp.set_value("id", id.get(), nullptr);
    }
  }

  void setError(const JsonRpcError& err) {
    auto errorSection = psResp.open_section("error", nullptr, true);
    psResp.set_value("code", err.code, errorSection);
    psResp.set_value("message", err.message, errorSection);
  }

  bool getError(JsonRpcError& err) {
    auto errorSection = psResp.open_section("error", nullptr, false);
    if (!errorSection) {
      return false;
    }

    psResp.get_value("code", err.code, errorSection);
    psResp.get_value("message", err.message, errorSection);
    return true;
  }

  std::string getBody() {
    std::string responseBody;
    psResp.set_value("jsonrpc", std::string("2.0"), nullptr);
    psResp.dump_as_json(responseBody);
    return responseBody;
  }

  template <typename T>
  bool setResult(const T& v) {
    return epee::serialization::kv_serialize(v, psResp, nullptr, "result");
  }

  template <typename T>
  bool getResult(T& v) const {
    return epee::serialization::kv_unserialize(v,
      const_cast<epee::serialization::portable_storage&>(psResp), nullptr, "result");
  }

private:
  epee::serialization::portable_storage psResp;
};


void invokeJsonRpcCommand(HttpClient& httpClient, JsonRpcRequest& req, JsonRpcResponse& res);

template <typename Request, typename Response>
void invokeJsonRpcCommand(HttpClient& httpClient, const std::string& method, const Request& req, Response& res) {
  JsonRpcRequest jsReq;
  JsonRpcResponse jsRes;

  jsReq.setMethod(method);
  jsReq.setParams(req);

  invokeJsonRpcCommand(httpClient, jsReq, jsRes);

  jsRes.getResult(res);
}

template <typename Request, typename Response, typename Handler>
bool invokeMethod(const JsonRpcRequest& jsReq, JsonRpcResponse& jsRes, Handler handler) {
  Request req;
  Response res;

  if (!jsReq.loadParams(req)) {
    throw JsonRpcError(JsonRpc::errInvalidParams);
  }

  bool result = handler(req, res);

  if (result) {
    if (!jsRes.setResult(res)) {
      throw JsonRpcError(JsonRpc::errInternalError);
    }
  }
  return result;
}

typedef std::function<bool(void*, const JsonRpcRequest& req, JsonRpcResponse& res)> JsonMemberMethod;

template <typename Class, typename Params, typename Result>
JsonMemberMethod makeMemberMethod(bool (Class::*handler)(const Params&, Result&)) {
  return [handler](void* obj, const JsonRpcRequest& req, JsonRpcResponse& res) {
    return JsonRpc::invokeMethod<Params, Result>(
      req, res, std::bind(handler, static_cast<Class*>(obj), std::placeholders::_1, std::placeholders::_2));
  };
}


}


}
