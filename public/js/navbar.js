document.addEventListener("DOMContentLoaded", () => {
  const toggleBtn = document.querySelector(".dropdown-toggle");
  const menu = document.querySelector(".dropdown-menu");

  if (!toggleBtn || !menu) return;

  const openMenu = () => {
    menu.classList.add("is-open");
    toggleBtn.setAttribute("aria-expanded", "true");
  };

  const closeMenu = () => {
    menu.classList.remove("is-open");
    toggleBtn.setAttribute("aria-expanded", "false");
  };

  toggleBtn.addEventListener("click", (e) => {
    e.stopPropagation();
    const isOpen = menu.classList.contains("is-open");
    if (isOpen) closeMenu();
    else openMenu();
  });

  // Close when clicking outside
  document.addEventListener("click", () => closeMenu());

  // Close on ESC
  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") closeMenu();
  });

  // Prevent inside clicks from closing immediately
  menu.addEventListener("click", (e) => e.stopPropagation());
});
