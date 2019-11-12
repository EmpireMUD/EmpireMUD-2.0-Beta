#16100
Hydra Load~
0 n 100
~
dg_affect #16100 %self% IMMUNE-DAMAGE on -1
set HydraBuffCounter 0
global HydraBuffCounter
if %instance.location%
  mgoto %instance.location%
end
* remove the portal leading to the inside of the instance.
set portal %self.room.contents(16100)%
if %portal%
  %purge% %portal%
end
* spawn one of the possible hydra heads.
eval randomhead %random.4% + 16100
%load% mob %randomhead% ally
~
#16101
hydra vicious head attack~
0 k 10
~
set person %self.room.people%
set counter 0
while %person%
  if !%person.is_npc%
    eval counter %counter% + 1
  end
  set person %person.next_in_room%
done
if %counter% == 1
  set ViciousDamage 300
else
  set ViciousDamage 110
end
%send% %actor% %self.name% suddenly strikes with the speed of a cobra!
%echoaround% %actor% %self.name% suddenly strikes %actor.name% with the speed of a cobra!
%damage% %actor% %ViciousDamage%
~
#16102
hydra head death~
0 f 100
~
if !%self.char_target(hydra oceanic monstrous)%
  halt
end
set hydranum %instance.mob(16100)%
if %self.vnum% == 16101
  set LowestHead 16101
  eval headspawn %random.4% + 16100
else
  set LowestHead 16102
  eval headspawn %random.3% + 16101
end
set person %self.room.people%
set headcount 0
while %person%
  if %person.is_npc% && %person% != %self%
    if %person.vnum% >= %LowestHead% && %person.vnum% <= 16104
      eval headcount %headcount% + 1
    end
  end
  set person %person.next_in_room%
done
if %headcount% <= 2
  %echo% As the life leaves the one head's eyes a bulge appears on the leading end of the hydra before the skin splits and a head shoots forward.
  %load% mob %headspawn%
  %force% %self.room.people% mfollow %hydranum%
end
if %hydranum.aff_flagged(IMMUNE-DAMAGE)%
  dg_affect #16100 %hydranum% off
  set LostImmunity %timestamp%
  remote LostImmunity %hydranum.id%
end
~
#16103
hydra head random spawn~
0 bw 50
~
set person %self.room.people%
set headcount 0
while %person%
  if %person.is_npc%
    if %person.vnum% >= 16101 && %person.vnum% <= 16104
      eval headcount %headcount% + 1
    end
  end
  set person %person.next_in_room%
done
if %headcount% <= 3
  %echo% A new bulge on the hydra's trunk begins forming.
  wait 5 s
  %echo% The growth suddenly splits open and a new head bursts forth.
  eval randomhead %random.4% + 16100
  %load% mob %randomhead% ally
end
if %self.varexists(LostImmunity)%
  eval SinceImmunity %timestamp% - %LostImmunity%
  if %SinceImmunity% >= 90
    dg_affect #16100 %self% IMMUNE-DAMAGE on -1
    rdelete LostImmunity %self.id%
  end
end
eval HydraBuffCounter %HydraBuffCounter% + 1
global HydraBuffCounter
eval HydraBuffCounter %HydraBuffCounter% - 3
if %HydraBuffCounter% >= 1
  eval BuffUpHydra %HydraBuffCounter% * 30
  dg_affect #16103 %self% off
  dg_affect #16103 %self% bonus-physical %BuffUpHydra% -1
end
~
#16104
hydra head count player enters~
0 h 100
~
wait 1
set person %self.room.people%
while %person%
  if %person.is_pc% && %person.empire%
    nop %person.empire.start_progress(16100)%
  end
  if %person.is_pc% && %person.inventory(16133)%
    %send% %person% The hydra seeking stone flares and vanishes now that you've found the beast.
    %purge% %person.inventory(16133)%
  end
  set person %person.next_in_room%
done
if %actor.is_npc% || %self.fighting%
  halt
end
set person %self.room.people%
set headcount 0
while %person%
  if %person.is_npc%
    if %person.vnum% >= 16102 && %person.vnum% <= 16104
      eval headcount %headcount% + 1
    end
  end
  set person %person.next_in_room%
