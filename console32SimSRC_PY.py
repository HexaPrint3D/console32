import pygame
import sys
import random
import re # Für das Extrahieren der Zahlen aus dem C++ Code
import ast
import json
import os
import copy
import tkinter as tk

# --- C++ LEVEL IMPORT ---
# Kopiere hier einfach die Zeilen aus deinem world[] Array hinein!
cpp_levels = [
    "{{{112,36,9,26},{-4,34,94,6},{0,4,88,7},{82,4,6,35}}, 4, {86,30,1.0,50,86}, {{124,6},{20,60},{69,30}}, 3, 122, 57}",
    "{{{-1,51,129,5},{49,6,4,45},{53,24,17,4},{80,18,16,3}}, 4, {21,59,1.0,50,21}, {{50,2},{62,20},{88,14}}, 3, 88, 57}",
    "{{{0,50,40,4}}, 1, {50,46,0.5,30,50}, {{20,35},{80,45}}, 2, 110,20}"
]

def parse_cpp_level(cpp_str):
    """Wandelt eine C++ struct Zeile in ein Python Dictionary um."""
    # Replace { with [ and } with ] to make it valid Python list syntax
    s = cpp_str.replace('{', '[').replace('}', ']')
    # Parse the nested list
    data = ast.literal_eval(s)

    # Some pasted strings have an extra outer container or trailing comma,
    # e.g. {{{...}}}, which yields a single-item tuple/list.
    if isinstance(data, (tuple, list)) and len(data) == 1 and isinstance(data[0], (tuple, list)):
        data = data[0]

    if len(data) < 7:
        raise ValueError("Parsed level structure is too short")
    
    plats = data[0]
    plat_count = data[1]
    enemy_data = data[2]
    coins_data = data[3]
    coin_count = data[4]
    goal = (data[5], data[6])
    
    enemy = {"x": enemy_data[0], "y": enemy_data[1], "speed": enemy_data[2], "range": enemy_data[3], "startX": enemy_data[4]}
    coins = [{"x": c[0], "y": c[1], "collected": False} for c in coins_data[:coin_count]]
    
    return {
        "plats": plats,
        "enemy": enemy,
        "coins": coins,
        "goal": goal
    }

def get_save_path(filename):
    if getattr(sys, "frozen", False):
        appname = "ESP32LevelSimulator"
        base_dir = os.getenv("LOCALAPPDATA") or os.path.expanduser("~")
        save_dir = os.path.join(base_dir, appname)
    else:
        save_dir = os.path.dirname(__file__)
    os.makedirs(save_dir, exist_ok=True)
    return os.path.join(save_dir, filename)

SAVE_FILE = get_save_path("imported_levels.json")

