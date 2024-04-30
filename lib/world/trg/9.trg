#900
Shoddy Raft Drop~
1 n 100
~
%load% veh 923
set raft %self.room.vehicles%
if %raft% && %raft.vnum% == 923
  nop %raft.unlink_instance%
end
%purge% %self%
~
#916
Harvest Beehive~
5 c 0
harvest check~
if !%arg% || %actor.veh_target(%arg%)% != %self%
  return 0
  halt
end
* otherwise return 1 in all cases
return 1
* pull out variable
if %self.varexists(stored)%
  set stored %self.stored%
else
  set stored 0
end
* just checking honey?
if %cmd% == check
  if %stored% > 1
    %send% %actor% It looks like you could harvest %stored% honeycombs from this hive.
  elseif %stored% == 1
    %send% %actor% It looks like you could harvest 1 honeycomb from this hive.
  else
    %send% %actor% There's no honeycomb to spare in this hive.
  end
  halt
end
if %stored% < 1
  %send% %actor% There isn't enough honeycomb in %self.shortdesc% to harvest.
  halt
end
* prepare to extract
set got 0
set full 0
* now in a loop
while %stored% > 0 && !%full%
  if %actor.carrying% >= %actor.maxcarrying%
    set full 1
  else
    eval stored %stored% - 1
    eval got %got% + 1
    %load% obj 3069 %actor% inv
  end
done
* store var back
remote stored %self.id%
* messaging
if %got% > 1
  %send% %actor% You harvest some honeycomb (x%got%) from %self.shortdesc%.
  %echoaround% %actor% ~%actor% harvests some honeycomb from %self.shortdesc%.
elseif %got% == 1
  %send% %actor% You harvest some honeycomb from %self.shortdesc%.
  %echoaround% %actor% ~%actor% harvests some honeycomb from %self.shortdesc%.
end
if %full%
  %send% %actor% You can't carry any more.
end
~
#917
Convert Old Beehives~
1 hn 100
~
* default return is 1 -- unless we hit a particular condition
return 1
if %command% && %command% == junk
  halt
end
%load% veh 916
set hive %self.room.vehicles%
if %hive% && %hive.vnum% == 916
  nop %hive.unlink_instance%
end
wait 1
%purge% %self%
~
#918
Beehive Growth~
5 ab 2
~
* config: max honeycomb to store
set max 24
* ensure we can act here: outdoors or in apiary
if !%self.room.is_outdoors% && !%self.room.function(APIARY)%
  wait 3600 s
  halt
end
* pull out variable
if %self.varexists(stored)%
  set stored %self.stored%
else
  set stored 0
end
set season %self.room.season%
* season-based changes
if %season% == spring
  eval stored %stored% + 1
elseif %season% == summer
  eval stored %stored% + 2
elseif %season% == autumn
  eval stored %stored% + 1
elseif %season% == winter
  * no change over winter
end
if %stored% > %max%
  set stored %max%
end
remote stored %self.id%
if %stored% >= %max%
  * prevents the script from running for a while
  wait 7200 s
end
~
#919
Start Axe Tutorial~
2 u 0
~
* start flint tutorial
if !%actor.completed_quest(100)% && !%actor.on_quest(100)%
  %quest% %actor% start 100
end
* start handle tutorial
if !%actor.completed_quest(121)% && !%actor.on_quest(121)%
  %quest% %actor% start 121
end
~
#921
Caravan setup~
5 o 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 5518
end
detach 921 %self.id%
~
#929
bloodletter arrows~
1 s 25
~
set scale 100
if !%target%
  halt
end
%send% %target% You begin to bleed from the wound left by |%actor% bloodletter arrow!
%echoaround% %target% ~%target% begins to bleed from the wound left by |%actor% bloodletter arrow!
%dot% %target% %scale% 15 physical
~
#952
Caravel setup~
5 o 100
~
set inter %self.interior%
if (!%inter% || %inter.aft%)
  halt
end
%door% %inter% aft add 5500
detach 952 %self.id%
~
#953
Cog setup~
5 o 100
~
set inter %self.interior%
if (!%inter% || %inter.down%)
  halt
end
%door% %inter% down add 5502
detach 953 %self.id%
~
#954
Longship setup~
5 o 100
~
set inter %self.interior%
if (!%inter% || %inter.aft%)
  halt
end
%door% %inter% aft add 5500
detach 954 %self.id%
~
#955
Brigantine setup~
5 o 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 5512
end
if (!%inter.fore%)
  %door% %inter% fore add 5506
end
detach 955 %self.id%
~
#956
Carrack setup~
5 o 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 5513
end
if (!%inter.fore%)
  %door% %inter% fore add 5506
end
detach 956 %self.id%
~
#957
Hulk setup~
5 o 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 5500
end
if (!%inter.down%)
  %door% %inter% down add 5502
end
detach 957 %self.id%
~
$
