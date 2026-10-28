// Smoke test for the browser build in headless Chromium (Playwright).
//
//   node web/smoke/smoke.cjs <url> <out-dir> [--phone]
//
// Checks: the page starts with no page errors and shows How to play on a first
// visit; the canvas draws the cabin (not one flat colour); walking moves the
// player (W on a desktop, the stick on the phone profile) and looking turns
// them; a tool can be taken (a number key, or a tap on the tool bar); a save
// survives a reload (IndexedDB). Writes PNGs to <out-dir>.
'use strict';
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { chromium, devices } = require('playwright');

const [url, outDir] = process.argv.slice(2);
const phone = process.argv.includes('--phone');
if (!url || !outDir) {
  console.error('usage: smoke.cjs <url> <out-dir> [--phone]');
  process.exit(2);
}
fs.mkdirSync(outDir, { recursive: true });

// Minimal PNG decoder (8-bit RGB/RGBA, non-interlaced: what Chromium writes).
function decodePng(buf) {
  let pos = 8, width = 0, height = 0, channels = 0;
  const idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos);
    const type = buf.toString('ascii', pos + 4, pos + 8);
    const data = buf.subarray(pos + 8, pos + 8 + len);
    if (type === 'IHDR') {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      channels = { 2: 3, 6: 4 }[data[9]];
      if (data[8] !== 8 || !channels || data[12] !== 0) throw new Error('unsupported PNG');
    } else if (type === 'IDAT') idat.push(data);
    pos += 12 + len;
  }
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = width * channels;
  const out = Buffer.alloc(height * stride);
  for (let y = 0; y < height; y++) {
    const f = raw[y * (stride + 1)];
    for (let x = 0; x < stride; x++) {
      const v = raw[y * (stride + 1) + 1 + x];
      const a = x >= channels ? out[y * stride + x - channels] : 0;
      const b = y > 0 ? out[(y - 1) * stride + x] : 0;
      const c = x >= channels && y > 0 ? out[(y - 1) * stride + x - channels] : 0;
      let p;
      if (f === 0) p = 0;
      else if (f === 1) p = a;
      else if (f === 2) p = b;
      else if (f === 3) p = (a + b) >> 1;
      else {
        const pa = Math.abs(b - c), pb = Math.abs(a - c), pc = Math.abs(a + b - 2 * c);
        p = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      out[y * stride + x] = (v + p) & 255;
    }
  }
  return { width, height, channels, data: out };
}

// Distinct colours (quantised to 4 bits per channel) in a PNG.
function colourCount(png) {
  const img = decodePng(png);
  const seen = new Set();
  for (let i = 0; i < img.data.length; i += img.channels)
    seen.add(((img.data[i] >> 4) << 8) | ((img.data[i + 1] >> 4) << 4) | (img.data[i + 2] >> 4));
  return seen.size;
}

