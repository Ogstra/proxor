# Package publication handoff

This runbook covers the AUR, winget and Homebrew tap channels. It starts only after
`publish-release` has succeeded and the protected `winget-publication` validation has been
approved. Review the release report and every entry in
`SHA256SUMS`; corrections require a new reviewed version, never asset replacement.

## Review gates

1. Confirm the public release assets, provenance report, and checksums.
2. Run the protected actual-asset install/upgrade validation
   (`.github/workflows/validate-winget-release.yml`) and check the identifier collision.
3. Review the rendered AUR `PKGBUILD` and `.SRCINFO` from the `release-recipes` artifact.
4. Verify the GitHub Flatpak bundle. A Flathub submission is optional and needs separate manual
   review; it is not CI automation.

## Publishing

`.github/workflows/publish-packages.yml` performs the AUR push, the `microsoft/winget-pkgs`
pull request and, on request, the Homebrew tap bump. It takes a published release tag, re-derives every input from the release assets
and their checksums, and rebuilds the AUR recipe from the published archive before pushing.
Run it with `dry_run` first to get the rendered recipe and manifests as artifacts without
publishing anything:

```bash
gh workflow run publish-packages.yml -f release_tag=vX.Y.Z -f channels=both -f dry_run=true
```

The AUR and winget jobs run in protected environments (`aur-publication`,
`winget-publication`), so the review gates above stay in front of the credentials. `both` means
AUR plus winget; `all` adds the Homebrew tap.

### Windows-only stable release

The `windows_only` dispatch input (default `n`) publishes a stable release that carries only the
source tarball, `proxor-<version>-windows64.zip`, `proxor-<version>-winget-x64.zip` and a
`SHA256SUMS` listing exactly those files, while the other platforms stay on prereleases. It is
refused unless `publish=y` and a tag are set. All build jobs still run and gate the release;
`bump-homebrew-tap` is skipped, and the AUR and Homebrew channels of `publish-packages.yml`
must not be used for such a release (use `channels=winget`).

```bash
gh workflow run build-proxor-cmake.yml --ref main -f tag=vX.Y.Z -f publish=y -f prerelease=n -f windows_only=y
```

### Homebrew tap

`bump-homebrew-tap` in `.github/workflows/build-proxor-cmake.yml` runs right after
`publish-release` on every publishing dispatch. It downloads
`proxor-<version>-macos-arm64.zip` and `SHA256SUMS` from the published release, verifies the
hash, renders `packaging/homebrew/proxor.rb.in` into `Casks/proxor.rb` of `Ogstra/homebrew-tap`
and pushes `proxor <version>` to `main`. It refuses to move the cask to an older version and
does nothing when the tap already has this version. To re-run it (for example after rotating
the token) or to preview it:

```bash
gh workflow run publish-packages.yml -f release_tag=vX.Y.Z -f channels=homebrew -f dry_run=true
gh workflow run publish-packages.yml -f release_tag=vX.Y.Z -f channels=homebrew
```

The cask follows every published release including prereleases (all Proxor releases are
currently prereleases); the tap allows this through
`audit_exceptions/github_prerelease_allowlist.json`. A separate stable-only cask would be a new
`proxor@beta`-style split later, not a change to this one. Changes to the cask are commits to
`packaging/homebrew/proxor.rb.in`, never edits in the tap.

There is still no automatic publish path to COPR/Fedora, Flathub, or any other store. The
release workflow holds exactly one publishing credential, `HOMEBREW_TAP_TOKEN`, confined to the
`bump-homebrew-tap` job and the `homebrew-publication` environment.

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

4. Set up the Homebrew tap credential (`HOMEBREW_TAP_TOKEN`):
   - Create a fine-grained personal access token: GitHub -> Settings -> Developer settings ->
     Personal access tokens -> Fine-grained tokens -> Generate new token. Token name
     `proxor-homebrew-tap`; Resource owner `Ogstra`; Expiration at most 1 year (put a rotation
     reminder in a calendar); Repository access -> Only select repositories ->
     `Ogstra/homebrew-tap`; Repository permissions -> Contents: Read and write (Metadata:
     Read-only is added automatically). No other permission.
   - Create the environment and store the token:

     ```bash
     gh api -X PUT repos/Ogstra/proxor/environments/homebrew-publication
     gh secret set HOMEBREW_TAP_TOKEN --env homebrew-publication -R Ogstra/proxor
     ```

   - Optional: add a required reviewer to `homebrew-publication` in Settings -> Environments if
     the bump should wait for approval; without one the bump runs as soon as the release is
     published.
   - Rotation: generate a new token the same way, run the `gh secret set` line again, then
     re-run the bump with `channels=homebrew` for the latest release if a bump failed while the
     token was expired.

After that, publishing a version is one workflow run, and changing what gets published is a
commit to `packaging/arch/PKGBUILD.in`, `packaging/winget/templates` or
`packaging/homebrew/proxor.rb.in`.
