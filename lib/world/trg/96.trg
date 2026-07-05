#9600
SCF Script Fight: Setup dodge, interrupt, struggle, duck, jump (needs 9601, 9604)~
0 c 0 4
L c 9602
L f 9601
L f 9604
L w 9602
scfight~
* Also requires triggers 9601 and 9604 on the same mob
* Uses a 'diff' var on the mob with 1=normal, 2=hard, 3=group, 4=boss
*    diff is set automatically by script 9604 unless you set it ahead of time,
*    for example to make scripts harder on 'normal' trash mobs in a dungeon.
* To initialize or clear data:
*    scfight clear <all | dodge | interrupt | struggle | duck | jump>
* To set up players for a response command:
*    scfight setup <dodge | interrupt | struggle | duck | jump> <all | player>
* Optional vars you can set on the mob (remote them to the mob):
*    set scf_dodge_mode [leap | roll | swim]  * dodge messaging modes
* Optional vars you can set on the player (remote them to the player):
*    set scf_strug_char <string>  * shown to self when struggling
*    set scf_strug_room <string>  * to room when struggling, use %%actor%% var
*    set scf_free_char <string>  * to self when I get free
*    set scf_free_room <string>  * to room when I get free, use %%actor%% var
* To ensure nobody else is also acting:
*    scfight lockout <cooldown vnum> <my cooldown> <everyone else's cooldown>
if %actor% != %self%
  return 0
  halt
end
return 1
set mode %arg.car%
set arg %arg.cdr%
if %mode% == clear
  * Clear data
  * usage: scfight clear <all | dodge | interrupt | struggle | duck | jump>
  set ch %self.room.people%
  while %ch%
    if %arg% == dodge || %arg% == all
      rdelete did_scfdodge %ch.id%
      rdelete needs_scfdodge %ch.id%
    end
    if %arg% == interrupt || %arg% == all
      rdelete did_scfinterrupt %ch.id%
      rdelete needs_scfinterrupt %ch.id%
    end
    if %arg% == struggle || %arg% == all
      dg_affect #9602 %ch% off
      rdelete did_scfstruggle %ch.id%
      rdelete needs_scfstruggle %ch.id%
    end
    if %arg% == duck || %arg% == all
      rdelete did_scfduck %ch.id%
      rdelete needs_scfduck %ch.id%
    end
    if %arg% == jump || %arg% == all
      rdelete did_scfjump %ch.id%
      rdelete needs_scfjump %ch.id%
    end
    set ch %ch.next_in_room%
  done
  if %arg% == dodge || %arg% == all
    set count_scfdodge 0
    set wants_scfdodge 0
    remote count_scfdodge %self.id%
    remote wants_scfdodge %self.id%
  end
  if %arg% == interrupt || %arg% == all
    set count_scfinterrupt 0
    set wants_scfinterrupt 0
    remote count_scfinterrupt %self.id%
    remote wants_scfinterrupt %self.id%
  end
  if %arg% == struggle || %arg% == all
    set count_scfstruggle 0
    set wants_scfstruggle 0
    remote count_scfstruggle %self.id%
    remote wants_scfstruggle %self.id%
  end
  if %arg% == duck || %arg% == all
    set count_scfduck 0
    set wants_scfduck 0
    remote count_scfduck %self.id%
    remote wants_scfduck %self.id%
  end
  if %arg% == jump || %arg% == all
    set count_scfjump 0
    set wants_scfjump 0
    remote count_scfjump %self.id%
    remote wants_scfjump %self.id%
  end
elseif %mode% == setup
  * Prepare for a response
  * usage: scfight setup <dodge | interrupt | struggle | duck | jump> <all | player>
  set diff %self.var(diff,1)%
  set type %arg.car%
  set arg %arg.cdr%
  set target %arg.car%
  set value %arg.cdr%
  * self vars
  set wants_varname wants_scf%type%
  set %wants_varname% 1
  remote %wants_varname% %self.id%
  * target vars
  set needs_varname needs_scf%type%
  set did_varname did_scf%type%
  if %target% == all
    set all 1
    set ch %self.room.people%
  else
    set all 0
    set ch %target%
  end
  while %ch%
    set ok 0
    if %all%
      if %self.is_enemy(%ch%)% || %ch.is_pc%
        set ok 1
      elseif %ch.leader%
        if %ch.leader.is_pc%
          set ok 1
        end
      end
    else
      set ok 1
    end
    if %ok%
      set %needs_varname% 1
      set %did_varname% 0
      remote %needs_varname% %ch.id%
      remote %did_varname% %ch.id%
      if %type% == struggle
        if !%value%
          * default
          set value 10
        end
        dg_affect #9602 %ch% HARD-STUNNED on %value%
        %load% obj 9602 %ch% inv
        set obj %ch.inventory(9602)%
        if %obj%
          remote diff %obj.id%
        end
      end
    end
    if %all%
      set ch %ch.next_in_room%
    else
      set ch 0
    end
  done
