#19000
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
#19001
Swamp Hag 2.0: Summon Allies~
0 k 100
~
if %self.cooldown(19001)%
  halt
end
* Clear blind just in case...
if %self.affect(BLIND)%
  %echo% %self.name%'s eyes flash blue, and %self.hisher% vision clears!
  dg_affect %self% BLIND off 1
end
set person %self.room.people%
while %person%
  if %person.vnum% == 19001 || %person.vnum% == 19002
    * Rat already present
    halt
  end
  set person %person.next_in_room%
done
nop %self.set_cooldown(19001, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
%echo% %self.name% reaches under the bed and opens a cage.
wait 1 sec
if %heroic_mode%
  %load% mob 19002 ally
else
  %load% mob 19001 ally
end
set summon %self.room.people%
if %summon%
  %echo% %summon.name% scurries out from a cage!
  %force% %summon% %aggro% %actor%
end
~
#19002
Swamp Hag 2.0: Pestle Smash~
0 k 20
~
if %self.cooldown(19001)%
  halt
end
nop %self.set_cooldown(19001, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
%send% %actor% %self.name% swings %self.hisher% pestle into the side of your head!
%echoaround% %actor% %self.name% swings %self.hisher% pestle into the side of %actor.name%'s head!
if %heroic_mode%
  dg_affect #19002 %actor% HARD-STUNNED on 10
end
%damage% %actor% 100
~
#19003
Swamp Hag 2.0: Voodoo Dolls~
0 k 40
~
if %self.cooldown(19001)%
  halt
end
nop %self.set_cooldown(19001, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
if !%heroic_mode%
  %echo% %self.name% grabs a doll off a nearby shelf...
  %send% %actor% The doll looks like you!
  %echoaround% %actor% The doll looks like %actor.name%!
  wait 2 sec
  %echo% %self.name% starts stabbing needles into the doll!
  %send% %actor% You feel stabbing pains in your limbs!
  %echoaround% %actor% %actor.name% winces in pain.
  dg_affect #19003 %actor% SLOW on 30
  %dot% #19003 %actor% 50 30 magical 1
else
  %echo% %self.name% grabs a handful of needles!
  wait 2 sec
  %echo% %self.name% starts rapidly stabbing needles into the dolls on the nearby shelves!
  %echo% You feel stabbing pains in your limbs!
  set person %self.room.people%
  while %person%
    if %self.is_enemy(%person%)%
      dg_affect #19003 %person% SLOW on 45
      %dot% #19003 %person% 75 45 magical 1
    end
    set person %person.next_in_room%
  done
end
~
#19004
Swamp Hag 2.0: Insect Swarm~
0 k 60
~
if %self.cooldown(19001)%
  halt
end
nop %self.set_cooldown(19001, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
%echo% %self.name% throws open a small window.
wait 3 sec
%echo% A swarm of biting insects fills the room!
if %heroic_mode%
  %echo% Everyone is blinded and stung by the insects!
else
  %echo% Everyone is bitten and stung by the insects!
end
if %heroic_mode%
  set person %self.room.people%
  while %person%
    if %person.is_pc%
      dg_affect #19004 %person% BLIND on 20
    end
    set person %person.next_in_room%
  done
  %aoe% 100 physical
end
~
#19005
Swamp Hag 2.0 Diff Select~
1 c 4
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty.
  return 1
  halt
end
* TODO: Check nobody's in the adventure before changing difficulty
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set difficulty 1
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set difficulty 2
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  set difficulty 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  set difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
set vnum 19000
while %vnum% <= 19000
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
%send% %actor% You tug on a hanging rope, and a rope ladder unfolds and drops from the ceiling...
%echoaround% %actor% %actor.name% tugs on a hanging rope, and a rope ladder unfolds and drops from the ceiling...
set newroom i19001
set exitroom %instance.location%
if %exitroom%
  %door% %exitroom% %exitroom.enter_dir% room %newroom%
  %door% %newroom% %exitroom.exit_dir% room %exitroom%
end
set person %self.room.people%
while %person%
  set next_person %person.next_in_room%
  %teleport% %person% %newroom%
  set person %next_person%
done
otimer 24
~
#19006
Swamp Hag delayed despawn~
1 f 0
~
%adventurecomplete%
~
#19007
Swamp Hag load BoP->BoE~
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
#19008
Swamp Hag 2.0 Death~
0 f 100
~
* Crystal ball
%load% obj 19000
set item %room.contents%
%echo% As %self.name% falls to the ground, %self.heshe% pulls a cloth off the table, revealing a crystal ball!
* Mark the adventure as complete
%adventurecomplete%
* For each player in the room (on hard+ only):
if %self.mob_flagged(HARD)% || %self.mob_flagged(GROUP)%
  set ch %self.room.people%
  while %ch%
    if %ch.is_pc%
      * Token reward
      set token_amount 1
      if %self.mob_flagged(GROUP)%
        eval token_amount %token_amount% * 2
        if %self.mob_flagged(HARD)%
          eval token_amount %token_amount% * 2
        end
      end
      if %token_amount% > 1
        set string %token_amount% %currency.19000(2)%
        set pronoun them
      else
        set string a %currency.19000(1)%
        set pronoun it
      end
      %send% %ch% Searching the room, you find %string%! You take %pronoun%.
      nop %ch.give_currency(19000, %token_amount%)%
      * Random item is handled by the loot replacer.
    end
    set ch %ch.next_in_room%
  done
end
return 0
~
#19009
Swamp Hag 2.0 group: Bind ~
0 k 100
~
if %self.cooldown(19001)%
  halt
end
set heroic_mode %self.mob_flagged(GROUP)%
if !%heroic_mode%
  halt
end
* Find a non-bound target
set target %actor%
set person %self.room.people%
set target_found 0
set no_targets 0
while %target.affect(19009)% && %person%
  if %person.is_pc% && %person.is_enemy(%self%)%
    set target %person%
  end
  set person %person.next_in_room%
done
if !%target%
  * Sanity check
  halt
end
if %target.affect(19009)%
  * No valid targets
  halt
end
* Valid target found, start attack
nop %self.set_cooldown(19001, 30)%
%send% %target% %self.name% grabs a murky green potion off a nearby shelf and takes aim at you...
%echoaround% %target% %self.name% grabs a murky green potion off a nearby shelf and takes aim at %target.name%...
wait 3 sec
%send% %target% %self.name% throws the murky potion at you!
%echoaround% %target% %self.name% throws the murky potion at %target.name%!
wait 3 sec
%send% %target% The potion bottle shatters, and tendrils of dark energy lash out to bind your limbs!
%echoaround% %target% The potion bottle shatters, and tendrils of dark energy lash out to bind %target.name%'s limbs!
%send% %target% Type 'struggle' to break free!
dg_affect #19009 %actor% HARD-STUNNED on 75
~
#19010
Swamp Hag bind struggle~
0 c 0
struggle~
set break_free_at 3
if !%actor.affect(19009)%
  return 0
  halt
end
if %actor.cooldown(19010)%
  %send% %actor% You need to gather your strength.
  halt
end
nop %actor.set_cooldown(19010, 3)%
if !%actor.varexists(struggle_counter)%
  set struggle_counter 0
  remote struggle_counter %actor.id%
else
  set struggle_counter %actor.struggle_counter%
end
eval struggle_counter %struggle_counter% + 1
if %struggle_counter% >= %break_free_at%
  %send% %actor% You break free of your bindings!
  %echoaround% %actor% %actor.name% breaks free of %actor.hisher% bindings!
  dg_affect #19009 %actor% off
  rdelete struggle_counter %actor.id%
  halt
else
  %send% %actor% You struggle against your bindings, but fail to break free.
  %echoaround% %actor% %actor.name% struggles against %actor.hisher% bindings!
  remote struggle_counter %actor.id%
  halt
end
~
#19011
Swamp hag bind fallback~
2 c 0
struggle~
* Only if the hag is dead.
if %instance.mob(19000)%
  return 0
  halt
end
if !%actor.affect(19009)%
  return 0
  halt
end
%send% %actor% You break free of your bindings!
%echoaround% %actor% %actor.name% breaks free of %actor.hisher% bindings!
dg_affect #19009 %actor% off
if %actor.varexists(struggle_counter)%
  rdelete struggle_counter %actor.id%
  halt
end
~
#19012
Hag difficulty select: wrong command~
1 c 4
up climb~
%send% %actor% You can't climb up the rope. Select a difficulty level first.
return 1
~
#19013
Swamp Rat Combat 2.0~
0 k 100
~
* Scale up (group only)
if %self.vnum% == 19002
  if %self.varexists(enrage_counter)%
    set enrage_counter %self.enrage_counter%
  else
    set enrage_counter 0
  end
  eval enrage_counter %enrage_counter% + 1
  dg_affect #19014 %self% off
  eval amount %enrage_counter% * 1
  dg_affect #19014 %self% BONUS-PHYSICAL %amount% -1
  remote enrage_counter %self.id%
  if %random.4% == 4
    %echo% %self.name% seems to grow slightly!
  end
  if %enrage_counter% == 100
    set master %self.master%
    if %master%
      %force% %master% shout Magic wand, make my monster groooooooow!
      %echo% %master.name% smacks %self.name% with her pestle.
    end
  end
end
* Bite deep
if %random.6% == 6
  wait 1
  %echo% %self.name% bites deep!
  %send% %actor% You don't feel so good...
  %echoaround% %actor% %actor.name% doesn't look so good...
  %dot% #19013 %actor% 50 30 physical 4
end
~
#19014
Rat despawn~
0 n 100
~
wait 30 sec
%purge% %self% $n scurries into a crack in the floor.
~
#19015
Swamp Hag 2.0 loot replacer~
1 n 100
~
set actor %self.carried_by%
if %actor%
  if %actor.mob_flagged(HARD)% || %actor.mob_flagged(GROUP)%
    * Swamp water
    %load% obj 19001 %actor% inv
    * Roll for drop
    set percent_roll %random.100%
    if %percent_roll% <= 2
      * Minipet
      set vnum 19007
    else
      eval percent_roll %percent_roll% - 2
      if %percent_roll% <= 2
        * Land mount
        set vnum 19008
      else
        eval percent_roll %percent_roll% - 2
        if %percent_roll% <= 2
          * Sea mount
          set vnum 19009
        else
          eval percent_roll %percent_roll% - 2
          if %percent_roll% <= 1
            * Flying mount
            set vnum 19010
          else
            eval percent_roll %percent_roll% - 1
            if %percent_roll% <= 1
              * Morpher
              set vnum 19040
            else
              eval percent_roll %percent_roll% - 1
              if %percent_roll% <= 1
                * Seed
                set vnum 19041
              else
                eval percent_roll %percent_roll% - 1
                if %percent_roll% <= 1
                  * Coffee
                  set vnum 19048
                else
                  eval percent_roll %percent_roll% - 1
                  if %percent_roll% <= 1
                    * Feathers
                    set vnum 19004
                  else
                    eval percent_roll %percent_roll% - 1
                    if %percent_roll% <= 1
                      * Clothing
                      set vnum 19006
                    else
                      eval percent_roll %percent_roll% - 1
                      if %percent_roll% <= 1
                        * Shoes
                        set vnum 19038
                      else
                        * Nothing
                        set vnum -1
                      end
                    end
                  end
                end
              end
            end
          end
        end
      end
    end
    if %vnum% > 0
      if %self.level%
        set level %self.level%
      else
        set level 100
      end
      set person %self.room.people%
      while %person%
        if %person.is_pc%
          %load% obj %vnum% %actor% inv %level%
          set item %actor.inventory(%vnum%)%
          if %item.is_flagged(BOP)%
            nop %item.bind(%self%)%
          end
        end
        set person %person.next_in_room%
      done
    end
  end
end
%purge% %self%
~
#19016
SH Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(19000)%
end
~
#19047
Walking Hut setup~
5 n 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.fore%)
  %door% %inter% fore add 19048
end
detach 19047 %self.id%
~
#19048
Swamp hag hut: Fill with Coffee~
2 c 0
fill~
set liquid_num 19048
set name hag's coffee
if !%arg%
  * Fill what?
  return 0
  halt
end
set target %actor.obj_target(%arg%)%
if %target.type% != DRINKCON
  * You can't fill [item]!
  return 0
  halt
end
if %target.val1% >= %target.val0%
  %send% %actor% There is no room for more.
  halt
end
if %target.val1% > 0 && %target.val2% != %liquid_num%
  %send% %actor% There is already another liquid in it. Pour it out first.
  halt
end
%send% %actor% You fill %target.shortdesc% with %name%.
%echoaround% %actor% %actor.name% fills %target.shortdesc% with %name%.
nop %target.val2(%liquid_num%)%
nop %target.val1(%target.val0%)%
~
#19060
Goblin Challenge 2.0 Difficulty Selector~
1 c 4
difficulty~
set room %self.room%
if !%arg%
  %send% %actor% You must specify a level of difficulty.
  return 1
  halt
end
* TODO: Check nobody's in the adventure before changing difficulty
set level 50
set person %room.people%
while %person%
  if %person.level% > %level%
    set level %person.level%
  end
  set person %person.next_in_room%
done
if easy /= %arg%
  %echo% Setting difficulty to Easy...
  set difficulty 1
  if %level% > 100
    set level 100
  end
elseif medium /= %arg%
  %echo% Setting difficulty to Medium...
  set difficulty 2
elseif difficult /= %arg%
  %echo% Setting difficulty to Difficult...
  set difficulty 3
  if %level% < 100
    set level 100
  end
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Scale adventure
nop %instance.level(%level%)%
* Load mob, apply difficulty setting
%load% mob 10200
set mob %room.people%
remote difficulty %mob.id%
set mob_diff %difficulty%
if %mob.vnum% >= 10204 && %mob.vnum% <= 10205
  eval mob_diff %mob_diff% + 1
end
dg_affect %mob% !ATTACK on 5
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %mob_diff% == 1
  * Then we don't need to do anything
elseif %mob_diff% == 2
  nop %mob.add_mob_flag(HARD)%
elseif %mob_diff% == 3
  nop %mob.add_mob_flag(GROUP)%
elseif %mob_diff% == 4
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
%scale% %mob% %mob.level%
* Done applying difficulty setting
%send% %actor% You ring %self.shortdesc%, and %mob.name% charges out to meet you.
%echoaround% %actor% %actor.name% rings %self.shortdesc%, and %mob.name% charges out to meet you.
%adventurecomplete%
%purge% %self%
~
#19061
Power Strike~
0 k 33
~
if %self.cooldown(10200)%
  halt
end
nop %self.set_cooldown(10200, 30)%
* Group only -- boss only for Nilbog
if %self.mob_flagged(GROUP)%
  if %self.mob_flagged(HARD)% || %self.vnum% != 10204
    set heroic_mode 1
  end
end
%send% %actor% %self.name% raises %self.hisher% weapon high and deals you a powerful blow!
%echoaround% %actor% %self.name% raises %self.hisher% weapon high and deals %actor.name% a powerful blow!
if %heroic_mode%
  %damage% %actor% 200 physical
  %send% %actor% You are stunned and knocked off-balance!
  dg_affect #10202 %actor% HARD-STUNNED on 10
  dg_affect #10201 %actor% DODGE -20 20
  dg_affect #10201 %actor% TO-HIT -20 20
else
  %damage% %actor% 75 physical
  %send% %actor% You are knocked off-balance!
  dg_affect #10201 %actor% DODGE -10 20
  dg_affect #10201 %actor% TO-HIT -10 20
end
~
#19062
Whirlwind Attack~
0 k 50
~
if %self.cooldown(10200)%
  halt
end
nop %self.set_cooldown(10200, 30)%
if %self.mob_flagged(GROUP)%
  if %self.mob_flagged(HARD)% || %self.vnum% != 10204
    set heroic_mode 1
  end
end
%echo% %self.name% starts spinning in circles!
wait 3 sec
%echo% &r%self.name% swings %self.hisher% weapon wildly, hitting everything in sight!
if %heroic_mode%
  %aoe% 125 physical
  %echo% &r%self.name%'s wild swings leave bleeding wounds!
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      %dot% #10203 %person% 100 20 physical
    end
    set person %person.next_in_room%
  done
else
  %aoe% 75 physical
end
~
#19063
Filks & Walts: Backstab!~
0 k 33
~
if %self.cooldown(10200)%
  halt
end
if %actor.fighting% == %self%
  halt
end
nop %self.set_cooldown(10200, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
if !%heroic_mode%
  %send% %actor% You spot %self.name% trying to sneak around behind you...
end
%echoaround% %actor% %self.name% slips around behind %actor.name% and draws a wicked dagger...
wait 5 sec
if %self.disabled% || %actor.fighting% == %self% || !%actor.fighting% || %self.aff_flagged(DISARM)% || %self.aff_flagged(ENTANGLED)%
  %echo% %self.name%'s backstab is interrupted!
  halt
else
  %send% %actor% %self.name% sinks %self.hisher% dagger into your back!
  %echoaround% %actor% %self.name% sinks %self.hisher% dagger into %actor.name%'s back!
  if %heroic_mode%
    %damage% %actor% 750
  else
    %damage% %actor% 150
  end
end
~
#19064
Filks & Walts: Flank Attack~
0 k 50
~
if %self.cooldown(10200)%
  halt
end
if %actor.fighting% == %self%
  halt
end
set target 0
set person %self.room.people%
while %person%
  if %person.vnum% >= 10200 && %person.vnum% <= 10205 && %person.vnum% != %self.vnum% && %person.fighting% == %self.fighting%
    set target %person%
  end
  set person %person.next_in_room%
done
if !%target%
  halt
end
nop %self.set_cooldown(10200, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
%send% %actor% %self.name% flanks you as you attack %target.name%, leaving you vulnerable!
%echoaround %actor% %self.name% and %target.name% flank %actor.name%, giving %self.name% an advantage!
if %heroic_mode%
  dg_affect #10204 %self% TO-HIT 75 30
else
  dg_affect #10204 %self% TO-HIT 25 30
end
~
#19065
Goblin Shaman: Goblinfire~
0 k 33
~
if %self.cooldown(10200)%
  halt
end
nop %self.set_cooldown(10200, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
%send% %actor% %self.name% splashes goblinfire at you, causing serious burns!
%echoaround% %actor% %self.name% splashes goblinfire at %actor.name%, causing serious burns!
if %heroic_mode%
  %dot% #10205 %actor% 100 120 fire 5
  %damage% %actor% 100 fire
else
  %dot% #10205 %actor% 75 15 fire
  %damage% %actor% 75 fire
end
~
#19066
Goblin Shaman: Fire Spiral~
0 k 50
~
if %self.cooldown(10200)%
  halt
end
nop %self.set_cooldown(10200, 30)%
if %self.mob_flagged(GROUP)%
  if %self.mob_flagged(HARD)% || %self.vnum% != 10205
    set heroic_mode 1
  end
end
%echo% %self.name% begins spinning spirals of flame...
if %heroic_mode%
  set cycles 5
else
  set cycles 1
end
set cycle 1
while %cycle% <= %cycles%
  wait 3 sec
  %echo% &r%self.name% unleashes a flame spiral!
  %aoe% 50 fire
  eval cycle %cycle% + 1
done
wait 3 sec
%echo% %self.name%'s flame spirals fade away.
~
#19067
Zelkab: Knockout Punch~
0 k 100
~
if %self.cooldown(10200)%
  halt
end
nop %self.set_cooldown(10200, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
%echo% %self.name% draws back %self.hisher% fist for a mighty blow...
wait 3 sec
if %heroic_mode%
  %send% %actor% &r%self.name%'s fist flies right at your face!
  %damage% %actor% 150 physical
  %send% %actor% Everything turns dark and confusing...
  %echoaround% %actor% %self.name% decks %actor.name% with one powerful punch!
  dg_affect #10206 %actor% BLIND on 15
  dg_affect #10206 %actor% HARD-STUNNED on 15
else
  %send% %actor% %self.name% hits you hard, stunning you!
  %echoaround% %actor% %self.name% hits %actor.name% hard, stunning %actor.himher%!
  %damage% %actor% 100 physical
  dg_affect #10206 %actor% STUNNED on 5
end
~
#19068
Garlgarl: Troll Blood~
0 k 100
~
if %self.cooldown(10200)%
  halt
end
nop %self.set_cooldown(10200, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
%echo% %self.name% starts searching for something...
wait 5 sec
if %self.disabled%
  %echo% %self.name%'s search is interrupted.
  halt
end
%echo% %self.name% grabs a vial labeled 'TROL BLUD' and drinks it!
%echo% %self.name%'s wounds start to close!
if %heroic_mode%
  %damage% %self% -200
  dg_affect #10207 %self% HEAL-OVER-TIME 125 30
else
  %damage% %self% -100
  dg_affect #10207 %self% HEAL-OVER-TIME 20 30
end
~
#19069
Filks: Poison Arrow~
0 k 100
~
if %self.cooldown(10200)%
  halt
end
nop %self.set_cooldown(10200, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
set target %random.enemy%
if !%target%
  set target %actor%
end
if !%target%
  halt
end
%send% %actor% %self.name% dashes backwards, draws %self.hisher% bow, and aims at you!
%echoaround% %actor% %self.name% dashes backwards, draws %self.hisher% bow, and aims at %actor.name%!
wait 2 sec
%send% %actor% %self.name% shoots you with a poisoned arrow!
%echoaround% %actor% %self.name% shoots %actor.name% with a poisoned arrow!
if %heroic_mode%
  %damage% %actor% 50 physical
  %dot% #10208 %actor% 200 30 poison
else
  %damage% %actor% 50 physical
  %dot% #10208 %actor% 75 15 poison
end
~
#19070
Walts: Bomb Lob~
0 k 100
~
if %self.cooldown(10200)%
  halt
end
nop %self.set_cooldown(10200, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
%echo% Walts runs to the edge of the nest and pulls out a bomb!
wait 5 sec
%echo% %self.name% hurls the bomb at you!
if %heroic_mode%
  %echo% &rThe bomb explodes, stunning you!
  %aoe% 100 physical
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      dg_affect #10209 %person% HARD-STUNNED on 5
    end
    set person %person.next_in_room%
  done
else
  %echo% &rThe bomb explodes!
  %aoe% 75 physical
end
~
#19071
Nilbog: Shield Block~
0 k 100
~
if %self.cooldown(10200)%
  halt
end
nop %self.set_cooldown(10200, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
if %heroic_mode%
  %echo% %self.name% raises %self.hisher% tarnished shield and strikes from behind it.
  eval magnitude %self.level%/2
  dg_affect #10210 %self% DODGE %magnitude% 30
  eval magnitude %self.level% / 6
  dg_affect #10210 %self% HEAL-OVER-TIME %magnitude% 30
else
  %echo% %self.name% raises %self.hisher% tarnished shield and cowers behind it.
  eval magnitude %self.level% / 4
  dg_affect #10210 %self% DODGE %magnitude% 30
  dg_affect #10210 %self% STUNNED on 30
  eval magnitude %self.level% / 6
  dg_affect #10210 %self% HEAL-OVER-TIME %magnitude% 30
end
~
#19072
Furl: Spellstorm / Moonrise~
0 k 50
~
if %self.cooldown(10200)%
  halt
end
set goblin 0
set room %self.room%
set person %room.people%
while %person%
  if %person.vnum% >= 10200 && %person.vnum% <= 10205 && %person.vnum% != %self.vnum%
    set goblin %person%
  end
  set person %person.next_in_room%
done
nop %self.set_cooldown(10200, 30)%
%echo% %self.name% starts casting a spell...
wait 3 sec
set heroic_mode %self.mob_flagged(GROUP)%
set hard %self.mob_flagged(HARD)%
if %goblin% || !%heroic_mode% || !%hard%
  if %heroic_mode%
    %echo% &r%self.name% unleashes a storm of uncontrolled magical energy!
    %aoe% 100 magical
    set person %room.people%
    while %person%
      if %person.is_enemy(%self%)%
        %dot% #10221 %person% 75 30 magical
        dg_affect #10211 %person% SLOW on 30
      end
      set person %person.next_in_room%
    done
  else
    %send% %actor% &r%self.name% unleashes a bolt of uncontrolled magical energy, which strikes you!
    %echoaround% %actor% %self.name% unleashes a bolt of uncontrolled magical energy, which strikes %actor.name%!
    %damage% %actor% 100 magical
    %dot% #10211 %actor% 75 30 magical
    dg_affect #10211 %actor% SLOW on 30
  end
  dg_affect #10211 %self% HASTE on 30
  if %goblin%
    dg_affect #10211 %goblin% HASTE on 30
  end
else
  eval vnum 10200 + %random.5% - 1
  %load% mob %vnum% ally %self.level%
  set mob %room.people%
  %echo% %self.name% slams %self.hisher% staff into the ground.
  set str_name %mob.alias%
  set str_name %str_name.car%
  shout Get up, lazy %str_name%! We still fighting!
  set difficulty 1
  remote difficulty %mob.id%
  nop %mob.add_mob_flag(SPAWNED)%
  nop %mob.add_mob_flag(!LOOT)%
  nop %mob.add_mob_flag(UNDEAD)%
  nop %mob.remove_mob_flag(HARD)%
  nop %mob.remove_mob_flag(GROUP)%
  %scale% %mob% %mob.level%
  %force% %mob% %aggro% %actor%
end
~
$
