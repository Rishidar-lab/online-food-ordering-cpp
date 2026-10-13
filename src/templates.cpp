#include "templates.hpp"
#include "utils.hpp"

#include <stdexcept>

namespace ofos {

std::string TemplateEngine::render(const std::string& name, const Context& ctx) const {
    std::string tpl;
    if (!util::read_file(views_dir_ + "/" + name, tpl)) {
        throw std::runtime_error("template not found: " + name);
    }
    return render_string(tpl, ctx);
}

std::string TemplateEngine::render_string(const std::string& tpl,
                                          const Context& ctx) const {
    std::string out;
    out.reserve(tpl.size() + 256);

    size_t i = 0;
    while (i < tpl.size()) {
        size_t open = tpl.find("{{", i);
        if (open == std::string::npos) {
            out.append(tpl, i, std::string::npos);
            break;
        }
        out.append(tpl, i, open - i);
        size_t close = tpl.find("}}", open);
        if (close == std::string::npos) {   // unterminated -> emit literally
            out.append(tpl, open, std::string::npos);
            break;
        }
        std::string token = util::trim(tpl.substr(open + 2, close - open - 2));
        i = close + 2;

        if (token.empty()) continue;

        if (token[0] == '>') {   // partial include
            std::string pname = util::trim(token.substr(1));
            out += render(pname, ctx);
        } else if (token[0] == '&') {   // raw (unescaped) value
            std::string key = util::trim(token.substr(1));
            auto it = ctx.find(key);
            if (it != ctx.end()) out += it->second;
        } else {                       // escaped value
            auto it = ctx.find(token);
            if (it != ctx.end()) out += util::html_escape(it->second);
        }
    }
    return out;
}

} // namespace ofos
