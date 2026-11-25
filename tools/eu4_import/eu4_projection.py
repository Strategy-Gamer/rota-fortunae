"""Local estimate of the EU4 map's (inconsistent) projection from anchor places."""
import numpy as np
W = 5632
PX_PER_DEG = 15.5  # rough global longitude scale, only used to unwrap longitudes near the query

def wrap180(d):
    return (d + 180) % 360 - 180

def local_fit(q, P, LL, sigma):
    """Affine fit of (lat, lon) around pixel q, weighting anchors by a Gaussian of pixel distance.
    Returns [[lat, lon], [dlat/dx, dlon/dx], [dlat/dy, dlon/dy]] with lon relative to a guess at q."""
    dx = (P[:, 0] - q[0] + W / 2) % W - W / 2
    dy = P[:, 1] - q[1]
    w = np.sqrt(np.exp(-(dx ** 2 + dy ** 2) / sigma ** 2) + 1e-12)
    guess = (q[0] - 2805) / PX_PER_DEG
    lon_rel = wrap180(LL[:, 1] - guess)
    A = np.vstack([np.ones_like(dx), dx, dy]).T * w[:, None]
    sol = np.linalg.lstsq(A, np.column_stack([LL[:, 0], lon_rel]) * w[:, None], rcond=None)[0]
    sol[0, 1] = wrap180(sol[0, 1] + guess)
    return sol

def metres_per_pixel(sol):
    """Ground distance of one pixel step in x and in y at the fitted point."""
    lat = np.radians(sol[0, 0])
    def step(dlat, dlon):
        return 111_195 * np.hypot(dlat, dlon * np.cos(lat))
    return step(sol[1, 0], sol[1, 1]), step(sol[2, 0], sol[2, 1])
