'use strict'


import { connect } from 'react-redux';
import React, { useState, useEffect,useRef } from 'react';
import { useSelector,useDispatch } from 'react-redux';

import * as BASE_COM from './component/baseComponent.jsx';
import ComponentBoundary from './component/ComponentBoundary';
import ReactResizeDetector from 'react-resize-detector';
import dateFormat from "dateformat";
import INFO from './info.js';
import { TagOptions_rdx,UINSP_UI ,SLID_UI} from './component/rdxComponent.jsx';
import { UINSP_ESP32_MINI, UINSP_ESP32_UI, runSequence } from './component/uInspESP32_UI.jsx';
import { StationRegionPanel } from './component/StationRegionPanel.jsx';
import dclone from 'clone';
import Color from 'color';
import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import { websocket_autoReconnect, websocket_reqTrack, copyToClipboard, ConsumeQueue,defFileGeneration,stampRefImagePath,GetObjElement,dictLookUp} from 'UTIL/MISC_Util';
import { SHAPE_TYPE, DEFAULT_UNIT } from 'REDUX_STORE_SRC/actions/UIAct';
import { MEASURERSULTRESION, MEASURERSULTRESION_reducer } from 'UTIL/InspectionEditorLogic';
import { usePerifConn, getPerifAPI } from './perif/PerifAPI';
import { withPerifConns } from './perif/PerifStatus';
import { DEF_EXTENSION, CameraTransferCtrl as CameraCtrl } from 'UTIL/BPG_Protocol';
import { mkLog } from 'UTIL/logger';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import {TagDisplay_rdx} from './component/rdxComponent.jsx';
// import { PageHeader } from 'antd/lib/page-header';
//import {Doughnut} from 'react-chartjs-2';

import { round } from 'UTIL/MISC_Util';
const log = mkLog('ui.insp');

import Row from 'antd/lib/row';
import Col from 'antd/lib/col';
import Slider from 'antd/lib/slider';
import message from 'antd/lib/message';
import Checkbox from 'antd/lib/checkbox'
import Popover from 'antd/lib/popover';
import Table from 'antd/lib/table';
import { acceptanceFloor, headroom } from 'UTIL/matchThreshold';
import Switch from 'antd/lib/switch';
import Tag from 'antd/lib/tag';
import Input from 'antd/lib/input';
import InputNumber from 'antd/lib/input-number';

import Select from 'antd/lib/select';
import Button, { default as AntButton } from 'antd/lib/button';
import Menu from 'antd/lib/menu';


import {
  DisconnectOutlined,
  FileOutlined,
  LinkOutlined,
  ExclamationCircleOutlined,
  RedoOutlined,
  ExpandOutlined,
  ArrowLeftOutlined,
  FullscreenOutlined,
  SettingOutlined,
  CaretDownOutlined,
  BarChartOutlined,
  SaveOutlined,
  EyeInvisibleOutlined,
} from '@ant-design/icons';
import { stripOverlayOnly } from './UTIL/dbRecord';
import { pickCtrlMargin } from './UTIL/ctrlMarginPick';
import { pendingInsertCount, droppedCount } from './UTIL/inspDBQueue';



import Divider from 'antd/lib/divider';

import Chart from 'chart.js';
import 'chartjs-plugin-annotation';
import Modal from "antd/lib/modal";
// import Upload from 'antd/lib/upload';
// import Input from 'antd/lib/input';
import Dropdown from 'antd/lib/dropdown'

import Typography from 'antd/lib/typography';
const { Paragraph, Title } = Typography;


// import Tag from 'antd/lib/tag';
// import Select from 'antd/lib/select';
// import Menu from 'antd/lib/menu';
// import Button from 'antd/lib/button';
// import Icon from 'antd/lib/icon';

let stream_PGID_=10004;

function urlConcat(base,add)
{
  let xbase=base;
  while(xbase.charAt(xbase.length-1)=="/")
    xbase=xbase.slice(0, xbase.length-1)
    
  let xadd=add;
  while(xadd.charAt(0)=="/")
    xadd=xadd.slice(1, xbase.length)
  

  return xbase+"/"+xadd;
}

const ButtonGroup = Button.Group;

const Option = Select.Option;

const selectBefore = (
  <Select defaultValue="Http://" style={{ width: 90 }}>
    <Option value="Http://">Http://</Option>
    <Option value="Https://">Https://</Option>
  </Select>
);
const selectAfter = (
  <Select defaultValue=".com" style={{ width: 80 }}>
    <Option value=".com">.com</Option>
    <Option value=".jp">.jp</Option>
    <Option value=".cn">.cn</Option>
    <Option value=".org">.org</Option>
  </Select>
);
const SubMenu = Menu.SubMenu;
const MenuItemGroup = Menu.ItemGroup;


// THE LOCATOR IS NOT THE ONE THE DEF WAS BUILT AROUND.
//
// This is the quiet failure the whole SBM cache mechanism can produce: the
// trained features stop matching the def's own settings, the core refuses them,
// and sig360 picks the part up instead. Every measurement then passes, the
// verdict panel is green, and the ONLY difference from the def running as
// designed is a string in the report envelope.
//
// So it gets a banner rather than a status pill. Somebody has to be told that
// the recipe on the screen is not the recipe being executed.
function LocateNoteBanner() {
  const note = useSelector((s) => s.UIData.edit_info.locateNote);
  if (!note) return null;
  // On the CODE, never on the prose. `reason` is written for a human and will
  // be reworded; a screen that branches on its wording stops working that day
  // and shows nothing, which is the same silence this banner exists to break.
  //
  // A working region that rejected objects is a DIFFERENT problem -- the part
  // is not at the station -- and it is normal on an empty conveyor, so it is
  // not shouted about. Only the two fallback codes take the banner.
  const isFallback = note.code === 'untrained' || note.code === 'train_failed';
  if (!isFallback) return null;
  return <div style={{
      background: '#a8071a', color: '#fff', padding: '4px 10px',
      fontSize: 13, fontWeight: 600, display: 'flex', gap: 10, alignItems: 'center' }}>
    <span>⚠ 這個 def 沒有在用 SBM 定位</span>
    <span style={{ fontWeight: 400, fontSize: 12, opacity: 0.9 }}>
      訓練好的特徵與設定不符,已退回 sig360。量測結果看起來正常,但用的不是這個 def 設計的定位方式
      —— 進 Shape-based 定位設定按「生成特徵點」再存檔。
    </span>
  </div>;
}

// insert_skip: upload 1 report out of every N. Defaults to 1 (upload all), NOT
// 0 -- `total % 0` is NaN, NaN != 0, so a zero here skips every single report
// and the machine uploads nothing while looking like it is working.
function InspectionReportInsert2DB({onDBInsertSuccess,onDBInsertFail,LANG_DICT,insert_skip=1})
{

  const _s = useRef({sendCounter:0,sendedCounter:0,totalCounter:0,pre_newAddedReport:undefined});

  let _this=_s.current;
  // const c_state = useSelector(state => state.UIData.c_state);
  const dispatch = useDispatch();
  const Insp_DB_W_ID = useSelector(state => state.ConnInfo.Insp_DB_W_ID);
  const Insp_DB_W_ID_CONN_INFO = useSelector(state => state.ConnInfo.Insp_DB_W_ID_CONN_INFO);
  const newAddedReport = useSelector(state => state.UIData.edit_info.reportStatisticState.newAddedReport);

  const WS_SEND= (id,data,return_cb) => dispatch(UIAct.EV_WS_SEND_PLAIN(id,data,return_cb));

  // How much is waiting, and how much has been thrown away.
  //
  // The banner said "disconnected" and then three send counters, which answer
  // "is it up" and "how many did I try". Neither answers the question an
  // operator actually has during an outage: is my data safe, and for how much
  // longer. Second 44 and second 45 of an outage looked identical while the
  // second one was destroying records.
  //
  // Polled slowly on purpose. It is an IndexedDB count on the renderer thread,
  // it changes at part rate, and nobody needs it to the second -- 2 s is often
  // enough to watch a buffer fill and cheap enough to leave running.
  const [dbQ, setDbQ] = useState({ pending: 0, dropped: 0 });
  useEffect(() => {
    let live = true;
    const tick = () => {
      Promise.resolve(pendingInsertCount())
        .then((n) => { if (live) setDbQ({ pending: n, dropped: droppedCount() }); })
        .catch(() => {});
    };
    tick();
    const h = setInterval(tick, 2000);
    return () => { live = false; clearInterval(h); };
  }, []);
  
  useEffect(()=>{
    if(newAddedReport===undefined || 
      _this.pre_newAddedReport===newAddedReport || 
      Array.isArray(newAddedReport)==false ||
      newAddedReport.length==0)//there is no new report
    {
      // console.log("no report...");
      return;
    }

    _this.pre_newAddedReport=newAddedReport;
    // console.log(newAddedReport,insert_skip);
    
    const _skip = (Number.isFinite(insert_skip) && insert_skip >= 1) ? insert_skip : 1;
    let res=_this.totalCounter%_skip;
    _this.totalCounter++;
    if(res!=0)
    {
      // console.log("SKIP...");
      return;
    }

    _this.sendCounter++;
    // Overlay-only fields never reach the archive. See UTIL/dbRecord.js for
    // what is dropped and why; the screen still has the full report, because
    // the prune shares structure and copies nothing that it does not change.
    WS_SEND(Insp_DB_W_ID,stripOverlayOnly(newAddedReport))
    .then(retInfo=>{
      _this.sendedCounter++;
      onDBInsertSuccess(retInfo);
    })
    .catch(err=>{

    })
  },[newAddedReport]);


  // let retx=
  //   this.props.ACT_WS_GET_OBJ(this.props.WS_InspDataBase_W_ID, (obj)=>{

  //     console.log(obj);
  //     return obj.websocket.send_obj({type:"PING"});
  //   })
  //   .then(d=>{
  //     console.log(d);
  //   })
  //   .catch(e=>{
  //     console.log(e);

  //   })

  let isConnected=GetObjElement(Insp_DB_W_ID_CONN_INFO,["type"])==="WS_CONNECTED";

      

  // return null;


  return <Button type="primary" size={"large"} 
    className={ (isConnected ? "blackText lgreen" : "DISCONNECT_Blink")}
    icon={isConnected ? <LinkOutlined /> : <DisconnectOutlined />} >
        {(isConnected ? LANG_DICT.connection.server_connected : LANG_DICT.connection.server_disconnected)
        + " " + _this.sendedCounter+"<"+_this.sendCounter + ":" + _this.totalCounter + "/" + insert_skip
        + (dbQ.pending ? "  待補傳 " + dbQ.pending : "")}
        {/* Loud and separate. A discarded record is not a delayed one, and it
            must not read as another counter in the same grey run-on. */}
        {dbQ.dropped ? <span style={{ marginLeft: 8, padding: '0 6px', borderRadius: 3,
                                      background: '#a8071a', color: '#fff', fontWeight: 700 }}>
          已丟棄 {dbQ.dropped}
        </span> : null}
    </Button>
}



function SLID_InspMonitor({})
{

  let _this= useRef({}).current;
  return <SLID_UI SIMPLE_CTRL_UI/>;
}


// <InspectionReportInsert2DB 
// reportStatisticState={this.props.reportStatisticState} 

// // DBStatus,
// // DBPushPromise,
// onDBInsertSuccess={(data, info) => {
//   // log.info(data, info);
//   this.setState({ inspUploadedCount: this.state.inspUploadedCount + 1 });
// }}
// onDBInsertFail={(data, info) => {
//   log.error(data, info);
// }}
// insert_skip={InspectionReportPullSkip}/>







class DB extends React.Component {
  constructor(props) {
    super(props);
    this.resultDB = undefined;
    this.state = {
      db_open: false,
      resultX: undefined
    }
  }
  componentDidMount() {

  }
  echoTime() {
    return "" + new Date().getSeconds();
  }
  query() {
    this.resultDB = "[!]";
    fetch('http://127.0.0.1:4000/gui', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
      },
      body: JSON.stringify({ query: "{ hello }" })
    }).then(r => r.json()).then((datax) => {
      log.info('[O]DB data returned:', JSON.stringify(datax));

      this.resultDB = JSON.stringify(datax);
    });
    return this.resultDB;
  }
  render() {
    return (
      <div style={{ 'display': 'inline-block' }}>
        {/*{this.echoTime()}*/}
        <p>
          {/*{this.query()}*/}
        </p>
      </div>
    );
  }
}


// How the VALUE cell is painted for each verdict.
//
// This replaces the OK/NG tag column. The tag spent a fixed 56 px on every row
// to repeat what the number itself can carry, and in a 366 px strip that width
// is better spent on the measurement name. Colouring the value says the same
// thing in no space at all.
//
// Deliberately asymmetric: a passing row stays quiet (no fill, black figures)
// and a failing one is filled solid. A 全檢 operator is scanning for the
// exceptions, so the exceptions are what should carry the ink -- a page where
// every row is coloured is a page where nothing stands out.
// The FIGURES are coloured, never the cell. A filled cell sits on top of the
// margin bar, which is painted into the row's own background -- so the fill
// would hide exactly the thing the row exists to show.
//
// Three colours, from the VERDICT -- not a ramp off the margin.
//
// A ramp was tried and dropped: with the shade changing continuously there is
// no fixed thing to recognise, so the colour stops being a category and turns
// into a second, blurrier copy of the number that is already there. Three
// states are three states.
//
// Where the value sits inside its limits is the BAR's job, and the bar says it
// precisely.
// First finite number of the candidates, else undefined. The limits arrive
// from two places and either can be missing or NaN.
function num(...xs) {
  for (const x of xs) if (typeof x === 'number' && Number.isFinite(x)) return x;
  return undefined;
}

function valueInk(detailStatus, ratio, blank) {
  if (blank) return { fg: "#bfbfbf", w: 400 };
  // NA / UNSET is "no measurement", not "a good measurement".
  //
  // The fall-through at the bottom returns green, so simplifying this function
  // to three states quietly painted every NaN reading as a pass -- the one
  // colour it must never be. A measurement that did not happen is grey, the
  // same grey as an empty slot, because that is what it is.
  if (detailStatus === MEASURERSULTRESION.NA
      || detailStatus === MEASURERSULTRESION.UNSET
      || detailStatus === undefined) {
    return { fg: "#bfbfbf", w: 400 };
  }
  if (NG_STATUSES.has(detailStatus)) return { fg: "#f5222d", w: 700 };
  // Outside the production control limits but still inside spec.
  //
  // A traffic light: green, amber, red. The first attempt at this was a green
  // leaning yellow, chosen so a passing part would never look like a reject --
  // but it landed close enough to the green that the two were hard to tell
  // apart at a glance, which loses the warning altogether. Amber is the whole
  // point of the middle light: unmistakably not green, and unmistakably not
  // red. Darkened from a pure yellow because these are figures on white that
  // an operator transcribes, and a bright yellow is not readable there.
  if (CAUTION_STATUSES.has(detailStatus)) return { fg: "#c89000", w: 700 };
  return { fg: "#389e0d", w: 400 };
}

// Which detailStatus values count as a failure worth naming when a group is
// collapsed. The C-variants are the caution band, which is not a failure.
const CAUTION_STATUSES = new Set([
  MEASURERSULTRESION.UCNG, MEASURERSULTRESION.LCNG, MEASURERSULTRESION.CNG,
]);

const NG_STATUSES = new Set([
  MEASURERSULTRESION.USNG, MEASURERSULTRESION.LSNG,
  MEASURERSULTRESION.SNG,  MEASURERSULTRESION.NG,
]);

// What a slot renders when it currently has no measurement in it.
//
// It must go through the SAME JSX as a real row: the point of a slot pool is
// that the nodes are never created or destroyed, so an empty slot returning
// null would give back exactly what the pool exists to avoid. The row is
// hidden with display:none instead, which keeps every node in the document.
// NOT exported. A module that exports components must export nothing else, or
// React Fast Refresh gives up on it and invalidates upward -- which turned a
// one-second style edit back into a full reload of MAINUI.
const EMPTY_MEASURE_REP = Object.freeze({
  name: '', value: NaN, status: MEASURERSULTRESION.NA,
  detailStatus: MEASURERSULTRESION.NA, def: Object.freeze({}),
});

// Not exported either, and nothing outside this file ever imported it. See the
// note on EMPTY_MEASURE_REP: one stray non-component export costs Fast Refresh
// for the whole module.
const OK_NG_BOX_COLOR_TEXT = {
  [MEASURERSULTRESION.UNSET]: { COLOR: "#999", TEXT: MEASURERSULTRESION.UNSET },
  [MEASURERSULTRESION.NA]: { COLOR: "#aaa", TEXT: MEASURERSULTRESION.NA },

  [MEASURERSULTRESION.UOK]: { COLOR: "#87d068", TEXT: MEASURERSULTRESION.UOK },
  [MEASURERSULTRESION.LOK]: { COLOR: "#87d068", TEXT: MEASURERSULTRESION.LOK },
  [MEASURERSULTRESION.OK]: { COLOR: "#87d068", TEXT: MEASURERSULTRESION.OK },

  [MEASURERSULTRESION.UCNG]: { COLOR: "#d0d068", TEXT: MEASURERSULTRESION.UCNG },
  [MEASURERSULTRESION.LCNG]: { COLOR: "#d0d068", TEXT: MEASURERSULTRESION.LCNG },
  [MEASURERSULTRESION.CNG]: { COLOR: "#d0d068", TEXT: MEASURERSULTRESION.CNG },

  [MEASURERSULTRESION.USNG]: { COLOR: "#f50", TEXT: MEASURERSULTRESION.USNG },
  [MEASURERSULTRESION.LSNG]: { COLOR: "#f50", TEXT: MEASURERSULTRESION.LSNG },
  [MEASURERSULTRESION.SNG]: { COLOR: "#f50", TEXT: MEASURERSULTRESION.SNG },
  [MEASURERSULTRESION.NG]: { COLOR: "#f50", TEXT: MEASURERSULTRESION.SNG },
};

// One measurement group, as its own subtree.
//
// Rendered from DATA in both places that show it -- the strip on the left and
// the fullscreen modal -- so each gets its own DOM and neither can be holding
// the other's. Passing React ELEMENTS between the two was the previous design
// and it leaked; see the note on ObjInfoList.render.
class ResultGroupItems extends React.PureComponent {
  render() {
    const { group, DICT, onFullScreen, slots, ghostReports } = this.props;
    // With no group, fall back to the last list this slot showed. The names
    // are then guaranteed to match what was actually being measured, in the
    // same order and the same slots -- which reading them back out of the
    // recipe would not guarantee.
    const reports = (group && group.reports) || ghostReports || [];
    const ghost = !group;
    // Keyed by SLOT, not by measurement name.
    //
    // That is the whole change and also its only real risk: with a name key,
    // React destroys a row the moment its measurement leaves and builds a new
    // one when the next arrives, and at 23 parts/s that churn was the entire
    // 410..1080 swing in getDOMCounters -- 560 nodes that were never in the
    // document, created and discarded between two samples. With a slot key the
    // same row object stays mounted and only its text changes.
    //
    // The risk is that a slot showing the wrong report puts part A's number on
    // part B's line, which looks completely normal on screen. Nothing here is
    // cached across renders: slot j is handed reports[j] every time, so the
    // mapping cannot drift -- but any future change that memoises per slot has
    // to preserve that.
    const n = Math.max(slots || 0, reports.length, 1);
    const out = [];
    for (let j = 0; j < n; j++) {
      out.push(
        // Slot 0 stays visible even with nothing in it, showing "-". A report
        // that comes and goes otherwise takes the whole column with it and
        // everything below jumps.
        <InspectionResultDisplay DICT={DICT} key={"slot" + j} placeholder={j === 0}
          ghost={ghost} singleInspection={reports[j]}
          fullScreenToggleCallback={onFullScreen} />
      );
    }
    // table-layout:fixed so the columns come from the colgroup and not from
    // the content: a long measurement name must not be able to push the value
    // column narrower on one row than on the next.
    // separate, NOT collapse -- see the note on the row background. With
    // border-collapse:collapse a <tr> has no background box of its own, so
    // background-size and background-position on it are ignored and the scale
    // floods the whole row height.
    //
    // (A JSX comment here instead would be a second root expression in the
    // return, which is a parse error -- made twice now.)
    return (
      <table style={{ width: "100%", tableLayout: "fixed",
                      borderCollapse: "separate", borderSpacing: 0,
                      background: "#fff" }}>
        <colgroup>
          <col />
          <col style={{ width: 108 }} />
        </colgroup>
        <tbody>{out}</tbody>
      </table>
    );
  }
}

