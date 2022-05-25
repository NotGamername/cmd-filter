/* Apply FIR/IIR filter to a single-channel buffer that is a single block
 * using direct form I filter flow graph
 * This is "block-processing" as you would do with a portio or VST framework
 */
#include "filter2.h"	//filter coefficients and state

void filter(
 	double *x, 		//input buffer
 	double *y, 		//output buffer
 	int N, 			//number of samples
 	struct Filt *pf,	//filter structure
 	struct State *ps)	//state structure
{
 	int Mb = pf->num_b;	//number of FIR filter coefficients
 	double *b = pf->b;	//pointer to FIR filter coefficients
 	int Ma = pf->num_a;	//number of IIR filter coefficients
 	double *a = pf->a;	//pointer to IIR filter coefficients


	for(int n = 0; n < (N+Mb-1); n++){
		double sum = 0;
		for(int k = 0; k < Mb; k++){
			if ((n-k) >= 0){
				sum += x[n-k]*b[k];
			} else {
				sum += ps->xState[(Mb-1)+(n-k)]*b[k];
			}
		}
		y[n] = sum;
	}
	for(int i = 0; i < Mb-1; i++){
		ps->xState[(Mb-2)-i] = x[(N-1)-i];
	}
}	

//xState is FIR
//yState is IIR