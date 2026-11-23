# Riff Racer Customs agent instructions

Riff Racer Customs is an authoring tool for Riff Racer, separate from the
ReRiff client and server packages. Read and follow the ReRiff agent
instructions from the current ReRiff checkout
([Aftonstjarna/ReRiff](https://github.com/Aftonstjarna/ReRiff)) when it is
available. The applicable rules are mirrored below with Customs-specific notes;
ReRiff-only rules are listed at the end.

## Extending the original game

- Prefer the game's existing native solution. Inspect its original functions,
  controls and behavior before adding a replacement implementation.
- Copy, reuse or extend a suitable original game function instead of creating
  a separate system for the same job. Preserve its native input handling,
  navigation, repeat behavior, artwork and lifecycle where applicable.
- Add a new implementation only when no suitable original solution exists;
  document that gap and keep the integration consistent with the game.
- For every new menu or menu addition, retain native spacing and control
  styling. Check the available space at supported resolutions and UI scales;
  reuse the native scrolling solution when entries overflow. Do not compress
  entries or allow controls to crowd, overlap, or become unreachable.
- Consult the RiffRacerDecomp recovery project for reverse-engineering work and
  contribute verified new findings there. Preserve original hashes, exact bytes,
  metadata tokens and provenance; keep intentional ReRiff patches separate from
  byte-for-byte original reconstruction and do not claim matching without comparison.

## Original-file reference

- Use ReRiff's recorded unmodified Windows game reference for file hashes and
  byte comparisons. `packaging/vanilla-reference.json` records the executable
  and DLL SHA-256 hashes, sizes and Steam depot/build/manifest identifiers.
  Compare working copies and backups against this baseline before treating
  them as original game files.
- Compare exported packages against the stock archive and the selected donor.
  Never modify the game installation or stock archives to make an export work;
  keep authoring changes in the exported mod folder or working copy.

## Changelog review before publication

- Before publishing any release or prerelease, prepare its complete proposed
  changelog and present it to the user for review. Allow the user to reword it
  and remove redundant or irrelevant information; wait for explicit approval
  of the final wording before publication.
- A request to generate or prepare a release does not waive this review. Builds,
  validation and a GitHub draft may be prepared beforehand, but do not trigger
  an automatic publishing workflow or update public release channels before
  approval. Use draft-safe workflow inputs and avoid publication-triggering pushes.
- Keep the approved changelog consistent in `RELEASE-NOTES.md`, the packaged
  release notes and GitHub release body. Request review again if its wording
  materially changes after approval.

## In-game release summaries

- When preparing any release or prerelease, update `RELEASE-NOTES.md` with
  a version-specific entry before creating its release commit and tag. Include
  the shipped changes, validation and known limitations; preserve prior entries
  and consolidate redundant wording without dropping distinct changes.
- Use that entry for the corresponding GitHub release notes so the repository
  and published changelog agree. Do not leave published versions marked unreleased.
- For each new release, add a `## In-game summary` section to its release notes,
  followed by `## Full release notes` for the detailed changelog.
- Summarize the most important player-facing changes in at most three complete
  bullet sentences, each at most 72 characters (excluding the bullet prefix).
  Write concise summaries rather than cutting detailed sentences mid-thought.
- Keep the full changelog on GitHub. Native News displays the short summary and
  uses its existing Visit URL action to open that exact release's full notes.

## Release naming and channels

- New prereleases use `X.X.X-beta.N` (starting at beta.1), followed by stable
  `X.X.X`. Remind the user and suggest the beta spelling if they request the
  old `X.X.X-N` style. Keep legacy versions readable for updater compatibility.
- Stable remains GitHub Latest and the default download/update channel. Publish
  betas as prereleases with `--latest=false`; experimental updates require opt-in.
- Do not delete or draft releases automatically. Superseded betas may be removed
  when requested; preserve Git tags for the tested commits.

## Customs build and validation

- Build with the pinned container and run `make -C LiveryMaker OUT=... all test`.
- Run the relevant GUI smokes for UI changes: `--smoke-unified`,
  `--smoke-mod-attachments` and `--smoke-model-import` with the Impala GLB.
- For importer or package changes, run the owner test with
  `test-model-import` and a temporary output folder; never write test output
  into the game's `mods` directory.
- For a release, download the CI artifacts, verify their checksums, and test the
  updater from the previous stable and from the current beta before publishing.
- Keep `rr-customs/main` and `rr-customs/experimental` synchronized after release
  commits, and update `dist/` from the downloaded CI artifacts.

## ReRiff-only rules

These sections of the ReRiff instructions do not apply to Riff Racer Customs:

- **Upstream server restoration after stable releases.** Do not open
  `riffapi-local-extended` PRs for Customs changes.
- **ReRiff research submissions and ReRiff-managed file repair.** Those flows
  belong to ReRiff; research findings from Customs work still go through
  ReRiff's submission flow rather than being written into the decomp checkout.