// The header of one object's group: identity, verdict, and a collapse toggle.
//
// `group` is undefined for an empty slot. Render the same nodes with no text
// rather than bailing out: returning null here would unmount the icon
// (span+svg+path) and the badge on every slot that empties, which is the churn
// the pool exists to remove.
//
// It carries its own background. It had none, so it showed whatever was behind
// it and read as a gap between the cards rather than as the thing that owns
// them -- and with the cards now on white, a transparent header has nothing to
// separate them at all.
function ResultGroupTitle({ group, slot, collapsed, simThres, onToggle, onFullScreen }) {
  // Collapsed, the header IS the readout: which measurements failed. A verdict
  // alone says a part is bad without saying what about it is bad, and that is
  // the one thing worth showing when the numbers are hidden.
  let ngNames = '';
  if (collapsed && group) {
    const bad = group.reports.filter(
      (r) => NG_STATUSES.has(r.detailStatus)).map((r) => r.name);
    ngNames = bad.length ? bad.join('、') : '';
  }
  return (
    <div data-slot={slot} onClick={onToggle}
      style={{ display: "flex", alignItems: "center", gap: 8, cursor: "pointer",
               background: "#fafafa", borderBottom: "1px solid #e8e8e8",
               padding: "5px 10px", color: "#595959", userSelect: "none" }}>
      <span style={{ flex: "0 0 auto", fontSize: 10, color: "#8c8c8c",
                     transform: collapsed ? "none" : "rotate(90deg)",
                     transition: "transform .12s" }}>&#9654;</span>
      {/* Moved up from every row. It opens one modal regardless of which row
          it was clicked on, so one per group is the same control. */}
      <FullscreenOutlined onClick={onFullScreen}
        style={{ flex: "0 0 auto", fontSize: 12, color: "#8c8c8c" }} />
      <span style={{ flex: "0 0 auto", fontSize: 13 }}>
        {group ? `${group.idx}  ${group.isFlipped ? "反" : "正"}` : ''}
      </span>
      {/* MATCH SCORE. Headroom above the acceptance threshold, which is the
          number to tune against: the core only reports a part it already
          accepted, so this is never "did it match" -- it is "by how much".
          Amber inside 0.02 of the threshold means the next shift's dust or
          lighting drift is what decides, and that is worth seeing BEFORE the
          part stops being found. Hidden entirely when the locator does not
          produce a score, rather than shown as a dash that reads like 0. */}
      {group && Number.isFinite(group.similarity) &&
        <span style={{ flex: "0 0 auto", fontSize: 11, fontVariantNumeric: "tabular-nums",
                       // Colour by HEADROOM, not by a fixed distance. "Within
                       // 0.02 of the gate" means something different against a
                       // 0.50 floor than against a 0.90 one; the fraction of
                       // the remaining range means the same thing against both,
                       // which is what lets one rule serve two locators.
                       color: headroom(group.similarity, simThres) >= 0.15 ? "#8c8c8c"
                            : group.similarity >= simThres ? "#d46b08" : "#cf1322" }}
          title={`比對分數 ${group.similarity.toFixed(4)}／接受門檻 ${simThres.toFixed(2)}`}>
          {group.similarity.toFixed(3)}
        </span>}
      {/* Only when collapsed, and it takes the slack so the badge stays put. */}
      <span style={{ flex: "1 1 auto", minWidth: 0, fontSize: 12, color: "#f50",
                     overflow: "hidden", textOverflow: "ellipsis",
                     whiteSpace: "nowrap" }} title={ngNames}>
        {ngNames}
      </span>
      {/* The group verdict, sized for this header rather than for the old
          card layout -- OK_NG_BOX draws an antd Tag at 20 px, which next to a
          26 px-tall row reads as the loudest thing on the panel. */}
      <span style={{ flex: "0 0 auto", display: "inline-block", width: 46,
                     textAlign: "center", fontSize: 11, fontWeight: 600,
                     lineHeight: "17px", borderRadius: 3, color: "#fff",
                     background: (OK_NG_BOX_COLOR_TEXT[
                       group ? group.finalResult : MEASURERSULTRESION.NA]
                       || OK_NG_BOX_COLOR_TEXT[MEASURERSULTRESION.NA]).COLOR }}>
        {(OK_NG_BOX_COLOR_TEXT[group ? group.finalResult : MEASURERSULTRESION.NA]
          || OK_NG_BOX_COLOR_TEXT[MEASURERSULTRESION.NA]).TEXT}
      </span>
    </div>
  );
}

// One measurement, laid out for READING AND WRITING DOWN.
//
// The strip on the left and this are the same numbers for two different jobs.
// On the strip an operator glances at a verdict while the machine sorts; here,
// in sampling mode, they read the value off and record it -- so the limits and
// the margin belong on screen, not behind a hover, and the columns have to line
// up down the page or the digits get transcribed wrong.
class ResultRowExpanded extends React.PureComponent {
  render() {
    const rep = this.props.singleInspection;
    const def = rep.def || {};
    const essential = GetObjElement(rep, ["def", "quality_essential"]) !== false;

    // Guarded. An unmapped detailStatus threw right here and took the whole
    // inspection panel down with it -- the error boundary replaces the screen
    // and the operator loses the session, which is a very expensive way to
    // report an unknown enum value.
    let color = (OK_NG_BOX_COLOR_TEXT[rep.detailStatus]
                 || OK_NG_BOX_COLOR_TEXT[MEASURERSULTRESION.NA]).COLOR;
    if (!essential) color = Color(color).desaturate(0.6).darken(0.5);

    const numeric = (rep.value === +rep.value) ? +rep.value : undefined;
    const unit = DEFAULT_UNIT[rep.subtype] || "";
    const shown = numeric === undefined ? "NaN" : numeric.toFixed(3);

    let ratio;
    if (numeric !== undefined && def.value !== undefined) {
      const span = numeric > def.value ? (def.USL - def.value) : (def.value - def.LSL);
      if (span > 0) ratio = (numeric - def.value) / span;
    }
    const OUT = 1.35;
    const pos = 50 + (ratio === undefined ? 0 : Math.max(-OUT, Math.min(OUT, ratio))) * 42;

    const num = (v) => (v === undefined || v === null ? "—" : Number(v).toFixed(3));
    const mono = { fontFamily: "ui-monospace, Consolas, monospace",
                   fontVariantNumeric: "tabular-nums" };

    return (
      <div style={{ padding: "10px 12px", borderBottom: "1px solid #f0f0f0" }}>
        <div style={{ display: "flex", alignItems: "baseline", gap: 10 }}>
          <span style={{ flex: "1 1 auto", minWidth: 0, fontSize: 14,
                         color: essential ? "#262626" : "#bfbfbf",
                         overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
            {rep.name}
          </span>
          <span style={{ ...mono, flex: "0 0 auto", fontSize: 30, lineHeight: "34px",
                         letterSpacing: "-0.01em", textAlign: "right", minWidth: 148,
                         color: essential ? "#141414" : "#999" }}>
            {shown}<span style={{ fontSize: 14, color: "#8c8c8c", marginLeft: 4 }}>{unit}</span>
          </span>
          <span style={{ flex: "0 0 auto", fontSize: 12, fontWeight: 600, color: "#fff",
                         background: color, borderRadius: 3, padding: "2px 8px" }}>
            {(OK_NG_BOX_COLOR_TEXT[rep.detailStatus]
              || OK_NG_BOX_COLOR_TEXT[MEASURERSULTRESION.NA]).TEXT}
          </span>
        </div>

        <div style={{ position: "relative", height: 6, borderRadius: 3, marginTop: 7,
                      background: essential
                        ? "linear-gradient(90deg,#e8e8e8 0 7.6%,#bdbdbd 7.6% 8.4%,"
                          + "#e8e8e8 8.4% 49.6%,#8c8c8c 49.6% 50.4%,#e8e8e8 50.4% 91.6%,"
                          + "#bdbdbd 91.6% 92.4%,#e8e8e8 92.4% 100%)"
                        : "#f0f0f0" }}>
          {ratio === undefined ? null : (
            <span style={{ position: "absolute", top: -2, left: `calc(${pos}% - 1.5px)`,
                           width: 3, height: 10, borderRadius: 2, background: color,
                           boxShadow: "0 0 0 1.5px #fff" }} />
          )}
        </div>

        {/* The limits, spelled out. On the strip they live behind a hover
            because there is no room; here there is, and an operator writing a
            number down needs to see what it is being judged against. */}
        <div style={{ ...mono, display: "flex", justifyContent: "space-between",
                      fontSize: 11.5, color: "#8c8c8c", marginTop: 5 }}>
          <span>{num(def.LSL)}</span>
          <span>目標 {num(def.value)}</span>
          <span>{ratio === undefined ? "" : "餘裕 " + (ratio >= 0 ? "+" : "") + ratio.toFixed(2)}</span>
          <span>{num(def.USL)}</span>
        </div>
      </div>
    );
  }
}

export class InspectionResultDisplay_FullScren extends React.Component {

  constructor(props) {
    super(props);
    this.state = {
      folderStruct: {},
      history: ["./"],
    }
  }
  render() {
    const groups = this.props.groups;
    if (!Array.isArray(groups)) return null;


    // MOUNTED ONLY WHILE OPEN, and destroyed on close -- the same pattern as
    // the uInspESP32 panel below, and for a stronger reason.
    //
    // This modal used to render its whole contents on every parent render,
    // which on the live path is every image frame. It was handed the very
    // React elements the visible strip was built from and rendered them into a
    // SECOND Menu tree, so each frame built two complete copies of the result
    // list and threw both away. Measured 2026-08-23: 19 detached <div>, 13
    // <span>, 11 text nodes and one <svg> retained per frame, 3470 DOM nodes
    // and 478 event listeners per minute, renderer RSS +10 MB/min and no
    // collection could reclaim any of it -- the tree held them.
    //
    // A panel nobody is looking at should not be built at all.
    const body = this.props.visible ? (
      // A GRID OF GROUPS, not a row of navigation menus.
      //
      // Each group used to be wrapped in its own antd Menu with a SubMenu
      // inside it -- a navigation widget used purely as a box, carrying its own
      // open/selected state, keyboard handling and markup for a panel that
      // never navigates anywhere. Sampling puts several objects down at once,
      // usually three or fewer, and what the operator wants is those objects
      // side by side with their numbers lined up.
      //
      // auto-fit rather than a fixed span={8}: one object should not be
      // rendered into a third of the window with two thirds empty.
      <div style={{ display: "grid", gap: 14, alignItems: "start",
                    gridTemplateColumns: "repeat(auto-fit, minmax(320px, 1fr))",
                    maxHeight: "70vh", overflowY: "auto" }}>
        {groups.map((g, index) => (
          <div key={"fsc" + index}
               style={{ border: "1px solid #e8e8e8", borderRadius: 6, overflow: "hidden" }}>
            <div style={{ display: "flex", alignItems: "center", gap: 8,
                          padding: "8px 12px", background: "#fafafa",
                          borderBottom: "1px solid #e8e8e8", fontSize: 14 }}>
              <ResultGroupTitle group={g} />
            </div>
            {g.reports.map((rep) => (
              <ResultRowExpanded key={"x" + rep.name} singleInspection={rep} />
            ))}
          </div>
        ))}
      </div>
    ) : null;

    return (
      <Modal
        title={`量測結果 · ${groups.length} 個物件`}
        visible={this.props.visible}
        width={this.props.width === undefined ? 900 : this.props.width}
        onCancel={this.props.onCancel}
        onOk={this.props.onOk}
        destroyOnClose
        footer={null}
      >
        {body}
      </Modal>
    );
  }
}

// PureComponent: with a stable callback identity above, a card whose
// measurement has not changed does no work at all on the next frame.
class InspectionResultDisplay extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      // fullScreen:false
    }
    // Bound once, for the same reason as toggleFullscreenBound below: a fresh
    // bound function per render is a fresh object holding `this`, and the heap
    // snapshot found 1440 of them pointing at 828 of these components.
    this.clickFullScreenBound = this.clickFullScreen.bind(this);
  }


  clickFullScreen() {
    // this.setState({
    //     ...this.state,fullScreen:!this.state.fullScreen
    // });
    // console.log("[XLINX]fullScreen="+this.state.fullScreen);
    this.props.fullScreenToggleCallback();
  };

  showResultValueCheck(o) {
    if (o.status == MEASURERSULTRESION.NA)
      return "NaN ";
    else if (o.value === +o.value)
      return o.value.toFixed(3);
    else
      return "NaN ";
  }


  render() {
    // `empty` is a SLOT with nothing in it, not an error. See EMPTY_MEASURE_REP.
    //
    // An empty slot is normally hidden, EXCEPT the first one: reports come and
    // go, and a column that disappears when there is nothing to report drags
    // everything below it up and down. `placeholder` keeps one row on screen
    // showing "-", so the layout has a floor.
    // Three states, not two:
    //   a reading      -- name and value
    //   a GHOST        -- the item is in the list but has no reading right now
    //   an empty slot  -- nothing at all, hidden unless it is the placeholder
    // A ghost is what the operator sees between parts. Dropping the row would
    // make the whole list appear and disappear with every gap in the reports;
    // dropping just the NAME would leave a column of dashes that says nothing
    // about what is being measured.
    const ghost = !!this.props.ghost;
    const empty = !this.props.singleInspection;
    const placeholder = empty && this.props.placeholder;
    const rep = this.props.singleInspection || EMPTY_MEASURE_REP;
    const blank = empty || ghost;
    const def = rep.def || {};
    const essential = GetObjElement(rep, ["def", "quality_essential"]) !== false;

    // Guarded. An unmapped detailStatus threw right here and took the whole
    // inspection panel down with it -- the error boundary replaces the screen
    // and the operator loses the session, which is a very expensive way to
    // report an unknown enum value.
    let color = (OK_NG_BOX_COLOR_TEXT[rep.detailStatus]
                 || OK_NG_BOX_COLOR_TEXT[MEASURERSULTRESION.NA]).COLOR;
    if (!essential) color = Color(color).desaturate(0.6).darken(0.5);

    const shown = blank ? "-" : this.showResultValueCheck(rep);
    const unit = DEFAULT_UNIT[rep.subtype] || "";
    const numeric = (rep.value === +rep.value) ? +rep.value : undefined;

    // Where the value sits between its limits, as a signed fraction of the
    // margin: 0 is on target, +1 is exactly at USL, -1 exactly at LSL.
    //
    // This number already existed and was buried in the hover text. It is the
    // one thing on the card that answers "is this drifting?", which neither the
    // value nor the verdict can: 2.785 mm means nothing without its limits, and
    // OK covers everything from dead-on to one micron inside the line.
    // THE EFFECTIVE LIMITS, and everything on this row reads them.
    //
    // rep.lim is stamped by the reducer from the same margin info that produced
    // the verdict (see resultGrading), so it already carries the 製程 overrides
    // and the flipped-part _b fields. shape_def carries the ROOT limits.
    //
    // The bubble used to read shape_def while the scale and the colour read
    // rep.lim: with a 製程 selected it stated one set of numbers beside a
    // verdict computed from another, and nothing on screen said which applied.
    // A reading is only meaningful against the limits it was judged by.
    const lim = rep.lim || {};
    const L = { LSL: num(lim.LSL, def.LSL), USL: num(lim.USL, def.USL),
                TGT: num(lim.value, def.value),
                LCL: num(lim.LCL, undefined), UCL: num(lim.UCL, undefined) };

    // Say so when the row is not being judged by the recipe's own numbers --
    // otherwise an operator comparing the bubble against the def sees a
    // discrepancy and has no way to tell an override from a fault.
    const overridden = (L.LSL !== def.LSL) || (L.USL !== def.USL) || (L.TGT !== def.value);

    let ratio;
    if (numeric !== undefined && L.TGT !== undefined) {
      const span = numeric > L.TGT ? (L.USL - L.TGT) : (L.TGT - L.LSL);
      if (span > 0) ratio = (numeric - L.TGT) / span;
    }

    const show = (v) => (v === undefined ? "—" : v);
    const detailInfo = <>
      類型:{dictLookUp(def.subtype, this.props.DICT)} <br/>
      目標:{show(L.TGT)}<br/>
      規格上限:{show(L.USL)}<br/>
      規格下限:{show(L.LSL)}<br/>
      {(L.UCL !== undefined || L.LCL !== undefined) && <>
        管制上限:{show(L.UCL)}<br/>
        管制下限:{show(L.LCL)}<br/>
      </>}
      檢測值:{shown}<br/>
      界限比例:{ratio === undefined ? "—" : ratio.toFixed(2)}<br/>
      {overridden && <span style={{ color: "#c89000" }}>
        製程界限(非配方原始值)<br/>
      </span>}
      {!essential && <span style={{ color: "#8c8c8c" }}>
        參考項目 · 不列入件判定<br/>
      </span>}
    </>;


    // The scale behind the reading: SIX lines.
    //
    //   LSL   LCL      target      UCL   USL        and the value's marker
    //
    // Spec limits are pinned to 8% and 92% and the target to 50% on every row,
    // so the eye can compare one row against the next -- the alternative,
    // scaling each row to its own numeric range, means the same x means a
    // different thing on every line and the column stops being scannable.
    // The control limits then land wherever they fall in between, which is the
    // one thing that does vary per measurement and is worth seeing.
    //
    // Every line is drawn with a gradient, so the whole scale costs zero DOM
    // nodes on a row that is redrawn at the frame rate.
    //
    // Where a raw value sits, in the row's 8..92% coordinates.
    //
    // ONLY the spec limits are normalised: LSL is always 8%, USL always 92%,
    // and everything else -- target, the two production control limits, the
    // reading -- lands wherever it falls in between. One linear map, no kink.
    //
    // The first version pinned the target to 50% as well and scaled each half
    // separately. That reads well when the tolerance is symmetric and lies when
    // it is not: with 2.9..3.3 around a target of 3, half the bar covered
    // 0.1 mm and the other half 0.3, so two markers the same distance apart
    // meant different things depending on which side of centre they sat.
    //
    // Degenerate cases still need the target, because with a limit missing
    // there is no span to normalise against:
    //   both limits, USL > LSL   the ordinary case, one linear map
    //   a limit missing          fall back to the target-anchored side
    //   limit === target         every value past it is out of spec
    const OFF = 999;    // sentinel: off-scale, pinned by the caller
    const twoSided = L.LSL !== undefined && L.USL !== undefined && L.USL > L.LSL;
    const place = (v) => {
      if (v === undefined) return undefined;
      if (twoSided) return 8 + ((v - L.LSL) / (L.USL - L.LSL)) * 84;
      if (L.TGT === undefined) return undefined;
      if (v === L.TGT) return 50;
      if (v > L.TGT) {
        if (L.USL === undefined) return 50;
        const span = L.USL - L.TGT;
        return span > 0 ? 50 + ((v - L.TGT) / span) * 42 : OFF;
      }
      if (L.LSL === undefined) return 50;
      const span = L.TGT - L.LSL;
      return span > 0 ? 50 - ((L.TGT - v) / span) * 42 : -OFF;
    };

    // A control limit at or outside its spec limit is TURNED OFF, not broken.
    //
    // It is a supported way to say "judge this one on spec alone": the core
    // never reads UCL/LCL at all -- FeatureManager_sig360_circle_line.cpp
    // decides SUCCESS/FAILURE from USL/LSL and nothing else -- and the WebUI's
    // grading checks the spec limits first, so a control limit sitting on its
    // spec limit can never produce a UCNG/LCNG. Nothing reacts to it, so
    // nothing should be drawn for it.
    //
    // An earlier version painted these in a warning colour as a misconfigured
    // scale. That was wrong about the intent, and the loud colour would have
    // been on screen permanently for a setup that is working as asked.
    const ctlOff = (v, lower) =>
      v === undefined || (lower
        ? (L.LSL !== undefined && v <= L.LSL)
        : (L.USL !== undefined && v >= L.USL));

    // Deduplicated, because a one-sided tolerance puts a spec line exactly on
    // the target. Two ticks at the same position give a gradient two identical
    // stops, which is not a drawing error but is a stop ordering the browser
    // has to resolve -- and the second line adds nothing to look at anyway.
    const SCALE = [];
    for (const t of [
      // The same traffic light as the figures: red is the line that rejects
      // the part, amber the line that means react. The target is neither -- it
      // is the reference the other four are measured from -- so it stays
      // neutral, and being the only grey line makes it the easy one to find.
      // Paler than the figures by a wide margin. These are a scale, not data:
      // they have to be locatable without ever competing with the reading that
      // sits on top of them, and the marker has to stay the most saturated
      // thing on the row so the eye lands on it first.
      { at: place(L.LSL), half: 1,   c: "#ffc9c7" },   // spec, the hard limits
      { at: place(L.USL), half: 1,   c: "#ffc9c7" },
      { at: ctlOff(L.LCL, true)  ? undefined : place(L.LCL), half: 0.8, c: "#ffe7a3" },
      { at: ctlOff(L.UCL, false) ? undefined : place(L.UCL), half: 0.8, c: "#ffe7a3" },
      { at: place(L.TGT), half: 0.7, c: "#cfcfcf" },   // target
    ]) {
      if (t.at === undefined) continue;
      const at = Math.max(1.5, Math.min(98.5, t.at));
      if (SCALE.some((u) => Math.abs(u.at - at) < 0.6)) continue;
      SCALE.push({ ...t, at });
    }
    SCALE.sort((x, y) => x.at - y.at);

    const tick = (t) =>
      "transparent calc(" + t.at + "% - " + t.half + "px), "
      + t.c + " calc(" + t.at + "% - " + t.half + "px), "
      + t.c + " calc(" + t.at + "% + " + t.half + "px), "
      + "transparent calc(" + t.at + "% + " + t.half + "px)";

    // The LIMIT LINES carry the colour; the field between them stays white.
    //
    // Tinted zones were tried first and dropped: five bands of pale colour is a
    // lot of surface on a row 26 px tall that already carries a name and a
    // reading, and the figures end up sitting on whichever band they land in.
    // Colouring the lines says the same thing -- red is the edge that rejects
    // the part, amber the one that means react -- in a fraction of the ink, and
    // leaves the numbers on plain white where they are easiest to read.
    const track = (essential && SCALE.length)
      ? "linear-gradient(90deg, " + SCALE.map(tick).join(", ") + ")"
      : "#fafafa";

    // Out-of-tolerance values are PINNED to the edge and drawn thicker.
    //
    // The marker used to be placed at 50 + ratio*42 clamped to +/-1.35, which
    // puts anything past about 1.2 outside the 0..100% band -- so the further a
    // value went out of spec the less of its marker was visible, and a really
    // bad one had no marker at all. That is backwards: the worse it is, the
    // more it has to show.
    const ink = valueInk(rep.detailStatus, ratio, blank);
    // The marker is placed through the SAME mapping as the scale lines, so a
    // reading sitting on its limit lands on that limit's tick by construction.
    // It used to be derived from `ratio`, computed separately further up with
    // its own span logic -- two ways of saying where a number is, which is one
    // too many.
    const rawAt = place(numeric);
    const markerOff = rawAt !== undefined && (rawAt < 0 || rawAt > 100);
    const outOfSpec = markerOff
      || (rawAt !== undefined && (rawAt <= 8 - 0.01 || rawAt >= 92 + 0.01));
    const HALF = outOfSpec ? 2.5 : 1.25;
    const pos = rawAt === undefined ? 50 : Math.max(1.5, Math.min(98.5, rawAt));

    const barLayers = (rawAt === undefined || blank)
      ? track
      : "linear-gradient(90deg, " + tick({ at: pos, half: HALF, c: ink.fg }) + "), "
        + track;

    // No bottom border here: the scale row below carries the separator, so the
    // two rows read as one entry rather than as two.
    const cell = { padding: "3px 4px 1px", verticalAlign: "middle" };

    // TWO rows per measurement: the reading, then a 9 px strip carrying the
    // scale across both columns.
    //
    // The scale wants to be a band along the bottom of the row and not to run
    // through the figures -- a red spec line landing on the "mm" of a number
    // someone is copying out. Two attempts to do that with the row's own
    // background failed: background-size/position are ignored on a <tr> under
    // border-collapse:collapse, and switching to separate did not fix it
    // either, so the gradient kept flooding the full height. A td with
    // colSpan is not a workaround, it is simply the element that has the box
    // we want -- and it costs two nodes.
    const hide = (empty && !placeholder) ? "none" : undefined;
    return (
      <>
      <tr style={{ display: hide }}>
        <td style={{ ...cell, overflow: "hidden", textOverflow: "ellipsis",
                     whiteSpace: "nowrap", fontSize: 12, color: "#595959" }}
            title={essential ? rep.name : rep.name + "(不列入判定)"}>
          {/* The same eye the canvas overlay already draws on these shapes, so
              the two views name the thing the same way. A row only reaches here
              if it is IN rank -- out-of-rank rows are filtered out upstream --
              so this mark always means "shown, measured, and not counted",
              never "hidden". */}
          {!essential && <EyeInvisibleOutlined
              style={{ fontSize: 11, marginRight: 4, color: "#8c8c8c" }} />}
          {rep.name}
        </td>
        <td style={{ ...cell, textAlign: "right", padding: "3px 6px",
                     fontVariantNumeric: "tabular-nums", letterSpacing: "-0.01em",
                     fontSize: 18, lineHeight: "20px",
                     fontWeight: ink.w,
                     color: ink.fg, position: "relative" }}>
          <Popover content={detailInfo} placement="bottomLeft" trigger={["click", "hover"]}>
            <span>{shown}<span style={{ fontSize: 11, marginLeft: 2, opacity: .65 }}>
              {blank ? "" : unit}</span></span>
          </Popover>
          {/* A TINT, NOT A COLOUR CHANGE. The reading keeps the ink its status
              earned -- a reference dimension that is out of tolerance is still
              red, because it IS out of tolerance -- and the wash says only that
              it does not decide the part. Repainting it grey instead would use
              the one colour that already means NA or empty slot, so a real NG
              that happens not to count would look like no reading at all. */}
          {!essential && <span style={{
              position: "absolute", inset: 0, pointerEvents: "none",
              background: "rgba(140,140,140,0.22)" }} />}
        </td>
      </tr>
      <tr style={{ display: hide }}>
        {/* The separator is heavier than a hairline and stands off the scale.
            The scale is itself a row of vertical marks, so a 1 px rule tight
            underneath joined the two into one busy band and the eye could not
            tell where an entry ended -- which matters more here than usual,
            because each entry is two rows and the wrong grouping reads as one
            measurement's scale belonging to the next measurement's number. */}
        <td colSpan={2} style={{ padding: "0 0 7px", height: 16, lineHeight: 0,
                                 background: barLayers,
                                 backgroundRepeat: "no-repeat",
                                 backgroundPosition: "top",
                                 backgroundSize: "100% 9px",
                                 borderBottom: "3px solid #c4c4c4" }} />
      </tr>
      </>
    );
  }
}

