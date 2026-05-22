// fwupdate.js — Firmware update SPA page logic
// Runs in the SPA context: jQuery ($) and generic.js globals (fwUpdateInProgress) available.

(function () {
  "use strict";

  // ── Constants ─────────────────────────────────────────────────────────────
  var MAX_FILE_BYTES = 1200 * 1024; // 1.17 MB — matches backend limit

  // File descriptor table
  var FILES = [
    {
      id:       "fw-input-main",
      expected: "ota_main.bin",
      rowId:    "fw-row-main",
      chosenId: "fw-chosen-main",
      iconId:   "fw-icon-main",
      stepId:   "fw-step-main",
      pctId:    "fw-pct-main"
    },
    {
      id:       "fw-input-support",
      expected: "ota_support.bin",
      rowId:    "fw-row-support",
      chosenId: "fw-chosen-support",
      iconId:   "fw-icon-support",
      stepId:   "fw-step-support",
      pctId:    "fw-pct-support"
    },
    {
      id:       "fw-input-filesystem",
      expected: "ota_filesystem.bin",
      rowId:    "fw-row-filesystem",
      chosenId: "fw-chosen-filesystem",
      iconId:   "fw-icon-filesystem",
      stepId:   "fw-step-filesystem",
      pctId:    "fw-pct-filesystem"
    }
  ];

  // ── Init ──────────────────────────────────────────────────────────────────
  $(document).ready(function () {
    bindFileInputs();
    $("#fw-btn-upload").on("click", onUploadClick);
  });

  // ── File input binding & validation ──────────────────────────────────────

  function bindFileInputs() {
    $.each(FILES, function (i, f) {
      $("#" + f.id).on("change", function () {
        onFileChosen(f, this);
      });
    });
  }

  function onFileChosen(f, input) {
    var $row  = $("#" + f.rowId);
    var $name = $("#" + f.chosenId);
    var $icon = $("#" + f.iconId);

    if (!input.files || !input.files.length) {
      $name.text("No file chosen");
      $icon.text("");
      $row.removeClass("fw-row-ok fw-row-bad");
      updateUploadButton();
      return;
    }

    var file = input.files[0];
    $name.text(file.name);

    if (file.name !== f.expected) {
      $icon.text("\u2716");
      $row.removeClass("fw-row-ok").addClass("fw-row-bad");
      showError("Wrong file for \u201c" + f.expected + "\u201d: got \u201c" + file.name + "\u201d.");
    } else if (file.size > MAX_FILE_BYTES) {
      $icon.text("\u2716");
      $row.removeClass("fw-row-ok").addClass("fw-row-bad");
      showError(f.expected + " is too large (maximum 1.2 MB).");
    } else {
      $icon.text("\u2714");
      $row.removeClass("fw-row-bad").addClass("fw-row-ok");
      // Clear error only if all remaining rows are OK
      if (allFilesValid()) { clearError(); }
    }

    updateUploadButton();
  }

  function allFilesValid() {
    var ok = true;
    $.each(FILES, function (i, f) {
      var input = document.getElementById(f.id);
      if (!input || !input.files || !input.files.length) { ok = false; return false; }
      var file = input.files[0];
      if (file.name !== f.expected || file.size > MAX_FILE_BYTES) { ok = false; return false; }
    });
    return ok;
  }

  function updateUploadButton() {
    $("#fw-btn-upload").prop("disabled", !allFilesValid());
  }

  // Extract plain text from an HTML error response body (ESP-IDF wraps errors in HTML)
  function extractErrorText(html) {
    var tmp = document.createElement("div");
    tmp.innerHTML = html;
    return (tmp.textContent || tmp.innerText || "").trim() || html;
  }

  // ── Error / phase helpers ─────────────────────────────────────────────────

  function showError(msg) {
    $("#fwupdate-error").text(msg).show();
  }

  function clearError() {
    $("#fwupdate-error").hide().text("");
  }

  function showPhase(name) {
    $("#fwupdate-phase-select").hide();
    $("#fwupdate-phase-progress").hide();
    $("#fwupdate-phase-done").hide();
    if (name === "select")   { $("#fwupdate-phase-select").show(); }
    if (name === "progress") { $("#fwupdate-phase-progress").show(); }
    if (name === "done")     { $("#fwupdate-phase-done").show(); }
  }

  function showDonePhase(isError) {
    fwUpdateInProgress = false;
    showPhase("done");

    if (isError) {
      $("#fw-done-title").text("Update failed");
      $("#fw-done-msg").hide();
      $("#fw-done-error").show();
    } else {
      $("#fw-done-title").text("Update complete");
      $("#fw-done-error").hide();
      $("#fw-done-msg").show();
    }
  }

  // ── Main upload click ─────────────────────────────────────────────────────

  function onUploadClick() {
    if (!allFilesValid()) { return; }
    if (!confirm(
      "You are about to flash your UberLogger.\n\n" +
      "Do NOT power off the device during the update.\n\n" +
      "Continue?"
    )) { return; }

    // Stop the background getValues poller immediately.
    // The ESP32 HTTP server has a very limited number of simultaneous connections
    // (CONFIG_LWIP_MAX_SOCKETS=10 → 7 HTTP slots). If getValues keeps opening
    // connections during the upload, LRU purge can close the upload connection
    // mid-transfer, causing ERR_CONNECTION_RESET.
    if (window.valuesInterval) {
      clearInterval(window.valuesInterval);
      window.valuesInterval = null;
    }

    // Persist in-progress flag so it survives page navigation.
    sessionStorage.setItem('fwFlashInProgress', '1');

    // Suppress the getValues "could not update values" alert during flashing
    fwUpdateInProgress = true;

    clearError();
    showPhase("progress");
    $("#fw-upload-label").text("Enabling firmware update mode...");

    enableFWUpdate()
      .then(uploadAllFiles)
      .then(startUpgrade)
      .then(function () {
        // Files uploaded + upgrade triggered — HTTP server is now stopped on device.
        // Switch to reconnect polling instead of state polling.
        $("#fw-upload-section").hide();
        $("#fw-flash-section").show();
        $("#fw-upload-bar").css("width", "100%");
        beginReconnectPolling();
      })
      .catch(function (err) {
        sessionStorage.removeItem('fwFlashInProgress');
        showPhase("select");
        showError(String(err) || "An unknown error occurred.");
        fwUpdateInProgress = false;
        // Restart the getValues poller that was stopped above
        if (!window.valuesInterval) {
          window.valuesInterval = setInterval(function () {
            if (sessionStorage.getItem('fwFlashInProgress')) return;
            getValues();
          }, 1000);
        }
      });
  }

  // ── Step 1: enable firmware update mode ──────────────────────────────────

  function enableFWUpdate() {
    return new Promise(function (resolve, reject) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/fwupdate/enable", true);
      xhr.timeout = 10000;

      xhr.onload = function () {
        if (xhr.status === 200) {
          resolve();
        } else if (xhr.status === 403) {
          reject("Cannot start update: the logger is busy. Stop logging first.");
        } else {
          reject("Failed to enable firmware update (HTTP\u00a0" + xhr.status + ").");
        }
      };
      xhr.onerror   = function () { reject("Network error while enabling firmware update."); };
      xhr.ontimeout = function () { reject("Timeout while enabling firmware update."); };
      xhr.send();
    });
  }

  // ── Step 2: upload all three files sequentially ───────────────────────────

  function uploadAllFiles() {
    // Build a sequential Promise chain; each file waits for the previous to complete.
    var chain = Promise.resolve();
    $.each(FILES, function (i, f) {
      chain = chain.then(function () { return uploadOneFile(f, i); });
    });
    return chain;
  }

  function uploadOneFile(f, index) {
    return new Promise(function (resolve, reject) {
      var file = document.getElementById(f.id).files[0];
      var xhr  = new XMLHttpRequest();

      xhr.open("POST", "/upload/" + f.expected, true);
      // Do NOT set Content-Type manually. With a File object, the browser sends
      // application/octet-stream with a correct Content-Length header. Setting
      // multipart/form-data (without a required boundary) can cause some browsers
      // to omit Content-Length, which makes the server see content_len=0 and
      // close the connection while the body is still being sent → ERR_CONNECTION_RESET.
      xhr.timeout = 120000; // 2 min per file

      // Mark this step as active
      $("#" + f.stepId).addClass("fw-step-active");
      $("#fw-upload-label").text("Uploading " + f.expected + "...");

      // Per-file upload progress events
      xhr.upload.onprogress = function (evt) {
        if (!evt.lengthComputable) { return; }
        var filePct = evt.loaded / evt.total;
        $("#" + f.pctId).text(Math.round(filePct * 100) + "%");
        $("#fw-upload-bar").css("width", Math.round(filePct * 100) + "%");
      };

      xhr.onload = function () {
        if (xhr.status === 200 || xhr.status === 0) {
          $("#" + f.stepId).removeClass("fw-step-active").addClass("fw-step-done");
          $("#" + f.pctId).text("100%");
          $("#fw-upload-bar").css("width", "0%");
          resolve();
        } else {
          $("#" + f.stepId).removeClass("fw-step-active").addClass("fw-step-error");
          var serverMsg = xhr.responseText ? extractErrorText(xhr.responseText) : "";
          reject(
            "Upload of " + f.expected + " failed" +
            (serverMsg ? ": " + serverMsg : " (HTTP\u00a0" + xhr.status + "). Reset the device and try again.")
          );
        }
      };

      xhr.onerror = function () {
        $("#" + f.stepId).removeClass("fw-step-active").addClass("fw-step-error");
        reject("Network error uploading " + f.expected + ". Reset the device and try again.");
      };

      xhr.ontimeout = function () {
        $("#" + f.stepId).removeClass("fw-step-active").addClass("fw-step-error");
        reject("Upload of " + f.expected + " timed out. Reset the device and try again.");
      };

      xhr.send(file);
    });
  }

  // ── Step 3: trigger firmware flash ───────────────────────────────────────

  function startUpgrade() {
    return new Promise(function (resolve) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/fwupdate/startupgrade", true);
      xhr.timeout = 15000;

      // The HTTP server will be stopped on the device shortly after this request.
      // Treat both onload and onerror/ontimeout as success.
      xhr.onload    = function () { resolve(); };
      xhr.onerror   = function () { resolve(); }; // connection reset when server stops
      xhr.ontimeout = function () { resolve(); }; // server stopped before response
      xhr.send();
    });
  }

  // ── Step 4: reconnect polling — wait for device to come back online ───────

  function beginReconnectPolling() {
    var start    = Date.now();
    var deadline = start + 120000; // 2 min timeout
    var done     = false;

    // Tick the elapsed-time display every second
    var clockTimer = setInterval(function () {
      if (done) { clearInterval(clockTimer); return; }
      var remaining = Math.max(0, Math.ceil((deadline - Date.now()) / 1000));
      $("#fw-flash-timer").text("The green LED flashes while updating. Your PC may disconnect from the UberLogger Wi-Fi \u2014 reconnect manually if needed. Waiting... " + remaining + "s");
    }, 1000);

    var pollTimer = setInterval(function () {
      if (Date.now() > deadline) {
        done = true;
        clearInterval(pollTimer);
        clearInterval(clockTimer);
        sessionStorage.removeItem('fwFlashInProgress');
        showPhase("select");
        showError("Update timed out. When the green LED stops flashing, the update is complete. Your PC may have disconnected from the UberLogger Wi-Fi \u2014 reconnect manually and reload this page.");
        fwUpdateInProgress = false;
        return;
      }
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/", true);
      xhr.timeout = 2000;
      xhr.onload = function () {
        done = true;
        clearInterval(pollTimer);
        clearInterval(clockTimer);
        sessionStorage.removeItem('fwFlashInProgress');
        fwUpdateInProgress = false;
        showDonePhase(false);
      };
      xhr.send();
    }, 3000);
  }

})(); // end IIFE
