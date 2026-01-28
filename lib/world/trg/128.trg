#12800
Celestial Forge: Donate to open portal~
0 c 0 7
L c 12800
L c 12801
L c 12806
L j 12810
L j 12850
L w 5100
L w 5101
donate~
set room %self.room%
set which 0
set dest 0
* validate argument
if !%actor.canuseroom_guest(%room%)%
  %send% %actor% You don't have permission to do that here.
elseif !%arg%
  %send% %actor% Donate to which celestial forge? (iron, imperium, ...)
elseif iron forge /= %arg% || lodestone forge /= %arg%
  set which 12800
  set dest 12810
  set curr 5100
  set str an iron shard
elseif imperium forge /= %arg% || victory forge /= %arg%
  set which 12801
  set dest 12850
  set curr 5101
  set str an imperium shard
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
#12801
Celestial Forge: Request exit~
2 c 0 8
L c 9680
L c 12800
L c 12801
L c 12806
L e 5195
L j 12800
L j 12810
L j 12850
return~
if %actor.is_npc%
  * possibly immortal trying to return
  return 0
  halt
end
* try adventure summon
if %actor.adventure_summoned_from%
  nop %actor.end_adventure_summon%
  halt
end
* try portal
set cf_return %actor.var(cf_return)%
if %cf_return%
  makeuid toroom room %cf_return%
  if %toroom% && %toroom.building_vnum% == 5195 && %actor.canuseroom_guest(%toroom%)%
    * valid location! check existing portals
    set obj %room.contents%
    while %obj%
      if %obj.type% == PORTAL && %obj.val0% == %toroom.vnum%
        * oops
        %send% %actor% There's already a portal open for you!
        halt
      end
      set obj %obj.next_in_list%
    done
    * if we got here, make a portal
    switch %room.template%
      case 12810
        set in_vnum 12800
      break
      case 12850
        set in_vnum 12801
      break
      default
        set in_vnum 0
      break
    done
    * success?
    if %in_vnum%
      * portal out
      %load% obj 12806
      set outport %room.contents%
      if %outport.vnum% == 12806
        nop %outport.val0(%toroom.vnum%)%
        set emp %toroom.empire_name%
        if %emp%
          %mod% %outport% shortdesc the portal back to %emp%
          %mod% %outport% longdesc The portal back to %emp% is open here.
          %mod% %outport% keywords portal back %emp%
          %echo% A portal back to %emp% whirls open!
        else
          %echo% A portal whirls open!
        end
      end
      * portal back here
      %load% obj %in_vnum% %toroom%
      set inport %toroom.contents%
      if %inport.vnum% == %in_vnum%
        nop %inport.val0(%room.vnum%)%
        %at% %toroom% %echo% A portal whirls open!
      end
      halt
    end
  end
end
* if we got this far, explore other options
set tele -1
if %actor.home%
  set tele %actor.home%
else
  set tele %startloc%
end
if %tele% != -1
  %send% %actor% You're whisked away in a flash of starlight!
  %echoaround% %actor% ~%actor% is whisked away in a flash of starlight!
  %teleport% %actor% %tele%
  %echoaround% %actor% ~%actor% arrives in a flash!
  %load% obj 9680 %actor% inv
  * friends
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch.is_npc% && %ch.leader% == %actor%
      %at% %room% %echo% ~%ch% is whisked away, too!
      %teleport% %ch% %tele%
      %echo% ~%ch% arrives in a flash!
    end
    set ch %next_ch%
  done
else
  %send% %actor% You can't seem to find a way to return. Surely this is a bug.
end
~
#12802
Celestial Forge: Detect player entry, Grant abilities, Start progress~
2 gA 100 12
L c 9684
L e 5195
L i 12800
L j 12810
L j 12815
L j 12850
L j 12855
L o 12810
L o 12850
L q 6
L y 12810
L y 12850
~
if %actor.is_npc%
  halt
end
* Ensure ability first, if over 75 Trade
if %actor.skill(6)% >= 76
  if %room.template% >= 12810 && %room.template% <= 12815
    if !%actor.has_bonus_ability(12810)%
      * grant the ability after a short delay
      %load% obj 9684 %actor%
      set obj %actor.inventory%
      if %obj.vnum% == 9684
        nop %obj.val0(12810)%
      end
    end
    if %actor.empire%
      nop %actor.empire.start_progress(12810)%
    end
  end
  if %room.template% >= 12850 && %room.template% <= 12855
    if !%actor.has_bonus_ability(12850)%
      * grant the ability after a short delay
      %load% obj 9684 %actor%
      set obj %actor.inventory%
      if %obj.vnum% == 9684
        nop %obj.val0(12850)%
      end
    end
    if %actor.empire%
      nop %actor.empire.start_progress(12850)%
    end
  end
end
* Track origin
if !%was_in%
  halt
end
if %was_in.template% >= 12800 && %was_in.template% <= 12999
  * inside same adventure
  halt
end
if %was_in.building_vnum% == 5195
  * Celestial Forge
  set cf_return %was_in.vnum%
  remote cf_return %actor.id%
else
  * no return location available
  rdelete cf_return %actor.id%
end
~
#12803
Celestial Forge: Time and Weather commands~
2 c 0 2
L j 12810
L j 12850
time weather~
if %cmd.mudcommand% == time
  * TIME
  switch %room.template%
    case 12810
      %send% %actor% It looks like nighttime through the hole at the top of the tunnel.
    break
    case 12850
      %send% %actor% It looks like nighttime out through the flap.
    break
    default
      %send% %actor% The beautiful night sky overhead tells you it's nighttime.
    break
  done
  return 1
elseif %cmd.mudcommand% == weather
  * WEATHER
  switch %room.template%
    case 12810
      %send% %actor% It's hard to tell the weather from in here.
    break
    case 12850
      %send% %actor% The night sky is cloudless outside.
    break
    default
      %send% %actor% The night sky is cloudless and vast.
    break
  done
  return 1
else
  * unknown command somehow?
  return 0
end
~
#12804
Celestial Forge: Block adventure/survey on Shoals tile~
2 c 0 0
adventure survey~
* This adventure uses a shallow tile as its real-world location. This script
* is to prevent players from seeing what the shoals tile actually is.
if %cmd% == adventure
  %send% %actor% You are not in or near an adventure zone.
elseif %cmd% == survey
  %send% %actor% You survey the area:
  if %room.island%
    if %room.island(%actor%)% != %room.island%
      %send% %actor% Location: %room.island(%actor%)% (%room.island%)
    else
      %send% %actor% Location: %room.island%
    end
  end
  %send% %actor% Climate: %room.climate%
  eval temp %%temperature.%room.temperature%%%
  %send% %actor% Temperature: %temp%
  %send% %actor% This location cannot be claimed.
else
  return 0
end
~
#12805
Celestial Forge: Immortal controller~
1 c 2 2
L j 12810
L j 12850
cforge~
if !%actor.is_immortal%
  %send% %actor% You lack the power to use this.
  halt
end
set mode %arg.car%
set arg2 %arg.cdr%
if goto /= %mode%
  * target handling
  if iron /= %arg2% || lodestone forge /= %arg2%
    set to_room %instance.nearest_rmt(12810)%
  elseif imperium /= %arg2% || victory forge /= %arg2%
    set to_room %instance.nearest_rmt(12850)%
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
1 c 4 0
enter~
return 0
if %actor.obj_target(%arg.argument1%)% != %self% || %self.val0% <= 0 || %actor.is_immortal%
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
0 gh 100 0
~
* Uses mob custom script1 for a once-ever greeting per character and script2
*   for players who have been to the instance before. Each script1/2 line
*   sent every %line_gap% (7 sec) until it runs out of strings. The mob will
*   be SENTINEL and SILENT during this period.
* usage: .custom add <script1 | script2> <command> <string>
* valid commands: say, emote, do (execute command), echo (script), skip, attackable
* also: vforce <mob vnum in room> <command>
* also: set line_gap <time> sec
* also: mod <field> <value> -- runs %mod% %self%
* NOTE: waits for %line_gap% (7 sec) after all commands EXCEPT do/vforce/set/attackable/mod
set line_gap 7 sec
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
2 c 0 0
mine~
%send% %actor% There doesn't seem to be anything left here to mine.
~
#12811
Celestial Forge: Unique item exclusion~
1 j 0 10
L c 12810
L c 12814
L c 12818
L c 12822
L c 12826
L c 12852
L c 12856
L c 12860
L c 12864
L c 12868
~
set ring_list 12810 12814 12818 12822 12826 12852 12856 12860 12864 12868
set ring_pos rfinger lfinger
set pos_list
*
if %ring_list% ~= %self.vnum%
  set pos_list %ring_pos%
  set vnum_list %ring_list%
