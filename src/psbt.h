// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PSBT_H
#define BITCOIN_PSBT_H

#include <attributes.h>
#include <node/transaction.h>
#include <optional.h>
#include <policy/feerate.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/sign.h>
#include <script/signingprovider.h>

#include <bitset>

// Magic bytes
static constexpr uint8_t PSBT_MAGIC_BYTES[5] = {'p', 's', 'b', 't', 0xff};

// Global types
static constexpr uint8_t PSBT_GLOBAL_UNSIGNED_TX = 0x00;
static constexpr uint8_t PSBT_GLOBAL_XPUB = 0x01;
static constexpr uint8_t PSBT_GLOBAL_TX_VERSION = 0x02;
static constexpr uint8_t PSBT_GLOBAL_FALLBACK_LOCKTIME = 0x03;
static constexpr uint8_t PSBT_GLOBAL_INPUT_COUNT = 0x04;
static constexpr uint8_t PSBT_GLOBAL_OUTPUT_COUNT = 0x05;
static constexpr uint8_t PSBT_GLOBAL_TX_MODIFIABLE = 0x06;
static constexpr uint8_t PSBT_GLOBAL_VERSION = 0xFB;
static constexpr uint8_t PSBT_GLOBAL_PROPRIETARY = 0xFC;

// Input types
static constexpr uint8_t PSBT_IN_NON_WITNESS_UTXO = 0x00;
static constexpr uint8_t PSBT_IN_WITNESS_UTXO = 0x01;
static constexpr uint8_t PSBT_IN_PARTIAL_SIG = 0x02;
static constexpr uint8_t PSBT_IN_SIGHASH = 0x03;
static constexpr uint8_t PSBT_IN_REDEEMSCRIPT = 0x04;
static constexpr uint8_t PSBT_IN_WITNESSSCRIPT = 0x05;
static constexpr uint8_t PSBT_IN_BIP32_DERIVATION = 0x06;
static constexpr uint8_t PSBT_IN_SCRIPTSIG = 0x07;
static constexpr uint8_t PSBT_IN_SCRIPTWITNESS = 0x08;
static constexpr uint8_t PSBT_IN_RIPEMD160 = 0x0A;
static constexpr uint8_t PSBT_IN_SHA256 = 0x0B;
static constexpr uint8_t PSBT_IN_HASH160 = 0x0C;
static constexpr uint8_t PSBT_IN_HASH256 = 0x0D;
static constexpr uint8_t PSBT_IN_PREVIOUS_TXID = 0x0e;
static constexpr uint8_t PSBT_IN_OUTPUT_INDEX = 0x0f;
static constexpr uint8_t PSBT_IN_SEQUENCE = 0x10;
static constexpr uint8_t PSBT_IN_REQUIRED_TIME_LOCKTIME = 0x11;
static constexpr uint8_t PSBT_IN_REQUIRED_HEIGHT_LOCKTIME = 0x12;
static constexpr uint8_t PSBT_IN_TAP_KEY_SIG = 0x13;
static constexpr uint8_t PSBT_IN_TAP_SCRIPT_SIG = 0x14;
static constexpr uint8_t PSBT_IN_TAP_LEAF_SCRIPT = 0x15;
static constexpr uint8_t PSBT_IN_TAP_BIP32_DERIVATION = 0x16;
static constexpr uint8_t PSBT_IN_TAP_INTERNAL_KEY = 0x17;
static constexpr uint8_t PSBT_IN_TAP_MERKLE_ROOT = 0x18;
static constexpr uint8_t PSBT_IN_PROPRIETARY = 0xFC;

// Output types
static constexpr uint8_t PSBT_OUT_REDEEMSCRIPT = 0x00;
static constexpr uint8_t PSBT_OUT_WITNESSSCRIPT = 0x01;
static constexpr uint8_t PSBT_OUT_BIP32_DERIVATION = 0x02;
static constexpr uint8_t PSBT_OUT_AMOUNT = 0x03;
static constexpr uint8_t PSBT_OUT_SCRIPT = 0x04;
static constexpr uint8_t PSBT_OUT_TAP_INTERNAL_KEY = 0x05;
static constexpr uint8_t PSBT_OUT_TAP_TREE = 0x06;
static constexpr uint8_t PSBT_OUT_TAP_BIP32_DERIVATION = 0x07;
static constexpr uint8_t PSBT_OUT_PROPRIETARY = 0xFC;

