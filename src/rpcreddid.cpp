// Copyright (c) 2010-2011 Vincent Durham
// Copyright (c) 2014 The Reddcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


Value name_list(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "name_list [<name>]\n"
                "list my own names"
                );

    vchType vchNameUniq;
    if (params.size () == 1)
      vchNameUniq = vchFromValue (params[0]);

    std::map<vchType, int> vNamesI;
    std::map<vchType, Object> vNamesO;

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
      {
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item,
                      pwalletMain->mapWallet)
          {
            const CWalletTx& tx = item.second;

            vchType vchName, vchValue;
            int nOut;
            if (!tx.GetNameUpdate (nOut, vchName, vchValue))
              continue;

            if(!vchNameUniq.empty () && vchNameUniq != vchName)
              continue;

            const int nHeight = tx.GetHeightInMainChain ();
            if (nHeight == -1)
              continue;
            assert (nHeight >= 0);

            // get last active name only
            if (vNamesI.find (vchName) != vNamesI.end ()
                && vNamesI[vchName] > nHeight)
              continue;

            Object oName;
            oName.push_back(Pair("name", stringFromVch(vchName)));
            oName.push_back(Pair("value", stringFromVch(vchValue)));
            if (!hooks->IsMine (tx))
                oName.push_back(Pair("transferred", 1));
            string strAddress = "";
            GetNameAddress(tx, strAddress);
            oName.push_back(Pair("address", strAddress));

            const int expiresIn = nHeight + GetDisplayExpirationDepth (nHeight) - pindexBest->nHeight;
            oName.push_back (Pair("expires_in", expiresIn));
            if (expiresIn <= 0)
              oName.push_back (Pair("expired", 1));

            vNamesI[vchName] = nHeight;
            vNamesO[vchName] = oName;
        }
    }

    Array oRes;
    BOOST_FOREACH(const PAIRTYPE(vector<unsigned char>, Object)& item, vNamesO)
        oRes.push_back(item.second);

    return oRes;
}

Value name_debug(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 0)
        throw runtime_error(
            "name_debug\n"
            "Dump pending transactions id in the debug file.\n");

    printf("Pending:\n----------------------------\n");
    pair<vector<unsigned char>, set<uint256> > pairPending;

    CRITICAL_BLOCK(cs_main)
        BOOST_FOREACH(pairPending, mapNamePending)
        {
            string name = stringFromVch(pairPending.first);
            printf("%s :\n", name.c_str());
            uint256 hash;
            BOOST_FOREACH(hash, pairPending.second)
            {
                printf("    ");
                if (!pwalletMain->mapWallet.count(hash))
                    printf("foreign ");
                printf("    %s\n", hash.GetHex().c_str());
            }
        }
    printf("----------------------------\n");
    return true;
}

Value name_debug1(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
            "name_debug1 <name>\n"
            "Dump name blocks number and transactions id in the debug file.\n");

    vector<unsigned char> vchName = vchFromValue(params[0]);
    printf("Dump name:\n");
    CRITICAL_BLOCK(cs_main)
    {
        //vector<CDiskTxPos> vtxPos;
        vector<CNameIndex> vtxPos;
        CNameDB dbName("r");
        if (!dbName.ReadName(vchName, vtxPos))
        {
            error("failed to read from name DB");
            return false;
        }
        //CDiskTxPos txPos;
        CNameIndex txPos;
        BOOST_FOREACH(txPos, vtxPos)
        {
            CTransaction tx;
            if (!tx.ReadFromDisk(txPos.txPos))
            {
                error("could not read txpos %s", txPos.txPos.ToString().c_str());
                continue;
            }
            printf("@%d %s\n", GetTxPosHeight(txPos), tx.GetHash().GetHex().c_str());
        }
    }
    printf("-------------------------\n");
    return true;
}

