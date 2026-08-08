#12200
Ribbon Serragon: Load~
0 n 100 3
L c 12201
L j 12200
L w 12200
~
dg_affect #12200 %self% !ATTACK on -1
wait 1
set istart %instance.start%
set iloc %instance.location%
if !%istart%
  halt
end
* first ever load ?
if !%istart.var(loaded)%
  *
  * mark loaded
  set loaded 1
  remote loaded %istart.id%
  *
  * check location
  if %iloc% && %self.room.template% == 12200
    * delete exit portal
    set outport %self.room.contents(12201)%
    if %outport%
      %purge% %outport%
    end
    * add exit door
    %door% %self.room% up room %iloc.vnum%
    * move me out
    mgoto %iloc%
    %echo% ~%self% pokes up from the pit!
  end
end
~
#12201
Ribbon Serragon: Down to enter pit~
1 c 4 0
down~
%force% %actor% enter pit
~
#12202
Ribbon Serragon: Death trigger and loot~
0 f 100 15
L b 12200
L b 12201
L b 12202
L c 600
L c 12200
L c 12203
L c 12204
L c 12205
L c 12206
L c 12207
L c 12208
L c 12209
L c 12214
L c 12216
L c 12220
~
if %self.vnum% != 12200
  if %instance.mob(12200)%
    halt
  end
end
if %self.vnum% != 12201
  if %instance.mob(12201)%
    halt
  end
end
if %self.vnum% != 12202
  if %instance.mob(12202)%
    halt
  end
end
* step difficulty back up on split serragons
if %self.vnum% != 12200
  switch %self.var(difficulty,1)%
    case 2
      nop %self.add_mob_flag(HARD)%
    break
    case 3
      nop %self.remove_mob_flag(HARD)%
      nop %self.add_mob_flag(GROUP)%
    break
    case 4
      nop %self.add_mob_flag(HARD)%
    break
  done
end
* load delated completer
set inside %instance.start%
if %inside%
  %at% %inside% %load% obj 12200
  * allow loot?
  if !%inside.var(loot)%
    set loot 1
    remote loot %inside.id%
    nop %self.remove_mob_flag(!LOOT)%
    *
    * LOOT: build list of vnums to load
    set load_list 0
    *
    set difficulty %self.var(difficulty,1)%
    *
    * MAIN ITEM
    eval chance %random.10000%
    if %chance% <= 2000
      * heart / building 20%
      set load_list 12220 %load_list%
    elseif %chance% <= 4000
      * dung / crop 20%
      set load_list 12203 %load_list%
    elseif %chance% <= 6000
      * pocket pants / lung 20%
      set load_list 12216 %load_list%
    elseif %chance% <= 8000
      * greatness cloak / tooth 20%
      set load_list 12214 %load_list%
    else
      * bonus scales, final 20%
      set load_list 12207 %load_list%
    end
    *
    * MOUNT?
    eval chance %random.10000%
    if %difficulty% >= 3
      * better chance on group+
      eval chance %chance% / 2
    end
    if %chance% <= 500
      * earthworm 5% / 10% group+
      set load_list 12204 %load_list%
    elseif %chance% <= 1000
      * caterpillar 5% / 10%
      set load_list 12205 %load_list%
    elseif %chance% <= 1500
      * sproutling 5% / 10%
      set load_list 12206 %load_list%
    end
    *
    * WEALTH
    eval chance %random.10000%
    if %difficulty% >= 3
      * group/boss
      if %chance% <= 1000
        * skull 10%
        set load_list 12209 %load_list%
      elseif %chance% <= 4000
        * coilstone 30%
        set load_list 12208 %load_list%
      else
        * scales 60%
        set load_list 12207 %load_list%
      end
    else
      * normal/hard
      if %chance% <= 4000
        * skull 40%
        set load_list 12209 %load_list%
      elseif %chance% <= 8000
        * coilstone 40%
        set load_list 12208 %load_list%
      else
        * scales 20%
        set load_list 12207 %load_list%
      end
    end
    *
    * SEED
    if %random.10000% <= 500
      set load_list 600 %load_list%
    end
    *
    * RUN LOAD LIST
    while %load_list%
      set vnum %load_list.car%
      set load_list %load_list.cdr%
      *
      %load% obj %vnum% %self% inv
      set loaded %self.inventory%
      * check it
      if !%loaded% || %loaded.vnum% != %vnum%
        %echo% [Ribbon Serragon] Error loading loot: %vnum%
      else
        * flags/binding
        if %loaded.is_flagged(BOP)%
          nop %loaded.bind(%self%)%
        end
        if %loaded.is_flagged(SCALABLE)%
          * hard/group flags
          if %self.mob_flagged(HARD)% && !%loaded.is_flagged(HARD-DROP)%
            nop %loaded.flag(HARD-DROP)%
          end
          if %self.mob_flagged(GROUP)% && !%loaded.is_flagged(GROUP-DROP)%
            nop %loaded.flag(GROUP-DROP)%
          end
        end
        %scale% %loaded% %self.level%
      end
    done
  end
