#9501
Combat: Ice Shard~
0 k 15 1
L w 9501
~
* Generic combat script: Ice Shard
* Damage: 25% magic damage, 50% magic DoT for 30 seconds
* Effects: -(level/10) Dodge for 30 seconds
* Cooldown: 15 seconds
if %self.cooldown(9501)%
  halt
end
nop %self.set_cooldown(9501, 15)%
%send% %actor% ~%self% spits a bolt of icy magic at you, chilling you to the bone!
%echoaround% %actor% ~%self% spits a bolt of icy magic at ~%actor%, who starts shivering violently.
%damage% %actor% 25 magical
%dot% %actor% 50 30 magical 1
eval amnt (%self.level%/10)
if %amnt% > 0
  dg_affect %actor% DODGE -%amnt% 30
end
~
#9502
Combat: Snow Flurry~
0 k 15 1
L w 9502
~
* Generic combat script: Snow Flurry
* Effects: - (level/10) Dodge to all enemies for 15 seconds
* Cooldown: 15 seconds
if %self.cooldown(9502)%
  halt
end
nop %self.set_cooldown(9502, 15)%
%echo% ~%self% kicks up a flurry of snow!
eval room %self.room%
eval person %room.people%
while %person%
  eval test %%self.is_enemy(%person%)%%
  if %test%
    %send% %person% The snow gets in your eyes!
    * 7 ~ 13
    eval amnt (%self.level%/10)
    if %amnt% > 0
      dg_affect %person% TO-HIT -%amnt% 15
    end
  end
  eval person %person.next_in_room%
done
~
#9503
Combat: Bull Rush~
0 k 15 1
L w 9503
~
* Generic combat script: Bull Rush
* Damage: 50% physical
* Effects: Target stunned for 5 seconds
* Cooldown: 15 seconds
if %self.cooldown(9503)%
  halt
end
nop %self.set_cooldown(9503, 15)%
%echo% ~%self% puts ^%self% head down and charges ~%actor%!
wait 2 sec
if !%actor% || %actor.room% != %self.room% || %self.aff_flagged(IMMOBILIZED)% || %self.aff_flagged(STUNNED)%
  %echo% |%self% attack is interrupted.
  halt
end
%send% %actor% ~%self% crashes into you, leaving you briefly stunned!
%echoaround% %actor% ~%self% crashes into ~%actor%, stunning *%actor%!
if !%actor.aff_flagged(NO-STUN)%
  dg_affect %actor% STUNNED on 5
end
%damage% %actor% 50
~
#9504
Combat: Minor Heal~
0 k 15 1
L w 9504
~
* Generic combat script: Minor Heal
* Effects: 150% healing on self
* Cooldown: 15 seconds
if %self.cooldown(9504)%
  halt
end
nop %self.set_cooldown(9504, 15)%
%echo% ~%self% glows gently, and fights with renewed vitality!
%damage% %self% -150
~
#9505
Combat: Ankle Biter~
0 k 15 1
L w 9505
~
* Generic combat script: Ankle Biter
* Damage: 50% physical DoT for 15 seconds
* Effects: -1 Dexterity for 15 seconds
* Cooldown: 15 seconds
if %self.cooldown(9505)%
  halt
end
nop %self.set_cooldown(9505, 15)%
%send% %actor% ~%self% nips viciously at your ankles, drawing blood and slowing you down!
%echoaround% %actor% ~%self% nips viciously at |%actor% ankles, drawing blood and slowing *%actor% down!
%dot% %actor% 50 15 physical
dg_affect %actor% DEXTERITY -1 15
~
#9506
Combat: Net immobilize (for attack 38 net lash)~
0 k 12 3
L b 9506
L w 9506
L A 38
~
if !%hit% || %actor.aff_flagged(IMMUNE-PHYSICAL-DEBUFFS)% || %self.aff_flagged(DISARMED)%
  halt
end
set id %actor.id%
wait 0
if !%actor% || %actor.id% != %id% || %actor.dead% || %self.dead%
  halt
end
if %actor.affect(9506)%
  * already on me
  halt
end
dg_affect #9506 @%actor% %actor% IMMOBILIZED on 20
%load% mob 9506
~
#9507
Net immobilize cut free helper~
0 c 0 2
L f 9506
L w 9506
cut~
if !%actor.affect(9506)%
  return 0
  halt
end
if !%arg%
  %send% %actor% Cut what?
  return 1
  halt
elseif %arg% != net && %arg% != free
  %send% %actor% You can't cut that right now.
  return 1
  halt
end
set knife %actor.tool(knife)%
if !%knife%
  %send% %actor% You need a knife to cut yourself free.
  return 1
  halt
end
* looks ok
dg_affect #9506 %actor% off silent
%send% %actor% You use @%knife% to cut yourself free of the net.
%echoaround% %actor% ~%actor% uses @%knife% to cut *%person%self free of the net.
~
$