Value name_show(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "name_show <name>\n"
            "Show values of a name.\n"
            );

    Object oLastName;
    vector<unsigned char> vchName = vchFromValue(params[0]);
    string name = stringFromVch(vchName);
    CRITICAL_BLOCK(cs_main)
    {
        //vector<CDiskTxPos> vtxPos;
        vector<CNameIndex> vtxPos;
        CNameDB dbName("r");
        if (!dbName.ReadName(vchName, vtxPos))
            throw JSONRPCError(RPC_WALLET_ERROR, "failed to read from name DB");

        if (vtxPos.size() < 1)
            throw JSONRPCError(RPC_WALLET_ERROR, "no result returned");

        CDiskTxPos txPos = vtxPos[vtxPos.size() - 1].txPos;
        CTransaction tx;
        if (!tx.ReadFromDisk(txPos))
            throw JSONRPCError(RPC_WALLET_ERROR, "failed to read from from disk");

        Object oName;
        vector<unsigned char> vchValue;
        int nHeight;
        uint256 hash;
        if (!txPos.IsNull() && GetValueOfTxPos(txPos, vchValue, hash, nHeight))
        {
            oName.push_back(Pair("name", name));
            string value = stringFromVch(vchValue);
            oName.push_back(Pair("value", value));
            oName.push_back(Pair("txid", tx.GetHash().GetHex()));
            string strAddress = "";
            GetNameAddress(txPos, strAddress);
            oName.push_back(Pair("address", strAddress));
            oName.push_back(Pair("expires_in", nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight));
            if(nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
            {
                oName.push_back(Pair("expired", 1));
            }
            oLastName = oName;
        }
    }
    return oLastName;
}

Value name_history(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "name_history <name>\n"
            "List all name values of a name.\n");

    Array oRes;
    vector<unsigned char> vchName = vchFromValue(params[0]);
    string name = stringFromVch(vchName);
    CRITICAL_BLOCK(cs_main)
    {
        vector<CNameIndex> vtxPos;
        CNameDB dbName("r");
        if (!dbName.ReadName(vchName, vtxPos))
            throw JSONRPCError(RPC_WALLET_ERROR, "failed to read from name DB");

        CNameIndex txPos2;
        CDiskTxPos txPos;
        BOOST_FOREACH(txPos2, vtxPos)
        {
            txPos = txPos2.txPos;
            CTransaction tx;
            if (!tx.ReadFromDisk(txPos))
            {
                error("could not read txpos %s", txPos.ToString().c_str());
                continue;
            }

            Object oName;
            vector<unsigned char> vchValue;
            int nHeight;
            uint256 hash;
            if (!txPos.IsNull() && GetValueOfTxPos(txPos, vchValue, hash, nHeight))
            {
                oName.push_back(Pair("name", name));
                string value = stringFromVch(vchValue);
                oName.push_back(Pair("value", value));
                oName.push_back(Pair("txid", tx.GetHash().GetHex()));
                string strAddress = "";
                GetNameAddress(txPos, strAddress);
                oName.push_back(Pair("address", strAddress));
                oName.push_back(Pair("expires_in", nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight));
                if(nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
                {
                    oName.push_back(Pair("expired", 1));
                }
                oRes.push_back(oName);
            }
        }
    }
    return oRes;
}

