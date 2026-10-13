#include "app.hpp"
#include "utils.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

// Online Food Ordering System — C++ edition.
// Usage: ofos [--host H] [--port N] [--dir PATH] [--db FILE]
int main(int argc, char** argv) {
    ofos::AppConfig cfg;

    // Sensible environment defaults (handy for containers / scripts).
    if (const char* e = std::getenv("OFOS_HOST")) cfg.host = e;
    if (const char* e = std::getenv("OFOS_PORT")) cfg.port = static_cast<int>(ofos::util::to_long(e, cfg.port));
    if (const char* e = std::getenv("OFOS_DIR"))  cfg.base_dir = e;
    if (const char* e = std::getenv("OFOS_DB"))   cfg.db_path = e;
    // PaaS platforms (Render, Cloud Run, Koyeb, Fly, Heroku…) inject $PORT and
    // expect the app to bind it on 0.0.0.0. This takes precedence.
    if (const char* e = std::getenv("PORT"))      cfg.port = static_cast<int>(ofos::util::to_long(e, cfg.port));

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--host") cfg.host = next("--host");
        else if (a == "--port") cfg.port = static_cast<int>(ofos::util::to_long(next("--port"), cfg.port));
        else if (a == "--dir") cfg.base_dir = next("--dir");
        else if (a == "--db") cfg.db_path = next("--db");
        else if (a == "--help" || a == "-h") {
            std::cout << "Online Food Ordering System (C++)\n"
                      << "  --host H   bind address (default 127.0.0.1)\n"
                      << "  --port N   port (default 8080)\n"
                      << "  --dir P    project dir holding views/ public/ data/ (default .)\n"
                      << "  --db F     sqlite database file (default data/ofos.db)\n";
            return 0;
        } else {
            std::cerr << "unknown argument: " << a << " (try --help)\n";
            return 2;
        }
    }

    // If db_path is relative, resolve it under the project dir.
    if (!cfg.db_path.empty() && cfg.db_path[0] != '/') {
        cfg.db_path = cfg.base_dir + "/" + cfg.db_path;
    }

    try {
        ofos::App app(cfg);
        app.init();
        if (!app.run()) {
            std::cerr << "[fatal] server failed to start (port in use?)\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[fatal] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
