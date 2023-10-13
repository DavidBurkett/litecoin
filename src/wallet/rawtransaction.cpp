#include <wallet/rawtransaction.h>

#include <core_io.h>
#include <key_io.h>
#include <rpc/protocol.h>
#include <rpc/request.h>
#include <rpc/util.h>
#include <script/script.h>
#include <univalue.h>
#include <util/rbf.h>
#include <wallet/coincontrol.h>
#include <wallet/spend.h>
#include <wallet/wallet.h>

using namespace wallet;

RawTransaction RawTransaction::FromRPC(const UniValue& inputs_in, const UniValue& outputs_in, const UniValue& locktime, std::optional<bool> rbf)
{
    if (outputs_in.isNull()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, output argument must be non-null");
    }

    RawTransaction rawTx;

    if (!locktime.isNull()) {
        int64_t nLockTime = locktime.getInt<int64_t>();
        if (nLockTime < 0 || nLockTime > LOCKTIME_MAX)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, locktime out of range");
        rawTx.tx.nLockTime = nLockTime;
    }

    UniValue inputs;
    if (inputs_in.isNull()) {
        inputs = UniValue::VARR;
    } else {
        inputs = inputs_in.get_array();
    }

    for (unsigned int idx = 0; idx < inputs.size(); idx++) {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        uint256 txid = ParseHashO(o, "txid");

        const UniValue& vout_v = find_value(o, "vout");
        if (!vout_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.getInt<int>();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout cannot be negative");

        uint32_t nSequence;

        if (rbf.value_or(true)) {
            nSequence = MAX_BIP125_RBF_SEQUENCE; /* CTxIn::SEQUENCE_FINAL - 2 */
        } else if (rawTx.tx.nLockTime) {
            nSequence = CTxIn::MAX_SEQUENCE_NONFINAL; /* CTxIn::SEQUENCE_FINAL - 1 */
        } else {
            nSequence = CTxIn::SEQUENCE_FINAL;
        }

        // set the sequence number if passed in the parameters object
        const UniValue& sequenceObj = find_value(o, "sequence");
        if (sequenceObj.isNum()) {
            int64_t seqNr64 = sequenceObj.getInt<int64_t>();
            if (seqNr64 < 0 || seqNr64 > CTxIn::SEQUENCE_FINAL) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, sequence number is out of range");
            } else {
                nSequence = (uint32_t)seqNr64;
            }
        }

        CTxIn in(COutPoint(txid, nOutput), CScript(), nSequence);

        rawTx.tx.vin.push_back(in);
    }

    const bool outputs_is_obj = outputs_in.isObject();
    UniValue outputs = outputs_is_obj ? outputs_in.get_obj() : outputs_in.get_array();
    if (!outputs_is_obj) {
        // Translate array of key-value pairs into dict
        UniValue outputs_dict = UniValue(UniValue::VOBJ);
        for (size_t i = 0; i < outputs.size(); ++i) {
            const UniValue& output = outputs[i];
            if (!output.isObject()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, key-value pair not an object as expected");
            }
            if (output.size() != 1) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, key-value pair must contain exactly one key");
            }
            outputs_dict.pushKVs(output);
        }
        outputs = std::move(outputs_dict);
    }

    // Duplicate checking
    std::set<CTxDestination> destinations;
    bool has_data{false};

    for (const std::string& name_ : outputs.getKeys()) {
        if (name_ == "data") {
            if (has_data) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicate key: data");
            }
            has_data = true;
            std::vector<unsigned char> data = ParseHexV(outputs[name_].getValStr(), "Data");
            CScript scriptPubKey = (CScript() << OP_RETURN << data);

            CTxOut out(0, scriptPubKey);
            rawTx.tx.vout.push_back(std::move(out));
            //rawTx.m_recipients.push_back(CRecipient{GenericAddress(scriptPubKey), 0, false});
        } else {
            CTxDestination destination = DecodeDestination(name_);
            if (!IsValidDestination(destination)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Litecoin address: ") + name_);
            }

            if (!destinations.insert(destination).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + name_);
            }

            CAmount nAmount = AmountFromValue(outputs[name_]);

            GenericAddress address{destination};
            if (address.IsMWEB()) {
                mw::MutableOutput mweb_output;
                mweb_output.amount = nAmount;
                mweb_output.address = address.GetMWEBAddress();
                rawTx.tx.mweb_tx.outputs.push_back(std::move(mweb_output));
            } else {
                CTxOut out(nAmount, address.GetScript());
                rawTx.tx.vout.push_back(std::move(out));
            }
        }
    }

    if (rbf.has_value() && rbf.value() && rawTx.tx.vin.size() > 0 && !SignalsOptInRBF(rawTx.ToTransaction())) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter combination: Sequence number(s) contradict replaceable option");
    }

    return rawTx;
}