elseif %mode% == lockout
  * Starts a cooldown on everyone
  * usage: scfight lockout <cooldown vnum> <my cooldown> <everyone else's cooldown>
  set vnum %arg.car%
  set arg %arg.cdr%
  set my_cd %arg.car%
  set them_cd %arg.cdr%
  set ch %self.room.people%
  while %ch%
    if %ch% == %self%
      if %my_cd%
        nop %self.set_cooldown(%vnum%,%my_cd%)%
      end
    elseif %them_cd% && %ch.is_npc% && %ch.has_trigger(9600)%
      nop %ch.set_cooldown(%vnum%,%them_cd%)%
    end
    set ch %ch.next_in_room%
  done
end
~
#9601
SCF Script Fight: Player dodges, interrupts, duck, jumpss~
0 c 0 6
L f 9600
L f 9604
L w 9600
L w 9601
L w 9604
L w 9605
dodge interrupt duck jump~
* Also requires triggers 9600 and 9604
* handles dodge, interrupt, duck, jump
return 1
if dodge /= %cmd%
  set type dodge
  set past dodged
elseif interrupt /= %cmd%
  if %actor.is_npc%
    %send% %actor% NPCs cannot interrupt.
    halt
  end
  set type interrupt
  set past interrupted
elseif duck /= %cmd%
  set type duck
  set past ducked
elseif jump /= %cmd%
  set type jump
  set past jumped
else
  return 0
  halt
end
* check things that prevent it
if %actor.var(did_scf%type%,0)%
  %send% %actor% You already %past%.
  halt
elseif %actor.disabled%
  %send% %actor% You can't do that right now!
  halt
elseif %actor.position% != Fighting && %actor.position% != Standing
  %send% %actor% You need to get on your feet first!
  halt
elseif %actor.aff_flagged(IMMOBILIZED)%
  %send% %actor% You can't do that right now... you're stuck!
  halt
elseif %actor.aff_flagged(BLIND)%
  %send% %actor% You can't see anything!
  halt
end
* check 'cooldown'
if %actor.affect(9600)%
  %send% %actor% You're still recovering from that last dodge.
  halt
elseif %actor.affect(9601)%
  %send% %actor% You're still distracted from that last interrupt.
  halt
elseif %actor.affect(9604)%
  %send% %actor% You're still distracted from last time you ducked.
  halt
elseif %actor.affect(9605)%
  %send% %actor% You're still recovering from the last jump.
  halt
end
* setup
set no_need 0
* does the actor even need it
if !%actor.var(needs_scf%type%,0)%
  set no_need 1
elseif !%self.var(wants_scf%type%,0)%
  * see if this mob needs it, or see if someone else here does
  * not me...
  set ch %self.room.people%
  set any 0
  while %ch% && !%any%
    if %ch.var(wants_scf%type%,0)%
      set any 1
    end
    set ch %ch.next_in_room%
  done
  if %any%
    * let them handle it
    return 0
    halt
  else
    set no_need 1
  end
end
* failure?
if %no_need%
  * ensure no var
  rdelete needs_scf%type% %actor.id%
  eval penalty %self.level% * %self.var(diff,1)% / 20
  * messaging
  if %type% == dodge
    %send% %actor% You dodge out of the way... of nothing!
    %echoaround% %actor% ~%actor% leaps out of the way of nothing in particular.
    dg_affect #9600 %actor% DODGE -%penalty% 20
  elseif %type% == interrupt
    %send% %actor% You look for something to interrupt...
    %echoaround% %actor% ~%actor% looks around for something...
    dg_affect #9601 %actor% DODGE -%penalty% 20
  elseif %type% == duck
    %send% %actor% You duck below... nothing! You look around trying to figure out what's going on.
    %echoaround% %actor% ~%actor% ducks, for no particular reason.
    dg_affect #9604 %actor% DODGE -%penalty% 20
  elseif %type% == jump
    %send% %actor% You jump over nothing and manage to fall in the process.
    %echoaround% %actor% ~%actor% jumps over nothing and falls to the ground.
    dg_affect #9605 %actor% DODGE -%penalty% 20
  end
  halt
