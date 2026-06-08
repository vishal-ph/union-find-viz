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
