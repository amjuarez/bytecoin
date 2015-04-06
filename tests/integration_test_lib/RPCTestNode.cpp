// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
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

#include "RPCTestNode.h"

#include <vector>
#include <thread>

#include "rpc/core_rpc_server_commands_defs.h"
#include "node_rpc_proxy/NodeRpcProxy.h"

#include "serialization/JsonOutputStreamSerializer.h"
#include "serialization/JsonInputStreamSerializer.h"
#include "storages/portable_storage_base.h"
#include "storages/portable_storage_template_helper.h"

#include "../contrib/epee/include/net/jsonrpc_structs.h"

#include <System/TcpConnection.h>
#include <System/TcpConnector.h>
#include <System/TcpStream.h>
#include "HTTP/HttpParser.h"

#include "CoreRpcSerialization.h"
#include "Logger.h"

using namespace Tests;
using namespace cryptonote;
using namespace System;

void RPCTestNode::prepareRequest(HttpRequest& httpReq, const std::string& method, const std::string& params){
  httpReq.setUrl(method);
  httpReq.addHeader("Host", "127.0.0.1:" + boost::lexical_cast<std::string>(m_rpcPort));
  httpReq.addHeader("Content-Type", "application/json-rpc");
  httpReq.setBody(params);
}

void RPCTestNode::sendRequest(const HttpRequest& httpReq, HttpResponse& httpResp) {
  TcpConnector connector(m_dispatcher, "127.0.0.1", m_rpcPort);
  TcpConnection connection = connector.connect();
  TcpStreambuf streambuf(connection);
  std::iostream connectionStream(&streambuf);
  LOG_DEBUG("invoke rpc:" + httpReq.getMethod() + " " + httpReq.getBody());
  connectionStream << httpReq;
  connectionStream.flush();
  HttpParser parser;
  parser.receiveResponse(connectionStream, httpResp);  
}

bool RPCTestNode::startMining(size_t threadsCount, const std::string& address) { 
  LOG_DEBUG("startMining()");
  using namespace cryptonote;
  COMMAND_RPC_START_MINING::request req;
  COMMAND_RPC_START_MINING::response resp;
  req.miner_address = address;
  req.threads_count = threadsCount;
  std::stringstream requestStream;
  JsonOutputStreamSerializer enumerator;
  enumerator(req, "");
  requestStream << enumerator;
  HttpRequest httpReq;
  prepareRequest(httpReq, "/start_mining", requestStream.str());
  HttpResponse httpResp;
  sendRequest(httpReq, httpResp);
  if (httpResp.getStatus() != HttpResponse::STATUS_200) return false;
  std::stringstream responseStream(httpResp.getBody());
  JsonInputStreamSerializer en(responseStream);
  en(resp, "");
  if (resp.status != CORE_RPC_STATUS_OK) {
    std::cout << "startMining() RPC call fail: " << resp.status;
    return false;
  }

  return true;
}

