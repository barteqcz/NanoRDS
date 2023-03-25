
# name of executable
name = micrords

# Set to 1 to enable RBDS, set to 0 to disable it
RBDS = 0

# Set to 1 to enable Stereo encoder, set to 0 to disable it.
STEREO = 1

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c18

CFLAGS += -DVERSION=\"$(VERSION)\"

obj = micrords.o waveforms.o rds.o fm_mpx.o control_pipe.o osc.o \
	resampler.o modulator.o lib.o ascii_cmd.o
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
