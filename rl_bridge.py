import socket
import time

# Gelen veri kanalı (ns-3 -> Python)
IN_IP, IN_PORT = "127.0.0.1", 5555
in_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
in_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
in_sock.bind((IN_IP, IN_PORT))

# Karar gönderim kanalı (Python -> ns-3)
OUT_IP, OUT_PORT = "127.0.0.1", 5556
out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# RL Hafızası
q_values = {str(i): 0.5 for i in range(9)}
alpha = 0.1

print(f"--- [AI AJANI] Karar Merkezi Baslatildi ---")

try:
    while True:
        data, addr = in_sock.recvfrom(1024)
        msg = data.decode('utf-8').split(',')
        node_id, rssi = msg[0], float(msg[1])

        # Ödül/Ceza Mantığı
        reward = 1.0 if rssi > -72 else (-1.0 if rssi < -78 else 0.1)
        
        # Q-Skoru Güncelleme
        q_values[node_id] += alpha * (reward - q_values[node_id])

        # Karar: En yüksek puana sahip düğümü ns-3'e fırlat
        best_node = max(q_values, key=q_values.get)
        out_sock.sendto(best_node.encode(), (OUT_IP, OUT_PORT))
        
        # Her 10 pakette bir durumu yazdır
        if int(time.time() * 10) % 5 == 0: 
             print(f"Analiz: Dugum {node_id} RSSI: {rssi} | Karar: {best_node} uzerinden git.")

except KeyboardInterrupt:
    print("\nAjan durduruldu.")
except Exception as e:
    print(f"Hata: {e}")
