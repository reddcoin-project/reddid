// Copyright (c) 2010-2011 Vincent Durham
// Copyright (c) 2014 Reddcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "redddb.h"

#include <boost/xpressive/xpressive_dynamic.hpp>

using namespace std;
using namespace json_spirit;


map<vchType, uint256> mapMyNames;
map<vchType, set<uint256> > mapNamePending;


vchType vchFromString(const string& str)
{
    const unsigned char* strbeg;
    strbeg = reinterpret_cast<const unsigned char*> (str.c_str());
    return vchType(strbeg, strbeg + str.size());
}

vchType vchFromValue(const Value& value)
{
    const string str = value.get_str();
    return vchFromString(str);
}

string stringFromVch(const vchType &vch) {
    string res;
    vchType::const_iterator vi = vch.begin();
    while (vi != vch.end()) {
        res += (char)(*vi);
        vi++;
    }
    return res;
}

string GetNameFromOp(int op)
{
    switch (op)
    {
        case OP_NAME_NEW:
            return "name_new";
        case OP_NAME_UPDATE:
            return "name_update";
        case OP_NAME_FIRSTUPDATE:
            return "name_firstupdate";
        default:
            return "<unknown name op>";
    }
}

int64_t GetNetworkFee(int nHeight)
{
    return 2000 * COIN;
}

int GetExpirationDepth(int nHeight)
{
    return 365 * 24 * 60; // 1 year
}

int GetTxPosHeight(const CNameIndex& txPos)
{
    return txPos.nHeight;
}

int GetTxPosHeight(const CDiskTxPos& txPos)
{
    CBlock block;
    if (!ReadBlockFromDisk(block, txPos))
        return -1;

    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return -1;

    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !chainActive.Contains(pindex))
        return -1;

    return pindex->nHeight;
}

CScript RemoveNameScriptPrefix(const CScript& scriptIn)
{
    int op;
    vector<vchType > vvch;
    CScript::const_iterator pc = scriptIn.begin();

    if (!DecodeNameScript(scriptIn, op, vvch,  pc))
        throw runtime_error("RemoveNameScriptPrefix() : could not decode name script");
    return CScript(pc, scriptIn.end());
}

bool GetValueOfTxPos(const CNameIndex& txPos, vchType& vchValue, uint256& hash, int nHeight)
{
    if (!fTxIndex)
        return false;

    nHeight = GetTxPosHeight(txPos);
    vchValue = txPos.vValue;
    CTransaction tx;
    if (!ReadTxFromDisk(tx, txPos.txPos))
        return error("GetValueOfTxPos() : could not read tx from disk");
    hash = tx.GetHash();
    return true;
}

bool GetValueOfTxPos(const CDiskTxPos& txPos, vchType& vchValue, uint256& hash, int& nHeight)
{
    nHeight = GetTxPosHeight(txPos);
    CTransaction tx;
    if (!ReadTxFromDisk(tx, txPos))
        return error("GetValueOfTxPos() : could not read tx from disk");
    if (!GetValueOfNameTx(tx, vchValue))
        return error("GetValueOfTxPos() : could not decode value from tx");
    hash = tx.GetHash();
    return true;
}

bool GetValueOfNameTx(const CTransaction& tx, vchType& value)
{
    vector<vchType > vvch;
    int op;
    int nOut;
    if (!DecodeNameTx(tx, op, nOut, vvch))
        return false;

    switch (op)
    {
        case OP_NAME_NEW:
            return false;
        case OP_NAME_FIRSTUPDATE:
            value = vvch[2];
            return true;
        case OP_NAME_UPDATE:
            value = vvch[1];
            return true;
        default:
            return false;
    }
}

bool GetNameAddress(const CTransaction& tx, string& strAddress)
{
    int op;
    int nOut;
    vector<vchType> vvch;
    if (!DecodeNameTx(tx, op, nOut, vvch))
        return false;
    const CTxOut& txout = tx.vout[nOut];
    const CScript& scriptPubKey = RemoveNameScriptPrefix(txout.scriptPubKey);
    strAddress = CBitcoinAddress(scriptPubKey.GetID()).ToString();
    return true;
}

bool GetNameAddress(const CDiskTxPos& txPos, string& strAddress)
{
    CTransaction tx;
    if (!ReadTxFromDisk(tx, txPos))
        return error("GetNameAddress() : could not read tx from disk");

    return GetNameAddress(tx, strAddress);
}

bool CheckNameTxPos(const vector<CNameIndex> &vtxPos, const CDiskTxPos& txPos)
{
    if (vtxPos.empty())
        return false;

    return vtxPos.back().txPos == txPos;
}

