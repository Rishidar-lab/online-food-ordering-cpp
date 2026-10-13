#include "router.hpp"

namespace ofos {

void Router::add(const std::string& method, const std::string& path, Handler h) {
    routes_[method + " " + path] = std::move(h);
}

void Router::dispatch(const Request& req, Response& res) const {
    auto it = routes_.find(req.method + " " + req.path);
    if (it != routes_.end()) {
        it->second(req, res);
        return;
    }
    // A path that exists for another method -> 405.
    for (const auto& kv : routes_) {
        size_t sp = kv.first.find(' ');
        if (sp != std::string::npos && kv.first.substr(sp + 1) == req.path) {
            res.text("405 Method Not Allowed", 405);
            return;
        }
    }
    if (fallback_) {
        fallback_(req, res);
    } else {
        res.text("404 Not Found", 404);
    }
}

} // namespace ofos