FundTransactionResult RawTransaction::FundTransaction(CWallet& wallet, const int nChangePosIn, const bool lockUnspents, const std::set<int>& setSubtractFeeFromOutputs, const CCoinControl& coin_control_in)
{
    CCoinControl coinControl = coin_control_in;

    // Acquire the locks to prevent races to the new locked unspents between the
    // CreateTransaction call and LockCoin calls (when lockUnspents is true).
    LOCK(wallet.cs_wallet);

    // Fetch specified UTXOs from the UTXO set to get the scriptPubKeys and values of the outputs being selected
    // and to match with the given solving_data. Only used for non-wallet outputs.
    std::map<GenericOutputID, GenericCoin> coins;
    for (const GenericInput& input : tx.GetInputs()) {
        coins[input.GetID()]; // Create empty map entry keyed by prevout.
    }
    wallet.chain().findCoins(coins);

    for (const GenericInput& input : tx.GetInputs()) {
        const GenericOutputID input_idx = input.GetID();
        if (wallet.IsMine(input_idx)) {
            // The input was found in the wallet, so select as internal
            coinControl.Select(input_idx);
        } else if (coins[input_idx].IsNull()) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Unable to find UTXO for external input");
        } else {
            // The input was not in the wallet, but is in the UTXO set, so select as external
            coinControl.SelectExternal(input_idx, coins[input_idx].ToOutput());
        }
    }

    std::vector<CRecipient> recipients = BuildRecipients(setSubtractFeeFromOutputs);
    auto res = CreateTransaction(wallet, recipients, nChangePosIn, coinControl, false);
    if (!res) {
        throw JSONRPCError(RPC_WALLET_ERROR, util::ErrorString(res).original);
    }
    const auto& txr = *res;
    CTransactionRef tx_new = txr.tx;
    tx.vout = tx_new->vout;
    //if (txr.change_pos.IsLTC()) {
    //    const int nChangePos = (int)txr.change_pos.ToLTC();
    //    tx.vout.insert(tx.vout.begin() + nChangePos, tx_new->vout[nChangePos]);
    //    //CRecipient change_recipient{GenericAddress(tx_new->vout[nChangePos].scriptPubKey), tx_new->vout[nChangePos].nValue, false};
    //    //m_recipients.insert_at((size_t)nChangePos, std::move(change_recipient));
    //}
    //assert(tx.vout.size() == tx_new->vout.size());
    //// MW: TODO - Copy finalized MWEB inputs, outputs, and kernels to tx.mweb_tx
    //
    //// Copy output amounts from new transaction; they may have had the fee
    //// subtracted from them.
    //for (unsigned int idx = 0; idx < tx_new->vout.size(); idx++) {
    //    tx.vout[idx].nValue = tx_new->vout[idx].nValue;
    //    //m_recipients[idx].nAmount = tx_new->vout[idx].nValue;
    //}

    // Add new txins while keeping original txin scriptSig/order.
    for (const GenericInput& tx_input : tx_new->GetInputs()) {
        if (!coinControl.IsSelected(tx_input.GetID())) {
            if (tx_input.IsMWEB()) {
                mw::MutableInput mweb_input(tx_input.ToMWEB());
                // MW: TODO - Finish populating mweb_input
                tx.mweb_tx.inputs.push_back(std::move(mweb_input));
            } else {
                tx.vin.push_back(tx_input.GetTxIn());
            }
        }
        if (lockUnspents) {
            wallet.LockCoin(tx_input.GetID());
        }
    }

    return FundTransactionResult{txr.tx, txr.fee, txr.change_pos};
}