end
*
if %pos_list% && %vnum_list%
  while %pos_list%
    set pos %pos_list.car%
    set pos_list %pos_list.cdr%
    set obj %actor.eq(%pos%)%
    if %obj%
      if %vnum_list% ~= %obj.vnum%
        * oops
        %send% %actor% You can't equip @%self% while wearing @%obj%.
        return 0
        halt
      end
    end
  done
end
* ok
return 1
~
#12816
Lodestone Colossus combat: Rust Stream, Armor All, Shield Fling, Rain of Rusted Blades~
0 c 0 6
L w 12817
L w 12818
L w 12819
L w 12820
L w 12821
L w 12822
!rust !armor !fling !blades~
set targ %arg%
set room %self.room%
set diff %self.var(diff,1)%
set cmd %cmd.substr(1)%
if %actor% != %self% || !%targ% || %targ.id% == %self.id%
  halt
elseif %cmd% == rust
  * Rust Stream (interrupt)
  scfight clear interrupt
  %echo% &&w**** Motes of rust begin to rise from the field the colossus's lodestones pulse... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  scfight setup interrupt all
  wait 3 s
  if %diff% > 2
    set needed %room.players_present%
  else
    set needed 1
  end
  if %self.var(count_scfinterrupt,0)% < %needed%
    %echo% &&w**** Vibrant red streams of rust flow through the air, toward the colossus... ****&&0 (interrupt)
  end
  wait 3 s
  if %self.var(count_scfinterrupt,0)% >= %needed%
    %echo% &&wThe rust rains down onto the battlefield as ~%self% loses focus and the lodestones fall silent!&&0
    if %diff% == 1
      dg_affect #12817 %self% HARD-STUNNED on 5
    end
    wait 30 s
  else
    %echo% &&wThe colossus grows as the streams of rust join with its tremendous body!&&0
    eval amount %self.maxhealth% / 15
    dg_affect #12818 %self% HEAL-OVER-TIME %amount% 15
  end
  scfight clear interrupt
elseif %cmd% == armor
  * Armor All (interrupt)
  scfight clear interrupt
  %echo% &&w**** The lodestones of the colossus pulse and the whole field begins to rattle... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  scfight setup interrupt all
  wait 3 s
  if %diff% > 2
    set needed %room.players_present%
  else
    set needed 1
  end
  if %self.var(count_scfinterrupt,0)% < %needed%
    %echo% &&w**** Pauldrons, greaves, and breastplates rise from the ashes, hovering in the air for just a moment... ****&&0 (interrupt)
  end
  wait 3 s
  if %self.var(count_scfinterrupt,0)% >= %needed%
    %echo% &&wThe cacophanous clatter deafens you momentarily as metal rains down on the battlefield.&&0
    if %diff% == 1
      dg_affect #12817 %self% HARD-STUNNED on 5
    end
    wait 30 s
  else
    %echo% &&wSlowly at first, then all at once, pieces of floating armor snap to the colossus, reinforcing its massive frame!&&0
    eval amount %self.level% / 4
    dg_affect #12819 %self% RESIST-PHYSICAL %amount% 30
    dg_affect #12819 %self% RESIST-MAGICAL %amount% 30
  end
  scfight clear interrupt
elseif %cmd% == fling
  * Shield Fling (group duck)
  scfight clear duck
  %echo% &&wClanging echoes across the battlefield, distant at first, then all around you!&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  %echo% &&w**** Shields, large and small, round and pointed, fly spinning from all across the field... toward you! ****&&0 (duck)
  set cycle 1
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    scfight setup duck all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfduck)%
          %echo% &&wA flying shield strikes ~%ch% in the head!&&0
          dg_affect #12820 %ch% STUNNED on 10
          %damage% %ch% 50 physical
        elseif %ch.is_pc%
          %send% %ch% &&wYou feel the air whoosh just above your head as you duck just in time to avoid a shield!&&0
          if %diff% == 1
            dg_affect #12821 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&w**** More shields are still coming! ****&&0 (duck)
        end
      end
      set ch %next_ch%
    done
    scfight clear duck
    eval cycle %cycle% + 1
  done
  wait 8 s
elseif %cmd% == blades
  * Rain of Rusted Blades (group dodge)
  scfight clear dodge
  %echo% &&wA high-pitched screech pierces your ears as thousands of rusted blades scrape up from the ashes and rise into the air!&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  %echo% &&w**** Rusted blades begin plummeting from above, one by one, all around you! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    scfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfdodge)%
          set hit 1
          %echo% &&wA rusted blade gashes ~%ch% as it falls!&&0
          %dot% #12822 %ch% 100 30 physical 2
          %damage% %ch% 50 physical
        elseif %ch.is_pc%
          %send% %ch% &&wYou narrowly avoid a pair of blades that fall inches from your feet!&&0
          if %diff% == 1
            dg_affect #12821 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&w**** There are still more blades coming down... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
    eval cycle %cycle% + 1
  done
  wait 8 s
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#12817
Celestial Forge: Arena return command~
2 c 0 9
L c 9680
L j 12811
L j 12817
L j 12818
L j 12819
L j 12851
L j 12857
L j 12858
L j 12859
return~
if %actor.fighting% || %actor.disabled%
  %send% %actor% You can't do that right now.
  halt
elseif %actor.position% != Standing
  %send% %actor% You need to stand up first.
  halt
end
* setup
switch %room.template%
  case 12817
  case 12818
  case 12819
    set dest %instance.nearest_rmt(12811)%
    set mes swirl of iron filings
  break
  case 12857
  case 12858
  case 12859
    set dest %instance.nearest_rmt(12851)%
    set mes gleaming flash of imperium
  break
done
if !%dest%
  %send% %actor% You can't do that right now.
  halt
end
* teleport
%echoaround% %actor% ~%actor% vanishes with a %mes%!
%send% %actor% You vanish with a %mes%!
%teleport% %actor% %dest%
%echoaround% %actor% ~%actor% appears in a %mes%!
%load% obj 9680 %actor% inv
* fellows
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.leader% == %actor% && !%ch.fighting%
    %echoaround% %ch% ~%ch% vanishes with a %mes%!
    %teleport% %ch% %dest%
    %echoaround% %ch% ~%ch% appears in a %mes%!
    if %ch.position% != Sleeping
      %load% obj 9680 %ch% inv
    end
  end
  set ch %next_ch%
done
~
#12818
Celetsial Forge: Reset arena and spawn mob~
2 bw 100 10
L b 12817
L b 12857
L b 12858
L b 12859
L j 12817
L j 12818
L j 12819
L j 12857
L j 12858
L j 12859
~
* setup
switch %self.template%
  case 12817
  case 12818
  case 12819
    set check_list 12817
    set mob 12817
    set mes The scattered weapons across the field suddenly shudder and slide, racing inward as a towering form of lodestone rises from the center, armored in the spoils of a thousand forgotten battles!
  break
  case 12857
  case 12858
  case 12859
    set check_list 12857 12858 12859
    set mob 12857
    set mes The ground shakes as a fearsome War Machine rolls onto the battlefield, roaring like a furnace!
  break
  default
    halt
  break
