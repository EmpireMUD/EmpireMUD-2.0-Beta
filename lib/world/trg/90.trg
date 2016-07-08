#9000
Cow Animation~
0 b 1
~
Cow Animation
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
0 b 3
~
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
#9004
Sheep Animation~
0 b 1
~
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
0 b 4
~
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
0 b 3
~
Cow Animation
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
0 b 3
~
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
0 b 3
~
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
0 b 3
~
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
0 b 3
~
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
0 b 1
~
* Vulture Animation (9018)
%echo% %self.name% circles high overhead, patiently waiting...
~
#9020
Quail Animation~
0 b 3
~
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
0 b 3
~
* Donkey Animation (9021)
%echo% %self.name% brays loudly.
~
#9022
Black Cat Animation~
0 b 0
~
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
0 b 3
~
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
#9034
Horse Animation~
0 b 3
~
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