end
~
#12203
Ribbon Serragon: Delayed completion~
1 f 0 0
~
%adventurecomplete%
~
#12204
Ribbon Serragon: Start progress goal~
2 g 100 1
L y 12200
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12200)%
end
~
#12205
Ribbon Serragon: Wander~
0 b 8 1
L j 12200
~
if %self.fighting% || %self.disabled%
  halt
elseif %self.room.template% == 12200
  %echo% ~%self% slithers up out of the pit.
  mgoto %instance.location%
  %echo% ~%self% slithers up out of the pit.
  %mod% %self% longdesc A tremendous ribbon serragon glimmers as it coils through the trees!
else
  %echo% ~%self% slithers down into the pit.
  mgoto %instance.start%
  %echo% ~%self% slithers down into the pit.
  %mod% %self% longdesc A tremendous ribbon serragon glimmers all around you in the dim light!
end
~
#12206
Ribbon Serragon: Difficulty selector~
0 c 0 1
L w 12200
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
elseif hard /= %arg%
  set difficulty 2
elseif group /= %arg%
  set difficulty 3
elseif boss /= %arg%
  set difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure. (Normal, Hard, Group, or Boss)
  return 1
  halt
end
* messaging
%send% %actor% You set the difficulty...
%echoaround% %actor% ~%actor% sets the difficulty...
* Clear existing difficulty flags and set new ones.
nop %self.remove_mob_flag(HARD)%
nop %self.remove_mob_flag(GROUP)%
if %difficulty% == 1
  * Then we don't need to do anything
  %echo% ~%self% has been set to Normal.
elseif %difficulty% == 2
  %echo% ~%self% has been set to Hard.
  nop %self.add_mob_flag(HARD)%
elseif %difficulty% == 3
  %echo% ~%self% has been set to Group.
  nop %self.add_mob_flag(GROUP)%
elseif %difficulty% == 4
  %echo% ~%self% has been set to Boss.
  nop %self.add_mob_flag(HARD)%
  nop %self.add_mob_flag(GROUP)%
end
remote difficulty %self.id%
%restore% %self%
nop %self.unscale_and_reset%
wait 1
* remove no-attack
if %self.affect(12200)%
  dg_affect #12200 %self% off
end
* mark me as scaled
set scaled 1
remote scaled %self.id%
~
#12207
Ribbon Serragon: Instruction to diff-sel~
0 B 0 1
L w 12200
~
if %self.affect(12200)%
  %send% %actor% You need to choose a difficulty before you can attack ~%self%.
  %send% %actor% Usage: difficulty <normal \| hard \| group \| boss>
  %echoaround% %actor% ~%actor% considers attacking ~%self%.
  return 0
else
  detach 12207 %self.id%
  return 1
end
~
#12208
Ribbon Serragon: Phase change~
0 l 50 2
L b 12201
L b 12202
~
* loads and sets up mobs 12201, 12202
%echo% &&lThe ribbon serragon rises up and splits -- it's not one serragon, it's two!&&0
set vnum 12201
while %vnum% <= 12202
  %load% mob %vnum%
  set mob %self.room.people%
  if %mob.vnum% == %vnum%
    set difficulty %self.var(difficulty,2)%
    remote difficulty %mob.id%
    * step down 1 difficulty
    if %difficulty% == 2
      nop %mob.add_mob_flag(TANK)%
    elseif %difficulty% == 3
      nop %mob.add_mob_flag(HARD)%
    elseif %difficulty% == 4
      nop %mob.add_mob_flag(GROUP)%
    end
    %restore% %mob%
    nop %mob.unscale_and_reset%
    %scale% %mob% %self.level%
    if %self.fighting%
      %force% %mob% maggro %self.fighting%
    end
    if !%mob.fighting%
      %force% %mob% maggro
    end
  end
  eval vnum %vnum% + 1
