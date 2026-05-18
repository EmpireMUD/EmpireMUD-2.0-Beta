#16000
hidden necro gob~
2 g 100 1
L o 18
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
0 l 75 1
L b 16001
~
if %self.aff_flagged(BLIND)%
  %echo% |%self% eyes shine extra bright, and ^%self% vision returns!
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
0 l 50 1
L b 16002
~
if %self.aff_flagged(BLIND)%
  %echo% |%self% eyes shine extra bright, and ^%self% vision returns!
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
0 l 25 1
L b 16003
~
if %self.aff_flagged(BLIND)%
  %echo% |%self% eyes shine extra bright, and ^%self% vision returns!
  dg_affect %self% BLIND off 1
end
say Fine then! Now you can meet, my rawhead!
%load% mob 16003 ally
detach 16003 %self.id%
~
#16004
nature combat~
0 k 30 4
L o 15
L o 121
L o 125
L w 3012
~
if !%self.affect(3012)%
  eartharmor
elseif !%actor.aff_flagged(IMMOBILIZED)%
  entangle
else
  skybrand
end
~
#16005
bone dust usage~
1 c 2 0
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if !%actor.aff_flagged(blind)%
  %echoaround% %actor% ~%actor% upends an entire bag of bone dust over ^%actor% head.
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
0 s 100 0
~
if %actor.is_pc%
  if %actor.can_see(%self%)%
    %send% %actor% ~%self% won't let you pass!
  else
    mkill %actor%
  end
  return 0
end
~
#16007
skeletal combat~
0 k 30 3
L o 17
L o 96
L w 3048
~
if !%actor.affect(3048)%
  terrify
else
  kick
end
~
#16008
nature buff up~
0 k 25 5
L o 109
L o 114
L o 120
L w 3021
L w 3023
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
1 c 2 10
L b 16008
L b 16009
L b 16010
L b 16011
L b 16012
L b 16013
L b 16014
L b 16015
L b 16016
L b 16017
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
  %send% %actor% You are unable to implant @%self% into ~%target%.
  unset target
  halt
else
  %send% %actor% You stab @%self% into |%target% neck and watch the mutation begin.
  %echoaround% %actor% As ~%actor% stabs @%self% into |%target% neck, a horrific transformation takes place.
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
  elseif %target.aff_flagged(FLYING)%
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
0 f 100 0
~
%echo% As ~%self% dies, it crumbles and returned to the earth.
~
#16011
necrogoblin trigger reattach~
0 h 100 3
L f 16001
L f 16002
L f 16003
~
if %self.fighting%
  halt
end
if !%self.has_trigger(16001)%
  attach 16001 %self.id%
end
if !%self.has_trigger(16002)%
  attach 16002 %self.id%
end
if !%self.has_trigger(16003)%
  attach 16003 %self.id%
end
~
#16012
necrogoblin adventure removal~
0 f 100 0
~
%adventurecomplete%
~
#16020
no drop me~
1 h 100 1
L c 16021
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
1 c 4 2
L c 16021
L t 16039
enter~
if %actor.obj_target(%arg%)% == %self%
  if %actor.completed_quest_instance(16039)%
    %send% %actor% You have already fed the invasion. You have no more business here.
    halt
  end
  if %actor.inventory(16021)%
    return 0
    %purge% %actor.inventory(16021)%
    %send% %actor% The disc of solidified blood melts as it touches the barier and allows you to pass.
  else
    %send% %actor% You bounce off of a magical barier. Seems there's a key of some sort needed to get through.
    %echoaround% %actor% You watch ~%actor% flatten ^%actor% nose against a magical barier.
    return 1
  end
else
  return 0
end
~
#16022
drops blood disc~
0 f 100 1
L c 16021
~
if !%actor.inventory(16021)%
  %load% obj 16021 %actor% inv
  %send% %actor% As you deal the final blow, you find a disc of solidified blood and slip it into your pocket.
else
  %send% %actor% The solidified disc of blood you carry pulses in response to the death, but there is no change.
