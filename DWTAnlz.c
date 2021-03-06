#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include "DWTAnlz.h"


static int magComp(const void *a, const void *b){
	if( fabs(*((float*)a)) > fabs(*((float*)b)) )	return -1;
	if( fabs(*((float*)a)) < fabs(*((float*)b)) )	return 1;
	return 0;
}

static int magCompIdx(const void *ip, const void *jp, void *dwt){
	int i = *((int*)ip);
	int j = *((int*)jp);
	float a = ((float*)dwt)[i];
	float b = ((float*)dwt)[j];
	if( fabs(a) > fabs(b) )	return -1;
	else if( fabs(a) < fabs(b) )	return 1;
	else if(i<j)	return -1;	// Stable sort
	else if(i>j)	return 1;
}

static float median(float *ip, int n, float *med){
	float tip[n];
	memcpy(tip, ip, n*sizeof(float));
	qsort(tip, n, sizeof(float), magComp);
	
	*med = tip[n/2];
	return tip[n/2];
}



void sort_DWTAnlz(float *ip, float *op, int n){
	if(ip!=op)
		memcpy(op, ip, n*sizeof(float));
	qsort(op, n, sizeof(float), magComp);
}

void sortIdx_DWTAnlz(float *dwt, int **idxSorted, int n){
	*idxSorted = malloc(n*sizeof(int));
	for(int i=0; i<n; i++)
		(*idxSorted)[i] = i;
	qsort_r(*idxSorted, n, sizeof(int), magCompIdx, dwt);
}



float sum_DWTAnlz(float *dwt, int n, float *sum){
	float tsum = 0;
	
	for(int i=1; i<n; i++)
		tsum += dwt[i];

	if(sum)	*sum = tsum;
	return tsum;
}

float max_DWTAnlz(float *dwt, int n, float *max){
	float tmax = -FLT_MAX;
	
	for(int i=1; i<n; i++)
		if(dwt[i]>tmax)	tmax=dwt[i];

	if(max)	*max = tmax;
	return tmax;
}

float min_DWTAnlz(float *dwt, int n, float *min){
	float tmin = FLT_MAX;
	
	for(int i=1; i<n; i++)
		if(dwt[i]<tmin)	tmin=dwt[i];

	if(min)	*min = tmin;
	return tmin;
}

float mean_DWTAnlz(float *dwt, int n, float *mean){
	float tmean;

	tmean = sum_DWTAnlz(dwt, n, NULL)/(n-1);

	if(mean)	*mean = tmean;
	return tmean;
}

float var_DWTAnlz(float *dwt, int n, float *var){
	float tvar = 0, mean = sum_DWTAnlz(dwt, n, NULL)/(n-1);

	for(int i=1; i<n; i++)
		tvar += (dwt[i]-mean)*(dwt[i]-mean);
	tvar /= n-1;

	if(var)	*var = tvar;
	return tvar;
}

float stddev_DWTAnlz(float *dwt, int n, float *stddev){
	float tstddev = sqrt(var_DWTAnlz(dwt, n, NULL));

	if(stddev)	*stddev = tstddev;
	return tstddev;
}

void param_DWTAnlz(float *dwt, int n, float *mean, float *stddev, float *max, float *min){
	float tsum=0, tmean, tmax=-FLT_MAX, tmin=FLT_MAX;

	for(int i=1; i<n; i++){
		tsum += dwt[i];
		if(dwt[i]>tmax)	tmax=dwt[i];
		if(dwt[i]<tmin)	tmin=dwt[i];
	}

	tmean = tsum/(n-1);

	float tvar=0, tstddev;
	
	for(int i=1; i<n; i++)
		tvar += (dwt[i]-tmean)*(dwt[i]-tmean);

	tvar /= n-1;
	tstddev = sqrt(tvar);

	if(mean)	*mean = tmean;
	if(stddev)	*stddev = tstddev;
	if(max)		*max = tmax;
	if(min)		*min = tmin;
}



