from flask import Flask, render_template, request, jsonify
import serial

app = Flask(__name__)

# Configuración del puerto serie
try:
    ser = serial.Serial('/dev/serial0', 9600, timeout=1)
    ser.reset_input_buffer()
    print("--- Puerto serie /dev/serial0 conectado ---")
except Exception as e:
    print(f"--- ERROR SERIAL: {e} ---")
    ser = None

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/control', methods=['POST'])
def control_sistema():
    # Capturamos los datos tal cual vienen en tu log
    data = request.get_json(silent=True) or request.form
    print(f"Log: Datos recibidos -> {data}")
    
    # Extraemos 'action' y 'duty' según tus logs
    action = data.get('action') # Viene como 'ON' u 'OFF'
    duty = data.get('duty')     # Viene como el número del slider
    
    if ser:
        # 1. Procesamos el encendido/apagado
        if action == 'ON':
            ser.write(b"S1\n")
        elif action == 'OFF':
            ser.write(b"S0\n")
            
        # 2. Procesamos el Duty Cycle si viene en la misma petición
        if duty is not None:
            mensaje_duty = f"D{duty}\n"
            ser.write(mensaje_duty.encode())
            
        ser.flush()
        print(f"Enviado a Tiva -> Action: {action}, Duty: {duty}")
        return jsonify({"status": "ok"}), 200
    
    return jsonify({"status": "error", "message": "Serial no disponible"}), 500

# Mantenemos esta ruta por si el slider dispara a otra dirección
@app.route('/api/set_duty', methods=['POST'])
def set_duty():
    data = request.get_json(silent=True) or request.form
    duty = data.get('duty')
    if ser and duty is not None:
        mensaje = f"D{duty}\n"
        ser.write(mensaje.encode())
        ser.flush()
        return jsonify({"status": "ok"}), 200
    return jsonify({"status": "error"}), 400

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)