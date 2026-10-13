#include "app.hpp"
#include "utils.hpp"

namespace ofos {

static std::string next_target(const std::string& next) {
    if (next == "cart") return "/cart";
    if (next == "checkout") return "/checkout";
    return "/menu";
}

void App::h_register_form(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    if (customer_id(sid) > 0) { res.redirect("/menu"); return; }
    Context c;
    c["error"] = "";
    c["name"] = c["email"] = c["phone"] = c["address"] = "";
    render_layout(res, "Create account", tpl_->render("register.html", c), sid, "");
}

void App::h_register_submit(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    std::string name = util::trim(req.post("name"));
    std::string email = util::to_lower(util::trim(req.post("email")));
    std::string phone = util::trim(req.post("phone"));
    std::string address = util::trim(req.post("address"));
    std::string pw = req.post("password");
    std::string pw2 = req.post("confirm");

    auto fail = [&](const std::string& msg) {
        Context c;
        c["error"] = msg;
        c["name"] = name; c["email"] = email;
        c["phone"] = phone; c["address"] = address;
        render_layout(res, "Create account", tpl_->render("register.html", c), sid, "");
    };

    if (name.empty() || email.empty() || pw.empty())
        return fail("Name, email and password are required.");
    if (email.find('@') == std::string::npos || email.find('.') == std::string::npos)
        return fail("Please enter a valid email address.");
    if (pw.size() < 6)
        return fail("Password must be at least 6 characters.");
    if (pw != pw2)
        return fail("Passwords do not match.");
    if (db_->scalar("SELECT COUNT(*) FROM customers WHERE email=?", {email}) != "0")
        return fail("An account with that email already exists.");

    std::string salt = util::random_hex(16);
    std::string hash = util::hash_password(pw, salt);
    db_->execute(
        "INSERT INTO customers(name,email,password_hash,salt,phone,address,created_at)"
        " VALUES (?,?,?,?,?,?,?)",
        {name, email, hash, salt, phone, address, util::now_utc()});
    long id = static_cast<long>(db_->last_insert_id());

    sessions_.set(sid, "customer_id", std::to_string(id));
    sessions_.set(sid, "customer_name", name);
    res.redirect("/menu");
}

void App::h_login_form(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    if (customer_id(sid) > 0) { res.redirect("/menu"); return; }
    Context c;
    c["error"] = "";
    c["email"] = "";
    c["next"] = req.get("next");
    render_layout(res, "Login", tpl_->render("login.html", c), sid, "");
}

void App::h_login_submit(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    std::string email = util::to_lower(util::trim(req.post("email")));
    std::string pw = req.post("password");
    std::string next = req.post("next");

    auto rows = db_->query(
        "SELECT id,name,password_hash,salt FROM customers WHERE email=?", {email});
    bool ok = false;
    if (!rows.empty()) {
        std::string computed = util::hash_password(pw, rows[0]["salt"]);
        ok = util::secure_equals(computed, rows[0]["password_hash"]);
    }
    if (!ok) {
        Context c;
        c["error"] = "Invalid email or password.";
        c["email"] = email;
        c["next"] = next;
        render_layout(res, "Login", tpl_->render("login.html", c), sid, "");
        return;
    }
    sessions_.set(sid, "customer_id", rows[0]["id"]);
    sessions_.set(sid, "customer_name", rows[0]["name"]);
    res.redirect(next_target(next));
}

void App::h_logout(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    sessions_.erase_key(sid, "customer_id");
    sessions_.erase_key(sid, "customer_name");
    res.redirect("/");
}

} // namespace ofos
