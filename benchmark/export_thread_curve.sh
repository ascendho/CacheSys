#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
BIN_PATH="${BUILD_DIR}/benchmark/cache_benchmarks"
OUT_DIR="${BUILD_DIR}/benchmark"
RAW_PATH="${OUT_DIR}/thread_curve_raw.txt"
TSV_PATH="${OUT_DIR}/thread_curve_parsed.tsv"
CSV_PATH="${OUT_DIR}/thread_curve.csv"
SVG_PATH="${OUT_DIR}/thread_curve.svg"

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "benchmark binary not found: ${BIN_PATH}" >&2
  echo "Run: cmake --build ${BUILD_DIR} --target cache_benchmarks" >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

"${BIN_PATH}" \
  --benchmark_filter='BM_Sharded(Lru|Lfu)_MixedOps_MT' \
  --benchmark_min_time=0.3s \
  --benchmark_out="${RAW_PATH}" \
  --benchmark_out_format=csv > /dev/null

awk -F, '
{
  if ($1 !~ /^"BM_Sharded(Lru|Lfu)_MixedOps_MT/) {
    next;
  }

  name = $1;
  gsub(/"/, "", name);

  policy = (name ~ /^BM_ShardedLru_/) ? "LRU" : "LFU";
  thread = name;
  sub(/.*threads:/, "", thread);

  ops = $NF + 0.0;
  printf "%s,%d,%.0f\n", policy, thread + 0, ops;
}
' "${RAW_PATH}" > "${TSV_PATH}"

echo "threads,lru_ops_per_s,lfu_ops_per_s" > "${CSV_PATH}"
while IFS= read -r thread; do
  lru="$(awk -F, -v t="${thread}" '$1=="LRU" && $2==t {print $3}' "${TSV_PATH}" | head -n 1)"
  lfu="$(awk -F, -v t="${thread}" '$1=="LFU" && $2==t {print $3}' "${TSV_PATH}" | head -n 1)"
  [[ -z "${lru}" ]] && lru="0"
  [[ -z "${lfu}" ]] && lfu="0"
  echo "${thread},${lru},${lfu}" >> "${CSV_PATH}"
done < <(cut -d, -f2 "${TSV_PATH}" | sort -n | uniq)

awk -F, '
NR == 1 { next }
{
  n++;
  thread[n] = $1 + 0;
  lru[n] = $2 + 0;
  lfu[n] = $3 + 0;
  if (lru[n] > maxv) maxv = lru[n];
  if (lfu[n] > maxv) maxv = lfu[n];
}
END {
  if (n == 0) {
    exit 1;
  }

  w = 960; h = 560;
  ml = 90; mr = 40; mt = 50; mb = 90;
  pw = w - ml - mr; ph = h - mt - mb;
  if (maxv <= 0) maxv = 1;

  print "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" w "\" height=\"" h "\" viewBox=\"0 0 " w " " h "\">";
  print "<rect x=\"0\" y=\"0\" width=\"" w "\" height=\"" h "\" fill=\"#ffffff\"/>";
  print "<text x=\"" (w/2) "\" y=\"30\" text-anchor=\"middle\" font-size=\"20\" font-family=\"Helvetica\">Thread Scaling (Sharded LRU vs LFU)</text>";

  x0 = ml; y0 = mt + ph;
  x1 = ml + pw; y1 = mt;
  print "<line x1=\"" x0 "\" y1=\"" y0 "\" x2=\"" x1 "\" y2=\"" y0 "\" stroke=\"#222\" stroke-width=\"2\"/>";
  print "<line x1=\"" x0 "\" y1=\"" y0 "\" x2=\"" x0 "\" y2=\"" y1 "\" stroke=\"#222\" stroke-width=\"2\"/>";

  for (g = 0; g <= 5; ++g) {
    gy = mt + ph - (ph * g / 5.0);
    gv = maxv * g / 5.0;
    print "<line x1=\"" x0 "\" y1=\"" gy "\" x2=\"" x1 "\" y2=\"" gy "\" stroke=\"#e7e7e7\" stroke-width=\"1\"/>";
    printf("<text x=\"%d\" y=\"%.2f\" text-anchor=\"end\" font-size=\"11\" font-family=\"Helvetica\">%.1fM</text>\n", x0 - 8, gy + 4, gv / 1e6);
  }

  lruPoly = "";
  lfuPoly = "";
  for (i = 1; i <= n; ++i) {
    if (n == 1) {
      x = ml + pw / 2.0;
    } else {
      x = ml + (i - 1) * pw / (n - 1.0);
    }

    yl = mt + ph * (1.0 - lru[i] / maxv);
    yf = mt + ph * (1.0 - lfu[i] / maxv);

    lruPoly = lruPoly sprintf("%.2f,%.2f ", x, yl);
    lfuPoly = lfuPoly sprintf("%.2f,%.2f ", x, yf);

    print "<line x1=\"" x "\" y1=\"" y0 "\" x2=\"" x "\" y2=\"" y0 + 6 "\" stroke=\"#222\" stroke-width=\"1\"/>";
    printf("<text x=\"%.2f\" y=\"%d\" text-anchor=\"middle\" font-size=\"12\" font-family=\"Helvetica\">%d</text>\n", x, y0 + 24, thread[i]);

    printf("<circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" fill=\"#1f77b4\"/>\n", x, yl);
    printf("<circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" fill=\"#d62728\"/>\n", x, yf);
  }

  print "<polyline fill=\"none\" stroke=\"#1f77b4\" stroke-width=\"3\" points=\"" lruPoly "\"/>";
  print "<polyline fill=\"none\" stroke=\"#d62728\" stroke-width=\"3\" points=\"" lfuPoly "\"/>";

  printf("<text x=\"%d\" y=\"%d\" text-anchor=\"middle\" font-size=\"14\" font-family=\"Helvetica\">Threads</text>\n", ml + pw / 2, h - 25);
  print "<text x=\"22\" y=\"" (mt + ph / 2) "\" text-anchor=\"middle\" font-size=\"14\" font-family=\"Helvetica\" transform=\"rotate(-90 22 " (mt + ph / 2) ")\">Throughput (ops/s)</text>";

  lx = ml + pw - 210; ly = mt + 18;
  print "<rect x=\"" lx "\" y=\"" ly - 12 "\" width=\"190\" height=\"44\" fill=\"#ffffff\" stroke=\"#cccccc\"/>";
  print "<line x1=\"" lx + 10 "\" y1=\"" ly "\" x2=\"" lx + 40 "\" y2=\"" ly "\" stroke=\"#1f77b4\" stroke-width=\"3\"/>";
  print "<text x=\"" lx + 48 "\" y=\"" ly + 4 "\" font-size=\"12\" font-family=\"Helvetica\">Sharded LRU</text>";
  print "<line x1=\"" lx + 10 "\" y1=\"" ly + 20 "\" x2=\"" lx + 40 "\" y2=\"" ly + 20 "\" stroke=\"#d62728\" stroke-width=\"3\"/>";
  print "<text x=\"" lx + 48 "\" y=\"" ly + 24 "\" font-size=\"12\" font-family=\"Helvetica\">Sharded LFU</text>";

  print "</svg>";
}
' "${CSV_PATH}" > "${SVG_PATH}"

echo "Generated: ${CSV_PATH}"
echo "Generated: ${SVG_PATH}"
echo "Raw log  : ${RAW_PATH}"
