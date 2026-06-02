// New Calibration UI — chessboard (lens) + bright/dark field capture.
// Stage 1: scaffold only. Real capture / BPG wiring lands in follow-up tasks
// (see #63-#67). Keep BackLightCalibUI alongside until this fully replaces it.
import React, { useState, useEffect } from 'react';
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
  const [chessShots, setChessShots] = useState([]);  // [{path, ts}]
  const [brightAvgN, setBrightAvgN] = useState(16);
  const [darkAvgN, setDarkAvgN] = useState(16);
  const [bright, setBright] = useState(null);        // {W, H, data}
  const [dark, setDark] = useState(null);

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
    // TODO #64 hook: SV __LAST_DATA_VIEW_CACHE_IMG__ -> ./data/calib/<sess>/chess_NN.png
    setChessShots([...chessShots, { ts: Date.now() }]);
  };
  const captureField = (which) => {
    // TODO #64 hook: __AVERAGE_N_FRAMES_TO_GRID__ BPG -> returns {W,H,grid[]}
    const stub = { W: 32, H: 24, data: [] };
    (which === 'bright' ? setBright : setDark)(stub);
  };
  const runCalib = () => {
    // TODO #65 hook: cmd "lens_calibrate" -> report JSON
    console.log('runCalib', { squareMm, lensModel, chessShots, bright, dark });
  };

  return (
    <div className="s width12 height12 overlayCon" style={{ padding: 12 }}>
      <div style={{ height: '60vh', position: 'relative' }}>
        <PreviewCanvas_rdx addClass="s width12 height12"/>
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
          </Card>
        </Tabs.TabPane>

        <Tabs.TabPane tab={`Bright field ${bright ? '✓' : ''}`} key="bright">
          <Card size="small">
            <label>frames to average:
              <InputNumber value={brightAvgN} min={1} max={256}
                onChange={(v) => setBrightAvgN(v || 16)} style={{ marginLeft: 6 }} />
            </label>
            <Button type="primary" style={{ marginLeft: 12 }} onClick={() => captureField('bright')}>
              📷 Capture
            </Button>
            {bright && <span style={{ marginLeft: 12 }}>captured {bright.W}×{bright.H}</span>}
          </Card>
        </Tabs.TabPane>

        <Tabs.TabPane tab={`Dark field ${dark ? '✓' : ''}`} key="dark">
          <Card size="small">
            <label>frames to average:
              <InputNumber value={darkAvgN} min={1} max={256}
                onChange={(v) => setDarkAvgN(v || 16)} style={{ marginLeft: 6 }} />
            </label>
            <Button type="primary" style={{ marginLeft: 12 }} onClick={() => captureField('dark')}>
              📷 Capture
            </Button>
            {dark && <span style={{ marginLeft: 12 }}>captured {dark.W}×{dark.H}</span>}
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
