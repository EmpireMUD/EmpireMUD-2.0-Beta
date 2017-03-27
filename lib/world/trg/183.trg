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
        %force% %summon% mkill %actor%
      end
    else
      * Jackal
      %load% mob 18301 ally
      eval summon %room.people%
      if %summon.vnum% == 18301
        %echo% %self.name% summons %summon.name%, and directs it to attack %actor.name%!
        %force% %summon% mkill %actor%
      end
    end
    wait 30 sec
  break
  * Rising Waters
  case 2
    %echo% %self.name% raises %self.hisher% hands!
    %echo% &AWater starts to flood the chamber! Swim for your life!&0
    * Give the group time to type 'swim' (if they're going to)
    context %instance.id%
    eval running 1
    global running
    wait 10 sec
    unset running
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
        context %person.id%
        if !%swimming%
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
          unset swimming
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
  %damage% %self% -100000
  %echo% %self.name% settles down to rest.
end
~
#18303
Detect swim~
0 c 0
swim~
context %instance.id%
if !%running%
  %send% %actor% There's nothing to swim in right now.
  return 1
  halt
end
%send% %actor% You start swimming.
%echoaround% %actor% %actor.name% starts swimming.
dg_affect %actor% STUNNED on 10
context %actor.id%
set swimming 1
global swimming
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
%goto% %instance.location%
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
$
