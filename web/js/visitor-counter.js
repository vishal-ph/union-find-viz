// Visitor counter backed by GoatCounter (https://www.goatcounter.com/help/visitor-counter).
// Tracking itself is done by the count.js snippet in index.html; this module only
// fetches and renders the public totals.

const GOATCOUNTER_CODE = 'union-find-viz';
const TOTALS_URL = `https://${GOATCOUNTER_CODE}.goatcounter.com/counter/TOTAL.json`;

let loaded = false;     // did we successfully fetch totals?
let onHomepage = true;  // is the landing screen currently showing?

function toInt(value) {
  const digits = String(value ?? '').replace(/[^0-9]/g, '');
  return digits === '' ? 0 : Number(digits);
}

// Pure: { count, count_unique } -> display string. Robust to numbers or
// pre-formatted strings ("12,345"), and to missing fields.
export function formatCounterText(data) {
  const views = toInt(data && data.count);
  const visitors = toInt(data && data.count_unique);
  return `\u{1F441} ${views.toLocaleString('en-US')} views · ${visitors.toLocaleString('en-US')} visitors`;
}

// Fetch totals. fetchFn injectable for testing; defaults to global fetch.
export async function fetchCounts(fetchFn = fetch) {
  const res = await fetchFn(TOTALS_URL);
  if (!res.ok) throw new Error(`GoatCounter totals request failed: ${res.status}`);
  return res.json();
}

function applyVisibility() {
  const el = document.getElementById('visitor-counter');
  if (!el) return;
  if (loaded && onHomepage) el.classList.remove('hidden');
  else el.classList.add('hidden');
}

// Called by main.js when switching between homepage and graph view.
export function setHomepageVisible(visible) {
  onHomepage = visible;
  applyVisibility();
}

// Fetch totals once and render. Stays hidden on any failure.
export async function initVisitorCounter() {
  const el = document.getElementById('visitor-counter');
  if (!el) return;
  try {
    const data = await fetchCounts();
    el.textContent = formatCounterText(data);
    loaded = true;
  } catch (err) {
    loaded = false;
    console.warn('Visitor counter unavailable:', err);
  }
  applyVisibility();
}
