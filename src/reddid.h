// Copyright (c) 2010-2011 Vincent Durham
// Copyright (c) 2014 The Reddcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef REDDCOIN_REDDID_H
#define REDDCOIN_REDDID_H

#include "json/json_spirit.h"

using namespace json_spirit;

typedef std::vector<unsigned char> vchType;


static const int REDDID_TX_VERSION = 0x7100;
static const int64_t MIN_AMOUNT = CENT;
static const int MAX_NAME_LENGTH = 255;
static const int MAX_VALUE_LENGTH = 1023;
static const int OP_NAME_INVALID = 0x00;
static const int OP_NAME_NEW = 0x01;
static const int OP_NAME_FIRSTUPDATE = 0x02;
static const int OP_NAME_UPDATE = 0x03;
static const int OP_NAME_NOP = 0x04;
static const int MIN_FIRSTUPDATE_DEPTH = 12;

/* 
 * Maximum value length that is allowed by the UIs. Currently,
 * if the value is set above 520 bytes, it can't ever be updated again
 * due to limitations in the scripting system.  Enforce this
 * in the UIs.
 */
// static const int UI_MAX_VALUE_LENGTH = 520;

extern std::map<vchType, uint256> mapMyNames;
extern std::map<vchType, std::set<uint256> > mapNamePending;


vchType vchFromString(const std::string& str);
vchType vchFromValue(const Value& value);
std::string stringFromVch(const vchType& vch);

std::string GetNameFromOp(int op);
int64_t GetNetworkFee(int nHeight);
int GetExpirationDepth(int nHeight);
int GetTxPosHeight(const CNameIndex& txPos);
int GetTxPosHeight(const CDiskTxPos& txPos);
bool GetValueOfTxPos(const CNameIndex& txPos, std::vector<unsigned char>& vchValue, uint256& hash, int& nHeight);
bool GetValueOfTxPos(const CDiskTxPos& txPos, std::vector<unsigned char>& vchValue, uint256& hash, int& nHeight);
bool GetValueOfNameTx(const CTransaction& tx, std::vector<unsigned char>& value);

bool GetNameAddress(const CTransaction& tx, std::string& strAddress);
bool GetNameOfTx(const CTransaction& tx, std::vector<unsigned char>& name);

bool DecodeNameScript(const CScript& script, int& op, std::vector<std::vector<unsigned char> > &vvch);
bool DecodeNameScript(const CScript& script, int& op, std::vector<std::vector<unsigned char> > &vvch, CScript::const_iterator& pc);
bool DecodeNameTx(const CTransaction& tx, int& op, int& nOut, std::vector<std::vector<unsigned char> >& vvch);


int IndexOfNameOutput(const CTransaction& tx);
// int GetDisplayExpirationDepth(int nHeight);
// std::string SendMoneyWithInputTx(const CScript& scriptPubKey, int64 nValue, int64 nNetFee, const CWalletTx& wtxIn, CWalletTx& wtxNew, bool fAskFee);
// bool CreateTransactionWithInputTx(const std::vector<std::pair<CScript, int64> >& vecSend, const CWalletTx& wtxIn, int nTxOut, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet);

/* Handle the name operation part of the RPC call createrawtransaction.  */
// void AddRawTxNameOperation(CTransaction& tx, const json_spirit::Object& obj);


// class CReddID
// {
//     bool ConnectInputs(DatabaseSet& dbset, std::map<uint256, CTxIndex>& mapTestPool, const CTransaction& tx, std::vector<CTransaction>& vTxPrev,
//     	std::vector<CTxIndex>& vTxindex, CBlockIndex* pindexBlock, CDiskTxPos& txPos, bool fBlock, bool fMiner);
//     bool DisconnectInputs (DatabaseSet& txdb, const CTransaction& tx, CBlockIndex* pindexBlock);

//     bool CheckTransaction(const CTransaction& tx);
//     bool ExtractAddress(const CScript& script, std::string& address);

//     void AcceptToMemoryPool(DatabaseSet& dbset, const CTransaction& tx);
//     void RemoveFromMemoryPool(const CTransaction& tx);

//     // These are for display and wallet management purposes.  Not for use to decide whether to spend a coin.
//     bool IsMine(const CTransaction& tx);
//     bool IsMine(const CTransaction& tx, const CTxOut& txout, bool ignore_name_new = false);
// }

#endif // REDDCOIN_REDDID_H
