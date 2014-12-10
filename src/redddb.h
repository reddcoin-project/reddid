// Copyright (c) 2010-2011 Vincent Durham
// Copyright (c) 2014 The Reddcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef REDDCOIN_REDDDB_H
#define REDDCOIN_REDDDB_H

#include "db.h"
#include "main.h"
#include "reddid.h"


class CNameDB : public CDB
{
public:
    CNameDB(const char* pszMode="r+") : CDB("reddid.dat", pszMode)
    {
    }

    bool WriteName(const vchType& vchName, const std::vector<CNameIndex>& vtxPos)
    {
        return Write(make_pair(std::string("reddid"), vchName), vtxPos);
    }

    bool ReadName(const vchType& vchName, std::vector<CNameIndex>& vtxPos)
    {
        return Read(make_pair(std::string("reddid"), vchName), vtxPos);
    }

    bool ExistsName(const vchType& vchName)
    {
        return Exists(make_pair(std::string("reddid"), vchName));
    }

    bool EraseName(const vchType& vchName)
    {
        return Erase(make_pair(std::string("reddid"), vchName));
    }

    int GetNameHeight(const vchType& vchName);

    bool GetValueOfName(const vchType& vchName, vchType& vchValue, int nHeight);

    bool GetTxOfName(const vchType& vchName, CTransaction& tx);

    bool IsConflictedTx(const CTransaction& tx, vchType& name);

    bool ScanNames(const vchType& vchName, size_t nMax, std::vector<std::pair<vchType, CNameIndex> >& vchNameScan);

    bool ReconstructNameIndex();

    bool Verify();
};

#endif // REDDCOIN_REDDDB_H
