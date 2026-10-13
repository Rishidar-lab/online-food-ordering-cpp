#include "app.hpp"
#include "json.hpp"
#include "utils.hpp"

#include <sstream>

namespace ofos {

static std::string img_url(const std::string& image) {
    if (image.empty()) return "/public/img/placeholder.svg";
    if (util::starts_with(image, "http")) return image;
    return "/public/" + image;
}

// Build one pictorial product card.
static std::string build_card(const Database::Row& p, const std::string& currency) {
    auto get = [&](const char* k) {
        auto it = p.find(k);
        return it == p.end() ? std::string() : it->second;
    };
    std::string id = get("id");
    std::ostringstream c;
    c << "<article class=\"card\">"
      << "<div class=\"card-media\"><img loading=\"lazy\" src=\""
      << util::html_escape(img_url(get("image"))) << "\" alt=\""
      << util::html_escape(get("name")) << "\"></div>"
      << "<div class=\"card-body\">"
      << "<span class=\"card-cat\">" << util::html_escape(get("category_name")) << "</span>"
      << "<h3 class=\"card-title\">" << util::html_escape(get("name")) << "</h3>"
      << "<p class=\"card-desc\">" << util::html_escape(get("description").substr(0,
             get("description").size() > 70 ? 70 : get("description").size()))
      << (get("description").size() > 70 ? "&hellip;" : "") << "</p>"
      << "<div class=\"card-foot\">"
      << "<span class=\"price\">" << util::html_escape(currency) << " "
      << util::money(util::to_double(get("price"))) << "</span>"
      << "<div class=\"card-actions\">"
      << "<button type=\"button\" class=\"btn btn-ghost\" onclick=\"viewProduct("
      << id << ")\">Details</button>"
      << "<button type=\"button\" class=\"btn btn-primary\" "
      << "onclick=\"addToCart(" << id << ", this)\">Add</button>"
      << "</div></div></div></article>";
    return c.str();
}

void App::h_home(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    Context s = site_context();

    // Up to 6 available products as "popular picks".
    auto prods = db_->query(
        "SELECT p.*, c.name AS category_name FROM products p "
        "JOIN categories c ON c.id=p.category_id "
        "WHERE p.status=1 AND c.status=1 ORDER BY p.id DESC LIMIT 6");
    std::string cards;
    for (auto& p : prods) cards += build_card(p, s["currency"]);

    // Category tiles.
    auto cats = db_->query("SELECT * FROM categories WHERE status=1 ORDER BY name");
    std::string cat_html;
    for (auto& cat : cats) {
        cat_html += "<a class=\"cat-tile\" href=\"/menu?category=" + cat["id"] +
                    "\">" + util::html_escape(cat["name"]) + "</a>";
    }

    Context c = s;
    c["cover_url"] = img_url(s["cover_image"]);
    c["featured"] = cards;
    c["categories"] = cat_html;
    std::string content = tpl_->render("home.html", c);
    render_layout(res, s["system_name"], content, sid, "home");
}

void App::h_menu(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    Context s = site_context();

    long category = util::to_long(req.get("category"), 0);
    std::string search = util::trim(req.get("q"));
    long page = util::to_long(req.get("page"), 1);
    if (page < 1) page = 1;
    int per = cfg_.per_page;

    // Build WHERE clause + params safely (parameterised).
    std::string where = "WHERE p.status=1 AND c.status=1";
    std::vector<std::string> params;
    if (category > 0) {
        where += " AND p.category_id=?";
        params.push_back(std::to_string(category));
    }
    if (!search.empty()) {
        where += " AND (p.name LIKE ? OR p.description LIKE ?)";
        params.push_back("%" + search + "%");
        params.push_back("%" + search + "%");
    }

    long total = util::to_long(
        db_->scalar("SELECT COUNT(*) FROM products p JOIN categories c "
                    "ON c.id=p.category_id " + where, params, "0"), 0);
    long pages = (total + per - 1) / per;
    if (pages < 1) pages = 1;
    if (page > pages) page = pages;
    long offset = (page - 1) * per;

    auto pparams = params;
    pparams.push_back(std::to_string(per));
    pparams.push_back(std::to_string(offset));
    auto prods = db_->query(
        "SELECT p.*, c.name AS category_name FROM products p "
        "JOIN categories c ON c.id=p.category_id " + where +
            " ORDER BY p.id DESC LIMIT ? OFFSET ?",
        pparams);

    std::string cards;
    for (auto& p : prods) cards += build_card(p, s["currency"]);
    if (prods.empty())
        cards = "<p class=\"empty\">No items match your search.</p>";

    // Category filter chips.
    auto cats = db_->query("SELECT * FROM categories WHERE status=1 ORDER BY name");
    std::ostringstream chips;
    chips << "<a class=\"chip" << (category == 0 ? " chip-on" : "")
          << "\" href=\"/menu\">All</a>";
    for (auto& cat : cats) {
        bool on = (category == util::to_long(cat["id"]));
        chips << "<a class=\"chip" << (on ? " chip-on" : "") << "\" href=\"/menu?category="
              << cat["id"] << "\">" << util::html_escape(cat["name"]) << "</a>";
    }

    // Pagination controls preserving filters.
    auto qbase = [&](long pg) {
        std::ostringstream u;
        u << "/menu?page=" << pg;
        if (category > 0) u << "&category=" << category;
        if (!search.empty()) u << "&q=" << util::url_encode(search);
        return u.str();
    };
    std::ostringstream pager;
    if (pages > 1) {
        pager << "<nav class=\"pager\">";
        if (page > 1)
            pager << "<a class=\"page-btn\" href=\"" << qbase(page - 1) << "\">&laquo; Prev</a>";
        for (long i = 1; i <= pages; ++i) {
            pager << "<a class=\"page-btn" << (i == page ? " page-on" : "")
                  << "\" href=\"" << qbase(i) << "\">" << i << "</a>";
        }
        if (page < pages)
            pager << "<a class=\"page-btn\" href=\"" << qbase(page + 1) << "\">Next &raquo;</a>";
        pager << "</nav>";
    }

    Context c = s;
    c["cards"] = cards;
    c["chips"] = chips.str();
    c["pager"] = pager.str();
    c["search"] = search;
    c["result_count"] = std::to_string(total);
    std::string content = tpl_->render("menu.html", c);
    render_layout(res, "Menu", content, sid, "menu");
}

// AJAX: product details for the pop-up panel.
void App::h_product_json(const Request& req, Response& res) {
    long id = util::to_long(req.get("id"), 0);
    auto rows = db_->query(
        "SELECT p.*, c.name AS category_name FROM products p "
        "JOIN categories c ON c.id=p.category_id WHERE p.id=? AND p.status=1",
        {std::to_string(id)});
    if (rows.empty()) {
        res.json(Json::error("Product not found"), 404);
        return;
    }
    auto& p = rows[0];
    Context s = site_context();
    Json j;
    j.boolean("success", true)
        .integer("id", util::to_long(p["id"]))
        .str("name", p["name"])
        .str("description", p["description"])
        .str("category", p["category_name"])
        .str("image", img_url(p["image"]))
        .str("currency", s["currency"])
        .num("price", util::to_double(p["price"]));
    res.json(j.dump());
}

void App::h_about(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    Context s = site_context();
    Context c = s;
    c["cover_url"] = img_url(s["cover_image"]);
    std::string content = tpl_->render("about.html", c);
    render_layout(res, "About Us", content, sid, "about");
}

} // namespace ofos
