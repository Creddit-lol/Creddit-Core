
Debian
====================
This directory contains files used to package credditd/creddit-qt
for Debian-based Linux systems. If you compile credditd/creddit-qt yourself, there are some useful files here.

## creddit: URI support ##


creddit-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install creddit-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your creddit-qt binary to `/usr/bin`
and the `../../share/pixmaps/creddit128.png` to `/usr/share/pixmaps`

creddit-qt.protocol (KDE)

