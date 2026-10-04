"""Draws the share image for the project page.

WHAT A COVER HAS TO DO, AND WHAT IT DOES NOT
--------------------------------------------
The first version of this was the 13 x 13 grid of all 169 hands, shaded by
equity. As a chart it was honest; as a cover it failed, and the failure is
worth writing down because it is easy to repeat.

A cover on this site is seen in three places, and the smallest one decides
everything. In the projects list it is about 215 pixels wide, beside
photographs of a gallery and a pixel-art racetrack. At that size 169 labelled
cells are not a grid, they are grey noise, and a flat data table sitting next
to photographs looks like a spreadsheet somebody pasted in by mistake.

So this draws a picture instead: two aces, large, on the site's own dark
slate. It reads instantly at any size, it says "poker" before a word is read,
and the thing it depicts is the best hand in the deck, which the page then
spends a section explaining is beaten by nothing at all.

The grid is still there, behind the cards, at low contrast. Up close it is the
real shading of the real data, which is what the page is about; at thumbnail
size it is texture. It earns its place by being true rather than by being
legible.

No number is printed. A cover cannot be checked, hovered or read aloud, so it
makes no claim that the page does not make properly, with its method attached.
"""

import json
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap
from matplotlib.patches import FancyBboxPatch
from matplotlib.transforms import Affine2D

REPO = Path(__file__).resolve().parent.parent
WEB = REPO / "web"

RANKS = "AKQJT98765432"

# The site's own dark surface and ink, so the cover sits on the page it belongs
# to rather than on top of it.
SURFACE = "#1e293b"
CARD_FACE = "#f8fafc"
CARD_EDGE = "#cbd5e1"
BLACK_SUIT = "#0f172a"
RED_SUIT = "#dc2626"

# The single-hue sequential ramp behind the cards, weakest first. Named steps
# of the reference blue scale, not interpolated by hand.
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


def draw_background_grid(axes, equity, colours, geometry):
    """The real data, at low contrast, as texture behind the cards."""
    lowest, highest = min(equity.values()), max(equity.values())
    left, bottom, cell_w, cell_h, gap_w, gap_h = geometry

    for hand, value in equity.items():
        row, column = grid_position(hand)
        fraction = (value - lowest) / (highest - lowest)

        axes.add_patch(plt.Rectangle(
            (left + column * cell_w + gap_w / 2,
             bottom + (12 - row) * cell_h + gap_h / 2),
            cell_w - gap_w, cell_h - gap_h,
            facecolor=colours(fraction), edgecolor="none", alpha=0.30,
            transform=axes.transAxes, clip_on=False, zorder=1))


def draw_card(axes, centre_x, centre_y, width, height, angle, rank, suit, red):
    """One playing card: a rounded white face, rotated, with its rank and pip.

    Rotation is applied as a transform on top of the axes transform, so the
    card is composed in plain axes coordinates and tilted afterwards rather
    than every corner being worked out by hand.
    """
    rotation = (Affine2D().rotate_deg_around(centre_x, centre_y, angle)
                + axes.transAxes)
    colour = RED_SUIT if red else BLACK_SUIT

    axes.add_patch(FancyBboxPatch(
        (centre_x - width / 2, centre_y - height / 2), width, height,
        boxstyle="round,pad=0,rounding_size=0.018",
        facecolor=CARD_FACE, edgecolor=CARD_EDGE, linewidth=2,
        transform=rotation, clip_on=False, zorder=3))

    # The index in the top-left corner, as on a real card, and the large pip
    # in the middle. The bottom-right index is left off on purpose: at
    # thumbnail size it is invisible, and leaving it out keeps the face clean.
    axes.text(centre_x - width / 2 + 0.030, centre_y + height / 2 - 0.045,
              rank, transform=rotation, ha="center", va="center",
              fontsize=34, fontweight="bold", color=colour, zorder=4)
    axes.text(centre_x - width / 2 + 0.030, centre_y + height / 2 - 0.098,
              suit, transform=rotation, ha="center", va="center",
              fontsize=26, color=colour, zorder=4)
    axes.text(centre_x, centre_y - 0.012, suit,
              transform=rotation, ha="center", va="center",
              fontsize=132, color=colour, zorder=4)


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

    # 1200 x 900 at 100 dpi: 4:3, which is the ratio the site's list thumbnails
    # crop from, so the cover shows whole rather than cropped.
    width_inches, height_inches = 12.0, 9.0
    figure = plt.figure(figsize=(width_inches, height_inches), dpi=100)

    # The axes fills the whole figure. Left at its default it sits inside
    # matplotlib's margins, and then every fraction below would be a fraction
    # of that inset box rather than of the canvas.
    axes = figure.add_axes([0.0, 0.0, 1.0, 1.0])
    figure.patch.set_facecolor(SURFACE)
    axes.set_facecolor(SURFACE)
    axes.set_axis_off()

    # Geometry for the background grid. Axes fractions are fractions of each
    # dimension and the canvas is 4:3, so a cell that is square on screen needs
    # a different fraction of the width than of the height.
    side = height_inches / 13 * 0.92
    cell_w, cell_h = side / width_inches, side / height_inches
    gap = 0.045
    geometry = (0.5 - cell_w * 13 / 2, 0.5 - cell_h * 13 / 2,
                cell_w, cell_h, gap / width_inches, gap / height_inches)

    draw_background_grid(axes, equity, colours, geometry)

    # Two aces, overlapping and tilted apart, as a hand would be held. Drawn
    # back to front so the near card overlaps the far one.
    draw_card(axes, 0.405, 0.50, 0.255, 0.455, 9, "A", "♥", red=True)
    draw_card(axes, 0.580, 0.47, 0.255, 0.455, -8, "A", "♠", red=False)

    destination = WEB / "cover.png"
    figure.savefig(destination, facecolor=SURFACE, pad_inches=0)
    plt.close(figure)

    print(f"Wrote {destination.relative_to(REPO)} "
          f"({destination.stat().st_size / 1024:.0f} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
