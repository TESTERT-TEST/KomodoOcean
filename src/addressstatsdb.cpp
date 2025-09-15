// addressstatsdb.cpp
#include "addressstatsdb.h"
#include <boost/variant.hpp>
#include "pubkey.h"

EncodedAddressPayload EncodeDestinationPayload_NoVisitor(const CTxDestination& dest)
{
    EncodedAddressPayload r;

    if (const CKeyID* id = boost::get<CKeyID>(&dest)) {
        r.ok   = true;
        r.type = CAddressStatsDB::AddressType::P2PKH;
        r.data.assign(id->begin(), id->end());
        return r;
    }

    if (const CPubKey* pk = boost::get<CPubKey>(&dest)) {
        const CKeyID id = pk->GetID(); // hash160(pubkey)
        r.ok   = true;
        r.type = CAddressStatsDB::AddressType::P2PKH;
        r.data.assign(id.begin(), id.end());
        return r;
    }

    if (const CScriptID* id = boost::get<CScriptID>(&dest)) {
        r.ok   = true;
        r.type = CAddressStatsDB::AddressType::P2SH;
        r.data.assign(id->begin(), id->end());
        return r;
    }

    // CNoDestination or unknown type
    r.ok = false;
    return r;
}

CAddressStatsDB::CAddressStatsDB() : m_db(nullptr) {}
CAddressStatsDB::~CAddressStatsDB() { close(); }

void CAddressStatsDB::open_or_create(const std::string& path)
{
    ensure_sqlite_initialized(); // important! SQLITE_OMIT_AUTOINIT
    ensure_sqlite_version(MIN_SQLITE_LIB_VERSION_NUMBER);

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    sanitize_flags_for_runtime(flags);

    open_impl(path, flags);

    set_pragmas();
    begin_exclusive();
    ensure_schema();
    commit();
}

void CAddressStatsDB::open_existing(const std::string& path)
{
    ensure_sqlite_initialized();
    ensure_sqlite_version(MIN_SQLITE_LIB_VERSION_NUMBER);

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX;
    sanitize_flags_for_runtime(flags);

    open_impl(path, flags);

    set_pragmas();

    if (!table_exists("addresses")) {
        throw std::runtime_error("addressstatsdb: required table 'addresses' is missing");
    }
    const int uv = get_user_version();
    if (uv != SCHEMA_VERSION) {
        throw std::runtime_error("addressstatsdb: unexpected schema version: " + std::to_string(uv));
    }
}

void CAddressStatsDB::ensure_sqlite_initialized()
{
    const int rc = sqlite3_initialize();
    if (rc != SQLITE_OK) {
        throw std::runtime_error("sqlite3_initialize failed: rc=" + std::to_string(rc));
    }
}

void CAddressStatsDB::sanitize_flags_for_runtime(int& flags)
{
    const int ts = sqlite3_threadsafe(); // 0 - no mutexes; 1/2 - mutexes enabled
    if (ts == 0) {
        flags &= ~SQLITE_OPEN_FULLMUTEX;
        flags |= SQLITE_OPEN_NOMUTEX;
    }
}

void CAddressStatsDB::close()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

void CAddressStatsDB::open_impl(const std::string& path, int flags)
{
    if (m_db) return;
    int rc = sqlite3_open_v2(path.c_str(), &m_db, flags, nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = m_db ? sqlite3_errmsg(m_db) : "unknown";
        if (m_db) sqlite3_close(m_db);
        m_db = nullptr;
        throw std::runtime_error("sqlite open failed: " + msg);
    }
}

