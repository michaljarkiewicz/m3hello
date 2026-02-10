#!/usr/bin/env bash
set -euo pipefail

outdir="${1:-./_audit}"
mkdir -p "$outdir"

echo "[*] Exporting PAM configs (no secrets)..."

for f in /etc/pam.d/gdm-password /etc/pam.d/polkit-1 /usr/lib/pam.d/polkit-1; do
  if [ -f "$f" ]; then
    bn="$(basename "$f")"
    cp -a "$f" "$outdir/$bn"
    echo "  - copied: $f -> $outdir/$bn"
  fi
done

echo
echo "[*] Checking whether authfile exists (NOT copying secrets):"
if [ -f /etc/u2f_mappings ]; then
  echo "  - /etc/u2f_mappings exists (NOT copied)."
else
  echo "  - /etc/u2f_mappings not found."
fi

echo
echo "[*] Grep pam_u2f references:"
grep -R --line-number --no-messages "pam_u2f\.so" /etc/pam.d 2>/dev/null || true
echo
echo "[OK] Audit written to: $outdir"
