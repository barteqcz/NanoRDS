
# name of executable
name = micrords

# version
VERSION=1.0

# NRSC RBDS (FCC)
# Uncomment for NRSC LF/MF AF coding and PTY list
RBDS = 1

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c18

CFLAGS += -DVERSION=\"$(VERSION)\"

obj = micrords.o waveforms.o rds.o fm_mpx.o control_pipe.o osc.o \
	resampler.o modulator.o lib.o ascii_cmd.o
libs = -lm -lsamplerate -lpthread -lao

ifeq ($(RBDS), 1)
	CFLAGS += -DRBDS
endif

ifeq ($(CONTROL_PIPE_MESSAGES), 1)
	CFLAGS += -DCONTROL_PIPE_MESSAGES
endif

all: $(name)

$(name): $(obj)
	$(CC) $(obj) $(libs) -o $(name) -s

clean:
	rm -f *.o
