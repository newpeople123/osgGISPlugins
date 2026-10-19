#!/usr/bin/env bash
# Python-free matrix runner (this host's `python` is a Windows Store stub, see
# STATUS.md B-5, so scripts/run_matrix.py cannot run here).
#
# Runs Release/model23dtiles.exe over every test_assets/ entry and collects the
# metrics the tool writes itself (conversion-report.json) — no re-measuring, no
# reimplementing the acceptance numbers in shell.
#
#   bash scripts/run_matrix.sh [testAssetsDir] [outDir]
#   node scripts/collect_matrix.js <outDir>
#
# outDir must be a Windows-style path (E:/... or F:/...): the collector is node,
# which does not understand MSYS paths like /e/dev/...
set -u
root="E:/dev/C++/osgGISPlugins-gpt"
assets="${1:-$root/test_assets}"
out="${2:-$root/docs/model23dtiles-production/runs/matrix}"
mkdir -p "$out"
for f in "$assets"/*; do
  case "$(printf '%s' "$f" | tr 'A-Z' 'a-z')" in
    *.obj|*.osgt|*.osg|*.fbx|*.gltf|*.glb) ;;
    *) continue ;;
  esac
  name=$(basename "$f"); base="${name%.*}"
  dest="$out/$base"
  rm -rf "$dest"; mkdir -p "$dest"
  start=$(date +%s.%N)
  "$root/Release/model23dtiles.exe" -sp -i "$f" -o "$dest" > "$dest/run.log" 2>&1
  rc=$?
  end=$(date +%s.%N)
  wall=$(node -e "process.stdout.write((($end-$start)).toFixed(2))")
  rep="$dest/conversion-report.json"
  if [ -f "$rep" ]; then
    summ=$(node -e "
      const r=require('$(echo "$rep" | sed 's/\\/\\\\/g; s/"/\\"/g')');
      process.stdout.write([r.tileCount,r.maxDepth,r.contentTileCount,
        (r.totalSeconds||0).toFixed(2),Math.round((r.peakRssBytes||0)/1048576),
        r.errorMaxMeters,r.nodesCertified,r.nodesBoundLoose,
        r.aggregateProxyCount,r.proxySkippedCount,r.sourceFeatureCount,
        r.featuresMissingFromLeaves,r.proxyDroppedFeatureCount].join('|'));
    " 2>/dev/null)
  else
    summ="no-report"
  fi
  ts=$( [ -f "$dest/tileset.json" ] && echo yes || echo no )
  echo "$name rc=$rc wall=${wall}s tileset=$ts stats=$summ"
done
# stats field order: tiles|depth|content|sec|MiB|errMax|cert|loose|agg|skip|src|miss|drop
