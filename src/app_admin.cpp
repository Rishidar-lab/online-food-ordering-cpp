#include "app.hpp"
#include "json.hpp"
#include "utils.hpp"

#include <sstream>

namespace ofos {

static std::string img_url_a(const std::string& image) {
    if (image.empty()) return "/public/img/placeholder.svg";
    if (util::starts_with(image, "http")) return image;
    return "/public/" + image;
}

// ---- Admin authentication --------------------------------------------------
void App::h_admin_login_form(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    if (admin_id(sid) > 0) { res.redirect("/admin"); return; }
    Context c = site_context();
    c["error"] = "";
    res.html(tpl_->render("admin/login.html", c));
}

void App::h_admin_login_submit(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    std::string username = util::trim(req.post("username"));
    std::string pw = req.post("password");

    auto rows = db_->query(
        "SELECT id,full_name,password_hash,salt FROM users WHERE username=?",
        {username});
    bool ok = false;
    if (!rows.empty()) {
        std::string computed = util::hash_password(pw, rows[0]["salt"]);
        ok = util::secure_equals(computed, rows[0]["password_hash"]);
    }
    if (!ok) {
        Context c = site_context();
        c["error"] = "Invalid administrator credentials.";
        res.html(tpl_->render("admin/login.html", c));
        return;
    }
    sessions_.set(sid, "admin_id", rows[0]["id"]);
    sessions_.set(sid, "admin_name", rows[0]["full_name"]);
    res.redirect("/admin");
}

void App::h_admin_logout(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    sessions_.erase_key(sid, "admin_id");
    sessions_.erase_key(sid, "admin_name");
    res.redirect("/admin/login");
}

// ---- Dashboard -------------------------------------------------------------
void App::h_admin_dashboard(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    Context s = site_context();

    std::string cur = s["currency"];
    Context c = s;
    c["admin_name"] = sessions_.value(sid, "admin_name", "Administrator");
    c["count_orders"]   = db_->scalar("SELECT COUNT(*) FROM orders", {}, "0");
    c["count_pending"]  = db_->scalar("SELECT COUNT(*) FROM orders WHERE status='Pending'", {}, "0");
    c["count_products"] = db_->scalar("SELECT COUNT(*) FROM products", {}, "0");
    c["count_categories"] = db_->scalar("SELECT COUNT(*) FROM categories", {}, "0");
    c["count_customers"] = db_->scalar("SELECT COUNT(*) FROM customers", {}, "0");
    double revenue = util::to_double(db_->scalar(
        "SELECT COALESCE(SUM(total_amount),0) FROM orders "
        "WHERE status IN ('Confirmed','Completed')", {}, "0"));
    c["revenue"] = cur + " " + util::money(revenue);

    auto recent = db_->query(
        "SELECT order_number,customer_name,total_amount,status,created_at "
        "FROM orders ORDER BY id DESC LIMIT 8");
    std::string rows;
    if (recent.empty()) {
        rows = "<tr><td colspan=\"5\" class=\"empty\">No orders yet.</td></tr>";
    } else {
        for (auto& o : recent) {
            rows += "<tr><td>" + util::html_escape(o["order_number"]) + "</td><td>" +
                    util::html_escape(o["customer_name"]) + "</td><td>" + cur + " " +
                    util::money(util::to_double(o["total_amount"])) +
                    "</td><td><span class=\"status status-" + util::to_lower(o["status"]) +
                    "\">" + util::html_escape(o["status"]) + "</span></td><td>" +
                    util::html_escape(o["created_at"]) + "</td></tr>";
        }
    }
    c["recent_orders"] = rows;
    render_admin_layout(res, "Dashboard", tpl_->render("admin/dashboard.html", c), "dashboard");
}

// ---- Orders ----------------------------------------------------------------
void App::h_admin_orders(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    Context s = site_context();
    std::string cur = s["currency"];
    std::string filter = req.get("status");

    std::string sql = "SELECT * FROM orders";
    std::vector<std::string> params;
    if (!filter.empty() && filter != "All") {
        sql += " WHERE status=?";
        params.push_back(filter);
    }
    sql += " ORDER BY id DESC";
    auto orders = db_->query(sql, params);

    std::string rows;
    if (orders.empty()) {
        rows = "<tr><td colspan=\"7\" class=\"empty\">No orders found.</td></tr>";
    } else {
        for (auto& o : orders) {
            std::ostringstream tr;
            std::string st = o["status"];
            tr << "<tr id=\"order-" << o["id"] << "\">"
               << "<td><a href=\"/admin/order?id=" << o["id"] << "\">"
               << util::html_escape(o["order_number"]) << "</a></td>"
               << "<td>" << util::html_escape(o["customer_name"]) << "</td>"
               << "<td>" << util::html_escape(o["phone"]) << "</td>"
               << "<td>" << cur << " " << util::money(util::to_double(o["total_amount"])) << "</td>"
               << "<td><span class=\"status status-" << util::to_lower(st)
               << "\" id=\"st-" << o["id"] << "\">" << util::html_escape(st) << "</span></td>"
               << "<td>" << util::html_escape(o["created_at"]) << "</td>"
               << "<td class=\"row-actions\">";
            if (st == "Pending")
                tr << "<button class=\"btn btn-sm btn-primary\" onclick=\"setStatus("
                   << o["id"] << ",'Confirmed',this)\">Confirm</button>";
            if (st != "Completed" && st != "Cancelled")
                tr << "<button class=\"btn btn-sm\" onclick=\"setStatus("
                   << o["id"] << ",'Completed',this)\">Complete</button>";
            if (st != "Cancelled" && st != "Completed")
                tr << "<button class=\"btn btn-sm btn-danger\" onclick=\"setStatus("
                   << o["id"] << ",'Cancelled',this)\">Cancel</button>";
            tr << "<a class=\"btn btn-sm\" href=\"/admin/order?id=" << o["id"] << "\">View</a>";
            tr << "</td></tr>";
            rows += tr.str();
        }
    }
    auto chip = [&](const char* name) {
        bool on = (filter == name) || (filter.empty() && std::string(name) == "All");
        return "<a class=\"chip" + std::string(on ? " chip-on" : "") +
               "\" href=\"/admin/orders?status=" + name + "\">" + name + "</a>";
    };
    Context c = s;
    c["rows"] = rows;
    c["filters"] = chip("All") + chip("Pending") + chip("Confirmed") +
                   chip("Completed") + chip("Cancelled");
    render_admin_layout(res, "Orders", tpl_->render("admin/orders.html", c), "orders");
}

void App::h_admin_order_view(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    Context s = site_context();
    std::string cur = s["currency"];
    long id = util::to_long(req.get("id"), 0);
    auto rows = db_->query("SELECT * FROM orders WHERE id=?", {std::to_string(id)});
    if (rows.empty()) { render_admin_layout(res, "Order", "<p>Order not found.</p>", "orders"); return; }
    auto& o = rows[0];
    auto items = db_->query("SELECT * FROM order_items WHERE order_id=?", {std::to_string(id)});
    std::string lines;
    for (auto& it : items) {
        lines += "<tr><td>" + util::html_escape(it["product_name"]) + "</td><td>" + cur + " " +
                 util::money(util::to_double(it["price"])) + "</td><td>" + it["quantity"] +
                 "</td><td>" + cur + " " + util::money(util::to_double(it["subtotal"])) + "</td></tr>";
    }
    Context c = s;
    for (auto& kv : o) c["o_" + kv.first] = kv.second;
    c["lines"] = lines;
    c["total_fmt"] = cur + " " + util::money(util::to_double(o["total_amount"]));
    c["status_lower"] = util::to_lower(o["status"]);
    render_admin_layout(res, "Order " + o["order_number"],
                        tpl_->render("admin/order_view.html", c), "orders");
}

// AJAX: change order status (verify/confirm without page reload).
void App::h_admin_order_status(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    if (admin_id(sid) <= 0) { res.json(Json::error("Not authorised"), 401); return; }
    long id = util::to_long(req.post("id"), 0);
    std::string status = req.post("status");
    static const std::vector<std::string> allowed =
        {"Pending", "Confirmed", "Completed", "Cancelled"};
    bool valid = false;
    for (auto& a : allowed) if (a == status) valid = true;
    if (!valid) { res.json(Json::error("Invalid status"), 400); return; }

    int changed = db_->execute("UPDATE orders SET status=? WHERE id=?",
                               {status, std::to_string(id)});
    if (changed == 0) { res.json(Json::error("Order not found"), 404); return; }
    Json j; j.boolean("success", true).str("status", status)
        .integer("pending", util::to_long(db_->scalar(
            "SELECT COUNT(*) FROM orders WHERE status='Pending'", {}, "0")));
    res.json(j.dump());
}

// ---- Products --------------------------------------------------------------
void App::h_admin_products(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    Context s = site_context();
    std::string cur = s["currency"];

    // Category <option> list (for the add/edit form).
    auto cats = db_->query("SELECT id,name FROM categories ORDER BY name");
    long edit_id = util::to_long(req.get("edit"), 0);
    Database::Row edit;
    if (edit_id > 0) {
        auto e = db_->query("SELECT * FROM products WHERE id=?", {std::to_string(edit_id)});
        if (!e.empty()) edit = e[0];
    }
    std::string options;
    for (auto& cat : cats) {
        bool sel = (edit.count("category_id") && edit["category_id"] == cat["id"]);
        options += "<option value=\"" + cat["id"] + "\"" + (sel ? " selected" : "") +
                   ">" + util::html_escape(cat["name"]) + "</option>";
    }

    auto prods = db_->query(
        "SELECT p.*, c.name AS category_name FROM products p "
        "LEFT JOIN categories c ON c.id=p.category_id ORDER BY p.id DESC");
    std::string rows;
    if (prods.empty()) {
        rows = "<tr><td colspan=\"6\" class=\"empty\">No products yet.</td></tr>";
    } else {
        for (auto& p : prods) {
            rows += "<tr><td><img class=\"thumb\" src=\"" + util::html_escape(img_url_a(p["image"])) +
                    "\" alt=\"\"></td><td>" + util::html_escape(p["name"]) + "</td><td>" +
                    util::html_escape(p["category_name"]) + "</td><td>" + cur + " " +
                    util::money(util::to_double(p["price"])) + "</td><td>" +
                    (p["status"] == "1" ? "<span class=\"status status-confirmed\">Available</span>"
                                        : "<span class=\"status status-cancelled\">Hidden</span>") +
                    "</td><td class=\"row-actions\"><a class=\"btn btn-sm\" href=\"/admin/products?edit=" +
                    p["id"] + "\">Edit</a><form method=\"post\" action=\"/admin/product/delete\" "
                    "onsubmit=\"return confirm('Delete this product?')\" style=\"display:inline\">"
                    "<input type=\"hidden\" name=\"id\" value=\"" + p["id"] +
                    "\"><button class=\"btn btn-sm btn-danger\">Delete</button></form></td></tr>";
        }
    }

    Context c = s;
    c["options"] = options;
    c["rows"] = rows;
    c["f_id"] = edit.count("id") ? edit["id"] : "";
    c["f_name"] = edit.count("name") ? edit["name"] : "";
    c["f_description"] = edit.count("description") ? edit["description"] : "";
    c["f_price"] = edit.count("price") ? util::money(util::to_double(edit["price"])) : "";
    c["f_status_av"] = (!edit.count("status") || edit["status"] == "1") ? "selected" : "";
    c["f_status_hi"] = (edit.count("status") && edit["status"] == "0") ? "selected" : "";
    c["form_title"] = edit.count("id") ? "Edit product" : "Add product";
    c["current_image"] = edit.count("image") ? img_url_a(edit["image"]) : "";
    render_admin_layout(res, "Products", tpl_->render("admin/products.html", c), "products");
}

void App::h_admin_product_save(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    std::string id = util::trim(req.post("id"));
    std::string name = util::trim(req.post("name"));
    std::string desc = util::trim(req.post("description"));
    std::string price = util::money(util::to_double(req.post("price")));
    std::string cat = req.post("category_id");
    std::string status = req.post("status") == "0" ? "0" : "1";

    if (name.empty() || cat.empty()) { res.redirect("/admin/products"); return; }

    const UploadedFile* file = req.file("image");
    if (id.empty()) {
        std::string image = save_upload(file, "");
        db_->execute(
            "INSERT INTO products(category_id,name,description,price,image,status,created_at)"
            " VALUES (?,?,?,?,?,?,?)",
            {cat, name, desc, price, image, status, util::now_utc()});
    } else {
        std::string existing = db_->scalar("SELECT image FROM products WHERE id=?", {id});
        std::string image = save_upload(file, existing);
        db_->execute(
            "UPDATE products SET category_id=?,name=?,description=?,price=?,image=?,status=? WHERE id=?",
            {cat, name, desc, price, image, status, id});
    }
    res.redirect("/admin/products");
}

void App::h_admin_product_delete(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    db_->execute("DELETE FROM products WHERE id=?", {req.post("id")});
    res.redirect("/admin/products");
}

// ---- Categories ------------------------------------------------------------
void App::h_admin_categories(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    Context s = site_context();

    long edit_id = util::to_long(req.get("edit"), 0);
    Database::Row edit;
    if (edit_id > 0) {
        auto e = db_->query("SELECT * FROM categories WHERE id=?", {std::to_string(edit_id)});
        if (!e.empty()) edit = e[0];
    }
    auto cats = db_->query(
        "SELECT c.*, (SELECT COUNT(*) FROM products p WHERE p.category_id=c.id) AS n "
        "FROM categories c ORDER BY c.name");
    std::string rows;
    if (cats.empty()) {
        rows = "<tr><td colspan=\"4\" class=\"empty\">No categories yet.</td></tr>";
    } else {
        for (auto& ct : cats) {
            rows += "<tr><td>" + util::html_escape(ct["name"]) + "</td><td>" +
                    util::html_escape(ct["description"]) + "</td><td>" + ct["n"] +
                    "</td><td class=\"row-actions\"><a class=\"btn btn-sm\" href=\"/admin/categories?edit=" +
                    ct["id"] + "\">Edit</a><form method=\"post\" action=\"/admin/category/delete\" "
                    "onsubmit=\"return confirm('Delete category and its products?')\" style=\"display:inline\">"
                    "<input type=\"hidden\" name=\"id\" value=\"" + ct["id"] +
                    "\"><button class=\"btn btn-sm btn-danger\">Delete</button></form></td></tr>";
        }
    }
    Context c = s;
    c["rows"] = rows;
    c["f_id"] = edit.count("id") ? edit["id"] : "";
    c["f_name"] = edit.count("name") ? edit["name"] : "";
    c["f_description"] = edit.count("description") ? edit["description"] : "";
    c["form_title"] = edit.count("id") ? "Edit category" : "Add category";
    render_admin_layout(res, "Categories", tpl_->render("admin/categories.html", c), "categories");
}

void App::h_admin_category_save(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    std::string id = util::trim(req.post("id"));
    std::string name = util::trim(req.post("name"));
    std::string desc = util::trim(req.post("description"));
    if (name.empty()) { res.redirect("/admin/categories"); return; }
    if (id.empty()) {
        db_->execute("INSERT INTO categories(name,description,status,created_at) VALUES (?,?,1,?)",
                     {name, desc, util::now_utc()});
    } else {
        db_->execute("UPDATE categories SET name=?,description=? WHERE id=?", {name, desc, id});
    }
    res.redirect("/admin/categories");
}

void App::h_admin_category_delete(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    db_->execute("DELETE FROM categories WHERE id=?", {req.post("id")});
    res.redirect("/admin/categories");
}

// ---- Users (admin/staff accounts) ------------------------------------------
void App::h_admin_users(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    Context s = site_context();

    long edit_id = util::to_long(req.get("edit"), 0);
    Database::Row edit;
    if (edit_id > 0) {
        auto e = db_->query("SELECT * FROM users WHERE id=?", {std::to_string(edit_id)});
        if (!e.empty()) edit = e[0];
    }
    auto users = db_->query("SELECT id,username,full_name,role,created_at FROM users ORDER BY id");
    long me = admin_id(sid);
    std::string rows;
    for (auto& u : users) {
        bool self = (util::to_long(u["id"]) == me);
        rows += "<tr><td>" + util::html_escape(u["username"]) + (self ? " <em>(you)</em>" : "") +
                "</td><td>" + util::html_escape(u["full_name"]) + "</td><td>" +
                util::html_escape(u["role"]) + "</td><td class=\"row-actions\">"
                "<a class=\"btn btn-sm\" href=\"/admin/users?edit=" + u["id"] + "\">Edit</a>";
        if (!self)
            rows += "<form method=\"post\" action=\"/admin/user/delete\" "
                    "onsubmit=\"return confirm('Delete this user?')\" style=\"display:inline\">"
                    "<input type=\"hidden\" name=\"id\" value=\"" + u["id"] +
                    "\"><button class=\"btn btn-sm btn-danger\">Delete</button></form>";
        rows += "</td></tr>";
    }
    Context c = s;
    c["rows"] = rows;
    c["f_id"] = edit.count("id") ? edit["id"] : "";
    c["f_username"] = edit.count("username") ? edit["username"] : "";
    c["f_fullname"] = edit.count("full_name") ? edit["full_name"] : "";
    c["f_role_admin"] = (!edit.count("role") || edit["role"] == "admin") ? "selected" : "";
    c["f_role_staff"] = (edit.count("role") && edit["role"] == "staff") ? "selected" : "";
    c["form_title"] = edit.count("id") ? "Edit user" : "Add user";
    c["pw_hint"] = edit.count("id") ? "Leave blank to keep the current password." : "";
    render_admin_layout(res, "Users", tpl_->render("admin/users.html", c), "users");
}

void App::h_admin_user_save(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    std::string id = util::trim(req.post("id"));
    std::string username = util::trim(req.post("username"));
    std::string fullname = util::trim(req.post("full_name"));
    std::string role = req.post("role") == "staff" ? "staff" : "admin";
    std::string pw = req.post("password");
    if (username.empty()) { res.redirect("/admin/users"); return; }

    if (id.empty()) {
        if (pw.size() < 6) { res.redirect("/admin/users"); return; }
        if (db_->scalar("SELECT COUNT(*) FROM users WHERE username=?", {username}) != "0") {
            res.redirect("/admin/users");
            return;
        }
        std::string salt = util::random_hex(16);
        std::string hash = util::hash_password(pw, salt);
        db_->execute("INSERT INTO users(username,password_hash,salt,full_name,role,created_at)"
                     " VALUES (?,?,?,?,?,?)",
                     {username, hash, salt, fullname, role, util::now_utc()});
    } else {
        db_->execute("UPDATE users SET username=?,full_name=?,role=? WHERE id=?",
                     {username, fullname, role, id});
        if (pw.size() >= 6) {
            std::string salt = util::random_hex(16);
            std::string hash = util::hash_password(pw, salt);
            db_->execute("UPDATE users SET password_hash=?,salt=? WHERE id=?", {hash, salt, id});
        }
    }
    res.redirect("/admin/users");
}

void App::h_admin_user_delete(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    long id = util::to_long(req.post("id"), 0);
    if (id == admin_id(sid)) { res.redirect("/admin/users"); return; }  // never delete self
    if (util::to_long(db_->scalar("SELECT COUNT(*) FROM users", {}, "0")) <= 1) {
        res.redirect("/admin/users");   // keep at least one account
        return;
    }
    db_->execute("DELETE FROM users WHERE id=?", {std::to_string(id)});
    res.redirect("/admin/users");
}

// ---- Settings --------------------------------------------------------------
void App::h_admin_settings(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    Context s = site_context();
    Context c = s;
    c["cover_url"] = img_url_a(s["cover_image"]);
    c["saved"] = req.get("saved");
    render_admin_layout(res, "Settings", tpl_->render("admin/settings.html", c), "settings");
}

void App::h_admin_settings_save(const Request& req, Response& res) {
    std::string sid;
    if (!require_admin(req, res, sid)) return;
    std::string name = util::trim(req.post("system_name"));
    std::string email = util::trim(req.post("contact_email"));
    std::string phone = util::trim(req.post("contact_phone"));
    std::string address = util::trim(req.post("address"));
    std::string about = util::trim(req.post("about_us"));
    std::string currency = util::trim(req.post("currency"));
    if (currency.empty()) currency = "Rs.";
    if (name.empty()) name = "Online Food Ordering System";

    std::string existing = db_->scalar("SELECT cover_image FROM settings WHERE id=1");
    std::string cover = save_upload(req.file("cover_image"), existing);

    db_->execute(
        "UPDATE settings SET system_name=?,contact_email=?,contact_phone=?,address=?,"
        "cover_image=?,about_us=?,currency=? WHERE id=1",
        {name, email, phone, address, cover, about, currency});
    res.redirect("/admin/settings?saved=1");
}

} // namespace ofos