def load_imported_level_strings():
    if os.path.exists(SAVE_FILE):
        try:
            with open(SAVE_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                if isinstance(data, list):
                    loaded = [str(x) for x in data]
                    print(f"Loaded {len(loaded)} imported levels")
                    return loaded
        except Exception as e:
            print(f"Could not load imported levels: {e}")
    return []


def save_imported_level_strings(strings):
    try:
        os.makedirs(os.path.dirname(SAVE_FILE), exist_ok=True)
        with open(SAVE_FILE, "w", encoding="utf-8") as f:
            json.dump(strings, f, ensure_ascii=False, indent=2)
    except Exception as e:
        print(f"Could not save imported levels: {e}")


base_levels = [parse_cpp_level(lvl) for lvl in cpp_levels]
imported_level_strings = load_imported_level_strings()


def rebuild_levels():
    global levels
    imported_levels = []
    for idx, lvl_str in enumerate(imported_level_strings):
        try:
            imported_levels.append(parse_cpp_level(lvl_str))
        except Exception as e:
            print(f"Skipping invalid saved level #{idx+1}: {e}")
    levels = base_levels + imported_levels


rebuild_levels()

# --- SIMULATOR LOGIK ---
WIDTH, HEIGHT, SCALE = 128, 64, 8
FPS = 30

class Simulator:
    def __init__(self):
        pygame.init()
        # --- NEU: FENSTER NAME & ICON ---
        pygame.display.set_caption("ESP32 Console v2.1") # Dein Custom Name
        try:
            icon = pygame.image.load("mein_icon.ico")
            pygame.display.set_icon(icon)
        except:
            pass # Falls die Datei fehlt, nutzt Windows den Standard
        
        self.screen = pygame.display.set_mode((WIDTH * SCALE, HEIGHT * SCALE))
        # ... restlicher Code ...
	
        pygame.init()
        self.screen = pygame.display.set_mode((WIDTH * SCALE, HEIGHT * SCALE))
        pygame.display.set_caption("ESP32 Level Simulator")
        self.clock = pygame.time.Clock()
        self.font = pygame.font.SysFont("monospace", 15)
        self.current_level_idx = 0
        self.importing = False
        self.import_text = ""
        self.import_message = ""
        self.import_message_timer = 0
        self.menu_open = False
        self.menu_selected = 0
        self.menu_scroll = 0
        self.reset_level()

    def reset_level(self):
        if self.current_level_idx >= len(levels):
            self.current_level_idx = len(levels) - 1  # Stay on the last level instead of cycling

        # Deep copy the selected level so imported levels behave consistently
        self.active_level = copy.deepcopy(levels[self.current_level_idx])
        self.px, self.py = 5, 40

        enemy = self.active_level["enemy"]
        def collides(px, py, en):
            return abs(px - en["x"]) < 8 and abs(py - en["y"]) < 8

        if collides(self.px, self.py, enemy):
            if enemy["x"] < WIDTH / 2:
                self.px = min(WIDTH - 10, enemy["x"] + 20)
            else:
                self.px = max(5, enemy["x"] - 20)

            if collides(self.px, self.py, enemy):
                self.py = min(59, max(0, enemy["y"] + 10))
            if collides(self.px, self.py, enemy):
                self.py = max(0, min(59, enemy["y"] - 10))

        self.vy = 0
        self.is_jumping = False
        self.coins_collected = 0

    def run(self):
        while True:
            self.screen.fill((20, 20, 20))
            
            for event in pygame.event.get():
                if event.type == pygame.QUIT: pygame.quit(); sys.exit()
                if event.type == pygame.KEYDOWN:
                    if self.importing:
                        if event.key == pygame.K_RETURN:
                            try:
                                # Clean the input text
                                cleaned_text = ''.join(c for c in self.import_text if c.isprintable()).strip()
                                if cleaned_text:
                                    new_level = parse_cpp_level(cleaned_text)
                                    imported_level_strings.append(cleaned_text)
                                    save_imported_level_strings(imported_level_strings)
                                    rebuild_levels()
                                    self.import_message = f"Level {len(levels)} added"
                                    self.import_message_timer = FPS * 2
                                    self.importing = False
                                    self.import_text = ""
                                else:
                                    self.import_message = "Import cancelled"
                                    self.import_message_timer = FPS * 2
                                    self.importing = False
                                    self.import_text = ""
                            except Exception as e:
                                print(f"Error parsing level: {e}")
                                self.import_message = "Invalid level format"
                                self.import_message_timer = FPS * 2
                                self.importing = False
                                self.import_text = ""
                        elif event.key == pygame.K_BACKSPACE:
                            self.import_text = self.import_text[:-1]
                        elif event.key == pygame.K_ESCAPE:
                            self.importing = False
                            self.import_text = ""
                        elif event.mod & pygame.KMOD_CTRL and event.key == pygame.K_v:
                            # Handle paste
                            root = tk.Tk()
                            root.withdraw()
                            try:
                                clipboard = root.clipboard_get()
                                self.import_text += clipboard
                            except Exception:
                                pass
                            root.destroy()
                        else:
                            self.import_text += event.unicode
                    elif self.menu_open:
                        if event.key == pygame.K_UP:
                            self.menu_selected = max(0, self.menu_selected - 1)
                            self.menu_scroll = max(0, min(self.menu_scroll, self.menu_selected))
                        elif event.key == pygame.K_DOWN:
                            self.menu_selected = min(len(levels) - 1, self.menu_selected + 1)
                            visible_count = 12
                            self.menu_scroll = min(max(0, self.menu_selected - visible_count + 1), max(0, len(levels) - visible_count))
                        elif event.key == pygame.K_RETURN:
                            self.current_level_idx = self.menu_selected
                            self.reset_level()
                            self.menu_open = False
                        elif event.key == pygame.K_d:
                            if self.menu_selected >= len(base_levels):
                                delete_idx = self.menu_selected - len(base_levels)
                                del imported_level_strings[delete_idx]
                                save_imported_level_strings(imported_level_strings)
                                rebuild_levels()
                                if self.current_level_idx >= len(levels):
                                    self.current_level_idx = len(levels) - 1
                                self.reset_level()
                                self.import_message = "Imported level deleted"
                                self.import_message_timer = FPS * 2
                                self.menu_selected = min(self.menu_selected, len(levels) - 1)
                        elif event.key == pygame.K_ESCAPE or event.key == pygame.K_m:
                            self.menu_open = False
                    else:
                        if event.key == pygame.K_i:
                            self.importing = True
                        elif event.key == pygame.K_m:
                            self.menu_open = True
                            self.menu_selected = self.current_level_idx
                            self.menu_scroll = max(0, self.menu_selected - 3)
            
            if not self.importing and not self.menu_open:
                if self.import_message_timer > 0:
                    self.import_message_timer -= 1
                keys = pygame.key.get_pressed()
                
                # --- PHYSIK ---
                if keys[pygame.K_a]: self.px -= 2.0
                if keys[pygame.K_d]: self.px += 2.0
                
                self.vy += 0.35
                next_y = self.py + self.vy
                
                # Kollision mit Plattformen
                for p in self.active_level["plats"]:
                    if self.px + 4 > p[0] and self.px < p[0] + p[2]:
                        if self.py + 4 <= p[1] and next_y + 4 >= p[1]:
                            next_y = p[1] - 4
                            self.vy = 0
                            self.is_jumping = False
                
                if next_y >= 59: # Boden
                    next_y = 59
                    self.vy = 0
                    self.is_jumping = False
                    
                self.py = next_y
                if keys[pygame.K_SPACE] and not self.is_jumping:
                    self.vy = -5.2
                    self.is_jumping = True

                # Münzen sammeln
                for c in self.active_level["coins"]:
                    if not c["collected"]:
                        if abs(self.px - c["x"]) < 8 and abs(self.py - c["y"]) < 8:
                            c["collected"] = True
                            self.coins_collected += 1
                
                # Enemy bewegen
                en = self.active_level["enemy"]
                en["x"] += en["speed"]
                if en["x"] > en["startX"] + en["range"] or en["x"] < en["startX"]:
                    en["speed"] *= -1

                # Goal check
                if self.coins_collected >= len(self.active_level["coins"]):
                    g = self.active_level["goal"]
                    if abs(self.px - g[0]) < 6 and abs(self.py - g[1]) < 6:
                        self.current_level_idx += 1
                        self.reset_level()
            else:
                keys = pygame.key.get_pressed()  # maybe not needed, but to avoid error

            # --- ZEICHNEN ---
            # Plattformen
            for p in self.active_level["plats"]:
                pygame.draw.rect(self.screen, (200, 200, 200), (p[0]*SCALE, p[1]*SCALE, p[2]*SCALE, p[3]*SCALE))
            
            # Münzen
            for c in self.active_level["coins"]:
                if not c["collected"]:
                    pygame.draw.circle(self.screen, (255, 215, 0), (int(c["x"]+2)*SCALE, int(c["y"]+2)*SCALE), 2*SCALE)
            
            # Enemy
            en = self.active_level["enemy"]
            pygame.draw.rect(self.screen, (255, 50, 50), (en["x"]*SCALE, (en["y"]-3)*SCALE, 4*SCALE, 4*SCALE))

            # Collision with enemy
            if abs(self.px - en["x"]) < 8 and abs(self.py - en["y"]) < 8:
                self.reset_level()

            # Goal
            if self.coins_collected >= len(self.active_level["coins"]):
                g = self.active_level["goal"]
                pygame.draw.rect(self.screen, (50, 255, 50), (g[0]*SCALE, g[1]*SCALE, 6*SCALE, 6*SCALE), 2)

            # Player
            pygame.draw.rect(self.screen, (255, 255, 255), (self.px*SCALE, self.py*SCALE, 4*SCALE, 4*SCALE))

            # Info
            info = self.font.render(f"Level: {self.current_level_idx + 1}/{len(levels)}", True, (100, 100, 100))
            self.screen.blit(info, (10, 10))

            if self.menu_open:
                overlay = pygame.Surface((WIDTH * SCALE, HEIGHT * SCALE))
                overlay.set_alpha(220)
                overlay.fill((0, 0, 0))
                self.screen.blit(overlay, (0, 0))
                title = self.font.render("Level Menu", True, (255, 255, 255))
                self.screen.blit(title, (10, 10))
                subtitle = self.font.render("Enter=play  D=delete imported  M/ESC=close", True, (200, 200, 200))
                self.screen.blit(subtitle, (10, 32))
                visible_count = 12
                for i in range(self.menu_scroll, min(len(levels), self.menu_scroll + visible_count)):
                    text = f"{i+1}. {'Base' if i < len(base_levels) else 'Imported'} level"
                    color = (255, 255, 255)
                    y = 60 + (i - self.menu_scroll) * 24
                    if i == self.menu_selected:
                        pygame.draw.rect(self.screen, (50, 100, 180), (8, y-2, WIDTH * SCALE - 16, 22))
                        color = (255, 255, 0)
                    self.screen.blit(self.font.render(text, True, color), (12, y))
                if self.menu_selected >= len(base_levels):
                    info_text = "Selected imported level - press D to delete"
                else:
                    info_text = "Base level cannot be deleted"
                self.screen.blit(self.font.render(info_text, True, (200, 200, 200)), (10, HEIGHT * SCALE - 40))
            elif self.importing:
                overlay = pygame.Surface((WIDTH * SCALE, HEIGHT * SCALE))
                overlay.set_alpha(180)
                overlay.fill((0, 0, 0))
                self.screen.blit(overlay, (0, 0))
                prompt = self.font.render("Paste level string with Ctrl+V and press Enter:", True, (255, 255, 255))
                self.screen.blit(prompt, (10, HEIGHT * SCALE // 2 - 30))
                text_surf = self.font.render(self.import_text, True, (255, 255, 255))
                self.screen.blit(text_surf, (10, HEIGHT * SCALE // 2))
                instr = self.font.render("ESC to cancel", True, (200, 200, 200))
                self.screen.blit(instr, (10, HEIGHT * SCALE // 2 + 20))

            if self.import_message_timer > 0 and self.import_message:
                message_surf = self.font.render(self.import_message, True, (150, 255, 150))
                self.screen.blit(message_surf, (10, HEIGHT * SCALE - 30))

            pygame.display.flip()
            self.clock.tick(FPS)

if __name__ == "__main__":
    Simulator().run()