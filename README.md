# Koopky CQB CRX Compatibility

Keeps a Clear or Garrison order in charge of movement when CRX Enfusion AI is also loaded. Requires Koopky CQB and CRX Enfusion AI.

## What it changes

While one of those orders is running, the group's CRX movement settings are paused:

- Return To Position is set to Never
- Investigate and building search are turned off
- Combat move, combat cover, and in-cover cover search are set to 0
- Each soldier's danger-reaction chance is set to 0, so a projectile hit does not send him off the order

CRX does not copy its group config file, or a global settings override, back onto that group during the order. The saved settings are written back when the order ends. A Clear that hands off to Garrison stays paused the whole time.

Soldiers still aim and fire. Koopky CQB still owns the route, the posts, and the doors.

## Setting

On the game mode entity, under **Koopky CQB CRX**:

**While a Clear or Garrison order is running, pause the CRX settings that pull soldiers off that order.**

On by default. Turn it off to leave CRX's settings alone for the whole mission.