class OK_NG_BOX extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      // OK_NG: undefined,
    }
  }

  render() {
    //console.log(this.props.detailStatus)
    let detailStatus=this.props.detailStatus;
    if(detailStatus===undefined)
    {
      detailStatus=MEASURERSULTRESION.UNSET;
    }
    return (
      <div style={{ 'display': 'inline-block' }}>
        <Tag style={{ 'fontSize': 20 }}
          color={OK_NG_BOX_COLOR_TEXT[detailStatus].COLOR}>
          {OK_NG_BOX_COLOR_TEXT[detailStatus].TEXT}
        </Tag>
        {this.props.children}
      </div>
    )
  }
}



function UInspMiscCtrlPopUp({force_popUp=false,allow_auto_popUp=true,onCancel=_=>_})
{

  const _s = useRef({});
  let _this=_s.current;

  
  
  const dispatch = useDispatch();
  const uInsp_API_ID = useSelector(state => state.ConnInfo.uInsp_API_ID);
  const uInsp_API_ID_CONN_INFO = usePerifConn(uInsp_API_ID);
  const API= (callback)=>callback(getPerifAPI(uInsp_API_ID));


  const [limitCD, setLimitCD] = useState(20);

  const [popUp, setPopUp] = useState(false);
  const [btnDisable, setBtnDisable] = useState(false);

  
  let machineStatus = GetObjElement(uInsp_API_ID_CONN_INFO,["machineStatus"]);
  let OK_LIMIT_CD = GetObjElement(uInsp_API_ID_CONN_INFO,["machineStatus","OK_LIMIT_CD"]);
  useEffect(() => {
    if(OK_LIMIT_CD==0)
    {
      setPopUp(true);
    }
    setBtnDisable(false);
  }, [machineStatus]);


  let uInspPopUp=null;
  if(force_popUp==true || (allow_auto_popUp&&popUp) )
  {
    let bigInfo={
      icon:<ExclamationCircleOutlined style={{color:"#F00"}}/>,
      text:"UNDEFINED",
    }
    if(OK_LIMIT_CD===undefined)//no OK_LIMIT_CD feature activated
    {
      bigInfo={
        icon:<ExclamationCircleOutlined style={{color:"#AAA"}}/>,
        text:"Nothing...",
      }
    }
    else if(OK_LIMIT_CD>0)
    {
      bigInfo={
        icon:<ExclamationCircleOutlined style={{color:"#0F0"}}/>,
        text:`倒數:${OK_LIMIT_CD}`,
      }
    }
    else if(OK_LIMIT_CD==0)
    {
      bigInfo={
        icon:<ExclamationCircleOutlined style={{color:"#F00"}}/>,
        text:"請按再繼續!",
      }
    }

    uInspPopUp=<Modal
      title={"全檢設備"}
      visible={true}
      onOk={() => {}}
      onCancel={() => {
        onCancel();
        setPopUp(false);
        
      }}
      header={null}
      footer={null}
    >
      {
        <div className="antd-icon-sizing overlayCon" style={{height:"70px"}}>
          {bigInfo.icon}
          <div className="overlay veleXY" >
            {bigInfo.text}
          </div>
        </div>
      }
      
      設定目標數量:
      {/* Native input, not the numpad popup: keyboard types directly,
          inputMode asks a touch device for its numeric keys. */}
      <InputNumber min={0} precision={0} inputMode="numeric" value={limitCD}
        onChange={(v)=>{ const n=parseInt(v); if(Number.isFinite(n)) setLimitCD(n); }} />

      
      {"  "}

      <Button key="opt uInsp"
        loading={btnDisable}
        onClick={() => {
          setBtnDisable(true);
          API((api)=>{
            api.send({type: "set_OK_limit_cd",value:limitCD},
            (ret)=>{console.log(ret)},
            (e)=>console.log(e));

            api.triggerPing();
          })
      }} >{"  >>  "}</Button>
      {"  "}
      {OK_LIMIT_CD}

      <br/>
      <Button key="cancel target"
        loading={btnDisable}
        onClick={() => {
          setBtnDisable(true);
          API((api)=>{
            api.send({type: "set_OK_limit_cd",value:-1},
            (ret)=>{console.log(ret)},
            (e)=>console.log(e));
            api.triggerPing();
          })
      }} >{"取消目標數量"}</Button>
    </Modal>
  }



  // let uInspMachStatus = GetObjElement(this.props.uInsp_API_ID_CONN_INFO,["machineStatus"]);
  // if(uInspMachStatus!==undefined)
  // {
  //   let over_OK_limit = uInspMachStatus.over_OK_limit;
  //   if(over_OK_limit!==null && over_OK_limit>=0)
  //   {
      
      
  //   }
  // }



  // let uInspPopUp=<Modal {...modalInfo} visible={modalInfo!==undefined}>
  //   {GetObjElement(modalInfo,"content")}
  // </Modal>




  return uInspPopUp;
}





function SLIDMiscCtrlPopUp({force_popUp=false,allow_auto_popUp=true,onCancel=_=>_})
{

  const _this = useRef({}).current;
  
  const dispatch = useDispatch();
  const SLID_API_ID = useSelector(state => state.ConnInfo.SLID_API_ID);
  const SLID_API_ID_CONN_INFO = usePerifConn(SLID_API_ID);
  const API= (callback)=>callback(getPerifAPI(SLID_API_ID));

  const ACT_WS_GET_OBJ= (callback)=>callback(getPerifAPI(SLID_API_ID));

  const [popUp, setPopUp] = useState(false);

  



  let SLIDPopUp=null;
  if(force_popUp==true || (allow_auto_popUp&&popUp) )
  {

    SLIDPopUp=<Modal
      title={"坡檢設備"}
      visible={true}
      onOk={() => {}}
      onCancel={() => {
        onCancel();
        setPopUp(false);
        
      }}
      header={null}
      footer={null}
    >
      <SLID_UI UI_EM_STOP_UI/>

    </Modal>
  }

  return <>
    
    <SLID_UI on_EM_STOP_state_change={(api,report_stat)=>{

      if(_this.is_in_EM_STOP!=api.is_in_EM_STOP&&api.is_in_EM_STOP==true)
        setPopUp(api.is_in_EM_STOP)

      _this.is_in_EM_STOP=api.is_in_EM_STOP;
      }}/>
  
  
    {SLIDPopUp}</>;
}