done
%purge% %self%
~
#12209
Ribbon Serragon: Simple fight script for pair~
0 k 10 0
~
* Paired serragon has simpler combat scripts as its difficulty is higher
if !%self.aff_flagged(HASTE)% || !%self.aff_flagged(SLOW)%
  dg_affect %self% HASTE on 30
  %echo% &&l~%self% whirls faster and faster!&&0
elseif %self.aff_flagged(BLIND)%
  dg_affect %self% BLIND off silent
  %echo% &&l~%self% shakes off the blindness!&&0
end
~
#12210
Vermilion Serragon combat: Flaming Maw, Crushing Coils, Tail Pin~
0 k 100 6
L w 9602
L w 12202
L w 12203
L w 12205
L w 12206
L w 12208
~
if %self.cooldown(12202)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.var(difficulty,1)%
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
scfight lockout 12202 25 32
if %move% == 1
  * Flaming Maw
  scfight clear dodge
  %echo% &&l~%self% inhales deeply, jaws glowing faintly with inner fire...&&0
  wait 3 s
  if %self.disabled% || %self.aff_flagged(BLIND)%
    halt
  end
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  if %diff% == 1
    dg_affect #12203 %self% HARD-STUNNED on 20
  end
  %send% %targ% &&l**** Heat shimmers in the air around |%self% mouth as the glow brightens... ****&&0 (dodge)
  %echoaround% %targ% &&lHeat shimmers in the air around |%self% mouth as the glow brightens...&&0
  scfight setup dodge %targ%
  if %diff% > 1
    set ouch 100
  else
    set ouch 50
  end
  set cycle 0
  eval times %diff% * 2
  eval when 9 - %diff%
  set done 0
  while %cycle% < %times% && !%done%
    wait %when% s
    if %targ.id% != %targ_id%
      set done 1
    elseif %targ.var(did_scfdodge)%
      %send% %targ% &&lYou hurl yourself aside just as a torrent of fire erupts where you stood!&&0
      %echoaround% %targ% &&l~%targ% hurls *%targ%self aside just as a torrent of fire erupts where &%targ% stood!&&0
    else
      %echo% &&lFire engulfs ~%targ% in an instant -- the searing heat scorches flesh and armor alike!&&0
      %send% %targ% That really hurts!
      %damage% %targ% %ouch% fire
    end
    eval cycle %cycle% + 1
    if %cycle% < %times% && !%done%
      wait 1
      if %targ.id% == %targ_id%
        %send% %targ% &&l**** &&Z|%self% gaping maw blazes with fire -- it's about to strike again! ****&&0 (dodge)
        %echoaround% %targ% &&l|%self% gaping maw blazes with fire -- it's about to strike!&&0
        scfight clear dodge
        scfight setup dodge %targ%
      end
    elseif %done% && %targ.id% == %targ_id% && %diff% == 1
      dg_affect #12208 %targ% TO-HIT 25 20
    end
  done
  scfight clear dodge
  dg_affect #12203 %self% off