bool DecodeNameScript(const CScript& script, int& op, vector<vchType>& vvch)
{
    CScript::const_iterator pc = script.begin();
    return DecodeNameScript(script, op, vvch, pc);
}

bool DecodeNameScript(const CScript& script, int& op, vector<vchType>& vvch, CScript::const_iterator& pc)
{
    opcodetype opcode;
    if (!script.GetOp(pc, opcode))
        return false;
    if (opcode < OP_1 || opcode > OP_16)
        return false;

    op = opcode - OP_1 + 1;

    for (;;) {
        vchType vch;
        if (!script.GetOp(pc, opcode, vch))
            return false;
        if (opcode == OP_DROP || opcode == OP_2DROP || opcode == OP_NOP)
            break;
        if (!(opcode >= 0 && opcode <= OP_PUSHDATA4))
            return false;
        vvch.push_back(vch);
    }

    // move the pc to after any DROP or NOP
    while (opcode == OP_DROP || opcode == OP_2DROP || opcode == OP_NOP)
    {
        if (!script.GetOp(pc, opcode))
            break;
    }

    pc--;

    if ((op == OP_NAME_NEW && vvch.size() == 1) ||
            (op == OP_NAME_FIRSTUPDATE && vvch.size() == 3) ||
            (op == OP_NAME_UPDATE && vvch.size() == 2))
        return true;
    return error("invalid number of arguments for name op");
}

bool DecodeNameTx(const CTransaction& tx, int& op, int& nOut, vector<vchType >& vvch)
{
    bool found = false;

    for (size_t i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& out = tx.vout[i];
        vector<vchType > vvchRead;

        if (DecodeNameScript(out.scriptPubKey, op, vvchRead))
        {
            // If more than one name op, fail
            if (found)
            {
                vvch.clear();
                return false;
            }
            nOut = i;
            found = true;
            vvch = vvchRead;
        }
    }

    if (!found)
        vvch.clear();

    return found;
}

bool GetNameOfTx(const CTransaction& tx, vchType& name)
{
    if (tx.nVersion != REDDID_TX_VERSION)
        return false;

    vector<vchType > vvchArgs;
    int op;
    int nOut;
    if (!DecodeNameTx(tx, op, nOut, vvchArgs))
        return error("GetNameOfTx() : could not decode a namecoin tx");

    switch (op)
    {
        case OP_NAME_FIRSTUPDATE:
        case OP_NAME_UPDATE:
            name = vvchArgs[0];
            return true;
    }
    return false;
}

int IndexOfNameOutput(const CTransaction& tx)
{
    vector<vchType > vvch;
    int op;
    int nOut;
    if (!DecodeNameTx(tx, op, nOut, vvch))
        throw runtime_error("IndexOfNameOutput() : name output not found");

    return nOut;
}

int64_t GetNameNetFee(const CTransaction& tx)
{
    int64_t nFee = 0;

    for (size_t i = 0 ; i < tx.vout.size() ; i++)
    {
        const CTxOut& out = tx.vout[i];
        if (out.scriptPubKey.size() == 1 && out.scriptPubKey[0] == OP_RETURN)
        {
            nFee += out.nValue;
        }
    }

    return nFee;
}

int CheckTransactionAtRelativeDepth(CBlockIndex* pindexBlock, CDiskTxPos& txindex, int maxDepth)
{
    for (CBlockIndex* pindex = pindexBlock; pindex && pindexBlock->nHeight - pindex->nHeight < maxDepth; pindex = pindex->pprev)
        if (pindex->nDataPos == txindex.nPos && pindex->nFile == txindex.nFile)
            return pindexBlock->nHeight - pindex->nHeight;
    return -1;
}

// bool CReddID::IsMine(const CTransaction& tx)
// {
//     if (tx.nVersion != REDDID_TX_VERSION)
//         return false;

//     vector<vchType > vvch;

//     int op;
//     int nOut;

//     // We do the check under the correct rule set (post-hardfork)
//     bool good = DecodeNameTx(tx, op, nOut, vvch);

//     if (!good)
//     {
//         error("IsMine() hook : no output out script in name tx %s\n", tx.ToString().c_str());
//         return false;
//     }

//     const CTxOut& txout = tx.vout[nOut];
//     if (IsMyName(tx, txout))
//     {
//         //printf("IsMine() hook : found my transaction %s nout %d\n", tx.GetHash().GetHex().c_str(), nOut);
//         return true;
//     }
//     return false;
// }

