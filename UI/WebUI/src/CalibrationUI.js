// New Calibration UI — chessboard (lens) + bright/dark field capture.
// Stage 1: scaffold only. Real capture / BPG wiring lands in follow-up tasks
// (see #63-#67). Keep BackLightCalibUI alongside until this fully replaces it.
import React, { useState, useEffect, useRef } from 'react';
import { connect } from 'react-redux';
import Tabs from 'antd/lib/tabs';
import Button from 'antd/lib/button';
import Card from 'antd/lib/card';
import InputNumber from 'antd/lib/input-number';
import Select from 'antd/lib/select';
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
    // EditDBInfoSync calls scaleImageToFitScreen() every time img changes,
    // which would reset the user's pan/zoom on every streamed frame. Run the
    // full sync once to set db_obj + initial fit; after that, push only the
    // new img frame via SetImg.
    if (!this._didInitialFit) {
      this.ec_canvas.EditDBInfoSync(props.edit_info);
      if (props.edit_info && props.edit_info.img) this._didInitialFit = true;
    } else if (props.edit_info && props.edit_info.img) {
      this.ec_canvas.SetImg(props.edit_info.img);
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

  const CALIB_DIR = "data/calibImages";
  const canvasRef = useRef(null);

  // On mount, list any already-saved chessboard images so the user sees them
  // (with placeholder thumbs -- binary PNG read over BPG is not wired yet).
  useEffect(() => {
    const lazyLoadThumbs = (shots, setter) => {
      shots.forEach((shot) => {
        props.ACT_WS_SEND_BPG(props.CORE_ID, "LB", 0, { filename: shot.path }, undefined, {
          resolve: (rpkts) => {
            const bl = rpkts.find(p => p.type === "BL");
            if (!bl || !bl.rawdata || !bl.rawdata.byteLength) return;
            const blob = new Blob([new Uint8Array(bl.rawdata.buffer, bl.rawdata.byteOffset, bl.rawdata.byteLength)], { type: 'image/png' });
            const url = URL.createObjectURL(blob);
            setter(prev => prev.map(s => s.ts === shot.ts ? { ...s, thumb: url } : s));
          },
          reject: () => {},
        });
      });
    };
    props.ACT_WS_SEND_BPG(props.CORE_ID, "FB", 0, { path: CALIB_DIR, depth: 1 }, undefined, {
      resolve: (pkts) => {
        const fs = pkts.find(p => p.type === "FS");
        if (!fs || !fs.data || !fs.data.files) return;
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
      },
      reject: () => {},
    });
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

  const saveCameraSetting = () => {
    const payload = new TextEncoder().encode(JSON.stringify(
      { exposure, gain, gamma, blacklevel }, null, 2));
    props.ACT_WS_SEND_BPG(props.CORE_ID, "SV", 0, { filename: SETTING_PATH }, payload, {
      resolve: (pkts) => {
        const ss = pkts.find(p => p.type === "SS");
        if (ss && ss.data.ACK) console.log("saved", SETTING_PATH);
        else console.warn("save failed", SETTING_PATH);
      },
    });
  };

  // Mirror InspMode trigger policy AND register a CI subscription. trigger_mode
  // alone doesn't push frames; core streams only when a CI is active. We reuse
  // BackLightCalibUI's stage_light_report PGID -- lightest published path that
  // delivers raw frames + ignores any in-process lens calibration.
  useEffect(() => {
    const CALIB_STREAM_PGID = 10105;
    props.ACT_WS_SEND_BPG(props.CORE_ID, "ST", 0,
      { CameraSetting: { trigger_mode: 0 } });
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
  }, []);

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
    // Single-shot for now; averaging-to-grid (task #64) will replace this.
    const thumb = grabThumb();
    const ts = new Date().toISOString().replace(/[:.]/g, '-').replace('T', '_').replace('Z', '');
    const prefix = which === 'bright' ? 'bright_' : 'dark_';
    const name = `${prefix}${ts}.png`;
    const filename = `${CALIB_DIR}/${name}`;
    const entry = { ts: name, path: filename, thumb, persisted: false };
    const setter = which === 'bright' ? setBrightShots : setDarkShots;
    setter(s => [...s, entry]);
    props.ACT_WS_SEND_BPG(props.CORE_ID, "SV", 0,
      { filename, make_dir: true, type: "__LAST_DATA_VIEW_CACHE_IMG__" }, undefined, {
        resolve: (pkts) => {
          const ss = pkts.find(p => p.type === "SS");
          const ok = ss && ss.data && ss.data.ACK === true;
          setter(s => ok
            ? s.map(x => x.ts === name ? { ...x, persisted: true } : x)
            : s.filter(x => x.ts !== name));
        },
        reject: () => setter(s => s.filter(x => x.ts !== name)),
      });
  };
  const removeShot = (which, ts) => {
    const bucket = which === 'chess' ? chessShots : which === 'bright' ? brightShots : darkShots;
    const setter = which === 'chess' ? setChessShots : which === 'bright' ? setBrightShots : setDarkShots;
    const shot = bucket.find(s => s.ts === ts);
    setter(bucket.filter(s => s.ts !== ts));
    // Best-effort file delete (no core handler yet for __DELETE__ -- UI-only
    // for now; will leave the file on disk).
    if (shot && shot.path) {
      props.ACT_WS_SEND_BPG(props.CORE_ID, "SV", 0,
        { filename: shot.path, type: "__DELETE__" });
    }
  };

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
  const runCalib = () => {
    // Triggers core to run lens_calib_run_from_images on the saved folder.
    // Requires the matching BPG handler in wiringPanel (task #65 -- not yet
    // landed). Until then, this logs the intent and surfaces any error.
    props.ACT_WS_SEND_BPG(props.CORE_ID, "RC", 0, {
      target: "lens_calibrate",
      dir: CALIB_DIR,
      square_mm: squareMm,
      lens_model: lensModel,
      out: `${CALIB_DIR}/lens_calib.json`,
    }, undefined, {
      resolve: (pkts) => {
        const rp = pkts.find(p => p.type === "RP" || p.type === "FL");
        console.log("[calib] result", rp ? rp.data : pkts);
      },
      reject: (e) => console.warn("[calib] failed", e),
    });
  };

  return (
    <div className="s width12 height12 overlayCon" style={{ padding: 12 }}>
      <div style={{ height: '60vh', position: 'relative' }}>
        <PreviewCanvas_rdx addClass="s width12 height12"
          onCanvasInit={(c) => { canvasRef.current = c; }}/>
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
            <InputNumber value={blacklevel} min={0} max={255} step={1}
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
            <label>frames to average:
              <InputNumber value={brightAvgN} min={1} max={256}
                onChange={(v) => setBrightAvgN(v || 16)} style={{ marginLeft: 6 }} />
            </label>
            <Button type="primary" style={{ marginLeft: 12 }} onClick={() => captureField('bright')}>
              📷 Capture
            </Button>
            <ThumbStrip shots={brightShots} which="bright"/>
          </Card>
        </Tabs.TabPane>

        <Tabs.TabPane tab={`Dark field (${darkShots.length})`} key="dark">
          <Card size="small">
            <label>frames to average:
              <InputNumber value={darkAvgN} min={1} max={256}
                onChange={(v) => setDarkAvgN(v || 16)} style={{ marginLeft: 6 }} />
            </label>
            <Button type="primary" style={{ marginLeft: 12 }} onClick={() => captureField('dark')}>
              📷 Capture
            </Button>
            <ThumbStrip shots={darkShots} which="dark"/>
          </Card>
        </Tabs.TabPane>
      </Tabs>

      <div style={{ marginTop: 16 }}>
        <Button type="primary" size="large" onClick={runCalib}
          disabled={chessShots.length < 3}>
          Run Calibration
        </Button>
        <span style={{ marginLeft: 12, color: '#888' }}>
          (scaffold — wiring TODO: tasks #63-#67)
        </span>
      </div>
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