Value name_filter(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 5)
        throw runtime_error(
                "name_filter [[[[[regexp] maxage=36000] from=0] nb=0] stat]\n"
                "scan and filter names\n"
                "[regexp] : apply [regexp] on names, empty means all names\n"
                "[maxage] : look in last [maxage] blocks\n"
                "[from] : show results from number [from]\n"
                "[nb] : show [nb] results, 0 means all\n"
                "[stats] : show some stats instead of results\n"
                "name_filter \"\" 5 # list names updated in last 5 blocks\n"
                "name_filter \"^id/\" # list all names from the \"id\" namespace\n"
                "name_filter \"^id/\" 36000 0 0 stat # display stats (number of names) on active names from the \"id\" namespace\n"
                );

    string strRegexp;
    int nFrom = 0;
    int nNb = 0;
    int nMaxAge = 36000;
    bool fStat = false;
    int nCountFrom = 0;
    int nCountNb = 0;


    if (params.size() > 0)
        strRegexp = params[0].get_str();

    if (params.size() > 1)
        nMaxAge = params[1].get_int();

    if (params.size() > 2)
        nFrom = params[2].get_int();

    if (params.size() > 3)
        nNb = params[3].get_int();

    if (params.size() > 4)
        fStat = (params[4].get_str() == "stat" ? true : false);


    CNameDB dbName("r");
    Array oRes;

    vector<unsigned char> vchName;
    vector<pair<vector<unsigned char>, CNameIndex> > nameScan;
    if (!dbName.ScanNames(vchName, 100000000, nameScan))
        throw JSONRPCError(RPC_WALLET_ERROR, "scan failed");

    // compile regex once
    using namespace boost::xpressive;
    smatch nameparts;
    sregex cregex = sregex::compile(strRegexp);

    pair<vector<unsigned char>, CNameIndex> pairScan;
    BOOST_FOREACH(pairScan, nameScan)
    {
        string name = stringFromVch(pairScan.first);

        // regexp
        if(strRegexp != "" && !regex_search(name, nameparts, cregex))
            continue;

        CNameIndex txName = pairScan.second;
        int nHeight = txName.nHeight;

        // max age
        if(nMaxAge != 0 && pindexBest->nHeight - nHeight >= nMaxAge)
            continue;

        // from limits
        nCountFrom++;
        if(nCountFrom < nFrom + 1)
            continue;

        Object oName;
        if (!fStat) {
            oName.push_back(Pair("name", name));
	        int nExpiresIn = nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight;
            if (nExpiresIn <= 0)
            {
                oName.push_back(Pair("expired", 1));
            }
            else
            {
                string value = stringFromVch(txName.vValue);
                oName.push_back(Pair("value", value));
                oName.push_back(Pair("expires_in", nExpiresIn));
            }
        }
        oRes.push_back(oName);

        nCountNb++;
        // nb limits
        if(nNb > 0 && nCountNb >= nNb)
            break;
    }

    if (NAME_DEBUG) {
        dbName.test();
    }

    if (fStat)
    {
        Object oStat;
        oStat.push_back(Pair("blocks",    (int)nBestHeight));
        oStat.push_back(Pair("count",     (int)oRes.size()));
        //oStat.push_back(Pair("sha256sum", SHA256(oRes), true));
        return oStat;
    }

    return oRes;
}

Value name_scan(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "name_scan [<start-name>] [<max-returned>]\n"
                "scan all names, starting at start-name and returning a maximum number of entries (default 500)\n"
                );

    vector<unsigned char> vchName;
    int nMax = 500;
    if (params.size() > 0)
    {
        vchName = vchFromValue(params[0]);
    }

    if (params.size() > 1)
    {
        Value vMax = params[1];
        ConvertTo<double>(vMax);
        nMax = (int)vMax.get_real();
    }

    CNameDB dbName("r");
    Array oRes;

    //vector<pair<vector<unsigned char>, CDiskTxPos> > nameScan;
    vector<pair<vector<unsigned char>, CNameIndex> > nameScan;
    if (!dbName.ScanNames(vchName, nMax, nameScan))
        throw JSONRPCError(RPC_WALLET_ERROR, "scan failed");

    //pair<vector<unsigned char>, CDiskTxPos> pairScan;
    pair<vector<unsigned char>, CNameIndex> pairScan;
    BOOST_FOREACH(pairScan, nameScan)
    {
        Object oName;
        string name = stringFromVch(pairScan.first);
        oName.push_back(Pair("name", name));
        //vector<unsigned char> vchValue;
        CTransaction tx;
        CNameIndex txName = pairScan.second;
        CDiskTxPos txPos = txName.txPos;
        //CDiskTxPos txPos = pairScan.second;
        //int nHeight = GetTxPosHeight(txPos);
        int nHeight = txName.nHeight;
        vector<unsigned char> vchValue = txName.vValue;
        if ((nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
            || txPos.IsNull()
            || !tx.ReadFromDisk(txPos))
            //|| !GetValueOfNameTx(tx, vchValue))
        {
            oName.push_back(Pair("expired", 1));
        }
        else
        {
            string value = stringFromVch(vchValue);
            //string strAddress = "";
            //GetNameAddress(tx, strAddress);
            oName.push_back(Pair("value", value));
            //oName.push_back(Pair("txid", tx.GetHash().GetHex()));
            //oName.push_back(Pair("address", strAddress));
            oName.push_back(Pair("expires_in", nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight));
        }
        oRes.push_back(oName);
    }

    if (NAME_DEBUG) {
        dbName.test();
    }
    return oRes;
}

