// Mathematical engine ported directly from core/math/ and core/calib/ of MikMap

export type Point2D = [number, number];

export class Mat3 {
  // Column-major or row-major: standard 3x3 array [m00, m01, m02, m10, m11, m12, m20, m21, m22]
  public m: number[];

  constructor(elements?: number[]) {
    if (elements && elements.length === 9) {
      this.m = [...elements];
    } else {
      this.m = [
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
      ];
    }
  }

  static identity(): Mat3 {
    return new Mat3();
  }

  static multiply(a: Mat3, b: Mat3): Mat3 {
    const r = new Array(9).fill(0);
    for (let row = 0; row < 3; row++) {
      for (let col = 0; col < 3; col++) {
        r[row * 3 + col] =
          a.m[row * 3 + 0] * b.m[0 * 3 + col] +
          a.m[row * 3 + 1] * b.m[1 * 3 + col] +
          a.m[row * 3 + 2] * b.m[2 * 3 + col];
      }
    }
    return new Mat3(r);
  }

  transformPoint(p: Point2D): Point2D {
    const x = p[0];
    const y = p[1];
    const nx = this.m[0] * x + this.m[1] * y + this.m[2];
    const ny = this.m[3] * x + this.m[4] * y + this.m[5];
    const w = this.m[6] * x + this.m[7] * y + this.m[8];

    if (Math.abs(w) < 1e-10) {
      return [nx, ny];
    }
    return [nx / w, ny / w];
  }

  determinant(): number {
    const m = this.m;
    return (
      m[0] * (m[4] * m[8] - m[5] * m[7]) -
      m[1] * (m[3] * m[8] - m[5] * m[6]) +
      m[2] * (m[3] * m[7] - m[4] * m[6])
    );
  }

  invert(): Mat3 | null {
    const m = this.m;
    const det = this.determinant();
    if (Math.abs(det) < 1e-10) {
      return null;
    }
    const invDet = 1.0 / det;
    const res = [
      (m[4] * m[8] - m[5] * m[7]) * invDet,
      (m[2] * m[7] - m[1] * m[8]) * invDet,
      (m[1] * m[5] - m[2] * m[4]) * invDet,

      (m[5] * m[6] - m[3] * m[8]) * invDet,
      (m[0] * m[8] - m[2] * m[6]) * invDet,
      (m[2] * m[3] - m[0] * m[5]) * invDet,

      (m[3] * m[7] - m[4] * m[6]) * invDet,
      (m[1] * m[6] - m[0] * m[7]) * invDet,
      (m[0] * m[4] - m[1] * m[3]) * invDet
    ];
    return new Mat3(res);
  }
}

// Solve Homography using Direct Linear Transform (DLT) with 4 correspondences
// Ported from LinearSolver.cpp and Homography.cpp
export function solveHomographyDLT(src: Point2D[], dst: Point2D[]): Mat3 | null {
  if (src.length < 4 || dst.length < 4) return null;

  // Build 8x8 system setting h33 = 1
  const A: number[][] = [];
  const b: number[] = [];

  for (let i = 0; i < 4; i++) {
    const [x, y] = src[i];
    const [u, v] = dst[i];

    // Row 1: x, y, 1, 0, 0, 0, -u*x, -u*y  =  u
    A.push([x, y, 1, 0, 0, 0, -u * x, -u * y]);
    b.push(u);

    // Row 2: 0, 0, 0, x, y, 1, -v*x, -v*y  =  v
    A.push([0, 0, 0, x, y, 1, -v * x, -v * y]);
    b.push(v);
  }

  // Gaussian elimination with partial pivoting
  const n = 8;
  for (let col = 0; col < n; col++) {
    // Find pivot
    let maxRow = col;
    let maxVal = Math.abs(A[col][col]);
    for (let row = col + 1; row < n; row++) {
      if (Math.abs(A[row][col]) > maxVal) {
        maxVal = Math.abs(A[row][col]);
        maxRow = row;
      }
    }

    if (maxVal < 1e-10) return null; // Singular

    // Swap rows
    if (maxRow !== col) {
      const tempRow = A[col];
      A[col] = A[maxRow];
      A[maxRow] = tempRow;
      const tempB = b[col];
      b[col] = b[maxRow];
      b[maxRow] = tempB;
    }

    // Eliminate
    for (let row = col + 1; row < n; row++) {
      const factor = A[row][col] / A[col][col];
      for (let k = col; k < n; k++) {
        A[row][k] -= factor * A[col][k];
      }
      b[row] -= factor * b[col];
    }
  }

  // Back substitution
  const h = new Array(8).fill(0);
  for (let i = n - 1; i >= 0; i--) {
    let sum = b[i];
    for (let j = i + 1; j < n; j++) {
      sum -= A[i][j] * h[j];
    }
    h[i] = sum / A[i][i];
  }

  return new Mat3([
    h[0], h[1], h[2],
    h[3], h[4], h[5],
    h[6], h[7], 1.0
  ]);
}

