// 孤兒設定檔：找出來，在本機配方裡比對，推上去。
//
// An inspection record carries the def's sha and nothing else about how it was
// judged. If that def was never uploaded the record is an ORPHAN: the numbers
// survive and attach to no specification, and the query screen says 設定檔已不在
// 庫中. HY_DB measured 231 orphan shas over 322,640 records and still climbing,
// with SAMP001 / SLID001 / SLID002 among the machines producing them this month.
//
// InspectionUI now asks before it starts writing, which stops NEW orphans. This
// is the other half: the ones already on record. They are adoptable -- upload
// the def and the existing records attach to it -- and the def is usually still
// in the machine's own recipe folder, because the machine that measured those
// parts is the machine that has the file.
//
// So: ask the server which shas are orphaned, read the local recipe folder, and
// match on the sha the file already carries. NOT recomputed: featureSet_sha1 in
// the file is what the records reference and what the upload registers under.
// Recomputing could only produce a different answer, and a different answer
// adopts nothing.
import React, { useState, useCallback, useRef } from 'react';
import Button from 'antd/lib/button';
import Input from 'antd/lib/input';
import Table from 'antd/lib/table';
import Alert from 'antd/lib/alert';
import Progress from 'antd/lib/progress';
import Tag from 'antd/lib/tag';
import message from 'antd/lib/message';
import { SearchOutlined, CloudUploadOutlined, StopOutlined, FolderOpenOutlined } from '@ant-design/icons';
import * as BASE_COM from 'JSSRCROOT/component/baseComponent.jsx';
import { DEF_EXTENSION, isSyncArtifact } from 'UTIL/BPG_Protocol';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.orphan');
const BPG_FileBrowser = BASE_COM.BPG_FileBrowser;

// Browsing FOR A FOLDER, so the files are noise: the answer is a directory and
// every file listed is a row that cannot be the answer.
//
// Sync artefacts are dropped through isSyncArtifact rather than a rule of our
// own -- Resilio's .sync folders and its partial/conflict files are not places
// to scan, and the project already knows which those are.
const folderOnlyFilter = (fileInfo) =>
  !!fileInfo && fileInfo.type === 'DIR' && !isSyncArtifact(fileInfo);

// The scan reads every def in the folder, which is the expensive part: a recipe
// folder holds hundreds of files and each read is a round trip through the core.
// So it is bounded and interruptible rather than fast.
const FILE_CAP = 2000;

// How far down to look. The core puts NO cap on this -- cJSON_DirFiles just
// recurses while depth > 0 -- so the only reason for a number here is cost:
// every level is more directories to walk before any file is read.
//
// It is a control rather than a constant because "not found" and "not looked
// at" are the same picture on screen, and the difference matters: a def that
// was never scanned reports as a def with no orphan, which is the one wrong
// answer this panel must not give quietly. The scan says how deep it went and
// whether anything was still nested at the bottom.
const DEPTH_DEFAULT = 6;

// The http base, derived from the ws url when the http one was never filled in.
// Same host and port -- HY_DB serves both off 8085.
function httpBase(mus) {
  const direct = mus && mus.cusdisp_db_fetch_url;
  if (direct) return String(direct).replace(/\/+$/, '');
  const ws = mus && mus.inspection_db_ws_url;
  if (!ws) return null;
  return String(ws).replace(/^ws:/, 'http:').replace(/^wss:/, 'https:').replace(/\/+$/, '');
}

