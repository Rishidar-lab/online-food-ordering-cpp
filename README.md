# Online Food Ordering System — C++ Edition

[![CI](https://github.com/Rishidar-lab/online-food-ordering-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/Rishidar-lab/online-food-ordering-cpp/actions/workflows/ci.yml)

## Status

**MAINTAIN — academic project, complete for its brief.**

- IMPLEMENTED / TESTED: storefront + admin back-office + relational schema; CI green on `main`.
- NOT CLAIMED: production deployment or security review beyond the coursework scope.

A complete, web-based food-ordering application for a small restaurant: a
customer storefront and a protected administration back-office, backed by a
relational database. It automates the whole manual ordering workflow (phone /
paper slips) described in the project abstract.

The abstract specifies a PHP + MySQL implementation. **This project implements
the same system end-to-end in C++** — the HTTP server, routing, templating,
sessions, password hashing, database access and JSON/AJAX endpoints are all
written from scratch in standard C++17, with SQLite as the embedded relational
engine. Nothing is pseudocode; it compiles, runs, and passes an automated
28-case test suite.

## Deploy (free, hosted)

The app ships as a Docker container, so it runs on any free container host.
One-click deploy to **Koyeb** (free tier stays awake — good for a live demo):

[![Deploy to Koyeb](https://www.koyeb.com/static/images/deploy/button.svg)](https://app.koyeb.com/deploy?type=git&repository=github.com/Rishidar-lab/online-food-ordering-cpp&branch=main&builder=dockerfile&instance_type=free&ports=8080;http;/&name=online-food-ordering-cpp)

Sign in with GitHub, keep the defaults (Dockerfile builder, port `8080`, Free
instance), set the health-check path to `/healthz`, and deploy. You get a public
`https://…koyeb.app` URL — store at `/`, admin at `/admin/login`
(`admin` / `admin123`). Full steps and other hosts (Render, Fly.io, Cloud Run)
are in [`docs/DEPLOY.md`](docs/DEPLOY.md).

> On free tiers the filesystem is ephemeral, so the SQLite database resets on
> each redeploy and the app re-seeds the sample menu + default admin — ideal for
> a demo. **For permanent data, deploy to Fly.io** (Option C in
> [`docs/DEPLOY.md`](docs/DEPLOY.md)): the included `fly.toml` mounts a free
> persistent volume at `/data`, so the database and uploaded images survive
> redeploys (verified).

### Run it live on GitHub (Codespaces)

This is a C++ **server**, so GitHub **Pages cannot run it** (Pages serves only
static files; there is no backend to handle login, cart, checkout or the admin
panel). To run the real app on GitHub's own infrastructure, use **Codespaces**:

1. On the repo page: **Code ▸ Codespaces ▸ Create codespace on main**.
2. The dev container auto-installs the toolchain and compiles. In the terminal:
   ```bash
   make run
   ```
3. Click **Open in Browser** on the forwarded port **8080**. To share the URL
   with others, open the **Ports** tab, right-click port 8080 ▸
   **Port Visibility ▸ Public**.

The codespace runs the full application (storefront + admin). It stays live
while the codespace is running; GitHub's free tier includes monthly codespace
hours for personal accounts.

---

## 1. Feature overview

**Customer site**
- Pictorial menu shown as a **paginated catalogue of cards** (6 per page) with
  category filter and keyword search.
- **Product details in a pop-up panel**, loaded over AJAX (no page reload).
- **Register / log in** with salted, iterated password hashing.
- **Shopping cart**: add items, change quantities, remove lines — all via AJAX
  with a live cart badge and running total.
- **Checkout** form that places the order without reloading; order confirmation.
- **My Orders** history with live status.

**Administration site** (login-protected)
- **Dashboard** with summary counts (orders, pending, products, categories,
  customers) and revenue.
- **Order list** where staff verify and **confirm orders over AJAX**, plus a
  per-order detail view.
- Full **CRUD** for **menu items**, **categories** and **users (admin/staff)**,
  with image upload for products.
- **Site settings**: system name, contact details, cover picture, about-us
  text, currency symbol.

**Platform internals (all hand-written in C++)**
- POSIX-socket HTTP/1.1 server (thread-per-connection).
- Request parser: query string, URL-encoded forms, **multipart/form-data file
  uploads**, cookies.
- Exact-path router with static-file serving.
- File-backed HTML template engine with **automatic HTML escaping** (XSS-safe).
- Server-side **session store** with random 256-bit cookie tokens and idle TTL.
- **SHA-256** implementation (verified against FIPS test vectors) used for
  salted, 12 000-round password hashing.
- Thread-safe SQLite wrapper with parameterised queries (no SQL injection) and
  real transactions for order placement.
- Small JSON emitter for the AJAX API.

---

## 2. Technology mapping (abstract → this build)

| Abstract (PHP build)        | This project (C++ build)                                   |
|-----------------------------|------------------------------------------------------------|
| PHP (server-side language)  | C++17 application + from-scratch HTTP server               |
| MySQL / MariaDB             | SQLite 3 (embedded, zero-config; identical relational SQL) |
| Apache/Nginx                | Built-in socket server                                     |
| HTML5 / CSS3 / Bootstrap    | HTML5 templates + own stylesheet (+ Bootstrap 5 via CDN)   |
| jQuery / AJAX               | jQuery via CDN; AJAX endpoints return JSON                 |
| Salted password hashing     | Salt + 12 000-round SHA-256                                |
| Session-based admin login   | Cookie session store                                       |

> **Why SQLite instead of MySQL?** It is a drop-in *relational* engine that
> needs no separate server or configuration, so the project builds and runs
> anywhere with a single command — ideal for grading and demos. The schema is
> standard SQL; `docs/DESIGN.md` lists the one-line changes to target MySQL.

---

## 3. Requirements

- Linux (or any POSIX system) with a C++17 compiler.
- `g++` (tested on 13.3), `make`.
- `libsqlite3-dev` (headers + library).

Install the one dependency on Debian/Ubuntu:

```bash
sudo apt-get install -y g++ make libsqlite3-dev
```

---

## 4. Build & run

```bash
make            # compiles to ./ofos
make run        # builds and serves on http://127.0.0.1:8080
```

Then open **http://127.0.0.1:8080**.

- Storefront: `/`
- Admin panel: `/admin/login`

On first launch the database is created and **seeded** automatically with a
sample menu (4 categories, 12 products), default site settings, and a default
administrator:

```
username: admin
password: admin123
```

Other options:

```bash
./ofos --host 0.0.0.0 --port 9000 --dir . --db data/ofos.db
make fresh      # wipe the database and re-seed, then run
make clean      # remove build artefacts
```

---

## 5. Tests

A self-contained black-box suite drives the running server over HTTP and
checks **28 cases** across registration, browsing, cart, checkout and
administration:

```bash
make test
```

It starts its own server instance on a throwaway database, runs the 28 cases,
tears everything down, and exits non-zero if any fail. See
`docs/DESIGN.md` §6 for the full case list.

---

## 6. Project layout

```
food-ordering-cpp/
├── Makefile              Build / run / test targets
├── README.md
├── include/              C++ headers (one per module)
│   ├── sha256.hpp  utils.hpp  http.hpp  json.hpp  db.hpp
│   ├── router.hpp  server.hpp  session.hpp  templates.hpp  app.hpp
├── src/                  C++ sources
│   ├── sha256.cpp  utils.cpp  http.cpp  db.cpp  router.cpp
│   ├── server.cpp  session.cpp  templates.cpp  main.cpp
│   └── app.cpp  app_public.cpp  app_auth.cpp  app_cart.cpp  app_admin.cpp
├── views/                HTML templates (storefront + views/admin/)
├── public/               Static assets served at /public/*
│   ├── css/style.css  js/app.js  js/admin.js  img/*.svg  uploads/
├── data/
│   ├── schema.sql        The 8-table relational schema
│   └── ofos.db           SQLite database (created on first run)
├── tests/run_tests.sh    28-case end-to-end suite
└── docs/DESIGN.md        DFD, use-case, ER & class diagrams, tests, security
```

---

## 7. Security measures (implemented)

- Passwords stored as **salt + 12 000-round SHA-256**, never in plain text;
  verification uses a constant-time comparison.
- **Session-based access control** guards every `/admin/*` route; session
  tokens are 256-bit random values in `HttpOnly`, `SameSite=Lax` cookies.
- **Parameterised SQL** everywhere — user input is never concatenated into
  queries.
- **Automatic HTML escaping** in the template engine prevents stored/reflected
  XSS; the JSON emitter escapes control characters.
- Static serving is confined to `/public/` with path-traversal rejection.
- Uploads are restricted to image types and stored with randomised names.
- Request-body size cap (8 MB) and socket timeouts.

See `docs/DESIGN.md` §7 for planned enhancements (payment gateway,
notifications, sales reports, CSRF tokens, bcrypt/argon2).
