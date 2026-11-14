# Host checks for the phase-1 wheel teleop. The STM32 image is built from
# a CubeMX tree; see the README. This Makefile does not link firmware.

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -DWHEEL_LEG_HOST -ICore/Inc

HOST_SRCS = \
	Core/Src/board.c \
	Core/Src/mf9025.c \
	Core/Src/rs01.c \
	Core/Src/safety.c \
	Core/Src/teleop_uart.c \
	Core/Src/fsm.c \
	Core/Src/app.c \
	test/test_phase1.c

.PHONY: test test-firmware test-host check-scope clean

test: test-firmware test-host check-scope

test-firmware:
	mkdir -p build
	$(CC) $(CFLAGS) -o build/test_phase1 $(HOST_SRCS)
	./build/test_phase1

test-host:
	python3 tools/keyboard_teleop.py --self-test

check-scope:
	@if grep -nE 'RS01_SetSpeed' \
		Core/Src/fsm.c Core/Src/teleop_uart.c Core/Src/safety.c \
		Core/Src/mf9025.c Core/Src/app.c Core/Src/wheel_leg_hooks.c; then \
		exit 1; \
	fi
	@if grep -nR -E 'SetTorque|TorqueCmd|BALANCE|NM_PER_LSB' \
		--include='*.c' --include='*.h' --include='*.py' Core test tools; then \
		exit 1; \
	fi

clean:
	rm -rf build
