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

#pragma once

#include <system_error>
#include <INode.h>


namespace Tests {
  namespace Common {

    class TestNode {
    public:
      virtual bool startMining(size_t threadsCount, const std::string& address) = 0;
      virtual bool stopMining() = 0;
      virtual bool stopDaemon() = 0;
      virtual bool submitBlock(const std::string& block) = 0;
      virtual bool makeINode(std::unique_ptr<CryptoNote::INode>& node) = 0;
      virtual ~TestNode() { }
    };
  }
}