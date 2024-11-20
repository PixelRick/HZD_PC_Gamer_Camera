# HZD_PC_Gamer_Camera
You'd prefer Aloy to remain centered ? It's your lucky day.
Here is a simple runtime patch for Horizon Zero Dawn (legacy and remastered) which disables the camera function responsible for the cinematic panning.

## How-To
Either Compile the main.cpp or download the executable.
Run the game, then the exe, and enjoy the relief of having a stable camera.

## Works for the 2-Feb-21 legacy version and 20-Nov-24 remastered version 
It's not really meant to survive next game updates (as I'm not expecting any soon) but it could.
If you happen to come on this page after one and the program errors, just open an issue and i'll update it.

## Forbidden West
WIP (I didn't buy the game yet and it proves difficult to work on a fix without it).

## CameraOrbitFollow
The reason for all the troubles is that their camera is not a standard over the shoulder one, it is always straight behind the player, but pans.  
It means they also rotate character's forward direction, and it becomes a mess when you want to aim.