done
if !%self.aff_flagged(IMMUNE-DAMAGE)%
  dg_affect #16100 %self% IMMUNE-DAMAGE on -1
end
if %headcount% == 0
  eval randomhead 16101 + %random.3%
  %load% mob %randomhead% ally
  %echo% Water splashes everywhere as the hydra lifts a head to look at you.
end
dg_affect #16103 %self% off
set HydraBuffCounter 0
global HydraBuffCounter
~
#16105
wandering mob adventure command~
0 c 0
adventure~
if !(summon /= %arg.car%)
  %teleport% %actor% %instance.real_location%
  %force% %actor% adventure
  %teleport% %actor% %self%
  return 1
  halt
end
return 0
~
#16106
hydra ethereal head combat~
0 k 10
~
set person %self.room.people%
set counter 0
while %person%
  if !%person.is_npc%
    set ptarget %person%
    eval counter %counter% + 1
  end
  set person %person.next_in_room%
done
if %counter% == 1 && !%ptarget.disabled%
  %send% %ptarget% You're frozen with fear as %self.name% gazes directly at you!
  dg_affect #16106 %ptarget% HARD-STUNNED on 10
elseif %counter% == 1
  %send% %ptarget% %self.name% opens wide and spits a bolt of mana at you!
  %dot% #16105 %ptarget% 120 15 magical 3
else
  %send% %actor% %self.name% opens wide and spits a bolt of mana at you!
  %dot% #16105 %actor% 75 15 magical 3
  %echoaround% %actor% %self.name% opens wide and spits a bolt of mana at %actor.name%!
end
~
#16107
hydra vicious head buff~
0 l 40
~
if %self.cooldown(16107)%
  halt
end
%echo% %self.name%'s eyes glow bright red and a fury overtakes it.
dg_affect %self% bonus-physical 50 90
nop %self.set_cooldown(16107, 60)%
~
#16108
hydra death head purge~
0 f 100
~
set person %self.room.people%
while %person%
  if %person.vnum% >= 16101 && %person.vnum% <= 16104
    %purge% %person%
  end
  set person %person.next_in_room%
done
%at% i16100 %load% obj 16110 room
nop %instance.set_location(%instance.real_location%)%
~
#16109
hydra withering head debuff~
0 k 20
~
switch %random.13%
  case 1
    eval DebuffValue ( %self.level% - 274 ) / 15 + 2
    set WitheringDebuff greatness
  break
  case 2
    eval DebuffValue ( %self.level% - 274 ) / 15 + 2
    set WitheringDebuff dexterity
  break
  case 3
    eval DebuffValue %self.level% / 25 * 10
    set WitheringDebuff dodge
  break
  case 4
    eval DebuffValue ( %self.level% - 274 ) / 15 + 2
    set WitheringDebuff wits
  break
  case 5
    eval DebuffValue ( %self.level% - 274 ) / 15 + 2
    set WitheringDebuff charisma
  break
  case 6
    eval DebuffValue ( %self.level% - 274 ) / 15 + 2
    set WitheringDebuff intelligence
  break
  case 7
    eval DebuffValue %self.level% / 10
    set WitheringDebuff bonus-magical
  break
  case 8
    eval DebuffValue %self.level% / 10
    set WitheringDebuff resist-physical
  break
  case 9
    eval DebuffValue %self.level% / 10
    set WitheringDebuff bonus-physical
  break
  case 10
    eval DebuffValue %self.level% / 10
    set WitheringDebuff resist-magical
  break
  case 11
    eval DebuffValue ( %self.level% - 274 ) / 15 + 2
    set WitheringDebuff strength
  break
  case 12
    eval DebuffValue %self.level% / 25 * 10
    set WitheringDebuff to-hit
  break
  case 13
    eval DebuffValue %self.level% / 10
    set WitheringDebuff bonus-healing
  break
done
switch %random.3%
  case 1
    set target %random.enemy%
  break
  case 2
    set target allplayers
  break
  case 3
    set target %self.fighting%
  break