end
* success
nop %actor.command_lag(COMBAT-ABILITY)%
set did_scf%type% 1
remote did_scf%type% %actor.id%
eval count_scf%type% %self.var(count_scf%type%,0)% + 1
remote count_scf%type% %self.id%
if %type% == dodge
  set scf_dodge_mode %self.var(scf_dodge_mode)%
  switch %scf_dodge_mode%
    case swim
      %send% %actor% You swim out of the way!
      %echoaround% %actor% ~%actor% swims out of the way!
    break
    case roll
      %send% %actor% You roll out of the way!
      %echoaround% %actor% ~%actor% rolls out of the way!
    break
    default
      %send% %actor% You leap out of the way!
      %echoaround% %actor% ~%actor% leaps out of the way!
    break
  done
elseif %type% == interrupt
  %send% %actor% You prepare to interrupt ~%self%...
  %echoaround% %actor% ~%actor% prepares to interrupt ~%self%...
elseif %type% == duck
  %send% %actor% You duck!
  %echoaround% %actor% ~%actor% ducks!
elseif %type% == jump
  %send% %actor% You prepare to jump...
  %echoaround% %actor% ~%actor% prepares to jump...
end
~
#9602
SCF Script Fight: Struggle to get free~
1 c 2 2
L f 9600
L w 9602
*~
* To use STRUGGLE: Mob must have trig 9600, then:
*   scfight clear struggle
*   scfight setup struggle <target | all>
* runs on an obj in inventory; strength/intelligence help break out faster
* uses optional string vars: scf_strug_char, scf_strug_room, scf_free_char, scf_free_room
return 1
if !%actor.affect(9602)%
  * check on ANY command
  if struggle /= %cmd%
    %send% %actor% You don't need to struggle right now.
  else
    return 0
  end
  rdelete scf_strug_char %actor.id%
  rdelete scf_strug_room %actor.id%
  rdelete scf_free_char %actor.id%
  rdelete scf_free_room %actor.id%
  %purge% %self%
  halt
elseif !(struggle /= %cmd%)
  return 0
  halt
end
* stats
if %actor.strength% > %actor.intelligence%
  set amount %actor.strength%
else
  set amount %actor.intelligence%
end
eval struggle_counter %self.var(struggle_counter,0)% + %amount% + 1
eval needed 4 + (4 * %self.var(diff,1)%)
* I want to break free...
if %struggle_counter% >= %needed%
  * free!
  set char_msg %actor.var(scf_free_char,You manage to break out!)%
  %send% %actor% %char_msg.process%
  set room_msg %actor.var(scf_free_room,~%actor% struggles and manages to break out!)%
  %echoaround% %actor% %room_msg.process%
  set did_skycleave_struggle 1
  remote did_skycleave_struggle %self.id%
  nop %actor.command_lag(COMBAT-ABILITY)%
  dg_affect #9602 %actor% off
  rdelete scf_strug_char %actor.id%
  rdelete scf_strug_room %actor.id%
  rdelete scf_free_char %actor.id%
  rdelete scf_free_room %actor.id%
  %purge% %self%
else
  * the struggle continues
  set char_msg %actor.var(scf_strug_char,You struggle to break free...)%
  %send% %actor% %char_msg.process%
  set room_msg %actor.var(scf_strug_room,~%actor% struggles, trying to break free...)%
  %echoaround% %actor% %room_msg.process%
  nop %actor.command_lag(COMBAT-ABILITY)%
  remote struggle_counter %self.id%
end
~
#9603
SCF Script Fight: Check struggle and remove~
1 ab 100 1
L w 9602
~
* Ensures the 'struggle' handler does not stick around
set ch %self.carried_by%
if !%ch%
  * not carried
  %purge% %self%
elseif !%ch.affect(9602)%
  * not struggling
  %purge% %self%