RawTransaction RawTransaction::FromHex(const std::string& hex, const bool try_no_witness, const bool try_witness)
{
    RawTransaction raw_tx;

    // MW: TODO - How do we decode hex MWEB transactions?
    if (!DecodeHexTx(raw_tx.tx, hex, try_no_witness, try_witness)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    return raw_tx;
}

std::vector<CRecipient> RawTransaction::BuildRecipients(const std::set<int>& setSubtractFeeFromOutputs)
{
    std::vector<CRecipient> recipients;

    for (size_t i = 0; i < tx.vout.size(); i++) {
        const CTxOut& out = tx.vout[i];
        CRecipient recipient{};
        recipient.nAmount = out.nValue;
        recipient.receiver = out.scriptPubKey;
        recipient.fSubtractFeeFromAmount = (setSubtractFeeFromOutputs.count(i) == 1);
        recipients.push_back(std::move(recipient));
    }

    for (size_t i = 0; i < tx.mweb_tx.outputs.size(); i++) {
        mw::MutableOutput& mweb_output = tx.mweb_tx.outputs[i];
        assert(mweb_output.recipient.has_value());

        mweb_output.recipient->fSubtractFeeFromAmount = (setSubtractFeeFromOutputs.count(tx.vout.size() + i) == 1);
        recipients.push_back(std::move(recipient));
    }

    return recipients;
}

CMutableTransaction RawTransaction::ToMutableTx() const
{
    //CMutableTransaction tx;
    //tx.nVersion = m_version;
    //tx.nLockTime = m_lockTime;

    //for (const Input& input : m_inputs) {
    //    if (input.tx_input.IsMWEB()) {
    //        // MW: TODO - Finish this
    //        mw::MutableInput mutable_input{input.tx_input.ToMWEB()};

    //        if (input.coin.has_value()) {
    //            const MWWalletUTXO& mw_output = input.coin.value().GetMWEB();
    //            mutable_input.amount = mw_output.coin.amount;
    //            mutable_input.raw_blind = mw_output.coin.blind;
    //            mutable_input.spend_key = mw_output.coin.spend_key;
    //        }

    //        tx.mweb_tx.inputs.push_back(std::move(mutable_input));
    //    } else {
    //        tx.vin.push_back(input.tx_input.GetTxIn());
    //    }
    //}

    //for (const CRecipient& recipient : m_recipients.All()) {
    //    if (recipient.IsMWEB()) {
    //        mw::MutableOutput mutable_output{};
    //        mutable_output.amount = recipient.nAmount;
    //        mutable_output.address = recipient.GetMWEBAddress();
    //        tx.mweb_tx.outputs.push_back(std::move(mutable_output));
    //    } else {
    //        CTxOut out(recipient.nAmount, recipient.GetScript());
    //        tx.vout.push_back(out);
    //    }
    //}

    //return tx;
    return tx;
}

CTransaction RawTransaction::ToTransaction() const
{
    return CTransaction(tx);
}

PartiallySignedTransaction RawTransaction::ToPSBT(const CWallet& wallet) const
{
    // Make a blank psbt
    PartiallySignedTransaction psbtx(tx, 2);
    LogPrintf("ToPSBT() - Blank PSBT created\n");
    psbtx.SetupFromTx(tx);

    // First fill transaction with our data without signing,
    // so external signers are not asked sign more than once.
    bool complete;
    wallet.FillPSBT(psbtx, complete, SIGHASH_DEFAULT, false, true);
    LogPrintf("ToPSBT() - BIP32 Derived\n");
    const TransactionError err{wallet.FillPSBT(psbtx, complete, SIGHASH_DEFAULT, true, false)};
    if (err != TransactionError::OK) {
        LogPrintf("ToPSBT() - Signed with error\n");
        throw JSONRPCTransactionError(err);
    }

    LogPrintf("ToPSBT() - Signed successfully\n");
    return psbtx;
}

std::string RawTransaction::ToHex(const int serialize_flags) const
{
    return EncodeHexTx(ToTransaction(), serialize_flags);
}
