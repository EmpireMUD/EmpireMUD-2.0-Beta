#18300
Pharaoh combat~
0 k 100
~
eval heroic_mode %self.mob_flagged(GROUP)%
* Count combat script cycles until enrage
* 1 cycle should be 30 seconds?
if %heroic_mode%
  * Start scaling up after 5 minutes, game over at 15 minutes
  eval soft_enrage_cycles 10
  eval hard_enrage_cycles 30
else
  * Start scaling up after 5 minutes, game over at 15 minutes
  eval soft_enrage_cycles 10
  eval hard_enrage_cycles 30
end
eval enrage_counter 0
eval enraged 0
if %self.varexists(enrage_counter)%
  eval enrage_counter %self.enrage_counter%
end
eval enrage_counter %enrage_counter%+1
if %enrage_counter% >= %soft_enrage_cycles%
  * Start increasing damage
  eval enraged 1
  if %enrage_counter% > %hard_enrage_cycles%
    eval enraged 2
  end
  * Enrage messages
  if %enrage_counter% == %soft_enrage_cycles%
    %echo% %self.name%'s eyes start glowing brighter!
    %echo% You'd better return %self.himher% to the grave, quickly!
  end
  if %enrage_counter% == %hard_enrage_cycles%
    %echo% %self.name% floats into the air!
    shout I alone should rule these lands, as a living god!
  end
end
remote enrage_counter %self.id%
* Enrage effects:
if %enraged%
  if %enraged% == 2
    * Stops using normal script attacks, just spams this every hit
    %echo% %self.name% unleashes an unstoppable blast of energy, bringing stone blocks crashing down from the ceiling!
    %aoe% 1000 direct
    halt
  end
  * Don't always show the message or it would be kinda spammy
  if %random.4% == 4
    %echo% %self.name%'s power grows!
  end
  dg_affect %self% BONUS-PHYSICAL 50 3600
