#10300
Flame Dragon Terrorize~
0 ab 10 93
L h 0
L h 1
L h 2
L h 3
L h 4
L h 7
L h 12
L h 13
L h 14
L h 20
L h 21
L h 23
L h 24
L h 25
L h 26
L h 32
L h 33
L h 34
L h 36
L h 37
L h 38
L h 39
L h 40
L h 44
L h 45
L h 46
L h 47
L h 50
L h 51
L h 54
L h 56
L h 58
L h 59
L h 60
L h 63
L h 64
L h 70
L h 71
L h 72
L h 73
L h 74
L h 75
L h 76
L h 77
L h 79
L h 80
L h 81
L h 82
L h 83
L h 84
L h 88
L h 89
L h 90
L h 91
L h 98
L h 99
L h 200
L h 202
L h 203
L h 204
L h 210
L h 211
L h 212
L h 220
L h 221
L h 222
L h 223
L h 224
L h 230
L h 231
L h 233
L h 240
L h 241
L h 243
L h 244
L h 10300
L h 10301
L h 10302
L h 10303
L h 10304
L h 10305
L h 10306
L h 10307
L h 10308
L h 10309
L h 10310
L h 10311
L h 10562
L h 10563
L h 10564
L h 10565
L h 10566
L j 10300
~
if (%self.fighting% || %self.disabled%)
  halt
end
* Burnable sectors:
set crop_sects 7 13
set desert_crop_sects 12 14 77
set desert_sects 20 51 70 73 75
set woods_sects 1 2 3 4 34 37 38 39 44 45 47 54 60 64 90
set spruce_sects 10562 10563 10564 10565
set grove_sects 23 24 25 26 71 72 74 76 79
set oasis_sects 21 80 81 82 83 84 88 89 91
set plains_sects 0 36 40 46 50 56 59 63 10566
set jungle_sects 220 221 224
set tropic_sects 200 204 210 211 212 222 223 230 231 233 240 241 243 244
set tropic_crop_sects 202 203
set foothills_sects 58 98 99
* some other sects are handled individually below
*
set room %self.room%
set vnum %room.sector_vnum%
if (%instance.location% && (%room.template% == 10300 || (%room% != %instance.location% && %random.10% == 10)))
  %echo% ~%self% flies away!
  mgoto %instance.location%
  %echo% ~%self% flies into the cave!
elseif %crop_sects% ~= %vnum%
  %echo% ~%self% scorches the crops!
  %terraform% %room% 10303
elseif %desert_crop_sects% ~= %vnum%
  %echo% ~%self% scorches the crops!
  %terraform% %room% 10304
elseif %tropic_crop_sects% ~= %vnum%
  %echo% ~%self% scorches the crops!
  %terraform% %room% 10309
elseif %desert_sects% ~= %vnum%
  %echo% ~%self% scorches the desert!
  %terraform% %room% 10305
elseif %woods_sects% ~= %vnum%
  %echo% ~%self% scorches the trees!
  %terraform% %room% 10300
elseif %spruce_sects% ~= %vnum%
  %echo% ~%self% scorches the trees!
  %terraform% %room% 10307
elseif %grove_sects% ~= %vnum%
  %echo% ~%self% scorches the grove!
  %terraform% %room% 10301
elseif %oasis_sects% ~= %vnum%
  %echo% ~%self% scorches the oasis!
  %terraform% %room% 10306
elseif %plains_sects% ~= %vnum%
  %echo% ~%self% scorches the plains!
  %terraform% %room% 10302
elseif %jungle_sects% ~= %vnum%
  %echo% ~%self% scorches the jungle!
  %terraform% %room% 10310
elseif %tropic_sects% ~= %vnum%
  %echo% ~%self% scorches the grassland!
  %terraform% %room% 10311
elseif %foothills_sects% ~= %vnum%
  %echo% ~%self% scorches the foothills!
  %terraform% %room% 10308
elseif %vnum% == 33
  %echo% ~%self% melts the frozen lake!
  %terraform% %room% 32
end
~
#10301
Flame Dragon Start Progression: room~
2 g 100 1
L y 10300
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10300)%
end
~
#10302
Flame Dragon combat~
0 k 5 0
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
0 f 100 5
L b 10330
L b 10331
L b 10332
L b 10333
L c 10316
~
if %instance.start%
  * Attempt delayed despawn
  %at% %instance.start% %load% o 10316
  nop %instance.set_location(%instance.start%)%
else
  %adventurecomplete%
