// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLETDB_H
#define BITCOIN_WALLET_WALLETDB_H

#include <mw/models/wallet/Coin.h>
#include <script/sign.h>
#include <wallet/db.h>
#include <wallet/walletutil.h>
#include <key.h>

#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

class CScript;
class uint160;
class uint256;
struct CBlockLocator;

namespace wallet {
class CKeyPool;
class CMasterKey;
class CWallet;
class CWalletTx;
struct WalletContext;

/**
 * Overview of wallet database classes:
 *
 * - WalletBatch is an abstract modifier object for the wallet database, and encapsulates a database
 *   batch update as well as methods to act on the database. It should be agnostic to the database implementation.
 *
 * The following classes are implementation specific:
 * - BerkeleyEnvironment is an environment in which the database exists.
 * - BerkeleyDatabase represents a wallet database.
 * - BerkeleyBatch is a low-level database batch update.
 */

static const bool DEFAULT_FLUSHWALLET = true;

/** Error statuses for the wallet database */
enum class DBErrors
{
    LOAD_OK,
    CORRUPT,
    NONCRITICAL_ERROR,
    TOO_NEW,
    EXTERNAL_SIGNER_SUPPORT_REQUIRED,
    LOAD_FAIL,
    NEED_REWRITE,
    NEED_RESCAN,
    UNKNOWN_DESCRIPTOR
};

namespace DBKeys {
extern const std::string ACENTRY;
extern const std::string ACTIVEEXTERNALSPK;
extern const std::string ACTIVEINTERNALSPK;
extern const std::string BESTBLOCK;
extern const std::string BESTBLOCK_NOMERKLE;
extern const std::string COIN;
extern const std::string CRYPTED_KEY;
extern const std::string CSCRIPT;
extern const std::string DEFAULTKEY;
extern const std::string DESTDATA;
extern const std::string FLAGS;
extern const std::string HDCHAIN;
extern const std::string KEY;
extern const std::string KEYMETA;
extern const std::string LOCKED_UTXO;
extern const std::string MASTER_KEY;
extern const std::string MINVERSION;
extern const std::string NAME;
extern const std::string OLD_KEY;
extern const std::string ORDERPOSNEXT;
extern const std::string POOL;
extern const std::string PURPOSE;
extern const std::string SETTINGS;
extern const std::string TX;
extern const std::string VERSION;
extern const std::string WALLETDESCRIPTOR;
extern const std::string WALLETDESCRIPTORCKEY;
extern const std::string WALLETDESCRIPTORKEY;
extern const std::string WATCHMETA;
extern const std::string WATCHS;

// Keys in this set pertain only to the legacy wallet (LegacyScriptPubKeyMan) and are removed during migration from legacy to descriptors.
extern const std::unordered_set<std::string> LEGACY_TYPES;
} // namespace DBKeys

/* simple HD chain data model */
class CHDChain
{
public:
    uint32_t nExternalChainCounter;
    uint32_t nInternalChainCounter;
    uint32_t nMWEBIndexCounter;
    CKeyID seed_id; //!< seed hash160
    std::optional<SecretKey> mweb_scan_key;
    int64_t m_next_external_index{0}; // Next index in the keypool to be used. Memory only.
    int64_t m_next_internal_index{0}; // Next index in the keypool to be used. Memory only.
    int64_t m_next_mweb_index{0}; // Next index in the keypool to be used. Memory only.

    static const int VERSION_HD_BASE        = 1;
    static const int VERSION_HD_CHAIN_SPLIT = 2;
    static const int VERSION_HD_MWEB        = 3;
    static const int VERSION_HD_MWEB_WATCH  = 4;
    static const int CURRENT_VERSION        = VERSION_HD_MWEB_WATCH;
    int nVersion;

    CHDChain() { SetNull(); }

    SERIALIZE_METHODS(CHDChain, obj)
    {
        READWRITE(obj.nVersion, obj.nExternalChainCounter, obj.seed_id);
        if (obj.nVersion >= VERSION_HD_CHAIN_SPLIT) {
            READWRITE(obj.nInternalChainCounter);
        }

        if (obj.nVersion >= VERSION_HD_MWEB) {
            READWRITE(obj.nMWEBIndexCounter);
        }

        if (obj.nVersion >= VERSION_HD_MWEB_WATCH) {
            READWRITE(obj.mweb_scan_key);
        }
    }