bool RPCTestNode::submitBlock(const std::string& block) {
  HttpRequest httpReq;
  httpReq.setUrl("/json_rpc");
  httpReq.addHeader("Host", "127.0.0.1:" + boost::lexical_cast<std::string>(m_rpcPort));
  httpReq.addHeader("Content-Type", "application/json-rpc");
  JsonValue request(cryptonote::JsonValue::OBJECT);
  JsonValue jsonRpc;
  jsonRpc = "2.0";
  request.insert("jsonrpc", jsonRpc);
  JsonValue methodString;
  methodString = "submitblock";
  request.insert("method", methodString);
  JsonValue id;
  id = "sync";
  request.insert("id", id);
  JsonValue params(JsonValue::ARRAY);
  JsonValue blockstr;
  blockstr = block.c_str();
  params.pushBack(blockstr);
  request.insert("params", params);
  std::stringstream jsonOutputStream;
  jsonOutputStream << request;
  httpReq.setBody(jsonOutputStream.str());
  TcpConnector connector(m_dispatcher, "127.0.0.1", m_rpcPort);
  TcpConnection connection = connector.connect();
  TcpStreambuf streambuf(connection);
  std::iostream connectionStream(&streambuf);
  LOG_DEBUG("invoke json-rpc: " + httpReq.getBody());
  connectionStream << httpReq;
  connectionStream.flush();
  HttpResponse httpResp;
  HttpParser parser;
  parser.receiveResponse(connectionStream, httpResp);
  connectionStream.flush();
  if (httpResp.getStatus() != HttpResponse::STATUS_200) return false;

  epee::serialization::portable_storage ps;
  if (!ps.load_from_json(httpResp.getBody())) {
    LOG_ERROR("cannot parse response from daemon: " + httpResp.getBody());
    return false;
  }

  epee::json_rpc::response<COMMAND_RPC_SUBMITBLOCK::response, epee::json_rpc::error> jsonRpcResponse;
  jsonRpcResponse.load(ps);

  if (jsonRpcResponse.error.code || jsonRpcResponse.error.message.size()) {
    LOG_ERROR("RPC call of submit_block returned error: " + TO_STRING(jsonRpcResponse.error.code) + ", message: " + jsonRpcResponse.error.message);
    return false;
  }
   
  if (jsonRpcResponse.result.status != CORE_RPC_STATUS_OK)  return false;
  return true;
}

bool RPCTestNode::stopMining() { 
  LOG_DEBUG("stopMining()");
  using namespace cryptonote;
  COMMAND_RPC_STOP_MINING::request req;
  COMMAND_RPC_STOP_MINING::response resp;
  std::stringstream requestStream;
  JsonOutputStreamSerializer enumerator;
  enumerator(req, "");
  requestStream << enumerator;
  HttpRequest httpReq;
  prepareRequest(httpReq, "/stop_mining", requestStream.str());
  HttpResponse httpResp;
  sendRequest(httpReq, httpResp);
  if (httpResp.getStatus() != HttpResponse::STATUS_200) return false;
  std::stringstream responseStream(httpResp.getBody());
  JsonInputStreamSerializer en(responseStream);
  en(resp, "");
  if (resp.status != CORE_RPC_STATUS_OK) {
    std::cout << "stopMining() RPC call fail: " << resp.status;
    return false;
  }

  return true;
}

bool RPCTestNode::makeINode(std::unique_ptr<CryptoNote::INode>& node) {
  node.reset(new cryptonote::NodeRpcProxy("127.0.0.1", m_rpcPort));
  node->init([&](std::error_code ec) {
    if (ec) {
      LOG_ERROR("init error: " + ec.message() + ':' + TO_STRING(ec.value()));
    } else {
      LOG_DEBUG("NodeRPCProxy on port " + TO_STRING(m_rpcPort) + " initialized");
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(2000)); //for initial update  
  return true;
}


bool RPCTestNode::stopDaemon() {
  LOG_DEBUG("stopDaemon()");
  using namespace cryptonote;
  COMMAND_RPC_STOP_DAEMON::request req;
  COMMAND_RPC_STOP_DAEMON::response resp;
  std::stringstream requestStream;
  JsonOutputStreamSerializer enumerator;
  enumerator(req, "");
  requestStream << enumerator;
  HttpRequest httpReq;
  prepareRequest(httpReq, "/stop_daemon", requestStream.str());
  HttpResponse httpResp;
  sendRequest(httpReq, httpResp);
  if (httpResp.getStatus() != HttpResponse::STATUS_200) return false;
  std::stringstream responseStream(httpResp.getBody());
  JsonInputStreamSerializer en(responseStream);
  en(resp, "");
  if (resp.status != CORE_RPC_STATUS_OK) {
    std::cout << "stopDaemon() RPC call fail: " << resp.status;
    return false;
  }

  return true;
}