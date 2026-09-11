# Package builder provenance

Every release package is built from the clean recursive source archive produced by
`packaging/source/stage-recursive-source.sh`. Resolve every value below before
enabling a package job; mutable tags, unverified downloads, and unpinned actions
are not allowed.

| Builder | Immutable image | Toolchain | Verification |
| --- | --- | --- | --- |
| ubuntu-22.04 | `ubuntu@sha256:829f6df217bcbae2b371026e81711d1a787c61b2967ad09d015063663ebafbf7` | Qt 6 / Go 1.26.1 | Docker Hub index digest recorded 2026-09-10; `docker buildx imagetools inspect` |
| debian-12 | `debian@sha256:88200866dfff7ea7f5cbcb6ec7c8a701889efe6fe859fe64d6990e4b07ea4171` | Xvfb / package smoke tools | Docker Hub index digest recorded 2026-09-10; `docker buildx imagetools inspect` |
| fedora | `fedora@sha256:43b29f65a41eb9c35e1cd5323e3bdf3b655c2357a9f4f1ff2f9c2798e5045d80` | Qt 6 / Go 1.26.1 / rpmbuild | Docker Hub index digest recorded 2026-09-10; `docker buildx imagetools inspect` |
| arch | `archlinux@sha256:b944cc65c5f28665dfd5fdbf5ed2997c88f5bb4a0aefac7ee8a7ef01893e5ed9` | Qt 6 / Go 1.26.1 / makepkg | Docker Hub index digest recorded 2026-09-10; `docker buildx imagetools inspect` |

| Verified dependency | Pinned source | Verification |
| --- | --- | --- |
| actions/checkout@`fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09` | v5 | GitHub API commit resolution recorded 2026-09-10 |
| actions/setup-go@`40f1582b2485089dde7abd97c1529aa768e1baff` | v5 | GitHub API commit resolution recorded 2026-09-10 |
| Qt provision | Qt 6.7.2 via `jurplel/install-qt-action@c6c7281365daef91a238e1c2ddce4eaa94a2991d` | v4.1.1 action commit; action's aqt download verification is reviewed before enabling native package jobs |
| Flatpak SDK / Builder | Flatpak SDK 24.08 and Builder 24.08, resolved from the verified Flathub runtime remote in the Flatpak package job | Job must record the resolved SDK/Builder commits and reject an unsigned or changed remote summary before use |
| actions/upload-artifact@`65462800fd760344b1a7b4382951275a0abb4808` | v4.3.3 | GitHub action commit pinned in package artifact jobs |
| actions/download-artifact@`d3f86a106a0bac45b974a628896c90dbdf5c8093` | v4 | GitHub action commit pinned in package fan-in jobs |
| actions/cache@`0057852bfaa89a56745cba8c7296529d2fc39830` | v4 | GitHub action commit pinned in native build jobs |
| jurplel/install-qt-action@`c6c7281365daef91a238e1c2ddce4eaa94a2991d` | v4.1.1 | GitHub action commit resolution recorded 2026-09-11 |
| ilammy/msvc-dev-cmd@`0b201ec74fa43914dc39ae48a89fd1d8cb592756` | v1 | GitHub action commit resolution recorded 2026-09-11 |
| seanmiddleditch/gha-setup-ninja@`8b297075da4cd2a5f1fd21fe011b499edf06e9d2` | v4 | GitHub action commit resolution recorded 2026-09-11 |
