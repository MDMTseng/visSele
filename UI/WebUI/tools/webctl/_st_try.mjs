import fs from 'node:fs';
import { sendMachineSetting, dirtied } from './_rc_clean.mjs';
const M = 'C:/Users/w2110/Documents/workspace/visSele/InspectionCore/Core0_1/data/machine_setting.json';
const mset = JSON.parse(fs.readFileSync(M, 'utf8'));
const keep = { inspection_region: mset.inspection_region, clean_regions: mset.clean_regions };
console.log('thresholds now:', keep.clean_regions.map(c => c.dark_thresh).join('/'));
try { console.log('apply  ->', await sendMachineSetting(dirtied(keep, 255))); }
catch (e) { console.log('apply FAILED:', e.message); }
await new Promise(r => setTimeout(r, 1500));
try { console.log('restore->', await sendMachineSetting(keep)); }
catch (e) { console.log('restore FAILED:', e.message); }
