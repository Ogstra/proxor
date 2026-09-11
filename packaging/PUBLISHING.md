# Package publication handoff

This runbook starts only after `publish-release` has succeeded and the protected
`winget-publication` validation has been approved. Review the release report and every entry in
`SHA256SUMS`; corrections require a new reviewed version, never asset replacement.

## Manual review gates

1. Confirm the public release assets, provenance report, and checksums.
2. Run `winget validate`, check the identifier collision, review the protected actual-asset
   install/upgrade report, then open one manually reviewed PR to `microsoft/winget-pkgs`.
3. Review the rendered AUR `PKGBUILD` and generated `.SRCINFO` diff, then push it manually with
   the dedicated `AUR_SSH_PRIVATE_KEY` and pinned AUR host key.
4. Verify the GitHub Flatpak bundle. A Flathub submission is optional and needs separate manual
   review; it is not CI automation.

There is no automatic publish path to winget catalogs, AUR, COPR/Fedora, Flathub, or any other
store. No credentials belong in the normal release workflow.
