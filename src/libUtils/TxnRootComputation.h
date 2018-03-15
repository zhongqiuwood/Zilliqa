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

#ifndef __TXNROOTCOMPUTATION_H__
#define __TXNROOTCOMPUTATION_H__

#include <list>
#include <unordered_map>
#include <vector>

#include "libData/AccountData/Transaction.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"

TxnHash ComputeTransactionsRoot(const std::vector<TxnHash> & transactionHashes);

TxnHash ComputeTransactionsRoot
(
    const std::list<Transaction> & receivedTransactions,
    const std::list<Transaction> & submittedTransactions
);

TxnHash ComputeTransactionsRoot
(
    const std::unordered_map<TxnHash, Transaction> & receivedTransactions,
    const std::unordered_map<TxnHash, Transaction> & submittedTransactions
);

// not supposed to be used by other libraries
namespace _internal {
template<typename T>
const TxnHash & GetTranID(T item);

inline const TxnHash & GetTranID(const Transaction &item) {
    return item.GetTranID();
}

inline const TxnHash & GetTranID(const std::pair<const TxnHash, Transaction> item) {
    return item.second.GetTranID();
}
};

/// Construct a Merkle Tree over a list of containers and return the root.
/// The conatiners are passed in order and each one can be list or map.
template <typename ...Container>
TxnHash ComputeTransactionsRoot(const Container&... conts) {
    using namespace _internal;
    LOG_MARKER();

    dev::MemoryDB tm;
    dev::GenericTrieDB<dev::MemoryDB> txnTrie(&tm);
    txnTrie.init();

    size_t txnCount = 0;

    (void) std::initializer_list<int> {
        ([](const auto& list, decltype(txnCount) &txnCount, decltype(txnTrie) &txnTrie){
            dev::RLPStream streamer; // reuse this for better efficiency
            for(auto &item: list) {
                streamer.clear();
                streamer << txnCount++;
                txnTrie.insert(&streamer.out(), GetTranID(item).asBytes());
            }
        }(conts, txnCount, txnTrie), 0)...
    };

    return TxnHash{txnTrie.root()};
}

#endif // __TXNROOTCOMPUTATION_H__