Value name_firstupdate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
                "name_firstupdate <name> <rand> [<tx>] <value> [<toaddress>]\n"
                "Perform a first update after a name_new reservation.\n"
                "Note that the first update will go into a block 12 blocks after the name_new, at the soonest."
                + HelpRequiringPassphrase());

    const vchType vchName = vchFromValue (params[0]);
    const vchType vchRand = ParseHex (params[1].get_str ());

    vchType vchValue;
    if (params.size () == 3)
      vchValue = vchFromValue (params[2]);
    else
      vchValue = vchFromValue (params[3]);

    if (vchValue.size () > UI_MAX_VALUE_LENGTH)
      throw JSONRPCError(RPC_INVALID_PARAMETER, "the value is too long");

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;

    CRITICAL_BLOCK(cs_main)
    {
        if (mapNamePending.count(vchName) && mapNamePending[vchName].size())
        {
            error("name_firstupdate() : there are %d pending operations on that name, including %s",
                    mapNamePending[vchName].size(),
                    mapNamePending[vchName].begin()->GetHex().c_str());
            throw runtime_error("there are pending operations on that name");
        }
    }

    {
        CNameDB dbName("r");
        CTransaction tx;
        if (GetTxOfName(dbName, vchName, tx))
        {
            error("name_firstupdate() : this name is already active with tx %s",
                    tx.GetHash().GetHex().c_str());
            throw runtime_error("this name is already active");
        }
    }

    CScript scriptPubKeyOrig;
    if (params.size () == 5)
    {
        const std::string strAddress = params[4].get_str ();
        uint160 hash160;
        bool isValid = AddressToHash160 (strAddress, hash160);
        if (!isValid)
            throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY,
                                "Invalid namecoin address");
        scriptPubKeyOrig.SetBitcoinAddress (strAddress);
    }
    else
    {
        vector<unsigned char> vchPubKey = pwalletMain->GetKeyFromKeyPool ();
        scriptPubKeyOrig.SetBitcoinAddress (vchPubKey);
    }

    CRITICAL_BLOCK(cs_main)
    {
        EnsureWalletIsUnlocked();

        // Make sure there is a previous NAME_NEW tx on this name
        // and that the random value matches
        uint256 wtxInHash;
        if (params.size() == 3)
        {
            if (!mapMyNames.count(vchName))
            {
                throw runtime_error("could not find a coin with this name, try specifying the name_new transaction id");
            }
            wtxInHash = mapMyNames[vchName];
        }
        else
        {
            wtxInHash.SetHex(params[2].get_str());
        }

        if (!pwalletMain->mapWallet.count(wtxInHash))
        {
            throw runtime_error("previous transaction is not in the wallet");
        }

        CScript scriptPubKey;
        scriptPubKey << OP_NAME_FIRSTUPDATE << vchName << vchRand << vchValue << OP_2DROP << OP_2DROP;
        scriptPubKey += scriptPubKeyOrig;

        CWalletTx& wtxIn = pwalletMain->mapWallet[wtxInHash];
        vector<unsigned char> vchHash;
        bool found = false;
        BOOST_FOREACH(CTxOut& out, wtxIn.vout)
        {
            vector<vector<unsigned char> > vvch;
            int op;
            if (DecodeNameScript(out.scriptPubKey, op, vvch)) {
                if (op != OP_NAME_NEW)
                    throw runtime_error("previous transaction wasn't a name_new");
                vchHash = vvch[0];
                found = true;
            }
        }

        if (!found)
        {
            throw runtime_error("previous tx on this name is not a name tx");
        }

        vector<unsigned char> vchToHash(vchRand);
        vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
        uint160 hash =  Hash160(vchToHash);
        if (uint160(vchHash) != hash)
        {
            throw runtime_error("previous tx used a different random value");
        }

        int64 nNetFee = GetNetworkFee(pindexBest->nHeight);
        // Round up to CENT
        nNetFee += CENT - 1;
        nNetFee = (nNetFee / CENT) * CENT;
        string strError = SendMoneyWithInputTx(scriptPubKey, MIN_AMOUNT, nNetFee, wtxIn, wtx, false);
        if (strError != "")
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    return wtx.GetHash().GetHex();
}