    void SetNull()
    {
        nVersion = CHDChain::CURRENT_VERSION;
        nExternalChainCounter = 0;
        nInternalChainCounter = 0;
        nMWEBIndexCounter = 0;
        seed_id.SetNull();
        mweb_scan_key = std::nullopt;
    }

    bool operator==(const CHDChain& chain) const
    {
        return seed_id == chain.seed_id;
    }
};

class CKeyMetadata
{
public:
    static const int VERSION_BASIC=1;
    static const int VERSION_WITH_HDDATA=10;
    static const int VERSION_WITH_KEY_ORIGIN = 12;
    static const int VERSION_WITH_MWEB_INDEX = 14; // MW: TODO - Add new version which converts hdKeypath to new format. Also, gracefully handle migration of mweb_index from CKeyMetadata to KeyOriginInfo
    static const int CURRENT_VERSION = VERSION_WITH_MWEB_INDEX;
    int nVersion;
    int64_t nCreateTime; // 0 means unknown
    std::string hdKeypath; //optional HD/bip32 keypath. Still used to determine whether a key is a seed. Also kept for backwards compatibility
    CKeyID hd_seed_id; //id of the HD seed used to derive this key
    KeyOriginInfo key_origin; // Key origin info with path and fingerprint
    bool has_key_origin = false; //!< Whether the key_origin is useful

    CKeyMetadata()
    {
        SetNull();
    }
    explicit CKeyMetadata(int64_t nCreateTime_)
    {
        SetNull();
        nCreateTime = nCreateTime_;
    }

    SERIALIZE_METHODS(CKeyMetadata, obj)
    {
        READWRITE(obj.nVersion, obj.nCreateTime);
        if (obj.nVersion >= VERSION_WITH_HDDATA) {
            READWRITE(obj.hdKeypath, obj.hd_seed_id);
        }
        
        KeyOriginInfo origin_info;
        if (obj.nVersion >= VERSION_WITH_KEY_ORIGIN) {
            SER_WRITE(obj, std::copy(obj.key_origin.fingerprint, obj.key_origin.fingerprint + sizeof(origin_info.fingerprint), origin_info.fingerprint));
            SER_WRITE(obj, origin_info.path = obj.key_origin.path);
            READWRITE(origin_info.fingerprint, origin_info.path, obj.has_key_origin);
            SER_READ(obj, std::copy(origin_info.fingerprint, origin_info.fingerprint + sizeof(origin_info.fingerprint), obj.key_origin.fingerprint));
            SER_READ(obj, obj.key_origin.path = origin_info.path);
        }
        
        if (obj.nVersion >= VERSION_WITH_MWEB_INDEX) {
            SER_WRITE(obj, origin_info.mweb_index = obj.key_origin.mweb_index);
            READWRITE(origin_info.mweb_index);
            SER_READ(obj, obj.key_origin.mweb_index = origin_info.mweb_index);
        }
    }

    void SetNull()
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = 0;
        hdKeypath.clear();
        hd_seed_id.SetNull();
        key_origin.clear();
        has_key_origin = false;
    }
};

/** Access to the wallet database.
 * Opens the database and provides read and write access to it. Each read and write is its own transaction.
 * Multiple operation transactions can be started using TxnBegin() and committed using TxnCommit()
 * Otherwise the transaction will be committed when the object goes out of scope.
 * Optionally (on by default) it will flush to disk on close.
 * Every 1000 writes will automatically trigger a flush to disk.
 */
class WalletBatch
{
private:
    template <typename K, typename T>
    bool WriteIC(const K& key, const T& value, bool fOverwrite = true)
    {
        if (!m_batch->Write(key, value, fOverwrite)) {
            return false;
        }
        m_database.IncrementUpdateCounter();
        if (m_database.nUpdateCounter % 1000 == 0) {
            m_batch->Flush();
        }
        return true;
    }

    template <typename K>
    bool EraseIC(const K& key)
    {
        if (!m_batch->Erase(key)) {
            return false;
        }
        m_database.IncrementUpdateCounter();
        if (m_database.nUpdateCounter % 1000 == 0) {
            m_batch->Flush();
        }
        return true;
    }

public:
    explicit WalletBatch(WalletDatabase &database, bool _fFlushOnClose = true) :
        m_batch(database.MakeBatch(_fFlushOnClose)),
        m_database(database)
    {
    }
    WalletBatch(const WalletBatch&) = delete;
    WalletBatch& operator=(const WalletBatch&) = delete;