class ObjInfoList extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      collapsed: false,
      fullScreen: false,
      perifMISCCtrl_popUp:false,
      // Which group SLOTS are collapsed, by slot index rather than by object
      // id. The slot is what stays put; an object id comes and goes several
      // times a second and would take the operator's choice with it.
      collapsedSlots: {},
    }
    // Bound ONCE. `this.toggleFullscreen.bind(this)` inside render produces a
    // new function identity on every frame, which changes the props of every
    // card below it and defeats any memoisation they could otherwise do.
    this.toggleFullscreenBound = this.toggleFullscreen.bind(this);
    // One bound handler for every slot, for the same reason. The slot index
    // rides on the event target's dataset instead of a per-slot closure.
    this.toggleSlotBound = this.toggleSlot.bind(this);
  }

  toggleSlot(e) {
    const i = Number(e.currentTarget.dataset.slot);
    if (!Number.isFinite(i)) return;
    this.setState((st) => ({
      collapsedSlots: { ...st.collapsedSlots, [i]: !st.collapsedSlots[i] },
    }));
  }

  toggleCollapsed() {
    this.setState({
      ...this.state,
      collapsed: !this.state.collapsed,
    });
  }
  toggleFullscreen() {
    this.setState({
      ...this.state,
      fullScreen: !this.state.fullScreen,
    });

  }
  render() {

    let resultMenu = [];
    let reports = this.props.IR;
    let IR_decotrator = this.props.IR_decotrator;
    if (!Array.isArray(reports)) {
      return null;
    }


    let curObjList = reports.filter((rep) => rep.isCurObj);
    let optiona_id_order = undefined;
    if (IR_decotrator !== undefined) {
      optiona_id_order = IR_decotrator.list_id_order;
    }

    resultMenu = curObjList.map((singleReport, idx) => {
      let reportDetail = [];
      let judgeReports = singleReport.judgeReports;
      if(judgeReports===undefined||judgeReports.length==0)
      {
        return null;
      }
      if (optiona_id_order !== undefined) {
        judgeReports = optiona_id_order.
          map(id => judgeReports.find(judge => judge.id == id)).
          filter(rep => rep !== undefined);
      }



      let judgeInRank = judgeReports
      .map(rep=>({...rep,def:this.props.shape_def.find(def=>def.id==rep.id)}))
      .filter(rep=>{
        let rdef=rep.def;
        if(rdef.rank===undefined)return true;
        if(rdef.rank<=this.props.measureDisplayRank)return true;
        return false;
      });


      // WHAT DECIDES THE PART.
      //
      //   effective quality_essential = (rank <= N) AND quality_essential
      //
      // and the fold happens once, on mount, into shape_list -- see the rankN
      // tag handling in componentDidMount. rank never decided anything in the
      // core, which has no notion of it, so folding it into the flag the core
      // DOES reduce on is what makes one rule instead of two. Before this,
      // finalResult reduced over
      // the rank-filtered list while the core (InspStatusReduce in
      // wiringPanel.cpp) walked every report and tested quality_essential
      // only: turning the display level down made a part the machine was about
      // to reject read OK, which is what confused the sorting station.
      //
      // NAasNG / NGasNA are applied HERE, per item, before the reduction --
      // exactly where the core applies them. They are per-measurement switches,
      // they ride the def to the core, the core honours them, and this side
      // never did; any def using one of them had a screen verdict that could
      // not agree with the sorter. gradeMismatch cannot see that class of
      // divergence, because it compares ITEM status and these two act on the
      // roll-up.
      let finalResult = judgeReports.reduce((res, rep) => {
        const rdef = this.props.shape_def.find(d => d.id == rep.id);
        if (!rdef) return res;
        // quality_essential ALONE, because the level was already folded into it
        // when the 製程 overrides were written into shape_list on mount. Testing
        // rank again here would be a second copy of the rule, and the copy the
        // core does not have.
        if (rdef.quality_essential === false) return res;

        let st = rep.detailStatus;
        if (rdef.NAasNG && st === MEASURERSULTRESION.NA) st = MEASURERSULTRESION.NG;
        if (rdef.NGasNA && NG_STATUSES.has(st)) st = MEASURERSULTRESION.NA;
        return MEASURERSULTRESION_reducer(res, st);
      }, undefined);

      // DATA, not elements. See the note below.
      // similarity rides along so the header can show HOW WELL the part was
      // matched, not just that it was. Without it a def sitting a hair above
      // its acceptance threshold looks identical to one matching perfectly,
      // right up to the shift where it stops matching at all -- and there is
      // nothing on this screen to tune against.
      return { idx, isFlipped: singleReport.isFlipped, finalResult,
               similarity: singleReport.similarity, reports: judgeInRank };
    }
    ).filter((g) => g !== null);

    // The measurement groups are described ONCE, as plain objects, and each
    // place that shows them renders its own tree from that description.
    //
    // They used to be built as React elements here and the same element array
    // was handed to the fullscreen modal as a prop, which rendered them into a
    // second Menu. Two full copies of the result list, per frame, one of them
    // never visible -- and the discarded subtrees stayed reachable. Measured
    // 2026-08-23: renderer RSS +10 MB/min, 3470 DOM nodes and 478 listeners a
    // minute retained, none of it reclaimable by a forced collection. Turning
    // the image stream off stopped it dead, which is what pointed here.
    const resultGroups = resultMenu;
    // The def's acceptance floor, from the ONE place that knows it. It is not a
    // constant and it is not 0.9: a shape_based def is gated by line2Dup's
    // shape_min_score (core default 0.50), a sig360 def by sig_match_sim_thres
    // (core default 0.70 when the key is absent). A shape_based def scoring
    // 0.986 has enormous headroom over 0.50 and none over an assumed 0.99 --
    // and headroom is the entire reason the number is on screen.
    const simThres = acceptanceFloor(this.props.edit_info).floor;
    // Plain divs, NOT antd SubMenu.
    //
    // The modal was moved off Menu/SubMenu first and the growth fell about 20%
    // -- real, but the panel below kept producing it. The detached-node census
    // named the remainder outright: after the measurement rows themselves, the
    // most common thing retained after leaving the document was
    // `ant-menu-submenu-title`, which is the title of exactly this SubMenu.
    // rc-menu keeps references to child instances, and a title element rebuilt
    // on every frame therefore never becomes collectable. Nothing here needs a
    // menu: these groups are not navigation, they are never collapsed (openKeys
    // always listed all of them) and nothing is selectable.
    // A pool of group slots that GROWS and never shrinks.
    //
    // Same reasoning as the row slots inside ResultGroupItems, one level up:
    // the group container plus its title (a PaperClipOutlined icon alone is
    // span+svg+path) was mounted and unmounted every time the set of objects
    // in the core's trackingWindow changed. Shrinking the pool again would
    // reintroduce exactly that, so the high-water mark only ever rises. It is
    // bounded in practice by how many objects the operator places at once and
    // by the recipe's measurement count -- there is no need for a fixed cap,
    // and a cap would only have to fail somehow when exceeded.
    //
    // Held on the instance rather than in state: it is derived from the data
    // already being rendered, so raising it here is used by THIS render and
    // needs no second pass.
    // At least one group slot, always. Same reason as the placeholder row: with
    // no objects in the tracking window the panel would otherwise vanish and
    // reappear, which is the jitter, not a saving.
    this._groupSlots = Math.max(this._groupSlots || 0, resultGroups.length, 1);
    this._rowSlots = resultGroups.reduce(
      (m, g) => Math.max(m, g.reports.length), this._rowSlots || 0);

    resultMenu = [];
    for (let i = 0; i < this._groupSlots; i++) {
      const g = resultGroups[i];
      const shut = !!this.state.collapsedSlots[i];
      // Slot 0 is the one that must never disappear.
      const showSlot = !!g || i === 0;
      // Remember what this slot last held, so it can keep showing the item
      // names while there is no report. Kept on the instance and not in state:
      // it is a copy of data already rendered, and writing it must not cause
      // another render.
      if (!this._lastRows) this._lastRows = [];
      if (g) this._lastRows[i] = g.reports;
      resultMenu.push(
        <div style={{ 'textAlign': 'left', display: showSlot ? undefined : 'none',
                      border: "1px solid #e8e8e8", borderRadius: 4,
                      overflow: "hidden", marginBottom: 6, background: "#fff" }}
          key={"gslot" + i} className="Antd_Menu_Title_AutoHeight">
          <ResultGroupTitle group={g} slot={i} collapsed={shut} simThres={simThres}
            onToggle={this.toggleSlotBound} onFullScreen={this.toggleFullscreenBound} />
          {/* HIDDEN, not unmounted. Collapsing by dropping the children would
              destroy the very rows the slot pool keeps alive, so a shift
              collapses and expands would churn as much DOM as the pool saves. */}
          <div style={{ display: shut ? "none" : undefined }}>
            <ResultGroupItems group={g} slots={this._rowSlots} DICT={this.props.DICT}
              ghostReports={this._lastRows[i]}
              onFullScreen={this.toggleFullscreenBound} />
          </div>
        </div>
      );
    }

    let fullScreenMODAL = <InspectionResultDisplay_FullScren
      groups={resultGroups} DICT={this.props.DICT} visible={this.state.fullScreen}
      onCancel={this.toggleFullscreenBound} width="90%" />;

    let uInspUI=this.props.uInsp_API_ID_CONN_INFO===undefined? null:
    <SubMenu style={{ 'textAlign': 'left' }} key={"uInsp" } className="Antd_Menu_Title_AutoHeight Antd_Menu_Title_Padding_Left_small"
      title={
      <>
        <Divider orientation="center" key="divi" style={{ 'margin': '5px'}} className="Antd_Divider_Small_Text_Tight">全檢設備</Divider>
        <UINSP_UI UI_INSP_Count={true} UI_INSP_Count_font_size={15}/>
      </>}
      > 
        <div style={{margin:"15px"}}>
          <Divider orientation="left" style={{ 'margin': '2px',fontSize: "12px"}} >全檢速度(pcs/s)</Divider>
          <UINSP_UI UI_INSP_Count_Rate={true} UI_INSP_Count_font_size={15}/>
          <Divider orientation="left" style={{ 'margin': '10px 2px 2px 2px',fontSize: "12px"}} >轉盤速度RPM(圈/分)</Divider>
          <UINSP_UI UI_Speed_Slider={true}/>
          
          
          <Button key="opt uInsp" icon={<SettingOutlined/>}
            onClick={() => {
              this.setState({
                ...this.state,
                perifMISCCtrl_popUp: true,
              })
            }} ></Button>
        </div>
    </SubMenu>


    // The 2nd-gen board. A SEPARATE block from uInspUI above on purpose: that
    // one drives uInspMEGA, which speaks a different dialect entirely, and the
    // 1st-gen panels are not to be touched. Everything here comes out of
    // uInspESP32_UI.jsx.
    let uInspESP32UI = this.props.uInspESP32_API_ID_CONN_INFO === undefined ? null :
    <SubMenu style={{ 'textAlign': 'left' }} key={"uInspESP32"} className="Antd_Menu_Title_AutoHeight Antd_Menu_Title_Padding_Left_small"
      title={
      <>
        <Divider orientation="center" key="divi2" style={{ 'margin': '2px 0'}} className="Antd_Divider_Small_Text_Tight">全檢設備 v2</Divider>
        <UINSP_ESP32_MINI/>
      </>}
      >
        <div style={{margin:"15px"}}>
          <Button key="opt uInspESP32" icon={<SettingOutlined/>}
            onClick={() => this.setState({ ...this.state, uInspESP32_popUp: true })}
          >設定 / 診斷</Button>
        </div>
    </SubMenu>


    // The station -- inspection region + clean-space regions. Another NEW block,
    // and it lives HERE rather than in the def editor on purpose: these describe
    // where the part sits when the camera fires and which patch of plate has to
    // be empty. That is the machine, not the product, so it is authored on the
    // live image and stored in machine_setting.json. Everything is in
    // component/StationRegionPanel.jsx.
    // Everything goes in the TITLE, not the body. The Menu's openKeys is
    // controlled and only ever contains "sub1"+index from resultMenu, so a
    // SubMenu keyed anything else can never be expanded -- clicking it does
    // nothing at all. That is why the v2 block above puts its strip in the
    // title too. A body here would render and be permanently unreachable.
    let stationUI =
    <SubMenu style={{ 'textAlign': 'left' }} key={"station"} className="Antd_Menu_Title_AutoHeight Antd_Menu_Title_Padding_Left_small"
      title={
      <>
        <Divider orientation="center" key="divi3" style={{ 'margin': '2px 0'}} className="Antd_Divider_Small_Text_Tight">工位區域</Divider>
        <StationRegionPanel
          ecCanvas={this.props.ecCanvas}
          machineSetting={this.props.machineSetting}
          onApply={(patch) => {
            // Live first: setup_machine_setting() on the core re-reads
            // inspection_region and it takes effect on the very next frame,
            // with no def reload and no restart.
            this.props.WSCMD_CB("ST", 0,
              { MachineSetting: { ...(this.props.machineSetting||{}), ...patch } });
          }}
          onApplyRegionLive={(region) => {
            // Station only, and NOT a MachineSetting patch.
            //
            // setup_machine_setting() also runs load_clean_regions(), which
            // reads an absent key as "no clean regions" -- so a region-only
            // MachineSetting would wipe them. It would also re-push this tab's
            // cached copy of the whole file on every drag, which is the same
            // stale-cache overwrite onSave had to be fixed for. InspRegionLive
            // touches the station and nothing else, and never touches disk.
            this.props.WSCMD_CB("ST", 0, { InspRegionLive: region });
          }}
          onBypass={(on) => {
            // Runtime only, and deliberately NOT part of the MachineSetting
            // patch above: that one gets written to machine_setting.json by
            // onSave, and a bypass that persists is a machine that has silently
            // stopped enforcing its station. The core drops it on restart.
            this.props.WSCMD_CB("ST", 0, { InspAreaBypass: !!on });
          }}
          onSave={(setting, onDone) => {
            // onDone(ok) tells the panel whether to clear its dirty flag. A
            // refused/failed save reports ok=false so the panel keeps "未存檔"
            // and the retry affordance (the Save/放棄 buttons) instead of
            // pretending it saved.
            const done = (ok) => { if (typeof onDone === "function") onDone(ok); };
            // Read-merge-write, not write-the-cache. This handler and MAINUI's
            // settings panel both used to serialize their OWN copy of the whole
            // file -- a copy cached at connect and never refreshed -- so
            // whichever saved second silently reverted the other's changes
            // (browser B undoing browser A's InspectionMode was the observed
            // case). This panel owns exactly two keys; re-read the file and
            // overlay only those.
            const STATION_KEYS = ["inspection_region", "clean_regions"];
            const writeMerged = (base) => {
              let _s = { ...base };
              STATION_KEYS.forEach(k => {
                if (setting[k] !== undefined) _s[k] = setting[k];
                else delete _s[k];   // panel cleared it; absent must not resurrect
              });
              Object.keys(_s).forEach(k => { if (k.startsWith("_")) delete _s[k]; });
              this.props.WSCMD_CB("SV", 0,
                { filename: "data/machine_setting.json" },
                new TextEncoder().encode(JSON.stringify(_s, null, 2)),
                { resolve: () => {
                    this.props.ACT_machine_custom_setting_Update({ ...setting, ..._s });
                    log.info("[station] regions saved (merged onto the on-disk file)");
                    done(true);
                  },
                  reject: (e) => { log.error("[station] save failed", e); done(false); } });
            };
            this.props.WSCMD_CB("LD", 0, { filename: "data/machine_setting.json" },
              undefined,
              { resolve: (pkts) => {
                  // FL's data is the already-parsed JSON object (see
                  // CalibrationUI's readers of the same reply shape).
                  const fl = (pkts || []).find(p => p.type == "FL");
                  const base = fl && fl.data;
                  if (base && typeof base === "object" && !Array.isArray(base)) writeMerged(base);
                  else {
                    // REFUSE, do not fall back to the cached copy: writing the
                    // local snapshot as the whole file is exactly the
                    // browser-B-reverts-browser-A bug this read-merge-write
                    // exists to kill -- the fallback would resurrect keys
                    // another writer deleted. A failed save the operator can
                    // retry beats a "successful" save that silently undoes
                    // someone else's work.
                    log.error("[station] could not re-read machine_setting.json -- NOT saving (retry when the core answers)");
                    message.error("無法讀取 machine_setting.json，工位設定未儲存 — 請重試");
                    done(false);
                  }
                },
                reject: () => {
                  log.error("[station] re-read failed -- NOT saving (retry when the core answers)");
                  message.error("無法讀取 machine_setting.json，工位設定未儲存 — 請重試");
                  done(false);
                } });
          }} />
      </>}
      >
    </SubMenu>


    // console.log(this.state.SLID_EM_STOP_src_list);
    let SLIDUI=this.props.SLID_API_ID_CONN_INFO===undefined? null:
    <div style={{ 'textAlign': 'left' }} key={"uInsp" } className="Antd_Menu_Title_AutoHeight Antd_Menu_Title_Padding_Left_small"
    onClick={() => {
      this.setState({
        ...this.state,
        perifMISCCtrl_popUp: true,
      })
    }}
      > 
        <SLID_UI on_EM_STOP_state_change={(api,report_stat)=>{
          // console.log(api.is_in_EM_STOP,api.EM_STOP_src_list);
          if(api.is_in_EM_STOP==this.state.is_in_EM_STOP && api.EM_STOP_Rule.enable_EM_STOP==this.state.enable_EM_STOP)return;
          log.debug("[EM_STOP]", { in_EM_STOP: api.is_in_EM_STOP, enable_EM_STOP: api.EM_STOP_Rule.enable_EM_STOP });

          let SLID_EM_STOP_src_list=api.is_in_EM_STOP==true?api.EM_STOP_src_list:undefined;
          
          this.setState({
            enable_EM_STOP:api.EM_STOP_Rule.enable_EM_STOP,
            SLID_EM_STOP_src_list:SLID_EM_STOP_src_list,
            is_in_EM_STOP:api.is_in_EM_STOP
          });

          
        }}/>

        <Button key="opt SLID" icon={<SettingOutlined/>}
            onClick={() => {
              this.setState({
                ...this.state,
                perifMISCCtrl_popUp: true,
              })
            }} >

        <Tag style={{ 'fontSize': 15 }} 
          className={this.state.SLID_EM_STOP_src_list===undefined ||this.state.enable_EM_STOP==false?"":"Emergency_Blink"}
          color={this.state.enable_EM_STOP==false?"gray": (this.state.SLID_EM_STOP_src_list===undefined?"green":"white") }
          >坡檢停機功能:{this.state.enable_EM_STOP==false?"停用": (this.state.SLID_EM_STOP_src_list===undefined?"正常":"停機") }</Tag>


        </Button>
        {/* <SLIDMiscCtrlPopUp force_popUp={this.state.perifMISCCtrl_popUp} 
          onCancel={_=>this.setState({
            perifMISCCtrl_popUp: false,
          })}/> */}
    </div>




    // console.log(this.props.SLID_API_ID_CONN_INFO);

    // The result groups no longer live in the Menu, and the note above the
    // station SubMenu records that openKeys only ever held their keys -- so
    // this is now empty, and every remaining SubMenu stays closed exactly as
    // it already did.
    let openAllsubMenuKeyList = [];
    return (
      <>
        <Menu
          // onClick={this.handleClick}
          // selectedKeys={[this.current]}
          selectable={true}
          // style={{align: 'left', width: 200}}
          defaultSelectedKeys={openAllsubMenuKeyList}
          defaultOpenKeys={[]}
          openKeys={openAllsubMenuKeyList}
          mode="inline">
          {uInspUI}
          {uInspESP32UI}
          {stationUI}
          {SLIDUI}
        </Menu>

        {resultMenu}
        {/* The full setup panel, on demand. Mounted only while open so its 1Hz
            poll does not share the serial link with the strip above for the
            whole shift. */}
        {/* `visible`, not `open` -- antd 4.22.8 (see the note on the fake-camera
            modal in script.jsx). This one was dead the same way: 設定/診斷 set
            the state and no panel ever appeared. */}
        <Modal visible={this.state.uInspESP32_popUp === true} title="全檢設備 v2 (uInspESP32)"
          onCancel={() => this.setState({ ...this.state, uInspESP32_popUp: false })}
          onOk={() => this.setState({ ...this.state, uInspESP32_popUp: false })}
          footer={null} destroyOnClose width={560}>
          {this.state.uInspESP32_popUp === true ? <UINSP_ESP32_UI/> : null}
        </Modal>
        {fullScreenMODAL}
        {
          uInspUI===null?null:
          <UInspMiscCtrlPopUp force_popUp={this.state.perifMISCCtrl_popUp} 
          onCancel={_=>this.setState({
            perifMISCCtrl_popUp: false,
          })}/>
        }
        {
          SLIDUI===null?null:
          <SLIDMiscCtrlPopUp force_popUp={this.state.perifMISCCtrl_popUp} 
            onCancel={_=>this.setState({
              perifMISCCtrl_popUp: false,
            })}/>
        }



      </>
    );
  }
}


class CanvasComponent extends React.Component {
  constructor(props) {
    super(props);
    this.windowSize = {};
  }
  triggerROISelect()
  {

  }

  componentDidUpdate(prevProps) {
    if (prevProps.onROISettingCallBack !== this.props.onROISettingCallBack && this.ec_canvas !== undefined) {

      if( this.props.onROISettingCallBack!==undefined)
      {
        this.ec_canvas.SetROISettingCallBack(this.props.onROISettingCallBack);
      }
      else
      {
        this.ec_canvas.SetROISettingCallBack(undefined);
      }
    }

    // The stream resolution cannot be chosen until the instrument scale is
    // known, and lens_calib.json usually arrives AFTER the canvas has taken
    // its size. The handler bails in that window, and nothing re-emits on its
    // own -- so the level would stay at 1 for the whole session unless someone
    // happened to zoom. Negotiate once, as soon as mmpp turns up.
    if (!(prevProps.instrument_mmpp > 0) && this.props.instrument_mmpp > 0
        && this.ec_canvas !== undefined) {
      this.ec_canvas.zoom_emit();
    }
  }

  ec_canvas_EmitEvent(event) {
    switch (event.type) {
      case DefConfAct.EVENT.ERROR:
        log.error(event);
        this.props.ACT_ERROR();
        break;
      case "down_samp_level_update":
        // log.error(event);
        // this.props.ACT_ERROR();

        // Defensive: camera_calibration_report may be undefined, or its reports[]
        // may be missing/empty depending on backend state. R6/R7 found this throws
        // a TypeError caught by RootErrorBoundary on certain core states. Bail
        // gracefully — sending no down-sample update is preferable to a crash.
        // Instrument scale from lens_calib.json (UIData.instrument_mmpp), not
        // from the camera_calibration report -- the core stopped emitting that
        // report, so the old path was permanently undefined and this branch
        // always bailed.
        let mmpp = this.props.instrument_mmpp;
        if (!(mmpp > 0)) { log.warn("down_samp_level_update: no instrument mmpp (lens_calib.json not loaded); skipping"); break; }
        // event.data.down_samp_level*=this.props.downSampleFactor;
        let crop = event.data.crop.map(val => val / mmpp);
        // console.log(this.props.downSampleFactor);
        // Send only as many pixels as the canvas can show.
        //
        // event.data.down_samp_level is world units (mm) per CANVAS pixel, so
        // dividing by mmpp gives sensor pixels per canvas pixel -- exactly the
        // factor by which the frame is oversampled for this view. At the usual
        // zoom-to-fit, a 2448-wide sensor in a ~1200 px canvas is ~2.04.
        //
        // This was pinned to 1 in 1b843b32 ("full-res live view"), which cost
        // 9.04 ms of encode and 269 KB per frame and was paid for by dropping
        // the CI framerate 15 -> 8. Measured on a real frame (jpeg_bench
        // "downsamp"), DS 2 with INTER_AREA is 2.30 ms and 39.9 KB -- and it
        // is NOT softer: 1224 px into a 1200 px canvas is still 1:1. What made
        // the old behaviour unusable was the factor, not the idea. The old
        // formula was Math.floor(x / mmpp * 2) + 1, which returns 5 for x=2.04
        // -- 490 px stretched across 1200, visibly blurred -- on top of the
        // core's INTER_NEAREST aliasing (both now fixed).
        //
        // Quantised to 1/2/4: odd factors have no fast resize path in the core
        // (DS 3 spends 3.68 ms in resize alone, worse than DS 2 in total).
        // Rounding DOWN is deliberate -- it can only ever send more pixels
        // than the canvas needs, never fewer.
        // Taken from the canvas, not recomputed here. Deriving it from
        // instrument_mmpp looked equivalent and was not: the canvas builds its
        // camera transform from a different mmpp, and mixing the two asked for
        // level 4 where the truth was 0.68 -- a 204 px wide live view. If the
        // canvas cannot supply it, do nothing rather than guess.
        const oversample = event.data.sensor_px_per_canvas_px;
        // Published for the harness. The zoom test kept reporting FAIL and
        // every attempt to explain it came down to estimating how wide the
        // part was drawn in a screenshot, which gave a different answer each
        // time. This is the number the decision is actually made from.
        if (typeof window !== 'undefined') window.__STREAM_OVERSAMPLE__ = oversample;
        if (!Number.isFinite(oversample) || oversample <= 0) break;

        // A 10% deadband on the way up. Without it a canvas sitting exactly on
        // a boundary (a maximised window often does) flips between two levels
        // on every resize event, and each flip is a full re-negotiation.
        const prev = this._streamDS || 1;
        const up = 1.1, down = 0.9;
        let down_samp_level = 1;
        for (const cand of [4, 2]) {
          if (oversample >= cand * (cand > prev ? up : down)) { down_samp_level = cand; break; }
        }
        if (down_samp_level === prev) break;
        this._streamDS = down_samp_level;

        log.info(`stream downsample ${prev} -> ${down_samp_level} `
               + `(${oversample.toFixed(2)} sensor px per canvas px)`);
        this.props.ACT_WS_SEND_CORE_BPG("ST", 0, {
          CameraSetting: { down_samp_level },
          LAST_FRAME_RESEND: true,
        });
        break;

    }
  }

  componentDidMount() {
    this.ec_canvas = new EC_CANVAS_Ctrl.INSP_CanvasComponent(this.refs.canvas);
    this.ec_canvas.EmitEvent = this.ec_canvas_EmitEvent.bind(this);
    this.props.onCanvasInit(this.ec_canvas);
    this.updateCanvas(this.props.c_state);
  }

  componentWillUnmount() {
    this.ec_canvas.resourceClean();
  }

