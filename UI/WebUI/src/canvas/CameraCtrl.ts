import type { Point } from '../domain';

// View camera for the EverCheck canvas — pan/zoom/rotate as a DOMMatrix.
// Pure math, no app/redux coupling. (Distinct from the hardware CameraCtrl in
// UTIL/BPG_Protocol.js.) Extracted verbatim from EverCheckCanvasComponent.js.
//
// Note: setMatrixValue(DOMMatrix) is a non-standard browser method not in lib.dom,
// hence the `as DOMMatrixLike` casts.
type DOMMatrixLike = DOMMatrix & { setMatrixValue(m: DOMMatrix): DOMMatrix };

export class CameraCtrl {
  matrix: DOMMatrix;
  tmpMatrix: DOMMatrix;
  identityMat: DOMMatrix;

  constructor(_canvasDOM?: unknown) {
    this.matrix = new DOMMatrix();
    this.tmpMatrix = new DOMMatrix();
    this.identityMat = new DOMMatrix();
    this.Scale(1);
  }

  Scale(scale: number, center: Point = { x: 0, y: 0 }) {
    let mat = new DOMMatrix();
    mat.translateSelf(center.x, center.y);
    mat.scaleSelf(scale, scale);
    mat.translateSelf(-center.x, -center.y);

    this.matrix.preMultiplySelf(mat);
  }

  Rotate_matrix(theta: number, center: Point = { x: 0, y: 0 }) {
    let mat = new DOMMatrix();
    mat.translateSelf(center.x, center.y);
    let sinT = Math.sin(theta);
    let cosT = Math.cos(theta);

    mat.b = sinT;
    mat.c = -sinT;
    mat.a =
    mat.d = cosT;
    // mat.is2D=true;
    mat.translateSelf(-center.x, -center.y);
    // console.log(mat);

    let scale = this.GetCameraScale();

    this.matrix.b = 0;
    this.matrix.c = 0;
    this.matrix.a =
    this.matrix.d = scale;

    return mat;
  }

  Rotate(theta: number, center: Point = { x: 0, y: 0 }) {
    this.matrix.preMultiplySelf(this.Rotate_matrix(theta, center));
  }

  ResetRotate(_theta: number, _center: Point = { x: 0, y: 0 }) {
    // this.matrix.preMultiplySelf(this.Rotate_matrix(theta, center ));
  }

  SetRotate(theta: number, center: Point = { x: 0, y: 0 }) {
    this.matrix = this.Rotate_matrix(theta, center);
  }

  StartDrag(vector: Point = { x: 0, y: 0 }) {
    (this.tmpMatrix as DOMMatrixLike).setMatrixValue(this.identityMat);
    this.tmpMatrix.translateSelf(vector.x, vector.y);
  }

  EndDrag() {
    this.matrix.preMultiplySelf(this.tmpMatrix);
    (this.tmpMatrix as DOMMatrixLike).setMatrixValue(this.identityMat);
  }

  SetOffset(location: Point = { x: 0, y: 0 }) {
    this.matrix.m41 = 0;
    this.matrix.m42 = 0;
    this.matrix.translateSelf(location.x, location.y);
  }

  GetCameraRotation(matrix: DOMMatrix = this.matrix) {
    let rot = Math.atan2(matrix.m21 - matrix.m12, matrix.m11 + matrix.m22);
    return rot;
  }

  // Over-simplified scale extraction for a matrix; revisit if needed.
  GetCameraScale(matrix: DOMMatrix = this.matrix) {
    return Math.sqrt(matrix.m11 * matrix.m22 - matrix.m12 * matrix.m21);
  }

  GetCameraOffset(matrix: DOMMatrix = this.matrix) {
    return { x: matrix.m41, y: matrix.m42 };
  }

  CameraTransform(matrix_input: DOMMatrix) {
    matrix_input.multiplySelf(this.tmpMatrix);
    matrix_input.multiplySelf(this.matrix);
  }
}
