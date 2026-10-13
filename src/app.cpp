#include "app.hpp"
#include "json.hpp"
#include "utils.hpp"

#include <iostream>

namespace ofos {

App::App(AppConfig config) : cfg_(std::move(config)) {
    db_ = std::make_unique<Database>(cfg_.db_path);
    tpl_ = std::make_unique<TemplateEngine>(cfg_.base_dir + "/views");
}

// ---------------------------------------------------------------------------
// Initialisation: schema + first-run seed data.
// ---------------------------------------------------------------------------
void App::init() {
    std::string schema;
    if (!util::read_file(cfg_.base_dir + "/data/schema.sql", schema)) {
        throw std::runtime_error("cannot read data/schema.sql");
    }
    db_->exec(schema);

    // Settings singleton.
    if (db_->scalar("SELECT COUNT(*) FROM settings") == "0") {
        db_->execute(
            "INSERT INTO settings(id,system_name,contact_email,contact_phone,"
            "address,cover_image,about_us,currency) VALUES "
            "(1,?,?,?,?,?,?,?)",
            {"Tasty Bites Online", "support@tastybites.example", "+91 90000 00000",
             "12 MG Road, Bengaluru 560001",
             "img/cover.svg",
             "Tasty Bites is a neighbourhood kitchen serving fresh, home-style "
             "meals. Browse the menu, add your favourites to the cart and order "
             "online in a few clicks.",
             "Rs."});
    }

    // Default administrator account.
    if (db_->scalar("SELECT COUNT(*) FROM users") == "0") {
        std::string salt = util::random_hex(16);
        std::string hash = util::hash_password("admin123", salt);
        db_->execute(
            "INSERT INTO users(username,password_hash,salt,full_name,role,"
            "created_at) VALUES (?,?,?,?,?,?)",
            {"admin", hash, salt, "Store Administrator", "admin", util::now_utc()});
        std::cerr << "[seed] created admin account  username: admin  password: admin123\n";
    }

    // Sample categories + products.
    if (db_->scalar("SELECT COUNT(*) FROM categories") == "0") {
        struct Cat { const char* name; const char* desc; };
        Cat cats[] = {
            {"Burgers", "Flame-grilled patties in soft buns"},
            {"Pizza", "Wood-fired, hand-stretched bases"},
            {"Beverages", "Chilled drinks and fresh juices"},
            {"Desserts", "Sweet endings to your meal"},
        };
        for (auto& c : cats) {
            db_->execute("INSERT INTO categories(name,description,status,created_at)"
                         " VALUES (?,?,1,?)",
                         {c.name, c.desc, util::now_utc()});
        }

        struct Prod { int cat; const char* name; const char* desc; double price; const char* img; };
        Prod prods[] = {
            {1, "Classic Veg Burger", "Crisp veg patty, lettuce, tomato and house mayo.", 120, "img/burger.svg"},
            {1, "Double Cheese Burger", "Two cheese slices over a juicy grilled patty.", 180, "img/burger.svg"},
            {1, "Paneer Tikka Burger", "Spiced paneer, mint chutney and onions.", 160, "img/burger.svg"},
            {2, "Margherita Pizza", "Tomato, mozzarella and fresh basil.", 250, "img/pizza.svg"},
            {2, "Farmhouse Pizza", "Onion, capsicum, mushroom and corn.", 320, "img/pizza.svg"},
            {2, "Peppy Paneer Pizza", "Paneer, red paprika and crisp capsicum.", 340, "img/pizza.svg"},
            {3, "Fresh Lime Soda", "Sweet or salted, served chilled.", 60, "img/drink.svg"},
            {3, "Cold Coffee", "Rich cold coffee topped with cream.", 110, "img/drink.svg"},
            {3, "Masala Chaas", "Spiced buttermilk with roasted cumin.", 50, "img/drink.svg"},
            {4, "Chocolate Brownie", "Warm fudgy brownie with a molten centre.", 130, "img/dessert.svg"},
            {4, "Gulab Jamun (2 pcs)", "Soft milk dumplings in cardamom syrup.", 80, "img/dessert.svg"},
            {4, "Vanilla Ice Cream", "Two scoops of classic vanilla.", 90, "img/dessert.svg"},
        };
        for (auto& p : prods) {
            db_->execute(
                "INSERT INTO products(category_id,name,description,price,image,"
                "status,created_at) VALUES (?,?,?,?,?,1,?)",
                {std::to_string(p.cat), p.name, p.desc, util::money(p.price),
                 p.img, util::now_utc()});
        }
        std::cerr << "[seed] inserted sample menu (" << sizeof(prods) / sizeof(prods[0])
                  << " products)\n";
    }

    register_routes();
}

// ---------------------------------------------------------------------------
// Route table.
// ---------------------------------------------------------------------------
void App::register_routes() {
    auto bind = [this](void (App::*fn)(const Request&, Response&)) {
        return [this, fn](const Request& req, Response& res) { (this->*fn)(req, res); };
    };

    // Health check (used by hosting platforms' probes).
    router_.get("/healthz", [](const Request&, Response& res) { res.text("ok"); });

    // Storefront
    router_.get("/",               bind(&App::h_home));
    router_.get("/menu",           bind(&App::h_menu));
    router_.get("/product",        bind(&App::h_product_json));   // AJAX JSON
    router_.get("/about",          bind(&App::h_about));

    // Auth (customer)
    router_.get("/register",       bind(&App::h_register_form));
    router_.post("/register",      bind(&App::h_register_submit));
    router_.get("/login",          bind(&App::h_login_form));
    router_.post("/login",         bind(&App::h_login_submit));
    router_.get("/logout",         bind(&App::h_logout));

    // Cart + checkout
    router_.get("/cart",           bind(&App::h_cart_view));
    router_.post("/cart/add",      bind(&App::h_cart_add));
    router_.post("/cart/update",   bind(&App::h_cart_update));
    router_.post("/cart/remove",   bind(&App::h_cart_remove));
    router_.get("/cart/count",     bind(&App::h_cart_count));
    router_.get("/checkout",       bind(&App::h_checkout_form));
    router_.post("/checkout",      bind(&App::h_checkout_submit));
    router_.get("/orders",         bind(&App::h_my_orders));

    // Admin
    router_.get("/admin",              bind(&App::h_admin_dashboard));
    router_.get("/admin/login",        bind(&App::h_admin_login_form));
    router_.post("/admin/login",       bind(&App::h_admin_login_submit));
    router_.get("/admin/logout",       bind(&App::h_admin_logout));
    router_.get("/admin/orders",       bind(&App::h_admin_orders));
    router_.get("/admin/order",        bind(&App::h_admin_order_view));
    router_.post("/admin/order/status",bind(&App::h_admin_order_status));
    router_.get("/admin/products",     bind(&App::h_admin_products));
    router_.post("/admin/product/save",bind(&App::h_admin_product_save));
    router_.post("/admin/product/delete", bind(&App::h_admin_product_delete));
    router_.get("/admin/categories",   bind(&App::h_admin_categories));
    router_.post("/admin/category/save",   bind(&App::h_admin_category_save));
    router_.post("/admin/category/delete", bind(&App::h_admin_category_delete));
    router_.get("/admin/users",        bind(&App::h_admin_users));
    router_.post("/admin/user/save",   bind(&App::h_admin_user_save));
    router_.post("/admin/user/delete", bind(&App::h_admin_user_delete));
    router_.get("/admin/settings",     bind(&App::h_admin_settings));
    router_.post("/admin/settings/save", bind(&App::h_admin_settings_save));

    // Everything else -> static files / 404
    router_.set_fallback([this](const Request& req, Response& res) {
        serve_static(req, res);
    });
}

bool App::run() {
    HttpServer server(cfg_.host, cfg_.port, router_);
    std::cerr << "[ofos] serving on http://" << cfg_.host << ":" << cfg_.port
              << "  (admin at /admin/login)\n";
    return server.run();
}

// ---------------------------------------------------------------------------
// Session + auth helpers.
// ---------------------------------------------------------------------------
std::string App::ensure_session(const Request& req, Response& res) {
    std::string sid = req.cookie("ofos_sid");
    if (!sid.empty() && sessions_.exists(sid)) return sid;
    sid = sessions_.create();
    res.set_cookie("ofos_sid", sid, "/", 3600);   // 1-hour session cookie
    return sid;
}

long App::customer_id(const std::string& sid) {
    return util::to_long(sessions_.value(sid, "customer_id", "0"), 0);
}
long App::admin_id(const std::string& sid) {
    return util::to_long(sessions_.value(sid, "admin_id", "0"), 0);
}

bool App::require_customer(const std::string& sid, Response& res) {
    if (customer_id(sid) > 0) return true;
    res.redirect("/login?next=cart");
    return false;
}

bool App::require_admin(const Request& req, Response& res, std::string& sid) {
    sid = ensure_session(req, res);
    if (admin_id(sid) > 0) return true;
    res.redirect("/admin/login");
    return false;
}

// ---------------------------------------------------------------------------
// Settings-derived template context.
// ---------------------------------------------------------------------------
Context App::site_context() {
    Context c;
    auto rows = db_->query("SELECT * FROM settings WHERE id=1");
    if (!rows.empty()) {
        for (auto& kv : rows[0]) c[kv.first] = kv.second;
    }
    if (c.count("system_name") == 0 || c["system_name"].empty())
        c["system_name"] = "Online Food Ordering System";
    if (c.count("currency") == 0 || c["currency"].empty())
        c["currency"] = "Rs.";
    return c;
}

int App::cart_count(long cid) {
    if (cid <= 0) return 0;
    return static_cast<int>(util::to_long(
        db_->scalar("SELECT COALESCE(SUM(quantity),0) FROM carts WHERE customer_id=?",
                    {std::to_string(cid)}, "0"),
        0));
}

std::string App::nav_html(const std::string& sid) {
    long cid = customer_id(sid);
    std::string links;
    links += "<a href=\"/\">Home</a>";
    links += "<a href=\"/menu\">Menu</a>";
    links += "<a href=\"/about\">About</a>";
    links += "<a href=\"/cart\" class=\"nav-cart\">Cart "
             "<span id=\"cart-badge\" class=\"badge\">" +
             std::to_string(cart_count(cid)) + "</span></a>";
    if (cid > 0) {
        std::string name = sessions_.value(sid, "customer_name", "Account");
        links += "<a href=\"/orders\">My Orders</a>";
        links += "<span class=\"nav-user\">Hi, " + util::html_escape(name) + "</span>";
        links += "<a href=\"/logout\" class=\"btn-link\">Logout</a>";
    } else {
        links += "<a href=\"/login\">Login</a>";
        links += "<a href=\"/register\" class=\"btn-link\">Register</a>";
    }
    return links;
}

// ---------------------------------------------------------------------------
// Layout rendering.
// ---------------------------------------------------------------------------
void App::render_layout(Response& res, const std::string& title,
                        const std::string& content, const std::string& sid,
                        const std::string& active) {
    Context c = site_context();
    c["title"] = title;
    c["content"] = content;
    c["nav"] = nav_html(sid);
    c["active"] = active;
    c["year"] = util::now_utc().substr(0, 4);
    res.html(tpl_->render("layout.html", c));
}

void App::render_admin_layout(Response& res, const std::string& title,
                              const std::string& content,
                              const std::string& active) {
    Context c = site_context();
    c["title"] = title;
    c["content"] = content;
    c["active_" + active] = "active";
    c["year"] = util::now_utc().substr(0, 4);
    res.html(tpl_->render("admin/layout.html", c));
}

// ---------------------------------------------------------------------------
// Static file serving (restricted to /public/*).
// ---------------------------------------------------------------------------
static std::string content_type_for(const std::string& ext) {
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "js") return "application/javascript; charset=utf-8";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "ico") return "image/x-icon";
    if (ext == "json") return "application/json";
    if (ext == "txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

void App::serve_static(const Request& req, Response& res) {
    if (!util::starts_with(req.path, "/public/")) {
        res.html("<h1>404 Not Found</h1><p>" + util::html_escape(req.path) +
                     " does not exist.</p>",
                 404);
        return;
    }
    // Reject path traversal.
    if (req.path.find("..") != std::string::npos) {
        res.text("403 Forbidden", 403);
        return;
    }
    std::string rel = req.path.substr(std::string("/public/").size());
    std::string full = cfg_.base_dir + "/public/" + rel;
    std::string data;
    if (!util::read_file(full, data)) {
        res.text("404 Not Found", 404);
        return;
    }
    res.body = std::move(data);
    res.content_type = content_type_for(util::file_extension(rel));
    res.status = 200;
    res.status_text = "OK";
    res.set_header("Cache-Control", "public, max-age=3600");
}

// ---------------------------------------------------------------------------
// Upload handling: validate type + persist under public/uploads/.
// ---------------------------------------------------------------------------
std::string App::save_upload(const UploadedFile* file, const std::string& fallback) {
    if (!file || file->empty()) return fallback;
    std::string ext = util::file_extension(file->filename);
    if (ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "gif" &&
        ext != "webp" && ext != "svg") {
        return fallback;   // reject non-image types, keep existing image
    }
    std::string name = "up_" + util::random_hex(10) + "." + ext;
    std::string path = cfg_.base_dir + "/public/uploads/" + name;
    if (!util::write_file(path, file->data)) return fallback;
    return "uploads/" + name;
}

} // namespace ofos
