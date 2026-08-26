// The def's sibling images, and switching between them. ONE implementation.
//
// A def folder holds <base>.png plus whatever "立即測試 → 存成另一張" wrote next
// to it. Two screens need to move between them: DefConfUI, to look at the
// overlay on a different sample, and the SBM studio, to try the localizer
// against more than the one image it happened to open on.
//
// It lives here rather than twice because of what "switch the image" actually
// is: an FB scan to find the siblings, an LD to load one into the CORE's cache,
// and an IM dispatch so the canvas shows it. The core-cache half is the part
// that would rot in a copy -- every test round trip runs against
// `__CACHE_IMG__`, so a screen that changed only its own bitmap would show one
// image and measure another, and nothing on screen would say so.
import { useState, useEffect, useRef, useCallback, useSyncExternalStore } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import * as BPG_Protocol from 'UTIL/BPG_Protocol';

// Matches DefConfUI's IMG_LOAD_DOWNSAMP_LEVEL. Both sides must load at the same
// level or the two screens disagree about how many px a mm is.
export const IMG_LOAD_DOWNSAMP_LEVEL = 1;

const IMG_EXT = /\.(png|jpe?g|bmp)$/i;

// WHICH IMAGE IS LOADED is a property of the CORE, not of a component, so it is
// held once and every selector reads the same value.
//
// There are two selectors on screen at the same time -- DefConfUI's corner
// dropdown and the SBM studio's -- and the core has exactly one cached image.
// With per-component state, switching in the studio left DefConfUI's dropdown
// naming the previous file: a control that says the machine is looking at
// something it is not.
const sel = {
  path: undefined,
  subs: new Set(),
  get() { return sel.path; },
  set(p) { if (p === sel.path) return; sel.path = p; sel.subs.forEach((f) => f()); },
  subscribe(f) { sel.subs.add(f); return () => sel.subs.delete(f); },
};
// A different def has a different folder, so the selection cannot carry over.
export function resetSelectedImage() { sel.set(undefined); }

export function useDefImages({ afterLoad } = {}) {
  const dispatch = useDispatch();
  const edit_info = useSelector((s) => s.UIData.edit_info);
  const CORE_ID = useSelector((s) => s.ConnInfo.CORE_ID);
  const send = useCallback((...a) => dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, ...a)),
                          [dispatch, CORE_ID]);

  const [imageList, setImageList] = useState([]);          // [{name, path}]
  const currentImagePath = useSyncExternalStore(sel.subscribe, sel.get, sel.get);
  const setCurrentImagePath = sel.set;

  // "立即測試 → 存成另一張" writes a new sibling via SV and fires this event, so a
  // new image appears in the list without leaving the def.
  const [reloadTick, setReloadTick] = useState(0);
  useEffect(() => {
    const h = () => setReloadTick((t) => t + 1);
    window.addEventListener('defconf-images-changed', h);
    return () => window.removeEventListener('defconf-images-changed', h);
  }, []);

  // Discover siblings via FB, once per loaded def (and once per reloadTick).
  // curPathRef tracks the live selection so a re-scan keeps what the user is
  // looking at instead of snapping back to the base image.
  const scanRef = useRef(null);
  const curPathRef = useRef(undefined);
  curPathRef.current = currentImagePath;
  useEffect(() => {
    if (!CORE_ID || !edit_info || !edit_info.defModelPath) return;
    const dmp = edit_info.defModelPath;
    const scanKey = dmp + '#' + reloadTick;
    if (scanRef.current === scanKey) return;
    scanRef.current = scanKey;
    const slash = Math.max(dmp.lastIndexOf('/'), dmp.lastIndexOf('\\'));
    const dir = slash >= 0 ? dmp.substring(0, slash) : '.';
    const base = slash >= 0 ? dmp.substring(slash + 1) : dmp;
    send('FB', 0, { path: dir, depth: 1 }, undefined, { resolve: (darr) => {
      if (!(darr && darr[1] && darr[1].data && darr[1].data.ACK)) return;
      const fs = darr[0] && darr[0].data;
      const files = (fs && fs.files) || [];
      const fdir = (fs && fs.path) || dir;
      const imgs = files
        .filter((f) => f && f.type !== 'DIR' && typeof f.name === 'string'
                       && f.name.indexOf(base) === 0 && IMG_EXT.test(f.name))
        .map((f) => ({ name: f.name, path: fdir + '/' + f.name }));
      setImageList(imgs);
      const keep = imgs.find((im) => im.path === curPathRef.current);
      const cur = keep
        || imgs.find((im) => im.name.replace(IMG_EXT, '') === base)
        || imgs[0];
      if (cur) setCurrentImagePath(cur.path);
    }, reject: () => {} });
  }, [CORE_ID, edit_info.defModelPath, reloadTick, send]);

  // Image-only swap: LD with imgsrc and NO deffile, so the def, the shapes, the
  // selection and the edit mode are all untouched. IGNORE_DEFCONF_LOCK is what
  // lets the resulting IM through the post-load display lock.
  const switchImage = useCallback((imgPath) => {
    if (!CORE_ID || !imgPath) return;
    setCurrentImagePath(imgPath);
    send('LD', 0, { imgsrc: imgPath, down_samp_level: IMG_LOAD_DOWNSAMP_LEVEL },
         undefined, { resolve: (darr) => {
      const IM = (darr || []).find((p) => p.type === 'IM');
      if (IM) {
        const a = BPG_Protocol.map_BPG_Packet2Act(IM);
        if (a) { a.IGNORE_DEFCONF_LOCK = true; dispatch(a); }
      }
      // Whatever the caller has to redo now that the core is holding a
      // different image -- re-orient, or throw away a test result that was
      // measured on the previous one.
      if (afterLoad) afterLoad(imgPath);
    }, reject: () => {} });
  }, [CORE_ID, send, dispatch, afterLoad]);

  return { imageList, currentImagePath, switchImage };
}
