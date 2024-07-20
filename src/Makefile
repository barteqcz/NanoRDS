# RBDS - set to 1 for calculating PI from callsign
RBDS = 0

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c18

obj = nanords.o waveforms.o rds.o fm_mpx.o control_pipe.o osc.o resampler.o modulator.o lib.o ascii_cmd.o
libs = -lm -lsamplerate -lpthread -lao

ifeq ($(RBDS), 1)
	CFLAGS += -DRBDS
endif

all: nanords

nanords: $(obj)
	$(CC) $(obj) $(libs) -o nanords -s

clean:
	rm -f *.o nanords

install: check_root nanords
	chmod 755 nanords
	cp -f nanords /usr/local/bin/nanords

uninstall: check_root
	rm -f /usr/local/bin/nanords

check_root:
	@if [ "$$(id -u)" -ne 0 ]; then \
	    echo "To install or uninstall the program, run make as root (e.g. 'sudo make install' or 'sudo make uninstall')"; \
	    exit 1; \
	fi
