gcc -o filter2 -DFIR_FILT main2.c filter2.c paUtils.c \
	-Wall \
	-I/usr/local/include \
	-L/usr/local/lib -lsndfile -lportaudio
	