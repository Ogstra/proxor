# Release Notes Format

Proxor renders GitHub release bodies inside the in-app changelog window
(`DialogUpdateAvailable`). That window does **not** use a real Markdown library — it uses a
hand-rolled subset parser, `mdToHtml` in `src/ui/dialog_update_available.cpp`, feeding
`QTextBrowser::setHtml`.

So "it looks right on GitHub" is not the test. The test is what `mdToHtml` emits.

## The rule that decides everything: which parser will render this?

The changelog window that announces "version X is available" belongs to the version the
user is **currently running**, not the one being announced.

Notes for the next release are therefore rendered by the parser shipped in the *previous*
release. A parser improvement never helps the very next release — it helps the one after.

That splits authoring into two tiers.

## Tier 1 — legacy-safe (use this for any release whose audience may run <= 1.6.3)

Version 1.6.3 and earlier shipped a parser whose list regex was anchored at column 0
(`^[\-\*] (.*)`). Anything outside this subset renders wrong for those users.

Safe to use:

- `#`, `##`, `###`, `####` headings
- Unordered lists with `- ` or `* ` **starting at column 0, with no leading spaces**
- Ordered lists with `1. ` at column 0
- `**bold**`, `*italic*`, `` `code` ``, `[text](url)`
- Fenced code blocks with triple backticks
- `---` horizontal rules

Must avoid:

- **Leading spaces before a list marker.** This is the big one. An indented bullet did not
  match the old regex, so it broke out of the list and rendered as a paragraph with a
  literal `*` or `-` visible. Every bullet in the published v1.6.2 and v1.6.3 notes has two
  leading spaces and renders this way on shipped clients.
- **Nested lists.** Same cause.
- **Wrapped paragraphs.** Every source line becomes its own `<p>`, so a sentence wrapped
  across three lines renders as three separate paragraphs with gaps. Keep each paragraph
  and each bullet on **one line**, however long.
- **Blockquotes.** `> text` rendered literally, showing the `>`.
- `_italic_` and `__bold__`. Only the asterisk forms were supported.
- **A lone asterisk.** `*.zip and *.msi` italicised everything between the two markers.
  Write such tokens inside backticks.
- **Tables.** Never supported; they render as literal pipe characters.

## Tier 2 — after the tolerant parser ships

From the release *after* the parser fix, these also work:

- List markers indented by any amount, including `+` as a marker
- Nested lists, to any depth, mixing `ul` and `ol`
- Blockquotes with `> `
- `_italic_` and `__bold__`
- A lone `*` or `_` left inert — emphasis now requires a non-space character on both edges
- Markers inside backticks left alone — code spans are extracted before emphasis runs
- CRLF bodies, which the GitHub API returns, parsed identically to LF

Still unsupported, deliberately:

- **Tables.** Documented as out of scope rather than implemented; the notes have never
  used one.
- **Bare-URL autolinking.** Write explicit `[text](url)` links.
- **Setext headings** (`===` or `---` underlines). A `---` line is parsed as a horizontal
  rule, so an underlined heading renders as text followed by a rule.
- **Hard line breaks** inside a paragraph. The one-line-per-paragraph rule from Tier 1
  still applies.

## House style

Match the existing releases:

- Start with a single `#` title naming the theme, not the version number — the version is
  already shown in its own field in the dialog. For example `# Routing, latency testing &
  Windows fixes`.
- Group with `## Added`, `## Changed`, `## Fixed`, in that order, omitting empty ones.
- One bullet per user-visible change, written from the user's point of view — what changed
  for them, not which function was edited.
- Prefix platform-specific entries with `Windows: ` or `Linux: `. Do not prefix an entry
  whose cause is cross-platform, even if it was only reported on one OS.

## Checking before you publish

`mdToHtml` has no test harness. The cheap check is to paste the body into a scratch release
on a throwaway tag, open the changelog window, and look. At minimum, re-read the draft
against the Tier 1 list above — the two failure modes that have actually bitten are
indented bullets and wrapped paragraphs.
