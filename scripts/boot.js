// How THIS application starts. Shipped inside every version package as
// scripts/boot.js, and read by the launcher through src/boot.js.
//
// Everything the launcher used to hard-code about visSele lives here instead:
// the executable's name and where it sits, the chdir argument, the control
// port, where the UI's index.html is, and what has to exist on disk before any
// of it can run. The launcher knows none of that, which is what lets a version
// change its own layout without a new launcher.
//
// It is DECLARATIVE on purpose. describe() returns a description; the launcher
// does the spawning, drains both stdio pipes, runs the health checks and owns
// the graceful-stop-then-force-kill sequence. The previous design handed the
// payload a spawn() and let it do all of that itself, and the result was that
// process supervision lived in the thing being supervised -- an unread stderr
// pipe that could wedge the whole machine, a kill('SIGINT') that delivers no
// signal on Windows, and a taskkill /f on the line after it. Those are
// launcher bugs; they should be fixed once, not once per version.

'use strict';

// The core's control socket. Declared in one place and used twice -- passed to
// the core as an environment variable, and told to the launcher so it knows
// where to ping and where to send shutdown. Two literals that must agree is
// exactly how they stop agreeing.
const CONTROL_PORT = 4098;

module.exports = {
  apiVersion: 1,

  describe(ctx) {
    const exe = ctx.platform === 'win32' ? 'Core/visSele.exe' : 'Core/visSele';

    return {
      name: `visSele core (${ctx.platform})`,

      core: {
        exe,

        // The core is started IN its own folder and then chdir()s to the
        // machine's working directory. Both halves matter:
        //
        //   cwd: 'Core'   the camera SDK's producer modules (MvProducer*.cti)
        //                 and its DLLs live beside the executable
        //   chdir=        every path inside the core is relative --
        //                 "data/machine_setting.json", "data/featureDetect",
        //                 "data/SAMPLE" and a dozen more -- so this is what
        //                 decides which machine's settings it runs on
        //
        // The core hard-fails if the chdir target does not exist. It used to
        // ignore the error and carry on in whatever directory it was launched
        // from, inspecting against a different data/ while reporting nothing.
        cwd: 'Core',
        args: [`chdir=${ctx.workingDir}`],

        // INSP_PERIF_CONSOLE opens a TCP line console onto the peripheral link
        // (see the block comment above PerifConsoleThread). It is OFF unless
        // the variable is set, and it is passed through from the launcher's own
        // environment rather than hard-coded here: a production machine must
        // not open a control port because a developer once needed one, but a
        // soak or a bench session has no other way to ask the board questions
        // while the core owns the serial port exclusively.
        //
        // Passed through, not invented -- so enabling it is a deliberate act by
        // whoever started the launcher, and is visible in the plan the shell
        // displays.
        env: {
          INSP_CONTROL_PORT: String(CONTROL_PORT),
          ...(process.env.INSP_PERIF_CONSOLE
              ? { INSP_PERIF_CONSOLE: String(process.env.INSP_PERIF_CONSOLE) }
              : {}),
        },

        control: { host: '127.0.0.1', port: CONTROL_PORT },

        // Camera init retries in a loop on a cold bench, and a GigE camera that
        // is still negotiating can take a while. This is how long the launcher
        // waits for the first answer before showing the UI anyway -- not a
        // failure deadline, just how long to hold the splash.
        readyTimeoutMs: 40000,
      },

      // The built UI, unless a dev server is named.
      //
      // INSP_UI_DEV_URL points the window at Vite instead of at the bundle on
      // disk, which is the difference between a UI change taking one second
      // and taking three minutes. Without it every tweak means `vite build`
      // (~60 s), re-assembling the app folder, restarting Electron, and
      // driving the whole bring-up again -- recipe, 製程, 檢測方式, start the
      // machine -- before the change can even be looked at.
      //
      // React Refresh is configured for src/**.js in vite.config.mjs, so an
      // edit re-mounts the touched component WITHOUT reloading the page: the
      // Redux store and the WS connection to the core are module-level and
      // survive, so the session stays where it was.
      //
      // Passed through from the launcher's environment rather than invented
      // here, for the same reason as INSP_PERIF_CONSOLE above: a production
      // machine must not be able to end up pointing at a dev server because
      // someone left a variable in a script. The launcher only accepts
      // loopback URLs (src/boot.js), so this cannot reach off the machine.
      ui: process.env.INSP_UI_DEV_URL
        ? { url: process.env.INSP_UI_DEV_URL }
        : { indexPath: 'WebUI/index.html' },

      // What must be present before there is any point starting.
      //
      // The launcher deliberately has no opinion about "data/" -- that is this
      // core's convention, so this package is the right place to state it AND
      // to say why, because the operator reading the error is the one who has
      // to fix it.
      //
      // data/ itself only, not the files inside it: a machine being
      // commissioned legitimately has no calibration yet, and refusing to start
      // over that would make the machine unusable at exactly the moment someone
      // is trying to set it up.
      requires: [
        {
          path: `${ctx.workingDir}/data`,
          kind: 'dir',
          why: '機台設定、校正與配方(machine_setting.json、lens_calib.json、'
             + 'featureDetect/ …)。要指定的是 data/ 的上一層,不是 data/ 本身。',
        },
      ],
    };
  },
};
