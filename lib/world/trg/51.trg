#5107
Mine Rename Based on Type~
2 o 100
~
wait 0
set type %room.mine_type%
if %type%
  set name %type.car%
  set pref %name.ana%
  %mod% %room% title %pref.cap% %name.cap% Mine
end
detach 5107 %room.id%
~
#5138
Tavern Hideout Completion~
2 o 100
~
if !%room.down(room)%
  * Add Secret Hidout
  %door% %room% down add 5510
  set hideout %room.down(room)%
  if %hideout%
    * Close doors
    %door% %room% down flags ab
    %door% %room% down name trapdoor
    %door% %hideout% up flags ab
    %door% %hideout% up name trapdoor
  end
end
detach 5138 %room.id%
~
#5139
Bartender: List drinks from empire inventory~
0 c 0
list~
set empire %self.empire%
set room %self.room%
set cost %room.var(tavern_cost,50)%
return 1
*
* basic checks
if identify /= %arg%
  %send% %actor% You can't use list-identify here.
  halt
elseif !%empire%
  %send% %actor% There's nothing available here.
  halt
end
*
* otherwise show what's available
%send% %actor% Available to buy by the barrel for %cost% %empire.adjective% coins:
set any 0
if %empire.has_storage(232,%room%)%
  %send% %actor% &&0  ale
  set any 1
end
if %empire.has_storage(233,%room%)%
  %send% %actor% &&0  lager
  set any 1
end
if %empire.has_storage(234,%room%)%
  %send% %actor% &&0  wheat beer
  set any 1
end
if %empire.has_storage(235,%room%)%
  %send% %actor% &&0  cider
  set any 1
end
if %empire.has_storage(236,%room%)%
  %send% %actor% &&0  mead
  set any 1
end
if !%any%
  %send% %actor% &&0  nothing
end
~
#5140
Bartender: Buy drink from empire inventory~
0 c 0
buy~
set empire %self.empire%
set room %self.room%
set cost %room.var(tavern_cost,50)%
return 1
*
* basic checks
if !%empire%
  %send% %actor% There's nothing available here.
  halt
elseif !%arg%
  return 0
  halt
elseif ale /= %arg%
  set vnum 232
elseif lager /= %arg%
  set vnum 233
elseif wheat beer /= %arg%
  set vnum 234
elseif cider /= %arg%
  set vnum 235
elseif mead /= %arg%
  set vnum 236
else
  %send% %actor% They don't seem to have that on tap.
  halt
end
*
* found drink... now advanced checks
if %empire.has_storage(%vnum%,%room%)% < 1
  %send% %actor% They don't seem to have any.
  halt
elseif !%actor.can_afford(%cost%)%
  %send% %actor% You can't afford the cost.
  halt
end
*
* charge and load
set money %actor.charge_coins(%cost%)%
nop %empire.give_coins(%cost%)%
%load% einv %vnum% %empire% %actor% inv
set obj %actor.inventory%
if %obj.vnum% == %vnum%
  %send% %actor% You buy @%obj% for %money%.
  %echoaround% %actor% ~%actor% buys @%obj%.
end
~
#5141
Tavern: Charge a differnet price~
2 c 0
price~
set empire %self.empire%
set cost %room.var(tavern_cost,50)%
eval val %arg% + 0
return 1
if !%arg%
  if %cost% != 1
    set coins coins
  else
    set coins coin
  end
  %send% %actor% The tavern charges %cost% %empire.adjective% %coins%.
elseif %actor.is_npc%
  %send% %actor% You can't do that.
elseif !%empire%
  %send% %actor% The tavern must be claimed for you to set the price.
elseif %empire% != %actor.empire%
  %send% %actor% You don't own the tavern; you can't set the price.
elseif %arg% != 0 && %val% < 1
  %send% %actor% You can't set the price to that.
else
  set tavern_cost %val%
  remote tavern_cost %room.id%
  if %val% != 1
    set coins coins
  else
    set coins coin
  end
  %send% %actor% You set the price to %val% %empire.adjective% %coins%.
  %echoaround% %actor% ~%actor% sets the price to %val% %empire.adjective% %coins%.