done
eval DebuffTimer %random.16% + 4
if %target% == allplayers
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      dg_affect #16109 %person% %WitheringDebuff% -%DebuffValue% %DebuffTimer%
    end
    set person %person.next_in_room%
  done
else
  dg_affect #16109 %target% %WitheringDebuff% -%DebuffValue% %DebuffTimer%
end
~
#16110
hydra leash~
0 i 100
~
set room %self.room%
if %room.distance(%instance.real_location%)% > 40
  %echo% %self.name% sinuously twists around and doubles back.
  return 0
  wait 1
  %echo% %self.name% sinuously twists around and doubles back.
  halt
end
if %room.sector_vnum% != 6
  nop %instance.set_location(%room%)%
end
~
#16111
hydra slayer build~
5 n 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 16102
end
if (!%inter.fore%)
  %door% %inter% fore add 16101
end
detach 16111 %self.id%
~
#16112
hydra minipet whistle~
1 c 2
use~
eval GrantPet %random.5% + 16104
set check_pet 1
set PetCounter 1
while %check_pet%
  if %PetCounter% > 5
    set check_pet 0
    %send% %actor% You've already gotten all the minipets from this whistle that you're able to.
    halt
  end
  if !%actor.has_minipet(%GrantPet%)%
    nop %actor.add_minipet(%GrantPet%)%
    %load% mob %GrantPet%
    set GrantPet %self.room.people.name%
    %purge% %self.room.people%
    %send% %actor% You gain '%GrantPet%' as a mini-pet. Use the minipets command to summon it.
    set check_pet 0
    %purge% %self%
  else
    if %GrantPet% < 16109
      eval GrantPet %GrantPet% + 1
    else
      set GrantPet 16105
    end
  end
  eval PetCounter %PetCounter% + 1
done
~
#16113
hydra majestic head buffs~
0 k 10
~
set person %self.room.people%
switch %random.4%
  case 1
    while %person%
      if %person.is_npc%
        if %person.vnum% >= 16101 && %person.vnum% <= 16104
          %heal% %person% health 100
        end
      end
      set person %person.next_in_room%
    done
    %echo% %self.name% heals all visible heads.
  break
  case 2
    while %person%
      if %person.is_npc% && %person.vnum% == 16102
        dg_affect #16104 %person% bonus-physical 15 20
      end
      set person %person.next_in_room%
    done
  break
  case 3
    while %person%
      if %person.is_npc% && %person.vnum% == 16101
        dg_affect #16104 %person% bonus-magical 15 20
      end
      set person %person.next_in_room%
    done
  break
  case 4
    while %person%
      if %person.is_npc%
        if %person.vnum% == 16104
          if %person% == %self%
            %echo% The majestic head begins to glow all over as it seems to enhance itself.
            dg_affect %person% bonus-healing 5 -1
          else
            dg_affect #16104 %person% bonus-healing 15 20
          end
        end
      end
      set person %person.next_in_room%
    done
  break
done
%heal% %random.ally% health 150
~
#16114
when hydra gear is crafted~
1 n 100
~
if %self.level%
  set level %self.level%
else
  set level 225
end
set actor %self.carried_by%
if !%actor.is_pc%
  if !%self.is_flagged(GROUP-DROP)%
    nop %self.flag(GROUP-DROP)%
  end
else
  if %self.is_flagged(GROUP-DROP)%
    nop %self.flag(GROUP-DROP)%
  end
end
if !%actor.is_pc%
  if !%self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
else
  if %self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if !%self.is_flagged(BOE)%
    nop %self.flag(BOE)%
    nop %self.bind(nobody)%
  end
end
wait 1
%scale% %self% %level%
~
#16115
hydra seeking stone~
1 c 2
seek~
if !%arg%
  %send% %actor% Seek what?
  halt
end
set room %self.room%
if %room.rmt_flagged(!LOCATION)%
  %send% %actor% %self.shortdesc% spins gently in a circle.
  halt
end
if !(hydra /= %arg%)
  return 0
  halt