// bool CReddID::IsMine(const CTransaction& tx, const CTxOut& txout, bool ignore_name_new /* = false*/)
// {
//     if (tx.nVersion != REDDID_TX_VERSION)
//         return false;

//     vector<vchType > vvch;

//     int op;
//     int nOut;

//     if (!DecodeNameScript(txout.scriptPubKey, op, vvch))
//         return false;

//     if (ignore_name_new && op == OP_NAME_NEW)
//         return false;

//     if (IsMyName(tx, txout))
//     {
//         //printf("IsMine() hook : found my transaction %s value %ld\n", tx.GetHash().GetHex().c_str(), txout.nValue);
//         return true;
//     }
//     return false;
// }

// void CReddID::AcceptToMemoryPool(const CTransaction& tx)
// {
//     if (tx.nVersion != REDDID_TX_VERSION)
//         return;

//     if (tx.vout.size() < 1)
//     {
//         error("AcceptToMemoryPool() : no output in name tx %s\n", tx.ToString().c_str());
//         return;
//     }

//     vector<vchType > vvch;
//     int op;
//     int nOut;

//     if (!DecodeNameTx(tx, op, nOut, vvch))
//     {
//         error("AcceptToMemoryPool() : no output out script in name tx %s", tx.ToString().c_str());
//         return;
//     }

//     if (op != OP_NAME_NEW)
//     {
//         LOCK(cs_main);
//         mapNamePending[vvch[0]].insert(tx.GetHash());
//     }
// }

// void CReddID::RemoveFromMemoryPool(const CTransaction& tx)
// {
//     if (tx.nVersion != REDDID_TX_VERSION)
//         return;

//     if (tx.vout.size() < 1)
//         return;

//     vector<vchType > vvch;
//     int op;
//     int nOut;

//     if (!DecodeNameTx(tx, op, nOut, vvch))
//         return;

//     if (op != OP_NAME_NEW)
//     {
//         LOCK(cs_main);
//         map<vchType, set<uint256> >::iterator mi = mapNamePending.find(vvch[0]);
//         if (mi != mapNamePending.end())
//             mi->second.erase(tx.GetHash());
//     }
// }

// bool CReddID::ConnectInputs(DatabaseSet& dbset, map<uint256, CTxIndex>& mapTestPool, const CTransaction& tx, vector<CTransaction>& vTxPrev,
//     vector<CTxIndex>& vTxindex, CBlockIndex* pindexBlock, CDiskTxPos& txPos, bool fBlock, bool fMiner)
// {
//     int nInput;
//     bool found = false;

//     int prevOp;
//     vector<vchType > vvchPrevArgs;

//     // Strict check - bug disallowed
//     for (int i = 0; i < tx.vin.size(); i++)
//     {
//         CTxOut& out = vTxPrev[i].vout[tx.vin[i].prevout.n];

//         vector<vchType > vvchPrevArgsRead;

//         if (DecodeNameScript(out.scriptPubKey, prevOp, vvchPrevArgsRead))
//         {
//             if (found)
//                 return error("ConnectInputHook() : multiple previous name transactions");
//             found = true;
//             nInput = i;

//             vvchPrevArgs = vvchPrevArgsRead;
//         }
//     }

//     if (tx.nVersion != REDDID_TX_VERSION)
//     {
//         /* See if there are any name outputs.  If they are, disallow
//            for mempool or after the corresponding soft fork point.  Note
//            that we can't just use 'DecodeNameTx', since that would also
//            report "false" if we have *multiple* name outputs.  This should
//            also be detected, though.  */
//         bool foundOuts = false;
//         for (int i = 0; i < tx.vout.size(); i++)
//         {
//             const CTxOut& out = tx.vout[i];

//             vector<vchType> vvchRead;
//             int opRead;

//             if (DecodeNameScript(out.scriptPubKey, opRead, vvchRead))
//                 foundOuts = true;
//         }

//         if (foundOuts
//             && (!fBlock || pindexBlock->nHeight >= FORK_HEIGHT_TXVERSION))
//             return error("ConnectInputHook: non-Namecoin tx has name outputs");

//         // Make sure name-op outputs are not spent by a regular transaction, or the name
//         // would be lost
//         if (found)
//             return error("ConnectInputHook() : a non-namecoin transaction with a namecoin input");
//         return true;
//     }

//     vector<vchType > vvchArgs;
//     int op;
//     int nOut;

//     bool good = DecodeNameTx(tx, op, nOut, vvchArgs);
//     if (!good)
//         return error("ConnectInputsHook() : could not decode a namecoin tx");

