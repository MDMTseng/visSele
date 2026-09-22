// Is this a name the machine can actually write?
//
// THE CORE WRITES FILES THROUGH THE NARROW C RUNTIME, and on Windows that means
// the system code page -- 950 on these machines. The WebUI sends UTF-8. The two
// do not meet: measured on this bench with the core's own toolchain and OpenCV,
// a path with Chinese in it gives imwrite=0, fopen=0, and NOTHING on disk. Not
// an error, not a partial file -- the write simply does not happen.
//
// So a non-ASCII name is not a preference, it is a name that cannot exist. The
// save dialog already refused non-ASCII keystrokes, silently: the box just
// stopped accepting characters, with nothing said. The snapshot dialog had no
// check at all, so the same name could be typed there and the save failed
// somewhere further down. One rule, in one place, that says why -- and an
// operator who is told "the machine cannot write Chinese file names" can pick
// another name, while one whose keyboard appears broken cannot.
//
// (There is a note in InspectionUI, from before this, recording that 製程 tags
// were dropped out of snapshot names for exactly this reason.)
//
// The rest is Windows: reserved characters, reserved device names, and trailing
// dots or spaces, which the filesystem strips silently -- so "part " and "part"
// are the same file while the screen shows two.

const WIN_RESERVED = /^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)/i;
// Membership, not a regular expression. Written as a plain list of
// characters plus a code-point test, because the escaping of a backslash
// inside a regex inside a string is the kind of detail that is wrong for
// months without anybody noticing -- and what it guards is whether a file
// can be created at all.
const WIN_ILLEGAL_CHARS = '<>:"/\|?*';

function hasIllegalChar(s) {
  for (let i = 0; i < s.length; i++) {
    if (WIN_ILLEGAL_CHARS.indexOf(s[i]) >= 0) return true;
    if (s.charCodeAt(i) < 32) return true;   // control characters
  }
  return false;
}

/**
 * @param name the name as typed -- usually a STEM, with the extension added later.
 * @param opts.extensionFollows true when ".hydef"/".png"/... will be appended.
 *        A trailing dot or space is then harmless, because it ends up in the
 *        middle of the real filename: this folder already holds
 *        "10155  3G2570090BSORTING..hydef", and refusing that stem would mean
 *        an existing recipe could not be saved back.
 * @returns null when the name is usable, otherwise a reason in Chinese.
 */
export function fileNameIssue(name, opts) {
  const s = String(name === undefined || name === null ? '' : name);
  if (!s.length) return '請輸入檔名。';
  // eslint-disable-next-line no-control-regex
  if (/[^\x00-\x7F]/.test(s)) {
    // The action first, the reason second. An operator needs to know what to
    // type; why the machine is like this is context, not instruction.
    return '檔名請改用英數字。中文檔名這台機器寫不出來,而且不會報錯 —— 檔案會直接不存在。';
  }
  if (hasIllegalChar(s)) return '檔名不能含有 \\ / : * ? " < > | 這些字元。';
  if (WIN_RESERVED.test(s)) return 'CON、PRN、AUX、NUL、COM1-9、LPT1-9 是保留名稱,不能當檔名。';
  if (!(opts && opts.extensionFollows) && /[ .]$/.test(s)) {
    return '檔名結尾不能是空白或句點,Windows 會把它去掉,兩個不同的名字會變成同一個檔。';
  }
  return null;
}

/** Strip one trailing ".<ext>", anchored at the end and case-insensitive. */
export function stripExtension(name, ext) {
  return String(name || '').replace(new RegExp('\\.' + ext + '$', 'i'), '');
}
