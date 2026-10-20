# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

---

## Reporting a Vulnerability

The Zenith Gaming Daemon engineering team takes the security of our low-latency systems infrastructure seriously. If you discover a security vulnerability or potential privilege escalation vector, please report it responsibly.

### How to Report

**Please DO NOT open a public issue on GitHub.**

Send an encrypted email report to:
- **Email**: `rokmit599@gmail.com`
- **Subject**: `[SECURITY VULNERABILITY] zenith-gaming-daemon: <brief-summary>`

### What to Include in Your Report
1. Description of the vulnerability and its potential impact.
2. Step-by-step instructions or proof-of-concept (PoC) code to reproduce the issue.
3. Relevant environment details (Linux kernel version, distribution, architecture).
4. Proposed mitigation or patch if you have developed one.

---

## Response Process & Timelines

1. **Initial Acknowledgment**: We will acknowledge receipt of your vulnerability report within **48 hours**.
2. **Assessment & Validation**: The engineering team will confirm the vulnerability and assess its severity within **5 business days**.
3. **Fix Development & Coordination**: We will develop and test a remediation patch in private branches.
4. **Public Disclosure**: A security advisory and fixed release will be published simultaneously, with full credit given to the reporter (unless anonymity is requested).

---

## Security Architecture Highlights
- **Crash-Only Journaling**: All hardware and kernel state mutations are logged in `/run/zenith/journal.json` using Write-Ahead Logging (WAL) and restored automatically.
- **Zero Shell Policy**: No reliance on `system()` or `popen()`. All subprocesses run through `execve` using strict binary whitelists.
- **Strict Capabilities**: Ambient capabilities are dropped down to `CAP_SYS_NICE`, `CAP_SYS_ADMIN`, and `CAP_SYS_RESOURCE`.
- **Systemd Hardening**: Runs under strict isolation (`ProtectSystem=strict`, `ProtectHome=true`, `PrivateTmp=true`).
