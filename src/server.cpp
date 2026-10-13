#include "server.hpp"
#include "utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

namespace ofos {

HttpServer::HttpServer(std::string host, int port, const Router& router)
    : host_(std::move(host)), port_(port), router_(router) {}

void HttpServer::stop() {
    running_ = false;
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
}

bool HttpServer::run() {
    // Writing to a peer that closed the connection must not kill the process.
    std::signal(SIGPIPE, SIG_IGN);

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::perror("socket");
        return false;
    }
    int yes = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (host_ == "0.0.0.0" || host_.empty()) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        return false;
    }
    if (::listen(listen_fd_, 128) < 0) {
        std::perror("listen");
        return false;
    }

    running_ = true;
    while (running_) {
        sockaddr_in peer{};
        socklen_t plen = sizeof(peer);
        int client = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&peer), &plen);
        if (client < 0) {
            if (!running_) break;
            continue;
        }
        std::thread([this, client] { handle_client(client); }).detach();
    }
    return true;
}

void HttpServer::handle_client(int client_fd) {
    // Avoid hanging forever on a half-open client.
    timeval tv{};
    tv.tv_sec = 15;
    ::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    Request req;
    std::string error;
    Response res;

    if (!read_request(client_fd, req, max_body_bytes, error)) {
        if (error == "too_large") {
            res.text("413 Payload Too Large", 413);
        } else {
            res.text("400 Bad Request", 400);
        }
    } else {
        try {
            router_.dispatch(req, res);
        } catch (const std::exception& e) {
            std::cerr << "[error] " << req.method << " " << req.path << " : "
                      << e.what() << "\n";
            res.html("<h1>500 Internal Server Error</h1><p>" +
                         util::html_escape(e.what()) + "</p>",
                     500);
        }
    }

    std::string out = res.serialize();
    size_t sent = 0;
    while (sent < out.size()) {
        ssize_t n = ::send(client_fd, out.data() + sent, out.size() - sent, 0);
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
    ::close(client_fd);
}

bool HttpServer::read_request(int fd, Request& req, size_t max_body,
                              std::string& error) {
    std::string buffer;
    char chunk[8192];
    size_t header_end = std::string::npos;

    // 1) Read until the end of the header block.
    while (true) {
        ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) { error = "read"; return false; }
        buffer.append(chunk, static_cast<size_t>(n));
        header_end = buffer.find("\r\n\r\n");
        if (header_end != std::string::npos) break;
        if (buffer.size() > 64 * 1024) { error = "headers_too_large"; return false; }
    }

    std::string head = buffer.substr(0, header_end);
    std::string rest = buffer.substr(header_end + 4);
    parse_request_head(head, req);

    // 2) Read the body, if any, up to Content-Length.
    size_t content_length = 0;
    auto cl = req.headers.find("content-length");
    if (cl != req.headers.end()) {
        content_length = static_cast<size_t>(util::to_long(cl->second, 0));
    }
    if (content_length > max_body) { error = "too_large"; return false; }

    req.body = rest;
    while (req.body.size() < content_length) {
        ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        req.body.append(chunk, static_cast<size_t>(n));
    }
    if (req.body.size() > content_length) req.body.resize(content_length);

    parse_cookies(req);
    parse_body(req);
    return true;
}

void HttpServer::parse_request_head(const std::string& head, Request& req) {
    std::istringstream iss(head);
    std::string line;
    std::getline(iss, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();

    // Request line: METHOD SP target SP version
    {
        std::istringstream ls(line);
        std::string target;
        ls >> req.method >> target >> req.http_version;
        req.raw_path = target;
        size_t q = target.find('?');
        if (q != std::string::npos) {
            req.query_string = target.substr(q + 1);
            target = target.substr(0, q);
        }
        req.path = util::url_decode(target);
        if (req.path.size() > 1 && req.path.back() == '/') req.path.pop_back();
        if (req.path.empty()) req.path = "/";
        req.query = util::parse_urlencoded(req.query_string);
    }

    // Header lines.
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = util::to_lower(util::trim(line.substr(0, colon)));
        std::string value = util::trim(line.substr(colon + 1));
        req.headers[name] = value;
    }
}

void HttpServer::parse_cookies(Request& req) {
    auto it = req.headers.find("cookie");
    if (it == req.headers.end()) return;
    for (auto& part : util::split(it->second, ';')) {
        std::string p = util::trim(part);
        size_t eq = p.find('=');
        if (eq == std::string::npos) continue;
        req.cookies[util::trim(p.substr(0, eq))] = util::trim(p.substr(eq + 1));
    }
}

void HttpServer::parse_body(Request& req) {
    if (req.method != "POST" && req.method != "PUT") return;
    std::string ctype = util::to_lower(req.header("content-type"));
    if (util::starts_with(ctype, "application/x-www-form-urlencoded")) {
        req.form = util::parse_urlencoded(req.body);
    } else if (util::starts_with(ctype, "multipart/form-data")) {
        std::string marker = "boundary=";
        size_t b = ctype.find(marker);
        if (b != std::string::npos) {
            // Use the original-case header to extract the boundary value.
            std::string orig = req.header("content-type");
            size_t ob = util::to_lower(orig).find(marker);
            std::string boundary = util::trim(orig.substr(ob + marker.size()));
            if (!boundary.empty() && boundary.front() == '"' && boundary.back() == '"') {
                boundary = boundary.substr(1, boundary.size() - 2);
            }
            parse_multipart(req, boundary);
        }
    }
}

void HttpServer::parse_multipart(Request& req, const std::string& boundary) {
    const std::string delim = "--" + boundary;
    const std::string& body = req.body;
    size_t pos = body.find(delim);
    if (pos == std::string::npos) return;
    pos += delim.size();

    while (pos < body.size()) {
        // After a boundary: either "--" (end) or CRLF then a part.
        if (pos + 2 <= body.size() && body.compare(pos, 2, "--") == 0) break;
        if (pos + 2 <= body.size() && body.compare(pos, 2, "\r\n") == 0) pos += 2;

        size_t head_end = body.find("\r\n\r\n", pos);
        if (head_end == std::string::npos) break;
        std::string part_head = body.substr(pos, head_end - pos);
        size_t content_start = head_end + 4;

        size_t next = body.find("\r\n" + delim, content_start);
        if (next == std::string::npos) break;
        std::string content = body.substr(content_start, next - content_start);

        // Parse this part's headers.
        std::string name, filename, part_ctype;
        for (auto& hl : util::split(part_head, '\n')) {
            std::string h = hl;
            if (!h.empty() && h.back() == '\r') h.pop_back();
            std::string lower = util::to_lower(h);
            if (util::starts_with(lower, "content-disposition:")) {
                size_t np = h.find("name=\"");
                if (np != std::string::npos) {
                    np += 6;
                    size_t ne = h.find('"', np);
                    if (ne != std::string::npos) name = h.substr(np, ne - np);
                }
                size_t fp = h.find("filename=\"");
                if (fp != std::string::npos) {
                    fp += 10;
                    size_t fe = h.find('"', fp);
                    if (fe != std::string::npos) filename = h.substr(fp, fe - fp);
                }
            } else if (util::starts_with(lower, "content-type:")) {
                part_ctype = util::trim(h.substr(h.find(':') + 1));
            }
        }

        if (!filename.empty()) {
            UploadedFile f;
            f.field_name = name;
            f.filename = filename;
            f.content_type = part_ctype;
            f.data = content;
            req.files.push_back(std::move(f));
        } else if (!name.empty()) {
            req.form[name] = content;
        }

        pos = next + 2 + delim.size();   // move past CRLF + boundary
    }
}

} // namespace ofos
