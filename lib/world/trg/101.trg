#10100
Swamp Hut passive~
2 bw 5
~
switch %random.4%
  case 1
    %echo% You swat at a mosquito as it bites into your arm.
  break
  case 2
    %echo% The hut seems to sway in the wind.
  break
  case 3
    %echo% The floorboards creak beneath your feet.
  break
  case 4
    %echo% You hold your nose as a new stench emanates from the hut.
  break
done
~
#10101
Swamp Hag passive~
0 bw 5
~
* This script is no longer used. It was replaced by custom strings.
switch %random.4%
  case 1
    %echo% %self.name% rocks in %self.hisher% chair.
  break
  case 2
    %echo% %self.name% grinds a pile of bones using a huge mortar and pestle.
  break
  case 3
    %echo% %self.name% plucks out one of %self.hisher% eyes, cleans it on %self.hisher% shirt, then pops it back in.
  break
  case 4
    %echo% %self.name% gurgles the words, Something wicked this way is.
  break
done
~
#10102
Swamp Hag combat~
0 k 15
~
if !%actor.affect(blind)%
  blind
else
  switch %random.4%
    case 1
      %send% %actor% %self.name% swings %self.hisher% pestle into the side of your head!
      %echoaround% %actor% %self.name% swings %self.hisher% pestle into the side of %actor.name%s head!
      dg_affect %actor% STUNNED on 10
      %damage% %actor% 25
    break
    case 2
      %echo% %self.name% reaches under the bed and opens a cage
      wait 1 sec
      %load% mob 10101 ally
      makeuid rat mob rat
      if %rat%
        %echo% %rat.name% scurries out from a cage!
        %force% %rat% %aggro% %actor%
      end
    break
    case 3
      %send% %actor% %self.name% hexes you!
      %echoaround% %actor% %self.name% hexes %actor.name%!
      dg_affect %actor% SLOW on 30
    break
    case 4
      %echo% %self.name% begins swinging %self.hisher% pestle wildly!
      wait 20
      %echo% Everyone is hit by the pestle!
      %aoe% 100 physical
    break
  done
end
~
#10103
Swamp Hag reward~
0 f 100
~
%adventurecomplete%
eval room_var %self.room%
eval ch %room_var.people%
while %ch%
  eval test %%ch.is_ally(%actor%)%%
  if %ch.is_pc% && %test%
    if %ch.skill(Natural Magic)% < 50 && %ch.skill(Natural Magic)% > 0
      %ch.gain_skill(Natural Magic,1)%
    elseif %ch.skill(High Sorcery)% < 50 && %ch.skill(High Sorcery)% > 0
      %ch.gain_skill(High Sorcery,1)%
    elseif %ch.skill(Battle)% < 50 && %ch.skill(Battle)% > 0
      %ch.gain_skill(Battle,1)%
    elseif %ch.skill(Stealth)% < 50 && %ch.skill(Stealth)% > 0
      %ch.gain_skill(Stealth,1)%
    elseif %ch.skill(Vampire)% < 50 && %ch.skill(Vampire)% > 0
      %ch.gain_skill(Vampire,1)%
    end
  end
  eval ch %ch.next_in_room%
done
~
#10104
Swamp Rat combat~
0 k 15
~
wait 10
%echo% %self.name% bites deep!
%send% %actor% You don't feel so good...
%echoaround% %actor% %actor.name% doesnt look so good...
%dot% %actor% 200 30
~
#10105
Berk combat~
0 k 15
~
if !%actor.affect(disarm)%
  disarm
elseif !%actor.affect(blind)% && %random.2% == 2
  blind
else
  wait 10
  %send% %actor% Berk's dagger seems to have left some poison in your system!
  %echoaround% %actor% %actor.name% looks as if %actor.heshe%'s been poisoned!
  * 1 or both poison effects:
  switch %random.3%
    case 1
      dg_affect %actor% SLOW on 30
    break
    case 2
      %dot% %actor% 100 30 poison
    break
    case 3
      dg_affect %actor% SLOW on 15
      %dot% %actor% 100 30 poison
    break
  done
end
~
#10106
Jorr combat~
0 k 15
~
if !%actor.affect(colorburst)%
  colorburst
elseif !%self.affect(foresight)%
  foresight
else
  wait 10
  switch %random.3%
    case 1
      %send% %actor% Jorr raises her hand over the fire and it whips toward you, causing painful burns!
      %echoaround% %actor% Jorr raises her hand over the fire and it whips toward %actor.name%, causing painful burns!
      %dot% %actor% 100 15 fire
      %damage %actor% 30 fire
    break
    case 2
      %echo% Jorr kicks at the fire, shooting hot embers in all directions!
      %aoe% 50 fire
    break
    case 3
      %echo% The fire envelops Jorr and then shoots out in all directions, burning everyone!
      eval room_var %self.room%
      eval ch %room_var.people%
      while %ch%
        eval next_ch %ch.next_in_room%
        eval test %%self.is_enemy(%ch%)%%
        if %test%
          %dot% %ch% 50 10 fire
          %damage% %ch% 30 fire
        end
        eval ch %next_ch%
      done
    break
  done
end
~
#10107
Tranc combat~
0 k 15
~
wait 10
* If dog summoned, tank
if %self.varexists(hound)%
  if %self.hound%
    if !%actor.affect(disarm)%
      disarm
    elseif %self.health% < (%self.maxhealth% / 2)
      %echo% Tranc quaffs a potion!
      %damage% %self% -50
    end
    halt
  end
end
* Otherwise, summon dog
%echo% Tranc whistles loudly!
%load% mob 10108 ally
makeuid hound mob black-haired
if %hound%
  echo %hound.name% appears!
  %force% %hound% %aggro% %actor%
