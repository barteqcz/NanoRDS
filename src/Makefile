# name of executable
name = micrords

# change those values to "1" to enable RBDS and Stereo
# (don't forget to run "make clean" first)
RBDS = 0
STEREO = 0

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c18

obj = micrords.o waveforms.o rds.o fm_mpx.o control_pipe.o osc.o resampler.o modulator.o lib.o ascii_cmd.o
libs = -lm -lsamplerate -lpthread -lao

ifeq ($(RBDS), 1)
	CFLAGS += -DRBDS
endif

ifeq ($(STEREO), 1)
	CFLAGS += -DSTEREO
endif

ifeq ($(CONTROL_PIPE_MESSAGES), 1)
	CFLAGS += -DCONTROL_PIPE_MESSAGES
endif

all: $(name)

$(name): $(obj)
	$(CC) $(obj) $(libs) -o $(name) -s

clean:
	rm -f *.o
