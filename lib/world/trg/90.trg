#9000
Cow Animation~
0 bw 1
~
* This script is no longer used. It was replaced by custom strings.
* The cow will do a couple different things, so we do an if on a random generator.
if (%random.2% == 2)
  %echo% %self.name% moos contentedly.
else
  * We need the current terrain.
  eval room %self.room%
  if (%room.sector% == Plains)
    %echo% %self.name% eats some grass.
  else
    %echo% %self.name% chews %self.hisher% cud.
  end
end
~
#9001
Wolf Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Wolf Animation (9001-9003)
* Time of day is important, can't have wolves howling at the sun...
if ((%time.hour% < 7) && (%time.hour% > 19))
  * Can't have wolves howling at the rain clouds either...
  if (%weather% == clear)
    %echo% self.name points %self.hisher% muzzle toward the sky and howls.
  end
else
  if (%self.name% == a snarling wolf)
    %echo% %self.name% looks directly at you, opens %self.hisher% mouth to show %self.hisher% fangs, and growls deep in %self.hisher% chest as %self.hisher% hackles stand on end...
  else
    switch (%random.4%)
      case 1
        * scratch
        %echo% %self.name% scratches %self.hisher% ear with a hind foot.
      break
      case 2
        * roll
        %echo% %self.name% rolls around, all four paws in the air, making odd snorting sounds.
      break
      case 3
        * Yawn
        %echo% %self.name% yawns, showing off an impressive set of gleaming white fangs.
      break
      default
        %echo% %self.name% pants, with %self.hisher% tongue hanging out and tail gently wagging.
      break
    done
  end
end
~
#9002
Herd Cats~
0 c 0
herd~
* test targeting me
eval test %%actor.char_target(%arg.car%)%%
if %test% != %self%
  return 0
  halt
end
%send% %actor% Herding cats is not as easy as it looks; you fail.
%echoaround% %actor% %actor.name% tries and fails to herd %self.name%.
return 1
~
#9003
Daily Quest Item Handout~
2 u 100
~
if %questvnum% == 9009
  %load% obj 9010 %actor% inv
  eval item %actor.inventory(9010)%
  if %item%
    %send% %actor% The stablemaster gives you %item.shortdesc%.
  end
elseif %questvnum% == 9033
  %load% obj 9034 %actor% inv
  eval item %actor.inventory(9034)%
  if %item%
    %send% %actor% The guildmaster gives you %item.shortdesc%.
  end
elseif %questvnum% == 9030
  %load% obj 9031 %actor% inv
  eval item %actor.inventory(9031)%
  if %item%
    %send% %actor% The barkeep gives you %item.shortdesc%.
  end
elseif %questvnum% == 9036
  if %actor.varexists(last_quest_9036_time)%
    * 15 minute cooldown
    if %timestamp% - %actor.last_quest_9036_time% < 900
      eval diff (%actor.last_quest_9036_time% - %timestamp%) + 900
      eval diff2 %diff%/60
      eval diff %diff%//60
      if %diff%<10
        set diff 0%diff%
      end
      %send% %actor% You must wait %diff2%:%diff% to start this quest again.
      return 0
      halt
    end
  end
  eval last_quest_9036_time %timestamp%
  remote last_quest_9036_time %actor.id%
  nop %actor.add_resources(9036, 5)%
  %send% %actor% The High Sorcerer gives you five enchanted trinkets.
end
~
#9004
Sheep Animation~
0 bw 1
~
* This script is no longer used. It was replaced by custom strings.
* The sheep will do a couple different things, so we do an if on a random generator.
if (%random.2% == 2)
  %echo% %self.name% baas contentedly.
else
  * We need the current terrain.
  eval room %self.room%
  if (%room.sector% == Plains)
    %echo% %self.name% eats some grass.
  else
    %echo% %self.name% chews %self.hisher% cud.
  end
end
~
#9008
Squirrel Animation~
0 bw 4
~
* This script is no longer used. It was replaced by custom strings.
* Get current terrain.
eval room %self.room%
* If we are in forest, bombard players with acorns!
if (%room.sector% ~= Forest)
  %echo% %self.name% bombards you with acorns!
