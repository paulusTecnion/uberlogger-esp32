$(document).ready(function () {
  filebrowserRefresh("/");
  setInterval(renderLogStatus, 1000);
});

var currentFilePath = "/";

var FILE_ICONS = {
  file: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/></svg>',
  folder: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/></svg>',
  download: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 4v11m0 0l-4.5-4.5M12 15l4.5-4.5M5 20h14"/></svg>',
  trash: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 7h16M10 11v6M14 11v6M6 7l1 12a2 2 0 0 0 2 2h6a2 2 0 0 0 2-2l1-12M9 7V4h6v3"/></svg>',
};

function renderLogStatus() {
  populateFields("#log", valuesData);
}

function escapeAttr(s) {
  return String(s).replace(/[&<>"']/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]; });
}

// Size in readable units (the list reports bytes).
function formatFileSize(bytes) {
  var b = Number(bytes) || 0;
  if (b < 1024) return b + " B";
  if (b < 1024 * 1024) return (b / 1024).toFixed(0) + " KB";
  if (b < 1024 * 1024 * 1024) return (b / 1024 / 1024).toFixed(1) + " MB";
  return (b / 1024 / 1024 / 1024).toFixed(2) + " GB";
}

// A value inside a '...' JavaScript string in an onclick attribute: escape for JS, then for HTML.
function jsArg(s) {
  return escapeAttr(String(s).replace(/\\/g, "\\\\").replace(/'/g, "\\x27"));
}

// URL-encode each part of a path ("/a b/x#1.csv" -> "/a%20b/x%231.csv"); the logger decodes it.
function encodePath(path) {
  return String(path).split("/").map(encodeURIComponent).join("/");
}

function parentOf(path) {
  var m = String(path).match(/(.*[\/])[^\/]+[\/]?$/);
  return m ? m[1] : "/";
}

// "/" -> "SD card", "/logs/old/" -> "SD card / logs / old" (each part clickable)
function renderBreadcrumb(path) {
  var parts = path.split("/").filter(function (p) { return p; });
  var html = path === "/" ? "<b>SD card</b>" : '<a href="javascript:void(0)" onclick="filebrowserRefresh(\'/\')">SD card</a>';
  var acc = "/";
  parts.forEach(function (p, i) {
    acc += p + "/";
    html += ' <span class="muted">/</span> ' + (i === parts.length - 1 ? "<b>" + escapeAttr(p) + "</b>"
      : '<a href="javascript:void(0)" onclick="filebrowserRefresh(\'' + jsArg(acc) + '\')">' + escapeAttr(p) + "</a>");
  });
  $("#file-path").html(html);
}

function filebrowserRefresh(filebrowserPath, page = 999) {
  if (!filebrowserPath) filebrowserPath = "/";
  currentFilePath = filebrowserPath;
  renderBreadcrumb(filebrowserPath);

  $.getJSON("./ajax/getFileList" + encodePath(filebrowserPath) + "?filepage=" + page, (data) => {
    var rows = buildFileTree(data["root"], "", filebrowserPath);
    if (filebrowserPath !== "/") {
      rows = '<tr class="file-row folder"><td class="file-name" colspan="3"><a class="fname" href="javascript:void(0)" onclick="filebrowserRefresh(\'' +
        jsArg(parentOf(filebrowserPath)) + '\')">' + FILE_ICONS.folder + "<span>.. (up one folder)</span></a></td></tr>" + rows;
    }
    var html = rows
      ? '<table class="file-table"><thead><tr><th>Name</th><th class="num">Size</th><th class="actions"></th></tr></thead><tbody>' + rows + "</tbody></table>"
      : '<div class="files-empty">No files on the SD card.</div>';

    var pg = data.pagination;
    if (pg && pg.total_pages > 1) {
      html += '<div class="pagination">';
      for (let i = 1; i <= pg.total_pages; i++) {
        html += i == pg.current_page
          ? '<span class="current-page">' + i + "</span>"
          : '<a href="javascript:void(0)" onclick="return filebrowserRefresh(\'' + jsArg(filebrowserPath) + "', " + i + ')">' + i + "</a>";
      }
      html += "</div>";
    }
    $("#filelist").removeClass("muted").html(html);
  }).fail(function () {
    $("#filelist").html('<div class="files-empty">The file list is not available while logging or when no SD card is inserted.</div>');
  });
}

function formatSdcard() {
  if (confirm("Format the SD card? This erases ALL files on it.") == true) {
    $.getJSON("./ajax/formatSdcard", (data) => {
      // to do: implement a proper response sequence
      alert(
        "SD-card now formatting. Please wait, this will take up to 30 seconds."
      );
    }).fail(function () {
      alert("Error: could not format SD-card. Are you logging?");
      console.log("Data query failed. NOK.");
    });

    return true;
  } else {
    return false;
  }
}

function promptDelete(file, filepath) {
  if (confirm("Delete file " + file + ". Are you sure?") == true) {
    var parentPath = parentOf(filepath);

    $.ajax({
      method: "POST",
      url: "/delete" + encodePath(filepath),
      data: "delete",

      success: function (response) {
        if (response == "ack") {
          setTimeout(function () { filebrowserRefresh(parentPath); }, 1000);
        } else {
          alert("Error: could not delete file.");
          console.log("Failed, response=" + response["responseText"]);
        }
      },

      error: function (response) {
        alert(
          "Error: could not delete file, response=" + response["responseText"]
        );
        console.log("Failed, response=" + response["responseText"]);
      },
    });

    return true;
  } else {
    return false;
  }
}

// One table row per file; folders open on click (their contents are listed when opened).
function buildFileTree(data, html, path) {
  $.each(data, function (key, value) {
    var name = value["NAME"], full = path + name, a = escapeAttr;
    if (value["TYPE"] == "FILE") {
      html += '<tr class="file-row">' +
        '<td class="file-name"><span class="fname">' + FILE_ICONS.file + "<span>" + a(name) + "</span></span></td>" +
        '<td class="num">' + formatFileSize(value["SIZE"]) + "</td>" +
        '<td class="actions">' +
        '<a class="icon-action" href="/ajax/getFileList' + a(encodePath(full)) + '" download="' + a(name) + '" title="Download ' + a(name) + '">' + FILE_ICONS.download + "</a>" +
        '<a class="icon-action danger" href="javascript:void(0)" title="Delete ' + a(name) + '" onclick="return promptDelete(\'' + jsArg(name) + "', '" + jsArg(full) + '\')">' + FILE_ICONS.trash + "</a>" +
        "</td></tr>";
    } else if (value["TYPE"] == "DIRECTORY") {
      html += '<tr class="file-row folder">' +
        '<td class="file-name" colspan="2"><a class="fname" href="javascript:void(0)" onclick="return filebrowserRefresh(\'' + jsArg(full) + '/\')">' +
        FILE_ICONS.folder + "<span>" + a(name) + "</span></a></td>" +
        '<td class="actions"><a class="icon-action" href="javascript:void(0)" title="Open folder" onclick="return filebrowserRefresh(\'' + jsArg(full) + '/\')">&rsaquo;</a></td>' +
        "</tr>";
    }
  });
  return html;
}
