-- ============================================================================
-- Online Food Ordering System — relational schema (8 tables)
-- Engine: SQLite 3 (portable, zero-config equivalent of MySQL/MariaDB for the
--         coursework deployment; see docs/DESIGN.md for the MySQL mapping).
-- ============================================================================

-- 1. Administration / staff accounts (back-office login).
CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT    NOT NULL UNIQUE,
    password_hash TEXT    NOT NULL,
    salt          TEXT    NOT NULL,
    full_name     TEXT    NOT NULL DEFAULT '',
    role          TEXT    NOT NULL DEFAULT 'admin',   -- admin | staff
    created_at    TEXT    NOT NULL DEFAULT ''
);

-- 2. Registered customers (storefront login).
CREATE TABLE IF NOT EXISTS customers (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT    NOT NULL,
    email         TEXT    NOT NULL UNIQUE,
    password_hash TEXT    NOT NULL,
    salt          TEXT    NOT NULL,
    phone         TEXT    NOT NULL DEFAULT '',
    address       TEXT    NOT NULL DEFAULT '',
    created_at    TEXT    NOT NULL DEFAULT ''
);

-- 3. Menu categories.
CREATE TABLE IF NOT EXISTS categories (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL,
    description TEXT    NOT NULL DEFAULT '',
    status      INTEGER NOT NULL DEFAULT 1,            -- 1 active, 0 hidden
    created_at  TEXT    NOT NULL DEFAULT ''
);

-- 4. Menu items / products.
CREATE TABLE IF NOT EXISTS products (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    category_id INTEGER NOT NULL,
    name        TEXT    NOT NULL,
    description TEXT    NOT NULL DEFAULT '',
    price       REAL    NOT NULL DEFAULT 0,
    image       TEXT    NOT NULL DEFAULT '',
    status      INTEGER NOT NULL DEFAULT 1,            -- 1 available, 0 off-menu
    created_at  TEXT    NOT NULL DEFAULT '',
    FOREIGN KEY (category_id) REFERENCES categories(id) ON DELETE CASCADE
);

-- 5. Active shopping-cart lines (one per customer+product).
CREATE TABLE IF NOT EXISTS carts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    customer_id INTEGER NOT NULL,
    product_id  INTEGER NOT NULL,
    quantity    INTEGER NOT NULL DEFAULT 1,
    created_at  TEXT    NOT NULL DEFAULT '',
    UNIQUE (customer_id, product_id),
    FOREIGN KEY (customer_id) REFERENCES customers(id) ON DELETE CASCADE,
    FOREIGN KEY (product_id)  REFERENCES products(id)  ON DELETE CASCADE
);

-- 6. Placed orders (order header).
CREATE TABLE IF NOT EXISTS orders (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    order_number   TEXT    NOT NULL UNIQUE,
    customer_id    INTEGER NOT NULL,
    customer_name  TEXT    NOT NULL DEFAULT '',
    phone          TEXT    NOT NULL DEFAULT '',
    address        TEXT    NOT NULL DEFAULT '',
    payment_method TEXT    NOT NULL DEFAULT 'Cash on Delivery',
    total_amount   REAL    NOT NULL DEFAULT 0,
    status         TEXT    NOT NULL DEFAULT 'Pending',  -- Pending|Confirmed|Completed|Cancelled
    note           TEXT    NOT NULL DEFAULT '',
    created_at     TEXT    NOT NULL DEFAULT '',
    FOREIGN KEY (customer_id) REFERENCES customers(id)
);

-- 7. Order line items (order detail; price captured at purchase time).
CREATE TABLE IF NOT EXISTS order_items (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    order_id     INTEGER NOT NULL,
    product_id   INTEGER NOT NULL,
    product_name TEXT    NOT NULL,
    price        REAL    NOT NULL DEFAULT 0,
    quantity     INTEGER NOT NULL DEFAULT 1,
    subtotal     REAL    NOT NULL DEFAULT 0,
    FOREIGN KEY (order_id) REFERENCES orders(id) ON DELETE CASCADE
);

-- 8. Global site settings (single row, id = 1).
CREATE TABLE IF NOT EXISTS settings (
    id            INTEGER PRIMARY KEY CHECK (id = 1),
    system_name   TEXT NOT NULL DEFAULT 'Online Food Ordering System',
    contact_email TEXT NOT NULL DEFAULT '',
    contact_phone TEXT NOT NULL DEFAULT '',
    address       TEXT NOT NULL DEFAULT '',
    cover_image   TEXT NOT NULL DEFAULT '',
    about_us      TEXT NOT NULL DEFAULT '',
    currency      TEXT NOT NULL DEFAULT 'Rs.'
);

CREATE INDEX IF NOT EXISTS idx_products_category ON products(category_id);
CREATE INDEX IF NOT EXISTS idx_carts_customer    ON carts(customer_id);
CREATE INDEX IF NOT EXISTS idx_orders_customer   ON orders(customer_id);
CREATE INDEX IF NOT EXISTS idx_orderitems_order  ON order_items(order_id);