done
* auto-restore if nobody is fighting
wait 6 s
set any 0
set ch %room.people%
while %ch% && !%any%
  if %ch.fighting%
    set any 1
  end
  set ch %ch.next_in_room%
done
if !%any%
  set ch %room.people%
  while %ch%
    if %ch.health% < %ch.maxhealth% || %ch.mana% < %ch.maxmana%
      %send% %ch% &&wThe spirit of the forge flows through you and restores you!&&0
    end
    %restore% %ch%
    set ch %ch.next_in_room%
  done
end
* spawn mob?
set any 0
while %check_list% && !%any%
  set vnum %check_list.car%
  set check_list %check_list.cdr%
  if %room.people(%vnum%)%
    set any 1
  end
done
if !%any%
  * load me!
  wait 6 s
  %load% mob %mob%
  set guy %room.people%
  if %guy.vnum% == %mob%
    * success
    %echo% &&w%mes%&&0
  end
end
~
#12819
Celestial Forge: Challenge command to enter arena~
2 c 0 9
L c 9680
L j 12811
L j 12817
L j 12818
L j 12819
L j 12851
L j 12857
L j 12858
L j 12859
challenge~
* Tries to find an available arena to fight in
* optional 'empty' arg gets you one with zero players
if %actor.fighting% || %actor.disabled%
  %send% %actor% You can't do that right now.
  halt
elseif %actor.position% != Standing
  %send% %actor% You need to stand up first.
  halt
end
* setup
switch %room.template%
  case 12811
    set room_list 12817 12818 12819
    set mes swirl of iron filings
  break
  case 12851
    set room_list 12857 12858 12859
    set mes gleaming flash of imperium
  break
done
eval empty %arg% == empty
set dest 0
while %room_list% && !%dest%
  set vnum %room_list.car%
  set room_list %room_list.cdr%
  set dest %instance.nearest_rmt(%vnum%)%
  set ok 1
  if %dest%
    set ch %dest.people%
    while %ch% && %ok%
      if %ch.fighting%
        set ok 0
      elseif %empty% && %ch.is_pc%
        set ok 0
      end
      set ch %ch.next_in_room%
    done
    if !%ok%
      set dest 0
    end
  end
done
if !%dest%
  %send% %actor% No challenge arena was available. Try again later.
  halt
end
* teleport
%echoaround% %actor% ~%actor% vanishes with a %mes%!
%send% %actor% You vanish with a %mes%!
%teleport% %actor% %dest%
%echoaround% %actor% ~%actor% appears in a %mes%!
%load% obj 9680 %actor% inv
* fellows
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.leader% == %actor% && !%ch.fighting%
    %echoaround% %ch% ~%ch% vanishes with a %mes%!
    %teleport% %ch% %dest%
    %echoaround% %ch% ~%ch% appears in a %mes%!
    if %ch.position% != Sleeping
      %load% obj 9680 %ch% inv
    end
  end
  set ch %next_ch%
done
~
#12820
Celestial Forge: Loot once per day per person~
0 f 100 4
L b 12817
L b 12857
L b 12858
L b 12859
~
eval min_level %self.minlevel% - 25
set room %self.room%
set ch %room.people%
set any_ok 0
switch %self.vnum%
  case 12817
    set varname %self.vnum%_daily
    set loot a lodestone relic shield
    set death The Lodestone Colossus crumbles into ash and bits of broken armor!
  break
  case 12857
  case 12858
  case 12859
    set varname 12857_daily
    set loot a great imperium gear
    set death The War Machine collapses in a heap of broken wood and metal!
  break
  default
    set varname %self.vnum%_daily
    set loot
    set death
  break
done
* ensure a player has loot permission
if %actor.is_pc% && %actor.level% >= %min_level% && %self.is_tagged_by(%actor%)%
  if %actor.var(%varname%,0)% < %dailycycle%
    * actor qualifies
    set %varname% %dailycycle%
    remote %varname% %actor.id%
    nop %self.remove_mob_flag(!LOOT)%
    set any_ok 1
  end
end
* actor didn't qualify -- find anyone present who does
set ch %room.people%
while %ch% && !%any_ok%
  if %ch.is_pc% && %ch.level% >= %min_level% && %self.is_tagged_by(%ch%)%
    if %ch.var(%varname%,0)% < %dailycycle%
      * ch qualifies
      set %varname% %dailycycle%
      remote %varname% %ch.id%
      nop %self.remove_mob_flag(!LOOT)%
      set any_ok 1
    end
  end
  set ch %ch.next_in_room%
done
* death message
if %death%
  %echo% &&w&&Z%death%&&0
  return 0
end
* did we drop it?
if %any_ok% && %loot%
  %echo% &&w&&Z%loot% falls to the ground as ~%self% is defeated!&&0
end
~
#12821
Celestial Forge: Single mob difficulty selector~
0 c 0 0
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
end
if normal /= %arg%
  set diff 1
elseif hard /= %arg%
  set diff 2
elseif group /= %arg%
  set diff 3
elseif boss /= %arg%
  set diff 4
else
  %send% %actor% That is not a valid difficulty level for this encounter. (Normal, Hard, Group, or Boss)
  return 1
  halt
end
* messaging
%send% %actor% You set the difficulty...
%echoaround% %actor% ~%actor% sets the difficulty...
* Clear existing difficulty flags and set new ones.
remote diff %self.id%
set mob %self%
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %diff% == 1
  * Then we don't need to do anything
  %echo% ~%self% has been set to Normal.
elseif %diff% == 2
  %echo% ~%self% has been set to Hard.
  nop %mob.add_mob_flag(HARD)%
elseif %diff% == 3
  %echo% ~%self% has been set to Group.
  nop %mob.add_mob_flag(GROUP)%
elseif %diff% == 4
  %echo% ~%self% has been set to Boss.
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
nop %mob.unscale_and_reset%
* remove no-attack
if %mob.aff_flagged(!ATTACK)%
  dg_affect %mob% !ATTACK off
end
* unscale and restore me
nop %self.unscale_and_reset%
* mark me as scaled
set scaled 1
remote scaled %self.id%
* attempt to remove (difficulty) from longdesc
set test (difficulty)
if %self.longdesc% ~= %test%
  set string %self.longdesc%
  set replace %string.car%
  set string %string.cdr%
  while %string%
    set word %string.car%
    set string %string.cdr%
    if %word% != %test%
      set replace %replace% %word%
    end
  done
  if %replace%
    %mod% %self% longdesc %replace%
  end
end
~
#12822
Celestial Forge: Mob gains no-attack on load~
0 nA 100 0
~
* turn on no-attack (until diff-sel)
dg_affect %self% !ATTACK on -1
~
#12823
Celestial Forge: Message when no-attack mob is attacked~
0 B 0 0
~
if %self.aff_flagged(!ATTACK)%
  %send% %actor% You need to choose a difficulty before you can fight ~%self%.
  %send% %actor% Usage: difficulty <normal \| hard \| group \| boss>
  %echoaround% %actor% ~%actor% considers attacking ~%self%.
  return 0
else
  * no need for this script anymore
  detach 12823 %self.id%
  return 1
end
~
#12824
Celestial Forge: Arena commands~
2 cD 0 0
flee respawn~
if %cmd% == flee
  %send% %actor% You cannot flee from this challenge. You must face it and live or die on your own merits.
elseif %cmd% == respawn
  %send% %actor% You cannot respawn from here. The spirit of the forge will restore you in a moment.
else
  return 0
