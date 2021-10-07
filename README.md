# HZD_PC_Gamer_Camera
I just couldn't play with Horizon Zero Dawn's camera drifting around, so I fixed it.  
Mouse mostly back in full control of the camera direction :)  
There is still a drift when running to the right and then aiming depending on where you aim at (aim at your feet and you'll see).  
Because the aiming camera overrides does rotate around the target.. and somehow the forward direction gets changed.  
I'll try to find another patch point for this second weird behavior.  
..and to modify the camera pitch angle, currently aiming behind Aloy's head.

## how-to
Either Compile the main.cpp or download the executable.
Run the game, then the exe, and enjoy the relief of having a stable camera.


## Works for the 2-Feb-21 version 
It's not really meant to survive next game updates (as I'm not expecting any soon) but it could.
If you happen to come on this page after one and the program errors, just open an issue and i'll update it.

## CameraOrbitFollow
The reason for all the troubles is that their camera is not a standard over the shoulder one, it is always straight behind the player, but pans.  
It means they also rotate character's forward direction.  
When you look at the problem of how to now center the character, then it's all about rotating the real forward direction at the same time as the camera offset.. but obviously they didn't manage doing that properly with the different acceleration curves, camera instance swaps, overrides (aiming + focus are overrides).  
My fix is to completly disable the camera auto-panning target and set it to zero, transitions remain smooth between overrides and normal cameras.


