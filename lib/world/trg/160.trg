#16000
hidden necro gob~
2 g 100
~
wait 1
if (%actor.ability(Search)%)
  %send% %actor% As you enter the tunnel you see a goblin, who promptly takes off the other direction.
else
  %send% %actor% A disembodied scrabbling can be heard as someone, or something rushes away from your location.
end
%echo% You hear a goblin shout, "You'll never catch the great Necro Goblin!"
detach 16000 %self.id%
~
#16001
necro summon 1~
0 l 75
~
* No script
if %self.cooldown(16001)%
  halt
end
if %self.affect(BLIND)%
  %echo% %self.name%'s eyes shine extra bright, and %self.hisher% vision returns!
  dg_affect %self% BLIND off 1
end
say If you insist on this course of action... Then try this on for size!
set person %self.room.people%
while %person%
  if %person.is_pc%
    %load% mob 16001 ally
  end
  set person %person.next_in_room%
done
detach 16001 %self.id%
~
#16002
necro summon 2~
0 l 50
~
if %self.cooldown(16002)%
  halt
end
if %self.affect(BLIND)%
  %echo% %self.name%'s eyes shine extra bright, and %self.hisher% vision returns!
  dg_affect %self% BLIND off 1
end
say You handled the last round easy enough, but how about this experiment?
chuckle
set person %self.room.people%
while %person%
  if %person.is_pc%
    %load% mob 16002 ally
  end
  set person %person.next_in_room%
done
detach 16002 %self.id%
~
#16003
necro summon 3~
0 l 25
~
if %self.cooldown(16003)%
  halt
end
if %self.affect(BLIND)%
  %echo% %self.name%'s eyes shine extra bright, and %self.hisher% vision returns!
  dg_affect %self% BLIND off 1
end
say Fine then! Now you can meet, my rawhead!
%load% mob 16003 ally
detach 16003 %self.id%
~
#16004
nature combat~
0 k 30
~
if !%self.affect(3012)%
  eartharmor
elseif !%actor.aff_flagged(entangle)%
  entangle
else
  skybrand
end
~
#16005
bone dust usage~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if !%actor.aff_flagged(blind)%
  %echoaround% %actor% %actor.name% upends an entire bag of bone dust over %actor.hisher% head.
  %send% %actor% You dump a full bag of bone dust over your head and even as you lose your sight, you feel the protective spell take hold.
  dg_affect %actor% blind on 10
  dg_affect %actor% resist-magical 32 10
  dg_affect %actor% resist-physical 32 10
  %purge% %self%
else
  %send% %actor% You can't see the bag right now.
  halt
end
~
#16006
undead blocking~
0 s 100
~
%send% %actor% %self.name% won't let you pass!
return 0
~
#16007
skeletal combat~
0 k 30
~
if !%actor.affect(3048)%
  terrify
else
  kick
end
~
#16008
nature buff up~
0 k 25
~
if !%self.affect(3021)%
  counterspell
elseif !%self.affect(3023)%
  rejuvenate
else
  heal
end
~
#16009
transformative tooth~
1 c 2
implant~
if !%arg%
  %send% %actor% What would you like to implant with the tooth?
  return 1
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% Seems you were too slow. Better luck next time.
  return 1
  halt
end
if !%target.mob_flagged(mountable)%
  %send% %actor% You are unable to implant %self.shortdesc% into %target.name%.
  unset target
  halt
else
  %send% %actor% You stab %self.shortdesc% into %target.name%'s neck and watch the mutation begin.
  %echoaround% %actor% As %actor.name% stabs %self.shortdesc% into %target.name%'s neck, a horrific transformation takes place.
  set beast_chance %random.100%
  if %beast_chance% == (6)
    eval monster_level %actor.level% + 13
    wait 1
    %load% mob 16012 %monster_level%
    %echo% You get a terrible feeling that something has gone entirely wrong.
  elseif %target.is_name(dragon)%
    wait 1
    %echo% As the changes are completed, a monster is born.
    switch %random.2%
      case 1
        %load% mob 16014 %actor.level%
      break
      case 2
        %load% mob 16013 %actor.level%
      break
    done
  elseif %target.aff_flagged(fly)%
    wait 1
    %echo% As the changes are completed, a monster is born.
    switch %random.2%
      case 1
        %load% mob 16016 %actor.level%
      break
      case 2
        %load% mob 16017 %actor.level%
      break
    done
  elseif %target.mob_flagged(aquatic)%
    wait 1
    %echo% As the changes are completed, a monster is born.
    %load% mob 16015 %actor.level%
  elseif %target.is_name(horse)%
    wait 1
    %echo% As the changes are completed, a monster is born.
    switch %random.2%
      case 1
        %load% mob 16008 %actor.level%
      break
      case 2
        %load% mob 16009 %actor.level%
      break
    done
  else
    wait 1
    %echo% As the changes are completed, a monster is born.
    switch %random.2%
      case 1
        %load% mob 16010 %actor.level%
      break
      case 2
        %load% mob 16011 %actor.level%
      break
    done
  end
  %purge% %target%
  %purge% %self%
  unset beast_chance
  unset monster_level
  unset target