end
~
#12832
Lodestone Firefly: Northward pull~
0 bt 8 0
~
set message %random.3%
set ch %self.room.people%
while %ch%
  if %ch.is_pc% && %ch.can_see(%self%)%
    switch %message%
      case 1
        %send% %ch% ~%self% tries to draw you to the %ch.dir(north)%.
      break
      case 2
        %send% %ch% ~%self% darts toward the %ch.dir(north)%.
      break
      case 3
        %send% %ch% ~%self% seems drawn to the %ch.dir(north)%.
      break
    done
  end
  set ch %ch.next_in_room%
done
~
#12833
Celestial Forge: Buy mastery item~
1 n 100 6
L o 12810
L o 12811
L o 12850
L o 12851
L w 5100
L w 5101
~
set actor %self.carried_by%
if !%actor%
  %purge% %self%
  halt
end
switch %self.vnum%
  case 12833
    set requires 12810
    set grants 12811
    set shard 5100
    set refund 1000
  break
  case 12872
    set requires 12850
    set grants 12851
    set shard 5101
    set refund 1000
  break
  default
    %echo% @%self% is not implemented.
    %purge% %self%
    halt
  break
done
*
set do_refund 0
if %actor.is_npc%
  * oops
elseif !%actor.ability(%requires%)%
  %send% %actor% You require %_abil.name(%requires%)% to buy this.
  set do_refund 1
elseif %actor.ability(%grants%)%
  %send% %actor% You already know %_abil.name(%grants%)%.
  set do_refund 1
else
  %send% %actor% You're inspired! You learn %_abil.name(%grants%)%!
  nop %actor.add_bonus_ability(%grants%)%
end
if %do_refund%
  nop %actor.give_currency(%shard%,%refund%)%
  eval curname %%currency.%shard%(%refund%)%%
  %send% %actor% You're refunded %refund% %curname%.
end
%purge% %self%
~
#12834
Shard companion: Buy shard companion~
1 n 100 11
L b 12834
L b 12844
L c 12879
L c 12880
L c 12881
L f 12837
L w 5100
L w 5101
L w 5102
L w 5103
L w 5104
~
set cost 150
*
* list in order from highest to lowest, count=max
set comp_list 12844 12834
set comp_count 2
*
* init
set tier 1
set error 0
set has_tier 0
set has_vnum 0
*
* Determine player stats
set actor %self.carried_by%
if !%actor%
  %purge% %self%
  halt
end
*
* Determine my stats
switch %self.vnum%
  case 12834
    set tier 1
    set new_vnum 12834
    set upgrade tank
  break
  case 12835
    set tier 1
    set new_vnum 12834
    set upgrade dps
  break
  case 12836
    set tier 1
    set new_vnum 12834
    set upgrade caster
  break
  case 12879
    set tier 2
    set new_vnum 12844
    set upgrade tank
  break
  case 12880
    set tier 2
    set new_vnum 12844
    set upgrade dps
  break
  case 12881
    set tier 2
    set new_vnum 12844
    set upgrade caster
  break
  default
    %send% %actor% This companion is not yet implemented.
    set error 1
  break
done
*
* Determine existing
while %comp_list% && !%has_tier%
  set vnum %comp_list.car%
  set comp_list %comp_list.cdr%
  if %actor.has_companion(%vnum%)%
    set has_tier %comp_count%
    set has_vnum %vnum%
    if !%actor.companion% || %actor.companion.vnum% != %vnum%
      %mod% %actor% companion %vnum%
    end
  end
  eval comp_count %comp_count% - 1
done
if %has_tier% && (!%actor.companion% || %actor.companion.vnum% != %vnum%)
  %send% %actor% There was an error updating your shard. Please report this as a bug (1).
  set error 1
end
*
* Validate
if !%error% && %has_tier% > %tier%
  %send% %actor% You already have a higher level Celestial Forge companion!
  set error 1
end
*
* check new companion
if !%error% && %tier% > %has_tier%
  nop %actor.remove_companion(%has_vnum%)%
  nop %actor.add_companion(%new_vnum%)%
end
*
* ensure we have him loaded and then upgrade him
if !%error%
  if !%actor.companion% || %actor.companion.vnum% != %new_vnum%
    %mod% %actor% companion %new_vnum%
  end
  if !%actor.companion% || %actor.companion.vnum% != %new_vnum%
    %send% %actor% There was an error updating your shard companion. Please report this as a bug (2).
    set error 1
  end
  *
  * find upgrades
  set comp %actor.companion%
  set tank %comp.var(tank,0)%
  set dps %comp.var(dps,0)%
  set caster %comp.var(caster,0)%
  * test first
  if (%tank% + %dps% + %caster%) >= 3
    %send% %actor% Your shard companion is already fully upgraded.
    set error 1
  else
    eval %upgrade% %%%upgrade%%% + 1
    remote %upgrade% %comp.id%
    if !%comp.has_trigger(12837)%
      attach 12837 %comp.id%
    end
  end
end
*
* Refund?
if %error%
  eval genv 5100 + %tier% - 1
  nop %actor.give_currency(%genv%,%cost%)%
  eval curname %%currency.%genv%(%cost%)%%
  %send% %actor% You receive a refund of %cost% %curname%.
end
*
* And purge
%purge% %self%
~
#12835
Shard companion: Load script~
0 nt 100 1
L f 12837
~
wait 1
%echo% ~%self% assembles itself and whirs to life!
* also verify setup
if !%self.has_trigger(12837)%
  attach 12837 %self.id%
end
~
#12836
Shard companion: Death trigger~
0 ft 100 8
L b 12834
L b 12844
L w 5100
L w 5101
L w 5102
L w 5103
L w 5104
L w 12833
~
set actor %self.companion%
if !%actor% || %actor.is_npc%
  halt
end
*
* cancel ability cooldown
if %actor.cooldown(12833)%
  nop %actor.set_cooldown(12833,0)%
end
*
* check for refund?
set tier 0
switch %self.vnum%
  case 12834
    set tier 1
  break
  case 12844
    set tier 2
  break
done
if %tier%
  * refund shard type
  eval genv 5100 + %tier% - 1
  * refund amount based on levels
  eval number %self.var(tank,0)% + %self.var(dps,0)% + %self.var(caster,0)%
  set cost 0
  while %number% > 0
    eval cost %cost% + %random.90% + 10
    eval number %number% - 1
  done
  * refund
  nop %actor.give_currency(%genv%,%cost%)%
  eval curname %%currency.%genv%(%cost%)%%
  %send% %actor% You scavenge %cost% %curname% as ~%self% falls apart.
end
*
* and delete me
nop %actor.remove_companion(%self.vnum%)%
~
#12837
Shard companion: Setup and update~
0 bt 100 8
L b 12834
L b 12844
L c 12808
L w 12834
L w 12835
L w 12836
L w 12837
L w 12838
~
* list of triggers available
set trigger_list 12841 12842 12843 12844
* handles naming and stats on purchase or summon
set order 1
set changed 0
*
* detect upgrades
set tank %self.var(tank,0)%
set dps %self.var(dps,0)%
set caster %self.var(caster,0)%
set use_triggers
*
* set traits and attach scripts
nop %self.add_mob_flag(NO-ATTACK)%
* tank
if %tank% >= 1
  nop %self.add_mob_flag(CHAMPION)%
  nop %self.add_mob_flag(TANK)%
end
if %tank% >= 2
  eval amount %self.level% / 25
  dg_affect #12836 %self% off
  dg_affect #12836 %self% RESIST-PHYSICAL %amount% -1
  dg_affect #12836 %self% RESIST-MAGICAL %amount% -1
end
if %tank% >= 3
  dg_affect #12841 %self% off
  dg_affect #12841 %self% CRAFTING 1 -1
  set use_triggers %use_triggers% 12841
end
* damage
if %dps% >= 1
  nop %self.remove_mob_flag(NO-ATTACK)%
  if %caster% >= 1
    nop %self.add_mob_flag(DPS)%
  end
end
if %dps% >= 2
  eval amount %self.level% / 100 + 1
  dg_affect #12834 %self% off
  dg_affect #12834 %self% BONUS-PHYSICAL %amount% -1
  dg_affect #12834 %self% BONUS-MAGICAL %amount% -1
  set use_triggers %use_triggers% 12842
