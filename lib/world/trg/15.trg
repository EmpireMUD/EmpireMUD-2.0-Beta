#1502
Silent Ascension: Detect pickpocket~
1 c 2
pickpocket~
* basic checks first
return 0
set target %actor.char_target(%arg%)%
if !%target%
  halt
elseif !%actor.on_quest(1502)% || !%actor.ability(Pickpocket)%
  halt
elseif %target.empire% == %actor.empire%
  * must be different empire
  halt
elseif %target.mob_flagged(*PICKPOCKETED)% || !%target.mob_flagged(HUMAN)%
  halt
end
* ok to proceed
return 1
set person %self.room.people%
while %person%
  if %person.mob_flagged(CITYGUARD)%
    %force% %person% say Stop, %actor.name%! You aren't going to pickpocket anything on my watch!
    halt
  end
  set person %person.next_in_room%
done
* going to attempt to detect the true result of pickpocket
set inv_start %actor.inventory%
set coins %actor.coins%
* let them try it...
return 0
wait 0
* did we actually pickpocket something?
if %actor.coins% > %coins% || %actor.inventory% != %inv_start%
  * got one!
  %quest% %actor% trigger 1502
end
~
#1506
Archetype unlock quests: Give quest items~
2 u 0
~
switch %questvnum%
  case 1506
    %load% obj 1001 %actor% inv
  break
  case 1502
    %load% obj 1011 %actor% inv
  break
done
~
$
