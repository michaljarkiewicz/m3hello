# m3hello (M3HELLO)

Windows-Hello-like authentication and secrets unlocking for Linux desktop using a hardware token (YubiKey).

## TL;DR
Goal: **Touch the YubiKey → unlock session secrets** (Secret Service / keyring-equivalent) with password as fallback.
This project aims to provide a cohesive, reproducible, Linux-native alternative to the "type password to unlock keyring" flow.

---

## Motivation
On Windows, Windows Hello can provide a smooth UX:
- biometric / presence gesture
- secrets are unlocked transparently (TPM-backed)
- password remains a fallback

On Linux (GNOME), secrets are typically gated by **GNOME Keyring** which is password-driven.
When login is performed by **PAM U2F / FIDO2 touch**, GNOME Keyring often stays locked and prompts for a password.

m3hello aims to bridge that gap.

---

## Non-goals (initially)
- Replacing a full display manager login stack (GDM) from scratch
- Re-implementing the whole GNOME Keyring feature set
- Providing "no-touch background unlock" (presence must be user-mediated)

---

## High-level architecture
m3hello consists of separate layers. Each layer can be adopted independently.

### Layer 0: Login presence (already working)
Use PAM with a YubiKey to confirm local presence for:
- desktop login (GDM / sudo / polkit)

**Status:** Working in our environment via `pam_u2f` (touch-only).

### Layer 1: Vault key derivation (FIDO2 touch → key)
Derive a cryptographic secret (session key material) from a YubiKey using:
- FIDO2 credential + `hmac-secret` extension
- user presence (touch)

**Output:** 32-byte secret used to decrypt the local vault.

### Layer 2: Vault (encrypted secret store)
A small encrypted file stored under the user home directory, e.g.:
`~/.local/share/m3hello/vault.v1`

The vault contains:
- wrapped keys / secrets needed to unlock Secret Service
- metadata for multi-token support
- integrity (AEAD + MAC)

### Layer 3A (bridge): Unlock GNOME Keyring (optional)
A compatibility bridge for GNOME environments:
- decrypt the stored keyring password (or key material)
- unlock existing GNOME Keyring daemon

This is a transitional layer; it is fragile because GNOME Keyring was not designed for FIDO2-first login.

### Layer 3B (target): Secret Service provider
Implement our own `org.freedesktop.secrets` provider (Secret Service API), backed by m3hello vault.
Apps that speak Secret Service (Chrome, NetworkManager integrations, etc.) can store/retrieve secrets without GNOME Keyring.

This is the long-term "Hello-like" solution.

### Layer 4 (optional): TPM-backed sealing
Optionally seal the vault master key to TPM:
- tie secrets to a specific machine state (PCRs)
- still require YubiKey touch as presence factor

---

## Data flow (session)
1. User logs into GNOME using YubiKey touch (PAM U2F).
2. `m3hello-agent` starts (systemd --user).
3. Agent requests a touch and derives `K_session` from YubiKey (FIDO2 hmac-secret).
4. Agent decrypts the vault and unlocks:
   - either GNOME Keyring (bridge)
   - or m3hello Secret Service (target)
5. Apps request secrets through Secret Service (no password prompts).
6. If YubiKey is missing, fallback path is available (password / recovery key).

---

## Threat model (short)
### Protects against:
- Offline home directory theft (vault is encrypted; needs YubiKey-derived secret)
- Remote login attempts (presence required)
- Accidental password prompts / shoulder surfing reduction

### Does NOT protect against:
- A fully compromised running session (malware in your session can request secrets)
- Physical attacker with the unlocked session and active user context

---

## Roadmap (milestones)
### M0: Repo + docs
- architecture docs
- threat model docs
- reproducible setup notes

### M1: CLI prototype (key derivation + vault format)
- enroll YubiKey FIDO2 credential for `hmac-secret`
- derive deterministic 32B secret with touch
- implement vault.v1 format + tests

### M2: Agent (systemd --user)
- touch prompt
- decrypt vault
- provide a stable unlock action

### M3: Bridge to GNOME Keyring (optional)
- unlock GNOME keyring reliably OR document limitations
- ensure no plaintext passwords are stored

### M4: Secret Service provider (target)
- implement minimal Secret Service API needed by common apps
- replace GNOME Keyring as secrets backend

### M5: Multi-token & migration
- add second key
- rotate keys
- safe export/import to another machine/user

### M6 (optional): TPM sealing
- tpm2 integration
- PCR policies

---

## Repo structure (proposed)
