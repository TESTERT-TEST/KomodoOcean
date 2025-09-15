// addressstatsdb.h
#pragma once

#include <string>
#include <sstream>
#include <stdexcept>
#include <sqlite3.h>
#include <vector>
#include "script/standard.h"
#include "amount.h"

// Structure with optional fields for activity.
// Any field < 0 means "do not update / leave as is".
class CAddressActivity
{
public:
    int64_t last_in_height   = -1;
    int64_t last_in_time     = -1;  // UNIX timestamp
    int64_t last_out_height  = -1;
    int64_t last_out_time    = -1;
    int64_t last_seen_height = -1;
    int64_t last_seen_time   = -1;
};

class CAddressStatsDB
{
public:
    enum class AddressType : int {
        P2PKH  = 1, // 20 bytes (hash160(pubkey))
        P2SH   = 2, // 20 bytes (hash160(redeemScript))
        P2WPKH = 3, // 20 bytes (witness v0 keyhash)
        P2WSH  = 4, // 32 bytes (witness v0 scripthash)
        P2TR   = 5  // 32 bytes (taproot x-only pubkey)
    };

    CAddressStatsDB();
    ~CAddressStatsDB();

    void open_or_create(const std::string& path);

    void open_existing(const std::string& path);

    void close();

    sqlite3* handle() const { return m_db; }

    // Insert or update an address record.
    // - type_id: address type
    // - vch_addr: raw address bytes (20 or 32 depending on type)
    // - balance_delta: added to the stored balance (can be negative)
    // - activity: fields >= 0 will be updated, fields < 0 are ignored
    void upsert_address(AddressType type_id,
                        const std::vector<unsigned char>& vch_addr,
                        CAmount balance_delta,
                        const CAddressActivity& activity);
    
    // Drop all rows from the 'addresses' table inside a transaction.
    void clear_addresses();
    // Drop and recreate the 'addresses' table and its indexes.
    void reset_addresses_table();

private:
    sqlite3* m_db;

    static constexpr int SCHEMA_VERSION = 1;
    static constexpr int MIN_SQLITE_LIB_VERSION_NUMBER = 3046001; // 3.46.1

    void open_impl(const std::string& path, int flags);
    static void ensure_sqlite_version(int min_lib_version_number);

    void set_pragmas();
    void ensure_schema();

    bool table_exists(const std::string& name);
    int  get_user_version();
    void set_user_version(int v);

    void begin_exclusive();
    void commit();
    void exec_sql(const char* sql);
    void sanitize_flags_for_runtime(int& flags);
    static void ensure_sqlite_initialized();
    // Validate address length for a given type_id.
    // Throws std::runtime_error if mismatch.
    static int expected_addr_len(AddressType type_id);

    // Bind INTEGER or NULL depending on value (<0 => NULL).
    static void bind_int64_or_null(sqlite3_stmt* stmt, int idx, int64_t value);
};

class EncodedAddressPayload {
public:
    bool ok;
    CAddressStatsDB::AddressType type; // valid only if ok == true
    std::vector<unsigned char> data;
};

EncodedAddressPayload EncodeDestinationPayload_NoVisitor(const CTxDestination& dest);