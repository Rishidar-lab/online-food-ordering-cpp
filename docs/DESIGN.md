# Online Food Ordering System — Design Document

This document records the analysis and design of the Online Food Ordering
System: context and data-flow diagrams, use cases, the entity-relationship
model, the class design, the database schema, the 28 test cases, and the
security analysis. Diagrams are written in Mermaid (renders on GitHub and in
most Markdown viewers).

---

## 1. System context

Two actors use the system through a web browser:

- **Customer** — browses the menu, registers/logs in, manages a cart and places
  orders.
- **Administrator / Staff** — logs into a protected back-office to verify and
  confirm orders and to manage the catalogue, users and settings.

The C++ application server handles all requests and persists everything to a
single relational database (SQLite; MySQL-compatible schema).

```mermaid
flowchart LR
    C(["Customer"]) -->|HTTP/AJAX| APP
    A(["Admin / Staff"]) -->|HTTP/AJAX| APP
    APP["Online Food Ordering System\n(C++ application server)"] <-->|SQL| DB[("Relational DB\n8 tables")]
    APP -->|static assets| C
```

---

## 2. Data-flow diagram (Level 0 / context)

```mermaid
flowchart TB
    customer([Customer])
    admin([Administrator])
    subgraph P[0. Online Food Ordering System]
    end
    customer -- registration, login, cart, order --> P
    P -- menu, cart state, order confirmation --> customer
    admin -- login, menu/category/user edits, status changes --> P
    P -- dashboard, order list, reports --> admin
    P <-- records --> store[(Database)]
```

### 2.1 Data-flow diagram (Level 1)

```mermaid
flowchart TB
    customer([Customer])
    admin([Administrator])

    P1[1. Authenticate\n& manage session]
    P2[2. Browse menu\n& view product]
    P3[3. Manage cart]
    P4[4. Place order]
    P5[5. Administer\ncatalogue & orders]

    D1[(users / customers)]
    D2[(categories / products)]
    D3[(carts)]
    D4[(orders / order_items)]
    D5[(settings)]

    customer --> P1 --> D1
    customer --> P2 --> D2
    customer --> P3 --> D3
    P3 --> P4 --> D4
    admin --> P1
    admin --> P5
    P5 --> D2
    P5 --> D4
    P5 --> D1
    P5 --> D5
    D5 --> P2
```

---

## 3. Use-case diagram

```mermaid
flowchart LR
    customer([Customer])
    admin([Administrator / Staff])

    subgraph Storefront
      uc1((Register))
      uc2((Log in / out))
      uc3((Browse & search menu))
      uc4((View product details))
      uc5((Manage cart))
      uc6((Checkout / place order))
      uc7((View my orders))
    end

    subgraph "Back office"
      uc8((View dashboard))
      uc9((Verify & confirm orders))
      uc10((Manage products))
      uc11((Manage categories))
      uc12((Manage users))
      uc13((Edit site settings))
    end

    customer --- uc1
    customer --- uc2
    customer --- uc3
    customer --- uc4
    customer --- uc5
    customer --- uc6
    customer --- uc7

    admin --- uc2
    admin --- uc8
    admin --- uc9
    admin --- uc10
    admin --- uc11
    admin --- uc12
    admin --- uc13
```

---

## 4. Entity-relationship diagram

```mermaid
erDiagram
    CUSTOMERS ||--o{ CARTS       : "has"
    CUSTOMERS ||--o{ ORDERS      : "places"
    CATEGORIES ||--o{ PRODUCTS   : "groups"
    PRODUCTS  ||--o{ CARTS       : "appears in"
    PRODUCTS  ||--o{ ORDER_ITEMS : "sold as"
    ORDERS    ||--o{ ORDER_ITEMS : "contains"
    USERS     ||..|| SETTINGS    : "maintains"

    USERS {
      int    id PK
      string username
      string password_hash
      string salt
      string full_name
      string role
      string created_at
    }
    CUSTOMERS {
      int    id PK
      string name
      string email
      string password_hash
      string salt
      string phone
      string address
      string created_at
    }
    CATEGORIES {
      int    id PK
      string name
      string description
      int    status
      string created_at
    }
    PRODUCTS {
      int    id PK
      int    category_id FK
      string name
      string description
      real   price
      string image
      int    status
      string created_at
    }
    CARTS {
      int    id PK
      int    customer_id FK
      int    product_id FK
      int    quantity
      string created_at
    }
    ORDERS {
      int    id PK
      string order_number
      int    customer_id FK
      string customer_name
      string phone
      string address
      string payment_method
      real   total_amount
      string status
      string note
      string created_at
    }
    ORDER_ITEMS {
      int    id PK
      int    order_id FK
      int    product_id
      string product_name
      real   price
      int    quantity
      real   subtotal
    }
    SETTINGS {
      int    id PK
      string system_name
      string contact_email
      string contact_phone
      string address
      string cover_image
      string about_us
      string currency
    }
```

The **8 tables** are: `users`, `customers`, `categories`, `products`, `carts`,
`orders`, `order_items`, `settings`. Full DDL is in
[`data/schema.sql`](../data/schema.sql).

---

## 5. Class / module design

The server is organised into single-responsibility modules (one header + source
each). The `App` facade owns the long-lived services and all request handlers.

