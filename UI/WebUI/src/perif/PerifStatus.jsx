// The one place a peripheral link's health is drawn. Fed by usePerifLink()
// (src/perif/PerifAPI.js) -- no Redux. Two renderings:
//
//   <PerifStatusChip id={...} label={...}/>   menu-row dot + label
//   <PerifStatusPanel id={...}/>              the counters, for setup panels
//
// The panel exists because the core has counted every way a verdict can
// silently stop reaching the machine (perif_pairing.link: tx_fail /
// dropped_no_channel / suspect) since 0338671f, and no UI ever showed them.
// The reading rule is printed with the numbers because it IS the feature:
// any of these moving while parts run means the machine is not sorting.

import React from 'react';
import { useSelector } from 'react-redux';
import { usePerifLink, usePerifConn } from './PerifAPI';

// HOC for CLASS components (they cannot hook): injects the three legacy
// conn-info props APP_INSP_MODE reads, sourced from the module store instead
// of Redux. Delete when InspectionUI's inner class goes functional.
export function withPerifConns(Comp) {
  return function WithPerifConns(props) {
    const uInspId = useSelector((s) => s.ConnInfo.uInsp_API_ID);
    const esp32Id = useSelector((s) => s.ConnInfo.uInspESP32_API_ID);
    const slidId = useSelector((s) => s.ConnInfo.SLID_API_ID);
    const uInsp = usePerifConn(uInspId);
    const esp32 = usePerifConn(esp32Id);
    const slid = usePerifConn(slidId);
    return (
      <Comp
        {...props}
        uInsp_API_ID_CONN_INFO={uInsp}
        uInspESP32_API_ID_CONN_INFO={esp32}
        SLID_API_ID_CONN_INFO={slid}
      />
    );
  };
}

const STATE_COLOR = {
  CONNECTED: '#52c41a',      // green
  SUSPECT: '#fa8c16',        // orange -- link up, verdicts in doubt
  CONNECTING: '#1890ff',     // blue
  DISCONNECTED: '#bfbfbf',   // grey
};

export function PerifStatusChip({ id, label }) {
  const link = usePerifLink(id);
  const state = (link && link.state) || 'DISCONNECTED';
  const color = STATE_COLOR[state] || STATE_COLOR.DISCONNECTED;
  return (
    <span style={{ display: 'inline-flex', alignItems: 'center', gap: 6 }}>
      <span style={{
        width: 10, height: 10, borderRadius: 5, background: color,
        display: 'inline-block', flex: 'none',
      }} />
      <span>{label}</span>
      {state === 'SUSPECT' && (
        <span style={{ color: STATE_COLOR.SUSPECT, fontWeight: 'bold' }}>連線異常</span>
      )}
    </span>
  );
}

export function PerifStatusPanel({ id }) {
  const link = usePerifLink(id);
  if (!link) return null;
  const lk = link.linkHealth || {};
  const bad = lk.suspect || (lk.dropped_no_channel > 0) || (lk.tx_fail_consec >= 3);
  return (
    <div style={{ fontSize: 12, lineHeight: 1.7 }}>
      <div>
        鏈路狀態：<b style={{ color: STATE_COLOR[link.state] || '#000' }}>{link.state}</b>
      </div>
      {lk.connected !== undefined && (
        <div style={{ fontFamily: 'monospace' }}>
          tx_fail {lk.tx_fail || 0}（連續 {lk.tx_fail_consec || 0}）
          ／ 無通道丟棄 {lk.dropped_no_channel || 0}
          ／ 佇列丟棄 {lk.queue_dropped || 0}
        </div>
      )}
      {bad && (
        <div style={{
          color: '#fff', background: '#cf1322', padding: '2px 8px',
          borderRadius: 4, display: 'inline-block', fontWeight: 'bold',
        }}>
          判定結果未送達分選機 — 機器可能未在分選
        </div>
      )}
    </div>
  );
}