end
~
#9604
SCF Script Fight: Greeting setup~
0 h 100 2
L f 9600
L f 9601
~
* Works with trigs 9600/9601 to ensure players who enter the room do not have
* stale SCF data. Also sets diff variable.
* ensure difficulty
if !%self.varexists(diff)%
  set diff 1
  if %self.mob_flagged(HARD)%
    eval diff %diff% + 1
  end
  if %self.mob_flagged(GROUP)%
    eval diff %diff% + 2
  end
  remote diff %self.id%
end
* check data on actor
if %self.var(wants_scfdodge,0)%
  scfight setup dodge %actor%
else
  rdelete did_scfdodge %actor.id%
  rdelete needs_scfdodge %actor.id%
end
if %self.var(wants_scfinterrupt,0)%
  scfight setup interrupt %actor%
else
  rdelete did_scfinterrupt %actor.id%
  rdelete needs_scfinterrupt %actor.id%
end
if %self.var(wants_scfduck,0)%
  scfight setup duck %actor%
else
  rdelete did_scfduck %actor.id%
  rdelete needs_scfduck %actor.id%
end
if %self.var(wants_scfjump,0)%
  scfight setup jump %actor%
else
  rdelete did_scfjump %actor.id%
  rdelete needs_scfjump %actor.id%
end
~
#9608
SCF Script Fight: Fight move controller using script4 (needs 9601, 9604)~
0 k 100 4
L f 9600
L f 9601
L f 9604
L w 9600
~
* Requires SCF triggers 9601, 9604, and possibly 9600
* Set the list of commands as script4 with spaces between them like 'bash kick special'
* Each command in that list will be called with %actor% as the first arg
if %self.cooldown(9603)% || %self.disabled%
  halt
end
* detect moves
set moves_left %self.var(moves_left,)%
if !%moves_left%
  set moves_left %self.custom(script4)%
  if !%moves_left%
    %log% syslog Trigger 9609 on mob %self.vnum% has no move list in script4.
  end
end
* detect count
set num_left %self.var(num_left,)%
if !%num_left%
  set list %moves_left%
  set num_left 0
  while %list%
    eval num_left %num_left% + 1
    set list %list.cdr%
  done
end
* pick
eval which %%random.%num_left%%%
set old %moves_left%
set moves_left
set move 0
while %which% > 0
  set move %old.car%
  if %which% != 1
    set moves_left %moves_left% %move%
  end
  set old %old.cdr%
  eval which %which% - 1
done
set moves_left %moves_left% %old%
* store
eval num_left %num_left% - 1
remote moves_left %self.id%
remote num_left %self.id%
* very short delay
set id %self.id%
set actor_id %actor.id%
wait 1
if !%actor% || %actor.id% != %actor_id% || %self.id% != %id% || %self.disabled%
  * lost it
  halt
end
* perform move
scfight lockout 9603 30 35
%move% %actor%
~
#9609
SCF Script Fight: Fight move controller using script5 (needs 9601, 9604)~
0 k 100 4
L f 9600
L f 9601
L f 9604
L w 9600
~
* Requires SCF triggers 9601, 9604, and possibly 9600
* Set the list of commands as script5 with spaces between them like 'bash kick special'
* Each command in that list will be called with %actor% as the first arg
if %self.cooldown(9603)% || %self.disabled%
  halt
end
* detect moves
set moves_left %self.var(moves_left,)%
if !%moves_left%
  set moves_left %self.custom(script5)%
  if !%moves_left%
    %log% syslog Trigger 9609 on mob %self.vnum% has no move list in script5.
  end
end
* detect count
set num_left %self.var(num_left,)%
if !%num_left%
  set list %moves_left%
  set num_left 0
  while %list%
    eval num_left %num_left% + 1
    set list %list.cdr%
  done
end
* pick
eval which %%random.%num_left%%%
set old %moves_left%
set moves_left
set move 0
while %which% > 0
  set move %old.car%
  if %which% != 1
    set moves_left %moves_left% %move%
  end
  set old %old.cdr%
  eval which %which% - 1
done
set moves_left %moves_left% %old%
* store
eval num_left %num_left% - 1
remote moves_left %self.id%
remote num_left %self.id%
* very short delay
set id %self.id%
set actor_id %actor.id%
wait 1
if !%actor% || %actor.id% != %actor_id% || %self.id% != %id% || %self.disabled%
  * lost it
  halt