//     int nPrevHeight;
//     int nDepth;
//     int64 nNetFee;

//     bool fBugWorkaround = false;


//     switch (op)
//     {
//         case OP_NAME_NEW:
//             if (found)
//                 return error("ConnectInputsHook() : name_new tx pointing to previous namecoin tx");
//             break;
//         case OP_NAME_FIRSTUPDATE:
//             nNetFee = GetNameNetFee(tx);
//             if (nNetFee < GetNetworkFee(pindexBlock->nHeight))
//                 return error("ConnectInputsHook() : got tx %s with fee too low %d", tx.GetHash().GetHex().c_str(), nNetFee);
//             if (!found || prevOp != OP_NAME_NEW)
//                 return error("ConnectInputsHook() : name_firstupdate tx without previous name_new tx");

//             {
//                 // Check hash
//                 const vchType &vchHash = vvchPrevArgs[0];
//                 const vchType &vchName = vvchArgs[0];
//                 const vchType &vchRand = vvchArgs[1];
//                 vchType vchToHash(vchRand);
//                 vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
//                 uint160 hash = Hash160(vchToHash);
//                 if (uint160(vchHash) != hash)
//                     return error("ConnectInputsHook() : name_firstupdate hash mismatch");
//             }

//             nPrevHeight = GetNameHeight(dbset, vvchArgs[0]);
//             if (nPrevHeight >= 0 && pindexBlock->nHeight - nPrevHeight < GetExpirationDepth(pindexBlock->nHeight))
//                 return error("ConnectInputsHook() : name_firstupdate on an unexpired name");
//             nDepth = CheckTransactionAtRelativeDepth(pindexBlock, vTxindex[nInput], MIN_FIRSTUPDATE_DEPTH);
//             // Do not accept if in chain and not mature
//             if ((fBlock || fMiner) && nDepth >= 0 && nDepth < MIN_FIRSTUPDATE_DEPTH)
//                 return false;

//             // Do not mine if previous name_new is not visible.  This is if
//             // name_new expired or not yet in a block
//             if (fMiner)
//             {
//                 // TODO CPU intensive
//                 nDepth = CheckTransactionAtRelativeDepth(pindexBlock, vTxindex[nInput], GetExpirationDepth(pindexBlock->nHeight));
//                 if (nDepth == -1)
//                     return error("ConnectInputsHook() : name_firstupdate cannot be mined if name_new is not already in chain and unexpired");
//                 // Check that no other pending txs on this name are already in the block to be mined
//                 set<uint256>& setPending = mapNamePending[vvchArgs[0]];
//                 BOOST_FOREACH(const PAIRTYPE(uint256, CTxIndex)& s, mapTestPool)
//                 {
//                     if (setPending.count(s.first))
//                     {
//                         printf("ConnectInputsHook() : will not mine %s because it clashes with %s",
//                                 tx.GetHash().GetHex().c_str(),
//                                 s.first.GetHex().c_str());
//                         return false;
//                     }
//                 }
//             }
//             break;
//         case OP_NAME_UPDATE:
//             if (!found || (prevOp != OP_NAME_FIRSTUPDATE && prevOp != OP_NAME_UPDATE))
//                 return error("name_update tx without previous update tx");

//             // Check name
//             if (vvchPrevArgs[0] != vvchArgs[0])
//                 return error("ConnectInputsHook() : name_update name mismatch");

//             // TODO CPU intensive
//             nDepth = CheckTransactionAtRelativeDepth(pindexBlock, vTxindex[nInput], GetExpirationDepth(pindexBlock->nHeight));
//             if ((fBlock || fMiner) && nDepth < 0)
//                 return error("ConnectInputsHook() : name_update on an expired name, or there is a pending transaction on the name");
//             break;
//         default:
//             return error("ConnectInputsHook() : name transaction has unknown op");
//     }

//     if (!fBlock && op == OP_NAME_UPDATE)
//     {
//         vector<CNameIndex> vtxPos;
//         if (dbset.name ().ExistsName (vvchArgs[0])
//             && !dbset.name ().ReadName (vvchArgs[0], vtxPos))
//           return error("ConnectInputsHook() : failed to read from name DB");
//         // Valid tx on top of buggy tx: if not in block, reject
//         if (!CheckNameTxPos(vtxPos, vTxindex[nInput].pos))
//             return error("ConnectInputsHook() : Name bug workaround: tx %s rejected, since previous tx (%s) is not in the name DB\n", tx.GetHash().ToString().c_str(), vTxPrev[nInput].GetHash().ToString().c_str());
//     }