```mermaid
classDiagram
    class App {
      -AppConfig cfg
      -Database db
      -SessionStore sessions
      -TemplateEngine tpl
      -Router router
      +init()
      +run()
      -register_routes()
      -ensure_session()
      -require_admin()
      -render_layout()
      -h_menu() h_cart_add() h_checkout_submit() ...
    }
    class HttpServer {
      +run()
      +stop()
      -handle_client()
      -read_request()
      -parse_multipart()
    }
    class Router {
      +get() post() add()
      +dispatch()
    }
    class Request {
      +method path query form cookies files
      +param() cookie() file()
    }
    class Response {
      +status body
      +html() json() redirect() set_cookie()
      +serialize()
    }
    class Database {
      +query() execute() scalar()
      +transaction()
    }
    class SessionStore {
      +create() value() set() destroy()
    }
    class TemplateEngine {
      +render()
    }
    class Sha256 {
      +update() hex_digest()
    }

    App --> Database
    App --> SessionStore
    App --> TemplateEngine
    App --> Router
    App --> HttpServer
    HttpServer --> Router
    Router --> Request
    Router --> Response
    App ..> Sha256 : password hashing
```

### Request lifecycle

```mermaid
sequenceDiagram
    participant B as Browser
    participant S as HttpServer
    participant R as Router
    participant H as App handler
    participant D as Database
    B->>S: HTTP request (socket)
    S->>S: parse line, headers, cookies, body/multipart
    S->>R: dispatch(Request, Response)
    R->>H: matched handler
    H->>D: parameterised SQL
    D-->>H: rows
    H->>H: render template / build JSON
    H-->>S: Response
    S-->>B: HTTP/1.1 response
```

---

## 6. Test cases (28)

Executed automatically by [`tests/run_tests.sh`](../tests/run_tests.sh)
(`make test`). Each drives the running server over HTTP.

| #  | Area           | Test case                                   | Expected result |
|----|----------------|---------------------------------------------|-----------------|
| 1  | Browsing       | Home page loads                             | HTTP 200 |
| 2  | Browsing       | Home shows configured system name           | name present |
| 3  | Browsing       | Menu page loads                             | HTTP 200 |
| 4  | Browsing       | Menu pagination page size                   | 6 cards/page |
| 5  | Browsing       | Keyword search filters catalogue            | matching item shown |
| 6  | Browsing       | Category filter restricts items             | only that category |
| 7  | Browsing       | Product details AJAX                         | JSON `success:true` |
| 8  | Browsing       | Product AJAX with unknown id                | HTTP 404 |
| 9  | Browsing       | CSS asset served                            | `text/css` |
| 10 | Registration   | Register valid account                      | redirect to `/menu` |
| 11 | Registration   | Duplicate email rejected                    | error message |
| 12 | Registration   | Password mismatch rejected                  | error message |
| 13 | Registration   | Too-short password rejected                 | error message |
| 14 | Authentication | Login with wrong password rejected          | error message |
| 15 | Authentication | Login with correct password                 | redirect to `/menu` |
| 16 | Cart           | Add to cart without login                   | HTTP 401 + `login_required` |
| 17 | Cart           | Authenticated add to cart                   | `cart_count:2` |
| 18 | Cart           | Adding same item merges quantity            | `cart_count:3` |
| 19 | Cart           | Update quantity recomputes subtotal         | `line_subtotal:"360.00"` |
| 20 | Cart           | Remove line updates cart                    | remaining `items:1` |
| 21 | Checkout       | Checkout with missing fields                | HTTP 400 |
| 22 | Checkout       | Checkout places order                       | `order_number:"ORD…"` |
| 23 | Checkout       | Cart emptied after checkout                 | `count:0` |
| 24 | Orders         | My Orders shows a Pending order             | "Pending" |
| 25 | Administration | Admin route requires auth                   | redirect to `/admin/login` |
| 26 | Administration | Admin login succeeds                        | redirect to `/admin` |
| 27 | Administration | Confirm order via AJAX                      | `status:"Confirmed"` |
| 28 | Administration | Create category (CRUD)                      | HTTP 302 (saved) |

---

## 7. Security analysis & future enhancements

**Implemented**
- Passwords: 16-byte random salt + **12 000-round SHA-256**; constant-time
  verify. No plaintext storage.
- **Session access control** on every `/admin/*` route; 256-bit random tokens
  in `HttpOnly`, `SameSite=Lax` cookies; idle TTL with lazy GC.
- **Parameterised SQL** throughout — no string-concatenated queries.
- **Output escaping** by default in the template engine (XSS mitigation);
  JSON control-character escaping.
- Static serving restricted to `/public/` with `..` traversal rejection.
- Image-only upload validation; randomised stored filenames; 8 MB body cap.

**Future enhancements** (as noted in the abstract)
- Online **payment gateway** integration (currently pay-on-delivery).
- **Order notifications** (email / SMS / push) on status changes.
- **Sales reports** and analytics on the dashboard.
- **CSRF tokens** on state-changing POST forms.
- Upgrade password hashing to **bcrypt / Argon2id**.
- HTTPS/TLS termination (via reverse proxy or built-in).
- Role-based granularity for staff vs. administrators.

---

## 8. Mapping the schema to MySQL/MariaDB

The SQL is standard; to target MySQL instead of SQLite, change only:

- `INTEGER PRIMARY KEY AUTOINCREMENT` → `INT AUTO_INCREMENT PRIMARY KEY`
- `REAL` → `DECIMAL(10,2)`
- `TEXT` for short fields → `VARCHAR(n)` (optional)
- Append `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;` to each `CREATE TABLE`
- Replace the SQLite `ON CONFLICT(...) DO UPDATE` upsert in the cart with MySQL's
  `INSERT ... ON DUPLICATE KEY UPDATE`

The application's `Database` class would swap its SQLite backend for the MySQL C
client (`libmysqlclient`) behind the same `query`/`execute`/`transaction`
interface.
