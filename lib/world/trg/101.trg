#10100
Swamp Hut passive~
2 b 5
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
0 b 5
~
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
        %force% %rat% %kill% %actor%
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
  eval test %%self.is_enemy(%ch%)%%
  if %ch.is_pc% && %test%
    if %ch.skill(Natural Magic)% < 50 && %ch.skill(Natural Magic)% > 0
      %ch.gain_skill(Natural Magic,1)%
    elseif %ch.skill(Sorcery)% < 50 && %ch.skill(Sorcery)% > 0
      %ch.gain_skill(Sorcery,1)%
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
%send %actor% You dont feel so good...
%echoaround% %actor% %actor.name% doesnt look so good...
%dot% %actor% 200 30
~
#10105
Berk comabt~
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
* spawn dog if not present
makeuid hound mob black-haired
makeuid corpse obj hound
if !%hound% && !%corpse%
  %echo% Tranc whistles loudly!
  %load% mob 10108 ally
  makeuid hound mob black-haired
  if %hound%
    echo %hound.name% appears!
    %force% %hound% %kill% %actor%
  end
  halt
end
* Otherwise, tanking
if !%actor.affect(disarm)%
  disarm
elseif %self.health% < (%self.maxhealth% / 2)
  %echo% Tranc quaffs a potion!
  %damage% %self% -50
end
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
      if %test && %ch.maxmana% > %actor.maxmana%
        %send% %ch% %self.name% is coming for you!
        %echoaround% %ch% %self.name% runs for %ch.name%!
        %kill% %ch%
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
$
