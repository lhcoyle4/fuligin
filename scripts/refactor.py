import os
import re

game_c_path = "src/game.c"
entities_h_path = "include/entities.h"
state_h_path = "include/state.h"
state_c_path = "src/state.c"
data_h_path = "include/game_data.h"
data_c_path = "src/game_data.c"

with open(game_c_path, "r", encoding="utf-8") as f:
    lines = f.readlines()

def get_lines(start_marker, end_marker):
    start_idx = -1
    end_idx = -1
    for i, line in enumerate(lines):
        if start_marker in line:
            start_idx = i
        if end_marker in line and start_idx != -1:
            end_idx = i
            break
    return start_idx, end_idx

math_start, _ = get_lines("/* =========== MATH CONSTANTS =========== */", "dummy")
structs_start, structs_end = get_lines("/* =========== STRUCT DEFINITIONS =========== */", "/* =========== STATIC GEOMETRY TABLES =========== */")
geometry_start, strings_end = get_lines("/* =========== STATIC GEOMETRY TABLES =========== */", "/* =========== MODULE-SCOPE GLOBALS =========== */")
globals_start, globals_end = get_lines("/* =========== MODULE-SCOPE GLOBALS =========== */", "/* =========== FORWARD DECLARATIONS =========== */")

print(f"Indices: math={math_start}, structs_end={structs_end}, geo={geometry_start}, strings_end={strings_end}, glob={globals_start}, glob_end={globals_end}")

# 1. Create entities.h
entities_content = "#ifndef ENTITIES_H\n#define ENTITIES_H\n\n#include <SDL2/SDL.h>\n#include \"vector_graphics.h\"\n\n"
entities_content += "".join(lines[math_start:structs_end])
entities_content += "\n#endif\n"

with open(entities_h_path, "w", encoding="utf-8") as f:
    f.write(entities_content)

# 2. Create game_data.h and game_data.c
data_lines = "".join(lines[geometry_start:strings_end])
data_c_content = '#include "game_data.h"\n\n' + data_lines.replace("static Line ", "Line ").replace("static const char *", "const char *")
data_h_content = "#ifndef GAME_DATA_H\n#define GAME_DATA_H\n\n#include \"entities.h\"\n\n"

data_h_content += """extern Line ship_lines[5];
extern Line ufo_lines[10];
extern Line kamikaze_lines[4];
extern Line bomber_lines[10];
extern Line home_station_lines[16];
extern Line eldritch_lines[10];
extern Line daemon_lines[16];
extern Line npc_drone_lines[7];
extern const char *upgrade_names[30];
extern const char *upgrade_descs[30];
extern const char *difficulty_names[4];
"""
data_h_content += "\n#endif\n"

with open(data_c_path, "w", encoding="utf-8") as f:
    f.write(data_c_content)
with open(data_h_path, "w", encoding="utf-8") as f:
    f.write(data_h_content)

# 3. Create state.h and state.c
global_text = "".join(lines[globals_start:globals_end])
state_c_content = '#include "state.h"\n\n' + global_text.replace("static ", "")
state_h_content = "#ifndef STATE_H\n#define STATE_H\n\n#include \"entities.h\"\n#include \"game.h\"\n\n"

cleaned_global_text = re.sub(r"\{.*?\}", ";", global_text, flags=re.DOTALL)
lines_clean = cleaned_global_text.split("\n")

for line in lines_clean:
    if line.strip() == "" or line.strip().startswith("/*") or line.strip().startswith("//") or line.startswith("#"):
        state_h_content += line + "\n"
        continue
    
    decl = line.split("=")[0].strip()
    if decl:
        if decl.endswith(";"):
            decl = decl[:-1].strip()
        decl = decl.replace("static ", "").strip()
        state_h_content += f"extern {decl};\n"

state_h_content += "\n#endif\n"

with open(state_c_path, "w", encoding="utf-8") as f:
    f.write(state_c_content)
with open(state_h_path, "w", encoding="utf-8") as f:
    f.write(state_h_content)

# 4. Update game.c
new_game_c = lines[:math_start]
new_game_c.append('#include "entities.h"\n')
new_game_c.append('#include "game_data.h"\n')
new_game_c.append('#include "state.h"\n\n')
new_game_c.extend(lines[globals_end:])

with open(game_c_path, "w", encoding="utf-8") as f:
    f.write("".join(new_game_c))

print("Extraction script complete. Updated game.c")
