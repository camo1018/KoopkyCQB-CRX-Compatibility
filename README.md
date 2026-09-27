# Koopky CQB CRX Compatibility

Keeps a Clear or Garrison order in charge of movement when CRX Enfusion AI is also loaded. Requires Koopky CQB and CRX Enfusion AI.

## What it changes

While one of those orders is running:

- Return To Position is set to Never
- Investigate and building search are turned off
- Each soldier's danger-reaction chance is set to 0, so a projectile hit does not send him off the order

Combat move, combat cover, and in-cover cover search stay at the group's CRX values. They are not turned off for the squad. A soldier who is holding a post does not take those CRX moves himself: no new cover, no cover-to-cover, and no "this cover is useless" relocation. A shot, a hit, or an explosion does not send him off the post either. CRX also stops raising his move order up to attack priority and sprinting him. The rest of the group keeps normal CRX combat movement.

Recognition keeps CRX's rates during the order. Koopky's recognition-speed slider multiplies those rates. At 1, soldiers spot the way CRX is configured.

CRX does not copy its group config file, or a global settings override, back onto that group during the order. The saved settings are written back when the order ends. A Clear that hands off to Garrison stays paused the whole time.

Soldiers still aim and fire. Koopky CQB still owns the route, the posts, and the doors.

## Setting

On the game mode entity, under **Koopky CQB CRX**:

**While a Clear or Garrison order is running, pause the CRX settings that pull soldiers off that order.**

On by default. Turn it off to leave CRX's settings alone for the whole mission.
