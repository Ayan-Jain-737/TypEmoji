import os
import shutil
import subprocess
from PIL import Image, ImageDraw, ImageFont, ImageFilter

WIDTH = 840
HEIGHT = 540

FONT_BOLD = '/usr/share/fonts/liberation/LiberationSans-Bold.ttf'
FONT_REG = '/usr/share/fonts/liberation/LiberationSans-Regular.ttf'
FONT_MONO = '/usr/share/fonts/TTF/DejaVuSansMono.ttf'
FONT_MONO_BOLD = '/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf'
FONT_EMOJI = '/usr/share/fonts/noto/NotoColorEmoji.ttf'

emoji_font = ImageFont.truetype(FONT_EMOJI, 109)

# Cache rendered emojis
emoji_cache = {}

def get_emoji(char, size):
    key = (char, size)
    if key in emoji_cache:
        return emoji_cache[key]
    canvas = Image.new('RGBA', (140, 140), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw.text((10, 10), char, font=emoji_font, embedded_color=True)
    bbox = canvas.getbbox()
    if bbox:
        cropped = canvas.crop(bbox)
        w, h = cropped.size
        scale = size / max(w, h)
        nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
        res = cropped.resize((nw, nh), Image.Resampling.LANCZOS)
    else:
        res = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    emoji_cache[key] = res
    return res

def draw_editor(d, overlay, code_state, cursor_visible):
    # Editor box
    ew, eh = 760, 480
    ex, ey = 40, 30
    
    # Window container
    d.rounded_rectangle([ex, ey, ex + ew, ey + eh], radius=12, fill=(24, 25, 34, 255), outline=(50, 52, 70, 255), width=1)
    
    # Titlebar
    d.rounded_rectangle([ex, ey, ex + ew, ey + 38], radius=12, fill=(20, 21, 28, 255))
    d.rectangle([ex, ey + 24, ex + ew, ey + 38], fill=(20, 21, 28, 255))
    d.line([ex, ey + 38, ex + ew, ey + 38], fill=(35, 37, 50, 255), width=1)
    
    # Window traffic lights
    d.ellipse([ex + 16, ey + 13, ex + 28, ey + 25], fill=(255, 95, 86, 255))
    d.ellipse([ex + 36, ey + 13, ex + 48, ey + 25], fill=(255, 189, 46, 255))
    d.ellipse([ex + 56, ey + 13, ex + 68, ey + 25], fill=(39, 201, 63, 255))
    
    font_title = ImageFont.truetype(FONT_BOLD, 12)
    d.text((ex + ew//2 - 68, ey + 12), "main.py — TypEmoji Demo", font=font_title, fill=(148, 163, 184, 255))
    
    font_meta = ImageFont.truetype(FONT_REG, 11)
    d.text((ex + ew - 150, ey + 13), "Wayland • UTF-8", font=font_meta, fill=(100, 116, 139, 255))
    
    # Tab bar
    tab_y = ey + 38
    d.rectangle([ex, tab_y, ex + ew, tab_y + 32], fill=(26, 27, 38, 255))
    d.line([ex, tab_y + 32, ex + ew, tab_y + 32], fill=(35, 37, 50, 255), width=1)
    
    # Active Tab (main.py)
    d.rectangle([ex + 12, tab_y, ex + 140, tab_y + 32], fill=(32, 33, 46, 255))
    d.line([ex + 12, tab_y, ex + 140, tab_y], fill=(99, 102, 241, 255), width=2)
    font_tab = ImageFont.truetype(FONT_REG, 12)
    d.text((ex + 26, tab_y + 8), "🐍  main.py", font=font_tab, fill=(241, 245, 249, 255))
    
    # Inactive Tab (config.json)
    d.text((ex + 158, tab_y + 8), "⚙️  config.json", font=font_tab, fill=(100, 116, 139, 255))
    
    # Gutter & Line Numbers
    gutter_w = 48
    d.rectangle([ex, tab_y + 32, ex + gutter_w, ey + eh], fill=(22, 23, 32, 255))
    d.line([ex + gutter_w, tab_y + 32, ex + gutter_w, ey + eh], fill=(35, 37, 50, 255), width=1)
    
    font_code = ImageFont.truetype(FONT_MONO, 13)
    
    lines = [
        ("1", "# TypEmoji: Instant Unicode Typing for Linux", (100, 116, 139)),
        ("2", "import typemoji, os", (192, 132, 252)),
        ("3", "", (200, 200, 200)),
        ("4", "print('Zero clipboard overwrite • Pure speed')", (244, 244, 245)),
    ]
    
    text_y_start = tab_y + 46
    line_h = 28
    
    for idx, (num, txt, col) in enumerate(lines):
        ly = text_y_start + idx * line_h
        d.text((ex + 16, ly), num, font=font_code, fill=(82, 82, 91, 255))
        d.text((ex + gutter_w + 16, ly), txt, font=font_code, fill=col)
    
    # Line 5: The active typing line
    ly5 = text_y_start + 4 * line_h
    d.text((ex + 16, ly5), "5", font=font_code, fill=(161, 161, 170, 255))
    
    # Active line highlight in gutter/editor
    d.rectangle([ex + gutter_w + 1, ly5 - 3, ex + ew - 1, ly5 + 23], fill=(255, 255, 255, 8))
    
    # Code tokens on Line 5
    d.text((ex + gutter_w + 16, ly5), 'status = ', font=font_code, fill=(244, 244, 245))
    
    l_box = font_code.getbbox('status = ')
    s_x = ex + gutter_w + 16 + (l_box[2] - l_box[0])
    d.text((s_x, ly5), '"Ship faster with ', font=font_code, fill=(134, 239, 172)) # soft green string
    
    str_box = font_code.getbbox('"Ship faster with ')
    cursor_x = s_x + (str_box[2] - str_box[0])
    
    if code_state == "injected":
        # Draw injected rocket emoji!
        rocket_img = get_emoji("🚀", 22)
        overlay.alpha_composite(rocket_img, (cursor_x + 2, ly5 - 3))
        cursor_x += rocket_img.width + 6
        # Closing quote
        d.text((cursor_x, ly5), '"', font=font_code, fill=(134, 239, 172))
        q_box = font_code.getbbox('"')
        cursor_x += (q_box[2] - q_box[0]) + 2
    else:
        # Closing quote in progress
        d.text((cursor_x, ly5), '"', font=font_code, fill=(134, 239, 172))
    
    if cursor_visible:
        d.line([cursor_x, ly5 - 1, cursor_x, ly5 + 17], fill=(99, 102, 241, 255), width=2)
        
    # Line 6
    ly6 = text_y_start + 5 * line_h
    d.text((ex + 16, ly6), "6", font=font_code, fill=(82, 82, 91, 255))
    d.text((ex + gutter_w + 16, ly6), "launch_production()", font=font_code, fill=(147, 197, 253))

def draw_typemoji_panel(overlay, search_text, selected_idx, highlight_enter=False):
    d = ImageDraw.Draw(overlay)
    
    PANEL_W = 370
    PANEL_H = 430
    PANEL_X = 400
    PANEL_Y = 60
    
    # Shadow
    sh = Image.new('RGBA', (WIDTH, HEIGHT), (0, 0, 0, 0))
    d_sh = ImageDraw.Draw(sh)
    d_sh.rounded_rectangle([PANEL_X - 6, PANEL_Y + 10, PANEL_X + PANEL_W + 6, PANEL_Y + PANEL_H + 20],
                           radius=20, fill=(0, 0, 0, 180))
    sh = sh.filter(ImageFilter.GaussianBlur(20))
    overlay.alpha_composite(sh)
    
    # Panel Body
    d.rounded_rectangle([PANEL_X, PANEL_Y, PANEL_X + PANEL_W, PANEL_Y + PANEL_H],
                           radius=16, fill=(32, 33, 36, 255), outline=(255, 255, 255, 35), width=1)
    
    # Header & Search
    search_box = [PANEL_X + 14, PANEL_Y + 12, PANEL_X + PANEL_W - 14, PANEL_Y + 48]
    d.rounded_rectangle(search_box, radius=9, fill=(45, 47, 52, 255), outline=(99, 102, 241, 220), width=1)
    
    # Search icon
    dsi = ImageDraw.Draw(overlay)
    dsi.ellipse([PANEL_X + 24, PANEL_Y + 20, PANEL_X + 34, PANEL_Y + 30], outline=(160, 160, 175, 255), width=2)
    dsi.line([PANEL_X + 32, PANEL_Y + 28, PANEL_X + 38, PANEL_Y + 34], fill=(160, 160, 175, 255), width=2)
    
    font_s = ImageFont.truetype(FONT_REG, 14)
    if search_text:
        d.text((PANEL_X + 48, PANEL_Y + 20), search_text, font=font_s, fill=(255, 255, 255, 255))
        sb = font_s.getbbox(search_text)
        cx = PANEL_X + 48 + (sb[2] - sb[0]) + 2
        d.line([cx, PANEL_Y + 19, cx, PANEL_Y + 39], fill=(99, 102, 241, 255), width=2)
    else:
        d.text((PANEL_X + 48, PANEL_Y + 20), "Search emojis & kaomoji...", font=font_s, fill=(120, 120, 130, 255))
        d.line([PANEL_X + 48, PANEL_Y + 19, PANEL_X + 48, PANEL_Y + 39], fill=(99, 102, 241, 255), width=2)
        
    # Clear / close x
    d.ellipse([PANEL_X + PANEL_W - 38, PANEL_Y + 20, PANEL_X + PANEL_W - 22, PANEL_Y + 36], fill=(255, 255, 255, 20))
    font_x = ImageFont.truetype(FONT_BOLD, 12)
    d.text((PANEL_X + PANEL_W - 33, PANEL_Y + 22), "×", font=font_x, fill=(180, 180, 190, 255))
    
    # Category Tab Bar
    tab_y = PANEL_Y + 54
    tab_emojis = ["🕒", "😀", "👋", "🐱", "🍕", "⚽", "🚀", "💡", "❤️", "(•‿•)"]
    active_tab = 6 if search_text else 1
    
    t_start = PANEL_X + 12
    t_spacing = (PANEL_W - 24) / len(tab_emojis)
    for i, t_char in enumerate(tab_emojis):
        tx = int(t_start + i * t_spacing)
        tw = int(t_spacing)
        if i == active_tab:
            d.rounded_rectangle([tx + 2, tab_y, tx + tw - 2, tab_y + 28], radius=6, fill=(99, 102, 241, 55))
            d.line([tx + 6, tab_y + 28, tx + tw - 6, tab_y + 28], fill=(99, 102, 241, 255), width=2)
        
        if t_char == "(•‿•)":
            font_kao = ImageFont.truetype(FONT_REG, 10)
            d.text((tx + 2, tab_y + 8), "(•‿•)", font=font_kao, fill=(200, 200, 210, 255))
        else:
            t_img = get_emoji(t_char, 16)
            overlay.alpha_composite(t_img, (tx + (tw - t_img.width)//2, tab_y + 5))
        
    d.line([PANEL_X + 12, tab_y + 32, PANEL_X + PANEL_W - 12, tab_y + 32], fill=(255, 255, 255, 20), width=1)
    
    # Emoji Grid
    grid_y = tab_y + 38
    if not search_text:
        grid_items = [
            ["😀", "😃", "😄", "😁", "😆", "😅"],
            ["🚀", "✨", "🔥", "💡", "🐧", "⚡"],
            ["🎉", "🛸", "🪐", "🌟", "💥", "🎯"],
            ["🌌", "🛰️", "🌠", "💫", "🤖", "🕹️"]
        ]
    elif search_text in ["r", "ro"]:
        grid_items = [
            ["🚀", "🤖", "🌈", "💍", "👑", "🌹"],
            ["🔥", "🎯", "🎸", "🏆", "🚗", "⛵"],
            ["🛸", "🪐", "🛰️", "🌠", "💫", "🕹️"],
            ["🔮", "🧪", "🛡️", "⚙️", "🔑", "📦"]
        ]
    else: # "rock" or "rocket"
        grid_items = [
            ["🚀", "🛸", "🛰️", "🪐", "🌌", "🌠"],
            ["🔥", "✨", "⚡", "💥", "💫", "🌟"],
            ["🤖", "🕹️", "🎯", "🏆", "🐧", "💡"],
            ["🔮", "🧪", "🛡️", "⚙️", "🔑", "📦"]
        ]
        
    cell_w = (PANEL_W - 28) // 6
    cell_h = 58
    
    for r_idx, row in enumerate(grid_items):
        for c_idx, em_char in enumerate(row):
            cx = PANEL_X + 14 + c_idx * cell_w
            cy = grid_y + r_idx * cell_h
            
            flat_idx = r_idx * 6 + c_idx
            is_selected = (flat_idx == selected_idx)
            
            if is_selected:
                if highlight_enter:
                    # Flash when enter pressed
                    d.rounded_rectangle([cx + 2, cy + 2, cx + cell_w - 2, cy + cell_h - 2],
                                           radius=10, fill=(99, 102, 241, 160), outline=(255, 255, 255, 255), width=2)
                else:
                    d.rounded_rectangle([cx + 2, cy + 2, cx + cell_w - 2, cy + cell_h - 2],
                                           radius=10, fill=(99, 102, 241, 70), outline=(129, 140, 248, 230), width=2)
            else:
                d.rounded_rectangle([cx + 2, cy + 2, cx + cell_w - 2, cy + cell_h - 2],
                                       radius=8, fill=(255, 255, 255, 6))
                
            e_img = get_emoji(em_char, 30 if is_selected else 28)
            overlay.alpha_composite(e_img, (cx + (cell_w - e_img.width)//2, cy + (cell_h - e_img.height)//2))
            
    # Footer
    footer_y = PANEL_Y + PANEL_H - 52
    d.rounded_rectangle([PANEL_X, footer_y, PANEL_X + PANEL_W, PANEL_Y + PANEL_H],
                           radius=16, fill=(22, 24, 30, 255))
    d.line([PANEL_X, footer_y, PANEL_X + PANEL_W, footer_y], fill=(255, 255, 255, 25), width=1)
    
    active_emoji = "🚀" if (search_text or selected_idx != 0) else "😀"
    active_title = "Rocket" if active_emoji == "🚀" else "Grinning Face"
    active_code = ":rocket: • U+1F680" if active_emoji == "🚀" else ":grinning: • U+1F600"
    
    # Prev box
    d.rounded_rectangle([PANEL_X + 12, footer_y + 8, PANEL_X + 46, footer_y + 42], radius=8, fill=(255, 255, 255, 12))
    p_img = get_emoji(active_emoji, 24)
    overlay.alpha_composite(p_img, (PANEL_X + 12 + (34 - p_img.width)//2, footer_y + 8 + (34 - p_img.height)//2))
    
    font_pt = ImageFont.truetype(FONT_BOLD, 12)
    font_pc = ImageFont.truetype(FONT_MONO, 9)
    d.text((PANEL_X + 54, footer_y + 11), active_title, font=font_pt, fill=(255, 255, 255, 255))
    d.text((PANEL_X + 54, footer_y + 27), active_code, font=font_pc, fill=(148, 163, 184, 255))
    
    # Hints
    font_hint = ImageFont.truetype(FONT_BOLD, 10)
    hint_box = [PANEL_X + PANEL_W - 146, footer_y + 11, PANEL_X + PANEL_W - 12, footer_y + 39]
    d.rounded_rectangle(hint_box, radius=6, fill=(35, 38, 48, 255), outline=(255, 255, 255, 25), width=1)
    d.text((PANEL_X + PANEL_W - 138, footer_y + 17), "↵ Insert   ⇧↵ Keep", font=font_hint, fill=(226, 232, 240, 240))

def draw_hud_keycaps(overlay, keys_list):
    d = ImageDraw.Draw(overlay)
    font_k = ImageFont.truetype(FONT_BOLD, 13)
    font_plus = ImageFont.truetype(FONT_REG, 13)
    
    # Calculate total width
    total_w = 0
    key_widths = []
    for k in keys_list:
        if k == "+":
            w = 16
        else:
            bb = font_k.getbbox(k)
            w = (bb[2] - bb[0]) + 24
        key_widths.append(w)
        total_w += w + 6
    total_w -= 6
    
    hud_x = (WIDTH - total_w) // 2
    hud_y = HEIGHT - 76
    
    # HUD container background
    d.rounded_rectangle([hud_x - 14, hud_y - 8, hud_x + total_w + 14, hud_y + 38],
                           radius=14, fill=(15, 23, 42, 230), outline=(99, 102, 241, 180), width=1)
    
    cur_x = hud_x
    for i, k in enumerate(keys_list):
        kw = key_widths[i]
        if k == "+":
            d.text((cur_x + 4, hud_y + 6), "+", font=font_plus, fill=(148, 163, 184, 255))
        else:
            # Keycap tile
            d.rounded_rectangle([cur_x, hud_y, cur_x + kw, hud_y + 30],
                                   radius=8, fill=(30, 41, 59, 255), outline=(148, 163, 184, 120), width=1)
            d.line([cur_x + 2, hud_y + 29, cur_x + kw - 2, hud_y + 29], fill=(15, 23, 42, 255), width=2)
            bb = font_k.getbbox(k)
            tw = bb[2] - bb[0]
            d.text((cur_x + (kw - tw)//2, hud_y + 7), k, font=font_k, fill=(255, 255, 255, 255))
        cur_x += kw + 6

def draw_toast_notification(overlay, title, sub):
    d = ImageDraw.Draw(overlay)
    tw, th = 330, 56
    tx = WIDTH - tw - 30
    ty = 44
    
    # Shadow
    sh = Image.new('RGBA', (WIDTH, HEIGHT), (0, 0, 0, 0))
    d_sh = ImageDraw.Draw(sh)
    d_sh.rounded_rectangle([tx - 4, ty + 6, tx + tw + 4, ty + th + 12], radius=16, fill=(0, 0, 0, 160))
    sh = sh.filter(ImageFilter.GaussianBlur(12))
    overlay.alpha_composite(sh)
    
    # Toast Body
    d.rounded_rectangle([tx, ty, tx + tw, ty + th], radius=12, fill=(24, 30, 45, 255), outline=(34, 197, 94, 200), width=1)
    
    # Checkmark icon pill
    d.ellipse([tx + 14, ty + 14, tx + 42, ty + 42], fill=(34, 197, 94, 40), outline=(34, 197, 94, 180), width=1)
    font_c = ImageFont.truetype(FONT_BOLD, 15)
    d.text((tx + 22, ty + 17), "✔", font=font_c, fill=(34, 197, 94, 255))
    
    font_t = ImageFont.truetype(FONT_BOLD, 13)
    font_s = ImageFont.truetype(FONT_REG, 11)
    d.text((tx + 52, ty + 12), title, font=font_t, fill=(255, 255, 255, 255))
    d.text((tx + 52, ty + 31), sub, font=font_s, fill=(148, 163, 184, 255))

def generate_frames():
    os.makedirs('/tmp/typemoji_frames', exist_ok=True)
    frames_info = []
    
    specs = [
        # (name, duration_ms, code_state, cursor_vis, show_panel, search_txt, sel_idx, hud_keys, enter_hl, toast)
        ("01_idle", 350, "normal", True, False, "", 0, None, False, None),
        ("02_idle", 350, "normal", False, False, "", 0, None, False, None),
        ("03_idle", 350, "normal", True, False, "", 0, None, False, None),
        ("04_hotkey", 350, "normal", True, False, "", 0, ["Super", "+", "."], False, None),
        ("05_hotkey", 200, "normal", True, False, "", 0, ["Super", "+", "."], False, None),
        ("06_open", 350, "normal", False, True, "", 0, None, False, None),
        ("07_type_r", 180, "normal", False, True, "r", 0, None, False, None),
        ("08_type_ro", 180, "normal", False, True, "ro", 0, None, False, None),
        ("09_type_rock", 180, "normal", False, True, "rock", 0, None, False, None),
        ("10_type_rocket", 450, "normal", False, True, "rocket", 0, None, False, None),
        ("11_press_enter", 300, "normal", False, True, "rocket", 0, ["Enter ↵"], True, None),
        ("12_injected", 350, "injected", True, False, "", 0, None, False, None),
        ("13_injected_toast", 400, "injected", False, False, "", 0, None, False, ("Zero Clipboard Overwrite", "Preserved: 'git push origin main'")),
        ("14_injected_toast", 600, "injected", True, False, "", 0, None, False, ("Zero Clipboard Overwrite", "Preserved: 'git push origin main'")),
        ("15_injected_toast", 750, "injected", False, False, "", 0, None, False, ("Zero Clipboard Overwrite", "Preserved: 'git push origin main'")),
        ("16_hold", 650, "injected", True, False, "", 0, None, False, None),
    ]
    
    print(f"Rendering {len(specs)} animation frames...")
    
    # Wallpaper background
    bg_base = Image.new('RGBA', (WIDTH, HEIGHT), (15, 23, 42, 255))
    d_bg = ImageDraw.Draw(bg_base)
    for y in range(HEIGHT):
        t = y / HEIGHT
        r = int(14 * (1 - t) + 26 * t)
        g = int(18 * (1 - t) + 24 * t)
        b = int(34 * (1 - t) + 68 * t)
        d_bg.line([0, y, WIDTH, y], fill=(r, g, b, 255))
        
    # Subtle ambient lighting in background
    for r in range(250, 0, -5):
        alpha = int(24 * (1 - r / 250)**1.5)
        d_bg.ellipse([580 - r, 250 - r, 580 + r, 250 + r], fill=(99, 102, 241, alpha))

    for idx, (name, dur, c_state, cur_vis, s_panel, s_txt, s_idx, h_keys, ent_hl, toast) in enumerate(specs):
        frame = bg_base.copy()
        overlay = Image.new('RGBA', (WIDTH, HEIGHT), (0, 0, 0, 0))
        d_ov = ImageDraw.Draw(overlay)
        
        # 1. Draw editor
        draw_editor(d_ov, overlay, c_state, cur_vis)
        
        # 2. Draw panel if open
        if s_panel:
            draw_typemoji_panel(overlay, s_txt, s_idx, highlight_enter=ent_hl)
            
        # 3. Draw HUD keycaps if active
        if h_keys:
            draw_hud_keycaps(overlay, h_keys)
            
        # 4. Draw Toast if active
        if toast:
            draw_toast_notification(overlay, toast[0], toast[1])
            
        final_frame = Image.alpha_composite(frame, overlay)
        frame_path = f"/tmp/typemoji_frames/frame_{idx:03d}.png"
        final_frame.save(frame_path)
        frames_info.append((final_frame.convert('RGB'), dur))
        
    print(f"All {len(frames_info)} frames rendered successfully.")
    return frames_info

def compile_gif_palette(frames_info, output_path):
    print("Computing unified master palette across key animation states...")
    # Composite a multi-panel collage of keyframes for optimal color coverage
    raw_images = [img for img, _ in frames_info]
    durations = [dur for _, dur in frames_info]
    
    sample_img = Image.new('RGB', (WIDTH * 2, HEIGHT * 2))
    sample_img.paste(raw_images[3], (0, 0))             # Hotkey HUD
    sample_img.paste(raw_images[9], (WIDTH, 0))         # Search rocket + emojis
    sample_img.paste(raw_images[10], (0, HEIGHT))       # Enter pressed flash
    sample_img.paste(raw_images[13], (WIDTH, HEIGHT))   # Injected emoji + Green toast
    
    # 256 colors adaptive palette
    palette_img = sample_img.quantize(colors=256, method=Image.Resampling.LANCZOS)
    
    print("Quantizing all frames using unified palette with Floyd-Steinberg dithering...")
    quantized_frames = [
        im.quantize(palette=palette_img, dither=Image.Dither.FLOYDSTEINBERG)
        for im in raw_images
    ]
    
    print(f"Saving optimized GIF to {output_path}...")
    quantized_frames[0].save(
        output_path,
        save_all=True,
        append_images=quantized_frames[1:],
        duration=durations,
        loop=0,
        optimize=True
    )
    
    size_bytes = os.path.getsize(output_path)
    print(f"Successfully generated {output_path} ({quantized_frames[0].size[0]}x{quantized_frames[0].size[1]}), size: {size_bytes} bytes ({size_bytes / (1024*1024):.2f} MB)")

if __name__ == '__main__':
    frames = generate_frames()
    compile_gif_palette(frames, 'assets/demo.gif')
