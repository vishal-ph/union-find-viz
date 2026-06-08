# Visitor Counter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a privacy-friendly visitor counter (total page views + unique visitors) to the GitHub Pages static site, tracked via GoatCounter and shown as a badge at the bottom of the right-side panel, visible only on the homepage.

**Architecture:** GoatCounter's async `count.js` records views + cookieless unique-visitor hashes. A small ES module fetches GoatCounter's public `TOTAL.json` counter endpoint and renders the two numbers into a `#visitor-counter` element inside `#panel`. The pure format/fetch logic is dependency-injected so it can be unit-tested with Node's built-in test runner. Show/hide is wired into the existing homepage <-> graph state transitions in `main.js`.

**Tech Stack:** Vanilla ES modules, GoatCounter (hosted), Node built-in `node --test` for unit tests. No build step, no new dependencies.

**GoatCounter site code:** `union-find-viz`
- Tracking script src: `//gc.zgo.at/count.js`, `data-goatcounter="https://union-find-viz.goatcounter.com/count"`
- Totals endpoint: `https://union-find-viz.goatcounter.com/counter/TOTAL.json` → `{ "count": "...", "count_unique": "..." }`

---

## File Structure

- **Create** `web/js/visitor-counter.js` — module: config constant, `formatCounterText()`, `fetchCounts()`, visibility helpers, `initVisitorCounter()`.
- **Create** `web/js/visitor-counter.test.js` — Node unit tests for the pure logic (format + fetch success/error).
- **Modify** `web/index.html` — add GoatCounter tracking snippet in `<head>`/end of body, and the `#visitor-counter` element as the last child of `#panel`.
- **Modify** `web/style.css` — styles for `#visitor-counter` and its `.hidden` rule.
- **Modify** `web/js/main.js` — import + call `initVisitorCounter()` at startup; toggle homepage visibility in the graph-loaded and return-home paths.

---

## Task 1: Visitor counter module + unit tests (pure logic)

**Files:**
- Create: `web/js/visitor-counter.js`
- Test: `web/js/visitor-counter.test.js`

- [ ] **Step 1: Write the failing tests**

Create `web/js/visitor-counter.test.js`:

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { formatCounterText, fetchCounts } from './visitor-counter.js';

test('formatCounterText formats plain integer fields', () => {
  const out = formatCounterText({ count: 1234, count_unique: 567 });
  assert.equal(out, '\u{1F441} 1,234 views · 567 visitors');
});

test('formatCounterText strips separators already present in strings', () => {
  const out = formatCounterText({ count: '12,345', count_unique: '1,234' });
  assert.equal(out, '\u{1F441} 12,345 views · 1,234 visitors');
});

test('formatCounterText treats missing/garbage fields as zero', () => {
  const out = formatCounterText({});
  assert.equal(out, '\u{1F441} 0 views · 0 visitors');
});

test('fetchCounts returns parsed JSON on ok response', async () => {
  const fakeFetch = async () => ({
    ok: true,
    json: async () => ({ count: '10', count_unique: '3' }),
  });
  const data = await fetchCounts(fakeFetch);
  assert.deepEqual(data, { count: '10', count_unique: '3' });
});

