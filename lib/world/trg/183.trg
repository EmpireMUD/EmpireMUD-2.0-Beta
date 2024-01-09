#18300
Pharaoh Neferkare Combat~
0 k 100
~
set heroic_mode %self.mob_flagged(GROUP)%
* Count combat script cycles until enrage
* 1 cycle should be 30 seconds?
if %heroic_mode%
  * Start scaling up after 5 minutes, game over at 15 minutes
  set soft_enrage_cycles 10
  set hard_enrage_cycles 30
else
  * Start scaling up after 5 minutes, game over at 15 minutes
  set soft_enrage_cycles 10
  set hard_enrage_cycles 30
end
set enrage_counter 0
set enraged 0
if %self.varexists(enrage_counter)%
  set enrage_counter %self.enrage_counter%
end
eval enrage_counter %enrage_counter%+1
if %enrage_counter% >= %soft_enrage_cycles%
  * Start increasing damage
  set enraged 1
  if %enrage_counter% > %hard_enrage_cycles%
    set enraged 2
  end
  * Enrage messages
  if %enrage_counter% == %soft_enrage_cycles%
    %echo% |%self% eyes start glowing brighter!
    %echo% You'd better return *%self% to the grave, quickly!
  end
  if %enrage_counter% == %hard_enrage_cycles%
    %echo% ~%self% floats into the air!
    shout I alone should rule these lands, as a living god!
  end
end
remote enrage_counter %self.id%
* Enrage effects:
if %enraged%
  if %enraged% == 2
    * Stops using normal script attacks, just spams this every hit
    %echo% ~%self% unleashes an unstoppable blast of energy, bringing stone blocks crashing down from the ceiling!
    %aoe% 1000 direct
    halt
  end
  * Don't always show the message or it would be kinda spammy
  if %random.4% == 4
    %echo% |%self% power grows!
  end
  dg_affect %self% BONUS-PHYSICAL 50 3600
