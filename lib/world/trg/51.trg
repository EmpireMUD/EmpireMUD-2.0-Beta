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
#5142
Chant of Druids (gain natural magic at a henge)~
2 c 0
chant~
* Chant of Druids gains the player their 1st point of natural magic
if !(druids /= %arg%)
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
  %send% %actor% You can't perform the chant of druids because the building isn't in a city.
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
      %send% %actor% You start the chant of druids...
      %echoaround% %actor% ~%actor% starts the chant of druids...
    break
    case 12
      %send% %actor% You dance around the henge...
      %echoaround% %actor% ~%actor% dances around the henge...
    break
    case 11
      %send% %actor% You recite the chant of druids as best you can...
      %echoaround% %actor% ~%actor% recites the chant of druids...
    break
    case 10
      %send% %actor% You chant as you dance in and out of the henge's ring...
      %echoaround% %actor% ~%actor% chants as &%actor% dances in and out of the henge's ring...
    break
    case 9
      %send% %actor% Your voice falters as you try to remember all the words to the chant...
      %echoaround% %actor% ~%actor% recites the chant of druids to the best of ^%actor% ability...
    break
    case 8
      %send% %actor% You hum and chant as you dance in and out of the henge's ring...
      %echoaround% %actor% ~%actor% hums and chants as &%actor% dances in and out of the henge's ring...
    break
    case 7
      %send% %actor% You begin the second verse of the chant of druids...
      %echoaround% %actor% ~%actor% begins the second verse of the chant of druids...
    break
    case 6
      %send% %actor% You make percussion noises with your mouth between lines of the chant...
      %echoaround% %actor% ~%actor% makes percussion noises with ^%actor% mouth...
    break
    case 5
      %send% %actor% You reach the chorus and recite the chant of druids loudly...
      %echoaround% %actor% ~%actor% recites the chant of druids as loud as &%actor% can...
    break
    case 4
      %send% %actor% You chant as you dance in and out of the henge's ring...
      %echoaround% %actor% ~%actor% chants as &%actor% dances in and out of the henge's ring...
    break
    case 3
      %send% %actor% You chant the final verse of the chant of druids...
      %echoaround% %actor% ~%actor% chants the final verse of the chant of druids...
    break
    case 2
      %send% %actor% You spin in place, waving your arms as you near the end of the chant of druids...
      %echoaround% %actor% ~%actor% spins in place, waving ^%actor% arms up and down as &%actor% chants...
    break
    case 1
      %send% %actor% You collapse in the center of the henge, exhausted from the chant.
      %echoaround% %actor% ~%actor% collapses in the center of the henge.
    break
    case 0
      * chant complete
      if %actor.skill(Natural Magic)% < 1
        %send% %actor% &&gAs you finish the chant, you begin to see the weave of mana through nature...&&0
        nop %actor.gain_skill(Natural Magic,1)%
        if %actor.skill(Natural Magic)% < 1
          %send% %actor% But you fail to grasp the concepts of the chant of druids (you cannot gain Natural Magic skill).
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
Chant of Druids 2nd person (denial)~
2 c 0
chant~
if !(druids /= %arg%)
  return 0
  halt
end
%send% %actor% Only one person can perform the chant of druids at a time.
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