end
set control %instance.mob(16041)%
if %control.varexists(PlayersHunted)%
  set id %actor.id%
  set PlayersHunted %control.PlayersHunted%
  while %PlayersHunted%
    if %id% == %PlayersHunted.car%
      halt
    end
    set PlayersHunted %PlayersHunted.cdr%
  done
  set PlayersHunted %control.PlayersHunted%
end
set PlayersHunted %PlayersHunted% %id%
remote PlayersHunted %control.id%
~
#16023
city husking~
0 z 100 0
~
%echoaround% %actor% ~%self% drains the blood from ~%actor% and leaves a husk on the ground.
%heal% %self% health 10
if %actor.is_pc%
  set id %actor.id%
  %send% %actor% As ~%self% drains the last drop of blood from your body, &%self% drops the dried husk to the ground.
  if !%self.room.people(16042)%
    %load% mob 16042
  end
  set ControlId %self.room.people(16042)%
  remote id %ControlId%
end
if %actor.is_npc%
  wait 1
  if %actor.mob_flagged(no-corpse)%
    halt
  end
  %load% obj 16032 room
  set husk %self.room.contents(16032)%
  %echo% the husk variable targets @%husk%. raw %husk%.
  set corpse %self.room.contents(1000)%
  if %corpse% && %husk%
    while %corpse.contents%
      %echo% moving @%corpse.contents%
      %teleport% %corpse.contents% %husk%
    done
    %echo% finished
    %purge% %corpse%
  end
end
~
#16024
kill the remembered~
0 o 100 0
~
set person %self.room.people%
while %person%
  if %person.mob_flagged(cityguard)%
    %send% %actor% The vampire growls at you and says, "You won't always be protected. Eventually I'll catch you alone."
    mmove
    mmove
    mmove
    mmove
    halt
  end
  set person %person.next_in_room%
done
%send% %actor% The vampire growls at you and says, "I told you I'd catch you eventually. Now, you die!"
mkill %actor%
wait 1 sec
mforget %actor%
~
#16025
hunting memory~
0 s 100 0
~
%echo% Debug: it fired.
if !%actor.is_pc%
  halt
end
%echo% debug: it was a player, not a mob.
if %actor.level% < 160
  halt
end
%echo% debug: player's level was over 160.
set control %instance.mob(16041)%
if %control.varexists(PlayersHunted)%
  %echo% debug: should be checking the hunted players list now.
  set PlayerId %control.PlayersHunted%
  while %PlayerId%
    %echo% debug: player id is %actor.id% and listed id is %PlayerId.car%.
    if %PlayerId.car% == %actor.id%
      halt
    end
    set PlayerId %PlayerId.cdr%
  done
end
eval roll %random.10%
if %self.vnum% == 16022
  if %roll% < 10
    %send% %actor% A vampire scout tells you, 'You won't get far! Sooner or later, I will kill you!'
    Mremember %actor%
    wait 2 sec
    set room %self.room%
    if %room.is_outdoors% && %room.sun% == light
      halt
    else
      mhunt %actor%
    end
  end
else
  if %roll% < 6
    %send% %actor% %self.firstname% the vampire tells you, 'You won't get far! Sooner or later, I will kill you!'
    Mremember %actor%
  end
end
~
#16026
wandering vamps~
0 n 100 0
~
set vnum %self.vnum%
if %vnum% >= 16020 && %vnum% <= 16022
  %echo% ~%self% appears from the shadows and flashes out through the arch.
  mgoto %instance.location%
  eval move_count %random.5% * 3
  while %move_count%
    mmove
    eval move_count %move_count% - 1
  done
elseif %vnum% == 16025
  eval random_room %random.8% + 16021
  mgoto i%random_room%
end
~
#16027
purge blood vial~
1 s 100 0
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
2 g 70 0
~
if !%actor.is_flying% && %actor.is_pc%
  %echoaround% %actor% ~%actor% trips on the uneven ground and hits the dirt.
  %send% %actor% Your foot catches on something and you drop to the ground.
  dg_affect %actor% stunned on 6