// The separator is 0x00. Reading this in means that the unserializer can interpret it
// as a 0 length key which indicates that this is the separator. The separator has no value.
static constexpr uint8_t PSBT_SEPARATOR = 0x00;

// BIP 174 does not specify a maximum file size, but we set a limit anyway
// to prevent reading a stream indefinitely and running out of memory.
const std::streamsize MAX_FILE_SIZE_PSBT = 100000000; // 100 MiB

// PSBT version number
static constexpr uint32_t PSBT_HIGHEST_VERSION = 2;

/** A structure for PSBTs which contain per-input information */
struct PSBTInput
{
    CTransactionRef non_witness_utxo;
    CTxOut witness_utxo;
    CScript redeem_script;
    CScript witness_script;
    CScript final_script_sig;
    CScriptWitness final_script_witness;
    std::map<CPubKey, KeyOriginInfo> hd_keypaths;
    std::map<CKeyID, SigPair> partial_sigs;

    uint256 prev_txid;
    Optional<uint32_t> prev_out;
    Optional<uint32_t> sequence;
    Optional<uint32_t> time_locktime;
    Optional<uint32_t> height_locktime;

    std::map<std::vector<unsigned char>, std::vector<unsigned char>> unknown;
    int sighash_type = 0;

    uint32_t m_psbt_version;

    bool IsNull() const;
    void FillSignatureData(SignatureData& sigdata) const;
    void FromSignatureData(const SignatureData& sigdata);
    void Merge(const PSBTInput& input);
    /**
     * Retrieves the UTXO for this input
     *
     * @param[out] utxo The UTXO of this input
     * @return Whether the UTXO could be retrieved
     */
    bool GetUTXO(CTxOut& utxo) const;
    COutPoint GetOutPoint() const;
    PSBTInput(uint32_t version) : m_psbt_version(version) {}

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        // Write the utxo
        if (non_witness_utxo) {
            SerializeToVector(s, PSBT_IN_NON_WITNESS_UTXO);
            OverrideStream<Stream> os(&s, s.GetType(), s.GetVersion() | SERIALIZE_TRANSACTION_NO_WITNESS | SERIALIZE_NO_MWEB);
            SerializeToVector(os, non_witness_utxo);
        }
        if (!witness_utxo.IsNull()) {
            SerializeToVector(s, PSBT_IN_WITNESS_UTXO);
            SerializeToVector(s, witness_utxo);
        }

        if (final_script_sig.empty() && final_script_witness.IsNull()) {
            // Write any partial signatures
            for (auto sig_pair : partial_sigs) {
                SerializeToVector(s, PSBT_IN_PARTIAL_SIG, MakeSpan(sig_pair.second.first));
                s << sig_pair.second.second;
            }

            // Write the sighash type
            if (sighash_type > 0) {
                SerializeToVector(s, PSBT_IN_SIGHASH);
                SerializeToVector(s, sighash_type);
            }

            // Write the redeem script
            if (!redeem_script.empty()) {
                SerializeToVector(s, PSBT_IN_REDEEMSCRIPT);
                s << redeem_script;
            }

            // Write the witness script
            if (!witness_script.empty()) {
                SerializeToVector(s, PSBT_IN_WITNESSSCRIPT);
                s << witness_script;
            }

            // Write any hd keypaths
            SerializeHDKeypaths(s, hd_keypaths, PSBT_IN_BIP32_DERIVATION);
        }

        // Write script sig
        if (!final_script_sig.empty()) {
            SerializeToVector(s, PSBT_IN_SCRIPTSIG);
            s << final_script_sig;
        }
        // write script witness
        if (!final_script_witness.IsNull()) {
            SerializeToVector(s, PSBT_IN_SCRIPTWITNESS);
            SerializeToVector(s, final_script_witness.stack);
        }