end
~
#5142
Chant of Magic (gain natural magic at a henge)~
2 c 0
chant~
* Chant of Magic gains the player their 1st point of natural magic
if !(magic /= %arg%)
  return 0
  halt
end
if !%room.complete%
  %send% %actor% The building isn't even finished.
  halt
end
if !%actor.canuseroom_guest%
  %send% %actor% You don't have permission to chant here.
  halt
end
if !%room.in_city(true)% && %room.bld_flagged(IN-CITY-ONLY)%
  %send% %actor% You can't perform the chant of magic because the building isn't in a city.
  halt
end
if %actor.fighting% || %actor.disabled%
  %send% %actor% You're a bit busy right now.
  halt
end
if %actor.position% != Standing
  %send% %actor% You need to be standing.
  halt
end
* ready:
set cycles_left 13
while %cycles_left% >= 0
  if (%actor.room% != %room%) || !%actor.can_act%
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 13
      %echoaround% %actor% |%actor% chant is interrupted.
      %send% %actor% Your chant is interrupted.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  *
  * messaging
  switch %cycles_left%
    case 13
      %send% %actor% You start the Chant of Magic in the shadow of the henge...
      %echoaround% %actor% ~%actor% starts the Chant of Magic beneath the henge...
    break
    case 12
      %send% %actor% You chant, 'Feel the mana stir within, a whisper of the earth's embrace,' as you walk in and out of the henge...
      %echoaround% %actor% ~%actor% chants, 'Feel the mana stir within, a whisper of the earth's embrace,' as &%actor% walks in and out of the henge...
    break
    case 11
      %send% %actor% You chant, 'Raise your hands to the sky, beckoning the energies untold.'
      %echoaround% %actor% ~%actor% chants, 'Raise your hands to the sky, beckoning the energies untold.'
    break
    case 10
      %send% %actor% You chant, 'Step lightly, dance with the winds, as nature's rhythm unfolds.'
      %echoaround% %actor% ~%actor% chants, 'Step lightly, dance with the winds, as nature's rhythm unfolds.'
    break
    case 9
      %send% %actor% Your voice falters as you chant, 'In every leaf, in every breeze, the mana's secret stories reside.'
      %echoaround% %actor% ~%actor%'s voice falters as &%actor% chants, 'In every leaf, in every breeze, the mana's secret stories reside.'
    break
    case 8
      %send% %actor% You dance across the henge, chanting, 'Embrace the essence, become one with the ebb and flow.'
      %echoaround% %actor% ~%actor% dances across the henge, chanting, 'Embrace the essence, become one with the ebb and flow.'
    break
    case 7
      %send% %actor% You look up, saying, 'Chant softly, words that echo in the language of the land.'
      %echoaround% %actor% ~%actor% looks up, saying, 'Chant softly, words that echo in the language of the land.'
    break
    case 6
      %send% %actor% You chants, 'Sense the currents, weave the threads, as mana responds to your command.'
      %echoaround% %actor% ~%actor% chants, 'Sense the currents, weave the threads, as mana responds to your command.'
    break
    case 5
      %echo% The henge seems to hum, resonating with |%actor% footsteps.
    break
    case 4
      %send% %actor% You chant, 'In this dance of mana, the path to magic unfolds.'
      %echoaround% %actor% ~%actor% chants, 'In this dance of mana, the path to magic unfolds.'
    break
    case 3
      %send% %actor% You fall into the grass beneath the henge as colors swirl around you...
      %echoaround% %actor% ~%actor% falls into the grass beneath the henge...
    break
    case 2
      %send% %actor% The sun rises and sets and rises and sets above you as the stars act out a strange play...
      %echoaround% %actor% ~%actor% stares up at the sky...
    break
    case 1
      %send% %actor% The fragrant aroma of the world's colors fades from your eyes, but not your mind, as you slowly climb to your feet.
      %echoaround% %actor% ~%actor% slowly climbs to ^%actor% feet.
    break
    case 0
      * chant complete
      if %actor.skill(Natural Magic)% < 1
        %send% %actor% &&gAs you finish the chant, you finally to see the weave of mana through nature...&&0
        nop %actor.gain_skill(Natural Magic,1)%
        if %actor.skill(Natural Magic)% < 1
          %send% %actor% But you fail to grasp the concepts of the Chant of Magic (you cannot gain Natural Magic skill).
        end
      else
        %send% %actor% You finish the chant.
      end
      halt
    break
  done
  *
  * cycle cleanup
  eval cycles_left %cycles_left% - 1
  wait 5 sec