end
~
#16029
illusion magic~
0 k 33 0
~
set dead_char %random.enemy%
set verify_target %dead_char.id%
switch %random.5%
  case 1
    %send% %dead_char% A lightning bolt comes down from the roof and sends your rings flying.
    %echoaround% %dead_char% A lightningbolt strikes ~%dead_char% and blows ^%dead_char% rings off ^%dead_char% hands!
  break
  case 2
    %echo% All of the exits brick over as the vampire smirks.
    say This chamber will be your tomb.
    wait 3 s
    %echo% ~%self% seems to lose concentration and the exits all return to normal.
  break
  case 3
    dg_affect %dead_char% stoned on 180
    %send% %dead_char% A flash from the illusionist's hand forces you to shut your eyes and when you open them again, the world doesn't quite look the same.
    %echoaround% %dead_char% A flash of light strikes ~%dead_char% with no visible affect.
  break
  case 4
    %send% %dead_char% A blade spins out of no where and carves a line across your throat.
    %echoaround% %dead_char% A blade comes flying through the air and opens |%dead_char% throat.
    wait 3 sec
    if %verify_target% != %actor.id%
      halt
    end
    %send% %dead_char% Blood sprays all down your front.
    %echoaround% %dead_char% Blood sprays all down ^%dead_char% front.
    wait 2 sec
    %echo% The mess vanishes, wound and all.
  break
  case 5
    %echo% Blood begins to fill the room as ~%self% floats up to the ceiling.
    say Drown! drown, in blood!
    wait 5 sec
    %echo% All of the blood vanishes and ~%self% is back in front of you, grinning.
  break
done
~
#16030
illusionist's death~
0 f 100 2
L f 16031
L y 16020
~
attach 16031 %self.room.id%
%echo% As ~%self% dies all of ^%self% illusions fade away.
%echo% A hole in the ground opens to the level below.
eval newroom %self.room.template% + 8
%door% %self.room% down room i%newroom%
%door% i%newroom% up room %self.room%
set person %self.room.people%
while %person%
  if %person.is_pc% && %person.empire%
    nop %person.empire.start_progress(16020)%
  end
  set person %person.next_in_room%
done
~
#16031
remove the illusionist's exits~
2 f 100 8
L j 16022
L j 16023
L j 16024
L j 16025
L j 16026
L j 16027
L j 16028
L j 16029
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
alchemist healing~
0 l 30 8
L j 16022
L j 16023
L j 16024
L j 16025
L j 16026
L j 16027
L j 16028
L j 16029
~
if %self.cooldown(16032)%
  halt
end
nop %self.set_cooldown(16032, 30)%
%echo% ~%self% grabs a large beaker of blood and chugs it down!
set chance %random.100%
%echo% %chance% now
if %chance% <= 50
  eval healing %random.11% * 5 + 45
  set healing health %healing%
elseif %chance% <= 85
  eval healing %random.15% * 5 + 75
  set healing health %healing%
elseif %chance% <= 98
  set healing debuffs
elseif %chance% <= 100
  set healing health 500
end
%heal% %self% %healing%
~
#16033
vampire blocking~
0 s 100 0
~
if %actor.is_npc%
  halt
end
set room_var %self.room%
eval move_dir %%room_var.%direction%(room)%%
if %self.vnum% == 16031
  if %actor.vampire%
    say Enslaved or not, I refuse to let your kind live. You won't be leaving this chamber alive %actor.name%!
    return 0
  else
    if !%move_dir% || %move_dir.template% < %room_var.template%
      say please %actor.name%, take me with you!
    else
      set check %actor.name%
      if %self.varexists(MayPass)%
        set MayPass %self.MayPass%
        while %MayPass%
          if %check% == %MayPass.car%
            say You've proven yourself %actor.name%, you may pass.
            return 1
            halt
          end
          set MayPass %MayPass.cdr%
        done
      elseif %self.varexists(failed)%
        set failed %self.failed%
        while %failed%
          if %check% == %failed.car%
            say You failed to beat me in a contest of the bow %actor.name%, you must kill me to pass.
            return 0
            halt
          end
          set failed %failed.cdr%
        done
      else
        say Perhaps if you beat me in an archery challenge I could let you pass %actor.name%.
        if %actor.ability(archery)%
          %send% %actor% (type: 'challenge archery')
        else
          %send% %actor% (You need the archery ability to challenge ~%self% to an archery contest.)
        end
        return 0
      end
    end
  end