end
if %dps% >= 3
  nop %self.add_mob_flag(DPS)%
  if !%self.affect(12835)%
    dg_affect #12835 %self% !DISARM on -1
  end
end
* caster
if %caster% >= 1
  nop %self.add_mob_flag(CASTER)%
  set use_triggers %use_triggers% 12843
  dg_affect #12837 %self% off
  dg_affect #12837 %self% CRAFTING 1 -1
end
if %caster% >= 2
  eval amount %self.level% / 100 + 1
  dg_affect #12838 %self% off
  dg_affect #12838 %self% BONUS-PHYSICAL %amount% -1
  dg_affect #12838 %self% BONUS-MAGICAL %amount% -1
  if !%self.eq(wield)%
    * magic attack
    %load% obj 12808 %self% wield
  end
end
if %caster% >= 3
  set use_triggers %use_triggers% 12844
end
*
* check triggers
while %trigger_list%
  set trig %trigger_list.car%
  set trigger_list %trigger_list.cdr%
  if %use_triggers% ~= %trig%
    if !%self.has_trigger(%trig%)%
      attach %trig% %self.id%
    end
  elseif %self.has_trigger(%trig%)%
    detach %trig% %self.id%
  end
done
*
* determine names
if %tank% && %dps% && %caster%
  set name maelstone
  set pose whirls chaotically around you
  set order 2
elseif %tank% == 2 && %dps%
  set name crusher
  set pose grinds forward ahead of you
  set order 2
elseif %dps% == 2 && %tank%
  set name mauler
  set pose hammers violently around the area
  set order 2
elseif %tank% && %dps%
  set name shrapnel
  set pose bristles and creaks before you
  set order 2
elseif %tank% == 2 && %caster%
  set name anchor
  set pose stills the air around you
  set order 2
elseif %caster% == 2 && %tank%
  set name gravitic
  set pose swirls with flying shards
elseif %tank% && %caster%
  set name bulwark
  set pose stands immovable before you
  set order 2
elseif %dps% == 2 && %caster%
  set name stormrending
  set pose whirs with spinning blades
elseif %caster% == 2 && %dps%
  set name fluxshard
  set pose spasms with unstable magnetic surges
  set order 2
elseif %dps% && %caster%
  set name stormcoil
  set pose crackles and sparks above you
  set order 2
elseif %tank% >= 3
  set name bastion
  set pose stands like an unbreakable wall
  set order 2
elseif %tank%
  set name plated
  set pose towers grimly over you
elseif %dps% >= 3
  set name shardmaw
  set pose thrashes and roars around you
  set order 2
elseif %dps%
  set name jagged
  set pose gnashes and roils before you
elseif %caster% >= 3
  set name magnetar
  set pose churns and screeches in the air
  set order 2
elseif %caster%
  set name magnetic
  set pose whirls in the air above you
else
  set name rough
  set pose is here
end
* descs
if %tank%
  set desc_class It's a towering figure with massive, broad shoulders supporting a helm with narrow, slitted eyes that emit a dim, steady light.
end
if %tank% >= 3
  set desc_ext %desc_ext% Plates of iron are interlocked seamlessly, creating an impenetrable fortress of a body. It exudes an aura of unyielding defense.
elseif %tank% >= 2
  set desc_ext %desc_ext% It exudes an aura of unyielding defense.
end
if %dps%
  set desc_class %desc_class% Its angular limbs are adorned with razor-sharp edges; each movement is accompanied by a sharp, metallic hiss, the sound of frictionless precision.
end
if %dps% >= 3
  set desc_ext %desc_ext% Eyes like incandescent coals burn with intense focus, locked onto the target.
elseif %dps% >= 2
  set desc_ext %desc_ext% Glistening iron muscles ripple with latent power, ready to unleash swift, brutal force.
end
if %caster%
  set desc_class %desc_class% Every part of the elemental is intricately covered in arcane symbols, glowing with an ethereal blue light. Its frame is edged with filigree that seems to shift and shimmer.
end
if %caster% >= 3
  set desc_ext %desc_ext% Its eyes are twin orbs of swirling light, constantly shifting in color and intensity.
elseif %caster% >= 2
  set desc_ext %desc_ext% An aura of otherworldly power envelops it.
end
*
* type portion
switch %self.vnum%
  case 12834
    set metal iron
    set desc_base The heavy, dark gray elemental is build from interlocking shards of iron.
  break
  case 12844
    set metal imperium
    set desc_base The elemental gleams bright white, reflecting every stray beam of light that touches it.
  break
  default
    set metal tin
    set desc_base The elemental looks to be made from old tin.
  break
done
*
* prepare for naming
set kws elemental %name% %metal%
if %order% == 1
  set shortd %name.ana% %name% %metal% elemental
  set longd &Z%name.ana% %name% %metal% elemental %pose%.
elseif %order% == 2
  set shortd %metal.ana% %metal% %name% elemental
  set longd &Z%metal.ana% %metal% %name% elemental %pose%.
end
*
* set names and check for changes
set oldname %self.name%
if %kws% != %self.pc_name%
  %mod% %self% keyword %kws%
  set changed 1
end
if %shortd% != %self.name%
  %mod% %self% shortdesc %shortd%
  set changed 1
end
if %longd% != %self.longdesc%
  %mod% %self% longdesc %longd%
  set changed 1
end
* desc
%mod% %self% lookdesc %desc_base% %desc_class% %desc_ext%
%mod% %self% append-lookdesc-noformat &0   The elemental will accept commands via 'order companion'.
*
* messaging
if %changed%
  %echo% %oldname% clacks and clangs as it reconfigures itself into %self.name%!
else
  %echo% ~%self% clacks and clangs as it configures itself.
end
*
* rescale and detach
%scale% %self% %self.level%
detach 12837 %self.id%
~
#12838
Celestial Forge: Set up training dummy with use~
1 c 6 2
L b 12838
L b 12873
use~
* List of dummies to exclude here
set dummy_list 12838 12873
*
if %actor.obj_target(%arg.argument1%)% != %self%
  return 0
  halt
end
* validate
set room %actor.room%
if %actor.fighting% || %actor.disabled%
  %send% %actor% You're a little busy for that right now.
  halt
elseif %actor.position% != Standing
  %send% %actor% You need to be standing to set up @%self%.
  halt
elseif !%actor.canuseroom_guest(%room%)%
  %send% %actor% You don't have permission to set up training dummies here.
  halt
end
* check other dummies
set ch %room.people%
while %ch%
  if %ch.is_npc% && %dummy_list% ~= %ch.vnum%
    %send% %actor% There is already ~%ch% set up here.
    halt
  end
  set ch %ch.next_in_room%
done
* ok!
%load% mob %self.val0%
set mob %room.people%
if %mob.vnum% == %self.val0%
  %send% %actor% You set up ~%mob%!
  %echoaround% %actor% ~%actor% sets up ~%mob%!
  set time %timestamp%
  remote time %mob.id%
else
  %send% %actor% It looks like the dummy failed when you try to set it up. Report this as a bug.
end
%purge% %self%
~
#12839
Celestial Forge: Expire training dummy~
0 b 2 0
~
if %self.fighting%
  halt
end
set ch %self.room.people%
while %ch%
  if %ch.is_pc%
    halt
  end
  set ch %ch.next_in_room%
done
if (%timestamp% - %self.var(time,0)%) > 64800
  %echo% ~%self% breaks down and collapses in a heap.
  %purge% %self%
  halt
end
~
#12840
Celestial Forge: Mobs can't start combat with dummies~
0 B 0 0
~
if %actor.is_npc% && !%self.fighting%
  %send% %actor% Mobs can't start combat against this dummy.
  return 0
  halt
else
  return 1
