// Headless sweep over simulator.html: eval its <script> against a stubbed
// canvas, drive every page / palette / fault state, and report.
//   node scripts/sim_check.js
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'simulator.html'), 'utf8');
const src = html.slice(html.lastIndexOf('<script>') + 8, html.lastIndexOf('</script>'));

const stubCtx = new Proxy({}, { get: () => () => {} });
const els = {};
const mkEl = () => ({
  innerHTML: '', value: '', textContent: '', dataset: {}, style: {},
  width: 0, height: 0,
  getContext: () => stubCtx,
  addEventListener() {}, setAttribute() {}, closest: () => mkEl(),
});
const document = {
  getElementById: (id) => (els[id] ||= mkEl()),
  querySelectorAll: () => [],
  addEventListener() {},
};
const history = { replaceState() {} };
const location = { search: '' };

const ctxObj = { document, history, location, URLSearchParams, Date, Math, console };
const fn = new Function(...Object.keys(ctxObj), src + '\n;return { S, sweep, stress, render, LOGS: () => LOGS };');
const api = fn(...Object.values(ctxObj));

api.render();
const first = api.LOGS();
const total = first.filter((l) => l[0] === 'pass' || l[0] === 'fail').length;

const bad = api.sweep();
const failures = bad.filter((b) => b.fail.length);
const warnings = bad.filter((b) => b.warn.length);

console.log(`${total} checks per state, swept ${2 * 3 * 3 * 3 * 2 * 5} states`);

// Collapse identical messages: one layout bug should read as one line, not
// once per swept state.
const byMsg = new Map();
for (const b of failures) {
  for (const [, m] of b.fail) {
    if (!byMsg.has(m)) byMsg.set(m, []);
    byMsg.get(m).push(`night=${+b.night} app=${b.app} conn=${b.conn} ap=${b.ap} owned=${+b.owned} theme=${b.theme}`);
  }
}
for (const [m, states] of byMsg) {
  console.log(`\nFAIL  ${m}`);
  console.log(`      in ${states.length}/${2 * 3 * 3 * 3 * 2 * 5} states, e.g. ${states[0]}`);
}
const seen = new Set();
for (const b of warnings) {
  for (const [, m] of b.warn) {
    if (seen.has(m)) continue;
    seen.add(m);
    console.log(`warn  ${m}`);
  }
}
// The truncation path must actually fire, or "0 warnings" only means no label
// happened to overflow -- not that degrading works.
const stressWarns = api.stress();
console.log(`\nstress: ${stressWarns.length} truncation warning(s)`);
for (const m of stressWarns) console.log('      ' + m);
const stressOk = stressWarns.length > 0;
if (!stressOk) console.log('FAIL  textTrunc did not fire on a deliberately overlong value');

if (!failures.length && stressOk)
  console.log(`\nclean — 0 failures, ${seen.size} distinct warning(s) in the sweep, truncation verified`);
process.exit(failures.length || !stressOk ? 1 : 0);