    bool WriteName(const std::string& strAddress, const std::string& strName);
    bool EraseName(const std::string& strAddress);

    bool WritePurpose(const std::string& strAddress, const std::string& purpose);
    bool ErasePurpose(const std::string& strAddress);

    bool WriteTx(const CWalletTx& wtx);
    bool EraseTx(uint256 hash);

    bool WriteKeyMetadata(const CKeyMetadata& meta, const CPubKey& pubkey, const bool overwrite);
    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata &keyMeta);
    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta);
    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey);

    bool WriteCScript(const uint160& hash, const CScript& redeemScript);
    bool WriteMWEBCoin(const mw::Coin& coin);

    bool WriteWatchOnly(const CScript &script, const CKeyMetadata &keymeta);
    bool EraseWatchOnly(const CScript &script);

    bool WriteBestBlock(const CBlockLocator& locator);
    bool ReadBestBlock(CBlockLocator& locator);

    bool WriteOrderPosNext(int64_t nOrderPosNext);

    bool ReadPool(int64_t nPool, CKeyPool& keypool);
    bool WritePool(int64_t nPool, const CKeyPool& keypool);
    bool ErasePool(int64_t nPool);

    bool WriteMinVersion(int nVersion);

    bool WriteDescriptorKey(const uint256& desc_id, const CPubKey& pubkey, const CPrivKey& privkey);
    bool WriteCryptedDescriptorKey(const uint256& desc_id, const CPubKey& pubkey, const std::vector<unsigned char>& secret);
    bool WriteDescriptor(const uint256& desc_id, const WalletDescriptor& descriptor);
    bool WriteDescriptorDerivedCache(const CExtPubKey& xpub, const uint256& desc_id, uint32_t key_exp_index, uint32_t der_index);
    bool WriteDescriptorParentCache(const CExtPubKey& xpub, const uint256& desc_id, uint32_t key_exp_index);
    bool WriteDescriptorLastHardenedCache(const CExtPubKey& xpub, const uint256& desc_id, uint32_t key_exp_index);
    bool WriteDescriptorCacheItems(const uint256& desc_id, const DescriptorCache& cache);

    bool WriteLockedUTXO(const GenericOutputID& output);
    bool EraseLockedUTXO(const GenericOutputID& output);

    /// Write destination data key,value tuple to database
    bool WriteDestData(const std::string &address, const std::string &key, const std::string &value);
    /// Erase destination data tuple from wallet database
    bool EraseDestData(const std::string &address, const std::string &key);

    bool WriteActiveScriptPubKeyMan(uint8_t type, const uint256& id, bool internal);
    bool EraseActiveScriptPubKeyMan(uint8_t type, bool internal);

    DBErrors LoadWallet(CWallet* pwallet);
    DBErrors FindWalletTx(std::vector<uint256>& vTxHash, std::list<CWalletTx>& vWtx);
    DBErrors ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut);
    /* Function to determine if a certain KV/key-type is a key (cryptographical key) type */
    static bool IsKeyType(const std::string& strType);

    //! write the hdchain model (external chain child index counter)
    bool WriteHDChain(const CHDChain& chain);

    //! Delete records of the given types
    bool EraseRecords(const std::unordered_set<std::string>& types);

    bool WriteWalletFlags(const uint64_t flags);
    //! Begin a new transaction
    bool TxnBegin();
    //! Commit current transaction
    bool TxnCommit();
    //! Abort current transaction
    bool TxnAbort();
private:
    std::unique_ptr<DatabaseBatch> m_batch;
    WalletDatabase& m_database;
};

//! Compacts BDB state so that wallet.dat is self-contained (if there are changes)
void MaybeCompactWalletDB(WalletContext& context);

//! Callback for filtering key types to deserialize in ReadKeyValue
using KeyFilterFn = std::function<bool(const std::string&)>;

//! Unserialize a given Key-Value pair and load it into the wallet
bool ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue, std::string& strType, std::string& strErr, const KeyFilterFn& filter_fn = nullptr);

/** Return object for accessing dummy database with no read/write capabilities. */
std::unique_ptr<WalletDatabase> CreateDummyWalletDatabase();

/** Return object for accessing temporary in-memory database. */
std::unique_ptr<WalletDatabase> CreateMockWalletDatabase(DatabaseOptions& options);
std::unique_ptr<WalletDatabase> CreateMockWalletDatabase();
} // namespace wallet

#endif // BITCOIN_WALLET_WALLETDB_H
