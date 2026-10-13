#ifndef OFOS_HTTP_HPP
#define OFOS_HTTP_HPP

#include <string>
#include <map>
#include <vector>
#include <utility>

namespace ofos {

// An uploaded file from a multipart/form-data request.
struct UploadedFile {
    std::string field_name;
    std::string filename;
    std::string content_type;
    std::string data;   // raw bytes (may be binary)
    bool empty() const { return filename.empty() && data.empty(); }
};

class Request {
public:
    std::string method;        // GET, POST, ...
    std::string path;          // decoded path, no query string
    std::string raw_path;      // path as received
    std::string query_string;
    std::string http_version;
    std::string body;          // raw request body

    std::map<std::string, std::string> headers;   // keys lower-cased
    std::map<std::string, std::string> query;      // ?a=b parsed
    std::map<std::string, std::string> form;       // POST fields (urlencoded/multipart)
    std::map<std::string, std::string> cookies;
    std::vector<UploadedFile> files;

    std::string header(const std::string& name, const std::string& def = "") const;
    std::string get(const std::string& name, const std::string& def = "") const;   // query
    std::string post(const std::string& name, const std::string& def = "") const;  // form
    // Looks in form first, then query (handy for shared handlers).
    std::string param(const std::string& name, const std::string& def = "") const;
    std::string cookie(const std::string& name, const std::string& def = "") const;
    const UploadedFile* file(const std::string& field) const;
    bool is_ajax() const;   // X-Requested-With: XMLHttpRequest
};

class Response {
public:
    int status = 200;
    std::string status_text = "OK";
    std::string content_type = "text/html; charset=utf-8";
    std::string body;
    std::vector<std::pair<std::string, std::string>> extra_headers;  // allows dup keys

    void set_header(const std::string& name, const std::string& value);
    void set_cookie(const std::string& name, const std::string& value,
                    const std::string& path = "/", int max_age = -1,
                    bool http_only = true);
    void html(const std::string& content, int code = 200);
    void json(const std::string& content, int code = 200);
    void text(const std::string& content, int code = 200);
    void redirect(const std::string& location, int code = 302);

    // Serialise the full HTTP/1.1 response (status line, headers, body).
    std::string serialize() const;

    static std::string reason_phrase(int code);
};

} // namespace ofos

#endif // OFOS_HTTP_HPP
