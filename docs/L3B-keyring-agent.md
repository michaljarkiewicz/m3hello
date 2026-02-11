# L3B: Keyring Agent + DBus/Autostart Deconfliction (m3hello)

Goal: make GNOME Keyring unlock reliable and deterministic at login by ensuring there is exactly **one**
Secret Service provider (systemd user service) and by running a small user-session agent.

This layer builds on L3A (wrapper-based unlock at daemon startup).

---

## Problem observed

During L3A testing, GNOME also started a second `gnome-keyring-daemon` instance via:

- XDG autostart (`/etc/xdg/autostart/gnome-keyring-*.desktop`)
- DBus activation (`/usr/share/dbus-1/services/org.freedesktop.secrets.service`)

That DBus-activated instance runs:

`gnome-keyring-daemon --start --foreground --components=secrets`

and can grab the `org.freedesktop.secrets` bus name before the systemd-managed daemon starts.
When this happens, the wrapper-started daemon logs:

`another secret service is running`

Result: the Login keyring remains locked even after a successful YubiKey touch.

---

## Solution overview

1) **Disable XDG autostart** for gnome-keyring components at user level (do not edit system files).
2) **Override DBus activation** for Secret Service names at user level so DBus delegates activation to
   `systemd --user gnome-keyring-daemon.service` (which runs the L3A wrapper).
3) Add a small `m3hello-agent` oneshot user service that runs at `graphical-session.target` and:
   - exits quickly if the keyring is already unlocked,
   - otherwise restarts/starts the keyring daemon (triggering the wrapper unlock path),
   - polls the Secret Service `Locked` property until it becomes `false` or times out.
4) Provide a small CLI `m3hello-keyringctl` to check status and trigger a retry.

---

## Repo files

- `contrib/l3b-keyring/systemd-user/m3hello-agent.service`
- `contrib/l3b-keyring/m3hello-agent-keyring`
- `contrib/l3b-keyring/m3hello-keyringctl`
- `contrib/l3b-keyring/dbus-services/`
  - `org.freedesktop.secrets.service`
  - `org.gnome.keyring.service`
  - `org.freedesktop.impl.portal.Secret.service`
- `contrib/l3b-keyring/autostart/`
  - `gnome-keyring-secrets.desktop`
  - `gnome-keyring-pkcs11.desktop`
  - `gnome-keyring-ssh.desktop`

---

## Installation (user: mehow)

### 1) Install scripts (root)

```bash
sudo install -m 0755 contrib/l3b-keyring/m3hello-agent-keyring /usr/local/bin/
sudo install -m 0755 contrib/l3b-keyring/m3hello-keyringctl /usr/local/bin/
```

### 2) Install user systemd unit

```bash
mkdir -p ~/.config/systemd/user
install -m 0644 contrib/l3b-keyring/systemd-user/m3hello-agent.service \
  ~/.config/systemd/user/m3hello-agent.service

systemctl --user daemon-reload
systemctl --user enable --now m3hello-agent.service
```

### 3) Disable gnome-keyring autostart (user override)

```bash
mkdir -p ~/.config/autostart
install -m 0644 contrib/l3b-keyring/autostart/gnome-keyring-secrets.desktop ~/.config/autostart/
install -m 0644 contrib/l3b-keyring/autostart/gnome-keyring-pkcs11.desktop  ~/.config/autostart/
install -m 0644 contrib/l3b-keyring/autostart/gnome-keyring-ssh.desktop     ~/.config/autostart/
```

### 4) Override DBus activation (user override)

```bash
mkdir -p ~/.local/share/dbus-1/services
install -m 0644 contrib/l3b-keyring/dbus-services/org.freedesktop.secrets.service \
  ~/.local/share/dbus-1/services/
install -m 0644 contrib/l3b-keyring/dbus-services/org.gnome.keyring.service \
  ~/.local/share/dbus-1/services/
install -m 0644 contrib/l3b-keyring/dbus-services/org.freedesktop.impl.portal.Secret.service \
  ~/.local/share/dbus-1/services/
```

### 5) Relog required

DBus activation service mappings are reliably refreshed only after **log out / log in**.

---

## Verification

After relog:

### 1) No competing provider

```bash
ps -u "$USER" -o pid,cmd | grep -F "gnome-keyring-daemon --start --foreground --components=secrets" | grep -v grep || true
```

### 2) Secret Service owner should be the systemd-managed daemon

```bash
busctl --user status org.freedesktop.secrets | sed -n '1,60p'
```

### 3) Agent + keyring status

```bash
systemctl --user status m3hello-agent.service --no-pager
m3hello-keyringctl status
```

Expected: `(<false>,)`.

---

## Retry / Operations

If the keyring is locked:

```bash
m3hello-keyringctl retry
m3hello-keyringctl status
```

Logs:

```bash
m3hello-keyringctl logs
journalctl --user -u m3hello-agent.service -n 120 --no-pager
```

---

## Rollback

Disable agent:

```bash
systemctl --user disable --now m3hello-agent.service
rm -f ~/.config/systemd/user/m3hello-agent.service
systemctl --user daemon-reload
```

Remove overrides:

```bash
rm -f ~/.config/autostart/gnome-keyring-secrets.desktop
rm -f ~/.config/autostart/gnome-keyring-pkcs11.desktop
rm -f ~/.config/autostart/gnome-keyring-ssh.desktop

rm -f ~/.local/share/dbus-1/services/org.freedesktop.secrets.service
rm -f ~/.local/share/dbus-1/services/org.gnome.keyring.service
rm -f ~/.local/share/dbus-1/services/org.freedesktop.impl.portal.Secret.service
```

Optional: remove scripts:

```bash
sudo rm -f /usr/local/bin/m3hello-agent-keyring /usr/local/bin/m3hello-keyringctl
```

Relog recommended after rollback.