        // Write PSBTv2 fields
        if (m_psbt_version >= 2) {
            // Write prev txid, vout, sequence, and lock times
            if (!prev_txid.IsNull()) {
                SerializeToVector(s, CompactSizeWriter(PSBT_IN_PREVIOUS_TXID));
                SerializeToVector(s, prev_txid);
            }
            if (prev_out != nullopt) {
                SerializeToVector(s, CompactSizeWriter(PSBT_IN_OUTPUT_INDEX));
                SerializeToVector(s, *prev_out);
            }
            if (sequence != nullopt) {
                SerializeToVector(s, CompactSizeWriter(PSBT_IN_SEQUENCE));
                SerializeToVector(s, *sequence);
            }
            if (time_locktime != nullopt) {
                SerializeToVector(s, CompactSizeWriter(PSBT_IN_REQUIRED_TIME_LOCKTIME));
                SerializeToVector(s, *time_locktime);
            }
            if (height_locktime != nullopt) {
                SerializeToVector(s, CompactSizeWriter(PSBT_IN_REQUIRED_HEIGHT_LOCKTIME));
                SerializeToVector(s, *height_locktime);
            }
        }

        // Write unknown things
        for (auto& entry : unknown) {
            s << entry.first;
            s << entry.second;
        }

        s << PSBT_SEPARATOR;
    }


    template <typename Stream>
    inline void Unserialize(Stream& s) {
        // Used for duplicate key detection
        std::set<std::vector<unsigned char>> key_lookup;

        // Read loop
        bool found_sep = false;
        while(!s.empty()) {
            // Read
            std::vector<unsigned char> key;
            s >> key;

            // the key is empty if that was actually a separator byte
            // This is a special case for key lengths 0 as those are not allowed (except for separator)
            if (key.empty()) {
                found_sep = true;
                break;
            }

            // First byte of key is the type
            unsigned char type = key[0];

            // Do stuff based on type
            switch(type) {
                case PSBT_IN_NON_WITNESS_UTXO:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, input non-witness utxo already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Non-witness utxo key is more than one byte type");
                    }
                    // Set the stream to unserialize with witness since this is always a valid network transaction
                    OverrideStream<Stream> os(&s, s.GetType(), s.GetVersion() & ~SERIALIZE_TRANSACTION_NO_WITNESS);
                    UnserializeFromVector(os, non_witness_utxo);
                    break;
                }
                case PSBT_IN_WITNESS_UTXO:
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, input witness utxo already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Witness utxo key is more than one byte type");
                    }
                    UnserializeFromVector(s, witness_utxo);
                    break;
                case PSBT_IN_PARTIAL_SIG:
                {
                    // Make sure that the key is the size of pubkey + 1
                    if (key.size() != CPubKey::SIZE + 1 && key.size() != CPubKey::COMPRESSED_SIZE + 1) {
                        throw std::ios_base::failure("Size of key was not the expected size for the type partial signature pubkey");
                    }
                    // Read in the pubkey from key
                    CPubKey pubkey(key.begin() + 1, key.end());
                    if (!pubkey.IsFullyValid()) {
                       throw std::ios_base::failure("Invalid pubkey");
                    }
                    if (partial_sigs.count(pubkey.GetID()) > 0) {
                        throw std::ios_base::failure("Duplicate Key, input partial signature for pubkey already provided");
                    }

                    // Read in the signature from value
                    std::vector<unsigned char> sig;
                    s >> sig;

                    // Add to list
                    partial_sigs.emplace(pubkey.GetID(), SigPair(pubkey, std::move(sig)));
                    break;
                }
                case PSBT_IN_SIGHASH:
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, input sighash type already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Sighash type key is more than one byte type");
                    }
                    UnserializeFromVector(s, sighash_type);
                    break;
                case PSBT_IN_REDEEMSCRIPT:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, input redeemScript already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Input redeemScript key is more than one byte type");
                    }
                    s >> redeem_script;
                    break;
                }
                case PSBT_IN_WITNESSSCRIPT:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, input witnessScript already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Input witnessScript key is more than one byte type");
                    }
                    s >> witness_script;
                    break;
                }
                case PSBT_IN_BIP32_DERIVATION:
                {
                    DeserializeHDKeypaths(s, key, hd_keypaths);
                    break;
                }
                case PSBT_IN_SCRIPTSIG:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, input final scriptSig already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Final scriptSig key is more than one byte type");
                    }
                    s >> final_script_sig;
                    break;
                }
                case PSBT_IN_SCRIPTWITNESS:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, input final scriptWitness already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Final scriptWitness key is more than one byte type");
                    }
                    UnserializeFromVector(s, final_script_witness.stack);
                    break;
                }
                case PSBT_IN_PREVIOUS_TXID:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, previous txid is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Previous txid key is more than one byte type");
                    } else if (m_psbt_version == 0) {
                        throw std::ios_base::failure("Previous txid is not allowed in PSBTv0");
                    }
                    UnserializeFromVector(s, prev_txid);
                    break;
                }
                case PSBT_IN_OUTPUT_INDEX:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, previous output's index is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Previous output's index is more than one byte type");
                    } else if (m_psbt_version == 0) {
                        throw std::ios_base::failure("Previous output's index is not allowed in PSBTv0");
                    }
                    uint32_t v;
                    UnserializeFromVector(s, v);
                    prev_out = v;
                    break;
                }
                case PSBT_IN_SEQUENCE:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, sequence is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Sequence key is more than one byte type");
                    } else if (m_psbt_version == 0) {
                        throw std::ios_base::failure("Sequence is not allowed in PSBTv0");
                    }
                    uint32_t v;
                    UnserializeFromVector(s, v);
                    sequence = v;
                    break;
                }
                case PSBT_IN_REQUIRED_TIME_LOCKTIME:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, required time based locktime is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Required time based locktime is more than one byte type");
                    } else if (m_psbt_version == 0) {
                        throw std::ios_base::failure("Required time based locktime is not allowed in PSBTv0");
                    }
                    uint32_t v;
                    UnserializeFromVector(s, v);
                    if (v < LOCKTIME_THRESHOLD) {
                        throw std::ios_base::failure("Required time based locktime is invalid (less than 500000000)");
                    }
                    time_locktime = v;
                    break;
                }
                case PSBT_IN_REQUIRED_HEIGHT_LOCKTIME:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, required height based locktime is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Required height based locktime is more than one byte type");
                    } else if (m_psbt_version == 0) {
                        throw std::ios_base::failure("Required height based locktime is not allowed in PSBTv0");
                    }
                    uint32_t v;
                    UnserializeFromVector(s, v);
                    if (v >= LOCKTIME_THRESHOLD) {
                        throw std::ios_base::failure("Required time based locktime is invalid (greater than or equal to 500000000)");
                    }
                    height_locktime = v;
                    break;
                }
                // Unknown stuff
                default:
                    if (unknown.count(key) > 0) {
                        throw std::ios_base::failure("Duplicate Key, key for unknown value already provided");
                    }
                    // Read in the value
                    std::vector<unsigned char> val_bytes;
                    s >> val_bytes;
                    unknown.emplace(std::move(key), std::move(val_bytes));
                    break;
            }
        }

        if (!found_sep) {
            throw std::ios_base::failure("Separator is missing at the end of an input map");
        }

        // Make sure required PSBTv2 fields are present
        if (m_psbt_version >= 2) {
            if (prev_txid.IsNull()) {
                throw std::ios_base::failure("Previous TXID is required in PSBTv2");
            }
            if (prev_out == nullopt) {
                throw std::ios_base::failure("Previous output's index is required in PSBTv2");
            }
        }
    }

    template <typename Stream>
    PSBTInput(deserialize_type, Stream& s) {
        Unserialize(s);
    }
};

