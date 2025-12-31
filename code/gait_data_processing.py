def read_pitch_data(filename):
    pitch_values = []
    
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if '/' in line:
                pitch, roll = line.split('/')
                pitch_values.append(float(pitch))
            elif line:
                pitch_values.append(float(line))
    return pitch_values


def motion_file(pitch_values, filename ="thigh_motionv2.mot"):
    time_interval = 0.5
    print(f"There are {len(pitch_values)} rows\n")
    with open(filename, 'w') as f:
        f.write("Coordinates\n")
        f.write("version = 1\n")
        f.write(f"nRows={len(pitch_values)}\n")
        f.write("nColumns=2\n")
        f.write("inDegrees=yes\n")
        f.write("endheader\n")
        f.write("time\thip_flexion_l\n")

        for i, pitch in enumerate(pitch_values):
            time = i * time_interval
            f.write(f"{time:.3f}\t{pitch:.3f}\n")
    print(f"Created OpenSim motion file: {filename}")

data  = read_pitch_data("pitch_angle")
motion_file(data)