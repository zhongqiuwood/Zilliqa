/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#include <algorithm>
#include <chrono>
#include <thread>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

#ifndef IS_LOOKUP_NODE
void DirectoryService::ComposeDSBlock()
{
    LOG_MARKER();

    // Compute hash of previous DS block header
    BlockHash prevHash;
    if (m_mediator.m_dsBlockChain.GetBlockCount() > 0)
    {
        DSBlock lastBlock = m_mediator.m_dsBlockChain.GetLastBlock();
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        const DSBlockHeader& lastHeader = lastBlock.GetHeader();
        lastHeader.Serialize(vec, 0);
        sha2.Update(vec);
        const vector<unsigned char>& resVec = sha2.Finalize();
        copy(resVec.begin(), resVec.end(), prevHash.asArray().begin());
    }

    // Assemble DS block header

    lock_guard<mutex> g(m_mutexAllPOW);
    const PubKey& winnerKey = m_allPoWs.front().first;
    const uint256_t& winnerNonce = m_allPoWs.front().second;

    uint64_t blockNum = 0;
    uint8_t difficulty = POW2_DIFFICULTY;
    if (m_mediator.m_dsBlockChain.GetBlockCount() > 0)
    {
        DSBlock lastBlock = m_mediator.m_dsBlockChain.GetLastBlock();
        blockNum = lastBlock.GetHeader().GetBlockNum() + 1;
        difficulty = lastBlock.GetHeader().GetDifficulty();
    }

    // Assemble DS block
    {
        lock_guard<mutex> g(m_mutexPendingDSBlock);
        // To-do: Handle exceptions.
        m_pendingDSBlock.reset(
            new DSBlock(DSBlockHeader(difficulty, prevHash, winnerNonce,
                                      winnerKey, m_mediator.m_selfKey.second,
                                      blockNum, get_time_as_int()),
                        CoSignatures()));
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "New DSBlock created with chosen nonce = 0x" << hex
                                                           << winnerNonce);
}

bool DirectoryService::RunConsensusOnDSBlockWhenDSPrimary()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the leader DS node. Creating DS block.");

    ComposeDSBlock();

    // Create new consensus object
    // Dummy values for now
    uint32_t consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    // kill first ds leader (used for view change testing)
    /**
    if (m_consensusMyID == 0 && m_viewChangeCounter < 1)
    {
        LOG_GENERAL(INFO, "I am killing myself to test view change");
        throw exception();
    }
    **/

    m_consensusObject.reset(new ConsensusLeader(
        consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, m_mediator.m_DSCommittee,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(DSBLOCKCONSENSUS),
        std::function<bool(const vector<unsigned char>&, unsigned int,
                           const Peer&)>(),
        std::function<bool(map<unsigned int, vector<unsigned char>>)>()));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "WARNING: Unable to create consensus object");
        return false;
    }

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

    vector<unsigned char> m;
    {
        lock_guard<mutex> g(m_mutexPendingDSBlock);
        m_pendingDSBlock->Serialize(m, 0);

        // Add the IP info of the PoW winner so the backups don't need to query this anymore
        auto winnerPeer = m_allPoWConns.find(
            m_pendingDSBlock->GetHeader().GetMinerPubKey());
        if (winnerPeer == m_allPoWConns.end())
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "WARNING: Why don't I know the IP of the winner???");
            return false;
        }
        winnerPeer->second.Serialize(m, m.size());
    }

    LOG_STATE(
        "[DSCON]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] BGIN");

    cl->StartConsensus(m, DSBlockHeader::SIZE);

    return true;
}

bool DirectoryService::DSBlockValidator(
    const vector<unsigned char>& dsblock,
    [[gnu::unused]] std::vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    // Message = [DS block] [PoW winner IP]

    // To-do: Put in the logic here for checking the proposed DS block
    lock(m_mutexPendingDSBlock, m_mutexAllPoWConns);
    lock_guard<mutex> g(m_mutexPendingDSBlock, adopt_lock);
    lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

    // DS block
    m_pendingDSBlock.reset(new DSBlock(dsblock, 0));

    // PoW winner IP
    Peer winnerPeer(dsblock, m_pendingDSBlock->GetSerializedSize());

    auto storedMember
        = m_allPoWConns.find(m_pendingDSBlock->GetHeader().GetMinerPubKey());

    // I know the winner but the winner IP given by the leader is different!
    if (storedMember != m_allPoWConns.end())
    {
        if (storedMember->second != winnerPeer)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "WARNING: Why is the IP of the winner different from "
                      "what I have in m_allPoWConns???");
            return false;
        }
    }
    // I don't know the winner -> store the IP given by the leader
    else
    {
        m_allPoWConns.emplace(m_pendingDSBlock->GetHeader().GetMinerPubKey(),
                              winnerPeer);
    }

    return true;
}

bool DirectoryService::RunConsensusOnDSBlockWhenDSBackup()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am a backup DS node. Waiting for DS block announcement.");

    // Dummy values for now
    uint32_t consensusID = 0x0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    auto func = [this](const vector<unsigned char>& message,
                       vector<unsigned char>& errorMsg) mutable -> bool {
        return DSBlockValidator(message, errorMsg);
    };

    m_consensusObject.reset(new ConsensusBackup(
        consensusID, m_consensusBlockHash, m_consensusMyID, m_consensusLeaderID,
        m_mediator.m_selfKey.first, m_mediator.m_DSCommittee,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(DSBLOCKCONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    return true;
}

void DirectoryService::RunConsensusOnDSBlock(bool isRejoin)
{
    LOG_MARKER();
    SetState(DSBLOCK_CONSENSUS_PREP);

    {
        lock_guard<mutex> g(m_mutexAllPOW);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Num of PoW sub rec: " << m_allPoWs.size());
        LOG_STATE("[POWR][" << std::setw(15) << std::left
                            << m_mediator.m_selfPeer.GetPrintableIPAddress()
                            << "][" << m_allPoWs.size() << "] ");

        if (m_allPoWs.size() == 0)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "To-do: Code up the logic for if we didn't get any "
                      "submissions at all");
            // throw exception();
            if (!isRejoin)
            {
                return;
            }
        }
    }

    if (m_mode == PRIMARY_DS)
    {
        if (!RunConsensusOnDSBlockWhenDSPrimary())
        {
            LOG_GENERAL(WARNING,
                        "Error after RunConsensusOnDSBlockWhenDSPrimary");
            // throw exception();
            return;
        }
    }
    else
    {
        if (!RunConsensusOnDSBlockWhenDSBackup())
        {
            LOG_GENERAL(WARNING,
                        "Error after RunConsensusOnDSBlockWhenDSBackup");
            // throw exception();
            return;
        }
    }

    SetState(DSBLOCK_CONSENSUS);
    cv_DSBlockConsensusObject.notify_all();

    // View change will wait for timeout. If conditional variable is notified before timeout, the thread will return
    // without triggering view change.
    std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeDSBlock);
    if (cv_viewChangeDSBlock.wait_for(cv_lk,
                                      std::chrono::seconds(VIEWCHANGE_TIME))
        == std::cv_status::timeout)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Initiated DS block view change. ");
        auto func = [this]() -> void { RunConsensusOnViewChange(); };
        DetachedFunction(1, func);
    }
}

#endif // IS_LOOKUP_NODE
