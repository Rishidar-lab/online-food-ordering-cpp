#ifndef OFOS_APP_HPP
#define OFOS_APP_HPP

#include "db.hpp"
#include "router.hpp"
#include "server.hpp"
#include "session.hpp"
#include "templates.hpp"

#include <memory>
#include <string>

namespace ofos {

struct AppConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string base_dir = ".";          // location of views/ and public/
    std::string db_path = "data/ofos.db";
    int per_page = 6;                     // menu cards per page
};

// The application: owns the database, sessions, templates and router, wires up
// every route, seeds first-run data, and serves requests.
class App {
public:
    explicit App(AppConfig config);

    void init();                 // schema + seed
    bool run();                  // start the HTTP server (blocking)

private:
    // ---- route registration -----------------------------------------------
    void register_routes();

    // ---- shared helpers (app.cpp) ------------------------------------------
    std::string ensure_session(const Request& req, Response& res);
    long customer_id(const std::string& sid);
    long admin_id(const std::string& sid);
    bool require_customer(const std::string& sid, Response& res);
    bool require_admin(const Request& req, Response& res, std::string& sid);
    Context site_context();                     // settings-derived template vars
    int cart_count(long customer_id);
    std::string nav_html(const std::string& sid);
    // Wrap page content in the public layout and write it to res.
    void render_layout(Response& res, const std::string& title,
                       const std::string& content, const std::string& sid,
                       const std::string& active = "");
    void render_admin_layout(Response& res, const std::string& title,
                             const std::string& content,
                             const std::string& active);
    void serve_static(const Request& req, Response& res);
    std::string save_upload(const UploadedFile* file, const std::string& fallback);

    // ---- public storefront (app_public.cpp) --------------------------------
    void h_home(const Request& req, Response& res);
    void h_menu(const Request& req, Response& res);
    void h_product_json(const Request& req, Response& res);   // AJAX popup
    void h_about(const Request& req, Response& res);

    // ---- auth (app_auth.cpp) -----------------------------------------------
    void h_register_form(const Request& req, Response& res);
    void h_register_submit(const Request& req, Response& res);
    void h_login_form(const Request& req, Response& res);
    void h_login_submit(const Request& req, Response& res);
    void h_logout(const Request& req, Response& res);

    // ---- cart + checkout (app_cart.cpp) ------------------------------------
    void h_cart_view(const Request& req, Response& res);
    void h_cart_add(const Request& req, Response& res);       // AJAX
    void h_cart_update(const Request& req, Response& res);    // AJAX
    void h_cart_remove(const Request& req, Response& res);    // AJAX
    void h_cart_count(const Request& req, Response& res);     // AJAX
    void h_checkout_form(const Request& req, Response& res);
    void h_checkout_submit(const Request& req, Response& res); // AJAX
    void h_my_orders(const Request& req, Response& res);

    // ---- admin (app_admin.cpp) ---------------------------------------------
    void h_admin_login_form(const Request& req, Response& res);
    void h_admin_login_submit(const Request& req, Response& res);
    void h_admin_logout(const Request& req, Response& res);
    void h_admin_dashboard(const Request& req, Response& res);
    void h_admin_orders(const Request& req, Response& res);
    void h_admin_order_view(const Request& req, Response& res);
    void h_admin_order_status(const Request& req, Response& res);  // AJAX
    void h_admin_products(const Request& req, Response& res);
    void h_admin_product_save(const Request& req, Response& res);
    void h_admin_product_delete(const Request& req, Response& res);
    void h_admin_categories(const Request& req, Response& res);
    void h_admin_category_save(const Request& req, Response& res);
    void h_admin_category_delete(const Request& req, Response& res);
    void h_admin_users(const Request& req, Response& res);
    void h_admin_user_save(const Request& req, Response& res);
    void h_admin_user_delete(const Request& req, Response& res);
    void h_admin_settings(const Request& req, Response& res);
    void h_admin_settings_save(const Request& req, Response& res);

    AppConfig cfg_;
    std::unique_ptr<Database> db_;
    SessionStore sessions_{3600};
    std::unique_ptr<TemplateEngine> tpl_;
    Router router_;
};

} // namespace ofos

#endif // OFOS_APP_HPP
