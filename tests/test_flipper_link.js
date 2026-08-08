"use strict";

const fs = require("fs");
const path = require("path");

const uiPath = path.join(__dirname, "../esphome/ar01v3_web_v3.js");
const source = fs.readFileSync(uiPath, "utf8");

const requiredGroups = [
  "ESP-RC01 Button Assignment",
  "Flipper File Import",
  "IR Signals",
  "RF 433.92 MHz Signals",
];

if (!source.startsWith("var Qr=Object.defineProperty;")) {
  throw new Error("local ESPHome v3 frontend is missing or has an unexpected format");
}
if (!source.includes('G=Di([Rt("esp-entity-table")],G)')) {
  throw new Error("local frontend does not contain the ESPHome entity table");
}
for (const group of requiredGroups) {
  const occurrences = source.split(`name:"${group}"`).length - 1;
  if (occurrences !== 1) {
    throw new Error(`expected one built-in group definition for ${group}, found ${occurrences}`);
  }
}

const defaults =
  'this.groups=G.ENTITY_CATEGORIES.map((e,n)=>({name:e,sorting_weight:n})),' +
  'this.groups.push({name:G.ENTITY_UNDEFINED,sorting_weight:-1},' +
  '{name:"ESP-RC01 Button Assignment",sorting_weight:3}';
if (!source.includes(defaults)) {
  throw new Error("custom groups are not initialized with the native entity-table defaults");
}
if (source.includes("ar01v3StockUi") || source.includes("patchEntityTableClass")) {
  throw new Error("obsolete asynchronous frontend bootstrap is still present");
}
for (const marker of [
  'i.name==="Flipper Import Page"',
  'window.location.assign("/flipper")',
  's.name!=="Home Assistant Slot Buttons"',
  'style="display:flex;align-items:center;gap:8px;width:100%"',
  'style="flex:1 1 auto;width:auto;min-width:0"',
  'this._slotValuesReady=!1',
  'setTimeout(()=>{this._slotValuesReady=!0,this.requestUpdate()},8e3)',
  'this.entities.filter(s=>a.test(s.name)).length===26',
  '!c&&a.test(o.name)?"":o.state',
]) {
  if (!source.includes(marker)) throw new Error(`missing UI enhancement: ${marker}`);
}
for (const obsolete of ["collectSlotRows", "stabilizeSlotRows", "setValuesVisible", "completeFrames"]) {
  if (source.includes(obsolete)) throw new Error(`obsolete DOM slot scanner remains: ${obsolete}`);
}

console.log("OK: complete ESPHome v3 frontend is embedded locally");
console.log("OK: all four visible custom groups are initialized inside the entity table");
console.log("OK: no asynchronous AR01V3 frontend loader remains");
console.log("OK: Flipper link, IR layout and HA hiding are patched natively");
console.log("OK: slot previews wait for all 26 entities without scanning the DOM");
