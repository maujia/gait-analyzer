import socket
import threading

HOST = '0.0.0.0'
PORT = 8080

esp32_angles = {1: None, 2: None}
lock = threading.Lock()

def handle_client(conn, addr, client_id):
    print(f"ESP32 {client_id} connected from {addr}")
    while True:
        data = conn.recv(1024)
        if not data:
            break
        
        angle = float(data.decode('utf-8').strip())
        print(f"ESP32 {client_id}: {angle}°")
        
        with lock:
            esp32_angles[client_id] = angle
            if esp32_angles[1] is not None and esp32_angles[2] is not None:
                result = esp32_angles[1] - esp32_angles[2]
                print(f">>> Angle difference: {result}°")
    
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
