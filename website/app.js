/**
 * TypEmoji — Modern Promotional Experience Script
 * Handles: 3D perspective tilt, floating emoji particle canvas, virtual panel simulator,
 * distro tab switcher, and clipboard copy.
 */

(function () {
  'use strict';

  // State Management
  const state = {
    activeCategory: 'Smileys & Emotion',
    searchQuery: '',
    recentEmojis: ['🚀', '🐧', '🔥', '✨', '💻', '💡', '🎉'],
    isPanelVisible: true,
    shiftKeyDown: false,
    defaultSkinTone: 0
  };

  // ==========================================================================
  // 1. Floating Emoji Particle Canvas (Global Background)
  // ==========================================================================
  function initParticleCanvas() {
    const canvas = document.getElementById('hero-particles');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let width, height;
    const particles = [];
    const emojiPool = ['✨', '🚀', '🐧', '⚡', '💻', '💡', '🎉', '🔥', '💫', '🎨', '🐱'];
    const particleCount = 22;
    let mouseX = -9999;
    let mouseY = -9999;

    function resize() {
      width = canvas.width = window.innerWidth;
      height = canvas.height = window.innerHeight;
    }
    window.addEventListener('resize', resize, { passive: true });
    resize();

    window.addEventListener('mousemove', (e) => {
      mouseX = e.clientX;
      mouseY = e.clientY;
    }, { passive: true });

    class Particle {
      constructor() {
        this.reset(true);
      }
      reset(initial = false) {
        this.x = Math.random() * width;
        this.y = initial ? Math.random() * height : height + 30;
        this.vx = (Math.random() - 0.5) * 0.45;
        this.vy = -(Math.random() * 0.55 + 0.25);
        this.emoji = emojiPool[Math.floor(Math.random() * emojiPool.length)];
        this.size = Math.floor(Math.random() * 8 + 16); // 16px to 24px
        this.alpha = Math.random() * 0.2 + 0.16; // 0.16 to 0.36
        this.rot = Math.random() * Math.PI * 2;
        this.rotSpeed = (Math.random() - 0.5) * 0.01;
      }
      update() {
        this.x += this.vx;
        this.y += this.vy;
        this.rot += this.rotSpeed;

        // Gentle cursor push/deflection
        const dx = this.x - mouseX;
        const dy = this.y - mouseY;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < 120) {
          const force = (120 - dist) / 120;
          this.x += (dx / dist) * force * 2;
          this.y += (dy / dist) * force * 2;
        }

        if (this.y < -30 || this.x < -30 || this.x > width + 30) {
          this.reset(false);
        }
      }
      draw() {
        ctx.save();
        ctx.globalAlpha = this.alpha;
        ctx.translate(this.x, this.y);
        ctx.rotate(this.rot);
        ctx.font = `${this.size}px "Noto Color Emoji", "Apple Color Emoji", "Segoe UI Emoji", sans-serif`;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(this.emoji, 0, 0);
        ctx.restore();
      }
    }

    for (let i = 0; i < particleCount; i++) {
      particles.push(new Particle());
    }

    let isVisible = true;
    document.addEventListener('visibilitychange', () => {
      isVisible = !document.hidden;
    });

    function loop() {
      if (isVisible) {
        ctx.clearRect(0, 0, width, height);
        for (let i = 0; i < particles.length; i++) {
          particles[i].update();
          particles[i].draw();
        }
      }
      requestAnimationFrame(loop);
    }
    loop();
  }

  // ==========================================================================
  // 3. 3D Perspective Tilt on Logo Card
  // ==========================================================================
  function init3DTilt() {
    const tiltWrapper = document.getElementById('hero-tilt-card');
    if (!tiltWrapper) return;
    const card = tiltWrapper.querySelector('.hero-3d-card');
    const glare = tiltWrapper.querySelector('.card-glare');
    if (!card) return;

    tiltWrapper.addEventListener('mousemove', (e) => {
      const rect = tiltWrapper.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;
      const centerX = rect.width / 2;
      const centerY = rect.height / 2;

      // Rotation range: -14 to +14 deg
      const rotX = ((y - centerY) / centerY) * -12;
      const rotY = ((x - centerX) / centerX) * 12;

      card.style.transform = `perspective(1000px) rotateX(${rotX.toFixed(2)}deg) rotateY(${rotY.toFixed(2)}deg) scale3d(1.02, 1.02, 1.02)`;

      if (glare) {
        const glareX = (x / rect.width) * 100;
        const glareY = (y / rect.height) * 100;
        glare.style.background = `radial-gradient(circle at ${glareX}% ${glareY}%, rgba(255, 255, 255, 0.22) 0%, transparent 60%)`;
      }
    });

    tiltWrapper.addEventListener('mouseleave', () => {
      card.style.transform = `perspective(1000px) rotateX(0deg) rotateY(0deg) scale3d(1, 1, 1)`;
      if (glare) {
        glare.style.background = `linear-gradient(135deg, rgba(255, 255, 255, 0.12) 0%, transparent 60%)`;
      }
    });
  }

  // ==========================================================================
  // 4. Toast Notifications
  // ==========================================================================
  function showToast(message, icon = '✓') {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = 'toast';
    toast.innerHTML = `<span class="toast-icon">${icon}</span><span>${message}</span>`;
    container.appendChild(toast);

    requestAnimationFrame(() => {
      toast.classList.add('show');
    });

    setTimeout(() => {
      toast.classList.remove('show');
      setTimeout(() => {
        if (toast.parentNode) toast.parentNode.removeChild(toast);
      }, 250);
    }, 2400);
  }

  // ==========================================================================
  // 5. Interactive In-Browser TypEmoji Panel Simulator
  // ==========================================================================
  function initSimulator() {
    const panelWrapper = document.getElementById('virtual-typemoji-panel');
    const searchInput = document.getElementById('sim-search-input');
    const searchClear = document.getElementById('sim-search-clear');
    const panelClose = document.getElementById('sim-panel-close');
    const gridContainer = document.getElementById('sim-grid-container');
    const categoryTabs = document.querySelectorAll('.cat-tab');
    const previewEmoji = document.getElementById('previewEmoji');
    const previewName = document.getElementById('previewName');
    const previewShortcode = document.getElementById('previewShortcode');
    const targetText = document.getElementById('sim-target-text');

    const sampleBtn = document.getElementById('btn-sample-text');
    const clearBtn = document.getElementById('btn-clear-term');
    const copyTermBtn = document.getElementById('btn-copy-term');
    const summonToggle = document.getElementById('sim-summon-toggle');

    if (!gridContainer || !window.TYPEMOJI_DATA) return;

    const dataset = window.TYPEMOJI_DATA;
    const simSkinToneBar = document.getElementById('simSkinToneBar');

    function isToneCapableEmoji(item) {
      if (item.category === 'Kaomoji') return false;
      if (item.category === 'People & Body') return true;
      const c = item.char;
      return /[\u{1F440}-\u{1F4AA}\u{1F590}-\u{1F596}\u{1F918}-\u{1F91F}\u{1F926}\u{1F937}\u{1F9CF}-\u{1F9DF}\u{1FAF0}-\u{1FAF8}]/u.test(c);
    }

    function applyFitzpatrickTone(char, toneIndex) {
      if (!toneIndex || toneIndex <= 0) return char;
      const clean = char.replace(/\uFE0F/g, '');
      const modifier = String.fromCodePoint(0x1F3FA + toneIndex);
      return clean + modifier;
    }

    // Wire up tone selector swatches
    if (simSkinToneBar) {
      const toneBtns = simSkinToneBar.querySelectorAll('.tone-btn');
      toneBtns.forEach((btn) => {
        btn.addEventListener('click', () => {
          toneBtns.forEach((b) => b.classList.remove('active'));
          btn.classList.add('active');
          state.defaultSkinTone = parseInt(btn.getAttribute('data-tone') || '0', 10);
          renderGrid();
        });
      });
    }

    // Render Grid items based on category and search query
    function renderGrid() {
      gridContainer.innerHTML = '';
      const query = state.searchQuery.trim().toLowerCase();

      // Update skin tone bar visibility: visible for People & Body or search results with tone capability
      if (simSkinToneBar) {
        if (query) {
          simSkinToneBar.classList.remove('hidden');
        } else if (state.activeCategory === 'People & Body') {
          simSkinToneBar.classList.remove('hidden');
        } else {
          simSkinToneBar.classList.add('hidden');
        }
      }

      let items = [];

      if (query) {
        items = dataset.filter((item) => {
          if (item.name.toLowerCase().includes(query)) return true;
          if (item.shortcode.toLowerCase().includes(query)) return true;
          if (item.keywords && item.keywords.some((kw) => kw.toLowerCase().includes(query))) return true;
          if (item.char === query) return true;
          return false;
        });
      } else {
        if (state.activeCategory === 'Recent') {
          items = dataset.filter((item) => state.recentEmojis.includes(item.char));
        } else {
          items = dataset.filter((item) => item.category === state.activeCategory);
        }
      }

      if (items.length === 0) {
        gridContainer.innerHTML = `
          <div class="empty-search-state">
            <span style="font-size: 2rem;">🔍</span>
            <span>No matching emojis or kaomojis for "<strong>${escapeHtml(query)}</strong>"</span>
          </div>
        `;
        return;
      }

      const fragment = document.createDocumentFragment();
      items.forEach((item) => {
        const btn = document.createElement('button');
        const isKaomoji = item.category === 'Kaomoji';
        const isTone = !isKaomoji && isToneCapableEmoji(item);
        const displayChar = isTone && state.defaultSkinTone > 0 ? applyFitzpatrickTone(item.char, state.defaultSkinTone) : item.char;

        btn.className = `emoji-item ${isKaomoji ? 'is-kaomoji' : ''} ${isTone ? 'has-tone' : ''}`;
        btn.textContent = displayChar;
        btn.setAttribute('title', `${item.name} (${item.shortcode})`);
        btn.setAttribute('aria-label', item.name);

        // Hover updates preview bar
        btn.addEventListener('mouseenter', () => {
          updatePreview({ ...item, char: displayChar });
        });

        // Click injects into active host window
        btn.addEventListener('click', (e) => {
          injectEmoji({ ...item, char: displayChar }, e.shiftKey || state.shiftKeyDown);
        });

        fragment.appendChild(btn);
      });

      gridContainer.appendChild(fragment);

      // Default preview to first item
      if (items.length > 0) {
        const first = items[0];
        const isTone = isToneCapableEmoji(first);
        const firstDisplay = isTone && state.defaultSkinTone > 0 ? applyFitzpatrickTone(first.char, state.defaultSkinTone) : first.char;
        updatePreview({ ...first, char: firstDisplay });
      }
    }

    function updatePreview(item) {
      if (!previewEmoji || !previewName || !previewShortcode) return;
      previewEmoji.textContent = item.char;
      previewName.textContent = item.name;
      previewShortcode.textContent = item.shortcode;
      const isKaomoji = item.category === 'Kaomoji' || (item.char && item.char.length > 2);
      previewEmoji.classList.toggle('is-kaomoji', isKaomoji);
    }

    // Direct injection into terminal field
    function injectEmoji(item, keepOpen) {
      // Add to recent emojis
      if (!state.recentEmojis.includes(item.char)) {
        state.recentEmojis.unshift(item.char);
        if (state.recentEmojis.length > 14) state.recentEmojis.pop();
      }

      if (targetText) {
        targetText.focus();
        // Insert character into contenteditable field
        const textNode = document.createTextNode(item.char + ' ');
        const sel = window.getSelection();
        if (sel && sel.rangeCount > 0) {
          const range = sel.getRangeAt(0);
          range.collapse(false);
          range.insertNode(textNode);
          range.setStartAfter(textNode);
          range.setEndAfter(textNode);
          sel.removeAllRanges();
          sel.addRange(range);
        } else {
          targetText.appendChild(textNode);
        }

        // Pulse injection animation
        targetText.style.boxShadow = '0 0 20px rgba(0, 240, 255, 0.4)';
        setTimeout(() => {
          targetText.style.boxShadow = '';
        }, 200);
      }

      if (keepOpen) {
        showToast(`Typed "${item.char}" (Shift active: Panel stays open)`, '⚡');
      } else {
        showToast(`Injected "${item.char}" directly into active window!`, '⌨️');
        // Simulate auto-dismiss feedback
        flashPanelDismiss();
      }
    }

    function flashPanelDismiss() {
      if (!panelWrapper) return;
      panelWrapper.style.transition = 'transform 120ms ease, opacity 120ms ease';
      panelWrapper.style.transform = 'scale(0.97)';
      setTimeout(() => {
        panelWrapper.style.transform = 'scale(1)';
      }, 140);
    }

    function togglePanel(forceState) {
      if (typeof forceState === 'boolean') {
        state.isPanelVisible = forceState;
      } else {
        state.isPanelVisible = !state.isPanelVisible;
      }

      if (state.isPanelVisible) {
        panelWrapper.classList.remove('hidden');
        panelWrapper.classList.add('visible');
        if (searchInput) searchInput.focus();
      } else {
        panelWrapper.classList.remove('visible');
        panelWrapper.classList.add('hidden');
      }
    }

    // Category Tabs Switching
    categoryTabs.forEach((tab) => {
      tab.addEventListener('click', () => {
        categoryTabs.forEach((t) => {
          t.classList.remove('active');
          t.setAttribute('aria-selected', 'false');
        });
        tab.classList.add('active');
        tab.setAttribute('aria-selected', 'true');

        state.activeCategory = tab.getAttribute('data-cat') || 'Smileys & Emotion';
        state.searchQuery = '';
        if (searchInput) searchInput.value = '';
        if (searchClear) searchClear.style.display = 'none';

        renderGrid();
      });
    });

    // Search Input Handling
    if (searchInput) {
      searchInput.addEventListener('input', (e) => {
        state.searchQuery = e.target.value;
        if (searchClear) {
          searchClear.style.display = state.searchQuery ? 'block' : 'none';
        }
        renderGrid();
      });

      searchInput.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') {
          if (searchInput.value) {
            searchInput.value = '';
            state.searchQuery = '';
            if (searchClear) searchClear.style.display = 'none';
            renderGrid();
          } else {
            togglePanel(false);
          }
        }
      });
    }

    if (searchClear) {
      searchClear.addEventListener('click', () => {
        if (searchInput) searchInput.value = '';
        state.searchQuery = '';
        searchClear.style.display = 'none';
        renderGrid();
      });
    }

    if (panelClose) {
      panelClose.addEventListener('click', () => togglePanel(false));
    }

    if (summonToggle) {
      summonToggle.addEventListener('click', () => togglePanel());
    }

    // Terminal host window buttons
    if (sampleBtn && targetText) {
      sampleBtn.addEventListener('click', () => {
        targetText.textContent = 'TypEmoji brings native Wayland & X11 keyboard magic to Linux! 🐧✨ ';
        showToast('Inserted sample terminal buffer');
      });
    }

    if (clearBtn && targetText) {
      clearBtn.addEventListener('click', () => {
        targetText.textContent = '';
        showToast('Cleared active window buffer');
      });
    }

    if (copyTermBtn && targetText) {
      copyTermBtn.addEventListener('click', () => {
        const text = targetText.textContent || '';
        navigator.clipboard.writeText(text).then(() => {
          showToast('Copied terminal output to clipboard!', '📋');
        }).catch(() => {
          showToast('Buffer copied', '📋');
        });
      });
    }

    // Initial render
    renderGrid();

    // Export toggle for global key listener
    window.toggleTypEmojiPanel = togglePanel;
  }

  // ==========================================================================
  // 6. Global Shortcuts & Mechanical Keycap Animation
  // ==========================================================================
  function initKeycapShortcuts() {
    const metaKey = document.getElementById('key-meta');
    const dotKey = document.getElementById('key-dot');
    const heroDemoBtn = document.getElementById('hero-demo-trigger');
    const headerTryBtn = document.getElementById('header-try-btn');

    function animateKeypress() {
      if (metaKey) metaKey.classList.add('pressed');
      if (dotKey) dotKey.classList.add('pressed');

      setTimeout(() => {
        if (metaKey) metaKey.classList.remove('pressed');
        if (dotKey) dotKey.classList.remove('pressed');
      }, 160);
    }

    // Global Key Listener for Meta + . or Super + . or Escape
    window.addEventListener('keydown', (e) => {
      // Track Shift for continuous typing
      if (e.key === 'Shift') state.shiftKeyDown = true;

      // Check for Meta + . or Ctrl + .
      if ((e.metaKey || e.ctrlKey) && (e.key === '.' || e.code === 'Period')) {
        e.preventDefault();
        animateKeypress();
        if (window.toggleTypEmojiPanel) {
          window.toggleTypEmojiPanel();
        }
        // Smooth scroll towards simulator if far away
        const simSection = document.getElementById('simulator');
        if (simSection && Math.abs(window.scrollY - simSection.offsetTop) > 800) {
          simSection.scrollIntoView({ behavior: 'smooth' });
        }
      }

      if (e.key === 'Escape' && window.toggleTypEmojiPanel && state.isPanelVisible) {
        window.toggleTypEmojiPanel(false);
      }
    });

    window.addEventListener('keyup', (e) => {
      if (e.key === 'Shift') state.shiftKeyDown = false;
    });

    if (metaKey) {
      metaKey.addEventListener('click', () => {
        animateKeypress();
        if (window.toggleTypEmojiPanel) window.toggleTypEmojiPanel();
      });
    }
    if (dotKey) {
      dotKey.addEventListener('click', () => {
        animateKeypress();
        if (window.toggleTypEmojiPanel) window.toggleTypEmojiPanel();
      });
    }
    if (heroDemoBtn) {
      heroDemoBtn.addEventListener('click', () => {
        animateKeypress();
        if (window.toggleTypEmojiPanel) window.toggleTypEmojiPanel(true);
        const sim = document.getElementById('simulator');
        if (sim) sim.scrollIntoView({ behavior: 'smooth' });
      });
    }
    if (headerTryBtn) {
      headerTryBtn.addEventListener('click', (e) => {
        e.preventDefault();
        animateKeypress();
        if (window.toggleTypEmojiPanel) window.toggleTypEmojiPanel(true);
        const sim = document.getElementById('simulator');
        if (sim) sim.scrollIntoView({ behavior: 'smooth' });
      });
    }
  }

  // ==========================================================================
  // 7. Interactive Distro Selector
  // ==========================================================================
  function initDistroSelector() {
    const tabs = document.querySelectorAll('.distro-tab-btn');
    const snippetCode = document.getElementById('distro-install-snippet');
    const titleEl = document.getElementById('distro-code-title');
    const artifactEl = document.getElementById('distro-artifact-name');
    const sizeEl = document.getElementById('distro-size');
    const depsEl = document.getElementById('distro-deps');
    const downloadBtn = document.getElementById('distro-direct-download');
    const copyBtn = document.getElementById('btn-copy-install-cmd');

    const distroData = {
      arch: {
        title: 'Terminal — Install on Arch Linux / Manjaro',
        cmd: 'sudo pacman -U https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Arch-Linux-x86_64.pkg.tar.zst',
        artifact: 'TypEmoji-2.3.0-Arch-Linux-x86_64.pkg.tar.zst',
        size: '~774 KB',
        deps: 'qt6-base, libx11, libxtst (optional: kglobalaccel)',
        url: 'https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Arch-Linux-x86_64.pkg.tar.zst'
      },
      ubuntu: {
        title: 'Terminal — Install on Ubuntu / Debian / Linux Mint',
        cmd: 'wget https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Ubuntu-Debian-x86_64.deb && sudo apt install ./TypEmoji-2.3.0-Ubuntu-Debian-x86_64.deb',
        artifact: 'TypEmoji-2.3.0-Ubuntu-Debian-x86_64.deb',
        size: '~1.1 MB',
        deps: 'libqt6core6, libqt6widgets6, libx11-6, libxtst6, ydotool / wtype',
        url: 'https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Ubuntu-Debian-x86_64.deb'
      },
      fedora: {
        title: 'Terminal — Install on Fedora 39/40 / RHEL 9',
        cmd: 'sudo dnf install https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Fedora-openSUSE-x86_64.rpm',
        artifact: 'TypEmoji-2.3.0-Fedora-openSUSE-x86_64.rpm',
        size: '~1.1 MB',
        deps: 'qt6-qtbase, libX11, libXtst, kf6-kglobalaccel (optional)',
        url: 'https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Fedora-openSUSE-x86_64.rpm'
      },
      opensuse: {
        title: 'Terminal — Install on openSUSE Tumbleweed / Leap',
        cmd: 'sudo zypper install https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Fedora-openSUSE-x86_64.rpm',
        artifact: 'TypEmoji-2.3.0-Fedora-openSUSE-x86_64.rpm',
        size: '~1.1 MB',
        deps: 'libQt6Core6, libQt6Widgets6, libX11-6, libXtst6',
        url: 'https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Fedora-openSUSE-x86_64.rpm'
      },
      universal: {
        title: 'Terminal — Universal .run Self-Extracting Installer',
        cmd: 'wget https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Universal-Installer.run && chmod +x TypEmoji-2.3.0-Universal-Installer.run && ./TypEmoji-2.3.0-Universal-Installer.run',
        artifact: 'TypEmoji-2.3.0-Universal-Installer.run',
        size: '~1.1 MB',
        deps: 'Self-contained binary installer with desktop & autostart integration',
        url: 'https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Universal-Installer.run'
      },
      source: {
        title: 'Terminal — Compile from Source (C++23 & Qt 6)',
        cmd: 'git clone https://github.com/Ayan-Jain-737/TypEmoji.git && cd TypEmoji && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && sudo cmake --install build',
        artifact: 'TypEmoji-2.3.0-Portable-Linux-x86_64.tar.gz',
        size: 'Source Tarball',
        deps: 'cmake >= 3.25, ninja, g++-13 / clang-16, qt6-base, libx11-dev',
        url: 'https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Portable-Linux-x86_64.tar.gz'
      }
    };

    tabs.forEach((tab) => {
      tab.addEventListener('click', () => {
        tabs.forEach((t) => t.classList.remove('active'));
        tab.classList.add('active');

        const key = tab.getAttribute('data-distro') || 'arch';
        const d = distroData[key];
        if (!d) return;

        if (snippetCode) snippetCode.textContent = d.cmd;
        if (titleEl) titleEl.textContent = d.title;
        if (artifactEl) artifactEl.textContent = d.artifact;
        if (sizeEl) sizeEl.textContent = d.size;
        if (depsEl) depsEl.textContent = d.deps;
        if (downloadBtn) downloadBtn.setAttribute('href', d.url);
      });
    });

    if (copyBtn && snippetCode) {
      copyBtn.addEventListener('click', () => {
        const text = snippetCode.textContent || '';
        navigator.clipboard.writeText(text).then(() => {
          showToast('Installation command copied to clipboard!', '📋');
          const originalText = copyBtn.querySelector('.copy-text');
          if (originalText) {
            originalText.textContent = 'Copied!';
            setTimeout(() => {
              originalText.textContent = 'Copy Command';
            }, 1800);
          }
        }).catch(() => {
          showToast('Command copied');
        });
      });
    }
  }

  // ==========================================================================
  // 8. Kaomoji Interactive Clip Station
  // ==========================================================================
  function initKaomojiStation() {
    const container = document.getElementById('kaomoji-clip-grid');
    if (!container || !window.TYPEMOJI_DATA) return;

    const topChars = ['(•‿•)', '(◕‿◕✿)', '¯\\_(ツ)_/¯', '( ͡° ͜ʖ ͡°)', '(｡♥‿♥｡)', 'ʕ•ᴥ•ʔ', '(>_<)', 'ಠ_ಠ', '(☞ﾟヮﾟ)☞', '(づ￣ ³￣)づ', "(ง'̀-'́)ง", '(ノಠ益ಠ)ノ彡┻━┻'];
    const kaomojis = window.TYPEMOJI_DATA
      .filter((item) => item.category === 'Kaomoji' && topChars.includes(item.char))
      .sort((a, b) => topChars.indexOf(a.char) - topChars.indexOf(b.char));

    const fragment = document.createDocumentFragment();
    kaomojis.forEach((k) => {
      const card = document.createElement('button');
      card.className = 'kaomoji-clip-btn';
      card.setAttribute('aria-label', `Copy ${k.name}`);
      card.innerHTML = `
        <span class="kaomoji-char">${escapeHtml(k.char)}</span>
        <span class="kaomoji-name">${escapeHtml(k.name)}</span>
      `;

      card.addEventListener('click', () => {
        navigator.clipboard.writeText(k.char).then(() => {
          showToast(`Copied ${k.char} (${k.name})`, '🎉');
        }).catch(() => {
          showToast(`Copied ${k.char}`);
        });

        // Tactile bounce animation
        card.style.transform = 'scale(1.08)';
        card.style.borderColor = 'var(--neon-cyan)';
        setTimeout(() => {
          card.style.transform = '';
          card.style.borderColor = '';
        }, 180);
      });

      fragment.appendChild(card);
    });

    container.appendChild(fragment);
  }

  // ==========================================================================
  // 9. Scroll Header Glass State & Reveal Animations
  // ==========================================================================
  function initScrollEffects() {
    const header = document.querySelector('.site-header');
    window.addEventListener('scroll', () => {
      if (!header) return;
      if (window.scrollY > 20) {
        header.classList.add('scrolled');
      } else {
        header.classList.remove('scrolled');
      }
    }, { passive: true });

    // Reveal animations using IntersectionObserver
    const observer = new IntersectionObserver((entries) => {
      entries.forEach((entry) => {
        if (entry.isIntersecting) {
          entry.target.classList.add('revealed');
          observer.unobserve(entry.target);
        }
      });
    }, { threshold: 0.15 });

    document.querySelectorAll('.arch-card, .bench-card, .cli-terminal-card').forEach((el) => {
      observer.observe(el);
    });
  }

  // Utility to escape HTML
  function escapeHtml(str) {
    if (!str) return '';
    return str
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#039;');
  }

  // ==========================================================================
  // Initialize Everything on DOM Ready
  // ==========================================================================
  document.addEventListener('DOMContentLoaded', () => {
    initParticleCanvas();
    init3DTilt();
    initSimulator();
    initKeycapShortcuts();
    initDistroSelector();
    initKaomojiStation();
    initScrollEffects();
  });

})();
