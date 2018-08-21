#12400
Reset rep on entry~
2 g 100
~
if %actor.is_pc%
  eval test %%visited_%actor.id%%%
  if !%test%
    set visited_%actor.id% 1
    global visited_%actor.id%
    * %send% %actor% You have not visited this instance before, so your reputation with [faction] has been reset to 0.
    nop %actor.set_reputation(12401, Neutral)%
  end
end
~
#12401
Exit cave~
1 c 4
exit leave~
%force% %actor% enter exit
~
#12402
free deckhand~
0 c 0
free~
if %actor.char_target(%arg%)% != %self%
  return 0
  halt
end
if %actor.has_reputation(12401,Liked)%
  %send% %actor% You'd rather not upset the goblins by doing that.
  halt
end
%send% %actor% You untie %self.name%.
%echoaround% %actor% %actor.name% unties %self.name%.
nop %actor.set_reputation(12401, Despised)%
%load% mob 12407
%purge% %self%
~
#12403
Goblin Pirate death~
0 f 100
~
dg_affect %self% BLIND off
* load new goblin?
switch %self.vnum%
  case 12403
    set vnum 12404
  break
  case 12404
    set vnum 12405
  break
  case 12405
  break
done
set room %self.room%
if %vnum%
  %load% mob %vnum%
  set summon %room.people%
  if %summon.vnum% == %vnum%
    if %self.mob_flagged(HARD)%
      nop %summon.add_mob_flag(HARD)%
    else
      nop %summon.remove_mob_flag(HARD)%
    end
    %echo% %summon.name% arrives!
  end
end
* lose rep
set person %room.people%
while %person%
  if %person.is_pc%
    if %self.vnum% != 12401
      set amount 1
      if %self.mob_flagged(HARD)%
        set amount 2
      end
      %send% %person% You loot %amount% %currency.12403(%amount%)% from %self.name%.
      nop %person.give_currency(12403, %amount%)%
    end
    nop %person.set_reputation(12401, Despised)%
  end
  set person %person.next_in_room%
done
~
#12404
Cove: Delayed Despawn~
1 f 0
~
%adventurecomplete%
return 0
%echo% %self.shortdesc% stands up on small green legs and sidles away.
%purge% %self%
~
#12405
Underwater Cave difficulty select~
1 c 4
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty (Normal or Hard).
  return 1
  halt
end
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set difficulty 1
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set difficulty 2
elseif group /= %arg%
  %send% %actor% You can't set this adventure to that difficulty...
  return 1
  halt
elseif boss /= %arg%
  %send% %actor% You can't set this adventure to that difficulty...
  return 1
  halt
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
set vnum 12403
while %vnum% <= 12420
  if %vnum% == 12407
    set vnum 12419
  end
  set mob %instance.mob(%vnum%)%
  if !%mob%
    * This was for debugging. We could do something about this.
    * Maybe just ignore it and keep on setting?
  else
    nop %mob.remove_mob_flag(HARD)%
    nop %mob.remove_mob_flag(GROUP)%
    if %difficulty% == 1
      * Then we don't need to do anything
    elseif %difficulty% == 2
      nop %mob.add_mob_flag(HARD)%
    elseif %difficulty% == 3
      nop %mob.add_mob_flag(GROUP)%
    elseif %difficulty% == 4
      nop %mob.add_mob_flag(HARD)%
      nop %mob.add_mob_flag(GROUP)%
    end
  end
  eval vnum %vnum% + 1
done
%send% %actor% You set the difficulty...
%echoaround% %actor% %actor.name% sets the difficulty...
%echo% You discover a passage hidden behind %self.shortdesc%.
set newroom i12401
%door% %self.room% north room %newroom%
%load% obj 12461 room
%purge% %self%
~
#12406
breath messaging~
1 n 100
~
set actor %self.carried_by%
wait 1
if %actor.varexists(breath)%
  set breath %actor.breath%
  if %breath% == 10 && %instance.mob(12406)% && %actor.has_reputation(12401, Liked)%
    %send% %actor% The sea hag's magic is running out! You need to find some air to let it recharge!
  end
  if %breath% == 0
    %send% %actor% You are about to drown!
  elseif %breath% == 1
    %send% %actor% You cannot hold your breath much longer! You should find air, and soon!
  elseif %breath% < 0
    %send% %actor% &rYou are drowning!
  elseif %breath% <= 5
    %send% %actor% You can't hold your breath much longer... You think you could swim for another %breath% rooms.
  end
