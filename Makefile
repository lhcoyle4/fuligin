# =============================================================================
# FULIGIN — Makefile
# =============================================================================
# Vector-graphics space game built with SDL2 + SDL2_mixer.
# Targets:
#   make          — build native Windows executable (launch_fuligin.exe)
#   make wasm     — build WebAssembly bundle via Emscripten (index.html)
#   make clean    — remove all build artifacts
# =============================================================================

# --- Compiler -----------------------------------------------------------------
CC = gcc
# Use absolute paths so GCC resolves headers correctly under Windows/MSYS.
PROJ_DIR = $(subst \,/,$(CURDIR))

# --- Flags --------------------------------------------------------------------
CFLAGS = -Wall -Wextra -std=c99 -O2 -I"$(PROJ_DIR)/include" \
         -I"$(PROJ_DIR)/thirdparty/SDL2-2.30.3/x86_64-w64-mingw32/include" \
         -I"$(PROJ_DIR)/thirdparty/SDL2-2.30.3/x86_64-w64-mingw32/include/SDL2" \
         -I"$(PROJ_DIR)/thirdparty/SDL2_mixer-2.8.2/x86_64-w64-mingw32/include" \
         -I"$(PROJ_DIR)/thirdparty/SDL2_mixer-2.8.2/x86_64-w64-mingw32/include/SDL2"
LDFLAGS = -L"C:/Users/lhcoy/OneDrive/Desktop/sandbox/fuligin/thirdparty/SDL2-2.30.3/x86_64-w64-mingw32/lib" \
          -L"$(PROJ_DIR)/thirdparty/SDL2_mixer-2.8.2/x86_64-w64-mingw32/lib" \
          -lmingw32 -lSDL2main -lSDL2 -lSDL2_mixer

# --- Sources ------------------------------------------------------------------
SRC = src/main.c src/vector_graphics.c src/game.c src/vector_font.c src/audio.c src/ui.c src/world_builder.c src/ai.c src/ui_hud.c src/state.c src/game_data.c src/drone_chatter.c src/enemy_rustweaver.c
OBJ = $(SRC:.c=.o)

# --- Native target ------------------------------------------------------------
TARGET = launch_fuligin.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# --- Clean --------------------------------------------------------------------
# Removes all compiled objects and build artifacts.
# Safe to run on a clean tree (errors suppressed).
clean:
	del /q /f src\*.o src\*.wasm.o $(TARGET) $(WASM_TARGET) index.js index.wasm 2>nul || exit 0

# --- WebAssembly target (Emscripten) ------------------------------------------
WASM_CC = emcc
WASM_CFLAGS = -Wall -Wextra -std=c99 -O2 -I"$(PROJ_DIR)/include" -s USE_SDL=2 -s USE_SDL_MIXER=2
WASM_LDFLAGS = -s USE_SDL=2 -s USE_SDL_MIXER=2 -s ALLOW_MEMORY_GROWTH=1 \
               --shell-file "$(PROJ_DIR)/src/wasm_shell.html"
WASM_TARGET = index.html
WASM_OBJ = $(SRC:.c=.wasm.o)

wasm: $(WASM_TARGET)

$(WASM_TARGET): $(WASM_OBJ)
	$(WASM_CC) -o $@ $^ $(WASM_LDFLAGS)

%.wasm.o: %.c
	$(WASM_CC) $(WASM_CFLAGS) -c -o $@ $<