elseif %move% == 2
  * Crushing Coils
  scfight clear struggle
  if %room.players_present% > 1
    %echo% &&l**** &&Z~%self% whips around you, its coils locking tight and pinning everyone's arms to their sides! ****&&0 (struggle)
  else
    %echo% &&l**** &&Z~%self% whips around you, its coils locking tight and pinning your arms to your sides! ****&&0 (struggle)
  end
  if %diff% == 1
    dg_affect #12203 %self% HARD-STUNNED on 20
  end
  scfight setup struggle all 20
  set person %room.people%
  while %person%
    if %person.affect(9602)%
      set scf_strug_char You thrash and strain, but the coils only squeeze tighter...
      set scf_strug_room ~%%actor%% thrashes and strains in the coils, but the serragon only squeezes tighter.
      set scf_free_char You wrench an arm free and twist violently, slipping loose from the crushing coils!
      set scf_free_room ~%%actor%% bursts free of the serragon's coils, staggering from the crushing grip!
      remote scf_strug_char %person.id%
      remote scf_strug_room %person.id%
      remote scf_free_char %person.id%
      remote scf_free_room %person.id%
    end
    set person %person.next_in_room%
  done
  * messages
  set cycle 0
  set ongoing 1
  while %cycle% < 5 && %ongoing%
    wait 4 s
    set ongoing 0
    set person %room.people%
    while %person%
      if %person.affect(9602)%
        set ongoing 1
        if %diff% > 1
          %send% %person% &&l**** The coils tighten mercilessly, bones creaking as the air is driven from your lungs! ****&&0 (struggle)
          %dot% #12205 %person% 100 30 physical 5
        else
          %send% %person% &&l**** The coils tighten mercilessly, bones creaking as the air is driven from your lungs! ****&&0 (struggle)
        end
      end
      set person %person.next_in_room%
    done
    eval cycle %cycle% + 1
  done
  scfight clear struggle
  dg_affect #12203 %self% off
elseif %move% == 3
  * Tail Pin
  scfight clear struggle
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  %send% %targ% &&l**** &&Z~%self% slams its tail down across your legs, pinning you hard against the ground! ****&&0 (struggle)
  %echoaround% %targ% &&l~%self% slams its tail down across |%targ% legs, pinning *%targ% against the ground!&&0
  if %diff% == 1
    dg_affect #12203 %self% HARD-STUNNED on 20
  end
  scfight setup struggle %targ% 20
  if %targ.affect(9602)%
    set scf_strug_char You strain to push the tail away, but it presses down even harder!
    set scf_strug_room ~%%actor%% struggles under the serragon's tail, but it presses down even harder, crushing *%%actor%% into the ground!
    set scf_free_char You twist violently, slipping free just as the tail crashes down again!
    set scf_free_room ~%%actor%% twists violently, slipping free just as the serragon's tail crashes down again!
    remote scf_strug_char %targ.id%
    remote scf_strug_room %targ.id%
    remote scf_free_char %targ.id%
    remote scf_free_room %targ.id%
  end
  * messages
  eval punish -1 * %diff%
  set cycle 0
  set ongoing 1
  while %cycle% < 5 && %ongoing%
    wait 4 s
    set ongoing 0
    if %targ.id% != %targ_id%
      set done 1
    else
      if %targ.affect(9602)%
        set ongoing 1
        if %diff% > 1
          %send% %targ% &&l**** The heavy tail grinds into your legs, crushing muscle and bone beneath its weight! ****&&0 (struggle)
          %echoaround% %targ% &&lThe serragon's massive tail grinds into |%targ% legs, holding *%targ% helpless!&&0
          dg_affect #12206 %targ% BONUS-PHYSICAL %punish% 20
          dg_affect #12206 %targ% BONUS-MAGICAL %punish% 20
        else
          %send% %targ% &&l**** You're still trapped under the tail! ****&&0 (struggle)
        end
      end
    end
    eval cycle %cycle% + 1
  done
  scfight clear struggle
  dg_affect #12203 %self% off
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#12211
Veridian Serragon combat: Needle Lunge, Sawtooth Constriction, Leafblade Sweep~
0 k 100 8
L w 9602
L w 12202
L w 12203
L w 12207
L w 12208
L w 12209
L w 12211
L w 12212
~
if %self.cooldown(12202)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.var(difficulty,1)%
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
scfight lockout 12202 25 32
if %move% == 1
  * Needle Lunge
  scfight clear dodge
  %echo% &&l~%self% lowers its head. Its needle-like teeth glint as it coils to strike...&&0
  wait 3 s
  if %self.disabled% || %self.aff_flagged(BLIND)%
    halt
  end
  if %diff% == 1
    dg_affect #12203 %self% HARD-STUNNED on 20
    set ouch 50
  else
    set ouch 100
  end
  set cycle 0
  eval times %diff% * 2
  eval when 9 - %diff%
  while %cycle% < %times%
    * new enemy each cycle
    set targ %random.enemy%
    if !%targ%
      halt
    end
    set targ_id %targ.id%
    %send% %targ% &&l**** &&Z~%self% fixes its silver eyes on you -- a forest of needle teeth are aimed straight at you... ****&&0 (dodge)
    %echoaround% %targ% &&l~%self% fixes its silver eyes on ~%targ% and opens its jaws... &&0
    scfight setup dodge %targ%
    wait %when% s
    if %targ.id% != %targ_id%
      * no action this time
    elseif %targ.var(did_scfdodge)%
      %send% %targ% &&lYou throw yourself aside at the last instant and its needle teeth snap shut with a terrible crack inches from your skin!&&0
      %echoaround% %targ% &&l~%targ% throws *%targ%self aside at the last instant as the serragon's teeth snap shut just short!&&0
      if %diff% == 1 && !%targ.affect(12208)%
        dg_affect #12208 %targ% TO-HIT 25 20
      end
    else
      %echo% &&lThe serragon lunges with lightning speed -- its needle teeth punch through your body a thousand times!&&0
      %send% %targ% That really hurts!
      %damage% %targ% %ouch% physical
      %dot% #12212 %targ% %ouch% 30 physical 5
    end
    eval cycle %cycle% + 1
    wait 1
  done
  scfight clear dodge
  dg_affect #12203 %self% off
