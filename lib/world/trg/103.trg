#10300
Flame Dragon Terrorize~
0 ab 10
~
if (%self.fighting% || %self.disabled%)
  halt
end
set room %self.room%
if (%instance.location% && (%room.template% == 10300 || (%room% != %instance.location% && %random.10% == 10)))
  %echo% ~%self% flies away!
  mgoto %instance.location%
  %echo% ~%self% flies into the cave!
elseif %room.sector_vnum% == 7 || %room.sector_vnum% == 13 || %room.sector_vnum% == 15 || %room.sector_vnum% == 16
  %echo% ~%self% scorches the crops!
  %terraform% %room% 10303
elseif %room.sector_vnum% == 12 || %room.sector_vnum% == 14
  %echo% ~%self% scorches the crops!
  %terraform% %room% 10304
elseif %room.sector_vnum% == 20
  %echo% ~%self% scorches the desert!
  %terraform% %room% 10305
elseif (%room.sector_vnum% >= 1 && %room.sector_vnum% <= 4) || %room.sector_vnum% == 27 || %room.sector_vnum% == 28 || %room.sector_vnum% == 44 || %room.sector_vnum% == 45
  %echo% ~%self% scorches the trees!
  %terraform% %room% 10300
elseif %room.sector_vnum% == 26
  %echo% ~%self% scorches the grove!
  %terraform% %room% 10301
elseif %room.sector_vnum% == 0 || %room.sector_vnum% == 40
  %echo% ~%self% scorches the plains!
  %terraform% %room% 10302
end
~
#10301
Flame Dragon Start Progression: room~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10300)%
end
~
#10302
Flame Dragon combat~
0 k 5
~
set chance %random.3%
if %chance% < 3
  * Searing burns on tank
  %echo% ~%self% spits fire at ~%actor%, causing searing burns!
  %dot% %actor% 100 60 fire
  %damage% %actor% 75 fire
else
  * Flame wave AoE
  %echo% ~%self% begins puffing smoke and spinning in circles!
  * Give the healer (if any) time to prepare for group heals
  wait 3 sec
  %echo% ~%self% unleashes a flame wave!
  %aoe% 100 fire
end
~
#10303
Flame Dragon delay-completer~
0 f 100
~
if %instance.start%
  * Attempt delayed despawn
  %at% %instance.start% %load% o 10316
else
  %adventurecomplete%
end
~
#10304
Flame Dragon environmental~
0 bw 5
~
* This script is no longer used. It was replaced by custom strings.
if (%self.fighting% || %self.disabled%)
  halt
end
switch %random.4%
  case 1
    %echo% ~%self% spurts fire into the air.
  break
  case 2
    %echo% ~%self% curls up and begins puffing clouds of smoke.
  break
  case 3
    %echo% ~%self% hunkers down and starts coughing out bits of ash.
  break
  case 4
    %echo% ~%self% coughs up some charred bone fragments.
  break
done
~
#10305
Flame Dragon Start Progression: mob~
0 h 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10300)%
end
~
#10307
Flame dragon despawn timer~
1 f 0
~
%adventurecomplete%
~
#10330
Abandoned Dragon Fly Home~
0 ab 10
~
if (%self.fighting% || %self.disabled%)
  halt
end
set room %self.room%
if (%instance.real_location% && %room% != %instance.real_location% && (%room.template% == 10330 || %room.sector% == Ocean))
  %echo% ~%self% flies away!
  mgoto %instance.real_location%
  nop %instance.set_location(%instance.real_location%)%
  %echo% ~%self% flies into the nest!
else
  nop %instance.set_location(%room%)%
end
~
#10331
Abandoned Nest Spawner~
1 n 100
~
eval vnum 10330 + %random.4% - 1
%load% m %vnum%
%purge% %self%
~
#10332
Abandon Dragon Start Progression: room~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10330)%
end
~
#10333
Abandon Dragon Start Progression: mob~
0 h 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10330)%
end
~
#10334
Abandoned Dragon animation~
0 bw 5
~
if (%self.fighting% || %self.disabled%)
  halt