done
~
#5143
Chant of Magics 2nd person (denial)~
2 c 0
chant~
if !(magic /= %arg%)
  return 0
  halt
end
%send% %actor% Only one person can perform the Chant of Magic at a time.
~
#5149
Library commands~
2 c 0
browse checkout shelve~
* just passes through commands to the library command
return 1
if browse /= %cmd%
  %force% %actor% library browse %arg%
elseif checkout /= %cmd%
  %force% %actor% library checkout %arg%
elseif shelve /= %cmd%
  %force% %actor% library shelve %arg%
else
  return 0
end
~
#5156
Swamp Platform~
2 o 100
~
%echo% The platform is complete and the area is now plains.
%terraform% %room% 0
return 0
~
#5161
Baths: Clothes / bathing process~
1 ab 100
~
* this runs in a loop
while %self.val1% > 0
  * update timer
  eval timer %self.val1% - 1
  nop %self.val1(%timer%)%
  * check vars
  makeuid actor %self.val0%
  if !%actor%
    * gone?
    %purge% %self%
    halt
  elseif %actor.id% != %self.val0% || %actor.room% != %self.room% || %actor.stop_command%
    * moved or stopped?
    %purge% %self%
    halt
  elseif %actor.fighting% || %actor.position% == Sleeping
    * bad pos
    %purge% %self%
    halt
  end
  * next message
  switch %timer%
    case 3
      %send% %actor% You swim through the water...
      %echoaround% %actor% ~%actor% swims through the water...
    break
    case 2
      %send% %actor% You scrub your hair to get out any dirt and insects...
      %echoaround% %actor% ~%actor% scrubs ^%actor% hair to get out any dirt and insects...
    break
    case 1
      %send% %actor% You wash yourself off...
      %echoaround% %actor% ~%actor% washes *%actor%self off carefully...
    break
    case 0
      %send% %actor% You finish bathing and climb out of the water to dry off.
      %echoaround% %actor% ~%actor% finishes bathing and climbs out of the water to dry off.
      * cancel stop
      set needs_stop_command 0
      remote needs_stop_command %actor.id%
      * done
      dg_affect #5162 %actor% off
      dg_affect #5162 %actor% DEXTERITY 1 1800
      dg_affect #5162 %actor% CHARISMA 1 1800
      %purge% %self%
      halt
    break
  done
  wait 8 s
done
~
#5162
Baths: Bathe command~
2 c 0
bathe~
set valid_positions Standing Sitting Resting
if %actor.action% || %actor.fighting% || !(%valid_positions% ~= %actor.position%)
  %send% %actor% You can't do that right now.
  halt
elseif !%actor.canuseroom_guest%
  %send% %actor% You wouldn't want to get caught bathing in here.
  halt
end
* ok bathe:
%load% obj 5162
set obj %room.contents%
* ensure it worked
if %obj.vnum% != 5162
  %send% %actor% You don't seem to be able to bathe here.
  halt
end
* update data on obj
nop %obj.val0(%actor.id%)%
nop %obj.val1(4)%
* set up 'stop' command
set stop_command 0
set stop_message_char You climb out of the water and get dressed.
set stop_message_room ~%actor% climbs out of the water and gets dressed.
set needs_stop_command 1
remote stop_command %actor.id%
remote stop_message_char %actor.id%
remote stop_message_room %actor.id%
remote needs_stop_command %actor.id%
* start messages
if %actor.eq(clothes)%
  %send% %actor% You undress and climb into the water...
  %echoaround% %actor% ~%actor% undresses and climbs into the water...
else
  %send% %actor% You climb into the water...
  %echoaround% %actor% ~%actor% climbs into the water...
end
~
#5164
Sorcery Tower Completion~
2 o 100
~
if !%room.up(room)%
  * Add Top of the Tower
  %door% %room% up add 5511
