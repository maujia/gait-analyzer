import socket
import threading

HOST = '0.0.0.0'
PORT = 8082

esp32_angles = {1: None, 2: None}
lock = threading.Lock()
time_interval = 0.05  # 50 ms between rows
time_counter = 0.0
connected = {1: False, 2: False}

# Open text file for logging
log_file = open("pitch_angle.txt", "a")

def handle_client(conn, addr, client_id):
    global time_counter
    print(f"ESP32 {client_id} connected successfully from {addr}. Current connections: {connected}", flush=True)
    while True:
        data = conn.recv(1024)
        if not data:
            print(f"ESP32 {client_id} disconnected", flush=True)
            connected[client_id] = False
            break
        
        try:
            angle = float(data.decode('utf-8').strip())
        except ValueError:
            continue
        
        print(f"ESP32 {client_id}: {angle}°")
        
        with lock:
            esp32_angles[client_id] = angle
            
            # Only log when both ESP32s have valid values
            if esp32_angles[1] is not None and esp32_angles[2] is not None:
                # Log to text file: time ESP321/ESP322
                log_file.write(f"{time_counter:.3f} {esp32_angles[1]:.2f}/{esp32_angles[2]:.2f}\n")
                log_file.flush()
                
                # Print angle difference for debugging
                result = esp32_angles[1] - esp32_angles[2]
                print(f">>> Angle difference: {result}°")
                
                # Increment time counter
                time_counter += time_interval
    
    conn.close()

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen(2)
    print(f"Server listening on {HOST}:{PORT}")
    
    client_num = 0
    while True:
        conn, addr = s.accept()
        client_num += 1
        threading.Thread(target=handle_client, args=(conn, addr, client_num), daemon=True).start()
