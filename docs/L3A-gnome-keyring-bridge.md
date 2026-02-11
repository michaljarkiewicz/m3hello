# L3A: GNOME Keyring Bridge (m3hello)

Goal: after a PAM touch-only login, unlock **GNOME Keyring (Login)** by requiring only a **YubiKey touch** (no GNOME password prompt).

This implements **L3A** using a systemd --user approach.

---

## Background / Why this approach

In this environment, calling `gnome-keyring-daemon --unlock` as a standalone helper **did not unlock** the existing, systemd-managed keyring daemon.  
However, starting **a keyring daemon instance with `--unlock` at daemon startup** (with the keyring password on stdin) reliably unlocked the `login` collection.

Therefore, the operational approach is:

- keep using the standard `gnome-keyring-daemon.service` (systemd --user),
- override its `ExecStart` to run a wrapper that:
  - retrieves the keyring password via `m3hello-vault open` (touch-only),
  - feeds it to `gnome-keyring-daemon ... --unlock` on **stdin**,
  - uses timeout + fallback to avoid hanging the session.

No plaintext password is stored on disk, and nothing is passed via argv.

---

## Files

- `contrib/gnome-keyring/m3hello-keyringd-wrapper`  
  Wrapper that starts `gnome-keyring-daemon` and performs unlock at daemon startup.

- `contrib/gnome-keyring/10-m3hello-unlock.conf`  
  systemd user drop-in override for `gnome-keyring-daemon.service` which replaces the stock `ExecStart`
  with the wrapper and silences stdout to avoid printing environment output.

---

## Installation (user: mehow)

### 1) Install the wrapper (root)

```bash
sudo install -m 0755 contrib/gnome-keyring/m3hello-keyringd-wrapper /usr/local/bin/
```

### 2) Install the systemd drop-in (user)

```bash
mkdir -p ~/.config/systemd/user/gnome-keyring-daemon.service.d
install -m 0644 contrib/gnome-keyring/10-m3hello-unlock.conf \
  ~/.config/systemd/user/gnome-keyring-daemon.service.d/10-m3hello-unlock.conf
```

### 3) Reload + restart keyring service

```bash
systemctl --user daemon-reload
systemctl --user restart gnome-keyring-daemon.service
```

Expected behavior:
- a YubiKey touch is requested (via `m3hello-vault`),
- GNOME Keyring (Login) becomes unlocked.

---

## Verification / Test

### A) Lock the keyring

```bash
gdbus call --session \
  --dest org.gnome.keyring \
  --object-path /org/freedesktop/secrets \
  --method org.freedesktop.Secret.Service.LockService
```

### B) Restart keyring daemon (should require YubiKey touch)

```bash
systemctl --user restart gnome-keyring-daemon.service
```

### C) Confirm the `login` collection is unlocked

```bash
COL="$(gdbus call --session \
  --dest org.freedesktop.secrets \
  --object-path /org/freedesktop/secrets \
  --method org.freedesktop.Secret.Service.ReadAlias default \
  | awk -F"'" '{print $2}')"

gdbus call --session \
  --dest org.freedesktop.secrets \
  --object-path "$COL" \
  --method org.freedesktop.DBus.Properties.Get \
    org.freedesktop.Secret.Collection Locked
```

Expected output: `(<false>,)`.

---

## Logs

Inspect logs:

```bash
journalctl --user -u gnome-keyring-daemon.service -n 100 --no-pager
```

Note:
You may see a message similar to:

`NotInInitialization: Setenv interface is only available during the DisplayServer and Initialization phase`

This is expected when restarting the daemon after GNOME session initialization and does not prevent the Secret Service from working.

---

## Security notes

- The keyring password is never stored in plaintext by this bridge.
- The password is delivered to `gnome-keyring-daemon` only via **stdin**.
- No password is placed in command-line arguments.
- Wrapper uses `umask 077`.
- Wrapper includes a timeout and a fallback path to avoid breaking the session if YubiKey is unavailable.

---

## Rollback

Remove the drop-in and restart the service:

```bash
rm -f ~/.config/systemd/user/gnome-keyring-daemon.service.d/10-m3hello-unlock.conf
systemctl --user daemon-reload
systemctl --user restart gnome-keyring-daemon.service
```

If needed, also remove the wrapper from `/usr/local/bin`:

```bash
sudo rm -f /usr/local/bin/m3hello-keyringd-wrapper
```