test('fetchCounts throws on non-ok response', async () => {
  const fakeFetch = async () => ({ ok: false, status: 503, json: async () => ({}) });
  await assert.rejects(() => fetchCounts(fakeFetch), /503/);
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `node --test web/js/visitor-counter.test.js`
Expected: FAIL — cannot resolve module `./visitor-counter.js` (file does not exist yet).

- [ ] **Step 3: Write the module**

Create `web/js/visitor-counter.js`:

```js
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
```

Note: `document`/`fetch` are referenced only inside functions, so importing this module in Node (for the test) does not execute any browser-only code.

- [ ] **Step 4: Run tests to verify they pass**

Run: `node --test web/js/visitor-counter.test.js`
Expected: PASS — 5 tests passing.

- [ ] **Step 5: Commit**

```bash
git add web/js/visitor-counter.js web/js/visitor-counter.test.js
git commit -m "feat: add visitor counter module with format/fetch unit tests"
```

---

## Task 2: HTML — tracking snippet + counter element

**Files:**
- Modify: `web/index.html`

- [ ] **Step 1: Add the counter element as the last child of `#panel`**

In `web/index.html`, immediately after the closing `</div>` of `legend-section` (the `<div id="legend-section" ...> ... </div>` block) and before the `</div>` that closes `#panel`, insert:

```html
            <!-- Visitor counter (homepage only) -->
            <div id="visitor-counter" class="hidden" aria-live="polite"></div>
```

- [ ] **Step 2: Add the GoatCounter tracking snippet**

In `web/index.html`, just before the closing `</body>` tag (after the two existing `<script>` tags), add:

```html
    <!-- GoatCounter analytics: records pageviews + cookieless unique visitors -->
    <script data-goatcounter="https://union-find-viz.goatcounter.com/count"
            async src="//gc.zgo.at/count.js"></script>
```

- [ ] **Step 3: Verify in a browser (manual sanity check)**

Run: `python3 -m http.server 8000 --directory web` then open `http://localhost:8000`.
Expected: Page loads with no console errors from the snippet; the `#visitor-counter` div exists in the DOM (still empty/hidden — wiring comes in Task 4). GoatCounter dashboard may record the visit.

- [ ] **Step 4: Commit**

```bash
git add web/index.html
git commit -m "feat: add GoatCounter snippet and visitor-counter element"
```

---

## Task 3: CSS — style the badge

**Files:**
- Modify: `web/style.css`

- [ ] **Step 1: Add styles at the end of `web/style.css`**

```css
/* Visitor counter — anchored to the bottom of the panel, homepage only */
#visitor-counter {
    margin-top: auto;          /* push to bottom of the flex-column panel */
    padding-top: 12px;
    font-size: 11px;
    letter-spacing: 0.02em;
    color: var(--text-muted, #8888aa);
    opacity: 0.75;
    text-align: center;
    user-select: none;
}

#visitor-counter.hidden {
    display: none;
}
```

If `--text-muted` is not defined in `:root`, the `#8888aa` fallback (matching the spanning-tree legend color already used in the page) applies.

- [ ] **Step 2: Commit**

```bash
git add web/style.css
git commit -m "feat: style visitor counter badge"
```

---

## Task 4: Wire into main.js (init + homepage show/hide)

**Files:**
- Modify: `web/js/main.js` (imports at top; `onGraphLoaded` ~lines 33–38; return-home function ~lines 151–157; startup init)

- [ ] **Step 1: Add the import**

At the top of `web/js/main.js`, after the existing imports (the `import { Interaction } from './interaction.js';` line), add:

```js
import { initVisitorCounter, setHomepageVisible } from './visitor-counter.js';
```

- [ ] **Step 2: Hide the counter when a graph loads**

In `onGraphLoaded`, in the block that hides the loading screen and shows panel sections (the lines calling `document.getElementById('loading-screen').classList.add('hidden');` and the following `...classList.remove('hidden')` calls), add at the end of that block:

```js
    setHomepageVisible(false);
```

- [ ] **Step 3: Show the counter when returning to the homepage**

In the return-home function (the one that calls `document.getElementById('loading-screen').classList.remove('hidden');` and re-hides the panel sections, ~lines 151–157), add at the end of that block:

```js
    setHomepageVisible(true);
```

- [ ] **Step 4: Initialize the counter at startup**

Find where the app bootstraps at the bottom of `main.js` (e.g. the top-level call that wires up DOM listeners / runs `initWasm()` — look for the `DOMContentLoaded` listener or the trailing init call). Add a call to `initVisitorCounter();` there so it runs once on page load. It is intentionally not awaited — the counter loads independently of the WASM/graph setup. Example, if there is a `DOMContentLoaded` handler:

```js
    initVisitorCounter();
```

If bootstrap runs at module top level instead, add `initVisitorCounter();` as the last top-level statement.

- [ ] **Step 5: Manual verification in browser**

Run: `python3 -m http.server 8000 --directory web` then open `http://localhost:8000`.
Expected:
- On the landing screen, the badge appears at the bottom of the right panel showing `👁 N views · M visitors` (real numbers from GoatCounter once tracking has registered at least one visit).
- Load a preset graph → badge disappears.
- Click "← New Graph" → badge reappears.
- If GoatCounter totals are not yet available, the badge stays hidden with only a `console.warn` (no broken UI).

- [ ] **Step 6: Commit**

```bash
git add web/js/main.js
git commit -m "feat: wire visitor counter into homepage state transitions"
```

---

## Task 5: Final verification

- [ ] **Step 1: Re-run unit tests**

Run: `node --test web/js/visitor-counter.test.js`
Expected: PASS — 5 tests.

- [ ] **Step 2: Full manual pass**

Confirm the four behaviors from Task 4 Step 5 in the browser, and that the GoatCounter dashboard at `https://union-find-viz.goatcounter.com` records the visit.

---

## Notes / Assumptions

- The GoatCounter site `union-find-viz` exists and its public visitor-counter/totals endpoint is enabled (done by the user). If totals are disabled, `TOTAL.json` returns non-200 and the badge stays hidden by design.
- No `package.json` is added; tests run via the Node built-in runner. If the project later adds an npm test script, add `node --test web/js/*.test.js`.
- The exact line numbers in `main.js` are approximate; locate the described blocks by their `getElementById('loading-screen')` calls rather than by line number.