end
* add death cry
switch %self.vnum%
  case 10330
    * Wandering Wyvern
    %regionecho% %self.room% 50 An ear-piercing screech deafens the land as a great wyvern dies!
  break
  case 10331
    * Bull Dragon
    %regionecho% %self.room% 50 A deep, guttural wail rolls over the land as a bull dragon dies!
  break
  case 10332
    * Dragon Guardian
    %regionecho% %self.room% 50 A deep roar of anguish shakes the land as a dragon guardian dies!
  break
  case 10333
    * Emerald Dragon
    %regionecho% %self.room% -50 The sky turns green for a moment as the death cry of an emerald dragon pierces the air!
  break
done
return 0
~
#10304
Flame Dragon environmental~
0 bw 5 0
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
0 h 100 1
L y 10300
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10300)%
end
~
#10306
Flame Dragon: Difficulty selector~
0 c 0 0
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty. (Normal, Hard, Group, or Boss)
  return 1
  halt
elseif %self.fighting%
  %send% %actor% You can't change |%self% difficulty while &%self% is in combat!
  return 1
  halt
elseif %self.disabled%
  %send% %actor% You can't change |%self% difficulty right now.
  return 1
  halt
end
if normal /= %arg%
  set difficulty 1
  set str normal
elseif hard /= %arg%
  set difficulty 2
  set str hard
elseif group /= %arg%
  set difficulty 3
  set str group
elseif boss /= %arg%
  set difficulty 4
  set str boss
else
  %send% %actor% That is not a valid difficulty level for this adventure. (Normal, Hard, Group, or Boss)
  halt
  return 1
end
* messaging
set old_diff %self.var(difficulty,2)%
%send% %actor% You set the difficulty to %str%...
%echoaround% %actor% ~%actor% sets the difficulty to %str%...
* Clear existing difficulty flags and set new ones.
nop %self.remove_mob_flag(HARD)%
nop %self.remove_mob_flag(GROUP)%
if %difficulty% == 1
  * Then we don't need to do anything
elseif %difficulty% == 2
  nop %self.add_mob_flag(HARD)%
elseif %difficulty% == 3
  nop %self.add_mob_flag(GROUP)%
elseif %difficulty% == 4
  nop %self.add_mob_flag(HARD)%
  nop %self.add_mob_flag(GROUP)%
end
remote difficulty %self.id%
%restore% %self%
wait 1
* in case
dg_affect %self% !ATTACK off
* alert
if %old_diff% > %difficulty%
  %echo% ... this flame dragon doesn't look so big up close.
elseif %old_diff% < %difficulty%
  %echo% ~%self% cranes its neck and bellows flames across the sky!
  if %self.room.sun% == light
    %regionecho% %self.room% -5 A massive fan of flames erupts through the air!
  else
    %regionecho% %self.room% -10 A massive fan of flames momentarily lights up the sky!
  end
end
~
#10307
Flame dragon despawn timer~
1 f 0 0
~
%adventurecomplete%
~
#10330
Abandoned Dragon Fly Home~
0 ab 10 2
L h 6
L j 10330
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
1 n 100 4
L b 10330
L b 10331
L b 10332
L b 10333
~
eval vnum 10330 + %random.4% - 1
%load% m %vnum%
%purge% %self%
~
#10332
Abandon Dragon Start Progression: room~
2 g 100 1
L y 10330
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10330)%
end
~
#10333
Abandon Dragon Start Progression: mob~
0 h 100 1
L y 10330
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10330)%
end
~
#10334
Abandoned Dragon animation (deprecated)~
0 bw 5 0
~
* replaced by custom strings
detach 10334 %self.id%
halt
*
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
1 c 2 1
L f 9910
use~
* Deprecated: the whistle now uses trig 9910 instead
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
1 c 2 0
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
0 bw 5 0
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
0 bw 5 0
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
1 c 2 0
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
#10342
Wandering Dragon: Single-mob difficulty selector~
0 c 0 4
L b 10330
L b 10331
L b 10332
L b 10333
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty. (Normal, Hard, Group, or Boss)
  return 1
  halt
end
if %self.fighting%
  %send% %actor% You can't change |%self% difficulty while &%self% is in combat!
  return 1
  halt
elseif %self.disabled%
  %send% %actor% You can't change |%self% difficulty right now.
  return 1
  halt
end
if normal /= %arg%
  set difficulty 1
  set str normal
elseif hard /= %arg%
  set difficulty 2
  set str hard
elseif group /= %arg%
  set difficulty 3
  set str group