end
detach 5164 %room.id%
~
#5186
Haven Interior~
2 o 100
~
eval firsthall %%room.%room.enter_dir%(room)%%
* Add first hallway
if !%firsthall%
  %door% %room% %room.enter_dir% add 5605
end
eval firsthall %%room.%room.enter_dir%(room)%%
if !%firsthall%
  * Failed to add
  halt
end
* Add study
eval study %%firsthall.%room.enter_dir%(room)%%
if !%study%
  %door% %firsthall% %room.enter_dir% add 5608
end
* Add basement tunnel
set basement %firsthall.down(room)%
if !%basement%
  %door% %firsthall% down add 5612
end
set basement %firsthall.down(room)%
if !%basement%
  * Failed to add
  halt
end
* Add bedroom
set edir %room.bld_dir(east)%
eval bedroom %%basement.%edir%(room)%%
if !%bedroom%
  %door% %basement% %edir% add 5601
end
* Add crypt
set sdir %room.bld_dir(south)%
eval crypt %%basement.%sdir%(room)%%
if !%crypt%
  %door% %basement% %sdir% add 5620
end
detach 5186 %room.id%
~
#5187
Tunnel Haven Interior~
2 o 100
~
eval firsthall %%room.%room.enter_dir%(room)%%
* Add first tunnel
if !%firsthall%
  %door% %room% %room.enter_dir% add 5612
end
eval firsthall %%room.%room.enter_dir%(room)%%
if !%firsthall%
  * Failed to add
  halt
end
* Add study
eval study %%firsthall.%room.enter_dir%(room)%%
if !%study%
  %door% %firsthall% %room.enter_dir% add 5608
end
* Add basement tunnel
set basement %firsthall.down(room)%
if !%basement%
  %door% %firsthall% down add 5612
end
set basement %firsthall.down(room)%
if !%basement%
  * Failed to add
  halt
end
* Add bedroom
set edir %room.bld_dir(east)%
eval bedroom %%basement.%edir%(room)%%
if !%bedroom%
  %door% %basement% %edir% add 5601
end
* Add crypt
set sdir %room.bld_dir(south)%
eval crypt %%basement.%sdir%(room)%%
if !%crypt%
  %door% %basement% %sdir% add 5620
end
detach 5187 %room.id%
~
#5191
Oasis Drainage: Completing building converts to Dry Oasis~
2 o 100
~
%echo% The drainage is complete. The plants begin to die as the oasis dries up.
%terraform% %room% 82
return 0
~
#5195
Equipment Cruncher: shatter~
2 c 0
shatter~
eval target %%actor.obj_target(%arg%)%%
if !%target%
  %send% %actor% Convert what into shards?
  halt
end
if !%target.carried_by%
  %send% %actor% Pick @%target% up first.
  halt
end
if !%target.wearable%
  %send% %actor% @%target% is not an equipment item.
  halt
end
if %target.is_flagged(*keep)%
  %send% %actor% You can not shatter something you are keeping.
  halt
end
* 26 ~ 200
eval level_modified %target.level% + 25
eval level_in_range (%level_modified% // 100)
if %level_in_range% == 0
  set level_in_range 100
end
eval shard_value %level_in_range% + 25
if %target.is_flagged(HARD-DROP)%
  eval shard_value %shard_value% + 25
end
if %target.is_flagged(GROUP-DROP)%
  eval shard_value %shard_value% + 50
end
* 0~75: 5100
* 76~175: 5101
* 176~275: 5102
* 276~375: 5103
eval shard_type 5100 + ((%level_modified%-1) / 100)
eval currency_name %%currency.%shard_type%%%
if !%currency_name% || %currency_name% == UNKNOWN
  %send% %actor% @%target% is too high level to convert into any current type of shards!
  halt
end
if %shard_value% < 1 || %target.quest% || %target.level% < 25
  %send% %actor% @%target% can't be converted into shards.
  halt
end
%send% %actor% You shatter @%target% into %shard_value% %currency_name%.
%echoaround% %actor% ~%actor% shatters @%target% into %currency_name%.
eval money %%actor.give_currency(%shard_type%, %shard_value%)%%
nop %money%
%purge% %target%
~
$
