// Two rules, and deliberately only two.
//
// This exists because `usePerifConn is not defined` shipped on 2026-08-16 and
// was found on 2026-09-24, when somebody plugged in a SLID for the first time.
// The commit that moved the peripheral panels off Redux used two new functions
// in component/rdxComponent.jsx and never added the import. Vite does not care
// -- it bundles the file, the name resolves to nothing, and the ReferenceError
// waits for the one code path that renders that panel.
//
// A month of exposure for a class of mistake that a parser can see.
//
// NO STYLE RULES. The moment this reports indentation or quotes, its output
// becomes something to scroll past, and the next missing import scrolls past
// with it. Adding a rule here should require the same argument this file made:
// name the bug it would have caught.
//
//     npm run lint
//
import globals from 'globals';
// Registered, but NONE of its rules are switched on. The source already carries
// `// eslint-disable-next-line react-hooks/exhaustive-deps` in ten places, and
// eslint reports a disable directive for a rule it cannot find as an ERROR --
// ten of them, drowning the seven real ones on the first run. Loading the
// plugin makes those comments resolve without adding a third rule.
import reactHooks from 'eslint-plugin-react-hooks';

export default [
  {
    files: ['src/**/*.js', 'src/**/*.jsx', 'src/**/*.mjs', 'tools/**/*.mjs'],
    languageOptions: {
      ecmaVersion: 2023,
      sourceType: 'module',
      parserOptions: {
        // The source is JSX throughout and is compiled by vite's esbuild, not
        // babel -- so there is no .babelrc to read the setting from and it has
        // to be stated here.
        ecmaFeatures: { jsx: true },
      },
      globals: {
        ...globals.browser,
        ...globals.node,      // the tools/ scripts run under node
        ...globals.es2021,
        // Injected by vite.config.mjs `define:` at build time. Not a browser
        // global and not an import, so without this line every use reads as
        // "not defined". Read from the config rather than guessed -- two names
        // I assumed were here are not, and a global declared but never injected
        // is a hole in exactly the check this file exists for.
        __DEV_MODE__: 'readonly',
      },
    },
    plugins: { 'react-hooks': reactHooks },
    linterOptions: {
      // OFF, reluctantly. It is a good rule -- a file that disables something it
      // never triggers is a lie about the code. But every one of those ten
      // directives is for a rule this config deliberately does not enable, so
      // it would report all ten forever. Turn this back on the day
      // react-hooks/exhaustive-deps is actually enabled.
      reportUnusedDisableDirectives: 'off',
    },
    rules: {
      'no-undef': 'error',

      // Warn, not error. Unused names are usually a half-finished edit rather
      // than a defect, and making them fail the command would tempt people to
      // stop running it -- which costs the no-undef check too.
      //
      // Arguments are exempt: a callback that takes (err, data) and uses only
      // data is correct, and renaming it to _err to satisfy a linter makes the
      // signature harder to read.
      'no-unused-vars': ['warn', {
        args: 'none',
        varsIgnorePattern: '^_',
        caughtErrors: 'none',
      }],
    },
  },
  {
    // JSX compiles to React.createElement here, so React is used even where it
    // is never named in the file body.
    files: ['src/**/*.jsx', 'src/**/*.js'],
    rules: {
      'no-unused-vars': ['warn', {
        args: 'none',
        varsIgnorePattern: '^(_|React$)',
        caughtErrors: 'none',
      }],
    },
  },
];