//     if (fBlock)
//     {
//         if (op == OP_NAME_FIRSTUPDATE || op == OP_NAME_UPDATE)
//         {
//             vector<CNameIndex> vtxPos;
//             if (dbset.name ().ExistsName (vvchArgs[0])
//                 && !dbset.name ().ReadName (vvchArgs[0], vtxPos))
//               return error("ConnectInputsHook() : failed to read from name DB");

//             if (op == OP_NAME_UPDATE && !CheckNameTxPos(vtxPos, vTxindex[nInput].pos))
//             {
//                 printf("ConnectInputsHook() : Name bug workaround: tx %s rejected, since previous tx (%s) is not in the name DB\n", tx.GetHash().ToString().c_str(), vTxPrev[nInput].GetHash().ToString().c_str());
//                 // Valid tx on top of buggy tx: reject only after hard-fork
//                 return false;
//             }

//             vchType vchValue; // add
//             int nHeight;
//             uint256 hash;
//             GetValueOfTxPos(txPos, vchValue, hash, nHeight);
//             CNameIndex txPos2;
//             txPos2.nHeight = pindexBlock->nHeight;
//             txPos2.vValue = vchValue;
//             txPos2.txPos = txPos;
//             vtxPos.push_back(txPos2); // fin add
//             if (!dbset.name ().WriteName (vvchArgs[0], vtxPos))
//               return error("ConnectInputsHook() : failed to write to name DB");
//         }

//         if (op != OP_NAME_NEW)
//         {
//             CRITICAL_BLOCK(cs_main)
//             {
//                 map<vchType, set<uint256> >::iterator mi = mapNamePending.find(vvchArgs[0]);
//                 if (mi != mapNamePending.end())
//                     mi->second.erase(tx.GetHash());
//             }
//         }
//     }

//     return true;
// }

// bool CReddID::DisconnectInputs(DatabaseSet& dbset, const CTransaction& tx, CBlockIndex* pindexBlock)
// {
//     if (tx.nVersion != REDDID_TX_VERSION)
//         return true;

//     vector<vchType > vvchArgs;
//     int op;
//     int nOut;

//     bool good = DecodeNameTx(tx, op, nOut, vvchArgs);
//     if (!good)
//         return error("DisconnectInputsHook() : could not decode namecoin tx");
//     if (op == OP_NAME_FIRSTUPDATE || op == OP_NAME_UPDATE)
//     {
//         //vector<CDiskTxPos> vtxPos;
//         vector<CNameIndex> vtxPos;
//         if (!dbset.name ().ReadName (vvchArgs[0], vtxPos))
//             return error("DisconnectInputsHook() : failed to read from name DB");
//         // vtxPos might be empty if we pruned expired transactions.  However, it should normally still not
//         // be empty, since a reorg cannot go that far back.  Be safe anyway and do not try to pop if empty.
//         if (vtxPos.size())
//         {
//             CTxIndex txindex;
//             if (!dbset.tx ().ReadTxIndex (tx.GetHash (), txindex))
//                 return error("DisconnectInputsHook() : failed to read tx index");

//             if (vtxPos.back().txPos == txindex.pos)
//                 vtxPos.pop_back();

//             // TODO validate that the first pos is the current tx pos
//         }
//         if (!dbset.name ().WriteName (vvchArgs[0], vtxPos))
//             return error("DisconnectInputsHook() : failed to write to name DB");
//     }

//     return true;
// }

// bool CReddID::CheckTransaction(const CTransaction& tx)
// {
//     if (tx.nVersion != REDDID_TX_VERSION)
//         return true;

//     vector<vchType > vvch;
//     int op;
//     int nOut;

//     if (!DecodeNameTx(tx, op, nOut, vvch))
//         return error("name transaction has unknown script format");

//     if (vvch[0].size() > MAX_NAME_LENGTH)
//         return error("name transaction with name too long");

//     switch (op)
//     {
//         case OP_NAME_NEW:
//             if (vvch[0].size() != 20)
//                 return error("name_new tx with incorrect hash length");

//         case OP_NAME_FIRSTUPDATE:
//             if (vvch[1].size() > 20)
//                 return error("name_firstupdate tx with rand too big");
//             if (vvch[2].size() > MAX_VALUE_LENGTH)
//                 return error("name_firstupdate tx with value too long");

//         case OP_NAME_UPDATE:
//             if (vvch[1].size() > MAX_VALUE_LENGTH)
//                 return error("name_update tx with value too long");

//         default:
//             return error("name transaction has unknown op");
//     }

//     return true;
// }