/** A structure for PSBTs which contains per output information */
struct PSBTOutput
{
    CScript redeem_script;
    CScript witness_script;
    std::map<CPubKey, KeyOriginInfo> hd_keypaths;

    Optional<CAmount> amount;
    Optional<CScript> script;

    std::map<std::vector<unsigned char>, std::vector<unsigned char>> unknown;

    uint32_t m_psbt_version;

    bool IsNull() const;
    void FillSignatureData(SignatureData& sigdata) const;
    void FromSignatureData(const SignatureData& sigdata);
    void Merge(const PSBTOutput& output);
    PSBTOutput(uint32_t version) : m_psbt_version(version) {}

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        // Write the redeem script
        if (!redeem_script.empty()) {
            SerializeToVector(s, PSBT_OUT_REDEEMSCRIPT);
            s << redeem_script;
        }

        // Write the witness script
        if (!witness_script.empty()) {
            SerializeToVector(s, PSBT_OUT_WITNESSSCRIPT);
            s << witness_script;
        }

        // Write any hd keypaths
        SerializeHDKeypaths(s, hd_keypaths, PSBT_OUT_BIP32_DERIVATION);

        if (m_psbt_version >= 2) {
            // Write amount and spk
            if (amount != nullopt) {
                SerializeToVector(s, CompactSizeWriter(PSBT_OUT_AMOUNT));
                SerializeToVector(s, *amount);
            }
            if (script.has_value()) {
                SerializeToVector(s, CompactSizeWriter(PSBT_OUT_SCRIPT));
                s << *script;
            }
        }

