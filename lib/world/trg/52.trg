#5222
Mountain Terrace construction~
2 o 100 2
L h 8
L h 95
~
if %room.base_sector_vnum% == 8 || %room.base_sector_vnum% == 52
  %terraform% %room% 95
else
  %echo% The terraces fail and collapse. (They my not be implemented for this terrain type.)
  %build% %room% demolish
end
~
$
