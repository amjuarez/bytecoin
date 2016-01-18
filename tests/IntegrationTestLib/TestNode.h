// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <system_error>
#include <INode.h>

namespace Tests {

class TestNode {
public:
  virtual bool startMining(size_t threadsCount, const std::string& address) = 0;
  virtual bool stopMining() = 0;
  virtual bool stopDaemon() = 0;
  virtual bool getBlockTemplate(const std::string& minerAddress, CryptoNote::Block& blockTemplate, uint64_t& difficulty) = 0;
  virtual bool submitBlock(const std::string& block) = 0;
  virtual bool getTailBlockId(Crypto::Hash& tailBlockId) = 0;
  virtual bool makeINode(std::unique_ptr<CryptoNote::INode>& node) = 0;
  virtual uint64_t getLocalHeight() = 0;

  std::unique_ptr<CryptoNote::INode> makeINode() {
    std::unique_ptr<CryptoNote::INode> node;
    if (!makeINode(node)) {
      throw std::runtime_error("Failed to create INode interface");
    }

    return node;
  }

  virtual ~TestNode() {}
};

}
