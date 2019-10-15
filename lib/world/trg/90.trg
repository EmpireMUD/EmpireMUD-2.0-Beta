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
  if (%self.room.sector% == Plains)
    %echo% %self.name% eats some grass.
  else
    %echo% %self.name% chews %self.hisher% cud.
  end
end
~
#9001
Wolf Pack~
0 n 100
~
set num %random.2%
while %num% > 0
  eval num %num% - 1
  %load% mob 9003 ally
done
~
#9002
Herd Cats~
0 c 0
herd~
* test targeting me
if %actor.char_target(%arg.car%)% != %self%
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
  set item %actor.inventory(9010)%
  if %item%
    %send% %actor% The stablemaster gives you %item.shortdesc%.
  end
elseif %questvnum% == 9033
  %load% obj 9034 %actor% inv
  set item %actor.inventory(9034)%
  if %item%
    %send% %actor% The guildmaster gives you %item.shortdesc%.
  end
elseif %questvnum% == 9030
  %load% obj 9031 %actor% inv
  set item %actor.inventory(9031)%
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
  set last_quest_9036_time %timestamp%
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
  if (%self.room.sector% == Plains)
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
* If we are in forest, bombard players with acorns!
if (%self.room.sector% ~= Forest)
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
  if (%self.room.sector% == Plains)
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
    set target %self.room.people%
    while (%target%)
      set obj %target.inventory()%
      while (%obj%)
        if (%obj.type% == FOOD)
          %send% %target% %self.name% looks at you with pleading in %self.hisher% eyes as though saying, "please, just one bite of %obj.shortdesc%, I haven't eaten in months."
          %echoaround% %target% %self.name% stares at %target.name%, %self.hisher% eyes tracking every move of %obj.shortdesc% as though %self.heshe% were hypnotized.
          halt
        end
        set obj %obj.next_in_list%
      done
      set target %target.next_in_room%
    done
  break
  case 6
    * jump
    %echo% %self.name% jumps up on you, getting mud all over you!
  break
  case 7
    if (%weather% != clear)
      %echo% %self.name% shakes vigorously showering you with doggy-scented drops of water.
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
    if ((%self.room.sector% == Plains) || (%self.room.sector% /= Garden))
      %echo% %self.name% spots a butterfly and immediately gives chase!
    else
      %echo% %self.name% meows.
    end
  break
  default
    set target %random.char%
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
if (%self.room.sector% == River)
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
#9027
Feed to Tame: Fruit/Veg/Grain~
0 j 100
~
* Amount of tameness required
set target 5
if %actor.cooldown(9027)%
  %send% %actor% You can't feed wild animals again yet...
  return 0
  halt
end
nop %actor.set_cooldown(9027, 5)%
wait 3
if !%object.is_component(fruit)% && !%object.is_component(vegetable)% && !%object.is_component(grain)%
  %echo% %self.name% does not seem interested.
  drop all
  halt
end
* Block NPCs
if %actor.is_npc%
  halt
end
* Load tameness
if %self.varexists(tameness)%
  set tameness %self.tameness%
else
  set tameness 0
end
eval tameness %tameness% + 1
if %actor.charisma% > %random.10%
  eval tameness %tameness% + 1
end
remote tameness %self.id%
* Messaging
if %self.name% ~= horse
  set emotion and nickers
elseif %self.name% ~= elephant
  set emotion and trumpets happily
else
  set emotion and chews contentedly
end
%echo% %self.name% eats %object.shortdesc% %emotion%.
if %tameness% >= %target%
  %send% %actor% %self.heshe% really seems to like you.
  %echoaround% %actor% %self.heshe% really seems to like %actor.name%.
end
mjunk all
~
#9028
Tameness Required to Tame~
0 c 0
tame feed~
* Amount of tameness required
set target 5
* This script also overrides 'feed'
if %cmd% == feed
  if %actor.char_target(%arg.cdr%) == %self%
    %send% %actor% Just 'give' the food to %self.himher%.
    return 1
  else
    * ignore 'feed'
    return 0
  end
  halt
end
* Check target and tech
if (!%actor.has_tech(Tame)% || %actor.char_target(%arg%)% != %self%)
  return 0
  halt
end
* Skill checks / load tameness
if %actor.ability(Summon Animals)%
  set tameness %target%
  %send% %actor% You whistle at %self.name%...
  %echoaround% %actor% %actor.name% whistles at %self.name%...
