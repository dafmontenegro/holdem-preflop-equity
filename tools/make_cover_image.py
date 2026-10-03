"""Draws the share image for the project page: the 13 x 13 grid of all 169 hands.

This is a cover, not a chart to be read: there is no hover, no table view and
no way to look a number up, so it carries no axis values and makes no claim
the page does not make properly elsewhere. What it has to do is be honest
about its own encoding and recognisable as the thing the page is about.

Encoding. Equity against one opponent holding a random hand — exact, from the
head-to-head matrix — on a **single-hue sequential ramp**, light to dark, which
is the only correct choice for a continuous magnitude. No rainbow, no
red-to-green: a multi-hue ramp would invent categories the data does not have,
and a red-green one would be unreadable to a tenth of the men who look at it.
The ramp is the validated blue scale, and the surface is the site's own dark
slate, so the image sits on the page it belongs to.

Layout. The grid is the one every poker player already knows how to read:
rows and columns run from the ace down to the deuce, pairs on the diagonal,
same-suit hands above it and different-suit hands below. That convention is
worth more than any arrangement sorted by strength, because a reader who knows
it can find a hand without being taught.
"""

import json
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap

REPO = Path(__file__).resolve().parent.parent
WEB = REPO / "web"

RANKS = "AKQJT98765432"

# The site's dark surface, and the ink that reads on it.
SURFACE = "#1e293b"
INK = "#f8fafc"
INK_MUTED = "#94a3b8"

# The single-hue sequential ramp, weakest first. This is a *sequential*
# encoding of a continuous quantity, not an ordinal one, so it is allowed the
# full range of the scale: the end nearest the surface recedes into it, which
# is exactly what "this hand is worth almost nothing" should look like. An
# ordinal ramp — discrete, ordered tiers — would have to hold its near-surface
# end clear of the background instead, and the page's banded view does.
#
# Every step is a named step of the reference blue scale, not interpolated by
# hand, so the ramp is reproducible rather than eyeballed.
RAMP = ["#104281", "#184f95", "#1c5cab", "#256abf", "#2a78d6", "#3987e5",
        "#5598e7", "#6da7ec", "#86b6ef", "#9ec5f4", "#b7d3f6", "#cde2fb"]


def grid_position(hand):
    """Where a hand sits in the conventional 13 x 13 grid: (row, column).

    Pairs on the diagonal, same-suit hands above it, different-suit below.
    """
    high, low = RANKS.index(hand[0]), RANKS.index(hand[1])

    if len(hand) == 2:
        return high, high
    if hand[2] == "s":
        return min(high, low), max(high, low)
    return max(high, low), min(high, low)


def main():
    hands_path = WEB / "hands.json"
    if not hands_path.exists():
        print("Nothing to draw: web/hands.json not built. Run `make web` first.")
        return 1

    data = json.loads(hands_path.read_text())
    scale = data["meta"]["scale"]
    equity = {record["hand"]: record["equityVsRandom"] / scale
              for record in data["hands"]}

    colours = LinearSegmentedColormap.from_list("preflop", RAMP)
    lowest, highest = min(equity.values()), max(equity.values())

    # 1200 x 900 at 100 dpi: 4:3, which is what the site's list thumbnails crop
    # from, so the cover shows whole rather than cropped.
    # The axes is made to fill the whole figure. Left at its default it sits
    # inside matplotlib's margins, and then every fraction below would be a
    # fraction of that inset box rather than of the canvas, which quietly makes
    # the geometry wrong while still producing a picture.
    figure = plt.figure(figsize=(12.0, 9.0), dpi=100)
    axes = figure.add_axes([0.0, 0.0, 1.0, 1.0])
    figure.patch.set_facecolor(SURFACE)
    axes.set_facecolor(SURFACE)
    axes.set_axis_off()

    # Geometry. Axes fractions are fractions of each dimension, and the canvas
    # is 4:3, so a cell that is square on screen needs a different fraction of
    # the width than of the height. Getting this wrong is what makes a grid of
    # squares come out as a grid of rectangles.
    width_inches, height_inches = 12.0, 9.0
    title_band = 1.6        # inches reserved above the grid, for the two lines
    bottom_margin = 0.45    # inches of surface below it, so nothing touches the edge

    side = min(height_inches - title_band - bottom_margin, width_inches) / 13
    gap = 0.035             # inches of surface between cells, not a border

    # Fractions of each dimension, derived from one measurement in inches, so
    # the cells and the gaps between them are square on screen rather than
    # square in the coordinate system.
    cell_w, cell_h = side / width_inches, side / height_inches
    gap_w, gap_h = gap / width_inches, gap / height_inches
    left = 0.5 - cell_w * 13 / 2
    bottom = bottom_margin / height_inches

    for hand, value in equity.items():
        row, column = grid_position(hand)
        shade = (value - lowest) / (highest - lowest)

        axes.add_patch(plt.Rectangle(
            (left + column * cell_w + gap_w / 2,
             bottom + (12 - row) * cell_h + gap_h / 2),
            cell_w - gap_w, cell_h - gap_h,
            facecolor=colours(shade), edgecolor="none",
            transform=axes.transAxes, clip_on=False))

        # The label is identity, not a value: it says which hand the cell is,
        # and the number it stands for is nowhere on this image on purpose.
        # Ink flips to dark on the pale end of the ramp so it stays readable.
        axes.text(left + column * cell_w + cell_w / 2,
                  bottom + (12 - row) * cell_h + cell_h / 2,
                  hand, transform=axes.transAxes,
                  ha="center", va="center",
                  fontsize=12 if len(hand) == 2 else 10.5,
                  color="#0f172a" if shade > 0.52 else INK,
                  fontweight="bold" if len(hand) == 2 else "normal")

    axes.text(0.5, 0.945, "The 169 starting hands of Texas Hold'em",
              transform=axes.transAxes, ha="center", va="center",
              fontsize=29, color=INK, fontweight="semibold")
    axes.text(0.5, 0.872,
              "Shaded by exact equity against one random hand  ·  "
              f"{lowest * 100:.1f}% to {highest * 100:.1f}%",
              transform=axes.transAxes, ha="center", va="center",
              fontsize=15.5, color=INK_MUTED)

    destination = WEB / "cover.png"
    figure.savefig(destination, facecolor=SURFACE, pad_inches=0)
    plt.close(figure)

    print(f"Wrote {destination.relative_to(REPO)} "
          f"({destination.stat().st_size / 1024:.0f} KB, "
          f"shading {lowest * 100:.2f}% to {highest * 100:.2f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