end
~
#12841
Shard companion: Tank tier 3 commands: Rebuild, Reinforce, Reproach, Reset~
0 ct 0 4
L w 12827
L w 12828
L w 12832
L w 12833
rebuild reinforce reproach reset~
if %actor% != %self%
  return 0
  halt
end
set actor %self.companion%
if !%actor%
  halt
end
if %self.cooldown(12833)%
  %send% %actor% Your elemental companion is still recharging.
  halt
end
*
if %cmd% == rebuild
  * heal 20% over 15 seconds
  if %actor.cooldown(12827)% || %self.cooldown(12827)%
    %send% %actor% Your elemental companion is still rebuilding.
    halt
  end
  eval amount %self.maxhealth% / 15
  dg_affect #12832 %self% HEAL-OVER-TIME %amount% 15
  %echo% &&Y~%self% begins rebuilding itself from shards!&&0
  * special cooldown
  nop %self.set_cooldown(12827,60)%
  nop %actor.set_cooldown(12827,60)%
  * and a normal one
  set cooldown 30
elseif %cmd% == reinforce
  * boost resists
  eval amount %self.level% / 8
  dg_affect #12828 %self% off
  dg_affect #12828 %self% RESIST-PHYSICAL %amount% 30
  dg_affect #12828 %self% RESIST-MAGICAL %amount% 30
  %echo% &&Y~%self% hardens its shards and reinforces itself!&&0
  set cooldown 30
elseif %cmd% == reproach
  * super-taunt
  %echo% &&Y~%self% grinds and roars in a mighty reproach!&&0
  set ch %self.room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch% != %self% && %ch% != %actor% && !%actor.is_ally(%ch%)% && %self.can_fight(%ch%)% && %actor.can_fight(%ch%)%
      if !%ch.fighting% || %ch.fighting% != %self%
        %force% %ch% mkill %self%
      end
    end
    set ch %ch.next_in_room%
  done
  set cooldown 30
elseif %cmd% == reset
  * remove debuffs
  %echo% &&YA shard from ~%self% dissolves into a ball of gleaming metallic mana...&&0
  cleanse
  set cooldown 30
end
if %cooldown%
  nop %self.set_cooldown(12833,%cooldown%)%
  nop %actor.set_cooldown(12833,%cooldown%)%
end
~
#12842
Shard companion: DPS tier 2 command: actuate~
0 ct 0 5
L w 12829
L w 12830
L w 12831
L w 12833
L w 12842
actuate~
if %actor% != %self%
  return 0
  halt
end
set actor %self.companion%
if !%actor%
  halt
end
if %self.cooldown(12833)%
  %send% %actor% Your elemental companion is still recharging.
  halt
end
*
if %arg%
  set enemy %self.char_target(%arg%)%
  if !%enemy%
    %send% %actor% No one by that name for your companion to target.
    halt
  end
else
  set enemy %self.fighting%
  if !%enemy%
    %send% %actor% Your elemental companion isn't fighting anything.
    halt
  end
end
*
if %self.is_ally(%enemy%)%
  %send% %actor% Your elemental companion can't do that to ~%enemy%.
  halt
elseif %self.fighting% != %enemy% && (!%actor.can_fight(%enemy%)% || !%self.can_fight(%enemy%)%)
  %send% %actor% Your elemental companion can't attack ~%enemy%.
  halt
end
*
set dps %self.var(dps)%
set move %random.4%
*
if %move% == 1
  * breach: debuff target's resistances
  eval amount %self.level% / 8
  dg_affect #12842 @%self% %enemy% off
  dg_affect #12842 %enemy% RESIST-PHYSICAL -%amount% 30
  dg_affect #12842 %enemy% RESIST-MAGICAL -%amount% 30
  %echo% &&Y~%self% jabs ~%enemy% with shard after shard, breaching ^%enemy% defenses!&&0
  if !%self.fighting%
    mkill %enemy%
  end
  set cooldown 30
elseif %move% == 2
  * shard pin: immobilize / stun
  %echo% &&Y~%self% launches jagged shards at ~%enemy%, pinning *%enemy% down!&&0
  if %enemy.aff_flagged(IMMUNE-PHYSICAL-DEBUFFS)%
    * alternate move (immune)
    eval amount %self.level% / 10
    dg_affect #12831 @%self% %enemy% off silent
    dg_affect #12831 %enemy% DODGE -%amount% 30
  elseif %dps% >= 3 && !%enemy.aff_flagged(!STUN)%
    * stun
    dg_affect #12831 @%self% %enemy% off silent
    dg_affect #12831 %enemy% STUNNED on 5
  else
    * immobilize
    dg_affect #12831 @%self% %enemy% off silent
    dg_affect #12831 %enemy% IMMOBILIZED on 15
  end
  if !%self.fighting%
    mkill %enemy%
  end
  set cooldown 30
elseif %move% == 3
  * shard flurry: slow target
  if %dps% >= 3
    eval duration 30
  else
    eval duration 20
  end
  dg_affect #12830 @%self% %enemy% off
  dg_affect #12830 %enemy% SLOW on %durtion%
  %echo% &&Y~%self% unleashes a shard flurry at ~%enemy%, slowing ^%enemy% advance considerably!&&0
  if !%self.fighting%
    mkill %enemy%
  end
  set cooldown 30
elseif %move% == 4
  * shard pen: reduce dodge
  if %dps% >= 3
    eval amount %self.level% / 8
  else
    eval amount %self.level% / 10
  end
  dg_affect #12829 @%self% %enemy% off
  dg_affect #12829 %enemy% DODGE -%amount% 30
  %echo% &&Y~%self% creates a pen around ~%enemy% using shards from its body!&&0
  if !%self.fighting%
    mkill %enemy%
  end
  set cooldown 30
end
if %cooldown%
  nop %self.set_cooldown(12833,%cooldown%)%
  nop %actor.set_cooldown(12833,%cooldown%)%
end
~
#12843
Shard companion: Caster tier 1 and 2 magnetize move~
0 ct 0 6
L w 12833
L w 12839
L w 12840
L w 12843
L w 12844
L w 12845
magnetize~
if %actor% != %self%
  return 0
  halt
end
set actor %self.companion%
if !%actor%
  halt
end
if %self.cooldown(12833)%
  %send% %actor% Your elemental companion is still recharging.
  halt
end
*
set caster %self.var(caster,1)%
if %caster% > 1
  set duration 120
else
  set duration 30
end
*
eval last_cmd %self.var(last_cmd,0)% + 1
if %last_cmd% > 5 || (%last_cmd% > 4 && !%self.var(allow_cmd_5)%)
  * commands 1-4 unless player meets requirements, then it allows 5
  set last_cmd 1
end
remote last_cmd %self.id%
*
if %last_cmd% == 1
  * Haste IF player isn't already hastened OR is already fighting
  if %actor.aff_flagged(HASTE)% || %actor.fighting%
    set last_cmd 2
    remote last_cmd %self.id%
    set allow_cmd_5 1
    remote allow_cmd_5 %self.id%
  else
    * Electromagnetic (haste buff)
    %echo% The air crackles with static as ~%self% begins to glow...
    %echo% &&YBits of iron hang in the air as a magnetic pulse passes over ~%actor%!&&0
    dg_affect #12843 %actor% off silent
    dg_affect #12843 %actor% HASTE on %duration%
    set cooldown 30
  end
end
if %last_cmd% == 2
  * To-Hit boost (may have cascaded from 1)
  if %caster% >= 3
    eval amount %self.level% / 5
  elseif %caster% >= 2
    eval amount %self.level% / 8
  else
    eval amount %self.level% / 10
  end
  %echo% ~%self% hums with arcane energy as the symbols on its body glow brighter...
  %echo% &&YWave after wave of energy flows over ~%actor%!&&0
  dg_affect #12844 %actor% off silent
  dg_affect #12844 %actor% TO-HIT %amount% %duration%
  set cooldown 30