elseif %move% == 2
  * Sawtooth Constriction
  scfight clear struggle
  if %room.players_present% > 1
    %echo% &&l**** &&Z~%self% loops its jagged coils around the party -- serrated scales press against you! ****&&0 (struggle)
  else
    %echo% &&l**** &&Z~%self% loops its jagged coils around you -- serrated scales press against you! ****&&0 (struggle)
  end
  if %diff% == 1
    dg_affect #12203 %self% HARD-STUNNED on 20
  end
  scfight setup struggle all 20
  set person %room.people%
  while %person%
    if %person.affect(9602)%
      set scf_strug_char You fight to tear yourself loose, but the scales only cut deeper.
      set scf_strug_room ~%%actor%% thrashes against the coils, but only drives the serrated scales deeper.
      set scf_free_char With a desperate heave, you wrench free of the jagged scales!
      set scf_free_room ~%%actor%% wrenches free of the serragon's jagged coils!
      remote scf_strug_char %person.id%
      remote scf_strug_room %person.id%
      remote scf_free_char %person.id%
      remote scf_free_room %person.id%
    end
    set person %person.next_in_room%
  done
  * messages
  set cycle 0
  set ongoing 1
  while %cycle% < 5 && %ongoing%
    wait 4 s
    set ongoing 0
    set person %room.people%
    while %person%
      if %person.affect(9602)%
        set ongoing 1
        %send% %person% &&l**** The coils grind tighter, scales biting deeper into your skin! ****&&0 (struggle)
        if %diff% > 1
          %dot% #12211 %person% 150 30 physical 15
        end
      end
      set person %person.next_in_room%
    done
    eval cycle %cycle% + 1
  done
  scfight clear struggle
  dg_affect #12203 %self% off
elseif %move% == 3
  * Leafblade Sweep
  scfight clear dodge
  %echo% &&l~%self% lowers its frilled crest, which gleams like jagged leaves in the dim light...&&0
  eval dodge %diff% * 40
  dg_affect #12207 %self% DODGE %dodge% 20
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  scfight setup dodge all
  wait 5 s
  if %self.disabled%
    dg_affect #12207 %self% off
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %echo% &&l**** &&Z~%self% sweeps its crest toward you in a wide, scything arc! ****&&0 (dodge)
  if %diff% > 1
    set ouch 125
  else
    set ouch 75
  end
  set cycle 1
  set hit 0
  eval wait 9 - %diff%
  while %cycle% <= %diff%
    scfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfdodge)%
          set hit 1
          %echo% &&lThe serragon's leaflike crest slams into ~%ch%, its jagged spines lodging deep!&&0
          %send% %ch% That really hurt!
          dg_affect #12209 %ch% SLOW on 10
          dg_affect #12209 %ch% MOVE-REGEN -3 10
          dg_affect #12209 %ch% MANA-REGEN -3 10
          %damage% %ch% %ouch% physical
        elseif %ch.is_pc%
          %send% %ch% &&lYou duck low and roll as the crest whistles over your head!&&0
          if %diff% == 1
            dg_affect #12208 %ch% TO-HIT 25 10
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&l**** The serragon arcs around and is coming back at you again! ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
    eval cycle %cycle% + 1
  done
  dg_affect #12207 %self% off
  if !%hit% && %diff% == 1
    %echo% &&l~%self% wide crest hisses through the air, striking nothing but empty air.&&0
    dg_affect #12203 %self% HARD-STUNNED on 10
  end
  wait 8 s
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#12212
Ribbon Serragon: Pickpocket rejection strings~
0 p 100 1
L o 142
~
if %ability% != 142
  * not pickpocket
  return 1
  halt
