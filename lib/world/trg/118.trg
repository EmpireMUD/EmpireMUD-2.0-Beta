#11800
Skycleave: Tower global announcement spammer~
2 ab 1
~
if %random.3% == 3
  %regionecho% %room% -100 You can see the Tower Skycleave in the distance. %room.coords%
end
~
#11801
Skycleave: Complex leave rules for floors 2 and 3~
0 sA 100
~
set sneakable_vnums 11815 11816 11817 11841 11842 11843 11844 11845 11846
set maze_vnums 11812 11813 11818 11819 11820 11821 11823 11824 11825 11826
set pixy_vnums 11820
* simple checks
if %direction% == portal || %direction% == down
  halt
end
if %self.var(bribed,0)%
  halt
end
* One quick trick to get the target room
set room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
if (%actor.nohassle% || !%tricky% || %self.disabled% || %self.position% != Standing)
  halt
end
* pixies allow the player to pass if they have given a gift to the queen
if %pixy_vnums% ~= %self.vnum%
  if %actor.var(skycleave_queen,0)% == %dailycycle%
    halt
  end
  * or they're playing the flute?
  if %actor.action% == playing && %actor.eq(hold).vnum% == 11877
    halt
  end
end
* check difficulty if sneakable (group/boss require players to stay behind)
if %actor.aff_flagged(SNEAK)% && %sneakable_vnums% ~= %self.vnum%
  set diff %self.var(diff,1)%
  eval skill_req 25 * %diff%
  if %diff% == 1 || %actor.skill(Stealth)% >= %skill_req%
    * allow sneak
    halt
  end
end
* check direction
if %maze_vnums% ~= %room_var.template%
  * in pixy maze (allow only south)
  if %direction% == south
    halt
  end
else
  * not in pixy maze: block higher template id only
  if (%tricky.template% < %room_var.template%)
    halt
  end
end
* failed
if %actor.aff_flagged(SNEAK)%
  %send% %actor% You can't seem to sneak past ~%self%!
else
  %send% %actor% You can't get past ~%self%!
end
return 0
~
#11802
Skycleave: Bribe mercenaries with coins~
0 m 1
~
set required 500
set given %self.var(given,0)%
if %given% >= %required%
  say Easy money.
  halt
end
eval given %given% + %amount%
remote given %self.id%
if %given% >= %required%
  set bribed 1
  remote bribed %self.id%
  switch %self.vnum%
    case 11841
      * rogue mercenary
      say Don't mind me, I'll just be over here checking the wall.
    break
    case 11842
      * caster mercenary
      say More than double what that cheap enchantress paid. Go on ahead.
    break
    case 11843
      * archer mercenary
      say I won't stop you, but just stay away from the boss.
    break
    case 11844
      * nature mage mercenary
      say Go on, then.
    break
    case 11845
      * vampire mercenary
      say My love for Vye is worth less than this. You may pass.
    break
    case 11846
      * armored mercenary
      say Hard to be a soldier of fortune for what they're paying us. This is the better deal. Go on through.
    break
  done
else
  say It's going to take more than that if you want me to betray Trixton Vye.
end
~
#11803
Skycleave: Custom pickpocket rejection~
0 p 100
~
if %ability% != 142
  * not pickpocket
  return 1
  halt
end
switch %self.vnum%
  case 11833
  case 11838
  case 11938
  case 11900
    * shimmer and spirits
    %send% %actor% You can't pickpocket someone who doesn't even have a body.
  break
  case 11829
  case 11834
  case 11934
    * Otherworlder
    %send% %actor% If it has any pockets, you're not sure where.
  break
  case 11863
  case 11869
    * Shade/Shadow
    %send% %actor% That's just a shadow; it doesn't have any pockets.
  break
  case 11866
  case 11871
    * Mezvienne
    %send% %actor% She's floating; it would be very difficult to pickpocket her.
  break
  case 11872
    * Skithe
    %send% %actor% The Lion of Time definitely does not have pockets.
  break
  case 11884
    * Queen's image
    %send% %actor% She's not really here; only her image sits on the throne.
  break
  case 11888
    * Iskip
    %send% %actor% Even if you could get up to his pockets, anything in there would be enormous.
  break
  case 11972
  case 11981
    * goblin crowds
    %send% %actor% They don't have anything interesting in their pockets.
  break
  case 11988
    * plural mob
    %send% %actor% There's no way to get close enough to pickpocket them without them noticing.
  break
  case 11915
    * prevents global pickpocket
    %send% %actor% Who knows why the caged goblin sings -- but it might be because his pockets are empty.
  break
  case 11916
  case 11917
    * prevents global pickpocket
    %send% %actor% Someone has already emptied the goblin's pockets.
  break
  case 11928
    * First Water
    %send% %actor% You seem to be the only thing in its pocket!
  break
  case 11937
    * skeleton
    %send% %actor% You check the skeleton's pockets, which are on the floor, but someone has beaten you to it.
  break
  default
    %send% %actor% There's no way to get close enough to pickpocket ~%self% without *%self% noticing.
  break
done
return 0
~
#11804
Everflowing flagon (free refills)~
1 bw 10
~
* drinkcon values: 0=capacity, 1=contents, 2=generic vnum
if %self.val2% == 0
  * convert plain water to enchanted water
  nop %self.val2(11804)%
end
* slowly refill water
if %self.val2% == 11804 && %self.val1% < %self.val0%
  eval amt %self.val1% + 12
  if %amt% > %self.val0%
    set amt %self.val0%
  end
  nop %self.val1(%amt%)%
end
~
#11805
Skycleave: Shared load trigger for objects~
1 n 100
~
switch %self.vnum%
  case 11805
    * looker: force look on load
    wait 1
    if %self.carried_by% && %self.carried_by.position% != Sleeping
      %force% %self.carried_by% look
    end
    %purge% %self%
  break
  case 11960
    * donation to the lab
    set actor %self.carried_by%
    set walt %self.room.people(11940)%
    if %actor% && %actor.is_pc%
      wait 1
      if %actor.has_reputation(11800,Venerated)%
        %send% %actor% You already have the maximum reputation with the tower.
      else
        nop %actor.gain_reputation(11800,20)%
      end
      if %walt%
        %force% %walt% say Thanks for that.
      end
    end
    %purge% %self%
  break
done
~
#11806
One-time greetings using script1~
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
if %self.fighting% || %self.disabled%
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
* Show the script1 text
* tell story
set pos 0
set msg %self.custom(script1,%pos%)%
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
* Done: cancel sentinel/silent
if %self.varexists(no_sentinel)%
  nop %self.remove_mob_flag(SENTINEL)%
end
if %self.varexists(no_silent)%
  nop %self.remove_mob_flag(SILENT)%
end
~
#11807
Skycleave: Shared receive trigger (Goblins, Queen, pixies, sorcerers)~
0 j 100
~
if (%self.vnum% >= 11815 && %self.vnum% <= 11818) || %self.vnum% == 11821
  * goblins
  return 0
  set goblin_vnums 11815 11816 11817
  set trinket_vnums 11976 11893 11894 11896
  if %self.vnum% == 11818
    say Venjer cannot be bribed!
    %aggro% %actor%
  elseif !(%trinket_vnums% ~= %object.vnum%) && (!(%object.keywords% ~= goblin) || (%object.keywords% ~= wrangler))
    if %self.vnum% == 11821
      say Hugh Mann don't want %object.shortdesc%, stupid person!
    else
      say Goblin doesn't want %object.shortdesc%, stupid human!
    end
  elseif %self.vnum% == 11821 && %actor.on_quest(11821)% && %object.vnum% == 11976
    %send% %actor% Use 'quest finish Need It For A Friend' instead.
  else
    * actor has given a goblin object
    %send% %actor% You give ~%self% @%object%...
    %echoaround% %actor% ~%actor% gives ~%self% @%object%...
    if %trinket_vnums% ~= %object.vnum%
      say This is what we came for! These is not belonging to you! But thanks, you, for returning.
    else
      say Not what items we came for but still a good prize...
    end
    %purge% %object%
    * despawn goblins here EXCEPT if it's Hugh
    if %self.vnum% != 11821
      set ch %self.room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %ch.is_npc% && %goblin_vnums% ~= %ch.vnum%
          %echo% ~%ch% leaves.
          if %ch% != %self%
            %purge% %ch%
          end
        end
        set ch %next_ch%
      done
      * and leave
      %purge% %self%
    end
  end
elseif %self.vnum% == 11819
  * Pixy Queen's gift
  return 0
  if !%actor.varexists(skycleave_queen)%
    %send% %actor% You try to give @%object% to ~%self% but &%self% rebuffs you.
    %echoaround% %actor% ~%actor% tries to give something to ~%self% but &%self% rebuffs &%actor%.
  elseif %object.vnum% != 1206
    %send% %actor% ~%self% politely refuses @%object%.
  else
    * actor has given queen back a flower
    %send% %actor% You give the queen @%object%...
    %echoaround% %actor% ~%actor% gives the queen @%object%...
    say Oh my! I thought you had forgotten! Oh dear, how very sweet. For you, I will open the maze.
    * open the maze
    makeuid maze room i11825
    set shield %maze.contents(11887)%
    if %shield%
      %at% %maze% %echo% %shield.shortdesc% flickers and fades!
      %purge% %shield%
    end
    * mark player as having visited on this cycle
    set skycleave_queen %dailycycle%
    remote skycleave_queen %actor.id%
    * get rid of iris
    %purge% %object%
  end
elseif %self.vnum% == 11820
  * trash pixies: if the player has met the queen in SNP, an iris will despawn a guard pixy
  return 0
  if !%actor.varexists(skycleave_queen)%
    %send% %actor% You try to give @%object% to ~%self% but &%self% rebuffs you.
    %echoaround% %actor% ~%actor% tries to give something to ~%self% but &%self% rebuffs &%actor%.
  elseif %object.vnum% != 1206
    %send% %actor% ~%self% politely refuses @%object%.
  else
    %send% %actor% You give @%object% to ~%self%, who looks puzzled for a moment, then smiles...
    %echoaround% %actor% ~%actor% gives @%object% to ~%self%, who looks puzzled for a moment, then smiles...
    switch %random.5%
      case 1
        say I am as surprised as I am honored, giant.
      break
      case 2
        say You know a surprising amount about pixies.
      break
      case 3
        say Truly a special gift. Thank you.
      break
      case 4
        say Just like the queen's!
      break
      case 5
        say This is magnificent! I shall treasure it.
      break
    done
    %echo% ~%self% flies off with the iris.
    %purge% %object%
    %purge% %self%
  end
elseif %self.vnum% == 11825 || %self.vnum% == 11925
  * Ravinder: will not keep any item; purges a good jar
  return 0
  if %object.vnum% != 11914
    * wrong item
    %send% %actor% You try to give @%object% to ~%self%, but &%self% doesn't seem to want it.
    %echoaround% %actor% ~%actor% tries to give @%object% to ~%self% but &%self% hands it right back.
    halt
  elseif !%actor.on_quest(11915)%
    %echo% ~%self% refuses |%actor% jar.
    say I don't want your filthy pixy. I'm trying to win on my own merits here.
  elseif %object.var(last_race,0)% + 300 > %timestamp%
    %send% %actor% You can't give @%object% to Ravinder right now (your pixy might still be racing).
  elseif (%object.luck% + %object.guile% + %object.speed%) < 13
    * low stats
    %send% %actor% You hand @%object% to ~%self% and &%self% examines it carefully and hands it back...
    %echoaround% %actor% ~%actor% hands @%object% to ~%self%, who examines it carefully and hands it back...
    say This seems like a perfectly good pixy, but it just isn't championship material. I'm looking to win, not place.
  else
    * ok!
    %echo% ~%self% eagerly takes @%object% from ~%actor%.
    say Oh, splendid, really. This is exactly the sort of pixy I was looking for. O racing world, prepare to tremble!
    %send% %actor% \&0
    %quest% %actor% trigger 11915
    %quest% %actor% finish 11915
    %purge% %object%
  end
elseif %self.vnum% == 11827
  * Marina
  return 0
  if %object.vnum% == 11872
    %echo% ~%self% politely refuses |%actor% note.
    say No thank you, I've already seen it.
  else
    %echo% ~%self% refuses @%object% from ~%actor%.
    say I have a boyfriend.
  end
elseif %self.vnum% == 11828
  * Djon
  return 0
  if %object.vnum% == 11872
    say Incendior! Oh, oops...
    %echo% As ~%self% speaks, the note in |%actor% hand goes up in a burst of flame and a puff of smoke.
    %purge% %object%
  else
    %echo% ~%self% refuses @%object% from ~%actor%.
    say This is an A and B conversation so please C your way out. Door's to your left. Thank you. Bye.
  end
elseif (%self.vnum% == 11835 || %self.vnum% == 11935 || %self.vnum% == 11942) && %object.vnum% == 11872
  * Sanjiv
  wait 1
  if %self.room.people(11847)%
    %echo% ~%self% holds perfectly still, totally ignoring ~%actor%.
  else
    %echo% ~%self% blushes and crumples up the note.
  end
  %purge% %object%
elseif %self.vnum% == 11931 && %object.vnum% == 11872
  * Resident Niamh 3B
  return 0
  %send% %actor% You try to give @%object% to ~%self% but she sighs and refuses it.
  %echoaround% %actor% ~%actor% tries to give something to ~%self% but she sighs and refuses it.
  wait 1
  say Please don't show it to anyone else.
elseif %self.vnum% == 11831 && %object.vnum% == 11872
  * Resident Niamh 3A
  wait 1
  %echo% ~%self% reads the note and her eyes grow wide as it dissolves into inky shadows in her hands.
  wait 1
  say Well, I... I never. I shall look into this.
  %purge% %object%
elseif %self.vnum% == 11970 && %object.vnum% == 11872
  * AHS Niamh 4B
  wait 1
  %echo% ~%self% sighs.
  wait 1
  say Well, we'll just sweep this under the rug. In fact, I'll have him do it himself.
  %purge% %object%
elseif (%self.vnum% == 11959 || || %self.vnum% == 11813 || %self.vnum% == 11913) && %object.vnum% == 11872
  * Page Sheila and Apprentice Tyrone
  wait 1
  %echo% ~%self% just shrugs.
  %purge% %object%
elseif (%self.vnum% == 11805 || %self.vnum% == 11905) && %object.vnum% == 11872
  * Mageina
  return 0
  %send% %actor% You try to give @%object% to ~%self% but she cackles and hands it back.
  %echoaround% %actor% ~%actor% tries to give something to ~%self% but she cackles and hands it back.
  wait 1
  say I've got lots of these, you can keep it.
elseif %self.vnum% == 11808 && %object.vnum% == 11872
  * Unrequited young lady
  wait 1
  %echo% ~%self% doesn't even look at the note.
  %purge% %object%
elseif %self.vnum% == 11920 && %object.vnum% == 11872
  * GHSs
  return 0
  %send% %actor% You pass @%object% to ~%self% and she glances at it, but hands it back.
  %echoaround% %actor% ~%actor% gives something to ~%self% and she glances at it, but hands it back.
  wait 1
  say Wow, you too?
end
~
#11808
Skycleave: Mob gains no-attack on load~
0 nA 100
~
* turn on no-attack (until diff-sel)
dg_affect %self% !ATTACK on -1
~
#11809
Skycleave: Single mob difficulty selector~
0 c 0
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
%restore% %mob%
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
* special handling: Elver the Worthy
if %self.vnum% == 11956
  wait 0
  %echo% &&j~%self% cuts his palm with a small jade knife and spatters blood across the sand.
  %load% obj 11972
end
~
#11810
Skycleave: Message when no-attack mob is attacked~
0 B 0
~
if %self.aff_flagged(!ATTACK)%
  if %self.has_trigger(11809)%
    %send% %actor% You need to choose a difficulty before you can fight ~%self%.
    %send% %actor% Usage: difficulty <normal \| hard \| group \| boss>
    %echoaround% %actor% ~%actor% considers attacking ~%self%.
  else
    * no diff-sel: base it on vnum
    switch %self.vnum%
      case 11849
        * Trixton Vye
        set bleak %instance.mob(11848)%
        set kara
        if %instance.mob(11847)%
          %send% %actor% When you approach Trixton Vye, you're pushed back by a cold force... Something -- or someone -- is protecting him.
        elseif %instance.mob(11848)%
          %send% %actor% Trixton Vye ignores all attempts to harm him as he wounds instantly seal themselves. The magic comes from elsewhere; something -- or someone -- is protecting him.
        else
          * nobody protecting him?
          dg_affect %self% !ATTACK off
          return 1
          halt
        end
      break
      default
        %send% %actor% This is a peaceful area.
      break
    done
  end
  return 0
else
  * no need for this script anymore
  detach 11810 %self.id%
  return 1
end
~
#11811
Skycleave: Directory look description~
1 c 4
look examine~
return 0
if %actor.obj_target(%arg%)% != %self%
  halt
