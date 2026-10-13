#include "http.hpp"
#include "utils.hpp"

#include <sstream>

namespace ofos {

static std::string lookup(const std::map<std::string, std::string>& m,
                          const std::string& k, const std::string& def) {
    auto it = m.find(k);
    return it == m.end() ? def : it->second;
}

std::string Request::header(const std::string& name, const std::string& def) const {
    return lookup(headers, util::to_lower(name), def);
}
std::string Request::get(const std::string& name, const std::string& def) const {
    return lookup(query, name, def);
}
std::string Request::post(const std::string& name, const std::string& def) const {
    return lookup(form, name, def);
}
std::string Request::param(const std::string& name, const std::string& def) const {
    auto it = form.find(name);
    if (it != form.end()) return it->second;
    return lookup(query, name, def);
}
std::string Request::cookie(const std::string& name, const std::string& def) const {
    return lookup(cookies, name, def);
}
const UploadedFile* Request::file(const std::string& field) const {
    for (const auto& f : files) {
        if (f.field_name == field && !f.empty()) return &f;
    }
    return nullptr;
}
bool Request::is_ajax() const {
    return util::to_lower(header("x-requested-with")) == "xmlhttprequest";
}

// ---------------------------------------------------------------------------

void Response::set_header(const std::string& name, const std::string& value) {
    extra_headers.emplace_back(name, value);
}

void Response::set_cookie(const std::string& name, const std::string& value,
                          const std::string& path, int max_age, bool http_only) {
    std::ostringstream c;
    c << name << "=" << value << "; Path=" << path << "; SameSite=Lax";
    if (max_age >= 0) c << "; Max-Age=" << max_age;
    if (http_only) c << "; HttpOnly";
    extra_headers.emplace_back("Set-Cookie", c.str());
}

void Response::html(const std::string& content, int code) {
    status = code;
    status_text = reason_phrase(code);
    content_type = "text/html; charset=utf-8";
    body = content;
}
void Response::json(const std::string& content, int code) {
    status = code;
    status_text = reason_phrase(code);
    content_type = "application/json; charset=utf-8";
    body = content;
}
void Response::text(const std::string& content, int code) {
    status = code;
    status_text = reason_phrase(code);
    content_type = "text/plain; charset=utf-8";
    body = content;
}
void Response::redirect(const std::string& location, int code) {
    status = code;
    status_text = reason_phrase(code);
    set_header("Location", location);
    content_type = "text/html; charset=utf-8";
    body = "<html><body>Redirecting to <a href=\"" + util::html_escape(location) +
           "\">" + util::html_escape(location) + "</a></body></html>";
}

std::string Response::serialize() const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << " " << status_text << "\r\n";
    out << "Content-Type: " << content_type << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n";
    out << "X-Content-Type-Options: nosniff\r\n";
    for (const auto& h : extra_headers) {
        out << h.first << ": " << h.second << "\r\n";
    }
    out << "\r\n";
    out << body;
    return out.str();
}

std::string Response::reason_phrase(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default:  return "OK";
    }
}

} // namespace ofos
