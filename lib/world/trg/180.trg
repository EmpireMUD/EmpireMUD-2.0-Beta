#18000
Molten Fiend: Difficulty selector~
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
* Clear existing difficulty flags and set new ones.
remote diff %self.id%
set mob %self%
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
* messaging
if %diff% == 1
  * Then we don't need to do anything
  if %self.longdesc% ~= (difficulty)
    %send% %actor% You ignore the words on the infernal slab...
    %echoaround% %actor% ~%actor% ignores the words on the infernal slab...
  else
    %send% %actor% You frantically read the words on the infernal slab in reverse...
    %echoaround% %actor% ~%actor% frantically reads the words on the infernal slab in reverse...
  end
  %echo% ~%self% has been set to Normal.
elseif %diff% == 2
  %send% %actor% You begin to read the words on the infernal slab, but stop short as you realize what they say...
  %echoaround% %actor% ~%actor% begins to read the words on the infernal slab, but stops short...
  %echo% ~%self% has been set to Hard.
  nop %mob.add_mob_flag(HARD)%
elseif %diff% == 3
  %send% %actor% You chant the words on the infernal slab...
  %echoaround% %actor% ~%actor% chants the words on the infernal slab...
  %echo% ~%self% has been set to Group.
  nop %mob.add_mob_flag(GROUP)%
elseif %diff% == 4
  %send% %actor% You bellow out the words on the infernal slab with all your might...
  %echoaround% %actor% ~%actor% bellows out the words on the infernal slab with all ^%actor% might...
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
* menacing echo
wait 1
switch %diff%
  case 1
    %echo% &&O&&Z~%self% stirs, its fiery eyes flickering with anger as you approach.&&0
  break
  case 2
    %echo% &&O&&Z~%self% shifts, chains straining, fiery eyes burning with fury, the flames around its horns flaring brightly.&&0
  break
  case 3
    %echo% &&O&&Z~%self% roars thunderously, shaking the fissure as magma ripples around its chains and searing heat blisters your skin!&&0
  break
  case 4
    %echo% &&O&&Z|%self% overwhelming presence snarls with pure malevolence, its blazing eyes locked on you, chains groaning in protest against its undeniable power!&&0
  break
done
~
#18001
Molten Fiend: No-attack until diff-sel~
0 n 100
~
* turn on no-attack (until diff-sel)
dg_affect %self% !ATTACK on -1
~
#18002
Molten Fiend: Message when attacked before diff-sel~
0 B 0
~
if %self.aff_flagged(!ATTACK)%
  %echoaround% %actor% ~%actor% considers attacking ~%self%...
  %echo% ~%self% roars as &%self% flares up and rages toward you, restrained only by the power of the infernal slab!
  %send% %actor% You need to choose a difficulty before you can fight ~%self%.
  %send% %actor% Usage: difficulty <normal \| hard \| group \| boss>
  return 0
else
  * no need for this script anymore
  detach 18002 %self.id%
  return 1
end
~
#18003
Molten Fiend: change phases (hit percent)~
0 l 100
~
* Attach the heal-and-reset trigger
if !%self.has_trigger(18007)%
  attach 18007 %self.id%
end
eval health_perc 100 * %self.health% / %self.maxhealth%
set phase %self.var(phase, 1)%
* probably add an 'or' condition here for the power charge affect
if %phase% < 2 && %health_perc% < 70
  set phase 2
end
if %phase% < 3 && %health_perc% < 35
  set phase 3
end
remote phase %self.id%
if !%lastphase%
  set lastphase 1
  remote lastphase %self.id%
end
if %phase% > 1
  * Messaging and morphs
  if %lastphase% != %phase%
    switch %phase%
      case 2
        %echo% &&OOne of |%self% chains shatters!&&0
        %morph% %self% 18005
      break
      case 3
        %echo% &&O|%self% second chain shatters, freeing *%self%!&&0
        %morph% %self% 18006
      break
    done
    set lastphase %phase%
    remote lastphase %self.id%
    * Fix issues with phases having different numbers of moves in the shuffle
    rdelete moves_left %self.id%
    rdelete num_left %self.id%
  end
