/* Online Food Ordering System — admin behaviour.
   Order verification/confirmation happens over AJAX (no page reload). */
(function () {
  "use strict";

  function toast(msg) {
    var $t = $("#toast");
    $t.text(msg).addClass("show");
    clearTimeout($t.data("timer"));
    $t.data("timer", setTimeout(function () { $t.removeClass("show"); }, 2200));
  }

  function badgeClass(status) { return "status status-" + status.toLowerCase(); }

  window.setStatus = function (id, status, btn) {
    var $btn = $(btn).addClass("disabled");
    $.ajax({ url: "/admin/order/status", method: "POST",
             data: { id: id, status: status }, dataType: "json" })
      .done(function (d) {
        if (!d.success) { toast(d.error || "Error"); return; }
        // Orders table badge
        $("#st-" + id).attr("class", badgeClass(d.status)).text(d.status);
        // Order detail badge
        $("#detail-status").attr("class", badgeClass(d.status)).text(d.status);
        // Hide actions that no longer apply (in the table row)
        var $row = $("#order-" + id);
        if ($row.length) {
          $row.find(".row-actions button").remove();
          if (d.status !== "Completed" && d.status !== "Cancelled") {
            $row.find(".row-actions").prepend(
              '<button class="btn btn-sm" onclick="setStatus(' + id +
              ",'Completed',this)\">Complete</button>");
          }
        }
        toast("Order marked " + d.status);
      })
      .fail(function () { toast("Update failed"); })
      .always(function () { $btn.removeClass("disabled"); });
  };
})();
