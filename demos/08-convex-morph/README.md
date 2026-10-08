# 08-convex-morph: volume-floored morph between symmetric convex crystals

**What you see.** Five generic origin-symmetric polyhedra serve as keyframes: tablets, a needle and random crystals with volumes from 0.8 to 1.5. They share no symmetry except x → −x. The video morphs through them in five 5-second segments, showing rotating glass bodies, endpoint thumbnails and a volume-vs-λ plot. The plot shows the guaranteed floor |K|^(1−λ)|L|^λ as a dashed white line.

Both panels build the in-between body the same way: they intersect the half-spaces u·x ≤ h(u) over the same 352–354 directions with the same clipper. The directions are every facet normal of K and L, both signs, plus 160 rotated Fibonacci pairs.

- **WITHOUT:** Minkowski interpolation, h = (1−λ)h_K + λh_L. It is the fairest baseline because Brunn–Minkowski already gives it a volume floor. Its weakness is that it bloats.
- **WITH:** the logarithmic Wulff body W[h_K^(1−λ) h_L^λ] from Theorem `thm:main`. Eq. `eq:main` guarantees its volume is at least |K|^(1−λ)|L|^λ.
- **Third curve (gray, plot only):** the naive game-dev morph, a radial vertex lerp on a shared sphere mesh. It has no guarantee.

**Paper:** Family 091, `preprints/The-logarithmic-Brunn-Minkowski-conjecture-September-23-2026/build/introduction.tex`, Theorem `thm:main`, eq. `eq:main` (with the Wulff-body definition just before it).

| metric (whole video) | WITHOUT (Minkowski) | WITH (log, eq:main) |
|---|---|---|
| min volume/floor for 0<λ<1 | 1.032 | 1.020 (never below 1) |
| max volume/floor (bloat) | 1.647 | 1.382 |
| body volume vs the other side | up to 1.253× larger | always ≤ 1× (contained) |
| compute, median ms/frame (single thread, `out/bench.json`) | 7.65 | 3.40 |

The naive radial lerp reaches a minimum of 0.795× the floor.

**Caveats**
- Both morphs sit above the floor, so the new math does not rescue a collapsing baseline here. What it adds is a guaranteed floor for the tighter morph: the log body is contained in the Minkowski body (AM–GM) and stays much closer to the geometric-mean volume path. The theorem is new for generic symmetric bodies in 3D; earlier proofs covered the plane and bodies with extra common symmetries.
- A sampled direction set gives a body that contains the continuum Wulff body, so the floor holds exactly in exact arithmetic. Floating-point clipping uses a tolerance of 1e-12, reported separately from the bound.
- Refining the directions at λ = ½ for keyframes 1→2 moves log/floor from 1.365 (M = 8) to 1.336 (M = 512), as stored in `refine_M*` in `results.json`. The decrease is not monotone because the Fibonacci sets are not nested.
- The 2.2× compute gap is real, but it is a side effect: fewer planes cut the smaller log body. It is not the point of the result.
- The theorem covers only origin-symmetric convex bodies. It does not cover nonconvex rooms, physics or collision.

Rebuild with `./build.sh 08-convex-morph run`. The bench numbers come from `demo.exe --novideo`.