elseif %last_cmd% == 3
  * dodge
  if %caster% >= 3
    eval amount %self.level% / 5 * 1.43
  elseif %caster% >= 2
    eval amount %self.level% / 5
  else
    eval amount %self.level% / 10
  end
  %echo% &&Y~%self% extends its magnetic deflection field over ~%actor%!&&0
  dg_affect #12840 %actor% off silent
  dg_affect #12840 %actor% DODGE %amount% %duration%
  set cooldown 30
elseif %last_cmd% == 4
  * boosts damage
  set weap %actor.eq(wield)%
  if %weap%
    if %weap.attack(magic)%
      set field BONUS-MAGICAL
      set desc glowing
    else
      set field BONUS-PHYSICAL
      set desc jagged
    end
  else
    set field BONUS-PHYSICAL
    set desc jagged
  end
  eval amount %self.level% / 25
  %echo% ~%self% draws delicate symbols in the air using solid light...
  %echo% &&YDozens of tiny %desc% shards attach themselves to ~%actor%!&&0
  dg_affect #12845 %actor% off silent
  dg_affect #12845 %actor% %field% %amount% %duration%
  set cooldown 30
elseif %last_cmd% == 5
  * special regen move IF player had haste (skipped otherwise)
  if %actor.role% == Caster || %actor.role% == Healer
    set field MANA-REGEN
  elseif %actor.role% == Tank || %actor.role% == Melee
    set field MOVE-REGEN
  elseif %actor.mana% < %actor.maxmana%
    set field MANA-REGEN
  else
    set field MOVE-REGEN
  end
  if %caster% >= 3
    eval amount %self.level% / 10
  elseif %caster% >= 2
    eval amount %self.level% / 15
  else
    eval amount %self.level% / 30
  end
  %echo% &&Y~%self% vibrates near ~%actor% with a strong magnetic resonance!&&0
  dg_affect #12839 %actor% off silent
  dg_affect #12839 %actor% %field% %amount% %duration%
  set cooldown 30
  * only allowed once
  rdelete allow_cmd_5 %self.id%
end
if %cooldown%
  nop %self.set_cooldown(12833,%cooldown%)%
  nop %actor.set_cooldown(12833,%cooldown%)%
end
~
#12844
Shard companion: Caster tier 3 auto-cast~
0 k 67 1
L w 12833
~
* this ONLY appears on the Caster 3 module
* This just periodically triggers the 'magnetize' command trigger.
set actor %self.companion%
if !%actor%
  halt
elseif %self.cooldown(12833)%
  * cooldown
  halt
else
  * just trigger my command script
  wait 1
  magnetize
end
~
#12845
Celestial Forge: Open shard box~
1 c 2 0
open~
if %actor.obj_target(%arg.argument1%)% != %self%
  return 0
  halt
elseif %self.val0% <= 0 || %self.val1% < 1
  %send% %actor% @%self% seems to be empty.
  halt
end
* ok
nop %actor.give_currency(%self.val0%,%self.val1%)%
eval name %%currency.%self.val0%(%self.val1%)%%
%send% %actor% You open @%self% and gain %self.val1% %name%!
%echoaround% %actor% ~%actor5 opens @%self% and gains %self.val1% %name%!
%purge% %self%
~
#12850
Celestial Forge: Change camp standards on entry~
2 gA 100 9
L b 12851
L b 12852
L b 12855
L c 12850
L j 12851
L j 12852
L j 12853
L j 12854
L j 12855
~
set room_list 12851 12852 12853 12854 12855
* sanity
if %method% != portal || %actor.is_npc% || !%actor.empire% || !%was_in%
  halt
elseif %actor.empire% != %was_in.empire%
  * not coming from own empire
  halt
end
* short delay so only the first to enter this way triggers the change, if someone is following
wait 1
* detect color
set color %actor.empire.banner_name_simple%
if !%color% || %color% == none
  halt
end
* update all the banners
while %room_list%
  set vnum %room_list.car%
  set room_list %room_list.cdr%
  set there %instance.nearest_rmt(%vnum%)%
  if %there%
    set obj %there.contents(12850)%
    if %obj%
      * update 1 banner
      %mod% %obj% longdesc &Z%color.ana% %color% camp standard flies over the forge.
      %mod% %obj% lookdesc The %color% camp standard rises above the forge, snapping and sighing in the night wind. Firelight from the smelter makes it visible against the starry night sky, splashed with the color of old embers and fresh blood.
      %mod% %obj% append-lookdesc In the center of the %color% standard is the symbol of %actor.empire.name%.
      %at% %there% %echo% The camp standard ripples and gleams as it changes to a new %color% emblem.
    end
  end
done
* and update mobs
set Mateo %instance.mob(12851)%
set Amina %instance.mob(12852)%
set Oksana %instance.mob(12855)%
if %Mateo%
  %mod% %Mateo% lookdesc The master campwright is dressed in tight black trousers and a white shirt under a short, fitted black jacket, wrapped at the waist with %color.ana% %color% sash.
  %mod% %Mateo% append-lookdesc His face, soot-stained from work at the smelter, is gentle, with sharp creases around his eyes. On his head, he wears a wide-brimmed black hat, though the sun has already set.
end
if %Amina%
  %mod% %Amina% lookdesc The forgehand is steady as she raises and drops her hammer onto the anvil over and over, in perfect rhythm. She wears a hefty leather apron over a loose white dress
  %mod% %Amina% append-lookdesc hemmed just above the ankle with patterns of red, yellow, and green. Over her shoulders, she has pinned a gold shawl with colorful beads and her head is
  %mod% %Amina% append-lookdesc covered in a brimless gold cap decorated on the front with a stylized %color% flower.
end
if %Oksana%
  %mod% %Oksana% lookdesc She wears a white blouse with %color% flowers, tucked loosely into a striped red and white skirt. Her long brown hair flows freely over her shoulders and, at the
  %mod% %Oksana% append-lookdesc top, she has adorned it with flowers of all colors. Though she isn't working the anvil, she has the muscled build of a smith, and the old burn scars on her arms from years of toil at the forge.
end
~
#12851
Celestial Forge: Fake exits from Victory Forge~
2 c 0 0
north south east west northeast ne southeast se southwest sw northwest nw down~
set dir %actor.parse_dir(%cmd%)%
eval to_room %%room.%dir%(room)%%
if !%to_room%
  if %dir% == down
    %send% %actor% You start to wander down the hill but somehow end up back by the forge.
  else
    %send% %actor% You wander %actor.dir(%dir%)% but somehow end up back by the forge.
  end
  set ch %room.people%
  while %ch%
    if %ch% != %actor%
      if %dir% == down
        %send% %ch% ~%actor% starts to wander down the hill but somehow ends up back by the forge.
      else
        %send% %ch% ~%actor% wanders %ch.dir(%dir%)% but somehow ends up back by the forge.
      end
    end
    set ch %ch.next_in_room%
  done
else
  return 0
end
~
#12857
Celestial Forge: War Machine phase 1 to 2~
0 l 85 1
L b 12858
~
%load% mob 12858
set mob %self.room.people%
if %mob.vnum% == 12858
  set diff %self.var(diff,1)%
  remote diff %mob.id%
  nop %mob.remove_mob_flag(HARD)%
  nop %mob.remove_mob_flag(GROUP)%
  if %diff% == 1
    * Then we don't need to do anything
  elseif %diff% == 2
    nop %mob.add_mob_flag(HARD)%
  elseif %diff% == 3
    nop %mob.add_mob_flag(GROUP)%
  elseif %diff% == 4
    nop %mob.add_mob_flag(HARD)%
    nop %mob.add_mob_flag(GROUP)%
  end
  nop %mob.unscale_and_reset%
  %scale% %mob% %self.level%
  set scaled 1
  remote scaled %mob.id%
  * messaging
  %echo% &&wPlate after plate breaks off of the War Machine, exposing its weapons!&&0
  %force% %mob% maggro %self.fighting%
  %purge% %self%
