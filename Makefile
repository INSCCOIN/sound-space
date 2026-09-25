# SoundSpace — Linux framebuffer or SDL2 window
# Build on the machine that will run it.
#
#   sudo apt install -y build-essential libasound2-dev
#   make clean && make
#   ./soundspace
#
# Do not copy *.o or the binary from another machine. EM:62 = x86_64.

CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11 -D_DEFAULT_SOURCE -Isrc
LDFLAGS ?= -lm -lpthread

ALSA_H := $(shell test -f /usr/include/alsa/asoundlib.h && echo yes)
SDL_C  := $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2 2>/dev/null)
SDL_L  := $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2 2>/dev/null)

ifeq ($(ALSA_H),yes)
  CFLAGS  += -DHAVE_ALSA
  LDFLAGS += -lasound
endif
ifneq ($(SDL_L),)
  CFLAGS  += -DHAVE_SDL $(SDL_C)
  LDFLAGS += $(SDL_L)
endif

SRC = \
	src/main.c \
	src/fb.c \
	src/audio.c \
	src/fft.c \
	src/analyze.c \
	src/cloud.c \
	src/draw.c \
	src/input.c \
	src/picker.c

OBJ = $(SRC:.c=.o)
BIN = soundspace
HOST_ARCH := $(shell uname -m)

.PHONY: all clean rebuild check-arch

all: check-arch $(BIN)

# Drop objects built on a different CPU so `make` after a scp cannot link x86 .o
check-arch:
	@echo "soundspace: building on $(HOST_ARCH)"
	@for o in $(OBJ); do \
	  if [ -f $$o ]; then \
	    desc=$$(LC_ALL=C file $$o 2>/dev/null || true); \
	    case "$(HOST_ARCH)" in \
	      aarch64|arm64) \
	        echo "$$desc" | grep -qi 'x86-64\|Intel 80386\|EM: 62' && \
	          echo "dropping foreign object $$o" && rm -f $$o || true ;; \
	      x86_64|amd64|i686) \
	        echo "$$desc" | grep -qi 'ARM\|aarch64\|EM: 183' && \
	          echo "dropping foreign object $$o" && rm -f $$o || true ;; \
	    esac; \
	  fi; \
	done; true

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)
	@echo "built ./$(BIN)  ($(HOST_ARCH)$(if $(filter yes,$(ALSA_H)), +ALSA,)$(if $(SDL_L), +SDL,))"

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

rebuild: clean all