end
switch %random.4%
  case 1
    %echo% ~%self% flies in circles overhead.
  break
  case 2
    %echo% ~%self% spurts fire into the air.
  break
  case 3
    %echo% ~%self% eyes you warily.
  break
  case 4
    %echo% ~%self% swoops low, then soars back into the air.
  break
done
~
#10335
Dragon Whistle use~
1 c 2
use~
if !%self.is_name(%arg%)%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
%load% m %self.val0%
%send% %actor% You use @%self% and a dragon mount appears!
%echoaround% %actor% ~%actor% uses @%self% and a dragon mount appears!
%purge% %self%
~
#10336
Non-Mount Summon~
1 c 2
use~
if !%self.is_name(%arg%)%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
%load% m %self.val0%
set mob %self.room.people%
if (%mob% && %mob.vnum% == %self.val0%)
  %send% %actor% You use @%self% and ~%mob% appears!
  %echoaround% %actor% ~%actor% uses @%self% and ~%mob% appears!
  nop %mob.unlink_instance%
end
%purge% %self%
~
#10337
Fire Ox animation~
0 bw 5
~
if (%self.fighting% || %self.disabled%)
  halt
end
if (%random.2% == 2)
  %echo% ~%self% releases a demonic moo, and fire spurts from ^%self% nostrils.
else
  * We need the current terrain.
  set room %self.room%
  if (%room.sector% == Plains || %room.sector% ~= Forest)
    %echo% ~%self% scorches some grass, and eats it.
  else
    %echo% ~%self% spurts fire from ^%self% nostrils.
  end
end
~
#10338
Dragonguard animation~
0 bw 5
~
if (%self.fighting% || %self.disabled%)
  halt
end
switch %random.5%
  case 1
    say The only thing that would make this day more beautiful is fire, raining from the sky.
  break
  case 2
    say Pleasant and eternal greetings.
  break
  case 3
    say May the burning eye watch over you.
  break
  case 4
    say All humans must die. Eventually.
  break
  case 5
    say Mortals tremble before the Dragonguard.
  break
done
~
#10339
Empire Non-Mount Summon~
1 c 2
use~
if !%self.is_name(%arg%)%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
%load% m %self.val0%
set mob %self.room.people%
if (%mob% && %mob.vnum% == %self.val0%)
  %own% %mob% %actor.empire%
  %send% %actor% You use @%self% and ~%mob% appears!
  %echoaround% %actor% ~%actor% uses @%self% and ~%mob% appears!
end
%purge% %self%
~
#10370
Uninvited Guest: Delayed despawn box~
1 f 0
~
%adventurecomplete%
%load% obj 10371
return 0
%purge% %self%
~
#10371
Uninvited Guest: Coffin collapsed~
1 n 100
~
%echo% The long box on the floor collapses into a pile of boards.
wait 1 s
set vampire %self.room.people(10370)%
if %vampire% && !%vampire.aff_flagged(!SEE)%
  %force% %vampire% say Well that isn't good. Guess it's time to move on.
end
while %vampire%
  wait 300 s
  set vampire %self.room.people(10370)%
  if %vampire%
    if !%vampire.fighting%
      if !%vampire.aff_flagged(!SEE)%
        %echo% ~%vampire% leaves.
      end
      %purge% %vampire%
    end
  end
done
~
#10372
Uninvited Guest: Complete on death~
0 f 100
~
%adventurecomplete%
~
#10373
Uninvited Guest: Box commands~
1 c 4
look open close~
return 0
if close /= %cmd%
  if %actor.obj_target(%arg.car%)% == %self%
    %send% %actor% It's already closed.
    return 1
  end
elseif open /= %cmd%
  if %actor.obj_target(%arg.car%)% == %self%
    %send% %actor% You can't seem to get the box open. It's almost as if it's magically sealed.
    return 1
  end