end
* perform move
scfight lockout 9603 30 35
%move% %actor%
~
#9620
Storytime using script1-5~
0 bw 100 1
L w 9621
~
* uses mob custom strings script1-script5 to tell short stories
* usage: .custom add script# <command> <string>
* valid commands: say, emote, do (execute command), echo (script), and skip
* also: vforce <mob vnum in room> <command>
* also: set <line_gap|story_gap> <time> sec
* NOTE: waits for %line_gap% (9 sec) after all commands EXCEPT do/vforce/set
set line_gap 9 sec
set story_gap 180 sec
* random wait to offset competing scripts slightly
wait %random.30%
* ensure not fighting
if %self.disabled% || %self.fighting%
  halt
end
* ensure no other stories
set ch %self.room.people%
while %ch%
  if %ch% != %self% && %ch.affect(9621)%
    halt
  end
  set ch %ch.next_in_room%
done
* find story number
if %self.varexists(story)%
  eval story %self.story% + 1
  if %story% > 5
    set story 1
  end
else
  set story 1
end
* determine valid story number
set tries 0
set ok 0
while %tries% < 5 && !%ok%
  if %self.custom(script%story%,0)%
    set ok 1
  else
    eval story %story% + 1
    if %story% > 5
      set story 1
    end
  end
  eval tries %tries% + 1
done
if !%ok%
  wait %story_gap%
  halt
end
* story detected: prepare (storing as variables prevents reboot issues)
dg_affect #9621 %self% AGE 0 3600
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
* tell story
set pos 0
set done 0
while !%done%
  set msg %self.custom(script%story%,%pos%)%
  if %msg% && !%self.fighting% && !%self.disabled%
    set mode %msg.car%
    set msg %msg.cdr%
    if %mode% == say
      say %msg%
      wait %line_gap%
    elseif %mode% == do
      %msg.process%
      * no wait
    elseif %mode% == echo
      %echo% %msg.process%
      wait %line_gap%
    elseif %mode% == vforce
      set vnum %msg.car%
      set msg %msg.cdr%
      set targ %self.room.people(%vnum%)%
      if %targ%
        %force% %targ% %msg.process%
      end
    elseif %mode% == emote
      emote %msg%
      wait %line_gap%
    elseif %mode% == set
      set subtype %msg.car%
      set msg %msg.cdr%
      if %subtype% == line_gap
        set line_gap %msg%
      elseif %subtype% == story_gap
        set story_gap %msg%
      else
        %echo% ~%self%: Invalid set type '%subtype%' in storytime script.
      end
    elseif %mode% == skip
      * nothing this round
      wait %line_gap%
    else
      %echo% %self.name%: Invalid script message type '%mode%'.
    end
  else
    set done 1
  end
  eval pos %pos% + 1
done
remote story %self.id%
* cancel sentinel/silent
dg_affect #9621 %self% off
if %self.var(no_sentinel,0)%
  nop %self.remove_mob_flag(SENTINEL)%
end
if %self.var(no_silent,0)%
  nop %self.remove_mob_flag(SILENT)%
end
* wait between stories
wait %story_gap%
~
#9621
Storytime for Factions using script1-2 and script3-4~
0 bw 100 1
L w 9621
~
* Faction-based variant of 9620 Storytime scipt
*  - custom script1 and script2 (optional) are used in order as alternating
*    greeting stories when the player is BELOW "Liked" reputation with this mob
*  - script3 and script4 (optional) are used when at "Liked" or higher
*  - script5 is not used
* usage: .custom add script# <command> <string>
* valid commands: say, emote, do (execute command), echo (script), and skip
* also: vforce <mob vnum in room> <command>
* also: set <line_gap|story_gap> <time> sec
* NOTE: waits for %line_gap% (9 sec) after all commands EXCEPT do/vforce/set
set line_gap 9 sec
set story_gap 180 sec
* random wait to offset competing scripts slightly
wait %random.30%
* ensure not fighting
if %self.disabled% || %self.fighting%
  halt
end
* ensure no other stories
set ch %self.room.people%
while %ch%
  if %ch% != %self% && %ch.affect(9621)%
    halt
  end
  set ch %ch.next_in_room%
done
* look for a player with rep
set friend 0
set ch %self.room.people%
while %ch% && !%friend%
  if %ch.is_pc% && %ch.has_reputation(%self.allegiance%,Liked)%
    set friend 1
  end
  set ch %ch.next_in_room%
done
* find story number
if %friend%
  eval story_friend %self.var(story_friend,2)% + 1
  if %story_friend% > 4 || !%self.custom(script4,0)%
    set story_friend 3
  end
  set story %story_friend%
  set story_other %self.var(story_other,0)%
