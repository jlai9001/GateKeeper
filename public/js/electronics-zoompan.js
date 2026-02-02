// public/js/electronics-zoompan.js
// Zoom (wheel/buttons/pinch) + Pan (drag/touch) for a single large image.

(() => {
  const viewport = document.getElementById("electronicsZoomViewport");
  const img = document.getElementById("electronicsZoomImage");

  // If the electronics page doesn't include the viewer, quietly do nothing.
  if (!viewport || !img) return;

  const btnZoomIn = document.getElementById("electronicsZoomIn");
  const btnZoomOut = document.getElementById("electronicsZoomOut");
  const btnReset = document.getElementById("electronicsZoomReset");

  // Transform state
  let scale = 1;
  let translateX = 0;
  let translateY = 0;

  const MIN_SCALE = 0.2;
  const MAX_SCALE = 1.5;
  const START_ZOOM = 2.2;
  const START_OFFSET_X = 0;   // + moves image right, - moves left
  const START_OFFSET_Y = 270;   // + moves image down,  - moves up

  // Pointer / drag state
  let isDragging = false;
  let lastX = 0;
  let lastY = 0;

  // Touch pinch state
  let pinchActive = false;
  let pinchStartDist = 0;
  let pinchStartScale = 1;
  let pinchCenter = { x: 0, y: 0 };

  function clamp(v, min, max) {
    return Math.max(min, Math.min(max, v));
  }

  function clampPan() {
  const r = getViewportRect();
  const vw = r.width;
  const vh = r.height;

  // scaled image size (in pixels on screen)
  const iw = (img.naturalWidth || img.width) * scale;
  const ih = (img.naturalHeight || img.height) * scale;

  // X bounds:
  // translateX = 0 means image left edge is flush with viewport left edge.
  // translateX = vw - iw means image right edge is flush with viewport right edge.
  if (iw <= vw) {
    // Image narrower than viewport: keep centered, no panning
    translateX = (vw - iw) / 2;
  } else {
    const minX = vw - iw; // most you can drag left
    const maxX = 0;       // most you can drag right
    translateX = clamp(translateX, minX, maxX);
  }

  // Y bounds (same logic):
  if (ih <= vh) {
    translateY = (vh - ih) / 2;
  } else {
    const minY = vh - ih;
    const maxY = 0;
    translateY = clamp(translateY, minY, maxY);
  }
}



  function applyTransform() {
    img.style.transform = `translate(${translateX}px, ${translateY}px) scale(${scale})`;
  }

  function getViewportRect() {
    return viewport.getBoundingClientRect();
  }

  function clientToViewport(clientX, clientY) {
    const r = getViewportRect();
    return { x: clientX - r.left, y: clientY - r.top };
  }

  // Zoom around a point inside the viewport (keeps that point visually stable)
  function zoomAtPoint(newScale, point) {
    newScale = clamp(newScale, MIN_SCALE, MAX_SCALE);

    // Convert "point in viewport" to "point in image local space" before zoom
    const beforeX = (point.x - translateX) / scale;
    const beforeY = (point.y - translateY) / scale;

    // Update scale
    scale = newScale;

    // Recompute translation so that the same local point stays under the cursor
    translateX = point.x - beforeX * scale;
    translateY = point.y - beforeY * scale;

    applyTransform();
    clampPan();
  }

  // Fit image to viewport (contain) and center it
  function fitToViewport() {
    // Wait until image has real dimensions
    const iw = img.naturalWidth || img.width;
    const ih = img.naturalHeight || img.height;
    const r = getViewportRect();
    const vw = r.width;
    const vh = r.height;

    if (!iw || !ih || !vw || !vh) return;

    const fitScale = Math.min(vw / iw, vh / ih) * START_ZOOM;
    scale = clamp(fitScale, MIN_SCALE, MAX_SCALE);

    // Center
    translateX = (vw - iw * scale) / 2 + START_OFFSET_X;
    translateY = (vh - ih * scale) / 2 + START_OFFSET_Y;

    applyTransform();
    clampPan();
  }

  // Ensure image is ready before initial fit
  if (img.complete) {
    fitToViewport();
  } else {
    img.addEventListener("load", fitToViewport, { once: true });
  }

  // Re-fit on resize (keeps it sane on window size changes)
  window.addEventListener("resize", () => {
    // Only auto-fit if user hasn't zoomed way in/out
    // (Feel free to remove this guard if you want always-fit.)
    fitToViewport();
  });

  // Mouse wheel zoom
  viewport.addEventListener(
    "wheel",
    (e) => {
      e.preventDefault();
      const point = clientToViewport(e.clientX, e.clientY);

      // Smooth zoom factor
      const delta = -e.deltaY;
      const zoomFactor = delta > 0 ? 1.12 : 1 / 1.12;

      zoomAtPoint(scale * zoomFactor, point);
    },
    { passive: false }
  );

  // Mouse drag pan
  viewport.addEventListener("mousedown", (e) => {
    isDragging = true;
    lastX = e.clientX;
    lastY = e.clientY;
    viewport.style.cursor = "grabbing";
  });

  window.addEventListener("mousemove", (e) => {
    if (!isDragging) return;
    const dx = e.clientX - lastX;
    const dy = e.clientY - lastY;
    lastX = e.clientX;
    lastY = e.clientY;

    translateX += dx;
    translateY += dy;
    applyTransform();
    clampPan();
  });

  window.addEventListener("mouseup", () => {
    if (!isDragging) return;
    isDragging = false;
    viewport.style.cursor = "grab";
  });

  // Touch: pan + pinch zoom
  viewport.addEventListener(
    "touchstart",
    (e) => {
      if (e.touches.length === 1) {
        pinchActive = false;
        isDragging = true;
        lastX = e.touches[0].clientX;
        lastY = e.touches[0].clientY;
      } else if (e.touches.length === 2) {
        isDragging = false;
        pinchActive = true;

        const t0 = e.touches[0];
        const t1 = e.touches[1];

        const dx = t1.clientX - t0.clientX;
        const dy = t1.clientY - t0.clientY;
        pinchStartDist = Math.hypot(dx, dy);
        pinchStartScale = scale;

        // Pinch center point in viewport coords
        const cx = (t0.clientX + t1.clientX) / 2;
        const cy = (t0.clientY + t1.clientY) / 2;
        pinchCenter = clientToViewport(cx, cy);
      }
    },
    { passive: true }
  );

  viewport.addEventListener(
    "touchmove",
    (e) => {
      if (pinchActive && e.touches.length === 2) {
        e.preventDefault();

        const t0 = e.touches[0];
        const t1 = e.touches[1];
        const dx = t1.clientX - t0.clientX;
        const dy = t1.clientY - t0.clientY;
        const dist = Math.hypot(dx, dy);

        const factor = dist / pinchStartDist;
        zoomAtPoint(pinchStartScale * factor, pinchCenter);
      } else if (isDragging && e.touches.length === 1) {
        e.preventDefault();
        const x = e.touches[0].clientX;
        const y = e.touches[0].clientY;

        const dx = x - lastX;
        const dy = y - lastY;
        lastX = x;
        lastY = y;

        translateX += dx;
        translateY += dy;
        applyTransform();
        clampPan();
      }
    },
    { passive: false }
  );

  viewport.addEventListener("touchend", (e) => {
    if (e.touches.length === 0) {
      isDragging = false;
      pinchActive = false;
    }
    if (e.touches.length === 1) {
      // back to drag mode
      pinchActive = false;
      isDragging = true;
      lastX = e.touches[0].clientX;
      lastY = e.touches[0].clientY;
    }
  });

  // Buttons
  if (btnZoomIn) {
    btnZoomIn.addEventListener("click", () => {
      const r = getViewportRect();
      zoomAtPoint(scale * 1.2, { x: r.width / 2, y: r.height / 2 });
    });
  }

  if (btnZoomOut) {
    btnZoomOut.addEventListener("click", () => {
      const r = getViewportRect();
      zoomAtPoint(scale / 1.2, { x: r.width / 2, y: r.height / 2 });
    });
  }

  if (btnReset) {
    btnReset.addEventListener("click", () => {
      fitToViewport();
    });
  }

  // Nice UX cursor
  viewport.style.cursor = "grab";
})();
