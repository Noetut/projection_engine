import os
import sys
import json
import re
import base64
import urllib.request
import subprocess
import numpy as np

FFMPEG_PATH = r"C:\Users\noeam\AppData\Local\Overwolf\Extensions\ncfplpkmiejjaklknfnkgcpapnhkggmlcppckhcb\270.0.25\obs\bin\64bit\ffmpeg.exe"
OUTPUT_DIR = r"d:\Proyectos\area_ciclogenica_display\audios"
CACHE_FILE = os.path.join(OUTPUT_DIR, ".soundfont_cache.json")
SAMPLE_RATE = 44100

def get_soundfont():
    if os.path.exists(CACHE_FILE):
        with open(CACHE_FILE, 'r', encoding='utf-8') as f:
            return json.load(f)
    print("Descargando SoundFont...")
    url = 'https://raw.githubusercontent.com/gleitz/midi-js-soundfonts/gh-pages/MusyngKite/acoustic_grand_piano-mp3.js'
    data = urllib.request.urlopen(url).read().decode('utf-8')
    start = data.find('{\n')
    end = data.rfind('}') + 1
    raw_json = re.sub(r',\s*\}', '}', data[start:end])
    parsed = json.loads(raw_json)
    with open(CACHE_FILE, 'w', encoding='utf-8') as f:
        json.dump(parsed, f)
    return parsed

soundfont = get_soundfont()

def decode_note(note_name):
    raw_b64 = soundfont[note_name].split(',')[1]
    mp3_bytes = base64.b64decode(raw_b64)
    cmd = [
        FFMPEG_PATH, "-y", "-i", "pipe:0",
        "-f", "s16le", "-acodec", "pcm_s16le",
        "-ar", str(SAMPLE_RATE), "-ac", "2", "pipe:1"
    ]
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, _ = p.communicate(input=mp3_bytes)
    return np.frombuffer(out, dtype=np.int16).reshape(-1, 2).astype(np.float32) / 32768.0

NOTE_CACHE = {}
def get_note_audio(note_name):
    if note_name not in NOTE_CACHE:
        NOTE_CACHE[note_name] = decode_note(note_name)
    return NOTE_CACHE[note_name]

def create_warm_reverb_ir(duration=3.0, decay=1.6):
    num_samples = int(duration * SAMPLE_RATE)
    t = np.linspace(0, duration, num_samples)
    envelope = np.exp(-t * 2.5 / decay)
    np.random.seed(123)
    left_noise = np.random.randn(num_samples) * envelope
    right_noise = np.random.randn(num_samples) * envelope
    freqs = np.fft.rfftfreq(num_samples, 1.0 / SAMPLE_RATE)
    # Calidez de sala de conciertos (corte progresivo de agudos para que no choque con la voz)
    lp_filter = 1.0 / (1.0 + (freqs / 2800.0) ** 2)
    left_filt = np.fft.irfft(np.fft.rfft(left_noise) * lp_filter, n=num_samples)
    right_filt = np.fft.irfft(np.fft.rfft(right_noise) * lp_filter, n=num_samples)
    ir = np.column_stack([left_filt, right_filt])
    return ir / (np.max(np.abs(ir)) + 1e-9)

REVERB_IR = create_warm_reverb_ir()

