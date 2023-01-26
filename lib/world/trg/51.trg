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
Oasis Drainage~
2 o 100
~
%echo% The drainage is complete and the area is now a desert.
%terraform% %room% 20
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
