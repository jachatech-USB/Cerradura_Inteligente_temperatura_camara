from flask import Flask, request, jsonify
import mysql.connector
from datetime import datetime
import base64

app = Flask(__name__)

db_config = {
    'host': 'localhost',
    'user': 'Jacha-Tech',
    'password': 'KXz-Mxkt16OlNhzA',
    'database': 'cerradura_db'
}

@app.route('/verificar', methods=['POST'])
def verificar():
    print("Función verificar() llamada")
    data = request.get_json()
    
    if not data or 'contraseña' not in data:
        return jsonify({'error': 'No se proporcionó contraseña'}), 400

    contraseña_ingresada = data['contraseña']
    temperatura = data.get('temperatura', None)
    foto_base64 = data.get('foto', None)

    if foto_base64:
        try:
            foto_bytes = base64.b64decode(foto_base64)
        except Exception as e:
            return jsonify({'error': f'Error decodificando la foto: {str(e)}'}), 400
    else:
        foto_bytes = None

    conn = mysql.connector.connect(**db_config)
    cursor = conn.cursor()

    cursor.execute(
        "SELECT id, nombre, activo FROM Personas WHERE contraseña=%s",
        (contraseña_ingresada,)
    )
    resultado = cursor.fetchone()

    if resultado:
        persona_id, nombre_usuario, activo = resultado
        if activo == 1:
            exito = True
        else:
            exito = False
    else:
        persona_id = None
        exito = False

    if temperatura is not None:
        try:
            temp_val = float(temperatura)
            if temp_val > 37.5:
                exito = False
        except ValueError:
            pass 

    if foto_bytes and len(foto_bytes) > 0:
        cursor.execute(
            "SELECT COUNT(*) FROM registros WHERE persona_id=%s AND contraseña_ingresada=%s AND timestamp > NOW() - INTERVAL 10 SECOND",
            (persona_id, contraseña_ingresada) # type: ignore
        )
        ya_registrado = cursor.fetchone()[0] # type: ignore
        if ya_registrado == 0:
            cursor.execute(
                "INSERT INTO registros (persona_id, contraseña_ingresada, exito, timestamp, foto, temperatura) "
                "VALUES (%s, %s, %s, NOW(), %s, %s)",
                (persona_id, contraseña_ingresada, 1 if exito else 0, foto_bytes, temperatura) # type: ignore
            )
            conn.commit()
        else:
            print("Intento duplicado detectado, no se inserta")




    cursor.close()
    conn.close()

    if exito:
        return jsonify({'resultado': 'correcto', 'nombre': nombre_usuario})
    else:
        return jsonify({'resultado': 'incorrecto'})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
