#9800
Generic No Sacrifice~
1 h 100
~
if %command% == sacrifice
  %send% %actor% @%self% cannot be sacrificed.
  return 0
  halt
end
return 1
~
#9801
Read looks at target~
1 c 6
read~
if %actor.obj_target(%arg%)% == %self%
  %force% %actor% look %arg%
  return 1
else
  return 0
end
~
#9802
Give rejection~
0 j 100
~
%send% %actor% You can't give items to ~%self%.
return 0
~
#9803
Companion dies permanently~
0 f 100
~
* This script deletes a companion's entry when it dies.
* If the companion comes from an ability (like Bodyguard) the player will
* get a fresh copy of the companion right away.
set ch %self.companion%
if %self.is_npc% && %ch% && !%ch.is_npc%
  %ch.remove_companion(%self.vnum%)%
end
~
#9804
Cancel follow on load (for summons)~
0 n 100
~
* Summons enter the game following (silentyl). This cancels that.
if %self.leader%
  mfollow self
end
detach 9804 %self.id%
~
#9805
Summoned mob is charmed~
0 n 100
~
* Adds a CHARMED flag to the mob
if %self.leader%
  dg_affect %self% *CHARM on -1
end
detach 9805 %self.id%
~
#9806
Detect stop command (objects)~
1 c 7
stop~
* Detects that a player has typed 'stop' for the purpose of a script.
* You must intialize the player's 'stop_command' variable to 0 (use set and remote)
* If you set 'needs_stop_command' to 1, this will also block the 'You can stop if you want to' message.
* You can also set 'stop_message_char' for what to show the player.
* You can also set 'stop_message_room' for what to show the player.
* 1. CHECK IF WE ARE JUST CLEARING DATA
if %arg% == cleardata
  set stop_command 0
  set needs_stop_command 0
  remote stop_command %actor.id%
  remote needs_stop_command %actor.id%
  rdelete stop_message_char %actor.id%
  rdelete stop_message_room %actor.id%
  return 1
  halt
end
* 2. NORMAL USE OF 'stop'
if %actor.varexists(needs_stop_command)%
  set needs_stop %actor.needs_stop_command%
else
  set needs_stop 0
end
* re-set needs_stop_command on the actor (prevents it from silencing repeatedly)
set needs_stop_command 0
remote needs_stop_command %actor.id%
* mark player as stopped
set stop_command 1
remote stop_command %actor.id%
* message if necessary
if %actor.varexists(stop_message_char)%
  %send% %actor% %actor.stop_message_char%
  rdelete stop_message_char %actor.id%
end
if %actor.varexists(stop_message_room)%
  %echoaround% %actor% %actor.stop_message_room%
  rdelete stop_message_room %actor.id%
end
* prevent basic stop if needed
if %needs_stop% && !%actor.action%
  * prevents basic 'stop' output
  return 1
else
  * will show normal 'stop' output
  return 0
end
~
#9807
No pickpocket while unkillable~
0 c 0
pickpocket~
if !%arg% || %actor.char_target(%arg.car%)% != %self%
  return 0
  halt
end
if %self.aff_flagged(!ATTACK)%
  %send% %actor% You can't pick ^%self% pockets right now.
  return 1
else
  return 0
end
~
#9850
Equip imm-only~
1 j 0
~
if !%actor.is_immortal%
  %send% %actor% You can't wear ~%self%.
  return 0
else
  return 1
end
~
#9851
Obj rescale on load~
1 n 100
~
%scale% %self% %self.level%
~
$
