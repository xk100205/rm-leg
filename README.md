# rm-leg

Phase 1 is keyboard teleop of the wheel motors only. The firmware can sit in
`TEST`, `TELEOP`, `ESTOP`, or `FAULT`. It has no balance mode, and leg
selectors `SEL 1`..`SEL 4` are rejected.

## UART

STM32H7 UART5, 115200 8N1, no flow control.

| Signal | Pin | Alternate function |
| --- | --- | --- |
| UART5_TX | PC12 | AF8 |
| UART5_RX | PB12 | AF14 |

PC12 is the board transmit pin. Connect it to the USB-UART adapter RX, and
PB12 to the adapter TX. Common ground.

There is no CubeMX `.ioc` in this tree. FDCAN1 is the wheel bus (classic
CAN, 1 Mbps, 11-bit ids). FDCAN2 is not used. FDCAN1 pin mux is not in this
tree; set it in Cube before the bus is connected. `Board_CanTxStd` only
queues a frame on `hfdcan1`.

## Placeholders (change before power)

Defined in `Core/Inc/wheel_leg_config.h`:

| Constant | Value | Meaning |
| --- | --- | --- |
| `MF9025_NODE_ID_LEFT` | 5 | CAN node for `SEL 5` |
| `MF9025_NODE_ID_RIGHT` | 6 | CAN node for `SEL 6` |
| `MF9025_RIGHT_WHEEL_SIGN` | -1 | Applied only to `SEL 6` |
| `MF9025_A2_MAX_TORQUE_CURRENT_A_PLACEHOLDER` | 0 A | Set this Max Torque Current in the LK tool. It is not a byte in the `0xA2` frame. |
| `TELEOP_SPD_MAX_DPS_PLACEHOLDER` | 360 | Firmware rejects a faster `SPD` |
| `MF9025_CAN_CMD_BASE` | `0x140` | LK command id, so node 5 is CAN id `0x145` |

Command `0xA2` is speed in 0.01 dps/LSB. Command `0x81` is the stop sent to
both wheels by `Safety_ApplyStop`. The run/resume command is not sent.

## Host checks

```sh
make test
```

`gcc` builds `build/test_phase1` and runs it. The keyboard mapping is checked
with `python3 tools/keyboard_teleop.py --self-test`. Neither step needs the
board. `arm-none-eabi-gcc` is not required for these checks, and this tree
does not link a flash image.

## Firmware in a Cube project

1. Generate an STM32H7 project (H723/H743 class) with UART5 and FDCAN1.
   Leave FDCAN2 off. Set the HAL time base to 1 ms.
2. FDCAN1: classic CAN, 1 Mbps, standard identifiers, TX FIFO enabled.
   Program the FDCAN1 pins in Cube. This tree does not mux them.
3. Add these sources beside Cube's own `main.c` (do not replace it):
   `app.c`, `board.c`, `fsm.c`, `mf9025.c`, `rs01.c`, `safety.c`,
   `teleop_uart.c`, `wheel_leg_hooks.c`, and the headers in `Core/Inc`.
4. Call `WheelLeg_BringUp()` after the clocks are up. It initializes the
   teleop state and UART5. Skip `Board_DebugUartInit` only if Cube already
   programs UART5 with the pins above; still call `WheelLeg_Init()` and arm
   the RX interrupt.
5. `wheel_leg_hooks.c` overrides the HAL SysTick and UART RX callbacks.
   If Cube already defines those, delete this file's copies and call
   `App_Tick1kHz`, `Board_DebugUartOnRxCplt`, and `Board_DebugUartOnError`
   from the existing ones. UART5's IRQ must preempt the 1 ms tick.
6. Cube must be the one definition of `huart5` and `hfdcan1`.
7. Do not enable leg outputs.
8. Flash with STM32CubeProgrammer. No hex is produced here.

## Keyboard script

```sh
python3 tools/keyboard_teleop.py --list-devices
python3 tools/keyboard_teleop.py --device /dev/input/eventX --port /dev/ttyUSB0 --verbose
```

The process needs access to the input device and the tty. Omit `--port` to
print lines without opening a serial port. Ctrl-C sends `ESTOP`.

After power-up the deadman latches at 100 ms with no valid line. Press `C`,
then `T`, then hold `A` or `D`. Releasing the direction key sends `ESTOP`.
Press `C` and `T` again before the next motion. `W`/`S` change speed by
30 dps. `1`..`4` select a leg on the link and immediately send `ESTOP`.