else
  return 0
  * after messaging...
end
if !%self.aff_flagged(!ATTACK)%
  %send% %actor% You can't imagine which part of it might be the "pocket" but it doesn't matter... you've attracted too much attention!
  %aggro% %actor%
else
  %send% %actor% You can't imagine which part of it might be the "pocket".
end
~
#12213
Ribbon Serragon: Environmental echoes~
0 b 8 1
L j 12200
~
if %self.fighting%
  halt
end
if %self.room.template% == 12200
  * in the pit
  set above %instance.location%
  switch %random.7%
    case 1
      %echo% The pit quakes with an excruciating grating noise as the serragon grinds its twin ribbons together.
      if %above%
        %at% %above% %echo% An excruciating grating noise echoes up from the pit.
      end
    break
    case 2
      %echo% The ground around you quivers as the serragon coils around the edges and scrapes at the dirt and rock.
      if %above%
        %at% %above% echo The ground quivers as the serragon briefly emerges from the pit and showers you with dirt and rocks.
      end
    break
    case 3
      %echo% The serragon's jagged scapes rasp against the stone walls, sending a shiver through the pit.
      if %above%
        %at% %above% %echo% Jagged scales rasp against stone, sending a shiver through the forest floor.
      end
    break
    case 4
      if %above%
        %at% %above% %echo% A frilled head rises suddenly from the pit, jaws opening wide before sinking back down.
      end
    break
    case 5
      %echo% The ribbons of the serragon spark faintly as they scrape together.
    break
    case 6
      %echo% The serragon's hiss echoes like cloth tearing, sharp and sudden in the forest silence.
    break
    case 7
      %echo% A scale fragment crunches underfoot, sharp as broken glass.
    break
  done
else
  * on the surface
  set below %instance.start%
  switch %random.5%
    case 1
      %echo% Strips of bark and leaves rain down into the pit as the serragon twists through the trees.
      if %below%
        %at% %below% %echo% Strips of bark and leaves rain down into the pit as the creature twists through the trees above.
      end
    break
    case 2
      %echo% A flash of vermillion and veridian whips past, too fast to follow with your eyes.
      if %below%
        %at% %below% %echo% Dirt trickles down the gouged walls, as if something is stirring above.
      end
    break
    case 3
      %echo% The ground quivers as the the serragon slides into the pit and reemerges with a shower of dirt and rocks.
      if %below%
        %at% %below% %echo% The ground around you quivers as the serragon descends into the pit, scraping at the sides as it coils, then writhes back out.
      end
    break
    case 4
      %echo% The ribbons of the serragon spark faintly as they scrape together.
    break
    case 5
      %echo% The serragon's hiss echoes like cloth tearing, sharp and sudden in the forest silence.
    break
  done
end
~
#12214
Ribbon Serragons: Recombine if out of combat~
0 ab 33 3
L b 12200
L b 12201
L b 12202
~
wait 30 s
if %self.fighting% || %self.disabled%
  halt
end
if %self.vnum% == 12201
  set other %instance.mob(12202)%
else
  set other %instance.mob(12201)%
end
if !%other%
  detach 12214 %self.id%
  halt
end
if %other.fighting% || %other.disabled%
  halt
end
if %other.room% != %self.room%
  %at% %other.room% %echo% ~%other% slithers away.
  %teleport% %other% %self.room%
  %echo% The second serragon slithers in.