end
* Start of regular combat script:
switch %random.4%
  * Summon
  case 1
    %echo% %self.name% makes a mystical gesture!
    eval room %self.room%
    if %heroic_mode%
      * Scarab swarm
      %load% mob 18302 ally
      eval summon %room.people%
      if %summon.vnum% == 18302
        %echo% %self.name% summons %summon.name%, and directs them to attack %actor.name%!
        %force% %summon% %aggro% %actor%
      end
    else
      * Jackal
      %load% mob 18301 ally
      eval summon %room.people%
      if %summon.vnum% == 18301
        %echo% %self.name% summons %summon.name%, and directs it to attack %actor.name%!
        %force% %summon% %aggro% %actor%
      end
    end
    wait 30 sec
  break
  * Rising Waters
  case 2
    %echo% %self.name% raises %self.hisher% hands!
    %echo% &AWater starts to flood the chamber! Swim for your life!&0
    * Give the group time to type 'swim' (if they're going to)
    eval running 1
    remote running %self.id%
    wait 10 sec
    eval running 0
    remote running %self.id%
    %echo% &AWater floods the chamber!
    eval room %self.room%
    eval person %room.people%
    while %person%
      if %person.is_pc%
        if %person.health% <= 0
          * Finish them off
          %send% %person% You are pummeled by raging waters...
          %damage% %person% 100 physical
        end
        eval test %%self.varexists(swimming_%person.id%)%%
        if %test%
          eval test %%self.swimming_%person.id%%%
        end
        if !%test%
          %send% %person% &rYou are drowned by the rising waters!
          %echoaround% %person% %person.name% sinks beneath the rising waters!
          if %heroic_mode%
            %damage% %person% 750 direct
          else
            %damage% %person% 500 direct
          end
        else
          %send% %person% You barely keep your head above the water!
          %echoaround% %person% %person.name% barely keeps %person.hisher% head above the water!
          unset swimming_%person.id%
        end
      end
      eval person %person.next_in_room%
    done
    %echo% %self.name% looks rejuvenated by the water!
    if %heroic_mode%
      %damage% %self% -2000
    else
      %damage% %self% -300
    end
    wait 3 sec
    %echo% The water level starts to lower...
    wait 17 sec
  break
  * Sandstorm
  case 3
    %echo% %self.name% makes a sweeping gesture, and the wind picks up!
    wait 5 sec
    %echo% &rA howling tornado of sand fills the chamber, blinding and slashing at everyone!
    set cycle 1
    while %cycle% <= 4
      eval room %self.room%
      eval person %room.people%
      while %person%
        eval check %%self.is_enemy(%person%)%%
        if %check%
          if %cycle% == 1
            dg_affect %person% BLIND on 20
          end
          %damage% %person% 50 physical
        end
        eval person %person.next_in_room%
      done
      wait 5 sec
      eval cycle %cycle% + 1
      if %cycle% <= 4
        %echo% &rThe sandstorm rages on!
      end
    done
    wait 5 sec
    %echo% The sandstorm dies down...
  break
  * Bandage bondage
  case 4
    %send% %actor% %self.name%'s bandages unfurl and lash out like tentacles, wrapping around you!
    %echoaround% %actor% %self.name%'s bandages unfurl and lash out like tentacles, wrapping around %actor.name%!
    if %heroic_mode%
      set max_cycles 5
    else
      set max_cycles 2
    end
    eval duration %max_cycles%*5
    dg_affect %actor% STUNNED on %duration%
    set cycle 1
    while %cycle% <= %max_cycles%
      if %actor.health% > -10
        %send% %actor% &r%self.name%'s bandages tighten around you, trying to squeeze the life out of you!
        %echoaround% %actor% %self.name%'s bandages tighten around %actor.name%!
        %damage% %actor% 200 physical
        wait 5 sec
      end
      eval cycle %cycle% + 1
    done
    %send% %actor% %self.name%'s bandages release you.
    %echoaround% %actor% %self.name%'s bandages release %actor.name%.
    eval time_left 30 - %duration%
    wait %time_left% sec
  break
done
~
#18301
Pyramid boss summon timer~
0 n 100
~
wait 30 sec
switch %self.vnum%
  default
    %echo% %self.name% vanishes.
  break
done
%purge% %self%
~
#18302
Enrage Buff/Counter Reset~
0 bw 25
~
if !%self.fighting% && %self.varexists(enrage_counter)%
  if %self.enrage_counter% == 0
    halt
  end
  dg_affect %self% BONUS-PHYSICAL off 1
  dg_affect %self% BONUS-MAGICAL off 1
  set enrage_counter 0
  remote enrage_counter %self.id%
  %restore% %self%
  %echo% %self.name% settles down to rest.
end
~
#18303
Detect swim~
0 c 0
swim~
if !%self.varexists(running)%
  set running 0
  remote running %self.id%
end
if !%self.running%
  %send% %actor% There's nothing to swim in right now.
  return 1
  halt
end
%send% %actor% You start swimming.
%echoaround% %actor% %actor.name% starts swimming.
dg_affect %actor% STUNNED on 10
set swimming_%actor.id% 1
remote swimming_%actor.id% %self.id%
~
#18304
Pyramid difficulty selector~
1 c 4
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty.
  return 1
  halt
end
* TODO: Check nobody's in the adventure before changing difficulty
if hard /= %arg%
  %echo% Setting difficulty to Hard...
  eval difficulty 2
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  eval difficulty 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  eval difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
eval vnum 18300
while %vnum% <= 18300
  eval mob %%instance.mob(%vnum%)%%
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
%send% %actor% You touch one of the symbols on the wall, and a section of the wall slowly slides into the floor...
%echoaround% %actor% %actor.name% touches one of the symbols on the wall, and a section of the wall slowly slides into the floor...
eval newroom i18302
eval exitroom i18300
if %exitroom%
  %door% %exitroom% down room %newroom%
end
eval room %self.room%
eval person %room.people%
while %person%
  eval next_person %person.next_in_room%
  %teleport% %person% %newroom%
  eval person %next_person%
done
otimer 24
~
#18305
Pyramid delayed despawn~
1 f 0
~
%adventurecomplete%
~
#18306
Pharaoh: Block flee~
0 c 0
flee~
%send% %actor% You can't flee from %self.name%!
return 1
~
#18307
Pyramid bonus loot replacer~
1 n 100
~
eval actor %self.carried_by%
if %actor%
  if %actor.mob_flagged(GROUP)% && %actor.mob_flagged(HARD)%
    * Roll for mount
    eval percent_roll %random.100%
    if %percent_roll% <= 10
      * Sobek
      eval vnum 18300
    else
      eval percent_roll %percent_roll% - 10
      if %percent_roll% <= 10
        * Bastet
        eval vnum 18301
      else
        eval percent_roll %percent_roll% - 10
        if %percent_roll% <= 10
          * Anubis
          eval vnum 18302
        else
          eval percent_roll %percent_roll% - 10
          if %percent_roll% <= 10
            * Horus
            eval vnum 18303
          else
            eval percent_roll %percent_roll% - 10
            if %percent_roll% <= 5
              * Morpher
              eval vnum 18325
            else
              eval percent_roll %percent_roll% - 5
              if %percent_roll% <= 20
                * Pet
                eval vnum 18326
              else
                eval percent_roll %percent_roll% - 20
                if %percent_roll% <= 15
                  * Croc mount
                  eval vnum 18327
                else
                  eval percent_roll %percent_roll% - 15
                  if %percent_roll% <= 5
                    * Falcon mount
                    eval vnum 18328
                  else
                    eval percent_roll %percent_roll% - 5
                    if %percent_roll% <= 5
                      * disk
                      eval vnum 18329
                    else
                      * flax seeds
                      eval vnum 18322
                    end
                  end
                end
              end
            end
          end
        end
      end
    end
    if %self.level%
      eval level %self.level%
    else
      eval level 100
    end
    %load% obj %vnum% %actor% inv %level%
    if %vnum% == 18322
      * flax seeds
      %load% obj %vnum% %actor% inv
      %load% obj %vnum% %actor% inv
      %load% obj %vnum% %actor% inv
    end
    eval item %%actor.inventory(%vnum%)%%
    if !%item.is_flagged(GROUP-DROP)% && %vnum% >= 18300 && %vnum% <= 18303
      * flag clothes with group-drop
      nop %item.flag(GROUP-DROP)%
      %scale% %item% %level%
    end
    if %item.is_flagged(BOP)%
      eval bind %%item.bind(%self%)%%
      nop %bind%
    end
  end
end
%purge% %self%
~
#18308
Scarab special attack~
0 k 100
~
%echo% &rEveryone is bitten and stung by %self.name%!
%aoe% 100 physical
~
#18309
Wander Mummy~
0 n 100
~
if (!%instance.location%)
  halt
end
mgoto %instance.location%
nop %self.unlink_instance%
~
#18310
Wandering Mummy despawn~
0 ab 1
~
if %random.100% == 1
  if !%self.fighting%
    %purge% %self% $n takes a halting step, then crumbles to dust.
  end
end
~
#18353
Load: Link random temple~
2 n 100
~
* Pick a temple
eval room_vnum 18353 + %random.4%
makeuid active_temple room i%room_vnum%
makeuid corridor room i18353
eval tofind 18354
* Link it
set direction east
%door% %corridor% %direction% room %active_temple%
~
#18354
Offering puzzle~
2 c 0
offer~
if %room.east(room)%
  %send% %actor% The door is open - you do not need to make another offering.
  return 1
  halt
end
if !%arg%
  %send% %actor% Offer what?
  return 1
  halt
end
eval item %%actor.obj_target(%arg%)%%
if %item.worn_by% || !%item.carried_by%
  %send% %actor% You can only offer an item from your inventory.
  return 1
  halt
end
* What IS it?
set solution 0
if %item.vnum% == 103
  * Lightning stone
  set solution 1
elseif %item.vnum% == 3362
  * Lettuce seed oil
  set solution 2
elseif %item.vnum% == 1359
  * Cotton cloth
  set solution 3
elseif %item.is_name(fish)%
  * Any fish (but I'd rather check for 'raw, aquatic meat')
  set solution 4
else
  %send% %actor% The gods reject your offering of %item.shortdesc%.
  return 1
  halt
end
%send% %actor% You offer %item.shortdesc% to the gods...
%echoaround% %actor% %actor.name% offers %item.shortdesc% to the gods...
eval temple_num %room.template% - 18353
if %solution% == %temple_num%
  * Bonus reward
  %echo% The gods are pleased!
  eval person %room.people%
  while %person%
    if %person.is_pc%
      * random reward
    end
    eval person %person.next_in_room%
  done
else
  %echo% The gods accept the offering.
end
%echo% One wall of the room slowly slides into the floor with a heavy grinding noise!
makeuid next_room room i18358
%door% %room% east room %next_room%
* Since the next room is the maze, we don't link back. If we did:
* %door% %next_room% west room %room%
~
#18358
Drop Random Canopic Jars~
1 n 100
~
eval actor %self.carried_by%
set number 1
if %actor.mob_flagged(HARD)%
  eval number %number% + 1
end
if %actor.mob_flagged(GROUP)%
  eval number %number% + 2
end
set item_1 18359
set item_2 18360
set item_3 18361
set item_4 18362
set items_left 4
while %number% > 0
  eval index %%random.%items_left%%%
  * Remove the item from the list
  eval vnum %%item_%index%%%
  eval iterator %index% + 1
  while %iterator% <= %items_left%
    eval new_num %iterator% - 1
    eval item_%new_num% %%item_%iterator%%%
    eval iterator %iterator% + 1
  done
  %load% obj %vnum% %actor% inv
  eval item %%actor.inventory(%vnum%)%%
  * Bind item
  eval bind %%item.bind(%self%)%%
  nop %bind%
  eval items_left %items_left% - 1
  eval number %number% - 1
done
%purge% %self%
~
#18359
Combine canopic jars~
1 c 2
combine~
eval targ %%actor.obj_target(%arg%)%%
if %targ% != %self%
  return 0
  halt
end
return 1
* got all 4?
if %actor.inventory(18359)% && %actor.inventory(18360) && %actor.inventory(18361)% && %actor.inventory(18362)%
  %load% obj 18350 %actor% inv
  eval item %actor.inventory(18350)%
  %send% %actor% You combine your four canopic jars into %item.shortdesc%!
  if %self.vnum% != 18359
    nop %actor.add_resources(18359, -1)%
  end
  if %self.vnum% != 18360
    nop %actor.add_resources(18360, -1)%
  end
  if %self.vnum% != 18361
    nop %actor.add_resources(18361, -1)%
  end
  if %self.vnum% != 18362
    nop %actor.add_resources(18362, -1)%
  end
  %purge% %self%
else
  %send% %actor% You need four unique canopic jars.
end
~
#18363
Falling block trap~
2 q 100
~
if %actor.is_npc%
  return 1
  halt
end
context %instance.id%
* One quick trick to get the target room
eval room_var %self%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  return 1
  halt
end
return 0
* Trap triggered
eval trap_running 1
global trap_running
eval last_trap_command 0
remote last_trap_command %actor.id%
%echoaround% %actor% %actor.name% starts walking %direction%...
%send% %actor% There is a loud groaning sound from the ceiling!
%echoaround% %actor% There is a loud groaning sound from the ceiling above %actor.name%!
wait 8 sec
eval trap_running 0
global trap_running
if %actor.last_trap_command% == run && %actor.position% == Standing
  %send% %actor% You dash forward as a huge stone block crashes to the floor right behind you!
  %echoaround% %actor% %actor.name% dashes forward, barely avoiding a huge stone block which falls from the ceiling!
  %send% %actor% You dive through an archway into the next room.
  %echoaround% %actor% %actor.name% dives through an archway into the next room.
  %teleport% %actor% %to_room%
  %echoaround% %actor% %actor.name% dives through the doorway from the previous room.
  %force% %actor% look
else
  %send% %actor% &rA huge stone block falls from the ceiling, knocking you to the floor!
  %echoaround% %actor% A huge stone block falls on %actor.name%, knocking %actor.himher% to the floor!
  %damage% %actor% 400 physical
  wait 5
  if %actor.health% > 0 && %actor.position% == Standing
    %send% %actor% You stagger forward to the next room.
    %echoaround% %actor% %actor.name% staggers through to the next room, looking dazed.
    %teleport% %actor% %to_room%
    %echoaround% %actor% %actor.name% staggers through the doorway from the previous room, looking dazed.
    %force% %actor% look
  end
end
* Send NPC followers after player
eval person %room_var.people%
while %person%
  eval next_person %person.next_in_room%
  if %person.is_npc% && %person.master% == %actor%
    %echoaround% %person% %person.name% follows %actor.name%.
    %teleport% %person% %actor.room%
    %send% %actor% %person.name% follows you.
    %echoaround% %actor% %person.name% follows %actor.name%.
  end
  eval person %next_person%
done
~
#18364
Search for Traps - Pyramid~
2 p 100
~
if %abilityname% != Search
  return 1
  halt
end
%send% %actor% You search for traps...
%echoaround% %actor% %actor.name% searches for traps...
switch %room.template%
  case 18353
    %send% %actor% You find a falling block trap. RUN after triggering to avoid it.
  break
  case 18364
    %send% %actor% You find no signs that anyone was ever buried here. This chamber is a fake.
  break
  default
    %send% %actor% You can't spot any traps in this room...
  break
done
return 0
halt
~
#18365
Trap flee fallback~
2 q 100
~
context %instance.id%
if %trap_running%
  %send% %actor% You can't leave right now!
  return 0
  halt
end
~
#18366
Trap action detection~
2 c 0
run jump duck~
context %instance.id%
if !%trap_running%
  %send% %actor% You don't need to do that right now.
  return 1
  halt
end
if run /= %cmd%
  set last_trap_command run
  %send% %actor% You lean forward and start sprinting!
  %echoaround% %actor% %actor.name% leans forward and starts sprinting!
elseif jump /= %cmd%
  set last_trap_command jump
  %send% %actor% You dash forward and prepare to jump...
  %echoaround% %actor% %actor.name% dashes forward and prepares to jump...
elseif duck /= %cmd%
  set last_trap_command duck
  %send% %actor% You dive for the floor!
  %echoaround% %actor% %actor.name% dives for the floor and presses %actor.himher%self against the stone!
end
remote last_trap_command %actor.id%
~
#18389
Pyramid 2 difficulty selector~
1 c 4
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty.
  return 1
  halt
end
* TODO: Check nobody's in the adventure before changing difficulty
if hard /= %arg%
  %echo% Setting difficulty to Hard...
  eval difficulty 2
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  eval difficulty 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  eval difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
eval vnum 18350
while %vnum% <= 18350
  eval mob %%instance.mob(%vnum%)%%
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
%send% %actor% You touch one of the symbols on the wall, and a section of the wall slowly slides into the floor...
%echoaround% %actor% %actor.name% touches one of the symbols on the wall, and a section of the wall slowly slides into the floor...
eval newroom i18352
eval exitroom i18350
if %exitroom%
  %door% %exitroom% down room %newroom%
end
eval room %self.room%
eval person %room.people%
while %person%
  eval next_person %person.next_in_room%
  %teleport% %person% %newroom%
  eval person %next_person%
done
otimer 24
~
$
