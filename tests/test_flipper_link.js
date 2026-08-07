"use strict";

let clickHandler;
let redirectedTo = null;
let visibleRows = [];
const animationFrames = [];

global.Element = class Element {
  constructor(textContent = "", className = "", children = []) {
    this.textContent = textContent;
    this.children = children;
    this.style = {};
    this.classList = {
      contains: (candidate) => className.split(/\s+/).includes(candidate),
    };
  }
};

global.document = {
  querySelectorAll(selector) {
    return selector === ".entity-row" ? visibleRows : [];
  },
};
global.requestAnimationFrame = (callback) => {
  animationFrames.push(callback);
};

global.window = {
  addEventListener(type, handler, capture) {
    if (type !== "click" || capture !== true) throw new Error("wrong listener registration");
    clickHandler = handler;
  },
  location: {
    assign(path) {
      redirectedTo = path;
    },
  },
};

require("../esphome/flipper_link.js");

function slotRow(name) {
  return new Element(`${name} state`, "entity-row", [
    new Element(""),
    new Element(name),
    new Element("state"),
  ]);
}

visibleRows = [slotRow("IR Slot 0"), slotRow("RF Slot 0")];
animationFrames.shift()();
if (visibleRows.some((row) => row.children[2].style.visibility !== "hidden")) {
  throw new Error("incomplete slot values were not hidden");
}

visibleRows = [
  ...Array.from({ length: 10 }, (_, slot) => slotRow(`IR Slot ${slot}`)),
  ...Array.from({ length: 16 }, (_, slot) => slotRow(`RF Slot ${slot}`)),
];
animationFrames.shift()();
if (visibleRows.some((row) => row.children[2].style.visibility !== "hidden")) {
  throw new Error("slot values were revealed before the layout stabilized");
}
animationFrames.shift()();
if (visibleRows.some((row) => row.children[2].style.visibility !== "")) {
  throw new Error("complete slot values were not revealed");
}

function click(path) {
  let prevented = false;
  let stopped = false;
  clickHandler({
    composedPath: () => path,
    preventDefault: () => {
      prevented = true;
    },
    stopImmediatePropagation: () => {
      stopped = true;
    },
  });
  return { prevented, stopped };
}

const appRoot = new Element("Flipper Import Page PRESS IR Clear PRESS RF Clear PRESS");
const ordinaryRow = new Element("RF Clear PRESS", "entity-row", [
  new Element(""),
  new Element("RF Clear"),
  new Element("PRESS"),
]);
const ordinary = click([
  new Element("PRESS"),
  new Element("PRESS"),
  ordinaryRow,
  appRoot,
]);
if (redirectedTo !== null || ordinary.prevented || ordinary.stopped) {
  throw new Error("ordinary PRESS button was intercepted");
}

const flipperRow = new Element("Flipper Import Page PRESS", "entity-row", [
  new Element(""),
  new Element("Flipper Import Page"),
  new Element("PRESS"),
]);
const flipper = click([
  new Element("PRESS"),
  new Element("PRESS"),
  flipperRow,
  appRoot,
]);
if (redirectedTo !== "/flipper" || !flipper.prevented || !flipper.stopped) {
  throw new Error("Flipper PRESS button did not redirect exclusively");
}

console.log("OK: Flipper link intercepts only its own PRESS button");
console.log("OK: slot values stay hidden until all 26 rows are stable");
