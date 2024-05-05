# HZD_PC_Gamer_Camera
I just couldn't play with Horizon Zero Dawn's camera drifting around, so I fixed it.  
Mouse now has full control of the camera direction (most of the time) :)  
There is still a drift when running to the right and then aiming at your feet.  
The different cameras do rotate around the target and somehow the forward direction gets changed when switched.
The camera pitch angle currently aims behind Aloy's head.

## Forbidden West
WIP

## how-to
Either Compile the main.cpp or download the executable.
Run the game, then the exe, and enjoy the relief of having a stable camera.

## Works for the 2-Feb-21 version 
It's not really meant to survive next game updates (as I'm not expecting any soon) but it could.
If you happen to come on this page after one and the program errors, just open an issue and i'll update it.

## CameraOrbitFollow
The reason for all the troubles is that their camera is not a standard over the shoulder one, it is always straight behind the player, but pans.  
It means they also rotate character's forward direction.  
When you look at the problem of how to center the character, it's about rotating the real forward direction at the same time as the camera offset.
But obviously they didn't manage doing that properly with the different acceleration curves, camera instance swaps, overrides (aiming + focus are overrides).  
My fix is to completly disable the camera auto-panning target and set it to zero, transitions remain smooth between overrides and normal cameras.