end
set hound 1
remote hound %self.id%
~
#10108
Liza the Hound combat~
0 k 15
~
wait 10
switch %random.3%
  case 1
    eval room_var %self.room%
    eval ch %room_var.people%
    while %ch%
      eval test %%self.is_enemy(%ch%)%%
      if %test% && %ch.maxmana% > %actor.maxmana%
        %send% %ch% %self.name% is coming for you!
        %echoaround% %ch% %self.name% runs for %ch.name%!
        %aggro% %ch%
        halt
      end
      eval ch %ch.next_in_room%
    done
  break
  case 2
    %send% %actor% %self.name% sinks her teeth into your leg! You don't feel so good.
    %echoaround% %actor% %self.name% sinks her teeth into %actor.name%'s leg! %actor.name% doesn't look so good.
    dg_affect %actor% MAX-MANA -100 10
  break
  case 3
    %send% %actor% %self.name% sinks her teeth into your leg! You don't feel so good.
    %echoaround% %actor% %self.name% sinks her teeth into %actor.name%'s leg! %actor.name% doesn't look so good.
    dg_affect %actor% MANA-REGEN -5 20
  done
done
~
#10110
Egg hatch~
1 ab 1
~
* Random trigger to hatch the egg.
%echo% %self.shortdesc% begins to twitch.
wait 60 sec
%echo% %self.shortdesc% twitches.
wait 60 sec
%echo% %self.shortdesc% begins to crack...
wait 10 sec
%echo% %self.shortdesc% splits open!
wait 1 sec
%load% mob 10113
%echo% A baby dragon emerges from %self.shortdesc% and stretches its wings!
%purge% %self%
~
#10111
Sly combat~
0 k 10
~
%send% %actor% Sly's shadow stretches out and grabs at you...
%echoaround% %actor% Sly's shadow stretches out and grabs at %actor.name%...
dg_affect %actor% STUNNED on 10
wait 2 sec
%send% %actor% The shadows grasp and pull at you!
wait 2 sec
%send% %actor% The shadows rip at your very soul!
wait 2 sec
%send% %actor% Feelings of terror was over you!
wait 2 sec
%send% %actor% Your own shadow reaches up to strangle you!
~
#10112
Chiv combat~
0 k 8
~
wait 10
%echo% Chiv sinks into the shadows...
wait 30
%send% %actor% Chiv appears behind you and sinks her dagger into your back!
%echoaround% %actor% Chiv appears behind %actor.name% and sinks her dagger into %actor.hisher% back!
%dot% %actor% 50 15 physical
%damage% %actor% 150 physical
~
#10113
Stealth combat low-level~
0 k 15
~
if !%actor.affect(blind)%
  blind
else
  kick
