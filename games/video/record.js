// record.js <game 1..5> : records one game page with headless Chrome over CDP (screencast frames),
// renders the title/caption bands, and writes frames + bands + timeline.json for compose.py.
const { spawn } = require('child_process');
const fs = require('fs'), path = require('path');
const G = require('./scenes.js');
const id = +process.argv[2] || 1, g = G[id];
const OUT = path.join(__dirname, 'out', 'g' + id);
if (process.argv[3] !== 'bands') fs.rmSync(path.join(OUT, 'frames'), { recursive: true, force: true });
fs.mkdirSync(path.join(OUT, 'frames'), { recursive: true });
const CHROME = process.env.CHROME || 'C:/Program Files/Google/Chrome/Application/chrome.exe', PORT = 9333 + id;
const BASE = 'file:///' + path.resolve(__dirname, '..').split(path.sep).join('/') + '/';
const sleep = ms => new Promise(r => setTimeout(r, ms));

async function main() {
  const prof = path.join(process.env.TEMP, 'rec-chrome-' + id);
  const ch = spawn(CHROME, ['--headless=new', `--remote-debugging-port=${PORT}`, `--user-data-dir=${prof}`, '--hide-scrollbars',
    '--disable-background-timer-throttling', '--disable-renderer-backgrounding', '--disable-backgrounding-occluded-windows',
    '--allow-file-access-from-files', ...(g.webgl ? ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] : []), 'about:blank'], { stdio: 'ignore' });
  let ws;
  try {
    let target;
    for (let i = 0; i < 50 && !target; i++) { await sleep(200); try { target = (await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json()).find(t => t.type === 'page'); } catch {} }
    ws = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise(r => ws.onopen = r);
    let seq = 0; const pend = new Map(), handlers = [];
    ws.onmessage = ev => { const m = JSON.parse(ev.data); if (m.id && pend.has(m.id)) { pend.get(m.id)(m); pend.delete(m.id); } else handlers.forEach(h => h(m)); };
    const send = (method, params = {}) => new Promise(r => { const i = ++seq; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
    const evalJs = async expr => { const r = await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true }); if (r.result?.exceptionDetails) console.log('JS error:', JSON.stringify(r.result.exceptionDetails).slice(0, 300)); return r.result?.result?.value; };
    await send('Page.enable'); await send('Runtime.enable');
    handlers.push(m => { if (m.method === 'Runtime.exceptionThrown') console.log('page exception:', JSON.stringify(m.params.exceptionDetails).slice(0, 300)); });

    // 1) bands: title (1600x150) and one caption per step (1600x120)
    const shot = async (html, w, h, file) => {
      await send('Emulation.setDeviceMetricsOverride', { width: w, height: h, deviceScaleFactor: 1, mobile: false });
      await send('Page.navigate', { url: 'data:text/html;charset=utf-8,' + encodeURIComponent(html) }); await sleep(500);
      const r = await send('Page.captureScreenshot', { format: 'png' }); fs.writeFileSync(file, Buffer.from(r.result.data, 'base64'));
    };
    const CSS = `<style>html,body{margin:0;background:#0f0e0c;color:#f3eee4;font-family:"Segoe UI",sans-serif;overflow:hidden}
      .o{color:#ff8a57;font-weight:800}.n{color:#5ea3ff;font-weight:800}b{color:#fff}</style>`;
    await shot(`${CSS}<div style="padding:20px 44px 0;height:150px;box-sizing:border-box">
      <div style="font:700 20px 'Segoe UI';color:#f4c46b;letter-spacing:.08em;text-transform:uppercase">${g.tag}</div>
      <div style="font:700 46px/1.1 'Segoe UI';margin-top:4px">${g.title}</div>
      <div style="font:25px/1.3 'Segoe UI';color:#cfc7b8;margin-top:8px">${g.sub}</div></div>`, 1600, 150, path.join(OUT, 'top.png'));
    const caps = g.steps.filter(s => s[2]);
    for (let k = 0; k < caps.length; k++)
      await shot(`${CSS}<div style="height:120px;box-sizing:border-box;padding:14px 44px 0;border-top:2px solid #2b2822;position:relative">
        <div style="font:600 31px/1.25 'Segoe UI'">${caps[k][2]}</div>
        <div style="position:absolute;right:44px;bottom:10px;font:18px 'Segoe UI';color:#8f8778">${g.link}</div></div>`, 1600, 120, path.join(OUT, `cap${k}.png`));

    if (process.argv[3] === 'bands') { console.log(`g${id}: bands only`); await send('Browser.close').catch(() => {}); return; }
    // 2) the game
    const [cy, chh] = g.crop, dpr = 1.25;
    await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: chh, deviceScaleFactor: dpr, mobile: false });
    await send('Page.navigate', { url: BASE + g.url });
    await sleep(g.settle || 6000);
    await evalJs(`window.scrollTo(0, ${cy}); 1`);
    if (g.setup) await evalJs(g.setup);
    await sleep(g.presettle || 1500);
    const frames = []; let t0 = null;
    handlers.push(async m => {
      if (m.method !== 'Page.screencastFrame') return;
      const ts = m.params.metadata.timestamp; if (t0 === null) t0 = ts;
      const f = `f${String(frames.length).padStart(5, '0')}.jpg`;
      frames.push([f, ts - t0]); fs.writeFileSync(path.join(OUT, 'frames', f), Buffer.from(m.params.data, 'base64'));
      send('Page.screencastFrameAck', { sessionId: m.params.sessionId });
    });
    await send('Page.startScreencast', { format: 'jpeg', quality: 92, everyNthFrame: 1 });
    const start = Date.now(), timeline = [];
    let capIdx = 0;
    for (const [t, act, cap] of g.steps) {
      const wait = t * 1000 - (Date.now() - start); if (wait > 0) await sleep(wait);
      if (act) await evalJs(act);
      if (cap) timeline.push([t, capIdx++]);
    }
    const dur = g.steps[g.steps.length - 1][0];
    await send('Page.stopScreencast'); await sleep(300);
    fs.writeFileSync(path.join(OUT, 'timeline.json'), JSON.stringify({ frames, caps: timeline, dur, crop: g.crop, dpr }, null, 0));
    console.log(`g${id}: ${frames.length} frames over ${dur}s (${(frames.length / dur).toFixed(1)} fps)`);
    await send('Browser.close').catch(() => {});
  } finally {
    try { ws && ws.close(); } catch {}
    await sleep(500); try { ch.kill(); } catch {}
  }
}
main().catch(e => { console.error(e); process.exit(1); });