Value name_update(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
                "name_update <name> <value> [<toaddress>]\nUpdate and possibly transfer a name"
                + HelpRequiringPassphrase());

    const vchType vchName = vchFromValue (params[0]);
    const vchType vchValue = vchFromValue (params[1]);

    if (vchValue.size () > UI_MAX_VALUE_LENGTH)
      throw JSONRPCError(RPC_INVALID_PARAMETER, "the value is too long");

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;
    CScript scriptPubKeyOrig;

    if (params.size() == 3)
    {
        string strAddress = params[2].get_str();
        uint160 hash160;
        bool isValid = AddressToHash160(strAddress, hash160);
        if (!isValid)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid namecoin address");
        scriptPubKeyOrig.SetBitcoinAddress(strAddress);
    }
    else
    {
        vector<unsigned char> vchPubKey = pwalletMain->GetKeyFromKeyPool();
        scriptPubKeyOrig.SetBitcoinAddress(vchPubKey);
    }

    CScript scriptPubKey;
    scriptPubKey << OP_NAME_UPDATE << vchName << vchValue << OP_2DROP << OP_DROP;
    scriptPubKey += scriptPubKeyOrig;

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        if (mapNamePending.count(vchName) && mapNamePending[vchName].size())
        {
            error("name_update() : there are %d pending operations on that name, including %s",
                    mapNamePending[vchName].size(),
                    mapNamePending[vchName].begin()->GetHex().c_str());
            throw runtime_error("there are pending operations on that name");
        }

        EnsureWalletIsUnlocked();

        CNameDB dbName("r");
        CTransaction tx;
        if (!GetTxOfName(dbName, vchName, tx))
        {
            throw runtime_error("could not find a coin with this name");
        }

        uint256 wtxInHash = tx.GetHash();

        if (!pwalletMain->mapWallet.count(wtxInHash))
        {
            error("name_update() : this coin is not in your wallet %s",
                    wtxInHash.GetHex().c_str());
            throw runtime_error("this coin is not in your wallet");
        }

        CWalletTx& wtxIn = pwalletMain->mapWallet[wtxInHash];
        string strError = SendMoneyWithInputTx(scriptPubKey, MIN_AMOUNT, 0, wtxIn, wtx, false);
        if (strError != "")
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    return wtx.GetHash().GetHex();
}

Value name_new(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "name_new <name>"
                + HelpRequiringPassphrase());

    vector<unsigned char> vchName = vchFromValue(params[0]);

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;

    uint64 rand = GetRand((uint64)-1);
    vector<unsigned char> vchRand = CBigNum(rand).getvch();
    vector<unsigned char> vchToHash(vchRand);
    vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
    uint160 hash =  Hash160(vchToHash);

    vector<unsigned char> vchPubKey = pwalletMain->GetKeyFromKeyPool();
    CScript scriptPubKeyOrig;
    scriptPubKeyOrig.SetBitcoinAddress(vchPubKey);
    CScript scriptPubKey;
    scriptPubKey << OP_NAME_NEW << hash << OP_2DROP;
    scriptPubKey += scriptPubKeyOrig;

    CRITICAL_BLOCK(cs_main)
    {
        EnsureWalletIsUnlocked();

        string strError = pwalletMain->SendMoney(scriptPubKey, MIN_AMOUNT, wtx, false);
        if (strError != "")
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
        mapMyNames[vchName] = wtx.GetHash();
    }

    printf("name_new : name=%s, rand=%s, tx=%s\n", stringFromVch(vchName).c_str(), HexStr(vchRand).c_str(), wtx.GetHash().GetHex().c_str());

    vector<Value> res;
    res.push_back(wtx.GetHash().GetHex());
    res.push_back(HexStr(vchRand));
    return res;
}