  updateCanvas(ec_state, props = this.props) {
    if (this.ec_canvas !== undefined) {
      // log.debug("updateCanvas>>");

      let cur__surpress_display=props._edit_info.reportStatisticState.__surpress_display;
      // The overlay must belong to the image under it.
      //
      // Reports and images are throttled INDEPENDENTLY in the core: images stop
      // above OK/NG/NA_MAX_FPS (6), reports never do. Measured on this machine,
      // 870 of 1470 verdicts -- 59% -- were sent with no image behind them. Sync
      // the report on every one of those and the overlay races ahead of the
      // picture, which is exactly the "new overlay on an old frame" that only
      // shows up at speed.
      //
      // Dropping the extra reports instead is not an option: the DB upload rides
      // on them, so it has to be every report and a sampled image.
      //
      // Ordering does the pairing for free -- the core sends RP then IM inside
      // one group, so when a new image arrives the last report IS its report.
      // updateImgOnly keeps the previous edit_DB_info, i.e. the overlay keeps
      // matching what is on screen, while statistics and upload still see every
      // report through redux, untouched.
      const _imgChanged = (this.pre_img !== props.img);
      if(cur__surpress_display!=true || _imgChanged)
      {
        this.ec_canvas.EditDBInfoSync(props._edit_info, /*updateImgOnly=*/ !_imgChanged);
        this.ec_canvas.SetState(ec_state);
        this.ec_canvas.SetMeasureDisplayRank(props.measureDisplayRank);
        // Mirror System_Setting.SHOW_CALIPER_HITS_INSP to the renderer; per-
        // shape drawInspection reads renderer.show_caliper_hits.
        this.ec_canvas.rUtil.show_caliper_hits = props.showCaliperHits !== false;
        //this.ec_canvas.ctrlLogic();
        // When EditDBInfoSync just started a JPEG decode, the decode callback
        // will draw with the NEW bitmap after this synchronous block finishes
        // (single-threaded: it always sees the setters above already applied).
        // Drawing here too painted the OLD bitmap under the new overlays and
        // doubled the canvas work -- see SetImg's _imgDecodePending.
        if (!this.ec_canvas._imgDecodePending) this.ec_canvas.draw();
        this.ec_canvas.doRotateView=this.props.renderObjAlignRotate;

      }
      this.pre_img=props.img;
    }
  }

  onResize(width, height) {
    
    if (Math.hypot(this.windowSize.width - width, this.windowSize.height - height) < 15) return;
    if (this.ec_canvas !== undefined) {
      this.ec_canvas.resize(width, height);
      this.windowSize = {
        width, height
      }
      this.updateCanvas(this.props.c_state);
    }
  }

  componentWillUpdate(nextProps, nextState) {
    this.updateCanvas(nextProps.c_state, nextProps);
  }

  render() {
    return (
      <div className={this.props.addClass}>
        <canvas ref="canvas" className="width12 HXF" />
        <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
      </div>
    );
  }
}


const mapStateToProps_CanvasComponent = (state) => {
  //log.info("mapStateToProps",JSON.stringify(state.UIData.c_state));
  return {
    c_state: state.UIData.c_state,
    img:state.UIData.edit_info.img,
    _edit_info:state.UIData.edit_info,
    // Mirror the System_Setting overlay flag so updateCanvas can push it
    // onto rUtil before each draw (per-shape drawInspection reads it).
    showCaliperHits: state.UIData.System_Setting?.SHOW_CALIPER_HITS_INSP !== false,
    // 儀器尺度的單一來源 (data/lens_calib.json)。down_samp_level_update 用它把
    // mm 換成 px, 舊路徑讀的 camera_calibration report 核心已不再發出。
    instrument_mmpp: state.UIData.instrument_mmpp,
    //just to trigger update if changed
  }
}


const mapDispatchToProps_CanvasComponent = (dispatch, ownProps) => {
  return {
    ACT_EXIT: (arg) => {
      dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))
    },
    ACT_ERROR: (arg) => {
      dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.ERROR))
    }
  }
}
const CanvasComponent_rdx = connect(
  mapStateToProps_CanvasComponent,
  mapDispatchToProps_CanvasComponent)(CanvasComponent);



class ControlChart extends React.Component {
  constructor(props) {
    super(props);
    this.divID = "ControlChart_ID_" + Math.round(Math.random() * 10000);
    this.charObj = undefined;

    this.state = {
      chartOpt: {
        type: 'line',
        data: {
          labels: [], datasets: [{
            type: "line",
            borderColor: "rgb(100, 255, 100)",
            lineTension: 0.2, data: [],
            pointBackgroundColor: []
          }]
        },
        bezierCurve: false,
        options: {
          scales: {
            xAxes: [{
              ticks: {
                callback: function (value, index, values) {
                  return value;
                }
              }
            }]
          },
          elements: {
            line: { fill: false },
            point: { radius: 6 }
          },
          bezierCurve: false,
          animation: {
            duration: 0
          },
          maintainAspectRatio: false,
          responsive: true,
          title: {
            display: true,
            text: ''
          },
          annotation: {
            annotations: []
          },
          legend: {
            display: false
          },
          tooltips: {
            enabled: true
          }
        }
      }
    };

    this.default_annotationTargets = [
      { type: "USL", color: "rgba(200, 0, 0,0.2)" },
      { type: "LSL", color: "rgba(200, 0, 0,0.2)" },
      { type: "UCL", color: "rgba(100, 100, 0,0.2)" },
      { type: "LCL", color: "rgba(100, 100, 0,0.2)" },
      { type: "value", color: "rgba(0, 0, 0,0.2)" },
    ];
  }


  componentWillUpdate(nextProps, nextState)
  {

  }

  updateChart(nextProps)
  {
    if(this.charObj===undefined)return;

    //Make sure the data object is the same, don't change it/ you gonna set the data object to chart again
    this.state.chartOpt.data.labels = [];
    this.state.chartOpt.data.datasets.forEach((datInfo) => {
      datInfo.data = [];
      datInfo.pointBackgroundColor = [];
    });
    let length = nextProps.reportArray.length;
    if (length == 0) return;
    let newTime = nextProps.reportArray[length - 1].time_ms;
    this.state.chartOpt.options.title.text = nextProps.reportArray[0].judgeReports.find((jrep) => jrep.id == nextProps.targetMeasure.id).name;

    nextProps.reportArray.reduce((acc_data, rep, idx) => {
      acc_data.labels.push((newTime - rep.time_ms) / 1000);

      let measureObj = rep.judgeReports.find((jrep) => jrep.id == nextProps.targetMeasure.id);

      let pointColor = undefined;
      let val = measureObj.value;
      pointColor = OK_NG_BOX_COLOR_TEXT[measureObj.detailStatus].COLOR;
      if (pointColor === undefined) {
        pointColor = "#AA0000";
      }
      //console.log(measureObj.detailStatus);
      if (measureObj.detailStatus === MEASURERSULTRESION.NA) {
        val = undefined;
      }



      acc_data.datasets[0].pointBackgroundColor.push(pointColor);
      //TODO:for now there is only one data set in one chart
      acc_data.datasets[0].data.push(val);
      return acc_data;
    }, this.state.chartOpt.data);



    let annotationTargets = this.props.nnotationTargets;
    if (annotationTargets === undefined) {
      annotationTargets = this.default_annotationTargets
    }

    this.state.chartOpt.options.annotation.annotations =
      annotationTargets.map((annotationTar) => {

        let val = nextProps.targetMeasure[annotationTar.type];
        return ({
          type: 'line',
          mode: 'horizontal',
          scaleID: 'y-axis-0',
          value: val,
          borderColor: annotationTar.color,
          borderWidth: 4,
          borderDash: [12, 12],
          label: {
            position: "right",
            enabled: true,
            content: val
          }
        });
      });


    this.charObj.update();
  }


  componentDidMount() {
    var ctx = document.getElementById(this.divID).getContext("2d");

    this.charObj = new Chart(ctx, this.state.chartOpt);
    this.updateChart(this.props);
  }
  onResize(width, height) {
    //log.debug("G2HOT resize>>", width, height);
    //this.state.G2Chart.changeSize(width, height);

  }

  render() {
    this.updateChart(this.props);
    return <div className={this.props.className}>
      <canvas id={this.divID} style={{ height: "400px" }} />
      <ReactResizeDetector handleWidth handleHeight onResize={this.onResize.bind(this)} />
    </div>
  }

}



class DataStatsTable extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      drawList: []
    };
  }
  shouldComponentUpdate(nextProps,nextState) {
    
    if(nextState!=this.state)
    {
      return true;
    }

    let statstate = nextProps.reportStatisticState;
    if (statstate.historyReport === undefined) {
      return false;
    }
    let historyReport = statstate.historyReport;
    if (historyReport.length == 0) {
      return false;
    }
    if (historyReport.length > 0 && this.pre_last_rep == historyReport[historyReport.length - 1]) {
      return false;
    }
    this.pre_last_rep = historyReport[historyReport.length - 1];
    return true;

  }
  render() {
    let statstate = this.props.reportStatisticState;
    //console.log(statstate);
    if (statstate.statisticValue === undefined) {
      return null;
    }
    let measureList = statstate.statisticValue.measureList;

    // console.log(measureList);
    let measureReports = measureList.filter(m=>m.rank===undefined || m.rank<=this.props.measureDisplayRank).map((measure) =>
      ({
        id: measure.id,
        name: measure.name,
        subtype: measure.subtype,
        count: measure.statistic.count,
        mean: round(measure.statistic.mean, 0.001),
        sigma: round(measure.statistic.sigma, 0.0001),


        // OK and NA are summed from the tags that MEAN them; NG is whatever is
        // left of the count. Both halves used to be hardcoded lists, so a
        // detailStatus the core learns to emit later would land in neither and
        // the row would silently stop adding up -- count says 900, OK+NG+NA
        // says 880, and nothing says where the twenty went.
        //
        // Deriving NG makes the row reconcile by construction, and puts an
        // unrecognised status on the NG side rather than the OK side. That is
        // the safe direction: a part whose verdict this UI cannot name is not
        // one to quietly count as good.
        OK: ["UOK", "LOK", "UCNG", "LCNG"].reduce((sum, tag) => sum + (measure.statistic.count_stat[tag] || 0), 0),
        NG: Math.max(0, measure.statistic.count
              - ["UOK", "LOK", "UCNG", "LCNG"].reduce((sum, tag) => sum + (measure.statistic.count_stat[tag] || 0), 0)),
        // count excludes NA (statReducer returns before count++ on an NA), so
        // NA is its own tally and does not belong in the subtraction above.
        NA: measure.statistic.count_stat.NA,
        WARN: ["UCNG", "LCNG"].reduce((sum, tag) => sum + (measure.statistic.count_stat[tag] || 0), 0),

        CK: round(measure.statistic.CK, 0.001),
        // CPU:round(measure.statistic.CPU,0.001),
        // CPL:round(measure.statistic.CPL,0.001),
        CP: round(measure.statistic.CP, 0.001),
        CPK: round(measure.statistic.CPK, 0.001),
        MIN: round(measure.statistic.MIN, 0.001),
        MAX: round(measure.statistic.MAX, 0.001),
      })
    );

    if (measureReports.length == 0) return null;
    //log.error(measureReports);


    //statstate.historyReport.map((rep)=>rep.judgeReports[0]);
    const dataSource = measureReports;
    const columns = ["name", "subtype", "count", "mean", 
      "MIN", "MAX","OK", "NG", "CPK","sigma",
      "NA", "WARN",
      "CK", "CP",
      //"CPU","CPL",
      ]
      .map((type) => ({ title: type, dataIndex: type, key: type }))
      .map((col) => (typeof measureReports[0][col.title] == 'number') ?//Find the first dataset and if it's number then add a sorter
        Object.assign(col, { sorter: (a, b) => a[col.title] - b[col.title] }) : col)
    /*columns[0].fixed="left";
    columns[0].width=100;

    columns[columns.length-1].fixed="right";
    columns[columns.length-1].width=100;*/
    columns.push(
      {
        title: "Draw Toggle", key: "draw", fixed: "right",
        render: (text, record) => {
          return <Switch onChange={(val) => {
            // this.state.drawList[record.id] = val;
            let newDrawList = [...this.state.drawList];
            newDrawList[record.id]=val;
            this.setState({drawList:newDrawList});
            
            log.debug("[trigger]");
          }
          } />
        }
      }
    );

    let graphX = Object.keys(this.state.drawList).map((key, idx) => {
      if (this.state.drawList[key] == true) {
        let targetMeasure = measureList.find((m) => m.id == key);

        // Math.max(..., 0), not 1. With the history capped at 100 the window is
        // now SHORTER than lastN, and the old floor of 1 sliced from index 1 --
        // quietly dropping the oldest point from every chart.
        let lastN = 500;
        let lastNArr = statstate.historyReport.slice(Math.max(statstate.historyReport.length - lastN, 0));
        return <ControlChart reportArray={lastNArr} targetMeasure={targetMeasure} />
      }
      return null;
    });
    //console.log(graphX);

    return (
      <div className={this.props.className}>
        <Table key="dat_table" className="antd-table-small" dataSource={dataSource} columns={columns} size="small" scroll={{ x: 1000 }} pagination={false} />
        {graphX}
      </div>
    );
  }
}

let skip_counter = 0;

class AngledCalibrationHelper extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      target_line: undefined,
      target_measure: undefined,
      target_spoint: undefined,
      angledOffsetTable: {},
      angleStatTable: [],
      updateCount: 0,
      shape_list: undefined,
      camera_calibration_report: undefined,//just for comparison
      mmpp: -1
    };


    for (let i = 0; i < 6; i++) {
      this.state.angleStatTable.push({
        w: 0,
        value: 0
      });
    }
  }

  static getDerivedStateFromProps(props, state) {
    let newState = state;
    if (newState.shapeList === undefined) {
      let shape_list = dclone(props.shape_list);
      newState = { ...newState, shape_list };
    }

    if (newState.shape_list === undefined) return newState;


    // Instrument scale straight from lens_calib.json -- see the note on the
    // other consumer above. Keyed on the mmpp itself now, since the report it
    // used to watch no longer arrives.
    if (props.instrument_mmpp !== state.mmpp) {
      newState = { ...newState, mmpp: props.instrument_mmpp };
    }


    if (
      newState.target_measure !== undefined &&
      newState.target_line !== undefined
      && props.reportStatisticState.newAddedReport !== undefined
      && props.reportStatisticState.newAddedReport.length !== 0) {
      let historyReport = props.reportStatisticState.newAddedReport;
      //console.log(historyReport);
      historyReport.forEach(rep => {


        let measure = rep.judgeReports.find(jrep => jrep.id == state.target_measure.id);
        //let spoint=rep.searchPoints.find(jrep=>jrep.id==state.target_spoint.id);
        let line = rep.detectedLines.find(jrep => jrep.id == state.target_line.id);



        let ang =
          Math.atan2(line.vy, line.vx) * 180 / Math.PI
          + 90;

        if (state.target_spoint !== undefined) {
          ang += state.target_spoint.angleDeg - 180;
        }
        //if(ang<0)ang+=360;


        function fmod(a, b) {
          return a - (Math.floor(a / b) * b);
        }
        ang = fmod(ang, 360);

        let pixOffset = measure.value / newState.mmpp;

        function addStatValue(id, portion, value) {
          let tab = newState.angleStatTable[id];
          if (tab === undefined) return;
          tab.w += portion;
          tab.value += portion * value;
        }

        function AddAngleOffsetRec(angleDeg, pixOffset) {
          angleDeg = fmod(angleDeg, 360);
          let section_f = angleDeg / (360 / newState.angleStatTable.length);
          let section = Math.floor(section_f);
          let portion = 1 - (section_f - section);

          let lowID = section;
          let highID = (section + 1) % newState.angleStatTable.length;

          addStatValue(lowID, portion, pixOffset);
          addStatValue(highID, 1 - portion, pixOffset);


        }

        //console.log(ang,pixOffset);
        AddAngleOffsetRec(ang, pixOffset);
        AddAngleOffsetRec(ang + 180, pixOffset);


      })

      if ((skip_counter++) % 10 == 0) {
        let angleStatTable = dclone(newState.angleStatTable);

        //averaging
        if (true) {
          for (let i = 0; i < angleStatTable.length; i++) {
            let i_warp = i + angleStatTable.length;
            let pre_stat_1 = newState.angleStatTable[(i_warp - 1) % angleStatTable.length];
            let pre_stat_2 = newState.angleStatTable[(i_warp - 0) % angleStatTable.length];
            let pre_stat_3 = newState.angleStatTable[(i_warp + 1) % angleStatTable.length];

            let new_stat = angleStatTable[i];
            let beta = 4;
            new_stat.w = (pre_stat_1.w + beta * pre_stat_2.w + pre_stat_3.w) / (2 + beta);
            new_stat.value = (pre_stat_1.value + beta * pre_stat_2.value + pre_stat_3.value) / (2 + beta);
          }
        }

        let mean = angleStatTable.reduce((sum, stat) => sum + stat.value / (stat.w + 0.0001), 0) / angleStatTable.length;

        let offsetMap = angleStatTable.map(stat => stat.value / (stat.w + 0.0001) - mean);
        let tableX = {};
        offsetMap.forEach((ele, idx) => {
          let key = "" + 360 * idx / offsetMap.length;
          tableX[key] = -round(ele, 0.00001)
        });
        // angledOffsetTable
        let updateCount = (newState.updateCount === undefined) ? 0 : newState.updateCount + 1;
        newState = {
          ...newState
          , angledOffsetTable: tableX
          , updateCount
        };
      }
    }

    return newState;
  }

  render() {


    let measure_dist_list = this.state.shape_list
      .filter(shape => shape.type == "measure" && shape.subtype == "distance")


    const menu = (
      <Menu
        onClick={(ev) => {
          if (ev.key == -1) {

            this.setState({
              target_measure: undefined, target_line: undefined, target_spoint: undefined,
              angledOffsetTable: {}, updateCount: 0
            })
            return;
          }
          let slist = this.state.shape_list;
          let target_measure = slist.find(shape => shape.id == ev.key);
          let middleObject = target_measure;

          let ref_id = middleObject.ref[0].id;
          middleObject = slist.find(shape => shape.id == ref_id);
          let target_spoint = undefined;
          let target_line = middleObject;

          if (middleObject.type == "search_point") {
            target_spoint = middleObject;
            ref_id = middleObject.ref[0].id;
            middleObject = slist.find(shape => shape.id == ref_id);
            target_line = middleObject;
          }


          let angleStatTable = dclone(this.state.angleStatTable);
          angleStatTable.forEach(st => { st.w = st.value = 0; })
          this.setState({
            target_measure, target_line, target_spoint,
            angledOffsetTable: {}, angleStatTable, updateCount: 0
          })
        }}>
        {measure_dist_list.concat([{ id: -1, name: "CANCEL" }])
          .map((mea, idx) =>
            <Menu.Item key={mea.id}>
              <a target="_blank" rel="noopener noreferrer">
                {mea.id + ":  " + mea.name}
              </a>
            </Menu.Item>)}
      </Menu>
    );



    let displayDropDown = <Dropdown overlay={menu}>
      <a className="ant-dropdown-link HX1" href="#">
        {this.state.target_measure === undefined ?
          "SELECT" :
          ">>>" + this.state.target_measure.name}
        <CaretDownOutlined />
      </a>
    </Dropdown>
    return <div className={this.props.className}>
      <div className="s HXA width12" style={{ padding: "10px" }}>
        <Divider orientation="left" key="divi">Angled Calib</Divider>
        {displayDropDown}
        <Button type="primary" icon="download"
          onClick={() => { copyToClipboard(JSON.stringify(this.state.angledOffsetTable)) }}>
          {this.state.updateCount}
        </Button>

      </div>

    </div>
  }

}

function RestrictiveCircleREdit ({initR,onRChanged}){
      
  let rankMin=0;
  let rankMax=1000;
  const [sliderV,setSliderV]=useState(initR===undefined?rankMax:initR);

  
  useEffect(()=>{
    if(onRChanged!==undefined)
    {
      onRChanged(sliderV);
    }
  },[sliderV])

  return <>
  
    <Slider
      step={10}
      min={rankMin}
      max={rankMax}
      onChange={setSliderV}
      value={sliderV}
    />
  </>;
}


