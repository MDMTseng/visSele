#pragma once


float* vecAdd(float* vr,float* v1,float* v2,int dim);
float* vecSub(float* vr,float* v1,float* v2,int dim);
float* vecLerp(float* vr,float* v0,float* v1,int dim,float ratio);
float vecDotProduct(float* v1,float* v2,int dim);
float EuclideanDistance(float *v1,float *v2,int vecL);
float EuclideanMagnitude(float *vec,int vecL);
float ManhattanDistance(float *v1,float *v2,int vecL,int *ret_idx);
float ManhattanMagnitude(float *vec,int vecL,int *ret_idx);
float calcAngleAndOthers(float* p0,float*p1,float* p2,int dim,float spDistRatio,float*ret_sp0,float*ret_sp2,float *ret_distance);
float Ang2SplineKappa_PAP(float angle_rad);
float Ang2RDivDist_PAP(float angle_rad);
void cubicBezier_TCoeff(double t,float *coeff_4);
float cubicBezier_Ele(float p0, float p1, float p2, float p3,float *coeff_4);