end
%echo% &&lThe two serragons nuzzle up against each other and begin to coil together again. You cover your ears as the grinding noise shreds through the air!&&0
%load% mob 12200 %self.level%
%purge% %other%
%purge% %self%
~
#12250
Stomping Ground: Shared mob load trig~
0 n 100 2
L b 12250
L b 12251
~
set important_vnums 12250 12251
wait 0
set loc %instance.location%
if %loc%
  mgoto %loc%
  mmove
  mmove
  mmove
  set start %instance.start%
  if %start% && %important_vnums% ~= %self.vnum%
    eval elephants %start.var(elephants,0)% + 1
    remote elephants %start.id%
  end
else
  * not instanced?
  nop %self.add_mob_flag(SPAWNED)%
  nop %self.remove_mob_flag(IMPORTANT)%
end
~
#12251
Stomping Ground: Terraform tile on cleanup~
2 e 100 18
L h 200
L h 203
L h 210
L h 211
L h 212
L h 220
L h 221
L h 222
L h 223
L h 224
L h 232
L h 233
L h 237
L h 239
L h 247
L h 249
L h 250
L h 252
~
* converts to grassland when despawning adventure
set terra_sects 203 210 211 212 220 221 222 223 224 232 233 237 239 247 249 250 252
if %terra_sects% ~= %room.base_sector_vnum%
  %terraform% %room% 200
end
~
#12252
Stomping Ground: Elephant death~
0 f 100 2
L c 12251
L c 12255
~
* This only belongs on elephants listed as %important_vnums% in trig 12250
set start %instance.start%
if %start%
  eval elephants %start.var(elephants,1)% - 1
  remote elephants %start.id%
  if %elephants% <= 0
    %load% obj 12251 %instance.location%
    %load% obj 12255 %instance.location%
  end
end
~
#12253
Stomping Ground: Leash~
0 i 100 10
L e 12250
L h 5
L h 9
L h 32
L h 33
L h 57
L h 58
L h 98
L h 99
L h 200
~
set allow_outside_sects 5 9 32 33 57 58 98 99
set room %self.room%
* safety
if %method% != move
  return 1
  halt
end
* terrain-based leash
if (%room.sector_vnum% < 200 || %room.sector_vnum% > 299) && !(%allow_outside_sects% ~= %room.sector_vnum%) && %room.building_vnum% != 12250
  * sector I don't like
  return 0
  halt
end
* no distance leash if no instance
set loc %instance.location%
if !%loc%
  return 1
  halt
end
* compute range
eval max_x %world.width% / 220
eval max_y %world.height% / 120
if %max_x% < %max_y%
  set range %max_x%
else
  set range %max_y%
end
if %range% < 5
  set range 5
end
* odd directions
set dir %loc.direction(%room%)%
if %dir% == northwest
  eval range %range% + 1
elseif %dir% == southeast
  eval range %range% - 1
end
* leash distance
set dist %loc.distance(%room%)%
if (%dist% >= %range% && %random.3% != 3) || %dist% >= (%range% + 2)
  return 0
  halt
end
~
#12254
Stomping Ground: Terraform jungle to grassland~
0 ab 7 22
L c 128
L c 134
L c 135
L c 147
L c 150
L h 200
L h 203
L h 210
L h 212
L h 220
L h 221
L h 223
L h 224
L h 232
L h 234
L h 237
L h 239
L h 247
L h 249
L h 250
L h 252
L h 12250
~
* Terraforms ONLY the listed vnums, and only when attached to an instance
set room %self.room%
if !%instance.location%
  * no instance / no terraform
  halt
elseif %room.empire%
  * claimed
  halt
end
set sect %room.sector_vnum%
* Convert territory
if %sect% == 203
  * crop -> soil
  %echo% ~%self% rips up the crop and devours it!
  %terraform% %room% 12250
elseif %sect% == 210 && %random.3% == 3
  * savanna -> grassland
  %echo% ~%self% rips up a tree!
  %load% obj 147 %room%
  %terraform% %room% 200
elseif %sect% == 212
  * savanna copse -> grassland
  %echo% ~%self% rips up some saplings!
  %load% obj 134 %room%
  %load% obj 134 %room%
  %terraform% %room% 200
elseif %sect% == 220
  * jungle -> partial
  %echo% ~%self% rips up a tree!
  %load% obj 128 %room%
  %terraform% %room% 221