// The caliper-hit toggle. It controls whether the CORE EMITS the payload, not
// whether the WebUI draws it -- one gate, at the source. With no hits in the
// report there is nothing to draw, so the overlay follows for free, and the
// ~85% of the inspection record that cal_hits costs (measured: 22 KB -> 3.5 KB
// at ~300 hits a part) is actually saved rather than merely hidden.
//
//   ST { "DEBUG_EMIT": { "cal_hits": bool } }   <- ROOT level
//
// Sent at the ST root, NOT inside MachineSetting: that path runs
// setup_machine_setting() -> load_clean_regions(), where an absent key means
// "no clean regions", so a one-key MachineSetting silently wipes the station's
// clean areas. Unknown names are logged and ignored by the core, so an older
// core degrades to "not sent" rather than to a rejected command.
//
// It is subscribed to the store on its own rather than reading APP_INSP_MODE's
// props: the modal that hosts it is built ONCE into this.state.additionalUI
// (setInspectionRankUI) as already-created elements, so `checked` would be
// frozen at the value it had when the modal was opened; and APP_INSP_MODE's
// shouldComponentUpdate gates re-render on the report counter, so a prop-only
// change would be swallowed anyway.
//
// The core has no read-back for DEBUG_EMIT and resets to its built-in default
// (cal_hits: true) whenever it restarts, so the UI's remembered position is the
// only record of intent -- it is re-sent on mount to make the core agree.
const CaliperHitsSwitch = (props) => {
  const { CORE_ID, System_Setting, SEND_ST, ACT_System_Setting_Update } = props;
  const on = System_Setting?.EMIT_CALIPER_HITS !== false;

  useEffect(() => {
    if (CORE_ID === undefined) return;
    SEND_ST(CORE_ID, on);
  }, [CORE_ID]);

  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginTop: 8 }}>
      <Switch
        size="small"
        checked={on}
        onChange={(val) => {
          ACT_System_Setting_Update({ ...System_Setting, EMIT_CALIPER_HITS: val });
          if (CORE_ID !== undefined) SEND_ST(CORE_ID, val);
        }}
      />
      <span>核心送出卡尺命中點 / Emit caliper hits</span>
    </div>
  );
};

const CaliperHitsSwitch_rdx = connect(
  (state) => ({
    System_Setting: state.UIData.System_Setting,
    CORE_ID: state.ConnInfo.CORE_ID,
  }),
  (dispatch) => ({
    ACT_System_Setting_Update: (sysSetting) =>
      dispatch({ type: "System_Setting_Update", data: sysSetting }),
    SEND_ST: (coreId, on) =>
      dispatch(UIAct.EV_WS_SEND_BPG(coreId, "ST", 0, { DEBUG_EMIT: { cal_hits: on } })),
  }))(CaliperHitsSwitch);

// What this machine writes to disk, per verdict and per part.
//
// Measured on the bench before this existed: 146 KB a part (103 KB image +
// 43 KB report) at ~11 parts/s into a folder capped at 8 files -- ~138 GB a
// day written to retain 1.2 MB, because every NA part was saved whole and
// removeOldestRep() deleted it within a second. The image is 70% of those
// bytes and is usually NOT the evidence: a report says what the machine
// decided, an image says what it saw. Being able to keep one without the
// other is what makes routine evidence affordable.
//
// Persisted in machine_custom_setting (FI_INSP_SNAP_POLICY) because the core
// zeroes its own policy at the start of every inspection session -- the
// machine setting is the only record of what was chosen, and it is re-pushed
// on entering inspection.
const SNAP_VERDICTS = [
  { key: 'NG', label: 'NG 不良' },
  { key: 'NA', label: 'NA 無判定' },
  { key: 'OK', label: 'OK 良品' },
];

// Matches the core's own initialisers (wiringPanel.cpp g_snap_policy):
// everything off. A machine writes to its disk because someone asked it to,
// not because nobody turned it off.
export const SNAP_POLICY_DEFAULT = {
  OK: { img: false, rep: false },
  NG: { img: false, rep: false },
  NA: { img: false, rep: false },
};

// How many reports go to the DB: 1 uploaded out of every N produced.
//
// The value used to live only in System_Setting, which script.jsx rebuilds from
// GetDefaultSystemSetting() on every launch -- so it was not a setting at all,
// only a compiled-in constant with a debug override. machine_custom_setting is
// the persisted store, so that is where an operator-set value belongs; the
// System_Setting numbers stay as the fallback for machines that never set one.
//
// Returns at least 1. A 0 would reach `total % skip` as a division by zero,
// come back NaN, and NaN != 0 is true -- so every report would be skipped and
// the machine would silently upload nothing. That was unreachable while the
// value was compiled in; it stops being unreachable the moment a person can
// type into it.
export function uploadSkipOf(machine_custom_setting, System_Setting) {
  const mcs = machine_custom_setting || {};
  const sys = System_Setting || {};
  const isCI = mcs.InspectionMode == "CI";
  const key = isCI ? "CI_MODE_UPLOAD_SKIP" : "FI_MODE_UPLOAD_SKIP";
  const v = (mcs[key] !== undefined && mcs[key] !== null) ? mcs[key] : sys[key];
  const n = parseInt(v);
  return Number.isFinite(n) && n >= 1 ? n : 1;
}

// The tracking-window parameters, resolved the same way as uploadSkipOf:
// machine_custom_setting (persisted) first, System_Setting (rebuilt from
// GetDefaultSystemSetting on every launch) as the compiled fallback.
//
// What these control is how many times ONE part is measured and how those
// measurements are combined -- reports of the same object are blended in the
// tracking window, so "repeat" is an averaging depth, not a display option.
//
// Merged per-key rather than replaced wholesale: FI's default carries no
// maxReportRepeat at all, and an absent cap means "never stop blending". A
// whole-object replace would have to invent a number for it, which silently
// turns an unbounded average into a bounded one.
export function statSettingOf(machine_custom_setting, System_Setting, mode) {
  const mcs = machine_custom_setting || {};
  const sys = System_Setting || {};
  const key = (mode === "CI") ? "CI_MODE_StatSettingParam" : "FI_MODE_StatSettingParam";
  return { ...(sys[key] || {}), ...(mcs[key] || {}) };
}

export function snapPolicyOf(machine_custom_setting) {
  const mcs = machine_custom_setting || {};
  const stored = mcs.FI_INSP_SNAP_POLICY;
  if (stored) {
    return {
      OK: { ...SNAP_POLICY_DEFAULT.OK, ...(stored.OK || {}) },
      NG: { ...SNAP_POLICY_DEFAULT.NG, ...(stored.NG || {}) },
      NA: { ...SNAP_POLICY_DEFAULT.NA, ...(stored.NA || {}) },
    };
  }
  // No policy stored yet: everything off, deliberately NOT derived from the
  // legacy FI_INSP_NG_SNAP. That key defaults to true on existing machines,
  // and inheriting it would mean the new all-off default never actually
  // applies anywhere it matters. The legacy ST keys are still accepted by the
  // core, so an older WebUI driving this core is unaffected.
  return { OK: { ...SNAP_POLICY_DEFAULT.OK },
           NG: { ...SNAP_POLICY_DEFAULT.NG },
           NA: { ...SNAP_POLICY_DEFAULT.NA } };
}

const SnapPolicyPanel = (props) => {
  const { CORE_ID, machine_custom_setting, SEND_ST, ACT_machine_custom_setting_Update } = props;
  const pol = snapPolicyOf(machine_custom_setting);
  const maxNum = (machine_custom_setting || {}).FI_INSP_NG_SNAP_MAX_NUM || 1000;

  const set = (verdict, part, val) => {
    const next = { ...pol, [verdict]: { ...pol[verdict], [part]: val } };
    ACT_machine_custom_setting_Update({
      ...(machine_custom_setting || {}),
      FI_INSP_SNAP_POLICY: next,
      // Keep the legacy key in step so the 設定 page and an older core still
      // see something truthful rather than a stale value.
      FI_INSP_NG_SNAP: next.NG.img || next.NG.rep,
    });
    if (CORE_ID !== undefined) SEND_ST(CORE_ID, { INSP_SNAP_POLICY: next });
  };

  return (
    <div style={{ marginTop: 8 }}>
      <div style={{ fontWeight: 600, marginBottom: 4 }}>快照儲存 / Snapshot to disk</div>
      <table style={{ borderCollapse: 'collapse' }}>
        <tbody>
          <tr style={{ color: '#888', fontSize: 12 }}>
            <td style={{ padding: '2px 10px 2px 0' }}></td>
            <td style={{ padding: '2px 10px' }}>影像 .jpg</td>
            <td style={{ padding: '2px 10px' }}>報告 .xreps</td>
          </tr>
          {SNAP_VERDICTS.map((v) => (
            <tr key={v.key}>
              <td style={{ padding: '2px 10px 2px 0' }}>{v.label}</td>
              <td style={{ padding: '2px 10px' }}>
                <Switch size="small" checked={pol[v.key].img}
                  onChange={(c) => set(v.key, 'img', c)} />
              </td>
              <td style={{ padding: '2px 10px' }}>
                <Switch size="small" checked={pol[v.key].rep}
                  onChange={(c) => set(v.key, 'rep', c)} />
              </td>
            </tr>
          ))}
        </tbody>
      </table>
      <div style={{ marginTop: 6 }}>
        每資料夾上限:
        <InputNumber size="small" min={1} max={100000} step={1} value={maxNum}
          style={{ marginLeft: 6, width: 90 }}
          onChange={(v) => {
            const n = Math.max(1, Math.round(v || 1));
            ACT_machine_custom_setting_Update({
              ...(machine_custom_setting || {}), FI_INSP_NG_SNAP_MAX_NUM: n });
            if (CORE_ID !== undefined) SEND_ST(CORE_ID, { INSP_NG_SNAP_MAX_NUM: n });
          }} />
      </div>
      <div style={{ fontSize: 12, color: '#888', marginTop: 6, lineHeight: 1.7 }}>
        存到 <code>data/SAMPLE/日期/配方/</code>。滿了就刪最舊的一組。
        每組約 146 KB（影像 103 KB + 報告 43 KB），所以全開時 20 件/秒 ≈ 每天上百 GB
        寫入——而資料夾只留最後 {maxNum} 組，其餘全部是白寫的。
      </div>
    </div>
  );
};

// Exported, and rendered ONLY by the setup tab. Everything that decides what
// gets written to disk lives in one place; this file keeps snapPolicyOf()
// because FI start is what actually pushes the policy to the core.
export const SnapPolicyPanel_rdx = connect(
  (state) => ({
    machine_custom_setting: state.UIData.machine_custom_setting,
    CORE_ID: state.ConnInfo.CORE_ID,
  }),
  (dispatch) => ({
    ACT_machine_custom_setting_Update: (setting) =>
      dispatch(UIAct.EV_machine_custom_setting_Update(setting)),
    SEND_ST: (coreId, obj) => dispatch(UIAct.EV_WS_SEND_BPG(coreId, "ST", 0, obj)),
  }))(SnapPolicyPanel);