end
~
#10114
Baby dragon death~
0 f 100
~
%load% mob 9060
%echo% A massive green dragon crashes through the roof!
return 0
~
#10115
Thieves spawner~
1 n 100
~
* Warning: This script completely ignores spawn limits! Don't use it in instances that reset often
* Set up variables: cumulative is the cumulative probability of all results so far (including the current one, found indicates we've rolled something and should stop checking
eval cumulative 0
eval found 0
eval Rand %random.100%
* vnums of the mobs to spawn
eval vnumSly 10110
eval vnumSlyBetterloot 10114
eval vnumChiv 10111
eval vnumThief 10112
* Probabilities of each number of non-generic thieves spawning
* Treat this as an exclusive interaction list
eval Double 20
eval Single 80
* Independant chance of spawning the generic thief
eval Generic 50
* There is probably a way to do this with a loop...
eval cumulative %cumulative% + %Double%
if (%Rand% <= %cumulative%) && %found% == 0
  * Spawn both thieves
  %load% mob %vnumChiv%
  %echo% Chiv arrives!
  %load% mob %vnumSlyBetterloot%
  %echo% Sly arrives!
  * Set found - if not done, this will fall through to lower blocks (Rand will still be less than cumulative)
  eval found 1
end
eval cumulative %cumulative% + %Single%
if (%Rand% <= %cumulative%) && %found% == 0
  * Choose a thief to spawn
  switch %random.2%
    case 1
      * Chiv
      %load% mob %vnumChiv%
      %echo% Chiv arrives!
    break
    case 2
      * Sly
      %load% mob %vnumSly%
      %echo% Sly arrives!
    break
  done
  * Set found - if not done, this will fall through to lower blocks (Rand will still be less than cumulative)
  eval found 1
end
if %random.100% <= %Generic%
  * Spawn the generic thief
  %load% mob %vnumThief%
  %echo% A thief arrives!
end
%purge% %self%
~
#10116
Banditos spawner~
1 n 100
~
* Warning: This script completely ignores spawn limits! Don't use it in instances that reset often
* Set up variables: cumulative is the cumulative probability of all results so far (including the current one, found indicates we've rolled something and should stop checking
eval cumulative 0
eval found 0
eval Rand %random.100%
* vnums of the mobs to spawn
eval vnumBerk 10105
eval vnumJorr 10106
eval vnumTranc 10107
* Probabilities of each number of banditos spawning
* Treat this as an exclusive interaction list
eval Triple 4
eval Double 30
eval Single 66
* There is probably a way to do this with a loop...
eval cumulative %cumulative% + %Triple%
if (%Rand% <= %cumulative%) && %found% == 0
  * Spawn all 3 banditos
  %load% mob %vnumBerk%
  %echo% Berk arrives!
  %load% mob %vnumJorr%
  %echo% Jorr arrives!
  %load% mob %vnumTranc%
  %echo% Tranc arrives!
  * Set found - if not done, this will fall through to lower blocks (Rand will still be less than cumulative)
  eval found 1
end
eval cumulative %cumulative% + %Double%
if (%Rand% <= %cumulative%) && %found% == 0
  * Choose a bandito NOT to spawn
  switch %random.3%
    case 1
      * Berk + Jorr
      %load% mob %vnumBerk%
      %echo% Berk arrives!
      %load% mob %vnumJorr%
      %echo% Jorr arrives!
    break
    case 2
      * Berk + Tranc
      %load% mob %vnumBerk%
      %echo% Berk arrives!
      %load% mob %vnumTranc%
      %echo% Tranc arrives!
    break
    case 3
      * Jorr + Tranc
      %load% mob %vnumJorr%
      %echo% Jorr arrives!
      %load% mob %vnumTranc%
      %echo% Tranc arrives!
    break
  done
  * Set found - if not done, this will fall through to lower blocks (Rand will still be less than cumulative)
  eval found 1
end
eval cumulative %cumulative% + %Single%
if (%Rand% <= %cumulative%) && %found% == 0
  * Choose a bandito to spawn
  switch %random.3%
    case 1
      * Berk
      %load% mob %vnumBerk%
      %echo% Berk arrives!
    break
    case 2
      * Tranc
      %load% mob %vnumTranc%
      %echo% Tranc arrives!
    break
    case 3
      * Jorr
      %load% mob %vnumJorr%
      %echo% Jorr arrives!
    break
  done
  * Set found - if not done, this will fall through to lower blocks (Rand will still be less than cumulative)
  eval found 1
end
%purge% %self%
~
#10140
Cactus combat~
0 k 25
~
switch %random.3%
  case 1
    %send% %actor% %self.name% brushes against you, leaving lots of painful spines stuck in you!
    %echoaround% %actor% %self.name% brushes against %actor.name%, leaving spines stuck in %actor.himher%!
    %damage% %actor% 10 physical
    %dot% %actor% 50 30 physical 3
  break
  case 2
    %echo% %self.name% puffs up like an angry cat, summoning miniature storm clouds!
    wait 1 sec
    if %actor% && (%actor.room% == %self.room%)
      %send% %actor% A torrent of water strikes you in the face, briefly blinding you!
      %echoaround% %actor% A torrent of water strikes %actor.name% in the face!
      dg_affect %actor% BLIND on 10
    else
      halt
    end
  break
  case 3
    %echo% %self.name%'s spines ripple!
    wait 1 sec
    if %actor% && (%actor.room% == %self.room%)
      %echo% %self.name% regrows slightly.
      %damage% %self% -25
    else
      halt
    end
  break
done
~
#10141
Delayed aggro greet/entry~
0 gi 100
~
if %actor%
  * Actor entered room - valid target?
  if (%actor.is_npc% && %actor.mob_flagged(HUMAN)% && !%actor.aff_flagged(!ATTACK)%) || (%actor.is_pc% && %actor.level% > 25 && !%actor.on_quest(10147)% && !%actor.nohassle%)
    eval target %actor%
  end
else
  * entry - look for valid target in room
  eval person %room.people%
  eval count 0
  while %person%
    * Manage cactus population
    if %person.vnum% >= 10140 && %person.vnum% <= 10142
      eval count %count% + 1
      if %count% >= 2 || %random.2% == 2
        %echo% %self.name% turns back into an ordinary cactus.
        %purge% %self%
        halt
      end
    end
    * validate
    if (%person.is_npc% && %person.mob_flagged(HUMAN)% && !%person.aff_flagged(!ATTACK)%) || (%person.is_pc% && %person.level% > 25 && !%person.on_quest(10147)% && !%person.nohassle%)
      eval target %person%
    end
    eval person %person.next_in_room%
  done
end
if !%target%
  halt
end
wait 2
if %target.room% != %self.room% || %self.disabled% || %self.fighting%
  halt
end
%send% %target% %self.name% moves menacingly towards you...
%echoaround% %target% %self.name% moves menacingly towards %target.name%...
wait 1 sec
if %target.room% != %self.room% || %self.disabled% || %self.fighting%
  halt
end
%echoaround% %target% %self.name% attacks %target.name%!
%send% %target% %self.name% attacks you!
%aggro% %target%
look
~
#10142
Monsoon Rift cleanup + complete~
2 v 100
~
* Start of script fragment: Monsoon cleanup
* Iterates over a series of vnums and removes all mobs with those vnums from the instance.
* Also cleans up the entrance portal and saguaro cactus.
* This script fragment is duplicated in triggers: 10142, 10177, 10180
eval loc %instance.location%
eval obj %loc.contents%
while %obj%
  eval next_obj %obj.next_in_list%
  if %obj.vnum% == 10140
    %at% %loc% %echo% The monsoon rift closes.
    %purge% %obj%
  end
  eval obj %next_obj%
done
* Despawn saguaro obj
makeuid loc room i10145
if %loc%
  eval obj %loc.contents%
  while %obj%
    eval next_obj %obj.next_in_list%
    if %obj.vnum% == 10171
      %at% %loc% %echo% You lose track of %obj.shortdesc%.
      %purge% %obj%
    end
    eval obj %next_obj%
  done
end
%adventurecomplete%
eval current_vnum 10147
while %current_vnum% >= 10140
  if %current_vnum% <= 10143
    set message $n turns back into an ordinary cactus.
  else
    set message $n leaves.
  end
  %purge% instance mob %current_vnum% %message%
  eval current_vnum %current_vnum% - 1
done
* End of script fragment.
~
#10143
Saguaro treant combat~
0 k 25
~
switch %random.4%
  case 1
    %send% %actor% %self.name% brushes an arm against you, leaving dozens of painful spines behind!
    %echoaround% %actor% %self.name% brushes an arm against %actor.name%, leaving behind dozens of spines!
    %dot% %actor% 20 30 physical 10
    %dot% %actor% 20 30 physical 10
  break
  case 2
    %echo% %self.name%'s spines ripple, and storm clouds form overhead.
    wait 1 sec
    %send% %actor% You are struck by lightning!
    %echoaround% %actor% %actor.name% is struck by lightning!
    %damage% %actor% 75 magical
  break
  case 3
    %echo% %self.name% puffs up like an angry cat, and monsoon clouds form overhead.
    wait 1 sec
    %send% %actor% The clouds burst, turning the ground beneath your feet into mud and slowing you down!
    %echoaround% %actor% The clouds burst, turning the ground beneath %actor.name%'s feet into mud and slowing %actor.himher% down!
    dg_affect %actor% SLOW on 30
  break
  case 4
    %send% %actor% %self.name% clubs you painfully with an arm, leaving stinging spines stuck into you!
    %echoaround% %actor% %self.name% clubs %actor.name% with an arm, leaving several spines stuck in %actor.himher%!
    %damage% %actor% 25 physical
    %dot% %actor% 20 30 physical 10
  break
done
~
#10144
Monsoon Cloud~
1 bw 10
~
switch %random.4%
  case 1
    %echo% Lightning crawls from cloud to cloud like spiderwebs.
  break
  case 2
    %echo% Rain drops like sheets from the clouds, until you can barely see where you're going.
  break
  case 3
    %echo% The sky flashes as a brilliant column of lightning splits it in two.
  break
  case 4
    %echo% Rolling thunder shakes the rain-soaked desert.
  break
end
~
#10145
Monsoon totem fake chant command~
1 c 2
chant~
if (!(monsoon /= %arg%) || %actor.position% != Standing)
  return 0
  halt
end
eval room %actor.room%
eval cycles_left 5
while %cycles_left% >= 0
  eval permission %%actor.canuseroom_member(%room%)%%
  eval sector_valid ((%room.sector% == Desert) || (%room.sector% == Grove))
  eval cloud_present 0
  eval object %room.contents%
  while %object% && !%cloud_present%
    if %object.vnum% == 10144
      eval cloud_present 1
    end
    eval object %object.next_in_list%
  done
  if (%actor.room% != %room%) || !%permission% || !%sector_valid% || %cloud_present% || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 5
      %echoaround% %actor% %actor.name%'s chant is interrupted.
      %send% %actor% Your chant is interrupted.
    elseif !%permission%
      %send% %actor% You don't have permission to use the monsoon chant here.
    elseif !%sector_valid%
      %send% %actor% You must perform the chant on a desert or grove.
    elseif %cloud_present%
      %send% %actor% The monsoon chant has already been performed here.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% %actor.name% pulls out a toad-shaped totem and begins to chant...
      %send% %actor% You pull out your monsoon totem and begin to chant...
    break
    case 4
      %echoaround% %actor% %actor.name% sways as %actor.heshe% whispers strange words into the air...
      %send% %actor% You sway as you whisper the words of the monsoon chant...
    break
    case 3
      %echoaround% %actor% %actor.name%'s monsoon totem takes on a soft green glow, and the air around it seems to crackle...
      %send% %actor% Your monsoon totem takes on a soft green glow, and the air around it crackles with electricity...
    break
    case 2
      %echoaround% %actor% A tiny raincloud forms in the air around %actor.name%'s monsoon totem...
      %send% %actor% A tiny raincloud forms in the air around your monsoon totem...
    break
    case 1
      %echoaround% %actor% %actor.name% whispers into the raincloud, which grows dark and begins to rise...
      %send% %actor% You whisper ancient words of power into the raincloud as it grows dark and begins to rise...
    break
    case 0
      %echoaround% %actor% %actor.name% completes %actor.hisher% chant, and the raincloud fills the sky!
      %send% %actor% You complete your chant, and the raincloud fills the sky!
      %echo% Thunder rolls across the sky as heavy drops of rain begin to fall.
      %load% obj 10144 %room%
      if %actor.varexists(monsoon_chant_counter)%
        eval monsoon_chant_counter %actor.monsoon_chant_counter)% + 1
      else
        eval monsoon_chant_counter 1
      end
      remote monsoon_chant_counter %actor.id%
      if %monsoon_chant_counter% >= 6
        %quest% %actor% trigger 10147
        %send% %actor% You have finished quenching the cacti, and should return to the druid.
        %send% %actor% Your monsoon totem splinters and breaks!
        %echoaround% %actor% %actor.name%'s ironwood totem splinters and breaks!
        %purge% %self%
      end
      halt
    break
  done
  * Shortcut for immortals
  if !%actor.nohassle%
    wait 5 sec
  end
  eval cycles_left %cycles_left% - 1
done
~
#10147
Natural Magic: Cacti quench quest start~
2 u 100
~
eval monsoon_chant_counter 0
remote monsoon_chant_counter %actor.id%
if !%actor.inventory(10143)%
  %load% obj 10143 %actor% inv
  eval item %actor.inventory(10143)%
  %send% %actor% You receive %item.shortdesc%.
end
~
#10148
Monsoon cactus death tracker + reward~
0 f 100
~
if %actor.on_quest(10147)%
  %quest% %actor% drop 10147
  %send% %actor% You fail the quest Quench the Desert - you're supposed to water the cacti, not kill them!
end
* Number of attacker kills for quest completion
eval target 4
eval room %self.room%
eval char %room.people%
while %char%
  if %char.is_pc%
    if %char.on_quest(10141)%
      * We're already on the quest
      if %char.varexists(monsoon_attacker_kills)%
        eval monsoon_attacker_kills %char.monsoon_attacker_kills)% + 1
      else
        eval monsoon_attacker_kills 1
      end
      remote monsoon_attacker_kills %char.id%
      if %monsoon_attacker_kills% >= %target%
        %quest% %char% trigger 10141
        %send% %char% You have killed enough of these cacti. Head into the rift and look for their leader on top of the hill.
      else
        %send% %char% You have killed %monsoon_attacker_kills% cacti.
      end
    else
      * We're not on the quest yet; start it
      if %char.can_start_quest(10141)%
        %quest% %char% start 10141
      end
      if !%char.on_quest(10141)%
        * Quest start failed
      else
        * Reset attacker kill count for the new quest
        eval monsoon_attacker_kills 1
        remote monsoon_attacker_kills %char.id%
      end
    end
    * Bonus exp reward
    if %random.2% == 2 && (%random.2% == 2 || (%char.on_quest(10141)% && !%char.quest_triggered(10141)%))
      %send% %char% You gain 1 bonus experience point.
      nop %char.bonus_exp(1)%
    end
  end
  eval char %char.next_in_room%