        // Write unknown things
        for (auto& entry : unknown) {
            s << entry.first;
            s << entry.second;
        }

        s << PSBT_SEPARATOR;
    }


    template <typename Stream>
    inline void Unserialize(Stream& s) {
        // Used for duplicate key detection
        std::set<std::vector<unsigned char>> key_lookup;

        // Read loop
        bool found_sep = false;
        while(!s.empty()) {
            // Read
            std::vector<unsigned char> key;
            s >> key;

            // the key is empty if that was actually a separator byte
            // This is a special case for key lengths 0 as those are not allowed (except for separator)
            if (key.empty()) {
                found_sep = true;
                break;
            }

            // First byte of key is the type
            unsigned char type = key[0];

            // Do stuff based on type
            switch(type) {
                case PSBT_OUT_REDEEMSCRIPT:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, output redeemScript already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Output redeemScript key is more than one byte type");
                    }
                    s >> redeem_script;
                    break;
                }
                case PSBT_OUT_WITNESSSCRIPT:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, output witnessScript already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Output witnessScript key is more than one byte type");
                    }
                    s >> witness_script;
                    break;
                }
                case PSBT_OUT_BIP32_DERIVATION:
                {
                    DeserializeHDKeypaths(s, key, hd_keypaths);
                    break;
                }
                case PSBT_OUT_AMOUNT:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, output amount is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Output amount key is more than one byte type");
                    } else if (m_psbt_version == 0) {
                        throw std::ios_base::failure("Output amount is not allowed in PSBTv0");
                    }
                    CAmount v;
                    UnserializeFromVector(s, v);
                    amount = v;
                    break;
                }
                case PSBT_OUT_SCRIPT:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, output script is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Output script key is more than one byte type");
                    } else if (m_psbt_version == 0) {
                        throw std::ios_base::failure("Output script is not allowed in PSBTv0");
                    }
                    CScript v;
                    s >> v;
                    script = v;
                    break;
                }
                // Unknown stuff
                default: {
                    if (unknown.count(key) > 0) {
                        throw std::ios_base::failure("Duplicate Key, key for unknown value already provided");
                    }
                    // Read in the value
                    std::vector<unsigned char> val_bytes;
                    s >> val_bytes;
                    unknown.emplace(std::move(key), std::move(val_bytes));
                    break;
                }
            }
        }

        if (!found_sep) {
            throw std::ios_base::failure("Separator is missing at the end of an output map");
        }

        // Make sure required PSBTv2 fields are present
        if (m_psbt_version >= 2) {
            if (amount == nullopt) {
                throw std::ios_base::failure("Output amount is required in PSBTv2");
            }
            if (!script.has_value()) {
                throw std::ios_base::failure("Output script is required in PSBTv2");
            }
        }
    }

    template <typename Stream>
    PSBTOutput(deserialize_type, Stream& s) {
        Unserialize(s);
    }
};

/** A version of CTransaction with the PSBT format*/
struct PartiallySignedTransaction
{
    Optional<CMutableTransaction> tx;
    Optional<int32_t> tx_version;
    Optional<uint32_t> fallback_locktime;
    Optional<std::bitset<8>> m_tx_modifiable;
    std::vector<PSBTInput> inputs;
    std::vector<PSBTOutput> outputs;
    std::map<std::vector<unsigned char>, std::vector<unsigned char>> unknown;
    Optional<uint32_t> m_version;

    bool IsNull() const;
    uint32_t GetVersion() const;

    /** Merge psbt into this. The two psbts must have the same underlying CTransaction (i.e. the
      * same actual Bitcoin transaction.) Returns true if the merge succeeded, false otherwise. */
    NODISCARD bool Merge(const PartiallySignedTransaction& psbt);
    bool AddInput(PSBTInput& psbtin);
    bool AddOutput(const PSBTOutput& psbtout);
    void SetupFromTx(const CMutableTransaction& tx);
    void CacheUnsignedTxPieces();
    bool ComputeTimeLock(uint32_t& locktime) const;
    CMutableTransaction GetUnsignedTx() const;
    uint256 GetUniqueID() const;
    PartiallySignedTransaction() {}
    PartiallySignedTransaction(uint32_t version);
    explicit PartiallySignedTransaction(const CMutableTransaction& tx, uint32_t version = 0);

