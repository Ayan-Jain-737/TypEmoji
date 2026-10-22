# Plasma Emoji Selector UI/UX Research Summary

## Primary Sources Investigated

### Plasma Desktop Repository
- https://github.com/KDE/plasma-desktop/tree/master/emojier
  The Emoji Selector is bound to Meta+dot by default per CODEMAP.md
  - emojier/app/ui/CategoryPage.qml - Main page component
  - Uses Kirigami.ScrollablePage with Kirigami.SearchField
  - Uses org.kde.plasma.emoji components

### Code Analysis Findings

**Current Implementation Analysis**

1. **EmojiPanel.cpp:setupUi()** lines 56-138
   - Creates container with rounded corners 12px
   - Header 48px height with search field
   - Category tab bar with 9 tabs using single emoji characters

2. **EmojiPanel.cpp:createCategoryViews()** lines 173-208
   - QListView IconMode
   - IconSize 36x36, GridSize 52x52
   - Spacing 4px
   - No custom delegate

3. **EmojiDatabase.cpp:codepointsToChar()** lines 208-228
   - Properly handles supplementary characters using QString::fromUcs4(&cp, 1)
   - Should handle emoji correctly

4. **EmojiInjector.cpp** lines 17-42
   - Tries ydotool → wtype → XTest → clipboard fallback
   - No logging of which method succeeds

## UI/UX Differences vs Plasma

### Plasma Emoji Selector Characteristics (from source analysis)

1. **Layout Structure**
   - Uses Kirigami.ScrollablePage with top search field
   - Categories likely in sidebar or tab bar
   - Emoji grid with larger cells

2. **Search Integration**
   - SearchField always visible, focuses on page change
   - Search filters current category

3. **Visual Design**
   - Uses system theme
   - Kirigami controls provide consistent spacing
   - Grid items have proper padding

### Current Gaps

1. **Panel Sizing**
   - Current: min 360x400, max 480x580
   - Expected: ~500-560 width for Plasma-style

2. **Grid Layout**
   - Current grid 52x52 with 36px icon
   - Plasma uses larger emoji with more breathing room

3. **Rendering**
   - No QStyledItemDelegate
   - Font may not be emoji-capable
   - QListView text rendering may use system font without emoji support

4. **Category Mapping**
   - Current uses 9 hardcoded tabs: 😀🐶🍕⚽🌍💡🔣🎌⏱
   - Plasma categories: Smileys & Emotion, People & Body, Animals & Nature, Food & Drink, Activities, Travel & Places, Objects, Symbols, Flags

## UTF-8 Encoding Investigation

**Code Review**: EmojiDatabase.cpp line 119, 177
```cpp
entry.emojiChar = codepointsToChar(codepoints);
```

codepointsToChar correctly parses hex codepoints and uses QString::fromUcs4 for >0xFFFF.

**Potential Issues**:
1. Font does not contain emoji glyphs → shows boxes
2. QListView item rendering uses default font which may lack emoji
3. Item delegate not set, so emoji might be rendered too small

## Injection Issue

**Wayland Limitations**:
- ydotool requires root privileges or setuid
- wtype requires text-input-v3 portal
- XTest only works on X11/Xwayland
- Clipboard fallback is unreliable

## Recommended Fixes

1. **Add Emoji Delegate**
   Create QStyledItemDelegate that:
   - Renders emoji character with 28-32pt emoji font
   - Centers emoji in item rect
   - Uses font family: "Noto Color Emoji", "Apple Color Emoji", "Segoe UI Emoji", sans-serif

2. **Update Panel Sizing**
   - SetMinimumSize(440, 520)
   - SetMaximumSize(560, 720)
   - GridSize 64x64, iconSize 44x44

3. **Improve Injection Logging**
   - Add qDebug for which method succeeds
   - Add 30ms delay before typing
   - Show user notification on fallback

## Citations

- Plasma emojier location: CODEMAP.md states "emojier/ The Emoji Selector, bound to Meta+dot by default"
- CategoryPage.qml structure from invent.kde.org/plasma/plasma-desktop/-/raw/master/emojier/app/ui/CategoryPage.qml
- Current code: /home/furious/Projects/emojiboard/src/panel/EmojiPanel.cpp:186-187 sets iconSize 36, gridSize 52
- Current code: /home/furious/Projects/emojiboard/src/data/EmojiDatabase.cpp:222-224 uses QString::fromUcs4 for supplementary characters
