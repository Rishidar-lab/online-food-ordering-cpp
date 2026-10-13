#ifndef OFOS_DB_HPP
#define OFOS_DB_HPP

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <stdexcept>

struct sqlite3;

namespace ofos {

class DbError : public std::runtime_error {
public:
    explicit DbError(const std::string& msg) : std::runtime_error(msg) {}
};

// Thin, thread-safe wrapper over SQLite with positional (?) parameter binding.
// All values are bound/returned as text; SQLite's column affinity handles the
// numeric columns. A single mutex serialises access, which is ample for the
// concurrency of a demo / coursework deployment.
class Database {
public:
    using Row = std::map<std::string, std::string>;

    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Run one or more statements with no parameters (schema, PRAGMA, etc.).
    void exec(const std::string& sql);

    // SELECT -> rows (column name -> value, NULL becomes "").
    std::vector<Row> query(const std::string& sql,
                           const std::vector<std::string>& params = {});

    // INSERT/UPDATE/DELETE -> number of affected rows.
    int execute(const std::string& sql,
                const std::vector<std::string>& params = {});

    long long last_insert_id();

    // First column of the first row, or def if no rows.
    std::string scalar(const std::string& sql,
                       const std::vector<std::string>& params = {},
                       const std::string& def = "");

    // Run body() inside a BEGIN IMMEDIATE / COMMIT transaction. The connection
    // mutex is held for the whole body (recursive, so nested query/execute
    // calls are safe), giving real isolation on this single-connection DB.
    // On exception the transaction is rolled back and the exception rethrown.
    void transaction(const std::function<void()>& body);

private:
    sqlite3* db_ = nullptr;
    std::recursive_mutex mutex_;
};

} // namespace ofos

#endif // OFOS_DB_HPP