done
~
#10149
Saguaro treant must-fight~
0 s 100
~
%send% %actor% You cannot flee from %self.name%!
return 0
~
#10150
Free-tailed bat emotes~
0 bw 10
~
if (%self.disabled% || %self.fighting%)
  halt
end
switch %random.3%
  case 1
    %echo% %self.name% flits about overhead.
  break
  case 2
    %echo% %self.name% snatches a moth out of the air.
  break
  case 3
    %echo% %self.name% joins a swarm of bats overhead.
    %purge% %self%
  break
end
~
#10151
Gila monster emotes~
0 bw 10
~
if (%self.disabled% || %self.fighting%)
  halt
end
switch %random.3%
  case 1
    %echo% %self.name% suns itself on the rocks.
  break
  case 2
    %echo% %self.name% flicks its tongue in search of a scent.
  break
  case 3
    %echo% %self.name% crawls into a burrow in the ground.
    %purge% %self%
  break
end
~
#10152
Armadillo emotes~
0 bw 10
~
if (%self.disabled% || %self.fighting%)
  halt
end
switch %random.3%
  case 1
    %echo% %self.name% rolls up into a ball and rolls away from you.
    %purge% %self%
  break
  case 2
    %echo% %self.name% digs at the ground until it finds a tasty grub.
  break
  case 3
    %echo% %self.name% scampers around in the dirt.
  break
