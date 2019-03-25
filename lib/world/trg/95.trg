#9501
Combat: Ice Shard~
0 k 15
~
* Generic combat script: Ice Shard
* Damage: 25% magic damage, 50% magic DoT for 30 seconds
* Effects: -(level/10) Dodge for 30 seconds
* Cooldown: 15 seconds
if %self.cooldown(9501)%
  halt
end
nop %self.set_cooldown(9501, 15)%
%send% %actor% %self.name% spits a bolt of icy magic at you, chilling you to the bone!
%echoaround% %actor% %self.name% spits a bolt of icy magic at %actor.name%, who starts shivering violently.
%damage% %actor% 25 magical
%dot% %actor% 50 30 magical 1
eval amnt (%self.level%/10)
if %amnt% > 0
  dg_affect %actor% DODGE -%amnt% 30
end
~
#9502
Combat: Snow Flurry~
0 k 15
~
* Generic combat script: Snow Flurry
* Effects: - (level/10) Dodge to all enemies for 15 seconds
* Cooldown: 15 seconds
if %self.cooldown(9502)%
  halt
end
nop %self.set_cooldown(9502, 15)%
%echo% %self.name% kicks up a flurry of snow!
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
0 k 15
~
* Generic combat script: Bull Rush
* Damage: 50% physical
* Effects: Target stunned for 5 seconds
* Cooldown: 15 seconds
if %self.cooldown(9503)%
  halt
end
nop %self.set_cooldown(9503, 15)%
%send% %actor% %self.name% puts %self.hisher% head down and charges you!
%echoaround% %actor% %self.name% puts %self.hisher% head down and charges %actor.name%!
wait 2 sec
if !%actor% || %actor.room% != %self.room% || %self.aff_flagged(ENTANGLED)% || %self.aff_flagged(STUNNED)%
  %echo% %self.name%'s attack is interrupted.
  halt
end
%send% %actor% %self.name% crashes into you, leaving you briefly stunned!
%echoaround% %actor% %self.name% crashes into %actor.name%, stunning %actor.himher%!
if !%actor.aff_flagged(!STUN)%
  dg_affect %actor% STUNNED on 5
end
%damage% %actor% 50
~
#9504
Combat: Minor Heal~
0 k 15
~
* Generic combat script: Minor Heal
* Effects: 150% healing on self
* Cooldown: 15 seconds
if %self.cooldown(9504)%
  halt
end
nop %self.set_cooldown(9504, 15)%
%echo% %self.name% glows gently, and fights with renewed vitality!
%damage% %self% -150
~
#9505
Combat: Ankle Biter~
0 k 15
~
* Generic combat script: Ankle Biter
* Damage: 50% physical DoT for 15 seconds
* Effects: -1 Dexterity for 15 seconds
* Cooldown: 15 seconds
if %self.cooldown(9505)%
  halt
end
nop %self.set_cooldown(9505, 15)%
%send% %actor% %self.name% nips viciously at your ankles, drawing blood and slowing you down!
%echoaround% %actor% %self.name% nips viciously at %actor.name%'s ankles, drawing blood and slowing %actor.himher% down!
%dot% %actor% 50 15 physical
dg_affect %actor% DEXTERITY -1 15
~
$