end
~
#16010
mutant mounts die~
0 f 100
~
%echo% As %self.name% dies, it crumbles and returned to the earth.
~
#16020
no drop me~
1 h 100
~
if %command% == drop || %command% == put
  return 0
  if %self.vnum% == 16021
    %send% %actor% The blood sticks to your hand and won't allow itself to be released.
  else
    %send% %actor% The vial is far too precious to be let go.
  end
end
~
#16021
entering the adventure~
1 c 4
enter~
if %actor.obj_target(%arg%)% == %self%
  if %actor.inventory(16021)%
    return 0
    wait 0.5
    %purge% %actor.inventory(16021)%
    %send% %actor% The disc of solidified blood melts as it touches the barier and allows you to pass.
  else
    %send% %actor% You bounce off of a magical barier. Seems there's a key of some sort needed to get through.
%echoaround% %actor% You watch %actor.name% flatten %actor.hisher% nose against a magical barier.
    return 1
  end
else
  return 0
end
~
#16022
drops blood disc~
0 f 100
~
if !%actor.inventory(16021)%
  %load% obj 16021 %actor% inv
  %send% %actor% As you deal the final blow, you find a disc of solidified blood and slip it into your pocket.
  halt
else
  %send% %actor% The solidified disc of blood you carry pulses in response to the death, but there is no change.
  halt
end
~
#16023
memorise and taunt~
0 s 50
~
if %actor.is_pc%
  Mremember %actor%
  %send% %actor% %self.firstname% the vampire tells you, 'You won't get far! Sooner or later, I will kill you!'
else
  halt
end
~
#16024
kill the remembered~
0 o 100
~
%send% %actor% The vampire growls at you and says, "I told you I'd catch you eventually. Now, you die!"
mkill %actor%
wait 1 sec
mforget %actor%
~
#16025
hunting memory~
0 s 90
~
if %actor.is_pc%
  Mremember %actor%
  %send% %actor% A vampire scout tells you, 'You won't get far! Sooner or later, I will kill you!'
  wait 2 sec
  mhunt %actor%
else
  halt
end
~
#16026
wandering vamps~
0 n 100
~
%echo% A vampire appears from the shadows and flashes out through the arch.
mgoto %instance.location%
mmove
mmove
mmove
mmove
mmove
mmove
detach 16026 %self.id%
~
#16027
purge blood vial~
1 s 100
~
wait 1
if %self.val1% == 0
  %echo% The bone vial crumbles to dust after the last drop of blood is drained from it.
  %purge% %self%
else
  halt
end
~
#16028
tripping in the cave~
2 g 60
~
wait 1
if %actor.is_flying%
  %echoaround% %actor% %actor.name% trips on the uneven ground and hits the dirt.
  %send% %actor% Your foot catches on something and you drop to the ground.
  wait 1
  dg_affect %actor% stunned on 2
end
~
#16029
illusion magic~
0 k 33
~
switch %random.5%
  case 1
    set dead_char %random.char%
    wait 1
    %send% %dead_char% A lightning bolt comes down from the roof and sends your rings flying.
    %echoaround% %dead_char% A lightningbolt strikes %dead_char.name% and blows their rings off their hands.
    wait 1
    unset dead_char
  break
  case 2
    %echo% All of the exits brick over as the vampire smirks.
    say This chamber will be your tomb.
  break
  case 3
    set dead_char %random.char%
    wait 1
    dg_affect %dead_char% stoned on 180
    %send% %dead_char% A flash from the illusionist's hand forces you to shut your eyes and when you open them again, the world doesn't quite look the same.
    %echoaround% %dead_char% A flash of light strikes %dead_char.name% with no visible affect.
    wait 1
    unset dead_char
  break
  case 4
    set dead_char %random.char%
    wait 1
    %send% %dead_char% A blade spins out of no where and carves a line across your throat.
    %echoaround% %dead_char% A blade comes flying through the air and opens %dead_char%'s throat.
    wait 3 sec
    %send% %dead_char% Blood sprays all down your front.
    %echoaround% %dead_char% Blood sprays all down %dead_char.hisher% front.
    wait 2 sec
    %echo% The mess vanishes, wound and all.
    wait 1
    unset dead_char
  break
  case 5
    %echo% Blood begins to fill the room as %self.name% floats up to the ceiling.
    say Drown! drown, in blood!
    wait 5 sec
    %echo% All of the blood vanishes and %self.name% is back in front of you, grinning.
  break