end
%purge% %self%
~
#12407
Goblin cove: Collect seaweed~
2 c 0
pick forage~
if %depleted%
  %send% %actor% You can't find any seaweed here.
  halt
end
set depleted 1
global depleted
%load% obj 12407 %actor% inv
set item %actor.inventory(12407)%
%send% %actor% Searching the cavern, you find %item.shortdesc%.
%echoaround% %actor% %actor.name% searches the cavern and finds %item.shortdesc%.
~
#12408
Goblin Cove trash spawner~
1 n 100
~
* Ensure no mobs here
set ch %self.room.people%
set found 0
while %ch% && !%found%
  if (%ch.is_npc%)
    eval found %found% + 1
  end
  set ch %ch.next_in_room%
done
if (%found% == 0)
  switch %random.4%
    case 1
      %load% mob 12408
    break
    case 2
      %load% mob 12409
    break
    case 3
      %load% mob 12410
    break
    case 4
      %load% mob 12411
    break
  done
end
wait 1
%purge% %self%
~
#12409
Underwater cave mob block~
0 s 100
~
if (%actor.nohassle% || %direction% == south || %actor.on_quest(12406)% || %actor.on_quest(12407)% || %actor.on_quest(12408)% || %actor.on_quest(12409)%)
  halt
end
%send% %actor% %self.name% won't let you pass!
return 0
~
#12410
Track detects higher template~
2 c 0
track~
eval tofind %room.template%+1
if (!%actor.ability(Track)% || !%actor.ability(Navigation)%)
  * Fail through to ability message
  return 0
  halt
end
* It's never south -- that's always backtrack
set north %room.north(room)%
set east %room.east(room)%
set west %room.west(room)%
set south %room.south(room)%
set northeast %room.northeast(room)%
set northwest %room.northwest(room)%
set southeast %room.southeast(room)%
set southwest %room.southwest(room)%
set result 0
if (%north% && %north.template% >= %tofind%)
  set result north
elseif (%east% && %east.template% >= %tofind%)
  set result east
elseif (%west% && %west.template% >= %tofind%)
  set result west
elseif (%northeast% && %northeast.template% >= %tofind%)
  set result northeast
elseif (%northwest% && %northwest.template% >= %tofind%)
  set result northwest
elseif (%southeast% && %southeast.template% >= %tofind%)
  set result southeast
elseif (%southwest% && %southwest.template% >= %tofind%)
  set result southwest
elseif (%south% && %south.template% >= %tofind%)
  set result south
end
if %result%
  %send% %actor% You sense a trail to the %result%!
  return 1
end
~
#12411
quest complete: no kill~
2 v 0
~
set vnum 12401
while %vnum% <= 12406
  set mob %instance.mob(%vnum%)%
  if %mob%
    dg_affect %mob% !ATTACK on -1
  end
  eval vnum %vnum% + 1
done
~
#12412
Goblin Cove loot load boe/bop~
1 n 100
~
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
end
if !%actor%
  halt
end
if %actor% && %actor.is_pc%
  * Item was crafted
  if %self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if !%self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
  * Default flag is BOP so need to unbind when setting BOE
  nop %self.bind(nobody)%
else
  * Item was probably dropped
  if !%self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if %self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
end
~
#12413
Underwater~
2 bgw 100
~
if !%actor%
  set person %room.people%
  while %person%
    if %person.is_pc%
      if %person.varexists(breath)%
        if %person.breath% < 0
          if %person.is_god% || %person.is_immortal% || %person.health% < 0
            halt
          end
          %send% %person% &rYou are drowning!
          eval amount (%person.breath%) * (-250)
          %damage% %person% %amount%
        end
      end
    end
    set person %person.next_in_room%
  done
  halt
end
if %actor.is_npc%
  halt
end
if !%actor.varexists(breath)%
  set breath 3
  remote breath %actor.id%
