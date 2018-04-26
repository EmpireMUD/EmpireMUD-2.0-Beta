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
  %send% %actor% Pick %target.shortdesc% up first.
  halt
end
if !%target.wearable%
  %send% %actor% %target.shortdesc% is not an equipment item.
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
  %send% %actor% %target.shortdesc% is too high level to convert into any current type of shards!
  halt
end
if %shard_value% < 1 || %target.quest% || %target.level% < 25
  %send% %actor% %target.shortdesc% can't be converted into shards.
  halt
end
%send% %actor% You shatter %target.shortdesc% into %shard_value% %currency_name%.
%echoaround% %actor% %actor.name% shatters %target.shortdesc% into %currency_name%.
eval money %%actor.give_currency(%shard_type%, %shard_value%)%%
nop %money%
%purge% %target%
~
$
