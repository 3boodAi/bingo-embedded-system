import asyncio
import os
import ssl
from pathlib import Path

import certifi

os.environ["SSL_CERT_FILE"] = certifi.where()
os.environ["REQUESTS_CA_BUNDLE"] = certifi.where()
ssl.create_default_context = ssl._create_unverified_context

import edge_tts


VOICE = "en-US-GuyNeural"
RATE = "+8%"
VOLUME = "+20%"
OUTPUT_DIR = Path(__file__).resolve().parents[1] / "audio" / "MP3"
BACKUP_DIR = Path(__file__).resolve().parents[1] / "audio" / "backup"


def bingo_label(number: int) -> str:
    if number <= 15:
        return f"B {number}"
    if number <= 30:
        return f"I {number}"
    if number <= 45:
        return f"N {number}"
    if number <= 60:
        return f"G {number}"
    return f"O {number}"


async def synthesize(filename: str, text: str) -> None:
    path = OUTPUT_DIR / filename
    communicate = edge_tts.Communicate(text, VOICE, rate=RATE, volume=VOLUME)
    await communicate.save(str(path))


async def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    BACKUP_DIR.mkdir(parents=True, exist_ok=True)

    existing_first_file = OUTPUT_DIR / "0001.mp3"
    if existing_first_file.exists():
        backup_file = BACKUP_DIR / "0001.original.mp3"
        if not backup_file.exists():
            backup_file.write_bytes(existing_first_file.read_bytes())

    tasks = []
    for number in range(1, 76):
        label = bingo_label(number)
        tasks.append((f"{number:04d}.mp3", f"Bingo number {label}"))

    for player_number in range(1, 4):
        tasks.append((f"{100 + player_number:04d}.mp3", f"Player {player_number} won. Bingo!"))

    for filename, text in tasks:
        print(f"Generating {filename}: {text}")
        await synthesize(filename, text)

    print(f"Generated {len(tasks)} MP3 files in {OUTPUT_DIR}")


if __name__ == "__main__":
    asyncio.run(main())