elseif look /= %cmd% && %arg.car% == in
  set arg %arg.cdr%
  if %actor.obj_target(%arg.car%)% == %self%
    %send% %actor% You can't seem to get the box open to look inside.
    return 1
  end
end
~
#10374
Uninvited Guest: Vampire wake/sleep~
0 b 50
~
set room %self.room%
set sun %room.sun%
set affected %self.affect(10370)%
set box %room.contents(10370)%
if %sun% == dark && %affected% && %box%
  dg_affect #10370 %self% off
  %echo% @%box% opens and ~%self% climbs out.
  wait 1 s
  emote $n yawns and stretches.
elseif %sun% != dark && !%affected% && %box%
  say If you'll excuse me...
  wait 1 s
  %echo% ~%self% opens the lid of @%box%, climbs inside, and slams the lid shut.
  dg_affect #10370 %self% !SEE on -1
end
~
#10375
Uninvited Guest: Bite in combat~
0 k 33
~
set room %self.room%
if (%actor.health% * 100 / %actor.maxhealth%) > 10
  * over 10% health
  halt
elseif !%self.vampire% || %actor.is_npc% || %actor.vampire% || %room.sun% != dark || %actor.nohassle%
  halt
elseif !%actor.can_gain_new_skills% || %actor.noskill(Vampire)%
  halt
elseif %actor.aff_flagged(!DRINK-BLOOD)%
  * Don't bite !DRINK-BLOOD targets
  halt
end
%send% %actor% ~%self% lunges forward and sinks ^%self% teeth into your neck!
%echoaround% %actor% ~%self% lunges forward and sinks ^%self% teeth into |%actor% neck!
* Strings copied from sire_char
%send% %actor% You fall limply to the ground. In the distance, you think you see a light...
%echoaround% %actor% ~%actor% drops limply from |%self% fangs...
%echoaround% %actor% ~%self% tears open ^%self% wrist with ^%self% teeth and drips blood into |%actor% mouth!
%send% %actor% &&rSuddenly, a warm sensation touches your lips and a stream of blood flows down your throat...&&0
%send% %actor% &&rAs the blood fills you, a strange sensation covers your body... The light in the distance turns blood-red and a hunger builds within you!&&0
nop %actor.vampire(on)%
dg_affect %self% STUNNED on 5
~
#10376
Uninvited Guest: Custom one-time greetings using script1~
0 hnwA 100
~
* Uses mob custom script1 to for one-time greetings, with each script1 line
*   sent every %line_gap% (9 sec) until it runs out of strings. The mob will
*   be SENTINEL and SILENT during this period.
* usage: .custom add script1 <command> <string>
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
if %self.fighting% || %self.disabled% || %self.aff_flagged(!SEE)%
  halt
end
* check for someone who needs the greeting
set any 0
set ch %room.people%
while %ch%
  if %ch.is_pc%
    set varname greet_%ch.id%
    if !%self.varexists(%varname%)%
      set any 1
      set %varname% 1
      remote %varname% %self.id%
    end
  end
  set ch %ch.next_in_room%
done
* did anyone need the intro
if !%any%
  halt
end
* greeting detected: prepare (storing as variables prevents reboot issues)
if !%self.mob_flagged(SILENT)%
  set no_silent 1
  remote no_silent %self.id%
  nop %self.add_mob_flag(SILENT)%
end
* Show the script1 text
* tell story
set pos 0
set msg %self.custom(script1,%pos%)%
while !%msg.empty%
  * check early end
  if %self.disabled% || %self.fighting% || %self.aff_flagged(!SEE)%
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
  set msg %self.custom(script1,%pos%)%
  if %waits% && %msg%
    wait %line_gap%
  end
done
* Done: mark as greeted for anybody now present
set ch %room.people%
while %ch%
  if %ch.is_pc%
    set varname greet_%ch.id%
    set %varname% 1
    remote %varname% %self.id%
  end
  set ch %ch.next_in_room%
done
* Done: cancel silent
if %self.varexists(no_silent)%
  nop %self.remove_mob_flag(SILENT)%
end
~
$
