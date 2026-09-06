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
        raw = ser.readline()
        if not raw:
            return None
        try:
            return raw.decode("ascii", errors="ignore").strip()
        except UnicodeDecodeError:
            return None

    def update(_frame):
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
    ani = animation.FuncAnimation(fig, update, interval=200, blit=False, cache_frame_data=False)
    plt.tight_layout()
    plt.show()

    ser.close()


if __name__ == "__main__":
    main()
