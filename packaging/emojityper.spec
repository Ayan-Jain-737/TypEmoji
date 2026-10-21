Name:           emojityper
Version:        1.0.0
Release:        1%{?dist}
Summary:        Direct emoji picker and typing utility for Linux (X11 & Wayland)

License:        MIT
URL:            https://github.com/Ayan-Jain-737/TypEmoji
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  kf6-kglobalaccel-devel
BuildRequires:  kf6-kxmlgui-devel
BuildRequires:  libX11-devel
BuildRequires:  libXtst-devel

Requires:       qt6-qtbase
Requires:       kf6-kglobalaccel
Requires:       google-noto-color-emoji-fonts

Recommends:     wl-clipboard
Recommends:     wtype
Recommends:     xclip

%description
EmojiTyper is a lightweight, responsive emoji picker and direct typing tool for Linux.
It works natively on both X11 and Wayland (KDE Plasma, GNOME, Sway, and other compositors),
allowing users to quickly insert emojis into any focused application.

%prep
%autosetup

%build
%cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%check
%ctest

%files
%license LICENSE
%doc README.md
%{_bindir}/EmojiTyper
%{_datadir}/applications/org.emojiboard.EmojiTyper.desktop
%{_datadir}/emojityper/
%{_datadir}/icons/hicolor/scalable/apps/emojityper.svg
%{_userunitdir}/emojityper.service

%changelog
* Wed Oct 21 2026 EmojiTyper Contributors <contact@emojityper.org> - 1.0.0-1
- Initial release for Fedora / openSUSE
