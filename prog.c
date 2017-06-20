#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "BitVec.h"
#include "DaubDWT.h"
#include "DWTAnlz.h"
#include "HaarDWT.h"
#include "Intrpl.h"
#include "SigAnlz.h"


static int constrain(int n, int min, int max){
	if(n<min)
		return min;
	if(n>max)
		return max;
	return n;
}

// DEBUG
void printbv(BitVec *bv, float *sig){
	int iter=0;
	int idx, len;
	while(hasNext_BitVec(bv,1,iter)){
		getNext_BitVec(bv,1, &iter, &idx, &len);
		printf("%d\t%d :\t%d\t\t%d :\t%d\n", len, idx, (int)sig[idx], idx+len-1, (int)sig[idx+len-1]);
	}
	printf("\n");
}


int main(int argc, char *argv[]){

	char *sigDir;	// Dir containing signal files n.txt
	int numSig;		// Number of sigals in the dir
	char *peakDir;	// Dir to create dwt files
	float sigTh;	// th*sigma for thresholding of signal
	int minSigLen;	// Min length of a valid signal
	int maxZero;	// Max length of holes in a signal
	float sigPad;	// Added padding L/R to sig before dwt
	char *dwtDir;	// Dir conntaing DWTs
	int dwtLen;		// Length of dwt analysis
	char *ratFile;	// File containg ratios
	int numRat;		// Number of ratios to calculate
	char *anlzChoice;

	if(argc != 13){
		printf("sigDir numSig peakDir sigTh minSigLen maxZero sigPad dwtDir dwtLen ratFile numRat anlzChoice\n");
		return 0;
	}
	
	// Argument parsing

	sigDir = argv[1];
	numSig = atoi(argv[2]);
	peakDir = argv[3];
	sigTh = atof(argv[4]);
	minSigLen = atoi(argv[5]);
	maxZero = atoi(argv[6]);
	sigPad = atof(argv[7]);
	dwtDir = argv[8];
	dwtLen = atoi(argv[9]);
	ratFile = argv[10];
	numRat = atoi(argv[11]);
	anlzChoice = argv[12];


	FILE *ratFilePtr = fopen(ratFile, "w");


	// Loop through all signal files

	for(int i_numSig=0; i_numSig<numSig; i_numSig++){
		char sigFile[128];	// Name of current signal file
		sprintf(sigFile, "%s/%d.txt", sigDir, i_numSig);
		printf("Reading from %s\n", sigFile);
		FILE *sigFilePtr = fopen(sigFile, "r");

		// Read signal
		int sigLen;
		fscanf(sigFilePtr, " %d", &sigLen);
		float *sig = malloc(sigLen*sizeof(float));
		for(int i=0; i<sigLen; i++)
			fscanf(sigFilePtr, " %f", &sig[i]);

		fclose(sigFilePtr);


		// Peak identification
		BitVec *bv = new_BitVec(sigLen);
		threshold_SigAnlz(sig, sigLen, bv, sigTh, 0.01);

		// Ignore dips
		ignoreDipBitVec_SigAnlz(bv, sig, sigLen, sigTh);

		// DEBUG
		// printbv(bv, sig);

		// Consolidate signal
		toggleMaxLen(bv,0,1);	// Eliminate blocks of...
		toggleMaxLen(bv,1,2);	// ...above average noise
		toggleMaxLen(bv,0,maxZero);
		toggleMaxLen(bv,1,minSigLen-1);
		
		// DEBUG
		// printbv(bv, sig);


		// Loop through peaks
		int iter=0;
		for(int i_numPeak=0; hasNext_BitVec(bv,1,iter)==1; i_numPeak++){
			
			int peakIdx;	// Storing peak location
			int peakLen;
			getNext_BitVec(bv,1,&iter,&peakIdx,&peakLen);	// Get peak idx and len

			peakIdx -= sigPad;	// Add padding to signal
			peakLen += 2*sigPad;
			if(peakIdx < 0)	// Bound check L
				peakIdx = 0;
			if(peakIdx+peakLen > sigLen)	// Bound check R
				peakLen = sigLen-peakIdx;

			// DEBUG
			// printf("%d %d\n", peakIdx, peakLen);

			int sigNewLen = dwtLen;
			float *sigNew = malloc(sigNewLen*sizeof(float));	// Resized signal
			cubic_Intrpl(sig+peakIdx, peakLen, sigNew, sigNewLen);
			// linear_Intrpl(sig+peakIdx, peakLen, sigNew, sigNewLen);


			// Write peak signal to file
			char peakFile[128];	// Name of current DWT file
			sprintf(peakFile, "%s/%d_%d.txt", peakDir, i_numSig, i_numPeak);
			// printf("Writing to %s\n", peakFile);
			FILE *peakFilePtr = fopen(peakFile, "w");

			fprintf(peakFilePtr, "%d\n", sigNewLen);
			for(int i=0; i<sigNewLen; i++)
				fprintf(peakFilePtr, "%f\n", sigNew[i]);

			fclose(peakFilePtr);


			float *dwt = malloc(dwtLen*sizeof(float));


			if( strcmp(anlzChoice, "haar")==0 )
				coef_1D_HaarDWT(sigNew, dwt, dwtLen);
			else if( strcmp(anlzChoice, "db2")==0 || strcmp(anlzChoice, "D4")==0 )
				coef_1D_Dx_DaubDWT(sigNew, dwt, dwtLen, 4);
			else if( strcmp(anlzChoice, "db3")==0 || strcmp(anlzChoice, "D6")==0 )
				coef_1D_Dx_DaubDWT(sigNew, dwt, dwtLen, 6);
			else if( strcmp(anlzChoice, "db4")==0 || strcmp(anlzChoice, "D8")==0 )
				coef_1D_Dx_DaubDWT(sigNew, dwt, dwtLen, 8);


			// Output to ratio file
			float rat[numRat];
			ratio_DWTAnlz(dwt, dwtLen, rat, numRat);
			fprintf(ratFilePtr, "%d\t%d\t%d\t%d\t", i_numSig, i_numPeak, peakIdx, peakLen);
			for(int i_numRat=0; i_numRat<numRat; i_numRat++)
				fprintf(ratFilePtr, "%f\t", rat[i_numRat]);
			fprintf(ratFilePtr, "\n");


			// // DEBUG
			// for(int i_numRat=0; i_numRat<numRat; i_numRat++)
			// 	printf("%f\t", rat[i_numRat]);
			// printf("\n");


			// Output DWT
			char dwtFile[128];	// Name of current DWT file
			sprintf(dwtFile, "%s/%d_%d.txt", dwtDir, i_numSig, i_numPeak);
			// printf("Writing to %s\n", dwtFile);
			FILE *dwtFilePtr = fopen(dwtFile, "w");

			fprintf(dwtFilePtr, "%d\n", dwtLen);
			for(int i=0; i<dwtLen; i++)
				fprintf(dwtFilePtr, "%f\n", dwt[i]);
			
			fclose(dwtFilePtr);

			free(sigNew);
			free(dwt);
		}

		free_BitVec(bv);
		free(sig);

	}

	fclose(ratFilePtr);

	return 0;
}