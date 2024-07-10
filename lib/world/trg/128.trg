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
  %at% %toroom% %echo% A portal whirls open!
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
#12806
Celestial Forge: Require permission to enter portal~
1 c 4
enter~
return 0
if %actor.obj_target(%arg.argument1%)% != %self% || %self.val0% <= 0
  halt
end
* find target
makeuid toroom room %self.val0%
if !%toroom%
  halt
end
* validate permission
if !%actor.canuseroom_guest(%toroom%)%
  %send% %actor% You don't have permission to enter that portal.
  return 1
  halt
end
* otherwise they're good to go!
~
#12807
Celestial Forge: One-time greet using script1 or script2~
0 gh 100
~
* Uses mob custom script1 for a once-ever greeting per character and script2
*   for players who have been to the instance before. Each script1/2 line
*   sent every %line_gap% (9 sec) until it runs out of strings. The mob will
*   be SENTINEL and SILENT during this period.
* usage: .custom add <script1 | script2> <command> <string>
* valid commands: say, emote, do (execute command), echo (script), skip, attackable
* also: vforce <mob vnum in room> <command>
* also: set line_gap <time> sec
* also: mod <field> <value> -- runs %mod% %self%
* NOTE: waits for %line_gap% (9 sec) after all commands EXCEPT do/vforce/set/attackable/mod
set line_gap 9 sec
* begin
if %actor.is_npc%
  halt
end
set room %self.room%
* let everyone arrive
wait 0
if %self.fighting% || %self.disabled%
  halt
end
* check for someone who needs the greeting
set any_first 0
set any_repeat 0
set ch %room.people%
while %ch%
  if %ch.is_pc%
    * check first visit
    set 12800_visits %ch.var(12800_visits)%
    if !(%12800_visits% ~= %self.vnum%)
      set any_first 1
      set 12800_visits %12800_visits% %self.vnum%
      remote 12800_visits %ch.id%
    end
    * check repeat visit
    set varname greet_%ch.id%
    if !%self.varexists(%varname%)%
      set any_repeat 1
      set %varname% 1
      remote %varname% %self.id%
    end
  end
  set ch %ch.next_in_room%
done
* did anyone need the intro
if %any_first%
  set stype script1
elseif %any_repeat%
  set stype script2
else
  halt
end
* greeting detected: prepare (storing as variables prevents reboot issues)
if !%self.mob_flagged(SENTINEL)%
  set no_sentinel 1
  remote no_sentinel %self.id%
  nop %self.add_mob_flag(SENTINEL)%
end
if !%self.mob_flagged(SILENT)%
  set no_silent 1
  remote no_silent %self.id%
  nop %self.add_mob_flag(SILENT)%
end
* Show the scripted text
* tell story
set pos 0
set msg %self.custom(%stype%,%pos%)%
while !%msg.empty%
  * check early end
  if %self.disabled% || %self.fighting%
    halt
  end
  * next message
  set mode %msg.car%
  set msg %msg.cdr%
  if %mode% == say
    say %msg%
    set waits 1
  elseif %mode% == do
    %msg.process%
    set waits 0
  elseif %mode% == echo
    %echo% %msg.process%
    set waits 1
  elseif %mode% == vforce
    set vnum %msg.car%
    set msg %msg.cdr%
    set targ %self.room.people(%vnum%)%
    if %targ%
      %force% %targ% %msg.process%
    end
    set waits 0
  elseif %mode% == emote
    emote %msg%
    set waits 1
  elseif %mode% == set
    set subtype %msg.car%
    set msg %msg.cdr%
    if %subtype% == line_gap
      set line_gap %msg%
    else
      %echo% ~%self%: Invalid set type '%subtype%' in storytime script.
    end
    set waits 0
  elseif %mode% == skip
    * nothing this round
    set waits 1
  elseif %mode% == attackable
    if %self.aff_flagged(!ATTACK)%
      dg_affect %self% !ATTACK off
    end
    set waits 0
  elseif %mode% == mod
    %mod% %self% %msg.process%
    set waits 0
  else
    %echo% %self.name%: Invalid script message type '%mode%'.
  end
  * fetch next message and check wait
  eval pos %pos% + 1
  set msg %self.custom(%stype%,%pos%)%
  if %waits% && %msg%
    wait %line_gap%
  end
done
* Done: mark as greeted for anybody now present (but NOT first-greet)
set ch %room.people%
while %ch%
  if %ch.is_pc%
    set varname greet_%ch.id%
    set %varname% 1
    remote %varname% %self.id%
  end
  set ch %ch.next_in_room%
done
* Done: cancel sentinel/silent
if %self.varexists(no_sentinel)%
  nop %self.remove_mob_flag(SENTINEL)%
end
if %self.varexists(no_silent)%
  nop %self.remove_mob_flag(SILENT)%
end
~
#12810
Celestial Forge: Mine attempt~
2 c 0
mine~
%send% %actor% There doesn't seem to be anything left here to mine.
~
$
