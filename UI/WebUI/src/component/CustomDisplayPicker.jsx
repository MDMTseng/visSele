// The 機台設定 dialog: pick a recipe from the shared catalogue, or manage it.
//
// Replaces CustomDisplaySelectUI + CustomDisplayUI + SingleDisplayEditUI in
// rdxComponent.jsx. The data contract is unchanged -- records are
// { _id, name, cat, targetDeffiles: [{ path, name, tags, featureSet_sha1 }] }
// read from and written to the customDisplay collection through CusDisp_DB.
//
// What the old one got wrong, and what this does instead:
//
//   * Managing the catalogue was a TAB called __SET__ sitting in the same row
//     as the product categories, so the door to editing looked like one more
//     place to look for a recipe. Here picking is the whole dialog and managing
//     is behind a button that says so.
//   * A card said only the name. What it actually does -- load THAT def file,
//     apply THOSE tags -- was invisible until after it happened. Now the card
//     shows the def and the tags, and marks the one already loaded.
//   * Choosing swapped the recipe on the machine on a single click. It asks.
//   * "Add field" wrote a record named 新設定 into the SHARED database the
//     instant it was pressed. New records are local drafts until saved.
//   * Every read/write failure was swallowed by an empty catch, so a server
//     that was down looked exactly like a catalogue with nothing in it.
//     Loading, empty and failed are three different screens now.
import React, { useState, useEffect, useMemo } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import Button from 'antd/lib/button';
import Input from 'antd/lib/input';
import Tag from 'antd/lib/tag';
import Empty from 'antd/lib/empty';
import Spin from 'antd/lib/spin';
import Alert from 'antd/lib/alert';
import Modal from 'antd/lib/modal';
import Divider from 'antd/lib/divider';
import Tooltip from 'antd/lib/tooltip';
import { SettingOutlined, ArrowLeftOutlined, PlusOutlined, DeleteOutlined,
         FolderOpenOutlined, ReloadOutlined, CheckCircleFilled } from '@ant-design/icons';
import { CusDisp_DB } from 'UTIL/DB_Query';
import { defFileFilter } from 'UTIL/BPG_Protocol';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as BASE_COM from 'JSSRCROOT/component/baseComponent.jsx';
import dclone from 'clone';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.cusdisp');

const BPG_FileBrowser = BASE_COM.BPG_FileBrowser;
const NO_CAT = '未分類';

const errText = (e) => (e && e.message) ? e.message
  : (typeof e === 'string' && e.length) ? e : '沒有回應';

// Records carry at most one def in practice, but the field is an array and the
// old editor looped over it. Read defensively; never index blindly.
const defOf = (info) => (info && Array.isArray(info.targetDeffiles) && info.targetDeffiles[0])
  ? info.targetDeffiles[0] : {};
const tagsOf = (info) => String(defOf(info).tags || '')
  .split(',').map((t) => t.trim()).filter(Boolean);
const catOf = (info) => {
  const c = (info && typeof info.cat === 'string') ? info.cat.trim() : '';
  return c.length ? c : NO_CAT;
};