end
* Start of regular combat script:
switch %random.4%
  * Summon
  case 1
    %echo% ~%self% makes a mystical gesture!
    set room %self.room%
    if %heroic_mode%
      * Scarab swarm
      %load% mob 18302 ally
      set summon %room.people%
      if %summon.vnum% == 18302
        %echo% ~%self% summons ~%summon%, and directs them to attack ~%actor%!
        %force% %summon% %aggro% %actor%
      end
    else
      * Jackal
      %load% mob 18301 ally
      set summon %room.people%
      if %summon.vnum% == 18301
        %echo% ~%self% summons ~%summon%, and directs it to attack ~%actor%!
        %force% %summon% %aggro% %actor%
      end
    end
    wait 30 sec
  break
  * Rising Waters
  case 2
    %echo% ~%self% raises ^%self% hands!
    %echo% &&AWater starts to flood the chamber! Swim for your life!&&0
    * Give the group time to type 'swim' (if they're going to)
    set running 1
    remote running %self.id%
    wait 10 sec
    set running 0
    remote running %self.id%
    %echo% &&AWater floods the chamber!
    set room %self.room%
    set person %room.people%
    while %person%
      if %person.is_pc%
        if %person.health% <= 0
          * Finish them off
          %send% %person% You are pummeled by raging waters...
          %damage% %person% 100 physical
        end
        set test %self.varexists(swimming_%person.id%)%
        if %test%
          eval test %%self.swimming_%person.id%%%
        end
        if !%test%
          %send% %person% &&rYou are drowned by the rising waters!
          %echoaround% %person% ~%person% sinks beneath the rising waters!
          if %heroic_mode%
            %damage% %person% 500 direct
          else
            %damage% %person% 250 direct
          end
        else
          %send% %person% You barely keep your head above the water!
          %echoaround% %person% ~%person% barely keeps ^%person% head above the water!
          unset swimming_%person.id%
        end
      end
      set person %person.next_in_room%
    done
    %echo% ~%self% looks rejuvenated by the water!
    if %heroic_mode%
      %damage% %self% -300
    else
      %damage% %self% -150
    end
    wait 3 sec
    %echo% The water level starts to lower...
    wait 17 sec
  break
  * Sandstorm
  case 3
    %echo% ~%self% makes a sweeping gesture, and the wind picks up!
    wait 5 sec
    %echo% &&rA howling tornado of sand fills the chamber, blinding and slashing at everyone!
    set cycle 1
    while %cycle% <= 4
      set room %self.room%
      set person %room.people%
      while %person%
        if %self.is_enemy(%person%)%
          if %cycle% == 1
            dg_affect %person% BLIND on 20
          end
          %damage% %person% 40 physical
        end
        eval person %person.next_in_room%
      done
      wait 5 sec
      eval cycle %cycle% + 1
      if %cycle% <= 4
        %echo% &&rThe sandstorm rages on!
      end
    done
    wait 5 sec
    %echo% The sandstorm dies down...
  break
  * Bandage bondage
  case 4
    %echo% |%self% bandages unfurl and lash out like tentacles, wrapping around ~%actor%!
    if %heroic_mode%
      set max_cycles 5
    else
      set max_cycles 2
    end
    eval duration %max_cycles%*5
    dg_affect %actor% HARD-STUNNED on %duration%
    set verify_target %actor.id%
    set cycle 1
    while %cycle% <= %max_cycles%
      if %verify_target% != %actor.id%
        %echo% |%self% bandages return to their proper place.
        halt
      end
      if %actor.health% > -10
        %send% %actor% &&r|%self% bandages tighten around you, trying to squeeze the life out of you!
        %echoaround% %actor% |%self% bandages tighten around ~%actor%!
        %damage% %actor% 150 physical
        wait 5 sec
      end
      eval cycle %cycle% + 1
    done
    %echo% |%self% bandages release ~%actor%.
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
    %echo% ~%self% vanishes.
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
  %echo% ~%self% settles down to rest.
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
%echoaround% %actor% ~%actor% starts swimming.
dg_affect %actor% HARD-STUNNED on 10
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
if %instance.players_present% > %self.room.players_present%
  %send% %actor% You cannot set a difficulty while players are elsewhere in the adventure.
  return 1
  halt
end
if hard /= %arg%
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
set vnum 18300
while %vnum% <= 18300
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
%send% %actor% You touch one of the symbols on the wall, and a section of the wall slowly slides into the floor...
%echoaround% %actor% ~%actor% touches one of the symbols on the wall, and a section of the wall slowly slides into the floor...
set newroom i18302
set exitroom i18300
if %exitroom%
  %door% %exitroom% down room %newroom%
end
set person %self.room.people%
while %person%
  set next_person %person.next_in_room%
  %teleport% %person% %newroom%
  set person %next_person%
done
otimer 24
%at% i18300 %load% mob 18309
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
%send% %actor% You can't flee from ~%self%!
return 1
~
#18307
Pyramid bonus loot replacer~
1 n 100
~
set actor %self.carried_by%
if %actor%
  if %actor.mob_flagged(GROUP)% && %actor.mob_flagged(HARD)%
    * Roll for mount
    set percent_roll %random.100%
    if %percent_roll% <= 10
      * Sobek
      set vnum 18300
    else
      eval percent_roll %percent_roll% - 10
      if %percent_roll% <= 10
        * Bastet
        set vnum 18301
      else
        eval percent_roll %percent_roll% - 10
        if %percent_roll% <= 10
          * Anubis
          set vnum 18302
        else
          eval percent_roll %percent_roll% - 10
          if %percent_roll% <= 10
            * Horus
            set vnum 18303
          else
            eval percent_roll %percent_roll% - 10
            if %percent_roll% <= 5
              * Morpher
              set vnum 18325
            else
              eval percent_roll %percent_roll% - 5
              if %percent_roll% <= 20
                * Pet
                set vnum 18326
              else
                eval percent_roll %percent_roll% - 20
                if %percent_roll% <= 15
                  * Croc mount
                  set vnum 18327
                else
                  eval percent_roll %percent_roll% - 15
                  if %percent_roll% <= 5
                    * Falcon mount
                    set vnum 18328
                  else
                    eval percent_roll %percent_roll% - 5
                    if %percent_roll% <= 5
                      * disk
                      set vnum 18329
                    else
                      * flax seeds
                      set vnum 18322
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
      set level %self.level%
    else
      set level 100
    end
    %load% obj %vnum% %actor% inv %level%
    if %vnum% == 18322
      * flax seeds
      %load% obj %vnum% %actor% inv
      %load% obj %vnum% %actor% inv
      %load% obj %vnum% %actor% inv
    end
    set item %actor.inventory(%vnum%)%
    if !%item.is_flagged(GROUP-DROP)% && %vnum% >= 18300 && %vnum% <= 18303
      * flag clothes with group-drop
      nop %item.flag(GROUP-DROP)%
      %scale% %item% %level%
    end
    if %item.is_flagged(BOP)%
      nop %item.bind(%self%)%
    end
  end
end
%purge% %self%
~
#18308
Scarab special attack~
0 k 100
~
%echo% &&rEveryone is bitten and stung by ~%self%!
%aoe% 75 physical
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
#18311
Neferkare Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(18300)%
end
~
$
