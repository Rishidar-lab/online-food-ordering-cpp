#ifndef OFOS_SERVER_HPP
#define OFOS_SERVER_HPP

#include "router.hpp"
#include <atomic>
#include <string>

namespace ofos {

// A small blocking HTTP/1.1 server built directly on POSIX sockets.
// One detached thread per connection; each connection serves a single
// request then closes (Connection: close). Adequate and robust for the
// concurrency this project targets.
class HttpServer {
public:
    HttpServer(std::string host, int port, const Router& router);

    // Binds, listens and serves until stop() is called. Returns false if the
    // socket could not be bound.
    bool run();
    void stop();

    size_t max_body_bytes = 8 * 1024 * 1024;   // 8 MB upload cap

private:
    void handle_client(int client_fd);
    static bool read_request(int fd, Request& req, size_t max_body,
                             std::string& error);
    static void parse_request_head(const std::string& head, Request& req);
    static void parse_cookies(Request& req);
    static void parse_body(Request& req);
    static void parse_multipart(Request& req, const std::string& boundary);

    std::string host_;
    int port_;
    const Router& router_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
};

} // namespace ofos

#endif // OFOS_SERVER_HPP
