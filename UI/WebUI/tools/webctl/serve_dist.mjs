// Serve the built WebUI, statically, on 8081.
//
//   node serve_dist.mjs [port] [root]
//
// The dev server is the wrong thing to hang a long run on: it compiles on
// demand (the first load exceeded a 10s navigation timeout more than once this
// session), it holds a module graph that can wedge while still holding the port
// -- observed: listening, accepting connections, answering nothing -- and every
// restart re-pays the compile. None of that is a criticism of vite; it is a
// development tool being asked to be infrastructure.
//
// A static directory has no such states. It is either serving the files or the
// process is gone, and the difference is visible from outside.
//
// No dependencies on purpose: this has to survive a node_modules that is being
// rebuilt in the next terminal.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

const PORT = Number(process.argv[2] || 8081);
const ROOT = path.resolve(process.argv[3]
  || path.join(path.dirname(url.fileURLToPath(import.meta.url)), '..', '..', 'dist'));

const TYPES = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8', '.svg': 'image/svg+xml',
  '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg',
  '.gif': 'image/gif', '.ico': 'image/x-icon', '.ttf': 'font/ttf',
  '.woff': 'font/woff', '.woff2': 'font/woff2', '.wasm': 'application/wasm',
  '.map': 'application/json; charset=utf-8',
};

http.createServer((req, res) => {
  let p;
  try { p = decodeURIComponent(new URL(req.url, 'http://x').pathname); }
  catch (e) { res.writeHead(400); return res.end('bad path'); }
  if (p.endsWith('/')) p += 'index.html';
  // Resolve, then require the result to be inside ROOT. A path that escapes is
  // refused rather than normalised into something that happens to work.
  const file = path.resolve(path.join(ROOT, p));
  if (file !== ROOT && !file.startsWith(ROOT + path.sep)) {
    res.writeHead(403); return res.end('outside root');
  }
  fs.readFile(file, (err, buf) => {
    if (err) {
      // The app is a single page: unknown paths are its routes, not misses.
      if (path.extname(file)) { res.writeHead(404); return res.end('not found'); }
      return fs.readFile(path.join(ROOT, 'index.html'), (e2, idx) => {
        if (e2) { res.writeHead(404); return res.end('no index'); }
        res.writeHead(200, { 'Content-Type': TYPES['.html'] });
        res.end(idx);
      });
    }
    res.writeHead(200, {
      'Content-Type': TYPES[path.extname(file).toLowerCase()] || 'application/octet-stream',
      // No caching: a soak that reloads the page must get the build on disk,
      // not one from an hour ago.
      'Cache-Control': 'no-store',
    });
    res.end(buf);
  });
}).listen(PORT, '127.0.0.1', () => {
  console.log(`serving ${ROOT} on http://127.0.0.1:${PORT}`);
});
