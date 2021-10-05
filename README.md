# HZD_PC_Gamer_Camera
I just couldn't play with the camera drifting around, so I fixed it.  
Mouse mostly back in full control of the camera direction :)  
There is still a small flash-drift when going from running to the right to aiming (doesn't happen when running to the left).  
I didn't notice this small bug, probably comes from the aiming camera override using speed data to derive the panning offset instead of reading the panning offset.. as in "logic", duh.

## how-to
Either Compile the main.cpp or download the executable.
Run the game, then the exe, and enjoy the relief of having a stable camera.


## Works for the 2-Feb-21 version 
It's not meant to survive next game updates as I'm not expecting any soon.  
If you happen to come on this page after one, just open an issue and i'll update it.

## CameraOrbitFollow
The reason for all the troubles is that their camera is not a standard over the shoulder one, it is always straight behind the player, but pans.  
It means they also rotate character's forward direction.  
When you look at the problem of how to now center the character, then it's all about rotating the real forward direction at the same time as the camera offset.. but obviously they didn't manage doing that properly with the different acceleration curves, camera instance swaps, overrides (aiming + focus are overrides).  
My fix is to completly disable the camera auto-panning target and set it to zero, transitions remain smooth between overrides and normal cameras.


