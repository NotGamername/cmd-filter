#define MAX_CHAN	2
#define MAX_COEF	256

/* define Filt struct */
struct Filt {
	int num_b;			//number of coefficients in FIR filer b
	double b[MAX_COEF]; //FIR coefficients
	int num_a;			//number of coefficients in IIR filer a
	double a[MAX_COEF];	//IIR coefficients
};

/* define State struct */
struct State {
	double xState[MAX_COEF];
	double yState[MAX_COEF];
};

void filter(double *x, double *y, int num_samp, struct Filt *pf, struct State *ps);