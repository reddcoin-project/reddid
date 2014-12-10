// Copyright (c) 2010-2011 Vincent Durham
// Copyright (c) 2014 The Reddcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "redddb.h"
#include "txdb.h"

using namespace std;


int CNameDB::GetNameHeight(const vchType& vchName)
{
    vector<CNameIndex> vtxPos;
    if (ExistsName(vchName))
    {
        if (!ReadName(vchName, vtxPos))
            return error("GetNameHeight() : failed to read from name DB");
        if (vtxPos.empty ())
            return -1;

        CNameIndex& txPos = vtxPos.back();
        return GetTxPosHeight(txPos);
    }

    return -1;
}

bool CNameDB::GetValueOfName(const vchType& vchName, vchType& vchValue, int nHeight)
{
    vector<CNameIndex> vtxPos;
    if (!ReadName(vchName, vtxPos) || vtxPos.empty())
        return false;

    CNameIndex& txPos = vtxPos.back();
    nHeight = txPos.nHeight;
    vchValue = txPos.vValue;
    return true;
}

bool CNameDB::GetTxOfName(const vchType& vchName, CTransaction& tx)
{
    vector<CNameIndex> vtxPos;
    if (!ReadName(vchName, vtxPos) || vtxPos.empty())
        return false;

    CNameIndex& txPos = vtxPos.back();
    int nHeight = txPos.nHeight;
    if (nHeight + GetExpirationDepth(nHeight) < chainActive.Tip()->nHeight)
    {
        string name = stringFromVch(vchName);
        printf("GetTxOfName(%s) : expired", name.c_str());
        return false;
    }

    if (!ReadTxFromDisk(tx, txPos.txPos))
        return error("GetTxOfName() : could not read tx from disk");
    return true;
}

bool CNameDB::IsConflictedTx(const CTransaction& tx, vchType& name)
{
    if (tx.nVersion != REDDID_TX_VERSION)
        return false;

    vector<vchType> vvchArgs;
    int op;
    int nOut;
    if (!DecodeNameTx(tx, op, nOut, vvchArgs))
        return error("IsConflictedTx() : could not decode a name tx");

    int nPrevHeight;
    switch (op)
    {
        case OP_NAME_FIRSTUPDATE:
            nPrevHeight = GetNameHeight(vvchArgs[0]);
            name = vvchArgs[0];
            if (nPrevHeight >= 0 && chainActive.Tip()->nHeight - nPrevHeight < GetExpirationDepth(nPrevHeight))
                return true;
    }
    return false;
}

bool CNameDB::ScanNames(const vchType& vchName, size_t nMax, vector<pair<vchType, CNameIndex> >& nameScan)
{
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    while (true)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(string("reddid"), vchName);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "reddid")
        {
            vchType vchName;
            ssKey >> vchName;
            string strName = stringFromVch(vchName);
            vector<CNameIndex> vtxPos;
            ssValue >> vtxPos;
            CNameIndex txPos;
            if (!vtxPos.empty())
            {
                txPos = vtxPos.back();
            }
            nameScan.push_back(make_pair(vchName, txPos));
        }

        if (nameScan.size() >= nMax)
            break;
    }
    pcursor->close();
    return true;
}

bool CNameDB::ReconstructNameIndex()
{
    AssertLockHeld(cs_main);

    CDiskTxPos postx;
    CBlockIndex* pindex = chainActive.Genesis();
    while (pindex)
    {
        TxnBegin();
        CBlock block;
        ReadBlockFromDisk(block, pindex);
        int nHeight = pindex->nHeight;

        BOOST_FOREACH(CTransaction& tx, block.vtx)
        {
            if (tx.nVersion != REDDID_TX_VERSION)
                continue;

            vector<vchType > vvchArgs;
            int op;
            int nOut;

            if (!DecodeNameTx(tx, op, nOut, vvchArgs))
                continue;

            if (op == OP_NAME_NEW)
                continue;

            const vchType& vchName = vvchArgs[0];
            const vchType& vchValue = vvchArgs[op == OP_NAME_FIRSTUPDATE ? 2 : 1];

            if(!pblocktree->ReadTxIndex(tx.GetHash(), postx))
                continue;

            vector<CNameIndex> vtxPos;
            if (ExistsName(vchName))
            {
                if (!ReadName(vchName, vtxPos))
                    return error("ReconstructNameIndex() : failed to read from name DB");
            }

            CNameIndex txPos;
            txPos.nHeight = nHeight;
            txPos.vValue = vchValue;
            txPos.txPos = postx;
            vtxPos.push_back(txPos);
            if (!WriteName(vchName, vtxPos))
                return error("ReconstructNameIndex() : failed to write to name DB");
        }
        pindex = chainActive.Next(pindex);
        TxnCommit();
    }
    return true;
}

bool CNameDB::Verify()
{
    if (!fTxIndex)
        return false;

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    while (true)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue);
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "reddid")
        {
            vchType vchName;
            ssKey >> vchName;
            string strName = stringFromVch(vchName);
            vector<CNameIndex> vtxPos;
            ssValue >> vtxPos;
            bool fPrintReddid = GetBoolArg("printreddid", false);
            if (fPrintReddid)
              printf("REDDID %s : ", strName.c_str());
            BOOST_FOREACH(CNameIndex& txPos, vtxPos)
            {
                if (fPrintReddid)
                  printf(" @ %d, ", GetTxPosHeight(txPos));
            }
            if (fPrintReddid)
              printf("\n");
        }
    }
    pcursor->close();
    return true;
}
