import os
import threading
import time

from flask import Flask, render_template, request, jsonify
from strobe_engine import StrobeEngine
from audio_engine import AudioEngine
import logging

# Configure Logger
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger('StrobeApp')

app = Flask(__name__)

# Paths
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
SESSIONS_YAML = os.path.join(DATA_DIR, "sessions.yaml")

strobe_engine = None
audio_engine = None


def init_engines():
    global strobe_engine, audio_engine
    if strobe_engine is None:
        strobe_engine = StrobeEngine(SESSIONS_YAML, DATA_DIR)
        audio_engine = AudioEngine()

        # Sync loop
        def sync_worker():
            while True:
                current_pos = audio_engine.get_current_pos()
                strobe_engine.send_update(current_pos, audio_engine.is_playing)
                time.sleep(0.05)  # 20Hz update rate for strobe params

        threading.Thread(target=sync_worker, daemon=True).start()


@app.route('/')
def index():
    return render_template('index.html', sessions=strobe_engine.sessions['blocks'])


@app.route('/select_session', methods=['POST'])
def select_session():
    data = request.json
    sound_rel_path = data.get('sound')
    strobe_rel_path = data.get('strobe')

    # Paths are relative to sounds_dir and strobes_dir in sessions.yaml
    sounds_dir = strobe_engine.sessions['configuration']['sounds_dir']

    sound_full_path = os.path.join(DATA_DIR, sounds_dir, sound_rel_path)

    if audio_engine.load_track(sound_full_path):
        if strobe_rel_path:
            strobe_engine.load_session_data(strobe_rel_path)
        return jsonify({"status": "success"})
    return jsonify({"status": "error", "message": "Failed to load track"}), 400


@app.route('/play', methods=['POST'])
def play():
    audio_engine.play()
    return jsonify({"status": "playing"})


@app.route('/pause', methods=['POST'])
def pause():
    audio_engine.pause()
    return jsonify({"status": "paused"})


@app.route('/seek', methods=['POST'])
def seek():
    pos = float(request.json.get('pos', 0))
    audio_engine.seek(pos)
    return jsonify({"status": "seeked", "pos": pos})


@app.route('/update_params', methods=['POST'])
def update_params():
    data = request.json
    if 'brightness' in data:
        strobe_engine.brightness = float(data['brightness']) / 100.0
    if 'color' in data:
        strobe_engine.color = data['color']  # [R, G, B, W]
    if 'enabled' in data:
        strobe_engine.enabled = bool(data['enabled'])
    if 'bitmask' in data:
        strobe_engine.bitmask = int(data['bitmask'])

    return jsonify({"status": "updated"})


@app.route('/status')
def status():
    return jsonify({
        "is_playing": audio_engine.is_playing,
        "current_pos": audio_engine.get_current_pos(),
        "duration": audio_engine.duration,
        "track": audio_engine.current_track,
        "brightness": strobe_engine.brightness * 100,
        "color": strobe_engine.color,
        "enabled": strobe_engine.enabled,
        "bitmask": strobe_engine.bitmask,
        "mask_ip": strobe_engine.controller.board_ip
    })


if __name__ == '__main__':
    init_engines()
    app.run(host='0.0.0.0', port=5000, debug=True, use_reloader=False)
