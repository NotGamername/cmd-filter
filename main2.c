/* FIR or FIR/IIR filter of a WAV file using block processing */

#include <stdio.h>
#include <stdlib.h>		//malloc()
#include <unistd.h>		//sleep()
#include <stdbool.h>	//bool
#include <stdatomic.h>	//atomic read/write
#include <sndfile.h>	//sndfile
#include <portaudio.h>	//portaudio
#include "paUtils.h"	//portaudio utility functions
#include "filter2.h"	//declares struct Filt and State

#include "main.h"		//defines struct Filt filt
struct State state[MAX_CHAN];	//defines struct State state

#define BLK_LEN	1024	//block length for block processing

/* PortAudio callback structure */
struct PABuf {
	float *ifbuf;
	float *ofbuf;
	int num_chan;
	int next_frame;
	int total_frame;
	atomic_bool done;
	struct Filt *pf;
	struct State *ps;
};

/* PortAudio callback function protoype */
static int paCallback( const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData );

void print_filt(struct Filt *p);

int main(int argc, char *argv[])
{
	char *ifile, *ofile;
	int N, C, icount, ocount;
	float *ifbuf, *ofbuf; //frame buffers
    SNDFILE *isndfile, *osndfile;
    SF_INFO isfinfo, osfinfo;
	struct PABuf paBuf; //PortAudio data struct
    PaStream *stream;

	/* usage and parse command line */
	if (argc != 3){
		printf("Usage: %s input.wav output.wav\n",argv[0]);
		return -1;
	}

	//initialize pointers to files
	ifile = argv[1];
	ofile = argv[2];

	//open input file
	if ((isndfile = sf_open(ifile,SFM_READ,&isfinfo)) == NULL){
		printf("ERROR: could not open file %s\n",argv[1]);
		return -1;
	}

	/* Determine number of frames and channels in input file
	 * Set N to number of frames and C to number of channels
	 */
	N = isfinfo.frames;
	C = isfinfo.channels;

	//print information about input file
	printf("Frames: %lld Channels: %d Sampling Rate: %d\n",isfinfo.frames,isfinfo.channels,isfinfo.samplerate);

	//make sure output file has same stats as input file
	osfinfo.format = isfinfo.format;
	osfinfo.channels = C;
	osfinfo.samplerate = isfinfo.samplerate;

	//open output file
	if ((osndfile = sf_open(ofile,SFM_WRITE,&osfinfo)) == NULL){
		printf("ERROR: could not open file %s\n",argv[2]);
		return -1;
	}

	osfinfo.frames = N;

	//print information about output file
	printf("Frames: %lld Channels: %d Sampling Rate: %d\n",osfinfo.frames,osfinfo.channels,osfinfo.samplerate);

	/* Allocate buffers
	 * malloc storage ifbuf[] for N frames of C channels/frame
	 * malloc storage ofbuf[] for N frames of C channels/frame
	 */
	ifbuf = (float *)malloc(N * C * sizeof(float));
	if (ifbuf == NULL){
		printf("ERROR: Returned pointer to ifbuf was null\n");
		return -1;
	}

	ofbuf = (float *)malloc(N * C * sizeof(float));
	if (ofbuf == NULL){
		printf("ERROR: Returned pointer to ofbuf was null\n");
		return -1;
	}

	/* Read input WAV file into ifbuf[] */
	icount = sf_readf_float(isndfile,ifbuf,isfinfo.frames);
	if (icount != isfinfo.frames){
		printf("ERROR: input count does not equal number of frames\n");
		return -1;
	}

	//Close input file
	sf_close(isndfile);

	/* initialize Port Audio data struct */
	paBuf.ifbuf = ifbuf;
	paBuf.ofbuf = ofbuf;
	paBuf.num_chan = isfinfo.channels;
	paBuf.next_frame = 0;
	paBuf.total_frame = isfinfo.frames;
	paBuf.done = false;
	paBuf.pf = &filt;
	paBuf.ps = &state[0];

    /* start up Port Audio */
    printf("Starting PortAudio\n");
    stream = startupPa(1, isfinfo.channels, 
      isfinfo.samplerate, BLK_LEN, paCallback, &paBuf);

	/* 
	 * sleep and let callback process audio until done 
	 */
    while (!paBuf.done) {
    	printf("%d\n", paBuf.next_frame);
    	sleep(1);
    }

    /* shut down Port Audio */
    shutdownPa(stream);

    /* write output buffer to WAV file */
	ocount = sf_writef_float(osndfile,ofbuf,N);
	if (ocount != osfinfo.frames){
		printf("ERROR: output count does not equal number of frames\n");
		return -1;
	}

	/* close WAV files 
	 * free allocated storage
	 */
	sf_close(osndfile);

	free(ifbuf);
	free(ofbuf);

	printf("Successfully closed files and freed memory!\n");

	return 0;
}

static int paCallback(
	const void *inputBuffer, 
	void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData)
{
    /* Cast data passed via paCallback to our struct */
    struct PABuf *p = (struct PABuf *)userData; 
    /* cast input and output buffers */
    float *output = (float *)outputBuffer;
    //float *input = (float *)inputBuffer; //not used in this code
	/* since blocks are short, just declare single-channel arrays */
	double icbuf[MAX_CHAN][BLK_LEN], ocbuf[MAX_CHAN][BLK_LEN];
	int N = framesPerBuffer;
	int C = p->num_chan;
	/* local pointers to ifbuf[] and ofbuf[] */
    float *ifbuf = p->ifbuf + p->next_frame*C;
    float *ofbuf = p->ofbuf + p->next_frame*C;

	/* zero PortAudio output buffer for:
	 * partial output buffer
	 * or call to PortAudio after done == true (after all input data has been processed)
	 */
	for (int i=0; i<N*C; i++) {
		output[i] = 0;
	}

	/* return if done */
	if (p->done == true) {
		 return 0;
	}

	/* adjust N if last frame is partial frame */
	if (p->next_frame + N > p->total_frame) {
		N = p->total_frame - p->next_frame;
	}

	/* de-interleave next block of ifbuf[] into icbuf[][] */
	for (int i = 0; i < C; i++){
		for (int j = 0; j < N; j++){
			icbuf[i][j] = ifbuf[j * C + i];
		}
	}

	/* filter each channel */
	for (int chan=0; chan<C; chan++) {
		filter(&icbuf[chan][0], &ocbuf[chan][0], N, p->pf, &p->ps[chan]);
	}

	/* interleave ocbuf[][] into frame buffer ofbuf[] */
	for (int i = 0; i < C; i++){
		for (int j = 0; j < N; j++){
			ofbuf[j * C + i] = ocbuf[i][j];
		}
	}

	/* copy ofbuf[] to portaudio output buffer 
	 * both are in interleaved format
	 */
	for (int i = 0; i < N * C; i++){
		output[i] = ofbuf[i];
	}

	/* increment next_frame counter */
	p->next_frame += N;
	/* check if done */
	if (p->next_frame >= p->total_frame) {
		p->done = true;
	}

	return 0;
}

void print_filt(struct Filt *p)
{
	if (p->num_b > 0) {
		printf("FIR coefficients:\n");
		for (int i=0; i<p->num_b; i++)
			printf("%2d %12.8f\n", i, p->b[i]);
	}
	if (p->num_a > 0) {
		printf("IIR coefficients:\n");
		for (int i=0; i<p->num_a; i++)
			printf("%2d %12.8f\n", i, p->a[i]);
	}
}