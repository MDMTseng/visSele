// Build provenance for the WebUI, captured at Vite config load (= build start for
// `npm run build`, or dev-server start for `npm run dev`). Exposed to the app as
// the `__WEBUI_BUILD__` define so the UI can show what build it's running.
import { execSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));
const g = (cmd) => {
  try { return execSync(cmd, { cwd: root }).toString().trim(); }
  catch { return 'unknown'; }
};

export const WEBUI_BUILD = {
  time: new Date().toISOString(),
  git_hash: g('git rev-parse --short=12 HEAD'),
  git_branch: g('git rev-parse --abbrev-ref HEAD'),
  git_describe: g('git describe --always --tags --dirty'),
};

// Expose WEBUI_BUILD to the app as `import { WEBUI_BUILD } from 'virtual:webui-build'`.
// A virtual module works in BOTH dev and prod, unlike `define` (this config's
// custom JSX transform bypasses Vite's define in dev -- see __DEV_MODE__).
export function webuiBuildPlugin() {
  const id = 'virtual:webui-build';
  const resolved = '\0' + id;
  return {
    name: 'webui-build',
    resolveId(s) { return s === id ? resolved : null; },
    load(s) { return s === resolved ? `export const WEBUI_BUILD = ${JSON.stringify(WEBUI_BUILD)};` : null; },
  };
}