else
  eval story_other %self.var(story_other,0)% + 1
  if %story_other% > 2 || !%self.custom(script2,0)%
    set story_other 1
  end
  set story %story_other%
  set story_friend %self.var(story_friend,2)%
end
* check if this story exists
if !%self.custom(script%story%,0)%
  wait %story_gap%
  halt
end
* story detected: prepare (storing as variables prevents reboot issues)
dg_affect #9621 %self% AGE 0 3600
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
* tell story
set pos 0
set done 0
while !%done%
  set msg %self.custom(script%story%,%pos%)%
  if %msg% && !%self.fighting% && !%self.disabled%
    set mode %msg.car%
    set msg %msg.cdr%
    if %mode% == say
      say %msg%
      wait %line_gap%
    elseif %mode% == do
      %msg.process%
      * no wait
    elseif %mode% == echo
      %echo% %msg.process%
      wait %line_gap%
    elseif %mode% == vforce
      set vnum %msg.car%
      set msg %msg.cdr%
      set targ %self.room.people(%vnum%)%
      if %targ%
        %force% %targ% %msg.process%
      end
    elseif %mode% == emote
      emote %msg%
      wait %line_gap%
    elseif %mode% == set
      set subtype %msg.car%
      set msg %msg.cdr%
      if %subtype% == line_gap
        set line_gap %msg%
      elseif %subtype% == story_gap
        set story_gap %msg%
      else
        %echo% ~%self%: Invalid set type '%subtype%' in storytime script.
      end
    elseif %mode% == skip
      * nothing this round
      wait %line_gap%
    else
      %echo% %self.name%: Invalid script message type '%mode%'.
    end
  else
    set done 1
  end
  eval pos %pos% + 1
done
remote story_friend %self.id%
remote story_other %self.id%
* cancel sentinel/silent
dg_affect #9621 %self% off
if %self.var(no_sentinel,0)%
  nop %self.remove_mob_flag(SENTINEL)%
end
if %self.var(no_silent,0)%
  nop %self.remove_mob_flag(SILENT)%
end
* wait between stories
wait %story_gap%
~
#9680
Force look after wait~
1 n 100 0
~
wait 1
if %self.carried_by% && %self.carried_by.position% != Sleeping
  %force% %self.carried_by% look
end
%purge% %self%
~
#9681
Vehicle: Set load time on-load~
5 n 100 0
~
set load_time %timestamp%
remote load_time %self.id%
~
#9682
Remove spawned NPCs in room~
1 n 100 0
~
* Removes any spawned NPCs that aren't linked to an adventure
* Uses:
* - Ensure no stray NPCs on adventure tile
*
set room %self.room%
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_npc% && %ch.mob_flagged(SPAWNED)% && !%ch.linked_to_instance%
    %echo% ~%ch% leaves.
    %purge% %ch%
  end
  set ch %next_ch%
done
%purge% %self%
~
#9683
Delayed Despawner: Attach to Obj with Timer~
1 f 0 0
~
* Marks the adventure complete when the item decays.
%adventurecomplete%
~
#9684
Bonus Ability: grant abilities after wait~
1 n 100 0
~
wait 1
set actor %self.carried_by%
if %actor%
  set num 0
  while %num% <= 2
    eval vnum %%self.val%num%%%
    if %vnum% > 0 && %_abil.exists(%vnum%)% && !%actor.has_bonus_ability(%vnum%)%
      nop %actor.add_bonus_ability(%vnum%)%
      %send% %actor% &&YYou gain the %_abil.name(%vnum%)% ability!&&0
    end
    eval num %num% + 1
  done
end
%purge% %self%
~
#9685
Vehicle: Only harness flying mobs~
5 c 0 0
harness~
set anim_arg %arg.argument1%
set veh_arg %arg.argument2%
if (!%anim_arg% || !%veh_arg%)
  return 0
  halt
end
set mob %actor.char_target(%anim_arg%)%
set veh %actor.veh_target(%veh_arg%)%
if (!%mob% || !%veh% || %veh% != %self%)
  return 0
  halt
end
* check flying (block harness if not)
if !%mob.is_flying%
  %send% %actor% You can only harness flying animals to %veh.shortdesc%.
  return 1
  halt
end
* otherwise, just return 0 (allow the harness)
return 0
~
$