static Value
name_pending (const Array& params, bool fHelp)
{
  if (fHelp || params.size () != 0)
    throw runtime_error(
      "name_pending\n"
      "List all pending name operations known of.\n");

  Array res;

  CRITICAL_BLOCK (cs_main)
    {
      std::map<vchType, std::set<uint256> >::const_iterator i;
      for (i = mapNamePending.begin (); i != mapNamePending.end (); ++i)
        {
          if (i->second.empty ())
            continue;

          const std::string name = stringFromVch (i->first);

          for (std::set<uint256>::const_iterator j = i->second.begin ();
               j != i->second.end (); ++j)
            {
              CTransaction tx;
              uint256 hashBlock;
              if (!GetTransaction (*j, tx, hashBlock))
                {
                  printf ("name_pending: failed to GetTransaction of hash %s\n",
                          j->GetHex ().c_str ());
                  continue;
                }

              int op, nOut;
              std::vector<vchType> vvch;
              if (!DecodeNameTx (tx, op, nOut, vvch, -1))
                {
                  printf ("name_pending: failed to find name output in tx %s\n",
                          j->GetHex ().c_str ());
                  continue;
                }

              /* Decode the name operation.  */
              std::string value;
              std::string opString;
              switch (op)
                {
                case OP_NAME_FIRSTUPDATE:
                  assert (vvch.size () == 3);
                  opString = "name_firstupdate";
                  value = stringFromVch (vvch[2]);
                  break;

                case OP_NAME_UPDATE:
                  assert (vvch.size () == 2);
                  opString = "name_update";
                  value = stringFromVch (vvch[1]);
                  break;

                default:
                  printf ("name_pending: unexpected op code %d for tx %s\n",
                          op, j->GetHex ().c_str ());
                  continue;
                }

              /* See if it is owned by the wallet user.  */
              const CTxOut& txout = tx.vout[nOut];
              const bool isMine = IsMyName (tx, txout);

              /* Construct the JSON output.  */
              Object obj;
              obj.push_back (Pair ("name", name));
              obj.push_back (Pair ("txid", j->GetHex ()));
              obj.push_back (Pair ("op", opString));
              obj.push_back (Pair ("value", value));
              obj.push_back (Pair ("ismine", isMine));
              res.push_back (obj);
            }
        }
    }

  return res;
}

Value name_clean(const Array& params, bool fHelp)
{
    if (fHelp || params.size())
        throw runtime_error("name_clean\nClean unsatisfiable transactions from the wallet - including name_update on an already taken name\n");

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        map<uint256, CWalletTx> mapRemove;

        printf("-----------------------------\n");

        {
            DatabaseSet dbset("r");
            BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, pwalletMain->mapWallet)
            {
                CWalletTx& wtx = item.second;
                vchType vchName;
                if (wtx.GetDepthInMainChain () < 1
                    && IsConflictedTx (dbset, wtx, vchName))
                {
                    uint256 hash = wtx.GetHash();
                    mapRemove[hash] = wtx;
                }
            }
        }

        bool fRepeat = true;
        while (fRepeat)
        {
            fRepeat = false;
            BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, pwalletMain->mapWallet)
            {
                CWalletTx& wtx = item.second;
                BOOST_FOREACH(const CTxIn& txin, wtx.vin)
                {
                    uint256 hash = wtx.GetHash();

                    // If this tx depends on a tx to be removed, remove it too
                    if (mapRemove.count(txin.prevout.hash) && !mapRemove.count(hash))
                    {
                        mapRemove[hash] = wtx;
                        fRepeat = true;
                    }
                }
            }
        }

        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapRemove)
        {
            CWalletTx& wtx = item.second;

            UnspendInputs(wtx);
            wtx.RemoveFromMemoryPool();
            pwalletMain->EraseFromWallet(wtx.GetHash());
            vector<unsigned char> vchName;
            if (GetNameOfTx(wtx, vchName) && mapNamePending.count(vchName))
            {
                string name = stringFromVch(vchName);
                printf("name_clean() : erase %s from pending of name %s",
                        wtx.GetHash().GetHex().c_str(), name.c_str());
                if (!mapNamePending[vchName].erase(wtx.GetHash()))
                    error("name_clean() : erase but it was not pending");
            }
            wtx.print();
        }

        printf("-----------------------------\n");
    }

    return true;
}

Value sendtoname(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "sendtoname <namecoinname> <amount> [comment] [comment-to]\n"
            "<amount> is a real and is rounded to the nearest 0.01"
            + HelpRequiringPassphrase());

    vector<unsigned char> vchName = vchFromValue(params[0]);
    CNameDB dbName("r");
    if (!dbName.ExistsName(vchName))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Name not found");

    string strAddress;
    CTransaction tx;
    GetTxOfName(dbName, vchName, tx);
    GetNameAddress(tx, strAddress);

    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No valid namecoin address");

    // Amount
    int64 nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["to"]      = params[3].get_str();

    CRITICAL_BLOCK(cs_main)
    {
        EnsureWalletIsUnlocked();

        string strError = pwalletMain->SendMoneyToBitcoinAddress(strAddress, nAmount, wtx);
        if (strError != "")
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().GetHex();
}
