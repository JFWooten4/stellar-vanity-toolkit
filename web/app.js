import { ALPHABET, decodeStrKey, VERSION_PUBLIC, VERSION_SEED } from "./strkey.mjs";

const el = (id) => document.getElementById(id);
const form = el("generator-form"), prefixEl = el("prefix"), suffixEl = el("suffix");
const start = el("start"), stop = el("stop"), validation = el("validation");
const result = el("result"), publicField = el("public-key"), secretField = el("secret-key");
const reveal = el("reveal"), copySecret = el("copy-secret");
let worker = null, found = null, secretVisible = false, revealTimer = null;
const MASK = "••••••••••••••••••••••••••••••••••••";

function hideSecret() {
  clearTimeout(revealTimer);
  secretVisible = false;
  secretField.textContent = MASK;
  reveal.textContent = "Reveal";
  copySecret.hidden = true;
}
function discard() {
  hideSecret();
  found = null;
  publicField.textContent = "";
  result.hidden = true;
}
function state(mode, message) {
  el("mode").textContent = mode;
  el("mode").classList.toggle("active", mode === "SEARCHING");
  el("status").textContent = message;
  start.disabled = mode === "SEARCHING";
  stop.disabled = mode !== "SEARCHING";
  prefixEl.disabled = suffixEl.disabled = mode === "SEARCHING";
}
function normalize(value) { return value.trim().toUpperCase(); }
function check(prefix, suffix) {
  if (!prefix && !suffix) return "Enter a prefix, suffix, or both.";
  if (![...prefix, ...suffix].every((char) => ALPHABET.includes(char))) return "Only A–Z and 2–7 are allowed.";
  if (prefix && !"ABCD".includes(prefix[0])) return "A prefix must begin A, B, C, or D (immediately after G).";
  if (prefix.length + suffix.length > 8) return "Use at most eight chosen characters in total; long searches are impractical.";
  return "";
}
function expected(prefix, suffix) {
  const constrained = prefix.length + suffix.length;
  if (!constrained) return "—";
  const estimate = (prefix ? 4 : 1) * Math.pow(32, constrained - (prefix ? 1 : 0));
  return "≈ " + new Intl.NumberFormat("en-US", { notation: "compact", maximumFractionDigits: 1 }).format(estimate) + " tries";
}
function refreshEstimate() {
  const prefix = normalize(prefixEl.value), suffix = normalize(suffixEl.value);
  el("estimate").textContent = expected(prefix, suffix);
  validation.textContent = check(prefix, suffix);
}
function endSearch() {
  if (worker) { worker.terminate(); worker = null; }
  el("progress-bar").style.width = "0";
}
function stopSearch() {
  endSearch();
  state("STOPPED", "Stopped. No secret was retained.");
}
function clearOnExit() { endSearch(); discard(); }
for (const input of [prefixEl, suffixEl]) input.addEventListener("input", refreshEstimate);
form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const prefix = normalize(prefixEl.value), suffix = normalize(suffixEl.value);
  validation.textContent = check(prefix, suffix);
  if (validation.textContent) return;
  if (!window.isSecureContext || !window.crypto?.subtle || !window.Worker) {
    validation.textContent = "HTTPS/localhost, Web Crypto Ed25519, and Web Workers are required.";
    return;
  }
  endSearch();
  discard();
  el("attempts").textContent = "0";
  state("SEARCHING", "Generating and checking addresses on this device…");
  el("progress-bar").style.width = "30%";
  try {
    worker = new Worker("/worker.js", { type: "module" });
    worker.onmessage = ({ data }) => {
      if (data.type === "progress") {
        el("attempts").textContent = data.attempts.toLocaleString("en-US");
      } else if (data.type === "error") {
        endSearch();
        state("ERROR", data.message);
      } else if (data.type === "match") {
        try {
          const publicBytes = decodeStrKey(data.address, VERSION_PUBLIC);
          decodeStrKey(data.secret, VERSION_SEED);
          if (publicBytes.length !== 32 || !data.address.startsWith("G" + prefix) || !data.address.endsWith(suffix)) {
            throw new Error("Generated result failed validation");
          }
          found = { address: data.address, secret: data.secret };
          publicField.textContent = found.address;
          hideSecret();
          result.hidden = false;
          el("attempts").textContent = data.attempts.toLocaleString("en-US");
          endSearch();
          state("MATCH", "Match found. Review and secure the secret offline.");
          result.scrollIntoView({ behavior: "smooth", block: "start" });
        } catch {
          endSearch();
          discard();
          state("ERROR", "Generated key encoding could not be verified.");
        }
      }
    };
    worker.onerror = () => { endSearch(); state("ERROR", "Key generator failed. Check Web Crypto support."); };
    worker.postMessage({ type: "start", prefix, suffix });
  } catch {
    endSearch();
    state("ERROR", "Cannot start a Web Worker here. Try HTTPS or localhost.");
  }
});
stop.addEventListener("click", stopSearch);
el("discard").addEventListener("click", () => { discard(); state("IDLE", "Keypair discarded from the page. This cannot clear screenshots or clipboards."); });
reveal.addEventListener("click", () => {
  if (!found) return;
  if (secretVisible) { hideSecret(); return; }
  secretVisible = true;
  secretField.textContent = found.secret;
  reveal.textContent = "Hide";
  copySecret.hidden = false;
  revealTimer = setTimeout(hideSecret, 30000);
});
async function copy(value, label) {
  if (!value || !navigator.clipboard?.writeText) {
    el("status").textContent = "Clipboard unavailable. Use localhost or HTTPS and browser clipboard permission.";
    return;
  }
  try { await navigator.clipboard.writeText(value); el("status").textContent = label + " copied. Clipboard history may retain it."; }
  catch { el("status").textContent = "Clipboard permission denied; nothing copied."; }
}
el("copy-public").addEventListener("click", () => copy(found?.address, "Public address"));
copySecret.addEventListener("click", () => { if (secretVisible) copy(found?.secret, "Secret seed"); });
document.addEventListener("visibilitychange", () => { if (document.hidden) hideSecret(); });
window.addEventListener("pagehide", clearOnExit);
refreshEstimate();