end
set breath %actor.breath%
eval breath %breath% - 1
if %breath% < 0
  eval amount (%breath%) * (-250)
  %damage% %actor% %amount% direct
end
remote breath %actor.id%
%load% obj 12409 %actor% inv
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12400)%
end
~
#12414
Air Supply~
2 g 100
~
* change based on quest, etc
set breath 8
if %instance.mob(12406)% && %actor.has_reputation(12401, Liked)%
  set breath 45
end
if %actor.varexists(breath)%
  if %actor.breath% < %breath%
    %send% %actor% You take a deep breath of air, refreshing your air supply.
  end
end
remote breath %actor.id%
~
#12415
Maelstrom pull~
2 g 100
~
wait 5
%echo% The water is starting to swirl...
%echo% If you don't get to shore, you may be pulled under.
wait 8 sec
%echo% The maelstrom pulls you beneath the water!
%teleport% all i12405
~
#12416
Maelstrom~
2 bgw 100
~
wait 5 sec
%echo% You are tossed to and fro by the raging waters!
wait 5 sec
%echo% The powerful current pulls you deeper and deeper beneath the surface!
wait 5 sec
%echo% After an indeterminate amount of time, you find yourself in a small pocket of air...
set person %room.people%
while %person%
  set next_person %person.next_in_room%
  %echoaround% %person% %person.name% washes up nearby.
  %teleport% %person% i12406
  %force% %person% look
  %echoaround% %person% %person.name% washes up nearby.
  set person %next_person%
done
~
#12417
Parrot script~
0 dt 0
*~
if %random.2% == 2
  wait 5
  say Awk! %speech%
end
~
#12418
Goblin Pirate: Flintlock Pistol~
0 k 25
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
set target %random.enemy%
if !%target%
  set target %actor%
end
%send% %target% %self.name% draws a flintlock pistol and takes aim at you!
%echoaround% %target% %self.name% draws a flintlock pistol and takes aim at %target.name%!
wait 3 sec
if !%target%
  halt
end
if !%self.is_enemy(%target%)%
  halt
end
%send% %target% &r%self.name% shoots you with %self.hisher% pistol!
%echoaround% %target% %self.name% shoots %target.name% with %self.hisher% pistol!
%damage% %target% 200 physical
wait 5
%echo% %self.name% blows on the barrel of %self.hisher% pistol and tosses it carelessly aside.
~
#12419
Goblin Pirate: Ankle Stab~
0 k 33
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
%send% %actor% &r%self.name% stabs you in the ankle with %self.hisher% cutlass!
%echoaround% %actor% %self.name% stabs %actor.name% in the ankle with %self.hisher% cutlass!
%damage% %actor% 50 physical
dg_affect #12419 %actor% SLOW on 5
dg_affect #12419 %actor% DODGE -10 5
~
#12420
Goblin Pirate: Grog~
0 k 50
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
%echo% %self.name% pulls a canteen of goblin grog from somewhere and tips %self.hisher% head back, drinking deeply.
%echo% %self.name% fights with renewed strength!
%heal% %self% health 75
dg_affect #12420 %self% HASTE on 10
~
#12421
Goblin Pirate: Blind~
0 k 100
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
set target %random.enemy%
if !%target%
  set target %actor%
end
%send% %target% %self.name% grabs a handful of sand from one of %self.hisher% pockets and tosses it in your eyes!
%echoaround% %target% %self.name% grabs a handful of sand from one of %self.hisher% pockets and tosses it in %target.name%'s eyes!
dg_affect #12421 %target% BLIND on 5
~
#12422
Goblin Pirate Captain: Parrot Attack~
0 k 100
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
say Get 'em, Polly!
wait 2 sec
if %random.20% == 20
  %echo% %self.name%'s parrot says, 'Awk! Polly want a cracker!'
  wait 5 sec
  say Arr! Curse ye, mutinous fowl!
else
  %send% %actor% &r%self.name%'s parrot dive-bombs you, forcing you to cover your eyes!
  %echoaround% %actor% %self.name%'s parrot dive-bombs %actor.name%, forcing %actor.himher% to cover %actor.hisher% eyes!
  %damage% %actor% 25 physical
  dg_affect #12422 %actor% BLIND on 5
  %dot% #12423 %actor% 50 10 physical
