// Drive N hardware triggers/s at the board's CAM1 pin for T seconds while an
// FI session runs, so the core's per-frame log paths see production-shaped
// traffic. Board side goes through the core's perif console (4099) -- opening
// the board's own serial port would reboot it.
import net from 'node:net';
const rate = Number(process.argv[2] || 25);
const secs = Number(process.argv[3] || 60);
const s = net.connect(4099, '127.0.0.1');
let n = 0;
s.on('connect', () => {
  const iv = setInterval(() => {
    s.write(JSON.stringify({ type: 'trig_cam_pulse', cpin: 17, lpin: 16,
                             light_delay: 100, light_duration: 600 }) + '\n');
    n++;
  }, 1000 / rate);
  setTimeout(() => { clearInterval(iv); console.log('pulses sent:', n); s.end(); process.exit(0); }, secs * 1000);
});
s.on('error', e => { console.error('console 4099:', e.message); process.exit(1); });