end
%echo% %self.name% chatters angrily at you for invading %self.hisher% territory!
~
#9009
Cow Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* The cow will do a couple different things, so we do an if on a random generator.
if (%random.2% == 2)
  %echo% %self.name% moos contentedly.
else
  * We need the current terrain.
  eval room %self.room%
  if (%room.sector% == Plains)
    %echo% %self.name% eats some grass.
  else
    %echo% %self.name% chews %self.hisher% cud.
  end
end
~
#9010
Chicken Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Chicken Animation (9010)
switch (%random.4%)
  case 1
    * Hunt for food.
    %echo% %self.name% searches around on the ground, looking for bugs.
  break
  case 2
    * Catch and eat.
    %echo% %self.name% deftly catches a bug in its beak and gobbles it down.
  break
  case 3
    * Acts like a chicken.
    %echo% %self.name% clucks in panic as %self.heshe% desperately flaps %self.hisher% wings in an attempt to escape a threat that only %self.heshe% perceives!
  break
  default
    * Bock bock...
    %echo% %self.name% clucks with contentment as %self.heshe% preens %self.himher%self.
  break
done
~
#9011
Rooster Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Rooster Animation (9011)
switch (%random.4%)
  case 1
    * Hunt for food.
    %echo% %self.name% searches around on the ground, looking for bugs.
  break
  case 2
    * Catch and eat.
    %echo% %self.name% deftly catches a bug in its beak and gobbles it down.
  break
  case 3
    * Acts like a rooster.
    %echo% %self.name% flaps his wings as hard as he can to get as much height as possible and crows at the top of his lungs!
  break
  default
    * Bock bock...
    %echo% %self.name% clucks with contentment as %self.heshe% prunes %self.hisher% feathers.
  break
done
~
#9012
Dog Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Dog Animation (9012)
switch (%random.8%)
  case 1
    * scratch
    %echo% %self.name% scratches %self.hisher% ear with a hind foot.
  break
  case 2
    * roll
    %echo% %self.name% rolls around, all four paws in the air, making odd snorting sounds.
  break
  case 3
    * Yawn
    %echo% %self.name% yawns, showing off an impressive set of gleaming white fangs.
  break
  case 4
    * sniff
    %echo% %self.name% sniffs at the ground.
  break
  case 5
    * Beg
    eval room %self.room%
    eval target %room.people%
    while (%target%)
      eval obj %target.inventory()%
      while (%obj%)
        if (%obj.type% == FOOD)
          %send% %target% %self.name% looks at you with pleading in %self.hisher% eyes as though saying, "please, just one bite of %obj.shortdesc%, I haven't eaten in months."
          %echoaround% %target% %self.name% stares at %target.name%, %self.hisher% eyes tracking every move of %obj.shortdesc% as though %self.heshe% were hypnotized.
          halt
        end
        eval obj %obj.next_in_list%
      done
      eval target %target.next_in_room%
    done
  break
  case 6
    * jump
    %echo% %self.name% jumps up on you, getting mud all over you!
  break
  case 7
    if (%weather% != clear)
      %echo% %self.name% shakes vigorously showering you with doggy scented drops of water.
    end
  break
  default
    %echo% %self.name% pants, with %self.hisher% tongue hanging out and tail gently wagging.
  break
done
~
#9013
Prairie Dog Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Prairie Dog Animation (9013)
switch (%random.6%)
  case 1
    * Vanishing act.
    %echo% %self.name% disappears down its hole.
    %purge% %self%
  break
  case 2
    %echo% %self.name% peeks out of %self.hisher% hole.
  break
  default
    %echo% %self.name stands on %self.hisher% hind legs and looks around.
  break
done
~
#9018
Vulture Animation~
0 bw 1
~
* This script is no longer used. It was replaced by custom strings.
* Vulture Animation (9018)
%echo% %self.name% circles high overhead, patiently waiting...
~
#9020
Quail Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Quail Animation (9020)
if (%random.2% == 1)
  %echo% %self.name% runs around on the ground looking for food.