class APP_INSP_MODE extends React.Component {

  
  componentDidMount() {
    let DefFileHash=this.props.edit_info.DefFileHash;
    // Trigger-mode policy: only CI InspMode runs the camera free-running.
    // Flip to continuous on mount and back to trigger=On on unmount so the
    // camera doesn't flood frames when no one is inspecting.
    //
    // FI must NOT free-run. It pairs one frame to one part off the machine's
    // own trigger, so a free-running sensor produces frames no part asked for
    // -- the core logs them as "frame with no pending trigger -- pairing
    // desynced?" and drops the results. This mount ran before the FI branch
    // below armed the hardware trigger, so it silently undid it: entering
    // InspMode left the camera streaming at the free-run framerate and the
    // plate's trigger unused.
    // if (this.props.machine_custom_setting.InspectionMode != "FI") {
    //   this.props.ACT_WS_SEND_CORE_BPG("ST", 0,
    //     { CameraSetting: { trigger_mode: 0 } });
    // }
    this.CameraCtrl.setCameraImageTransfer(true);

    this.CameraCtrl.setImageCropParam(undefined,1);

    {


      let ctrlMarginInfos=GetObjElement(this.props.info_decorator,["control_margin_info"]);

      // ONE FOLD, ONE FIELD, ONE TRUTH.
      //
      // Two things are written into the editor's shape_list before the wire def
      // is generated from it: the 製程's limit overrides, and the inspection
      // level. The level arrives as a tag named rankN -- so which level applies
      // is chosen the same way everything else per-製程 is chosen, by which tag
      // is on the part.
      //
      // The level is folded INTO quality_essential:
      //
      //   quality_essential := (rank <= N) AND quality_essential
      //
      // and after that nobody needs to know about rank again. The screen's
      // roll-up, the wire def and the core's InspStatusReduce all read the one
      // field, so they cannot disagree -- which they did until now, because the
      // core has no notion of rank and never had one.
      //
      // Without a rankN tag there is no fold and rank decides nothing. That is
      // deliberate: the 檢測等級 slider is a viewing control, and a viewing
      // control must never be able to change what a part is.
      const rankTag = (this.props.inspOptionalTag || [])
        .map((t) => /^rank(\d+)$/i.exec(String(t)))
        .find((m) => m !== null);
      const rankLimit = rankTag ? Number(rankTag[1]) : undefined;
      // The SAME level the fold used, kept for the display filter. There used
      // to be a 檢測等級 slider here deciding what was drawn; a viewing control
      // that hides measurements is fine, but it has to agree with the one the
      // core was told about, and a slider is a thing the core can never learn.
      this.setState({ measureDisplayRank: rankLimit === undefined ? Infinity : rankLimit });

      // The SAME pick the grading path uses (UTIL/ctrlMarginPick.js). This one
      // decides what the core is told; that one decides what the screen shows.
      // They took different tags until 2026-08-26.
      const _mpick = pickCtrlMargin(this.props.inspOptionalTag, ctrlMarginInfos);
      const curMarginInfo_name = _mpick.tag;
      const curMarginInfo = _mpick.info;

      if(curMarginInfo!==undefined || rankLimit!==undefined)
      {
        // Remember the ORIGINAL SHAPES this touches, keyed by id, so unmount
        // can put the values back. The write is into the EDITOR's shape_list
        // (the canvas overlays and the def generation below both need them
        // there), but it used to be one-way: after a single inspection run the
        // editor reported unsaved changes the operator never made, and saving
        // then baked the tag's limits into the base def -- with a recorded def
        // hash that no longer matched any file on disk.
        //
        // By VALUE per overridden id, not by list identity: this very mount
        // changes the list identity again two dispatches later
        // (Define_File_Update), so an identity-compared snapshot never matches
        // at unmount and a restore gated on it silently never runs (caught by
        // the inspCycle flow's console trace).
        this._preInspById = {};
        this._preInspDefName = (this.props.edit_info.loadedDefFile||{}).name;
        let newShapeList = [...this.props.shape_list];

        const remember = (idx) => {
          const sh = newShapeList[idx];
          if (this._preInspById[sh.id] === undefined) this._preInspById[sh.id] = sh;
        };

        if (curMarginInfo !== undefined) {
          curMarginInfo.forEach(info=>{
            let cur_shape_idx = newShapeList.findIndex(shape=>shape.id==info.id);
            if(cur_shape_idx!==-1)
            {
              remember(cur_shape_idx);
              newShapeList[cur_shape_idx]= { ...newShapeList[cur_shape_idx], ...info };
            }
          });
        }

        // AFTER the margin overrides, because a 製程 may override rank itself
        // and the level must be applied to the rank that ends up in force.
        if (rankLimit !== undefined) {
          newShapeList = newShapeList.map((shape, idx) => {
            if (shape.rank === undefined || shape.rank <= rankLimit) return shape;
            if (shape.quality_essential === false) return shape;
            remember(idx);
            return { ...shape, quality_essential: false };
          });
        }

        this.props.ACT_Shape_List_Update_EXPRESS(newShapeList);
      }

    }



    function insp_resolve(pkts,main_ch)
    {
      // console.log(pkts)
      
      let RP=pkts.find(pkt=>pkt.type=="RP");
      let IM=pkts.find(pkt=>pkt.type=="IM");
      if(RP!==undefined && IM===undefined)
      {
        RP.data.__surpress_display=true;
      }
      // console.log(IM!==undefined,RP!==undefined);
      // console.log(RP.data.__surpress_display);
      main_ch(pkts);
    }

    {
      // console.log("defFileGeneration>>>>>>>>");
      let deffile = defFileGeneration(this.props.edit_info);

      this.props.ACT_WS_Define_File_Update_EXPRESS(deffile,true)
      console.log("deffile",JSON.parse(JSON.stringify(deffile)));
      deffile.featureSet_sha1=DefFileHash;//fake the sha1 data since we might modify the deffile, but still need to have the same deffile hex


      // Shape-based matching needs its template, and the def carries only a
      // POINTER to it. Every other sender stamps that pointer (DefConfUI's
      // four, SBMStudio's two); this one did not, so a def authored with
      // shape_based matching trained fine in the editor and then, on entering
      // inspection, logged "[shape] no template path" and fell back to sig360
      // -- a SILENT downgrade, with the def unchanged and the editor still
      // showing it working. The core cannot fill it in: FI/CI get the def
      // inline as definfo, so there is no def path to resolve <base>.png
      // against.
      //
      // Stamped onto a COPY, at the moment of sending. _ref_image_path is a
      // single underscore and the def hash only strips DOUBLE-underscore
      // keys, so it counts toward featureSet_sha1 -- put it on the shared
      // object and the def that gets persisted no longer matches its own
      // recorded hash, and the next load is refused outright by the
      // integrity guard. It belongs on the wire and nowhere else.
      const wireDef = { ...deffile,
        featureSet: deffile.featureSet.map((f, i) => (i === 0 ? { ...f } : f)) };
      // The localizer's regions must never reach features[].
      //
      // inherentfeatures and features are CLOSED vocabularies in the core: an
      // unrecognised type does not get skipped, it fails the whole def --
      // "feature[19] has unknown type:[loc_include]" then "cJSON parse failed".
      // AddMatchingFeature then throws, the session-start code below never
      // runs, and the camera keeps whatever trigger mode the last thing left it
      // in. Measured here: every FI after a 快速驗證 preview died exactly this
      // way, and what reached a human was "the machine will not finish timing
      // calibration".
      //
      // defFileGeneration strips them too, and should. This is the boundary
      // where the contract is actually violated, so it is checked here as well:
      // a def that leaves for the core is the last place the mistake is still
      // cheap.
      if (Array.isArray(wireDef.featureSet[0].features)) {
        const _n = wireDef.featureSet[0].features.length;
        wireDef.featureSet[0].features = wireDef.featureSet[0].features.filter(
          (sh) => !sh || (sh.type !== 'loc_include' && sh.type !== 'loc_exclude'));
        if (wireDef.featureSet[0].features.length !== _n)
          log.warn('[wire-def] stripped ' + (_n - wireDef.featureSet[0].features.length)
                 + ' loc_include/loc_exclude shape(s) that would have failed the whole def');
      }
      stampRefImagePath(wireDef, this.props.edit_info);

      if (this.props.machine_custom_setting.InspectionMode== "FI" || this.props.machine_custom_setting.InspectionMode== "FI_C") {

        
        // On wireDef, not deffile: the copy is taken above, so mutating the
        // shared object here would leave these overrides out of what is
        // actually sent -- and would keep scribbling on the def the rest of
        // the app holds.
        wireDef.featureSet[0].matching_angle_margin_deg=180;//By default, match whole round -180~180
        wireDef.featureSet[0].matching_face=0;//By default, match two sides



        // deffile.featureSet[0].sig_st1_matching_sim_thres=0.2;
        // deffile.briThres=100;



        this.props.ACT_WS_SEND_CORE_BPG( "FI", 0, { _PGID_: stream_PGID_, _PGINFO_: { keep: true }, definfo: wireDef}
        , undefined,{ 
          resolve:insp_resolve, 
          reject:(e)=>{
          } 
        });
        this.props.ACT_StatSettingParam_Update(statSettingOf(
          this.props.machine_custom_setting, this.props.System_Setting, "FI"))
        // The core zeroes its snapshot policy at the start of every CI/FI
        // session, so this push is what the machine actually records -- not a
        // convenience. INSP_SNAP_POLICY carries all three verdicts and both
        // parts; the legacy INSP_NG_SNAP is deliberately NOT sent alongside it,
        // because it would overwrite NG with "both" after the policy set it.
        this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
        { 
          INSP_SNAP_POLICY: snapPolicyOf(this.props.machine_custom_setting),
          INSP_NG_SNAP_MAX_NUM:this.props.machine_custom_setting.FI_INSP_NG_SNAP_MAX_NUM||1000
        });
        this.CameraCtrl.setCameraSpeed_HIGHEST();
      }
      else if (this.props.machine_custom_setting.InspectionMode == "CI") {


        // CI runs at 10fps (was setCameraSpeed_LOW = 2fps, too sluggish). The
        // walk-away/idle case is now handled by the auto-exit guard, not by
        // crawling the framerate.
        this.CameraCtrl.setCameraFrameRate(10);



        // deffile.featureSet[0].single_result_area_ratio=0.9;
        this.props.ACT_WS_SEND_CORE_BPG( "CI", 0, { _PGID_: stream_PGID_, _PGINFO_: { keep: true }, definfo: wireDef
        }, undefined, { 
          resolve:insp_resolve, 
          reject:(e)=>{
          } 
        });


        // this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
        // { CameraSetting: { down_samp_w_calib:false } });

        // this.props.ACT_WS_SEND_CORE_BPG( "CI", 0, { _PGID_: stream_PGID_, _PGINFO_: { keep: true }, definfo: {
        //   type:"gen"
        // }
        // }, undefined);

        this.props.ACT_StatSettingParam_Update(statSettingOf(
          this.props.machine_custom_setting, this.props.System_Setting, "CI"))
      }

      // Re-apply the machine's own camera settings on the way in.
      //
      // This carries no VALUE from the browser -- it asks the core to reload
      // the file it already treats as the authority. That distinction is the
      // whole reason the old push was removed (see below), and it is also why
      // its removal left a hole: the core applied that file at STARTUP only,
      // while DefConf, the backlight calib and MAINUI all open the sensor fully
      // at runtime for their own reasons and never put it back. Coming back
      // here then inspected on the full frame, at a lower rate, silently.
      this.props.ACT_WS_SEND_CORE_BPG("ST", 0, { CameraSettingFile: "data/" });

      // The camera ROI is NOT pushed from here any more.
      //
      // This used to read localStorage LS_INSP_ROI and send it as
      // ST {CameraSetting:{ROI}} on every connect, which meant a per-browser
      // copy decided a MACHINE setting: open the WebUI from a different laptop
      // and the machine's camera crop changed under it, silently.
      //
      // The core already owns this. It persists an ROI change into
      // data/default_camera_setting.json (wiringPanel.cpp, the ST handler) and
      // applies that file at startup via CameraSettingFromFile(camera,"data/")
      // -> CameraSetup, which reads the "ROI" array. So the file is already the
      // authority; this push could only ever disagree with it.

      this.exitGate=false;

      
      this.props.ACT_WS_GET_OBJ(this.props.uInsp_API_ID,(api)=>{
        if(api===undefined)return;
        api.send({type: "enter_inspection"},
        (ret)=>{},(e)=>console.log(e));
      })
    }
  }

  componentWillUnmount() {
    if (this._autoExitTimer !== null) { clearTimeout(this._autoExitTimer); this._autoExitTimer = null; }
    this.props.ACT_WS_GET_OBJ(this.props.uInsp_API_ID,(api)=>{
      if(api===undefined)return;
      api.send({type: "exit_inspection"},
      (ret)=>{},(e)=>console.log(e));
    })

    // Stop the v2 board too.
    //
    // The line above only reaches uInspMEGA: `exit_inspection` is handled in
    // MEGA_W5500_FullInsp.cpp and nowhere else, while the v2 board's command is
    // `exit_insp_mode` on a different API. So on a uInspESP32 machine, leaving
    // this screen left the PLATE RUNNING while the two lines below tore down
    // the inspection -- the CI stream closed and the camera went back to
    // trigger_mode 1. Parts kept being fed and registered with nothing left to
    // judge them: every one goes SKIP/UNSET, CONSEC_UNANSWERED climbs, and with
    // UNANSWERED_POLICY==1 the machine faults OBJECT_HAS_NO_INSP_RESULT after
    // UNANSWERED_STOP_AFTER parts. An operator walking off this screen was
    // enough to do it.
    //
    // runSequence is the SAME stop the sidebar strip and the setup panel use
    // (exit_insp_mode + plate_freq 0), imported rather than re-written -- a
    // third copy of this sequence is exactly what its own comment warns about.
    this.props.ACT_WS_GET_OBJ(this.props.uInspESP32_API_ID,(api)=>{
      if(api===undefined)return;   // not a v2 machine; nothing to stop
      try { runSequence(api, false); }
      catch(e){ log.error("[insp-exit] uInspESP32 stop failed", e); }
    })

    this.props.ACT_WS_SEND_CORE_BPG( "CI", 0, { _PGID_: stream_PGID_, _PGINFO_: { keep: false } });
    // Stop the camera flooding when leaving InspMode.
    this.props.ACT_WS_SEND_CORE_BPG("ST", 0,
      { CameraSetting: { trigger_mode: 1 } });

    // Undo the tag-limit overrides applied on mount: put the ORIGINAL shape
    // objects back for exactly the ids the tag touched, onto whatever list
    // is current (the mount itself churns the list identity, so an identity
    // check can never be the gate). Guard on the def still being the one we
    // overrode -- if a different def was loaded mid-inspection, its shapes
    // are not ours to rewrite.
    if (this._preInspById !== undefined && Object.keys(this._preInspById).length > 0) {
      const curDefName = (this.props.edit_info.loadedDefFile||{}).name;
      if (curDefName === this._preInspDefName) {
        const restored = this.props.shape_list.map(s =>
          this._preInspById[s.id] !== undefined ? this._preInspById[s.id] : s);
        this.props.ACT_Shape_List_Update_EXPRESS(restored);
        log.info("[insp-exit] tag-limit overrides restored on", Object.keys(this._preInspById).length, "shapes");
      } else {
        log.warn("[insp-exit] def changed during inspection (" + this._preInspDefName + " -> " + curDefName + "); NOT restoring tag limits");
      }
      this._preInspById = undefined;
    }
  }

  constructor(props) {
    super(props);
    this.ec_canvas = null;

    // CI auto-exit (power/overheat guard): CI is a STATIONARY inspection -- the
    // user puts objects on the plate and the camera streams + re-inspects the
    // same scene forever. If nobody is there (no object) or the same object just
    // sits stuck, the machine burns power/heat computing the same frame over and
    // over. So: no object for NO_OBJ_MS, OR the same object persisting for
    // SAME_OBJ_MS, flashes a reason then exits inspection mode entirely.
    // Both are time-based (epoch ms), so they're robust to render cadence.
    this.NO_OBJ_MS = 30 * 1000;     // no object on the plate -> idle line
    this.SAME_OBJ_MS = 60 * 1000;   // same object stuck in view -> user walked off
    this._noObjSince = null;        // epoch ms when the no-object streak began
    this._autoExiting = false;      // latch: flashing + leaving
    this._autoExitTimer = null;

    this.state = {
      GraphUIDisplayMode: 0,
      CanvasWindowRatio: 9,
      onROISettingCallBack:undefined,
      // Infinity until a rankN tag says otherwise: with no level chosen,
      // rank hides nothing. 0 hid every measurement above the lowest level
      // before anyone had asked for that.
      measureDisplayRank:Infinity,
      isInSettingUI:false,
      SettingParamInfo:undefined,
      modalInfo:undefined,
      renderObjAlignRotate:false,
      autoExitReason:undefined
    };

    

    new Promise((resolve, reject) => {
      this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
      { 
        InspectionParam:[{
          get_param:true
        }]
      },undefined, { resolve, reject })

    }).then((pkts) => {
      let DT=pkts.find(pkt=>pkt.type=="DT");
      log.debug("[DT]", { DT, pkts });
      if(DT!==undefined && DT.data!==undefined&& DT.data[0]!==undefined)
      {
        this.setState({SettingParamInfo:DT.data[0]});
      }
      else
      {
        this.setState({SettingParamInfo:undefined});
      }
    })


    this.CameraCtrl = new CameraCtrl({
      ws_ch: (STData, promiseCBs) => {
        this.props.ACT_WS_SEND_CORE_BPG( "ST", 0, STData, undefined, promiseCBs)
      },
      ev_frameRateChange: (fps) => {
      }
    });
    // this.IR = undefined;



    // new Promise((resolve, reject) => {
    //   this.props.ACT_WS_SEND_CORE_BPG( "LD", 0,
    //     { filename: "data/default_camera_setting.json" },
    //     undefined, { resolve, reject }
    //   );
    //   setTimeout(() => reject("Timeout"), 2000)
    // }).then((pkts) => {
    //   if (pkts[0].type != "FL") return;
    //   let cam_setting = pkts[0].data;
    //   if (typeof cam_setting.ROIs !== 'object') return;
    //   let ROIs = cam_setting.ROIs;
    //   // console.log(">>>>", ROIs);
    //   let down_samp_factor =cam_setting.down_samp_factor===undefined? 1:cam_setting.down_samp_factor;
    //   this.setState({ ROIs, ROI_key: undefined,down_samp_factor});
    // }).catch((err) => { })





  }

  EXIT()
  {
    if(this.exitGate==false)
    {//prevent double exit
      this.exitGate=true;

      // Stop the core RIGHT NOW, on the click task, BEFORE kicking off the SM
      // transition. The same two commands also run in componentWillUnmount, but
      // that fires only after the SM EXIT event + reducer + unmount of this heavy
      // tree have committed -- and when a turntable (SLID/uInsp) is connected the
      // camera is hardware-triggered and floods RP/IR reports, saturating the JS
      // main thread. In that state the deferred unmount sends queue behind the
      // inbound-report backlog and reach the core 5-10s late. Sending here gets
      // the stop bytes onto the wire in the input task: trigger_mode:1 halts the
      // camera grab (killing the flood at its source) and CI keep:false closes
      // the inspection PG (clearing the loaded def). Idempotent with unmount.
      this.props.ACT_WS_SEND_CORE_BPG("ST", 0,
        { CameraSetting: { trigger_mode: 1 } });
      this.props.ACT_WS_SEND_CORE_BPG("CI", 0,
        { _PGID_: stream_PGID_, _PGINFO_: { keep: false } });

      this.props.ACT_EXIT();
    }
  }

  // CI-only idle watchdog. Called from componentDidUpdate with each fresh
  // inspection report (already gated to CI there). Two exit triggers, both
  // time-based:
  //  - no object on the plate for NO_OBJ_MS, or
  //  - the SAME object (reducer tracking-window identity, matched by orientation/
  //    area/position) still present after SAME_OBJ_MS, i.e. user walked off and
  //    left a part sitting there.
  checkAutoExitForCI(report) {
    if (this._autoExiting) return;
    const now = Date.now();

    // --- no object ---
    const hasObj = report && report.reports && report.reports.length > 0;
    if (!hasObj) {
      if (this._noObjSince == null) this._noObjSince = now;
      else if (now - this._noObjSince > this.NO_OBJ_MS) {
        this.autoExit("no_obj");
        return;
      }
    } else {
      this._noObjSince = null;
    }

    // --- same object stuck too long ---
    // Entries remain in trackingWindow only while still being seen (the reducer
    // ages them out keepInTrackingTime_ms after the last sighting), so a present
    // entry with a far-past add_time_ms means the same object has persisted that
    // long. repeatTime can't be used here -- it caps at maxReportRepeat.
    const tw = this.props.reportStatisticState && this.props.reportStatisticState.trackingWindow;
    if (Array.isArray(tw)) {
      for (let i = 0; i < tw.length; i++) {
        const e = tw[i];
        if (e && typeof e.add_time_ms === 'number' && (now - e.add_time_ms > this.SAME_OBJ_MS)) {
          this.autoExit("same_obj");
          return;
        }
      }
    }
  }

  // Flash the reason for a moment, then leave inspection mode. Halt the camera
  // immediately (trigger_mode:1) so the wasteful compute stops during the flash;
  // EXIT() does the full clean teardown after.
  autoExit(reason) {
    if (this._autoExiting) return;
    if (this.props.machine_custom_setting.InspectionMode !== "CI") return;
    this._autoExiting = true;
    this.props.ACT_WS_SEND_CORE_BPG("ST", 0, { CameraSetting: { trigger_mode: 1 } });
    const msg = (reason === "no_obj")
      ? "長時間無物件，自動退出檢測以節省電力"
      : "物件長時間停滯，自動退出檢測以節省電力";
    this.setState({ autoExitReason: msg });
    this._autoExitTimer = setTimeout(() => { this._autoExitTimer = null; this.EXIT(); }, 2000);
  }

  componentDidUpdate() {
    if (this.props.machine_custom_setting.InspectionMode== "CI")
      this.checkAutoExitForCI(this.props.inspectionReport);

    if(this.props.uInsp_API_ID_CONN_INFO!==undefined)
    {
      if(this.props.uInsp_API_ID_CONN_INFO.type!=="WS_CONNECTED")
      {
        this.EXIT();
      }
    }
    if(this.props.CAM1_ID_CONN_INFO!==undefined)
    {
      if(this.props.CAM1_ID_CONN_INFO.type!=="WS_CONNECTED")
      {
        this.EXIT();
      }
    }
  }
  shouldComponentUpdate(nextProps, nextState)
  {
    let pre_reportCount=nextProps.reportStatisticState.reportCount;
    let isReportInc=pre_reportCount!==this.pre_reportCount
    this.pre_reportCount=pre_reportCount;


    let doUpdate=true;
    {
      // console.log(">>>",props.edit_info);
      let cur__surpress_display=nextProps.edit_info.reportStatisticState.__surpress_display;
      // console.log(cur__surpress_display,this.pre__surpress_display);
      if( ((cur__surpress_display==false) ||(this.pre__surpress_display==false) )&& this.cacheIM!=nextProps.edit_info.img)
      {
        doUpdate&=true;
      }
      else
      {
        doUpdate&=false;//skip this update
      }
  
      this.pre__surpress_display=cur__surpress_display;
      this.cacheIM=nextProps.edit_info.img;

    }

    if(this.state!==nextState){
      return true;
    }
    // console.log("///",isReportInc);
    return isReportInc & doUpdate;
  }
  setInspectionRankUI()
  {
    this.setState({
      additionalUI: [
        <Modal
        title={""}
        visible={true}
        onOk={() => {
          this.setState({ additionalUI: [] });
          this.props.EV_UI_inspMode();
        }}
        onCancel={() => {
          this.setState({ additionalUI: [] });
        }}
      >
        
        
        {/* The 檢測等級 slider stood here. It filtered what was drawn AND, until
            the roll-up was fixed, what the screen's verdict was computed from --
            so an operator could change a part's verdict by moving a viewing
            control the core had never heard of. The level now arrives as a
            rankN tag chosen with every other per-part tag, which reaches the
            wire def and therefore both sides. */}
        <Divider orientation="left" key="div2"></Divider>

        <Button key="opt uInsp" icon={<SettingOutlined/>}
          onClick={() => {
            this.props.ACT_StatInfo_Clear();
          }} >清空統計數據</Button>

        {/* Per-caliper hit payload toggle. Default on; irrelevant for shapes
            whose def has locating != 'caliper' (cal_hits is absent then).
            Gates the CORE's emission -- see the note on CaliperHitsSwitch_rdx. */}
        <CaliperHitsSwitch_rdx key="caliper-hits-toggle" />

        <Divider orientation="left" key="img_tran_weight">圖像檢視側重</Divider>
        <Button key="okf"
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              ImageTransferSetup:{
                OK_MAX_FPS:6,
                NG_MAX_FPS:6,
                NA_MAX_FPS:6,
              }
            })

        }} >平均</Button>
        <Button key="okf"
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              ImageTransferSetup:{
                OK_MAX_FPS:8,
                NG_MAX_FPS:4,
                NA_MAX_FPS:4,
              }
            })

        }} >OK</Button>
        <Button key="ngf"
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              ImageTransferSetup:{
                OK_MAX_FPS:4,
                NG_MAX_FPS:8,
                NA_MAX_FPS:4,
              }
            })
        }} >NG</Button>
        <Button key="naf"
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              ImageTransferSetup:{
                OK_MAX_FPS:4,
                NG_MAX_FPS:4,
                NA_MAX_FPS:8,
              }
            })
        }} >NA</Button>



        <Divider orientation="left" key="img_tran_focus">圖像檢視專注</Divider>
        <Button key="okngo"
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              ImageTransferSetup:{
                OK_MAX_FPS:7,
                NG_MAX_FPS:7,
                NA_MAX_FPS:0.001,
              }
            })

        }} >OK&NG</Button>
        <Button key="oko"
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              ImageTransferSetup:{
                OK_MAX_FPS:8,
                NG_MAX_FPS:0.001,
                NA_MAX_FPS:0.001,
              }
            })

        }} >OK</Button>
        <Button key="ngo"
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              ImageTransferSetup:{
                OK_MAX_FPS:0.001,
                NG_MAX_FPS:8,
                NA_MAX_FPS:0.001,
              }
            })
        }} >NG</Button>
        <Button key="nao"
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              ImageTransferSetup:{
                OK_MAX_FPS:0.001,
                NG_MAX_FPS:0.001,
                NA_MAX_FPS:8,
              }
            })
        }} >NA</Button>
        {/* <br/>
        SAVE:
        <Button key="opt uInsp" icon={<SettingOutlined/>}
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              INSP_NG_SNAP:true
            })

          }} >O</Button>
          
        <Button key="opt uInsp" icon={<SettingOutlined/>}
          onClick={() => {

            this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
            { 
              INSP_NG_SNAP:false
            })

          }} >X</Button> */}
      </Modal>]
    })
  }


  MatchingEnginParamSet(key,value)
  {
    this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
              { 
                InspectionParam:[{
                  [key]:value
                }]
              })
  }

  notifyPopUp(title,msg)
  {
    this.setState({
      modalInfo:{
        title:title,
        onOk:()=>this.setState({modalInfo:undefined}),
        onCancel:()=>this.setState({modalInfo:undefined}),
        footer:null,
        children:msg
      }
    })
  }
  warnPopUp(msg)
  {
    this.notifyPopUp("警告",msg)
  }
  render() {
    let MenuSet = [];
    let menu_height = "HXA";//auto
    log.debug("CanvasComponent render");
    let CanvasWindowRatio = 12;
    let menuOpacity = 1;

    // let MenuSet_2nd = [];



    let onTagEdit = () =>
      this.setState({
        additionalUI: [
          <Modal
            title={"警告"}
            visible={true}
            onOk={() => {
              this.setState({ additionalUI: [] });
              this.props.EV_UI_inspMode();
            }}
            onCancel={() => {
              this.setState({ additionalUI: [] });
            }}
          >
            <div style={{ height: "500px" }}>
              <TagOptions_rdx className="s width12 HXA" />
            </div>
          </Modal>
        ]
      });

    let maxTextLength=20;
    let text_more="...";
    let shortedModelName=this.props.defModelName.length<(maxTextLength+text_more.length)?
      this.props.defModelName:
      this.props.defModelName.substring(0, maxTextLength)+text_more
    //console.log(">>>>defModelName>>>>>"+this.props.defModelName);
    MenuSet = [

    ];

    // MenuSet.push(
      
    // );

    
    MenuSet.push(

    //   <Button type="primary" icon={<SearchOutlined />}>
    //   Search
    // </Button>
      <Button
        icon={<SaveOutlined />}
        key="SVX"
        style={{width:"100%"}}
        type="primary"
        onClick={() =>{



          

          let curList = this.props.reportStatisticState.trackingWindow.filter(rep=>rep.isCurObj==true);

          
          let tag_str = (curList.length==0)?"":curList[0].tag;


          let default_dst_Path=this.props.machine_custom_setting.InspSampleSavePath;
          
          if(default_dst_Path===undefined)
          {
            default_dst_Path="data"
          }
          let targetName=this.props.edit_info.DefFileName+"-"+dateFormat(new Date(), "yyyymmdd-hh-mm-ss_l");
          //the tag might have Chinease char and it breaks the file access function for hide it for now
          // let targetName=this.props.edit_info.DefFileName+"-["+tag_str+"]-"+dateFormat(new Date(), "yyyymmdd-hh-mm-ss_l");
          this.setState({
            modalInfo:{
              title:"快照命名",
              onOk:()=>{

                this.setState({
                  modalInfo:{...this.state.modalInfo,confirmLoading:true}})

                
                let name = this.state.modalInfo.targetName;
                let path_name = default_dst_Path+"/"+name;
                
                this.props.ACT_WS_SEND_CORE_BPG( "SV", 0,
                { filename: path_name,
                  report_extension:"xreps",
                  img_extension:"png",
                  make_dir:true, 
                  type: "__LAST_DATA_VIEW_CACHE_INFO__" },undefined,
                {
                  resolve:(pkts,action_ch)=>{
                    let SS=pkts.find(pkt=>pkt.type=="SS");
                    
                    // console.log(SS)
                    if(SS.data.ACK==false)
                    {
                      this.warnPopUp(`儲存報告  ${ path_name }   失敗`);
                    }
                    else
                    {
                      
                      this.setState({
                        modalInfo:{...this.state.modalInfo,confirmLoading:false,onOk:_=>_,onCancel:_=>_,okText:"存檔成功"}})
                      
                      setTimeout(()=>{//close after 1s
                        this.setState({modalInfo:undefined})
                      },1000);
                    }
                
          
                  },
                  reject:(e)=>{
      
                    this.warnPopUp(`儲存報告  ${ path_name }   失敗`);
                  }
                })

                if(false)//the old way
                this.props.ACT_WS_SEND_CORE_BPG( "SV", 0,
                { filename: path_name+".png",make_dir:true, type: "__LAST_DATA_VIEW_CACHE_IMG__" },undefined,
                {
                  resolve:(pkts,action_ch)=>{

                    
                    let SS=pkts.find(pkt=>pkt.type=="SS");
                    if(SS.data.ACK==true)
                    {
                      let deffile = defFileGeneration(this.props.edit_info);
                      // console.log(curList);
                      let reportSave = {
                        reports:JSON.parse(JSON.stringify(curList,(key, val) => val===undefined? undefined:(val.toFixed ? Number(val.toFixed(6)) : val  ))),
                        defInfo:deffile,
                        camera_param:this.props.edit_info._obj.cameraParam
                      }
                      var enc = new TextEncoder();

                      
                
          
                      this.props.ACT_WS_SEND_CORE_BPG( "SV", 0,
                      { filename: path_name+".xreps" },enc.encode(JSON.stringify(reportSave)),
                      {
                        resolve:(pkts,action_ch)=>{
                          let SS=pkts.find(pkt=>pkt.type=="SS");
                          if(SS.data.ACK==true)
                          {
                            // this.setState({modalInfo:undefined})

                            // this.notifyPopUp(null,`儲存快照  ${ path_name }  成功`);
                            
                            this.setState({
                              modalInfo:{...this.state.modalInfo,confirmLoading:false,onOk:_=>_,onCancel:_=>_,okText:"存檔成功"}})
                            
                            setTimeout(()=>{//close after 1s
                              this.setState({modalInfo:undefined})
                            },1000);

                          }
                          else
                          {
                            this.warnPopUp(`儲存檔案  ${ path_name+".xreps" }   失敗`);
                          }
                          // 
                          
                        },
                        reject:(e)=>{
                          this.warnPopUp(`儲存檔案  ${ path_name+".xreps" }   失敗`);
                          // this.setState({modalInfo:undefined})
                        }
                      }
                      
                      )
                    }
                    else
                    {
                      this.warnPopUp(`儲存圖像  ${ path_name+".png" }   失敗`);
                    }


                  },
                  reject:(e)=>{
      
                    this.warnPopUp(`儲存圖像  ${ path_name+".png" }   失敗`);
                  }
                })


              },
              onCancel:()=>this.setState({modalInfo:undefined}),

              targetName:targetName,
              children:(modalInfo)=><>
              路徑:{default_dst_Path}<br/>
              名稱:
              <Input size="small"
                value={modalInfo.targetName} 
                onChange={(ev)=> this.setState({
                  modalInfo:{...modalInfo,targetName:ev.target.value}})}
              />
              
              </>
            }
          })
          return;
        }} >檢測快照</Button>);
        


    {//if the FLAGS.CI_INSP_SEND_REP_TO_DB_SKIP is undefined it will use the default number
    }
    let InspectionReportPullSkip=uploadSkipOf(
      this.props.machine_custom_setting, this.props.System_Setting);
    // console.log(this.props.inspMode,InspectionReportPullSkip);
    if(!this.state.isInSettingUI)
    {

      let trackingWindowInfo = this.props.reportStatisticState.trackingWindow;
      //console.log(">>>>>>inspection_db_ws_url:",this.props.machine_custom_setting);
      MenuSet.push(
        <ObjInfoList
          IR={trackingWindowInfo}
          DICT={this.props.DICT}
          measureDisplayRank={this.state.measureDisplayRank}
          edit_info={this.props.edit_info}
          IR_decotrator={this.props.info_decorator}
          shape_def={this.props.shape_list}
          key="ObjInfoList"
          uInsp_API_ID_CONN_INFO={this.props.uInsp_API_ID_CONN_INFO}
          SLID_API_ID_CONN_INFO={this.props.SLID_API_ID_CONN_INFO}
          uInspESP32_API_ID_CONN_INFO={this.props.uInspESP32_API_ID_CONN_INFO}
          ACT_WS_GET_OBJ={this.props.ACT_WS_GET_OBJ}
          // promiseCBs was being dropped. Nothing needed it until the station
          // panel, which has to know whether its save actually landed before it
          // clears the "unsaved" state -- otherwise a failed write looks saved.
          WSCMD_CB={(tl, prop, data, uintArr, promiseCBs) => { this.props.ACT_WS_SEND_CORE_BPG( tl, prop, data, uintArr, promiseCBs); }}
          // The station panel lives in this list's sidebar, but the canvas and
          // the machine setting belong to APP_INSP_MODE. Hand them down rather
          // than reaching for props ObjInfoList never had -- which is what made
          // the drag button look armed while the canvas never heard about it.
          ecCanvas={this.ec_canvas}
          machineSetting={this.props.machine_custom_setting}
          ACT_machine_custom_setting_Update={this.props.ACT_machine_custom_setting_Update}
        />);
    }
    else
    {
      if(this.state.SettingParamInfo!==undefined)
      {
        let paramSet = this.state.SettingParamInfo;
        //["HFrom","HTo","VMax","VMin","SMax","SMin","boxFilter1_Size","boxFilter1_thres","boxFilter2_Size","boxFilter2_thres"]
        MenuSet.push(
          Object.keys(paramSet).map(key=>[
          <Divider orientation="left" key={key+"_div"}>{key+":"+paramSet[key]}</Divider>,
          
          <Slider key={key+"_slider"}
            className="layout width12"
            min={0}
            max={255}
            onChange={(value) => {
              if(paramSet[key]===undefined)return;
              this.MatchingEnginParamSet(key,value);

              this.setState({SettingParamInfo:{...paramSet,[key]:value}});
              
            }}
            value={paramSet[key]}
            step={1}
          />
          ])
        
        );
      }
    }
    switch (this.state.GraphUIDisplayMode) {
      case 0:
        CanvasWindowRatio = 12;
        menuOpacity = 1;
        break;

      case 1:
        CanvasWindowRatio = 4;
        menuOpacity = 0.3;
        break;

      case 2:
        CanvasWindowRatio = 0;
        menuOpacity = 0.3;
        break;

      case 3:
        CanvasWindowRatio = 3;
        menuOpacity = 0.3;
        break;
    }
    let headerUI = 
    <>
      
      <Button type="primary" size={"large"} onClick={()=>this.EXIT()}>
        <ArrowLeftOutlined />
      </Button>

      <Popover content={<div>{this.props.defModelName}<br />{this.props.defModelPath} </div>} placement="bottomLeft" trigger="click">
        <span style={{margin:"10px"}} ><FileOutlined /> {shortedModelName}</span>
      </Popover>
      <TagDisplay_rdx size="middle"/>
      
      <Tag className="large" color="gray" onClick={() =>{
            this.setInspectionRankUI()
          }}><SettingOutlined /></Tag>
      {this.state.additionalUI}




      
      {/* <Button type="primary" size={"large"} 
      className={ ((this.state.DB_Conn_state == 1) ? "blackText lgreen" : "DISCONNECT_Blink")}
      icon={this.state.DB_Conn_state == 1 ? <LinkOutlined /> : <DisconnectOutlined />} >
          {(this.state.DB_Conn_state == 1 ? this.props.DICT.connection.server_connected : this.props.DICT.connection.server_disconnected)
          + " " + this.state.inspUploadedCount + ":" + this.props.reportStatisticState.historyReport.length + "/" + InspectionReportPullSkip}
      </Button> */}
      
      <LocateNoteBanner />
      <InspectionReportInsert2DB 
        // newAddedReport={this.props.reportStatisticState.newAddedReport} 
        LANG_DICT={this.props.DICT}
        // DBStatus,
        // DBPushPromise,
        onDBInsertSuccess={(data, info) => {
          // log.info(data, info);
        }}
        onDBInsertFail={(data, info) => {
          log.error(data, info);
        }}
        insert_skip={InspectionReportPullSkip}/>



      <Button size={"large"} type={this.state.renderObjAlignRotate==true?"primary":"dashed"} onClick={()=>this.setState({renderObjAlignRotate:!this.state.renderObjAlignRotate})}>
        <RedoOutlined/>
        {this.state.renderObjAlignRotate==true?"旋轉標的":"不轉原圖"}
      </Button>

      <Button size={"large"} onClick={() => {
        const ts = new Date().toISOString().replace(/[:.]/g, '-').replace('T','_').replace('Z','');
        const filename = `./data/snap_${ts}.png`;
        this.props.ACT_WS_SEND_CORE_BPG("SV", 0,
          { filename, make_dir: true, type: "__LAST_DATA_VIEW_CACHE_IMG__" }, undefined,
          {
            resolve: (pkts) => {
              const SS = pkts.find(p => p.type === "SS");
              if (SS && SS.data.ACK === true) this.notifyPopUp(null, `儲存影像 ${filename}`);
              else this.warnPopUp(`儲存影像失敗 ${filename}`);
            },
            reject: () => this.warnPopUp(`儲存影像失敗 ${filename}`),
          });
      }}>存影像</Button>






{/* 
      <Checkbox  checked={this.CameraCtrl.data.DoImageTransfer}
      onChange={(ev)=>
          {
            this.CameraCtrl.setCameraImageTransfer(ev.target.checked);
            this.setState({});//just to kick update
          }
        } >{
          "相機影像更新"
        }</Checkbox> */}
      
      <Button type="primary" key="Info Graphs" size={"large"} icon={<BarChartOutlined />}
      onClick={() => {
        this.state.GraphUIDisplayMode = (this.state.GraphUIDisplayMode + 1) % 3;
        this.setState(Object.assign({}, this.state));
      }}
      >資料圖表</Button>

      <Button type={"primary"} danger={this.state.onROISettingCallBack!==undefined} key="Manual ZOOM" size={"large"}
        onClick={() => {


        // Open the sensor fully so the whole field is visible to drag on. The
        // core persists whatever ROI it is given; nothing is mirrored locally
        // any more (see the note where the connect-time push used to be).
        let FullSensorROI=[0,0,99999,99999];
        this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
        { CameraSetting: { ROI:FullSensorROI } });

        this.setState({ onROISettingCallBack:(ROI_setting)=>{
          
          let x = ROI_setting.start.pix.x;
          let y = ROI_setting.start.pix.y;
          
          let w = ROI_setting.end.pix.x-x;
          let h = ROI_setting.end.pix.y-y;
          if(w<0)
          {
            x+=w;
            w=-w;
          }
          if(h<0)
          {
            y+=h;
            h=-h;
          }
          
          let ROI = [x,y,w,h];
          if(w<10 || h<10 )
          {
            ROI=FullSensorROI;
          }

          
          // The ONLY write to the machine's stored crop, and it lands under
          // its own key (InspectionROI). The full-sensor open above says
          // nothing: that is the UI looking at the frame, not an operator
          // picking a crop. DefConf and the backlight calib open the sensor
          // fully too, for the same reason -- with a separate key none of them
          // can reach this value even by accident.
          this.props.ACT_WS_SEND_CORE_BPG( "ST", 0,
          {CameraSetting: { ROI, save_insp_roi:true }});


          this.setState({onROISettingCallBack:undefined});
        }})
      }} ><ExpandOutlined />
        {this.state.onROISettingCallBack===undefined?"設定ROI":"選擇ROI中"}</Button>
    </>

