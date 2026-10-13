#ifndef OFOS_SESSION_HPP
#define OFOS_SESSION_HPP

#include <ctime>
#include <map>
#include <mutex>
#include <string>

namespace ofos {

struct Session {
    std::string id;
    std::map<std::string, std::string> data;
    std::time_t created = 0;
    std::time_t last_seen = 0;
};

// Server-side session store keyed by a random cookie token. Idle sessions
// older than the TTL are garbage-collected lazily on access.
class SessionStore {
public:
    explicit SessionStore(int ttl_seconds = 3600) : ttl_(ttl_seconds) {}

    std::string create();                                   // -> new session id
    bool exists(const std::string& sid);
    std::string value(const std::string& sid, const std::string& key,
                      const std::string& def = "");
    void set(const std::string& sid, const std::string& key,
             const std::string& val);
    void erase_key(const std::string& sid, const std::string& key);
    void destroy(const std::string& sid);

private:
    void gc_locked();
    std::map<std::string, Session> sessions_;
    std::mutex mutex_;
    int ttl_;
};

} // namespace ofos

#endif // OFOS_SESSION_HPP
