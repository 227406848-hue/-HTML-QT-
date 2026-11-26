function buildPlane() {
  const GREY = [0.74, 0.78, 0.84], SLATE = [0.40, 0.46, 0.60], PALE = [0.84, 0.86, 0.90];
  const RADOME = [0.25, 0.27, 0.36], DARK = [0.13, 0.11, 0.18], NOZZLE = [0.34, 0.29, 0.33];
  const RED = [0.92, 0.31, 0.16], WHITE = [0.95, 0.93, 0.88], GLASS = [0.99, 0.74, 0.27], GLOW = [1.0, 0.60, 0.24];
  const out = [];
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  // two-tone splinter camouflage on top, pale belly
  const camo = (n, c) => n[1] < -0.35 ? PALE : (Math.sin(c[2] * 0.62 + c[0] * 0.95) + Math.sin(c[2] * 0.27 - c[0] * 0.7 + 1.9) > 0.42 ? SLATE : GREY);

  function tri(a, b, c, col, mat, ctr) {
    let n = cross(sub(b, a), sub(c, a));
    const l = Math.hypot(n[0], n[1], n[2]);
    if (l < 1e-9) return;
    n = [n[0] / l, n[1] / l, n[2] / l];
    const g = [(a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3];
    if (n[0] * (g[0] - ctr[0]) + n[1] * (g[1] - ctr[1]) + n[2] * (g[2] - ctr[2]) < 0) n = [-n[0], -n[1], -n[2]];
    const cc = typeof col === 'function' ? col(n, g) : col;
    for (const p of [a, b, c]) out.push(p[0], p[1], p[2], n[0], n[1], n[2], cc[0], cc[1], cc[2], mat);
  }
  // rings: arrays of points, all the same length; cols: one colour (or fn) per segment
  function loft(rings, cols, mat = 0, caps = true) {
    const K = rings[0].length;
    for (let s = 0; s < rings.length - 1; s++) {
      const A = rings[s], B = rings[s + 1];
      const ctr = [0, 0, 0];
      for (const p of A.concat(B)) { ctr[0] += p[0]; ctr[1] += p[1]; ctr[2] += p[2]; }
      ctr[0] /= 2 * K; ctr[1] /= 2 * K; ctr[2] /= 2 * K;
      const col = Array.isArray(cols[0]) || typeof cols[0] === 'function' ? cols[Math.min(s, cols.length - 1)] : cols;
      for (let k = 0; k < K; k++) {
        const k2 = (k + 1) % K;
        tri(A[k], A[k2], B[k2], col, mat, ctr); tri(A[k], B[k2], B[k], col, mat, ctr);
      }
      if (caps && (s === 0 || s === rings.length - 2)) {
        const R = s === 0 ? A : B;
        const c = [0, 0, 0]; for (const p of R) { c[0] += p[0] / K; c[1] += p[1] / K; c[2] += p[2] / K; }
        if (s === 0) for (let k = 0; k < K; k++) tri(c, A[k], A[(k + 1) % K], col, mat, ctr);
        if (s === rings.length - 2) for (let k = 0; k < K; k++) tri(c, B[k], B[(k + 1) % K], col, mat, ctr);
      }
    }
  }
  const ell = (z, rx, ry, cy, n = 10, cx = 0) => Array.from({ length: n }, (_, i) => { const a = (i + 0.5) / n * Math.PI * 2; return [cx + Math.cos(a) * rx, cy + Math.sin(a) * ry, z]; });
  // aerofoil section across x — points lie in the z,y plane
  const foil = (x, c, zc, th, y) => [[x, y, zc + 0.5 * c], [x, y + th, zc + 0.2 * c], [x, y + 0.8 * th, zc - 0.25 * c], [x, y + 0.1 * th, zc - 0.5 * c], [x, y - 0.3 * th, zc - 0.25 * c], [x, y - 0.45 * th, zc + 0.2 * c]];
  // fin section at height y, centred on xc — points lie in the z,x plane
  const finS = (y, xc, c, zc, th) => [[xc, y, zc + 0.5 * c], [xc + th, y, zc + 0.1 * c], [xc + th * 0.6, y, zc - 0.5 * c], [xc - th * 0.6, y, zc - 0.5 * c], [xc - th, y, zc + 0.1 * c]];
  function beam(a, b, w, col) {
    const d = sub(b, a); const up = Math.abs(d[1]) > Math.abs(d[0]) + Math.abs(d[2]) ? [1, 0, 0] : [0, 1, 0];
    let s = cross(d, up); let l = Math.hypot(...s); s = s.map(v => v / l * w);
    let t = cross(d, s); l = Math.hypot(...t); t = t.map(v => v / l * w);
    const ring = p => [[p[0] + s[0] + t[0], p[1] + s[1] + t[1], p[2] + s[2] + t[2]], [p[0] - s[0] + t[0], p[1] - s[1] + t[1], p[2] - s[2] + t[2]], [p[0] - s[0] - t[0], p[1] - s[1] - t[1], p[2] - s[2] - t[2]], [p[0] + s[0] - t[0], p[1] + s[1] - t[1], p[2] + s[2] - t[2]]];
    loft([ring(a), ring(b)], col);
  }

  // fuselage: dark radome, then camouflage back to the tail cone
  loft([ell(10.4, 0.05, 0.05, 0.0), ell(9.2, 0.34, 0.32, 0.02), ell(7.6, 0.62, 0.58, 0.08), ell(5.6, 0.88, 0.82, 0.2), ell(3.2, 1.02, 0.95, 0.3), ell(0.5, 1.05, 0.9, 0.3), ell(-3.5, 1.0, 0.8, 0.25), ell(-7.0, 0.8, 0.6, 0.2), ell(-8.9, 0.32, 0.28, 0.2)],
    [RADOME, RADOME, camo, camo, camo, camo, camo, camo]);
  for (const sgn of [1, -1]) {
    // engine: dark intake mouth, long nacelle, burnt nozzle
    loft([ell(2.66, 0.50, 0.56, -0.2, 8, sgn * 1.46), ell(2.6, 0.64, 0.70, -0.2, 8, sgn * 1.46), ell(1.5, 0.80, 0.82, -0.18, 8, sgn * 1.42), ell(-2.0, 0.90, 0.86, -0.14, 8, sgn * 1.36), ell(-6.5, 0.86, 0.82, -0.12, 8, sgn * 1.32), ell(-8.4, 0.80, 0.78, -0.12, 8, sgn * 1.32), ell(-9.5, 0.60, 0.60, -0.12, 8, sgn * 1.32)],
      [DARK, camo, camo, camo, camo, NOZZLE]);
    // the fire inside the nozzle
    { const cx = sgn * 1.32, cy = -0.12, z = -9.52, r = 0.5, N = 8; for (let i = 0; i < N; i++) { const a = i / N * Math.PI * 2, b = (i + 1) / N * Math.PI * 2; for (const p of [[cx, cy, z], [cx + Math.cos(a) * r, cy + Math.sin(a) * r, z], [cx + Math.cos(b) * r, cy + Math.sin(b) * r, z]]) out.push(p[0], p[1], p[2], 0, 0, -1, GLOW[0], GLOW[1], GLOW[2], 4); } }
    // leading-edge extension, wing, stabilator
    loft([foil(sgn * 0.8, 5.2, 4.5, 0.10, 0.26), foil(sgn * 1.75, 0.5, 2.5, 0.08, 0.14)], [camo]);
    loft([foil(sgn * 1.7, 7.6, -1.3, 0.26, 0.05), foil(sgn * 4.2, 4.4, -2.5, 0.16, 0.0), foil(sgn * 6.4, 2.0, -3.7, 0.09, -0.05)], [camo, camo]);
    loft([foil(sgn * 1.9, 3.0, -7.4, 0.14, 0.0), foil(sgn * 4.3, 1.2, -8.5, 0.07, -0.02)], [camo]);
    // canted fin with a red cap
    loft([finS(0.55, sgn * 1.35, 3.6, -6.3, 0.14), finS(2.8, sgn * 2.12, 2.0, -7.37, 0.09), finS(3.5, sgn * 2.35, 1.5, -7.7, 0.07)], [camo, RED]);
    // pylon for the missile under the wing (the missiles are separate, so they can leave)
    beam([sgn * 3.9, -0.42, -0.6], [sgn * 3.9, -0.05, -1.0], 0.09, SLATE);
    beam([sgn * 3.9, -0.42, -2.4], [sgn * 3.9, -0.05, -2.6], 0.09, SLATE);
  }
  // gold-tinted bubble canopy
  const can = (z, w, h, yb) => [[-w, yb - 0.1, z], [-w * 0.86, yb + 0.72 * h, z], [-w * 0.36, yb + h, z], [w * 0.36, yb + h, z], [w * 0.86, yb + 0.72 * h, z], [w, yb - 0.1, z]];
  loft([can(6.5, 0.28, 0.10, 0.78), can(5.5, 0.60, 0.80, 0.84), can(4.1, 0.70, 0.98, 0.96), can(2.7, 0.50, 0.42, 1.08)], [GLASS], 3);
  const solidEnd = out.length / 10;

  // flame: unit cone along -z, base radius 1
  const flame = [];
  const FS = 8;
  for (let i = 0; i < FS; i++) {
    const a = i / FS * Math.PI * 2, b = (i + 1) / FS * Math.PI * 2;
    const p1 = [Math.cos(a), Math.sin(a), 0], p2 = [Math.cos(b), Math.sin(b), 0], tip = [0, 0, -1];
    for (const p of [p1, p2, tip]) flame.push(p[0], p[1], p[2], 0, 0, 1, 1, 1, 1, 2);
  }
  // one missile, centred on its own middle, nose along +z
  const m0 = out.length;
  loft([ell(2.3, 0.03, 0.03, 0, 6), ell(1.5, 0.19, 0.19, 0, 6), ell(0.9, 0.19, 0.19, 0, 6), ell(-1.3, 0.19, 0.19, 0, 6), ell(-1.6, 0.13, 0.13, 0, 6)], [RED, RED, WHITE, DARK]);
  for (const [a, b] of [[[0.5, 0, -1.5], [-0.5, 0, -1.5]], [[0, 0.5, -1.5], [0, -0.5, -1.5]]]) loft([[[a[0], a[1], -0.9], [a[0], a[1], -1.55], [b[0], b[1], -1.55], [b[0], b[1], -0.9]].map(q => [q[0] + (a[1] ? 0.03 : 0), q[1] + (a[0] ? 0.03 : 0), q[2]]), [[a[0], a[1], -0.9], [a[0], a[1], -1.55], [b[0], b[1], -1.55], [b[0], b[1], -0.9]].map(q => [q[0] - (a[1] ? 0.03 : 0), q[1] - (a[0] ? 0.03 : 0), q[2]])], [SLATE]);
  const missile = new Float32Array(out.slice(m0)); out.length = m0;
  return { data: new Float32Array(out), solidEnd, flame: new Float32Array(flame), flameCount: flame.length / 10, missile, missileCount: missile.length / 10 };
}