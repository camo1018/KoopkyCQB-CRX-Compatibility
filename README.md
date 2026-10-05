# Koopky CQB CRX Compatibility

Keeps a Clear, Garrison, or Take cover order in charge of movement when CRX Enfusion AI is also loaded. Requires Koopky CQB and CRX Enfusion AI.

## What it changes

While one of those orders is running:

- Return To Position is set to Never
- Investigate and building search are turned off
- Each soldier's danger-reaction chance is set to 0, so a projectile hit does not send him off the order

Combat move, combat cover, and in-cover cover search stay at the group's CRX values during Clear and Garrison. They are not turned off for those squads. Take cover turns them off while that order still owns the route, including a walk back to the point, because CRX's cover search would leave the push. Once the squad is at the point and Take cover uses attack is on, those chances come back so normal attack can fight from there. Investigate and return-to-position stay off. A soldier who is holding a post does not take those CRX moves himself: no new cover, no cover-to-cover, and no "this cover is useless" relocation. A shot, a hit, or an explosion does not send him off the post either. The same reactions are ignored while Take cover still owns the route. CRX is not allowed to raise that move to attack priority, which would hand the push to an aimed strafe. The bounding pair keeps Koopky's sprint: rifle down, and no look at the enemy, so the run is not turned into a sidestep. A man who has stopped at his slot is pinned with the rifle up, and CRX is not allowed to aim that cancelled move into a sidestep. The jog still fires through Koopky's own shot. The move stays on Koopky's order instead of being cancelled. The rest of the group keeps normal CRX combat movement on Clear and Garrison.

While a soldier is on the way to a post, Koopky still steers that bound. A doorway burst stands and shoots. Inside the building, the nearest visible enemy is the shot, and the gun stays up while he moves or fires. On the sprint past a fight outside the building, threat, danger events, recognition, and look-at are dropped so he does not peel off. CRX calls the base game on those steps, so this addon applies Koopky's decision again after CRX.

With Sharp combat on, the base-game first-shot pause and CRX's attack-reaction delay are both set to 0 for a soldier on the order. A shot Koopky owns inside the building uses Koopky's shot delay instead.

Recognition otherwise keeps CRX's rates during the order. Koopky's recognition-speed slider multiplies those rates. At 1, soldiers spot the way CRX is configured.

CRX does not copy its group config file, or a global settings override, back onto that group during the order. The saved settings are written back when the order ends. A Clear that hands off to Garrison stays paused the whole time.

Soldiers still aim and fire. Koopky CQB still owns the route, the posts, and the doors.

## Setting

On the game mode entity, under **Koopky CQB CRX**:

**While a Clear, Garrison, or Take cover order is running, pause the CRX settings that pull soldiers off that order.**

On by default. Turn it off to leave CRX's settings alone for the whole mission.
