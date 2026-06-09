import { defineConfig, transformWithEsbuild } from 'vite';
import react from '@vitejs/plugin-react';
import { nodePolyfills } from 'vite-plugin-node-polyfills';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Production Vite build (companion to the dev-only vite.config.mjs). The legacy
// webpack production build does not resolve the .jsx/.ts files added on this
// branch, so production bundling is done here. Outputs a self-contained dist/.
const root = path.dirname(fileURLToPath(import.meta.url));
const r = (p) => path.resolve(root, p);

// Many source files are .js but contain JSX. @vitejs/plugin-react disables Vite's
// built-in esbuild JSX transform and itself skips JSX in .js, so transform it here
// (before plugin-react).
const jsxInJs = {
  name: 'jsx-in-js',
  enforce: 'pre',
  async transform(code, id) {
    const file = id.split('?')[0];
    if (!file.includes('/src/') || !file.endsWith('.js')) return null;
    return transformWithEsbuild(code, file, { loader: 'jsx' });
  },
};

export default defineConfig({
  root,
  base: './', // relative asset paths -> portable when copied/deployed
  define: { __DEV_MODE__: 'false' },
  plugins: [
    nodePolyfills({ globals: { Buffer: true, process: true } }),
    jsxInJs,
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
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    rollupOptions: {
      input: r('index.html'),
      external: ['electron', 'fs', 'path'],
    },
  },
});
