import os
import random
from pathlib import Path

def parse_lists(file_path):
    lists = {
        "Prefixes": [],
        "Suffixes": [],
        "Materials": [],
        "Adjectives": [],
        "Cryptic Vocabulary": []
    }
    
    current_list = None
    if os.path.exists(file_path):
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line.startswith('### '):
                    header = line[4:].strip()
                    if header in lists:
                        current_list = header
                    else:
                        current_list = None
                elif current_list and (line.startswith('- ') or line.startswith('* ')):
                    item = line[2:].strip()
                    if item:
                        lists[current_list].append(item)
                        
    # Fallbacks
    fallbacks = {
        "Prefixes": ["Aero", "Chrono", "Neo", "Void", "Quantum", "Necro", "Astral", "Hyper", "Exo", "Iso", "Morpho", "Poly", "Xeno"],
        "Suffixes": ["mancer", "tech", "lith", "core", "phage", "gon", "sphere", "tron", "form", "oid", "plasm", "cyte", "gen"],
        "Materials": ["Obsidian", "Plasteel", "Nanocarbon", "Eldritch Bone", "Tungsten", "Quicksilver", "Adamantine", "Dark Matter", "Neutronium", "Orichalcum", "Voidstone", "Gossamer"],
        "Adjectives": ["Luminous", "Cursed", "Resonant", "Fractured", "Abyssal", "Radiant", "Ethereal", "Malignant", "Dissonant", "Harmonic", "Temporal", "Spatial", "Vancian", "Eldritch"],
        "Cryptic Vocabulary": ["Echo", "Residue", "Whisper", "Anomaly", "Paradox", "Enigma", "Tesseract", "Singularity", "Nexus", "Monolith", "Cipher", "Fractal", "Miasma", "Ontology"]
    }
    
    for key, fallback_list in fallbacks.items():
        if not lists[key]:
            lists[key] = fallback_list
            
    return lists

def generate_reliquary_drop(lists):
    prefix = random.choice(lists["Prefixes"])
    suffix = random.choice(lists["Suffixes"])
    material = random.choice(lists["Materials"])
    adj = random.choice(lists["Adjectives"])
    vocab = random.choice(lists["Cryptic Vocabulary"])
    
    name = f"{adj} {prefix}{suffix} of {material}"
    
    descriptions = [
        f"A strange {material.lower()} artifact emitting a {vocab.lower()}.",
        f"Forged from pure {material.lower()}, it hums with {adj.lower()} energy.",
        f"Its surface is etched with {vocab.lower()}s, cold to the touch.",
        f"An impossibly dense core of {material.lower()} wrapped in a {adj.lower()} field."
    ]
    
    uses = [
        f"Can be used to trigger a {adj.lower()} {vocab.lower()}.",
        f"When shattered, releases a shockwave of {vocab.lower()}.",
        f"Powers the {prefix}-engines of ancient ships.",
        f"Grants fleeting glimpses into the {vocab.lower()}."
    ]
    
    desc = random.choice(descriptions)
    use = random.choice(uses)
    
    return f"#### Reliquary Drop: {name}\n- **Description**: {desc}\n- **Obscure Use**: {use}\n\n"

def generate_flight_log(lists):
    vocab = random.choice(lists["Cryptic Vocabulary"])
    prefix = random.choice(lists["Prefixes"])
    adj = random.choice(lists["Adjectives"])
    
    messages = [
        f"[CORRUPTED] ... seeking the {vocab} ...",
        f"Warning: {prefix}-field collapsing. {vocab.capitalize()} detected.",
        f"... they are in the {vocab.lower()} ... send {random.choice(lists['Materials']).lower()}.",
        f"Visual contact lost. Sensors indicate a {adj.lower()} {vocab.lower()} in quadrant 7.",
        f"Do not trust the {prefix}{random.choice(lists['Suffixes'])}. It's a {vocab.lower()}!"
    ]
    
    msg = random.choice(messages)
    drone_id = f"{random.choice(lists['Prefixes']).upper()}-{random.randint(1000, 9999)}"
    
    return f"**Ascian Drone Flight Log - {drone_id}**\n> {msg}\n\n"

def generate_sector(lists):
    adj = random.choice(lists["Adjectives"])
    vocab = random.choice(lists["Cryptic Vocabulary"])
    material = random.choice(lists["Materials"])
    
    designation = f"{random.choice('ABCDEFGHIJKLMNOPQRSTUVWXYZ')}{random.choice('ABCDEFGHIJKLMNOPQRSTUVWXYZ')}-{random.randint(100, 999)}"
    name = f"Sector {designation}: The {adj} {vocab}"
    
    descriptions = [
        f"A {adj.lower()} region of space defined by its overwhelming {vocab.lower()}.",
        f"Rich in {material.lower()} asteroid belts, plagued by {vocab.lower()}s.",
        f"An ancient battleground now silent, save for the {adj.lower()} {vocab.lower()}.",
        f"Once home to the {random.choice(lists['Prefixes'])}{random.choice(lists['Suffixes'])} empire."
    ]
    
    desc = random.choice(descriptions)
    planets = random.randint(1, 15)
    
    return f"#### Spatial Coordinates: {name}\n- **Description**: {desc}\n- **Planetary Bodies**: {planets} known masses.\n\n"

def main():
    script_dir = Path(__file__).parent
    world_bible_path = script_dir.parent / "WORLD_BIBLE.md"
    
    target_size = 20 * 1024 * 1024 # 20 MB
    
    lists = parse_lists(world_bible_path)
    
    current_size = 0
    if os.path.exists(world_bible_path):
        current_size = os.path.getsize(world_bible_path)
    
    if current_size >= target_size:
        print(f"File is already {current_size} bytes, which is >= {target_size} bytes.")
        return
        
    print(f"Generating World Bible to {world_bible_path}...")
    print(f"Current size: {current_size} bytes. Target: {target_size} bytes.")
    
    with open(world_bible_path, 'a', encoding='utf-8') as f:
        while current_size < target_size:
            # Batch generate to improve performance
            block = []
            for _ in range(500):
                choice = random.random()
                if choice < 0.6:  # 60% Reliquary Drops
                    block.append(generate_reliquary_drop(lists))
                elif choice < 0.9:  # 30% Flight Logs
                    block.append(generate_flight_log(lists))
                else:  # 10% Sectors
                    block.append(generate_sector(lists))
            
            chunk = "".join(block)
            chunk_bytes = chunk.encode('utf-8')
            f.write(chunk)
            current_size += len(chunk_bytes)
            
    print(f"Finished generating. Final size: {current_size} bytes.")

if __name__ == '__main__':
    main()
