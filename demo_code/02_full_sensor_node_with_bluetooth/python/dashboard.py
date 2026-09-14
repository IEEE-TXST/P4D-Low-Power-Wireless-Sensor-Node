#!/usr/bin/env python3
"""
P4-D live dashboard: reads "X,Y,Z,LIGHT\r\n" CSV lines arriving over the
HC-05's paired Bluetooth virtual serial port (see 02_full_sensor_node_
with_bluetooth/main.c) and plots accelerometer X/Y/Z and the light level
live, once per wake cycle (every 5 seconds by default).

Any line that isn't exactly four comma-separated integers is silently
skipped (the board's own startup banner text, for instance), not treated
as an error.

Usage:
    python3 dashboard.py <bluetooth-serial-port>

Examples:
    python3 dashboard.py /dev/tty.HC-05-DevB
    python3 dashboard.py COM7

Install dependencies first if needed:
    pip3 install pyserial matplotlib

Pair the HC-05 with your laptop first (Section 9 of the manual); the
paired module shows up as a normal serial port once paired, the same way
any other Bluetooth serial device would.
"""
# WHAT: A desktop companion to the P4-D sensor node firmware: reads its
# once-every-5-seconds CSV transmissions over a Bluetooth-backed serial
# port and plots accelerometer X/Y/Z and light level live.
#
# HOW: pyserial connects to the HC-05's paired virtual COM port exactly as
# if it were a wired serial connection (Bluetooth serial profiles are
# designed to look like a normal serial port to software); each received
# line is parsed and appended to rolling buffers, redrawn on a timer.
#
# WHY: This script's whole design is shaped by the sensor node being
# asleep almost all the time: unlike P3's continuous 10kHz stream or P4-A's
# request/response protocol, here a new data point only exists once every
# 5 seconds, so a much longer read timeout (6 seconds, see below) and a
# much coarser update interval are both correct here where they'd be wrong
# for those other projects' scripts.
import sys
import collections

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# Board wakes and transmits once every 5 seconds by default; keep enough
# history for a readable trend without the plot getting cluttered.
WINDOW_POINTS = 60  # 5 minutes of history at one point per 5-second wake


def parse_args():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 9600
    return port, baud


def main():
    port, baud = parse_args()
    # WHAT: Opens the Bluetooth serial connection with a 6-second read
    # timeout.
    # WHY: 6 seconds, deliberately longer than the board's 5-second wake
    # period: a read that waits the full timeout without receiving
    # anything is expected and normal here (the board just hasn't woken up
    # yet), not a sign anything is wrong; a short timeout tuned for a
    # continuously-streaming device (like P3's plot script) would report
    # constant, spurious "timeouts" against this intentionally-sleepy node.
    ser = serial.Serial(port, baud, timeout=6.0)  # generous timeout: the board only speaks once every 5 s

    x_data = collections.deque(maxlen=WINDOW_POINTS)
    y_data = collections.deque(maxlen=WINDOW_POINTS)
    z_data = collections.deque(maxlen=WINDOW_POINTS)
    light_data = collections.deque(maxlen=WINDOW_POINTS)

    fig, (ax_accel, ax_light) = plt.subplots(1, 2, figsize=(11, 4.5))
    fig.suptitle("P4-D: wireless sensor node, one reading every wake cycle")

    (x_line,) = ax_accel.plot([], [], label="X", color="tab:red")
    (y_line,) = ax_accel.plot([], [], label="Y", color="tab:green")
    (z_line,) = ax_accel.plot([], [], label="Z", color="tab:blue")
    ax_accel.set_title("Accelerometer (mg)")
    ax_accel.set_xlabel("wake cycle")
    ax_accel.set_ylim(-2000, 2000)
    ax_accel.legend(loc="upper right")

    (light_line,) = ax_light.plot([], [], color="tab:orange")
    ax_light.set_title("Light (ADC counts, 0-4095)")
    ax_light.set_xlabel("wake cycle")
    ax_light.set_ylim(0, 4095)

    def read_one_line():
        """Blocks up to the serial timeout waiting for one line; returns
        None on timeout (the board hasn't woken up yet) rather than
        raising, since a wake-every-5-seconds device is expected to go
        quiet between transmissions."""
        # WHAT: Reads one line, waiting up to the 6-second timeout, and
        # returns it as text, or None if nothing arrived or it couldn't be
        # decoded.
        # WHY: A plain timeout (readline returning empty) is treated as a
        # completely normal, expected outcome here, not an error to report,
        # since it just means the board is still asleep; that's a direct
        # consequence of this device's sleep-most-of-the-time design (see
        # the firmware's own EnterLlsUntilWake).
        raw = ser.readline()
        if not raw:
            return None
        try:
            return raw.decode("ascii", errors="ignore").strip()
        except UnicodeDecodeError:
            return None

    def update(_frame):
        # WHAT: Called on a timer; checks for one new line and, if a valid
        # one arrived, appends its four values to the rolling buffers and
        # redraws both plots.
        # HOW: Parses exactly 4 comma-separated integers (X, Y, Z, light);
        # anything else (wrong count, non-numeric) is treated as "not a
        # data line" and silently ignored, the same permissive-parsing
        # approach P3's and P4-A's plotting scripts use, for the same
        # reason: a corrupted or partial line over a live link shouldn't
        # crash the dashboard.
        line = read_one_line()
        if line:
            parts = line.split(",")
            if len(parts) == 4:
                try:
                    x, y, z, light = (int(p) for p in parts)
                except ValueError:
                    x = y = z = light = None  # not a data line; skip it
                if x is not None:
                    x_data.append(x)
                    y_data.append(y)
                    z_data.append(z)
                    light_data.append(light)

        x_line.set_data(range(len(x_data)), x_data)
        y_line.set_data(range(len(y_data)), y_data)
        z_line.set_data(range(len(z_data)), z_data)
        ax_accel.set_xlim(0, max(len(x_data), 1))

        light_line.set_data(range(len(light_data)), light_data)
        ax_light.set_xlim(0, max(len(light_data), 1))

        return x_line, y_line, z_line, light_line

    # interval matches roughly how often a new point could plausibly
    # arrive; read_one_line()'s own serial timeout does the real waiting.
    # WHAT: Starts the redraw timer at 200ms, much faster than data
    # actually arrives.
    # WHY: This interval only controls how often the plot's event loop
    # checks in, not how often real data shows up; read_one_line's own
    # 6-second timeout is what actually paces new points. A short interval
    # here just keeps the plot window responsive (e.g. to being resized or
    # closed) between the infrequent real updates.
    ani = animation.FuncAnimation(fig, update, interval=200, blit=False, cache_frame_data=False)
    plt.tight_layout()
    plt.show()

    ser.close()


if __name__ == "__main__":
    main()
