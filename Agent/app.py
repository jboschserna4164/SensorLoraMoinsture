from flask import Flask, jsonify, request, render_template_string, abort
import os
import json
from collections import OrderedDict

app = Flask(__name__)
DATA_FOLDER = 'data'
os.makedirs(DATA_FOLDER, exist_ok=True)

@app.route('/')
def index():
    sensores = [f[:-5] for f in os.listdir(DATA_FOLDER) if f.endswith('.json')]
    sensores.sort()
    template = '''
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<title>Sensores disponibles</title>
</head>
<body>
<h1>Listado de sensores registrados</h1>
        {% if sensores %}
<ul>
            {% for s in sensores %}
<li><a href="/{{ s }}">{{ s }}</a></li>
            {% endfor %}
</ul>
        {% else %}
<p>No hay sensores registrados aún.</p>
        {% endif %}
</body>
</html>
    '''
    return render_template_string(template, sensores=sensores)

@app.route('/<sensor_id>', methods=['GET'])
def ver_sensor(sensor_id):
    path = os.path.join(DATA_FOLDER, f"{sensor_id}.json")
    if not os.path.isfile(path):
        abort(404, description="Sensor no encontrado")
    with open(path, 'r') as f:
        data = json.load(f)
    return jsonify(data)

@app.route('/recibir', methods=['POST'])
def recibir():
    raw = request.get_json(force=True)

    sensor_id = raw.get("id", "").replace(":", "_")
    tipo = raw.get("type", "")
    datos = raw.get(tipo, {})
    valor = datos.get("value", None)

    if not sensor_id or not tipo or valor is None:
        return jsonify({"error": "Faltan campos obligatorios"}), 400

    # Construcción explícita del JSON con orden forzado
    ordenado = OrderedDict()
    ordenado["id"] = sensor_id
    ordenado["type"] = tipo
    ordenado[tipo] = OrderedDict([
        ("value", valor),
        ("type", "Float")
    ])
    ordenado["timestamp"] = raw.get("timestamp", "")

    filepath = os.path.join(DATA_FOLDER, f"{sensor_id}.json")
    with open(filepath, 'w') as f:
        json.dump(ordenado, f, indent=2)

    return jsonify({"status": "ok", "message": f"Dato de {sensor_id} guardado"}), 200