elseif %sect% == 221
  * partial jungle -> soil
  %echo% ~%self% rips up a tree!
  %load% obj 128 %room%
  %terraform% %room% 12250
elseif %sect% == 223
  * jungle copse -> soil
  %echo% ~%self% rips up some saplings!
  %load% obj 135 %room%
  %load% obj 135 %room%
  %terraform% %room% 12250
elseif %sect% == 224
  * jungle edge -> grassland
  %echo% ~%self% rips up some saplings!
  %load% obj 135 %room%
  %load% obj 135 %room%
  %terraform% %room% 200
elseif %sect% == 232
  * mangrove forest -> soil
  %echo% ~%self% rips up some trees!
  %load% obj 150 %room%
  %load% obj 150 %room%
  %terraform% %room% 12250
elseif %sect% == 237
  * seaside crop -> soil
  %echo% ~%self% rips up the crop and devours it!
  %terraform% %room% 12250
elseif %sect% == 239
  * seaside crop -> estuary shore
  %echo% ~%self% rips up the crop and devours it!
  %terraform% %room% 234
elseif %sect% == 247
  * riverbank crop -> soil
  %echo% ~%self% rips up the crop and devours it!
  %terraform% %room% 12250
elseif %sect% == 249
  * lakeshore crop -> soil
  %echo% ~%self% rips up the crop and devours it!
  %terraform% %room% 12250
elseif %sect% == 250 && %random.3% == 3
  * swamp -> soil
  %echo% ~%self% rips all the swamp plants!
  %terraform% %room% 12250
elseif %sect% == 252 && %random.3% == 3
  * swamp -> soil
  %echo% ~%self% rips all the marsh plants!
  %terraform% %room% 12250
end
* ensure not more than once per minute
wait 60 s
~
#12255
Stomping Ground: Tame small elephant to gain minipet~
0 c 0 2
L b 12255
L o 34
tame feed~
* mostly a copy of 9028 with updates for the minipet here
* Amount of tameness required
set target 5
* This script also overrides 'feed'
if %cmd% == feed
  if %actor.char_target(%arg.argument2%) == %self%
    %send% %actor% Just 'give' the food to *%self%.
    return 1
  else
    * ignore 'feed'
    return 0
  end
  halt
end
* Check target and tech
if (!%actor.has_tech(Tame-Command)% || %actor.char_target(%arg%)% != %self%)
  return 0
  halt
end
* Skill checks / load tameness
if %actor.ability(Summon Animals)%
  set tameness %target%
  %send% %actor% You whistle at ~%self%...
  %echoaround% %actor% ~%actor% whistles at ~%self%...
elseif %self.varexists(tameness)%
  set tameness %self.tameness%
else
  set tameness 0
end
if %tameness% < %target%
  %send% %actor% You can't seem to get close enough to ~%self% to tame *%self%. Try feeding *%self% some fruit or grain.
  return 1
elseif %actor.has_minipet(12255)%
  %send% %actor% ~%self% seems quite tame but you already have one as a minipet.
  return 1
else
  * Ok to tame (which is fake)
  %send% %actor% You try to tame ~%self%... and gain a playful little elephant as a minipet!
  %echoaround% %actor% ~%actor% tries to tame ~%self%... and gains a playful little elephant!
  nop %actor.add_minipet(12255)%
  return 1
  %purge% %self%
end
~
#12256
Stomping Ground: Trumpeting Call~
0 b 2 0
~
%echo% ~%self% raises ^%self% trunk to the sky and lets out a powerful call...
%regionecho% %self.room% 15 A trumpeting call echoes through the tropical air.
~
#12257
Stomping Ground: Spawn friends~
0 b 4 4
L b 12252
L b 12254
L e 12250
L h 200
~
* summons a friend at random over time
set room %self.room%
if (%room.sector_vnum% < 200 || %room.sector_vnum% > 299) && %room.building_vnum% != 12250
  * don't summon here
  halt
end
if %random.3% == 1
  set vnum 12254
else
  set vnum 12252
end
%load% mob %vnum%
set mob %room.people%
if %mob.vnum% == %vnum%
  %echo% Another elephant arrives and greets the matriarch with a polite trumpet.
end
~
$
