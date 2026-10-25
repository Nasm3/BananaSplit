Splitter @VERSION@ for Windows - six numbered virtual audio cables for BananaSplit
==============================================================================

Splitter adds six virtual cables to Windows. Cable N is:

  "Splitter N"         a playback device. Choose it as an app's output
                       (Discord, a browser, OBS, a game) to send that app
                       to BananaSplit.
  "Splitter N Output"  a recording device. BananaSplit's input strip N
                       records it, so whatever plays into "Splitter N"
                       arrives on strip N.

All cables run at 48 kHz stereo. The sample format (16-bit, 24-bit or
32-bit float) can be set per device under Sound settings > device >
Properties > Advanced. Each cable adds about 30 ms of delay.


Install
-------
Double-click install.cmd and allow it to make changes. It installs the
driver and creates the six cables. Running it again updates the driver.

THIS IS A TEST BUILD unless this folder has no splitter-test.cer file. A
test build is signed with a test certificate, not by Microsoft, and Windows
only loads it in test-signing mode:

  1. In an administrator Command Prompt:  bcdedit /set testsigning on
     (Secure Boot must be off for this; some games' anti-cheat refuse to
     run in test mode.)
  2. Restart the PC.
  3. Run install.cmd.

To leave test mode later: bcdedit /set testsigning off, and restart.


Remove
------
Double-click uninstall.cmd. It removes the cables and the driver.
"uninstall.cmd -RemoveCertificate" also removes the test certificate.


Licence
-------
MIT (LICENSE.txt). Parts of the driver come from Microsoft's
SimpleAudioSample (Windows-driver-samples) under the Microsoft Public
License (LICENSE-MS-PL.txt). Source code:
https://github.com/LudaLunizz/BananaSplit/tree/main/splitter
