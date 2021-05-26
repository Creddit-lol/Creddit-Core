// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021 The CREDD developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_creddit.h"

#include "evo/specialtx.h"
#include "evo/providertx.h"
#include "evo/deterministicmns.h"
#include "messagesigner.h"
#include "netbase.h"
#include "policy/policy.h"
#include "primitives/transaction.h"
#include "script/sign.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

typedef std::map<COutPoint, std::pair<int, CAmount>> SimpleUTXOMap;

// static 0.1 CREDD fee used for the special txes in these tests
static const CAmount fee = 10000000;

static SimpleUTXOMap BuildSimpleUtxoMap(const std::vector<CTransaction>& txs)
{
    SimpleUTXOMap utxos;
    for (size_t i = 0; i < txs.size(); i++) {
        auto& tx = txs[i];
        for (size_t j = 0; j < tx.vout.size(); j++) {
            utxos.emplace(std::piecewise_construct,
                          std::forward_as_tuple(tx.GetHash(), j),
                          std::forward_as_tuple((int)i + 1, tx.vout[j].nValue));
        }
    }
    return utxos;
}

static std::vector<COutPoint> SelectUTXOs(SimpleUTXOMap& utoxs, CAmount amount, CAmount& changeRet)
{
    changeRet = 0;
    amount += fee;

    std::vector<COutPoint> selectedUtxos;
    CAmount selectedAmount = 0;
    int chainHeight = chainActive.Height();
    while (!utoxs.empty()) {
        bool found = false;
        for (auto it = utoxs.begin(); it != utoxs.end(); ++it) {
            if (chainHeight - it->second.first < 100) {
                continue;
            }

            found = true;
            selectedAmount += it->second.second;
            selectedUtxos.emplace_back(it->first);
            utoxs.erase(it);
            break;
        }
        BOOST_ASSERT(found);
        if (selectedAmount >= amount) {
            changeRet = selectedAmount - amount;
            break;
        }
    }

    return selectedUtxos;
}

static void FundTransaction(CMutableTransaction& tx, SimpleUTXOMap& utoxs, const CScript& scriptPayout, const CScript& scriptChange, CAmount amount)
{
    CAmount change;
    auto inputs = SelectUTXOs(utoxs, amount, change);
    for (size_t i = 0; i < inputs.size(); i++) {
        tx.vin.emplace_back(inputs[i]);
    }
    tx.vout.emplace_back(CTxOut(amount, scriptPayout));
    if (change != 0) {
        tx.vout.emplace_back(change, scriptChange);
    }
}

static void SignTransaction(CMutableTransaction& tx, const CKey& coinbaseKey)
{
    CBasicKeyStore tempKeystore;
    tempKeystore.AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());

    for (size_t i = 0; i < tx.vin.size(); i++) {
        CTransactionRef txFrom;
        uint256 hashBlock;
        BOOST_ASSERT(GetTransaction(tx.vin[i].prevout.hash, txFrom, hashBlock));
        BOOST_ASSERT(SignSignature(tempKeystore, *txFrom, tx, i, SIGHASH_ALL));
    }
}

static CKey GetRandomKey()
{
    CKey keyRet;
    keyRet.MakeNewKey(true);
    return keyRet;
}

// Creates a ProRegTx.
// - if optCollateralOut is nullopt, generate a new collateral in the first output of the tx
// - otherwise reference *optCollateralOut as external collateral
static CMutableTransaction CreateProRegTx(Optional<COutPoint> optCollateralOut, SimpleUTXOMap& utxos, int port, const CScript& scriptPayout, const CKey& coinbaseKey, const CKey& ownerKey, const CKey& operatorKey)
{
    ProRegPL pl;
    pl.collateralOutpoint = (optCollateralOut ? *optCollateralOut : COutPoint(UINT256_ZERO, 0));
    pl.addr = LookupNumeric("1.1.1.1", port);
    pl.keyIDOwner = ownerKey.GetPubKey().GetID();
    pl.keyIDOperator = operatorKey.GetPubKey().GetID();
    pl.keyIDVoting = ownerKey.GetPubKey().GetID();
    pl.scriptPayout = scriptPayout;

    CMutableTransaction tx;
    tx.nVersion = CTransaction::TxVersion::SAPLING;
    tx.nType = CTransaction::TxType::PROREG;
    FundTransaction(tx, utxos, scriptPayout,
                    GetScriptForDestination(coinbaseKey.GetPubKey().GetID()),
                    (optCollateralOut ? 0 : Params().GetConsensus().nMNCollateralAmt));

    pl.inputsHash = CalcTxInputsHash(tx);
    SetTxPayload(tx, pl);
    SignTransaction(tx, coinbaseKey);

    return tx;
}

