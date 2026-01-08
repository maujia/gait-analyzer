import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# --- load data from text file ---
# Each line: timestamp_ms thigh_deg back_deg rel_deg
t_ms, thigh_deg, back_deg, rel_deg = np.loadtxt(
    "data.txt", unpack=True
)

# use relative angle for visualization
angles = np.deg2rad(rel_deg)   # convert to radians

# --- set up plot ---
fig, ax = plt.subplots()
ax.set_aspect('equal')
ax.set_xlim(-1.2, 1.2)
ax.set_ylim(-1.2, 1.2)
ax.set_xlabel("X")
ax.set_ylabel("Y")
ax.set_title("Thigh angle")

# vertical reference line (hip to straight-down leg)
ax.plot([0, 0], [0, -1.0], 'k--', linewidth=1)

# moving thigh line (hip at origin)
line, = ax.plot([], [], 'r-', linewidth=3)

def init():
    line.set_data([], [])
    return line,

def update(frame):
    theta = angles[frame]      # radians, positive = clockwise by your convention
    # start from vertical down (-pi/2) and rotate by theta
    base = -np.pi / 2.0
    phi = base + theta

    x = [0.0, np.cos(phi)]
    y = [0.0, np.sin(phi)]
    line.set_data(x, y)
    return line,

ani = FuncAnimation(
    fig, update, frames=len(angles),
    init_func=init, blit=True, interval=40
)

plt.show()
