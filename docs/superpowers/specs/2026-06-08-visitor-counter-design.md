# Visitor Counter — Design

**Date:** 2026-06-08
**Status:** Approved (design)

## Goal

Add a privacy-friendly visitor counter to the Union-Find Decoder Visualizer
(a static site hosted on GitHub Pages). It shows two totals:

- **Total page views**
- **Unique visitors**

## Approach

Use **GoatCounter** (free, cookieless, privacy-friendly hosted analytics) for
tracking and storage. The site has no backend of its own, and GitHub Pages
cannot store shared state, so a hosted service avoids us running and
maintaining any server or serverless function.

GoatCounter records both pageviews and a cookieless daily-rotating
unique-visitor hash, and exposes a public JSON counter endpoint we fetch to
render the numbers in the page's own visual style.

## Display

- A small badge anchored at the **bottom-right, at the bottom of the control
  panel**.
- **Visible only on the homepage** (the landing / loading-screen state, before
  a graph is loaded). When a graph is loaded the badge hides; when the user
  returns home ("New Graph") it shows again.
- Styled to match the existing dark/translucent panel aesthetic.
- Example rendered text: `👁 1,234 views · 567 visitors`.

## Components

1. **GoatCounter site (external, one-time setup by the user).**
   Register a free site at goatcounter.com to get a site code (e.g.
   `unionfind` → `https://unionfind.goatcounter.com`). In settings, enable the
   public visitor-counter / totals endpoint. This step requires the user's
   account and is a prerequisite for live data.

2. **Tracking snippet** in `web/index.html` — GoatCounter's async `count.js`,
   which records a pageview + unique-visitor hash on each load.

3. **Overlay element** `#visitor-counter` in `web/index.html` — a small badge
   container, initially hidden.

4. **`web/js/visitor-counter.js`** — fetches
   `https://<code>.goatcounter.com/counter/TOTAL.json`, which returns
   `{ "count": "...", "count_unique": "..." }`, formats the numbers
   (thousands separators), and writes them into `#visitor-counter`. The site
   code lives in a single config constant at the top of this module.

5. **CSS** in `web/style.css` — styles `#visitor-counter` (position, colors,
   typography) to match the panel.

## Data Flow

```
Page load
  → count.js pings GoatCounter (records view + visit)
  → visitor-counter.js fetches TOTAL.json
  → { count, count_unique } formatted
  → #visitor-counter populated
Homepage shown  → badge visible
Graph loaded    → badge hidden
Return to home  → badge visible again
```

Show/hide hooks into the existing state transitions in `web/js/main.js`
(graph-loaded path ~lines 33–38; return-home path ~lines 151–157).

## Error Handling

- If the fetch fails or the endpoint is disabled (e.g. before the user
  finishes GoatCounter setup), the badge stays hidden — no broken or zero-value
  UI is shown.
- The tracking snippet failing to load is silent by design.

## Testing

- The format-and-render logic is factored into a pure function so it can be
  exercised with mock JSON: correct number formatting and the hide-on-error
  behavior.
- Full end-to-end verification (real tracking + live totals) is manual, after
  the user completes GoatCounter site setup.

## Scope / YAGNI

- No per-page breakdowns, charts, or historical graphs in the page — those
  live in the GoatCounter dashboard.
- Just the two totals (views + unique visitors).
