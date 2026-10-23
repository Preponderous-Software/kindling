Kindling
========

A cozy first-person homestead game: clear, mend and furnish an abandoned
cabin in the hills, and kindle its hearth before its new settler arrives
at the first frost.

Play
----

Windows:  double-click kindling.exe. Windows may say it protected your PC,
          because the game is not signed: choose "More info", then
          "Run anyway".
Linux:    ./kindling   (needs OpenGL 4.5, which any current Mesa or
          vendor driver has)

Press F1 in the game for the controls. Click in the window to look around
with the mouse; Esc frees the mouse, and Esc again quits.

The game saves on its own every two minutes and when you quit, and
continues where you left off:

  Windows:  %APPDATA%\Kindling\default
  Linux:    ~/.local/share/kindling/default  (or $XDG_DATA_HOME/kindling/default)

  kindling --new          start over (the old save is kept as the previous one)
  kindling --slot DIR     keep the save somewhere else
  kindling --autopilot --speed 4
                          watch the autopilot restore the cabin

More: https://github.com/Preponderous-Software/kindling
Play in a browser: https://kindling.play.danielstephenson.dev

Kindling is MIT-licensed (LICENSE.txt). It is built on the Strata engine;
THIRD-PARTY.md lists what that links in, and their licences.
