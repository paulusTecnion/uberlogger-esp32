$(document).ready(function () {
  filebrowserRefresh("/");
  setInterval(renderLogStatus, 1000);
});

function renderLogStatus() {
  populateFields("#log", valuesData);
}

function filebrowserRefresh(filebrowserPath, page = 999) {
  // Add page parameter with default value
  parent = "#filelist";

  console.log(filebrowserPath);

  // Include the page number in the query
  query = "getFileList" + filebrowserPath + "?filepage=" + page;
  let pathmatch = filebrowserPath.match(/(.*[\/])[^\/]+[\/]?$/);
  let parentPath = [];

  if (pathmatch) {
    console.log(pathmatch);
    parentPath = pathmatch[1];
  }

  $.getJSON("./ajax/" + query, (data) => {
    let htmlstring = [];

    if (filebrowserPath == "/") {
      htmlstring += "<b>Current path: (root)</b><br/>";
    } else {
      htmlstring += "<b>Current path: " + filebrowserPath + "</b><br/>";
    }

    htmlstring += "<table width='100%' class=\"compact-table\">";
    htmlstring +=
      "<tr><th width='60%'>Name</th><th width='20%'>Size</th><th width='20%'>Action</th></tr>";

    if (filebrowserPath != "/") {
      htmlstring +=
        "<tr><td><a onClick=\"return filebrowserRefresh('" +
        parentPath +
        "');\" href='javascript:void(0);'>/.. (open parent directory)</a></td><td><i>(directory)</i></td><td></td></tr>";
    }

    htmlstring = buildFileTree(data["root"], htmlstring, 1, filebrowserPath);

    // Close the file list table
    htmlstring += "</table>";

    // Add pagination controls
    if (data.pagination) {
      htmlstring += "<div class='pagination'>";
      for (let i = 1; i <= data.pagination.total_pages; i++) {
        if (i == data.pagination.current_page) {
          htmlstring += `<span class='current-page'>${i}</span>`;
        } else {
          htmlstring += `<a href='javascript:void(0);' onClick='return filebrowserRefresh("${filebrowserPath}", ${i});'>${i}</a>`;
        }
        if (i < data.pagination.total_pages) {
          htmlstring += " | ";
        }
      }
      htmlstring += "</div>";
    }

    // Update the HTML of the file list
    $("#filelist").html(htmlstring);
  }).fail(function (response) {
    // alert(
    //   "Error: could not get list of SD-card files. Reason: " +
    //     response.responseText
    // );
    $("#filelist").html(
      "File browser is not available when logging or when no SD card is inserted."
    );
  });
}

function formatSdcard() {
  if (confirm("Format SD-card, are you sure?") == true) {
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
    let pathmatch = filepath.match(/(.*[\/])[^\/]+[\/]?$/);
    let parentPath = [];

    if (pathmatch) {
      console.log(pathmatch);
      parentPath = pathmatch[1];
    }

    $.ajax({
      method: "POST",
      url: "/delete" + filepath,
      data: "delete",

      success: function (response) {
        if (response == "ack") {
          setTimeout(filebrowserRefresh(parentPath), 1000);
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

function buildFileTree(data, htmlstring, depth, path) {
  $.each(data, function (key, value) {
    if (value["TYPE"] == "FILE") {
      htmlstring += "<tr>";
      htmlstring +=
        "<td style='padding-left: " +
        depth * 10 +
        "px;'>" +
        value["NAME"] +
        "</td>";
      htmlstring +=
        "<td>" +
        (value["SIZE"] / BYTES_PER_MB < 0.001
          ? 0.001
          : value["SIZE"] / BYTES_PER_MB
        ).toFixed(3) +
        " MB</td>";

      htmlstring +=
        "<td><a href='/ajax/getFileList" +
        path +
        value["NAME"] +
        "'>download</a> / <a onClick=\"return promptDelete('" +
        value["NAME"] +
        "', '" +
        path +
        value["NAME"] +
        "');\" href='javascript:void(0);'>delete</a></td>";
      htmlstring += "</tr>";
    } else if (value["TYPE"] == "DIRECTORY") {
      htmlstring += "<tr>";
      htmlstring +=
        "<td style='padding-left: " +
        depth * 10 +
        "px;'><b><i>" +
        value["NAME"] +
        "</i></b></td>";
      htmlstring += "<td><i>(directory)</i></td>";
      htmlstring +=
        "<td><a onClick=\"return filebrowserRefresh('" +
        path +
        value["NAME"] +
        "/');\" href='javascript:void(0);'>open</a></td>";
      htmlstring += "</tr>";

      htmlstring = buildFileTree(
        value,
        htmlstring,
        depth + 1,
        path + value["NAME"] + "/"
      );
    }
  });

  return htmlstring;
}
