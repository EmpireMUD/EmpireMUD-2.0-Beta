#5138
Tavern Hideout Completion~
2 o 100
~
if !%room.down(room)%
  * Add Secret Hidout
  %door% %room% down add 5510
  eval hideout %room.down(room)%
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
eval basement %%firsthall.down(room)%%
if !%basement%
  %door% %firsthall% down add 5612
end
eval basement %%firsthall.down(room)%%
if !%basement%
  * Failed to add
  halt
end
* Add bedroom
eval edir %room.bld_dir(east)%
eval bedroom %%basement.%edir%(room)%%
if !%bedroom%
  %door% %basement% %edir% add 5601
end
* Add crypt
eval sdir %room.bld_dir(south)%
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
eval basement %%firsthall.down(room)%%
if !%basement%
  %door% %firsthall% down add 5612
end
eval basement %%firsthall.down(room)%%
if !%basement%
  * Failed to add
  halt
end
* Add bedroom
eval edir %room.bld_dir(east)%
eval bedroom %%basement.%edir%(room)%%
if !%bedroom%
  %door% %basement% %edir% add 5601
end
* Add crypt
eval sdir %room.bld_dir(south)%
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
$
