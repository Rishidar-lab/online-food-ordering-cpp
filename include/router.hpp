#ifndef OFOS_ROUTER_HPP
#define OFOS_ROUTER_HPP

#include "http.hpp"
#include <functional>
#include <map>
#include <string>

namespace ofos {

using Handler = std::function<void(const Request&, Response&)>;

// Exact-path router keyed by "METHOD /path". Dynamic data is carried in the
// query string (e.g. /product?id=5), mirroring the classic PHP page model.
class Router {
public:
    void get(const std::string& path, Handler h)  { add("GET", path, std::move(h)); }
    void post(const std::string& path, Handler h) { add("POST", path, std::move(h)); }
    void add(const std::string& method, const std::string& path, Handler h);

    // Fallback runs when no exact route matches (static files + 404).
    void set_fallback(Handler h) { fallback_ = std::move(h); }

    void dispatch(const Request& req, Response& res) const;

private:
    std::map<std::string, Handler> routes_;
    Handler fallback_;
};

} // namespace ofos

#endif // OFOS_ROUTER_HPP
