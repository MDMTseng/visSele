import { defineConfig, transformWithEsbuild } from 'vite';
import react from '@vitejs/plugin-react';
import { nodePolyfills } from 'vite-plugin-node-polyfills';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Dev-only Vite config. Production builds still use webpack (npm run build).
// Runs on :8081 alongside the webpack dev server (:8080) so we can A/B.
const root = path.dirname(fileURLToPath(import.meta.url));
const r = (p) => path.resolve(root, p);

// Serve index.dev.html at "/" so the prod index.html (used by webpack/export) stays untouched.
// Many source files are .js but contain JSX. @vitejs/plugin-react disables Vite's
// built-in esbuild JSX transform and itself skips JSX in .js, so transform it here
// (before plugin-react) — plugin-react then only adds React-Refresh to plain JS.
const jsxInJs = {
  name: 'jsx-in-js',
  enforce: 'pre',
  async transform(code, id) {
    const file = id.split('?')[0];
    if (!file.includes('/src/') || !file.endsWith('.js')) return null;
    return transformWithEsbuild(code, file, { loader: 'jsx' });
  },
};

// Serve index.dev.html for "/" and "/index.html" (and any query/hash variant) so the
// prod index.html — which points at the webpack dist/ bundle — is never served by Vite.
const devHtmlAtRoot = {
  name: 'dev-html-at-root',
  configureServer(server) {
    server.middlewares.use((req, _res, next) => {
      const pathname = (req.url || '/').split('?')[0].split('#')[0];
      if (pathname === '/' || pathname === '/index.html') {
        const suffix = (req.url || '').slice(pathname.length); // preserve any query/hash
        req.url = '/index.dev.html' + suffix;
      }
      next();
    });
  },
};

export default defineConfig({
  root,
  define: { __DEV_MODE__: 'true' },
  plugins: [
    devHtmlAtRoot,
    // webpack auto-polyfilled Node builtins; several deps need them at runtime
    // (jsum → crypto.createHash for the featureSet_sha1 integrity hash, etc.).
    nodePolyfills({ globals: { Buffer: true, process: true } }),
    jsxInJs,
    // React 16 → classic runtime (React must be in scope). Include .js for refresh.
    react({ jsxRuntime: 'classic', include: /src\/.*\.(js|jsx)$/, exclude: /node_modules/ }),
  ],
  resolve: {
    alias: {
      UTIL: r('src/UTIL'),
      JSSRCROOT: r('src'),
      LANG: r('src/languages'),
      RES: r('resource'),
      REDUX_STORE_SRC: r('src/redux'),
      STYLE: r('style'),
    },
  },
  css: {
    preprocessorOptions: {
      less: {
        javascriptEnabled: true,
        modifyVars: {
          'primary-color': '#82CBCB',
          'link-color': '#1DA57A',
          'border-radius-base': '2px',
        },
      },
    },
  },
  optimizeDeps: {
    entries: ['index.dev.html'], // only scan our dev entry, not the stray index.html files in root
    esbuildOptions: { loader: { '.js': 'jsx' } },
    exclude: ['electron', 'fs', 'path'],
  },
  server: { host: true, port: 8081, strictPort: true },
});
