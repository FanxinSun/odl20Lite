# The manifest — format and rules

**Plan L0 step 3.** Every external input to this tree is declared here with a URL, a SHA-256
and a licence note, and is fetched from origin into a cache. Nothing enters the tree
undeclared.

The manifest covers **code and data alike**. That is not an embellishment of the plan's wording
but a requirement of it: L0 step 5 says NOTICE is generated *from this manifest*, and a NOTICE
that omitted the dependencies would be the thing it exists not to be. One manifest, one fetcher,
one cache, one licence register.

## Why the identity of an input is its hash

Rule R11 exists because the IERS revises EOP **retroactively at an unchanged URL** — three
documented instances (2025-06-05, 2026-02-05, 2026-03-09), two of which rewrote historical
values. A URL is therefore not an identity and a version number is not an identity. The hash is.

The same reasoning applies to code, which is why this tree pins dependencies by URL and SHA-256
rather than by a version range resolved at build time. A resolver's job is to choose a version
for you; this tree's requirement is that nothing chooses a version for you.

## Format

JSON, schema 1. JSON specifically so that CMake can read it with `string(JSON ...)` and no
parser dependency — a manifest format needing a third-party parser would mean the tool enforcing
"nothing enters undeclared" had itself entered undeclared. JSON has no comments, so notes are
`note` fields, which is better anyway: a note in a field reaches the generated NOTICE, a comment
does not.

```json
{
  "schema": 1,
  "cache": "data/cache",
  "entries": [ { … } ]
}
```

### Entry fields

| field | required | meaning |
|---|---|---|
| `id` | always | unique; names the cache subdirectory and the CMake dependency |
| `kind` | always | `code` (linked or compiled in), `data` (an input to a computation), `tool` (used at build time, ships in nothing) |
| `licence` | always | SPDX identifier. Checked against plan §5 constraint 3 by `fetch.py check-licences`. |
| `licence_note` | expected | why this licence is acceptable, and the multi-licence option chosen where there is one (plan §3.11 point 4) |
| `role` | expected | what it is for, in a few words |
| `url`, `filename`, `sha256` | unless `provided_by_host` | the pin. `sha256` is 64 lowercase hex characters. |
| `version` | expected | for humans and for NOTICE; **never** used to select anything |
| `provided_by_host` | optional | `true` for a tool expected on the build machine. No URL or hash; still needs a licence, because it is still an external input. |
| `unpack`, `unpacked_root` | for archives | how the artefact is opened and what the top-level directory inside is called |
| `licence_file` | for code | path *within* the archive to the licence text, so NOTICE generation can quote rather than paraphrase |
| `upstream_mutable` | for data | `true` where upstream is known to revise in place. Such an entry SHOULD also carry `archived_url` pinning an immutable snapshot the publisher maintains. |
| `archived_url` | for mutable data | the publisher's own archive path. The IERS maintains superseded EOP series at stable paths, so pinning records a URL and redistributes nothing. |
| `retrieved` | on fetch | the date the pin was established |

### A worked entry for mutable data

No data entry exists yet — L1 is the layer that introduces one, and it does not open until L0's
exit gate passes. The shape it will take, recorded here so the format is not being designed
twice:

```json
{
  "id": "eop-c04-20",
  "kind": "data",
  "role": "IERS EOP 20 C04, final Earth orientation series",
  "url": "https://hpiers.obspm.fr/iers/eop/eopc04_20_v3/eopc04.1962-now",
  "archived_url": "https://hpiers.obspm.fr/iers/eop/eopc04_20_v3/eopc04.1962-now",
  "filename": "eopc04.1962-now",
  "sha256": "…",
  "licence": "IERS-public",
  "licence_note": "IERS public product. Redistribution is not required: the pin is a URL the IERS itself maintains.",
  "upstream_mutable": true,
  "upstream_mutable_note": "The live series at .../eopc04/ is revised retroactively. Baselines and acceptance tests pin the archived path; operational runs may use the live series and record identity and hash in run provenance. Plan §3.11 point 3.",
  "retrieved": "…"
}
```

## The tool

`tools/fetch.py`, stdlib-only Python. A fetcher needing a package installed before it can fetch
anything is a bootstrapping regress.

| command | does |
|---|---|
| `verify` | **offline.** Is every entry cached and hash-correct? Never touches the network. |
| `fetch` | download what is missing, verify, stop |
| `fetch --refresh` | re-download everything and report upstream drift **without overwriting the cache** |
| `list [--json]` | the entries — the NOTICE generator's input |
| `path <id>` | the cache path of one entry — CMake's input |
| `check-licences` | plan §5 constraint 3: refuse GPL/LGPL/AGPL |

Exit codes are distinct because CI reads them: `0` ok, `1` missing from cache, `2` **hash
mismatch**, `3` manifest malformed, `4` network failure, `5` usage.

## What refusal means here

A hash mismatch is a **hard failure** and the message says, in as many words, that it is a fault
to investigate and not a hash to update. Updating a pin is a deliberate, reviewed, logged act
(plan §3.11 point 3). The one thing this whole step buys is that an input cannot change
underneath a result without somebody deciding that it should.

Two further consequences, both deliberate:

- **The build does not fetch.** `cmake -S . -B build` fails with the command that fixes it if
  the cache is not populated. A build that downloads is a build whose inputs depend on when it
  ran.
- **CMake re-checks the hash.** `FetchContent` is given `URL_HASH` as well, so the pin is
  asserted by two tools independently. One check of the single property the manifest exists to
  guarantee is a single point of failure.