// Compute CSS transform matrix3d for 4-point corner pinning
export function getPerspectiveTransformMatrix(
  srcWidth: number,
  srcHeight: number,
  quad: { tl: Point2D; tr: Point2D; br: Point2D; bl: Point2D }
): string {
  const src: Point2D[] = [
    [0, 0],
    [srcWidth, 0],
    [srcWidth, srcHeight],
    [0, srcHeight]
  ];
  const dst: Point2D[] = [quad.tl, quad.tr, quad.br, quad.bl];
  const h = solveHomographyDLT(src, dst);

  if (!h) {
    return 'matrix(1, 0, 0, 1, 0, 0)';
  }

  const [h0, h1, h2, h3, h4, h5, h6, h7, h8] = h.m;

  // matrix3d(a1, b1, c1, d1, a2, b2, c2, d2, a3, b3, c3, d3, a4, b4, c4, d4)
  // maps [x, y, 0, 1] column vector
  return `matrix3d(
    ${h0}, ${h3}, 0, ${h6},
    ${h1}, ${h4}, 0, ${h7},
    0, 0, 1, 0,
    ${h2}, ${h5}, 0, ${h8}
  )`;
}

// Point in polygon test for sensor ROI zone and masks
export function isPointInPolygon(point: Point2D, vs: Point2D[]): boolean {
  const x = point[0];
  const y = point[1];
  let inside = false;
  for (let i = 0, j = vs.length - 1; i < vs.length; j = i++) {
    const xi = vs[i][0], yi = vs[i][1];
    const xj = vs[j][0], yj = vs[j][1];
    const intersect = ((yi > y) !== (yj > y)) &&
      (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
    if (intersect) inside = !inside;
  }
  return inside;
}

// One-Euro Filter (from core/filter/OneEuroFilter.cpp)
export class OneEuroFilter {
  private minCutoff: number;
  private beta: number;
  private dCutoff: number;
  private xPrev: number | null = null;
  private dxPrev = 0;
  private tPrev: number | null = null;

  constructor(minCutoff = 1.0, beta = 0.007, dCutoff = 1.0) {
    this.minCutoff = minCutoff;
    this.beta = beta;
    this.dCutoff = dCutoff;
  }

  private alpha(rate: number, cutoff: number): number {
    const tau = 1.0 / (2 * Math.PI * cutoff);
    const te = 1.0 / rate;
    return 1.0 / (1.0 + tau / te);
  }

  filter(x: number, timestamp = performance.now()): number {
    if (this.tPrev === null || this.xPrev === null) {
      this.xPrev = x;
      this.tPrev = timestamp;
      this.dxPrev = 0;
      return x;
    }

    const dt = Math.max((timestamp - this.tPrev) / 1000.0, 1e-4);
    this.tPrev = timestamp;
    const rate = 1.0 / dt;

    const dx = (x - this.xPrev) / dt;
    const aD = this.alpha(rate, this.dCutoff);
    const dxHat = aD * dx + (1.0 - aD) * this.dxPrev;
    this.dxPrev = dxHat;

    const cutoff = this.minCutoff + this.beta * Math.abs(dxHat);
    const a = this.alpha(rate, cutoff);
    const xHat = a * x + (1.0 - a) * this.xPrev;
    this.xPrev = xHat;

    return xHat;
  }

  reset(): void {
    this.xPrev = null;
    this.tPrev = null;
    this.dxPrev = 0;
  }
}