export default function OrphanDefFinder({ machineSetting, defFolder, BPG_Channel, DB_SEND }) {
  // data/sync is the Resilio-shared recipe folder -- where the fleet's defs
  // actually live, and where the matches are. The loaded def's own folder wins
  // when there is one, because that is the folder this machine is working in.
  const [folder, setFolder]   = useState(defFolder || 'data/sync');
  const [busy, setBusy]       = useState('');
  const [prog, setProg]       = useState({ done: 0, total: 0 });
  const [err, setErr]         = useState(null);
  const [orphans, setOrphans] = useState(null);
  const [hits, setHits]       = useState([]);
  const [scanned, setScanned] = useState(0);
  const [depth, setDepth]     = useState(DEPTH_DEFAULT);
  const [deeper, setDeeper]   = useState(false);   // something sat at the floor
  const [browsing, setBrowsing] = useState(false);
  const cancelRef = useRef(false);

  const base = httpBase(machineSetting);

  const fetchOrphans = useCallback(async () => {
    if (!base) throw new Error('沒有資料庫位址（設定頁 → 後端位址）');
    // 500 is the endpoint's own cap, measured -- the documentation's 1000 is the
    // cap on /api/deffiles, and asking for it here returns
    // VALIDATION_FAILED "limit must be less than or equal to 500".
    //
    // Paged rather than clamped, because the whole point is to see all of them:
    // there are 231 today and the count is climbing, and a scan that silently
    // stopped at the first 500 would report "no match" for anything past it.
    // The endpoint reads a rollup, not the 13M-row main collection, so this is
    // cheap -- measured at 0.07s per call.
    const PAGE = 500;
    const m = new Map();
    let total = 0;
    for (let page = 1; page <= 20; page++) {
      const r = await fetch(base + '/api/deffiles/orphans?limit=' + PAGE + '&page=' + page);
      if (!r.ok) {
        // Page 1 failing is a real failure; a later page failing after some
        // were collected is reported as what it is, a partial list.
        if (page === 1) throw new Error('孤兒清單讀取失敗 HTTP ' + r.status);
        break;
      }
      const j = await r.json();
      const items = j.items || [];
      if (typeof j.totalOrphanShas === 'number') total = j.totalOrphanShas;
      for (const it of items) m.set(String(it.sha), it);
      if (items.length < PAGE) break;
    }
    return { map: m, total: total || m.size };
  }, [base]);

  // The browser's own shape: { path, files: [ { name, type, struct } ] }, where
  // a DIR carries its children in `struct`. Walked the same way BPG_FileBrowser
  // walks it (baseComponent.fList) rather than guessed at -- the first version
  // guessed, treated the whole thing as a plain nested object, and found zero
  // files in a folder that has plenty.
  // Returns true when a directory was reached that the depth stopped us from
  // opening -- i.e. there is more below than this scan saw.
  const collectDefs = (struct, out) => {
    let truncated = false;
    if (!struct || !Array.isArray(struct.files) || out.length >= FILE_CAP) return false;
    for (const f of struct.files) {
      if (out.length >= FILE_CAP) return truncated;
      if (f.type === 'DIR') {
        // A DIR with no struct is one the core did not descend into, which at
        // this point can only be the depth running out.
        if (!f.struct) { truncated = true; continue; }
        if (collectDefs(f.struct, out)) truncated = true;
        continue;
      }
      if (String(f.name || '').toLowerCase().endsWith('.' + DEF_EXTENSION)) {
        out.push(struct.path + '/' + f.name);
      }
    }
    return truncated;
  };

  const browse = (path, depth) => new Promise((resolve, reject) => {
    BPG_Channel('FB', 0, { path, depth }, undefined, { resolve, reject });
    setTimeout(() => reject(new Error('列目錄逾時')), 15000);
  });
  const loadFile = (filename) => new Promise((resolve, reject) => {
    BPG_Channel('LD', 0, { filename }, undefined, {
      resolve: (pkts) => {
        const fl = (pkts || []).find((p) => p.type === 'FL');
        resolve(fl && fl.data);
      },
      reject,
    });
    setTimeout(() => reject(new Error('讀檔逾時')), 15000);
  });

  const scan = useCallback(async () => {
    cancelRef.current = false;
    setErr(null); setHits([]); setScanned(0);
    try {
      setBusy('orphans');
      const got = await fetchOrphans();
      setOrphans(got);
      if (!got.map.size) { setBusy(''); return; }

      setBusy('listing');
      const tree = await browse(folder, depth);
      // packet[0] is the structure; packet[1] carries the ACK. Reading the ACK
      // packet as the structure is what the first version did.
      const ok = Array.isArray(tree) && tree[1] && tree[1].data && tree[1].data.ACK;
      if (!ok) throw new Error('讀不到這個資料夾:' + folder);
      const files = [];
      const cut = collectDefs(tree[0] && tree[0].data, files);
      setDeeper(cut);
      if (!files.length) {
        setErr('這個資料夾裡沒有 .' + DEF_EXTENSION + ' 檔');
        setBusy(''); return;
      }

      setBusy('reading');
      setProg({ done: 0, total: files.length });
      const found = [];
      for (let i = 0; i < files.length; i++) {
        if (cancelRef.current) break;
        setProg({ done: i + 1, total: files.length });
        let d = null;
        try { d = await loadFile(files[i]); } catch (e) { continue; }
        const sha = d && d.featureSet_sha1;
        if (!sha) continue;
        const hit = got.map.get(String(sha));
        if (!hit) continue;
        found.push({
          key: files[i], sha: String(sha), path: files[i],
          name: d.name || '（未命名）',
          records: hit.records, lastSeen: hit.lastSeen, tags: hit.tags || [],
          state: 'ready', doc: d,
        });
        setHits(found.slice());
      }
      setScanned(files.length);
      setBusy('');
    } catch (e) {
      log.warn('[orphan-scan]', { err: String(e) });
      setErr(String((e && e.message) || e));
      setBusy('');
    }
  }, [folder, fetchOrphans]);   // eslint-disable-line react-hooks/exhaustive-deps

  const upload = useCallback((row) => {
    setHits((hs) => hs.map((h) => (h.key === row.key ? { ...h, state: 'sending' } : h)));
    // Sent AS READ. It already carries the sha the records point at; restamping
    // it here could only produce a different sha, and that adopts nothing.
    DB_SEND(row.doc)
      .then(() => {
        setHits((hs) => hs.map((h) => (h.key === row.key ? { ...h, state: 'done' } : h)));
        message.success((row.records || 0).toLocaleString() + ' 筆檢驗資料接回來了');
      })
      .catch((e) => {
        const why = String((e && e.message) || e);
        setHits((hs) => hs.map((h) => (h.key === row.key ? { ...h, state: 'failed', why } : h)));
      });
  }, [DB_SEND]);

  const uploadAll = useCallback(() => {
    hits.filter((h) => h.state === 'ready').forEach(upload);
  }, [hits, upload]);

  const cols = [
    { title: '配方', dataIndex: 'name', ellipsis: true,
      render: (v, r) => (<div>
        <div>{v}</div>
        <div style={{ fontSize: 11, color: '#888' }}>{r.path}</div>
      </div>) },
    // Sorted by this: the biggest hole is the one worth closing first, and it is
    // also the one an operator recognises.
    { title: '筆數', dataIndex: 'records', width: 88, align: 'right',
      defaultSortOrder: 'descend',
      sorter: (a, b) => (a.records || 0) - (b.records || 0),
      render: (v) => (v || 0).toLocaleString() },
    { title: '末次', dataIndex: 'lastSeen', width: 104,
      render: (v) => (v ? new Date(v).toISOString().slice(0, 10) : '—') },
    { title: '站別', dataIndex: 'tags', ellipsis: true,
      render: (t) => (t || []).map((x, i) => <Tag key={i} style={{ fontSize: 11 }}>{x}</Tag>) },
    { title: '', width: 92, dataIndex: 'state',
      render: (st, r) => {
        if (st === 'done')    return <Tag color="green">已上傳</Tag>;
        if (st === 'sending') return <Tag>上傳中…</Tag>;
        if (st === 'failed')  return <Button size="small" danger title={r.why}
                                        onClick={() => upload(r)}>重試</Button>;
        return <Button size="small" type="primary" icon={<CloudUploadOutlined />}
                 onClick={() => upload(r)}>上傳</Button>;
      } },
  ];

  const readyCount = hits.filter((h) => h.state === 'ready').length;

  return (
    <div style={{ marginTop: 14 }}>
      <div style={{ fontWeight: 600, marginBottom: 6 }}>孤兒設定檔</div>
      <div style={{ fontSize: 12, color: '#888', lineHeight: 1.7, marginBottom: 8 }}>
        資料庫裡有檢驗資料指向一份不存在的設定檔時，那些資料查不到判定依據。
        設定檔通常還在這台機器的配方資料夾裡 —— 量那些零件的就是這台。
        補上去，先前的資料就接回來了。
      </div>

      <div style={{ display: 'flex', gap: 8, alignItems: 'center', flexWrap: 'wrap' }}>
        <Input size="small" style={{ flex: '1 1 220px', minWidth: 160 }}
          value={folder} onChange={(e) => setFolder(e.target.value)}
          placeholder="配方資料夾" disabled={!!busy} />
        <span style={{ fontSize: 12, color: '#888' }}>深度</span>
        <Input size="small" style={{ width: 58 }} type="number" min={1} max={20}
          value={depth} disabled={!!busy}
          onChange={(e) => {
            const v = parseInt(e.target.value, 10);
            setDepth(Number.isFinite(v) ? Math.max(1, Math.min(20, v)) : DEPTH_DEFAULT);
          }} />
        <Button size="small" icon={<FolderOpenOutlined />} disabled={!!busy}
          onClick={() => setBrowsing(true)} title="瀏覽資料夾">瀏覽</Button>
        <Button size="small" type="primary" icon={<SearchOutlined />}
          loading={!!busy} disabled={!!busy} onClick={scan}>掃描</Button>
        {busy === 'reading' ? (
          <Button size="small" danger icon={<StopOutlined />}
            onClick={() => { cancelRef.current = true; }}>停止</Button>
        ) : null}
      </div>

      {busy === 'reading' ? (
        <div style={{ marginTop: 8 }}>
          <Progress size="small" status="active"
            percent={prog.total ? Math.round(prog.done * 100 / prog.total) : 0}
            format={() => prog.done + '/' + prog.total} />
        </div>
      ) : null}

      {/* The same browser the def picker uses. Opened for a FOLDER, so it is
          onOk -- the OK button reports where you are standing -- rather than
          onFileSelected, which reports a file. searchDepth 1 because this is
          navigation: the deep walk is the scan's job and doing it here would
          make every click wait for it. */}
      {browsing ? (
        <BPG_FileBrowser key="orphan-browse" className="width8 modal-sizing"
          searchDepth={1} path={folder || 'data/'} visible={true}
          BPG_Channel={BPG_Channel}
          onOk={(folderPath) => { if (folderPath) setFolder(folderPath); setBrowsing(false); }}
          onCancel={() => setBrowsing(false)}
          fileFilter={folderOnlyFilter}
          onFileSelected={(filePath) => {
            // Cannot happen while the filter hides files, but the browser owns
            // that decision and this costs nothing: a file means its folder.
            const cut = String(filePath).lastIndexOf('/');
            if (cut > 0) setFolder(String(filePath).slice(0, cut));
            setBrowsing(false);
          }} />
      ) : null}

      {err ? <Alert type="error" showIcon style={{ marginTop: 8 }} message={err} /> : null}

      {orphans && !busy ? (
        <div style={{ marginTop: 8, fontSize: 12 }}>
          資料庫有 <b>{orphans.total}</b> 個孤兒設定檔
          {scanned ? <> · 本機掃過 <b>{scanned}</b> 個配方（深度 {depth}）</> : null}
          {' · '}對上 <b style={{ color: hits.length ? '#a8071a' : undefined }}>{hits.length}</b> 個
          {hits.length && readyCount ? (
            <Button size="small" type="primary" style={{ marginLeft: 10 }}
              icon={<CloudUploadOutlined />} onClick={uploadAll}>
              全部上傳（{readyCount}）
            </Button>
          ) : null}
        </div>
      ) : null}

      {/* Said out loud, because otherwise a folder that was never opened and a
          folder with nothing in it produce the same screen -- and only one of
          them means "no orphans here". */}
      {deeper && !busy ? (
        <Alert type="warning" showIcon style={{ marginTop: 8 }}
          message={'深度 ' + depth + ' 沒有走到底'}
          description="還有更深的資料夾沒有打開，裡面的配方等於沒有掃過。把深度調大再掃一次。" />
      ) : null}

      {hits.length ? (
        <Table style={{ marginTop: 8 }} size="small" rowKey="key"
          pagination={hits.length > 8 ? { pageSize: 8, size: 'small' } : false}
          columns={cols} dataSource={hits} />
      ) : null}

      {orphans && !busy && !hits.length && scanned ? (
        <Alert type="info" showIcon style={{ marginTop: 8 }}
          message="這個資料夾裡沒有對得上的設定檔"
          description="孤兒可能是別台機器產生的，或那份配方已經不在本機。換一個資料夾再掃，或到產生它的那台機器上做。" />
      ) : null}
    </div>
  );
}
