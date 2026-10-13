#include "db.hpp"
#include <sqlite3.h>

namespace ofos {

Database::Database(const std::string& path) {
    int rc = sqlite3_open_v2(path.c_str(), &db_,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                 SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "cannot open database";
        if (db_) sqlite3_close(db_);
        throw DbError("sqlite open failed: " + msg);
    }
    sqlite3_busy_timeout(db_, 5000);
    // Durability/concurrency tuning appropriate for a web workload.
    char* err = nullptr;
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON;",
                 nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        if (err) sqlite3_free(err);
        throw DbError("sqlite exec failed: " + msg);
    }
}

static sqlite3_stmt* prepare(sqlite3* db, const std::string& sql,
                             const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DbError(std::string("prepare failed: ") + sqlite3_errmsg(db));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(),
                          static_cast<int>(params[i].size()), SQLITE_TRANSIENT);
    }
    return stmt;
}

std::vector<Database::Row> Database::query(const std::string& sql,
                                           const std::vector<std::string>& params) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* stmt = prepare(db_, sql, params);
    std::vector<Row> rows;
    int cols = sqlite3_column_count(stmt);
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Row row;
        for (int c = 0; c < cols; ++c) {
            const char* name = sqlite3_column_name(stmt, c);
            const unsigned char* val = sqlite3_column_text(stmt, c);
            row[name ? name : ""] = val ? reinterpret_cast<const char*>(val) : "";
        }
        rows.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw DbError(std::string("query step failed: ") + sqlite3_errmsg(db_));
    }
    return rows;
}

int Database::execute(const std::string& sql,
                      const std::vector<std::string>& params) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* stmt = prepare(db_, sql, params);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw DbError(std::string("execute failed: ") + sqlite3_errmsg(db_));
    }
    return sqlite3_changes(db_);
}

long long Database::last_insert_id() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return sqlite3_last_insert_rowid(db_);
}

void Database::transaction(const std::function<void()>& body) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    exec("BEGIN IMMEDIATE");
    try {
        body();
        exec("COMMIT");
    } catch (...) {
        try { exec("ROLLBACK"); } catch (...) {}
        throw;
    }
}

std::string Database::scalar(const std::string& sql,
                             const std::vector<std::string>& params,
                             const std::string& def) {
    auto rows = query(sql, params);
    if (rows.empty() || rows[0].empty()) return def;
    return rows[0].begin()->second;
}

} // namespace ofos