else
  if %actor.vampire%
    if !%move_dir% || %move_dir.template% < %room_var.template%
      say By all means %actor.name%, go with my blessing.
    else
      say I'm sorry %actor.name%, but even being a fellow vampire, I may not let you pass.
      return 0
    end
  else
    say Good try %actor.name%, but you won't be making it out of this chamber alive.
    return 0
  end
end
~
#16034
vampire alchemist combat~
0 k 50 0
~
if %self.cooldown(16034)%
  halt
end
nop %self.set_cooldown(16034, 20)%
switch %random.2%
  case 1
    %echo% ~%self% begins spraying a mist of blood into the air!
    wait 2
    set person %self.room.people%
    while %person%
      if %person.vampire()%
        %send% %person% You begin to feel something is horribly wrong!
        %dot% #16036 %person% 100 30 poison 2
      end
      set person %person.next_in_room%
    done
  break
  case 2
    %echo% ~%self% smashes a vial on the ground and laughs as a gas cloud begins to spread!
    wait 2
    set person %self.room.people%
    while %person%
      if !%person.vampire()%
        %send% %person% You begin to feel something is horribly wrong!
        %dot% #16036 %person% 100 30 poison 2
      end
      set person %person.next_in_room%
    done
  break
done
~
#16035
tile password set~
2 f 100 0
~
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
global step_tile1
global step_tile2
global step_tile3
global step_tile4
~
#16036
stepping on tiles~
2 c 0 1
L j 16040
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
switch %tile_row%
  case 1
    if %stepped_tile% == c || %stepped_tile% == g || %stepped_tile% == t || %stepped_tile% == v
      %echo% The tention mounts as |%actor% foot descends toward the "%stepped_tile%" tile in row %tile_row%...
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
  break
  case 2
    if %stepped_tile% == a || %stepped_tile% == e || %stepped_tile% == i || %stepped_tile% == l
      %echo% The tention mounts as |%actor% foot descends toward the "%stepped_tile%" tile in row %tile_row%...
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
  break
  case 3
    if %stepped_tile% == a || %stepped_tile% == i || %stepped_tile% == m || %stepped_tile% == n
      %echo% The tention mounts as |%actor% foot descends toward the "%stepped_tile%" tile in row %tile_row%...
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
  break
  case 4
    if %stepped_tile% == d || %stepped_tile% == e || %stepped_tile% == n || %stepped_tile% == p
      %echo% The tention mounts as |%actor% foot descends toward the "%stepped_tile%" tile in row %tile_row%...
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
  break
  case 5
    %send% %actor% The tiles are no longer glowing, you can just walk across them.
    halt
  break
done
switch %random.4%
  case 1
    %send% %actor% As you step on the tile, a sudden shock rushes through your body!
    %echoaround% %actor% As ~%self% steps on a tile, &%actor% starts to convulse!
    eval shocking %random.6% * 20
    %damage% %actor% %shocking% direct
  break
  case 2
    %echo% The click of the tile depressing into the floor is drownd out by a sudden explosion as a fireball hits everyone!
    eval fireball %random.5% * 12 + 60
    %aoe% %fireball% fire
  break
  case 3
    %echo% A gas cloud is released!
    wait 2
    set person %self.people%
    while %person%
      if !%person.vampire()%
        if %person.is_npc%
          set poison 500
        else
          set poison 90
        end
        %send% %person% You start choking on the gas!
        %dot% #16036 %person% %poison% 30 poison 5
      end
      set person %person.next_in_room%
    done
  break
  case 4
    set blades 3
    while %blades%
      if %blades% == 1
        set dir north
      elseif %blades% == 2
        set dir ceiling
      else
        set dir west
      end
      set target %random.char%
      %echo% A blade comes flying from the %dir% and hits ~%target%!
      set crit %random.20%
      %damage% %target% 100 physical
      if %crit% == 20
        %dot% %target% 15 75 physical 2
      end
      eval blades %blades% - 1
    done
  break
done
~
$
