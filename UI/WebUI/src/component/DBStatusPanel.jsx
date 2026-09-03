// What the 設定DB / 檢測DB icons open.
//
// Both were a status colour and a "12>3405" string in the sidebar, which is the
// in-memory queue and the sent total with no label and no way to ask what they
// mean. Everything below already existed as a number inside DB_WS or
// inspDBQueue; none of it had a way out.
//
// The number that matters most is 已丟棄. The durable mirror drops the OLDEST
// record when it hits its cap, so a long enough outage destroys evidence rather
// than delaying it -- and an inspection report does not carry the settings it
// was judged against, so a hole here is a hole in what can be reconstructed
// later. It is rendered in red the moment it is non-zero, and says so.
import React, { useState, useEffect } from 'react';
import Button from 'antd/lib/button';
import Alert from 'antd/lib/alert';
import Tag from 'antd/lib/tag';
import Tooltip from 'antd/lib/tooltip';
import Modal from 'antd/lib/modal';
import { ReloadOutlined, ThunderboltOutlined, DeleteOutlined } from '@ant-design/icons';
import { pendingInsertCount, droppedCount, purgedCount } from 'UTIL/inspDBQueue';
import { GetObjElement } from 'UTIL/MISC_Util';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.dbpanel');

// The in-memory queue's own cap, from DB_WS's ConsumeQueue. Shown so a queue
// sitting at its limit reads as "full and refusing" rather than as a big number.
const CQ_CAP = 200;

const Row = ({ label, value, hint, danger }) => (
  <div style={{ display: 'flex', alignItems: 'baseline', gap: 10, padding: '3px 0' }}>
    <span style={{ minWidth: 150, color: '#888', fontSize: 13 }}>{label}</span>
    <span style={{ fontSize: 18, fontVariantNumeric: 'tabular-nums',
                   color: danger ? '#cf1322' : undefined,
                   fontWeight: danger ? 600 : 500 }}>{value}</span>
    {hint ? <span style={{ fontSize: 12, color: '#999' }}>{hint}</span> : null}
  </div>
);

