#include "app.hpp"
#include "json.hpp"
#include "utils.hpp"

#include <sstream>

namespace ofos {

static std::string img_url_c(const std::string& image) {
    if (image.empty()) return "/public/img/placeholder.svg";
    if (util::starts_with(image, "http")) return image;
    return "/public/" + image;
}

// Shared: load cart lines with product info + running total.
static std::vector<Database::Row> load_cart(Database& db, long cid, double& total) {
    auto rows = db.query(
        "SELECT ct.product_id, ct.quantity, p.name, p.price, p.image, p.status "
        "FROM carts ct JOIN products p ON p.id=ct.product_id "
        "WHERE ct.customer_id=? ORDER BY ct.id",
        {std::to_string(cid)});
    total = 0;
    for (auto& r : rows) {
        double sub = util::to_double(r["price"]) * util::to_long(r["quantity"]);
        r["subtotal"] = util::money(sub);
        total += sub;
    }
    return rows;
}

// ---- AJAX: add to cart -----------------------------------------------------
void App::h_cart_add(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    long cid = customer_id(sid);
    if (cid <= 0) {
        Json j; j.boolean("success", false).str("error", "login_required")
            .str("redirect", "/login?next=cart");
        res.json(j.dump(), 401);
        return;
    }
    long pid = util::to_long(req.post("product_id"), 0);
    long qty = util::to_long(req.post("quantity", "1"), 1);
    if (qty < 1) qty = 1;
    if (qty > 99) qty = 99;

    if (db_->scalar("SELECT COUNT(*) FROM products WHERE id=? AND status=1",
                    {std::to_string(pid)}) == "0") {
        res.json(Json::error("Item is not available."), 404);
        return;
    }
    db_->execute(
        "INSERT INTO carts(customer_id,product_id,quantity,created_at) VALUES (?,?,?,?) "
        "ON CONFLICT(customer_id,product_id) DO UPDATE SET quantity=quantity+excluded.quantity",
        {std::to_string(cid), std::to_string(pid), std::to_string(qty), util::now_utc()});

    Json j;
    j.boolean("success", true)
        .integer("cart_count", cart_count(cid))
        .str("message", "Added to cart");
    res.json(j.dump());
}

// ---- AJAX: update quantity -------------------------------------------------
void App::h_cart_update(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    long cid = customer_id(sid);
    if (cid <= 0) { res.json(Json::error("Not logged in"), 401); return; }

    long pid = util::to_long(req.post("product_id"), 0);
    long qty = util::to_long(req.post("quantity"), 1);
    if (qty < 1) {
        db_->execute("DELETE FROM carts WHERE customer_id=? AND product_id=?",
                     {std::to_string(cid), std::to_string(pid)});
    } else {
        if (qty > 99) qty = 99;
        db_->execute("UPDATE carts SET quantity=? WHERE customer_id=? AND product_id=?",
                     {std::to_string(qty), std::to_string(cid), std::to_string(pid)});
    }
    double total = 0;
    auto rows = load_cart(*db_, cid, total);
    std::string line_sub = "0.00";
    for (auto& r : rows)
        if (util::to_long(r["product_id"]) == pid) line_sub = r["subtotal"];

    Json j;
    j.boolean("success", true)
        .integer("cart_count", cart_count(cid))
        .integer("items", static_cast<long long>(rows.size()))
        .str("line_subtotal", line_sub)
        .num("total", total);
    res.json(j.dump());
}

// ---- AJAX: remove line -----------------------------------------------------
void App::h_cart_remove(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    long cid = customer_id(sid);
    if (cid <= 0) { res.json(Json::error("Not logged in"), 401); return; }
    long pid = util::to_long(req.post("product_id"), 0);
    db_->execute("DELETE FROM carts WHERE customer_id=? AND product_id=?",
                 {std::to_string(cid), std::to_string(pid)});
    double total = 0;
    auto rows = load_cart(*db_, cid, total);
    Json j;
    j.boolean("success", true)
        .integer("cart_count", cart_count(cid))
        .integer("items", static_cast<long long>(rows.size()))
        .num("total", total);
    res.json(j.dump());
}

// ---- AJAX: badge count -----------------------------------------------------
void App::h_cart_count(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    Json j; j.boolean("success", true).integer("count", cart_count(customer_id(sid)));
    res.json(j.dump());
}

// ---- Cart page -------------------------------------------------------------
void App::h_cart_view(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    if (!require_customer(sid, res)) return;
    long cid = customer_id(sid);
    Context s = site_context();

    double total = 0;
    auto rows = load_cart(*db_, cid, total);

    std::string body;
    if (rows.empty()) {
        body = "<tr><td colspan=\"5\" class=\"empty\">Your cart is empty. "
               "<a href=\"/menu\">Browse the menu</a>.</td></tr>";
    } else {
        for (auto& r : rows) {
            std::ostringstream tr;
            tr << "<tr data-id=\"" << r["product_id"] << "\">"
               << "<td class=\"cart-item\"><img src=\""
               << util::html_escape(img_url_c(r["image"])) << "\" alt=\"\">"
               << "<span>" << util::html_escape(r["name"]) << "</span></td>"
               << "<td>" << util::html_escape(s["currency"]) << " "
               << util::money(util::to_double(r["price"])) << "</td>"
               << "<td class=\"qty-cell\">"
               << "<button class=\"qstep\" onclick=\"changeQty(" << r["product_id"] << ",-1)\">&minus;</button>"
               << "<input class=\"qty-input\" id=\"qty-" << r["product_id"]
               << "\" value=\"" << r["quantity"] << "\" readonly>"
               << "<button class=\"qstep\" onclick=\"changeQty(" << r["product_id"] << ",1)\">+</button>"
               << "</td>"
               << "<td class=\"sub\" id=\"sub-" << r["product_id"] << "\">"
               << util::html_escape(s["currency"]) << " " << r["subtotal"] << "</td>"
               << "<td><button class=\"btn-del\" onclick=\"removeItem(" << r["product_id"]
               << ")\">Remove</button></td></tr>";
            body += tr.str();
        }
    }

    Context c = s;
    c["rows"] = body;
    c["total"] = util::money(total);
    c["has_items"] = rows.empty() ? "" : "1";
    c["checkout_disabled"] = rows.empty() ? "disabled" : "";
    std::string content = tpl_->render("cart.html", c);
    render_layout(res, "Your Cart", content, sid, "");
}

// ---- Checkout page ---------------------------------------------------------
void App::h_checkout_form(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    if (!require_customer(sid, res)) return;
    long cid = customer_id(sid);
    Context s = site_context();

    double total = 0;
    auto rows = load_cart(*db_, cid, total);
    if (rows.empty()) { res.redirect("/cart"); return; }

    std::string summary;
    for (auto& r : rows) {
        summary += "<li><span>" + util::html_escape(r["name"]) + " &times; " +
                   r["quantity"] + "</span><span>" + util::html_escape(s["currency"]) +
                   " " + r["subtotal"] + "</span></li>";
    }
    auto cust = db_->query("SELECT name,phone,address FROM customers WHERE id=?",
                           {std::to_string(cid)});
    Context c = s;
    c["summary"] = summary;
    c["total"] = util::money(total);
    if (!cust.empty()) {
        c["name"] = cust[0]["name"];
        c["phone"] = cust[0]["phone"];
        c["address"] = cust[0]["address"];
    }
    std::string content = tpl_->render("checkout.html", c);
    render_layout(res, "Checkout", content, sid, "");
}

// ---- AJAX: place order -----------------------------------------------------
void App::h_checkout_submit(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    long cid = customer_id(sid);
    if (cid <= 0) {
        Json j; j.boolean("success", false).str("error", "login_required")
            .str("redirect", "/login?next=checkout");
        res.json(j.dump(), 401);
        return;
    }
    std::string name = util::trim(req.post("name"));
    std::string phone = util::trim(req.post("phone"));
    std::string address = util::trim(req.post("address"));
    std::string pay = req.post("payment_method", "Cash on Delivery");
    std::string note = util::trim(req.post("note"));

    if (name.empty() || phone.empty() || address.empty()) {
        res.json(Json::error("Please fill name, phone and delivery address."), 400);
        return;
    }

    double total = 0;
    auto items = load_cart(*db_, cid, total);
    if (items.empty()) {
        res.json(Json::error("Your cart is empty."), 400);
        return;
    }

    // Order number: ORD + YYYYMMDDHHMMSS + short random suffix (UNIQUE-backed).
    std::string order_no = "ORD" + util::replace_all(util::now_utc(), "-", "");
    order_no = util::replace_all(order_no, ":", "");
    order_no = util::replace_all(order_no, " ", "");
    order_no += util::random_hex(2);

    long order_id = 0;
    db_->transaction([&] {
        db_->execute(
            "INSERT INTO orders(order_number,customer_id,customer_name,phone,address,"
            "payment_method,total_amount,status,note,created_at) "
            "VALUES (?,?,?,?,?,?,?,'Pending',?,?)",
            {order_no, std::to_string(cid), name, phone, address, pay,
             util::money(total), note, util::now_utc()});
        order_id = static_cast<long>(db_->last_insert_id());

        for (auto& it : items) {
            double sub = util::to_double(it["price"]) * util::to_long(it["quantity"]);
            db_->execute(
                "INSERT INTO order_items(order_id,product_id,product_name,price,"
                "quantity,subtotal) VALUES (?,?,?,?,?,?)",
                {std::to_string(order_id), it["product_id"], it["name"],
                 util::money(util::to_double(it["price"])), it["quantity"],
                 util::money(sub)});
        }
        db_->execute("DELETE FROM carts WHERE customer_id=?", {std::to_string(cid)});
    });

    Json j;
    j.boolean("success", true)
        .str("order_number", order_no)
        .num("total", total)
        .str("redirect", "/orders?placed=" + util::url_encode(order_no));
    res.json(j.dump());
}

// ---- My orders page --------------------------------------------------------
void App::h_my_orders(const Request& req, Response& res) {
    std::string sid = ensure_session(req, res);
    if (!require_customer(sid, res)) return;
    long cid = customer_id(sid);
    Context s = site_context();

    auto orders = db_->query(
        "SELECT * FROM orders WHERE customer_id=? ORDER BY id DESC",
        {std::to_string(cid)});

    std::string list;
    if (orders.empty()) {
        list = "<p class=\"empty\">You have not placed any orders yet. "
               "<a href=\"/menu\">Start ordering</a>.</p>";
    } else {
        for (auto& o : orders) {
            auto its = db_->query(
                "SELECT product_name,quantity,subtotal FROM order_items WHERE order_id=?",
                {o["id"]});
            std::string lines;
            for (auto& it : its)
                lines += "<li>" + util::html_escape(it["product_name"]) + " &times; " +
                         it["quantity"] + "<span>" + util::html_escape(s["currency"]) +
                         " " + util::money(util::to_double(it["subtotal"])) + "</span></li>";
            std::string status = o["status"];
            std::ostringstream card;
            card << "<div class=\"order-card\">"
                 << "<div class=\"order-head\"><div><strong>" << util::html_escape(o["order_number"])
                 << "</strong><br><small>" << util::html_escape(o["created_at"]) << "</small></div>"
                 << "<span class=\"status status-" << util::to_lower(status) << "\">"
                 << util::html_escape(status) << "</span></div>"
                 << "<ul class=\"order-lines\">" << lines << "</ul>"
                 << "<div class=\"order-foot\"><span>" << util::html_escape(o["payment_method"])
                 << "</span><strong>" << util::html_escape(s["currency"]) << " "
                 << util::money(util::to_double(o["total_amount"])) << "</strong></div></div>";
            list += card.str();
        }
    }

    Context c = s;
    c["orders"] = list;
    c["placed"] = req.get("placed");
    std::string content = tpl_->render("orders.html", c);
    render_layout(res, "My Orders", content, sid, "");
}

} // namespace ofos