def compose_track():
    """
    Composicion musical de 90 segundos (~1 minuto y medio) en Do menor.
    Estilo: Piano ambiental cinematografico / minimalista y calido.
    Disenado especificamente como pista de fondo para voz hablada (youtube/documental).
    Incorpora sutiles motivos de la espiral y el barrido.
    """
    events = [] # lista de (nota, t_segundos, volumen, paneo)
    
    bpm = 64.0
    beat = 60.0 / bpm         # ~0.9375 seg por pulso
    bar_len = beat * 4.0      # ~3.75 seg por compas
    # 24 compases = 24 * 3.75 = 90.0 segundos exactos (+ fade/decay de 4s = ~94s)
    
    def add_note(note, bar, beat_offset, vol, pan=0.0):
        t = (bar - 1) * bar_len + beat_offset * beat
        events.append((note, t, vol, pan))
        
    def add_arpeggio(pattern, bar, vol_scale=1.0):
        # pattern: list of (note, beat_offset, vol, pan)
        for note, b_off, vol, pan in pattern:
            add_note(note, bar, b_off, vol * vol_scale, pan)

    # =========================================================================
    # INTRO (Compases 1 - 4): Atmosfera inicial sutil
    # =========================================================================
    # Compas 1: Cm (Do menor etereo)
    add_note('C2', 1, 0.0, 0.48, -0.15)
    add_note('G2', 1, 0.0, 0.42, -0.10)
    add_note('Eb3', 1, 0.5, 0.38, -0.20)
    add_note('G3', 1, 1.0, 0.39, +0.10)
    add_note('C4', 1, 1.5, 0.40, +0.20)
    add_note('D4', 1, 2.0, 0.42, +0.25) # novena suave
    add_note('Eb4', 1, 2.5, 0.41, +0.15)
    add_note('G3', 1, 3.0, 0.36, -0.10)
    add_note('C4', 1, 3.5, 0.38, +0.10)

    # Compas 2: Ab maj7 (Calido y expansivo)
    add_note('Ab1', 2, 0.0, 0.48, -0.20)
    add_note('Eb2', 2, 0.0, 0.42, -0.15)
    add_note('C3', 2, 0.5, 0.38, -0.15)
    add_note('Eb3', 2, 1.0, 0.39, +0.10)
    add_note('G3', 2, 1.5, 0.42, +0.20)
    add_note('C4', 2, 2.0, 0.43, +0.25)
    add_note('Eb4', 2, 2.5, 0.40, +0.15)
    add_note('G3', 2, 3.0, 0.36, -0.10)
    add_note('C4', 2, 3.5, 0.38, +0.10)

    # Compas 3: Eb add9 (Luminoso)
    add_note('Eb2', 3, 0.0, 0.48, -0.15)
    add_note('Bb2', 3, 0.0, 0.42, -0.10)
    add_note('G3', 3, 0.5, 0.38, -0.15)
    add_note('Bb3', 3, 1.0, 0.39, +0.10)
    add_note('Eb4', 3, 1.5, 0.41, +0.20)
    add_note('F4', 3, 2.0, 0.43, +0.25) # novena de Eb
    add_note('G4', 3, 2.5, 0.42, +0.15)
    add_note('Bb3', 3, 3.0, 0.36, -0.10)
    add_note('Eb4', 3, 3.5, 0.38, +0.10)

    # Compas 4: Bb sus4 -> Bb
    add_note('Bb1', 4, 0.0, 0.48, -0.20)
    add_note('F2', 4, 0.0, 0.42, -0.15)
    add_note('D3', 4, 0.5, 0.38, -0.15)
    add_note('F3', 4, 1.0, 0.39, +0.10)
    add_note('Bb3', 4, 1.5, 0.40, +0.20)
    add_note('C4', 4, 2.0, 0.42, +0.25)
    add_note('D4', 4, 2.5, 0.40, +0.15)
    add_note('F3', 4, 3.0, 0.36, -0.10)
    add_note('Bb3', 4, 3.5, 0.38, +0.10)

    # =========================================================================
    # PARTE 1 (Compases 5 - 8): Flujo tranquilo y melodico
    # =========================================================================
    # Compas 5: Cm9 con melodia de voz suave
    add_note('C2', 5, 0.0, 0.50, -0.15)
    add_note('G2', 5, 0.0, 0.44, -0.10)
    add_note('Eb3', 5, 0.5, 0.38, -0.15)
    add_note('G3', 5, 1.0, 0.39, +0.10)
    add_note('D4', 5, 1.5, 0.42, +0.20)
    add_note('Eb4', 5, 2.0, 0.44, +0.25)
    add_note('G4', 5, 2.0, 0.46, +0.10) # melodia alta suave
    add_note('Eb4', 5, 2.5, 0.38, +0.15)
    add_note('D4', 5, 3.0, 0.40, +0.20)
    add_note('C4', 5, 3.5, 0.42, +0.10)

    # Compas 6: Ab maj7
    add_note('Ab1', 6, 0.0, 0.50, -0.20)
    add_note('Eb2', 6, 0.0, 0.44, -0.15)
    add_note('C3', 6, 0.5, 0.38, -0.15)
    add_note('Eb3', 6, 1.0, 0.39, +0.10)
    add_note('G3', 6, 1.5, 0.40, +0.20)
    add_note('C4', 6, 2.0, 0.42, +0.25)
    add_note('Eb4', 6, 2.0, 0.46, +0.15)
    add_note('D4', 6, 2.75, 0.38, +0.10)
    add_note('C4', 6, 3.25, 0.40, +0.05)

    # Compas 7: Eb add9
    add_note('Eb2', 7, 0.0, 0.50, -0.15)
    add_note('Bb2', 7, 0.0, 0.44, -0.10)
    add_note('G3', 7, 0.5, 0.38, -0.15)
    add_note('Bb3', 7, 1.0, 0.39, +0.10)
    add_note('Eb4', 7, 1.5, 0.41, +0.20)
    add_note('F4', 7, 2.0, 0.43, +0.25)
    add_note('Bb4', 7, 2.0, 0.46, +0.30)
    add_note('G4', 7, 3.0, 0.42, +0.20)
    add_note('F4', 7, 3.5, 0.38, +0.15)

    # Compas 8: Bb
    add_note('Bb1', 8, 0.0, 0.50, -0.20)
    add_note('F2', 8, 0.0, 0.44, -0.15)
    add_note('D3', 8, 0.5, 0.38, -0.15)
    add_note('F3', 8, 1.0, 0.39, +0.10)
    add_note('Bb3', 8, 1.5, 0.40, +0.20)
    add_note('D4', 8, 2.0, 0.43, +0.25)
    add_note('F4', 8, 2.5, 0.41, +0.15)
    add_note('D4', 8, 3.0, 0.38, +0.20)
    add_note('Bb3', 8, 3.5, 0.36, +0.10)

    # =========================================================================
    # PARTE 2 (Compases 9 - 12): Guiño tematico al BARRIDO (Segmento 1)
    # =========================================================================
    # Compas 9: Cm9
    add_note('C2', 9, 0.0, 0.50, -0.15)
    add_note('G2', 9, 0.0, 0.44, -0.10)
    add_note('Eb3', 9, 0.5, 0.38, -0.15)
    add_note('G3', 9, 1.0, 0.39, +0.10)
    add_note('C4', 9, 1.5, 0.41, +0.20)
    add_note('Eb4', 9, 2.0, 0.43, +0.25)
    add_note('G4', 9, 2.5, 0.45, +0.20)
    add_note('C5', 9, 3.0, 0.47, +0.10)
    add_note('Bb4', 9, 3.5, 0.41, +0.05)

    # Compas 10: Fm9 (Fa menor profundo y sosegado)
    add_note('F1', 10, 0.0, 0.48, -0.25)
    add_note('C2', 10, 0.0, 0.42, -0.20)
    add_note('Ab2', 10, 0.5, 0.38, -0.15)
    add_note('C3', 10, 1.0, 0.39, +0.10)
    add_note('Eb3', 10, 1.5, 0.40, +0.15)
    add_note('Ab3', 10, 2.0, 0.42, +0.20)
    add_note('C4', 10, 2.5, 0.43, +0.25)
    add_note('Eb4', 10, 3.0, 0.44, +0.15)
    add_note('Ab4', 10, 3.5, 0.46, +0.20)

    # Compas 11: Ab maj7 con Motivo BARRIDO sutil (3 notas -> 2 -> 2 -> 2 -> 1)
    add_note('Ab1', 11, 0.0, 0.50, -0.20)
    add_note('Eb2', 11, 0.0, 0.44, -0.15)
    add_note('C3', 11, 0.5, 0.38, -0.10)
    add_note('Eb3', 11, 1.0, 0.40, +0.10)
    # Motivo de barrido en el fondo (tocado muy suave como gotas de luz):
    # 3 notas:
    add_note('C4', 11, 1.50, 0.36, -0.30)
    add_note('Eb4', 11, 1.62, 0.38, -0.25)
    add_note('G4', 11, 1.74, 0.40, -0.20)
    # 2 notas:
    add_note('Bb4', 11, 2.10, 0.42, -0.05)
    add_note('C5', 11, 2.22, 0.44, +0.05)
    # 2 notas:
    add_note('D5', 11, 2.60, 0.45, +0.20)
    add_note('Eb5', 11, 2.72, 0.46, +0.25)
    # 2 notas:
    add_note('G5', 11, 3.10, 0.48, +0.35)
    add_note('Bb5', 11, 3.22, 0.49, +0.40)
    # 1 nota cumbre:
    add_note('C6', 11, 3.60, 0.50, +0.50)

    # Compas 12: G sus4 -> G (Resolucion a la dominante, dejando flotar la cumbre)
    add_note('G1', 12, 0.0, 0.50, -0.15)
    add_note('D2', 12, 0.0, 0.44, -0.10)
    add_note('G2', 12, 0.5, 0.38, -0.10)
    add_note('C3', 12, 1.0, 0.40, +0.10)
    add_note('D3', 12, 1.5, 0.41, +0.15)
    add_note('G3', 12, 2.0, 0.42, +0.20)
    add_note('B3', 12, 2.5, 0.44, +0.15) # resolucion mayor dulce
    add_note('D4', 12, 3.0, 0.42, +0.10)
    add_note('G4', 12, 3.5, 0.40, +0.05)

    # =========================================================================
    # PARTE 3 (Compases 13 - 16): Seccion central envolvente y muy calmada
    # =========================================================================
    # Compas 13: Cm
    add_note('C2', 13, 0.0, 0.46, -0.15)
    add_note('G2', 13, 0.0, 0.40, -0.10)
    add_note('Eb3', 13, 0.75, 0.36, -0.15)
    add_note('G3', 13, 1.5, 0.38, +0.10)
    add_note('C4', 13, 2.25, 0.40, +0.20)
    add_note('Eb4', 13, 3.0, 0.42, +0.15)
    add_note('D4', 13, 3.5, 0.38, +0.10)

    # Compas 14: Ab maj7
    add_note('Ab1', 14, 0.0, 0.46, -0.20)
    add_note('Eb2', 14, 0.0, 0.40, -0.15)
    add_note('C3', 14, 0.75, 0.36, -0.15)
    add_note('Eb3', 14, 1.5, 0.38, +0.10)
    add_note('G3', 14, 2.25, 0.40, +0.20)
    add_note('C4', 14, 3.0, 0.42, +0.15)

    # Compas 15: Eb add9
    add_note('Eb2', 15, 0.0, 0.46, -0.15)
    add_note('Bb2', 15, 0.0, 0.40, -0.10)
    add_note('G3', 15, 0.75, 0.36, -0.15)
    add_note('Bb3', 15, 1.5, 0.38, +0.10)
    add_note('F4', 15, 2.25, 0.40, +0.20)
    add_note('G4', 15, 3.0, 0.42, +0.15)

    # Compas 16: Bb
    add_note('Bb1', 16, 0.0, 0.46, -0.20)
    add_note('F2', 16, 0.0, 0.40, -0.15)
    add_note('D3', 16, 0.75, 0.36, -0.15)
    add_note('F3', 16, 1.5, 0.38, +0.10)
    add_note('Bb3', 16, 2.25, 0.40, +0.20)
    add_note('D4', 16, 3.0, 0.41, +0.15)

    # =========================================================================
    # PARTE 4 (Compases 17 - 20): Guiño tematico a la ESPIRAL (Segmento 6)
    # =========================================================================
    # Compas 17: Cm9
    add_note('C2', 17, 0.0, 0.48, -0.15)
    add_note('G2', 17, 0.0, 0.42, -0.10)
    add_note('Eb3', 17, 0.5, 0.36, -0.15)
    add_note('G3', 17, 1.0, 0.38, +0.10)
    add_note('C4', 17, 1.5, 0.40, +0.20)
    add_note('D4', 17, 2.0, 0.42, +0.25)
    add_note('Eb4', 17, 2.5, 0.43, +0.15)
    add_note('G4', 17, 3.0, 0.45, +0.10)
    add_note('C5', 17, 3.5, 0.47, +0.05)

    # Compas 18: Ab maj7
    add_note('Ab1', 18, 0.0, 0.48, -0.20)
    add_note('Eb2', 18, 0.0, 0.42, -0.15)
    add_note('C3', 18, 0.5, 0.36, -0.15)
    add_note('Eb3', 18, 1.0, 0.38, +0.10)
    add_note('G3', 18, 1.5, 0.40, +0.20)
    add_note('C4', 18, 2.0, 0.42, +0.25)
    add_note('Eb4', 18, 2.5, 0.44, +0.15)
    add_note('G4', 18, 3.0, 0.46, +0.20)
    add_note('Bb4', 18, 3.5, 0.48, +0.10)

    # Compas 19: Motivo de la ESPIRAL circular sutil (notas rapidas a 0.1s en paneo)
    add_note('Eb2', 19, 0.0, 0.48, -0.15)
    add_note('Bb2', 19, 0.0, 0.42, -0.10)
    # Secuencia espiral en Do menor (delicado, volumen de fondo ~0.35):
    add_note('C4', 19, 0.50, 0.34, -0.60) # ID 0/1 izq
    add_note('D4', 19, 0.62, 0.35, -0.50) # ID 2
    add_note('Eb4', 19, 0.74, 0.36, -0.30) # ID 3
    add_note('F4', 19, 0.86, 0.37,  0.00) # ID 4 arriba
    add_note('G4', 19, 0.98, 0.38, +0.30) # ID 5
    add_note('Ab4', 19, 1.10, 0.39, +0.60) # ID 6 der
    add_note('Bb4', 19, 1.22, 0.40, +0.30) # ID 7 abajo der
    add_note('C5', 19, 1.34, 0.42,  0.00) # ID 8 centro
    # Y destello central en C6:
    add_note('C6', 19, 1.50, 0.46,  0.00)
    # Continuacion armonica normal del compas:
    add_note('G4', 19, 2.25, 0.40, +0.15)
    add_note('Eb4', 19, 3.0, 0.38, +0.10)
    add_note('Bb3', 19, 3.5, 0.36, -0.05)

    # Compas 20: Bb
    add_note('Bb1', 20, 0.0, 0.48, -0.20)
    add_note('F2', 20, 0.0, 0.42, -0.15)
    add_note('D3', 20, 0.5, 0.36, -0.15)
    add_note('F3', 20, 1.0, 0.38, +0.10)
    add_note('Bb3', 20, 1.5, 0.40, +0.20)
    add_note('D4', 20, 2.0, 0.42, +0.15)
    add_note('F4', 20, 2.5, 0.43, +0.10)
    add_note('D4', 20, 3.0, 0.39, +0.05)
    add_note('Bb3', 20, 3.5, 0.36,  0.00)

    # =========================================================================
    # OUTRO / DESPEDIDA (Compases 21 - 24): Desvanecimiento suave (~1:15 a 1:32)
    # =========================================================================
    # Compas 21: Cm9 (ralentizando la sensacion, notas mas espaciadas)
    add_note('C2', 21, 0.0, 0.45, -0.15)
    add_note('G2', 21, 0.0, 0.39, -0.10)
    add_note('Eb3', 21, 0.75, 0.34, -0.15)
    add_note('G3', 21, 1.5, 0.36, +0.10)
    add_note('D4', 21, 2.25, 0.38, +0.15)
    add_note('Eb4', 21, 3.0, 0.40, +0.20)

    # Compas 22: Ab maj7 (aun mas suave)
    add_note('Ab1', 22, 0.0, 0.42, -0.20)
    add_note('Eb2', 22, 0.0, 0.36, -0.15)
    add_note('C3', 22, 0.75, 0.32, -0.15)
    add_note('G3', 22, 1.5, 0.35, +0.10)
    add_note('C4', 22, 2.5, 0.37, +0.15)

    # Compas 23: Bb sus2 (preparando el reposo)
    add_note('Bb1', 23, 0.0, 0.40, -0.20)
    add_note('F2', 23, 0.0, 0.35, -0.15)
    add_note('D3', 23, 1.0, 0.32, -0.10)
    add_note('F3', 23, 2.0, 0.34, +0.05)
    add_note('C4', 23, 2.75, 0.35, +0.10)

    # Compas 24: Acorde Final de Do menor con novena (Cm add9), dejando sonar la resonancia
    add_note('C1', 24, 0.0, 0.45, -0.20)
    add_note('C2', 24, 0.0, 0.42, -0.15)
    add_note('G2', 24, 0.0, 0.40, -0.10)
    add_note('Eb3', 24, 0.0, 0.38, -0.05)
    add_note('G3', 24, 0.0, 0.38, +0.05)
    add_note('D4', 24, 0.0, 0.40, +0.15)
    add_note('Eb4', 24, 0.0, 0.42, +0.20)
    add_note('G4', 24, 0.0, 0.44, +0.25)
    # Una sola gota final arriba:
    add_note('C5', 24, 1.5, 0.42,  0.00)

    return events