end
~
#10153
Cactus wren emotes~
0 bw 10
~
if (%self.disabled% || %self.fighting%)
  halt
end
switch %random.3%
  case 1
    %echo% %self.name% pecks at loose seeds on the ground.
  break
  case 2
    %echo% %self.name% picks through leaves and sticks, looking for insects.
  break
  case 3
    %echo% %self.name% pokes its head out from its home in a cactus.
  break
end
~
#10154
Antelope squirrel emotes~
0 bw 10
~
if (%self.disabled% || %self.fighting%)
  halt
end
switch %random.3%
  case 1
    %echo% %self.name% stashes a seed in its cheek.
  break
  case 2
    %echo% %self.name% grooms its long, thin tail.
  break
  case 3
    %echo% %self.name% scurries into the shade and hugs the cool ground.
  break
end
~
#10155
Bighorn sheep emotes~
0 bw 10
~
if (%self.disabled% || %self.fighting%)
  halt
end
switch %random.3%
  case 1
    %echo% %self.name% rubs its horns against a shrub.
  break
  case 2
    %echo% %self.name% runs off after an interloping ram.
    %purge% %self%
  break
  case 3
    %echo% %self.name% chews thoughtfully on a stray branch from a shrub.
  break
end
~
#10156
Coati emotes~
0 bw 10
~
if (%self.disabled% || %self.fighting%)
  halt
end
switch %random.3%
  case 1
    %echo% %self.name% rustles through the ground litter, looking for bugs.
  break
  case 2
    %echo% %self.name% chirps and snorts at you.
  break
  case 3
    %echo% %self.name% struts around with its long tail held straight up.
  break
end
~
#10157
Monsoon room environment~
2 bw 10
~
switch %random.4%
  case 1
    %echo% Large, cold raindrops fall on you from above.
  break
  case 2
    %echo% The winds shift, bringing fresh petrichor from the cool rain.
  break
  case 3
    %echo% Lightning flashes across the stormclouds.
  break
  case 4
    %echo% Rolling thunder shakes the rain-soaked desert.
  break
end
~
#10158
Hug a Cactus~
0 ct 0
hug~
* test targeting me
eval test %%actor.char_target(%arg%)%%
if (%test% != %self% || %actor.nohassle%)
  return 0
  halt
end
%send% %actor% You hug %self.name% and immediately realize your mistake as dozens of barbed spines stick into your skin!
%echoaround% %actor% %actor.name% hugs %self.name% but immediately lets out a yelp of pain as dozens of barbed spines stick into %actor.hisher% skin!
%dot% %actor% 100 10 physical 1
return 1
~
#10159
Teddybear cactus emotes~
0 btw 5
~
if (%self.disabled% || %self.fighting%)
  halt
end
switch %random.3%
  case 1
    say Why won't anyone huuuug me?
  break
  case 2
    %echo% %self.name% brushes its spines.
  break
  case 3
    %echo% You find %self.name% sitting dangerously close to your leg.
  break
end
~
#10160
Monsoon sorcery quest study command~
1 c 2
study~
if (!((monsoon /= %arg%) || (rift /= %arg%)) || %actor.position% != Standing)
  return 0
  halt
end
eval room %actor.room%
eval start_cycles 5
eval cycles_left %start_cycles%
while %cycles_left% >= 0
  eval permission %%actor.canuseroom_guest(%room%)%%
  eval location_valid (%room.building% == Tower of Sorcery || %room.building% == Top of the Tower)
  if (%actor.room% != %room%) || !%permission% || !%location_valid% || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < %start_cycles%
      %echoaround% %actor% %actor.name%'s studying is interrupted.
      %send% %actor% Your studying is interrupted.
    elseif !%permission%
      %send% %actor% You don't have permission to study here.
    elseif !%sector_valid%
      %send% %actor% You must study at a Tower of Sorcery.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% %actor.name% grabs several books and starts studying the monsoon...
      %send% %actor% You pull several books from their shelves and start studying the monsoon...
    break
    case 4
      %send% %actor% You flip through the books for references to the monsoon rift...
    break
    case 3
      %send% %actor% You discover an anecdote about a sorcerer who traveled through an older monsoon rift and discovered its source in the Magiterranean.
    break
    case 2
      %send% %actor% You learn that the rifts are opened by common druids, but require training in High Sorcery because of their similarity to portals.
    break
    case 1
      %send% %actor% It seems as if you should be able to close the rift using Sorcery, if you can find a good spell in one of these books...
    break
    case 0
      %echoaround% %actor% %actor.name% closes %actor.hisher% books and puts them away.
      %send% %actor% You close your books and put them away...
      * Load the scroll for the next quest
      %load% obj 10175 %actor% inv
      %quest% %actor% trigger 10144
      %quest% %actor% finish 10144
      halt
    break
  done
  wait 4 sec
  eval cycles_left %cycles_left% - 1
