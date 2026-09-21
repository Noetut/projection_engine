"""
Script para generar efectos de sonido de piano para el display de Área Ciclogénica.
Sincronizado con las animaciones de area_ciclogenica.txt (Segmento 1 y Segmento 6).
"""
import os
import re
import json
import base64
import urllib.request
import subprocess
import numpy as np

FFMPEG_PATH = r"C:\Users\noeam\AppData\Local\Overwolf\Extensions\ncfplpkmiejjaklknfnkgcpapnhkggmlcppckhcb\270.0.25\obs\bin\64bit\ffmpeg.exe"
OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))
SAMPLE_RATE = 44100

def get_soundfont():
    cache_path = os.path.join(OUTPUT_DIR, ".soundfont_cache.json")
    if os.path.exists(cache_path):
        with open(cache_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    print("Descargando muestras de piano de cola (MusyngKite)...")
    url = 'https://raw.githubusercontent.com/gleitz/midi-js-soundfonts/gh-pages/MusyngKite/acoustic_grand_piano-mp3.js'
    data = urllib.request.urlopen(url).read().decode('utf-8')
    start = data.find('{\n')
    end = data.rfind('}') + 1
    raw_json = re.sub(r',\s*\}', '}', data[start:end])
    parsed = json.loads(raw_json)
    with open(cache_path, 'w', encoding='utf-8') as f:
        json.dump(parsed, f)
    return parsed

SOUNDFONT = None
NOTE_CACHE = {}

def get_note_audio(note_name):
    global SOUNDFONT
    if SOUNDFONT is None:
        SOUNDFONT = get_soundfont()
    if note_name not in NOTE_CACHE:
        raw_b64 = SOUNDFONT[note_name].split(',')[1]
        mp3_bytes = base64.b64decode(raw_b64)
        cmd = [
            FFMPEG_PATH, "-y", "-i", "pipe:0",
            "-f", "s16le", "-acodec", "pcm_s16le",
            "-ar", str(SAMPLE_RATE), "-ac", "2", "pipe:1"
        ]
        p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, _ = p.communicate(input=mp3_bytes)
        NOTE_CACHE[note_name] = np.frombuffer(out, dtype=np.int16).reshape(-1, 2).astype(np.float32) / 32768.0
    return NOTE_CACHE[note_name]

def create_reverb_ir(duration=2.5, decay=1.2):
    num_samples = int(duration * SAMPLE_RATE)
    t = np.linspace(0, duration, num_samples)
    envelope = np.exp(-t * 3.0 / decay)
    np.random.seed(42)
    left_noise = np.random.randn(num_samples) * envelope
    right_noise = np.random.randn(num_samples) * envelope
    freqs = np.fft.rfftfreq(num_samples, 1.0 / SAMPLE_RATE)
    lp_filter = 1.0 / (1.0 + (freqs / 3500.0) ** 2)
    left_filt = np.fft.irfft(np.fft.rfft(left_noise) * lp_filter, n=num_samples)
    right_filt = np.fft.irfft(np.fft.rfft(right_noise) * lp_filter, n=num_samples)
    ir = np.column_stack([left_filt, right_filt])
    return ir / (np.max(np.abs(ir)) + 1e-9)

REVERB_IR = create_reverb_ir()

def render_events(events, reverb_wet=0.28, damp_early=False):
    max_time = max(e[1] for e in events) + 4.0
    total_samples = int(max_time * SAMPLE_RATE)
    mix = np.zeros((total_samples, 2), dtype=np.float32)
    
    for note_name, start_time, vol, pan in events:
        sample = get_note_audio(note_name)
        start_idx = int(start_time * SAMPLE_RATE)
        
        if damp_early and start_time < 0.9:
            note_len = int(0.14 * SAMPLE_RATE)
            s = sample[:note_len].copy()
            fade_len = int(0.04 * SAMPLE_RATE)
            s[-fade_len:] *= np.linspace(1.0, 0.0, fade_len)[:, None]
        else:
            s = sample
            
        end_idx = min(start_idx + len(s), total_samples)
        s_len = end_idx - start_idx
        angle = (pan + 1.0) * (np.pi / 4.0)
        mix[start_idx:end_idx, 0] += s[:s_len, 0] * np.cos(angle) * vol
        mix[start_idx:end_idx, 1] += s[:s_len, 1] * np.sin(angle) * vol
        
    n_audio, n_ir = len(mix), len(REVERB_IR)
    n_conv = n_audio + n_ir - 1
    out_left = np.fft.irfft(np.fft.rfft(mix[:, 0], n=n_conv) * np.fft.rfft(REVERB_IR[:, 0], n=n_conv), n=n_conv)
    out_right = np.fft.irfft(np.fft.rfft(mix[:, 1], n=n_conv) * np.fft.rfft(REVERB_IR[:, 1], n=n_conv), n=n_conv)
    reverb_audio = np.column_stack([out_left, out_right])
    padded_dry = np.zeros_like(reverb_audio)
    padded_dry[:n_audio] = mix
    mixed = (1.0 - reverb_wet) * padded_dry + reverb_wet * reverb_audio
    
    abs_mix = np.max(np.abs(mixed), axis=1)
    active_indices = np.where(abs_mix > 0.0005)[0]
    final_len = min(active_indices[-1] + int(SAMPLE_RATE * 0.5), len(mixed)) if len(active_indices) > 0 else len(mixed)
    mixed = mixed[:final_len]
    peak = np.max(np.abs(mixed))
    if peak > 1e-6:
        mixed = mixed * (0.94 / peak)
    return mixed

def export_audio(audio, base_name):
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
    print(f"Exportado: {base_name}.mp3 y .wav")

if __name__ == '__main__':
    print("Módulo de generación de audio listo.")