end
~
#12423
Hydra: Aggravated Assault~
0 k 25
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
eval health_percent (100 * %self.health%) / %self.maxhealth%
eval extra_heads %health_percent% / 10
eval heads 15 - %extra_heads%
if %heads% == 1
  %send% %actor% &rOne of %self.name%'s serpentine heads snaps at you!
  %echoaround% %actor% One of %self.name%'s serpentine heads snaps at %actor.name%!
  %damage% %actor% 100 physical
elseif %heads% == 2
  %send% %actor% &rA pair of %self.name%'s snake heads snap at you!
  %echoaround% %actor% A pair of %self.name%'s snake heads snap at %actor.name%!
  %damage% %actor% 150 physical
elseif %heads% < 9
  %send% %actor% &r%heads% of %self.name%'s heads batter you from all sides!
  %echoaround% %actor% %actor.name% is attacked from all sides by %heads% of %self.name%'s heads!
  eval amount 50+%heads%*25
  %damage% %actor% %amount% physical
elseif %heads% >= 9
  %echo% &r%self.name%'s %heads% heads lash out in all directions!
  eval amount 10+%heads%*10
  %aoe% %amount% physical
end
~
#12424
Hydra: Poison Bite~
0 k 33
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
%send% %actor% One of &r%self.name%'s serpentine heads snaps out and sinks its venomous fangs into your side!
%echoaround% %actor% One of %self.name%'s serpentine heads snaps out and sinks its venomous fangs into %actor.name%'s side!
%damage% %actor% 100 physical
%dot% #12424 %actor% 100 15 poison
~
#12425
Hydra: Regeneration~
0 k 50
~
if %self.cooldown(12400)%
  halt
end
if %self.health% == %self.maxhealth%
  halt
end
nop %self.set_cooldown(12400, 30)%
%echo% %self.name%'s wounds suddenly begin to close!
%damage% %self% -250
dg_affect #12425 %self% HEAL-OVER-TIME %self.level% 15
~
#12426
Hydra: Crushing Grip~
0 k 100
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
eval hitpercent %self.health% * 100 / %self.maxhealth%
if %self.mob_flagged(HARD)% && %hitpercent% < 80
  set keep_attacking 1
end
%send% %actor% %self.name%'s head moves suddenly, and its snake-like neck wraps around you!
%send% %actor% (Type 'struggle' to break free.)
%echoaround% %actor% %self.name%'s head moves suddenly, and its snake-like neck wraps around %actor.name%!
set struggle_counter 0
remote struggle_counter %actor.id%
dg_affect #12430 %actor% HARD-STUNNED on 20
if !%keep_attacking%
  dg_affect #12428 %self% HARD-STUNNED on 20
end
while %actor.affect(12430)%
  %send% %actor% &r%self.name% constricts and crushes you!
  %damage% %actor% 50 physical
  wait 5 sec
done
~
#12427
Scylla: Hydrokinesis~
0 k 25
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
%echo% %self.name% is surrounded by a shimmering blue light!
%echo% %self.name% is healed and strengthened!
%heal% %self% health 75
eval amount %self.level%/10
dg_affect #12427 %self% BONUS-PHYSICAL %amount% 30
dg_affect #12427 %self% BONUS-MAGICAL %amount% 30
dg_affect #12427 %self% HASTE on 30
~
#12428
Scylla: Chomp~
0 k 33
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
%echo% &r%self.name%'s canine heads lash out, snarling and gnashing.
set person %self.room.people%
while %person%
  if %person.is_npc% && %person.mob_flagged(FAMILIAR)%
    %echo% %self.name%'s heads tear a chunk out of %person.name%!
    %damage% %person% 350
  elseif %person% != %self%
    %damage% %person% 100
  end
  set person %person.next_in_room%