end
~
#12858
Celestial Forge: War Machine phase 2 to 3~
0 l 30 1
L b 12859
~
%load% mob 12859
set mob %self.room.people%
if %mob.vnum% == 12859
  set diff %self.var(diff,1)%
  remote diff %mob.id%
  nop %mob.remove_mob_flag(HARD)%
  nop %mob.remove_mob_flag(GROUP)%
  if %diff% == 1
    * Then we don't need to do anything
  elseif %diff% == 2
    nop %mob.add_mob_flag(HARD)%
  elseif %diff% == 3
    nop %mob.add_mob_flag(GROUP)%
  elseif %diff% == 4
    nop %mob.add_mob_flag(HARD)%
    nop %mob.add_mob_flag(GROUP)%
  end
  nop %mob.unscale_and_reset%
  %scale% %mob% %self.level%
  set scaled 1
  remote scaled %mob.id%
  * messaging
  %echo% &&wA trumpet sounds as the battered War Machine rallies for one last stand!&&0
  %force% %mob% maggro %self.fighting%
  %purge% %self%
end
~
#12859
War Machine phase 2 combat: Burning Catapult, Whirling Sawblades, Hammer Tremor, Scalding Vents, Grapple Winch~
0 c 0 5
L w 9602
L w 12821
L w 12857
L w 12858
L w 12859
!catapult !whirl !tremor !scald !winch~
set targ %arg%
set room %self.room%
set diff %self.var(diff,1)%
set cmd %cmd.substr(1)%
if %actor% != %self% || !%targ% || %targ.id% == %self.id%
  halt
elseif %cmd% == catapult
  * Burning Catapult (dodge)
  scfight clear dodge
  %echo% &&wSmoke streams upward as the burning arm of a catapult extends from the War Machine!&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  set cycle 1
  eval pain 100 * (diff + 1)
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    set targ %random.enemy%
    if !%targ%
      halt
    end
    %send% %targ% &&w**** The catapult takes aim... at you! ****&&0 (dodge)
    %echoaround% %targ% &&wThe catapult takes aim... at ~%targ%!&&0
    scfight setup dodge %targ%
    set targ_id %targ.id%
    wait %wait% s
    if %targ% && %targ_id% == %targ.id%
      * still here
      if %targ.var(did_scfdodge)%
        %echo% &&wThere's a fiery explosion as the catapult hits the ground mere feet from where ~%targ% dodged!&&0
        if %diff% == 1
          dg_affect #12821 %targ% TO-HIT 25 20
        end
      else
        * hit!
        %echo% &&wA burning mass of pitch and tar from the catapult explodes into |%targ% chest!&&0
        %damage% %targ% %pain% physical
      end
    end
    scfight clear dodge
    eval cycle %cycle% + 1
  done
  wait 8 s
elseif %cmd% == whirl
  * Whirling Sawblades (group duck)
  scfight clear duck
  %echo% &&wThe War Machine stops for a moment and creaks as long sawblades extend from its sides...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  %echo% &&w**** The War Machine begins to spin, slowly at first, then faster, and faster... ****&&0 (duck)
  set cycle 1
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    scfight setup duck all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfduck)%
          %echo% &&wA sawblade catches ~%ch%, ripping through ^%ch% flesh, causing deep and grievous wounds!&&0
          %dot% #12857 %ch% 100 30 physical 5
          %damage% %ch% 50 physical
        elseif %ch.is_pc%
          %send% %ch% &&wYou feel the air slice past the top of your head as you narrowly duck the whirling sawblade.&&0
          if %diff% == 1
            dg_affect #12821 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&w**** It's still spinning! ****&&0 (duck)
        end
      end
      set ch %next_ch%
    done
    scfight clear duck
    eval cycle %cycle% + 1
  done
  wait 8 s
elseif %cmd% == tremor
  * Hammer Tremor (group jump)
  scfight clear jump
  %echo% &&wA mast rises from the top of the War Machine...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  %echo% &&w**** The War Machine raises its tremendous mast to the highest position... this isn't good! ****&&0 (jump)
  set cycle 1
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    scfight setup jump all
    wait %wait% s
    %echo% &&wThe mast of the War Machine slams downward with a deafening thud! A tremor ripples across the battlefield...&&0
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfjump)%
          if %ch.aff_flagged(!STUN)%
            %echo% &&wThe tremor knocks ~%ch% into a broken wagon!&&0
          else
            %echo% &&wThe tremor knocks ~%ch% to the ground!&&0
            dg_affect #12858 %ch% STUNNED on 10
          end
          %damage% %ch% 50 physical
        elseif %ch.is_pc%
          %send% %ch% &&wYou time your jump perfectly to avoid the tremor.&&0
          if %diff% == 1
            dg_affect #12821 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&w**** The mast is rising again! ****&&0 (jump)
        end
      end
      set ch %next_ch%
    done
    scfight clear jump
    eval cycle %cycle% + 1
  done
  wait 8 s
elseif %cmd% == scald
  * Scalding Vents (group dodge)
  scfight clear dodge
  %echo% &&wA furnace roars within the War Machine as low vents on its sides slide open...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  %echo% &&w**** Steam rises from the open vents as the War Machine turns and takes aim... ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    scfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfdodge)%
          set hit 1
          %echo% &&wA blast of steam from the War Machine scalds ~%ch%!&&0
          %dot% #12859 %ch% 100 30 physical 5
          %damage% %ch% 50 physical
        elseif %ch.is_pc%
          %send% %ch% &&wYou hurl yourself out of the way just in time to avoid a scalding blast of steam!&&0
          if %diff% == 1
            dg_affect #12821 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&w**** The vents are still steaming... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
    eval cycle %cycle% + 1
  done
  wait 8 s
elseif %cmd% == winch
  * Grapple Winch (struggle)
  %echo% &&wA crossbow rises from the top of the War Machine...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  set cycle 1
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    set targ %random.enemy%
    if !%targ%
      halt
    elseif !%targ.affect(9602)%
      * only if not already struggling
      %send% %targ% &&w**** The War Machine fires a grappler at you... You're caught, and being pulled in! ****&&0 (struggle)
      %echoaround% %targ% &&wThe War Machine fires a grappler at ~%targ%... and starts reeling *%targ% in!&&0
      scfight setup struggle %targ% 30
      * messages
      set scf_strug_char You struggle to free yourself from the winch...
      set scf_strug_room ~%%actor%% struggles to free *%%actor%%self from the winch...
      remote scf_strug_char %targ.id%
      remote scf_strug_room %targ.id%
      set scf_free_char You cut yourself free of the winch!
      set scf_free_room ~%%actor%% cuts *%%actor%%self free of the winch!
      remote scf_free_char %targ.id%
      remote scf_free_room %targ.id%
    end
    eval cycle %cycle% + 1
    wait %wait% s
  done
  eval pause 28 - (%wait% * %diff%)
  wait %pause% s
  * punish anybody still grappled
  eval pain 100 * %diff%
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)% && %ch.affect(9602)%
      %echo% &&wThe War Machine reels ~%ch% in and impales *%ch% on a dull spike!&&0
      %damage% %ch% %pain% physical
    end
    set ch %next_ch%
  done
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#12877
Celestial Forge: Banner hawk load script~
0 nt 100 0
~
set guy %self.leader%
if !%guy% || !%guy.is_pc% || !%guy.empire%
  halt
end
set emp %guy.empire%
set color %emp.banner_name_simple%
if %color% != none
  %mod% %self% longdesc A sharp-eyed hawk trails %color.ana% %color% banner from its talons.
  %mod% %self% lookdesc The lean, broad-winged hawk has feathers mottled in gleaming white and stark black. A light harness made of leather crosses its chest. In its talons, it clutches the %color% banner of %emp.name%.
end
~
$
