#18075
Fissure eruption~
2 ab 1
~
%regionecho% %room% 10 You feel a sudden wave of heat from the nearby fissure!
wait 2 sec
%regionecho% %room% 5 There is a deep, rumbling roar from far beneath your feet!
%asound% The fissure erupts!
%echo% A pillar of molten rock the size of a castle flies right past you!
~
#18076
Fiend Battle~
0 k 25
~
eval test 100*%self.health%/%self.maxhealth%
eval phase 1
if %test%<67
  eval phase 2
end
if %test%<34
  eval phase 3
end
if !%lastphase%
  eval lastphase phase
  remote lastphase %self.id%
end
if %lastphase%!=%phase%
  if %lastphase% < %phase%
    *we've advanced a phase
    %echo% %self.name% roars with fury!
    if %phase% == 2
      %echo% One of %self.name%'s chains shatters!
    end
    if %phase% == 3
      %echo% %self.name%'s second chain shatters, freeing it!
    end
  end
  eval lastphase %phase%
  remote lastphase %self.id%
  halt
end
if %phase%==1
  switch %random.3%
    case 1
      if !%self.affect(counterspell)%
        counterspell
      else
        if !%actor.affect(colorburst)%
          colorburst
        else
          heal
        end
      end
    break
    case 2
      if !%actor.affect(colorburst)%
        colorburst
      else
        heal
      end
    break
    case 3
      heal
    break
  done
end
if %phase%==2
  switch %random.3%
    case 1
      %echo% %self.name% whistles with a sound like escaping steam...
      %load% mob 18077 ally %self.level%
      %load% mob 18077 ally %self.level%
      %echo% A pair of fire elementals appear!
      %echo% %self.name% looks exhausted!
      *Ensure that no other abilities are used while the elementals are up:
      dg_affect %self% STUNNED on 30
      wait 25 sec
    break
    case 2
      %echo% %self.name% raises a hand, leaning back from the platform...
      wait 5 sec
      if %self.disabled%
        %echo% %self.name%'s attack is interrupted!
        halt
      end
      *Buff dodge massively but debuff own tohit massively...
      *should be a breather for average geared parties:
      dg_affect %self% TO-HIT -100 30
      dg_affect %self% SLOW on 30
      dg_affect %self% DODGE 100 30
      %echo% A flickering barrier of force surrounds %self.name%!
      wait 30 sec *Make sure he doesn't do anything else while the shield is up.
      %echo% The magical shield around %self.name% fades!
    break
    case 3
      %echo% %self.name% draws back a hand, curling it into a fist...
      wait 2 sec
      if %self.disabled%
        %echo% %self.name%'s attack is interrupted!
        halt
      end
      %echo% The lava below begins to rise!
      wait 3 sec
      if %self.disabled%
        %echo% %self.name%'s attack is interrupted!
        halt
      end
      %echo% &rA wave of magma rolls over the platform!&0
      %aoe% 50 fire
      %send% %actor% &rYou take the full force of the wave, and are badly burned!&0
      %echoaround% %actor% %actor.name% takes the full force of the wave!
      %damage% %actor% 100 fire
      %dot% %actor% 100 20 fire
      wait 1 sec
      %echo% The lava sinks back down.
    break
  done
end
if %phase%==3
  switch %random.3%
    case 1
      %echo% %self.name% raises a huge, molten fist!
      wait 5 sec
      *(dex*5)% chance to not get hit
      if %actor.dex%<%random.20%
        %send% %actor% &rThe earth shakes as %self.name% punches you, stunning you!&0
        %echoaround% %actor% The earth shakes as %self.name% punches %actor.name%, stunning %actor.himher%!
        %damage% %actor% 100 physical
        %damage% %actor% 50 fire
        dg_affect %actor% STUNNED on 10
      else
        %send% %actor% You leap out of the way of %self.name%'s fist at the last second!
        %echoaround% %actor% %actor.name% leaps out of the way of %self.name%'s fist at the last second!
        %send% %actor% &rThe wave of heat from %self.name%'s fist burns you!&0
        %damage% %actor% 50 fire
      end
    break
    case 2
      eval target %random.enemy%
      %send% %target% %self.name% makes a heretical gesture at you!
      %echoaround% %target% %self.name% makes a heretical gesture at %target.name%!
      wait 5 sec
      if %target%&&(%target.room%==%self.room%)
        if %target.trigger_counterspell%
          %send% %target% A tornado of harmless smoke briefly surrounds you!
          %echoaround %target% A tornado of harmless smoke briefly surrounds %target%!
        else
          %send% %target% &rYour body bursts into unquenchable flames!&0
          %echoaround% %target% %target.name%'s body bursts into unquenchable flames!
          %damage% %target% 25 fire
          %dot% %target% 100 300 fire
        end
      end
    break
    case 3
      %echo% %self.name% rises up out of the lava entirely!
      wait 5 sec
      *Should not be hitting much during this attack
      dg_affect %self% TO-HIT -100 25
      %echo% &RMana begins condensing around %self.name%!&0
      wait 5 sec
      %echo% &RThe platform starts shaking beneath your feet!&0
      wait 5 sec
      %echo% &rWaves of fire flood the room!&0
      %aoe% 150 fire
      wait 5 sec
      %echo% &rA barrage of sharp rocks falls from the ceiling!&0
      %aoe% 150 physical
      wait 5 sec
      %echo% &rThe floating motes of mana in the air transform into ethereal blades!&0
      %aoe% 100 magical
      %aoe% 50 physical
      wait 5 sec
      %echo% %self.name% sinks back into the lava...
    break
  done
end
*global cooldown
wait 10 sec
~
#18077
Fiend adds timer~
0 n 100
~
wait 30 sec
%echo% %self.name% burns out, disappearing with a puff of smoke!
%purge% %self%
~
#18078
Fiend adds battle~
0 k 20
~
makeuid fiend mob chained
if %random.3%<3
  %echo% %self.name% transfers some of its heat to %fiend.name%!
  %damage% %fiend% -100
  %damage% %self% 100
else
  %send% %actor% &rThere is a destructive explosion as %self.name% hurls itself suicidally at you!&0
  %echoaround% %actor% &rThere is a destructive explosion as %self.name% hurls itself suicidally at %actor.name%!&0
  %damage% %actor% 200 fire
  %aoe% 50 fire
  %damage% %self% 1000
end
wait 5 sec *Global cooldown
~
#18079
Fiend No Leave~
0 s 100
~
%send% %actor% You try to leave, but a wall of fire blocks your escape!
%echoaround% %actor% %actor.name% tries to leave, but a wall of fire blocks %actor.hisher% escape!
return 0
~
#18080
Fiend Immunities~
0 p 100
~
if !(%abilityname%==disarm)
  halt
end
%send% %actor% You cannot disarm %self.name% - %self.hisher% magic is innate!
return 0
~
#18081
Fiend No Flee~
0 c 0
flee~
%send% %actor% You turn to flee, but a wall of fire blocks your escape!
%echoaround% %actor% %actor.name% turns to flee, but a wall of fire blocks %actor.hisher% escape!
~
$