done
~
#12429
Scylla: Pressure Wave~
0 k 50
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
%echo% %self.name% starts swimming rapidly in a tight circle...
wait 3 sec
%echo% &r%self.name%'s tentacle-tails lash out at you, blasting you with a wave of high-pressure water!
%aoe% 50 physical
set person %self.room.people%
while %person%
  if %person.is_enemy(%self%)%
    dg_affect #12429 %person% SLOW on 20
    eval amount %self.level% / 5
    dg_affect #12429 %person% RESIST-PHYSICAL -%amount% 20
    dg_affect #12429 %person% RESIST-MAGICAL -%amount% 20
  end
  set person %person.next_in_room%
done
~
#12430
Scylla: Crushing Grip~
0 k 100
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
%send% %actor% %self.name%'s tentacles lash out and wrap themselves around you in a crushing embrace!
%send% %actor% (Type 'struggle' to break free.)
%echoaround% %actor% %self.name%'s tentacles lash out and wrap themselves around %actor.name%.
set struggle_counter 0
remote struggle_counter %actor.id%
dg_affect #12430 %actor% HARD-STUNNED on 20
dg_affect #12428 %self% HARD-STUNNED on 20
while %actor.affect(12430)%
  %send% %actor% &r%self.name% crushes you in %self.hisher% grip!
  %damage% %actor% 150 physical
  wait 4 sec
done
~
#12431
Hydra/Scylla Grip Struggle~
0 c 0
struggle~
set break_free_at 2
if !%actor.affect(12430)%
  return 0
  halt
end
if %actor.cooldown(12431)%
  %send% %actor% You need to gather your strength.
  halt
end
nop %actor.set_cooldown(12431, 3)%
if !%actor.varexists(struggle_counter)%
  set struggle_counter 0
  remote struggle_counter %actor.id%
else
  set struggle_counter %actor.struggle_counter%
end
eval struggle_counter %struggle_counter% + 1
if %struggle_counter% >= %break_free_at%
  %send% %actor% You break free of %self.name%'s grip!
  %echoaround% %actor% %actor.name% breaks free of %self.name%'s grip!
  dg_affect #12430 %actor% off
  dg_affect #12428 %self% off
  rdelete struggle_counter %actor.id%
  halt
else
  %send% %actor% You struggle in %self.name%'s grip, but fail to break free.
  %echoaround% %actor% %actor.name% struggles in %self.name%'s grip!
  remote struggle_counter %actor.id%
  halt
end
~
#12432
Scylla dance phase~
0 l 25
~
if %self.varexists(phase)%
  halt
end
dg_affect #12432 %self% IMMUNE-DAMAGE on -1
dg_affect #12432 %self% HARD-STUNNED on -1
set phase 2
remote phase %self.id%
%echo% %self.name% holds out %self.hisher% arm, and a sword made of ice appears in %self.hisher% outstretched hand!
%echo% Get ready to dodge! Type 'up', 'down', 'left' and 'right' to evade %self.name%'s attacks.
wait 2 sec
set cycle 1
while %cycle% <= 4
  wait 3 sec
  if %cycle% == 1
    %echo% &Y%self.name% charges at you, aiming a wide slash at your head!
  elseif %cycle% == 2
    %echo% &Y%self.name% draws back %self.hisher% sword for a thrust!
  elseif %cycle% == 3
    %echo% &Y%self.name% raises %self.hisher% sword overhead for a vertical slash!
  elseif %cycle% == 4
    %echo% &Y%self.name% hurls %self.hisher% sword at the stone floor of the cave!
  end
  set running 1
  remote running %self.id%
  wait 6 sec
  set running 0
  remote running %self.id%
  set person %self.room.people%
  if %cycle% == 1
    %echo% %self.name%'s sword launches a barrage of ice spikes as %self.heshe% slashes!
  elseif %cycle% == 2
    %echo% %self.name% unleashes a blindingly fast flurry of stabs!
  elseif %cycle% == 3
    %echo% %self.name%'s sword launches a barrage of ice spikes as %self.heshe% slashes!
  elseif %cycle% == 4
    %echo% %self.name%'s sword detonates in a blast of freezing cold!
  end
  while %person%
    if %person.is_pc%
      set act %self.varexists(last_action_%person.id%)%
      if %act%
        eval act %%self.last_action_%person.id%%%
      end
      if (%cycle% == 1 && %act% == down) || ((%cycle% == 2 || %cycle% == 3) && (%act% == left || %act% == right)) || (%cycle% == 4 && %act% == up)
        %send% %person% You barely avoid %self.name%'s attack.
        %echoaround% %person% %person.name% barely avoids %self.name%'s attack.
      else
        %send% %person% &rYou are struck by %self.name%'s attack!
        %echoaround% %person% %person.name% is struck by %self.name%'s attack!
        set test %person.affect(12433)%
        if %test%
          %send% %person% &rYou are encased in a block of ice!
          %echoaround% %person% %person.name% is encased in a block of ice!
          %damage% %person% 9999 magical
        else
          dg_affect #12433 %person% DODGE -25 30
          dg_affect #12433 %person% TO-HIT -25 30
          dg_affect #12433 %person% SLOW on 30
          %send% %person% &rYou feel deathly cold...
          %echoaround% %person% %person.name% starts shivering violently.
          %damage% %person% 250 magical
        end
      end
    end
    set person %person.next_in_room%
  done
  eval cycle %cycle% + 1
