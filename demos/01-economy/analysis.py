# Offline exploration for 01-economy (not used by the demo binary).
# Structural checks (weak reversibility, deficiency, conservation laws, strong-endotactic
# witness, complex-balance residual) and SciPy reference integrations of both networks.
import itertools, sys
import numpy as np
from scipy.integrate import solve_ivp

SP = ["Ore", "Wood", "Tools", "Food", "Workers"]  # O W T F P
def C(**kw):
    v = [0]*5
    for k, n in kw.items(): v["OWTFP".index(k)] = n
    return tuple(v)

# (source, target, rate)
FULL = [
    # LC1 village
    (C(P=1, F=1), C(P=2), 1.0),       # eat + recruit
    (C(P=2), C(P=1), 0.35),            # crowding  (return)
    (C(P=1), C(P=1, F=1), 0.6),        # farm
    # LC2 forge
    (C(O=1, W=1), C(T=1), 1.2),        # craft tool
    (C(T=1), C(O=2), 0.25),            # melt tool (return)
    (C(O=2), C(O=1, W=1), 0.8),        # trade ore for wood
    # LC3 workshop
    (C(P=1, T=1, F=1), C(P=1, O=1, W=1), 0.9),  # salvage
    (C(P=1, O=1, W=1), C(P=2, T=1), 0.5),       # craft + recruit (return)
    (C(P=2, T=1), C(P=1, T=1, F=1), 0.7),       # retire to farm
]

def analyse(R, name):
    cx = sorted({r[0] for r in R} | {r[1] for r in R})
    idx = {c: i for i, c in enumerate(cx)}
    n = len(cx)
    adj = [[False]*n for _ in range(n)]
    for s, t, _ in R: adj[idx[s]][idx[t]] = True
    reach = [row[:] for row in adj]
    for k in range(n):
        for i in range(n):
            if reach[i][k]:
                for j in range(n):
                    if reach[k][j]: reach[i][j] = True
    wr = all(reach[idx[t]][idx[s]] for s, t, _ in R)
    # linkage classes
    par = list(range(n))
    def f(a):
        while par[a] != a: a = par[a]
        return a
    for s, t, _ in R: par[f(idx[s])] = f(idx[t])
    l = len({f(i) for i in range(n)})
    V = np.array([np.subtract(t, s) for s, t, _ in R], float)
    s = np.linalg.matrix_rank(V)
    _, sv, vt = np.linalg.svd(V)
    Z = vt[s:]
    print(f"[{name}] complexes={n} linkage={l} dimS={s} deficiency={n-l-s} WR={wr}")
    print("  S-perp basis:", np.round(Z, 3))
    return cx, idx, wr

def rhs(R):
    Y = np.array([r[0] for r in R], float); V = np.array([np.subtract(r[1], r[0]) for r in R], float)
    k = np.array([r[2] for r in R])
    def f(t, x):
        x = np.maximum(x, 0)
        m = k*np.prod(x[None, :]**Y, axis=1)
        return m @ V
    return f

def witness(R):
    srcs = {r[0] for r in R}
    V = np.array([np.subtract(t, s) for s, t, _ in R], float)
    for w in itertools.product([-1, 0, 1, 2], repeat=5):
        w = np.array(w)
        if not np.any(np.abs(V @ w) > 0): continue
        top = max(np.dot(w, y) for y in srcs)
        topR = [r for r in R if np.dot(w, r[0]) == top]
        if all(np.dot(w, np.subtract(r[1], r[0])) >= 0 for r in topR):
            return w
    return None

if __name__ == "__main__":
    analyse(FULL, "full")
    print("  non-strongly-endotactic witness w:", witness(FULL))
    drop = [int(a) for a in sys.argv[1:]] or [1, 4]
    LEFT = [r for i, r in enumerate(FULL) if i not in drop]
    analyse(LEFT, "left")
    starts = [np.array(v, float) for v in [[1, 1, 1, 1, 1], [5, 0.05, 0.3, 0.01, 20], [0.02, 3, 0.1, 8, 0.05], [0.5, 0.5, 2, 0.001, 0.002]]]
    for nm, R in [("full", FULL), ("left", LEFT)]:
        f = rhs(R)
        for x0 in starts:
            sol = solve_ivp(f, [0, 400], x0, method="LSODA", rtol=1e-9, atol=1e-14, dense_output=True,
                            events=lambda t, x: np.max(x) - 1e9)
            ts = [1, 10, 50, 100, 200, 400]
            out = []
            for T in ts:
                if T <= sol.t[-1]: out.append(" ".join(f"{v:9.3g}" for v in sol.sol(T)))
            print(nm, x0, "\n   " + "\n   ".join(out))
    # complex-balance residual at the WR equilibrium
    f = rhs(FULL)
    sol = solve_ivp(f, [0, 3000], np.ones(5), method="LSODA", rtol=1e-11, atol=1e-14)
    xe = sol.y[:, -1]
    cx, idx, _ = analyse(FULL, "eq")
    net = np.zeros(len(cx))
    for s, t, k in FULL:
        fl = k*np.prod(xe**np.array(s, float)); net[idx[s]] -= fl; net[idx[t]] += fl
    print("equilibrium", xe, "|f|", np.abs(f(0, xe)).max(), "complex-balance residual", np.abs(net).max())