static CScript GenerateRandomAddress()
{
    CKey key;
    key.MakeNewKey(false);
    return GetScriptForDestination(key.GetPubKey().GetID());
}

template<typename ProPL>
static CMutableTransaction MalleateProTxPayout(const CMutableTransaction& tx)
{
    ProPL pl;
    GetTxPayload(tx, pl);
    pl.scriptPayout = GenerateRandomAddress();
    CMutableTransaction tx2 = tx;
    SetTxPayload(tx2, pl);
    return tx2;
}

static bool CheckTransactionSignature(const CMutableTransaction& tx)
{
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const auto& txin = tx.vin[i];
        CTransactionRef txFrom;
        uint256 hashBlock;
        BOOST_ASSERT(GetTransaction(txin.prevout.hash, txFrom, hashBlock));

        CAmount amount = txFrom->vout[txin.prevout.n].nValue;
        if (!VerifyScript(txin.scriptSig, txFrom->vout[txin.prevout.n].scriptPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker(&tx, i, amount), tx.GetRequiredSigVersion())) {
            return false;
        }
    }
    return true;
}

BOOST_AUTO_TEST_SUITE(deterministicmns_tests)

BOOST_FIXTURE_TEST_CASE(dip3_protx, TestChain400Setup)
{
    auto utxos = BuildSimpleUtxoMap(coinbaseTxns);

    CBlockIndex* chainTip = chainActive.Tip();
    int nHeight = chainTip->nHeight;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V6_0, nHeight);
    int port = 1;

    // these maps are only populated, but not used for now. They will be needed later on, in the next commits.
    std::vector<uint256> dmnHashes;
    std::map<uint256, CKey> ownerKeys;
    std::map<uint256, CKey> operatorKeys;

    // register one MN per block
    for (size_t i = 0; i < 6; i++) {
        const CKey& ownerKey = GetRandomKey();
        const CKey& operatorKey = GetRandomKey();
        auto tx = CreateProRegTx(nullopt, utxos, port++, GenerateRandomAddress(), coinbaseKey, ownerKey, operatorKey);
        const uint256& txid = tx.GetHash();
        dmnHashes.emplace_back(txid);
        ownerKeys.emplace(txid, ownerKey);
        operatorKeys.emplace(txid, operatorKey);

        CValidationState dummyState;
        BOOST_CHECK(CheckSpecialTx(tx, chainTip, dummyState));
        BOOST_CHECK(CheckTransactionSignature(tx));

        // also verify that payloads are not malleable after they have been signed
        // the form of ProRegTx we use here is one with a collateral included, so there is no signature inside the
        // payload itself. This means, we need to rely on script verification, which takes the hash of the extra payload
        // into account
        auto tx2 = MalleateProTxPayout<ProRegPL>(tx);
        // Technically, the payload is still valid...
        BOOST_CHECK(CheckSpecialTx(tx2, chainTip, dummyState));
        // But the signature should not verify anymore
        BOOST_CHECK(!CheckTransactionSignature(tx2));

        CreateAndProcessBlock({tx}, coinbaseKey);
        chainTip = chainActive.Tip();
        BOOST_CHECK_EQUAL(chainTip->nHeight, nHeight + 1);

        deterministicMNManager->UpdatedBlockTip(chainTip);
        BOOST_CHECK(deterministicMNManager->GetListAtChainTip().HasMN(txid));

        // Add change to the utxos map
        if (tx.vout.size() > 1) {
            utxos.emplace(COutPoint(tx.GetHash(), 1), std::make_pair(nHeight + 1, tx.vout[1].nValue));
        }

        nHeight++;
    }
    // Mine 30 more blocks
    for (size_t i = 0; i < 30; i++) {
        CreateAndProcessBlock({}, coinbaseKey);
        chainTip = chainActive.Tip();
        BOOST_CHECK_EQUAL(chainTip->nHeight, ++nHeight);
        deterministicMNManager->UpdatedBlockTip(chainTip);
    }

    // Try to register used owner key
    {
        SimpleUTXOMap utxos_copy(utxos);
        const CKey& ownerKey = ownerKeys.at(dmnHashes[InsecureRandRange(dmnHashes.size())]);
        auto tx = CreateProRegTx(nullopt, utxos_copy, port, GenerateRandomAddress(), coinbaseKey, ownerKey, GetRandomKey());
        CValidationState state;
        BOOST_CHECK(!CheckSpecialTx(tx, chainTip, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-protx-dup-owner-key");
    }
    // Try to register used operator key
    {
        SimpleUTXOMap utxos_copy(utxos);
        const CKey& operatorKey = operatorKeys.at(dmnHashes[InsecureRandRange(dmnHashes.size())]);
        auto tx = CreateProRegTx(nullopt, utxos_copy, port, GenerateRandomAddress(), coinbaseKey, GetRandomKey(), operatorKey);
        CValidationState state;
        BOOST_CHECK(!CheckSpecialTx(tx, chainTip, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-protx-dup-operator-key");
    }
    // Try to register used IP address
    {
        SimpleUTXOMap utxos_copy(utxos);
        auto tx = CreateProRegTx(nullopt, utxos_copy, 1 + InsecureRandRange(port-1), GenerateRandomAddress(), coinbaseKey, GetRandomKey(), GetRandomKey());
        CValidationState state;
        BOOST_CHECK(!CheckSpecialTx(tx, chainTip, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-protx-dup-IP-address");
    }
    // Block with two ProReg txes using same owner key
    {
        SimpleUTXOMap utxos_copy(utxos);
        const CKey& ownerKey = GetRandomKey();
        const CKey& operatorKey1 = GetRandomKey();
        const CKey& operatorKey2 = GetRandomKey();
        auto tx1 = CreateProRegTx(nullopt, utxos_copy, port, GenerateRandomAddress(), coinbaseKey, ownerKey, operatorKey1);
        auto tx2 = CreateProRegTx(nullopt, utxos_copy, (port+1), GenerateRandomAddress(), coinbaseKey, ownerKey, operatorKey2);
        CBlock block = CreateBlock({tx1, tx2}, coinbaseKey);
        CBlockIndex indexFake(block);
        indexFake.nHeight = nHeight;
        indexFake.pprev = chainTip;
        CValidationState state;
        BOOST_CHECK(!ProcessSpecialTxsInBlock(block, &indexFake, state, true));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-protx-dup-owner-key");
        ProcessNewBlock(state, nullptr, std::make_shared<const CBlock>(block), nullptr);
        BOOST_CHECK_EQUAL(chainActive.Height(), nHeight);   // bad block not connected
    }
    // Block with two ProReg txes using same operator key
    {
        SimpleUTXOMap utxos_copy(utxos);
        const CKey& ownerKey1 = GetRandomKey();
        const CKey& ownerKey2 = GetRandomKey();
        const CKey& operatorKey = GetRandomKey();
        auto tx1 = CreateProRegTx(nullopt, utxos_copy, port, GenerateRandomAddress(), coinbaseKey, ownerKey1, operatorKey);
        auto tx2 = CreateProRegTx(nullopt, utxos_copy, (port+1), GenerateRandomAddress(), coinbaseKey, ownerKey2, operatorKey);
        CBlock block = CreateBlock({tx1, tx2}, coinbaseKey);
        CBlockIndex indexFake(block);
        indexFake.nHeight = nHeight;
        indexFake.pprev = chainTip;
        CValidationState state;
        BOOST_CHECK(!ProcessSpecialTxsInBlock(block, &indexFake, state, true));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-protx-dup-operator-key");
        ProcessNewBlock(state, nullptr, std::make_shared<const CBlock>(block), nullptr);
        BOOST_CHECK_EQUAL(chainActive.Height(), nHeight);   // bad block not connected
    }
    // Block with two ProReg txes using ip address
    {
        SimpleUTXOMap utxos_copy(utxos);
        auto tx1 = CreateProRegTx(nullopt, utxos_copy, port, GenerateRandomAddress(), coinbaseKey, GetRandomKey(), GetRandomKey());
        auto tx2 = CreateProRegTx(nullopt, utxos_copy, port, GenerateRandomAddress(), coinbaseKey, GetRandomKey(), GetRandomKey());
        CBlock block = CreateBlock({tx1, tx2}, coinbaseKey);
        CBlockIndex indexFake(block);
        indexFake.nHeight = nHeight;
        indexFake.pprev = chainTip;
        CValidationState state;
        BOOST_CHECK(!ProcessSpecialTxsInBlock(block, &indexFake, state, true));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-protx-dup-IP-address");
        ProcessNewBlock(state, nullptr, std::make_shared<const CBlock>(block), nullptr);
        BOOST_CHECK_EQUAL(chainActive.Height(), nHeight);   // bad block not connected
    }

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V6_0, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

BOOST_AUTO_TEST_SUITE_END()