end
~
#18004
Molten Fiend phase 1 attacks~
0 c 0
fiendfight1~
if %actor% != %self%
  return 0
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
eval delay 9 - %diff%
switch %move%
  case 1
    * Channel Ward
    * Gain immunity to [physical/magical], then increased resistances.
    * Wards stack, last the rest of the fight. 1/1/2/2 interrupts needed.
    if %self.affect(18006)% && %self.affect(18007)%
      * has both
      set ability resistance
    elseif !%self.affect(18006)% && %self.affect(18007)%
      * has magic only
      set ability physical
    elseif %self.affect(18006)% && !%self.affect(18007)%
      * has physical only
      set ability magical
    else
      * has neither
      if %random.2%==1
        set ability physical
      else
        set ability magical
      end
    end
    %echo% &&OA fiery sigil appears above |%self% forehead...&&0
    %echo% &&O**** &&Z~%self% starts channeling a %ability% ward! ****&&0 (interrupt)
    set needed 1
    if %diff%>2 && %room.players_present%>1
      set needed 2
    end
    scfight clear interrupt
    scfight setup interrupt all
    wait %delay% s
    if %self.count_scfinterrupt% >= %needed%
      set ch %room.people%
      while %ch%
        if %self.is_enemy(%ch%)%
          if %ch.var(did_scfinterrupt,0)%
            %send% %ch% &&OYou manage to disrupt |%self% %ability% ward!&&0
          else
            %send% %ch% &&O&&Z~%self% %ability% ward is disrupted!&&0
          end
        end
        set ch %ch.next_in_room%
      done
    else
      if %ability% == physical
        %echo% &&OA mist of fiery energy surrounds |%self% body!&&0
        dg_affect #18006 %self% IMMUNE-PHYSICAL-DEBUFFS on -1
        dg_affect #18006 %self% IMMUNE-POISON-DEBUFFS on -1
      elseif %ability% == magical
        %echo% &&OA mist of fiery energy surrounds |%self% head!&&0
        dg_affect #18007 %self% IMMUNE-MAGICAL-DEBUFFS on -1
        dg_affect #18007 %self% IMMUNE-MENTAL-DEBUFFS on -1
      elseif %ability% == resistance
        eval magnitude %self.level%/10
        %echo% &&OThe mist of fiery energy surrounding ~%self% grows more dense!&&0
        dg_affect #18008 %self% RESIST-PHYSICAL %magnitude% -1
        dg_affect #18008 %self% RESIST-MAGICAL %magnitude% -1
      end
    end
    scfight clear interrupt
  break
  case 2
    * Thrash
    * Bring down rocks from the ceiling
    * Each player: Dodge or damage + stun (just damage on normal)
    %echo% &&O&&Z~%self% thrashes and roars, straining against its chains!&&0
    wait 3 s
    %echo% &&O**** The ground shakes and rocks start to fall from the ceiling! ****&&0 (dodge)
    scfight clear dodge
    scfight setup dodge all
    wait %delay% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if %ch.var(did_scfdodge,0)%
          %echo% &&ORocks crash down all around ~%ch%, barely missing *%ch%!&&0
        else
          if %diff% == 1
            %echo% &&OA falling rock bonks ~%ch% on the head!&&0
            %damage% %ch% 150
          else
            %echo% &&OA large falling rock crashes into ~%ch%, knocking *%ch% flat!&&0
            if (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
              dg_affect #18009 %ch% STUNNED on 10
            end
            %damage% %ch% 300
          end
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
  break
  case 3
    * Death Glare
    * Glare at the tank until they burst into flames.
    * 1/1/2/2 successful interrupts needed.
    set tank %self.fighting%
    set verify %tank.id%
    %echo% &&O&&Z~%self% glares intensely at ~%tank%, eyes glowing with hatred!&&0
    wait 3 s
    if %verify% != %tank.id%
      %echo% &&O&&Z~%self% blinks, losing the target of its glare.&&0
      halt
    end
    %send% %tank% &&O**** You feel an intense pressure from |%self% glare! ****&&0 (interrupt)
    %echoaround% %tank% &&O**** ~%tank% starts to smoulder slightly! ****&&0 (interrupt)
    set needed 1
    if %diff%>2 && %room.players_present%>1
      set needed 2
    end
    scfight clear interrupt
    scfight setup interrupt all
    wait %delay% s
    if %verify% != %tank.id% || %self.room% != %tank.room%
      %echo% &&O&&Z~%self% blinks, losing the target of its glare.&&0
    elseif %self.count_scfinterrupt% >= %needed%
      %echo% &&O&&Z~%self% blinks and looks away, its curse dissipating!&&0
    else
      %send% %tank% &&OYou suddenly burst into flames!&&0
      %echoaround% %tank% &&O~%tank% suddenly bursts into flames!&&0
      %damage% %tank% 100 fire
      %dot% #18010 %tank% 300 30 fire
    end
    scfight clear interrupt
  break
done
~
#18005
Molten Fiend phase 2 attacks~
0 c 0
fiendfight2~
if %actor% != %self%
  return 0
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
eval delay 9 - %diff%
switch %move%
  case 1
    * Summon Fire Elemental
    * Summons a minion to attack the tank.
    * Minion gains stats over time (except on normal). Summons 2 at once on boss.
    %echo% &&O&&Z~%self% whistles, making a sound like escaping steam...&&0
    if %diff% == 4
      %echo% &&OA pair of fire elementals burst from the lava, leaping onto the platform!&&0
      set rep 2
    else
      %echo% &&OA fire elemental bursts from the lava, leaping onto the platform!&&0
      set rep 1
    end
    while %rep%>0
      %load% m 18077 ally %self.level%
      set ele %room.people%
      if %ele.vnum% == 18077
        remote diff %ele.id%
      end
      eval rep %rep%-1
    done
  break
  case 2
    * Wave of Fire
    * Launch multiple waves of fire at the group
    * Each player: Dodge or damage + dot
    %echo% &&O&&Z~%self% lifts up a hand, curling it into a fist...&&0
    wait 3 s
    %echo% &&OThe lava below the platform starts to churn and roil!&&0
    set i 1
    eval loops %diff% + 1
    while %i%<=%loops%
      if %i%==1
        %echo% &&O**** A wave of molten rock and fire sweeps toward you! ****&&0 (dodge)
      else
        %echo% &&O**** Another wave of fire is coming in! ****&&0 (dodge)
      end
      scfight clear dodge
      scfight setup dodge all
      wait %delay% s
      %echo% &&OA wave of fire rolls over the platform!&&0
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if %ch.var(did_scfdodge,0)%
            %send% %ch% &&OYou dive behind a rock just in time!&&0
            %echoaround% %ch% &&O&&Z~%ch% dives behind a rock just in time!&&0
          else
            if %diff% <= 2
              %send% %ch% &&OYou're burned by the wave of fire!&&0
              %echoaround% %ch% &&O&&Z~%ch% is burned by the wave of fire!&&0
              %damage% %ch% 100 fire
              %dot% #18013 %ch% 100 30 fire 3
            else
              %send% %ch% &&OYou're severely burned by the wave of fire!&&0
              %echoaround% %ch% &&O&&Z~%ch% is severely burned by the wave of fire!&&0
              %damage% %ch% 300 fire
              %dot% #18013 %ch% 200 30 fire 3
            end
          end
        end
        set ch %next_ch%
      done
      scfight clear dodge
      eval i %i%+1
      wait %delay% s
    done
  break
  case 3
    * Chain Strike
    * Tries to trip the group with the broken end of its chains (dodge or stun)
    * Almost no warning but relatively light punishment
    %echo% &&O**** &&Z~%self% suddenly whips the broken end of its chains at you! ****&&0 (dodge)
    scfight clear dodge
    scfight setup dodge all
    wait %delay% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if %ch.var(did_scfdodge,0)%
          if %diff% <= 2
            %send% %ch% &&OYou leap gracefully over the swinging chain!&&0
            %echoaround% %ch% &&O&&Z~%ch% leaps gracefully over the swinging chain!&&0
          else
            %send% %ch% &&OYou duck gracefully under the swinging chain!&&0
            %echoaround% %ch% &&O&&Z~%ch% ducks gracefully under the swinging chain!&&0
          end
        else
          if %diff% <= 2
            %send% %ch% &&OYou're tripped by the end of the swinging chain!&&0
            %echoaround% %ch% &&O&&Z~%ch% is tripped by the end of the swinging chain!&&0
            if (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
              dg_affect #18014 %ch% STUNNED on 5
            end
            %damage% %ch% 50 physical
          else
            %send% %ch% &&OYou are struck and sent flying by the end of the swinging chain!&&0
            %echoaround% %ch% &&O&&Z~%ch% is struck and sent flying by the end of the swinging chain!&&0
            if (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
              dg_affect #18014 %ch% STUNNED on 10
            end
            %damage% %ch% 100 physical
          end
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
  break
done
~
#18006
Molten Fiend phase 3 attacks~
0 c 0
fiendfight3~
if %actor% != %self%
  return 0
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
eval pain 33 * %diff%
set targ %self.fighting%
if !%targ%
  halt
end
eval delay 9 - %diff%
switch %move%
  case 1
    * Molten Punch
    set targ_id %targ.id%
    %send% %targ% &&O**** &&Z|%self% fist turns to molten lava as &%self% takes aim at you... ****&&0 (dodge)
    %echoaround% %targ% &&O&&Z|%self% fist turns to molten lava as &%self% takes aim at ~%targ%...&&0
    set cycle %diff% + 1
    while %cycle% > 0
      scfight clear dodge
      scfight setup dodge %targ%
      wait %delay% s
      if %targ.id% != %targ_id% || %targ.room% != %self.room%
        * gone
        halt
      end
      if !%targ.var(did_scfdodge,0)%
        %echo% &&O&&Z~%self% punches its molten fist into |%targ% chest!&&0
        %damage% %targ% %pain% fire
      else
        %send% %targ% &&OYou narrowly dodge |%self% fist!&&0
      end
      if %cycle% > 1
        %send% %targ% &&O**** &&Z~%self% pulls its fist back for another punch! ****&&0 (dodge)
      elseif !%targ.var(did_scfdodge,0)%
        * missed the last one
        %echo% &&OThat final punch sends a wave of molten lava across the lair!&&0
        %aoe% 100 fire
      end
      eval cycle %cycle% - 1
    done
    scfight clear dodge
  break
  case 2
    * Explosion Ritual
    * 1/1/2/2 successful interrupts needed on EACH cycle
    %echo% &&O**** &&Z~%self% creates a burning orb that simmers and pulses.... ****&&0 (interrupt)
    set needed 1
    if %diff% > 2 && %room.players_present% > 1
      set needed 2
    end
    set cycle %diff% + 1
    set fails 0
    while %cycle% > 0
      scfight clear interrupt
      scfight setup interrupt all
      wait %delay% s
      if %self.count_scfinterrupt% < %needed%
        %echo% &&OYou shout in agony as a fiery pulse from the burning orb sears through you!&&0
        %aoe% %pain% fire
        eval fails %fails% + 1
      end
      if %cycle% > 1
        %echo% &&O**** Fiery mana streams from ~%self% to the burning orb.... ****&&0 (interrupt)
      end
      eval cycle %cycle% - 1
    done
    if %fails% > 0
      eval pain %diff% * %fails% * 50
      %echo% &&OA fiery explosion engulfs the lair!&&0
      %aoe% %pain% fire
    else
      %echo% &&O&&Z~%self% is interrupted and the burning orb fizzles out!&&0
    end
    scfight clear interrupt
  break
  case 3
    * Claw Grab
    scfight clear struggle
    set targ_id %targ.id%
    %send% %targ% &&O**** &&Z~%self% grabs you with its enormous clawed hand! ****&&0 (struggle)
    %echoaround% %targ% &&O&&Z~%self% grabs ~%targ% with its clawed hand!&&0
    scfight setup struggle %targ% 20
    if %targ.affect(9602)%
      set scf_strug_char You struggle against the fiend's claws...
      set scf_strug_room ~%%targ%% struggles against the fiend's claws...
      set scf_free_char You slip out of the fiend's grip!
      set scf_free_room ~%%targ%% slips out of the fiend's grip!
      remote scf_strug_char %targ.id%
      remote scf_strug_room %targ.id%
      remote scf_free_char %targ.id%
      remote scf_free_room %targ.id%
    end
    if %diff% == 1
      dg_affect #18017 %self% HARD-STUNNED on 20
    end
    * ticks
    set cycle %diff% + 1
    while %cycle% > 0
      wait %delay% s
      if %targ.id% != %targ_id% || %targ.room% != %self.room%
        * gone
        dg_affect #18017 %self% off
        halt
      elseif !%targ.affect(9602)%
        * got free
        dg_affect #18017 %self% off
        halt
      elseif %cycle% > 1
        %send% %targ% &&O**** You shout in agony as ~%self% clenches its claws into you! ****&&0 (struggle)
        %echoaround% %targ% &&O&&Z~%self% clenches its claws into ~%targ%!&&0
        %damage% %targ% %pain% physical
      else
        * final cycle and they didn't get free
        if %diff% < 4
          %echo% &&O&&Z~%self% slams ~%targ% down hard into the rocks!&&0
          %damage% %targ% 200 physical
        else
          %echo% &&O&&Z~%self% throws ~%targ% into the lava below!&&0
          %slay% %targ% &&Z%targ.real_name% has been thrown into the lava at %self.room.coords%!
        end
      end
      eval cycle %cycle% - 1
    done
    scfight clear struggle
    dg_affect #18017 %self% off
  break
done
~
#18007
Molten Fiend out-of-combat reset~
0 ab 100
~
if %self.fighting%
  halt
end
%morph% %self% normal
if %active_phase% > 1
  %echo% &&OWith a blinding flash of light, the chains restraining ~%self% are restored!&&0
end
%echo% &&O&&Z|%self% wounds are healed!&&0
dg_affect #18006 %self% off
dg_affect #18007 %self% off
dg_affect #18008 %self% off
%restore% %self%
* Purge remaining fire elementals
%purge% instance mob 18077 $n vanishes in a flash of flames.
* Detach self (Will reattach from hitpercent trigger when fighting resumes)
detach 18007 %self.id%
~
#18008
Molten Fiend: new fight main controller~
0 k 100
~
if %self.cooldown(18004)%
  halt
end
nop %self.set_cooldown(18004, 30)%
set active_phase %self.var(phase, 1)%
* activate the command trigger for the appropriate combat phase
fiendfight%active_phase%
~
#18009
Molten Fiend: kill difficulty tracker (for infuse)~
0 f 100
~
set 18075_highest_kill_diff %self.var(diff, 1)%
set person %self.room.people%
while %person%
  * Only for PCs who tagged the fiend before it died
  if %person.is_pc% && %self.is_tagged_by(%person%)%
    * We don't want to ever reduce this value so only remote it if the current value is lower
    if %person.var(18075_highest_kill_diff, 0)% < %18075_highest_kill_diff%
      remote 18075_highest_kill_diff %person.id%
    end
  end
  set person %person.next_in_room%
done
%adventurecomplete%
~
#18010
Infuse fiend gear at fissure~
2 c 0
infuse~
set target %actor.obj_target(%arg.car%)%
set conf %arg.cdr%
* Validate target
if !%target%
  %send% %actor% What do you want to infuse and upgrade?
  halt
end
if %target.carried_by% != %actor%
  %send% %actor% You can only infuse an item in your inventory.
  halt
end
if %target.vnum% < 18012 || %target.vnum% > 18049 || !%target.wearable% || (%target.level% == 0)
  %send% %actor% You can't infuse @%target%!
  halt
end
set best_kill %actor.var(18075_highest_kill_diff, 0)%
set cost 5
if !%target.is_flagged(HARD-DROP) && !%target.is_flagged(GROUP-DROP)%
  if %best_kill%<2
    set best_kill_too_low 1
  end
  set new_tier hard
  set toggle_hard 1
  set cost 5
elseif %target.is_flagged(HARD-DROP) && !%target.is_flagged(GROUP-DROP)%
  if %best_kill%<3
    set best_kill_too_low 1
  end
  set new_tier group
  set toggle_hard 1
  set toggle_group 1
  set cost 5
elseif !%target.is_flagged(HARD-DROP) && %target.is_flagged(GROUP-DROP)%
  if %best_kill%<4
    set best_kill_too_low 1
  end
  set new_tier boss
  set toggle_hard 1
  set cost 10
elseif %target.is_flagged(HARD-DROP) && %target.is_flagged(GROUP-DROP)%
  set new_tier rescale
  set cost 2
end
if %best_kill_too_low%
  %send% %actor% You can't improve @%target% to %new_tier% quality yet; you need to kill the molten fiend at least once on %new_tier% difficulty first.
  halt
end
eval can_afford %actor.currency(18000)% >= %cost%
set max_level %actor.highest_level%
if %max_level% > 300
  set max_level 300
end
if %conf% != CONFIRM
  if %new_tier% == rescale
    %send% %actor% You can infuse @%target% with %cost% molten essence, rescaling it to level %max_level%. It will remain boss quality.
  else
    %send% %actor% You can infuse @%target% with %cost% molten essence, improving it to %new_tier% quality and rescaling it to level %max_level%.
  end
  if !%can_afford%
    %send% %actor% You don't have enough molten essence to do this.
  else
    %send% %actor% Use 'infuse <item> confirm' to confirm this action.
  end
else
  if !%can_afford%
    %send% %actor% You don't have enough molten essence to infuse @%target%! You need %cost%.
    halt
  end
  nop %actor.charge_currency(18000, %cost%)%
  %send% %actor% You hold @%target% aloft and infuse it with %cost% motes of molten essence...
  %echoaround% %actor% ~%actor% holds @%target% aloft and infuses it with %cost% motes of molten essence...
  if %new_tier% != rescale
    %send% %actor% @%target% trembles in your grasp and blazes brightly as its inner potential is unlocked!
    %echoaround% %actor% @%target% trembles in ^%actor% grasp and blazes brightly as its inner potential is unlocked!
  end
  if %toggle_hard%
    nop %target.flag(HARD-DROP)%
  end
  if %toggle_group%
    nop %target.flag(GROUP-DROP)%
  end
  if %new_tier% == rescale
    %send% %actor% @%target% is now level %max_level%. It remains boss quality.
  else
    %send% %actor% @%target% is now %new_tier% rarity, scaled to level %max_level%.
  end
  %scale% %target% %max_level%
end
~
#18011
Convert old molten essence~
1 b 100
~
eval actor %self.carried_by%
if !%actor%
  halt
end
* Old essence doesnt have quality flags and old items are all boss-drop so old essence should always be worth 4
set quantity 4
nop %actor.give_currency(18000, %quantity%)%
%teleport% %self% %self.room%
eval molten_essence_conversion_temp %actor.var(molten_essence_conversion_temp, 0)% + %quantity%
* Is this the last one?
if %actor.inventory(18097)%
  * It is not
  remote molten_essence_conversion_temp %actor.id%
else
  * It is
  rdelete molten_essence_conversion_temp %actor.id%
  %send% %actor% You transfer %molten_essence_conversion_temp% molten essence from your backpack to your coin pouch.
end
%purge% %self%
~
#18012
Molten Fiend: Fire elemental ramp up~
0 k 100
~
set boss %self.room.people(18075)%
set diff %boss.var(diff,1)%
if %diff%>1
  set stacks %self.var(stacks, 0)%
  * clear ramp buff
  dg_affect #18012 %self% off
  eval stacks %stacks% + 1
  remote stacks %self.id%
  eval tohit %stacks% * 2
  eval damage %stacks%
  dg_affect #18012 %self% TO-HIT %tohit% 30
  dg_affect #18012 %self% BONUS-PHYSICAL %damage% 30
  dg_affect #18012 %self% BONUS-MAGICAL %damage% 30
  if %stacks% // 5 == 0
    %echo% &&O&&Z|%self% flames grow hotter!&&0
  end
end
~
#18056
Tectonic Tuba~
1 b 15
~
if !%self.worn_by%
  halt
elseif %self.worn_by.action% != playing
  halt
end
switch %random.4%
  case 1
    %regionecho% %self.room% 25 A tremor shakes the earth.
  break
  case 2
    %regionecho% %self.room% 25 The earth rumbles beneath you.
  break
  case 3
    %regionecho% %self.room% 25 Everything rattles as the earth quakes around you.
  break
  case 4
    %regionecho% %self.room% 25 A small quake rocks the earth.
  break
done
~
#18070
Molten Fiend: Loot replacement~
1 n 100
~
* loot list: CAUTION: the vnum line is approaching max-line-length of 255
set loot_list 18012 18013 18014 18015 18016 18017 18018 18019 18021 18022 18023 18024 18025 18026 18027 18028 18029 18030 18031 18032 18033 18034 18035 18036 18037 18038 18039 18040 18041 18042 18043 18044 18046 18047 18048 18049 18050
set loot_size 37
* safety
set fiend %self.carried_by%
if !%fiend%
  %purge% %self%
  halt
end
* how much loot?
if %fiend.var(diff,1)% <= 2
  set amount 1
else
  set amount 2
end
* pick 2
eval loot1 %%random.%loot_size%%%
set loot2 %loot1%
while %loot2% == %loot1%
  eval loot2 %%random.%loot_size%%%
done
set count 1
while %count% <= %amount%
  eval pos %%loot%count%%%
  set list %loot_list%
  while %pos% > 0
    set vnum %list.car%
    set list %list.cdr%
    eval pos %pos% - 1
  done
  if %vnum% > 0
    %load% obj %vnum% %fiend% inv
  end
  set obj %self.carried_by.inventory%
  if %obj.vnum% == %vnum%
    if %fiend.mob_flagged(HARD) && !%obj.is_flagged(HARD-DROP)%
      nop %obj.flag(HARD-DROP)%
    end
    if %fiend.mob_flagged(GROUP) && !%obj.is_flagged(GROUP-DROP)%
      nop %obj.flag(GROUP-DROP)%
    end
    if %obj.is_flagged(BOP)%
      nop %obj.bind(%self%)%
    end
    * and scale it
    %scale% %obj% %self.level%
  end
  eval count %count% + 1
done
* bonus loot?
set roll %random.100%
if %roll% <= 5
  * phoenix ash
  set bonus 18096
elseif %roll% <= 15
  * hellsteed
  set bonus 18095
elseif %roll% <= 25
  * fiendskin
  set bonus 18052
elseif %roll% <= 35
  * minipet
  set bonus 18051
elseif %roll% <= 40
  * tectonic tuba
  set bonus 18056
else
  set bonus 0
end
if %bonus% > 0
  %load% obj %bonus% %fiend% inv
  set obj %self.carried_by.inventory%
  if %obj.vnum% == %bonus%
    if %obj.is_flagged(BOP)%
      nop %obj.bind(%self%)%
    end
    * and scale it
    %scale% %obj% %self.level%
  end
end
%purge% %self%
~
#18075
Fissure eruption~
2 ab 1
~
%regionecho% %room% 10 You feel a sudden wave of heat from the nearby fissure!
wait 2 sec
%regionecho% %room% 5 There is a deep, rumbling roar from far beneath your feet!
%asound% The fissure erupts!
%echo% A pillar of molten rock the size of a castle flies right past you!
~
#18076
Fiend Battle~
0 k 25
~
* Deprecated: No longer used
eval test 100*%self.health%/%self.maxhealth%
set phase 1
if %test%<67
  set phase 2
end
if %test%<34
  set phase 3
end
if !%lastphase%
  set lastphase phase
  remote lastphase %self.id%
end
if %lastphase%!=%phase%
  if %lastphase% < %phase%
    *we've advanced a phase
    %echo% ~%self% roars with fury!
    if %phase% == 2
      %echo% One of |%self% chains shatters!
    end
    if %phase% == 3
      %echo% |%self% second chain shatters, freeing it!
    end
  end
  set lastphase %phase%
  remote lastphase %self.id%
  halt
end
if %phase%==1
  switch %random.3%
    case 1
      if !%self.affect(counterspell)%
        counterspell
      else
        if !%actor.affect(colorburst)%
          colorburst
        else
          heal
        end
      end
    break
    case 2
      if !%actor.affect(colorburst)%
        colorburst
      else
        heal
      end
    break
    case 3
      heal
    break
  done
end
if %phase%==2
  switch %random.3%
    case 1
      set room %self.room%
      %echo% ~%self% whistles with a sound like escaping steam...
      %echo% A pair of fire elementals appear!
      %load% mob 18077 ally %self.level%
      %force% %room.people% %aggro% %actor%
      %load% mob 18077 ally %self.level%
      %force% %room.people% %aggro% %actor%
      %echo% ~%self% looks exhausted!
      *Ensure that no other abilities are used while the elementals are up:
      dg_affect %self% STUNNED on 30
      wait 25 sec
    break
    case 2
      %echo% ~%self% raises a hand, leaning back from the platform...
      wait 5 sec
      if %self.disabled%
        %echo% |%self% attack is interrupted!
        halt
      end
      *Buff dodge massively but debuff own tohit massively...
      *should be a breather for average geared parties:
      dg_affect %self% TO-HIT -100 30
      dg_affect %self% SLOW on 30
      dg_affect %self% DODGE 100 30
      %echo% A flickering barrier of force surrounds ~%self%!
      wait 30 sec *Make sure he doesn't do anything else while the shield is up.
      %echo% The magical shield around ~%self% fades!
    break
    case 3
      %echo% ~%self% draws back a hand, curling it into a fist...
      wait 2 sec
      if %self.disabled%
        %echo% |%self% attack is interrupted!
        halt
      end
      %echo% The lava below begins to rise!
      wait 3 sec
      if %self.disabled%
        %echo% |%self% attack is interrupted!
        halt
      end
      %echo% &&rA wave of magma rolls over the platform!&&0
      %aoe% 50 fire
      %send% %actor% &&rYou take the full force of the wave, and are badly burned!&&0
      %echoaround% %actor% ~%actor% takes the full force of the wave!
      %damage% %actor% 100 fire
      %dot% %actor% 100 20 fire
      wait 1 sec
      %echo% The lava sinks back down.
    break
  done
end
if %phase%==3
  switch %random.3%
    case 1
      %echo% ~%self% raises a huge, molten fist!
      wait 5 sec
      *(dex*5)% chance to not get hit
      if %actor.dex%<%random.20%
        %send% %actor% &&rThe earth shakes as ~%self% punches you, stunning you!&&0
        %echoaround% %actor% The earth shakes as ~%self% punches ~%actor%, stunning *%actor%!
        %damage% %actor% 100 physical
        %damage% %actor% 50 fire
        dg_affect %actor% HARD-STUNNED on 10
      else
        %send% %actor% You leap out of the way of |%self% fist at the last second!
        %echoaround% %actor% ~%actor% leaps out of the way of |%self% fist at the last second!
        %send% %actor% &&rThe wave of heat from |%self% fist burns you!&&0
        %damage% %actor% 50 fire
      end
    break
    case 2
      set target %random.enemy%
      * Unable to find a target = return self
      if !%target% || target == %self%
        halt
      end
      %send% %target% ~%self% makes a heretical gesture at you!
      %echoaround% %target% ~%self% makes a heretical gesture at ~%target%!
      wait 5 sec
      if %target%&&(%target.room%==%self.room%)
        if %target.trigger_counterspell(%self%)%
          %send% %target% A tornado of harmless smoke briefly surrounds you!
          %echoaround %target% A tornado of harmless smoke briefly surrounds %target%!
        else
          %send% %target% &&rYour body bursts into infernal flames!&&0
          %echoaround% %target% |%target% body bursts into infernal flames!
          %damage% %target% 25 fire
          %dot% %target% 300 30 fire
        end
      end
    break
    case 3
      %echo% ~%self% rises up out of the lava entirely!
      wait 5 sec
      *Should not be hitting much during this attack
      dg_affect %self% TO-HIT -100 25
      %echo% &&RMana begins condensing around ~%self%!&&0
      wait 5 sec
      %echo% &&RThe platform starts shaking beneath your feet!&&0
      wait 5 sec
      %echo% &&rWaves of fire flood the room!&&0
      %aoe% 100 fire
      wait 5 sec
      %echo% &&rA barrage of sharp rocks falls from the ceiling!&&0
      %aoe% 100 physical
      wait 5 sec
      %echo% &&rThe floating motes of mana in the air transform into ethereal blades!&&0
      %aoe% 50 magical
      %aoe% 50 physical
      wait 5 sec
      %echo% ~%self% sinks back into the lava...
    break
  done
end
*global cooldown
wait 10 sec
~
#18077
Fiend adds timer~
0 n 100
~
wait 30 sec
%echo% &&O&&Z~%self% burns out, disappearing with a puff of smoke!&&0
%purge% %self%
~
#18078
Fiend adds battle~
0 k 20
~
set fiend %self.room.people(18075)%
if %random.3%<3 && %fiend%
  %echo% ~%self% transfers some of its heat to ~%fiend%!
  %heal% %fiend% health 100
  %damage% %self% 100
else
  %send% %actor% &&rThere is a destructive explosion as ~%self% hurls itself suicidally at you!&&0
  %echoaround% %actor% &&rThere is a destructive explosion as ~%self% hurls itself suicidally at ~%actor%!&&0
  %damage% %actor% 200 fire
  %aoe% 50 fire
  %damage% %self% 1000
end
*Global cooldown
wait 5 sec
~
#18079
Fiend No Leave~
0 s 100
~
if !%actor.fighting% && !%self.fighting%
  halt
end
if %actor.nohassle%
  halt
end
%send% %actor% You try to leave, but a wall of fire blocks your escape!
%echoaround% %actor% ~%actor% tries to leave, but a wall of fire blocks ^%actor% escape!
return 0
~
#18080
Fiend Immunities~
0 p 100
~
if !(%abilityname%==disarm)
  halt
end
%send% %actor% You cannot disarm ~%self% - its magic is innate!
return 0
~
#18081
Fiend No Flee~
0 c 0
flee~
%send% %actor% You turn to flee, but a wall of fire blocks your escape!
%echoaround% %actor% ~%actor% turns to flee, but a wall of fire blocks ^%actor% escape!
~
#18082
Molten Fiend: throw stuff in fissure (for molten essence)~
2 c 0
throw~
set val_args lava fissure magma down off ledge
if !(%val_args% ~= %arg.cdr%)
  %send% %actor% Usage: throw <item> lava
  halt
end
set object %actor.obj_target(%arg.car%)%
if !%object%
  %send% %actor% You don't seem to have a %arg.car%.
  halt
end
%send% %actor% You hurl @%object% from the ledge, into the fissure!
%echoaround% %actor% ~%actor% hurls @%object% from the ledge, into the fissure!
%echo% @%object% vanishes into the fiery darkness...
if ((%object.vnum% >= 18075) && (%object.vnum% <= 18094)) || ((%object.vnum% >= 18012) && (%object.vnum% <= 18049))
  if %object.is_flagged(GROUP-DROP)% && %object.is_flagged(HARD-DROP)%
    set value 5
  elseif %object.is_flagged(GROUP-DROP)%
    set value 3
  elseif %object.is_flagged(HARD-DROP)%
    set value 2
  else
    set value 1
  end
end
if %value%
  nop %actor.give_currency(18000, %value%)%
  %send% %actor% The %value% molten essence released by the destruction of @%object% gathers around you.
  %echoaround% %actor% The %value% molten essence released by the destruction of @%object% gathers around ~%actor%.
end
return 1
%purge% %object%
~
#18084
Fissure shop list~
2 c 0
list~
* Deprecated: this script is no longer used
halt
* If player has no essence, ignore the command
if !%actor.has_resources(18097,1)%
  return 0
  halt
end
%send% %actor% &&0All items cost 10 essence each and will be the average level of the
%send% %actor% &&0essence spent to make them. All items are created exactly the same as if the
%send% %actor% &&0fiend dropped that item, but bound to you alone.
%send% %actor% You can create (with 'buy <item>') any of the following items:
%send% %actor% &&0
%send% %actor% firecloth skirt             (caster legs)
%send% %actor% firecloth pauldrons         (caster arms)
%send% %actor% hearthflame skirt           (healer legs)
%send% %actor% hearthflame sleeves         (healer arms)
%send% %actor% magma-forged pauldrons      (tank arms)
%send% %actor% core-forged plate armor     (tank armor)
%send% %actor% huge spiked pauldrons       (melee arms)
%send% %actor% fiendskin jerkin            (melee armor)
%send% %actor% bottomless pouch            (general pack)
%send% %actor% igneous shield              (hybrid shield)
%send% %actor% cold lava ring              (caster ring)
%send% %actor% mantle of flames            (caster about)
%send% %actor% frozen flame earrings       (healer ears)
%send% %actor% emberweave gloves           (healer hands)
%send% %actor% molten gauntlets            (tank hands)
%send% %actor% molten fiend earrings       (tank ears)
%send% %actor% Flametalon                  (melee dagger)
%send% %actor% obsidian earring            (melee ears)
%send% %actor% crown of abyssal flames     (healer head)
%send% %actor% fiendhorn bladed bow        (tank ranged)
~
#18085
Molten fiend shop buy~
2 c 0
buy~
* Deprecated: This script is no longer used
halt
* This trigger is REALLY LONG.
* If player has no essence, ignore the command
if !%actor.has_resources(18097,1)%
  return 0
  halt
end
if !%arg%
  %send% %actor% Create what from essence?
  halt
end
* If player doesn't have enough essence, give an error
if !%actor.has_resources(18097,10)%
  %send% %actor% You need 10 essence.
  return 1
  halt
end
* What are we buying?
set check %arg%
set vnum 0
set name nothing
if firecloth /= %check%
  %send% %actor% Did you mean firecloth skirt or firecloth pauldrons?
  halt
elseif hearthflame s /= %check%
  %send% %actor% Did you mean hearthflame skirt or hearthflame sleeves?
  halt
elseif molten /= %check%
  %send% %actor% Did you mean molten gauntlets or molten fiend earrings?
  halt
elseif fiend /= %check%
  %send% %actor% Did you mean fiendskin jerkin or fiendhorn bladed bow?
  halt
elseif firecloth skirt /= %check%
  set name a firecloth skirt
  set vnum 18075
elseif firecloth pauldrons /= %check%
  set name firecloth pauldrons
  set vnum 18076
elseif hearthflame skirt /= %check%
  set name a hearthflame skirt
  set vnum 18077
elseif hearthflame sleeves /= %check%
  set name hearthflame sleeves
  set vnum 18078
elseif magma-forged pauldrons /= %check%
  set name magma-forged pauldrons
  set vnum 18079
elseif core-forged plate armor /= %check%
  set name core-forged plate armor
  set vnum 18080
elseif huge spiked pauldrons /= %check%
  set name huge spiked pauldrons
  set vnum 18081
elseif fiendskin jerkin /= %check%
  set name a fiendskin jerkin
  set vnum 18082
elseif bottomless pouch /= %check%
  set name a bottomless pouch
  set vnum 18083
elseif igneous shield /= %check%
  set name an igneous shield
  set vnum 18084
elseif cold lava ring /= %check%
  set name a cold lava ring
  set vnum 18085
elseif mantle of flames /= %check%
  set name a mantle of flames
  set vnum 18086
elseif frozen flame earrings /= %check%
  set name frozen flame earrings
  set vnum 18087
elseif emberweave gloves /= %check%
  set name emberweave gloves
  set vnum 18088
elseif molten gauntlets /= %check%
  set name molten gauntlets
  set vnum 18089
elseif molten fiend earrings /= %check%
  set name the molten fiend's earrings
  set vnum 18090
elseif Flametalon /= %check%
  set name Flametalon
  set vnum 18091
elseif obsidian earring /= %check%
  set name an obsidian earring
  set vnum 18092
elseif crown of abyssal flames /= %check%
  set name a crown of abyssal flames
  set vnum 18093
elseif fiendhorn bladed bow /= %check%
  set name a fiendhorn bladed bow
  set vnum 18094
end
if !%vnum%
  %send% %actor% You can't create that from essence.
  halt
end
* We have a valid item
%send% %actor% You bring the ten motes of demonic essence together with a fiery explosion!
%echoaround% %actor% ~%actor% brings ten motes of fiery light together, causing an explosion!
* Charge essence cost and check level to scale to
set total_level 0
set essence_count 0
while %essence_count%<10
  set next_essence %actor.inventory(18097)%
  if !%next_essence%
    * We accidentally ran out of essence in the middle of taking them away... oops
    %send% %actor% Something has gone horribly wrong while creating your item! Please submit a bug report containing this message.
    eval total_level %total_level% + 250
  else
    eval total_level %total_level% + %next_essence.level%
    %purge% %next_essence%
  end
  eval essence_count %essence_count% + 1
done
* If unscaled essence was used, the level may be 0
eval avg_level %total_level% / 10
if %avg_level% < 1
  set avg_level 1
end
* Create the item
%send% %actor% You create %name%!
%echoaround% %actor% ~%actor% creates %name%!
%load% obj %vnum% %actor% inv
set made %actor.inventory(%vnum%)%
if %made%
  nop %made.flag(HARD-DROP)%
  nop %made.flag(GROUP-DROP)%
  %scale% %made% %avg_level%
else
  %send% %actor% Error setting flags and scaling.
end
~
#18086
Fiend Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(18075)%
end
~
$