end
set spirit %instance.mob(11900)%
* detect location
set template %self.room.template%
* base text
%mod% %self% lookdesc The directory is a standing slab of rough-cut stone with one smooth face engraved with information for each floor of the Tower Skycleave.
%mod% %self% append-lookdesc-noformat &0
%mod% %self% append-lookdesc-noformat &0      +-------------------------------------------------------+
%mod% %self% append-lookdesc-noformat &0      |                                                       |`+
if %template% == 11801 || %template% == 11901
  %mod% %self% append-lookdesc-noformat &0      | ============ The Tower Skycleave: Lobby ============= | |
elseif %template% == 11810 || %template% == 11910
  %mod% %self% append-lookdesc-noformat &0      | ========= The Tower Skycleave: Second Floor ========= | |
elseif %template% == 11830 || %template% == 11930
  %mod% %self% append-lookdesc-noformat &0      | ========= The Tower Skycleave: Third Floor ========== | |
elseif %template% == 11860 || %template% == 11960
  %mod% %self% append-lookdesc-noformat &0      | ========= The Tower Skycleave: Fourth Floor ========= | |
end
set blank &0      |                                                       | |
* floor 1
%mod% %self% append-lookdesc-noformat %blank%
if %spirit.phase1%
  %mod% %self% append-lookdesc-noformat &0      |              First Floor: Welcome Center              | |
  %mod% %self% append-lookdesc-noformat &0      |                    Cafe Fendreciel                    | |
  %mod% %self% append-lookdesc-noformat &0      |                      Study Hall                       | |
  %mod% %self% append-lookdesc-noformat &0      |               Towerwinkel's Gift Shoppe               | |
else
  %mod% %self% append-lookdesc-noformat &0      |               First Floor: Refuge Area                | |
  %mod% %self% append-lookdesc-noformat &0      |                       Mess Hall                       | |
  %mod% %self% append-lookdesc-noformat &0      |                       Bunkroom                        | |
  %mod% %self% append-lookdesc-noformat &0      |                      Supply Room                      | |
end
* floor 2
%mod% %self% append-lookdesc-noformat %blank%
if %spirit.phase2%
  %mod% %self% append-lookdesc-noformat &0      |            Second Floor: Apprentice Level             | |
  %mod% %self% append-lookdesc-noformat &0      |                     Goblin Cages                      | |
  %mod% %self% append-lookdesc-noformat &0      |                      Pixy Races                       | |
  %mod% %self% append-lookdesc-noformat &0      |                Apprentice Dormatories                 | |
else
  %mod% %self% append-lookdesc-noformat &0      |                 Second Floor: Danger!                 | |
  %mod% %self% append-lookdesc-noformat &0      |                  Goblins Everywhere                   | |
  %mod% %self% append-lookdesc-noformat &0      |                       Pixy Maze                       | |
end
* floor 3
%mod% %self% append-lookdesc-noformat %blank%
if %spirit.phase3%
  %mod% %self% append-lookdesc-noformat &0      |             Third Floor: Residents' Labs              | |
  %mod% %self% append-lookdesc-noformat &0      |            Enchanting and Attunement Labs             | |
  if %spirit.lab_open%
    %mod% %self% append-lookdesc-noformat &0      |                   Magichanical Labs                   | |
  end
  %mod% %self% append-lookdesc-noformat &0      |                      Eruditorium                      | |
  %mod% %self% append-lookdesc-noformat &0      |                       Lich Labs                       | |
else
  %mod% %self% append-lookdesc-noformat &0      |                 Third Floor: Danger!                  | |
  %mod% %self% append-lookdesc-noformat &0      |                 Mercenaries Detected                  | |
  if %instance.mob(11829)%
    %mod% %self% append-lookdesc-noformat &0      |              Loose Otherworlder Detected              | |
  end
  if %spirit.lich_released% > 0
    %mod% %self% append-lookdesc-noformat &0      |                  Loose Lich Detected                  | |
  end
end
* floor 4
%mod% %self% append-lookdesc-noformat %blank%
if %spirit.phase4%
  %mod% %self% append-lookdesc-noformat &0      |       Fourth Floor: High Sorcerers of Skycleave       | |
  if %instance.mob(11968)%
    %mod% %self% append-lookdesc-noformat &0      |             Office of High Sorcerer Celiya            | |
    %mod% %self% append-lookdesc-noformat &0      |            Office of High Sorcerer Barrosh            | |
    %mod% %self% append-lookdesc-noformat &0      |          Office of Grand High Sorcerer Knezz          | |
  else
    if %instance.mob(11970)%
      %mod% %self% append-lookdesc-noformat &0      |             Office of High Sorcerer Niamh             | |
    else
      %mod% %self% append-lookdesc-noformat &0      |                     Empty Office                      | |
    end
    %mod% %self% append-lookdesc-noformat &0      |            Office of High Sorcerer Barrosh            | |
    %mod% %self% append-lookdesc-noformat &0      |          Office of Grand High Sorcerer Celiya         | |
  end
else
  %mod% %self% append-lookdesc-noformat &0      |                 Fourth Floor: Danger!                 | |
  %mod% %self% append-lookdesc-noformat &0      |             Office of High Sorcerer Celiya            | |
  if %instance.mob(11867)% || !%spirit.diff4%
    %mod% %self% append-lookdesc-noformat &0      |        Mind-Controlled High Sorcerer - WARNING        | |
  elseif %instance.mob(11861)%
    %mod% %self% append-lookdesc-noformat &0      |            Office of High Sorcerer Barrosh            | |
  end
  if %instance.mob(11863)%
    %mod% %self% append-lookdesc-noformat &0      |       Office of the Shadow Ascendant - WARNING        | |
  elseif %instance.mob(11869)% || !%spirit.diff4%
    %mod% %self% append-lookdesc-noformat &0      |     Office of Grand High Sorcerer Knezz - WARNING     | |
  elseif %instance.mob(11870)%
    %mod% %self% append-lookdesc-noformat &0      |          Office of Grand High Sorcerer Knezz          | |
  else
    %mod% %self% append-lookdesc-noformat &0      |                     Empty Office                      | |
  end
end
* footer
%mod% %self% append-lookdesc-noformat %blank%
%mod% %self% append-lookdesc-noformat %blank%
if %spirit.phase1%
  %mod% %self% append-lookdesc-noformat &0      |     Have a pleasant visit to the Tower Skycleave      | |
else
  %mod% %self% append-lookdesc-noformat &0      | Have a safe and peaceful visit to the Tower Skycleave | |
end
%mod% %self% append-lookdesc-noformat &0      |                                                       |,+
%mod% %self% append-lookdesc-noformat &0      +-------------------------------------------------------+
~
#11812
Gemstone flute: Play to activate dancing~
1 c 1
play~
return 0
wait 1
if %self.worn_by% != %actor%
  halt
end
if %actor.action% != playing
  halt
end
if !%self.has_trigger(11998)%
  * short pause to ensure it plays a music message
  wait 6 s
  attach 11998 %self.id%
end
~
#11813
Skycleave: Shared enter trigger~
2 gwA 100
~
* based on room
if %room.template% == 11908
  * fountain 1B: react with awe
  if %direction% == down && %actor.is_pc%
    set awe_vnums 11904 11800 11802 11803 11808 11809 11822 11823 11824 11826
    set ch %room.people%
    set done 0
    while %ch% && !%done%
      if %ch.is_npc% && %ch.can_see(%actor%)% && %awe_vnums% ~= %ch.vnum%
        wait 1
        switch %random.4%
          case 1
            %force% %ch% say How did you get in the fountain?
          break
          case 2
            %force% %ch% say Did you just come out of the fountain?
          break
          case 3
            %send% %actor% ~%ch% jumps as you rise out of the fountain.
            %echoaround% %actor% ~%ch% jumps as ~%actor% rises out of the fountain.
          break
          case 4
            %send% %actor% ~%ch% stands there, mouth agape, as you rise from the fountain.
            %echoaround% %actor% ~%ch% stands there, mouth agape, as ~%actor% rises from the fountain.
          break
        done
        set done 1
      end
      set ch %ch.next_in_room%
    done
    * prevent commenting on everyone following
    wait 1
  end
else
  * All other rooms: ensure they're on the first progress goal
  set emp %actor.empire%
  if %emp%
    if !%emp.is_on_progress(11800)% && !%emp.has_progress(11800)%
      nop %emp.start_progress(11800)%
    end
  end
end
~
#11814
Skycleave: Loot controller~
1 n 100
~
* Handles loot drops for skycleave bosses
* determine where we are
set actor %self.carried_by%
if %self.level%
  set level %self.level%
else
  set level 100
end
* determine 1 item of loot
if %self.vnum% == 11837
  * BoE item list
  set roll %random.100%
  if %roll% == 1
    set loot_list 11916
  elseif %roll% <= 16
    set loot_list 11801 11802 11804 11806 11807
  else
    set loot_list 11810 11811 11812 11813 11814 11815 11816 11817 11818 11819 11820 11821 11822 11823 11824 11825 11826 11827 11828 11829
  end
  * count list
  set temp %loot_list%
  set count 0
  while !%temp.empty%
    set temp %temp.cdr%
    eval count %count% + 1
  done
  * roll
  eval pos %%random.%count%%%
  while %pos% > 0 && !%loot_list.empty%
    set loot %loot_list.car%
    set loot_list %loot_list.cdr%
    eval pos %pos% - 1
  done
elseif %self.vnum% == 11838
  * BoP item list 11840-11859
  eval loot 11840 - 1 + %random.20%
end
* load loot
if %loot%
  %load% obj %loot% %actor% inv %level%
  set item %actor.inventory%
  if %item.vnum% == %loot%
    if %item.is_flagged(BOE)% || %item.is_flagged(BOP)%
      %item.bind(%self%)%
    end
  end
end
* any bonus items?
*   no
* done
wait 0
%purge% %self%
~
#11815
Escaped Goblin combat: Throw Object, Pocket Sand~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2
  set num_left 2
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
* perform move
skyfight lockout 25 20
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Throw Object
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  set targ %random.enemy%
  if !%targ%
    set targ %actor%
  end
  set id %targ.id%
  * random obj
  set object_1 a mana deconfabulator
  set object_2 an ether condenser
  set object_3 an enchanted knickknack
  set object_4 a dynamic reconfabulator
  eval obj %%object_%random.4%%%
  * start
  %subecho% %room% &&y~%self% shouts, 'Goose!'&&0
  %send% %targ% &&m**** &&Z~%self% grabs %obj% off the floor and throws it at you! ****&&0 (dodge)
  %echoaround% %targ% &&m~%self% grabs %obj% off the floor and throws it at ~%targ%!&&0
  skyfight setup dodge %targ%
  wait 5 s
  nop %self.remove_mob_flag(NO-ATTACK)%
  wait 3 s
  if %self.disabled%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
    %echo% &&m%obj.cap% smashes into the far wall.&&0
    unset targ
  elseif %targ.var(did_sfdodge)%
    * dodged: switch targ
    %echo% &&m%obj.cap% bounces off the wall and rebounds toward ~%self%!&&0
    set targ %self%
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
  end
  * hit either them or me
  if %targ%
    * hit
    switch %random.3%
      case 1
        %echo% &&m%obj% bonks ~%targ% in the head!&&0
        eval ouch 75 * %diff%
        %damage% %targ% %ouch% physical
        if %diff% == 4 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
          dg_affect #11851 %targ% STUNNED on 5
        end
      break
      case 2
        %echo% &&m%obj% explodes in |%targ% face, blinding *%targ%!&&0
        eval ouch 50 * %diff%
        %damage% %targ% %ouch% magical
        dg_affect #11841 %targ% BLIND on 5
      break
      case 3
        if %targ.trigger_counterspell%
          %send% %targ% &&mYour counterspell prevents %obj% from draining your energy.&&0
          %echo% &&m%obj% bounces harmlessly off ~%targ%.&&0
        else
          %send% %targ% &&mYou feel %obj% drain your energy as it bounces off you!&&0
          %echoaround% %targ% &&m~%targ% looks tired as %obj% bounces off *%targ%.&&0
          eval ouch 50 * %diff%
          %damage% %targ% %ouch% magical
          eval mag %self.level% * %diff% / 10
          nop %targ.mana(-%mag%)%
          nop %targ.move(-%mag%)%
        end
      break
    done
  end
  skyfight clear dodge
elseif %move% == 2 && !%self.aff_flagged(BLIND)%
  * Pocket Sand
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %subecho% %room% &&y~%self% shouts, 'Pocket sand!'&&0
  %send% %targ% &&m**** &&Z~%self% sticks ^%self% hand into ^%self% pocket... ****&&0 (dodge)
  %echoaround% %targ% &&m~%self% sticks ^%self% hand into ^%self% pocket...&&0
  skyfight setup dodge %targ%
  wait 8 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
    %echo% &&m~%self% just laughs and throws sand into the air.&&0
  elseif %targ.var(did_sfdodge)%
    * miss
    %echo% &&m~%self% whips a fistful of sand out of ^%self% pocket and throws it toward ~%targ%... but it blows back in ^%self% face.&&0
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
    dg_affect #11841 %self% BLIND on 5
  else
    * hit
    %echo% &&m~%self% whips a fistful of sand from ^%self% pocket into |%targ% eyes!&&0
    dg_affect #11841 %targ% BLIND on 10
    eval ouch 5 * %diff%
    %damage% %targ% %ouch% physical
  end
  skyfight clear dodge
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11816
Goblin Commando combat: Goblinball, Slay the Griffin~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2
  set num_left 2
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
* perform move
skyfight lockout 25 20
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Goblinball / Ankle Stab
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  eval dodge %diff% * 20
  dg_affect #11810 %self% DODGE %dodge% 10
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %subecho% %room% &&y~%self% shouts, 'Goblinbaaaaaaaaaall!'&&0
  %send% %targ% &&m**** &&Z~%self% ducks and rolls toward you! ****&&0 (dodge)
  %echoaround% %targ% &&m~%self% ducks and rolls toward ~%targ%!&&0
  skyfight setup dodge %targ%
  wait 8 s
  dg_affect #11810 %self% off
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
    %echo% &&m~%self% quickly gets back up.&&0
  elseif %targ.var(did_sfdodge)%
    * miss
    %echo% &&m~%self% goes crashing into the wall as &%self% misses ~%targ%!&&0
    if %diff% < 3
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
      %damage% %self% 10 physical
    end
  else
    * hit
    %send% %targ% &&m~%self% stabs ^%self% knife into |%targ% ankle as &%self% pops up behind *%targ%!&&0
    if %diff% >= 3
      %send% %targ% &&mYour ankle wound slows your movement!&&0
      dg_affect #11816 %targ% SLOW on 15
    end
    eval dam 10 + (%diff% * 40)
    %damage% %targ% %dam% physical
    %dot% #11816 %targ% %dam% 15 physical
  end
  skyfight clear dodge
elseif %move% == 2
  * Slay the Griffin / Fan of Knives
  if %diff% < 3
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  %subecho% %room% &&y~%self% shouts, 'Slay the Griffin!'&&0
  %echo% &&m**** &&Z~%self% starts pulling out knives and throwing them in all directions! ****&&0 (dodge)
  skyfight setup dodge all
  set any 0
  set cycle 0
  eval max (%diff% + 2) / 2
  while %cycle% < %max%
    wait 5 s
    if %self.disabled%
      nop %self.remove_mob_flag(NO-ATTACK)%
      halt
    end
    set this 0
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if %ch.var(did_sfdodge)%
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        else
          set any 1
          set this 1
          %echo% &&m|%self% knife slices through the air and straight into |%ch% chest!&&0
          eval dam %diff% * 10
          eval ouch %diff% * 30
          %dot% #11811 %ch% %ouch% 10 physical 3
          %damage% %ch% %dam% physical
        end
      end
      set ch %next_ch%
    done
    if !%this%
      %echo% &&mKnives hit the wall with a dull thud.&&0
    end
    eval cycle %cycle% + 1
  done
  if !%any%
    * full miss
    %echo% &&m~%self% looks dizzy as &%self% throws the last knife.&&0
    dg_affect #11852 %self% HARD-STUNNED on 5
  end
  skyfight clear dodge
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11817
Goblin Miner combat: Leaping Strike and All Mine~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2
  set num_left 2
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
* perform move
skyfight lockout 25 20
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Leaping Strike
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %subecho% %room% &&y~%self% shouts, 'You got a miner problem!'&&0
  %send% %targ% &&m**** &&Z~%self% leaps high into the air with ^%self% pick over ^%self% head! ****&&0 (dodge)
  %echoaround% %targ% &&m~%self% leaps high into the air with ^%self% pick over ^%self% head!&&0
  skyfight setup dodge %targ%
  wait 8 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
    %echo% &&m~%self% lands hard with ^%self% pick embedded deep in the floor!&&0
  elseif %targ.var(did_sfdodge)%
    * miss
    %echo% &&m~%self% goes crashing into the wall as &%self% misses ^%self% leaping strike!&&0
    if %diff% < 3
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
      %damage% %self% 10 physical
    end
  else
    * hit
    %send% %targ% &&m~%self% knocks ~%targ% to the ground as &%self% lands with ^%self% pick in |%targ% chest!&&0
    eval dam 80 + (%diff% * 30)
    %damage% %targ% %dam% physical
  end
  skyfight clear dodge
elseif %move% == 2
  * All Mine
  if %diff% < 3
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  %subecho% %room% &&y~%self% shouts, 'Mine! Allllllll miiiiiiiiiiiiine!'&&0
  %echo% &&m**** &&Z~%self% holds out ^%self% pick and begins spinning wildly! ****&&0 (dodge)
  skyfight setup dodge all
  set any 0
  set cycle 0
  eval max (%diff% + 2) / 2
  while %cycle% < %max%
    wait 5 s
    if %self.disabled%
      nop %self.remove_mob_flag(NO-ATTACK)%
      halt
    end
    set this 0
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if %ch.var(did_sfdodge)%
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        else
          set any 1
          set this 1
          %echo% &&m|%self% wild spin strikes ~%ch% hard!&&0
          eval dam %diff% 40 + (20 * %diff%)
          %damage% %ch% %dam% physical
        end
      end
      set ch %next_ch%
    done
    if !%this%
      %echo% &&m~%self% spins wildly around the room!&&0
    end
    eval cycle %cycle% + 1
  done
  if !%any%
    * full miss
    %echo% &&m~%self% whirls around for a second and spins *%self%self out.&&0
    dg_affect #11852 %self% HARD-STUNNED on 5
  end
  skyfight clear dodge
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11818
Venjer the Fox combat: Pixycraft Embiggening Elixir, Blinding Barrage, Tower Quake, Blastmaster Fox, Summon~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4 5
  set num_left 5
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
* perform move
skyfight lockout 30 35
if %move% == 1
  * Pixycraft Embiggening Elixir
  skyfight clear interrupt
  %echo% &&m**** &&Z~%self% takes a pixycraft embiggening elixir from her belt... ****&&0 (interrupt)
  skyfight setup interrupt all
  wait 4 s
  if %self.disabled%
    halt
  end
  if %self.var(sfinterrupt_count,0)% < (%diff% + 1) / 2
    %echo% &&m**** &&Z~%self% uncorks the pixycraft embiggening elixir and licks her lips... ****&&0 (interrupt)
  end
  wait 4 s
  if %self.disabled%
    halt
  end
  if %self.var(sfinterrupt_count,0)% >= (%diff% + 1) / 2
    %echo% &&mVenjer is interrupted and drops the elixir, which smashes on the ground!&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  else
    %echo% &&mVenjer quaffs the pixycraft embiggening elixir in one go... and doubles in size!&&0
    eval amount %diff% * 15
    dg_affect #11817 %self% BONUS-MAGICAL %amount% 30
  end
  skyfight clear interrupt
elseif %move% == 2
  * Blinding Barrage
  skyfight clear dodge
  %echo% &&mVenjer steps back and begins shaking her staff, making an ominous rattle...&&0
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 3 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %subecho% %room% &&y~%self% shouts, 'Blinding Barrage!'&&0
  %echo% &&m**** There's a loud POP and a BANG as the air begins to explode around you! ****&&0 (dodge)
  set cycle 1
  set broke 0
  eval wait 10 - %diff%
  while %cycle% <= %diff% && !%broke%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          if %ch.trigger_counterspell%
            %echo% &&mThe blinding barrage blows back in Venjer's face as it hits |%ch% counterspell!&&0
            dg_affect #11823 %self% BLIND on 10
            set broke 1
          else
            %echo% &&mThere's a blinding flash as the air explodes right next to ~%ch%!&&0
            if %cycle% == %diff% && %diff% >= 2
              dg_affect #11823 %ch% BLIND on 10
            end
            %damage% %ch% 150 magical
          end
        elseif %ch.is_pc%
          %send% %ch% &&mYou cover your eyes as you dodge out of the way of the blinding barrage!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff% && !%broke%
          %send% %ch% &&m**** Here comes another one... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  if %broke%
    if %diff% < 3
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
    %damage% %self% 100 magical
  end
  wait 8 s
elseif %move% == 3
  * Tower Quake
  skyfight clear dodge
  %echo% &&mVenjer raises her staff high in the air and slams it into the floor!&&0
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %echo% &&mA large glowing sigil spreads across the floor from the impact of Venjer's staff...&&0
  wait 3 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  skyfight setup dodge all
  %subecho% %room% &&y~%self% shouts, 'TOWER QUAKE!'&&0
  %echo% &&m**** There's a low rumble as the tower begins to shake... ****&&0 (dodge)
  set cycle 1
  eval wait 10 - %diff%
  eval debuff %self.level% / 8
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    %subecho% %room% The Tower Skycleave quakes on its foundation!
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          %echo% &&mThe shaking floor knocks ~%ch% to the floor!&&0
          if %cycle% == %diff% && %diff% >= 3 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
            dg_affect #11814 %ch% STUNNED on 5
          end
          dg_affect #11818 %ch% TO-HIT -%debuff% 15
          dg_affect #11818 %ch% DODGE -%debuff% 15
          %damage% %ch% 100 physical
        elseif %ch.is_pc%
          %send% %ch% &&mYou jump at just the right time and avoid the tower quake!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&m**** Here comes another quake... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  wait 8 s
elseif %move% == 4 && !%self.aff_flagged(BLIND)%
  * Blastmaster Fox
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear interrupt
  set targ %self.fighting%
  set id %targ.id%
  %send% %targ% &&m**** &&Z~%self% spins her staff in a circle, and a ball of light starts to grow at its tip... ****&&0 (interrupt)
  %echoaround% %targ% &&m~%self% spins her staff in a circle, and a ball of light starts to grow at its tip...&&0
  skyfight setup interrupt %targ%
  set cycle 1
  set broke 0
  while %cycle% <= %diff% && !%broke%
    wait 4 s
    if %self.disabled%
      nop %self.remove_mob_flag(NO-ATTACK)%
      halt
    end
    %subecho% %room% &&y~%self% shouts, 'Blastmaster Fox!'&&0
    if !%targ% || %targ.id% != %id%
      * gone
      %echo% &&mLuminous bolts from Venjer's staff crash all over the room and explode!&&0
    elseif %targ.var(did_sfinterrupt)%
      %send% %targ% &&mYou knock Venjer's staff sideways and luminous bolts fly every which way, crashing all around you, but they all miss!&&0
      %echoaround% %targ% &&m~%targ% knocks Venjer's staff sideways and luminous bolts fly every which way, crashing around ~%targ%, but they all miss!&&0
    elseif %targ.trigger_counterspell%
      %echo% &&mVenjer's luminous bolt explodes against |%targ% counterspell!&&0
      set broke 1
    else
      * hit
      %send% %targ% &&mLuminous bolts from Venjer's staff crash into ~%targ% and explode!&&0
      dg_affect #11841 %targ% BLIND on 10
      %damage% %targ% 100 magical
    end
    if %diff% > 1
      %echo% &&mYou're caught in the explosion!&&0
      %aoe% 25 magical
    end
    skyfight clear interrupt
    if %cycle% < %diff% && %targ% && %targ.id% == %id% && !%broke%
      %send% %targ% &&m**** Here comes another blast... ****&&0 (interrupt)
      skyfight setup interrupt %targ%
    end
    eval cycle %cycle% + 1
  done
elseif %move% == 5
  * Summon Goblins
  %subecho% %room% &&y~%self% shouts, 'Gaaaaableeeeeeeeeeens!'&&0
  wait 4 s
  if %diff% > 2
    %load% m 11817 ally %self.level%
    set gob %room.people%
    if %gob.vnum% == 11817
      nop %gob.add_mob_flag(!LOOT)%
      set diff %diff%
      remote diff %gob.id%
      %echo% &&m~%gob% comes running in!&&0
      %force% %gob% mkill %actor%
    end
  end
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11819
Pixy Queen (Skycleave) combat: Baleful Polymorph, Creeping Vines, Shrinking Dust, Shimmering Glare, Summon~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4 5
  set num_left 5
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
* perform move
skyfight lockout 30 35
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Baleful Polymorph
  skyfight clear dodge
  set targ %random.enemy%
  if !%targ%
    set targ %actor%
  end
  if %diff% == 1
    set vnum 11826
  else
    set vnum 11827
  end
  if %targ.morph% == %vnum%
    * already morphed
    nop %self.set_cooldown(11800,0)%
    halt
  end
  * wait?
  set targ_id %targ.id%
  set fail 0
  if %diff% <= 2
    %send% %targ% &&m**** &&Z~%self% focuses a rainbow beam toward you... ****&&0 (dodge)
    %echoaround% %targ% &&m~%self% focuses a rainbow beam toward ~%targ%...&&0
    skyfight setup dodge %targ%
    wait 8 s
    if %self.disabled%
      halt
    end
    if !%targ% || %targ.id% != %targ_id%
      * lost targ
      set fail 1
    elseif %targ.var(did_sfdodge)%
      set fail 1
      %echo% &&mThe rainbow beam misses and scatters like a prism as it hits the vines.&&0
      if %diff% == 1
        dg_affect #11856 %targ% TO-HIT 25 20
      end
    else
      %send% %targ% &&mYou're picked up by the swirling rainbow light...&&0
      %echoaround% %targ% &&m~%targ% is picked up by the swirling rainbow light...&&0
    end
  else
    %echo% &&m~%self% blasts ~%targ% with a rainbow beam... it picks *%targ% right up off the ground...&&0
  end
  if !%fail%
    if %targ.trigger_counterspell%
      set fail 2
      %echo% &&mThe rainbow beam cascades off of |%targ% counterspell and ricochets back at ~%self%!&&0
    end
  end
  if !%fail%
    set old_shortdesc %targ.name%
    %morph% %targ% %vnum%
    %echoaround% %targ% &&m%old_shortdesc% grows long ears and a tail... and becomes ~%targ%!&&0
    %send% %targ% &&m**** You are suddenly transformed into %targ.name%! ****&&0 (fastmorph normal)
    if %diff% == 4 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
      dg_affect #11851 %targ% STUNNED on 5
    elseif %diff% >= 2
      nop %targ.command_lag(ABILITY)%
    end
  elseif %fail% == 2 || %diff% == 1
    dg_affect #11852 %self% HARD-STUNNED on 10
  end
  skyfight clear dodge
elseif %move% == 2
  * Creeping Vines
  skyfight clear struggle
  %echo% &&m~%self% shoots rainbow streamers into the foliage... which starts to stir!&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 sec
  %echo% &&m**** Creeping vines ensnare your arms and legs! ****&&0 (struggle)
  skyfight setup struggle all 20
  set ch %room.people%
  while %ch%
    set bug %ch.inventory(11890)%
    if %bug%
      set strug_char You struggle against the vines...
      set strug_room ~%%actor%% struggles against the vines...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You get a hand loose and are able to escape the vines!
      set free_room ~%%actor%% manages to free *%%actor%%self!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    set ch %ch.next_in_room%
  done
  * damage
  if %diff% > 1
    set cycle 0
    while %cycle% < 5
      wait 4 s
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %ch.affect(11822)%
          %send% %ch% &&m**** You bleed from your arms and legs as the thorny vines cut into you! ****&&0 (struggle)
          %damage% %ch% 100 magical
        end
        set ch %next_ch%
      eval cycle %cycle% + 1
    done
  else
    wait 10 s
    nop %self.remove_mob_flag(NO-ATTACK)%
    wait 10 s
  end
elseif %move% == 3
  * Shimmering Glare
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %send% %targ% &&m**** &&Z~%self% begins to shimmer as &%self% glares at you... ****&&0 (dodge)
  %echoaround% %targ% &&m~%self% begins to shimmer as &%self% glares at ~%targ%...&&0
  skyfight setup dodge %targ%
  wait 8 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
  elseif %targ.var(did_sfdodge)%
    * miss
    %echo% &&mA shimmering beam of light from ~%self% ricochets off a pair of saplings and smacks the queen in the face!&&0
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
    dg_affect #11841 %self% BLIND on 5
  elseif %targ.trigger_counterspell%
    %echo% &&mA shimmering beam of light from ~%self% ricochets off |%targ% counterspell and zaps the queen in the face!&&0
    dg_affect #11841 %self% BLIND on 10
  else
    * hit
    %echo% &&mAs ~%self% glares at ~%targ%, a shimmering beam of light blasts *%targ% right in the eyes!&&0
    dg_affect #11841 %targ% BLIND on 20
    if %diff% > 1
      %damage% %targ% 75 magical
    end
  end
  skyfight clear dodge
elseif %move% == 4
  * Shrinking Dust
  skyfight clear dodge
  %echo% &&m~%self% pulls out a tiny bag...&&0
  if %diff% == 1
    * normal: prevent her attack
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 1 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  skyfight setup dodge all
  %echo% &&m**** &&Z~%self% starts throwing handfuls of pixy dust around the room... ****&&0 (dodge)
  wait 8 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  set ch %room.people%
  set any 0
  eval penalty %self.level%/7
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)%
      if %ch.var(did_sfdodge)%
        if %diff% == 1
          dg_affect #11856 %ch% TO-HIT 25 20
        end
      else
        set any 1
        %send% %ch% &&mYou're hit by pixy dust... and you're shrinking!&&0
        %echo% &&m~%ch% is hit by pixy dust and starts to shrink!&&0
        dg_affect #11820 %ch% BONUS-PHYSICAL -%penalty% 30
        dg_affect #11821 %ch% BONUS-MAGICAL -%penalty% 30
        if %diff% == 4 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
          dg_affect #11851 %ch% STUNNED on 10
        end
        if %diff% >= 2
          dg_affect #11857 %ch% SLOW on 20
        end
      end
    end
    set ch %next_ch%
  done
  if !%any%
    %echo% &&m~%self% flies through a cloud of pixy dust and starts hacking and coughing!&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 10
    else
      eval amount 100 - (%diff% * 20)
      dg_affect #11854 %self% DODGE -%amount% 10
    end
  end
  skyfight clear dodge
elseif %move% == 5
  * Summon pixy
  %subecho% %room% &&y~%self% shouts, 'To me, my child!'&&0
  wait 2 sec
  if %diff% > 2
    %load% m 11820 ally %self.level%
    set pix %room.people%
    if %pix.vnum% == 11820
      nop %pix.add_mob_flag(!LOOT)%
      set diff %diff%
      remote diff %pix.id%
      %echo% &&mThere's a flash and a BANG! And ~%pix% comes flying out of nowhere!&&0
      %force% %pix% mkill %actor%
    end
  end
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11820
Escaped Pixy (Skycleave) combat: Pixy Trip, Dangling Vine/Weapon Steal~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2
  set num_left 2
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
* perform move
skyfight lockout 25 20
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Pixy Trip
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  set targ %random.enemy%
  if !%targ%
    set targ %actor%
  end
  set id %targ.id%
  * random form
  set form %random.2%
  if %form% == 1
    %send% %targ% &&m**** &&Z~%self% swoops toward your legs! ****&&0 (dodge)
    %echoaround% %targ% &&m~%self% swoops toward |%targ% legs!&&0
  else
    %send% %targ% &&m**** A vine creeps toward your feet! ****&&0 (dodge)
    %echoaround% %targ% &&mA vine creeps toward |%targ% feet!&&0
  end
  * start
  set miss 0
  skyfight setup dodge %targ%
  wait 5 s
  nop %self.remove_mob_flag(NO-ATTACK)%
  wait 3 s
  if %self.disabled%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
    set miss 1
  elseif %targ.var(did_sfdodge)%
    set miss 1
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
  end
  * hit either them or me
  if %miss%
    if %form% == 1
      %echo% &&m~%self% goes careening into a sapling and rebounds into a wall!&&0
    else
      %echo% &&mThe vine misses and whips back toward the ceiling, knocking ~%self% into the wall!&&0
    end
    dg_affect #11852 %self% HARD-STUNNED on 10
  else
    * hit
    if %form% == 1
      if %diff% > 1
        %echo% &&m~%self% bites ~%targ% on the ankle!&&0
        eval dam 60 + (%diff% * 40)
        %damage% %targ% %dam% physical
      else
        %echo% &&m~%self% dives toward |%targ% legs!&&0
      end
    else
      %echo% &&mA vine wraps itself around |%targ% ankle and tugs hard!&&0
    end
    %send% %targ% &&mYou trip and fall!&&0
    %echoaround% %targ% &&m~%targ% trips and falls!&&0
    if %diff% > 2
      if (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
        dg_affect #11814 %targ% STUNNED on 10
      end
      eval dam 40 + (%diff% * 40)
      %damage% %targ% %dam% physical
    elseif %diff% > 1
      dg_affect #11814 %targ% IMMOBILIZED on 20
      %damage% %targ% 100 physical
    else
      dg_affect #11814 %targ% IMMOBILIZED on 20
    end
  end
  skyfight clear dodge
elseif %move% == 2
  * Dangling Vine/Weapon Steal
  skyfight clear dodge
  set targ %self.fighting%
  if !%targ.eq(wield)%
    set targ %random.enemy%
    if !%targ.eq(wield)%
      halt
    end
  end
  set id %targ.id%
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  %send% %targ% &&m**** A dangling vine reaches down toward you... ****&&0 (dodge)
  %echoaround% %targ% &&mA dangling vine reaches down toward ~%targ%...&&0
  skyfight setup dodge %targ%
  wait 8 s
  set miss 0
  if !%targ% || %targ.id% != %id%
    * gone
    %echo% &&m~%self% isn't paying attention and runs straight into the dangling vine!&&0
    set miss 1
  elseif %targ.var(did_sfdodge)%
    * miss
    %echo% &&mThe dangling vine whips up suddenly, but hits ~%self% by mistake!&&0
    set miss 1
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
  else
    * hit
    switch %random.3%
      case 1
        %echo% &&mThe dangling vine whips up suddenly and snatches |%targ% weapon!&&0
      break
      case 2
        %send% %targ% While you're distracted by the dangling vine, ~%self% circles around you and grabs your weapon!&&0
        %echoaround% %targ% While ~%targ% is distracted by the dangling vine, ~%self% circles around and grabs ^%targ% weapon!&&0
      break
      case 3
        %send% %targ% &&mThe dangling vine taps you on the shoulder and when you turn, a second vine steals your weapon!&&0
        %echoaround% %targ% &&mThe dangling vine taps ~%targ% on the shoulder and when &%targ% turns, a second vine steals ^%targ% weapon!&&0
      break
    done
    dg_affect #11815 %targ% DISARMED on 20
    if %diff% >= 3
      %send% %targ% &&m... and worse, ~%self% stabs you with a tiny knife while you're distracted!&&0
      %echoaround% %targ% &&m... and ~%self% stabs *%targ% with a tiny knife while *%targ%'s distracted!&&0
      %damage% %targ% 200 physical
    end
  end
  if %miss%
    * already messaged
    if %diff% == 1 && %targ% && %targ.id% == %id%
      dg_affect #11856 %targ% TO-HIT 25 20
    end
    if %diff% < 3
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  skyfight clear dodge
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11821
Skycleave: Setup dodge, interrupt, struggle, free~
0 c 0
skyfight~
* Also requires trigger 11822
* To initialize or clear data:
*    skyfight clear <all | dodge | interrupt | struggle | free>
* To set up players for a response command:
*    skyfight setup <dodge | interrupt | struggle | free> <all | player>
* To ensure nobody else is also acting:
*    skyfight lockout <my cooldown> <everyone else's cooldown>
if %actor% != %self%
  return 0
  halt
end
return 1
set mode %arg.car%
set arg %arg.cdr%
if %mode% == clear
  * Clear data
  * usage: skyfight clear <all | dodge | interrupt | struggle | free>
  set ch %self.room.people%
  while %ch%
    if %arg% == dodge || %arg% == all
      rdelete did_sfdodge %ch.id%
      rdelete needs_sfdodge %ch.id%
    end
    if %arg% == interrupt || %arg% == all
      rdelete did_sfinterrupt %ch.id%
      rdelete needs_sfinterrupt %ch.id%
    end
    if %arg% == struggle || %arg% == all
      dg_affect #11822 %ch% off
      rdelete did_sfstruggle %ch.id%
      rdelete needs_sfstruggle %ch.id%
    end
    if %arg% == free || %arg% == all
      dg_affect #11888 %ch% off
      dg_affect #11861 %ch% off
      dg_affect #11863 %ch% off
      dg_affect #11949 %ch% off
      rdelete did_sffree %ch.id%
      rdelete needs_sffree %ch.id%
    end
    set ch %ch.next_in_room%
  done
  if %arg% == dodge || %arg% == all
    set sfdodge_count 0
    set wants_sfdodge 0
    remote sfdodge_count %self.id%
    remote wants_sfdodge %self.id%
  end
  if %arg% == interrupt || %arg% == all
    set sfinterrupt_count 0
    set wants_sfinterrupt 0
    remote sfinterrupt_count %self.id%
    remote wants_sfinterrupt %self.id%
  end
  if %arg% == struggle || %arg% == all
    set sfstruggle_count 0
    set wants_sfstruggle 0
    remote sfstruggle_count %self.id%
    remote wants_sfstruggle %self.id%
  end
  if %arg% == free || %arg% == all
    set sffree_count 0
    set wants_sffree 0
    remote sffree_count %self.id%
    remote wants_sffree %self.id%
  end
elseif %mode% == setup
  * Prepare for a response
  * usage: skyfight setup <dodge | interrupt | struggle | free> <all | player>
  set diff %self.var(diff,1)%
  set type %arg.car%
  set arg %arg.cdr%
  set target %arg.car%
  set value %arg.cdr%
  * self vars
  set wants_varname wants_sf%type%
  set %wants_varname% 1
  remote %wants_varname% %self.id%
  * target vars
  set needs_varname needs_sf%type%
  set did_varname did_sf%type%
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
        dg_affect #11822 %ch% HARD-STUNNED on %value%
        %load% obj 11890 %ch% inv
        set obj %ch.inventory(11890)%
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
  * usage: skyfight lockout <my cooldown> <everyone else's cooldown>
  set my_cd %arg.car%
  set them_cd %arg.cdr%
  set ch %self.room.people%
  while %ch%
    if %ch% == %self%
      if %my_cd%
        nop %self.set_cooldown(11800,%my_cd%)%
      end
    elseif %them_cd% && %ch.is_npc% && %ch.has_trigger(11821)%
      nop %ch.set_cooldown(11800,%them_cd%)%
    end
    set ch %ch.next_in_room%
  done
end
~
#11822
Skycleave: Dodge, Interrupt, Free commands for fights~
0 c 0
dodge interrupt free~
* Also requires trigger 11821
* handles dodge, interrupt, free
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
elseif free /= %cmd%
  set type free
  set past freed
  * target?
  set targ %actor.char_target(%arg.car%)%
  if !%targ%
    %send% %actor% Free whom?
    halt
  elseif %targ% == %actor%
    %send% %actor% You can't free yourself.
    halt
  elseif %targ.vnum% == 11834
    * special handling
    return 0
    halt
  elseif !%targ.var(needs_sffree,0)% || %targ.var(did_sffree,0)%
    %send% %actor% &%targ% doesn't need to be freed.
    halt
  end
else
  return 0
  halt
end
* check things that prevent it
if %actor.var(did_sf%type%,0)% && %type% != free
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
if %actor.affect(11812)%
  %send% %actor% You're still recovering from that last dodge.
  halt
elseif %actor.affect(11813)%
  %send% %actor% You're still distracted from that last interrupt.
  halt
end
* setup
set no_need 0
* does the actor even need it
if !%actor.var(needs_sf%type%,0)% && %type% != free
  set no_need 1
elseif !%self.var(wants_sf%type%,0)% && %type% != free
  * see if this mob needs it, or see if someone else here does
  * not me...
  set ch %self.room.people%
  set any 0
  while %ch% && !%any%
    if %ch.var(wants_sf%type%,0)%
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
  rdelete needs_sf%type% %actor.id%
  eval penalty %self.level% * %self.var(diff,1)% / 20
  * messaging
  if %type% == dodge
    %send% %actor% You dodge out of the way... of nothing!
    %echoaround% %actor% ~%actor% leaps out of the way of nothing in particular.
    dg_affect #11812 %actor% DODGE -%penalty% 20
  elseif %type% == interrupt
    %send% %actor% You look for something to interrupt...
    %echoaround% %actor% ~%actor% looks around for something...
    dg_affect #11813 %actor% DODGE -%penalty% 20
  end
  halt
end
* success
nop %actor.command_lag(COMBAT-ABILITY)%
set did_sf%type% 1
if %type% == free
  remote did_sf%type% %targ.id%
else
  remote did_sf%type% %actor.id%
end
eval sf%type%_count %self.var(sf%type%_count,0)% + 1
remote sf%type%_count %self.id%
if %type% == dodge
  if %self.room.template% == 11972
    %send% %actor% You swim out of the way!
    %echoaround% %actor% ~%actor% swims out of the way!
  else
    %send% %actor% You leap out of the way!
    %echoaround% %actor% ~%actor% leaps out of the way!
  end
elseif %type% == interrupt
  %send% %actor% You prepare to interrupt ~%self%...
  %echoaround% %actor% ~%actor% prepares to interrupt ~%self%...
elseif %type% == free
  %send% %actor% You free ~%targ%!
  %echoaround% %actor% ~%actor% frees ~%targ%!
  dg_affect #11888 %targ% off
  dg_affect #11861 %targ% off
  dg_affect #11863 %targ% off
  dg_affect #11949 %targ% off
end
~
#11823
Skycleave: Boss Deaths: Pixy, Kara, Rojjer, Trixton, Barrosh, Shade~
0 f 100
~
switch %self.vnum%
  case 11819
    * Pixy Queen
    makeuid maze room i11825
    set shield %maze.contents(11887)%
    if %shield%
      %at% %maze% %echo% &&m%shield.shortdesc% flickers and fades!&&0
      %purge% %shield%
    end
    %echo% &&mThe pixy queen sputters as her power fades and a look of shock -- and peace -- crosses her face.&&0
    %echo% As she dies, you hear the pixy queen say, 'Thank you for this release, be it ever so brief. We shall meet... again... in time.'
    * dies with death-cry
  break
  case 11847
    * Kara Virduke
    set sanjiv %self.room.people(11835)%
    if %sanjiv%
      %echo% The camouflage around Apprentice Sanjiv fades as he collapses to his knees and vomits on the floor.
      %mod% %sanjiv% longdesc An apprentice is kneeling on the floor, heaving.
      %mod% %sanjiv% lookdesc The young apprentice is on his knees, dirtying his white kurta. His wavy brown hair is matted with sweat and he heaves as if he might vomit again.
      detach 11803 %sanjiv.id%
    end
    * check Trixton's attackability
    if !%instance.mob(11848)%
      set trixton %instance.mob(11849)%
      if %trixton% && %trixton.aff_flagged(!ATTACK)%
        %at% %trixton.room% %echo% &&m~%trixton% has lost ^%trixton% protection!&&0
        dg_affect %trixton% !ATTACK off
      end
  break
  case 11848
    * Bleak Rojjer: check Trixton's attackability
    if !%instance.mob(11847)%
      set trixton %instance.mob(11849)%
      if %trixton% && %trixton.aff_flagged(!ATTACK)%
        %at% %trixton.room% %echo% &&m~%trixton% has lost ^%trixton% protection!&&0
        dg_affect %trixton% !ATTACK off
      end
  break
  case 11849
    * Trixton Vye: complete floor
    set ch %actor%
    if %ch%
      if %ch.is_npc% && %ch.leader%
        set ch %ch.leader%
      end
      if %ch.is_pc% || %ch.vnum% == 11836
        * mark who did this
        set spirit %instance.mob(11900)%
        if %ch.vnum% == 11836 && %spirit.lich_released%
          * Scaldorran kill
          makeuid killer %spirit.lich_released%
          if %killer.id% == %spirit.lich_released%
            set finish3 %killer.real_name%
          else
            set finish3 %ch.real_name%
          end
        else
          set finish3 %ch.real_name%
        end
        remote finish3 %spirit.id%
      end
    end
    %load% mob 11897
  break
  case 11863
    * Shadow Ascendant
    makeuid hall room i11870
    set shadow %hall.contents(11863)%
    if %shadow%
      %at% %hall% %echo% &&m@%shadow% fades and dissipates.&&0
      %purge% %shadow%
    end
    %echo% &&m~%self% dissolves as the shadows are cast out, dropping something as it fades away!&&0
    * set room desc back
    %mod% %self.room% description This dimly-lit office, the highest in the tower, reeks of decay. Little light filters in through the doorway; if there are any windows here, they must
    %mod% %self.room% append-description be closed; you can't even see them through the shadows. The wooden corners of furniture peek out from the shadows, but you can see few distinct shapes.
    return 0
  break
  case 11867
    * Mind-controlled Barrosh
    set ch %self.room.people%
    while %ch%
      if %ch% != %self%
        dg_affect #11867 %ch% HARD-STUNNED on 15
      end
      set ch %ch.next_in_room%
    done
    %load% mob 11861
    return 0
  break
  case 11869
    * Shade of Mezvienne
    makeuid hall room i11870
    set shadow %hall.contents(11863)%
    if %shadow%
      %at% %hall% %echo% &&m@%shadow% fades and dissipates.&&0
      %purge% %shadow%
    end
    %echo% &&m~%self% dissolves as the shadows are cast out!&&0
    if %self.diff% >= 3
      set dropped some skystones
    else
      set dropped a skystone
    end
    %echo% Knezz coughs and sits up, dropping %dropped%.
    * clean room desc
    %mod% %self.room% description This dimly-lit office, the highest in the tower, reeks of decay. Little light filters in through the doorway; if there are any windows here, they must
    %mod% %self.room% append-description be closed; you can't even see them through the shadows. The wooden corners of furniture peek out from the shadows, but you can see few distinct shapes.
    * swap Knezzes
    set knezz %instance.mob(11868)%
    if %knezz%
      %purge% %knezz%
      %load% mob 11870
    end
    return 0
  break
done
~
#11824
Skycleave: 2A Goblin names~
0 nA 100
~
* Names the goblins sequentially - 14 max each
switch %self.vnum%
  case 11815
    * uncaged goblin
    set name_list Grimscar Crunchleaf Rattlecage Quicklock Stabfriend Lootfirst Gutcheck Rocktooth Jumpdie Shivfast Biteasy Stabsall Dirtnap Littleglory
    set sex_list  male     female     female     male      male       male      female   female    male    female   male    female   male
  break
  case 11816
    * goblin commando
    set name_list Wormer Felks  Reek Lurn Myna   Cob  Jaden Yules  Gurk Hungore Bilk Mord Axer Nelbog
    set sex_list  male   female male male female male male  female male male    male male male female
  break
  case 11817
    * goblin miner
    set name_list Vyle Grosk Brang  Fance  Byth   Slansh Fonk Rong Shyf   Myte   Vrack Rend Worze  Chum Bamf Shad   Orry   Grast Dornt Onner  Rixen  Vyler Groshk Brank  Fancee  Bythe   Slange Fonke
    set sex_list  male male  female female female male   male male female female male  male female male male female female male  male  female female male  male   female female  female  male   male
    * note: list size is used below in the room 11973 section
  break
done
* get list pos
if %self.room.template% == 11973
  set pos %random.28%
else
  set varname %self.vnum%_name
  set spirit %instance.mob(11900)%
  eval pos %spirit.var(%varname%,0)% + 1
  * store back to spirit
  set %varname% %pos%
  remote %varname% %spirit.id%
end
* determine random name and sex
while %pos% > 0 && %name_list%
  set name %name_list.car%
  set name_list %name_list.cdr%
  set sex %sex_list.car%
  set sex_list %sex_list.cdr%
  eval pos %pos% - 1
done
* ensure data
if %pos% > 0% || !%name%
  * ran out
  set name another goblin
  set sex male
end
* and apply
%mod% %self% sex %sex%
%mod% %self% keywords %name% goblin
%mod% %self% shortdesc %name%
* longdesc/lookdesc based on vnum
switch %self.vnum%
  case 11815
    * uncaged goblin
    %mod% %self% longdesc %name.cap% is resting here.
    %mod% %self% lookdesc This little goblin looks positively malnourished as %self.hisher% bony frame protrudes through %self.hisher%
    %mod% %self% append-lookdesc pale green skin. Dressed only in rags and shaking as %self.heshe% stands, %self.heshe% somehow raises %self.hisher% fists to fight.
  break
  case 11816
    * goblin commando
    %mod% %self% longdesc %name.cap% is searching around for something.
    %mod% %self% lookdesc Armed with dozens of tiny knives, this forest-green goblin has come ready for war.
  break
  case 11817
    * goblin miner
    %mod% %self% longdesc %name.cap% is swinging a pickaxe at the wall.
    %mod% %self% lookdesc All muscle and no quit, this helmeted green little goblin looks like %self.heshe% means business. With a pickaxe in one hand and a shovel in
    %mod% %self% append-lookdesc the other, %self.heshe% paces back and forth, looking for the right place to strike.
  break
done
detach 11824 %self.id%
~
#11825
Skycleave: Say 'Ala lilo' to open the secret passage from either side~
2 d 0
ala lilo~
* works from: 11817, 11822, 11917, 11922
* Check if it's already open
if (%room.template% == 11817 || %room.template% == 11917) && %room.northwest(room)%
  * already open
  detach 11825 %self.id%
  halt
elseif (%room.template% == 11822 || %room.template% == 11922) && %room.southeast(room)%
  * already open
  detach 11825 %self.id%
  halt
end
* trigger opening
wait 1
%load% mob 11924
detach 11825 %self.id%
~
#11826
Skycleave: Pixy Maze track dummy~
2 c 0
track~
if !%actor.ability(Track)% || !%arg%
  return 0
  halt
end
if %room.varexists(random_track)%
  set random_track %room.random_track%
elseif %room.template% == 11818
  set random_track west
else
  switch %random.3%
    case 1
      set random_track west
    break
    case 2
      set random_track north
    break
    case 3
      set random_track east
    break
  done
  remote random_track %room.id%
end
%send% %actor% You find a trail heading %random_track%!
~
#11827
Skycleave: Spawn tourists in 1B~
2 b 20
~
* spawns 1 from the list at random each time and stores the rest to the room
if %room.varexists(vnum_list)%
  set vnum_list %room.vnum_list%
  set list_size %room.list_size%
else
  set vnum_list 11800 11802 11803 11808 11809 11821 11822 11823 11824 11826
  set list_size 11
end
* pick a spot in the list
eval pos %%random.%list_size%%%
while %pos% > 0
  set vnum %vnum_list.car%
  set vnum_list %vnum_list.cdr%
  if %pos% != 1
    set vnum_list %vnum_list% %vnum%
  end
  eval pos %pos% - 1
done
* store back the remaining list for next time
eval list_size %list_size% - 1
remote vnum_list %room.id%
remote list_size %room.id%
* load the mob
if %vnum%
  %load% mob %vnum%
  set mob %room.people%
  if %mob.vnum% == %vnum%
    * all other mobs
    %echo% ~%mob% walks in from the foyer.
    makeuid foyer room i11800
    %at% %foyer% %echo% ~%mob% walks through the foyer and into the tower.
  end
end
* and detach if the list is empty
if !%vnum% || !%vnum_list% || %list_size% < 1
  detach 11827 %room.id%
end
~
#11828
Skycleave: Floor 1B tourist dialogue~
0 bw 20
~
set room %self.room%
wait 2 s
if %self.room% != %room%
  halt
end
set varname last_%room.template%
if %self.var(%varname%,0)% + 150 > %timestamp%
  halt
elseif %room.var(speak_time,0)% + 30 > %timestamp%
  halt
end
* by mob vnum
if %self.vnum% == 11824 || %self.vnum% == 11826
  if %room.template% == 11907
    nop %self.add_mob_flag(SENTINEL)%
    nop %self.remove_mob_flag(SILENT)%
    detach 11828 %self.id%
  end
  halt
elseif %self.vnum% == 11803
  switch %room.template%
    case 11905
      say Ah, the famous Fendreciel Cafe... They are positively known for their cocoa van.
    break
    case 11907
      say A Towerwinkel's, eh? I've been to the one in Newcastle, of course.
    break
    case 11908
      say It's a well-known fact that a demon lives in the fountain. Of course, there's no way into its realm.
    break
    default
      set line %random.4%
      if %line% == 1
        say This whole tower is built on an ancient goblin burial ground. That's why it's so cursed.
      elseif %line% == 2
        say Nobody actually knows who built the Black Tower. One of the enduring mysteries.
      elseif %line% == 3
        say Grand High Sorcerer Ness and I go way, way back. Old chums. I was the one who told him to become a wizard.
      elseif %line% == 4
        say Most of that sculpture is actually iron. They just say it's nocturnium because you can't tell the difference if it's not rusted.
      end
    break
  done
elseif %self.vnum% == 11809
  switch %room.template%
    case 11905
      say Oh wow, I've never been to a Gordon Ram's Head restaurant before!
    break
    case 11907
      say Oh! I've been to one of these before. Where are the rides at?
    break
    case 11908
      say Wow, would you look at that fountain? That must be why they call it Fountain Heights.
    break
    default
      set line %random.4%
      if %line% == 1
        say I'm really excited to be here. I heard there was boxing.
      elseif %line% == 2
        say Do you think the king will be here or is he away on diplomacy?
      elseif %line% == 3
        say I've heard so much about this tower! Do you think they still have the portal with the dracosaurs?
      elseif %line% == 4
        say Wow, wait, does that tree mean we're on the ceiling?
      end
    break
  done
elseif %self.vnum% == 11822
  switch %room.template%
    case 11902
      %echo% ~%self% pulls out a small notebook and sketches the stairs and walkway from across the chamber.
    break
    case 11903
      %echo% ~%self% leans against the wall near the door and listens non-chalantly.
    break
    case 11904
      %echo% ~%self% seems to be measuring the distance from the doorway to the stairs.
    break
    case 11905
      %echo% ~%self% sits for a few moments at a table where &%self% can see most of the central chamber through the archway.
    break
    case 11907
      %echo% ~%self% wanders around the store, checking behind the racks and under the shelves when no one else is looking.
    break
    case 11908
      %echo% ~%self% studies the fountain and makes a surreptitious note before stashing the notebook in ^%self% sleeve.
    break
  done
elseif %self.vnum% == 11827
  set djon %room.people(11828)%
  if !%djon%
    halt
  end
  eval line %self.var(line,0)% + 1
  if %line% > 3
    set line 1
  end
  remote line %self.id%
  switch %line%
    case 1
      say Can you believe Mezvienne herself attacked the tower?
      wait 9 s
      %force% %djon% say I know, right? I heard she was possessed by a time lion.
      wait 9 s
      say Please don't tell me you believe in time lions now.
      wait 2 s
      set mm %room.people(11905)%
      %force% %mm% emote coughs.
      wait 7 s
      %force% %djon% say Ok, how does a rank amateur of modest reputation...
      wait 3 s
      say Modest?
      wait 3 s
      %force% %djon% say ...of above-average reputation invade the single greatest sorcery tower in history?
      wait 9 s
      if %instance.mob(11969)%
        say Well, she defeated GHS Knezz in single combat.
      else
        say And yet she did.
      end
      wait 9 s
      %force% %djon% say Fine. Where'd you hide out?
      wait 9 s
      say Attunement lab with a gargoyle charm.
      wait 9 s
      %force% %djon% say Clever. I warded myself in Niamh's office.
      wait 9 s
      say Smart.
      wait 9 s
      %echo% ~%self% and ~%djon% sip their tea.
    break
    case 2
      say Did you see Regin with the mercs?
      wait 9 s
      %force% %djon% say No! Wait, Regin Dall?
      wait 9 s
      say Didn't you two used to date? He looked awful.
      wait 9 s
      %force% %djon% say  Barely. Honestly he doesn't even count.
      wait 9 s
      say Wasn't it for three months?
      wait 9 s
      %force% %djon% say Honestly, Marina, you remember the most trivial things.
      wait 9 s
      %force% %djon% say And honestly, I'm not counting any dalliances before I came to the tower.
      wait 9 s
      say Okay, but that's still a lot of dalliances.
      wait 9 s
      %force% %djon% say New topic! Did I or did I not see you and Tyrone studying in the bunkroom three nights ago?
      wait 9 s
      say New topic! Are you keeping your hair like that or was that an accident?
      wait 9 s
      %force% %djon% say Wow, okay, new topic. Wait, what's wrong with my hair?
      wait 9 s
      say It looks like you got hit by a rebounding flow-bee charm.
      wait 9 s
      %force% %djon% say Okay, I wasn't going to say anything, but your eyebrows are uneven.
      wait 9 s
      say At least I know how to fix it.
      wait 9 s
      %echo% ~%self% points her wand at her forehead and her eyebrows even themselves out.
      wait 9 s
      %force% %djon% say Well I like it like this.
      wait 9 s
      %echo% ~%djon% waits until the barista turns her back and then twirls his wand in his teacup, which quickly refills itself.
      wait 3 s
      %echo% ~%djon% holds a finger to his lips and makes a shushing sound.
      wait 9 s
      %force% %djon% say So are you still sneaking off with Alastair?
      wait 9 s
      say No, oh my god, you didn't hear?
      wait 9 s
      %force% %djon% say Oh no, did he die in the invasion?
      wait 9 s
      say No, they turned him into a feather duster!
      wait 9 s
      %force% %djon% say They did not! At least he'll be quieter, though. He was a real know-it-all.
      wait 9 s
      say That's not funny, Djon. What if he was my soulmate?
      wait 9 s
      %force% %djon% say Just sprinkle some dust on yourself.
    break
    case 3
      set wright %instance.mob(11914)%
      say I'm glad they're getting the tower cleaned up. I did not come to study in some run-down old heap.
      wait 9 s
      %force% %djon% say Right? Floor 2 was a wreck. The pixies totally overran it. We're lucky they didn't get out of the tower.
      wait 9 s
      if %wright%
        say I saw Old Wright helping the warden wrangle them. Where were they when the whole thing was overrun?
        wait 9 s
        %force% %djon% say Glad I missed it.
        wait 9 s
        say He took care of the goblins, too. He's worked here for like 50 years.
      else
        say I heard Old Wright couldn't round up even a single goblin afterward.
        wait 9 s
        %force% %djon% say They are going to fire him.
        wait 9 s
        say They turned him into a sponge!
        wait 9 s
        %force% %djon% say They did not! He's worked here for like 50 years.
      end
      wait 9 s
      set mageina %room.people(11905)5
      %force% %mageina% say More like 100 at this point.
      wait 9 s
      %force% %djon% say Wow, rude.
      wait 9 s
      say Rude.
    break
  done
end
* short wait on room with var
set speak_time %timestamp%
remote speak_time %room.id%
set last_%room.template% %timestamp%
remote last_%room.template% %self.id%
~
#11829
Skycleave: Otherworlder escape~
0 ab 100
~
if %self.fighting% || %self.disabled%
  halt
end
set room %self.room%
if %room.template% == 11835
  %echo% The otherworlder whips its left arm against its chest and shouts, 'Bok jel thet!'
  wait 6 sec
  if %self.fighting% || %self.disabled%
    halt
  end
  %echo% A shrill, disembodied voice says, 'Mu cha shah ka pah ka.'
  wait 6 sec
  if %self.fighting% || %self.disabled%
    halt
  end
  set kara %room.people(11847)%
  if %kara%
    %echo% The otherworlder shrieks and whips one of its long arms at ~%kara%, who is hit in the head and stunned!
    dg_affect #11852 %kara% STUNNED on 30
    wait 3 sec
    if %self.fighting% || %self.disabled%
      halt
    end
  end
  %echo% The otherworlder swishes past you and darts out of the room!
  mgoto i11832
  %echo% ~%self% darts in from the east.
elseif %room.template% == 11832 || %room.template% == 11833
  set merc_vnums 11841 11842 11843 11844 11845 11846
  set ch %room.people%
  set done 0
  while %ch% && !%done%
    set next_ch %ch.next_in_room%
    if %ch.is_npc% && %merc_vnums% ~= %ch.vnum%
      nop %ch.add_mob_flag(!LOOT)%
      if %room.varexists(overboard)%
        * basic kill
        %echo% ~%self% swings ^%self% arms wildly, striking ~%ch% and killing *%ch%!
        %slay% %ch%
      else
        * overboard them
        makeuid downstairs room i11808
        %echo% ~%self% grabs ~%ch% and throws *%ch% over the railing!
        switch %random.4%
          case 1
            %subecho% %room% &&y~%ch% shouts, 'Aaaaaaaaaaaaaaah!'&&0
          break
          case 2
            %subecho% %room% &&y~%ch% shouts, 'Nooooooooooooooo!'&&0
          break
          case 3
            %subecho% %room% &&y~%ch% shouts, 'Heeeeeeeeeeeeelp!'&&0
          break
          case 4
            %subecho% %room% &&y~%ch% shouts, 'I don't get paid enough for thiiiiiiiiis!'&&0
          break
        done
        %echo% Your blood freezes as you hear a death cry and a splat!
        %teleport% %ch% %downstairs%
        %at% %downstairs% %echo% ~%ch% falls down from one of the floors above and splats on the tile floor!
        %at% %downstairs% %slay% %ch%
        set overboard 1
        remote overboard %room.id%
      end
      set done 1
    end
    set ch %next_ch%
  done
  if !%done%
    if %room.template% == 11832
      northwest
    elseif %room.template% == 11833
      north
    end
  end
elseif %room.template% == 11836
  if %self.morph%
    %echo% The otherworlder barrels through, rapidly shrinking back to its original size, and jumps out the window!
  else
    %echo% The otherworlder barrels through, flailing ^%self% arms wildly, and jumps out the window!
  end
  wait 0
  %echo% The otherworlder shouts, 'Bok jel thet far mesh bek tol cha... Shaw!
  %echo% You watch out the window as the otherworlder is enveloped in shimmering blue light and vanishes in midair!
  wait 0
  set ch %room.people%
  while %ch%
    if %ch.is_pc% && %ch.leader% == %self%
      %send% %ch% You are struck with vertigo as you stop short of the window and think better of following the otherworlder out.
    end
    set ch %ch.next_in_room%
  done
  %purge% %self%
else
  * unknown location somehow? Oh well, we tried
  detach 11829 %self.id%
end
~
#11830
Skycleave: Shared quest completion script~
2 v 0
~
return 1
switch %questvnum%
  case 11802
    * Dylane/Mop rescue
    set mop %instance.mob(11933)%
    if %mop%
      if %mop.room% != %room%
        %at% %mop.room% %echo% ~%mop% hops away.
        %teleport% %mop% %room%
      end
    else
      %load% mob 11933
      set mop %room.people%
      if %mop.vnum% != 11933
        %send% %actor% This quest seems to be broken.
        return 0
        halt
      end
    end
    if !%mop.has_trigger(11833)%
      attach 11833 %mop.id%
    end
    %force% %mop% cutscene
  break
  case 11810
  case 11811
  case 11812
  case 11875
  case 11975
    * these scripts guarantee Liked reputation
    if !%actor.has_reputation(11800,Liked)%
      nop %actor.set_reputation(11800,Liked)%
    end
    * check progress
    set emp %actor.empire%
    if %emp%
      if !%emp.is_on_progress(11800)% && !%emp.has_progress(11800)%
        %send% %actor% Your empire begins The Tower Skycleave (progress goal).
        nop %emp.start_progress(11800)%
        if %questvnum% == 11812
          * finish now
          nop %emp.add_progress(11800)%
        end
      end
    end
  break
  case 11821
    * Hugh Mann
    set hugh %room.people(11821)%
    if %hugh%
      %echo% # \&0
      %echo% # ~%hugh% grasps at some goblin trinkets and shouts, 'We did it! Victory for goblins!'
      %echo% # ~%hugh% throws off his coat to reveal he is actually two goblins, one on the other's shoulder!
      %echo% # The goblins sing and shout victory chants as they run out of the tower!
      %purge% %hugh%
    end
  break
done
~
#11831
Skycleave: Shared quest start script~
2 u 0
~
return 1
set spirit %instance.mob(11900)%
switch %questvnum%
  case 11810
    if %spirit.phase2%
      %send% %actor% You cannot start '%questname%' because floor 2 has already been rescued.
      return 0
    end
  break
  case 11811
    if %spirit.phase3%
      %send% %actor% You cannot start '%questname%' because floor 3 has already been rescued.
      return 0
    end
  break
  case 11812
    if %spirit.phase4%
      %send% %actor% You cannot start '%questname%' because floor 4 has already been rescued.
      return 0
    end
  break
done
~
#11832
Skycleave: Conditional mob visibility~
0 C 100
~
* toggles silent, !see
if %actor.is_npc%
  halt
end
* activated on greet or when moving
set see 0
set not 0
set vis 0
* special handling to catch actor when not in the room
if %actor%
  set ch %actor%
  set spec 1
else
  set ch %self.room.people%
  set spec 0
end
* determine if anyone should/shouldn't see me
while %ch%
  if %ch.is_pc%
    set varname %ch.id%_can_see
    set %varname% 0
    if %self.vnum% == 11900
      * intro spirit: shows when any floor quest complete
      if %ch.completed_quest(11810)% || %ch.completed_quest(11811)% || %ch.completed_quest(11812)%
        set see 1
        set %varname% 1
      else
        set not 1
      end
    elseif %self.vnum% == 11801
      * Dylane: visible only if quest incomplete
      if %ch.completed_quest(11802)%
        set not 1
      else
        set see 1
        set %varname% 1
      end
    end
    remote %varname% %self.id%
  end
  if %spec%
    set spec 0
    set ch %self.room.people%
  else
    set ch %ch.next_in_room%
  end
done
* determine behavior based on vnum
switch %self.vnum%
  case 11900
    * intro spirit: any "see"
    set vis %see%
  break
  case 11801
    * Dylane
    if !%not%
      set vis 1
    end
  break
done
* and make visible
if %vis% && %self.affect(11832)%
  dg_affect #11832 %self% off
  nop %self.remove_mob_flag(SILENT)%
  set arrives 1
elseif !%vis% && !%self.affect(11832)%
  dg_affect #11832 %self% !SEE on -1
  dg_affect #11832 %self% !TARGET on -1
  dg_affect #11832 %self% SNEAK on -1
  nop %self.add_mob_flag(SILENT)%
  set leaves 1
end
* messaging?
if %arrives% || %leaves%
  set ch %self.room.people%
  while %ch%
    if %ch.is_pc%
      if %arrives%
        %send% %ch% ~%self% arrives.
      elseif %leaves% && %self.var(%ch.id%_can_see)%
        %send% %ch% ~%self% leaves.
      end
    end
    set ch %ch.next_in_room%
  done
end
~
#11833
Skycleave: Quest cutscenes~
0 cx 0
cutscene~
* used for mob cutscenes
if %actor% && %actor% != %self%
  return 0
  halt
end
if %self.vnum% == 11933
  * mop 11933, Dylane quest 11802
  set annelise %self.room.people(11939)%
  if !%annelise%
    set annelise %self.room.people(11919)%
  end
  nop %self.add_mob_flag(SILENT)%
  nop %self.add_mob_flag(SENTINEL)%
  nop %annelise.add_mob_flag(SILENT)%
  wait 1
  %echo% ~%self% marches in from the north.
  wait 3 sec
  %echo% ~%annelise% pulls a surprisingly large staff out of her many-layered robes...
  wait 9 sec
  %force% %annelise% say Let the weight of your service to the Tower outweigh your offense against its masters.
  %echo% She taps her staff on the slate floor three times.
  wait 9 sec
  %force% %annelise% say Ahem... In service of the Tower I restore you bodily as you once were!
  %echo% She taps her staff on the floor again.
  wait 9 sec
  %echo% ~%annelise% sighs.
  wait 3 sec
  %force% %annelise% say By the Black Domain, by all humane, let him regain... Er, un-detain the plain Dylane!
  wait 1
  %echo% There's a blinding flash from Annelise's staff as she hits it on the floor one last time...
  * convert to boy
  detach 11935 %self.id%
  detach 11934 %self.id%
  %mod% %self% sex male
  %mod% %self% keywords Dylane younger boy apprentice
  %mod% %self% shortdesc Dylane the Younger
  %mod% %self% longdesc A boy of no more than fourteen stands in the center of the room.
  %mod% %self% lookdesc Dylane is no more than fourteen years old, tall and gangly with legs that look like a pair of sticks poking out from under white apprentice robes that
  %mod% %self% append-lookdesc are inexplicably wet at the bottom. Dark, teary eyes poke out from underneath a mop of straw-colored hair as he stares at you wordlessly.
  * and finish
  wait 9 sec
  %force% %annelise% say Rather embarrassing to admit I'm dreadful at non-rhyming magic.
  wait 6 sec
  say What happened? Oh! I can speak again! I'm free! I'm free!
  wait 9 sec
  %echo% ~%self% turns and sees ~%annelise% and his face goes sheet-white.
  wait 9 sec
  %force% %annelise% say Yes, you're free, you're free. You shall find your father in the lobby. May I suggest you make a hasty escape?
  wait 9 sec
  %force% %annelise% say Okay, on with you, get out of here before the grand high sorcerer catches you lacking.
  wait 3 sec
  %echo% The boy wastes no more time and darts out of the room. The last you hear of him is clambering down the stairs.
  nop %annelise.remove_mob_flag(SILENT)%
  %purge% %self%
end
~
#11834
Skycleave: Free the otherworlder~
0 c 0
free unchain unshackle break smash release~
return 1
* validate arg
if %actor.fighting%
  %send% %actor% You're a little busy at the moment!
  halt
end
* 2 modes:
if break /= %cmd% || smash /= %cmd%
  * break/smash only
  if !%arg%
    %send% %actor% Break what?
    halt
  elseif %arg% /= chains || %arg% /= shackles || %arg% /= gems
    * ok
  else
    %send% %actor% You can't break that.
    halt
  end
elseif !%arg%
  %send% %actor% Free whom?
  halt
elseif %actor.char_target(%arg%)% != %self%
  %send% %actor% You can't free that.
  halt
end
* more checks that apply to both modes
if %self.vnum% == 11934
  %send% %actor% The shackles have been magically secured. You cannot free the otherworlder.
  halt
end
* echoes
%send% %actor% You smash the violet gems that bind the shackles and they shatter... and the otherworlder breaks free!
%echoaround% %actor% ~%actor% smashes the violet gems that bind the otherworlder's shackles and they shatter... the otherworlder breaks free!
* let's get this bread
%load% mob 11829
set mob %self.room.people%
if %mob.vnum% == 11829
  set spirit %instance.mob(11900)%
  set diff %spirit.diff3%
  remote diff %mob.id%
  if (%diff% // 2) == 0
    nop %mob.add_mob_flag(HARD)%
  end
  if %diff% >= 3
    nop %mob.add_mob_flag(GROUP)%
  end
  nop %mob.unscale_and_reset%
end
* check triggers
set ch %self.room.people%
while %ch%
  if %ch.on_quest(11834)%
    %quest% %ch% trigger 11834
  end
  set ch %ch.next_in_room%
done
* and me
%purge% %self%
~
#11835
Skycleave: Barricade restrings~
1 n 100
~
* not listed here: 11810 (uses default barricade), 11808 (default crate wall)
switch %self.room.template%
  case 11830
    * Third Floor West (3A)
    %mod% %self% keywords table large barricade overturned
    %mod% %self% longdesc A large table barricades the top of the steps.
    %mod% %self% lookdesc The square oak table is on its side, requiring some agility to get past it and up off of the stairs. The table is missing one leg and there are scorch marks on both the top and bottom of it.
  break
  case 11832
    * Third Floor East (3A)
    %mod% %self% keywords furniture stack barricade chairs tables
    %mod% %self% shortdesc a stack of furniture
    %mod% %self% longdesc This section of the walkway is blocked off with a stack of furniture.
    %mod% %self% lookdesc Skycleave's fancy chairs and end tables have been stacked to force would-be rescuers to move single-file through a narrow gap in order to make progress along the walkway.
  break
  case 11836
    * Lich Labs (3A)
    %mod% %self% keywords bookshelf shelf overturned massive
    %mod% %self% shortdesc an overturned bookshelf
    %mod% %self% longdesc A massive bookshelf is overturned in front of the door.
    %mod% %self% lookdesc A truly massive bookshelf has been turned on its side and pushed in front of the door to block any view of the room from the walkway outside. You're forced to squeeze by and enter at a disadvantage.
  break
  case 11865
    * Celiya Living Quarters (4A)
    %mod% %self% keywords wardrobe hefty
    %mod% %self% shortdesc the wardrobe
    %mod% %self% longdesc A hefty wardrobe has been pushed in front of the door, which opens the other way.
    %mod% %self% lookdesc It looks like someone has pushed the oversized wardrobe in front of the door in a panic, not realizing that the door opens out to the office rather than into the bedroom.
  break
  case 11803
    * Ground Floor N (1A)
    %mod% %self% keywords crates wall stacked wooden makeshift boxes
    %mod% %self% longdesc A makeshift wall of crates blocks off the center of the chamber.
    %mod% %self% lookdesc A wall of stacked crates blocks access to the center of the chamber and its magnificent stone fountain.
  break
  case 11804
    * Ground Floor W (1A)
    %mod% %self% keywords crates wall piled stacked wooden makeshift boxes
    %mod% %self% longdesc Crates have been piled to form a wall blocking the chamber's central fountain to the east.
    %mod% %self% lookdesc Stacks of crates create a makeshift wall that blocks off the center of the great chamber and its grand stone fountain.
  break
done
detach 11835 %self.id%
~
#11836
Skycleave: Object interactions~
1 c 4
open release look free unleash~
return 0
if %arg.car% == in
  set arg %arg.cdr%
  set in_arg 1
else
  set in_arg 0
end
if %actor.obj_target(%arg.car%)% != %self%
  halt
end
if %self.vnum% == 11836
  * Scaldorran's repository: open/release/look in
  set spirit %instance.mob(11900)%
  if (open /= %cmd% || release /= %cmd% || free /= %cmd% || unleash /= %cmd%)
    return 1
    if %spirit.lich_released%
      %send% %actor% The repository is already open!
      halt
    end
    * load lich
    %load% mob 11836
    set mob %self.room.people%
    if %mob.vnum% != 11836
      %send% %actor% Something went wrong.
      halt
    end
    %send% %actor% You open the repository and release the Lich Scaldorran!
    %echoaround% %actor% ~%actor% opens the repository and releases the Lich Scaldorran!
    * Mark for claw game & directory
    set spirit %instance.mob(11900)%
    set claw3 1
    remote claw3 %spirit.id%
    set lich_released %actor.id%
    remote lich_released %spirit.id%
    * check triggers
    set ch %self.room.people%
    while %ch%
      if %ch.on_quest(11836)%
        %quest% %ch% trigger 11836
      end
      set ch %ch.next_in_room%
    done
    halt
  elseif look /= %cmd% && %in_arg%
    * only look IN
    return 1
    if %spirit.lich_released%
      %send% %actor% There's a message in the bottom of the repository that just says 'BOO!'
    else
      %send% %actor% The repository is closed.
    end
   end
elseif %self.vnum% == 11927
  * time-traveler's corpse
  if look /= %cmd% && %actor.on_quest(11920)%
    %quest% %actor% trigger 11920
  end
  * shhh
  return 0
end
~
#11837
Skycleave: List command in wrong phase~
0 c 0
list buy~
%send% %actor% They don't seem to be selling anything right now. The tower is still under siege!
return 1
~
#11838
Skycleave: Weyonomon wanders through walls in 1A~
0 b 15
~
set target 0
set dir north
set from south
switch %self.room.template%
  case 11801
    set target i11803
    set dir north
    set from south
  break
  case 11802
    set target i11807
    set dir south
    set from south
  break
  case 11803
    set target i11805
    set dir east
    set from north
  break
  case 11804
    set target i11801
    set dir south
    set from west
  break
  case 11805
    set target i11806
    set dir northwest
    set from east
  break
  case 11806
    set target i11802
    set dir southeast
    set from north
  break
  case 11807
    set target i11804
    set dir south
    set from south
  break
  case 11808
    set target i11804
    set dir west
    set from east
  break
done
if %target%
  %echo% ~%self% floats through the wall to the %dir%.
  set ch %self.room.people%
  while %ch%
    if %ch.leader% == %self% && %ch.position% == Standing
      %send% %ch% You try to follow but smack into the wall. Ouch!
      %echoaround% %ch% ~%ch% tries to follow but smacks into the wall with a thud.
      %damage% %ch% 10 physical
    end
    set ch %ch.next_in_room%
  done
  mgoto %target%
  %echo% ~%self% floats in through the wall from the %from%.
end
~
#11839
Skycleave: Scaldorran murder spree~
0 b 100
~
* the last 3 are the bosses; killing any 1 of them will stop him (only kills 1 boss)
set merc_list 11841 11842 11843 11844 11845 11846 11848 11847 11849
* check room first
set room %self.room%
set ch %room.people%
set target 0
set done 0
set final 0
while %ch% && !%target%
  if %ch.is_npc% && %merc_list% ~= %ch.vnum%
    set target %ch%
  end
  set ch %ch.next_in_room%
done
if %target%
  * found someone to merc
  nop %target.add_mob_flag(!LOOT)%
  switch %target.vnum%
    case 11841
      * rogue merc
      %echo% Scaldorran flies in through |%target% mouth... ~%target% pulls out ^%target% dagger and stabs *%target%self in the heart!
      %subecho% %room% &&y~%target% shouts, 'No! No! Nooooooooooo...'&&0
    break
    case 11842
      * caster merc
      %subecho% %room% &&y~%target% shouts, 'Stay back, fiend!'&&0
      %echo% ~%target% panics and hurls a spell at Scaldorran, who expertly reflects it. ~%target% is hit in the head and falls to the ground!
    break
    case 11843
      * archer merc
      %echo% ~%target% shoots a dozen arrows into Scaldorran, who comes apart at the wrappings and envelops ~%target%, stabbing *%target% with every arrow!
      %subecho% %room% &&y~%target% shouts, 'Aaaaaaaaaaaaaaagh!'&&0
    break
    case 11844
      * nature mage merc
      %echo% ~%target% starts to twist and morph into something enormous... until Scaldorran wraps his bandages around *%target%, whereupon &%target% dissolves into a sticky mess!
      %subecho% %room% &&ySomething unrecognizable lets out an anguished shout and then a splattering sound!&&0
    break
    case 11845
      * vampire merc
      %subecho% %room% &&y~%target% shouts, 'Face me, lich! I'm more than you can...'&&0
      %echo% Scaldorran twists and whirls and whips ~%target% over and over with his bandages, slicing open thousands of tiny cuts. You watch in horror as ~%target% bleeds out and crumples to the floor.
    break
    case 11846
      * armored merc
      %echo% Scaldorran appears behind ~%target% and silently slides his bandages into |%target% armor... You are forced to watch as ~%target% turns blue in the face and falls to the ground.
      %subecho% %room% There's a loud, echoing clang from heavy armor hitting the floor.
    break
    case 11848
      * Bleak Rojjer
      %subecho% %room% &&y~%target% shouts a truncated, 'Wha!?'&&0
      %echo% Linen wrappings whip out from the painting on the wall and you are forced to watch as Scaldorran strangles ~%target% and bashes his head against the floor.
      set done 1
    break
    case 11847
      * Kara Virduke
      %subecho% %room% &&y~%target% shouts, 'Oh my wo...'&&0
      %echo% Scaldorran pops out of a cabinet just as ~%target% opens it and snakes one of his long linen wrappings down her throat. You watch in horror as she claws at her mouth, unable to stop it.
      set done 1
    break
    case 11849
      * Trixton Vye
      %subecho% %room% &&y~%target% shouts, 'Pathetic dead thing, face now your new master! By the Vy...'&&0
      %echo% One of Scaldorran's bandages slices forward through the air, piercing Trixton Vye's decrepit throat above the collar.
      %echo% Trixton's mouth continues to move, but no sound comes out as a red spot spreads from the top of his shirt.
      set done 1
      set final 1
    break
  done
  %slay% %target%
  if %done% && !%final%
    * last one?
    if %room.template% != 11836
      wait 1
      %echo% Scaldorran twists and whirls until his tattered wrappings fold in on themselves and he vanishes!
      mgoto i11836
      %echo% Your blood freezes as the air cracks open and the Lich Scaldorran crawls out of the void!
    end
    %mod% %self% longdesc The Lich Scaldorran is drawing power from the tower.
    detach 11839 %self.id%
  end
  halt
end
* didn't find someone... try to find someone in another room
while %merc_list%
  set vnum %merc_list.car%
  set merc_list %merc_list.cdr%
  set mob %instance.mob(%vnum%)%
  if %mob% && %mob.room% != %room%
    %echo% Scaldorran twists and whirls until his tattered wrappings fold in on themselves and he vanishes!
    mgoto %mob.room%
    %echo% Your blood freezes as the air cracks open and the Lich Scaldorran crawls out of the void!
    halt
  end
done
* nobody left?
if %room.template% != 11836
  %echo% Scaldorran twists and whirls until his tattered wrappings fold in on themselves and he vanishes!
  mgoto i11836
  %echo% Your blood freezes as the air cracks open and the Lich Scaldorran crawls out of the void!
end
%mod% %self% longdesc The Lich Scaldorran is drawing power from the tower.
detach 11839 %self.id%
~
#11840
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
  if %msg%
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
if %self.varexists(no_sentinel)%
  nop %self.remove_mob_flag(SENTINEL)%
end
if %self.varexists(no_silent)%
  nop %self.remove_mob_flag(SILENT)%
end
* wait between stories
wait %story_gap%
~
#11841
Mercenary Rogue combat: Vicious Blind, Knife Throw~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2
  set num_left 2
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
* perform move
skyfight lockout 25 20
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Vicious Blind
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %send% %targ% &&m**** &&Z~%self% feints low and goes for your eyes... ****&&0 (dodge)
  %echoaround% %targ% &&m~%self% feints low and goes for |%targ% eyes...&&0
  skyfight setup dodge %targ%
  eval time 8 - %diff%
  wait %time% s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
  elseif %targ.var(did_sfdodge)%
    * miss
    if %diff% > 2
      %echo% &&m~%self% lunges toward |%targ% face with ^%self% dagger, but misses!&&0
    else
      %echo% &&m~%self% lunges toward |%targ% face with some kind of slime, but misses... and gets it in ^%self% own eyes!&&0
      eval dur 10 / %diff%
      if %dur% < 1
        set dur 1
      end
      dg_affect #11841 %self% BLIND on %dur%
      if %diff% == 1
        dg_affect #11856 %targ% TO-HIT 25 20
      end
    end
  else
    * hit
    if %diff% > 2
      %echo% &&m~%self% slashes |%targ% forehead, opening a vicious wound!&&0
      %send% %targ% &&mBlood trickles down into your eyes, blinding you!&&0
      eval dur 6 * %diff%
      eval ouch 75 * %diff%
      dg_affect #11841 %targ% BLIND on %dur%
      %dot% #11842 %targ% %ouch% %dur% physical
      %damage% %targ% %ouch% physical
    else
      %echo% &&m~%self% smears a vicious concoction on |%targ% eyes!&&0
      %send% %targ% &&mYou're blind!&&0
      eval dur 10 * %diff%
      dg_affect #11841 %targ% BLIND on %dur%
      %damage% %targ% 5 physical
    end
  end
  skyfight clear dodge
elseif %move% == 2 && !%self.aff_flagged(BLIND)%
  * Knife Throw
  skyfight clear dodge
  %echo% &&m~%self% draws a brace of throwing knives from a concealed pouch...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 1
  set cycle 1
  eval wait 8 - %diff%
  set targ %random.enemy%
  while %targ% && %cycle% <= %diff% + 1
    set targ_id %targ.id%
    skyfight setup dodge %targ%
    %send% %targ% &&m**** &&Z~%self% is aiming a knife at you! ****&&0 (dodge)
    wait %wait% s
    if %targ.id% != %targ_id% || %targ.position% == Dead || %self.disabled%
      * gone
    elseif %targ.var(did_sfdodge)%
      %echo% &&m~%self% flings a knife at ~%targ%, but misses!&&0
      if %diff% == 1
        dg_affect #11856 %targ% off silent
        dg_affect #11856 %targ% TO-HIT 25 20
      end
    else
      * hit
      %echo% &&m~%self% flings a knife at ~%targ%!&&0
      eval dam 25 + (25 * %diff%)
      %dot% #11842 %targ% %dam% 20 physical 5
      %damage% %targ% %dam% physical
    end
    skyfight clear dodge
    eval cycle %cycle% + 1
    set targ %random.enemy%
  done
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11842
Mercenary Caster combat: Firebrand, Enchant Weapons~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2
  set num_left 2
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
* perform move
skyfight lockout 25 20
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Firebrand
  skyfight clear interrupt
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  %echo% &&m**** &&Z~%self% mutters an incantation and taps |%targ% arm with ^%self% staff... ****&&0 (interrupt)
  skyfight setup interrupt all
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 8 s
  if %targ.id% != %targ_id% || %targ.position% == Dead
    * gone
  elseif %self.disabled% || (%self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2)
    %echo% &&mThe firebrand blasts back in |%self% face as &%self% is interrupted in the middle of the spell!
    if %diff% == 1
      dg_affect #11873 %self% TO-HIT -15 20
    end
    %damage% %self% 50 magical
  elseif %targ.trigger_counterspell%
    %echo% A vortex of smoke swirls briefly around |%targ% arm as ^%targ% counterspell breaks the magic.
  else
    * hit
    if %targ.affect(11843)%
      %echo% &&mThe flames around ~%targ% grow more intense as the firebrand bursts into flame!&&0
    elseif %diff% > 1
      %echo% &&mThe firebrand on |%targ% arm grows into a fiery rune and envelops *%targ%!
    else
      %echo% &&mA fiery rune appears on |%targ% arm, searing ^%targ% flesh!
    end
    * effect
    if %diff% > 1
      eval ouch 75 * %diff%
      %dot% #11843 %targ% %ouch% 45 fire 3
      %damage% %targ% %ouch% fire
    else
      %dot% #11843 %targ% 75 15 fire
      %damage% %targ% 50 fire
    end
  end
  skyfight clear interrupt
elseif %move% == 2
  * Enchant Weapons
  skyfight clear interrupt
  %echo% &&m**** &&Z~%self% thrusts ^%self% staff into the air and starts shouting magic words... ****&&0 (interrupt)
  skyfight setup interrupt all
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 8 s
  if %self.disabled% || (%self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2)
    %echo% &&m~%self% is distracted and misspeaks, and the spell explodes in ^%self% face!&&0
    dg_affect #11852 %self% HARD-STUNNED on 10
    if %diff% == 1
      %damage% %self% 50 magical
    end
  else
    set ch %room.people%
    eval amt %diff% * 5
    while %ch%
      if %self.is_ally(%ch%)%
        %echo% &&m|%ch% weapon glows silver.&&0
        if %self.diff% == 4
          dg_affect #11844 %ch% HASTE on 30
        end
        dg_affect #11844 %ch% BONUS-PHYSICAL %amt% 30
        dg_affect #11844 %ch% BONUS-MAGICAL %amt% 30
      end
      set ch %ch.next_in_room%
    done
  end
  skyfight clear interrupt
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11843
Mercenary Archer combat: Rapid Fire, Rain of Arrows~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2
  set num_left 2
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
* perform move
skyfight lockout 25 20
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Rapid Fire
  skyfight clear dodge
  %echo% &&m~%self% draws ^%self% bow and a fistful of arrows...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 1
  set cycle 1
  eval wait 8 - %diff%
  while %cycle% <= %diff% + 1
    set targ %random.enemy%
    set targ_id %targ.id%
    skyfight setup dodge %targ%
    if %targ%
      %send% %targ% &&m**** &&Z~%self% is aiming straight at you! ****&&0 (dodge)
    end
    wait %wait% s
    if %self.disabled% || !%targ% || %targ.id% != %targ_id% || %targ.position% == Dead
      * gone
    elseif %targ.var(did_sfdodge)%
      %echo% &&m~%self% looses an arrow at ~%targ%, but it thuds into the far wall!&&0
      if %diff% == 1
        dg_affect #11856 %targ% TO-HIT 25 20
      end
    else
      * hit
      %echo% &&m~%self% fires an arrow at ~%targ%!&&0
      eval dam 25 + (50 * %diff%)
      if %dam% > 1
        %dot% #11842 %targ% %dam% 20 physical 5
      end
      %damage% %targ% %dam% physical
    end
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
elseif %move% == 2
  * Rain of Arrows
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  %echo% &&m**** &&Z~%self% leans back and fires a barrage of arrows into the air! ****&&0 (dodge)
  skyfight setup dodge all
  eval dam %diff% * 30
  eval ouch %diff% * 50
  set any 0
  set cycle 0
  eval max (%diff% + 2) / 2
  while %cycle% < %max%
    wait 5 s
    if %self.disabled%
      nop %self.remove_mob_flag(NO-ATTACK)%
      halt
    end
    set this 0
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if %ch.var(did_sfdodge)%
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        else
          set any 1
          set this 1
          %echo% &&m|%self% arrows rain down on ~%ch%!&&0
          %dot% #11824 %ch% %ouch% 15 physical 3
          %damage% %ch% %dam% physical
        end
      end
      set ch %next_ch%
    done
    if !%this%
      %echo% &&mArrows rain down around you!&&0
    end
    eval cycle %cycle% + 1
  done
  if !%any%
    * full miss
    %echo% &&m~%self% looks exhausted as &%self% lowers ^%self% bow.&&0
    dg_affect #11852 %self% HARD-STUNNED on 5
  end
  skyfight clear dodge
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11844
Mercenary Nature Mage combat: Healing Wave, Snake Form, Bite~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 2
  set num_left 3
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
* perform move
skyfight lockout 25 20
if %move% == 1
  * Healing Wave / Morph Back
  if %self.morph%
    set current %self.name%
    %morph% %self% normal
    %echo% &&m%current% rapidly morphs back into ~%self%!&&0
    wait 2 s
    if %self.disabled%
      halt
    end
  end
  skyfight clear interrupt
  %echo% &&m**** &&Z~%self% thrusts ^%self% hands skyward... ****&&0 (interrupt)
  skyfight setup interrupt all
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 8 s
  if %self.disabled% || (%self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2)
    %echo% &&m~%self% is distracted and the healing spell goes wild!&&0
    set fail 1
    eval amt 100 / %diff%
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  else
    %echo% &&m~%self% sends out a wave of healing light!&&0
    set fail 0
    eval amt %diff% * 25
  end
  set ch %room.people%
  eval amt %diff% * 5
  while %ch%
    if %fail%
      set ok %self.is_enemy(%ch%)%
    elseif %ch% == %self%
      set ok 1
    else
      set ok %self.is_ally(%ch%)%
    end
    if %ok%
      %echo% &&mThe healing wave passes through ~%ch%!&&0
      %send% %ch% You're healed!
      if %self.diff% == 4 && !%fail%
        dg_affect #11844 %ch% HASTE on 5
      end
      %heal% %ch% health %amt%
    end
    set ch %ch.next_in_room%
  done
  skyfight clear interrupt
elseif %move% == 2
  * Snake Form / Bite
  if %self.morph% != 11848
    * Snake Form
    skyfight clear interrupt
    %echo% &&m**** &&Z~%self% begins to twist and coil... ****&&0 (interrupt)
    skyfight setup interrupt all
    if %diff% == 1
      nop %self.add_mob_flag(NO-ATTACK)%
    end
    wait 8 s
    if %self.disabled% || (%self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2)
      %echo% &&m~%self% is interrupted and can't finish morphing!&&0
      nop %self.remove_mob_flag(NO-ATTACK)%
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
      end
      halt
    end
    * morph ok
    skyfight clear interrupt
    set current %self.name%
    %morph% %self% 11848
    %echo% %current% rapidly morphs into ~%self%!
    dg_affect #3062 %self% HASTE on -1
    eval amt %diff% * 5
    dg_affect #3062 %self% BONUS-PHYSICAL %amt% -1
    dg_affect #3062 %self% DODGE %amt% -1
    if %diff% > 1
      %heal% %self% health 50
    end
    wait 8 s
    if %self.disabled%
      nop %self.remove_mob_flag(NO-ATTACK)%
      halt
    end
  end
  * Bite:
  skyfight clear dodge
  set targ %actor%
  set id %targ.id%
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  %send% %targ% &&m**** &&Z~%self% coils and pulls ^%self% head back... ****&&0 (dodge)
  skyfight setup dodge %targ%
  wait 5 s
  nop %self.remove_mob_flag(NO-ATTACK)%
  wait 3 s
  if %self.disabled%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
  elseif %targ.var(did_sfdodge)%
    %echo% &&m~%self% lunges forward with gnashing fangs and narrowly misses ~%targ% with the strike!&&0
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
  else
    * hit
    %echo% &&m~%self% lunges foward and bites right into ~%targ% with ^%self% enormous fangs!&&0
    if %diff% > 1
      %send% %targ% You don't feel so good...
      if %diff% > 2
        dg_affect #11848 %targ% SLOW on 15
      end
      eval ouch %diff% * 50
      %dot% #11848 %targ% %ouch% 15 poison
    end
    %damage% %targ% %ouch% physical
  end
  skyfight clear dodge
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11845
Mercenary Vampire combat: Blood Curse, Exsanguinate~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2
  set num_left 2
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
* perform move
skyfight lockout 25 20
if %move% == 1
  * Blood Curse
  skyfight clear interrupt
  %echo% &&m**** &&Z~%self% makes a cut on the palm of ^%self% hand... ****&&0 (interrupt)
  skyfight setup interrupt all
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 8 s
  if %self.disabled% || (%self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2)
    %echo% &&m~%self% can't seem to finish the blood ritual!&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  elseif %diff% < 3
    * 1 target
    %echo% &&m~%self% smears ~%actor% with ^%self% blood, which glows bright crimson!
    eval amt %self.level%/15
    %send% %actor% &&mYou feel weaker!&&0
    dg_affect #11849 %actor% BONUS-PHYSICAL -%amt% 30
    dg_affect #11849 %actor% BONUS-MAGICAL -%amt% 30
    dg_affect #11849 %actor% BONUS-HEALING -%amt% 30
  else
    * high diff
    %echo% &&m~%self% presses ^%self% palm to the floor... a bright crimson glow fills the room!&&0
    set ch %room.people%
    eval amt %self.level%/10
    while %ch%
      if %self.is_enemy(%ch%)%
        %send% %ch% &&mYou feel weaker!&&0
        dg_affect #11849 %ch% BONUS-PHYSICAL -%amt% 30
        dg_affect #11849 %ch% BONUS-MAGICAL -%amt% 30
        dg_affect #11849 %ch% BONUS-HEALING -%amt% 30
      end
      set ch %ch.next_in_room%
    done
  end
  skyfight clear interrupt
elseif %move% == 2
  * Exsanguinate
  skyfight clear interrupt
  %echo% &&m**** &&Z~%self% makes a cut on the palm of ^%self% hand... ****&&0 (interrupt)
  skyfight setup interrupt all
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 8 s
  if %self.disabled% || (%self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2)
    %echo% &&m~%self% can't seem to finish the blood ritual!&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  elseif %diff% < 3
    * 1 target
    %echo% &&m~%self% smears ~%actor% with ^%self% blood, which glows deep crimson!
    %echo% &&mOld injuries all over |%actor% body suddenly begin to bleed!&&0
    eval amt %diff% * 50
    %dot% #11842 %actor% %amt% 20 physical
  else
    * high diff
    %echo% &&m~%self% thrusts ^%self% hand into the air, which starts to glow deep crimson!&&0
    set ch %room.people%
    eval amt %diff% * 50
    while %ch%
      if %self.is_enemy(%ch%)%
        %echo% &&mOld injuries all over |%ch% body suddenly begin to bleed!&&0
        %dot% #11842 %ch% %amt% 20 physical
      end
      set ch %ch.next_in_room%
    done
  end
  skyfight clear interrupt
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11846
Mercenary Tank combat: Shield Bash, Big Kick~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2
  set num_left 2
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
* perform move
skyfight lockout 25 20
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Shield Bash
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %send% %targ% &&m**** &&Z~%self% holds up ^%self% shield and moves toward you... ****&&0 (dodge)
  %echoaround% %targ% &&m~%self% holds up ^%self% shield and moves toward ~%targ%...&&0
  skyfight setup dodge %targ%
  eval time 8 - %diff%
  wait %time% s
  if !%targ% || %targ.id% != %id%
    * gone
  elseif %self.disabled% || %targ.var(did_sfdodge)%
    * miss
    %echo% &&m~%self% tries to bash ~%targ% with ^%self% shield, but misses and loses ^%self% footing!&&0
    if %diff% < 3
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
  else
    %echo% &&m~%self% bashes ~%targ% with ^%self% shield!
    eval dur %diff% * 5
    if (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
      dg_affect #11851 %targ% STUNNED on %dur%
    end
    if %diff% > 1
      %send% %targ% That really hurt!
      eval amt %diff% * 50
      %damage% %targ% %amt% physical
    end
  end
  skyfight clear dodge
elseif %move% == 2 && !%self.aff_flagged(BLIND)%
  * Big Kick
  set death_rooms 11830 11831 11832 11833 11834
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %send% %targ% &&m**** &&Z~%self% plants ^%self% feet and checks ^%self% balance... ****&&0 (dodge)
  %echoaround% %targ% &&m~%self% plants ^%self% feet and checks ^%self% balance...&&0
  skyfight setup dodge %targ%
  eval time 8 - %diff%
  wait %time% s
  if !%targ% || %targ.id% != %id%
    * gone
  elseif %self.disabled% || %targ.var(did_sfdodge)%
    * miss
    %echo% &&m~%self% tries to kick ~%targ%, but misses and falls!&&0
    if %diff% < 3
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
  else
    %echo% &&m~%self% lets loose all of ^%self% might and kicks ~%targ% across the room!
    if %diff% == 4 && %death_rooms% ~= %room.template%
      %send% %targ% You go sailing over the railing! Oh no...
      %echoaround% %targ% ~%targ% goes sailing over the railing and vanishes down the central column!
      %teleport% %targ% i11808
      %force% %targ% shout Aaaaaaahhhhhhhhhhhhh!
      %send% %targ% You splat on the floor next to the fountain!
      %at% %targ.room% %echoaround% %targ% ~%targ% flies in from above and splats next to the fountain!
      %slay% %targ%
    else
      if %diff% > 1 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
        %send% %targ% You fly into the wall and hit your head! That hurt!
        eval dur %diff% * 3
        dg_affect #11851 %targ% STUNNED on %dur%
      end
      eval amt %diff% * 50
      %damage% %targ% %amt% physical
    end
  end
  skyfight clear dodge
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11847
Kara Virduke Mercenary Archmage combat: Chain Lightning, Arcane Tattoos, Rainbow Beam, Summon Minions~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4
  set num_left 4
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
* perform move
skyfight lockout 30 35
if %move% == 1
  * Chain Lightning
  %echo% &&m**** ~%self% holds out her staff as sparks crackle across the floor... ****&&0 (interrupt and dodge)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  skyfight clear interrupt
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  while !%broke% && %cycle% <= 4
    skyfight setup dodge all
    wait 4 s
    if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou manage to interrupt Lady Virduke before another chain of lightning!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&m~%self% seems momentarily distracted.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
        wait 10 s
      elseif %diff% < 4
        dg_affect #11852 %self% HARD-STUNNED on 5
        wait 5 s
      end
    else
      %echo% &&m|%self% calamander staff hisses and cracks as it focuses the lightning into a chain...&&0
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if %ch.var(did_sfdodge)%
            %send% %ch% &&mYou manage to narrowly dodge the chain lightning!&&0
            if %diff% == 1
              dg_affect #11856 %ch% off
              dg_affect #11856 %ch% TO-HIT 25 20
            end
          elseif %ch.trigger_counterspell%
            %echo% &&m|%ch% counterspell deflects a bolt of lightning back at Lady Virduke!&&0
            eval amt 200 / %diff%
            %damage% %self% %amt% magical
          else
            * hit
            %echo% &&mThe chain lightning hits ~%ch%!&&0
            %damage% %ch% 100 physical
            if %cycle% == 4 && %diff% == 4 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
              dg_affect #11851 %ch% STUNNED on 10
            end
          end
        end
        set ch %next_ch%
      done
    end
    if !%broke% && %cycle% < 4
      %echo% &&m**** Here comes more chain lightning... ****&&0 (interrupt and dodge)
    end
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 2
  * Arcane Tattoos
  skyfight clear interrupt
  %echo% &&m**** ~%self% mutters an incantation... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup interrupt all
  if %self.diff% < 3 || %room.players_present% < 2
    set requires 1
  else
    set requires 2
  end
  wait 4 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if %self.var(sfinterrupt_count,0)% < %requires%
    %echo% &&m**** ~%self% rolls up her sleeves as arcane tattoos all over her body glow gold! ****&&0 (interrupt)
    dg_affect #11847 %self% HEAL-OVER-TIME %self.level% 30
  end
  wait 4 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if %self.var(sfinterrupt_count,0)% >= %requires%
    %echo% &&m~%self% is interrupted before she can finish the spell!&&0
    dg_affect #11847 %self% off
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
    wait 30 s
  else
    %echo% &&m~%self% finishes another incantation as more tattoos on her arms light up in silver!&&0
    eval amount %diff% * 5
    dg_affect #11855 %self% BONUS-MAGICAL %amount% 60
    dg_affect #11855 %self% HASTE on 25
  end
  skyfight clear interrupt
elseif %move% == 3
  * Rainbow Beam
  %echo% &&m~%self% raises her staff high and slams it down!&&0
  %echo% &&m**** An iridescent sigil etches itself across the floor as ~%self% begins to chant! ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  set needed %room.players_present%
  if %needed% > 4
    set needed 4
  end
  set pain 0
  set cycle 1
  while %cycle% <= 5
    skyfight clear interrupt
    skyfight setup interrupt all
    wait 5 s
    if %self.sfinterrupt_count% >= %needed%
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou manage to interrupt |%self% spell, if only briefly!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&m~%self% seems distracted from her chant.&&0
    else
      eval pain %pain% + 1
      if %cycle% == 1
        %echo% &&m~%self% chants, 'Sun meets rain, illuminates the air...'&&0
      elseif %which% == 2
        %echo% &&m~%self% chants, 'Thus is shown your countenance fair...'&&0
      elseif %which% == 3
        %echo% &&m~%self% chants, 'Iris of the rainbow, I implore thee...'&&0
      elseif %which% == 4
        %echo% &&m~%self% chants, 'Annihilate all who stand before me!'&&0
      else
        %echo% &&m**** ~%self% shouts, 'Victory is mine at last... RAINBOW BEAM!' ****&&0
      end
    end
    if %cycle% < 5
      %echo% &&m**** She's still chanting... ****&&0 (interrupt)
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
  nop %self.remove_mob_flag(NO-ATTACK)%
  if %pain% > 0
    wait 1
    skyfight clear dodge
    %echo% &&m**** A blinding rainbow forms in the air over the iridescent sigil, spinning like a wild ribbon.... ****&&0 (dodge)
    set cycle 1
    eval wait 10 - %diff%
    while %cycle% <= %diff%
      skyfight setup dodge all
      wait %wait% s
      eval ouch %diff% * 20 * %pain%
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if !%ch.var(did_sfdodge)%
            set hit 1
            %echo% &&mA rainbow streamer cuts through ~%ch%!&&0
            if %diff% > 1 && %cycle% == %diff%
              %send% %ch% You're blinded! All you can see is rainbows!
              dg_affect #11841 %ch% BLIND on 5
            end
            %damage% %ch% %ouch% magical
          elseif %ch.is_pc%
            %send% %ch% &&mYou narrowly avoid a rainbow beam!&&0
            if %diff% == 1
              dg_affect #11856 %ch% TO-HIT 25 20
            end
          end
          if %cycle% < %diff%
            %send% %ch% &&m**** The rainbow is still spinning wildly... ****&&0 (dodge)
          end
        end
        set ch %next_ch%
      done
      skyfight clear dodge
      eval cycle %cycle% + 1
    done
    if !%hit% && %diff% == 1
      %echo% &&m~%self% is hit by a stray rainbow beam!&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
    wait 10 s
elseif %move% == 4
  * Summon Mercs
  %subecho% %room% &&y~%self% shouts, 'I could use some help in the Eruditorium!'&&0
  if %diff% == 1
    wait 5 sec
    %echo% &&m...nobody seems to respond to Lady Virduke's call.&&0
  else
    wait 1
    if %random.2% == 1
      set vnum 11842
    else
      set vnum 11846
    end
    set ally %instance.mob(%vnum%)%
    if %ally% && !%ally.disabled% && !%ally.fighting%
      %at% %ally.room% %echo% ~%ally% heads to assist Lady Virduke.
      %teleport% %ally% %self.room%
      %echo% &&m~%ally% rushes in!&&0
      %force% %ally% mkill %actor%
    end
  end
end
~
#11848
Bleak Rojjer merc-ssassin combat: Shadow Assassin, Venomous Jab, Complete Darkness, Pin the Shadow, Summon Mercs~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4 5
  set num_left 5
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
* perform move
skyfight lockout 30 35
if %move% == 1
  * Shadow Assassin
  skyfight clear dodge
  %echo% &&m|%self% shadow salutes *%self% and then vanishes into the dark corners of the lab...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 1
  set cycle 1
  eval wait 8 - %diff%
  set targ %random.enemy%
  while %targ% && %cycle% <= (2 * %diff%)
    set targ_id %targ.id%
    skyfight setup dodge %targ%
    %send% %targ% &&m**** The hairs on your neck stand up... ****&&0 (dodge)
    wait %wait% s
    if %targ.id% != %targ_id% || %targ.position% == Dead
      * gone
    elseif %targ.var(did_sfdodge)%
      %echo% &&mA shadow assasssin slices through the air, just past ~%targ%, and vanishes as it hits the floor!&&0
      if %diff% == 1
         dg_affect #11856 %targ% off
         dg_affect #11856 %targ% TO-HIT 25 20
     end
    else
      * hit
      %echo% &&mA shadow assassin stabs from the dark, cutting into ~%targ% out of nowhere!&&0
      %damage% %targ% 200 physical
      if %diff% > 2
        %send% %targ% &&mYou're knocked off balance by the surprise attack!&&0
        dg_affect #11818 %targ% TO-HIT -15 30
        dg_affect #11818 %targ% DODGE -15 30
      end
    end
    skyfight clear dodge
    eval cycle %cycle% + 1
    set targ %random.enemy%
  done
  wait 8 s
elseif %move% == 2 && !%self.aff_flagged(BLIND)%
  * Venomous Jab
  skyfight clear dodge
  set targ %actor%
  set id %targ.id%
  if %diff% < 3
    if %diff% == 1
      nop %self.add_mob_flag(NO-ATTACK)%
    end
    %send% %targ% &&m**** You notice ~%self% pull a knife from ^%self% belt with ^%self% free hand... ****&&0 (dodge)
    skyfight setup dodge %targ%
    wait 5 s
    nop %self.remove_mob_flag(NO-ATTACK)%
    wait 3 s
    if %self.disabled%
      halt
    end
    if !%targ% || %targ.id% != %id%
      * gone
      unset targ
    elseif %targ.var(did_sfdodge)%
      %send% %targ% &&mYou bend out of the way as ~%self% tries to jab you with the knife!&&0
      %echoaround% %targ% ~%targ% bends out of the way just as ~%self% tries to jab *%targ% with a knife!
      if %diff% == 1
        dg_affect #11856 %targ% TO-HIT 25 20
      end
      unset targ
    end
  end
  if %targ%
    * hit
    %send% %targ% &&m~%self% stabs you with a small knife held in ^%self% free hand!&&0
    %echoaround% %targ% &&m~%self% stabs ~%targ% with a small knife held in ^%self% free hand!&&0
    if %diff% > 1
      %send% %targ% &&mThe wound burns! The knife must have been coated in venom...&&0
      %echoaround% %targ% &&m~%targ% looks seriously ill!&&0
      %dot% #11858 %targ% 200 15 poison
    end
    %damage% %targ% 100 physical
  end
  skyfight clear dodge
elseif %move% == 3
  * Complete Darkness
  skyfight clear interrupt
  if %diff% == 1
    * normal: prevent his attack      -> TODO change this to a debuff?
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup interrupt all
  %echo% &&m**** &&Z~%self% closes ^%self% eyes and every light in the room starts to dim... ****&&0 (interrupt)
  wait 8 s
  if %self.disabled%
    skyfight clear interrupt
    halt
  end
  if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
    set broke 1
    set ch %room.people%
    while %ch%
      if %ch.var(did_sfinterrupt,0)%
        %send% %ch% &&mYou manage to throw a broken toaster at ~%self% at just the right second!&&0
      end
      set ch %ch.next_in_room%
    done
    %echo% &&m~%self% is hit in the head and loses focus... the lights start to glow again.&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
  else
    %echo% &&m|%self% eyes snap open, and you are suddenly plunged into complete darkness!&&0
    set ch %room.people%
    while %ch%
      if %self.is_enemy(%ch%)%
        if %ch.ability(Darkness)%
          %send% %ch% Luckily you can still see!
        else
          dg_affect #11860 %ch% BLIND on 25
        end
      end
      set ch %ch.next_in_room%
    done
  end
elseif %move% == 4
  * Pin the Shadow
  skyfight clear free
  skyfight clear struggle
  %echo% &&m~%self% draws a dagger made of shadows from the folds of ^%self% cloak...&&0
  wait 3 s
  if %self.disabled%
    halt
  end
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  if %self.fighting% == %targ% && %diff% < 4
    dg_affect #11845 %self% HARD-STUNNED on 20
  end
  %send% %targ% &&m~%self% throws the shadow dagger at you, but it passes through you and strikes your shadow!&&0
  %echoaround% %targ% &&m~%self% throws the shadow dagger at ~%actor%, but it passes through *%actor% and strikes ^%actor% shadow!&&0
  if %diff% <= 2 || (%self.level% + 100) <= %targ.level%
    %send% %targ% &&m**** The dagger has you stuck fast... You can't move! ****&&0 (struggle)
    %echoaround% %targ% &&m~%targ% suddenly freezes!&&0
    skyfight setup struggle %targ% 20
    set bug %targ.inventory(11890)%
    if %bug%
      set strug_char You struggle to try to get to the shadow dagger...
      set strug_room ~%%actor%% struggles to get to the shadow dagger...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You manage to reach the shadow dagger and pull it out!
      set free_room ~%%actor%% manages to pull out the shadow dagger!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    wait 20 s
    skyfight clear struggle
  else
    %send% %targ% &&mThe dagger has you stuck fast... You can't move!&&0
    %echoaround% %targ% &&m**** &&Z~%targ% suddenly freezes! You have to pull the dagger out! ****&&0 (free %targ.pc_name.car%)
    skyfight setup free %targ%
    eval time %diff% * 15
    dg_affect #11861 %targ% HARD-STUNNED on %time%
    if %diff% == 4
      dg_affect #11861 %targ% DODGE -999 %time%
      dg_affect #11861 %targ% BLOCK -999 %time%
    end
    * wait and clear
    set done 0
    while !%done% && %time% > 0
      wait 5 s
      eval time %time% - 5
      if %targ_id% != %targ.id%
        set done 1
      elseif !%targ.affect(11861)%
        set done 1
      end
    done
    skyfight clear free
    dg_affect #11861 %targ% off
  end
  dg_affect #11845 %self% off
elseif %move% == 5
  * Summon Mercs
  %subecho% %room% &&y~%self% shouts, 'In 'ere you lots!'&&0
  if %diff% == 1
    wait 5 sec
    %echo% &&m...nobody seems to respond to Rojjer's call.&&0
  else
    wait 1
    if %random.2% == 1
      set vnum 11841
    else
      set vnum 11845
    end
    set ally %instance.mob(%vnum%)%
    if %ally% && !%ally.disabled% && !%ally.fighting%
      %at% %ally.room% %echo% ~%ally% heads to see what Rojjer wants.
      %teleport% %ally% %self.room%
      %echo% &&m~%ally% runs in!&&0
      %force% %ally% mkill %actor%
    end
  end
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11849
Trixton Vye Mercenary Leader combat: Hurricane Stone, Dancing Cane, Deathstone, Corpse's Grasp, Animate Corpse~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4 5 5
  set num_left 6
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
* perform move
skyfight lockout 30 35
if %move% == 1
  * Hurricane Stone
  %echo% &&m**** ~%self% pulls a glowing gray stone from ^%self% pocket and holds it out... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear interrupt
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  set needed %room.players_present%
  if %needed% > 4
    set needed 4
  end
  while !%broke% && %cycle% < 5
    wait 4 s
    if %self.sfinterrupt_count% >= %needed%
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou manage to interrupt the mercenary!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&m~%self% fumbles the hurricane stone.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
        wait 10 s
      end
    else
      %echo% &&mHurricane winds whip around the room, throwing coffins and tools into the air!&&0
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          %echo% &&mFlying debris slams into ~%ch%!&&0
          %damage% %ch% 100 physical
          if %cycle% == 4 && %diff% == 4 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
            dg_affect #11851 %ch% STUNNED on 10
          end
        end
        set ch %next_ch%
      done
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 2
  * Dancing Cane
  skyfight clear dodge
  %echo% &&m~%self% laughs as he draws a slender sword from out of his cane and tosses it into the air!&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 3 s
  %echo% &&m**** The cane sword dances through the air... and it's dancing right toward you! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          set hit 1
          %echo% &&mThe dancing cane sword slashes at ~%ch%!&&0
          if %diff% > 2
            %dot% #11842 %ch% 50 20 physical 4
          end
          %damage% %ch% 80 physical
        elseif %ch.is_pc%
          %send% %ch% &&mYou manage to dodge the dancing cane sword!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&m**** The cane sword is still dancing through the air! ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  if !%hit%
    if %diff% < 3
      %echo% &&m~%self% stoops to retrieve the cane sword as it falls out of the air.&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
elseif %move% == 3
  * Deathstone
  %echo% &&m**** ~%self% draws an inscribed rock from ^%self% pocket and bites down on it... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear interrupt
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  set needed %room.players_present%
  if %needed% > 4
    set needed 4
  end
  eval heal %diff% * 15
  while !%broke% && %cycle% < 5
    wait 4 s
    if %self.sfinterrupt_count% >= %needed%
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou manage to interrupt the mercenary!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&m~%self% drops the deathstone.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
        wait 10 s
      end
    else
      %echo% &&mThe lamps flicker as an icy chill drains all the heat from the room!&&0
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          %echo% &&m~%self% seems invigorated as he saps life from ~%ch%!&&0
          if %diff% > 1
            %heal% %self% health %heal%
          end
          %damage% %ch% 100 physical
        end
        set ch %next_ch%
      done
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 4
  * Corpse's Grasp
  skyfight clear struggle
  %echo% &&m~%self% pulls an inky black sphere from his pocket and holds it up...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 sec
  %echo% &&m**** The caskets creak as corpses lurch out of them... One grabs onto you! ****&&0 (struggle)
  skyfight setup struggle all 20
  set ch %room.people%
  while %ch%
    set bug %ch.inventory(11890)%
    if %bug%
      set strug_char You struggle to escape the corpse's grasp...
      set strug_room ~%%actor%% struggles against the corpse's grasp...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You manage to slip out of the corpse's grasp!
      set free_room ~%%actor%% manages to slip out of the corpse's grasp!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    set ch %ch.next_in_room%
  done
  * messages
  set cycle 0
  while %cycle% < 4
    wait 5 s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %ch.affect(11822)%
        if %cycle% == 3
          * final
          %send% %ch% &&mThe corpse loses its hold on you and falls back into the casket, dead.&&0
          dg_affect #11822 %ch% off silent
          * penalty
          dg_affect #11850 %ch% SLOW on 25
        else
          %send% %ch% &&m**** You have to get free of the corpse's grasp! ****&&0 (struggle)
        end
      end
      set ch %next_ch%
    done
    eval cycle %cycle% + 1
  done
  nop %self.remove_mob_flag(NO-ATTACK)%
  skyfight clear struggle
elseif %move% == 5
  * Animate Corpse
  nop %self.set_cooldown(11800,5)%
  if %diff% == 1
    halt
  end
  %echo% &&m~%self% tosses a vial toward an open coffin -- it shatters all over the corpse!&&0
  wait 2 s
  eval lev %self.level% - 50
  %load% mob 11850 ally %lev%
  set mob %room.people%
  if %mob.vnum% == 11850
    %echo% &&mA corpse climbs out of its coffin and joins the fray!&&0
    %force% %mob% mkill %self.fighting%
  end
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11850
Skycleave: Use magical items~
1 c 2
use~
if %actor.obj_target_inv(%arg.car%)% != %self%
  return 0
  halt
end
return 1
set arg %arg.cdr%
if !%arg%
  set targ %actor.fighting%
else
  set targ %actor.char_target(%arg.car%)%
  if !%targ%
    %send% %actor% You don't see %arg.car% here.
    halt
  elseif !%actor.is_enemy(%targ%)%
    %send% %actor% You can't use that on *%targ% right now.
    halt
  elseif %targ.aff_flagged(!ATTACK)%
    %send% %actor% You can't use that on *%targ%.
    halt
  end
end
* vnum-based
switch %self.vnum%
  case 11864
    * mana deconfabulator: counterspell
    if %actor.fighting%
      if %actor.fighting.is_pc%
        %send% %actor% You can't use @%self% in PVP.
        halt
      end
    end
    %send% %actor% You seem to have broken @%self% but not before it protects you with some kind of counterspell!
    %echoaround% %actor% ~%actor% breaks @%self% and starts to glow, slightly.
    dg_affect #3021 %actor% MANA-REGEN -1 1800
    %purge% %self%
    * do not fall through
    halt
  break
  case 11865
    * ether condenser: slow and lower to-hit
    if !%targ%
      %send% %actor% You're not even fighting anybody.
      halt
    elseif %targ.is_pc%
      %send% %actor% You can't use @%self% in PVP.
      halt
    end
    %send% %actor% You hurl @%self% at ~%targ%... it hits *%targ% in the head!
    dg_affect #11865 %targ% SLOW on 30
    dg_affect #11865 %targ% TO-HIT -50 30
  break
  case 11867
    * dynamic reconfabulator: stuns mob
    if !%targ%
      %send% %actor% You're not even fighting anybody.
      halt
    elseif %targ.is_pc%
      %send% %actor% You can't use @%self% in PVP.
      halt
    end
    %send% %actor% You throw @%self% at ~%targ%... it hits *%targ% in the head!
    if !%targ.aff_flagged(!STUN)%
      %echoaround% %targ% ...&%targ% seems stunned.
      dg_affect #11851 %targ% STUNNED on 9
    end
  break
done
* if we made it this far, ensure we're fighting
if !%targ.fighting%
  %force% %targ% maggro %actor%
end
%purge% %self%
~
#11851
Skycleave: Shared mob speech trigger~
0 d 1
zenith Maureen Eloise Heather Alastair Marina John Wright Dylane Ametnik Boghylda mice mouse waltur~
switch %self.vnum%
  case 11902
    * bucket
    if %speech% ~= Maureen
      wait 1
      %echo% You hear a soft sobbing sound as the bucket slowly refills.
    end
  break
  case 11903
    * sponge
    if %speech% ~= Eloise
      wait 1
      %echo% The sponge seems to sob for a moment, then squeezes itself out.
    end
  break
  case 11921
    * broom
    if %speech% ~= Heather
      wait 1
      %echo% The broom drops a little and makes a sniffling noise.
    end
  break
  case 11922
    * duster
      wait 1
    if %speech% ~= Alastair
      %echo% The feather duster makes a sobbing noise but then shakes itself out and goes back to cleaning.
    elseif %speech% ~= Marina
      %echo% The feather duster sounds like it's crying as it slowly cleans around you.
    end
  break
  case 11926
    * sponge wrangler
    if %speech% ~= John || %speech% ~= Wright
      wait 1
      %echo% The sponge sits in one of the cages and grumbles.
    end
  break
  case 11933
    * mop
    if %speech% ~= Dylane
      wait 1
      if %actor.room% == %self.room%
        %send% %actor% The mop marches over to you.
        %echoaround% %actor% The mop marches over to ~%actor%.
        if %actor.on_quest(11801)%
          %quest% %actor% trigger 11801
        end
      end
    end
  break
  case 11960
    * filing cabinet
    if %speech% ~= Ametnik
      wait 1
      if %actor.room% == %self.room%
        %send% %actor% The filing cabient turns toward you.
        %echoaround% %actor% The filing cabinet turns toward ~%actor%.
      end
    end
  break
  case 11961
    * bookshelf
    if %speech% ~= Boghylda
      wait 1
      if %actor.room% == %self.room%
        %send% %actor% The bookshelf waddles over to you.
        %echoaround% %actor% The bookshelf waddles over to ~%actor%.
      end
    end
  break
  case 11825
  case 11925
    * Ravinders
    if %speech% ~= zenith
      wait 1
      emote $n winces.
    end
  break
  case 11919
    * Annelise
    wait 1
    if %speech% ~= mice
      say Oh deary, did you say mice?
    elseif %speech% ~= mouse
      say Oh deary, did you say mouse?
    elseif %speech% ~= waltur
      emote seems to have started crying.
    end
  break
  case 11939
    * Annelise
    wait 1
    if %speech% ~= mice
      say Oh deary, did you say mice?
    elseif %speech% ~= mouse
      say Oh deary, did you say mouse?
    end
  break
done
~
#11852
Skycleave: Single-try pickpocket~
0 p 100
~
return 1
* Only allows each player to attempt to pickpocket this mob 1 time
if %ability% != 142
  * not pickpocket ability
  halt
elseif %self.mob_flagged(*PICKPOCKETED)%
  * done already or can't see them
  halt
elseif %self.vnum% == 11920
  * special cast for the GHSs: stored to room not mob (also ignores can-see)
  set room %self.room%
  set varname pickpocket_%actor.id%
  if %room.var(%varname%,0)%
    %send% %actor% ~%self% is watching you closely; you can't get close enough to pick ^%self% pocket.
    return 0
  else
    set %varname% 1
    remote %varname% %room.id%
  end
elseif !%self.can_see(%actor%)%
  * done already or can't see them
  halt
end
set varname pickpocket_%actor.id%
if %self.varexists(pickpocket_%actor.id%)%
  * tried this before
  switch %self.vnum%
    default
      %send% %actor% ~%self% is watching you now; you can't get close enough to pick ^%self% pocket.
    break
  done
  return 0
else
  * ok to try
  set %varname% 1
  remote %varname% %self.id%
end
~
#11853
Escaped Otherworlder fight: Arm Grapple, Battle Form, Laser Clap~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3
  set num_left 3
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
* perform move
skyfight lockout 25 30
if %move% == 1
  * Arm Grapple
  set time 30
  if %diff% == 1
    dg_affect #11852 %self% HARD-STUNNED on %time%
  end
  skyfight clear struggle
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  %send% %targ% &&m**** &&Z~%self% rushes up to you and grapples you with its long arms! ****&&0 (struggle)
  %echoaround% %targ% &&m~%self% wraps its long arms around ~%targ%!&&0
  skyfight setup struggle %targ% %time%
  set bug %targ.inventory(11890)%
  if %bug%
    set strug_char You try to struggle out of the otherworlder's arms...
    set strug_room ~%%actor%% struggles to get out of the otherworlder's arms...
    remote strug_char %bug.id%
    remote strug_room %bug.id%
    set free_char You manage to wriggle out of the otherworlder's arms!
    set free_room ~%%actor%% manages to wriggle out of the otherworlder's arms!
    remote free_char %bug.id%
    remote free_room %bug.id%
  end
  while %time% > 0
    eval time %time% - 4
    wait 4 s
    if !%targ% || %targ.id% != %targ_id%
      set time 0
    elseif !%targ.affect(11822)%
      set time 0
    else
      %send% %targ% &&m**** You're caught tight in the otherworlder's arms! ****&&0 (struggle)
    end
  done
  skyfight clear struggle
  dg_affect #11852 %self% off
elseif %move% == 2
  * Battle Form
  set f_list 11829 11830 11835 11836
  if %self.morph% == 11831 || %self.morph% == 11837
    set form 0
  elseif %f_list% ~= %self.morph%
    eval form %self.morph% + 1
  elseif %diff% == 1
    set form 11829
  else
    set form 11835
  end
  if !%form%
    nop %self.set_cooldown(11800,0)%
    halt
  end
  * will morph
  skyfight clear interrupt
  %echo% &&m**** &&Z~%self% begins burbling and growing... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup interrupt all
  wait 4 s
  if %self.var(sfinterrupt_count,0)% < (%diff% + 1) / 2
    %echo% &&m**** &&Z~%self% twists and contorts as it grows... ****&&0 (interrupt)
  end
  wait 4 s
  if %self.disabled% || %self.var(sfinterrupt_count,0)% >= (%diff% + 1) / 2
    %echo% &&m~%self% is interrupted and shrinks back to its previous shape!&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
  else
    set old %self.name%
    %morph% %self% %form%
    %echo% &&m%old% has grown into ~%self%!&&0
  end
  skyfight clear interrupt
elseif %move% == 3 && !%self.aff_flagged(BLIND)%
  * Laser Clap
  skyfight clear dodge
  %echo% &&m~%self% holds its arms wide like it's preparing to clap its hands...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 1
  set cycle 1
  eval wait 8 - %diff%
  set last 0
  set targ %random.enemy%
  while %cycle% <= (2 * %diff%) && %targ%
    set targ_id %targ.id%
    skyfight setup dodge %targ%
    if %targ% == %last%
      %send% %targ% &&m**** Looks like ~%self% is going to clap at you again... ****&&0 (dodge)
    else
      %send% %targ% &&m**** &&Z~%self% turns toward you with its arms outstretched... ****&&0 (dodge)
    end
    set last %targ%
    wait %wait% s
    if %self.disabled% || %targ.id% != %targ_id% || %targ.position% == Dead
      * gone
    elseif %targ.var(did_sfdodge)%
      %echo% &&m~%self% claps its hands together, blasting a beam of light that narrowly misses ~%targ%!&&0
      if %diff% == 1
        dg_affect #11856 %targ% TO-HIT 25 20
      end
    else
      * hit
      %echo% &&m~%self% claps its hands together, blasting a beam of light that hits ~%targ% square in the chest!&&0
      %damage% %targ% 200 direct
    end
    skyfight clear dodge
    eval cycle %cycle% + 1
    set targ %random.enemy%
  done
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11854
Skithe Ler-Wyn combat: Echoes of Dancing Cane, Slay the Griffin, Chain Lightning, Freezing Air~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4
  set num_left 4
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
* perform move
skyfight lockout 30 35
if %move% == 1
  * Dancing Cane
  skyfight clear dodge
  %echo% &&mTrixton Vye laughs as he draws a slender sword from out of his cane and tosses it into the vortex!&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 3 s
  %echo% &&m**** The cane sword dances through the vortex... and it's dancing right toward you! ****&&0 (dodge)
  set cycle 1
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          %echo% &&mThe dancing cane sword slashes at ~%ch%!&&0
          if %diff% > 2
            %dot% #11842 %ch% 50 20 physical 4
          end
          %damage% %ch% 100 physical
        elseif %ch.is_pc%
          %send% %ch% &&mYou manage to dodge the dancing cane sword!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&m**** The cane sword is still dancing through the vortex! ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
elseif %move% == 2
  * Slay the Griffin / Fan of Knives
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  %echo% &&yA tiny voice in the vortex shouts, 'Slay the Griffin!'&&0
  %echo% &&m**** The goblin starts pulling out knives and throwing them in all directions! ****&&0 (dodge)
  skyfight setup dodge all
  set cycle 0
  eval max (%diff% + 2) / 2
  while %cycle% < %max%
    wait 5 s
    if %self.disabled%
      nop %self.remove_mob_flag(NO-ATTACK)%
      halt
    end
    set this 0
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if %ch.var(did_sfdodge)%
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        else
          set this 1
          %echo% &&mA throwing knife slices through the air and straight into |%ch% chest!&&0
          %dot% #11811 %ch% 80 10 physical 3
          %damage% %ch% 100 physical
        end
      end
      set ch %next_ch%
    done
    if !%this%
      %echo% &&mStray knives fly off into the vortex.&&0
    end
    eval cycle %cycle% + 1
  done
  skyfight clear dodge
elseif %move% == 3
  * Chain Lightning
  %echo% &&m**** Kara Virduke holds out her staff as sparks crackle across the vortex... ****&&0 (interrupt and dodge)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  skyfight clear interrupt
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  while !%broke% && %cycle% <= 4
    skyfight setup dodge all
    wait 4 s
    if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou manage to interrupt Skithe Ler-Wyn before another chain of lightning!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&mKara Virduke seems momentarily distracted.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
        wait 10 s
      end
    else
      %echo% &&mKara Virduke's calamander staff hisses and cracks as it focuses the lightning into a chain...&&0
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if %ch.var(did_sfdodge)%
            %send% %ch% &&mYou manage to narrowly dodge the chain lightning!&&0
            if %diff% == 1
              dg_affect #11856 %ch% off
              dg_affect #11856 %ch% TO-HIT 25 20
            end
          elseif %ch.trigger_counterspell%
            %echo% &&m|%ch% counterspell deflects a bolt of lightning toward ~%self%!&&0
            if %diff% == 1
              %damage% %self% 100 magical
            end
          else
            * hit
            %echo% &&mThe chain lightning hits ~%ch%!&&0
            %damage% %ch% 120 physical
            if %cycle% == 4 && %diff% == 4 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
              dg_affect #11851 %ch% STUNNED on 10
            end
          end
        end
        set ch %next_ch%
      done
    end
    if !%broke% && %cycle% < 4
      %echo% &&m**** Here comes more chain lightning... ****&&0 (interrupt and dodge)
    end
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 4
  * Freezing Air
  skyfight clear interrupt
  %echo% &&yA voice beyond the vortex shouts, 'By the power of Skycleave!'&&0
  wait 3 sec
  set targ %self.fighting%
  set id %targ.id%
  %echo% &&m**** A gnarled wand appears high in the air and the vortex starts to freeze FAST... ****&&0 (interrupt)
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  while !%broke% && %cycle% < 5
    wait 4 s
    if %self.disabled%
      halt
    end
    if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou manage to interrupt Skithe Ler-Wyn!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&mThe gnarled wand is stopped before it can finish the freezing spell.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
      end
    else
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          * hit!
          %send% %ch% &&mThe freezing air stings your skin, eyes, and lungs!&&0
          %echoaround% %ch% &&m~%ch% cries out as the air freezes *%ch%!&&0
          %damage% %ch% 100 magical
        end
        set ch %next_ch%
      done
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11855
Mezvienne combat: Baleful Polymorph, Blinding Light of Dawn, Belt of Venus, Dark Fate~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4
  set num_left 4
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
* perform move
skyfight lockout 30 35
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Baleful Polymorph
  skyfight clear dodge
  set targ %random.enemy%
  if !%targ%
    set targ %actor%
  end
  if %diff% == 1
    set vnum 11854
  else
    set vnum 11855
  end
  if %targ.morph% == %vnum%
    * already morphed
    nop %self.set_cooldown(11800,0)%
    halt
  end
  * wait?
  set targ_id %targ.id%
  set fail 0
  if %diff% <= 2
    %send% %targ% &&m**** &&Z~%self% waves her hands... An eerie green light casts out toward you... ****&&0 (dodge)
    %echoaround% %targ% &&m~%self% waves her hands... An eerie green light casts out toward ~%targ%...&&0
    skyfight setup dodge %targ%
    wait 8 s
    if %self.disabled%
      halt
    end
    if !%targ% || %targ.id% != %targ_id%
      * lost targ
      set fail 1
    elseif %targ.var(did_sfdodge)%
      set fail 1
      %echo% &&mThe green light scatters into the distance as Mezvienne's spell misses.&&0
      if %diff% == 1
        dg_affect #11856 %targ% TO-HIT 25 20
      end
    else
      %send% %targ% &&mYou are enveloped in the green light...&&0
    end
  else
    %echo% &&m~%self% waves her hands as an eerie green light casts out over the tower...&&0
  end
  if !%fail%
    if %targ.trigger_counterspell%
      set fail 2
      %echo% &&mThe green light rebounds and smacks ~%self% in the face -- she looks stunned!&&0
    end
  end
  if !%fail%
    set old_shortdesc %targ.name%
    %morph% %targ% %vnum%
    %echoaround% %targ% &&m%old_shortdesc% is suddenly transformed into ~%targ%!&&0
    %send% %targ% &&m**** You are suddenly transformed into %targ.name%! ****&&0 (fastmorph normal)
    if %diff% == 4 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
      dg_affect #11851 %targ% STUNNED on 5
    elseif %diff% >= 2
      nop %targ.command_lag(ABILITY)%
    end
  elseif %fail% == 2 || %diff% == 1
    dg_affect #11852 %self% HARD-STUNNED on 10
  end
  skyfight clear dodge
elseif %move% == 2
  * Blinding Light of Dawn
  %echo% &&m~%self% flies up and begins channeling the dawn...&&0
  %echo% &&m**** A bright light erupts from the sky, swirling past Mezvienne... ****&&0 (interrupt)
  if %diff% == 1
    * normal: prevent her attack
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear interrupt
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  while !%broke% && %cycle% < 5
    wait 4 s
    if %self.disabled%
      halt
    end
    if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou trick the enchantress into shining the light at the fountain...&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&m~%self% seems distracted as the blinding light of dawn ricochets off the hendecagon fountain and strikes her in the face!&&0
      eval amount 60 / %diff%
      %damage% %self% %amount% magical
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
      end
    else
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        eval skip %%skip_%ch.id%%%
        if !%skip% && %self.is_enemy(%ch%)%
          if %ch.trigger_counterspell%
            set skip_%ch.id% 1
            %send% %ch% &&mA shield forms in front of you as you masterfully counterspell the blinding light of dawn!&&0
            %echoaround% %ch% &&mA shield forms in front of ~%ch% as &%ch% masterfully counterspells the blinding light!&&0
            eval sfinterrupt_count %self.var(sfinterrupt_count,0)% + 1
            remote sfinterrupt_count %self.id%
          else
            * hit and no counterspell
            %send% %ch% &&mThe blinding light of dawn burns you!&&0
            %echoaround% %ch% &&m~%ch% recoils as the light burns *%ch%!&&0
            %damage% %ch% 100 magical
            if %cycle% == 4 && %diff% == 4
              dg_affect #11841 %ch% BLIND on 10
            end
          end
        end
        set ch %next_ch%
      done
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 3
  * Belt of Venus / rosy pink light
  skyfight clear dodge
  %echo% &&m~%self% spins in the air as rosy pink mana streaming off of the heartwood of time whips around her...&&0
  if %diff% == 1
    * normal: prevent her attack
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 1 s
  %echo% &&m**** The ribbon of rosy pink light streams toward you... ****&&0 (dodge)
  wait 8 s
  set ch %room.people%
  set any 0
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)%
      if %ch.var(did_sfdodge)%
        if %diff% == 1
          dg_affect #11856 %ch% TO-HIT 25 20
        end
      else
        set any 1
        %echo% &&mThe rosy pink light strikes ~%ch% in the chest and streams right through *%ch%!&&0
        if %diff% == 4 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
          dg_affect #11851 %ch% STUNNED on 10
        end
        if %diff% >= 3
          dg_affect #11857 %ch% SLOW on 20
        end
        if %diff% >= 2
          eval amount %diff% * 20
          dg_affect #11857 %ch% TO-HIT -%amount% 20
        end
      end
    end
    set ch %next_ch%
  done
  if !%any%
    %echo% &&mThe rosy pink light of the belt of Venus swirls back and hits Mezvienne herself!&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 10
    else
      eval amount 100 - (%diff% * 20)
      dg_affect #11854 %self% DODGE -%amount% 10
    end
  end
  skyfight clear dodge
elseif %move% == 4
  * Dark Fate
  skyfight clear struggle
  %echo% &&m~%self% rises up into the air and begins plucking unseen strands...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 sec
  %echo% &&m**** You are gripped by a dark fate! ****&&0 (struggle)
  skyfight setup struggle all 20
  set ch %room.people%
  while %ch%
    set bug %ch.inventory(11890)%
    if %bug%
      set strug_char You struggle against your dark fate...
      set strug_room ~%%actor%% struggles against ^%%actor%% dark fate...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You struggle out of your dark fate!
      set free_room ~%%actor%% manages to free *%%actor%%self!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    set ch %ch.next_in_room%
  done
  * damage
  if %diff% > 1
    set cycle 0
    while %cycle% < 5
      wait 4 s
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %ch.affect(11822)%
          %send% %ch% &&m**** You burn from the inside out as you face your dark fate! ****&&0 (struggle)
          %damage% %ch% 100 magical
        end
        set ch %next_ch%
      eval cycle %cycle% + 1
    done
  else
    wait 10 s
    nop %self.remove_mob_flag(NO-ATTACK)%
    wait 10 s
  end
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11856
Shadow Ascendant fight: Shadow Cage, Shadow Torrent, Freezing Air, Shadow Slice~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set rm %self.room%
set diff %self.diff%
set m_l %self.var(m_l)%
set n_m %self.var(n_m,0)%
if !%m_l% || !%n_m%
  set m_l 1 2 3 4
  set n_m 4
end
eval which %%random.%n_m%%%
set old %m_l%
set m_l
set move 0
while %which% > 0
  set move %old.car%
  if %which% != 1
    set m_l %m_l% %move%
  end
  set old %old.cdr%
  eval which %which% - 1
done
set m_l %m_l% %old%
eval n_m %n_m% - 1
remote m_l %self.id%
remote n_m %self.id%
skyfight lockout 30 35
if %move% == 1
  skyfight clear free
  skyfight clear struggle
  %echo% &&mThe Shadow seems to be growing...&&0
  wait 3 s
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  if %self.fighting% == %targ% && %diff% < 4
    dg_affect #11852 %self% HARD-STUNNED on 20
  end
  if %targ.trigger_counterspell%
    %echo% &&m~%self% sparks with primordial energy as it consumes |%targ% counterspell!&&0
    dg_affect #11864 %self% BONUS-MAGICAL 10 -1
  end
  if %diff% <= 2 || (%self.level% + 100) <= %targ.level% || %rm.players_present% == 1
    %send% %targ% &&m**** The Shadow envelops you like a cage! You have to break free! ****&&0 (struggle)
    %echoaround% %targ% &&mThe Shadow envelops ~%targ%, trapping *%targ%!&&0
    skyfight setup struggle %targ% 20
    set bug %targ.inventory(11890)%
    if %bug%
      set strug_char You try to get free out of the shadow cage...
      set strug_room You hear ~%%actor%% trying to get free of the shadow cage...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You fight your way out of the shadow cage!
      set free_room ~%%actor%% manages to get out of the shadow cage!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    set time 20
    while %time% > 0
      wait 5 s
      if %targ.affect(11822)%
        %send% %targ% &&m**** You have to break free of the cage! ****&&0 (struggle)
        eval time %time% - 5
      else
        set time 0
      end
    done
    skyfight clear struggle
  else
    %send% %targ% &&mThe Shadow envelops you like a cage! There's nothing you can do!&&0
    %echoaround% %targ% &&m**** The Shadow envelops ~%targ% like a cage, trapping *%targ%! ****&&0 (free %targ.pc_name.car%)
    skyfight setup free %targ%
    eval time %diff% * 15
    dg_affect #11863 %targ% HARD-STUNNED on %time%
    set done 0
    while !%done% && %time% > 0
      wait 5 s
      eval time %time% - 5
      if %targ_id% != %targ.id%
        set done 1
      elseif !%targ.affect(11863)%
        set done 1
      end
    done
    skyfight clear free
  end
  dg_affect #11852 %self% off
elseif %move% == 2
  %echo% &&mThe Shadow cracks and swirls as the office seems to darken...&&0
  %echo% &&m**** It seems to be drawing smaller shadows into itself! ****&&0 (interrupt and dodge)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  skyfight clear interrupt
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  while !%broke% && %cycle% <= 4
    skyfight setup dodge all
    wait 4 s
    if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
      set broke 1
      set ch %rm.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou manage to interrupt the Ascendant before another shadow torrent!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&mThe Shadow seems distracted, if only for a moment.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
        wait 10 s
      elseif %diff% < 4
        dg_affect #11852 %self% HARD-STUNNED on 5
        wait 5 s
      end
    else
      %echo% &&mThe Shadow cracks and swirls as a torrent of smaller shadows stream into it from around the tower...&&0
      set ch %rm.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if %ch.var(did_sfdodge)%
            %send% %ch% &&mYou manage to narrowly dodge the streaming shadows!&&0
            if %diff% == 1
              dg_affect #11856 %ch% off
              dg_affect #11856 %ch% TO-HIT 25 20
            end
          else
            if %ch.trigger_counterspell%
              %echo% &&mThe Shadow Ascendant sparks with primordial energy as it consumes |%ch% counterspell!&&0
              dg_affect #11864 %self% BONUS-MAGICAL 10 -1
            end
            %echo% &&mA shadow cuts straight through ~%ch% as it streams into the Ascendant!&&0
            %damage% %ch% 120 physical
            if %cycle% == 4 && %diff% == 4 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
              dg_affect #11851 %ch% STUNNED on 10
            end
          end
        end
        set ch %next_ch%
      done
    end
    if !%broke% && %cycle% < 4
      %echo% &&m**** Here comes another shadow torrent... ****&&0 (interrupt and dodge)
    end
    eval cycle %cycle% + 1
  done
  skyfight clear dodge
  skyfight clear interrupt
elseif %move% == 3
  skyfight clear interrupt
  %subecho% %rm% &&y~%self% shouts, 'By the power of Skycleave!'&&0
  wait 3 sec
  set targ %self.fighting%
  set id %targ.id%
  %echo% &&m**** &&ZThe shadow holds its gnarled wand high and the air starts to freeze FAST... ****&&0 (interrupt)
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  while !%broke% && %cycle% < 5
    wait 4 s
    if %self.disabled%
      halt
    end
    if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
      set broke 1
      set ch %rm.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou manage to distract the Shadow by throwing knickknacks at it!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&m~%self% is hit with a knickknack; the air starts to warm back up as the spell breaks.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
      end
    else
      set ch %rm.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if %ch.trigger_counterspell%
            %echo% &&m~%self% sparks with primordial energy as it consumes |%ch% counterspell!&&0
            dg_affect #11864 %self% BONUS-MAGICAL 10 -1
          end
          %send% %ch% &&mThe freezing air stings your skin, eyes, and lungs!&&0
          %echoaround% %ch% &&m~%ch% cries out as the air freezes *%ch%!&&0
          %damage% %ch% 100 magical
        end
        set ch %next_ch%
      done
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 4
  skyfight clear dodge
  %echo% &&mThe Shadow lets out a low rumble as it twists and contorts...&&0
  wait 3 s
  %echo% &&m**** The Shadow's sinuous tendrils sharpen into a vicious axe! ****&&0 (dodge)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 8 s
  set hit 0
  set ch %rm.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)%
      if !%ch.var(did_sfdodge)%
        if %ch.trigger_counterspell%
          %echo% &&m~%self% sparks with primordial energy as it consumes |%ch% counterspell!&&0
          dg_affect #11864 %self% BONUS-MAGICAL 10 -1
        end
        set hit 1
        %echo% &&mThe shadow axe slices right through ~%ch%!&&0
        %damage% %ch% 200 magical
      elseif %ch.is_pc%
        %send% %ch% &&mYou narrowly avoid a slice from the shadow axe!&&0
        if %diff% == 1
          dg_affect #11856 %ch% TO-HIT 25 20
        end
      end
    end
    set ch %next_ch%
  done
  skyfight clear dodge
  if !%hit%
    if %diff% < 3
      %echo% &&mThe Shadow disperses for a second as it fails to slice through the stone wall!&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11857
Skycleave: Mercenary name setup~
0 nA 100
~
* Mercenaries are spawned by trig 11900/skymerc, called in 5 rooms.
* Up to 3 mercenaries spawn in each room, for a total of 15.
* Variable setup by merc vnum: Note that lookdescs are below in another switch
switch %self.vnum%
  case 11841
    * rogue mercenary
    set name_list Mac    Iniko Trixie Trusty  Shada  Foster Ol'    Brielle Cassina Sage
    set mid_list  the    0     0      Eleanor Redd   0      Gatlin 0       0       0
    set last_list Dagger 0     0      0       0      0      0      0       0       0
    set sex_list  male   male  female female  female male   male   female  male    female
    set name_size 10
    set verbiage is skulking around.
  break
  case 11842
    * caster mercenary
    set name_list Eris   Borak Shabina Sophia Magnus Regin Calista Beatrice Mirabel Booker
    set mid_list  Darke  0     Darke   0      0      Dall  Darke   0        0       0
    set last_list 0      0     0       0      0      0     0       0        0       0
    set sex_list  female male  female  female male   male  female  female   female  male
    set name_size 10
    set verbiage is studying the books.
  break
  case 11843
    * archer mercenary
    set name_list Alvin  Gunnar Rebel  Jack  Kerenza Florian Easy   Dakarai Huntley Winnow
    set mid_list  Greene 0      0      Swift 0       the     Maude  0       0       0
    set last_list 0      0      0      0     0       Tall    0      0       0       0
    set sex_list  male   male   female male female  male    female male    male    female
    set name_size 10
    set verbiage is fletching an arrow.
  break
  case 11844
    * nature mage mercenary
    set name_list Aella  Green  Faron Aiden Bright Briar Ariadne Aislinn Daphne Ellis Bruin
    set mid_list  0      Oriana 0     0     Blaise 0     0       0       0      0     0
    set last_list 0      0      0     0     0      0     0       0       0      0     0
    set sex_list  female female male  male  female male  female  female  female male  male
    set name_size 11
    set verbiage is playing with fire.
  break
  case 11845
    * vampire mercenary
    set name_list Baron Draco Lady   Jareth Countess Faye   Reina  Lucia  Sebastian Ledger
    set mid_list  Osman the   Raven  0      Calypso  0      0      0      of        0
    set last_list 0     White 0      0      0        0      0      0      0         0
    set sex_list  male  male  female male   female   female female female male      male
    set name_size 10
    set verbiage watches you carefully.
  break
  case 11846
    * armored mercenary
    set name_list Sir      George Lady   Edward Gellert Titania Mad    Wolf Saskia Ansel
    set mid_list  Humphrey 0      Roxie  0      Gorr    0       Mabel  0    0      0
    set last_list 0        0      0      0      0       0       0      0    0      0
    set sex_list  male     male   female male   male    female  female male female male
    set name_size 10
    set verbiage is spoiling for a fight.
  break
done
* pull count data (ensures sequential naming)
set spirit %instance.mob(11900)%
set varname merc%self.vnum%
if %spirit.varexists(%varname%)%
  eval %varname% %spirit.var(%varname%)% + 1
  eval pos %%%varname%%%
else
  set %varname% 1
  set pos 1
end
if %pos% > %name_size%
  set %varname% 1
  set pos 1
end
remote %varname% %spirit.id%
* choose name at random
set name Anonymo
set sex male
while %name_list% && %pos% > 0
  set name %name_list.car%
  set name_list %name_list.cdr%
  if %mid_list.strlen%
    set mid %mid_list.car%
    set mid_list %mid_list.cdr%
  end
  if %last_list.strlen%
    set last %last_list.car%
    set last_list %last_list.cdr%
  end
  if %sex_list.strlen%
    set sex %sex_list.car%
    set sex_list %sex_list.cdr%
  end
  eval pos %pos% - 1
done
* build name
set full %name%
if %mid%
  set full %full% %mid%
end
if %last%
  set full %full% %last%
end
* variable setup
%mod% %self% keywords %full% %self.alias%
%mod% %self% shortdesc %full%
%mod% %self% longdesc %full% %verbiage%
%mod% %self% sex %sex%
* look descs are done in a separate switch as they require the mods be done
switch %self.vnum%
  * NOTE: be wary of the 255-characters-per-line limit on the lookdescs
  case 11841
    * rogue mercenary
    %mod% %self% lookdesc %self.name% is standing in a shadowy corner, watching you intently. You can't get a good look at %self.himher%, but it looks like %self.heshe% is holding something sharp.
  break
  case 11842
    * caster mercenary
    %mod% %self% lookdesc %self.name% is flipping through books, stopping only occasionally when %self.heshe% finds something of interest. Although %self.heshe% isn't dressed like the Skycleavers
    %mod% %self% append-lookdesc downstairs, %self.heshe% seems quite familiar with this sort of magic. A rather large coinpurse hangs from %self.hisher% belt.
  break
  case 11843
    * archer mercenary
    %mod% %self% lookdesc %self.name% is equipped with a viciously well-made bow, currently sitting to %self.hisher% side while %self.heshe% fletches arrows. Though %self.heshe% isn't exactly
    %mod% %self% append-lookdesc well-dressed, %self.heshe% bears a large, heavy coin purse on %self.hisher% belt.
  break
  case 11844
    * nature mage mercenary
    %mod% %self% lookdesc %self.name% rolls a little ball of fire back and forth over %self.hisher% hands, occasionally singeing the cuffs of %self.hisher% robe. The robe, brown and rough,
    %mod% %self% append-lookdesc conceals whatever %self.heshe% might be carrying underneath.
  break
  case 11845
    * vampire mercenary
    %mod% %self% lookdesc %self.name% is dressed all in black, in stark contrast to %self.hisher% deathly-pale face. Though %self.hisher% clothes look expensive, they're in poor condition,
    %mod% %self% append-lookdesc with frayed edges and holes. Strange, considering the size of the coin purse %self.heshe%'s carrying.
  break
  case 11846
    * armored mercenary
    %mod% %self% lookdesc %self.name% wears a well-polished imperium breastplate over a padded shirt with leather pants. It seems %self.heshe% showed up ready for a fight.
  break
done
* and detach
detach 11857 %self.id%
~
#11858
Shade of Mezvienne fight: Shadow Whip, Shadow Flail, Total Darkness, Shade's Grasp, Drain Knezz~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4 5
  set num_left 5
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
* perform move
skyfight lockout 30 35
if %move% == 1
  * Shadow Whip
  skyfight clear dodge
  %echo% &&mThe Shade spins itself into a ball...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 1
  set cycle 1
  eval wait 8 - %diff%
  set targ %random.enemy%
  while %cycle% <= (2 * %diff%) && %targ%
    set targ_id %targ.id%
    skyfight setup dodge %targ%
    %send% %targ% &&m**** The shadows gather around you... ****&&0 (dodge)
    wait %wait% s
    if %targ.id% != %targ_id% || %targ.position% == Dead
      * gone
    elseif %targ.var(did_sfdodge)%
      %echo% &&mA tendril whips out from the Shade but misses ~%targ%!&&0
      if %diff% == 1
        dg_affect #11856 %targ% TO-HIT 25 20
      end
    else
      * hit
      %echo% &&mA shadow tendril whips out from the Shade and flogs ~%targ%!&&0
      dg_affect #11815 %targ% off silent
      dg_affect #11815 %targ% DISARMED on 20
      if %diff% > 1
        %send% %targ% That really hurt!
        %damage% %targ% 200 physical
      end
    end
    skyfight clear dodge
    eval cycle %cycle% + 1
    set targ %random.enemy%
  done
elseif %move% == 2
  * Shadow Flail
  skyfight clear dodge
  %echo% &&mThe Shade spins, flailing its tendrils in all directions...&&0
  eval dodge %diff% * 40
  dg_affect #11869 %self% DODGE %dodge% 20
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 3 s
  %echo% &&m**** If you're going to dodge the Shade's flailing tendrils, now is the time! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          set hit 1
          %echo% &&mThe Shade hits ~%ch% with a flailing tendril!&&0
          if %diff% > 1
            dg_affect #11870 %ch% TO-HIT -15 30
          end
          %damage% %ch% 100 physical
        elseif %ch.is_pc%
          %send% %ch% &&mYou narrowly avoid a flailing tendril!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&m**** The Shade is still flailing! ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  dg_affect #11869 %self% off
  if !%hit%
    if %diff% < 3
      %echo% &&m~%self% slowly spins back down.&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
elseif %move% == 3
  * Total Darkness
  skyfight clear interrupt
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup interrupt all
  %echo% &&m**** The office grows dimmer as the shadows gather... ****&&0 (interrupt)
  wait 8 s
  if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
    set broke 1
    set ch %room.people%
    while %ch%
      if %ch.var(did_sfinterrupt,0)%
        %send% %ch% &&mYou somehow manage to interrupt the Shade!&&0
      end
      set ch %ch.next_in_room%
    done
    %echo% &&mThe darkness scatters for a moment as the Shade is distracted.&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
  else
    %echo% &&mThe last light fades and the office is plunged into total darkness!&&0
    set ch %room.people%
    while %ch%
      if %self.is_enemy(%ch%)%
        if %ch.ability(Darkness)%
          %send% %ch% Luckily you can still see!
        else
          dg_affect #11860 %ch% BLIND on 25
        end
      end
      set ch %ch.next_in_room%
    done
  end
elseif %move% == 4
  * Shade's Grasp
  eval time %diff% * 6
  if %diff% <= 2
    dg_affect #11852 %self% HARD-STUNNED on %time%
  end
  set targ 0
  set targ_id 0
  while %time% > 0
    if !%targ% || %targ.id% != %targ_id%
      set need 1
    elseif !%targ.affect(11822)%
      set need 1
    else
      set need 0
    end
    if %need%
      skyfight clear struggle
      set targ %random.enemy%
      if !%targ%
        halt
      end
      set targ_id %targ.id%
      %send% %targ% &&m**** The Shade wraps itself around you! ****&&0 (struggle)
      %echoaround% %targ% &&mThe Shade wraps itself around ~%targ%!&&0
      skyfight setup struggle %targ% %time%
      set bug %targ.inventory(11890)%
      if %bug%
        set strug_char You try to struggle free of the Shade's grasp...
        set strug_room ~%%actor%% struggles to get free of the Shade's grasp...
        remote strug_char %bug.id%
        remote strug_room %bug.id%
        set free_char You manage to wiggle out of the Shade's grasp!
        set free_room ~%%actor%% manages to wiggle out of the Shade's grasp!
        remote free_char %bug.id%
        remote free_room %bug.id%
      end
    else
      %send% %targ% &&m**** You're still caught in the Shade's grasp! ****&&0 (struggle)
    end
    eval time %time% - 3
    wait 3 s
  done
  skyfight clear struggle
  dg_affect #11852 %self% off
elseif %move% == 5 && %diff% > 1
  * Drain Knezz
  skyfight clear interrupt
  skyfight setup interrupt all
  %echo% &&m**** The Shade is draining more power from the Grand High Sorcerer! ****&&0 (interrupt)
  wait 8 s
  if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
    set broke 1
    set ch %room.people%
    while %ch%
      if %ch.var(did_sfinterrupt,0)%
        %send% %ch% &&mYou somehow manage to interrupt the Shade!&&0
      end
      set ch %ch.next_in_room%
    done
    %echo% &&mThe Shade is distracted, if only for a moment.&&0
    dg_affect #11852 %self% HARD-STUNNED on 5
  else
    %echo% &&mKnezz lets out an anguished groan as the Shade drains his life force!&&0
    eval knezz_timer %self.var(knezz_timer,%timestamp%)% - 30
    remote knezz_timer %self.id%
  end
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11859
Mind-Controlled HS Barrosh fight: Baleful Polymorph, Devastation Ritual, Wave of Guilt, Deathbolt~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4
  set num_left 4
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
* perform move
skyfight lockout 30 35
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Baleful Polymorph
  set targ %random.enemy%
  if !%targ%
    set targ %actor%
  end
  eval vnum 11866 + %random.3%
  if %diff% > 1
    eval vnum %vnum% + 10
  end
  if %targ.morph% == %vnum%
    * already morphed
    nop %self.set_cooldown(11800,0)%
    halt
  end
  * wait?
  set targ_id %targ.id%
  set fail 0
  if %diff% <= 2
    skyfight clear interrupt
    %send% %targ% &&m**** ~%self% stamps his staff three times... in your direction! ****&&0 (interrupt)
    %echoaround% %targ% &&m~%self% stamps his staff three times...&&0
    skyfight setup interrupt %targ%
    wait 8 s
    if %self.disabled%
      halt
    end
    if !%targ% || %targ.id% != %targ_id%
      * lost
      set fail 1
    elseif %targ.var(did_sfinterrupt)%
      set fail 1
      %echo% &&mThe spell from Barrosh's staff bounces around the room but hits nothing.&&0
      if %diff% == 1
        dg_affect #11856 %targ% TO-HIT 25 20
      end
    else
      %send% %targ% &&mYou are enveloped in an eerie violet light from the staff...&&0
    end
    skyfight clear interrupt
  else
    %echo% &&m~%self% stamps his staff three times...&&0
  end
  if !%fail%
    if %targ.trigger_counterspell%
      set fail 2
      %echo% &&mThe spell rebounds off of |%targ% counterspell and smacks ~%self% in the face -- he looks stunned!&&0
    end
  end
  if !%fail%
    set old_shortdesc %targ.name%
    %morph% %targ% %vnum%
    %echoaround% %targ% &&m%old_shortdesc% is suddenly transformed into ~%targ%!&&0
    %send% %targ% &&m**** You are suddenly transformed into %targ.name%! ****&&0 (fastmorph normal)
    if %diff% == 4 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
      dg_affect #11851 %targ% STUNNED on 5
    elseif %diff% >= 2
      nop %targ.command_lag(ABILITY)%
    end
  elseif %fail% == 2 || %diff% == 1
    dg_affect #11852 %self% HARD-STUNNED on 10
  end
elseif %move% == 2
  * Devastation Ritual
  %echo% &&m**** ~%self% plants his staff hard into the floor and begins drawing mana toward himself... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  set cycle 1
  while %cycle% <= 4
    skyfight clear interrupt
    skyfight setup interrupt all
    wait 5 s
    if %self.disabled%
      halt
    end
    set needed %room.players_present%
    if %needed% > 4
      set needed 4
    end
    if %self.sfinterrupt_count% >= %needed%
      %echo% &&mYou manage to disrupt Barrosh's intricate ritual, at least for now!&&0
      if %diff% == 1
        dg_affect #11868 %self% BONUS-MAGICAL -10 30
      end
    else
      %echo% &&mA wave of devastation pulses out from Barrosh's staff!&&0
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if %ch.trigger_counterspell%
            %echo% &&mA shield forms in front of ~%ch% to block the devastation ritual!&&0
          else
            %echo% &&mThe wave cuts through ~%ch%!&&0
            %damage% %ch% 100 magical
          end
        end
        set ch %next_ch%
      done
    end
    if %cycle% < 4
      %echo% &&m**** Here comes another wave! ****&&0 (interrupt)
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 3
  * Wave of Guilt
  %echo% &&m**** ~%self% plants his staff hard into the floor and starts to pulse out dark energy... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear interrupt
  skyfight setup interrupt all
  wait 8 s
  if %self.disabled%
    halt
  end
  if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
    %echo% &&mYou manage to disrupt Barrosh's ritual in time!&&0
    if %diff% == 1
      dg_affect #11868 %self% BONUS-MAGICAL -10 30
    end
  else
    %echo% &&mA dark pulse emanates from Barrosh's staff...&&0
    eval debuff %diff% * 15
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if %ch.trigger_counterspell%
          %echo% &&mA shield forms in front of ~%ch% to block the wave!&&0
        else
          %send% %ch% &&mYou feel a wave of guilt as it passes through you!&&0
          %echoaround% %ch% &&mThe wave passes through ~%ch%!&&0
          dg_affect #11871 %ch% DODGE -%debuff% 30
          dg_affect #11871 %ch% TO-HIT -%debuff% 30
          dg_affect #11871 %ch% RESIST-MAGICAL -%debuff% 30
          if %diff% >= 3
            %damage% %ch% 80 magical
          end
        end
      end
      set ch %next_ch%
    done
  end
  skyfight clear interrupt
elseif %move% == 4 && !%self.aff_flagged(BLIND)%
  * Deathbolt
  skyfight clear interrupt
  %echo% &&m~%self% holds up his glowing staff...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 1
  eval resist %diff% * 20
  set cycle 1
  eval wait 10 - %diff%
  set targ %random.enemy%
  while %cycle% <= %diff% && %targ%
    set targ_id %targ.id%
    skyfight setup interrupt %targ%
    %send% %targ% &&m**** |%self% staff is aimed at you! ****&&0 (interrupt)
    wait %wait% s
    if %targ.id% != %targ_id% || %targ.position% == Dead
      * gone
    elseif %targ.var(did_sfinterrupt)%
      if %random.2% == 1
        %echo% &&m~%self% is distracted by |%targ% quick reflexes and a flying knickknack!&&0
      else
        %send% %targ% &&m|%self% deathbolt flies wide as you knock some papers into the air!&&0
        %echoaround% %targ% &&m|%self% deathbolt flies wide as ~%targ% knocks some papers into the air!&&0
      end
      if %diff% == 1
        dg_affect #11873 %self% TO-HIT -15 20
      end
    else
      * hit
      %echo% &&mA powerful deathbolt rockets out of |%self% staff and slams into ~%targ%!&&0
      %damage% %targ% 200 magical
      if %diff% > 1
        dg_affect #11872 %targ% off silent
        dg_affect #11872 %targ% RESIST-MAGICAL -%resist% 20
      end
    end
    skyfight clear interrupt
    eval cycle %cycle% + 1
    * new targ
    set targ %random.enemy%
  done
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11860
Shard cultivator: upgrade shard tools~
1 c 2
cultivate~
return 1
if !%arg%
  %send% %actor% Cultivate which shard?
  halt
end
set target %actor.obj_target_inv(%arg%)%
if !%target%
  %send% %actor% You don't seem to have %arg.ana% '%arg%'. (You can only use @%self% on items in your inventory.)
  halt
end
if %target.vnum% != 11935 && %target.vnum% != 11936
  %send% %actor% You can only use @%self% to upgrade the skycleaver crafting shard or magichanical tedium shard.
  halt
end
if %target.is_flagged(SUPERIOR)%
  %send% %actor% @%target% is already upgraded; using @%self% would have no benefit.
  halt
end
%send% %actor% You carefully touch @%self% to @%target%...
%echoaround% %actor% ~%actor% touches @%self% to @%target%...
%echo% @%target% takes on a sky-blue glow!
nop %target.flag(SUPERIOR)%
eval level %actor.highest_level% + 50
%scale% %target% %level%
%purge% %self%
~
#11861
Skycleave: Barrosh mind-control struggle scene~
0 b 100
~
if %self.fighting% || %self.disabled%
  halt
end
* fetch position
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
* store line back
remote line %self.id%
* just echo in order (resets on default)
switch %line%
  case 1
    say Noooooo!
    %at% i11862 %echo% You hear shouting from the office to the east.
    %at% i11867 %echo% You hear shouting from the office.
  break
  case 2
    %echo% High Sorcerer Barrosh clasps at his head and winces in pain.
  break
  case 3
    %echo% A vase shatters as Barrosh throws it against the wall as hard as he can!
    %at% i11862 %echo% You hear the sound of something shattering against the wall from the office to the east.
    %at% i11867 %echo% You hear the sound of something shattering against the wall in the office.
  break
  case 4
    say I... will... burn you all down!
    %at% i11862 %echo% You hear shouting from the office to the east.
    %at% i11867 %echo% You hear shouting from the office.
    wait 6 sec
    %aggro%
  break
  case 5
    %echo% High Sorcerer Barrosh throws a book hard at you, but it misses and goes out the door.
    %at% i11862 %echo% A book comes flying out the office door and sails over the railing.
  break
  case 6
    say My mind... is not my own.
    wait 6 sec
    %aggro%
  break
  case 7
    say Flee while you can! I can't hold it off.
    wait 6 sec
    %aggro%
  break
  case 8
    %echo% High Sorcerer Barrosh grasps his staff and tries to support himself as he doubles over in agony!
  break
  default
    set line 0
    remote line %self.id%
  break
done
~
#11862
Skycleave: skymote conversation helper~
0 c 0
skymote~
* Manages bits of conversations that otherwise use '11840 Storytime' but need
* minor script-based information.
if %actor% != %self%
  return 0
  halt
end
set spirit %instance.mob(11900)%
set room %self.room%
if %arg% == caiusniamh
  if %room.people(11931)%
    say I have been discussing the otherworlder with Niamh here. Specifically, where we might locate a second one.
  else
    say Madame Niamh and I have been discussing where we might locate a second otherworlder.
  end
elseif %arg% == scaldniamh1
  if %room.people(11931)%
    say Little Niamh here goes through apprentices so fast I can't be bothered with their names.
  else
    say Frankly I can't be bothered with the apprentice names, either, at the rate Niamh goes through them.
  end
elseif %arg% == scaldniamh2
  if %room.people(11931)%
    say Healthy appreciation for the DARK ARTS, eh, Niamh?
  else
    say Always did like her, though. Niamh. She has a healthy respect for the DARK ARTS.
  end
elseif %arg% == scaldopen
  set rescuer %spirit.var(lich_released,0)%
  if !%rescuer%
    say I should have known none of those unlettered louts would open it.
  else
    set found 0
    set ch %room.people%
    while %ch% && !%found%
      if %ch.id% == %rescuer%
        set found %ch.name%
      end
      set ch %ch.next_in_room%
    done
    if %found%
      say My gratitude that you released me is... eternal.
    else
      say Lucky someone had the good sense to release me.
    end
  end
end
~
#11863
Skycleave: Shade ascension / Death of Knezz~
0 b 100
~
* This starts a timer that will kill Knezz and ascend the shade after 2 minutes
set room %self.room%
if %room.people(11868)%
  * fetch timer
  if %self.varexists(knezz_timer)%
    set knezz_timer %self.knezz_timer%
  else
    set knezz_timer 0
  end
  * check startup
  if %knezz_timer% == 0 && !%self.fighting%
    * not started
    halt
  end
  if %knezz_timer% == 0 && %self.fighting%
    set knezz_timer %timestamp%
    remote knezz_timer %self.id%
    halt
  end
  * the rest of this is based on time since start
  eval seconds %timestamp% - %knezz_timer%
else
  * no knezz at all
  set seconds 99999
end
* ensure affect
if !%self.affect(11874)%
  dg_affect #11874 %self% INTELLIGENCE 1 120
end
* how long is allowed?
eval allow_time 45 * %self.var(diff,1)%
* check timers
if %seconds% > %allow_time%
  * Knezz dies
  nop %self.add_mob_flag(NO-ATTACK)%
  %restore% %self%
  * stun everyone
  set room %self.room%
  set ch %room.people%
  while %ch%
    if %ch% != %self%
      dg_affect %ch% HARD-STUNNED on 12
    end
    set ch %ch.next_in_room%
  done
  * messaging
  %echo% &&mA blast of dark energy throws you backwards as a shadow claws its way out of Knezz's mouth and joins the Shade above him.&&0
  wait 6 sec
  %echo% The Shade of Mezvienne drops Knezz's lifeless body in the chair and snatches his wand.
  set knezz %room.people(11868)%
  if %knezz%
    %slay% %knezz%
  end
  wait 6 sec
  %echo% The Shade holds the gnarled wand high in the air...
  %subecho% %room% &&yThe Shade of Mezvienne shouts, 'By the power of Skycleave... I HAVE THE POWER!'&&0
  wait 3 sec
  %echo% The Shade of Mezvienne grows and transforms as it ascends into a higher being!
  %load% mob 11863
  set mob %room.people%
  if %mob.vnum% == 11863
    set diff %self.var(diff,1)%
    remote diff %mob.id%
    if %self.mob_flagged(HARD)%
      nop %mob.add_mob_flag(HARD)%
    end
    if %self.mob_flagged(GROUP)%
      nop %mob.add_mob_flag(GROUP)%
    end
    %scale% %mob% %self.level%
    if %self.fighting%
      %force% %mob% %aggro% %self.fighting%
    else
      %force% %mob% %aggro%
    end
  end
  * reskin room
  %mod% %room% description An inky shadow covers most of the room such that you can't see the walls or ceiling. It pours over the furniture and creeps along the floor. Only the area
  %mod% %room% append-description around your feet is free of it. The shadow seems to be alive... and worse, it appears to be drawing more power from Skycleave by the second.
  * and purge self
  %purge% %self%
elseif %seconds% > (%allow_time% - 10)
  %echo% Grand High Sorcerer Knezz goes limp as darkness begins to seep from his mouth and eyes.
  dg_affect %self% BONUS-MAGICAL 5 -1
elseif %seconds% > (%allow_time% - 30)
  %echo% The Shade of Mezvienne grows to fill the room as Knezz grows paler and paler.
  dg_affect %self% BONUS-MAGICAL 5 -1
elseif %seconds% > (%allow_time% - 60)
  %echo% The Shade of Mezvienne grows as it draws Knezz's life force.
  dg_affect %self% BONUS-MAGICAL 5 -1
end
~
#11864
Skycleave: Shared mob command trigger (Mez transition, tower mounts, Waltur)~
0 c 0
diagnose mount ride donate~
set aqua_vnums 11854 11855 11856 11857 11858
* modes
if donate /= %cmd% && %self.vnum% == 11840
  * Waltur 3A
  %send% %actor% You inquire about donating...
  %echoaround% %actor% ~%actor% inquires about donating...
  say This is not the time. We're in the middle of a crisis.
elseif donate /= %cmd% && %self.vnum% == 11940
  * Waltur 3B
  %send% %actor% Use 'buy donation' to donate to the laboratory.
elseif diagnose /= %cmd% && %self.vnum% == 11866
  * Mez 11866 during phase transition
  %send% %actor% ~%self% doesn't look very good.
elseif (mount /= %cmd% || ride /= %cmd%) && %self.vnum% == 11852
  * flying throne
  if !%arg% || %actor.char_target(%arg.car%)% != %self%
    * pass through
    return 0
  elseif %actor.completed_quest(11918)% || %actor.completed_quest(11919)% || %actor.completed_quest(11864)%
    * allow
    return 0
  else
    %send% %actor% You leap onto the flying throne but it dumps you on the ground!
    %echoaround% %actor% ~%actor% leaps onto the flying throne but it dumps *%actor% on the ground.
  end
elseif (mount /= %cmd% || ride /= %cmd%) && %aqua_vnums% ~= %self.vnum%
  * fake aquatic mounts
  set arg %arg.car%
  if %actor.has_tech(Riding)% && %arg% && %actor.char_target(%arg%)% == %self%
    set jelly %actor.inventory(11892)%
    if %jelly% && %actor.completed_quest(11972)%
      nop %self.add_mob_flag(MOUNTABLE)%
      %purge% %jelly%
      return 0
    else
      %send% %actor% You leap onto |%self% back... and splash right through!
      %echoaround% %actor% ~%actor% leaps onto |%self% back... and splashes right through!
    end
  else
    return 0
  end
else
  return 0
end
~
#11865
Skycleave: Knezz phase A post-fight cutscene~
0 bw 100
~
* messages starting shortly after load
* fetch position
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
* store line back
remote line %self.id%
* this goes +1 per 13 seconds and automatically ends when it hits default
switch %line%
  case 1
    say Ah... that was perilously close. Give me a moment to collect myself. She took a lot out of me.
    wait 9 sec
    %echo% Knezz draws a gnarled old wand from the sleeve of his robe and turns it toward himself.
    wait 1
    say On my authority, I cast out the shadow!
    wait 8 sec
    %echo% Knezz taps his wand against the arm of the chair a few times.
  break
  case 2
    say By the power of Skycleave, I cast out the shadow!
    wait 9 sec
    %echo% Knezz bangs his wand against the arm of his chair.
    wait 9 sec
    say By the power... oh, well, blast it all, I'll do it later.
  break
  case 3
    say Oh, you're still here... good.
    wait 9 sec
    say You need to get to the top of the tower and burn the heartwood.
    wait 9 sec
    say It's the only thing that will free us now.
  break
  case 4
    say With the heartwood burned, mighty Skycleave should finally be free of that wretched time loop.
    wait 9 sec
    say I dare not guess how many times we've repeated this horrid week.
    wait 9 sec
    say To think she was here on my personal invitation. I do hope her mercenaries haven't done too much damage downstairs.
  break
  case 5
    * HE MOVES WEST TO 11869 HERE
    say With apologies, I must retire to my quarters and get some rest now.
    wait 9 sec
    west
    * ensure right spot
    if %self.room.template% != 11869
      %echo% ~%self% leaves.
      mgoto i11869
      %echo% ~%self% walks in from the east.
    end
    wait 9 sec
    %echo% Knezz sits down to rest in his easy chair.
  break
  case 6
    say I just need to sit for a spell. There will be time for stories later. Go burn that heartwood.
  break
  case 7
    say What are you waiting for? You need to burn the heartwood before the time loop repeats again.
  break
  default
    * done
    if !%self.has_trigger(11840)%
      attach 11840 %self.id%
    end
    detach 11865 %self.id%
  break
done
~
#11866
Skycleave: Skip command (for cutscenes)~
0 c 0
skip~
if !%self.varexists(skip)%
  %send% %actor% You skip the cutscene.
  %echoaround% %actor% ~%actor% has skipped the cutscene.
  set skip 1
  remote skip %self.id%
else
  %send% %actor% Someone has already skipped the cutscene (it may still play another message or two before skipping).
end
~
#11867
Skycleave: Boss room relocator~
0 hnA 100
~
set no_purge_list 11871 11872
* Teleports people/items out of a room that's restricted while the mob is there
if %actor.nohassle%
  halt
end
wait 0
set room %self.room%
set to_vnum 0
set message 0
set move_objs 1
* 'to_vnum' must be set by this switch based on the from-vnum
switch %room.template%
  case 11871
    set to_vnum 11870
    set message The swirling vortex tosses you back down the stairs!
    set move_objs 0
  break
  case 11872
    set spirit %instance.mob(11900)%
    if %spirit.phase4%
      set to_vnum 11971
    else
      set to_vnum 11871
    end
    set message The time stream collapses and you're thrown back to the tower!
  break
  default
    * unknown room: nothing to do
    %purge% %self%
    halt
  break
done
* Ensure part of the instance
nop %self.link_instance%
set to_room %instance.nearest_rmt(%to_vnum%)%
if !%to_room%
  * No destination
  %purge% %self%
  halt
end
* echo now
if %message%
  %echo% %message%
end
* Check items here
if %move_objs%
  set obj %room.contents%
  while %obj%
    set next_obj %obj.next_in_list%
    if %obj.can_wear(TAKE)%
      %teleport% %obj% %to_room%
    end
    set obj %next_obj%
  done
end
* Check people here
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.nohassle% || (%ch.vnum% >= 11894 && %ch.vnum% <= 11899)
    * nothing (11894-11899 are mobs involved in phase change)
  elseif %ch.is_pc% || !%ch.linked_to_instance% || %ch.aff_flagged(*CHARM)%
    * Move ch
    %teleport% %ch% %to_room%
    %load% obj 11805 %ch%
  elseif %ch% != %self% && !(%no_purge_list% ~= %ch.vnum%)
    * Adventure mob: purge, usually
    %purge% %ch%
  end
  set ch %next_ch%
done
~
#11868
Skycleave: Skithe Ler-Wyn reset trigger~
0 ab 100
~
* this resets the whole fight
if %self.fighting% || %self.disabled% || %self.health% <= 0 || %self.aff_flagged(!ATTACK)%
  halt
end
* ensure nobody is fighting me
set room %self.room%
set ch %room.people%
while %ch%
  if %ch.fighting% == %self%
    halt
  end
  set ch %ch.next_in_room%
done
* if we got to here, nobody is fighting us, start a countdown...
wait 30 sec
if %self.fighting% || %self.disabled% || %self.health% <= 0
  halt
end
* reset: remove the relocator vortex
set vortex %instance.mob(11865)%
if %vortex%
  %at% i11871 %echo% The swirling time vortex on top of the tower dissipates.
  %purge% %vortex%
end
* reset: remove move-blocking vortex
makeuid hall room i11870
while %hall.contents(11870)%
  %purge% %hall.contents(11870)%
done
%at% %hall% %echo% The swirling time vortex on top of the tower dissipates.
* ensure no lingering Mezvienne
set mez %instance.mob(11866)%
if %mez%
  %purge% %mez%
end
* put Mezvienne back
%at% i11871 %load% mob 11871
set mez %instance.mob(11871)%
if %mez%
  set diff %self.var(diff,1)%
  remote diff %mez.id%
  set spirit %instance.mob(11900)%
  if (%spirit.diff4% // 2) == 0
    nop %mez.add_mob_flag(HARD)%
  end
  if %spirit.diff4% >= 3
    nop %mez.add_mob_flag(GROUP)%
  end
end
* place a relocator
%load% mob 11899
* lastly, remove me (will be reloaded by the boss)
%purge% %self%
~
#11869
Skycleave: Skithe Ler-Wyn load trigger~
0 nA 100
~
wait 0
set room %self.room%
dg_affect %self% !ATTACK on -1
if !%self.varexists(skip)%
  wait 9 sec
  %echo% Skithe Ler-Wyn, the Lion of Time, lets out a fearsome roar and lands in front of you just as you finally find your footing.
  wait 9 sec
end
* find highest visit count and raise visit counts
set ch %room.people%
set highest 1
while %ch%
  if %ch.is_pc%
    if %ch.varexists(skithe_visits)%
      set skithe_visits %ch.skithe_visits%
    else
      set skithe_visits 0
    end
    if %skithe_visits% > %highest%
      set highest %skithe_visits%
    end
    * only raise visits by 1 per instance
    if !%room.varexists(visited_%ch.id%)%
      eval skithe_visits %skithe_visits% + 1
      remote skithe_visits %ch.id%
      set visited_%ch.id% 1
      remote visited_%ch.id% %room.id%
    end
  end
  set ch %ch.next_in_room%
done
* message based on highest visit count
if %highest% >= 175
  set message Just as you have tried hundreds of times before, so too will you fail again...
elseif %highest% >= 125
  set message You will fail again here, as you have failed more than a hundred times before...
elseif %highest% >= 100
  set message You have tried a hundred times, and a hundred times you have failed...
elseif %highest% >= 75
  set message So many attempts to stop me, and yet here we are again...
elseif %highest% >= 50
  set message How many times will do this? Fifty? A hundred? A thousand? It is not within your power or purview to stop me...
elseif %highest% >= 25
  set message Again? Do you not grow tired of this dance? I have all of time. You have what, another twenty years? Don't dare think it's longer...
elseif %highest% >= 15
  set message You again? You could do this another dozen times or a hundred; nothing you have done here will matter. Surely you know it is futile...
elseif %highest% >= 10
  set message You again? Did we not settle this matter already?
elseif %highest% >= 5
  set message Your persistence is admirable, if misguided. You lack the power to stop any of this...
elseif %highest% >= 2
  set message Back again so soon? Pity, I thought you had learned a lesson...
else
  set message Ah, fresh blood has poured itself into my time stream. Have you come here to be a vessel or merely a meal?
end
%echo% Skithe Ler-Wyn, the Lion of Time, says, '%message%'
if %self.varexists(skip)%
  wait 1
else
  wait 6 sec
end
%echo% Skithe Ler-Wyn, the Lion of Time, says, 'No power you wield can stop me from digesting this tower and everything in it for all time!'
dg_affect %self% !ATTACK off
* and wait before attacking
wait 9 sec
if !%self.fighting%
  %aggro%
end
~
#11870
Skycleave: Mezvienne phase transition cutscene~
0 ab 100
~
* messaging following this mob being loaded and the players being teleported to her room
* fetch position
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
* check skip
if %self.varexists(skip)%
  set line 999
end
* store line back
remote line %self.id%
* this goes +1 per 13 seconds and automatically ends when it hits default
switch %line%
  case 1
    %echo% You struggle to find footing in the time stream as all the trappings of your world fade beyond the vortex.
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% ~%self% continues to scream as she pulls at her long hair, which is rapidly turning white again...
    %mod% %self% longdesc Mezvienne the Enchantress pulls at her hair as she struggles in the vortex!
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% ~%self% screams, 'Why?! My master, my lion, my Skithe... was I not faithful?'
  break
  case 2
    %echo% ~%self% lets out a blood-curdling screech, arches her back, and throws her head backwards as white light shoots from her eyes!
    %mod% %self% longdesc Mezvienne the Enchantress twists in the air as white light streams from her eyes!
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% ~%self% shouts, 'No! No, I did everything you asked!'
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% ~%self% wraps her scrawny, sagging arms around her body and claws at her own back as she screams...
    %mod% %self% longdesc Mezvienne the Enchantress claws at her own back as she screams in agony!
  break
  case 3
    %echo% ~%self% curls up into a ball and lets out the loudest, most anguished scream yet as a red mist spurts from her back...
    %mod% %self% longdesc Mezvienne the Enchantress is curled in a ball in the air, screaming!
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% A disembodied voice cuts through the roaring vortex, saying, 'Little lamb, you have indeed served your purpose.'
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% The voice continues, 'You were the most magnificent vessel and you plucked out your part like an enchanted harp.'
  break
  default
    * final step: message first
    if !%self.varexists(skip)%
      %echo% The disembodied voice grows louder, saying, 'But these interlopers have cut your strings short and so, here, your song ends.'
      wait 9 sec
    end
    %echo% ~%self% lets out one final truncated scream as her back bursts open and a gigantic, glimmering lion claws its way out!
    * and move on to the next phase
    %load% mob 11872
    set mob %self.room.people%
    if %mob.vnum% == 11872
      set diff %self.var(diff,1)%
      remote diff %mob.id%
      if %self.varexists(skip)%
        set skip 1
        remote skip %mob.id%
      end
      set spirit %instance.mob(11900)%
      if (%spirit.diff4% // 2) == 0
        nop %mob.add_mob_flag(HARD)%
      end
      if %spirit.diff4% >= 3
        nop %mob.add_mob_flag(GROUP)%
      end
      %restore% %mob%
    end
    %purge% %self%
    halt
  break
done
~
#11871
Skycleave: Mezvienne starts phase transition (hitprc)~
0 l 30
~
set room %self.room%
* messaging
%echo% \&0
%echo% ~%self% screams and flies up into the air as the whole tower is engulfed in an enormous vortex!
* remove relocator if present
makeuid vortex room i11872
set mob %vortex.people(11899)%
while %mob%
  %purge% %mob%
  set mob %vortex.people(11899)%
done
* load 2nd version
%at% %vortex% %load% mob 11866
set mez %instance.mob(11866)%
set diff %self.var(diff,1)%
remote diff %mez.id%
* relocate and stun everyone
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch% != %self%
    dg_affect #11867 %ch% HARD-STUNNED on 5
    %teleport% %ch% %vortex%
  end
  set ch %next_ch%
done
* load vortex blocker
%at% i11870 %load% obj 11870
* load relocator here
%load% mob 11865
* done
%purge% %self%
~
#11872
Skycleave: Mezvienne starts phase transition (death)~
0 f 100
~
set room %self.room%
* messaging
%echo% &&0
%echo% &&m~%self% screams and flies up into the air as the whole tower is engulfed in an enormous vortex!&&0
* remove relocator if present
makeuid vortex room i11872
set mob %vortex.people(11899)%
while %mob%
  %purge% %mob%
  set mob %vortex.people(11899)%
done
* load 2nd version
%at% %vortex% %load% mob 11866
set mez %instance.mob(11866)%
set diff %self.var(diff,1)%
remote diff %mez.id%
* relocate and stun everyone
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch% != %self%
    dg_affect #11867 %ch% HARD-STUNNED on 5
    %teleport% %ch% %vortex%
  end
  set ch %next_ch%
done
* load vortex blocker
%at% i11870 %load% obj 11870
* load relocator here
%load% mob 11865
* block death cry
return 0
~
#11873
Skithe Ler-Wyn death~
0 f 100
~
* message
%echo% &&mSkithe Ler-Wyn, Lion of Time, dissolves into the air above you with one final roar!&&0
makeuid tower room i11871
* remove the relocator vortex
set vortex %instance.mob(11865)%
if %vortex%
  %at% %tower% %echo% &&mThe swirling time vortex on top of the tower dissipates.&&0
  %purge% %vortex%
end
* remove move-blocking vortex
makeuid hall room i11870
while %hall.contents(11870)%
  %purge% %hall.contents(11870)%
done
%at% %hall% %echo% &&mThe swirling time vortex on top of the tower dissipates.&&0
* place a relocator here
%load% mob 11899
* move self
mgoto %tower%
* prevent death cry
return 0
~
#11874
Smol Nes-Pik: Torru and Nayyur~
0 bw 100
~
* controls both rot worshippers in the room; runs on 11886 Torru
* this is one big cycle of text and then repeats
* first find 11887 Nayyur or load him if he's somehow missing
set nayyur %instance.mob(11887)%
if !%nayyur%
  load mob 11887
  set nayyur %self.room.people%
elseif %nayyur.room% != %self.room%
  %at% %nayyur.room% %echo% ~%nayyur% leaves.
  %teleport% %nayyur% %self.room%
  %echo% ~%nayyur% arrives.
  wait 1
end
* fetch cycle
if %self.varexists(cycle)%
  eval cycle %self.cycle% + 1
else
  set cycle 1
end
* force a restart if the sap is gone
if !%self.room.contents(11888)% && %cycle% > 4
  set cycle 1
end
* check skip?
if %self.varexists(skip)%
  rdelete skip %self.id%
  set cycle 4
  set skip 1
end
* main messages: must be sequential starting with 1 or it will just loop back to 1
switch %cycle%
  case 1
    say Iskip wennur, faskoh vadur, Iskip sal-ash Tagra Nes.
    wait 9 sec
    say Iskip sal-ash Tagra Nes...
    wait 1
    %force% %nayyur% say Iskip sal-ash Tagra Nes.
    wait 9 sec
    say Foroh undune aspeh a wo owri...
  break
  case 2
    say Smol Nes-Pik pos ne wo owri...
    wait 9 sec
    %force% %nayyur% say Iskip sal-ash Tagra Nes.
    wait 9 sec
    say Fras kee selmo tagra tyewa...
    wait 1 sec
    %echo% Thousands of tiny splinters of rotted wood rise from the trunk and hover in the air...
    %load% obj 11889 room
  break
  case 3
    say Fras kee selmo tagra tyewa...
    wait 1 sec
    %echo% More and more splinters rise into the air until you can scarcely breathe!
    wait 8 sec
    say Badur eydur a wo owri...
    wait 1 sec
    %force% %nayyur% say Iskip sal-ash Tagra Nes.
    wait 7 sec
    say Ickor carzo a kee selmo, vadur, eydur Tagra Nes!
    wait 1 sec
    %echo% ~%self% stabs the rotten tree with a gemstone knife as the splinters of wood fall out of the air...
    set splint %self.room.contents(11889)%
    if %splint%
      %purge% %splint%
    end
  break
  case 4
    if !%skip%
      say Ickor carzo a wo owri...
      wait 1 sec
      %force% %nayyur% say Iskip sal-ash Tagra Nes.
      say Iskip sal-ash Tagra Nes.
      wait 2 sec
    end
    %echo% Putrid sap begins to trickle from the rotting tree.
    * load putrid sap now
    if !%self.room.contents(11888)%
      %load% obj 11888 room
    end
  break
  case 5
    %echo% ~%self% and ~%nayyur% taste the putrid sap and their eyes glaze over with a milky film.
  break
  case 6
    say Doron kasko Iskip sal-ash...
    wait 1
    %force% %nayyur% say Yozol sal-ash Tagra Nes.
  break
  case 7
    say Opol kasko Iskip sal-ash...
    wait 1
    %force% %nayyur% say Yozol sal-ash Tagra Nes.
  break
  case 8
    say Tofvon kaskon Iskip sal-ash...
    wait 1
    %force% %nayyur% say Yozol sal-ash Tagra Nes.
  break
  case 9
    say Afta kasko Iskip sal-ash...
    wait 1
    %force% %nayyur% Besta sulyo tagra irrun.
  break
  case 10
    %echo% ~%self% digs ^%self% fingernails into the rotted tree until blood trickles down ^%self% arms...
    wait 6 sec
    say Devor esh pik necafuir devor recless.
  break
  case 11
    say Nayyur beo a wo tock nefor.
    wait 1 sec
    %echo% ~%nayyur% scoops up a fistful of rotted splinters and cradles them in both hands.
  break
  case 12
    %echo% Blood from |%self% hands drips onto the rotted splinters in |%nayyur% hands.
    wait 3 sec
    %echo% The bloody splinters in |%nayyur% hands burst into jet-black flames, releasing choking smoke.
  break
  case 13
    wait 4 sec
    say Iskip sal-ash Tagra Nes.
    wait 1
    %echo% Smoke from |%nayyur% black flame fills the hollow and diffuses into the rotten wood.
  break
  case 14
    %force% %nayyur% say Iskip sal-ash Tagra Nes.
    wait 6 sec
    %force% %nayyur% say Iskip shallas Tagra Nes.
  break
  case 15
    wait 55 sec
  break
  default
    * just restart it
    set cycle 0
  break
done
* store current cycle
remote cycle %self.id%
~
#11875
Smol Nes-Pik: Joiago gossip helper~
0 c 0
joiago~
* gossip helper for Joiago, partner to trigger 11877
if %actor.vnum% != 11877 || %arg% != gossip
  halt
end
set mob_list 11878 11880 11881 11882 11883
set ch %self.room.people%
set partner 0
set pc_partner 0
set count 0
* find a random partner present
while %ch%
  if %ch.is_pc% && %self.can_see(%ch%)%
    set pc_partner %ch%
  elseif %mob_list% ~= %ch.vnum%
    eval count %count% + 1
    eval chance %%random.%count%%%
    if %chance% == %count% || !%partner%
      set partner %ch%
    end
  end
  set ch %ch.next_in_room%
done
if !%partner%
  set partner %pc_parnter%
end
if !%partner%
  * still no partner?
  halt
end
* gossip loop pos
if %self.varexists(gossip)%
  eval gossip %self.gossip% + 1
else
  set gossip 1
end
if %gossip% > 3
  set gossip 1
end
* what to say?
if %partner.vnum% == 11880
  * Lotte
  switch %gossip%
    case 1
      say Where did you get that garish red frock?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say Well, it's all the rage in Smol Kai-Pik, but you wouldn't know.
      end
    break
    case 2
      say How's the tree?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say Never better, than you very much.
      end
    break
    case 3
      say I heard you all have a problem with Iskippians.
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say Prune your own tree, mister.
      end
    break
  done
elseif %partner.vnum% == 11882
  * High Tresydion
  if %partner.varexists(talking)%
    %echo% ~%self% watches ~%partner% orate.
  else
    switch %gossip%
      case 1
        say Better or worse today, Tresydion?
        wait 1
        if %partner.room% == %self.room%
          %force% %partner% say If you're asking about me, I feel as right as rain.
        end
      break
      case 2
        say Better or worse, though, Tresydion?
        wait 1
        if %partner.room% == %self.room%
          %force% %partner% say The Great Tree is getting better every day. Glean the dew for yourself.
        end
      break
      case 3
        say What's the good news?
        wait 1
        if %partner.room% == %self.room%
          %force% %partner% say Don't start.
        end
      break
    done
  end
elseif %partner.vnum% == 11883
  * Black Meket
  switch %gossip%
    case 1
      say Any news on your mother, Meket?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say Not a word, friend.
      end
    break
    case 2
      say Can I bring you anything?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say No, friend, I have taken the Dew Oath.
      end
    break
    case 3
      say What have you seen in the dew?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say All I can see is that same giant eye.
      end
    break
  done
else
  * Kym, Keeper Bastain, or the player
  switch %random.10%
    case 1
      say I heard Nayyur is worshipping the rot.
      set response 1
    break
    case 2
      say Have you noticed something... off... about the Sparo?
      set response 1
    break
    case 3
      say The tresydion says all those mushrooms and lichen are natural but they look like they're eating the tree.
      set response 2
    break
    case 4
      say Why are we out here trying to repair the glimmering stair while the smol are smoldering?
      set response 2
    break
    case 5
      say I heard the giants have been clearing whole forests.
      set response 3
    break
    case 6
      say Nayyur thinks one of the giants is the Iskip.
      set response 3
    break
    case 7
      say Someone mentioned the Great Tree is budding again.
      set response 4
    break
    case 8
      say I heard the Great Tree has a new branch.
      set response 4
    break
    case 9
      say Someone told me the whole thing is a dream.
      set response 5
    break
    case 10
      say How would you even know if you're the one dreaming us?
      set response 5
    break
  done
  wait 1
  if %partner.is_npc% && %partner.room% == %self.room%
    switch %response%
      case 1
        if %partner.vnum% == 11881
          %force% %partner% say Sounds like nunya.
        else
          %force% %partner% say I don't think that's any of my business.
        end
      break
      case 2
        if %partner.vnum% == 11881
          %force% %partner% say Tagra Nes has stood since the Dawn and you know it. All this talk of rot and ruin is a little extra.
        else
          %force% %partner% say Quit trying to stir up trouble, %self.name%.
        end
      break
      case 3
        if %partner.vnum% == 11881
          %force% %partner% say You still afraid of giants, %self.name%?
          wait 1
          if %partner.room% == %self.room%
            say Yeah, and you should be, too.
          end
        else
          %force% %partner% say That's a children's story.
        end
      break
      case 4
        if %partner.vnum% == 11881
          %force% %partner% say Not that you could see it from down here the way the glow reflects off the mist.
        else
          %force% %partner% say Who would even know? Nobody has been up that high in ages. You'd almost forget we can fly.
        end
      break
      case 5
        * Having been told the whole thing is a dream, they respond with a non-sequitur as if they heard something else.
        if %partner.vnum% == 11881
          %force% %partner% say She told me it's going to be a boy.
        else
          %force% %partner% say Really? I heard it was the mushroom soup.
        end
      break
    done
  end
  * no wait 1
  * In the seashell, check if they're interrupting the Tresydion
  if %self.room.template% == 11882
    set tresydion %instance.mob(11882)%
    if %tresydion% && %tresydion.varexists(talking)%
      %echo% The tresydion makes a shushing noise.
    end
  end
end
~
#11876
Smol Nes-Pik: Reset comment count on move~
0 i 100
~
* pairs with trigger 11880 etc to reset their commentary when they move
set comment 0
remote comment %self.id%
~
#11877
Smol Nes-Pik: Joiago gossip and mirth driver~
0 bw 50
~
* pairs with trigger 11876, 11875, 11883
* check pickpocket
if %self.mob_flagged(*PICKPOCKETED)%
  attach 11883 %self.id%
  detach 11877 %self.id%
  halt
end
set max_comment 4
* determine position in the comment loop
if %self.varexists(comment)%
  eval comment %self.comment% + 1
else
  set comment 1
end
if %comment% > %max_comment%
  halt
end
remote comment %self.id%
* the comments don't use 'elseif' because some comments will skip to later ones
set room %self.room%
set template %room.template%
if %comment% == 1
  * 1: location-based
  if %template% == 11879
    * [11879] Atop the Glimmering Stair
    say Fancy meeting you up here. I hope you're enjoying Smol Nes-Pik. The views up here are beyond compare.
  elseif %template% == 11880
    * [11880] Grand Overlook: fall through to comment 3
    set comment 3
  elseif %template% == 11881
    * [11881] Outside the Seashell
    switch %random.3%
      case 1
        say Ah, our most famous feature...
        emote $n gestures broadly toward the seashell.
      break
      case 2
        say If you look down, you can see the palace from here.
      break
    done
  elseif %template% == 11882
    * [11882] Great Glowing Seashell: fall through to comment 2
    set comment 2
  elseif %template% == 11883
    * [11883] Outside Obsidian Palace
    say Can't recommend going below here. The stair is broken, the tree is ill, and those bleak-brained rot worshippers are praising both.
  end
end
* store the comment again, in case it changed
remote comment %self.id%
* continuing...
if %comment% == 2
  * 2. gossip with someone present
  joiago gossip
end
if %comment% == 3
  * 3. musical instrument: this one can actually change rooms
  %echo% ~%self% pulls a little gemstone flute from ^%self% pocket and begins to play...
  wait 7 sec
  * must check if the Tresydion is orating
  set tresydion %instance.mob(11882)%
  set dance_list 11878 11880 11881
  * begin musical loop
  set music %random.3%
  set loop 1
  while %loop% < 10
    * check pickpocket again
    if %self.mob_flagged(*PICKPOCKETED)%
      attach 11883 %self.id%
      detach 11877 %self.id%
      halt
    end
    if %tresydion% && %tresydion.varexists(talking)%
      * always play along if the tresydion is talking
      switch %random.3%
        case 1
          %echo% ~%self% plays a soft, reverent tune.
        break
        case 2
          %echo% ~%self% plays along to the tresydion's speech.
        break
        case 3
          %echo% ~%self% plays ^%self% flute quietly.
        break
      done
    elseif %music% == 1
      * song 1: soft, sad melody
      switch %loop%
        case 1
          %echo% ~%self% plays a soft, sad melody...
        break
        case 2
          %echo% The melancholy notes from the flute dance around the tree...
        break
        case 3
          %echo% |%self% music makes you close your eyes for a moment, and imagine yourself floating on a bed of clouds...
        break
        case 4
          %echo% The world around you seems to disappear for a moment...
        break
        case 5
          %echo% The music from |%self% flute rises, like the glimmering staircase...
        break
        case 6
          %echo% The music dissolves into dissonant notes, sinking into the haze...
        break
        case 7
          %echo% The soft, sad music wells up from below, louder, brighter, and faster, until it seems to stream back into the little flute...
        break
        case 8
          %echo% ~%self% dances backward slowly as &%self% plays...
        break
        case 9
          %echo% The tune ends with a flourish and ~%self% takes a little bow.
        break
      done
    elseif %music% == 2
      * song 2: jaunty dance
      switch %loop%
        case 1
          %echo% ~%self% starts a jaunty tune...
        break
        case 2
          %echo% ~%self% begins to dance about as &%self% plays...
        break
        case 3
          %echo% The staccato notes seem to escape the little flute and begin to dance about on their own...
        break
        case 4
          if %template% == 11881 || %template% == 11882
            %echo% The seashell pulses along to the music as the notes dance around on its curves...
          else
            %echo% The bricks and blocks light up as the notes dance around on them...
          end
        break
        case 5
          %echo% The merry melody makes your legs tingle and pull, as if they want to move...
        break
        case 6
          %echo% ~%self% twirls around, playing the flute, but the music has taken on a life beyond its instrument...
        break
        case 7
          %echo% The music notes flit around you, pulling you in circles, making everyone dance...
        break
        case 8
          %echo% ~%self% begins moving faster and faster, pursuing each escaped note and pulling it back into the flute...
        break
        case 9
          %echo% ~%self% twirls into a full bow as the music dances back into the flute at long last.
        break
      done
      * dancers
      set ch %room.people%
      while %ch%
        if %ch.is_npc% && %dance_list% ~= %ch.vnum%
          %echo% ~%ch% dances along.
        end
        set ch %ch.next_in_room%
      done
    elseif %music% == 3
      * song 3: history of the tree
      switch %loop%
        case 1
          %echo% As ~%self% begins a gentle tune on the flute, the haze seems to swirl around you...
        break
        case 2
          %echo% The flute music gyrates and ripples through the haze...
        break
        case 3
          %echo% ~%self% walks slowly backward, leading the music in a circle as it follows ^%self% flute...
        break
        case 4
          %echo% You stare at the trunk of the great tree as the music paints a picture of it young, fresh, vibrant...
        break
        case 5
          %echo% As the gentle melody is teased out of |%self% flute, you imagine the tree growing, reaching toward the sky even as a tiny staircase grows around it...
        break
        case 6
          %echo% One by one, the gentle notes from the flute drop from the branches of the tree, as mushrooms, moss, and lichen climb the stairs...
        break
        case 7
          %echo% You lean back, almost floating on the music, as you watch the tree in your mind, imagining every moment of its history...
        break
        case 8
          %echo% The decresendo builds as the notes fragment and the tree begins to fade as the trunk is choked and the staircase breaks...
        break
        case 9
          %echo% And then the music ends, suddenly, on a discordant note. ~%self% takes a little bow, and a tear escapes his eye.
        break
      done
    end
    eval loop %loop% + 1
    wait 7 sec
  done
  %echo% ~%self% slides the flute back into ^%self% pocket.
end
if %comment% == 4
  * 4. sleep hints
  switch %random.4%
    case 1
      say Think of us when you wake up, will you?
    break
    case 2
      say This may be your dream, but it was our nightmare.
    break
    case 3
      say Things weren't good before Iskip came, but at least we were free.
    break
    case 4
      say Remember to dance occasionally, when you're awake. It's no good if you only do it in dreams.
    break
  done
end
* force a delay at the end
wait 20 s
~
#11878
Smol Nes-Pik: Keeper Bastain greeting~
0 gw 90
~
if %direction% == none
  halt
end
wait 1
if %actor.room% == %self.room%
  %send% %actor% ~%self% greets you.
  %echoaround% %actor% ~%self% greets ~%actor%.
  * possible response
  wait 1
  if %actor.room% == %self.room% && %random.2% == 2
    switch %actor.vnum%
      case 11877
        * Joiago
        %force% %actor% say Greetings.
      break
      case 11880
        * Lotte
        %force% %actor% say Everyone is so friendly here.
      break
      case 11881
        * Kym
        %echoaround% %actor% ~%actor% nods at ~%self%.
      break
    done
  end
end
~
#11879
Smol Nes-Pik: Broken ladder drop~
2 gwA 100
~
* always returns 1
return 1
if %direction% != up
  halt
end
* detect who is already in the room
set ch %room.people%
while %ch%
  if %ch.is_pc%
    set have%ch.id% 1
  end
  set ch %ch.next_in_room%
done
* very short delay for messaging
wait 1
* now message to all new players present
set ch %room.people%
while %ch%
  if %ch.is_pc%
    eval check %%have%ch.id%%%
    if %ch% == %actor% || !%check%
      %send% %ch% The ladder breaks as you climb down and you come crashing down onto the stair!
      %send% %ch% It doesn't look like there's a way back up.
    end
  end
  set ch %ch.next_in_room%
done
~
#11880
Smol Nes-Pik: Lotte running commentary~
0 bw 45
~
* pairs with trigger 11876
set max_comment 4
* determine position in the comment loop
if %self.varexists(comment)%
  eval comment %self.comment% + 1
else
  set comment 1
end
if %comment% > %max_comment%
  halt
end
remote comment %self.id%
* comment based on location, except the first one
set room %self.room%
set template %room.template%
if %comment% == %max_comment%
  * final echo is a sleep hint
  switch %random.4%
    case 1
      %echo% Lotte turns to you and says, 'You're still asleep.'
    break
    case 2
      say Hey, wake up.
    break
    case 3
      %echo% Lotte snores.
    break
    * no case 4 -- just no hint
  done
elseif %template% == 11879
  * [11879] Atop the Glimmering Stair
  switch %comment%
    case 1
      %echo% Lotte seems to be out of breath from all the stairs.
    break
    case 2
      say Everyone here has been so friendly.
    break
    case 3
      say I don't think this tree is looking so good.
    break
  done
elseif %template% == 11880
  * [11880] Grand Overlook
  switch %comment%
    case 1
      say Oh wow, you can see the whole city from up here!
    break
    case 2
      say I don't think Tagra Kai is this tall.
    break
    case 3
      say You get a little woozy looking over that edge!
    break
  done
elseif %template% == 11881
  * [11881] Glimmering Staircase
  switch %comment%
    case 1
      say Oh, wow, the whole thing is glowing!
    break
    case 2
      say Ours isn't like this. I mean, it's not even green.
    break
    case 3
      %echo% Lotte runs her hand over the outside of the carved giant seashell.
      wait 1
      if %self.room% == %room%
        say Wait, am I allowed to touch it?
      end
    break
  done
elseif %template% == 11882
  * [11882] Great Green Seashell
  switch %comment%
    case 1
      %echo% Lotte carefully gets down onto one of the rugs and kneels until her forehead almost touches the water.
      wait 2 sec
      if %self.room% == %room%
        %echo% Lotte gets back up and brushes herself off.
      end
    break
    case 2
      say I just love the way the water looks on the ceiling.
    break
    case 3
      say You all just look so green in this light.
    break
  done
  * also check if the Tresydion is orating
  set tresydion %instance.mob(11882)%
  if %tresydion% && %tresydion.varexists(talking)%
    wait 1
    if %self.room% == %room%
      %echo% Lotte says, 'Oops, sorry,' under her voice, then shushes herself.
    end
  end
elseif %template% == 11883
  * [11883] Glimmering Staircase
  switch %comment%
    case 1
      say Bet they lose the palace in the dark a lot.
    break
    case 2
      say Do they just let anyone walk in?
    break
    case 3
      say Shoddy repair work here. It looks like the stairs just end down there.
    break
  done
end
* force a delay at the end
wait 20 s
~
#11881
Stasis charm: Block morph and fastmorph commands when worn~
1 c 1
morph fastmorph~
if %arg.car%
  %send% %actor% You can't seem to morph! Something must be preventing it.
  return 1
else
  return 0
end
~
#11882
Smol Nes-Pik: Tresydion orations~
0 bw 25
~
set max_comment 3
set mob_list 11877 11878 11880 11881 11883
* determine position in the comment loop
if %self.varexists(comment)%
  eval comment %self.comment% + 1
else
  set comment 0
end
if %comment% > %max_comment%
  set comment 0
end
remote comment %self.id%
* turn on oration
set talking 1
remote talking %self.id%
* has a series of behaviors based on the entry in the loop
if %comment% == 0
  * oration not in english
  set cycle 0
  while %cycle% < 7
    switch %cycle%
      case 0
        say Ebru sal-ash Tagra Nes.
      break
      case 1
        say Dolo carzo, dolo olo, owri carzo Tagra Nes.
      break
      case 2
        say Breelo sal-ash Tagra Nes.
      break
      case 3
        say A wo owri avagor.
      break
      case 4
        say Ebru sal-ash Tagra Nes.
      break
      case 5
        say Ebru sal-ash corun smol.
      break
      case 6
        say Ebru sal-ash Sparo Vehl.
        wait 1
        set ch %self.room.people%
        while %ch%
          if %ch.is_npc% && %mob_list% ~= %ch.vnum%
            %force% %ch% say Ebru sal-ash Sparo Vehl.
          end
          set ch %ch.next_in_room%
        done
      break
    done
    eval cycle %cycle% + 1
    wait 9 sec
  done
elseif %comment% == 1
  * doing the directions
  set cycle 0
  while %cycle% < 10
    switch %cycle%
      case 0
        %echo% ~%self% walks slowly to the east end of the shell, stepping with his right foot first and then bringing his left foot to meet it.
      break
      case 1
        say Sacred is the East, where the dawn breaks and casts out the dark.
      break
      case 2
        %echo% ~%self% walks to the west end of the shell, beside the doorway.
      break
      case 3
        say Sacred is the West, dwelling of the sun.
      break
      case 4
        %echo% ~%self% walks to the south wall of the shell and raises his arms.
      break
      case 5
        say Sacred is the South, heart of winter and font of perpetual renewal.
      break
      case 6
        %echo% ~%self% walks to the pool in the center of the room.
      break
      case 7
        say Sacred is the dew, gem of morning, let it quench you.
      break
      case 8
        %echo% ~%self% kneels and scoops a handful of dew, and drinks it.
        wait 1
        set ch %self.room.people%
        while %ch%
          if %ch.is_npc% && %mob_list% ~= %ch.vnum%
            %echo% ~%ch% kneels and drinks from the pool.
          end
          set ch %ch.next_in_room%
        done
      break
      case 9
        say Let it quench you.
      break
    done
    eval cycle %cycle% + 1
    wait 9 sec
  done
elseif %comment% == 2
  * cleaning cycle (turn off oration; can be interrupted)
  rdelete talking %self.id%
  set cycle 0
  while %cycle% < 8
    switch %cycle%
      case 0
        %echo% ~%self% pulls a silk cloth from his pocket and dips it in the shimmering pool.
      break
      case 1
        %echo% ~%self% begins cleaning the walls with his rag.
      break
      case 2
        %echo% ~%self% scrubs the fine carvings of the seashell.
      break
      case 3
        %echo% ~%self% dips his rag in the pool and squeezes it out.
      break
      case 4
        %echo% ~%self% washes scuffmarks off the floor with his rag.
      break
      case 5
        %echo% ~%self% wipes down the edges of the pool, stopping to clean the rag from time to time.
      break
      case 6
        %echo% ~%self% sits back on his heels and wipes his brow.
      break
      case 7
        %echo% ~%self% hangs the rag to dry by the door.
      break
    done
    eval cycle %cycle% + 1
    wait 9 sec
  done
elseif %comment% == 3
  * just a pause (turn off oration; can be interrupted)
  rdelete talking %self.id%
  wait 60 sec
end
* turn off oration
rdelete talking %self.id%
wait 30 sec
~
#11883
Smol Nes-Pik: Joiago gossip and humming driver, when pickpocketed~
0 bw 50
~
* pairs with trigger 11876, 11875, 11877
set max_comment 4
* determine position in the comment loop
if %self.varexists(comment)%
  eval comment %self.comment% + 1
else
  set comment 1
end
if %comment% > %max_comment%
  halt
end
remote comment %self.id%
* the comments don't use 'elseif' because some comments will skip to later ones
set room %self.room%
set template %room.template%
if %comment% == 1
  * 1: location-based
  if %template% == 11879
    * [11879] Atop the Glimmering Stair
    say Fancy meeting you up here. I hope you're enjoying Smol Nes-Pik. The views up here are beyond compare.
  elseif %template% == 11880
    * [11880] Grand Overlook: fall through to comment 3
    set comment 3
  elseif %template% == 11881
    * [11881] Outside the Seashell
    switch %random.3%
      case 1
        say Ah, our most famous feature...
        emote $n gestures broadly toward the seashell.
      break
      case 2
        say If you look down, you can see the palace from here.
      break
    done
  elseif %template% == 11882
    * [11882] Great Glowing Seashell: fall through to comment 2
    set comment 2
  elseif %template% == 11883
    * [11883] Outside Obsidian Palace
    say Can't recommend going below here. The stair is broken, the tree is ill, and those bleak-brained rot worshippers are praising both.
  end
end
* store the comment again, in case it changed
remote comment %self.id%
* continuing...
if %comment% == 2
  * 2. gossip with someone present
  joiago gossip
end
if %comment% == 3
  * 3. humming along: this one can actually change rooms
  * must check if the Tresydion is orating
  set tresydion %instance.mob(11882)%
  * begin musical loop
  set music %random.2%
  set loop 1
  while %loop% < 10
    if %tresydion% && %tresydion.varexists(talking)%
      * always play along if the tresydion is talking
      switch %random.3%
        case 1
          %echo% ~%self% hums quietly to *%self%self.
        break
        case 2
          %echo% ~%self% hums along in time with the tresydion's speech.
        break
        case 3
          %echo% ~%self% hums to *%self%self.
        break
      done
    elseif %music% == 1
      * song 1: soft, sad melody
      switch %random.4%
        case 1
          %echo% ~%self% hums a soft, sad melody.
        break
        case 2
          %echo% ~%self% skips along slowly as &%self% hums.
        break
        case 3
          %echo% ~%self% looks around wistfully as &%self% hums.
        break
        case 4
          %echo% You find yourself humming along with ~%self%.
        break
      done
    elseif %music% == 2
      * song 2: jaunty dance
      switch %loop%
        case 1
          %echo% ~%self% starts humming jaunty tune...
        break
        case 2
          %echo% ~%self% begins to dance about as &%self% hums...
        break
        case 3
          %echo% ~%self% whistles a staccato tune...
        break
        case 4
          %echo% ~%self% dances back and forth as &%self% whistles...
        break
        case 5
          %echo% The merry melody makes your legs tingle and pull, as if they want to move...
        break
        case 6
          %echo% ~%self% twirls around, whistling, dancing in circles...
        break
        case 7
          %echo% You find yourself dancing with ~%self% as &%self% whistles...
        break
        case 8
          %echo% ~%self% begins moving faster and faster, whistling almost too fast to keep up!
        break
        case 9
          %echo% ~%self% twirls into a full bow as the tune comes to a dramatic end.
        break
      done
    end
    eval loop %loop% + 1
    wait 7 sec
  done
end
if %comment% == 4
  * 4. sleep hints
  switch %random.4%
    case 1
      say Think of us when you wake up, will you?
    break
    case 2
      say This may be your dream, but it was our nightmare.
    break
    case 3
      say Things weren't good before Iskip came, but at least we were free.
    break
    case 4
      say Remember to dance occasionally, when you're awake. It's no good if you only do it in dreams.
    break
  done
end
* force a delay at the end
wait 20 s
~
#11884
Rot and Ruin: Taste putrid sap to teleport~
1 c 4
taste eat drink sip lick~
* This teleports players between templates 11887 and 11891
* Note: See below for adding requirements to use it
* 1. Basic checks
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
elseif %actor.position% != Standing
  %send% %actor% You need to be standing to taste the sap.
  halt
elseif %actor.fighting% || %actor.disabled%
  %send% %actor% You can't get to the sap right now!
  halt
end
* 2. Find target
set room %self.room%
if %room.template% == 11887
  set target %instance.nearest_rmt(11891)%
  set intro 1
else
  set target %instance.nearest_rmt(11887)%
  set intro 0
end
if !%target%
  * Note: this could log the error... but it shouldn't be possible anyway
  %send% %actor% It just tastes rotten.
  halt
end
* 3. Future use: Restrictions on who can use it, e.g. based on faction
* 4. Teleport player
%send% %actor% Before you can stop yourself, you reach out and %cmd% the sap...
%send% %actor% Everything goes black...
%echoaround% %actor% ~%actor%'s eyes go hazy as &%actor% reaches out and %cmd%s the putrid sap.
%echoaround% %actor% ~%actor% falls unconscious into the sap and vanishes beneath its sticky surface.
%teleport% %actor% %target%
if %intro%
  * player is in the intro chamber...
  dg_affect #11891 %actor% HARD-STUNNED on 104
else
  * player was exiting the sap
  %force% %actor% look
  %echoaround% %actor% ~%actor% emerges from the putrid sap.
end
* 5. Teleport followers
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_npc% && %ch.leader% == %actor% && !%ch.fighting% && !%ch.disabled%
    %send% %ch% You follow ~%actor% into the sap...
    %echoaround% %ch% ~%ch% follows ~%actor% into the sap...
    %teleport% %ch% %target%
    if !%intro%
      %force% %ch% look
      %echoaround% %ch% ~%ch% emerges from the putrid sap.
    end
  end
  set ch %next_ch%
done
* 6. If in room 11887, also set a decay timer on this sap
if %self.room.template% == 11887
  otimer 4
end
~
#11885
Skycleave: Struggle to escape~
1 c 2
*~
* runs on an obj in inventory; strength/intelligence help break out faster
* uses optional string vars: strug_char, strug_room, free_char, free_room
return 1
if !%actor.affect(11822)%
  * check on ANY command
  if struggle /= %cmd%
    %send% %actor% You don't need to struggle right now.
  else
    return 0
  end
  %purge% %self%
  halt
elseif !(%cmd% /= struggle)
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
  set char_msg %self.var(free_char,You manage to break out!)%
  %send% %actor% %char_msg.process%
  set room_msg %self.var(free_room,~%actor% struggles and manages to break out!)%
  %echoaround% %actor% %room_msg.process%
  set did_skycleave_struggle 1
  remote did_skycleave_struggle %self.id%
  nop %actor.command_lag(COMBAT-ABILITY)%
  dg_affect #11822 %actor% off
  %purge% %self%
else
  * the struggle continues
  set char_msg %self.var(strug_char,You struggle to break free...)%
  %send% %actor% %char_msg.process%
  set room_msg %self.var(strug_room,~%actor% struggles, trying to break free...)%
  %echoaround% %actor% %room_msg.process%
  nop %actor.command_lag(COMBAT-ABILITY)%
  remote struggle_counter %self.id%
end
~
#11886
Skycleave: Attack the statue to destroy it~
1 c 4
break kill destroy shatter hit kick bash smash stab jab backstab attack~
set target %actor.obj_target(%arg%)%
if !%target%
  %send% %actor% Break what?
  halt
end
if %target% != %self%
  %send% %actor% You can't break %target.shortdesc%.
  halt
end
set room %actor.room%
set cycles_left 3
while %cycles_left% >= 0
  if (%actor.room% != %room%) || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 3
      %echoaround% %actor% |%actor% combat is interrupted.
      %send% %actor% Your combat is interrupted.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake combat messages
  set weapon %actor.eq(wield)%
  switch %cycles_left%
    case 3
      if !%weapon%
        %echoaround% %actor% ~%actor% puts ^%actor% fists up and prepares to attack %self.shortdesc%...
        %send% %actor% You put your fists up and prepare to attack %self.shortdesc%...
      else
        %echoaround% %actor% ~%actor% readies %weapon.shortdesc% and prepares to attack %self.shortdesc%...
        %send% %actor% You ready %weapon.shortdesc% and prepare to attack %self.shortdesc%...
      end
    break
    case 2
      if !%weapon%
        %send% %actor% You punch %self.shortdesc%, sending cracks spreading across its surface!
        %echoaround% %actor% ~%actor% punches %self.shortdesc%, sending cracks spreading across its surface!
      elseif %weapon.attack(damage)% == physical
        %send% %actor% You %weapon.attack(1)% %self.shortdesc%, sending cracks spreading across its surface!
        %echoaround% %actor% ~%actor% %weapon.attack(3)% %self.shortdesc%, sending cracks spreading across its surface!
      else
        %echoaround% %actor% ~%actor% whacks %self.shortdesc% with %weapon.shortdesc%, cracking it slightly.
        %send% %actor% You whack %self.shortdesc% with %weapon.shortdesc%, cracking it slightly.
      end
    break
    case 1
      if !%weapon%
        %send% %actor% You punch %self.shortdesc%, breaking its arm off!
        %echoaround% %actor% ~%actor% punches %self.shortdesc%, breaking its arm off!
      elseif %weapon.attack(damage)% == physical
        %send% %actor% You %weapon.attack(1)% %self.shortdesc%, breaking its arm off!
        %echoaround% %actor% ~%actor% %weapon.attack(3)% %self.shortdesc%, breaking its arm off!
      else
        %echoaround% %actor% ~%actor% steps back and blasts %self.shortdesc% with a bolt from %weapon.shortdesc%, blowing its arm off!
        %send% %actor% You step back and blast %self.shortdesc% with a bolt from %weapon.shortdesc%, blowing its arm off!
      end
    break
    case 0
      if !%weapon%
        %send% %actor% You strike %self.shortdesc% with all your might, shattering it completely!
        %echoaround% %actor% ~%actor% punches %self.shortdesc% with a mighty roar, shattering it completely!
      else
        %send% %actor% You %weapon.attack(1)% %self.shortdesc% with all your might, shattering it completely!
        %echoaround% %actor% ~%actor% %weapon.attack(3)% %self.shortdesc% with a mighty roar, shattering it completely!
      end
      * Leave the loop
    break
  done
  if %cycles_left% > 0
    wait 3 sec
  end
  eval cycles_left %cycles_left% - 1
done
* mark who did this
set spirit %instance.mob(11900)%
set finish2 %actor.real_name%
remote finish2 %spirit.id%
* and phase transition
%load% mob 11896
%purge% %self%
~
#11887
Skycleave: Move-blocking object - Pixy Shield, Shadow Wall, Vortex~
1 q 100
~
* One quick trick to get the target room
set room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
switch %self.vnum%
  case 11863
    if %instance.mob(11869)%
      %send% %actor% You can't seem to get past %self.shortdesc%! You need to defeat the Shade of Mezvienne first!
    else
      %send% %actor% You can't seem to get past %self.shortdesc%! You need to defeat the Shadow Ascendant first!
    end
  break
  case 11887
    %send% %actor% You can't seem to get past %self.shortdesc%! You need to defeat the Pixy Queen first!
  break
  case 11870
    %send% %actor% The swirling vortex prevents you from climbing the stairs.
  break
  default
    %send% %actor% You can't seem to get past %self.shortdesc%!
  break
done
return 0
~
#11888
Skycleave: skygobpix trash loader~
0 c 0
skygobpix~
* Usage: skygobpix <difficulty 1-4> <a (any) | g (goblin) | p (pixy)>
if %actor% != %self%
  halt
end
set diff %arg.car%
set temp %arg.cdr%
set type %temp.car%
if !(%diff%)
  set diff 1
end
if %diff% > 2
  set amount 2
else
  set amount 1
end
if %type% == a
  if %random.2% == 2
    set type p
  else
    set type g
  end
end
switch %type%
  case g
    * Goblins: loads a group of mobs in the vnum range 11815, 11816, 11817
    set iter 0
    while %iter% < %amount%
      eval vnum 11814 + %random.3%
      %load% mob %vnum%
      set mob %self.room.people%
      if %mob.vnum% == %vnum%
        remote diff %mob.id%
      end
      eval iter %iter% + 1
    done
  break
  case p
    * Pixies: mob 11820
    set iter 0
    while %iter% < %amount%
      %load% mob 11820
      set mob %self.room.people%
      if %mob.vnum% == 11820
        remote diff %mob.id%
      end
      eval iter %iter% + 1
    done
  break
done
~
#11889
Skycleave: Skydel despawner~
0 c 0
skydel~
* Usage: skydel <vnum> <msg type>
*  msg 1: Name heads upstairs.
*  msg 2: Name goes back to work.
if %actor% != %self%
  halt
end
set vnum %arg.car%
set temp %arg.cdr%
set msg %temp.car%
if !(%vnum%)
  halt
end
set mob %instance.mob(%vnum%)%
if %mob%
  switch %msg%
    case 1
      %at% %mob.room% %echo% ~%mob% heads upstairs.
    break
    case 2
      %at% %mob.room% %echo% ~%mob% goes back to work.
    break
    case 3
      %at% %mob.room% %echo% ~%mob% heads downstairs.
    break
    default
      %at% %mob.room% %echo% ~%mob% leaves.
    break
  done
  %purge% %mob%
end
~
#11890
Skycleave: Difficulty selector floor 1 to 2~
0 c 0
difficulty up~
* Process argument
if %cmd.mudcommand% == up
  %send% %actor% ~%self% won't let you pass until you set a difficulty for the second floor.
  return 1
  halt
elseif !%arg%
  %send% %actor% You must specify a level of difficulty (normal, hard, group, or boss).
  return 1
  halt
end
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set diff 1
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set diff 2
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  set diff 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  set diff 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Messaging
%echo% ~%self% drops the protective wards and breathes a sigh of relief.
if !%self.room.up(room)%
  %door% %self.room% u room i11810
end
* Remove ward
set ward %self.room.contents(11831)%
if %ward%
  %purge% %ward%
end
* Store difficulty
set spirit %instance.mob(11900)%
set diff2 %diff%
remote diff2 %spirit.id%
set start2 %actor.real_name%
remote start2 %spirit.id%
* Load mobs
%at% i11810 skygobpix %diff% g  * trash mob
%at% i11811 skygobpix %diff% g  * trash mob
%at% i11812 skygobpix %diff% a  * trash mob
%at% i11813 skygobpix %diff% g  * trash mob
%at% i11814 skygobpix %diff% g  * trash mob
%at% i11816 %load% mob 11812  * Apprentice Cosimo (hiding)
%at% i11818 skyload 11819 %diff%  * Pixy Boss
%at% i11819 skygobpix %diff% a  * trash mob
%at% i11820 skygobpix %diff% p  * trash mob
%at% i11821 skygobpix %diff% a  * trash mob
%at% i11822 %load% mob 11810  * Watcher Annaca (phase A dummy)
%at% i11823 skygobpix %diff% p  * trash mob
%at% i11824 skygobpix %diff% a  * trash mob
%at% i11825 skyload 11818 %diff%  * Goblin Boss
%load% mob 11864  * Replacement Celiya
if %self.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11864)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
%purge% %self%
~
#11891
Skycleave: Difficulty selector floor 2 to 3~
0 c 0
difficulty up~
* Process argument
if %cmd.mudcommand% == up
  %send% %actor% ~%self% won't let you pass until you set a difficulty for the third floor.
  return 1
  halt
elseif !%arg%
  %send% %actor% You must specify a level of difficulty (normal, hard, group, or boss).
  return 1
  halt
end
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set diff 1
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set diff 2
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  set diff 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  set diff 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Messaging
%echo% ~%self% cancels ^%self% mana shield and steps away from the staircase.
if !%self.room.up(room)%
  %door% %self.room% u room i11830
end
* Remove ward
set ward %self.room.contents(11831)%
if %ward%
  %purge% %ward%
end
* Store difficulty
set spirit %instance.mob(11900)%
set diff3 %diff%
remote diff3 %spirit.id%
set start3 %actor.real_name%
remote start3 %spirit.id%
* Load mobs
%at% i11834 %load% mob 11830  * High Master Caius (phase A dummy)
%at% i11835 %load% mob 11834  * Otherworlder (chained)
%at% i11835 %load% mob 11835  * Lab assistant
%at% i11835 skyload 11847 %diff%  * Merc Caster boss
%at% i11839 skyload 11848 %diff%  * Merc Rogue boss
%at% i11837 skyload 11849 %diff%  * Merc Leader boss
%at% i11830 skymerc %diff%  * Load set of mercenaries: hall
%at% i11831 skymerc %diff%  *   hall
%at% i11832 skymerc %diff%  *   hall
%at% i11833 skymerc %diff%  *   hall
%at% i11836 skymerc %diff%  *   Lich Labs
%at% i11840 %load% mob 11831  * Resident Niamh
%at% i11840 %load% mob 11840  * Magineer
%load% mob 11910  * Replacement Watcher
if %self.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11910)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
%purge% %self%
~
#11892
Skycleave: Difficulty selector floor 3 to 4~
0 c 0
difficulty up~
* Process argument
if %cmd.mudcommand% == up
  %send% %actor% ~%self% won't let you pass until you set a difficulty for the fourth floor.
  return 1
  halt
elseif !%arg%
  %send% %actor% You must specify a level of difficulty (normal, hard, group, or boss).
  return 1
  halt
end
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set diff 1
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set diff 2
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  set diff 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  set diff 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Messaging
%echo% ~%self% drops ^%self% wards and warns you to be careful upstairs.
if !%self.room.up(room)%
  %door% %self.room% u room i11860
end
* Remove ward
set ward %self.room.contents(11831)%
if %ward%
  %purge% %ward%
end
* Store difficulty
set spirit %instance.mob(11900)%
set diff4 %diff%
remote diff4 %spirit.id%
set start4 %actor.real_name%
remote start4 %spirit.id%
* Load mobs
%at% i11865 %load% mob 11859  * Page Sheila
%at% i11865 %load% mob 11862  * Apprentice Thorley
%at% i11866 skyload 11867 %diff%  * Mind-controlled Barrosh
%at% i11867 %load% mob 11860  * Captive assistant
%at% i11867 %load% mob 11851  * Captive page Paige
%at% i11868 skyload 11869 %diff%  * Shade of Mezvienne
%at% i11868 %load% mob 11868  * Injured Knezz
%at% i11871 skyload 11871 %diff%  * Mezvienne the Enchantress
%load% mob 11930  * Replacement Caius
if %self.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11930)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
%purge% %self%
~
#11893
Skycleave: Dummy difficulty selector~
0 c 0
difficulty~
%send% %actor% You must complete this floor before you can select a difficulty for the next one.
~
#11894
Skycleave: Phase Change 1 Room~
0 hn 100
~
* Teleport all players/followers from 1 phase to the 2nd phase
* And despawn all adventure mobs here
if %actor.nohassle%
  halt
end
wait 0
set room %self.room%
if %room.template% < 11801 || %room.template% > 11899
  * Only works in phase 1 of Skycleave
  %purge% %self%
  halt
end
* Ensure part of the instance
nop %self.link_instance%
eval to_vnum %room.template% + 100
set to_room %instance.nearest_rmt(%to_vnum%)%
if !%to_room%
  * No destination
  %purge% %self%
  halt
end
* Message now
%echo% This part of Skycleave has been restored!
* Check items here
set obj %room.contents%
while %obj%
  set next_obj %obj.next_in_list%
  if %obj.can_wear(TAKE)%
    %teleport% %obj% %to_room%
  end
  set obj %next_obj%
done
* Check people here
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.nohassle% || (%ch.vnum% >= 11894 && %ch.vnum% <= 11899)
    * nothing (11894-11899 are mobs involved in phase change)
  elseif %ch.is_pc% || !%ch.linked_to_instance%
    * Move ch
    %teleport% %ch% %to_room%
    %load% obj 11805 %ch%
    * check quest completion
    if %room.template% >= 11810 && %room.template% <= 11826
      * Floor 2
      if %ch.on_quest(11810)%
        %quest% %ch% trigger 11810
      end
      if %ch.on_quest(11826)%
        %quest% %ch% trigger 11826
      end
    elseif %room.template% >= 11830 && %room.template% <= 11841
      * Floor 3
      if %ch.on_quest(11811)%
        %quest% %ch% trigger 11811
      end
    elseif %room.template% >= 11860 && %room.template% <= 11872
      * Floor 4
      if %ch.on_quest(11812)%
        %quest% %ch% trigger 11812
      end
      * check progress
      set emp %ch.empire%
      if %emp%
        if !%emp.has_progress(11800)%
          nop %emp.add_progress(11800)%
        end
      end
    end
  elseif %ch% != %self%
    * Adventure mob: purge
    %purge% %ch%
  end
  set ch %next_ch%
done
~
#11895
Skycleave: Phase change, floor 1~
0 n 100
~
* Converts the 1st floor of Skycleave from phase A to phase B
set start_room 11801
set end_room 11808
* Verify in-Skycleave
if %self.room.template% < 11800 || %self.room.template% > 11999
  %purge% %self%
  halt
end
wait 0
* Ensure part of the instance
nop %self.link_instance%
* 1. Re-link entrance (No announcement for floor 1)
%door% i11800 north room i11901
* 2. Re-link 'down' exit from floor 2
%door% i11910 down room i11904
* 3. Mobs: Any checks based on surviving mobs go here (step 4 will purge them)
%at% i11800 %load% mob 11901  * Gossipper
%at% i11800 %load% mob 11901  * Additional Gossipper
%at% i11905 %load% mob 11827  * Marina
%at% i11905 %load% mob 11828  * Djon
%at% i11905 %load% mob 11801  * Dylane
%at% i11906 %load% mob 11908  * Student Elamm
%at% i11906 %load% mob 11909  * Student Akeldama
%at% i11904 %load% mob 11902  * Bucket (and sponge)
* Page Corwin
set corwin %instance.mob(11804)%
if %corwin%
  eval temp %corwin.room.template% + 100
  %at% i%temp% %load% mob 11904  * Page Corwin, was room i11903
  if %corwin.mob_flagged(*PICKPOCKETED)%
    set mob %instance.mob(11904)%
    nop %mob.add_mob_flag(*PICKPOCKETED)%
  end
end
* Barista Mageina
set barista %instance.mob(11805)%
%at% i11905 %load% mob 11905  * Barista Mageina
if %barista.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11905)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
* Instructor Huberus
set huberus %instance.mob(11806)%
%at% i11906 %load% mob 11906  * Instructor Huberus
if %huberus.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11906)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
* Gift Shop Keeper
set shopkeep %instance.mob(11807)%
%at% i11907 %load% mob 11907  * Gift Shop Keeper
set newshop %instance.mob(11907)%
if %shopkeep% && %newshop%
  nop %newshop.namelist(%shopkeep.namelist%)%
  if %shopkeep.mob_flagged(*PICKPOCKETED)%
    nop %newshop.add_mob_flag(*PICKPOCKETED)%
  end
end
* attach some trigs
set 11937_list 11945 11929
while %11937_list%
  set vnum %11937_list.car%
  set 11937_list %11937_list.cdr%
  set mob %instance.mob(%vnum%)%
  if %mob%
    attach 11937 %mob.id%
  end
done
* attach tourist loader
makeuid entryway room i11901
attach 11827 %entryway.id%
* 4. Move people from the old rooms
set vnum %start_room%
while %vnum% <= %end_room%
  %at% i%vnum% %load% mob 11894
  eval vnum %vnum% + 1
done
* Store phase
set spirit %instance.mob(11900)%
set phase1 1
remote phase1 %spirit.id%
%purge% %self%
~
#11896
Skycleave: Phase change, floor 2~
0 n 100
~
* Converts the 2nd floor of Skycleave from phase A to phase B
set start_room 11810
set end_room 11826
* Verify in-Skycleave
if %self.room.template% < 11800 || %self.room.template% > 11999
  %purge% %self%
  halt
end
wait 0
* Ensure part of the instance
nop %self.link_instance%
* 1. Announce
%subecho% %self.room% # The second floor of the Tower Skycleave has been rescued!
* 2. Re-link 'up' exit from floor 1
%door% i11804 up room i11910
* 3. Mobs: Any checks based on surviving mobs go here (step 4 will purge them)
set spirit %instance.mob(11900)%
skydel 11813 1  * Apprentice Tyrone 1A
%at% i11918 %load% mob 11945  * Page Amets
%at% i11921 %load% mob 11913  * Apprentice Tyrone
* Apprentice Cosimo
set cos %instance.mob(11812)%
%at% i11912 %load% mob 11912  * Apprentice Cosimo
if %cos.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11912)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
skydel 11812 2  * Apprentice Cosimo 2A
* Apprentice Kayla
set kayla %instance.mob(11811)%
%at% i11925 %load% mob 11911  * Apprentice Kayla
if %kayla.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11911)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
skydel 11811 1  * Apprentice Kayla 1A
* Apprentice Ravinder
set rav %instance.mob(11825)%
%at% i11925 %load% mob 11925  * Apprentice Ravinder
if %rav.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11925)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
skydel 11825 1  * Apprentice Ravinder 1A
* Watcher Annaca
set annaca %instance.mob(11810)%
%at% i11922 %load% mob 11891  * Watcher Annaca diff-sel
if %annaca.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11891)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
* check for any remaining goblins
set goblin_11815 0
set goblin_11816 0
set goblin_11817 0
set valid_goblin_locs 11810 11811 11813 11814 11815 11820 11823
set vnum 11815
while %vnum% <= 11817
  set mob %instance.mob(%vnum%)%
  if %mob%
    if %valid_goblin_locs% ~= %mob.room.template%
      set goblin_%vnum% 1
    end
    %purge% %mob%
  else
    eval vnum %vnum% + 1
  end
done
* place goblins
skydel 11814 1  * Old Wright
if %goblin_11817%
  set any_goblin 1
  %at% i11915 %load% mob 11917  * Caged Goblin
end
if %goblin_11816%
  set any_goblin 1
  %at% i11915 %load% mob 11916  * Caged Goblin Commando
end
if !%any_goblin%
  %at% i11915 %load% obj 11915  * empty cage
end
if %any_goblin% || %goblin_11815%
  set any_goblin 1
  %at% i11914 %load% mob 11914  * Goblin Wrangler
  %at% i11914 %load% mob 11915  * Caged Goblin
else
  * no goblins at all?
  %at% i11914 %load% mob 11889  * Goblin Wrangler, briefly
  %at% i11914 %load% obj 11915  * empty cage
end
%at% i11918 %load% mob 11918  * Race Caller
%at% i11919 %load% mob 11921  * Broom
%at% i11912 %load% mob 11922  * Duster
%at% i11910 %load% mob 11837  * Swarm of Rags
* 4. Move people from the old rooms
set vnum %start_room%
while %vnum% <= %end_room%
  %at% i%vnum% %load% mob 11894
  eval vnum %vnum% + 1
done
* 5. Update a description
makeuid hall room i11911
if %any_goblin%
  %mod% %hall% append-description Unsettling sounds and high-pitched shrieks come from a sturdy door with a no-nonsense sign above it marked in red lettering.
else
  %mod% %hall% append-description A sturdy oak door bears a no-nonsense sign marked in red lettering.
end
* Store phase
set spirit %instance.mob(11900)%
set phase2 1
remote phase2 %spirit.id%
%purge% %self%
~
#11897
Skycleave: Phase change, floor 3~
0 n 100
~
* Converts the 3rd floor of Skycleave from phase A to phase B
set start_room 11830
set end_room 11841
* Verify in-Skycleave
if %self.room.template% < 11800 || %self.room.template% > 11999
  %purge% %self%
  halt
end
wait 0
* Ensure part of the instance
nop %self.link_instance%
* 1. Announce
%subecho% %self.room% # The third floor of the Tower Skycleave has been rescued!
* 2. Re-link 'up' exit from floor 2
%door% i11922 up room i11930
* 3. Mobs: Any checks based on surviving mobs go here (step 4 will purge them)
%at% i11932 %load% mob 11933  * Walking Mop
%at% i11930 %load% mob 11837  * Swarm of Rags
skydel 11833 0  * Goef the shimmer
%at% i11941 %load% mob 11941  * Goef the Attuner
set niamh %instance.mob(11831)%
if %niamh%
  %at% i11931 %load% mob 11931  * Resident Niamh (if she survived)
end
%at% i11932 %load% mob 11978  * moth
* High Master Caius
set caius %instance.mob(11830)%
%at% i11934 %load% mob 11892  * High Master Caius diff-sel
if %caius.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11892)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
* Resident Mohammed
set moh %instance.mob(11832)%
%at% i11935 %load% mob 11932  * Resident Mohammed
if %moh.mob_flagged(*PICKPOCKETED)%
  set mob %instance.mob(11932)%
  nop %mob.add_mob_flag(*PICKPOCKETED)%
end
skydel 11832 1  * Resident Mohammed
* Otherworlder and Sanjiv
if %instance.mob(11834)%
  %at% i11935 %load% mob 11934  * Chained Otherworlder (if it survived/stayed)
end
set sanjiv %instance.mob(11835)%
if %sanjiv%
  if %instance.mob(11834)%
    %at% i11935 %load% mob 11935  * Apprentice Sanjiv, scanning
    set mob %instance.mob(11935)%
  else
    %at% i11935 %load% mob 11942  * Apprentice Sanjiv, no otherworlder
    set mob %instance.mob(11942)%
  end
  if %mob% && %sanjiv.mob_flagged(*PICKPOCKETED)%
    nop %mob.add_mob_flag(*PICKPOCKETED)%
  end
end
%at% i11936 %load% mob 11936  * Scaldorran
skydel 11838 1  * Ghost
%at% i11938 %load% mob 11938  * Ghost
* Magineer Waltur and Enchanter Annelise
set waltur %instance.mob(11840)%
if %waltur%
  %at% i11940 %load% mob 11940  * Magineer Waltur (if he survived)
  if %waltur.mob_flagged(*PICKPOCKETED)%
    set mob %instance.mob(11940)%
    nop %mob.add_mob_flag(*PICKPOCKETED)%
  end
  %at% i11939 %load% mob 11939  * Enchanter Annelise (new pickpocket item)
else
  * no waltur
  %at% i11939 %load% mob 11919  * Enchanter Annelise (new pickpocket item)
end
skydel 11839 1  * Enchanter Annelise
* 4. Move people from the old rooms
set vnum %start_room%
while %vnum% <= %end_room%
  %at% i%vnum% %load% mob 11894
  eval vnum %vnum% + 1
done
* Store phase
set spirit %instance.mob(11900)%
set phase3 1
remote phase3 %spirit.id%
%purge% %self%
~
#11898
Skycleave: Phase change, floor 4~
0 n 100
~
* Converts the 4th floor of Skycleave from phase A to phase B
set start_room 11860
set end_room 11871
* Verify in-Skycleave
if %self.room.template% < 11800 || %self.room.template% > 11999
  %purge% %self%
  halt
end
wait 0
* Ensure part of the instance
nop %self.link_instance%
* 1. Announce
%subecho% %self.room% # The fourth floor of the Tower Skycleave has been rescued!
* 2. Re-link 'up' exit from floor 3
%door% i11934 up room i11960
* 3. Mobs: Any checks based on surviving mobs go here (step 4 will purge them)
skydel 11862 3  * Thorley 4A
%at% i11924 %load% mob 11962  * Apprentice Thorley
%at% i11965 %load% mob 11959  * Page Sheila
if %instance.mob(11861)%
  * Barrosh fight completed: captives survive
  %at% i11966 %load% mob 11966  * Assistant to Barrosh
  %at% i11933 %load% mob 11929  * page Paige
end
%at% i11967 %load% mob 11967  * Barrosh
* check knezz
set celiya %instance.mob(11864)%
set knezz %instance.mob(11868)%
if !%knezz%
  * backup knezz
  set knezz %instance.mob(11870)%
end
if %knezz%
  * Knezz survived
  %at% i11964 %load% mob 11964  * HS Celiya
  if %celiya.mob_flagged(*PICKPOCKETED)%
    set mob %instance.mob(11964)%
    nop %mob.add_mob_flag(*PICKPOCKETED)%
  end
  %at% i11964 %load% obj 11969  * Celiya's desk
  eval knezz_room %knezz.room.template% + 100
  %at% i%knezz_room% %load% mob 11968  * GHS Knezz
  %at% i11968 %load% obj 11968  * Knezz's desk
  * Mark for claw game
  set spirit %instance.mob(11900)%
  set claw4 1
  remote claw4 %spirit.id%
  * shout
  set mob %instance.mob(11968)%
  if %mob%
    %force% %mob% shout By the power of Skycleave... I HAVE THE POWER!
  end
  skydel 11864 1  * Celiya 1A
else
  * Knezz died
  %at% i11964 %load% mob 11965  * Office Cleaner
  %at% i11968 %load% mob 11969  * GHS Celiya
  %at% i11968 %load% obj 11969  * Celiya's desk
  %at% i11922 %load% obj 11923  * Celiya's unfinished portrait
  * shout
  set mob %instance.mob(11969)%
  if %mob%
    if %celiya.mob_flagged(*PICKPOCKETED)%
      nop %mob.add_mob_flag(*PICKPOCKETED)%
    end
    skydel 11864 1  * Celiya 1A
    %force% %mob% shout By the power of Skycleave... I HAVE THE POWER!
  else
    skydel 11864 1  * Celiya 1A
  end
end
%at% i11961 %load% mob 11960  * Filing Cabinet
%at% i11963 %load% mob 11961  * Bookshelf
%at% i11971 %load% obj 11970  * hendecagon fountain
* 4. Move people from the old rooms
set vnum %start_room%
while %vnum% <= %end_room%
  %at% i%vnum% %load% mob 11894
  eval vnum %vnum% + 1
done
* Store phase
set spirit %instance.mob(11900)%
set phase4 1
remote phase4 %spirit.id%
%purge% %self%
~
#11899
Skycleave: Skyload mobs on difficulty select~
0 c 0
skyload~
* Usage: skyload <mob vnum> <difficulty 1-4>
if %actor% != %self%
  halt
end
set vnum %arg.car%
set temp %arg.cdr%
set diff %temp.car%
if !(%vnum) || !(%diff%)
  halt
end
%load% mob %vnum%
set mob %self.room.people%
if %mob.vnum% != %vnum%
  halt
end
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
remote diff %mob.id%
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
~
$
