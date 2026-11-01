// Manual page: table of contents scrolls to a section (without touching the
// ?page= URL) and highlights the section currently being read.
(function () {
  var toc = document.querySelector(".manual-toc");
  if (!toc) return;
  var links = Array.prototype.slice.call(toc.querySelectorAll("a[data-target]"));
  var sections = links.map(function (a) { return document.getElementById(a.getAttribute("data-target")); });
  var locked = false;   // after a click: keep that entry highlighted until the user scrolls

  // bottom of the sticky page header (0 on phones, where the header scrolls away)
  function headerBottom() {
    var top = document.getElementById("top");
    if (!top || getComputedStyle(top).position !== "sticky") return 0;
    return Math.max(0, top.getBoundingClientRect().bottom);
  }

  function setActive(id) {
    links.forEach(function (a) { a.classList.toggle("active", a.getAttribute("data-target") === id); });
  }

  links.forEach(function (a) {
    a.addEventListener("click", function (e) {
      e.preventDefault();
      var target = document.getElementById(a.getAttribute("data-target"));
      if (!target) return;
      locked = true;
      setActive(target.id);
      var y = window.scrollY + target.getBoundingClientRect().top - headerBottom() - 12;
      window.scrollTo({ top: Math.max(0, y), behavior: "smooth" });
    });
  });

  // the active section is the last one whose top has passed just below the header
  function spy() {
    var panel = document.getElementById("panel-home");
    if (locked || (panel && panel.style.display === "none")) return;
    var line = headerBottom() + 80, active = sections[0] && sections[0].id;
    sections.forEach(function (s) { if (s && s.getBoundingClientRect().top <= line) active = s.id; });
    setActive(active);
  }

  var ticking = false;
  window.addEventListener("scroll", function () {
    if (ticking) return;
    ticking = true;
    requestAnimationFrame(function () { ticking = false; spy(); });
  }, { passive: true });
  // scrolling by the user ends the click lock
  ["wheel", "touchstart", "keydown", "mousedown"].forEach(function (ev) {
    window.addEventListener(ev, function (e) {
      if (ev === "mousedown" && toc.contains(e.target)) return;
      locked = false;
    }, { passive: true });
  });
  spy();
})();