/*
    </>;*/


    // console.log(this.props.FILE_default_camera_setting)

    // MenuSet_2nd.push(<AngledCalibrationHelper className="s width12 HXA"
    //   reportStatisticState={this.props.reportStatisticState} shape_list={this.props.shape_list}
    //   camera_calibration_report={this.props.camera_calibration_report} />);

    return (
      <div className="HXF flex_section">
        <div className="flex_header">{headerUI}</div>



        <div className="flex_div">
          {/* <$CSSTG transitionName="fadeIn"> */}
            <div key={"MENU"} className={"s overlay shadow1 scroll MenuAnim " + menu_height}
              style={{ opacity: menuOpacity, width: "250px",maxHeight:"90%" }}>
              {MenuSet}
            </div>
          {/* </$CSSTG> */}

          {(CanvasWindowRatio <= 0) ? null :
            <ComponentBoundary name="InspectionCanvas" fallbackHeight="60vh">
              <CanvasComponent_rdx addClass={"layout WXF " + " height" + CanvasWindowRatio}

                edit_info={this.props.edit_info}
                onROISettingCallBack={this.state.onROISettingCallBack}
                measureDisplayRank={this.state.measureDisplayRank}
          edit_info={this.props.edit_info}
                ACT_WS_SEND_CORE_BPG={this.props.ACT_WS_SEND_CORE_BPG}
                downSampleFactor={1}
                onCanvasInit={(canvas) => {
                  this.ec_canvas = canvas;
                  // ec_canvas is a plain field, so nothing re-renders when it
                  // arrives -- and it arrives AFTER the sidebar has rendered
                  // with ecCanvas=undefined. StationRegionPanel's drag button
                  // would then be permanently dead until some unrelated state
                  // change happened to repaint it. Nudge once.
                  this.setState({ ecCanvasReady: true });
                }}
                renderObjAlignRotate={this.state.renderObjAlignRotate}
                camera_calibration_report={this.props.camera_calibration_report} />
            </ComponentBoundary>}

          {(CanvasWindowRatio >= 12) ? null :
            <DataStatsTable className={"s scroll WXF" + " height" + (12 - CanvasWindowRatio)}
              reportStatisticState={this.props.reportStatisticState} measureDisplayRank={this.state.measureDisplayRank}/>}



        </div>
        <Modal {...this.state.modalInfo} visible={this.state.modalInfo!==undefined}> 
          {this.state.modalInfo===undefined?null:
            ((typeof this.state.modalInfo.children === 'function')?
            this.state.modalInfo.children(this.state.modalInfo):
            this.state.modalInfo.children)
          }
        </Modal>
        
        <>  

          {/* <Menu
            // onClick={this.handleClick}
            // selectedKeys={[this.current]}
            selectable={true}
            // style={{align: 'left', width: 200}}
            defaultSelectedKeys={['functionMenu']}
            // defaultOpenKeys={['functionMenu']}
            mode="inline">

            <SubMenu key="sss"
              className="s overlay overlayright scroll HXA WXA"
              style={{ color: '#333' }}
              title={<SettingOutlined />}>

              <div key={"MENU"}
                style={{ width: "250px" }}>
                {MenuSet_2nd}
              </div>

            </SubMenu>


          </Menu> */}
        </>

        <Modal
          visible={this.state.autoExitReason !== undefined}
          title={dictLookUp("WARNING", this.props.DICT)}
          closable={false}
          maskClosable={false}
          footer={null}>
          {this.state.autoExitReason}
        </Modal>

      </div>
    );
  }
}



const mapDispatchToProps_APP_INSP_MODE = (dispatch, ownProps,ff) => {
  return {
    ACT_EXIT: (arg) => {
      dispatch(UIAct.EV_UI_ACT(UIAct.UI_SM_EVENT.EXIT))
    },
    ACT_WS_SEND_BPG: (id,tl, prop, data, uintArr, promiseCBs) => 
      dispatch(UIAct.EV_WS_SEND_BPG(id, tl, prop, data, uintArr, promiseCBs)),
    ACT_WS_Define_File_Update_EXPRESS:(defFile,keepCurTag) => dispatch({...UIAct.EV_WS_Define_File_Update(defFile,keepCurTag),ActionThrottle_type: "express"}),

    
    ACT_StatSettingParam_Update: (arg) => dispatch(UIAct.EV_StatSettingParam_Update(arg)),
    ACT_StatInfo_Clear:()=>dispatch(UIAct.EV_StatInfo_Clear()),
    ACT_Shape_List_Update_EXPRESS:(newlist,cb)=>dispatch({...DefConfAct.Shape_List_Update(newlist,cb),ActionThrottle_type: "express"}),
    ACT_WS_GET_OBJ: (api_id,callback)=>{
      // Peripheral APIs live in the module registry now (synchronous); the
      // Redux round-trip stays only for non-perif objects (DB_WS, Platform).
      const api = getPerifAPI(api_id);
      if (api !== undefined) { callback(api); return; }
      dispatch(UIAct.EV_WS_GET_OBJ(api_id,callback));
    },
    // Station regions are edited here, on the live image, so the redux copy has
    // to be updated from here too -- otherwise the panel would keep re-adopting
    // the pre-save value the next time machine_custom_setting changed.
    ACT_machine_custom_setting_Update:(setting)=>dispatch(UIAct.EV_machine_custom_setting_Update(setting)),
  }
}

const mapStateToProps_APP_INSP_MODE = (state) => {
  return {
    
    edit_info :state.UIData.edit_info,
    c_state: state.UIData.c_state,
    shape_list: state.UIData.edit_info._obj.shapeList,
    info_decorator: state.UIData.edit_info.__decorator,
    defModelName: state.UIData.edit_info.DefFileName,
    FILE_default_camera_setting:state.UIData.FILE_default_camera_setting,
    
    defModelTag: state.UIData.edit_info.DefFileTag,
    machine_custom_setting: state.UIData.machine_custom_setting,
    machTag: state.UIData.MachTag,
    inspOptionalTag: state.UIData.edit_info.inspOptionalTag,
    defModelPath: state.UIData.edit_info.defModelPath,
    CORE_ID: state.ConnInfo.CORE_ID,
    WS_InspDataBase_W_ID: state.UIData.WS_InspDataBase_W_ID,
    inspectionReport: state.UIData.edit_info.inspReport,
    reportStatisticState: state.UIData.edit_info.reportStatisticState,
    

    uInsp_API_ID:state.ConnInfo.uInsp_API_ID,


    // Needed to reach the v2 board's API on the way out -- see componentWillUnmount.
    uInspESP32_API_ID:state.ConnInfo.uInspESP32_API_ID,

    CAM1_ID_CONN_INFO:state.ConnInfo.CAM1_ID_CONN_INFO,
    
    camera_calibration_report: state.UIData.edit_info.camera_calibration_report,
    // One instrument scale for the whole UI (data/lens_calib.json).
    instrument_mmpp: state.UIData.instrument_mmpp,
    DICT:state.UIData.DICT,
    
    System_Setting:state.UIData.System_Setting,
  }
};


const mergeProps_APP_INSP_MODE = (ownProps, mapProps, dispatchProps) => {
  // console.log(ownProps, mapProps, dispatchProps);
  return ({
    ...ownProps,
    ...mapProps,
    ...dispatchProps,
    ACT_WS_SEND_CORE_BPG: (tl, prop, data, uintArr, promiseCBs) => 
      mapProps.ACT_WS_SEND_BPG(ownProps.CORE_ID, tl, prop, data, uintArr, promiseCBs)
  })
}

const APP_INSP_MODE_rdx = withPerifConns(connect(
  mapStateToProps_APP_INSP_MODE,
  mapDispatchToProps_APP_INSP_MODE,
  mergeProps_APP_INSP_MODE)(APP_INSP_MODE));

export default APP_INSP_MODE_rdx;