elseif %self.varexists(tameness)%
  set tameness %self.tameness%
else
  set tameness 0
end
if %tameness% < %target%
  %send% %actor% You can't seem to get close enough to %self.name% to tame %self.himher%. Try feeding %self.himher% some fruit.
  return 1
  halt
else
  * Ok to tame
  return 0
  halt
end
~
#9030
Butcher detect~
1 c 3
butcher~
set target %actor.obj_target(%arg%)%
if !%target%
  * Invalid target
  return 0
  halt
end
* Debug...
if %test% != *CORPSE && %test%
  return 0
  halt
end
if %target.val0%
  set mob_vnum %target.val0%
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
set item %actor.inventory()%
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
set target %actor.char_target(%arg%)%
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
  set item %actor.inventory()%
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
set done 1
set obj %actor.inventory()%
while %obj%
  if %obj.vnum% == 9036
    if %obj.is_flagged(ENCHANTED)% && %obj% != %self%
      set done 0
    end
  end
  set obj %obj.next_in_list%
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
set item %actor.inventory(%vnum%)%
%send% %actor% You receive %item.shortdesc%.
~
#9043
Postmaster daily letter delivery~
1 i 100
~
switch %self.vnum%
  case 9042
    * Smith / Forgemaster
    if %victim.vnum% == 212 || %victim.vnum% == 278
      set found 1
    end
  break
  case 9043
    * High Sorcerer
    if %victim.vnum% == 228
      set found 1
    end
  break
  case 9044
    * Alchemist
    if %victim.vnum% == 231
      set found 1
    end
  break
done
eval wrong_empire %victim.vnum% == %recipient% && %victim.empire% != %actor.empire%
if %found% && !%wrong_empire%
  %send% %actor% You give the letter to %victim.name%.
  %quest% %actor% trigger 9042
  return 1
  %purge% %self%
elseif %found% && %wrong_empire%
  %send% %actor% %victim.name% does not belong to your empire.
  return 0
  halt
else
  %send% %actor% That's not who you need to deliver the letter to.
  return 0
  halt
end
~
#9046
Tame Sheep Evolution~
0 ab 100
~
%load% mob 9004
set sheep %self.room.people%
if %sheep.vnum% == 9004
  %echo% %self.name% is now %sheep.name%!
  %purge% %self%
end
~
#9061
Hypnotoad fight~
0 k 75
~
wait 1
if (!%actor% || %actor.room% != %self.room%)
  halt
end
dg_affect #9061 %actor% STUNNED on 15
%send% %actor% You lock eyes with %self.name% and black out for a moment...
%echoaround% %actor% %actor.name% locks eyes with %self.name% and appears dazed...
wait 1
flee
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
set can_turn 1
if %healthprct% > 50 || %actor.aff_flagged(!VAMPIRE)% || %actor.vampire()%
  * Too much health left, or immune to vampirism
  set can_turn 0
end
if %actor.is_pc%
  if %actor.nohassle%
    * PC is immune to vampirism (rare)
    set can_turn 0
  end
else
  if !%actor.mob_flagged(HUMAN)% || %actor.mob_flagged(GROUP)% || %actor.mob_flagged(HARD)%
    * NPC is immune to vampirism (at least from this)
    set can_turn 0
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
#9066
Nerf bat random debuffs~
0 bw 15
~
set effect %random.4%
switch %effect%
  case 1
    %echo% %self.name% brushes you with its wings. and you feel lethargic.
  break
  case 2
    %echo% %self.name% squeaks, and you feel your strength desert you.
  break
  case 3
    %echo% %self.name% scratches you with a claw, and you feel fragile.
  break
  case 4
    %echo% %self.name% bats at you with its wing, and you feel clumsy.
  break
done
set person %self.room.people%
while %person%
  if %person.is_pc%
    switch %effect%
      case 1
        dg_affect #9066 %person% SLOW on 120
      break
      case 2
        dg_affect #9066 %person% BONUS-PHYSICAL -10 120
        dg_affect #9066 %person% BONUS-MAGICAL -10 120
      break
      case 3
        dg_affect #9066 %person% RESIST-PHYSICAL -10 120
        dg_affect #9066 %person% RESIST-MAGICAL -10 120
      break
      case 4
        dg_affect #9066 %person% TO-HIT -25 120
        dg_affect #9066 %person% DODGE -25 120
      break
    done
  end
  set person %person.next_in_room%
done
~
$
