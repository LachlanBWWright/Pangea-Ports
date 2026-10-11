# Script Game Matrix

All eight Android Gradle builds explicitly set `PANGEA_ENABLE_SCRIPTING=ON`.
Desktop builds continue to control scripting through that CMake option.
The table records adapter hooks checked against native dispatch sites; runtime
capabilities and individual command results still govern object/player mutations.

| Game | Lifecycle and item dispatch | Gameplay dispatch highlights |
|------|-----------------------------|------------------------------|
| Bugdom 2 | Level lifecycle, terrain/spline items, object frames, save/load | Pickup, trigger, weapon-hit, damage, spawn/respawn, checkpoint, objective |
| Nanosaur | Level lifecycle, terrain items, object frames | Pickup, trigger, weapon-hit, damage, spawn/respawn |
| Bugdom | Level lifecycle, terrain/spline items, object frames, save/load | Pickup, trigger, weapon-hit, damage, spawn/respawn, checkpoint, objective |
| Otto Matic | Level lifecycle, terrain/spline items, object frames, save/load | Pickup, trigger, weapon-hit, damage, spawn/respawn, checkpoint |
| Billy Frontier | Area lifecycle, terrain/spline items, object frames, save/load | Pickup, trigger, weapon-hit, damage, spawn |
| Nanosaur 2 | Local level lifecycle, terrain/spline items, object frames, save/load | Pickup, trigger, weapon-hit, damage, spawn/respawn, checkpoint, lap, race finish, objective |
| Mighty Mike | 2D area lifecycle, map items, object frames, save/load | Pickup, trigger, weapon-hit, damage, spawn/respawn |
| Cro-Mag Rally | Local race lifecycle, terrain/spline items, object frames | Pickup, trigger, weapon-hit, damage, spawn, checkpoint, lap, race finish |

The exact supported hook lists are in
`shared/script/pangea_script_contract.h`, exposed at runtime through
`pangea.game.supportedHooks`. The gameplay column highlights dispatch and is not
an exhaustive list of every event (for example, damage-applied and death hooks).
Networked Cro-Mag Rally and Nanosaur 2 reject simulation-affecting scripts.
