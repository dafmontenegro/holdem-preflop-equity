# holdem-preflop-equity

Exact and estimated preflop probabilities for Texas Hold'em No Limit: the potential of all 169
starting hands, equity against 1 to 8 opponents, the head-to-head 169 × 169 matrix, and the all-in
decision. Every number published on
[montenegrodanielfelipe.com](https://montenegrodanielfelipe.com/) comes from this engine and is
reproducible with a single command.

Work in progress. The interactive trainer built on this data lives on the website.

## Layout

| Path | What it holds |
| --- | --- |
| `exploration/` | The first exploration phase, kept for provenance: validated results, the C programs that produced them, the Spanish reports and the trainer prototype. See `exploration/README.md`. |

## Reproducing

Nothing to build yet. The engine, its tests and the `make` targets land next.

To install the development dependencies (needed only to cross-check results and rebuild the
reports): `pip install -r requirements-dev.txt`.
