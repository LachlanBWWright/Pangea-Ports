# Script Game Matrix

| Game | Status | Initial scope |
|------|--------|---------------|
| Bugdom 2 | First adapter wired behind `PANGEA_ENABLE_SCRIPTING` | lifecycle hooks, terrain item hook, item remap config |
| Nanosaur | Adapter wired behind `PANGEA_ENABLE_SCRIPTING` | lifecycle hooks, terrain item hooks, simple pickups/traps |
| Bugdom | Adapter wired behind `PANGEA_ENABLE_SCRIPTING` | lifecycle hooks, terrain/spline hooks, simple triggers |
| Otto Matic | Adapter wired behind `PANGEA_ENABLE_SCRIPTING` | lifecycle hooks, terrain/spline hooks, simple powerups/triggers |
| Billy Frontier | Adapter wired behind `PANGEA_ENABLE_SCRIPTING` | area lifecycle hooks, terrain/spline hooks |
| Nanosaur 2 | Adventure adapter wired behind `PANGEA_ENABLE_SCRIPTING` | local adventure lifecycle hooks, terrain/spline hooks; network scripting disabled |
| Mighty Mike | Adapter wired behind `PANGEA_ENABLE_SCRIPTING` | 2D area lifecycle hooks, map item hooks |
| Cro-Mag Rally | Local race adapter wired behind `PANGEA_ENABLE_SCRIPTING` | race lifecycle hooks, terrain item hooks with player context; network scripting disabled |

Networked Cro-Mag Rally and Nanosaur 2 must reject simulation-affecting scripts
until script/config hashing and desync reporting are implemented.
