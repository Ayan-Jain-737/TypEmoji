# Research Findings - Plasma Emoji Selector UI/UX

## Issue Summary
User reports:
1. UI/UX does not match Plasma Emoji Selector
2. Emojis not visible (UTF-8 encoding issue)
3. Emojis not being typed

## Code Review Findings

### Emoji Visibility / UTF-8 Encoding
From `EmojiDatabase.cpp` line 208-228, `codepointsToChar` already uses `QString::fromUcs4(&cp, 1)` for supplementary characters (codepoint > 0xFFFF). This should handle surrogate pairs correctly.

However, potential issues:
1. `EmojiPanel.cpp` uses `QListView` in IconMode with `setIconSize(QSize(36,36))` and `setGridSize(QSize(52,52))`
2. No `QStyledItemDelegate` is used - emojis rendered as plain text
3. Model provides `EmojiCharRole` as QString, which should be fine
4. Font rendering may be missing emoji-capable font; Qt default font may not support emoji glyphs

### UI Replication Gaps

Current implementation (`EmojiPanel.cpp`):
- Header height: 48px ✅
- Search field with placeholder "Search", clear button ✅
- Category tabs with single emoji characters, 9 tabs ✅
- Panel size: min 360x400, max 480x580
- Rounded corners 12px ✅

Plasma Emoji Selector characteristics from source:
- Uses Kirigami.ScrollablePage with Kirigami.SearchField
- Grid layout with emoji items 40px
- Categories shown in left sidebar in some versions, tabs in others
- Search field at top of each page
- Emoji grid with about 6 columns

Key differences identified:
1. Panel width too narrow (360-480 vs Plasma ~500-600)
2. Grid size 52x52 may be too small; Plasma uses larger emoji with more spacing
3. Category tab bar design: Plasma uses bottom tab bar or left sidebar; current uses QTabBar at top
4. Missing proper emoji delegate with font sizing
5. Header search icon emoji 🔍 may not render properly

### Injection Issue
`EmojiInjector.cpp` tries ydotool → wtype → XTest → clipboard fallback
Wayland issue: ydotool requires root/setuid, wtype may not work without proper portal access
No logging of which path succeeded

## Recommendations

1. **UI Replication**
   - Increase panel size to 500-560px width
   - Change gridSize to 64x64, iconSize 44x44
   - Add QStyledItemDelegate to render emojis with 28-32pt emoji font
   - Match header style: search field with rounded 20px background, magnifying glass left
   - Category tabs: 40px high, emoji 22-24pt

2. **Encoding Fix**
   - Verify font supports emoji: set font family to "Segoe UI Emoji", "Noto Color Emoji", "Apple Color Emoji", system
   - Ensure QListView uses proper font for items
   - Test codepointsToChar with U+1F600 etc

3. **Typing Fix**
   - Log which injection method is used
   - Add 30ms delay before typing
   - Prioritize ydotool with better error handling
   - Show user feedback when clipboard fallback used

## References
- Plasma emojier source: https://invent.kde.org/plasma/plasma-desktop/-/tree/master/emojier
- Emoji-test.txt parsing: Unicode standard

## Next Steps
Implement delegate and UI adjustments
