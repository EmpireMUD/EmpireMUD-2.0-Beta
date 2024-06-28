#12800
Celestial Forge: Donate to open portal~
0 c 0
donate~
set room %self.room%
set which 0
set dest 0
* validate argument
if !%arg%
  %send% %actor% Donate to which celestial forge? (iron, ...)
elseif iron forge /= %arg% || lodestone forge /= %arg%
  set which 12800
  set dest 12810
  set curr 5100
  set str an iron shard
else
  %send% %actor% Unknown celestial forge.
end
* did we find one?
if !%which% || !%dest%
  halt
end
* validate target
if %room.contents(%which%)%
  %send% %actor% There is already a portal to that celestial forge here.
  halt
elseif %actor.currency(%curr%)% < 1
  eval curname %%currency.%curr%(1)%%
  %send% %actor% You don't even have %curname.ana% %curname% to donate!
  halt
end
set toroom %instance.nearest_rmt(%dest%)%
if !%toroom%
  %send% %actor% The forge is not accepting donations right now. Try again soon.
  halt
end
* create portal-in
%load% obj %which% %room%
set inport %room.contents%
if %inport.vnum% != %which%
  %send% %actor% Something went wrong.
  %log% syslog script Trig 12800 failed to open portal.
  halt
end
* charge
nop %actor.give_currency(%curr%, -1)%
* update portal-in
nop %inport.val0(%toroom.vnum%)%
%send% %actor% You donate %str% to the forge and @%inport% appears!
%echoaround% %actor% ~%actor% donates %str% to the forge and @%inport% appears!
* portal back
%load% obj 12806 %toroom%
set outport %toroom.contents%
if %outport% && %outport.vnum% == 12806
  nop %outport.val0(%room.vnum%)%
  set emp %room.empire_name%
  if %emp%
    %mod% %outport% shortdesc the portal back to %emp%
    %mod% %outport% longdesc The portal back to %emp% is open here.
    %mod% %outport% keywords portal back %emp%
  end
end
~
#12803
Celestial Forge: Time and Weather commands~
2 c 0
time weather~
if %cmd.mudcommand% == time
  if %room.template% == 12810
    %send% %actor% It looks like nighttime through the hole at the top of the tunnel.
  else
    %send% %actor% The beautiful night sky overhead tells you it's nighttime.
  end
  return 1
elseif %cmd.mudcommand% == weather
  if %room.template% == 12810
    %send% %actor% It's hard to tell the weather from in here.
  else
    %send% %actor% The night sky is cloudless and vast.
  end
  return 1
else
  * unknown command somehow?
  return 0
end
~
#12805
Celestial Forge: Immortal controller~
1 c 2
cforge~
if !%actor.is_immortal%
  %send% %actor% You lack the power to use this.
  halt
end
set mode %arg.car%
set arg2 %arg.cdr%
if goto /= %mode%
  * target handling
  if iron /= %arg2%
    set to_room %instance.nearest_rmt(12810)%
  else
    set to_room %instance.nearest_rmt(%arg2%)%
  end
  *
  if !%to_room%
    %send% %actor% Invalid target Celestial Forge room.
  elseif %to_room.template% < 12800 || %to_room.template% > 12999
    %send% %actor% You can only use this to goto a Celestial Forge room.
  else
    %echoaround% %actor% ~%actor% vanishes!
    %teleport% %actor% %to_room%
    %echoaround% %actor% ~%actor% appears in a flash!
    %force% %actor% look
  end
else
  %send% %actor% &&0Usage: cforge goto iron
  %send% %actor% &&0       cforge goto <template vnum>
end
~
#12810
Celestial Forge: Mine attempt~
2 c 0
mine~
%send% %actor% There doesn't seem to be anything left here to mine.
~
$
