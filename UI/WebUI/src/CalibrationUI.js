// New Calibration UI — chessboard (lens) + bright/dark field capture.
// Stage 1: scaffold only. Real capture / BPG wiring lands in follow-up tasks
// (see #63-#67). Keep BackLightCalibUI alongside until this fully replaces it.
import React, { useState, useEffect, useRef, useCallback } from 'react';
import { connect } from 'react-redux';
import Tabs from 'antd/lib/tabs';
import Button from 'antd/lib/button';
import Card from 'antd/lib/card';
import InputNumber from 'antd/lib/input-number';
import Select from 'antd/lib/select';
import Spin from 'antd/lib/spin';
import ReactResizeDetector from 'react-resize-detector';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';

// Live preview canvas — minimal SLCALIB_CanvasComponent wrapper. Pulls c_state /
// edit_info from redux so the streamed image (delivered via the usual BPG
// pipeline that calls SetImg on the canvas) shows up here too.
// Mirror the EXACT pattern from BackLightCalibUI's CanvasComponent (which is
// known to receive the live stream). Only difference: Preview_CanvasComponent
// instead of SLCALIB_CanvasComponent so it doesn't throw when no def is loaded.
class PreviewCanvas extends React.Component {
  constructor(props) {
    super(props);
    this.windowSize = {};
  }
  componentDidMount() {
    this.ec_canvas = new EC_CANVAS_Ctrl.Preview_CanvasComponent(this.refs.canvas);
    this.ec_canvas.disableImageAlign = true;   // raw live frame: no def_image_reg rotate
    // This page uses NO def-file state at all. It exists to aim and calibrate
    // the instrument itself, so its scale must come from the instrument
    // (lens_calib.json um_per_px, passed down as props.mmpp) -- a def's mmpp
    // describes whatever image was side-loaded with that def, which is a
    // different camera-lens-standoff and simply the wrong number here.
    this.ec_canvas.SetStandalonePreview(this.props.mmpp);
    this._didInitialFit = false;
    if (this.props.onCanvasInit) this.props.onCanvasInit(this.ec_canvas);
    this.updateCanvas(this.props.c_state);
  }
  componentWillUnmount() {
    if (this.ec_canvas) this.ec_canvas.resourceClean();
  }
  updateCanvas(ec_state, props = this.props) {
    if (this.ec_canvas === undefined) return;
    // SetState gates pan-drag inside onmousemove on this.state.substate; without
    // it, mouse drag is a no-op. (BackLightCalibUI hits the same code via the
    // DefConfUI flow that already pushes state.)
    if (ec_state) this.ec_canvas.SetState(ec_state);
    // lens_calib.json is fetched asynchronously, so the first frames can land
    // before the instrument scale does. Re-apply on change (it also re-fits).
    if (props.mmpp !== this._appliedMmpp && Number.isFinite(props.mmpp)) {
      this._appliedMmpp = props.mmpp;
      this.ec_canvas.SetStandalonePreview(props.mmpp);
    }
    // Only the frame is taken from redux -- edit_info.img is the live stream's
    // delivery slot, not def data. Fit once so the streamed frames don't reset
    // the operator's pan/zoom on every update.
    if (props.edit_info && props.edit_info.img) {
      this.ec_canvas.SetImg(props.edit_info.img);
      if (!this._didInitialFit) {
        this.ec_canvas.scaleImageToFitScreen();
        this._didInitialFit = true;
      }
    }
    this.ec_canvas.draw();
  }
  onResize(width, height) {
    if (Math.hypot(this.windowSize.width - width, this.windowSize.height - height) < 5) return;
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.resize(width, height);
      this.windowSize = { width, height };
      this.updateCanvas(this.props.c_state);
    }
  }
  componentWillUpdate(nextProps, nextState) {
    this.updateCanvas(nextProps.c_state, nextProps);
  }
  render() {
    return (
      <div className={this.props.addClass}>
        <canvas ref="canvas" className="width12 HXF"/>
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)}/>
      </div>
    );
  }
}
const PreviewCanvas_rdx = connect(
  (state) => ({ c_state: state.UIData.c_state, edit_info: state.UIData.edit_info }),
  () => ({})
)(PreviewCanvas);

