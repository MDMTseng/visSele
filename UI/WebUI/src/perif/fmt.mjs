// Formatting helpers for the peripheral panels.
//
// Separate from the components so they can be unit-tested without React or a
// browser -- compactN in particular is arithmetic with edge cases, and it
// shipped with one (see below) that only an exhaustive sweep found.

// Counter text that cannot outgrow its box.
//
// These run to ~900k on a shift, and the sidebar strip gives each one a third
// of its width. Sizing the font for the worst case makes the common 3-digit
// reading tiny; letting six digits into a fixed box clips them, and a clipped
// count is not a smaller number, it is a WRONG one -- "148200" losing its tail
// reads as 1482. So never render more than 5 characters:
//
//     0 .. 9999      exact          "9999"
//     10k .. 99.9k   one decimal    "42.7k"
//     100k .. 999k   whole          "900k"
//     >= 1M          one decimal    "1.2M"
//
// Precision below the top three digits is dropped on purpose -- at that size
// the digit nobody reads is the last one. The exact value is never gone: every
// caller puts it on the element's title, and the history modal lists it whole.
//
// Each branch is chosen by what the ROUNDING WILL PRODUCE, not by the raw
// value. Testing the raw value looks right and is not: 99999 is under 100000,
// so it took the one-decimal branch and .toFixed(1) rounded it up to "100.0k"
// -- six characters, the one thing this function exists to prevent. The
// boundaries are 99950 and 999500 because that is where the rounding carries.
// Found by walking every integer from 0 to 1,000,000, not by reading.
export const compactN = (v) => {
  if (typeof v !== 'number' || !isFinite(v)) return '—';
  const a = Math.abs(v);
  if (a < 10000)  return String(v);
  const k = v / 1000;
  if (a < 99950)  return k.toFixed(1) + 'k';        // 10.0k .. 99.9k
  if (a < 999500) return Math.round(k) + 'k';       //  100k ..  999k
  return (v / 1000000).toFixed(1) + 'M';            //  1.0M and up
};

// The verdict counters, in full.
//
// compactN above exists because a clipped number is a WRONG number, and it
// solves that by dropping precision. For OK/NG/NA that trade is the wrong way
// round: those are the counts an operator reads off the machine and writes on a
// sheet, and "1.9M" cannot be written on a sheet. A shift that made 1,982,530
// parts did not make 1.9 million of them.
//
// So the strip keeps compactN and the exact digits live one tap away, in the
// bubble the counts row opens (CountsBubble). That was `title` before, which is
// a hover affordance -- on the touchscreen this machine actually runs on, the
// exact value had nowhere to be read at all.
//
// Grouped, because seven ungrouped digits are read wrong more often than they
// are read slowly.
export const exactN = (v) => {
  if (typeof v !== 'number' || !isFinite(v)) return '—';
  return Math.round(v).toLocaleString('en-US');
};