    template <typename Stream>
    inline void Serialize(Stream& s) const {

        // magic bytes
        s << PSBT_MAGIC_BYTES;

        if (GetVersion() == 0) {
            // unsigned tx flag
            SerializeToVector(s, CompactSizeWriter(PSBT_GLOBAL_UNSIGNED_TX));

            // Write serialized tx to a stream
            OverrideStream<Stream> os(&s, s.GetType(), s.GetVersion() | SERIALIZE_TRANSACTION_NO_WITNESS | SERIALIZE_NO_MWEB);
            SerializeToVector(os, GetUnsignedTx());
        }

        if (GetVersion() >= 2) {
            // Write PSBTv2 tx version, locktime, counts, etc.
            SerializeToVector(s, CompactSizeWriter(PSBT_GLOBAL_TX_VERSION));
            SerializeToVector(s, *tx_version);
            if (fallback_locktime != nullopt) {
                SerializeToVector(s, CompactSizeWriter(PSBT_GLOBAL_FALLBACK_LOCKTIME));
                SerializeToVector(s, *fallback_locktime);
            }

            SerializeToVector(s, CompactSizeWriter(PSBT_GLOBAL_INPUT_COUNT));
            SerializeToVector(s, CompactSizeWriter(inputs.size()));
            SerializeToVector(s, CompactSizeWriter(PSBT_GLOBAL_OUTPUT_COUNT));
            SerializeToVector(s, CompactSizeWriter(outputs.size()));

            if (m_tx_modifiable != nullopt) {
                SerializeToVector(s, CompactSizeWriter(PSBT_GLOBAL_TX_MODIFIABLE));
                SerializeToVector(s, static_cast<uint8_t>(m_tx_modifiable->to_ulong()));
            }
        }

        // PSBT version
        if (GetVersion() > 0) {
            SerializeToVector(s, CompactSizeWriter(PSBT_GLOBAL_VERSION));
            SerializeToVector(s, *m_version);
        }

        // Write the unknown things
        for (auto& entry : unknown) {
            s << entry.first;
            s << entry.second;
        }

        // Separator
        s << PSBT_SEPARATOR;

        // Write inputs
        for (const PSBTInput& input : inputs) {
            s << input;
        }
        // Write outputs
        for (const PSBTOutput& output : outputs) {
            s << output;
        }
    }