else
  if %actor.cooldown(16115)%
    %send% %actor% %self.shortdesc% is on cooldown.
    halt
  end
  eval adv %instance.nearest_adventure(16100)%
  nop %actor.set_cooldown(16115, 300)%
  if !%adv%
    %send% %actor% Could not find a single oceanic hydra.
    halt
  end
  %send% %actor% You hold %self.shortdesc% aloft...
  eval real_dir %%room.direction(%adv%)%%
  eval direction %%actor.dir(%real_dir%)%%
  eval distance %%room.distance(%adv%)%%
  if %distance% == 0
    %send% %actor% The oceanic hydra was last seen here.
    halt
  elseif %distance% == 1
    set plural tile
  else
    set plural tiles
  end
  %send% %actor% The oceanic hydra was last seen %distance% %plural% to the %direction%.
  %echoaround% %actor% %actor.name% holds %self.shortdesc% aloft...
end
~
#16116
hydra seeking stone only one~
1 g 100
~
if %actor.inventory(16133)%
  %send% %actor% You've already got a hydra seeking stone and can't bring yourself to pick up a second.
  return 0
end
~
#16117
hydra seeking stone load~
1 n 100
~
eval actor %self.carried_by%
if %actor.vnum% >= 16100 && %actor.vnum% <= 16104
  %purge% %self%
end
~
#16118
baby hydra customizations~
0 bw 75
~
set random_roll %random.4%
if %self.morph%
  eval headcount %self.morph% - 16100
else
  set headcount 1
end
switch %random_roll%
  case 1
    if %headcount% == 1
      set eyes pair
    else
      set eyes pairs
    end
    %echo% %self.name% blinks its %headcount% %eyes% of eyes as it looks around.
  break
  case 2
    if %headcount% == 1
      %echo% %self.name% starts shuttering like somethings wrong, then gives up on sprouting a new head.
    elseif %headcount% == 2
      set which_char %random.char%
      %send% %which_char% %self.name%'s heads each pick an ankle of yours and begin biting them.
      %echoaround% %which_char% %self.name%'s heads each begin biting an ankle of %which_char.name%.
    elseif %headcount% == 3
      %echo% %self.name%'s left and center heads begin to fight over something, while its right head simply stares off dreamily.
    else
      %echo% %self.name%'s vicious head begins snapping at you!
    end
  break
  case 3
    if %headcount% == 1
      %echo% %self.name% slithers in a circle, faster and faster, as it chases its own tail.
    elseif %headcount% == 2
      %echo% %self.name% left head goes left, while its right head goes right. The end result? %self.name% doesn't go anywhere.
    elseif %headcount% == 3
      set person %self.room.people%
      set echo_count 1
      while %person%
        if %person.vnum% != 16109 && %echo_count% == 1
          %send% %person% %self.name%'s left head snaps at your hands.
          eval echo_count %echo_count% + 1
        elseif %person.vnum% != 16109 && %echo_count% == 2
          %send% %person% %self.name%'s right head begins to flutter its tongue around you. Perhaps you'd make a tasty snack?
          eval echo_count %echo_count% + 1
        elseif %person.vnum% != 16109 && %echo_count% == 3
          %send% %person% %self.name%'s center head stares challengingly at you.
          eval echo_count %echo_count% + 1
        end
        set person %person.next_in_room%
      done
    else
      set person %self.room.people%
      set echo_count 1
      while %person%
        if %person.vnum% != 16109 && %echo_count% == 1
          %send% %person% %self.name%'s vicious head snaps at you!
          eval echo_count %echo_count% + 1
        elseif %person.vnum% != 16109 && %echo_count% == 2
          %send% %person% %self.name%'s withering head begins to flutter its tongue around you. Watch out, it might give an infection!
          eval echo_count %echo_count% + 1
        elseif %person.vnum% != 16109 && %echo_count% == 3
          %send% %person% %self.name%'s majestic head stares challengingly at you from behind the other three heads!
          eval echo_count %echo_count% + 1
        elseif %person.vnum% != 16109 && %echo_count% == 4
          %send% %person% %self.name%'s ethereal head tries to lunge at you... but passes right through.
          eval echo_count %echo_count% + 1
        end
        set person %person.next_in_room%
      done
    end
  break
  case 4
    eval new_headcount %random.4%
    if %headcount% == %new_headcount%
      %echo% %self.name% does its best to roll into a ball.
    else
      if %new_headcount% == 1
        set morphnum normal
      else
        eval morphnum %new_headcount% + 16100
      end
      if %headcount% < %new_headcount%
        eval new_headcount %new_headcount% - %headcount%
        if %new_headcount% == 1
          set head_ing head sprouts from
        else
          set head_ing heads sprout from
        end
      elseif %headcount% > %new_headcount%
        eval new_headcount %headcount% - %new_headcount%
        if %new_headcount% == 1
          set head_ing head falls off of
        else
          set head_ing heads fall off of
        end
      end
      %echo% Suddenly %new_headcount% %head_ing% %self.name%'s body.
      %morph% %self% %morphnum%
    end
  break