done
~
#10161
Fake infiltrate higher template id~
2 c 0
infiltrate~
if !%arg%
  return 0
  halt
end
* One quick trick to get the target room
eval direction %%actor.parse_dir(%arg%)%%
eval room_var %self%
eval tricky %%room_var.%direction%(room)%%
if !%tricky% || (%tricky.template% < %room_var.template%)
  return 0
  halt
end
if !%actor.on_quest(10150)%
  %send% %actor% You don't currently have a reason to infiltrate there.
  return 1
  halt
end
%send% %actor% You successfully infiltrate!
%teleport% %actor% %tricky%
%force% %actor% look
return 1
~
#10162
Room block higher template id without infiltrate~
2 q 100
~
* One quick trick to get the target room
eval room_var %self%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if %actor.nohassle%
  %send% %actor% The obstruction gives you no hassle.
  halt
end
if (!%tricky% || %tricky.template% < %room_var.template%)
  halt
end
%send% %actor% You can't go that way without Infiltrate!
return 0
~
#10163
Weather in the Rift~
2 c 0
weather~
%send% %actor% It's raining lightly overhead, but the weather worsens further off the path, until you can't see past the wall of rain.
~
#10164
Weather outside the Rift~
1 c 4
weather~
%send% %actor% Dark monsoon clouds loom overhead, dropping rain in sheets.
~
#10165
Suppress Weather~
1 n 100
~
* Turns on !WEATHER for a number of seconds equal to <value1>
dg_affect_room %self.room% !WEATHER on %self.val0%
~
#10166
Cactus Spawn Teleport~
0 n 100
~
eval room %self.room%
if (!%instance.location% || %room.template% != 10146)
  halt
end
mgoto %instance.location%
%echo% A stream of mana flows from the rift, animating %self.name%!
detach 1016 %self.id%
~
#10167
Monsoon thief + vampire spawn teleport/hide~
0 n 100
~
eval loc %instance.location%
if !%loc% || !%self.vampire%
  eval loc %self.room%
end
eval hide_again 1
eval person %loc.people%
while %person% && %hide_again%
  if %person.is_pc% && ((%person.skill(Vampire)% > 50 && %self.vampire%) || (%person.skill(Stealth)% > 50 && !%self.vampire%))
    eval hide_again 0
  end
  eval person %person.next_in_room%
done
eval person %loc.people%
while %person%
  eval next_person %person.next_in_room%
  if %person% != %self% && %person.vnum% == %self.vnum%
    if !%person.aff_flagged(HIDE)%
      if %hide_again%
        %echoaround% %person% %person.name% steps into the shadows and disappears.
      end
    end
    %purge% %person%
  end
  eval person %next_person%
done
if %loc%
  mgoto %loc%
end
if %hide_again%
  dg_affect %self% HIDE on -1
end
~
#10168
Spawn Saguaro Treant~
2 u 100
~
* find and purge the saguaro obj
eval obj %room.contents%
eval found 0
while %obj%
  eval next_obj %obj.next_in_list%
  if (%obj.vnum% == 10171)
    eval found 1
    %purge% %obj%
  end
  eval obj %next_obj%
done
if %found%
  * load treant boss
  %load% mob 10143
end
~
#10169
Monsoon reward replacer~
1 n 100
~
* After 1 second, purge this object and load an object - rotating through the loot list
wait 1
eval actor %self.carried_by%
if %actor.varexists(last_monsoon_loot_item)%
  eval last_monsoon_loot_item %actor.last_monsoon_loot_item%
end
eval next_item 0
switch %last_monsoon_loot_item%
  case 10159
    eval next_item 10153
  break
  case 10153
    eval next_item 10160
  break
  case 10160
    eval next_item 10155
  break
  case 10155
    eval next_item 10167
  break
  case 10167
    eval next_item 10152
  break
  case 10152
    eval next_item 10168
  break
  case 10168
    eval next_item 10163
  break
  case 10163
    eval next_item 10165
  break
  case 10165
    eval next_item 10162
  break
  case 10162
    eval next_item 10166
  break
  default
    eval next_item 10159
  break
done
eval level %self.level%
if !%level%
  eval level %actor.level%
end
%load% obj %next_item% %actor% inv %level%
eval item %actor.inventory()%
%send% %actor% %self.shortdesc% opens, revealing %item.shortdesc%!
eval last_monsoon_loot_item %next_item%
remote last_monsoon_loot_item %actor.id%
%purge% %self%
~
#10171
Monsoon thief + vampire reveal~
0 hw 100
~
if (%self.vampire% && %actor.skill(Vampire)% < 51) || (!%self.vampire% && %actor.skill(Stealth)% < 51)
  halt
end
visible
wait 1
%send% %actor% %self.name% steps out of the shadows to greet you.
%echoaround% %actor% %self.name% steps out of the shadows to greet %actor.name%.
* Reveal hide affect from self
detach 10171 %self.id%
~
#10172
Wandering Merchant Spawner~
1 n 100
~
%load% mob 10146
~
#10173
Give Supply List~
2 u 100
~
%load% obj 10174 %actor% inv
~
#10174
Give Sorcery Notes~
2 u 100
~
if (%questvnum% == 10144)
  %load% obj 10142 %actor% inv
end
~
#10175
Monsoon wandering merchant leash~
0 in 100
~
eval start_room %instance.location%
if !%start_room%
  * No instance
  halt
end
eval room %self.room%
eval dist %%room.distance(%start_room%)%%
if %room.template% == 10146
  mgoto %start_room%
  mmove
  mmove
  mmove
  mmove
elseif %dist% > 20
  mgoto %start_room%
end
~
#10176
Monsoon infuse bat totem at oasis~
1 c 2
infuse~
if !%actor.vampire%
  return 0
  halt
end
* Infuse only at an oasis during the night. Costs 50 blood.
eval blood %actor.blood()%
eval room %actor.room%
eval cost 50
* Condition checking
if %blood% < %cost%
  %send% %actor% You don't have enough blood to infuse %self.shortdesc% - it costs %cost%.
  halt