    template <typename Stream>
    inline void Unserialize(Stream& s) {
        // Read the magic bytes
        uint8_t magic[5];
        s >> magic;
        if (!std::equal(magic, magic + 5, PSBT_MAGIC_BYTES)) {
            throw std::ios_base::failure("Invalid PSBT magic bytes");
        }

        // Used for duplicate key detection
        std::set<std::vector<unsigned char>> key_lookup;

        // Read global data
        bool found_sep = false;
        uint64_t input_count = 0;
        uint64_t output_count = 0;
        bool found_input_count = false;
        bool found_output_count = false;
        while(!s.empty()) {
            // Read
            std::vector<unsigned char> key;
            s >> key;

            // the key is empty if that was actually a separator byte
            // This is a special case for key lengths 0 as those are not allowed (except for separator)
            if (key.empty()) {
                found_sep = true;
                break;
            }

            // First byte of key is the type
            unsigned char type = key[0];

            // Do stuff based on type
            switch(type) {
                case PSBT_GLOBAL_UNSIGNED_TX:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, unsigned tx already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Global unsigned tx key is more than one byte type");
                    }
                    CMutableTransaction mtx;
                    // Set the stream to serialize with non-witness since this should always be non-witness
                    OverrideStream<Stream> os(&s, s.GetType(), s.GetVersion() | SERIALIZE_TRANSACTION_NO_WITNESS | SERIALIZE_NO_MWEB);
                    UnserializeFromVector(os, mtx);
                    tx = std::move(mtx);
                    // Make sure that all scriptSigs and scriptWitnesses are empty
                    for (const CTxIn& txin : tx->vin) {
                        if (!txin.scriptSig.empty() || !txin.scriptWitness.IsNull()) {
                            throw std::ios_base::failure("Unsigned tx does not have empty scriptSigs and scriptWitnesses.");
                        }
                    }
                    // Set the input and output counts
                    input_count = tx->vin.size();
                    output_count = tx->vout.size();
                    break;
                }
                case PSBT_GLOBAL_TX_VERSION:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, global transaction version is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Global transaction version key is more than one byte type");
                    }
                    uint32_t v;
                    UnserializeFromVector(s, v);
                    tx_version = v;
                    break;
                }
                case PSBT_GLOBAL_FALLBACK_LOCKTIME:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, global fallback locktime is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Global fallback locktime key is more than one byte type");
                    }
                    uint32_t v;
                    UnserializeFromVector(s, v);
                    fallback_locktime = v;
                    break;
                }
                case PSBT_GLOBAL_INPUT_COUNT:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, global input count is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Global input count key is more than one byte type");
                    }
                    CompactSizeReader reader(input_count);
                    UnserializeFromVector(s, reader);
                    found_input_count = true;
                    break;
                }
                case PSBT_GLOBAL_OUTPUT_COUNT:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, global output count is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Global output count key is more than one byte type");
                    }
                    CompactSizeReader reader(output_count);
                    UnserializeFromVector(s, reader);
                    found_output_count = true;
                    break;
                }
                case PSBT_GLOBAL_TX_MODIFIABLE:
                {
                    if (!key_lookup.emplace(key).second) {
                        throw std::ios_base::failure("Duplicate Key, tx modifiable flags is already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Global tx modifiable flags key is more than one byte type");
                    }
                    uint8_t tx_mod;
                    UnserializeFromVector(s, tx_mod);
                    m_tx_modifiable.emplace(tx_mod);
                    break;
                }
                case PSBT_GLOBAL_VERSION:
                {
                    if (m_version) {
                        throw std::ios_base::failure("Duplicate Key, version already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure("Global version key is more than one byte type");
                    }
                    uint32_t v;
                    UnserializeFromVector(s, v);
                    m_version = v;
                    if (*m_version > PSBT_HIGHEST_VERSION) {
                        throw std::ios_base::failure("Unsupported version number");
                    }
                    break;
                }
                // Unknown stuff
                default: {
                    if (unknown.count(key) > 0) {
                        throw std::ios_base::failure("Duplicate Key, key for unknown value already provided");
                    }
                    // Read in the value
                    std::vector<unsigned char> val_bytes;
                    s >> val_bytes;
                    unknown.emplace(std::move(key), std::move(val_bytes));
                }
            }
        }

        if (!found_sep) {
            throw std::ios_base::failure("Separator is missing at the end of the global map");
        }

        uint32_t psbt_ver = GetVersion();

        // Check PSBT version constraints
        if (psbt_ver == 0) {
            // Make sure that we got an unsigned tx for PSBTv0
            if (!tx) {
                throw std::ios_base::failure("No unsigned transaction was provided");
            }
            // Make sure no PSBTv2 fields are present
            if (tx_version != nullopt) {
                throw std::ios_base::failure("PSBT_GLOBAL_TX_VERSION is not allowed in PSBTv0");
            }
            if (fallback_locktime != nullopt) {
                throw std::ios_base::failure("PSBT_GLOBAL_FALLBACK_LOCKTIME is not allowed in PSBTv0");
            }
            if (found_input_count) {
                throw std::ios_base::failure("PSBT_GLOBAL_INPUT_COUNT is not allowed in PSBTv0");
            }
            if (found_output_count) {
                throw std::ios_base::failure("PSBT_GLOBAL_OUTPUT_COUNT is not allowed in PSBTv0");
            }
            if (m_tx_modifiable != nullopt) {
                throw std::ios_base::failure("PSBT_GLOBAL_TX_MODIFIABLE is not allowed in PSBTv0");
            }
        }
        // Disallow v1
        if (psbt_ver == 1) {
            throw std::ios_base::failure("There is no PSBT version 1");
        }
        if (psbt_ver >= 2) {
            // Tx version, input, and output counts are required
            if (tx_version == nullopt) {
                throw std::ios_base::failure("PSBT_GLOBAL_TX_VERSION is required in PSBTv2");
            }
            if (!found_input_count) {
                throw std::ios_base::failure("PSBT_GLOBAL_INPUT_COUNT is required in PSBTv2");
            }
            if (!found_output_count) {
                throw std::ios_base::failure("PSBT_GLOBAL_OUTPUT_COUNT is required in PSBTv2");
            }
            // Unsigned tx is disallowed
            if (tx) {
                throw std::ios_base::failure("PSBT_GLOBAL_UNSIGNED_TX is not allowed in PSBTv2");
            }
        }

        // Read input data
        unsigned int i = 0;
        while (!s.empty() && i < input_count) {
            PSBTInput input(psbt_ver);
            s >> input;
            inputs.push_back(input);

            // Make sure the non-witness utxo matches the outpoint
            if (input.non_witness_utxo && ((tx != nullopt && input.non_witness_utxo->GetHash() != tx->vin[i].prevout.hash) || (!input.prev_txid.IsNull() && input.non_witness_utxo->GetHash() != input.prev_txid))) {
                throw std::ios_base::failure("Non-witness UTXO does not match outpoint hash");
            }
            ++i;
        }
        // Make sure that the number of inputs matches the number of inputs in the transaction
        if (inputs.size() != input_count) {
            throw std::ios_base::failure("Inputs provided does not match the number of inputs in transaction.");
        }

        // Read output data
        i = 0;
        while (!s.empty() && i < output_count) {
            PSBTOutput output(psbt_ver);
            s >> output;
            outputs.push_back(output);
            ++i;
        }
        // Make sure that the number of outputs matches the number of outputs in the transaction
        if (outputs.size() != output_count) {
            throw std::ios_base::failure("Outputs provided does not match the number of outputs in transaction.");
        }

        CacheUnsignedTxPieces();
    }

    template <typename Stream>
    PartiallySignedTransaction(deserialize_type, Stream& s) {
        Unserialize(s);
    }
};