(async () => {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const base = phone ? devices['iPhone 13'] : { viewport: { width: 1280, height: 800 } };
  // "headless" in the UA keeps arcade from counting a smoke run as a play.
  const context = await browser.newContext({ ...base, userAgent: (base.userAgent || 'Mozilla/5.0') + ' kindling-smoke headless' });
  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  const fail = async (why) => {
    console.error('FAIL: ' + why);
    await page.screenshot({ path: path.join(outDir, 'failure.png') }).catch(() => {});
    await browser.close();
    process.exit(1);
  };

  // The main loop keeps the load event from ever firing: wait for commit only.
  const open = async () => {
    await page.goto(url, { waitUntil: 'commit' });
    try {
      await page.waitForFunction(() => window.kindling && window.kindlingStatus, null, { timeout: 90000 });
    } catch (e) {
      await fail('the game never became ready: ' + errors.join(' | '));
    }
  };
  await open();
  const status = async () => JSON.parse(await page.evaluate(() => window.kindling.status()));
  // Input is handled on the next frame, and SwiftShader frames are slow: poll.
  const until = async (pred, ms = 20000) => {
    const end = Date.now() + ms;
    let st = await status();
    while (!pred(st) && Date.now() < end) {
      await page.waitForTimeout(200);
      st = await status();
    }
    return st;
  };

  if (!(await page.locator('#help').isVisible())) await fail('How to play was not shown on a first visit');
  await page.locator('#helpClose').click();
  let s = await status();
  console.log(`ready: step ${s.step}, tool ${s.tool}, at (${s.x}, ${s.z})`);
  if (s.step !== 'Survey') await fail('a first visit should start at Survey, not ' + s.step);
  if (s.tools.length !== 10) await fail('expected 10 tools, got ' + s.tools.length);

  await page.waitForTimeout(1500);
  const shot = await page.screenshot({ path: path.join(outDir, 'kindling.png'), timeout: 180000 });
  const colours = colourCount(shot);
  console.log(`screenshot: ${colours} distinct colours`);
  if (colours < 12) await fail('the canvas looks blank (' + colours + ' colours)');

  const start = { x: s.x, z: s.z, yaw: s.yaw };
  const box = await page.locator('#canvas').boundingBox();
  if (phone) {
    if (!(await page.locator('#panel').isVisible())) await fail('the checklist panel is not shown on a narrow page');
    const panel = await page.locator('#panel').innerText();
    if (!/STEP: Survey/.test(panel)) await fail('the checklist panel does not show the step: ' + panel);
    // Walk: hold the stick forward on the left half.
    const cdp = await context.newCDPSession(page);
    const sx = box.x + box.width * 0.2, sy = box.y + box.height * 0.6;
    const touch = (type, pts) => cdp.send('Input.dispatchTouchEvent', { type, touchPoints: pts });
    await touch('touchStart', [{ x: sx, y: sy, id: 1 }]);
    for (let i = 1; i <= 6; i++) await touch('touchMove', [{ x: sx, y: sy - 10 * i, id: 1 }]);
    s = await until((st) => Math.hypot(st.x - start.x, st.z - start.z) > 0.3);
    await touch('touchEnd', []);
    console.log(`after the stick: at (${s.x}, ${s.z})`);
    if (Math.hypot(s.x - start.x, s.z - start.z) <= 0.3) await fail('the stick did not walk the player');
    // Look: drag on the right half.
    const lx = box.x + box.width * 0.75, ly = box.y + box.height * 0.4;
    await touch('touchStart', [{ x: lx, y: ly, id: 2 }]);
    for (let i = 1; i <= 5; i++) await touch('touchMove', [{ x: lx + 12 * i, y: ly, id: 2 }]);
    await touch('touchEnd', []);
    s = await until((st) => Math.abs(st.yaw - start.yaw) > 0.05);
    if (Math.abs(s.yaw - start.yaw) <= 0.05) await fail('a drag on the right half did not turn the view');
    // A tool from the tool bar (after the drag's touch has ended, or the tap
    // reads as a second finger and makes no click).
    await page.waitForTimeout(400);
    await page.locator('#tools [data-tool="1"]').tap();
    s = await until((st) => st.tool === 1);
    if (s.tool !== 1) await fail('tapping the Broom did not take it: tool ' + s.tool);
    await page.locator('#work').dispatchEvent('pointerdown');
    await page.waitForTimeout(500);
    await page.locator('#work').dispatchEvent('pointerup');
  } else {
    await page.keyboard.down('KeyW');
    s = await until((st) => Math.hypot(st.x - start.x, st.z - start.z) > 0.3);
    await page.keyboard.up('KeyW');
    console.log(`after W: at (${s.x}, ${s.z})`);
    if (Math.hypot(s.x - start.x, s.z - start.z) <= 0.3) await fail('W did not walk the player');
    await page.keyboard.press('Digit2');
    s = await until((st) => st.tool === 2);
    if (s.tool !== 2) await fail('2 did not take the scrub brush: tool ' + s.tool);
    // Closing How to play took the pointer lock: the mouse now looks.
    s = await until((st) => st.captured, 10000);
    if (!s.captured) await fail('the pointer lock was not taken when How to play closed');
    // Headless Chromium fakes movement under the lock (it jumps to the cursor
    // and back), which the page drops as spikes: look through the same call
    // the page's mouse handler makes.
    await page.evaluate(() => window.kindling.look(200, 0));
    s = await until((st) => Math.abs(st.yaw - start.yaw) > 0.1);
    console.log(`after looking: yaw ${start.yaw} -> ${s.yaw}`);
    if (Math.abs(s.yaw - start.yaw) <= 0.1) await fail('looking did not turn the view');
    // Esc's job in a browser: give the mouse back, as the page's buttons need it.
    await page.evaluate(() => document.exitPointerLock());
    s = await until((st) => !st.captured, 10000);
    if (s.captured) await fail('releasing the pointer lock did not reach the game');
    if (!(await page.locator('#clickToPlay').isVisible())) await fail('"Click to play" is not shown without the lock');
  }
  await page.screenshot({ path: path.join(outDir, 'kindling-moved.png'), timeout: 180000 });

  // Save, reload, and the player is where they were.
  await page.locator('#saveBtn').click();
  await page.waitForFunction(() => /Saved in this browser/.test(document.getElementById('saveNote').textContent), null,
    { timeout: 20000 }).catch(() => {});
  const note = await page.locator('#saveNote').innerText();
  if (!/Saved in this browser/.test(note) && !phone) await fail('the save note did not confirm: ' + note);
  const saved = await status();
  await open();
  const back = await status();
  console.log(`after a reload: at (${back.x}, ${back.z}), tool ${back.tool}; message "${back.message}"`);
  if (Math.hypot(back.x - saved.x, back.z - saved.z) > 0.25) await fail('the save did not survive a reload');
  if (back.tool !== saved.tool) await fail('the held tool did not survive a reload');
  if (await page.locator('#help').isVisible()) await fail('How to play came back after it was closed once');

  // The loading shell finished; and without WebGL 2 it shows the trailer (or
  // its still) and says why, and the engine never downloads.
  {
    await page.waitForFunction(() => { const r = document.querySelector('.sk-loader'); return r && r.hidden; }, null, { timeout: 10000 }).catch(() => {});
  const shell = await page.evaluate(() => {
      const r = document.querySelector('.sk-loader');
      return { hidden: !!(r && r.hidden), state: window.StrataLoader && StrataLoader.state };
    });
    if (!shell.hidden || shell.state !== 'done') await fail('the loading shell did not finish: ' + JSON.stringify(shell));
    console.log('ok: the loading shell showed and is gone');
    const bare = await browser.newContext({ ...base, userAgent: (base.userAgent || 'Mozilla/5.0') + ' kindling-smoke headless' });
    await bare.addInitScript(() => {
      const get = HTMLCanvasElement.prototype.getContext;
      HTMLCanvasElement.prototype.getContext = function (t, o) { return t === 'webgl2' ? null : get.call(this, t, o); };
    });
    const p2 = await bare.newPage();
    const engine = [];
    p2.on('request', (r) => { if (/kindling\.(wasm|js)$/.test(r.url())) engine.push(r.url()); });
    await p2.goto(url, { waitUntil: 'commit' });
    await p2.waitForFunction(() => window.StrataLoader && StrataLoader.state === 'fallback', null, { timeout: 30000 });
    const fb = await p2.evaluate(() => {
      const r = document.querySelector('.sk-loader');
      return { text: r.innerText, media: !!r.querySelector('video, img'), wide: document.documentElement.scrollWidth > innerWidth };
    });
    await p2.screenshot({ path: path.join(outDir, 'no-webgl2.png') });
    if (!/WebGL 2/.test(fb.text) || fb.wide) await fail('without WebGL 2 the shell did not say why: ' + JSON.stringify(fb));
    if (engine.length) await fail('without WebGL 2 the engine still downloaded: ' + engine.join(', '));
    console.log('ok: without WebGL 2 the shell ' + (fb.media ? 'shows the trailer and ' : '') + 'says why, and nothing of the engine downloads');
    await bare.close();
  }

  if (errors.length) await fail('page errors: ' + errors.join(' | '));
  console.log(`OK (${phone ? 'phone' : 'desktop'})`);
  await browser.close();
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