elseif boss /= %arg%
  set difficulty 4
  set str boss
else
  %send% %actor% That is not a valid difficulty level for this adventure. (Normal, Hard, Group, or Boss)
  halt
  return 1
end
* messaging
set old_diff %self.var(difficulty,2)%
%send% %actor% You set the difficulty to %str%...
%echoaround% %actor% ~%actor% sets the difficulty to %str%...
* Clear existing difficulty flags and set new ones.
set mob %self%
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %difficulty% == 1
  * Then we don't need to do anything
elseif %difficulty% == 2
  nop %mob.add_mob_flag(HARD)%
elseif %difficulty% == 3
  nop %mob.add_mob_flag(GROUP)%
elseif %difficulty% == 4
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
remote difficulty %self.id%
%restore% %mob%
wait 1
dg_affect %mob% !ATTACK off
* alert
if %old_diff% > %difficulty%
  %echo% ... ~%self% doesn't look so big from this distance.
elseif %old_diff% < %difficulty%
  switch %mob.vnum%
    case 10330
      * Wandering Wyvern
      %echo% ~%self% swoops low and blasts flame across the sky!
    break
    case 10331
      * Bull Dragon
      %echo% ~%self% fumes with smoke as &%self% prepares to charge!
    break
    case 10332
      * Dragon Guardian
      %echo% ~%self% extends ^%self% talons and prepares to defend!
    break
    case 10333
      * Emerald Dragon
      %echo% Green light blinds you as ~%self% swoops low!
    break
  done
end
~
#10370
Uninvited Guest: Delayed despawn box~
1 f 0 1
L c 10371
~
%adventurecomplete%
%load% obj 10371
return 0
%purge% %self%
~
#10371
Uninvited Guest: Coffin collapsed~
1 n 100 1
L b 10370
~
%echo% The long box on the floor collapses into a pile of boards.
wait 1 s
set vampire %self.room.people(10370)%
if %vampire%
  if !%vampire.aff_flagged(NO-SEE-IN-ROOM)%
    %force% %vampire% say Well that isn't good. Guess it's time to move on.
  end
end
while %vampire%
  wait 300 s
  set vampire %self.room.people(10370)%
  if %vampire%
    if !%vampire.fighting%
      if !%vampire.aff_flagged(NO-SEE-IN-ROOM)%
        %echo% ~%vampire% leaves.
      end
      %purge% %vampire%
    end
  end
done
~
#10372
Uninvited Guest: Complete on death~
0 f 100 0
~
%adventurecomplete%
~
#10373
Uninvited Guest: Box commands~
1 c 4 0
look examine open close~
return 0
if close /= %cmd%
  if %actor.obj_target(%arg.argument1%)% == %self%
    %send% %actor% It's already closed.
    return 1
  end
elseif open /= %cmd%
  if %actor.obj_target(%arg.argument1%)% == %self%
    %send% %actor% You can't seem to get the box open. It's almost as if it's magically sealed.
    return 1
  end
elseif (look /= %cmd% && %arg.car% == in) || examine /= %cmd%
  if %actor.obj_target(%arg.argument1%)% == %self%
    %send% %actor% You can't seem to get the box open to look inside.
    return 1
  end
end
~
#10374
Uninvited Guest: Vampire wake/sleep~
0 b 50 2
L c 10370
L w 10370
~
if %self.fighting% || %self.disabled%
  halt
end
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
  dg_affect #10370 %self% NO-SEE-IN-ROOM on -1
end
~
#10375
Uninvited Guest: Bite in combat~
0 k 33 0
~
set room %self.room%
if (%actor.health% * 100 / %actor.maxhealth%) > 10
  * over 10% health
  halt
elseif !%self.vampire% || %actor.is_npc% || %actor.vampire% || %room.sun% != dark || %actor.nohassle%
  halt
elseif !%actor.can_gain_new_skills% || %actor.noskill(Vampire)%
  halt
elseif %actor.aff_flagged(NO-DRINK-BLOOD)%
  * Don't bite NO-DRINK-BLOOD targets
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
0 hnwA 100 0
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
if %self.fighting% || %self.disabled% || %self.aff_flagged(NO-SEE-IN-ROOM)%
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
  if %self.disabled% || %self.fighting% || %self.aff_flagged(NO-SEE-IN-ROOM)%
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
#10377
Uninvited Guest: Put vampire in box on load~
0 n 100 1
L w 10370
~
if %self.room.sun% != light
  dg_affect #10370 %self% NO-SEE-IN-ROOM on -1
end
~
$