void denoise_donohoHard_DWTAnlz(float *ip, float *op, int n){
	float med, trshld;
	
	for(int j=1; j<n; j*=2){
		median(ip+j, j, &med);
		trshld = med/0.6745*sqrt(2*logf(j));
		for(int i=j; i<2*j; i++){
			if( fabs(ip[i])>trshld )
				op[i] = ip[i];
			else
				op[i] = 0;
		}
	}
}

void denoise_donohoSoft_DWTAnlz(float *ip, float *op, int n){
	float med, trshld;
	
	for(int j=1; j<n; j*=2){
		median(ip+j, j, &med);
		trshld = med/0.6745*sqrt(2*logf(j));
		for(int i=j; i<2*j; i++){
			if( ip[i]>trshld )
				op[i] = ip[i]-trshld;
			else if( ip[i]<-trshld )
				op[i] = ip[i]+trshld;
			else
				op[i] = 0;
		}
	}
}


void denoise_DWTAnlz(float *ip, float *op, int n, float k){
	float trshldlo = mean_DWTAnlz(ip, n, NULL) - k*stddev_DWTAnlz(ip, n, NULL);
	float trshldhi = mean_DWTAnlz(ip, n, NULL) + k*stddev_DWTAnlz(ip, n, NULL);
	
	for(int i=1; i<n; i++){
		if( ip[i]>trshldhi || ip[i]<trshldlo )
			op[i] = ip[i];
		else
			op[i] = 0;
	}
}

void sigCoef_DWTAnlz(float *dwt, int n, float k, int *nsig, int **idxsig){
	float mean = mean_DWTAnlz(dwt, n, NULL);
	float stddev = stddev_DWTAnlz(dwt, n, NULL);
	float trshldlo = mean - k*stddev;
	float trshldhi = mean + k*stddev;
	int tnsig = 0;

	int *idxbuf = malloc(n*sizeof(int)); // Memory leak !!!

	for(int i=1; i<n; i++)
		if( dwt[i]>trshldhi || dwt[i]<trshldlo )
			idxbuf[tnsig++] = i;

	idxbuf = realloc(idxbuf, tnsig*sizeof(int));
	qsort_r(idxbuf, tnsig, sizeof(int), magCompIdx, dwt);

	*nsig = tnsig;
	*idxsig = idxbuf;
}


void ratioFixed1_DWTAnlz(float* dwt, int n, float *ratio, int nRatio){
	for(int i=0; i<nRatio; i++)
		ratio[i] = dwt[i+2]/dwt[1];
}

void ratioFixed2_DWTAnlz(float* dwt, int n, float *ratio, int nRatio){
	ratio[0] = dwt[1]/dwt[2];
	for(int i=1; i<nRatio; i++)
		ratio[i] = dwt[i+2]/dwt[2];
}

void ratioMoving_DWTAnlz(float* dwt, int n, float *ratio, int nRatio){
	for(int i=0; i<nRatio; i++)
		ratio[i] = dwt[i+2]/dwt[i+1];
}

void normalize_DWTAnlz(float* dwt, int n, float *norm, int nNorm, int k){
	float factor = k/dwt[0];
	for(int i=0; i<nNorm; i++)
		norm[i] = dwt[i+1]*factor;
}



// idx = 2^j + k

int jkFromIdx_DWTAnlz(int idx, int *j, int *k){
	if(idx<1){
		printf("idx (%d) should be positive\n", idx);
		return -1;
	}

	int tj = 0;
	int tidx = idx;
	
	while(tidx >>= 1)
		tj++;

	int tk = idx-(1<<tj);

	if(j)	*j = tj;
	if(k)	*k = tk;

	return 0;
}


int jtFromIdx_DWTAnlz(int idx, int n, int *j, int *t, int *tmin, int *tmax){
	if(idx < 1){
		printf("idx (%d) should be positive\n", idx);
		return -1;
	}

	int tj = 0, tidx = idx;
	while(tidx >>= 1)
		tj++;

	int tk = idx-(1<<tj);

	int ttmin = (n>>tj)*tk;
	int ttmax = (n>>tj)*(tk+1);



	if(j)		*j = tj;
	if(tmin)	*tmin = ttmin;
	if(tmax)	*tmax = ttmax;
	if(t)		*t = (ttmin+ttmax)/2;

	return 1;
}