void CAddressStatsDB::ensure_sqlite_version(int min_lib_version_number)
{
    auto fmt = [](int ver) -> std::string {
        const int major = ver / 1000000;
        const int minor = (ver / 1000) % 1000;
        const int patch = ver % 1000;
        std::ostringstream ss;
        ss << major << '.' << minor << '.' << patch;
        return ss.str();
    };

    const int runtime_ver = sqlite3_libversion_number();
    if (runtime_ver < min_lib_version_number) {
        std::ostringstream oss;
        oss << "SQLite runtime too old: " << fmt(runtime_ver)
            << " (< " << fmt(min_lib_version_number) << ")";
        std::string error_message = oss.str();
        throw std::runtime_error(error_message);
    }
}

void CAddressStatsDB::set_pragmas()
{
    exec_sql("PRAGMA foreign_keys = OFF;");
    exec_sql("PRAGMA journal_mode = WAL;");
    exec_sql("PRAGMA synchronous = NORMAL;");
    exec_sql("PRAGMA temp_store = MEMORY;");
    exec_sql("PRAGMA cache_size = -100000;"); // ~100 MiB page cache
}

void CAddressStatsDB::ensure_schema()
{
    if (get_user_version() != 0) return;

    exec_sql(R"SQL(
CREATE TABLE IF NOT EXISTS addresses (
  type_id           INTEGER NOT NULL,
  addr              BLOB    NOT NULL,
  balance_sat       INTEGER NOT NULL DEFAULT 0
                       CHECK (balance_sat >= 0 AND balance_sat <= 9223372036854775807),
  last_in_height    INTEGER,
  last_in_time      INTEGER,
  last_out_height   INTEGER,
  last_out_time     INTEGER,
  last_seen_height  INTEGER,
  last_seen_time    INTEGER,
  addr_len          INTEGER GENERATED ALWAYS AS (length(addr)) STORED,
  PRIMARY KEY (type_id, addr)
) WITHOUT ROWID;
)SQL");

    exec_sql(R"SQL(
CREATE INDEX IF NOT EXISTS idx_addresses_balance_nonzero
  ON addresses(balance_sat) WHERE balance_sat > 0;
)SQL");

    exec_sql(R"SQL(
CREATE INDEX IF NOT EXISTS idx_addresses_last_seen_time
  ON addresses(last_seen_time);
)SQL");

    set_user_version(SCHEMA_VERSION);
}

bool CAddressStatsDB::table_exists(const std::string& name)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("addressstatsdb: prepare failed in table_exists");
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    const bool ok = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return ok;
}

int CAddressStatsDB::get_user_version()
{
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "PRAGMA user_version;", -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("addressstatsdb: prepare failed in get_user_version");
    }
    const int rc = sqlite3_step(stmt);
    const int ver = (rc == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : 0;
    sqlite3_finalize(stmt);
    return ver;
}

void CAddressStatsDB::set_user_version(int v)
{
    exec_sql(("PRAGMA user_version = " + std::to_string(v) + ";").c_str());
}

void CAddressStatsDB::begin_exclusive() { exec_sql("BEGIN EXCLUSIVE;"); }
void CAddressStatsDB::commit()          { exec_sql("COMMIT;"); }

void CAddressStatsDB::exec_sql(const char* sql)
{
    char* err = nullptr;
    const int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error(std::string("sqlite exec failed: ") + msg);
    }
}

int CAddressStatsDB::expected_addr_len(AddressType type_id)
{
    switch (type_id) {
        case AddressType::P2PKH:
        case AddressType::P2SH:
        case AddressType::P2WPKH:
            return 20;
        case AddressType::P2WSH:
        case AddressType::P2TR:
            return 32;
        default:
            throw std::runtime_error("addressstatsdb: unknown AddressType");
    }
}

void CAddressStatsDB::bind_int64_or_null(sqlite3_stmt* stmt, int idx, int64_t value)
{
    if (value < 0) {
        sqlite3_bind_null(stmt, idx);
    } else {
        sqlite3_bind_int64(stmt, idx, static_cast<sqlite3_int64>(value));
    }
}

