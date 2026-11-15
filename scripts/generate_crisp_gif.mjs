import { spawn, execSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

const PORT = 9333;
const CHROME_PATH = '/usr/bin/google-chrome-stable';
const SCENE_PATH = `file://${path.resolve('scripts/demo_scene.html')}`;
const FRAMES_DIR = '/tmp/typemoji_crisp_frames';

if (fs.existsSync(FRAMES_DIR)) {
  fs.rmSync(FRAMES_DIR, { recursive: true, force: true });
}
fs.mkdirSync(FRAMES_DIR, { recursive: true });

// 1. Launch Chrome
console.log('Starting headless Chrome on port', PORT);
const chrome = spawn(CHROME_PATH, [
  '--headless=new',
  '--no-sandbox',
  '--disable-gpu',
  `--remote-debugging-port=${PORT}`,
  '--window-size=880,540',
  'about:blank'
]);

async function waitForDevTools() {
  for (let i = 0; i < 30; i++) {
    try {
      const res = await fetch(`http://localhost:${PORT}/json/version`);
      if (res.ok) {
        const data = await res.json();
        return data.webSocketDebuggerUrl;
      }
    } catch (e) {
      await new Promise(r => setTimeout(r, 200));
    }
  }
  throw new Error('Chrome DevTools failed to start');
}

async function run() {
  try {
    await waitForDevTools();

    // Create target page
    const newPageRes = await fetch(`http://localhost:${PORT}/json/new?${encodeURIComponent(SCENE_PATH)}`, { method: 'PUT' });
    const pageData = await newPageRes.json();
    const pageWsUrl = pageData.webSocketDebuggerUrl;

    const ws = new WebSocket(pageWsUrl);
    await new Promise((resolve) => ws.onopen = resolve);

    let id = 1;
    function sendCommand(method, params = {}) {
      return new Promise((resolve, reject) => {
        const reqId = id++;
        const handler = (evt) => {
          const msg = JSON.parse(evt.data);
          if (msg.id === reqId) {
            ws.removeEventListener('message', handler);
            if (msg.error) reject(msg.error);
            else resolve(msg.result);
          }
        };
        ws.addEventListener('message', handler);
        ws.send(JSON.stringify({ id: reqId, method, params }));
      });
    }

    // Set viewport: 880x540 at 2x scale for Retina supersampling
    await sendCommand('Emulation.setDeviceMetricsOverride', {
      width: 880,
      height: 540,
      deviceScaleFactor: 2,
      mobile: false
    });

    // Wait for fonts to be ready
    console.log('Waiting for fonts to load...');
    for (let i = 0; i < 30; i++) {
      const res = await sendCommand('Runtime.evaluate', { expression: 'window.__fontsReady === true' });
      if (res.result?.value === true) break;
      await new Promise(r => setTimeout(r, 200));
    }

    const frameCountRes = await sendCommand('Runtime.evaluate', { expression: 'window.getFrameCount()' });
    const totalFrames = frameCountRes.result?.value || 17;

    console.log(`Capturing ${totalFrames} frames...`);
    for (let f = 0; f < totalFrames; f++) {
      await sendCommand('Runtime.evaluate', { expression: `window.setFrame(${f})` });
      await new Promise(r => setTimeout(r, 60));

      const screenshot = await sendCommand('Page.captureScreenshot', { format: 'png' });
      const buffer = Buffer.from(screenshot.data, 'base64');
      fs.writeFileSync(`${FRAMES_DIR}/frame_${String(f).padStart(3, '0')}.png`, buffer);
      process.stdout.write(`.`);
    }
    console.log('\nFrames captured!');

    ws.close();
    chrome.kill();

    // Generate concat specification with tailored durations for natural human pacing
    // Frame durations in seconds:
    const durations = [
      0.5,  // 0: Initial cursor blink on
      0.4,  // 1: cursor blink off
      0.4,  // 2: cursor blink on
      0.5,  // 3: Super + . hotkey HUD appears
      0.4,  // 4: Super + . hotkey HUD hold
      0.35, // 5: TypEmoji panel opens
      0.2,  // 6: Type 'r'
      0.2,  // 7: Type 'ro'
      0.2,  // 8: Type 'rock'
      0.35, // 9: Type 'rocket'
      0.6,  // 10: Selected rocket focused
      0.35, // 11: Enter key press effect
      0.5,  // 12: Injected 🚀 + Toast appears
      0.4,  // 13: cursor blink off
      0.4,  // 14: cursor blink on
      0.4,  // 15: cursor blink off
      2.0   // 16: Hold on completed command & toast for reading
    ];

    let concatContent = '';
    for (let i = 0; i < totalFrames; i++) {
      const d = durations[i] || 0.4;
      concatContent += `file '${FRAMES_DIR}/frame_${String(i).padStart(3, '0')}.png'\n`;
      concatContent += `duration ${d}\n`;
    }
    // Concat requires the last frame to be repeated
    concatContent += `file '${FRAMES_DIR}/frame_${String(totalFrames - 1).padStart(3, '0')}.png'\n`;

    const concatPath = `${FRAMES_DIR}/concat.txt`;
    fs.writeFileSync(concatPath, concatContent);

    console.log('Rendering high-quality GIF with ffmpeg...');
    const outGif = path.resolve('assets/demo.gif');
    
    // Downscale 2x retina frames to 880x540 using Lanczos filter + optimal 256-color palette
    const ffmpegCmd = `ffmpeg -f concat -safe 0 -i "${concatPath}" -filter_complex "scale=880:540:flags=lanczos,split [a][b];[a] palettegen=max_colors=256:reserve_transparent=off:stats_mode=diff [p];[b][p] paletteuse=dither=bayer:bayer_scale=2" -y "${outGif}"`;

    execSync(ffmpegCmd, { stdio: 'inherit' });

    const stats = fs.statSync(outGif);
    console.log(`\n🎉 New demo GIF generated successfully!`);
    console.log(`Path: ${outGif}`);
    console.log(`Size: ${(stats.size / 1024).toFixed(1)} KB`);

  } catch (err) {
    console.error('Error during recording:', err);
    chrome.kill();
    process.exit(1);
  }
}

run();
