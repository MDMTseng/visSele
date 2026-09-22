#!/usr/bin/env bash
# Fleet run: migrate every OK recipe through the real 升級 button, then quick stability sweeps.
cd "$(dirname "$0")"
LOG=_ok_migrate.log; : > "$LOG"; : > _ok_fail.txt
n=0; total=$(wc -l < _ok_names.txt)
while read -r name; do
  [ -z "$name" ] && continue
  n=$((n+1)); start=$(date +%s)
  if timeout 300 node migrate_dump.mjs "$name" >> "$LOG" 2>&1; then st=ok; else st=FAIL; echo "$name" >> _ok_fail.txt; fi
  echo "[$n/$total] $name $st $(( $(date +%s) - start ))s" >> _ok_progress.txt
done < _ok_names.txt
echo "MIGRATION DONE $(date)" >> _ok_progress.txt
# sweeps in chunks of 20 (quick mode)
i=0; chunk=(); : > _ok_sweep.log
run_chunk() { [ ${#chunk[@]} -eq 0 ] && return; STAB_QUICK=1 STAB_OUT="_ok_stab_$i.json" timeout 1800 node stability_sweep.mjs "${chunk[@]}" >> _ok_sweep.log 2>&1; echo "sweep chunk $i done ($(date +%H:%M))" >> _ok_progress.txt; i=$((i+1)); chunk=(); }
while read -r name; do [ -z "$name" ] && continue; chunk+=("$name"); [ ${#chunk[@]} -ge 20 ] && run_chunk; done < _ok_names.txt
run_chunk
echo "ALL DONE $(date)" >> _ok_progress.txt
