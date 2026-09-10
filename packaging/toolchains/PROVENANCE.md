# Package builder provenance

Every release package is built from the clean recursive source archive produced by
`packaging/source/stage-recursive-source.sh`. Resolve every value below before
enabling a package job; mutable tags, unverified downloads, and unpinned actions
are not allowed.

| Builder | Immutable image | Toolchain | Verification |
| --- | --- | --- | --- |
| Ubuntu 22.04 | TBD | Qt 6 / Go 1.26.1 | `docker image inspect` and package-manager version output |
| Debian 12 | TBD | Xvfb / package smoke tools | `docker image inspect` and `dpkg-query` |
| Fedora | TBD | Qt 6 / Go 1.26.1 / rpmbuild | `docker image inspect` and `rpm -q` |
| Arch | TBD | Qt 6 / Go 1.26.1 / makepkg | `docker image inspect` and `pacman -Q` |

| Verified dependency | Pinned source | Verification |
| --- | --- | --- |
| actions/checkout | TBD | commit SHA and upstream tag |
| actions/setup-go | TBD | commit SHA and upstream tag |
| Qt provision | TBD | archive SHA-256 |
| Flatpak SDK / Builder | TBD | SDK and builder version plus source checksum |
