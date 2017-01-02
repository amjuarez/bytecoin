// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdexcept>
#include <algorithm>
#include <string>

#include "IWalletLegacy.h"

namespace CryptoNote {
class ISerializer;

struct UnconfirmedTransferDetails;
struct WalletLegacyTransaction;
struct WalletLegacyTransfer;
struct DepositInfo;
struct Deposit;
struct UnconfirmedSpentDepositDetails;

void serialize(UnconfirmedTransferDetails& utd, ISerializer& serializer);
void serialize(UnconfirmedSpentDepositDetails& details, ISerializer& serializer);
void serialize(WalletLegacyTransaction& txi, ISerializer& serializer);
void serialize(WalletLegacyTransfer& tr, ISerializer& serializer);
void serialize(DepositInfo& depositInfo, ISerializer& serializer);
void serialize(Deposit& deposit, ISerializer& serializer);

}
