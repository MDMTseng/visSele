#pragma once
#include <stdint.h>
float* vecAdd(float* vr,float* v1,float* v2,int dim);
float* vecSub(float* vr,float* v1,float* v2,int dim);
float* vecAssign(float* v_dst,float* v_src,int dim);
float* vecLerp(float* vr,float* v0,float* v1,int dim,float ratio);
float vecDotProduct(float* v1,float* v2,int dim);
float EuclideanDistance(float *v1,float *v2,int vecL);
float EuclideanMagnitude(float *vec,int vecL);
float ManhattanDistance(float *v1,float *v2,int vecL,int *ret_idx);
float ManhattanMagnitude(float *vec,int vecL,int *ret_idx);
float calcAngleAndOthers(float* p0,float*p1,float* p2,int dim,float spDistRatio,float*ret_sp0,float*ret_sp2,float *ret_distance);
float Ang2SplineKappa_PAP(float angle_rad);
float Ang2RDivDist(float angle_rad);
void cubicBezier_TCoeff4(float t,float *coeff_4);
float cubicBezier_Ele(float p0, float p1, float p2, float p3,float *coeff_4);
void cubicBezier_Vec(float *resultVec, float *p0, float *p1, float *p2, float *p3,int dim,float *coeff_4);

char *int2bin_buffer(uint32_t a, int digits, char *buffer, int buf_size);
char *int2bin_static(uint32_t a, int digits);