done
~
#16030
illusionist's death~
0 f 100
~
attach 16031 %self.room.id%
%echo% As %self.name% dies all of %self.hisher% illusions fade away.
%echo% A hole in the ground opens to the level below.
eval newroom %self.room.template% + 8
%door% %self.room% down room i%newroom%
%door% i%newroom% up room %self.room%
wait 1
unset newroom
~
#16031
remove the illusionist's exits~
2 f 100
~
%echo% The exit leading down to the second level fades as though it were never there.
eval newroom %room.template% + 8
%door% i%newroom% up purge
%door% i%room.template% down purge
wait 1
unset newroom
detach 16031 %room.id%
~
#16032
illusionist in random room~
0 n 100
~
eval random_room %random.8% + 16021
mgoto i%random_room%
wait 2
unset random_room
detach 16032 %self.id%
~
#16033
vampire blocking~
0 s 100
~
%send% %actor% %self.name% won't let you pass!
return 0
~
#16034
vampire alchemist combat~
0 k 0
~
* No script
~
#16035
tile password set~
2 c 0
test~
if %tile_row% == 5
%door% %self% north purge
end
if %tile_row% >> 1
  unset tile_row
end
switch %random.18%
  case 1
    * camp
    set tile_password c a m p
  break
  case 2
    * cane
    set tile_password c a n e
  break
  case 3
    * clad
    set tile_password c l a d
  break
  case 4
    * clan
    set tile_password c l a n
  break
  case 5
    * gain
    set tile_password g a i n
  break
  case 6
    * gene
    set tile_password g e n e
  break
  case 7
    * tame
    set tile_password t a m e
  break
  case 8
    * tamp
    set tile_password t a m p
  break
  case 9
    * tend
    set tile_password t e n d
  break
  case 10
    * tied
    set tile_password t i e d
  break
  case 11
    * time
    set tile_password t i m e
  break
  case 12
    * tine
    set tile_password t i n e
  break
  case 13
    * vamp
    set tile_password v a m p
  break
  case 14
    * vane
    set tile_password v a n e
  break
  case 15
    * vein
    set tile_password v e i n
  break
  case 16
    * vend
    set tile_password v e n d
  break
  case 17
    * vine
    set tile_password v i n e
  break
  case 18
    * vlad
    set tile_password v l a d
  break
done
set step_tile1 %tile_password.car%
set tile_password %tile_password.cdr%
set step_tile2 %tile_password.car%
set tile_password %tile_password.cdr%
set step_tile3 %tile_password.car%
set tile_password %tile_password.cdr%
set step_tile4 %tile_password.car%
unset tile_password
global step_tile1
global step_tile2
global step_tile3
global step_tile4
%echo% t1 %step_tile1%, t2 %step_tile2%, t3 %step_tile3%, t4 %step_tile4%.
~
#16036
stepping on tiles~
2 c 0
step~
if !%arg%
  %send% %actor% Which letter tile are you stepping on?
  return 1
  halt
end
set stepped_tile %arg%
if !%tile_row%
  set tile_row 1
  global tile_row
end
if %tile_row% == 1
  if %stepped_tile% == c || %stepped_tile% == g || %stepped_tile% == t || %stepped_tile% == v
    if %step_tile1% == %stepped_tile%
      %echo% The tile holds and the first row of tiles stops glowing.
      set tile_row 2
      global tile_row
      halt
    end
  else
    %send% %actor% There's no such tile in the first row, try again.
    return 1
    halt
  end
end
if %tile_row% == 2
  if %stepped_tile% == a || %stepped_tile% == e || %stepped_tile% == i || %stepped_tile% == l
    if %step_tile2% == %stepped_tile%
      %echo% The tile holds and the second row of tiles stops glowing.
      set tile_row 3
      global tile_row
      halt
    end
  else
    %send% %actor% There's no such tile in the second row, try again.
    return 1
    halt
  end
end
if %tile_row% == 3
  if %stepped_tile% == a || %stepped_tile% == i || %stepped_tile% == m || %stepped_tile% == n
    if %step_tile3% == %stepped_tile%
      %echo% The tile holds and the third row of tiles stops glowing.
      set tile_row 4
      global tile_row
      halt
    end
  else
    %send% %actor% There's no such tile in the third row, try again.
    return 1
    halt
  end
end
if %tile_row% == 4
  if %stepped_tile% == d || %stepped_tile% == e || %stepped_tile% == n || %stepped_tile% == p
    if %step_tile4% == %stepped_tile%
      %echo% The tile holds and the final row of tiles stops glowing.
      set tile_row 5
      global tile_row
      %door% %self% south room i16040
      wait 1
      %echo% As the glow fades the south wall parts.
      halt
    end
  else
    %send% %actor% There's no such tile in the fourth row, try again.
    return 1
    halt
  end
end
~
$
