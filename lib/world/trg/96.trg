#9600
SCF Script Fight: Setup dodge, interrupt, struggle~
0 c 0
scfight~
* Also requires triggers 9601 and 9604 on the same mob
* Uses a 'diff' var on the mob with 1=normal, 2=hard, 3=group, 4=boss
*    diff is set automatically by script 9604 unless you set it ahead of time,
*    for example to make scripts harder on 'normal' trash mobs in a dungeon.
* To initialize or clear data:
*    scfight clear <all | dodge | interrupt | struggle>
* To set up players for a response command:
*    scfight setup <dodge | interrupt | struggle> <all | player>
* Optional vars you can set on the mob (remote them to the mob):
*    set scf_dodge_mode [leap | roll | swim]  * dodge messaging modes
* Optional vars you can set on the player (remote them to the player):
*    set scf_strug_char <string>  * shown to self when struggling
*    set scf_strug_room <string>  * to room when struggling
*    set scf_free_char <string>  * to self when I get free
*    set scf_free_room <string>  * to room when I get free
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
  * usage: scfight clear <all | dodge | interrupt | struggle>
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
elseif %mode% == setup
  * Prepare for a response
  * usage: scfight setup <dodge | interrupt | struggle> <all | player>
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
SCF Script Fight: Player dodges, interrupts~
0 c 0
dodge interrupt~
* Also requires triggers 9600 and 9604
* handles dodge, interrupt
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
end
~
#9602
SCF Script Fight: Struggle to get free~
1 c 2
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
1 ab 100
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
0 h 100
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
~
#9620
Storytime using script1-5~
0 bw 100
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
1 n 100
~
wait 1
if %self.carried_by% && %self.carried_by.position% != Sleeping
  %force% %self.carried_by% look
end
%purge% %self%
~
$
