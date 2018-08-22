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
if !%self.affect(eartharmor)%
  eartharmor
elseif !%actor.affect(entangle)%
  entangle
else
  skybrand
end
~
#16005
bone dust usage~
1 c 2
use~
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
0 k 75
~
if !%actor.affect(disarm)%
  disarm
elseif !%actor.affect(terrify)%
  terrify
else
  kick
end
~
#16008
nature buff up~
0 k 75
~
if !%self.affect(rejuvenat)%
  rejuvenate
elseif !%self.affect(counterspell)%
  counterspell
elseif !%self.affect(hasten)%
  hasten
else
  heal
end
~
#16009
transformative tooth~
1 c 2
implant~
set target %actor.char_target(%arg%)%
if !%target.mob_flagged(mountable)%
  %send% %actor% You are unable to implant %self.shortdesc% into %target.name%.
  unset target
  halt
else
  %send% %actor% You stab %self.shortdesc% into %target.name%'s neck and watch the mutation begin.
  %echoaround% %actor% As %actor% stabs %self.shortdesc% into %target.name%'s neck, a horrific transformation takes place.
  set beast_chance %random.100%
  if %beast_chance% == (6)
    eval monster_level %actor.level% + 13
    wait 1
    %load% mob 16012 %monster_level%
    %echo% You get a terrible feeling that something has gone entirely wrong.
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
$