done
~
#16120
loot replacer~
1 n 100
~
set actor %self.carried_by%
eval LootRoll %random.1200%
if %LootRoll% <= 25
  set LoadObj 16102
elseif %LootRoll% <= 50
  set LoadObj 16103
elseif %LootRoll% <= 75
  set LoadObj 16104
elseif %LootRoll% <= 100
  set LoadObj 16105
elseif %LootRoll% <= 125
  set LoadObj 16106
elseif %LootRoll% <= 150
  set LoadObj 16107
elseif %LootRoll% <= 175
  set LoadObj 16111
elseif %LootRoll% <= 200
  set LoadObj 16112
elseif %LootRoll% <= 225
  set LoadObj 16113
elseif %LootRoll% <= 250
  set LoadObj 16114
elseif %LootRoll% <= 275
  set LoadObj 16115
elseif %LootRoll% <= 300
  set LoadObj 16116
elseif %LootRoll% <= 325
  set LoadObj 16117
elseif %LootRoll% <= 400
  set LoadObj 16118
elseif %LootRoll% <= 425
  set LoadObj 16119
elseif %LootRoll% <= 450
  set LoadObj 16120
elseif %LootRoll% <= 475
  set LoadObj 16121
elseif %LootRoll% <= 500
  set LoadObj 16122
elseif %LootRoll% <= 525
  set LoadObj 16123
elseif %LootRoll% <= 550
  set LoadObj 16124
elseif %LootRoll% <= 575
  set LoadObj 16125
elseif %LootRoll% <= 600
  set LoadObj 16126
elseif %LootRoll% <= 625
  set LoadObj 16127
elseif %LootRoll% <= 650
  set LoadObj 16128
elseif %LootRoll% <= 675
  set LoadObj 16129
elseif %LootRoll% <= 700
  set LoadObj 16130
elseif %LootRoll% <= 725
  set LoadObj 16131
elseif %LootRoll% <= 800
  set LoadObj 16132
elseif %LootRoll% <= 875
  set LoadObj 16135
elseif %LootRoll% <= 900
  set LoadObj 16136
elseif %LootRoll% <= 925
  set LoadObj 16137
elseif %LootRoll% <= 950
  set LoadObj 16138
elseif %LootRoll% <= 975
  set LoadObj 16139
elseif %LootRoll% <= 1000
  set LoadObj 16140
elseif %LootRoll% <= 1025
  set LoadObj 16141
elseif %LootRoll% <= 1050
  set LoadObj 16142
elseif %LootRoll% <= 1075
  set LoadObj 16143
elseif %LootRoll% <= 1100
  set LoadObj 16144
elseif %LootRoll% <= 1125
  set LoadObj 16145
elseif %LootRoll% <= 1150
  set LoadObj 16146
elseif %LootRoll% <= 1175
  set LoadObj 16147
else
  set LoadObj 16148
end
if %self.level%
  set level %self.level%
else
  set level 225
end
%load% obj %LoadObj% %actor% inv %level%
set item %actor.inventory%
eval bind %%item.bind(%self%)%%
nop %bind%
wait 1
%purge% %self%
~
#16121
adventure clean up~
1 f 0
~
%adventurecomplete%
~
$
