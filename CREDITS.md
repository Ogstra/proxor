# Proxor — Credits and Third-Party Notices

Proxor is a fork of open-source projects distributed under the GNU General Public License v3.0.
This file fulfills the attribution requirements of that license.

---

## NekoBox for PC / NekoRay

**Original project:** NekoBox for PC (also known as NekoRay)
**Author:** Matsuridayo and contributors
**Repository:** https://github.com/MatsuriDayo/nekoray
**License:** GPL-3.0

Proxor is derived from NekoBox for PC. The original source code, commit history,
and all copyright notices are preserved in this repository's git history.

**Changes made in Proxor:**
- Rebranded all user-visible strings, window titles, and binary names from "NekoBox/NekoRay" to "Proxor"
- Changed auto-update target repository to github.com/Ogstra/proxor
- Replaced date-suffixed version scheme with semantic versioning (X.Y.Z)
- Renamed internal Go modules and packages (neko_common → proxor_common, etc.)
- Incorporated the libneko helper library directly into the project source tree

---

## libneko

**Original project:** libneko — Golang base helper library for NekoRay & NekoBox
**Author:** Matsuridayo and contributors
**Repository:** https://github.com/MatsuriDayo/libneko
**License:** GPL-3.0

The libneko library source has been incorporated into this repository under
`go/proxorlib/` and renamed to `proxorlib`. The original license and copyright
notices are reproduced in that directory.

---

## sing-box

**Project:** sing-box — The universal proxy platform
**Author:** nekohasekai (sagernet) and contributors
**Repository:** https://github.com/SagerNet/sing-box
**License:** GPL-3.0

sing-box is used as the core proxy engine. It is referenced as a dependency
and its source is not modified in this repository.

---

## Other dependencies

All other third-party Go and C++ dependencies are listed in:
- `go/cmd/proxor_core/go.mod` and `go.sum`
- `go/grpc_server/go.mod` and `go.sum`
- `3rdparty/` directory

Each dependency retains its own license. See individual `LICENSE` or `LICENSE.md`
files within each dependency for details.

---

## License

Proxor itself is distributed under the GNU General Public License v3.0.
See `LICENSE` in the root of this repository for the full license text.

In accordance with GPL-3.0:
- The complete corresponding source code of Proxor is available at:
  https://github.com/Ogstra/proxor
- You may copy, distribute, and modify this software under the terms of GPL-3.0.
- Any derivative work must also be distributed under GPL-3.0.
