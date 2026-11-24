# Riff Racer Customs agent instructions

Riff Racer Customs is an authoring tool for Riff Racer, separate from the ReRiff
client and server packages. Read and follow the current ReRiff agent guide and
its `docs/agents/README.md` from the ReRiff repository
([Aftonstjarna/ReRiff](https://github.com/Aftonstjarna/ReRiff)) when available.
The applicable rules are mirrored below with Customs-specific notes; ReRiff-only
rules are listed at the end.

## Working rules

- Prefer the game's existing native solution. Inspect original functions,
  controls and behavior before adding a replacement, and preserve native
  input handling, navigation, repeat behavior, artwork and lifecycle.
- Add a new implementation only when no suitable original solution exists;
  document the gap and keep the integration consistent with the game.
- Retain native spacing, control styling and scrolling for new menus. Check
  supported resolutions and UI scales; never crowd, overlap or hide controls.
- Trace the affected execution path before editing. Reuse existing utilities and
  conventions, and update affected callers when an interface changes.
- Address demonstrated failures rather than speculative ones. Make the smallest
  complete change; keep unrelated refactors and formatting out.
- New abstractions or dependencies need a concrete consumer.
- Preserve user changes, unrelated files, settings and ownership records.
  Investigate unfamiliar files before deleting or overwriting them.
- Use the pinned container and repository scripts; do not modify global
  installs. Keep build output, backups, logs and downloaded tools out of the
  tracked tree, and check free space before large builds or captures.
- Comments explain constraints or non-obvious decisions, not task history.
- Run the checks that validate the change and report what actually ran. Do not
  substitute a caveat for a check that could have been run.

## Documentation routing

- Check the [ReRiff wiki](https://github.com/Aftonstjarna/ReRiff/wiki) before
  adding or expanding repository Markdown. User-facing Customs guides belong in
  the wiki; the editor reference belongs in `LiveryMaker/README.md` and install
  details in `README.md`.
- Do not add a new Markdown file for a feature when the wiki or an existing
  README can hold it. Update the matching section and link to authoritative
  code and configuration instead of duplicating them.
- Clone the wiki outside the source tree and update `_Sidebar.md` when adding a
  page. Push wiki changes only when the task authorizes publishing docs.
- Keep household domains, addresses, local paths, credentials and deployment
  configuration out of public commits, docs, screenshots and AGENTS.md files.
  Use repository, wiki or reserved example references instead.

## Original-file reference

- Use ReRiff's recorded unmodified Windows game reference for file hashes and
  byte comparisons. `packaging/vanilla-reference.json` records the executable
  and DLL SHA-256 hashes, sizes and Steam depot/build/manifest identifiers.
  Compare working copies and backups against this baseline before treating
  them as original game files.
- Compare exported packages against the stock archive and selected donor. Never
  modify the game installation or stock archives to make an export work; keep
  authoring changes in the exported mod folder or working copy.

## Changelog review before publication

- Stable releases require review of the complete proposed changelog and explicit
  approval of the final wording before publication. Material wording changes
  require renewed approval.
- Beta prereleases do not require changelog approval, as explicitly requested by
  the user on November 18, 2026. Publish only after release checks pass.
- A request to generate a release does not waive stable review. Builds,
  validation and a GitHub draft may be prepared beforehand, but do not publish
  or update public channels before approval.
- Keep `RELEASE-NOTES.md`, the packaged release notes and the GitHub release
  body consistent.

## In-game release summaries

- For each new release, add a `## In-game summary` section followed by
  `## Full release notes` for the detailed changelog. Include shipped changes,
  validation and known limitations; preserve prior entries.
- Summarize the most important player-facing changes in at most three complete
  bullet sentences, each at most 72 characters (excluding the bullet prefix).
- Keep the full changelog on GitHub. Native News displays the short summary and
  uses its existing Visit URL action to open that exact release's full notes.

## Release naming and channels

- New prereleases use `X.X.X-beta.N` (starting at beta.1), followed by stable
  `X.X.X`. Remind the user and suggest the beta spelling if they request the old
  `X.X.X-N` style. Keep legacy versions readable for updater compatibility.
- Stable remains GitHub Latest and the default download/update channel. Publish
  betas as prereleases with `--latest=false`; experimental updates require
  opt-in.
- Keep `rr-customs/main` on the latest stable release. Publish stable builds
  from `rr-customs/main` after fast-forwarding it to the tested stable commit.
  Do not publish a stable from a side branch.
- Do not delete or draft releases automatically. Superseded betas may be removed
  when requested; preserve Git tags for the tested commits.

## Build and validation

- Build with the pinned container and run `make -C LiveryMaker OUT=... all test`.
- Run the relevant GUI smokes for UI changes: `--smoke-unified`,
  `--smoke-mod-attachments` and `--smoke-model-import` with the Impala GLB.
- For importer or package changes, run `test-model-import` with a temporary
  output folder; never write test output into the game's `mods` directory.
- For a release, download and verify CI artifacts, then test the updater from
  the previous stable and current beta before publishing.
- Do not publish from an untested commit. Keep `dist/` in sync with the
  downloaded CI artifacts after a release.

## Contracts and publication

- Do not commit, push, tag, publish packages, move release channels or post
  GitHub comments unless asked.
- Use imperative commit messages without an extended body: `Add ...`,
  `Fix ...`, `Sync ...`.
- Preserve licensing and author attribution. Do not invent co-author credits.
- Ask the user to write pull request descriptions. Do not reply to reviewers
  yourself.
- Keep this file short; put task-specific detail in the wiki or the matching
  component README.

## ReRiff-only rules

These ReRiff sections do not apply to Riff Racer Customs:

- **Upstream server restoration after stable releases.** Do not open
  `riffapi-local-extended` pull requests for Customs changes.
- **ReRiff research submissions and ReRiff-managed file repair.** Those flows
  belong to ReRiff; research findings from Customs work still go through
  ReRiff's submission flow rather than being written into the decomp checkout.