else
  if (%random.2% == 1)
    %echo% %self.name% calls, 'Bob, White'.
  else
    %echo% %self.name% calls, 'Bob, Bob, White'.
  end
end
~
#9021
Donkey Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Donkey Animation (9021)
%echo% %self.name% brays loudly.
~
#9022
Black Cat Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Black Cat Animation (9022)
switch (%random.3%)
  case 1
    %echo% %self.name% leaps from the shadows and dashes across your path!
  break
  case 2
    eval room %self.room%
    if ((%room.sector% == Plains) || (%room.sector% /= Garden))
      %echo% %self.name% spots a butterfly and immediately gives chase!
    else
      %echo% %self.name% meows.
    end
  break
  default
    eval target %random.char%
    if (%target.is_pc%)
      %send% %target% %self.name% rubs against your legs and purrs.
      %echoaround% %target% %self.name% walks in circles around %target.name%, rubbing against %target.hisher% legs and purring.
    else
      %echo% %self.name% hisses at %target.name% and growls.
    end
  break
done
~
#9025
Eagle Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Eagle Animation (9025)
eval room %self.room%
if (%room.sector% == River)
  %echo% %self.name% dives into the water, then emerges with a fish.
  halt
end
switch (%random.3)
  case 1
    %echo% %self.name% suddenly dives, then takes off again, a rabbit in %self.hisher% talons.
  break
  case 2
    %echo% %self.name% suddenly dives, then takes off again, a mouse in %self.hisher% talons.
  break
  default
    %echo% %self.name% suddenly dives, then takes off again, a squirrel in %self.hisher% talons.
  break
done
~
#9030
Butcher detect~
1 c 3
butcher~
eval target %%actor.obj_target(%arg%)%%
if !%target%
  * Invalid target
  return 0
  halt
end
if !%actor.ability(butcher)%
  * Player does not have butcher
  return 0
  halt
end
* Debug...
if %test% != *CORPSE && %test%
  return 0
  halt
end
if %target.val0%
  eval mob_vnum %target.val0%
else
  * Probably not really a corpse
  return 0
  halt
end
* Check mob vnum
if %mob_vnum% == 9017 || %mob_vnum% == 9177
  * Bear, polar bear
elseif %mob_vnum% == 9001 || %mob_vnum% == 9002
  * Snarling and brown wolf
elseif %mob_vnum% == 9016 || %mob_vnum% == 9100 || %mob_vnum% == 9143 || %mob_vnum% == 9118 || %mob.vnum% == 9102
  * Tiger, leopard, snow leopard, cougar, jaguar
else
  * Wrong kind of corpse
  return 0
  halt
end
if %actor.inventory(9030)% || !%actor.on_quest(9030)%
  * Don't need the trinket
  return 0
  halt
end
%send% %actor% You cut the head off %target.shortdesc%...
%load% obj 9030 %actor% inv
eval item %actor.inventory()%
if %item%
  %send% %actor% You get %item.shortdesc%!
end
* Don't butcher normally this time. Less buggy this way.
return 1
halt
~
#9033
Fake pickpocket~
1 c 2
pickpocket~
eval target %%actor.char_target(%arg%)%%
if !%target%
  * Invalid target
  return 0
  halt
end
if !%actor.ability(pickpocket)%
  * Player does not have pickpocket
  return 0
  halt
end
if %target.empire% != %actor.empire%
  * Wrong empire
  return 0
  halt
end
if (%target.vnum% != 202 && %target.vnum% != 203)
  * Not an empire citizen
  return 0
  halt
end
if %target.mob_flagged(*PICKPOCKETED)%
  return 0
  halt
end
if %actor.inventory(9033)% || !%actor.on_quest(9033)%
  * Don't need the trinket
  return 0
  halt
end
if %random.3% != 3
  %send% %actor% This must have been the wrong citizen.
  return 0
  halt
