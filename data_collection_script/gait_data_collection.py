import serial 
import time

with serial.Serial("COM27", 115200, timeout=1) as ser:
    with open("motion.txt", "a", newline="") as f:
        while True:
            line = ser.readline().decode("utf-8", errors="replace")
            if line:
                f.write(line)
                f.flush()
    
# original function for parsing the data once its recorded in the text file
# def read_pitch_data(filename, baud_rate):
#     pitch_values = []
#     with open(filename, 'a') as f:
#         for line in f:
#             line = line.strip()
#             if '/' in line:
#                 pitch, roll = line.split('/')
#                 pitch_values.append(float(pitch))
#             elif line:
#                 pitch_values.append(float(line))
#     return pitch_values

# func for converting the .txt file to a .mot file for OpenSim
# def motion_file(pitch_values, filename ="thigh_motionv2.mot"):
#     time_interval = 0.5
#     print(f"There are {len(pitch_values)} rows\n")
#     with open(filename, 'w') as f:
#         f.write("Coordinates\n")
#         f.write("version = 1\n")
#         f.write(f"nRows={len(pitch_values)}\n")
#         f.write("nColumns=2\n")
#         f.write("inDegrees=yes\n")
#         f.write("endheader\n")
#         f.write("time\thip_flexion_l\n")

#         for i, pitch in enumerate(pitch_values):
#             time = i * time_interval
#             f.write(f"{time:.3f}\t{pitch:.3f}\n")
#     print(f"Created OpenSim motion file: {filename}")

# data  = read_pitch_data("pitch_angle")
# motion_file(data)