// Compare the catalogue's stored path against the loaded def path.
//
// The stored path is relative to the data folder and carries no extension;
// defModelPath is the loaded model path. Normalise both ends rather than
// trusting either: a marker that says "this is the one you have open" is worse
// than no marker when it is wrong.
const samePath = (a, b) => {
  const norm = (p) => String(p || '').replace(/\\/g, '/').replace(/\.[^./]*$/, '')
    .replace(/^\.?\//, '').toLowerCase();
  const A = norm(a), B = norm(b);
  return A.length > 0 && A === B;
};

export function CustomDisplayPicker({ onSelect }) {
  const dispatch = useDispatch();
  const CORE_ID = useSelector((s) => s.ConnInfo.CORE_ID);
  const _mus = useSelector((s) => s.UIData.machine_custom_setting);
  const defModelPath = useSelector((s) => s.UIData.edit_info.defModelPath);
  const BPG_Channel = (tl, prop, data, uintArr, promiseCBs) =>
    dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, tl, prop, data, uintArr, promiseCBs));

  const url = _mus && _mus.cusdisp_db_fetch_url;

  const [rows, setRows] = useState(undefined);   // undefined = still loading
  const [err, setErr] = useState(undefined);
  const [q, setQ] = useState('');
  const [cat, setCat] = useState(undefined);     // undefined = all
  const [managing, setManaging] = useState(false);
  const [editing, setEditing] = useState(undefined);  // a draft record
  const [busy, setBusy] = useState('');

  const load = () => {
    setErr(undefined); setRows(undefined);
    if (!url) { setErr('尚未設定 cusdisp_db_fetch_url（設定頁 → 後端位址）'); setRows([]); return; }
    CusDisp_DB.read(url, '.')
      .then((data) => setRows(Array.isArray(data && data.prod) ? data.prod : []))
      .catch((e) => { log.warn('[cusdisp] read failed', e); setErr(errText(e)); setRows([]); });
  };
  useEffect(load, [url]);

  // Exact match, not substring.
  //
  // The old grouping tested cat.includes(record.cat), so a record in category
  // "A" also appeared under "AB" -- the same recipe listed twice, in a place it
  // did not belong.
  const cats = useMemo(() => {
    const seen = new Set((rows || []).map(catOf));
    return [...seen].sort((a, b) => (a === NO_CAT) - (b === NO_CAT) || a.localeCompare(b));
  }, [rows]);

  const shown = useMemo(() => {
    const needle = q.trim().toLowerCase();
    return (rows || [])
      .filter((r) => cat === undefined || catOf(r) === cat)
      .filter((r) => !needle || [r.name, r.cat, defOf(r).path, defOf(r).name, defOf(r).tags]
        .some((f) => String(f || '').toLowerCase().includes(needle)))
      .sort((a, b) => String(a.name || '').localeCompare(String(b.name || '')));
  }, [rows, cat, q]);

  const choose = (info) => {
    const d = defOf(info);
    if (!d.path) {
      Modal.error({ title: '這筆設定沒有指定配方檔',
        content: `「${info.name}」的 targetDeffiles 是空的,選了不會有任何動作。請先在管理裡指定配方檔。` });
      return;
    }
    // One click, no confirmation. This is a shop-floor control that gets used
    // constantly, and a confirm step doubles every recipe change to save an
    // operator from a button they meant to press. What a button loads is shown
    // on hover instead, which costs nothing when you already know.
    //
    // The empty-path case above still stops, because that one silently does
    // NOTHING -- an action that appears to work and does not is the failure a
    // dialog is actually worth spending.
    onSelect(info);
  };

  // ---- management ------------------------------------------------------
  //
  // A new record is a plain object with no _id. It exists only here until 建立,
  // so backing out of a mistake costs nothing and the shared catalogue never
  // grows a row called 新設定 that someone else has to clean up.
  const blank = () => ({ name: '', cat: '', targetDeffiles: [{}] });

  const save = (rec) => {
    if (!String(rec.name || '').trim()) {
      Modal.warning({ title: '需要名稱', content: '這是操作員在清單上唯一看得到的字。' });
      return;
    }
    setBusy('save');
    // create() with an id upserts; without one it inserts. Same call either way.
    CusDisp_DB.create(url, rec, rec._id)
      .then(() => { setEditing(undefined); load(); })
      .catch((e) => { log.warn('[cusdisp] save failed', e);
        Modal.error({ title: '儲存失敗', content: `${errText(e)}。清單沒有變更。` }); })
      .finally(() => setBusy(''));
  };

  const remove = (rec) => Modal.confirm({
    title: `刪除「${rec.name}」？`,
    // The catalogue is shared, which the old confirm did not mention -- it read
    // like a local delete.
    content: '這份清單是資料庫上的共用清單,刪除之後其他機台也會看不到。',
    okText: '刪除', okButtonProps: { danger: true }, cancelText: '取消',
    onOk: () => {
      setBusy('del');
      return CusDisp_DB.delete(url, rec._id)
        .then(() => { setEditing(undefined); load(); })
        .catch((e) => { log.warn('[cusdisp] delete failed', e);
          Modal.error({ title: '刪除失敗', content: `${errText(e)}。這筆設定仍然存在。` }); })
        .finally(() => setBusy(''));
    },
  });

  // ---- screens ---------------------------------------------------------
  if (rows === undefined) {
    return <div style={{ padding: 48, textAlign: 'center' }}>
      <Spin /><div style={{ marginTop: 12, color: '#888' }}>正在讀取共用設定清單…</div>
    </div>;
  }

  if (editing !== undefined) {
    return <EditRecord rec={editing} setRec={setEditing} busy={busy}
      BPG_Channel={BPG_Channel}
      onSave={() => save(editing)}
      onCancel={() => setEditing(undefined)}
      onDelete={editing._id ? () => remove(editing) : undefined} />;
  }

  return <div style={{ maxWidth: '100%' }}>
    <div style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 12 }}>
      <Input.Search allowClear placeholder="搜尋名稱、分類、配方檔、標籤"
        value={q} onChange={(e) => setQ(e.target.value)} style={{ flex: 1 }} />
      <Tooltip title="重新讀取"><Button icon={<ReloadOutlined />} onClick={load} /></Tooltip>
      <Button type={managing ? 'primary' : 'default'}
        icon={managing ? <ArrowLeftOutlined /> : <SettingOutlined />}
        onClick={() => setManaging(!managing)}>
        {managing ? '回到選擇' : '管理清單'}
      </Button>
    </div>

    {err !== undefined
      ? <Alert type="error" showIcon style={{ marginBottom: 12 }}
          message="讀不到共用設定清單" description={`${err}。畫面上的空白不代表清單是空的。`}
          action={<Button size="small" onClick={load}>重試</Button>} />
      : null}

    {cats.length > 1 ? (
      <div style={{ marginBottom: 12 }}>
        <Tag.CheckableTag checked={cat === undefined} onChange={() => setCat(undefined)}>
          全部 {rows.length}
        </Tag.CheckableTag>
        {cats.map((c) => (
          <Tag.CheckableTag key={c} checked={cat === c}
            onChange={() => setCat(cat === c ? undefined : c)}>
            {c} {rows.filter((r) => catOf(r) === c).length}
          </Tag.CheckableTag>
        ))}
      </div>
    ) : null}

    {managing ? (
      <Button type="dashed" icon={<PlusOutlined />} block style={{ marginBottom: 12 }}
        onClick={() => setEditing(blank())}>新增一筆</Button>
    ) : null}

    {shown.length === 0 ? (
      <Empty description={err !== undefined ? '讀取失敗,沒有可顯示的內容'
        : rows.length === 0 ? '共用清單裡還沒有任何設定'
        : '沒有符合搜尋條件的設定'} />
    ) : (
      // A wrapped row of buttons, the shape this had before and the shape the
      // floor uses. The card grid that replaced it briefly put one recipe per
      // 240px tile and pushed the list off-screen: on a machine where this is
      // opened many times a shift, having to scroll to reach a name that used
      // to be visible is a straight loss.
      //
      // Everything the cards were carrying -- def path, tags, category --
      // moves into the hover, where it costs no space and is still there when
      // someone needs to check which of two similar names is which.
      <div style={{ display: 'flex', flexWrap: 'wrap', gap: 8 }}>
        {shown.map((info) => {
          const d = defOf(info);
          const isCur = samePath(d.path, defModelPath);
          return (
            <Tooltip key={info._id || info.name} mouseEnterDelay={0.4}
              title={<div style={{ lineHeight: 1.8 }}>
                <div>{catOf(info)}</div>
                <div style={{ color: d.path ? undefined : '#ffa39e' }}>
                  {d.path || '未指定配方檔 — 選了不會有動作'}</div>
                {tagsOf(info).length ? <div>標籤：{tagsOf(info).join('、')}</div> : null}
                {isCur ? <div>目前載入中</div> : null}
              </div>}>
              <Button size="large" type={isCur ? 'primary' : 'default'}
                danger={!d.path}
                icon={isCur ? <CheckCircleFilled /> : undefined}
                onClick={() => (managing ? setEditing(dclone(info)) : choose(info))}>
                {info.name || '（未命名）'}
              </Button>
            </Tooltip>
          );
        })}
      </div>
    )}

    <div style={{ fontSize: 12, color: '#888', marginTop: 12, lineHeight: 1.7 }}>
      {managing ? '點一筆進入編輯。這份清單存在資料庫上,所有讀同一個位址的機台共用。'
                : '點一筆載入該配方並套用標籤。綠色框是目前載入中的那一筆。'}
    </div>
  </div>;
}

