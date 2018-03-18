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
#include "common/Serializable.h"
#include "libUtils/TxnRootComputation.h"
#include "libData/AccountData/Transaction.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <vector>

#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

Transaction createDummyTransaction() {
    Address toAddr;
    for (unsigned int i = 0; i < toAddr.asArray().size(); i++)
    {
        toAddr.asArray().at(i) = i + 4;
    }

    Address fromAddr;
    for (unsigned int i = 0; i < fromAddr.asArray().size(); i++)
    {
        fromAddr.asArray().at(i) = i + 8;
    }

    std::array<unsigned char, TRAN_SIG_SIZE> signature;
    for (unsigned int i = 0; i < signature.size(); i++)
    {
        signature.at(i) = i + 16;
    }

    PubKey pubKey = Schnorr::GetInstance().GenKeyPair().second;
    Address fromCheck;
    //To obtain address
    std::vector<unsigned char> vec;
    pubKey.Serialize(vec, 0);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);

    const std::vector<unsigned char> & output = sha2.Finalize();
    copy(output.end() - ACC_ADDR_SIZE, output.end(),fromCheck.asArray().begin());

    Transaction tx(1, 5, toAddr, pubKey, 55, signature);
    return tx;
}

std::list<Transaction> generateDummyTransactions(size_t n) {
    std::list<Transaction> txns{n};
    for(auto &txn: txns) {
        txn = createDummyTransaction();
    }

    return txns;
}

BOOST_AUTO_TEST_CASE(testUnorderdMapWithDifferentInsertionOrder)
{
    auto txnList1 = generateDummyTransactions(100);
    auto txnList2 = generateDummyTransactions(100);

    // Simulating primary's transactions
    std::unordered_map<TxnHash, Transaction> primaryReceivedTxns;
    std::unordered_map<TxnHash, Transaction> primarySubmittedTxns;

    for(auto &txn: txnList1) {
       primaryReceivedTxns[txn.GetTranID()] = txn; 
    }
    for(auto &txn: txnList2) {
       primarySubmittedTxns[txn.GetTranID()] = txn; 
    }

    // Simulating backup's transactions
    std::unordered_map<TxnHash, Transaction> backupReceivedTxns;
    std::unordered_map<TxnHash, Transaction> backupSubmittedTxns;

    for(auto &txn: txnList2) {
       backupReceivedTxns[txn.GetTranID()] = txn; 
    }
    for(auto &txn: txnList1) {
       backupSubmittedTxns[txn.GetTranID()] = txn; 
    }

    // compute Merkle Tree and compare the root hashes
    
    auto primaryRoot = ComputeTransactionsRoot(primaryReceivedTxns, primarySubmittedTxns);
    auto backupRoot = ComputeTransactionsRoot(backupReceivedTxns, backupSubmittedTxns);

    BOOST_ASSERT(primaryRoot !=  backupRoot);
}

BOOST_AUTO_TEST_CASE(testMapsWithDifferentInsertionOrder)
{
    auto txnList1 = generateDummyTransactions(100);
    auto txnList2 = generateDummyTransactions(100);

    // simulating primary's transactions
    std::map<TxnHash, Transaction> primaryTxns;
    std::map<TxnHash, Transaction> emptyTxns;

    for(auto &txn: txnList1) {
       primaryTxns[txn.GetTranID()] = txn; 
    }
    for(auto &txn: txnList2) {
       primaryTxns[txn.GetTranID()] = txn; 
    }

    // simulating backup's transactions
    std::map<TxnHash, Transaction> backupTxns;

    for(auto &txn: txnList2) {
       backupTxns[txn.GetTranID()] = txn; 
    }
    for(auto &txn: txnList1) {
       backupTxns[txn.GetTranID()] = txn; 
    }

    // compute Merkle Tree and compare the root hashes
    auto primaryRoot = ComputeTransactionsRoot(primaryTxns, emptyTxns);
    auto backupRoot = ComputeTransactionsRoot(backupTxns, emptyTxns);

    BOOST_CHECK_EQUAL(primaryRoot, backupRoot);
}

BOOST_AUTO_TEST_CASE(compareAllSequentialListVersions)
{
    auto txnList1 = generateDummyTransactions(100);
    auto txnList2 = generateDummyTransactions(100);

    std::vector<TxnHash> txnHashVec; // join the hashes of two lists;
    std::vector<Transaction> txnVec; // join two lists;
    std::vector<Transaction> txnVec2; // only list 2

    for(auto &txn: txnList1) {
        txnVec.push_back(txn);
        txnHashVec.push_back(txn.GetTranID());
    }

    for(auto &txn: txnList2) {
        txnVec.push_back(txn);
        txnHashVec.push_back(txn.GetTranID());

        txnVec2.push_back(txn);
    }

    std::array<Transaction, 200>  txnArr;
    copy(txnVec.begin(), txnVec.end(), txnArr.begin());

    auto twoTxnList = ComputeTransactionsRoot(txnList1, txnList2);
    auto oneHashVec = ComputeTransactionsRoot(txnHashVec);

    // two non-template versions
    BOOST_CHECK_EQUAL(twoTxnList, oneHashVec);

    auto templateTwoTxnList = ComputeTransactionsRoot(txnList1, txnList2);
    auto templateOneTxnArray = ComputeTransactionsRoot(txnArr);
    auto templateOneTxnVec = ComputeTransactionsRoot(txnArr);
    auto templateMixed = ComputeTransactionsRoot(txnList1, txnVec2);

    // three specialized versions against the previous
    BOOST_CHECK_EQUAL(templateTwoTxnList, oneHashVec);
    BOOST_CHECK_EQUAL(templateOneTxnArray, oneHashVec);
    BOOST_CHECK_EQUAL(templateOneTxnVec, oneHashVec);
    BOOST_CHECK_EQUAL(templateMixed, oneHashVec);
}

BOOST_AUTO_TEST_SUITE_END()
