#ifndef OFOS_TEMPLATES_HPP
#define OFOS_TEMPLATES_HPP

#include <map>
#include <string>

namespace ofos {

using Context = std::map<std::string, std::string>;

// Tiny file-backed template engine.
//   {{ key }}   -> HTML-escaped value of key (safe for user data)
//   {{& key }}  -> raw value of key (for pre-built HTML fragments)
//   {{> name }} -> include the partial views/name.html (recursively rendered)
// Missing keys render as empty. Templates are read from disk on every call so
// they can be edited without restarting the server.
class TemplateEngine {
public:
    explicit TemplateEngine(std::string views_dir)
        : views_dir_(std::move(views_dir)) {}

    std::string render(const std::string& name, const Context& ctx) const;
    std::string render_string(const std::string& tpl, const Context& ctx) const;

private:
    std::string views_dir_;
};

} // namespace ofos

#endif // OFOS_TEMPLATES_HPP
