"""
plot_tools.py -- normalized diagnostic plots for the secular-cycle sim.

Each function takes RAW arrays straight from the sim (like plot_simulation) and normalizes to 0-1 so every
curve shares one axis (1 = each series' own max, or plotted directly if already a 0-1 fraction). Pass
phase=<phase array> to shade prosperity/strain/fracture behind the curves. Pass extra={"label": array, ...}
to drop in more series without editing these functions (auto: 0-1 arrays plotted as-is, raw ones scaled /max).

    from plot_tools import plot_overview, plot_population, plot_elites, plot_state, plot_unrest
    plot_population(P_hist, pop_hist, cc_hist, wage_share_hist, immis_hist, phase=phase_hist)
"""
import numpy as np
import matplotlib.pyplot as plt

_PHASE_COLORS = {0: "green", 1: "yellow", 2: "red"}   # prosperity / strain / fracture


def _shade(phase):
    if phase is None:
        return
    start = 0
    for x in range(len(phase) - 1):
        if phase[x] != phase[x + 1]:
            plt.axvspan(start, x, color=_PHASE_COLORS.get(phase[x], "gray"), alpha=0.08)
            start = x + 1
    plt.axvspan(start, len(phase), color=_PHASE_COLORS.get(phase[-1], "gray"), alpha=0.08)


def _norm(a, ref=None):
    # scale to 0-1 by `ref` (or the series' own max); non-negative series only
    a = np.asarray(a, dtype=float)
    m = np.nanmax(a) if ref is None else ref
    return a / m if m else np.zeros_like(a)


def _auto(a):
    # already a 0-1 fraction -> as-is; otherwise scale by its own max
    a = np.asarray(a, dtype=float)
    return a if (np.nanmax(a) <= 1.0001 and np.nanmin(a) >= -1e-9) else _norm(a)


def _frame(title):
    plt.figure(figsize=(24, 6))
    plt.yticks([i / 10 for i in range(11)])
    plt.ylim(-0.02, 1.05)
    plt.title(title)


def _finish(phase, extra):
    if extra:
        for label, arr in extra.items():
            plt.plot(_auto(arr), label=label, linestyle=":")
    _shade(phase)
    plt.legend(loc="upper right", ncol=2)
    plt.show()


def plot_overview(P, E, U, S, phase=None, extra=None):
    """The 4 gauges together (matches plot_simulation)."""
    _frame("Secular cycle -- overview")
    plt.plot(P, label="Population (P)", color="blue")
    plt.plot(E, label="Elite overproduction (E)", color="orange")
    plt.plot(U, label="Instability (U)", color="red")
    plt.plot(S, label="State capacity (S)", color="green")
    _finish(phase, extra)


def plot_population(P, population, carrying_capacity, wage_share, immiseration, phase=None, extra=None):
    """P gauge, raw population & K on a shared scale, wage share and immiseration (already fractions)."""
    _frame("Population")
    ref = max(np.nanmax(population), np.nanmax(carrying_capacity)) or 1.0   # pop & K share one scale
    plt.plot(P, label="P (pressure gauge)", color="navy")
    plt.plot(_norm(population, ref), label="Population (raw)", color="blue")
    plt.plot(_norm(carrying_capacity, ref), label="Carrying capacity", color="green", linestyle="--")
    plt.plot(wage_share, label="Wage share", color="orange")
    plt.plot(immiseration, label="Immiseration", color="red")
    _finish(phase, extra)


def plot_elites(E, elites, positions, reproduction, up_mobility, down_mobility, deaths, phase=None, extra=None):
    """E gauge, raw elites & positions on the elite scale, the four flows on a shared flow scale."""
    _frame("Elites")
    e_ref = np.nanmax(elites) or 1.0                                        # elites & positions share this
    flow_ref = max(np.nanmax(reproduction), np.nanmax(up_mobility),
                   np.nanmax(down_mobility), np.nanmax(deaths)) or 1.0      # the four flows share this
    plt.plot(E, label="E (overproduction gauge)", color="darkorange")
    plt.plot(_norm(elites, e_ref), label="Elites (raw)", color="orange")
    plt.plot(_norm(positions, e_ref), label="Positions (raw)", color="saddlebrown", linestyle="--")
    plt.plot(_norm(reproduction, flow_ref), label="Reproduction", color="green")
    plt.plot(_norm(up_mobility, flow_ref), label="Upward mobility", color="blue")
    plt.plot(_norm(down_mobility, flow_ref), label="Downward mobility", color="purple")
    plt.plot(_norm(deaths, flow_ref), label="Elite deaths", color="red")
    _finish(phase, extra)


def plot_state(legitimacy, revenue, expenses, suppression, treasury=None, phase=None, extra=None):
    """Legitimacy (fraction); revenue, expenses & (optional) treasury on a shared fiscal scale; suppression on its own."""
    _frame("State capacity")
    fisc = [np.nanmax(revenue), np.nanmax(expenses)] + ([np.nanmax(treasury)] if treasury is not None else [])
    fisc_ref = max(fisc) or 1.0                                             # revenue, expenses, treasury share this
    plt.plot(legitimacy, label="Legitimacy", color="green")
    plt.plot(_norm(revenue, fisc_ref), label="Revenue", color="blue")
    plt.plot(_norm(expenses, fisc_ref), label="Expenses", color="red")
    if treasury is not None:
        plt.plot(_norm(treasury, fisc_ref), label="Treasury", color="teal", linestyle="--")
    plt.plot(_norm(suppression), label="Suppression (coercive reach)", color="purple", linestyle="--")
    _finish(phase, extra)


def plot_unrest(U, U_e, suppression=None, phase=None, extra=None):
    """Raw unrest stock U vs effective U_e (both fractions); optional suppression = the coercive reach subtracted.
    Drivers are model-specific and still in flux -- pass them via extra={...}."""
    _frame("Unrest")
    plt.plot(U, label="U (raw stock)", color="darkred")
    plt.plot(U_e, label="U_e (after suppression)", color="red")
    if suppression is not None:
        plt.plot(_norm(suppression), label="Suppression (/max)", color="purple", linestyle="--")
    _finish(phase, extra)


_CRISIS_COLORS = {"famine": "saddlebrown", "epidemic": "olive", "revolt": "orange",
                  "civil_war": "red", "coup": "purple", "bankruptcy": "black"}


def plot_crises(crisis_history, immis=None, phase=None, extra=None):
    """Each crisis's intensity band (0-1, sim.crisis_history dict) + optional immiseration. phase= shades phases."""
    _frame("Crises / events")
    for name, arr in crisis_history.items():
        plt.plot(arr, label=name, color=_CRISIS_COLORS.get(name))
    if immis is not None:
        plt.plot(immis, label="Immiseration", color="gray", linestyle="--")
    _finish(phase, extra)
