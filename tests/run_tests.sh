#!/usr/bin/env bash
# ===========================================================================
# Online Food Ordering System — end-to-end black-box test suite (28 cases).
# Starts a dedicated server instance on a throwaway database, exercises every
# major path over HTTP (as a real browser would), then tears everything down.
# Exit code 0 iff all 28 cases pass.
# ===========================================================================
set -u
cd "$(dirname "$0")/.." || exit 2

PORT=8099
HOST=127.0.0.1
BASE="http://$HOST:$PORT"
DB="data/ofos_test.db"
CJAR="$(mktemp)"       # customer cookie jar
AJAR="$(mktemp)"       # admin cookie jar
PASS=0; FAIL=0; N=0

rm -f "$DB" "$DB-wal" "$DB-shm"

# Build if needed.
make --no-print-directory >/dev/null 2>&1 || { echo "build failed"; exit 2; }

./ofos --host "$HOST" --port "$PORT" --dir . --db "$DB" >/tmp/ofos_test.log 2>&1 &
SRV=$!
cleanup() { kill "$SRV" 2>/dev/null; wait "$SRV" 2>/dev/null; rm -f "$DB" "$DB-wal" "$DB-shm" "$CJAR" "$AJAR"; }
trap cleanup EXIT

# Wait for the server to accept connections.
for i in $(seq 1 50); do
  code=$(curl -s -o /dev/null -w "%{http_code}" "$BASE/" 2>/dev/null)
  [ "$code" = "200" ] && break
  sleep 0.1
done

# check <description> <actual> <expected-substring>
check() {
  N=$((N+1))
  local desc="$1" actual="$2" expect="$3"
  if printf '%s' "$actual" | grep -qF -- "$expect"; then
    printf "  \033[32mPASS\033[0m [%02d] %s\n" "$N" "$desc"; PASS=$((PASS+1))
  else
    printf "  \033[31mFAIL\033[0m [%02d] %s\n"   "$N" "$desc"
    printf "        expected to contain: %s\n        got: %s\n" "$expect" "$(printf '%s' "$actual" | head -c 160)"
    FAIL=$((FAIL+1))
  fi
}
code()  { curl -s -o /dev/null -w "%{http_code}" "$@"; }
ctype() { curl -s -o /dev/null -w "%{content_type}" "$@"; }
redir() { curl -s -o /dev/null -w "%{redirect_url}" "$@"; }

echo "Running 28 test cases against $BASE"
echo "-- Browsing & catalogue --------------------------------------------------"
check "Home page returns 200"                "$(code $BASE/)"                              "200"
check "Home shows configured system name"    "$(curl -s $BASE/)"                           "Tasty Bites Online"
check "Menu page returns 200"                "$(code $BASE/menu)"                          "200"
check "Menu paginates at 6 cards/page"       "$(curl -s "$BASE/menu?page=1" | grep -o 'class=\"card\"' | wc -l | tr -d ' ')" "6"
check "Search filters the catalogue"         "$(curl -s "$BASE/menu?q=Margherita")"        "Margherita Pizza"
check "Category filter restricts items"      "$(curl -s "$BASE/menu?category=2")"          "Pizza"
check "Product AJAX returns JSON details"    "$(curl -s "$BASE/product?id=1")"             '"success":true'
check "Product AJAX 404s unknown id"         "$(code "$BASE/product?id=99999")"            "404"
check "CSS served as text/css"               "$(ctype $BASE/public/css/style.css)"         "text/css"

echo "-- Registration & authentication ----------------------------------------"
check "Register valid account -> /menu"      "$(redir -c $CJAR -b $CJAR -X POST $BASE/register -d 'name=Test User&email=user@test.com&password=secret1&confirm=secret1&phone=900&address=Addr')" "/menu"
check "Duplicate email is rejected"          "$(curl -s -X POST $BASE/register -d 'name=Dup&email=user@test.com&password=secret1&confirm=secret1')" "already exists"
check "Password mismatch is rejected"        "$(curl -s -X POST $BASE/register -d 'name=X&email=x@test.com&password=secret1&confirm=secret2')" "do not match"
check "Short password is rejected"           "$(curl -s -X POST $BASE/register -d 'name=X&email=y@test.com&password=123&confirm=123')" "at least 6"
check "Login wrong password is rejected"     "$(curl -s -X POST $BASE/login -d 'email=user@test.com&password=nope')" "Invalid email or password"
check "Login correct credentials -> /menu"   "$(redir -c $CJAR -b $CJAR -X POST $BASE/login -d 'email=user@test.com&password=secret1&next=')" "/menu"

echo "-- Cart (AJAX) -----------------------------------------------------------"
check "Add to cart requires login (401)"     "$(code -X POST $BASE/cart/add -d 'product_id=1&quantity=1')" "401"
check "Authenticated add to cart succeeds"   "$(curl -s -b $CJAR -c $CJAR -X POST $BASE/cart/add -d 'product_id=1&quantity=2')" '"cart_count":2'
check "Adding same item merges quantity"     "$(curl -s -b $CJAR -c $CJAR -X POST $BASE/cart/add -d 'product_id=1&quantity=1')" '"cart_count":3'
# setup (uncounted): add a second product so the remove test is meaningful
curl -s -b $CJAR -c $CJAR -X POST $BASE/cart/add -d 'product_id=4&quantity=1' >/dev/null
check "Update quantity recomputes subtotal"  "$(curl -s -b $CJAR -c $CJAR -X POST $BASE/cart/update -d 'product_id=1&quantity=3')" '"line_subtotal":"360.00"'
check "Remove line updates cart"             "$(curl -s -b $CJAR -c $CJAR -X POST $BASE/cart/remove -d 'product_id=4')" '"items":1'

echo "-- Checkout & orders -----------------------------------------------------"
check "Checkout missing fields -> 400"       "$(code -b $CJAR -c $CJAR -X POST $BASE/checkout -d 'name=&phone=&address=')" "400"
check "Checkout places order (order no.)"    "$(curl -s -b $CJAR -c $CJAR -X POST $BASE/checkout -d 'name=Test User&phone=900&address=Addr&payment_method=Cash on Delivery')" '"order_number":"ORD'
check "Cart emptied after checkout"          "$(curl -s -b $CJAR -c $CJAR $BASE/cart/count)" '"count":0'
check "My Orders shows a Pending order"      "$(curl -s -b $CJAR -c $CJAR $BASE/orders)"   "Pending"

echo "-- Administration --------------------------------------------------------"
check "Admin area requires auth (redirect)"  "$(redir $BASE/admin)"                        "/admin/login"
check "Admin login succeeds"                 "$(redir -c $AJAR -b $AJAR -X POST $BASE/admin/login -d 'username=admin&password=admin123')" "/admin"
check "Confirm order via AJAX"               "$(curl -s -b $AJAR -c $AJAR -X POST $BASE/admin/order/status -d 'id=1&status=Confirmed')" '"status":"Confirmed"'
check "Create category (CRUD)"               "$(code -b $AJAR -c $AJAR -X POST $BASE/admin/category/save -d 'name=Wraps&description=x')" "302"

echo "-------------------------------------------------------------------------"
echo "Total: $N   Passed: $PASS   Failed: $FAIL"
[ "$FAIL" -eq 0 ] && echo "ALL TESTS PASSED" || echo "SOME TESTS FAILED"
exit $([ "$FAIL" -eq 0 ] && echo 0 || echo 1)