export function DBStatusPanel({ id, connInfo, getObj, title }) {
  const [live, setLive] = useState(undefined);   // from the DB_WS instance
  const [pend, setPend] = useState(undefined);   // durable, from IndexedDB
  const [dropped, setDropped] = useState(0);
  const [purged, setPurged] = useState(0);
  const [kicked, setKicked] = useState('');
  const [purging, setPurging] = useState(false);

  // One sampler for both sources. DB_WS pushes its brief_info to Redux only
  // every 5 s, which is fine for a sidebar badge and too slow for a panel
  // someone opened BECAUSE the queue is moving.
  const sample = () => {
    try {
      getObj((obj) => {
        if (!obj) { setLive(null); return; }
        setLive({
          queued: typeof obj.getDataQueueCount === 'function' ? obj.getDataQueueCount() : undefined,
          sent: typeof obj.getDataSentCount === 'function' ? obj.getDataSentCount() : undefined,
          wsState: obj.curr_ws_state,
          url: (obj.websocket && typeof obj.websocket.getURL === 'function')
            ? obj.websocket.getURL() : undefined,
        });
      });
    } catch (e) { log.warn('[dbpanel] getObj failed', e); setLive(null); }

    pendingInsertCount(id).then(setPend).catch((e) => {
      log.warn('[dbpanel] pendingInsertCount failed', e); setPend(null); });
    try { setDropped(droppedCount()); setPurged(purgedCount()); } catch (e) { /* counters are best-effort */ }
  };

  useEffect(() => {
    sample();
    const h = setInterval(sample, 1000);
    return () => clearInterval(h);
  }, [id]);

  const connected = live && live.wsState === 'CONNECTED';
  const errInfo = GetObjElement(connInfo, ['data', 'errorInfo']);
  const url = (live && live.url) || GetObjElement(connInfo, ['data', 'URL']);

  // Retry now, instead of waiting for the queue's own next attempt.
  const kick = () => getObj((obj) => {
    if (!obj) return;
    try {
      if (obj.cQ && typeof obj.cQ.kick === 'function') obj.cQ.kick();
      // Records stranded in IndexedDB after an outage longer than the in-memory
      // queue are only replayed on reconnect; this reaches them without one.
      if (typeof obj._replayPending === 'function') obj._replayPending();
      setKicked('已要求重送');
      setTimeout(() => setKicked(''), 3000);
    } catch (e) { log.warn('[dbpanel] kick failed', e); setKicked('重送失敗:' + String(e)); }
  });

  // DELETE THE BACKLOG. Asks first, with the numbers in the question.
  //
  // The confirmation is not ceremony. Every other button on this panel makes the
  // queue smaller by SENDING it; this one is the only way an operator can make a
  // record cease to exist on purpose, and the panel's whole argument is that a
  // hole in the traceability record is a hole nobody can reconstruct later. So
  // the modal states the count it is about to destroy, says what cannot be
  // undone, and waits -- onOk returns the promise so it stays open until the
  // deletion actually finished rather than until the click was registered.
  const purge = () => getObj((obj) => {
    if (!obj || typeof obj.purgePending !== 'function') return;
    const total = (pend || 0);
    const inFlight = (live && live.queued) || 0;
    Modal.confirm({
      title: '刪除未上傳的資料',
      okText: '刪除',
      okButtonProps: { danger: true },
      cancelText: '取消',
      content: <div style={{ lineHeight: 1.9 }}>
        <div>本地還沒送出去的 <b>{total}</b> 筆會被刪掉{inFlight > 0
          ? <>(其中 {inFlight} 筆在記憶體佇列裡)</> : null}。</div>
        <div style={{ color: '#cf1322' }}>刪掉就沒有了,不能復原,之後查這段時間的資料會少掉這些筆。</div>
        <div style={{ fontSize: 12, color: '#888' }}>
          已經送出去的不受影響。正在線上傳送中的那一筆有可能仍然會被對方寫入。
        </div>
      </div>,
      onOk: () => {
        setPurging(true);
        return Promise.resolve(obj.purgePending())
          .then((r) => { setKicked(`已刪除 ${r ? r.fromDisk : 0} 筆`);
                         setTimeout(() => setKicked(''), 5000); })
          .catch((e) => { log.warn('[dbpanel] purge failed', e);
                          setKicked('刪除失敗:' + String(e)); })
          .finally(() => { setPurging(false); sample(); });
      },
    });
  });

  const reconnect = () => getObj((obj) => {
    if (!obj) return;
    try { if (obj.websocket && typeof obj.websocket.RESET === 'function') obj.websocket.RESET(url); }
    catch (e) { log.warn('[dbpanel] reconnect failed', e); }
  });

  return <div>
    <div style={{ marginBottom: 10 }}>
      <Tag color={connected ? 'green' : 'red'}>
        {live === null ? '找不到連線物件' : connected ? '已連線' : (live && live.wsState) || '未連線'}
      </Tag>
      {url ? <span style={{ fontSize: 12, color: '#888', wordBreak: 'break-all' }}>{url}</span> : null}
    </div>

    {errInfo ? (
      <Alert type="error" showIcon style={{ marginBottom: 10 }}
        message="連線錯誤" description={<pre style={{ margin: 0, fontSize: 11,
          maxHeight: 120, overflow: 'auto' }}>{JSON.stringify(errInfo, null, 2)}</pre>} />
    ) : null}

    {dropped > 0 ? (
      <Alert type="error" showIcon style={{ marginBottom: 10 }}
        message={`已經有 ${dropped} 筆被刪掉了`}
        description={'本地暫存滿了之後會刪掉最舊的一筆騰位置。這些是「已經沒有了」,不是「還在排隊」——'
                   + '之後查那段時間的資料會少掉這些筆。'} />
    ) : null}

    <Row label="待送(記憶體佇列)" value={live && live.queued !== undefined ? live.queued : '—'}
      hint={`上限 ${CQ_CAP} 筆,滿了就改由下面的本地暫存承接`}
      danger={!!(live && live.queued >= CQ_CAP)} />

    <Row label="待送(本地暫存)" value={pend === null ? '讀不到' : pend === undefined ? '…' : pend}
      hint="斷線或重新整理都不會消失,連線恢復後自動重送"
      danger={!!(pend && pend > 0 && !connected)} />

    <Row label="已送出並確認" value={live && live.sent !== undefined ? live.sent : '—'}
      hint="本次開啟 app 以來" />

    <Row label="已丟棄" value={dropped} danger={dropped > 0}
      hint={dropped > 0 ? '暫存滿了自動刪掉最舊的,不是延遲' : '暫存尚未滿過'} />

    {purged > 0 ? (
      <Row label="已手動刪除" value={purged} danger
        hint="本次開啟 app 以來,由操作人員刪除的未上傳資料" />
    ) : null}

    <div style={{ display: 'flex', gap: 8, marginTop: 14, alignItems: 'center' }}>
      <Tooltip title="立刻把待送的資料再送一次,不等佇列自己的下一次嘗試">
        <Button icon={<ThunderboltOutlined />} onClick={kick}>立即重送</Button>
      </Tooltip>
      <Button icon={<ReloadOutlined />} onClick={reconnect}>重新連線</Button>
      <Tooltip title="把還沒送出去的資料刪掉。刪掉就沒有了,不能復原">
        <Button danger icon={<DeleteOutlined />} loading={purging}
          disabled={!((pend && pend > 0) || (live && live.queued > 0))}
          onClick={purge}>刪除未上傳</Button>
      </Tooltip>
      {kicked ? <span style={{ color: '#389e0d' }}>{kicked}</span> : null}
    </div>

    <div style={{ fontSize: 12, color: '#888', marginTop: 12, lineHeight: 1.8 }}>
      {id && String(id).indexOf('DefFile') >= 0
        ? '這裡送的是量測設定檔。檢驗報告不包含當時的設定,要靠這份設定檔才能還原一筆報告是依據什麼判定的。'
        : '這裡送的是每一筆檢驗結果。'}
    </div>
  </div>;
}

export default DBStatusPanel;