// One record's editor. Local until saved: `rec` is a draft the parent holds, so
// Cancel really cancels -- the old editor's Cancel button was rendered without
// an onCancel handler at all and did nothing when pressed.
function EditRecord({ rec, setRec, onSave, onCancel, onDelete, BPG_Channel, busy }) {
  const [browsing, setBrowsing] = useState(false);
  const d = defOf(rec);
  const set = (patch) => setRec({ ...rec, ...patch });
  const setDef = (patch) => {
    const arr = Array.isArray(rec.targetDeffiles) && rec.targetDeffiles.length
      ? dclone(rec.targetDeffiles) : [{}];
    arr[0] = { ...arr[0], ...patch };
    setRec({ ...rec, targetDeffiles: arr });
  };

  const pick = (path) => {
    // Ask the core what this def IS, and where the data folder is, so the stored
    // path is relative and the hashes travel with the record.
    const ask = (tl, data) => new Promise((resolve, reject) => {
      BPG_Channel(tl, 0, data, undefined, { resolve, reject });
      setTimeout(() => reject(new Error('逾時')), 5000);
    });
    Promise.all([ask('LD', { filename: path }), ask('FB', { path: './', depth: 0 })])
      .then((pkts) => {
        if (pkts[0][0].type !== 'FL' || pkts[1][0].type !== 'FS')
          throw new Error('核心回覆的型別不是預期的');
        const dataFolderPath = pkts[1][0].data.path;
        const m = pkts[0][0].data;
        setDef({
          hash: m.featureSet_sha1,
          featureSet_sha1: m.featureSet_sha1,
          featureSet_sha1_pre: m.featureSet_sha1_pre,
          featureSet_sha1_root: m.featureSet_sha1_root,
          path: path.replace(dataFolderPath, '').replace(/^\//, ''),
          name: m.name,
        });
      })
      // The old version swallowed this. A file that cannot be read leaves the
      // record pointing at the previous def, which is the wrong recipe under
      // the right name.
      .catch((e) => Modal.error({ title: '讀不到這個配方檔',
        content: `${errText(e)}。設定沒有變更。` }));
  };

  return <div style={{ maxWidth: '100%' }}>
    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 12 }}>
      <Button icon={<ArrowLeftOutlined />} onClick={onCancel} />
      <span style={{ fontSize: 16, fontWeight: 600 }}>
        {rec._id ? '編輯設定' : '新增設定（尚未寫入資料庫）'}
      </span>
    </div>

    <div style={{ marginBottom: 8 }}>
      <div style={{ fontSize: 12, color: '#888' }}>名稱 — 操作員在清單上看到的字</div>
      <Input value={rec.name || ''} autoFocus
        onChange={(e) => set({ name: e.target.value })} />
    </div>

    <div style={{ marginBottom: 8 }}>
      <div style={{ fontSize: 12, color: '#888' }}>分類 — 清單上的篩選群組,留空歸「{NO_CAT}」</div>
      <Input value={rec.cat || ''} onChange={(e) => set({ cat: e.target.value })} />
    </div>

    <div style={{ marginBottom: 8 }}>
      <div style={{ fontSize: 12, color: '#888' }}>標籤 — 載入時一併套用,以逗號分隔</div>
      <Input value={d.tags || ''} placeholder="例如：熱前,首件"
        onChange={(e) => setDef({ tags: e.target.value })} />
    </div>

    <Divider style={{ margin: '12px 0' }}>配方檔</Divider>
    {d.path ? (
      <div style={{ fontSize: 13, lineHeight: 1.9, wordBreak: 'break-all' }}>
        <div><b>{d.name || '（無名稱）'}</b></div>
        <div style={{ color: '#555' }}>{d.path}</div>
        <div style={{ color: '#888', fontSize: 12 }}>
          sha1 {String(d.featureSet_sha1 || '—').slice(0, 12)}</div>
      </div>
    ) : (
      <div style={{ color: '#cf1322' }}>尚未指定 — 這筆設定被選到時不會有任何動作</div>
    )}
    <Button icon={<FolderOpenOutlined />} style={{ marginTop: 8 }}
      onClick={() => setBrowsing(true)}>{d.path ? '換一個配方檔' : '選擇配方檔'}</Button>

    {browsing ? (
      <BPG_FileBrowser key="BPG_FileBrowser" className="width8 modal-sizing"
        searchDepth={4} path="data/" visible={true} BPG_Channel={BPG_Channel}
        onFileSelected={(filePath) => { setBrowsing(false); pick(filePath); }}
        onCancel={() => setBrowsing(false)}
        fileFilter={defFileFilter} />
    ) : null}

    <Divider style={{ margin: '16px 0 12px' }} />
    <div style={{ display: 'flex', gap: 8 }}>
      <Button type="primary" loading={busy === 'save'} onClick={onSave}>
        {rec._id ? '儲存' : '建立'}
      </Button>
      <Button onClick={onCancel}>取消</Button>
      <div style={{ flex: 1 }} />
      {onDelete ? (
        <Button danger icon={<DeleteOutlined />} loading={busy === 'del'}
          onClick={onDelete}>刪除</Button>
      ) : null}
    </div>
  </div>;
}

export default CustomDisplayPicker;