function CalibrationUI(props) {
  const [tab, setTab] = useState('chessboard');
  const [squareMm, setSquareMm] = useState(1.0);
  const [lensModel, setLensModel] = useState('telecentric');
  // Each shot: {ts, thumb (dataURL)}. Thumb is grabbed from the secCanvas at
  // capture time -- that's the offscreen canvas the proto SetImg renders the
  // streamed frame into, so it has the unscaled pixel data.
  const [chessShots, setChessShots] = useState([]);
  const [brightShots, setBrightShots] = useState([]);
  const [darkShots, setDarkShots] = useState([]);
  const [brightAvgN, setBrightAvgN] = useState(16);
  const [darkAvgN, setDarkAvgN] = useState(16);
  const [bright, setBright] = useState(null);        // {W, H, data}
  const [dark, setDark] = useState(null);
  const [calibrating, setCalibrating] = useState(false);
  const [calibStatus, setCalibStatus] = useState('');  // 'running' | 'ok' | 'fail'
  const [calibMsg, setCalibMsg] = useState('');
  const [calibReport, setCalibReport] = useState(null);
  const [gridRows, setGridRows] = useState(16);
  const [gridCols, setGridCols] = useState(16);
  const [vignetteFrac, setVignetteFrac] = useState(0.5);
  const [brightStats, setBrightStats] = useState(null);
  const [darkStats, setDarkStats] = useState(null);
  const [fieldReport, setFieldReport] = useState(null);
  const [fieldBusy, setFieldBusy] = useState(false);
  const [savedField, setSavedField] = useState(null);   // { hasBright, hasDark }
  // Instrument scale (mm/px) straight from lens_calib.json -- the authority for
  // this page. Undefined until the file loads; the preview keeps the renderer
  // default until then rather than borrowing a number from somewhere wrong.
  const [instMmpp, setInstMmpp] = useState(undefined);

  const CALIB_DIR = "data/calibImages";
  const canvasRef = useRef(null);
  const previewBoxRef = useRef(null);
  const [hoverBri, setHoverBri] = useState(undefined);  // {ix,iy,lum,r,g,b,sx,sy}

  // Hover read-out: sample the pixel brightness under the pointer from the live
  // preview canvas and stash a screen-relative position for the tooltip.
  const onPreviewHover = (e) => {
    const ctrl = canvasRef.current;
    if (!ctrl || typeof ctrl.brightnessAtEvent !== 'function' || !previewBoxRef.current) return;
    const info = ctrl.brightnessAtEvent(e.nativeEvent);
    if (!info) { if (hoverBri !== undefined) setHoverBri(undefined); return; }
    const rect = previewBoxRef.current.getBoundingClientRect();
    setHoverBri({ ...info, sx: e.clientX - rect.left, sy: e.clientY - rect.top });
  };
  const onPreviewLeave = () => setHoverBri(undefined);

  // Stable identity so hover re-renders of this component don't churn the (pure,
  // connected) PreviewCanvas -- otherwise each mousemove would re-run SetImg
  // (JPEG re-decode) on the live preview.
  const onCanvasInit = useCallback((c) => {
    canvasRef.current = c;
    // QA handle: this canvas silently rendered nothing for a whole session
    // (CORE0_1_CAVEATS J8). Being able to read back the scale it is actually
    // using -- __GP_CALIB_CANVAS__.rUtil.get_mmpp() -- is what makes "is the
    // instrument mmpp applied?" a one-line question instead of an inference.
    if (typeof window !== "undefined") window.__GP_CALIB_CANVAS__ = c;
  }, []);

  // On mount, list any already-saved chessboard images and load their thumbnails.
  // Use LD with down_samp_level (DOWNSAMPLED) instead of LB (full-res PNG) so a
  // thumbnail costs ~1/THUMB_DS^2 the bytes -- the saved calib PNGs are full
  // camera res. no_cache:true tells the core's LD handler NOT to overwrite
  // __CACHE_IMG__ (the live working image) for these read-only loads. The IM
  // reply is already parsed (format 1/2 = JPEG); blob its bytes directly.
  const THUMB_DS = 8;
  // Gate the camera stream on this: load saved thumbnails FIRST, then start
  // streaming, so the two don't contend for the link on entry.
  const [thumbsLoaded, setThumbsLoaded] = useState(false);
  useEffect(() => {
    let pending = 0, listed = false, settled = false;
    const finishAll = () => { if (!settled) { settled = true; setThumbsLoaded(true); } };
    const checkDone = () => { if (listed && pending === 0) finishAll(); };
    // Safety: never let a slow/stuck thumbnail block the stream indefinitely.
    const guard = setTimeout(finishAll, 4000);
    const lazyLoadThumbs = (shots, setter) => {
      shots.forEach((shot) => {
        pending++;
        const tick = () => { pending--; checkDone(); };
        props.ACT_WS_SEND_BPG(props.CORE_ID, "LD", 0,
          { imgsrc: shot.path, down_samp_level: THUMB_DS, no_cache: true, jpeg_quality: 70 }, undefined, {
          resolve: (rpkts) => {
            const IM = rpkts.find(p => p.type === "IM");
            let url = null;
            if (IM && IM.image) {
              if (IM.format === 1 || IM.format === 2) {
                // JPEG: blob the bytes directly.
                const bytes = new Uint8Array(IM.image.byteLength);
                bytes.set(IM.image);   // copy: WS buffer is recycled on next message
                url = URL.createObjectURL(new Blob([bytes], { type: 'image/jpeg' }));
              } else if (IM.width && IM.height) {
                // Raw RGBA (format 0 -- what the LD-from-disk path sends). Paint to
                // an offscreen canvas and export a small JPEG data URL.
                try {
                  const rgba = new Uint8ClampedArray(IM.image.length || IM.image.byteLength);
                  rgba.set(IM.image);   // copy off the recycled WS buffer
                  const tc = document.createElement('canvas');
                  tc.width = IM.width; tc.height = IM.height;
                  tc.getContext('2d').putImageData(new ImageData(rgba, IM.width, IM.height), 0, 0);
                  url = tc.toDataURL('image/jpeg', 0.7);
                } catch (e) { /* leave thumb null */ }
              }
            }
            if (url) setter(prev => prev.map(s => s.ts === shot.ts ? { ...s, thumb: url } : s));
            tick();
          },
          reject: () => { tick(); },
        });
      });
    };
    props.ACT_WS_SEND_BPG(props.CORE_ID, "FB", 0, { path: CALIB_DIR, depth: 1 }, undefined, {
      resolve: (pkts) => {
        const fs = pkts.find(p => p.type === "FS");
        if (fs && fs.data && fs.data.files) {
          const toShot = (f) => ({ ts: f.name, path: `${CALIB_DIR}/${f.name}`, thumb: null, persisted: true });
          const pngs = fs.data.files.filter(f => /\.png$/i.test(f.name));
          const chess  = pngs.filter(f => /^chess[_\-.]/i.test(f.name)  || (!/^bright/i.test(f.name) && !/^dark/i.test(f.name))).map(toShot);
          const bright = pngs.filter(f => /^bright[_\-.]/i.test(f.name)).map(toShot);
          const dark   = pngs.filter(f => /^dark[_\-.]/i.test(f.name)).map(toShot);
          if (chess.length)  setChessShots(chess);
          if (bright.length) setBrightShots(bright);
          if (dark.length)   setDarkShots(dark);
          lazyLoadThumbs(chess,  setChessShots);
          lazyLoadThumbs(bright, setBrightShots);
          lazyLoadThumbs(dark,   setDarkShots);
        }
        listed = true;
        checkDone();   // no-shots case -> stream can start immediately
      },
      reject: () => { listed = true; finishAll(); },
    });
    return () => clearTimeout(guard);
  }, []);

  const grabThumb = () => {
    const c = canvasRef.current && canvasRef.current.secCanvas;
    if (!c || !c.width || !c.height) return null;
    // Downscale to a max ~240px wide thumbnail to keep state lean.
    const maxW = 240;
    const scale = Math.min(1, maxW / c.width);
    const tw = Math.max(1, Math.round(c.width * scale));
    const th = Math.max(1, Math.round(c.height * scale));
    const tc = document.createElement('canvas');
    tc.width = tw; tc.height = th;
    tc.getContext('2d').drawImage(c, 0, 0, tw, th);
    return tc.toDataURL('image/jpeg', 0.6);
  };

  // Camera tuning controls. Initial values are read from
  // data/default_camera_setting.json (the same file CameraSettingFromFile()
  // loads at core boot). User nudges push live via ST/CameraSetting; Save
  // writes the JSON back so it survives restart.
  const SETTING_PATH = "data/default_camera_setting.json";
  const [exposure, setExposure] = useState(10000);
  const [gain, setGain] = useState(1.0);
  const [gamma, setGamma] = useState(1.0);
  const [blacklevel, setBlacklevel] = useState(0);
  const [loaded, setLoaded] = useState(false);

  // On mount, ask core to (re)load camera setting + lens calib + field calib
  // from the paths the UI owns. Core has NO hard-coded paths and does NOT
  // auto-load anything at startup -- every read is UI-driven. load_lens_calib
  // also pushes mmpp into the sampler so downstream measurements pick it up.
  useEffect(() => {
    props.ACT_WS_SEND_BPG(props.CORE_ID, "RC", 0, {
      target: "calib_files_load",
      camera_setting_dir: "data/",
      lens_calib_path: "data/lens_calib.json",
      field_calib_path: "data/field_calib.json",
    });
  }, []);

  // Probe data/field_calib.json so the "Run Field Calibration" button can stay
  // enabled when the user re-enters this UI after a prior calibration (i.e.
  // brightStats/darkStats are null this session but the disk already has both
  // sides). If only one side exists in this session we still require the other
  // to be present on disk -- finalize uses the latest pending grid per side,
  // and missing-side pending means the saved file's side stays intact only if
  // we re-load it server-side first, which is out of scope here.
  // Instrument scale. um_per_px is what lens calibration produces and what the
  // core writes back; m (px/mm) is the same number inverted, kept as a fallback
  // for older files. Re-read after a successful run so the preview immediately
  // reflects the calibration the operator just made.
  const loadInstMmpp = useCallback(() => {
    props.ACT_WS_SEND_BPG(props.CORE_ID, "LD", 0, { filename: "data/lens_calib.json" },
      undefined, {
        resolve: (pkts) => {
          const fl = pkts.find(p => p.type === "FL");
          const d = fl && fl.data;
          if (!d) return;
          const mmpp = Number.isFinite(+d.um_per_px) && +d.um_per_px > 0 ? +d.um_per_px / 1000
                     : (Number.isFinite(+d.m) && +d.m > 0 ? 1 / +d.m : undefined);
          if (mmpp !== undefined) setInstMmpp(mmpp);
        },
        reject: () => {},
      });
  }, []);
  useEffect(() => { loadInstMmpp(); }, []);
  useEffect(() => {
    props.ACT_WS_SEND_BPG(props.CORE_ID, "LD", 0, { filename: "data/field_calib.json" },
      undefined, {
        resolve: (pkts) => {
          const fl = pkts.find(p => p.type === "FL");
          if (!fl || !fl.data) { setSavedField({ hasBright: false, hasDark: false }); return; }
          const d = fl.data;
          setSavedField({
            hasBright: !!(d.bright && d.bright.rows > 0 && d.bright.cols > 0),
            hasDark:   !!(d.dark   && d.dark.rows   > 0 && d.dark.cols   > 0),
          });
        },
        reject: () => setSavedField({ hasBright: false, hasDark: false }),
      });
  }, []);
  useEffect(() => {
    props.ACT_WS_SEND_BPG(props.CORE_ID, "LD", 0, { filename: SETTING_PATH }, undefined, {
      resolve: (pkts) => {
        const fl = pkts.find(p => p.type === "FL");
        if (!fl || !fl.data) { setLoaded(true); return; }
        const cfg = fl.data;
        if (cfg.exposure   != null) setExposure(cfg.exposure);
        if (cfg.gain       != null) setGain(cfg.gain);
        if (cfg.gamma      != null) setGamma(cfg.gamma);
        if (cfg.blacklevel != null) setBlacklevel(cfg.blacklevel);
        setLoaded(true);
      },
      reject: () => setLoaded(true),
    });
  }, []);

  const setCam = (key, val) => {
    props.ACT_WS_SEND_BPG(props.CORE_ID, "ST", 0,
      { CameraSetting: { [key]: val } });
  };

  // Calibration is a SENSOR measurement, so it needs the whole sensor.
  //
  // Focus, distortion and the field/brightness grids describe the optics, not
  // the product: measured through the inspection crop they would be valid only
  // inside that crop, and silently wrong the moment it moved. The frame this
  // page wants was already described as "the real 2448x2048" below -- the ROI
  // was simply never stated, because until InspectionROI existed the stored
  // setting was full-sensor anyway.
  //
  // Nothing restores this on the way out, deliberately: entering InspectionUI
  // reloads the machine's camera file, so there is ONE place that puts the crop
  // back rather than one per page that ever opened the sensor.
  const FULL_SENSOR_ROI = [0, 0, 99999, 99999];

  const saveCameraSetting = () => {
    // Read-modify-write: pull the current file, override only the four fields
    // this UI manages, write back. Any other keys (ROI, trigger_mode, vendor
    // extensions, etc.) are preserved.
    props.ACT_WS_SEND_BPG(props.CORE_ID, "LD", 0, { filename: SETTING_PATH }, undefined, {
      resolve: (pkts) => {
        const fl = pkts.find(p => p.type === "FL");
        const base = (fl && fl.data && typeof fl.data === 'object') ? fl.data : {};
        const merged = { ...base, exposure, gain, gamma, blacklevel };
        const payload = new TextEncoder().encode(JSON.stringify(merged, null, 2));
        props.ACT_WS_SEND_BPG(props.CORE_ID, "SV", 0, { filename: SETTING_PATH }, payload, {
          resolve: (pkts2) => {
            const ss = pkts2.find(p => p.type === "SS");
            if (ss && ss.data.ACK) console.log("saved", SETTING_PATH, merged);
            else console.warn("save failed", SETTING_PATH);
          },
        });
      },
      reject: () => console.warn("save aborted: could not read", SETTING_PATH),
    });
  };

  // Mirror InspMode trigger policy AND register a CI subscription. trigger_mode
  // alone doesn't push frames; core streams only when a CI is active. We reuse
  // BackLightCalibUI's stage_light_report PGID -- lightest published path that
  // delivers raw frames + ignores any in-process lens calibration.
  useEffect(() => {
    if (!thumbsLoaded) return;   // start the stream only after saved thumbnails load
    const CALIB_STREAM_PGID = 10105;
    // Full resolution on purpose. This is the one page where the pixels ARE the
    // subject: judging focus and reading chessboard corners needs the real
    // 2448x2048 frame, and a downsampled preview hides exactly the detail the
    // operator is here to set. It is not free -- ~5MB raw and ~38ms just to
    // encode, against ~3.3ms at DL:4 -- but this page never runs during
    // production, so the frame rate it costs buys nothing back elsewhere.
    props.ACT_WS_SEND_BPG(props.CORE_ID, "ST", 0,
      { CameraSetting: { trigger_mode: 0, down_samp_level: 1,
                         ROI: FULL_SENSOR_ROI } });
    props.ACT_WS_SEND_BPG(props.CORE_ID, "CI", 0, {
      _PGID_: CALIB_STREAM_PGID,
      _PGINFO_: { keep: true },
      definfo: {
        type: "stage_light_report",
        grid_size: [10, 10],
        nonBG_thres: 100,
        nonBG_spread_thres: 180,
      },
      IMG_ignore_calib: true,
    });
    return () => {
      props.ACT_WS_SEND_BPG(props.CORE_ID, "CI", 0,
        { _PGID_: CALIB_STREAM_PGID, _PGINFO_: { keep: false } });
      props.ACT_WS_SEND_BPG(props.CORE_ID, "ST", 0,
        { CameraSetting: { trigger_mode: 1 } });
    };
  }, [thumbsLoaded]);

  const captureChess = () => {
    const thumb = grabThumb();
    const ts = new Date().toISOString().replace(/[:.]/g, '-').replace('T', '_').replace('Z', '');
    const name = `chess_${ts}.png`;
    const filename = `${CALIB_DIR}/${name}`;
    // Optimistic UI: show thumb immediately, drop on save failure.
    const entry = { ts: name, path: filename, thumb, persisted: false };
    setChessShots(s => [...s, entry]);
    props.ACT_WS_SEND_BPG(props.CORE_ID, "SV", 0,
      { filename, make_dir: true, type: "__LAST_DATA_VIEW_CACHE_IMG__" }, undefined, {
        resolve: (pkts) => {
          const ss = pkts.find(p => p.type === "SS");
          const ok = ss && ss.data && ss.data.ACK === true;
          setChessShots(s => ok
            ? s.map(x => x.ts === name ? { ...x, persisted: true } : x)
            : s.filter(x => x.ts !== name));
        },
        reject: () => setChessShots(s => s.filter(x => x.ts !== name)),
      });
  };
  const captureField = (which) => {
    // Core averages N>0 distinct streamed frames into an M×N grid (mean+std per
    // cell), stores in pending bright/dark slot (LAST capture per side wins on
    // finalize). Thumb is grabbed from the live secCanvas at click time so the
    // user can visually confirm "yes, that was the bright frame I meant".
    const nFrames = which === 'bright' ? brightAvgN : darkAvgN;
    const ts = new Date().toISOString().replace(/[:.]/g, '-').replace('T', '_').replace('Z', '');
    const thumb = grabThumb();
    const setter = which === 'bright' ? setBrightShots : setDarkShots;
    const entry = { ts: `${which}_${ts}`, thumb, stats: null, pending: true };
    setter(s => [...s, entry]);
    setFieldBusy(true);
    props.ACT_WS_SEND_BPG(props.CORE_ID, "RC", 0, {
      target: "field_calib_capture",
      which, rows: gridRows, cols: gridCols, n_frames: nFrames,
      timeout_ms: Math.max(5000, nFrames * 800),
    }, undefined, {
      resolve: (pkts) => {
        const ss = pkts.find(p => p.type === "SS");
        const rp = pkts.find(p => p.type === "RP");
        const ok = ss && ss.data && ss.data.ACK === true;
        if (ok && rp && rp.data) {
          (which === 'bright' ? setBrightStats : setDarkStats)(rp.data);
          setter(s => s.map(x => x.ts === entry.ts ? { ...x, pending: false, stats: rp.data } : x));
        } else {
          setter(s => s.map(x => x.ts === entry.ts ? { ...x, pending: false, error: true } : x));
        }
        setFieldBusy(false);
      },
      reject: () => {
        setter(s => s.map(x => x.ts === entry.ts ? { ...x, pending: false, error: true } : x));
        setFieldBusy(false);
      },
    });
  };
  const finalizeField = () => {
    setFieldBusy(true);
    props.ACT_WS_SEND_BPG(props.CORE_ID, "RC", 0, {
      target: "field_calib_finalize",
      out: "data/field_calib.json",
      vignette_frac: vignetteFrac,
    }, undefined, {
      resolve: (pkts) => {
        const ss = pkts.find(p => p.type === "SS");
        const rp = pkts.find(p => p.type === "RP");
        const ok = ss && ss.data && ss.data.ACK === true;
        if (ok && rp && rp.data) setFieldReport(rp.data);
        else setFieldReport({ error: true });
        setFieldBusy(false);
      },
      reject: () => { setFieldReport({ error: true }); setFieldBusy(false); },
    });
  };
  const removeShot = (which, ts) => {
    const bucket = which === 'chess' ? chessShots : which === 'bright' ? brightShots : darkShots;
    const setter = which === 'chess' ? setChessShots : which === 'bright' ? setBrightShots : setDarkShots;
    const shot = bucket.find(s => s.ts === ts);
    if (!shot) return;
    setter(bucket.filter(s => s.ts !== ts));
    if (shot.path) {
      props.ACT_WS_SEND_BPG(props.CORE_ID, "SV", 0,
        { filename: shot.path, type: "__DELETE_FILE__" }, undefined, {
          resolve: (pkts) => {
            const ss = pkts.find(p => p.type === "SS");
            const ok = ss && ss.data && ss.data.ACK === true;
            if (!ok) setter(s => s.some(x => x.ts === ts) ? s : [...s, shot]);
          },
          reject: () => setter(s => s.some(x => x.ts === ts) ? s : [...s, shot]),
        });
    }
  };

  const removeFieldShot = (which, ts) => {
    const bucket = which === 'bright' ? brightShots : darkShots;
    const setter = which === 'bright' ? setBrightShots : setDarkShots;
    const remaining = bucket.filter(s => s.ts !== ts);
    setter(remaining);
    // Find most recent successful capture left in history; if none, clear
    // the per-side stats AND tell core to wipe its pending grid so finalize
    // falls back to the disk-loaded grid for this side.
    const lastGood = [...remaining].reverse().find(s => !s.pending && !s.error && s.stats);
    const statsSetter = which === 'bright' ? setBrightStats : setDarkStats;
    if (lastGood) {
      statsSetter(lastGood.stats);
    } else {
      statsSetter(null);
      props.ACT_WS_SEND_BPG(props.CORE_ID, "RC", 0,
        { target: "field_calib_clear_pending", which });
    }
  };
  const FieldThumbStrip = ({ shots, which }) => (
    <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', marginTop: 10 }}>
      {shots.map(s => (
        <div key={s.ts} style={{ position:'relative', border: s.pending ? '1px dashed #888' : '1px solid #444',
            borderRadius:4, padding:2, minWidth:160 }}>
          {s.thumb && <img src={s.thumb} style={{ display:'block', maxHeight:90, maxWidth:160 }}/>}
          <div style={{ fontSize:10, color:'#bbb', padding:'2px 4px' }}>
            {s.pending ? 'capturing…' :
              s.error ? <span style={{color:'#ff7676'}}>✗ failed</span> :
              s.stats ? `${s.stats.n_frames}f · μ${(+s.stats.cell_mean).toFixed(1)} · σ̃${(+s.stats.cell_std_median).toFixed(2)}` : ''}
          </div>
          <span onClick={() => removeFieldShot(which, s.ts)}
            title="remove this capture (last-removed side falls back to saved disk grid)"
            style={{ position:'absolute', top:-8, right:-8, width:18, height:18,
              background:'#e53', color:'#fff', borderRadius:9, fontSize:12, lineHeight:'18px',
              textAlign:'center', cursor:'pointer', userSelect:'none' }}>×</span>
        </div>
      ))}
    </div>
  );
  const ThumbStrip = ({ shots, which }) => (
    <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', marginTop: 10 }}>
      {shots.map(s => (
        <div key={s.ts} style={{ position: 'relative', border: '1px solid #444', borderRadius: 4 }}>
          {s.thumb
            ? <img src={s.thumb} style={{ display: 'block', maxHeight: 90, maxWidth: 160 }}/>
            : <div style={{ width: 140, height: 80, background:'#222', color:'#aaa',
                  display:'flex', alignItems:'center', justifyContent:'center',
                  fontSize:10, padding:4, textAlign:'center', wordBreak:'break-all' }}>
                {typeof s.ts === 'string' ? s.ts : 'saved'}
              </div>}
          <span onClick={() => removeShot(which, s.ts)}
            title="delete"
            style={{ position:'absolute', top:-8, right:-8, width:18, height:18,
              background:'#e53', color:'#fff', borderRadius:9, fontSize:12, lineHeight:'18px',
              textAlign:'center', cursor:'pointer', userSelect:'none',
              boxShadow:'0 1px 3px rgba(0,0,0,0.4)' }}>×</span>
        </div>
      ))}
    </div>
  );
  const CALIB_STREAM_PGID = 10105;
  const stopStream = () => {
    props.ACT_WS_SEND_BPG(props.CORE_ID, "CI", 0,
      { _PGID_: CALIB_STREAM_PGID, _PGINFO_: { keep: false } });
    props.ACT_WS_SEND_BPG(props.CORE_ID, "ST", 0,
      { CameraSetting: { trigger_mode: 1 } });
  };
  const startStream = () => {
    // Restate down_samp_level, not just trigger_mode: it is core-global state
    // that another page (or a lens-calibration run) may have moved since the
    // mount effect set it.
    props.ACT_WS_SEND_BPG(props.CORE_ID, "ST", 0,
      { CameraSetting: { trigger_mode: 0, down_samp_level: 1,
                         ROI: FULL_SENSOR_ROI } });
    props.ACT_WS_SEND_BPG(props.CORE_ID, "CI", 0, {
      _PGID_: CALIB_STREAM_PGID, _PGINFO_: { keep: true },
      definfo: { type: "stage_light_report", grid_size: [10, 10],
        nonBG_thres: 100, nonBG_spread_thres: 180 },
      IMG_ignore_calib: true,
    });
  };
  const runCalib = () => {
    setCalibrating(true);
    setCalibStatus('running');
    setCalibMsg('Running lens calibration…');
    stopStream();
    props.ACT_WS_SEND_BPG(props.CORE_ID, "RC", 0, {
      target: "lens_calibrate",
      dir: CALIB_DIR,
      square_mm: squareMm,
      lens_model: lensModel,
      out: `data/lens_calib.json`,
    }, undefined, {
      resolve: (pkts) => {
        const ss = pkts.find(p => p.type === "SS");
        const ok = ss && ss.data && ss.data.ACK === true;
        const rp = pkts.find(p => p.type === "RP");
        const rep = rp && rp.data;
        setCalibReport(rep || null);
        setCalibStatus(ok ? 'ok' : 'fail');
        if (ok && rep && rep.calib) {
          const c = rep.calib;
          setCalibMsg(
            `Calibration OK — RMS=${(+c.rms_px).toFixed(4)} px, ` +
            `m=${(+c.m).toFixed(4)} px/mm, um/px=${(+c.um_per_px).toFixed(3)}, ` +
            `${rep.image_count} image(s). Saved to ${rep.out_path}`);
        } else {
          setCalibMsg(ok ? 'Calibration complete. (no report payload)' :
            'Calibration failed. Check core log.');
        }
        if (ok) loadInstMmpp();   // the run just rewrote lens_calib.json
        startStream();
        setTimeout(() => setCalibrating(false), ok ? 4000 : 3500);
      },
      reject: (e) => {
        setCalibStatus('fail');
        setCalibMsg('Calibration RPC error: ' + (e && e.message ? e.message : String(e)));
        startStream();
        setTimeout(() => setCalibrating(false), 2500);
      },
    });
  };

  return (
    <div className="s width12 height12 overlayCon" style={{ padding: 12, overflowY: 'auto', overflowX: 'hidden', boxSizing: 'border-box' }}>
      <div ref={previewBoxRef} style={{ height: '60vh', position: 'relative', flexShrink: 0 }}
        onMouseMove={onPreviewHover} onMouseLeave={onPreviewLeave}>
        <PreviewCanvas_rdx addClass="s width12 height12"
          mmpp={instMmpp} onCanvasInit={onCanvasInit}/>
        {hoverBri !== undefined && (
          <div style={{
            position: 'absolute', left: hoverBri.sx + 14, top: hoverBri.sy + 14,
            pointerEvents: 'none', zIndex: 50,
            background: 'rgba(0,0,0,0.78)', color: '#fff',
            padding: '3px 7px', borderRadius: 4, fontSize: 12, lineHeight: 1.35,
            whiteSpace: 'nowrap', fontFamily: 'monospace'
          }}>
            <div>({hoverBri.ix}, {hoverBri.iy}) · 亮度 {hoverBri.lum}</div>
            <div style={{ color: '#bbb' }}>RGB {hoverBri.r},{hoverBri.g},{hoverBri.b}</div>
          </div>
        )}
      </div>
      <Card size="small" style={{ marginTop: 12 }}
        title={<span>Camera {loaded ? '' : <span style={{color:'#aaa'}}>(loading…)</span>}</span>}
        extra={<Button size="small" onClick={saveCameraSetting} disabled={!loaded}>Save</Button>}>
        <div style={{ display: 'flex', gap: 16, flexWrap: 'wrap', alignItems: 'center' }}>
          <label>exposure (µs):
            <InputNumber value={exposure} min={1} max={1000000} step={100}
              style={{ marginLeft: 6, width: 110 }}
              onChange={(v) => { const x = Math.max(1, v || 1); setExposure(x); setCam('exposure', x); }} />
          </label>
          <label>gain:
            <InputNumber value={gain} min={0} max={48} step={0.1}
              style={{ marginLeft: 6, width: 90 }}
              onChange={(v) => { const x = v || 0; setGain(x); setCam('gain', x); }} />
          </label>
          <label>gamma:
            <InputNumber value={gamma} min={0.1} max={4} step={0.05}
              style={{ marginLeft: 6, width: 90 }}
              onChange={(v) => { const x = v || 1; setGamma(x); setCam('gamma', x); }} />
          </label>
          <label>blacklevel:
            <InputNumber value={blacklevel} min={0} max={1000} step={1}
              style={{ marginLeft: 6, width: 90 }}
              onChange={(v) => { const x = v || 0; setBlacklevel(x); setCam('blacklevel', x); }} />
          </label>
        </div>
      </Card>

      <Tabs activeKey={tab} onChange={setTab} type="card" style={{ marginTop: 12 }}>
        <Tabs.TabPane tab={`Chessboard (${chessShots.length})`} key="chessboard">
          <Card size="small">
            <div style={{ display: 'flex', gap: 12, alignItems: 'center', flexWrap: 'wrap' }}>
              <label>square_mm:
                <InputNumber value={squareMm} step={0.1} min={0.01}
                  onChange={(v) => setSquareMm(v || 1)} style={{ marginLeft: 6 }} />
              </label>
              <label>lens_model:
                <Select value={lensModel} onChange={setLensModel} style={{ width: 140, marginLeft: 6 }}>
                  <Select.Option value="telecentric">telecentric</Select.Option>
                  <Select.Option value="perspective">perspective</Select.Option>
                </Select>
              </label>
              <Button type="primary" onClick={captureChess}>📷 Capture</Button>
              <span>shots: {chessShots.length}</span>
            </div>
            <ThumbStrip shots={chessShots} which="chess"/>
          </Card>
        </Tabs.TabPane>

        <Tabs.TabPane tab={`Bright field (${brightShots.length})`} key="bright">
          <Card size="small">
            <div style={{ display:'flex', gap:12, alignItems:'center', flexWrap:'wrap' }}>
              <label>rows:
                <InputNumber value={gridRows} min={2} max={128}
                  onChange={(v) => setGridRows(v || 16)} style={{ marginLeft:6, width:80 }}/>
              </label>
              <label>cols:
                <InputNumber value={gridCols} min={2} max={128}
                  onChange={(v) => setGridCols(v || 16)} style={{ marginLeft:6, width:80 }}/>
              </label>
              <label>frames:
                <InputNumber value={brightAvgN} min={1} max={512}
                  onChange={(v) => setBrightAvgN(v || 16)} style={{ marginLeft:6, width:80 }}/>
              </label>
              <Button type="primary" loading={fieldBusy} disabled={fieldBusy}
                onClick={() => captureField('bright')}>📷 Capture Bright</Button>
              <span style={{ color:'#888', fontSize:11 }}>
                turn backlight ON · last capture wins on calibrate
              </span>
            </div>
            <FieldThumbStrip shots={brightShots} which="bright"/>
          </Card>
        </Tabs.TabPane>

        <Tabs.TabPane tab={`Dark field (${darkShots.length})`} key="dark">
          <Card size="small">
            <div style={{ display:'flex', gap:12, alignItems:'center', flexWrap:'wrap' }}>
              <label>rows:
                <InputNumber value={gridRows} min={2} max={128}
                  onChange={(v) => setGridRows(v || 16)} style={{ marginLeft:6, width:80 }}/>
              </label>
              <label>cols:
                <InputNumber value={gridCols} min={2} max={128}
                  onChange={(v) => setGridCols(v || 16)} style={{ marginLeft:6, width:80 }}/>
              </label>
              <label>frames:
                <InputNumber value={darkAvgN} min={1} max={512}
                  onChange={(v) => setDarkAvgN(v || 16)} style={{ marginLeft:6, width:80 }}/>
              </label>
              <Button type="primary" loading={fieldBusy} disabled={fieldBusy}
                onClick={() => captureField('dark')}>📷 Capture Dark</Button>
              <span style={{ color:'#888', fontSize:11 }}>
                turn backlight OFF / cover lens · last capture wins on calibrate
              </span>
            </div>
            <FieldThumbStrip shots={darkShots} which="dark"/>
          </Card>
        </Tabs.TabPane>

      </Tabs>

      <div style={{ marginTop: 16, display:'flex', gap:12, alignItems:'center', flexWrap:'wrap' }}>
        <Button type="primary" size="large" onClick={runCalib}
          loading={calibrating}
          disabled={chessShots.length < 3 || calibrating}>
          Run Lens Calibration
        </Button>
        <Button type="primary" size="large" onClick={finalizeField}
          loading={fieldBusy}
          disabled={fieldBusy ||
            !( (brightStats || (savedField && savedField.hasBright)) &&
               (darkStats   || (savedField && savedField.hasDark)) )}>
          Run Field Calibration
        </Button>
        {savedField && (savedField.hasBright || savedField.hasDark) &&
         !brightStats && !darkStats && (
          <span style={{ color:'#888', fontSize:11 }}>
            using prior {savedField.hasBright ? 'bright' : ''}
            {savedField.hasBright && savedField.hasDark ? '+' : ''}
            {savedField.hasDark ? 'dark' : ''} from disk
          </span>
        )}
        <label style={{ marginLeft:8, color:'#aaa', fontSize:12 }}>
          vignette frac:
          <InputNumber value={vignetteFrac} min={0.05} max={0.95} step={0.05}
            onChange={(v) => setVignetteFrac(v || 0.5)} style={{ marginLeft:6, width:80 }}/>
        </label>
      </div>
      {fieldReport && !fieldReport.error && fieldReport.calib && fieldReport.calib.stats && (
        <Card size="small" style={{ marginTop:10 }} title="Field calibration result">
          <table style={{ fontSize:12, color:'#ddd', lineHeight:1.6 }}>
            <tbody>
              <tr><td style={{paddingRight:16}}>uniformity</td><td>{(+fieldReport.calib.stats.uniformity_pct).toFixed(1)} %</td></tr>
              <tr><td>dynamic range</td><td>{(+fieldReport.calib.stats.dynamic_range).toFixed(1)}</td></tr>
              <tr><td>vignette cells</td><td>{fieldReport.calib.stats.bright_vignette_cells}</td></tr>
              <tr><td>scratch/dust cleaned</td><td>{fieldReport.calib.stats.bright_rejected_cells}</td></tr>
              <tr><td>hot dark cells</td><td>{fieldReport.calib.stats.hot_cells}</td></tr>
              <tr><td>saved to</td><td style={{fontSize:11}}>{fieldReport.out_path}</td></tr>
            </tbody>
          </table>
        </Card>
      )}
      {fieldReport && fieldReport.error && (
        <div style={{ marginTop:8, color:'#ff7676' }}>✗ field calibration failed — check core log</div>
      )}
      {calibrating && (
        <div style={{ position:'fixed', inset:0, background:'rgba(0,0,0,0.55)',
          display:'flex', alignItems:'center', justifyContent:'center', zIndex:9999 }}>
          <div style={{ background:'#1b1b1b', color:'#eee', padding:'24px 32px',
            borderRadius:8, minWidth:320, textAlign:'center',
            boxShadow:'0 4px 20px rgba(0,0,0,0.6)' }}>
            {calibStatus === 'running' && (
              <>
                <Spin size="large" />
                <div style={{ marginTop:16, fontSize:14 }}>{calibMsg}</div>
                <div style={{ marginTop:6, fontSize:11, color:'#888' }}>
                  Camera stream paused — {chessShots.length} image(s)
                </div>
              </>
            )}
            {calibStatus === 'ok' && (
              <div style={{ color:'#6cf28a', fontSize:13, textAlign:'left' }}>
                <div style={{ fontSize:15, marginBottom:8 }}>✓ Calibration complete</div>
                {calibReport && calibReport.calib && (
                  <table style={{ fontSize:12, color:'#ddd', lineHeight:1.6 }}>
                    <tbody>
                      <tr><td style={{paddingRight:12}}>model</td><td>{calibReport.calib.lens_model}</td></tr>
                      <tr><td>RMS error</td><td>{(+calibReport.calib.rms_px).toFixed(4)} px</td></tr>
                      <tr><td>magnification</td><td>{(+calibReport.calib.m).toFixed(4)} px/mm</td></tr>
                      <tr><td>scale</td><td>{(+calibReport.calib.um_per_px).toFixed(3)} µm/px</td></tr>
                      <tr><td>principal pt</td><td>({(+calibReport.calib.u0).toFixed(1)}, {(+calibReport.calib.v0).toFixed(1)})</td></tr>
                      <tr><td>images used</td><td>{calibReport.image_count}</td></tr>
                      <tr><td>saved to</td><td style={{fontSize:11}}>{calibReport.out_path}</td></tr>
                    </tbody>
                  </table>
                )}
                <div style={{ marginTop:10, fontSize:11, color:'#888' }}>{calibMsg}</div>
              </div>
            )}
            {calibStatus === 'fail' && (
              <div style={{ color:'#ff7676', fontSize:14 }}>✗ {calibMsg}</div>
            )}
          </div>
        </div>
      )}
    </div>
  );
}

const mapStateToProps = (state) => ({
  CORE_ID: state.ConnInfo.CORE_ID,
});
const mapDispatchToProps = (dispatch) => ({
  ACT_WS_SEND_BPG: (CORE_ID, tl, prop, data, uintArr, promiseCBs) =>
    dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, tl, prop, data, uintArr, promiseCBs)),
});

export default connect(mapStateToProps, mapDispatchToProps)(CalibrationUI);
