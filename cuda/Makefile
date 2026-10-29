NVCC = nvcc
CFLAGS = -Wno-deprecated-gpu-targets -c -O3
LFLAGS = -Wno-deprecated-gpu-targets -lcudart -lcuda

all: assignment3.o rmm.o
	$(NVCC) $(LFLAGS) assignment3.o rmm.o -o assignment3

assignment3.o: assignment3.cu utility.h rmm.o
	$(NVCC) $(CFLAGS) $< -o $@

rmm.o: rmm.cu
	$(NVCC) $(CFLAGS) $< -o $@

clean:
	rm -f *.o assignment3