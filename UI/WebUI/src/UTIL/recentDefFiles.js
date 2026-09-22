// The 近期檔案 list (localStorage "RecentDefFiles"), in one place.
//
// Until 2026-09-09 only MAINUI's two loaders (the file selector and the recent
// list itself) appended to it; DefConfUI's own 載入 button and the catalogue
// picker (機台設定 -> 選配方) loaded defs without a trace, so a def opened
// there never showed up under 近期檔案. Every def load now goes through
// noteRecentDefFile.
//
// An entry is { name, path, type, ctime_ms?, mtime_ms?, size_bytes? }. The
// file metadata is what the file browser hands over; a load that only knows
// the path (catalogue, boot default) records name/path/type and leaves the
// rest out -- better a row with blank size than no row.
import { LocalStorageTools } from './MISC_Util';

export const RECENT_DEF_LS_KEY = 'RecentDefFiles';
const RECENT_MAX = 100;

export function isRecentDefEntry(fileInfo) {
  if (!fileInfo || typeof fileInfo !== 'object') return false;
  if (typeof fileInfo.name !== 'string' || !fileInfo.name.length) return false;
  if (typeof fileInfo.path !== 'string' || !fileInfo.path.length) return false;
  if (typeof fileInfo.type !== 'string') return false;
  for (const k of ['ctime_ms', 'mtime_ms', 'size_bytes'])
    if (fileInfo[k] !== undefined && typeof fileInfo[k] !== 'number') return false;
  return true;
}

export function getRecentDefFiles() {
  return LocalStorageTools.getlist(RECENT_DEF_LS_KEY).filter(isRecentDefEntry);
}

// Record a def load. `pathNoExt` is the def path without ".hydef" (the form the
// editor and the LD request use); `fileInfo` is the file browser's record when
// the load came from one, else omitted. Same path replaces the older entry, so
// the list is most-recent-first with no duplicates; capped at RECENT_MAX.
export function noteRecentDefFile(pathNoExt, fileInfo, defExtension) {
  if (typeof pathNoExt !== 'string' || !pathNoExt.length) return false;
  const ext = defExtension ? '.' + defExtension : '';
  const path = pathNoExt.replaceAll('\\', '/') + ext;
  const name = path.slice(path.lastIndexOf('/') + 1);
  const entry = { name, path, type: 'file' };
  if (fileInfo && typeof fileInfo === 'object') {
    for (const k of ['ctime_ms', 'mtime_ms', 'size_bytes'])
      if (typeof fileInfo[k] === 'number') entry[k] = fileInfo[k];
    if (typeof fileInfo.type === 'string') entry.type = fileInfo.type;
  }
  try {
    LocalStorageTools.appendlist(RECENT_DEF_LS_KEY, entry,
      (old, idx) => idx < RECENT_MAX && old.path !== entry.path);
    return true;
  } catch (e) { return false; }
}
