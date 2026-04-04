
## todoing


## todo

- when entities change they need to version up.
- make pots break on falling again, idk why that stopped.
- BugFix: switching from windowsize1920xdims1929 to windowed, makes the game not fit in the window.
- fall damage
- more enemies
- add fps option to video settings
- main menu backdrop looks fucked up at different resolutions
- working audio settings

- see if anything ends up in a weird nonexistent render state
- message queue renders on screen (on screen console)
- multiple damage preventer (dictionary in state for lookups on vids like this) (unique case for spikes. spikes cant multihit i think)

- display state router for more control on entity animations and rendering? (probably needs a whole system)
- check in on slicing for double mut in player code
- dog that wanders, gets mad if it sees you.
- some entity that can pick up and throw things
- bat makes sound on fall
- player makes better sound on fall
- block makes sound on fall
- entities and projectiles hit eachother multiple times a lot, its bad.
- merge groundedness check with collisions
- rock doesnt always make land sound for some reason
- narrow the stairs and rope climbing area
- rock floats when holder falls into spikes (not getting unheld id guess)
- rock sometimes throw backwards??
- figure out how moneys can look different without eating huge portions of the sprite atlas
- add falling anims, make the player not flinch on fall
- convert doors to be an entity
- bens item idea: iron false teeth. lets yu eat dead enemies one time for hp.
- implement picking up shit for non player entities.
- stage decorations of some sort... not in entity vector (standalone system)
- make a test room, atelast one. be able to spawn or kill enemies with mouse is nice.
- make a room for aligning frame offsets
- convert block tiles to be entity
- subdivide the exit templates because sometimes they block the path, or bore a hole into impassable vertically stacked rooms
- add all base room templates
- ensure distribution of rooms is kinda same
- look into wether turning the opposite direction is really required for equivalent map generation
- add conditional spider room and stuff
- items
- add more sub features for map gen
- performance metrics
- make bat.facing = player.facing
- redo bat logic. he sucks now. the ground wiggle sucks
- 3 stages
- stage transition
- boss

- entities can go into above tile on put, sometimes into floor
- player animating too fast
- hit manager? hit list to prevent double hits and multi hits, probably hit times and have them expire
- fix rendering entity and tile scaling at different resolutions.
- fix the title screen rendering for all resolutions
- abstract away inputs, audio, and rendering into shims for swapping the rendering libraries

## todont

- ecs

## tomaybe

- displaying icons for an entities state next to them would also be real nice

- cool entity state viewer??