void CAddressStatsDB::upsert_address(AddressType type_id,
                                     const std::vector<unsigned char>& vch_addr,
                                     CAmount balance_delta,
                                     const CAddressActivity& activity)
{
    if (!m_db) {
        throw std::runtime_error("addressstatsdb: DB not open");
    }

    // Validate address length (20 or 32 bytes depending on type).
    const int need_len = expected_addr_len(type_id);
    if (static_cast<int>(vch_addr.size()) != need_len) {
        std::ostringstream oss;
        oss << "addressstatsdb: invalid addr length " << vch_addr.size()
            << " for type_id=" << static_cast<int>(type_id)
            << " (expected " << need_len << ")";
        throw std::runtime_error(oss.str());
    }

    // UPSERT statement:
    // - adds balance_delta to existing balance
    // - updates activity fields only if provided (>=0), otherwise keeps old values
    static const char* kSql =
        "INSERT INTO addresses ("
        "  type_id, addr, balance_sat,"
        "  last_in_height, last_in_time,"
        "  last_out_height, last_out_time,"
        "  last_seen_height, last_seen_time"
        ") VALUES ("
        "  ?, ?, ?, ?, ?, ?, ?, ?, ?"
        ") ON CONFLICT(type_id, addr) DO UPDATE SET "
        "  balance_sat       = balance_sat + excluded.balance_sat, "
        "  last_in_height    = COALESCE(excluded.last_in_height,    last_in_height), "
        "  last_in_time      = COALESCE(excluded.last_in_time,      last_in_time), "
        "  last_out_height   = COALESCE(excluded.last_out_height,   last_out_height), "
        "  last_out_time     = COALESCE(excluded.last_out_time,     last_out_time), "
        "  last_seen_height  = COALESCE(excluded.last_seen_height,  last_seen_height), "
        "  last_seen_time    = COALESCE(excluded.last_seen_time,    last_seen_time);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("addressstatsdb: prepare failed: ") +
                                 sqlite3_errmsg(m_db));
    }

    int idx = 1;
    sqlite3_bind_int(stmt, idx++, static_cast<int>(type_id));
    sqlite3_bind_blob(stmt, idx++, vch_addr.data(),
                      static_cast<int>(vch_addr.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, idx++, static_cast<sqlite3_int64>(balance_delta));

    bind_int64_or_null(stmt, idx++, activity.last_in_height);
    bind_int64_or_null(stmt, idx++, activity.last_in_time);
    bind_int64_or_null(stmt, idx++, activity.last_out_height);
    bind_int64_or_null(stmt, idx++, activity.last_out_time);
    bind_int64_or_null(stmt, idx++, activity.last_seen_height);
    bind_int64_or_null(stmt, idx++, activity.last_seen_time);

    int rc = sqlite3_step(stmt);
    const bool ok = (rc == SQLITE_DONE);
    const char* errmsg = sqlite3_errmsg(m_db);
    sqlite3_finalize(stmt);

    if (!ok) {
        // Example: CHECK constraint violation if balance would go negative.
        throw std::runtime_error(std::string("addressstatsdb: upsert failed: ") +
                                 (errmsg ? errmsg : "unknown"));
    }
}

void CAddressStatsDB::clear_addresses()
{
    if (!m_db) throw std::runtime_error("addressstatsdb: DB not open");
    exec_sql("BEGIN IMMEDIATE;");         // lock for bulk delete
    exec_sql("DELETE FROM addresses;");   // fastest available "truncate" in SQLite
    exec_sql("COMMIT;");
    // Optional: reclaim disk space if you care about file size, slower:
    // exec_sql("VACUUM;");
}

void CAddressStatsDB::reset_addresses_table()
{
    if (!m_db) throw std::runtime_error("addressstatsdb: DB not open");
    exec_sql("BEGIN EXCLUSIVE;");
    exec_sql("DROP TABLE IF EXISTS addresses;");
    // Recreate exactly the same schema and indexes:
    ensure_schema(); // this will create addresses + indexes and set user_version if needed
    exec_sql("COMMIT;");
}