#11500
Clicky Pen~
1 c 2
click~
if !%arg%
  %send% %actor% Click what?
  halt
end
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
%send% %actor% You click %self.shortdesc%... ah, that feels good.
%echoaround% %actor% %actor.name% makes an annoying clicky sound with %actor.hisher% pen.
nop %actor.gain_event_points(11500,1)%
~
#11505
Start Supplies for GoA quests~
2 u 0
~
if !%actor.inventory(11505)%
  %load% obj 11505 %actor%
  %send% %actor% You receive a list of supplies for the Guild.
end
~
#11513
small shipment of resources~
1 n 100
~
wait 0
* uses val0 (quantity of common items), val1 (quantity of rare items)
if !%self.carried_by%
  %purge% %self%
  halt
end
set pers %self.carried_by%
set prc %random.1000%
set amt %self.val0%
set vnum -1
if %prc% <= 51
  * copper bar
  set vnum 179
  set amt %self.val1%
elseif %prc% <= 124
  * lettuce oil
  set vnum 3362
elseif %prc% <= 197
  * rope
  set vnum 2038
elseif %prc% <= 270
  * lumber
  set vnum 125
elseif %prc% <= 343
  * pillar
  set vnum 129
elseif %prc% <= 416
  * tin ingot
  set vnum 167
elseif %prc% <= 489
  * nails
  set vnum 1306
elseif %prc% <= 562
  * obsidian
  set vnum 106
  set amt %self.val1%
elseif %prc% <= 635
  * rock
  set vnum 100
elseif %prc% <= 708
  * small leather
  set vnum 1356
elseif %prc% <= 781
  * block
  set vnum 105
elseif %prc% <= 854
  * bricks
  set vnum 257
elseif %prc% <= 927
  * fox fur
  set vnum 1352
  set amt %self.val1%
else
  * big cat pelt
  set vnum 1353
  set amt %self.val1%
end
if %amt% < 0 || %vnum% == -1
  halt
end
nop %pers.add_resources(%vnum%,%amt%)%
set obj %pers.inventory(%vnum%)%
if %obj%
  %send% %pers% You find %amt%x %obj.shortdesc% in %self.shortdesc%!
end
%purge% %self%
~
#11514
great shipment of resources~
1 n 100
~
wait 0
* uses val0 (quantity of common items), val1 (quantity of rare items)
if !%self.carried_by%
  %purge% %self%
  halt
end
set pers %self.carried_by%
set prc %random.1000%
set amt %self.val0%
set vnum -1
if %prc% <= 5
  * crystal seed (1 only)
  set vnum 600
  set amt 1
elseif %prc% <= 20
  * gold bar
  set vnum 173
  set amt %self.val1%
elseif %prc% <= 90
  * silver bar
  set vnum 172
  set amt %self.val1%
elseif %prc% <= 160
  * magewood
  set vnum 604
elseif %prc% <= 230
  * sparkfiber
  set vnum 612
elseif %prc% <= 300
  * manasilk
  set vnum 613
elseif %prc% <= 370
  * briar hide
  set vnum 614
elseif %prc% <= 440
  * absinthian leather
  set vnum 615
elseif %prc% <= 510
  * nocturnium
  set vnum 176
elseif %prc% <= 580
  * imperium
  set vnum 177
elseif %prc% <= 650
  * distilled whitegrass
  set vnum 1210
elseif %prc% <= 720
  * fiveleaf
  set vnum 1211
elseif %prc% <= 790
  * redthorn
  set vnum 1212
elseif %prc% <= 860
  * magewhisper
  set vnum 1213
elseif %prc% <= 930
  * daggerbite
  set vnum 1214
else
  * bileberry
  set vnum 1215
end
if %amt% < 0 || %vnum% == -1
  halt
end
nop %pers.add_resources(%vnum%,%amt%)%
set obj %pers.inventory(%vnum%)%
if %obj%
  %send% %pers% You find %amt%x %obj.shortdesc% in %self.shortdesc%!
end
%purge% %self%
~
$