end
if %room.sector% != Oasis
  %send% %actor% You can only infuse %self.shortdesc% at an oasis.
  halt
end
if %time.hour% > 7 && %time.hour% < 19
  %send% %actor% You can only infuse %self.shortdesc% at night.
  if !%actor.is_immortal%
    halt
  else
    %send% %actor% You use your immortal powers to ignore this restriction.
  end
end
* Charge blood
eval charge %%actor.blood(-%cost%)%%
nop %charge%
* Quest sends a message already
%quest% %actor% trigger 10156
%quest% %actor% finish 10156
~
#10177
Monsoon eclipse vampire ritual~
1 c 2
ritual rite~
if (!(eclipse /= %arg%) || %actor.position% != Standing)
  return 0
  halt
end
* Check time of day (only at start to avoid sunset annoyances)
if %time.hour% < 7 || %time.hour% > 19
  %send% %actor% You can only perform this ritual during the day.
  halt
end
eval room %actor.room%
eval cycles_left 5
while %cycles_left% >= 0
  eval sector_valid (%room.template% == 10145)
  if (%actor.room% != %room%) || !%sector_valid% || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the ritual
    if %cycles_left% < 5
      %echoaround% %actor% %actor.name%'s ritual is interrupted.
      %send% %actor% Your ritual is interrupted.
      * Refund blood here if we want to
    elseif !%sector_valid%
      %send% %actor% You must perform the eclipse ritual at the rocky hilltop beyond the rift.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      * Check cost here so invalid location doesn't charge
      eval cost 0
      if %actor.blood% < %cost%
        %send% %actor% You don't have enough blood to perform the eclipse ritual - it costs %cost%.
        halt
      end
      eval charge %%actor.blood(-%cost%)%%
      nop %charge%
      %echoaround% %actor% %actor.name% begins the eclipse ritual...
      %send% %actor% You begin the eclipse ritual...
    break
    case 4
      %echoaround% %actor% %actor.name% holds a glowing bat totem up toward the sun...
      %send% %actor% You hold the glowing bat totem up toward the sun...
    break
    case 2
      %echoaround% %actor% Tendrils of blood-red shadow stretch upward from %actor.name%'s bat totem...
      %send% %actor% Tendrils of blood-red shadow stretch upward from your bat totem...
    break
    case 1
      %echoaround% %actor% As %actor.name% performs the eclipse ritual, the sun takes on a deep red color...
      %send% %actor% As you perform the eclipse ritual, the sun takes on a deep red color....
    break
    case 0
      %echoaround% %actor% %actor.name% completes %actor.hisher% ritual!
      %send% %actor% You complete your ritual!
      * Echo the eclipse globally
      %regionecho% %actor.room% -9999 A dark shadow covers the land as the sun is momentarily eclipsed.
      %quest% %actor% trigger 10157
      %send% %actor% %self.shortdesc% splinters and breaks!
      %echoaround% %actor% %self.shortdesc% splinters and breaks!
      * Leave the loop
    break
  done
  if %cycles_left% > 0
    wait 5 sec
  end
  eval cycles_left %cycles_left% - 1
done
* Start of script fragment: Monsoon cleanup
* Iterates over a series of vnums and removes all mobs with those vnums from the instance.
* Also cleans up the entrance portal and saguaro cactus.
* This script fragment is duplicated in triggers: 10142, 10177, 10180
eval loc %instance.location%
eval obj %loc.contents%
while %obj%
  eval next_obj %obj.next_in_list%
  if %obj.vnum% == 10140
    %at% %loc% %echo% The monsoon rift closes.
    %purge% %obj%
  end
  eval obj %next_obj%
done
* Despawn saguaro obj
makeuid loc room i10145
if %loc%
  eval obj %loc.contents%
  while %obj%
    eval next_obj %obj.next_in_list%
    if %obj.vnum% == 10171
      %at% %loc% %echo% You lose track of %obj.shortdesc%.
      %purge% %obj%
    end
    eval obj %next_obj%
  done
end
%adventurecomplete%
eval current_vnum 10147
while %current_vnum% >= 10140
  if %current_vnum% <= 10143
    set message $n turns back into an ordinary cactus.
  else
    set message $n leaves.
  end
  %purge% instance mob %current_vnum% %message%
  eval current_vnum %current_vnum% - 1
done
* End of script fragment.
%quest% %actor% finish 10157
* Quest finish will purge this for us
~
#10178
Give Bat Totem~
2 u 100
~
if (%questvnum% == 10156)
  %load% obj 10161 %actor% inv
elseif (%questvnum% == 10157)
  %load% obj 10173 %actor% inv
end
~
#10179
Age Herbicide~
1 f 0
~
if %self.carried_by%
  %load% obj 10177 %self.carried_by% inv
  %send% %self.carried_by% One of the herbicide vials turns a bright red color and is done aging.
  %purge% %self%
  return 0
end
~
#10180
Monsoon rift close sorcery ritual~
1 c 2
ritual rite~
if (!(rift /= %arg%) || %actor.position% != Standing)
  return 0
  halt
