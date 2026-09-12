# Package publication handoff

This runbook starts only after `publish-release` has succeeded and the protected
`winget-publication` validation has been approved. Review the release report and every entry in
`SHA256SUMS`; corrections require a new reviewed version, never asset replacement.

## Review gates

1. Confirm the public release assets, provenance report, and checksums.
2. Run the protected actual-asset install/upgrade validation
   (`.github/workflows/validate-winget-release.yml`) and check the identifier collision.
3. Review the rendered AUR `PKGBUILD` and `.SRCINFO` from the `release-recipes` artifact.
4. Verify the GitHub Flatpak bundle. A Flathub submission is optional and needs separate manual
   review; it is not CI automation.

## Publishing

`.github/workflows/publish-packages.yml` performs the AUR push and the `microsoft/winget-pkgs`
pull request. It takes a published release tag, re-derives every input from the release assets
and their checksums, and rebuilds the AUR recipe from the published archive before pushing.
Run it with `dry_run` first to get the rendered recipe and manifests as artifacts without
publishing anything:

```bash
gh workflow run publish-packages.yml -f release_tag=vX.Y.Z -f channels=both -f dry_run=true
```

Both jobs run in protected environments (`aur-publication`, `winget-publication`), so the
review gates above stay in front of the credentials.

There is still no automatic publish path to COPR/Fedora, Flathub, or any other store, and the
release workflow itself holds no publishing credentials.

## One-time setup

The accounts cannot be created from CI:

1. Register an AUR account and add the automation SSH public key to its profile at
   <https://aur.archlinux.org/account>. The `proxor` name is claimed by the first push.
2. Store the matching private key as the `AUR_SSH_PRIVATE_KEY` secret of the
   `aur-publication` environment. The server host keys are pinned in
   `packaging/arch/aur-known-hosts`, so they are not trusted on first use.
3. Create a GitHub personal access token with `public_repo` scope for the account that owns the
   `microsoft/winget-pkgs` fork, and store it as the `WINGET_PKGS_TOKEN` secret of the
   `winget-publication` environment.

```bash
gh secret set AUR_SSH_PRIVATE_KEY --env aur-publication < /path/to/aur_key
gh secret set WINGET_PKGS_TOKEN --env winget-publication
```

After that, publishing a version is one workflow run, and changing what gets published is a
commit to `packaging/arch/PKGBUILD.in` or `packaging/winget/templates`.