enum class PSBTRole {
    CREATOR,
    UPDATER,
    SIGNER,
    FINALIZER,
    EXTRACTOR
};

std::string PSBTRoleName(PSBTRole role);

/** Checks whether a PSBTInput is already signed. */
bool PSBTInputSigned(const PSBTInput& input);

/** Signs a PSBTInput, verifying that all provided data matches what is being signed. */
bool SignPSBTInput(const SigningProvider& provider, PartiallySignedTransaction& psbt, int index, int sighash = SIGHASH_ALL, SignatureData* out_sigdata = nullptr, bool use_dummy = false);

/** Counts the unsigned inputs of a PSBT. */
size_t CountPSBTUnsignedInputs(const PartiallySignedTransaction& psbt);

/** Updates a PSBTOutput with information from provider.
 *
 * This fills in the redeem_script, witness_script, and hd_keypaths where possible.
 */
void UpdatePSBTOutput(const SigningProvider& provider, PartiallySignedTransaction& psbt, int index);

/**
 * Finalizes a PSBT if possible, combining partial signatures.
 *
 * @param[in,out] psbtx PartiallySignedTransaction to finalize
 * return True if the PSBT is now complete, false otherwise
 */
bool FinalizePSBT(PartiallySignedTransaction& psbtx);

/**
 * Finalizes a PSBT if possible, and extracts it to a CMutableTransaction if it could be finalized.
 *
 * @param[in]  psbtx PartiallySignedTransaction
 * @param[out] result CMutableTransaction representing the complete transaction, if successful
 * @return True if we successfully extracted the transaction, false otherwise
 */
bool FinalizeAndExtractPSBT(PartiallySignedTransaction& psbtx, CMutableTransaction& result);

/**
 * Combines PSBTs with the same underlying transaction, resulting in a single PSBT with all partial signatures from each input.
 *
 * @param[out] out   the combined PSBT, if successful
 * @param[in]  psbtxs the PSBTs to combine
 * @return error (OK if we successfully combined the transactions, other error if they were not compatible)
 */
NODISCARD TransactionError CombinePSBTs(PartiallySignedTransaction& out, const std::vector<PartiallySignedTransaction>& psbtxs);

//! Decode a base64ed PSBT into a PartiallySignedTransaction
NODISCARD bool DecodeBase64PSBT(PartiallySignedTransaction& decoded_psbt, const std::string& base64_psbt, std::string& error);
//! Decode a raw (binary blob) PSBT into a PartiallySignedTransaction
NODISCARD bool DecodeRawPSBT(PartiallySignedTransaction& decoded_psbt, const std::string& raw_psbt, std::string& error);

#endif // BITCOIN_PSBT_H