def render_background_track():
    events = compose_track()
    print(f"Total de notas compuestas: {len(events)}")
    max_time = max(e[1] for e in events) + 6.0 # dejar 6s de decay al acorde final
    total_samples = int(max_time * SAMPLE_RATE)
    mix = np.zeros((total_samples, 2), dtype=np.float32)
    
    for note_name, start_time, vol, pan in events:
        sample = get_note_audio(note_name)
        start_idx = int(start_time * SAMPLE_RATE)
        end_idx = min(start_idx + len(sample), total_samples)
        s_len = end_idx - start_idx
        angle = (pan + 1.0) * (np.pi / 4.0)
        mix[start_idx:end_idx, 0] += sample[:s_len, 0] * np.cos(angle) * vol
        mix[start_idx:end_idx, 1] += sample[:s_len, 1] * np.sin(angle) * vol

    # Reverb ambiental calido de sala
    print("Aplicando reverberacion estéreo de sala...")
    n_audio, n_ir = len(mix), len(REVERB_IR)
    n_conv = n_audio + n_ir - 1
    out_left = np.fft.irfft(np.fft.rfft(mix[:, 0], n=n_conv) * np.fft.rfft(REVERB_IR[:, 0], n=n_conv), n=n_conv)
    out_right = np.fft.irfft(np.fft.rfft(mix[:, 1], n=n_conv) * np.fft.rfft(REVERB_IR[:, 1], n=n_conv), n=n_conv)
    reverb_audio = np.column_stack([out_left, out_right])
    padded_dry = np.zeros_like(reverb_audio)
    padded_dry[:n_audio] = mix
    
    reverb_wet = 0.32 # generoso para que se sienta en el fondo de la sala
    mixed = (1.0 - reverb_wet) * padded_dry + reverb_wet * reverb_audio
    
    # Recorte al final cuando el sonido desciende a silencio
    abs_mix = np.max(np.abs(mixed), axis=1)
    active_indices = np.where(abs_mix > 0.0003)[0]
    final_len = min(active_indices[-1] + int(SAMPLE_RATE * 1.0), len(mixed)) if len(active_indices) > 0 else len(mixed)
    mixed = mixed[:final_len]
    
    # Normalizacion suave ideal para musica de fondo bajo voz (peak a -2.0 dB = 0.794)
    peak = np.max(np.abs(mixed))
    if peak > 1e-6:
        mixed = mixed * (0.78 / peak)
        
    print(f"Duracion total de la pista: {len(mixed) / SAMPLE_RATE:.2f} segundos")
    return mixed

def export_track(audio, base_name):
    int16_pcm = (audio * 32767.0).astype(np.int16)
    mp3_path = os.path.join(OUTPUT_DIR, f"{base_name}.mp3")
    wav_path = os.path.join(OUTPUT_DIR, f"{base_name}.wav")
    
    subprocess.run([
        FFMPEG_PATH, "-y", "-f", "s16le", "-ar", str(SAMPLE_RATE),
        "-ac", "2", "-i", "pipe:0", "-b:a", "320k", mp3_path
    ], input=int16_pcm.tobytes(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    subprocess.run([
        FFMPEG_PATH, "-y", "-f", "s16le", "-ar", str(SAMPLE_RATE),
        "-ac", "2", "-i", "pipe:0", wav_path
    ], input=int16_pcm.tobytes(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f"Exportado correctamente: {base_name}.mp3 y .wav")

if __name__ == '__main__':
    print("Sintetizando pista de fondo ambiental (1:30 min) en Do menor...")
    audio = render_background_track()
    export_track(audio, "fondo_video_do_menor_tranquilo")
    print("Proceso completado.")
