# Layer 0 — PAM login + polkit via pam_u2f

This layer documents a working baseline:
- GDM (desktop login) uses `pam_u2f` with touch prompt.
- polkit (GUI authentication dialogs) uses `pam_u2f` with touch prompt.
- Password remains as fallback (nouserok + common-auth).

## Files used on Ubuntu 24.04 (GNOME)
- `/etc/pam.d/gdm-password`
- `/etc/pam.d/polkit-1` (may be absent by default; on Ubuntu the vendor file can be in `/usr/lib/pam.d/polkit-1`)
- `/etc/u2f_mappings` (or other authfile path)

## PAM snippets (reference)

### GDM
Add BEFORE `@include common-auth`:

auth sufficient pam_u2f.so cue nouserok authfile=/etc/u2f_mappings


### polkit
Use `/etc/pam.d/polkit-1` (copy from `/usr/lib/pam.d/polkit-1` if needed), then add BEFORE `@include common-auth`:

auth sufficient pam_u2f.so cue nouserok user=<YOUR_USER> authfile=/etc/u2f_mappings


`user=<YOUR_USER>` avoids polkit using an unexpected PAM user context.

## Verification checklist
- Lock screen → touch key unlocks (password still works).
- polkit prompt (e.g., GNOME Settings action requiring auth) → touch key works (password still works).

## Rollback (quick)
- Remove the `pam_u2f.so ...` lines from:
  - `/etc/pam.d/gdm-password`
  - `/etc/pam.d/polkit-1`
- Optionally keep `/etc/u2f_mappings` (harmless), or remove it if you want.

## Notes
- WebAuthn for websites is separate from PAM. Browsers use FIDO2/WebAuthn; PAM uses `pam_u2f`.
