import os
import math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

WIDTH = 1280
HEIGHT = 640

# Fonts
FONT_BOLD = '/usr/share/fonts/liberation/LiberationSans-Bold.ttf'
FONT_REG = '/usr/share/fonts/liberation/LiberationSans-Regular.ttf'
FONT_MONO = '/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf'
FONT_MONO_REG = '/usr/share/fonts/TTF/DejaVuSansMono.ttf'
FONT_EMOJI = '/usr/share/fonts/noto/NotoColorEmoji.ttf'

emoji_font = ImageFont.truetype(FONT_EMOJI, 109)

def render_emoji(char, size):
    canvas = Image.new('RGBA', (140, 140), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw.text((10, 10), char, font=emoji_font, embedded_color=True)
    bbox = canvas.getbbox()
    if bbox:
        cropped = canvas.crop(bbox)
        w, h = cropped.size
        scale = size / max(w, h)
        nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
        return cropped.resize((nw, nh), Image.Resampling.LANCZOS)
    return Image.new('RGBA', (size, size), (0, 0, 0, 0))

def draw_rounded_rect(draw, bbox, radius, fill=None, outline=None, width=1):
    draw.rounded_rectangle(bbox, radius=radius, fill=fill, outline=outline, width=width)

def create_social_preview():
    # Base image with dark luxury gradient: from (15, 23, 42) to (30, 27, 75)
    base = Image.new('RGBA', (WIDTH, HEIGHT), (15, 23, 42, 255))
    
    # Draw background gradient manually for perfect smoothness
    # Top-left #0b1120 (11, 17, 32) -> Bottom-right #1e1b4b (30, 27, 75)
    grad = Image.new('RGBA', (WIDTH, HEIGHT))
    draw_g = ImageDraw.Draw(grad)
    for y in range(HEIGHT):
        t_y = y / HEIGHT
        for x in range(0, WIDTH, 4):
            t_x = x / WIDTH
            t = (t_x * 0.6 + t_y * 0.4)
            r = int(12 * (1 - t) + 30 * t)
            g = int(18 * (1 - t) + 26 * t)
            b = int(34 * (1 - t) + 82 * t)
            draw_g.rectangle([x, y, x + 4, y + 1], fill=(r, g, b, 255))
    base = Image.alpha_composite(base, grad)

    # Ambient Glow Orbs
    glow = Image.new('RGBA', (WIDTH, HEIGHT), (0, 0, 0, 0))
    draw_glow = ImageDraw.Draw(glow)
    
    # Glowing orb behind UI mockup (Indigo / Cyan)
    for r in range(320, 0, -4):
        alpha = int(45 * (1 - r / 320)**1.8)
        draw_glow.ellipse([980 - r, 310 - r, 980 + r, 310 + r], fill=(99, 102, 241, alpha))
        
    for r in range(220, 0, -4):
        alpha = int(35 * (1 - r / 220)**1.5)
        draw_glow.ellipse([1120 - r, 180 - r, 1120 + r, 180 + r], fill=(56, 189, 248, alpha))

    # Soft ambient glow behind branding (left side)
    for r in range(260, 0, -4):
        alpha = int(25 * (1 - r / 260)**1.5)
        draw_glow.ellipse([260 - r, 240 - r, 260 + r, 240 + r], fill=(139, 92, 246, alpha))

    base = Image.alpha_composite(base, glow)

    # Subtle grid lines / dot mesh
    mesh = Image.new('RGBA', (WIDTH, HEIGHT), (0, 0, 0, 0))
    draw_mesh = ImageDraw.Draw(mesh)
    for gx in range(40, WIDTH, 40):
        for gy in range(40, HEIGHT, 40):
            draw_mesh.ellipse([gx - 1, gy - 1, gx + 1, gy + 1], fill=(255, 255, 255, 14))
    base = Image.alpha_composite(base, mesh)

    # --- LEFT SIDE: BRANDING & FEATURES ---
    overlay = Image.new('RGBA', (WIDTH, HEIGHT), (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    # App Icon (84x84)
    icon_raw = Image.open('assets/icons/emojityper-128.png').convert('RGBA')
    icon_84 = icon_raw.resize((84, 84), Image.Resampling.LANCZOS)
    
    # Icon shadow
    icon_shadow = Image.new('RGBA', (96, 96), (0, 0, 0, 0))
    d_is = ImageDraw.Draw(icon_shadow)
    d_is.rounded_rectangle([6, 6, 90, 90], radius=22, fill=(0, 0, 0, 160))
    icon_shadow = icon_shadow.filter(ImageFilter.GaussianBlur(8))
    overlay.alpha_composite(icon_shadow, (76, 78))
    overlay.alpha_composite(icon_84, (80, 80))

    # Brand Title: "TypEmoji"
    font_title = ImageFont.truetype(FONT_BOLD, 54)
    draw.text((184, 80), "TypEmoji", font=font_title, fill=(255, 255, 255, 255))

    # Version pill next to title
    v_pill_box = [436, 94, 520, 126]
    draw.rounded_rectangle(v_pill_box, radius=8, fill=(99, 102, 241, 60), outline=(129, 140, 248, 140), width=1)
    font_v = ImageFont.truetype(FONT_BOLD, 14)
    draw.text((450, 101), "v2.0.0", font=font_v, fill=(199, 210, 254, 255))

    # Tagline: "Native Emoji & Symbol Picker for Linux"
    font_tagline = ImageFont.truetype(FONT_BOLD, 25)
    draw.text((80, 186), "Native Emoji & Symbol Picker for Linux", font=font_tagline, fill=(56, 189, 248, 255))

    # Subtitle / description
    font_sub = ImageFont.truetype(FONT_REG, 16)
    sub_text1 = "Trigger globally with Super + . and type Unicode 16.0 emojis directly"
    sub_text2 = "into active windows without altering or overwriting your clipboard."
    draw.text((80, 226), sub_text1, font=font_sub, fill=(203, 213, 225, 230))
    draw.text((80, 250), sub_text2, font=font_sub, fill=(148, 163, 184, 220))

    # Feature Pills (Pill badges)
    pills = [
        [("🐧", "Wayland & X11"), ("⌨️", "Meta + . (Super + .)"), ("✨", "Unicode 16.0")],
        [("🔒", "Zero Clipboard Overwrite"), ("⚡", "C++23 & Qt6"), ("🚀", "Instant Injection")]
    ]
    font_pill = ImageFont.truetype(FONT_BOLD, 14)

    start_y = 310
    for row_idx, row in enumerate(pills):
        cur_x = 80
        cur_y = start_y + row_idx * 52
        for icon_char, label in row:
            # Measure text width
            left, top, right, bottom = font_pill.getbbox(label)
            text_w = right - left
            pill_w = text_w + 54
            pill_h = 38
            
            # Draw pill background & border
            draw.rounded_rectangle([cur_x, cur_y, cur_x + pill_w, cur_y + pill_h],
                                   radius=12,
                                   fill=(30, 41, 59, 190),
                                   outline=(148, 163, 184, 60),
                                   width=1)
            
            # Render pill emoji
            em_img = render_emoji(icon_char, 20)
            overlay.alpha_composite(em_img, (cur_x + 12, cur_y + 9))
            
            # Draw label
            draw.text((cur_x + 38, cur_y + 11), label, font=font_pill, fill=(241, 245, 249, 255))
            cur_x += pill_w + 12

    # Environment badges / compatibility bar at bottom left
    comp_box = [80, 442, 690, 560]
    draw.rounded_rectangle(comp_box, radius=14, fill=(15, 23, 42, 160), outline=(51, 65, 85, 120), width=1)
    
    font_sec_title = ImageFont.truetype(FONT_BOLD, 13)
    draw.text((100, 456), "SEAMLESS DESKTOP INTEGRATION", font=font_sec_title, fill=(148, 163, 184, 220))

    envs = ["GNOME 44+", "KDE Plasma 6", "Hyprland", "Sway", "XFCE", "X11 & Wayland"]
    font_env = ImageFont.truetype(FONT_REG, 13)
    env_x = 100
    for env in envs:
        l, t, r, b = font_env.getbbox(env)
        ew = r - l + 20
        draw.rounded_rectangle([env_x, 486, env_x + ew, 514], radius=6, fill=(30, 41, 59, 200), outline=(71, 85, 105, 100))
        draw.text((env_x + 10, 492), env, font=font_env, fill=(226, 232, 240, 240))
        env_x += ew + 8

    font_ftr = ImageFont.truetype(FONT_REG, 13)
    draw.text((100, 528), "Free & Open Source under MIT License  •  Fast, Minimal & Pure Native", font=font_ftr, fill=(100, 116, 139, 240))

    # --- RIGHT SIDE: FLOATING REALISTIC UI MOCKUP ---
    # Panel coordinates
    PANEL_W = 430
    PANEL_H = 524
    PANEL_X = 770
    PANEL_Y = 58

    # Multi-stage drop shadow for panel
    shadow_img = Image.new('RGBA', (WIDTH, HEIGHT), (0, 0, 0, 0))
    d_sh = ImageDraw.Draw(shadow_img)
    # Deep shadow
    d_sh.rounded_rectangle([PANEL_X - 10, PANEL_Y + 14, PANEL_X + PANEL_W + 10, PANEL_Y + PANEL_H + 26],
                           radius=24, fill=(0, 0, 0, 170))
    shadow_img = shadow_img.filter(ImageFilter.GaussianBlur(24))
    overlay = Image.alpha_composite(shadow_img, overlay)
    draw = ImageDraw.Draw(overlay)

    # Panel container (#202124 elevated dark)
    draw.rounded_rectangle([PANEL_X, PANEL_Y, PANEL_X + PANEL_W, PANEL_Y + PANEL_H],
                           radius=18,
                           fill=(32, 33, 36, 255),
                           outline=(255, 255, 255, 35),
                           width=1)

    # Top Header: Search bar (Height ~56)
    search_box = [PANEL_X + 16, PANEL_Y + 14, PANEL_X + PANEL_W - 16, PANEL_Y + 54]
    draw.rounded_rectangle(search_box, radius=10, fill=(45, 47, 52, 255), outline=(99, 102, 241, 220), width=2)

    # Vector search icon (Magnifying glass)
    search_icon_img = Image.new('RGBA', (24, 24), (0, 0, 0, 0))
    dsi = ImageDraw.Draw(search_icon_img)
    dsi.ellipse([3, 3, 14, 14], outline=(160, 160, 175, 255), width=2)
    dsi.line([12, 12, 20, 20], fill=(160, 160, 175, 255), width=2)
    overlay.alpha_composite(search_icon_img, (PANEL_X + 28, PANEL_Y + 22))

    # Search text: "rocket" + glowing cursor
    font_search = ImageFont.truetype(FONT_REG, 15)
    draw.text((PANEL_X + 58, PANEL_Y + 23), "rocket", font=font_search, fill=(255, 255, 255, 255))
    
    # Cursor
    l, t, r, b = font_search.getbbox("rocket")
    cur_x = PANEL_X + 58 + (r - l) + 2
    draw.line([cur_x, PANEL_Y + 22, cur_x, PANEL_Y + 44], fill=(99, 102, 241, 255), width=2)

    # Clear button (x)
    draw.ellipse([PANEL_X + PANEL_W - 46, PANEL_Y + 22, PANEL_X + PANEL_W - 24, PANEL_Y + 44], fill=(255, 255, 255, 20))
    font_x = ImageFont.truetype(FONT_BOLD, 14)
    draw.text((PANEL_X + PANEL_W - 39, PANEL_Y + 24), "×", font=font_x, fill=(200, 200, 210, 230))

    # Category Tab Bar (Height ~38)
    tab_y = PANEL_Y + 62
    tab_emojis = ["🕒", "😀", "👋", "🐱", "🍕", "🚀", "💡", "❤️", "🚩", "(•‿•)"]
    active_tab_idx = 6 # "🚀" is active

    tab_start_x = PANEL_X + 16
    tab_spacing = (PANEL_W - 32) / len(tab_emojis)

    for i, t_char in enumerate(tab_emojis):
        tx = int(tab_start_x + i * tab_spacing)
        tw = int(tab_spacing)
        if i == active_tab_idx:
            # Active tab background & bottom indicator
            draw.rounded_rectangle([tx + 2, tab_y, tx + tw - 2, tab_y + 32], radius=8, fill=(99, 102, 241, 55))
            draw.line([tx + 6, tab_y + 32, tx + tw - 6, tab_y + 32], fill=(99, 102, 241, 255), width=3)
        
        t_img = render_emoji(t_char, 18)
        overlay.alpha_composite(t_img, (tx + (tw - t_img.width)//2, tab_y + 6))

    # Tab separator line
    draw.line([PANEL_X + 14, tab_y + 36, PANEL_X + PANEL_W - 14, tab_y + 36], fill=(255, 255, 255, 20), width=1)

    # Emoji Grid Area (4 rows x 6 columns)
    grid_y = tab_y + 44
    grid_emojis = [
        ["🚀", "✨", "🔥", "💡", "🐧", "⚡"],
        ["🎉", "🛸", "🪐", "🌟", "💥", "🎯"],
        ["🌌", "🛰️", "🌠", "💫", "🤖", "🕹️"],
        ["🔮", "🧪", "🛡️", "⚙️", "🔑", "🏆"]
    ]

    cell_w = (PANEL_W - 36) // 6
    cell_h = 70

    for r_idx, row in enumerate(grid_emojis):
        for c_idx, em_char in enumerate(row):
            cx = PANEL_X + 18 + c_idx * cell_w
            cy = grid_y + r_idx * cell_h
            
            is_selected = (r_idx == 0 and c_idx == 0) # 🚀 is selected
            if is_selected:
                # Highlighted cell
                draw.rounded_rectangle([cx + 2, cy + 2, cx + cell_w - 2, cy + cell_h - 2],
                                       radius=12,
                                       fill=(99, 102, 241, 70),
                                       outline=(129, 140, 248, 240),
                                       width=2)
                # Outer glow for selection
                glow_box = [cx - 1, cy - 1, cx + cell_w + 1, cy + cell_h + 1]
                draw.rounded_rectangle(glow_box, radius=14, outline=(99, 102, 241, 100), width=1)
            else:
                # Normal cell subtle hover or dark tile
                draw.rounded_rectangle([cx + 2, cy + 2, cx + cell_w - 2, cy + cell_h - 2],
                                       radius=10,
                                       fill=(255, 255, 255, 8 if (r_idx+c_idx)%2==0 else 4))

            # Render emoji
            em_sz = 34 if is_selected else 32
            e_img = render_emoji(em_char, em_sz)
            overlay.alpha_composite(e_img, (cx + (cell_w - e_img.width)//2, cy + (cell_h - e_img.height)//2))

            # Subtle skin-tone badge dot for humanoid/relevant emojis
            if em_char in ["👋", "🤖", "🐧"]:
                draw.ellipse([cx + cell_w - 12, cy + cell_h - 12, cx + cell_w - 6, cy + cell_h - 6], fill=(255, 185, 80, 220))

    # Footer Preview Widget (Height ~56)
    footer_y = PANEL_Y + PANEL_H - 58
    # Footer background
    draw.rounded_rectangle([PANEL_X, footer_y, PANEL_X + PANEL_W, PANEL_Y + PANEL_H],
                           radius=18, fill=(22, 24, 30, 255))
    # Footer top border
    draw.line([PANEL_X, footer_y, PANEL_X + PANEL_W, footer_y], fill=(255, 255, 255, 25), width=1)

    # Preview box (left)
    prev_box = [PANEL_X + 14, footer_y + 8, PANEL_X + 54, footer_y + 48]
    draw.rounded_rectangle(prev_box, radius=10, fill=(255, 255, 255, 14), outline=(255, 255, 255, 25), width=1)
    prev_em = render_emoji("🚀", 28)
    overlay.alpha_composite(prev_em, (PANEL_X + 14 + (40 - prev_em.width)//2, footer_y + 8 + (40 - prev_em.height)//2))

    # Preview Text (middle)
    font_prev_title = ImageFont.truetype(FONT_BOLD, 13)
    font_prev_code = ImageFont.truetype(FONT_MONO_REG, 10)
    draw.text((PANEL_X + 64, footer_y + 11), "Rocket", font=font_prev_title, fill=(255, 255, 255, 255))
    draw.text((PANEL_X + 64, footer_y + 30), ":rocket:  •  U+1F680", font=font_prev_code, fill=(148, 163, 184, 255))

    # Preview Shortcuts Hints (right)
    hint_box = [PANEL_X + PANEL_W - 168, footer_y + 12, PANEL_X + PANEL_W - 14, footer_y + 44]
    draw.rounded_rectangle(hint_box, radius=8, fill=(35, 38, 48, 255), outline=(255, 255, 255, 30), width=1)
    font_hints = ImageFont.truetype(FONT_BOLD, 11)
    draw.text((PANEL_X + PANEL_W - 158, footer_y + 18), "↵ Insert    ⇧↵ Keep", font=font_hints, fill=(226, 232, 240, 240))

    # Composite all layers
    final_img = Image.alpha_composite(base, overlay)
    
    # Save optimized PNG
    out_path = 'assets/social-preview.png'
    final_img.save(out_path, format='PNG', optimize=True)
    print(f"Generated {out_path} ({final_img.size[0]}x{final_img.size[1]}), size: {os.path.getsize(out_path)} bytes")

if __name__ == '__main__':
    create_social_preview()
