import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react({
    // Add options to improve HMR behavior
    fastRefresh: true,
    // Exclude App.tsx from Fast Refresh to resolve component export issues
    exclude: [],
    // Add debug logs for HMR
    jsxRuntime: 'automatic'
  })],
  server: {
    port: 5173,
    open: true,
    hmr: {
      // Force HMR to work over WebSockets
      protocol: 'ws',
      // Log more detailed HMR info
      overlay: true,
    }
  },
  // Handle CRA-style environment variables
  define: {
    'process.env': process.env
  },
  resolve: {
    alias: {
      // If you use absolute imports based on src/
      '@': '/src',
    },
  },
  // Configure base path if your app is not hosted at the root
  base: './',
}); 