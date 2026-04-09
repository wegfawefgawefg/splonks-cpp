# Dev Readme

## Entity Logic

- There are functions for hurting an entity, healing an entity etc. These have to be used rather than just setting an entities hp.
  The entitiy might have some special logic that needs to be run when it is hurt or healed. (There might be cases where you want to mow this over.)
- Never make game logic depend on EntityRenderState.
- deadness is used on undieable entities to signal that they should blow up in some way and delete themself (like rock or jetpack, or bomb).
- block crushing doesnt work on things grounded under 5, 5 in size. (This may no longer be true.)

- entity_just_collided is a semi dangerous function that can be really annoying to the player if overused
  there shouldnt be any issues using it on entities that are supposed to explode on contact with something.
- entities should not set themselves to super state dead, just hurt the entity, and let the death checker handle it.

## Inputs

- Inputs are stored in structs in state.
- For a given mode, the inputs are updated just before the input processing functions are called, which is just before the state is stepped.
- That means they are guaranteed to be up to date for the current frame.
- Raylib debounce is not sufficient.
- Debounce is implemented for the directional inputs in the menu controls to give repeat on directional holding, but not for button presses.
  - This means that the player can press a button and hold it, and the game will only register the press once.
  - However the directions can be held and will repeat, hence the manual debounce.

## Systems

- Try to keep systems generic. This wont always be avoidable, but the systems are itterating through all the entities,
  so if you can make a generic system you reduce the full num_system \* num_entities steps.

## QUIRKS

if a swept contact behavior should only happen once per approach, it now needs to say so explicitly with cooldown
or stop_sweep. thats bc to make pusher/crusher work during sweep we disabled the sweep contact same frame dedupe logic.
basically use cooldowns on non blocking contacts.
