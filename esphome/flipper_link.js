const FLIPPER_ENTITY = "Flipper Import Page";
const SLOT_ENTITY = /^(?:IR Slot [0-9]|RF Slot (?:[0-9]|1[0-5]))$/;
const SLOT_COUNT = 26;

window.addEventListener(
  "click",
  (event) => {
    const path = event.composedPath();
    const pressed = path.some((node) => {
      if (!(node instanceof Element)) return false;
      return (node.textContent ?? "").trim().toLowerCase() === "press";
    });
    if (!pressed) return;

    // ESPHome Web Server v3 renders each entity as a direct .entity-row with
    // the name in its second cell. Limit the match to that one row; the page
    // root contains all entity names and must never be used for identification.
    const entityRow = path.find(
      (node) => node instanceof Element && node.classList?.contains("entity-row"),
    );
    const entityName = entityRow?.children?.[1]?.textContent?.trim();
    if (entityName !== FLIPPER_ENTITY) return;

    event.preventDefault();
    event.stopImmediatePropagation();
    window.location.assign("/flipper");
  },
  true,
);

// The stock v3 page receives entities one by one and redraws unkeyed rows while
// sorting groups are still being assembled. With equally positioned IR/RF slot
// rows this can briefly reuse an RF value cell in an IR row. Keep only the slot
// value cells hidden until all 26 slot entities have arrived and the layout has
// stayed complete for two animation frames. Other entities remain visible.
if (typeof document !== "undefined" && typeof requestAnimationFrame === "function") {
  const startedAt = performance.now();
  let completeFrames = 0;

  const collectSlotRows = (root, rows) => {
    if (typeof root?.querySelectorAll !== "function") return;
    for (const row of root.querySelectorAll(".entity-row")) {
      const name = row.children?.[1]?.textContent?.trim();
      if (SLOT_ENTITY.test(name ?? "")) rows.push(row);
    }
    for (const element of root.querySelectorAll("*")) {
      if (element.shadowRoot) collectSlotRows(element.shadowRoot, rows);
    }
  };

  const setValuesVisible = (rows, visible) => {
    for (const row of rows) {
      const valueCell = row.children?.[2];
      if (valueCell) valueCell.style.visibility = visible ? "" : "hidden";
    }
  };

  const stabilizeSlotRows = () => {
    const rows = [];
    collectSlotRows(document, rows);
    const complete = rows.length === SLOT_COUNT;
    completeFrames = complete ? completeFrames + 1 : 0;

    if (completeFrames >= 2 || performance.now() - startedAt >= 8000) {
      setValuesVisible(rows, true);
      return;
    }

    setValuesVisible(rows, false);
    requestAnimationFrame(stabilizeSlotRows);
  };

  requestAnimationFrame(stabilizeSlotRows);
}