done
wait 1 sec
dg_affect #12432 %self% off
if %self.fighting%
  set phase 3
  remote phase %self.id%
else
  %restore% %self%
  rdelete phase %self.id%
end
~
#12433
Scylla: Dance Commands~
0 c 0
up down left right~
if %self.varexists(running)%
  if %self.running%
    if up /= %cmd%
      set last_action_%actor.id% up
      %send% %actor% You quickly swim up.
      %echoaround% %actor% %actor.name% quickly swims up.
    elseif down /= %cmd%
      set last_action_%actor.id% down
      %send% %actor% You quickly dive down.
      %echoaround% %actor% %actor.name% quickly dives down.
    elseif left /= %cmd%
      set last_action_%actor.id% left
      %send% %actor% You lean to the left.
      %echoaround% %actor% %actor.name% leans to the left.
    elseif right /= %cmd%
      set last_action_%actor.id% right
      %send% %actor% You lean to the right.
      %echoaround% %actor% %actor.name% leans to the right.
    end
    eval test %%last_action_%actor.id%%%
    if %test%
      remote last_action_%actor.id% %self.id%
    end
    halt
  end
end
%send% %actor% You don't need to do that right now.
~
#12434
Scylla: Phase reset~
0 bw 100
~
if !%self.fighting% && (%self.varexists(phase)% || %self.affect(12432)%)
  %restore% %self%
  dg_affect #12432 %self% off
  rdelete phase %self.id%
end
~
#12435
Fathma: Drowning Curse~
0 k 25
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
%echo% %self.name% chants and waves %self.hisher% arms!
wait 5 sec
if %actor.trigger_counterspell%
  %send% %actor% %self.name% points at you, but nothing seems to happen.
  %echaround% %actor% %self.name% points at %actor.name%, but nothing seems to happen.
else
  %send% %actor% %self.name% points at you, and you feel your lungs begin to fill with water!
  %dot% #12435 %actor% 50 30 magical 5
  wait 5 sec
  while %actor.affect(12435)%
    %send% %actor% %self.name%'s curse strengthens!
    %dot% #12435 %actor% 50 30 magical 5
    wait 5 sec
  done
end
~
#12436
Fathma: Heal Self~
0 k 33
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
%echo% %self.name% chants and waves %self.hisher% arms!
wait 5 sec
%echo% %self.name% is restored!
%heal% %self% health 100
%heal% %self% debuffs
~
#12437
Fathma: Familiar~
0 k 50
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
eval vnum 12407 + %random.4%
%load% mob %vnum% ally
set summon %self.room.people%
if %summon.vnum% == %vnum%
  %echo% %self.name% sends up a jet of sparkling blue mana and %summon.name% appears!
  nop %summon.add_mob_flag(UNDEAD)%
  nop %summon.add_mob_flag(!LOOT)%
  nop %summon.add_mob_flag(SPAWNED)%
  nop %summon.add_mob_flag(BRING-A-FRIEND)%
  attach 12440 %summon.id%
  %force% %summon% %aggro% %actor%
end
~
#12438
Fathma: Ice Bolt~
0 k 100
~
if %self.cooldown(12400)%
  halt
end
nop %self.set_cooldown(12400, 30)%
set target %random.enemy%
if !%target%
  set target %actor%
