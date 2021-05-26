// Copyright (c) 2017-2020 The CREDD developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CREDD_ZEROCOIN_H
#define CREDD_ZEROCOIN_H

#include "uint256.h"
#include "libzerocoin/bignum.h"

uint256 GetPubCoinHash(const CBigNum& bnValue);

#endif //CREDD_ZEROCOIN_H
