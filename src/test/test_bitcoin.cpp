// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test_bitcoin.h"

#include "crypto/common.h"

#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "key.h"
#include "main.h"
#include "miner.h"
#include "pubkey.h"
#include "random.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet/db.h"
#include "wallet/wallet.h"
#endif

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

#include "librustzcash.h"

CClientUIInterface uiInterface; // Declared but not defined in ui_interface.h
CWallet* pwalletMain;
ZCJoinSplit *pzcashParams;

extern bool fPrintToConsole;
extern void noui_connect();

JoinSplitTestingSetup::JoinSplitTestingSetup(CBaseChainParams::Network network) : BasicTestingSetup(network)
{
    boost::filesystem::path pk_path = ZC_GetParamsDir() / "sprout-proving.key";
    boost::filesystem::path vk_path = ZC_GetParamsDir() / "sprout-verifying.key";
    pzcashParams = ZCJoinSplit::Prepared(vk_path.string(), pk_path.string());

    boost::filesystem::path sapling_spend = ZC_GetParamsDir() / "sapling-spend-testnet.params";
    boost::filesystem::path sapling_output = ZC_GetParamsDir() / "sapling-output-testnet.params";
    boost::filesystem::path sprout_groth16 = ZC_GetParamsDir() / "sprout-groth16-testnet.params";

    std::string sapling_spend_str = sapling_spend.string();
    std::string sapling_output_str = sapling_output.string();
    std::string sprout_groth16_str = sprout_groth16.string();

    librustzcash_init_zksnark_params(
        sapling_spend_str.c_str(),
        sapling_output_str.c_str(),
        sprout_groth16_str.c_str()
    );
}

JoinSplitTestingSetup::~JoinSplitTestingSetup()
{
    delete pzcashParams;
}

BasicTestingSetup::BasicTestingSetup(CBaseChainParams::Network network)
{
    assert(init_and_check_sodium() != -1);
    ECC_Start();
    SetupEnvironment();
    fPrintToDebugLog = false; // don't want to write to debug.log file
    fCheckBlockIndex = true;
    SelectParams(network);
    noui_connect();
}

BasicTestingSetup::~BasicTestingSetup()
{
    ECC_Stop();
}

TestingSetup::TestingSetup(CBaseChainParams::Network network) : JoinSplitTestingSetup(network)
{
#ifdef ENABLE_WALLET
        bitdb.MakeMock();
#endif
        ClearDatadirCache();
        pathTemp = GetTempPath() / strprintf("test_bitcoin_%lu_%i", (unsigned long)GetTime(), (int)(GetRand(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        pblocktree = new CBlockTreeDB(1 << 20, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);
        InitBlockIndex();
#ifdef ENABLE_WALLET
        bool fFirstRun;
        pwalletMain = new CWallet("wallet.dat");
        pwalletMain->LoadWallet(fFirstRun);
        RegisterValidationInterface(pwalletMain);
#endif
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        RegisterNodeSignals(GetNodeSignals());
}

TestingSetup::~TestingSetup()
{
        UnregisterNodeSignals(GetNodeSignals());
        threadGroup.interrupt_all();
        threadGroup.join_all();
#ifdef ENABLE_WALLET
        UnregisterValidationInterface(pwalletMain);
        delete pwalletMain;
        pwalletMain = NULL;
#endif
        UnloadBlockIndex();
        delete pcoinsTip;
        delete pcoinsdbview;
        delete pblocktree;
#ifdef ENABLE_WALLET
        bitdb.Flush(true);
        bitdb.Reset();
#endif
        boost::filesystem::remove_all(pathTemp);
}

TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // Generate a 100-block chain:
    coinbaseKey.MakeNewKey(true);
    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    for (int i = 0; i < COINBASE_MATURITY; i++)
    {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        coinbaseTxns.push_back(b.vtx[0]);
    }
}

//
// Create a new block with just given transactions, coinbase paying to
// scriptPubKey, and try to add it to the current chain.
//
CBlock
TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
    CBlockTemplate *pblocktemplate = CreateNewBlock(scriptPubKey);
    CBlock& block = pblocktemplate->block;

    // Replace mempool-selected txns with just coinbase plus passed-in txns:
    block.vtx.resize(1);
    BOOST_FOREACH(const CMutableTransaction& tx, txns)
        block.vtx.push_back(tx);
    // IncrementExtraNonce creates a valid coinbase and merkleRoot
    unsigned int extraNonce = 0;
    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

    while (!CheckProofOfWork(block.GetHash(), block.nBits, Params(CBaseChainParams::REGTEST).GetConsensus())) {
        block.nNonce = ArithToUint256(UintToArith256(block.nNonce) + 1);
    }

    CValidationState state;
    ProcessNewBlock(state, NULL, &block, true, NULL);

    CBlock result = block;
    delete pblocktemplate;
    return result;
}

TestChain100Setup::~TestChain100Setup()
{
}


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(CMutableTransaction &tx, CTxMemPool *pool) {
    return CTxMemPoolEntry(tx, nFee, nTime, dPriority, nHeight,
                           pool ? pool->HasNoInputsOf(tx) : hadNoDependencies,
                           spendsCoinbase, nBranchId);
}

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

bool ShutdownRequested()
{
  return false;
}
