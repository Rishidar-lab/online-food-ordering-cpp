/* Online Food Ordering System — storefront behaviour (jQuery + AJAX).
   All cart / checkout actions use AJAX so the page never reloads. */
(function () {
  "use strict";

  var currentProduct = null;

  function toast(msg) {
    var $t = $("#toast");
    $t.text(msg).addClass("show");
    clearTimeout($t.data("timer"));
    $t.data("timer", setTimeout(function () { $t.removeClass("show"); }, 2200));
  }

  function setBadge(n) { $("#cart-badge").text(n); }

  function money(v) { return (parseFloat(v) || 0).toFixed(2); }

  /* ---- product details pop-up ------------------------------------------ */
  window.viewProduct = function (id) {
    $.ajax({ url: "/product", method: "GET", data: { id: id }, dataType: "json" })
      .done(function (d) {
        if (!d.success) { toast(d.error || "Not found"); return; }
        currentProduct = d;
        $("#pm-img").attr("src", d.image).attr("alt", d.name);
        $("#pm-cat").text(d.category);
        $("#pm-name").text(d.name);
        $("#pm-price").text(d.currency + " " + money(d.price));
        $("#pm-desc").text(d.description);
        $("#pm-qty").val(1);
        $("#product-modal").addClass("open").attr("aria-hidden", "false");
      })
      .fail(function () { toast("Could not load item"); });
  };

  window.closeModal = function () {
    $("#product-modal").removeClass("open").attr("aria-hidden", "true");
  };

  window.pmQty = function (delta) {
    var el = $("#pm-qty");
    var q = Math.max(1, Math.min(99, (parseInt(el.val(), 10) || 1) + delta));
    el.val(q);
  };

  // Close modal on overlay click or Escape.
  $(document).on("click", "#product-modal", function (e) {
    if (e.target === this) closeModal();
  });
  $(document).on("keydown", function (e) { if (e.key === "Escape") closeModal(); });

  $("#pm-add").on("click", function () {
    if (!currentProduct) return;
    doAdd(currentProduct.id, parseInt($("#pm-qty").val(), 10) || 1, this);
  });

  /* ---- add to cart ----------------------------------------------------- */
  window.addToCart = function (id, btn) { doAdd(id, 1, btn); };

  function doAdd(id, qty, btn) {
    var $btn = $(btn);
    $btn.addClass("disabled");
    $.ajax({
      url: "/cart/add", method: "POST",
      data: { product_id: id, quantity: qty }, dataType: "json"
    }).done(function (d) {
      if (d.success) {
        setBadge(d.cart_count);
        toast(d.message || "Added to cart");
        closeModal();
      } else {
        toast(d.error || "Could not add");
      }
    }).fail(function (xhr) {
      var d = xhr.responseJSON;
      if (d && d.redirect) { window.location = d.redirect; return; }
      toast("Please log in to order");
    }).always(function () { $btn.removeClass("disabled"); });
  }

  /* ---- cart page: quantity & remove ------------------------------------ */
  window.changeQty = function (id, delta) {
    var el = $("#qty-" + id);
    var q = (parseInt(el.val(), 10) || 1) + delta;
    if (q < 1) { window.removeItem(id); return; }
    $.ajax({ url: "/cart/update", method: "POST",
             data: { product_id: id, quantity: q }, dataType: "json" })
      .done(function (d) {
        if (!d.success) { toast(d.error || "Error"); return; }
        el.val(q);
        var $sub = $("#sub-" + id);
        var cur = ($sub.text().trim().split(/\s+/)[0]) || "";  // keep currency prefix
        $sub.text(cur + " " + d.line_subtotal);
        $("#cart-total").text(money(d.total));
        setBadge(d.cart_count);
      });
  };

  window.removeItem = function (id) {
    $.ajax({ url: "/cart/remove", method: "POST",
             data: { product_id: id }, dataType: "json" })
      .done(function (d) {
        if (!d.success) return;
        $("tr[data-id='" + id + "']").fadeOut(150, function () {
          $(this).remove();
          if (d.items === 0) {
            $("#cart-body").html('<tr><td colspan="5" class="empty">Your cart is empty. <a href="/menu">Browse the menu</a>.</td></tr>');
            $("#checkout-btn").addClass("disabled");
          }
        });
        $("#cart-total").text(money(d.total));
        setBadge(d.cart_count);
        toast("Item removed");
      });
  };

  /* ---- checkout -------------------------------------------------------- */
  $("#checkout-form").on("submit", function (e) {
    e.preventDefault();
    var $btn = $("#place-btn").addClass("disabled").text("Placing order…");
    $("#checkout-error").text("");
    $.ajax({ url: "/checkout", method: "POST", data: $(this).serialize(), dataType: "json" })
      .done(function (d) {
        if (d.success) { window.location = d.redirect; }
        else { $("#checkout-error").text(d.error || "Could not place order"); }
      })
      .fail(function (xhr) {
        var d = xhr.responseJSON;
        if (d && d.redirect) { window.location = d.redirect; return; }
        $("#checkout-error").text((d && d.error) || "Could not place order");
      })
      .always(function () { $btn.removeClass("disabled").text("Place order"); });
  });

  /* ---- order placed confirmation --------------------------------------- */
  $(function () {
    var $placed = $("#placed-alert");
    if ($placed.length && $placed.data("order")) {
      $placed.text("Order " + $placed.data("order") + " placed successfully. Thank you!");
    }
  });
})();