end
eval room %actor.room%
eval cycles_left 5
while %cycles_left% >= 0
  eval sector_valid (%room.building% == Monsoon Rift)
  eval rift_present 0
  eval object %room.contents%
  while %object% && !%rift_present%
    if %object.vnum% == 10140
      eval rift_present 1
    end
    eval object %object.next_in_list%
  done
  if (%actor.room% != %room%) || !%sector_valid% || !%rift_present% || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 5
      %echoaround% %actor% %actor.name%'s ritual is interrupted.
      %send% %actor% Your ritual is interrupted.
    elseif !%sector_valid%
      %send% %actor% You must perform the ritual at the monsoon rift.
    elseif !%rift_present%
      %send% %actor% The rift here has already been closed.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% %actor.name% pulls out some chalk and begins the rift ritual...
      %send% %actor% You pull out some chalk and begin the rift ritual...
    break
    case 4
      %echoaround% %actor% %actor.name% draws a square frame in the air around the rift...
      %send% %actor% You draw a square frame in the air around the rift...
    break
    case 2
      %echoaround% %actor% %actor.name% pulls and reshapes the chaotic rift until it looks more stable, like a portal...
      %send% %actor% You pull and reshape the chaotic rift until it looks more stable, like a portal...
    break
    case 1
      %echoaround% %actor% %actor.name% whispers into the rift...
      %send% %actor% You whisper powerful words into the rift, weakening its ties to the Magiterranean...
    break
    case 0
      %echoaround% %actor% %actor.name% completes %actor.hisher% ritual by grabbing the monsoon rift with %actor.hisher% hands and pulling it shut!
      %send% %actor% You grab the monsoon rift with your hands and pull it shut!
      %quest% %actor% trigger 10145
      %send% %actor% %self.shortdesc% bursts into flames!
      %echoaround% %actor% %self.shortdesc% bursts into flames!
      * Leave the loop
    break
  done
  if %cycles_left% > 0
    wait 5 sec
  end
  eval cycles_left %cycles_left% - 1
done
* Start of script fragment: Monsoon cleanup
* Iterates over a series of vnums and removes all mobs with those vnums from the instance.
* Also cleans up the entrance portal and saguaro cactus.
* This script fragment is duplicated in triggers: 10142, 10177, 10180
eval loc %instance.location%
eval obj %loc.contents%
while %obj%
  eval next_obj %obj.next_in_list%
  if %obj.vnum% == 10140
    %at% %loc% %echo% The monsoon rift closes.
    %purge% %obj%
  end
  eval obj %next_obj%
done
* Despawn saguaro obj
makeuid loc room i10145
if %loc%
  eval obj %loc.contents%
  while %obj%
    eval next_obj %obj.next_in_list%
    if %obj.vnum% == 10171
      %at% %loc% %echo% You lose track of %obj.shortdesc%.
      %purge% %obj%
    end
    eval obj %next_obj%
  done
end
%adventurecomplete%
eval current_vnum 10147
while %current_vnum% >= 10140
  if %current_vnum% <= 10143
    set message $n turns back into an ordinary cactus.
  else
    set message $n leaves.
  end
  %purge% instance mob %current_vnum% %message%
  eval current_vnum %current_vnum% - 1
done
* End of script fragment.
%quest% %actor% finish 10145
* Quest finish will purge the ritual object for us
~
#10181
Druid tent fake search~
2 c 0
search~
if (%actor.position% != Standing)
  return 0
  halt
end
if %actor.quest_triggered(10150)%
  %send% %actor% You have already found all the evidence you need.
  return 1
  halt
end
if !(%actor.on_quest(10150)%
  %send% %actor% You don't need to search here right now.
  return 1
  halt
end
eval room %actor.room%
eval cycles_left 5
while %cycles_left% >= 0
  if (%actor.room% != %room%) || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the action
    if %cycles_left% < 5
      %echoaround% %actor% %actor.name%'s search is interrupted.
      %send% %actor% Your search is interrupted.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% %actor.name% starts searching the druid's tent...
      %send% %actor% You start searching the druid's tent...
    break
    case 4
      %echoaround% %actor% %actor.name% rummages through the druid's belongings...
      %send% %actor% You rummage through the druid's belongings...
    break
    case 3
      %echoaround% %actor% %actor.name% searches through the scrolls on the druid's shelves...
      %send% %actor% You search through the scrolls on the druid's shelves...
    break
    case 2
      %echoaround% %actor% %actor.name% opens the druid's ironwood trunk and peers inside...
      %send% %actor% You open the druid's ironwood trunk and peer inside...
    break
    case 1
      %echoaround% %actor% %actor.name% searches the druid's writing desk...
      %send% %actor% You search the druid's writing desk...
    break
    case 0
      %echoaround% %actor% %actor.name% completes %actor.hisher% search!
      %send% %actor% You complete your search!
      * Quest complete
      %load% obj 10178 %actor% inv
      %quest% %actor% trigger 10150
      %send% %actor% You should return to the robed thief.
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
~
#10190
Lavaformer Spawn~
0 n 100
~
eval room %self.room%
if (!%instance.location% || %room.template% != 10190)
  halt
end
mgoto %instance.location%
%purge% volcanoportal
~
#10191
Lavaforming~
0 i 100
~
eval room %self.room%
if !%instance.location%
  %purge% %self%
  halt
end
eval dist %%room.distance(%instance.location%)%%
if (%dist% > 4)
  mgoto %instance.location%
elseif (%room.sector% == Flowing Lava || %room.sector% == Cooling Lava || %room.building% == Volcano Caldera || %room.aff_flagged(*HAS-INSTANCE)%)
  * No Work
  halt
else
  %terraform% %room% 10190
  %load% obj 10192
  %echo% The raging lava comes crashing down the mountainside!
  %aoe% 1000 fire
end
~
#10192
Lava flow decay~
1 f 0
~
eval room %self.room%
if (%self.vnum% == 10192)
  if (%room.sector% != Flowing Lava)
    halt
  end
  %terraform% %room% 10191
  %load% obj 10193
  %echo% The lava flow cools and hardens.
elseif (%self.vnum% == 10193)
  if (%room.sector% != Cooling Lava)
    halt
  end
  %terraform% %room% 10192
end
%purge% %self%
return 0
~
#10193
Volcanic Weather~
1 c 4
weather~
%send% %actor% Dark clouds of volcanic ash cover the sky!
~
#10194
Lava Damage~
1 bw 100
~
%echo% The hot air from the lava flow blisters your skin!
%aoe% 100 fire
~
#10195
Volcano Cleanup~
2 e 100
~
%load% obj 10192
%terraform% %room% 10190
~
#10196
Caldera Damage~
2 bw 100
~
%echo% The hot air from the caldera causes your flesh to blister and melt!
%aoe% 1000 fire
~
$