end
%send% %target% &r%self.name% blasts you with a ball of icy energy.
%echoaround% %target% %self.name% blasts %target.name% with a ball of icy energy.
%damage% %target% 100 magical
dg_affect #12438 %target% SLOW on 10
%dot% #12438 %target% 100 10 magical
~
#12439
Underwater boss death: portal to exit~
0 f 100
~
%echo% As you slay %self.name%, a portal opens nearby.
set loc %instance.location%
set vnum %loc.vnum%
%load% obj 12402 room
set portal %self.room.contents%
if %portal.vnum% == 12402
  nop %portal.val0(%vnum%)%
end
~
#12440
Fathma summon timer~
0 bknw 100
~
wait 20 sec
%purge% %self% $n vanishes in a burst of sparkling blue mana.
~
#12441
Golden Goblin reputation gate~
0 s 100
~
if %self.vnum% == 12406
  if %actor.has_reputation(12401, Liked)%
    %send% %actor% As you leave, %self.name% mutters some magic words.
    %echoaround% %actor% As %actor.name% leaves, %self.name% mutters some magic words.
    * In case they just finished the quest, reset their breath now
    set breath 45
    remote breath %actor.id%
  end
  if %self.aff_flagged(!ATTACK)%
    halt
  end
end
if (%actor.nohassle% || %actor.has_reputation(12401, Liked)% || (%direction% == fore && %actor.has_reputation(12401, Neutral)%))
  halt
end
%send% %actor% %self.name% won't let you pass!
return 0
~
#12442
Fathma death~
0 f 100
~
dg_affect %self% BLIND off
set vnum 12414
while %vnum% <= 12416
  set mob %instance.mob(%vnum%)%
  if %mob%
    set room %mob.room%
    %purge% %mob% $n vanishes.
    %load% obj 12408 %room%
  end
  eval vnum %vnum% + 1
done
* lose rep
set person %self.room.people%
while %person%
  if %person.is_pc%
    set amount 1
    if %self.mob_flagged(HARD)%
      set amount 2
    end
    %send% %person% You loot %amount% %currency.12403(%amount%)% from %self.name%.
    nop %person.give_currency(12403, %amount%)%
    nop %person.set_reputation(12401, Despised)%
  end
  set person %person.next_in_room%
done
~
#12443
Golden Goblin underwater miniboss spawner~
1 n 100
~
eval mob_1 12413 + %random.3%
if %mob_1% == 12414
  if %random.2%==2
    set mob_2 12415
    set mob_3 12416
  else
    set mob_2 12416
    set mob_3 12415
  end
elseif %mob_1% == 12415
  if %random.2% == 2
    set mob_2 12414
    set mob_3 12416
  else
    set mob_2 12416
    set mob_3 12414
  end
else
  if %random.2% == 2
    set mob_2 12415
    set mob_3 12414
  else
    set mob_2 12414
    set mob_3 12415
  end
end
%at% i12409 %load% mob %mob_1%
%at% i12414 %load% mob %mob_2%
%at% i12417 %load% mob %mob_3%
%purge% %self%
~
#12444
Fathma portal-to-surface~
0 v 0
~
return 1
if %actor.completed_quest_instance(12406)% || %questvnum% == 12406
  set done_1 1
end
if %actor.completed_quest_instance(12407)% || %questvnum% == 12407
  set done_2 1
end
if %actor.completed_quest_instance(12408)% || %questvnum% == 12408
  set done_3 1
end
if %actor.completed_quest_instance(12409)% || %questvnum% == 12409
  set done_4 1
end
if %done_1% && %done_2% && %done_3% && %done_4%
  * already a portal?
  set room %self.room%
  set obj %room.contents%
  while %obj%
    if %obj.vnum% == 12402
      halt
    end
    set obj %obj.next_in_list%
  done
  wait 1
  %echo% %self.name% waves %self.hisher% hand and creates a portal!
  set loc %instance.location%
  set vnum %loc.vnum%
  %load% obj 12402 room
  set portal %room.contents%
  if %portal.vnum% == 12402
    nop %portal.val0(%vnum%)%
  end
end
~
#12445
GG Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12400)%
end
~
$