else
  %send% %actor% You pick %target.name%'s pocket...
  nop %target.add_mob_flag(*PICKPOCKETED)%
  %load% obj 9033 %actor% inv
  eval item %actor.inventory()%
  %send% %actor% You find %item.shortdesc%!
  return 1
  halt
end
~
#9034
Horse Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Horse Animation (9034)
switch (%random.3%)
  case 1
    %echo% %self.name% nays loudly.
  break
  case 2
    %echo% %self.name% stomps %self.hisher% hoof.
  break
  default
    %echo% %self.name%'s ears prick forward as though listening for danger...
  break
done
~
#9036
Disenchant detector~
1 p 100
~
if %abilityname% != Disenchant
  return 1
  halt
end
eval done 1
eval obj %actor.inventory()%
while %obj%
  if %obj.vnum% == 9036
    if %obj.is_flagged(ENCHANTED)% && %obj% != %self%
      eval done 0
    end
  end
  eval obj %obj.next_in_list%
done
if %done%
  %quest% %actor% trigger 9036
end
return 1
~
#9042
Postmaster quest start~
2 u 100
~
eval vnum 9041+%random.3%
%load% obj %vnum% %actor% inv
eval item %%actor.inventory(%vnum%)%%
%send% %actor% You receive %item.shortdesc%.
~
#9043
Postmaster daily letter delivery~
1 i 100
~
eval recipient 0
switch %self.vnum%
  case 9042
    * Smith
    eval recipient 212
  break
  case 9043
    * High Sorcerer
    eval recipient 228
  break
  case 9044
    * Alchemist
    eval recipient 231
  break
done
eval person %victim%
eval found (%person.vnum% == %recipient% && %person.empire% == %actor.empire%)
eval wrong_empire %person.vnum% == %recipient% && %person.empire% != %actor.empire%
if %found%
  %send% %actor% You give the letter to %person.name%.
  %quest% %actor% trigger 9042
  return 1
  %purge% %self%
elseif %wrong_empire%
  %send% %actor% %person.name% does not belong to your empire.
  return 0
  halt
else
  %send% %actor% That's not who you need to deliver the letter to.
  return 0
  halt
end
~
#9064
Wandering Vampire combat~
0 k 25
~
if !%self.vampire()%
  halt
end
if %time.hour%>=7 && %time.hour%<=19
  halt
end
if %actor.aff_flagged(!DRINK-BLOOD)%
  * Don't bite !DRINK-BLOOD targets
  halt
end
%send% %actor% %self.name% lunges forward and sinks %self.hisher% teeth into your shoulder!
%echoaround% %actor% %self.name% lunges forward and sinks %self.hisher% teeth into %actor.name%'s shoulder!
eval healthprct %actor.health% * 100 / %actor.maxhealth%
eval can_turn 1
if %healthprct% > 50 || %actor.aff_flagged(!VAMPIRE)% || %actor.vampire()%
  * Too much health left, or immune to vampirism
  eval can_turn 0
end
if %actor.is_pc%
  if %actor.nohassle%
    * PC is immune to vampirism (rare)
    eval can_turn 0
  end
else
  if !%actor.mob_flagged(HUMAN)% || %actor.mob_flagged(GROUP)% || %actor.mob_flagged(HARD)%
    * NPC is immune to vampirism (at least from this)
    eval can_turn 0
  end
end
if %can_turn%
  * Strings copied from sire_char
  %send% %actor% You fall limply to the ground. In the distance, you think you see a light...
  %echoaround% %actor% %actor.name% drops limply from %self.name%'s fangs...
  %echoaround% %actor% %self.name% tears open %self.hisher% wrist with %self.hisher% teeth and drips blood into %actor.name%'s mouth!
  %send% %actor% &rSuddenly, a warm sensation touches your lips and a stream of blood flows down your throat...&0
  %send% %actor% &rAs the blood fills you, a strange sensation covers your body... The light in the distance turns blood-red and a hunger builds within you!&0
  nop %actor.vampire(on)%
  if %actor.is_npc%
    attach 9064 %actor.id%
  end
  dg_affect %self% STUNNED on 5
else
  %damage% %actor% 25
end
~
$
