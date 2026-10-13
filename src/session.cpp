#include "session.hpp"
#include "utils.hpp"

namespace ofos {

void SessionStore::gc_locked() {
    std::time_t now = std::time(nullptr);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (now - it->second.last_seen > ttl_) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string SessionStore::create() {
    std::lock_guard<std::mutex> lock(mutex_);
    gc_locked();
    Session s;
    s.id = util::random_hex(32);          // 256-bit token
    s.created = s.last_seen = std::time(nullptr);
    sessions_[s.id] = s;
    return s.id;
}

bool SessionStore::exists(const std::string& sid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sid);
    if (it == sessions_.end()) return false;
    if (std::time(nullptr) - it->second.last_seen > ttl_) {
        sessions_.erase(it);
        return false;
    }
    it->second.last_seen = std::time(nullptr);
    return true;
}

std::string SessionStore::value(const std::string& sid, const std::string& key,
                                const std::string& def) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sid);
    if (it == sessions_.end()) return def;
    auto d = it->second.data.find(key);
    return d == it->second.data.end() ? def : d->second;
}

void SessionStore::set(const std::string& sid, const std::string& key,
                       const std::string& val) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sid);
    if (it == sessions_.end()) return;
    it->second.data[key] = val;
    it->second.last_seen = std::time(nullptr);
}

void SessionStore::erase_key(const std::string& sid, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sid);
    if (it == sessions_.end()) return;
    it->second.data.erase(key);
}

void SessionStore::destroy(const std::string& sid) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(sid);
}

} // namespace ofos
