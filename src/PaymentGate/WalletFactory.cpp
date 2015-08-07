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

#include "WalletFactory.h"

#include "NodeRpcProxy/NodeRpcProxy.h"
#include "Wallet/WalletGreen.h"
#include "CryptoNoteCore/Currency.h"

#include <stdlib.h>
#include <future>

namespace PaymentService {

WalletFactory WalletFactory::factory;

WalletFactory::WalletFactory() {
}

WalletFactory::~WalletFactory() {
}

CryptoNote::IWallet* WalletFactory::createWallet(const CryptoNote::Currency& currency, CryptoNote::INode& node, System::Dispatcher& dispatcher) {
  CryptoNote::IWallet* wallet = new CryptoNote::WalletGreen(dispatcher, currency, node);
  return wallet;
}

}
