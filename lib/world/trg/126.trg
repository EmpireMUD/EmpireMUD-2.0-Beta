#12600
Elemental trap~
1 c 2 5
L b 12601
L b 12602
L b 12603
L b 12604
L b 12605
trap~
if !%arg%
  %send% %actor% Trap whom?
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% They're not here.
  halt
end
if %target.is_pc%
  %send% %actor% That seems kind of mean, don't you think?
  halt
elseif %target.vnum% < 12601 || %target.vnum% > 12605
  %send% %actor% You can only use this trap on elementals from the rift.
  halt
elseif %target.fighting%
  %send% %actor% You can't trap someone who is fighting.
  halt
else
  %send% %actor% You capture ~%target% with @%self%!
  %echoneither% %actor% %target% ~%actor% captures ~%target% with @%self%!
  * Essences have the same vnum as the mob
  %load% obj %target.vnum% %actor% inv
  set obj %actor.inventory%
  %send% %actor% You receive @%obj%!
  %purge% %target%
  %purge% %self%
end
~
#12601
Earth Elemental: Burrow Charge~
0 k 100 3
L w 12601
L w 12605
L w 12606
~
if %self.cooldown(12605)%
  halt
end
nop %self.set_cooldown(12605, 20)%
%send% %actor% ~%self% burrows into the ground and starts shifting through the earth towards you!
%echoaround% %actor% ~%self% burrows into the ground and starts shifting through the earth towards ~%actor%!
%echo% (Type 'stomp' to interrupt it.)
set running 1
remote running %self.id%
set success 0
remote success %self.id%
dg_affect #12601 %self% DODGE 50 10
nop %self.add_mob_flag(NO-ATTACK)%
wait 10 seconds
set running 0
remote running %self.id%
if %self.varexists(success)%
  if %self.success%
    set success 1
  end
end
if %success% || %self.room% != %actor.room%
  nop %self.remove_mob_flag(NO-ATTACK)%
  dg_affect #12601 %self% off
else
  %send% %actor% ~%self% bursts from the ground beneath your feet, knocking you over!
  %echoaround% %actor% ~%self% bursts from the ground beneath |%actor% feet, knocking *%actor% over!
  %damage% %actor% 75 physical
  dg_affect #12606 %actor% STUNNED on 5
  nop %self.remove_mob_flag(NO-ATTACK)%
  dg_affect #12601 %self% off
end
~
#12602
Fire Elemental: Summon Ember~
0 k 100 2
L b 12605
L w 12605
~
if %self.cooldown(12605)%
  halt
end
nop %self.set_cooldown(12605, 20)%
%echo% ~%self% grabs a handful of soil from the ground and crushes it in ^%self% fist!
%load% mob 12605 ally
%echo% A floating ember is formed!
~
#12603
Air Elemental: Dust~
0 k 100 2
L w 12603
L w 12605
~
if %self.cooldown(12605)%
  halt
end
nop %self.set_cooldown(12605, 20)%
%send% %actor% ~%self% whips up a cloud of dust and launches it into your face, blinding you!
%echoaround% %actor% ~%self% whips up a cloud of dust and launches it into |%actor% face, blinding *%actor%!
dg_affect #12603 %actor% BLIND on 5
~
#12604
Water Elemental: Envelop~
0 k 100 3
L w 12604
L w 12605
L w 12607
~
if %self.cooldown(12605)%
  halt
end
set check_id %actor.id%
nop %self.set_cooldown(12605, 20)%
%send% %actor% ~%self% surges forward and envelops you!
%echoaround% %actor% ~%self% surges forward and envelops ~%actor%!
%send% %actor% (Type 'struggle' to break free.)
dg_affect #12604 %actor% HARD-STUNNED on 20
dg_affect #12607 %self% HARD-STUNNED on 20
while 1
  wait 5 s
  if %actor.id% != %check_id% || %actor.room% != %self.room%
    halt
  elseif !%actor.affect(12604)%
    halt
  end
  %send% %actor% ~%self% pummels and crushes you!
  %echoaround% %actor% ~%self% pummels and crushes ~%actor%!
  %damage% %actor% 50 physical
  %send% %actor% (Type 'struggle' to break free.)
done
~
#12605
Elemental Rift spawn~
0 n 100 4
L b 12600
L b 12602
L c 12611
L j 12600
~
if %self.vnum% == 12602
  %load% obj 12611 %self% inv
end
if %self.room.template% == 12600
  * normal spawn
  set room %instance.location%
  if !%room%
    halt
  end
  mgoto %room%
  if %self.vnum% != 12600
    mmove
    mmove
    mmove
    mmove
    mmove
    mmove
    mmove
    mmove
    mmove
    mmove
  end
end
~
#12606
Elemental Death~
0 f 100 5
L b 12601
L b 12602
L b 12603
L b 12604
L c 12610
~
* check level limit
if %actor.level% > 50
  * over-leveled: no loot and instant respawn
  nop %self.add_mob_flag(!LOOT)%
  switch %self.vnum%
    case 12601
      %echo% ~%self% falls apart but imediately reassembles itself!
    break
    case 12602
      %echo% ~%self% sputters for a moment but flares back to life!
    break
    case 12603
      %echo% ~%self% disspates for a moment but stirs up again almost immediately!
    break
    case 12604
      %echo% ~%self% splashes to the ground but rises again almost instantly!
    break
  done
  %load% mob %self.vnum%
  return 0
else
  * not over-leveled: die normally
  nop %self.remove_mob_flag(!LOOT)%
  if %instance.start%
    %at% %instance.start% %load% obj 12610
  end
  switch %self.vnum%
    case 12601
      %echo% ~%self% drops into a little pile of earth essence.
    break
    case 12602
      %echo% ~%self% drops to the ground as a burning bit of fire essence.
    break
    case 12603
      %echo% ~%self% dissipates until it's no more than some blowing wind essence.
    break
    case 12604
      %echo% ~%self% splashes to the ground as little more than water essence.
    break
  done
  return 1
end
set obj %self.inventory%
while %obj%
  set next_obj %obj.next_in_list%
  %purge% %obj%
  set obj %next_obj%
done
~
#12607
Stomp earth elemental~
0 c 0 2
L w 12601
L w 12606
stomp~
if !%self.varexists(success)%
  set success 0
  remote success %self.id%
end
if !%self.varexists(running)%
  set running 0
  remote running %self.id%
end
if !%self.running% || %self.success%
  %send% %actor% You don't need to do that right now.
  halt
end
%send% %actor% You stomp down on ~%self%, interrupting ^%self% burrowing charge and stunning *%self%.
%echoaround% %actor% ~%actor% stomps down on ~%self%, interrupting ^%self% burrowing charge and stunning *%self%.
set success 1
remote success %self.id%
dg_affect #12606 %self% STUNNED on 5
nop %self.remove_mob_flag(NO-ATTACK)%
dg_affect #12601 %self% off
%echo% ~%self% emerges from the earth, dazed.
~
#12608
Ember: Attack~
0 k 100 0
~
%send% %actor% ~%self% shoots a small bolt of fire at you.
%echoaround% %actor% ~%self% shoots a small bolt of fire at ~%actor%.
%damage% %actor% 25
* attempt to ensure the actor is in combat because this mob does not trigger combat itself
if %actor.position% == Standing && !%actor.disabled% && !%actor.fighting%
  %force% %actor% hit ember
end
~
#12609
Delayed completion on quest start~
0 uv 0 1
L c 12610
~
if %instance.start%
  %at% %instance.start% %load% obj 12610
end
~
#12610
Delayed Completer~
1 f 0 0
~
%adventurecomplete%
~
#12611
Water elemental: Struggle~
0 c 0 2
L w 12604
L w 12607
struggle~
set break_free_at 1
if !%actor.affect(12604)%
  return 0
  halt
end
if !%actor.varexists(struggle_counter)%
  set struggle_counter 0
  remote struggle_counter %actor.id%
else
  set struggle_counter %actor.struggle_counter%
end
eval struggle_counter %struggle_counter% + 1
if %struggle_counter% >= %break_free_at%
  %send% %actor% You break free of |%self% grip!
  %echoaround% %actor% ~%actor% breaks free of |%self% grip!
  dg_affect #12604 %actor% off
  dg_affect #12607 %self% off
  rdelete struggle_counter %actor.id%
  halt
else
  %send% %actor% You struggle against your bindings, but fail to break free.
  %echoaround% %actor% ~%actor% struggles against ^%actor% bindings!
  remote struggle_counter %actor.id%
  halt
end
~
#12612
Give rejection~
0 j 100 0
~
%send% %actor% Don't give the item to ~%self%, use 'quest finish <quest name>' instead (or 'finish all').
return 0
~
#12625
Strange Plant: Setup trig~
2 n 100 10
L b 12625
L b 12626
L b 12627
L b 12628
L b 12641
L c 9682
L e 12625
L e 12626
L e 12627
L e 12628
~
set loc %instance.location%
if !%loc%
  halt
end
switch %loc.building_vnum%
  case 12625
    * Pitcher plant
    set exitport %room.contents(12625)%
    if %exitport%
      %purge% %exitport%
    end
    %door% %room% up room %loc%
    %load% mob 12625
  break
  case 12626
    * Sundew
    %load% mob 12641
    %load% mob 12626
    set mob %room.people%
    if %mob.vnum% == 12626
      %teleport% %mob% %loc%
    end
  break
  case 12627
    * Lantern vines
    %load% mob 12641
    %load% mob 12627
    set mob %room.people%
    if %mob.vnum% == 12627
      %teleport% %mob% %loc%
    end
  break
  case 12628
    * Bog maw
    %load% mob 12641
    %load% mob 12628
    set mob %room.people%
    if %mob.vnum% == 12628
      %teleport% %mob% %loc%
    end
  break
done
%at% %loc% %load% obj 9682
~
#12626
Strange Plant: Player interacts with plant~
0 e 1 1
L c 12644
you~
if !%actor.nohassle%
  %load% obj 12644 %actor%
  %send% %actor% Uh oh...
  nop %actor.command_lag(COMBAT-ABILITY)%
end
~
#12627
Strange Plant: Mystery crop determination~
1 c 2 3
L g 12630
L g 12632
L g 12633
plant~
* temporarily changes crop type and then changes back
set safety_vnum 12632
return 0
*
if %actor.obj_target(%arg.argument1%)% != %self%
  halt
end
*
if %self.room.coords(x)% == ???
  %send% %actor% You can't plant @%self% here.
  return 1
  halt
elseif %self.room.coords(y)% >= (%world.height% / 2)
  set vnum 12633
else
  set vnum 12630
end
*
nop %self.val1(%vnum%)%
*
* set back if not planted by now
wait 1
nop %self.val1(%safety_vnum%)%
~
#12628
Strange Plant: Greet and trap~
2 g 100 1
L c 12644
~
if %method% != goto
  %load% obj 12644 %actor%
end
~
#12629
Strange Plant: Impending death countdown in the pitcher plant~
0 bw 100 1
L c 12642
~
* Tracks each player's time inside and kills them if it's been too long.
set room %self.room%
set limit 50
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_pc% && !%ch.dead%
    * find or set an entry time
    set entry_time %self.var(entry_time_%ch.id%)%
    if %ch.is_flying%
      rdelete entry_time_%ch.id% %self.id%
      set entry_time %timestamp%
    elseif !%entry_time%
      set entry_time %timestamp%
      set entry_time_%ch.id% %timestamp%
      remote entry_time_%ch.id% %self.id%
    end
    eval inside_time %timestamp% - %entry_time%
    if %inside_time% >= %limit%
      * death!
      %load% obj 12642 %ch% inv
    elseif %inside_time% >= %limit% / 2
      %send% %ch% &&L**** The liquid in the plant is getting to you... You don't feel so good. ****&&0
    end
  end
  set ch %next_ch%
done
~
#12630
Strange Plant: Trap on leave~
2 q 100 1
L c 12644
~
set safe_methods ability enter exit portal summon goto transfer system script
if %safe_methods% ~= %method%
  halt
end
*
if %actor.wits% > %actor.dexterity%
  set trait %actor.wits%
else
  set trait %actor.dexterity%
end
*
if %trait% > %random.14%
  * safe
elseif !%actor.nohassle% && !%actor.dead% && !%actor.is_flying%
  %load% obj 12644 %actor%
  %send% %actor% Uh oh...
  nop %actor.command_lag(MOVEMENT)%
  return 0
end
~
#12631
Strange Plant: Defeat the plant~
0 f 100 20
L b 12638
L b 12641
L c 12626
L c 12627
L c 12628
L c 12629
L c 12635
L c 12645
L e 12625
L e 12626
L e 12627
L e 12628
L f 12628
L f 12630
L f 12639
L j 12625
L w 12626
L w 12627
L w 12628
L w 12629
~
* Shared death trigger
set room %self.room%
if %room.template% == 12625
  set outside %instance.location%
else
  set outside %room%
end
*
* reward minipet
set ch %room.people%
while %ch%
  if %ch.is_pc% && !%ch.has_minipet(12638)%
    nop %ch.add_minipet(12638)%
    %send% %ch% &&LYou collect a seedling from the strange plant! A new minipet!&&0
  end
  set ch %ch.next_in_room%
done
*
switch %outside.building_vnum%
  case 12625
    * Pitcher plant
    *
    * defeat message
    %echo% The side of the plant splits open and gives you a way out!
    if %outside%
      if %outside.contents(12635)%
        %at% %outside% %echo% The strange plant splits open as it crackles and burns, spilling its contents out!
      else
        %at% %outside% %echo% The strange plant splits open from the inside and its contents tumble out!
      end
    end
    *
    * load safety mob - teleports players out
    if %room.template% == 12625
      %load% mob 12641
    end
    *
    * cancel outside traps
    if %outside%
      detach 12628 %outside.id%
      detach 12639 %outside.id%
      detach 12630 %outside.id%
      set portal %outside.contents(12625)%
      if %portal%
        %purge% %portal%
      end
      if !%outside.contents(12635)%
        %load% obj 12629 %outside%
      end
    end
  break
  case 12626
    * Sundew
    %echo% The sundew is completely crushed from all the struggling and has now stuck itself to the ground.
    %load% obj 12626
    * cancel traps
    detach 12630 %room.id%
    * free players
    set ch %room.people%
    while %ch%
      if %ch.affect(12626)%
        dg_affect #12626 %ch% off
        %send% %ch% You manage to free yourself from the sticky plant.
      end
      set obj %ch.inventory(12645)%
      if %obj% && %obj.vnum% == 12645
        %purge% %obj%
      end
      set ch %ch.next_in_room%
    done
  break
  case 12627
    * Lantern vines
    %echo% The vines whip and curl as the flames consume them, sending burning pods cascading to the ground!
    * cancel trap
    detach 12630 %room.id%
    * free players
    set ch %room.people%
    while %ch%
      if %ch.affect(12627)%
        dg_affect #12627 %ch% off
        %dot% #12628 %ch% 100 10 fire
        %send% %ch% You free yourself from the vines as the fire consumes them!
      end
      set ch %ch.next_in_room%
    done
    %load% obj 12627
  break
  case 12628
    * Bog maw
    %echo% The bog maw gags on the debris crammed into its throat, convulses, heaves, then sinks into the muck.
    * cancel trap
    detach 12630 %room.id%
    * free players
    set ch %room.people%
    while %ch%
      if %ch.affect(12629)%
        dg_affect #12629 %ch% off
        %send% %ch% You finally get free and gasp for air as you manage to break the surface!
      end
      set ch %ch.next_in_room%
    done
    %load% obj 12628 %instance.start%
  break
done
*
* no death cry
return 0
~
#12632
Strange Plant: Sundew struggle ticker~
0 bw 100 2
L c 12642
L w 12626
~
* ticks 3 times per 13-second random interval
set room %self.room%
set times 0
while %times% <= 2
  * safety
  if %self.dead%
    halt
  end
  * check for trapped players
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch% != %self% && %ch.affect(12626)%
      set tick %self.var(ticks_%ch.id%,0)%
      * messaging
      switch %self.var(ticks_%ch.id%,0)%
        case 0
          %send% %ch% &&L**** As you try to free yourself, more of the sundew's sticky tendrils attach themselves to you! ****&&0 (struggle)
        break
        case 1
          %send% %ch% &&L**** The plant's glistening fronds curl tighter, sticking to your arms and legs! ****&&0 (struggle)
          %echoaround% %ch% The plant curls tighter around ~%ch%.
        break
        case 2
          %send% %ch% &&L**** You're yanked lower as more tendrils snag your clothing and hair! ****&&0 (struggle)
        break
        case 3
          %send% %ch% &&L**** The sweet, sharp smell of the sundew fills your nose as it pulls you closer to the ground! ****&&0 (struggle)
          %echoaround% %ch% ~%ch% is pulled to the ground by the plant.
        break
        case 4
          %send% %ch% &&L**** Your limbs feel heavy and slow, glued down by layers of sticky dew! ****&&0 (struggle)
        break
        case 5
          %send% %ch% &&L**** The tendrils twitch and writhe, wrapping around your chest and shoulders! ****&&0 (struggle)
          %echoaround% %ch% The plant's tendrils wrap around |%ch% chest and shoulders.
        break
        case 6
          %send% %ch% &&L**** Strands of the plant stretch across your face, clinging to your skin! ****&&0 (struggle)
          %echoaround% %ch% Strands of the strange plant stretch across |%ch% face, clinging to ^%ch% skin.
        break
        case 7
          %send% %ch% &&L**** You can barely move as the sticky threads begin to pull at your head. ****&&0 (struggle)
          %echoaround% %ch% ~%ch% struggles as the plant's sticky threads pull at ^%ch% head.
        break
        case 8
          %send% %ch% &&L**** A tendril slaps across your face, sealing your mouth shut with its resinous slime! ****&&0 (struggle)
          %echoaround% %ch% A tendril slaps across |%ch% face, sealing ^%ch% mouth shut!
        break
        case 9
          %send% %ch% &&L**** The sundew's sticky sap clings to your nose and sucks inside as you struggle to breathe! ****&&0 (struggle)
          %echoaround% %ch% The sundew's sticky sap clings to |%ch% nose!
        break
        default
          * >= 10
          %load% obj 12642 %ch%
        break
      done
      * update
      eval ticks_%ch.id% %self.var(ticks_%ch.id%,0)% + 1
      remote ticks_%ch.id% %self.id%
    end
    set ch %next_ch%
  done
  * repeat
  wait 4 s
  eval times %times% + 1
done
~
#12633
Strange Plant: Lantern vine strangle ticker~
0 bw 100 2
L c 12642
L w 12627
~
* ticks twice per 13-second random interval
set room %self.room%
set times 0
while %times% <= 1
  * safety
  if %self.dead%
    halt
  end
  * check for trapped players
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch% != %self% && %ch.affect(12627)%
      set tick %self.var(ticks_%ch.id%,0)%
      * messaging
      switch %self.var(ticks_%ch.id%,0)%
        case 0
          %send% %ch% &&L**** Glowing pods sway above you as the rough vine coils tighter around your throat! ****&&0
        break
        case 1
          %send% %ch% &&L**** More tendrils lash out of the dark and wrap around your arms! ****&&0
          %echoaround% %ch% More tendrils lash out and entwine ~%ch%.
        break
        case 2
          %send% %ch% &&L**** The dry vine at your neck pulls sharply, cutting your breath short! ****&&0
        break
        case 3
          %send% %ch% &&L**** Coarse cords sinch across your chest and pull tight! ****&&0
          %echoaround% %ch% Vines sinch across |%ch% chest and pull tight!
        break
        case 4
          %send% %ch% &&L**** A lantern pod swings nearer, dampening the parched vines with dripping nectar. ****&&0
        break
        case 5
          %send% %ch% &&L**** The dry tendrils rasp together as they knot around your body, binding in midair! ****&&0
          %echoaround% %ch% The plant's tendrils rasp together as they knot around |%ch% body and bind *%ch% in midair!
        break
        case 6
          %send% %ch% &&L**** Your vision darkens as the constricting vines squeeze tight around your throat. ****&&0
          %echoaround% %ch% |%ch% eyes roll back as ^%ch% face turns purple.
        break
        default
          * >= 7
          %load% obj 12642 %ch%
        break
      done
      * update
      eval ticks_%ch.id% %self.var(ticks_%ch.id%,0)% + 1
      remote ticks_%ch.id% %self.id%
    end
    set ch %next_ch%
  done
  * repeat
  wait 6 s
  eval times %times% + 1
done
~
#12634
Strange Plant: Bog maw ticker~
0 bw 100 2
L c 12642
L w 12629
~
* ticks twice per 13-second random interval
set room %self.room%
set times 0
while %times% <= 1
  * safety
  if %self.dead%
    halt
  end
  * check for trapped players
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch% != %self% && %ch.affect(12629)%
      set tick %self.var(ticks_%ch.id%,0)%
      * messaging
      switch %self.var(ticks_%ch.id%,0)%
        case 0
          %send% %ch% &&L**** The swamp squeezes tighter around you as you're dragged deeper! ****&&0
        break
        case 1
          %send% %ch% &&L**** There's a crushing feeling in your chest as the muck rises around you! ****&&0
          %echoaround% %ch% ~%ch% sinks deeper into the mud.
        break
        case 2
          %send% %ch% &&L**** You strain with your arms to try to keep yourself above water! ****&&0
        break
        case 3
          %send% %ch% &&L**** Filthy swamp water enters your mouth as the bog maw drags you further down! ****&&0
          %echoaround% %ch% ~%ch% gasps for air as something drags *%ch% under.
        break
        case 4
          %send% %ch% &&L**** Muck fills your ears and burns your eyes as you sink beneath the surface! ****&&0
        break
        case 5
          %send% %ch% &&L**** You claw upward but the bog maw drags you deeper beneath the muck! ****&&0
          %echoaround% %ch% There are burbles from the swamp where ~%ch% sank.
        break
        case 6
          %send% %ch% &&L**** Your lungs ache for air as the bog presses in from every side! ****&&0
          %echoaround% %ch% There are burbles from the swamp where ~%ch% sank.
        break
        case 7
          %send% %ch% &&L**** The last bubbles escape your mouth and vanish above you in the mire! ****&&0
          %echoaround% %ch% There are burbles from the swamp where ~%ch% sank.
        break
        default
          * >= 8
          %load% obj 12642 %ch%
        break
      done
      * update
      eval ticks_%ch.id% %self.var(ticks_%ch.id%,0)% + 1
      remote ticks_%ch.id% %self.id%
    end
    set ch %next_ch%
  done
  * repeat
  wait 6 s
  eval times %times% + 1
done
~
#12635
Strange Plant: Reject attacks and trap player~
0 B 0 4
L b 12626
L b 12627
L b 12628
L c 12644
~
switch %self.vnum%
  case 12626
    * sundew
    %send% %actor% That might not have been the best idea... you get a little too close to the plant.
    %echoaround% %actor% ~%actor% gets a little too close to the plant.
    %load% obj 12644 %actor%
  break
  case 12627
    * lantern vines
    %send% %actor% You try to swing at the plant, but there are just too many tendrils... one of which has fallen onto your shoulder.
    %echoaround% %actor% ~%actor% tries to swing at the plant and doesn't notice the tendril dropping onto ^%actor% shoulder.
    %load% obj 12644 %actor%
  break
  case 12628
    * bog maw
    %send% %actor% You look for a way to attack the plant, but it's hard to hit it under the water...
    %echoaround% %actor% ~%actor% strikes frantically at the water but it's not having the effect &%actor% wanted.
    %load% obj 12644 %actor%
  break
done
return 0
~
#12636
Strange Plant: Delayed despawn~
1 f 0 6
L e 12625
L e 12626
L e 12627
L e 12628
L j 12625
L j 12626
~
* start here
set room %self.room%
* move outside if in the adventure
if %room.template% == 12625 || %room.template% == 12626
  set room %instance.location%
end
* ensure we have a room
if !%room%
  halt
end
* ensure it's part of this adventure
if %room.building_vnum% >= 12625 && %room.building_vnum% <= 12628
  %adventurecomplete%
  %terraform% %room% %room.base_sector_vnum%
end
~
#12637
Strange Plant: Trap timer helper~
1 n 100 2
L b 12625
L c 9680
~
wait 3 s
makeuid actor %self.var(actor_id,0)%
makeuid loc %self.var(room_id,-1)%
set inside %instance.start%
if !%actor% || !%loc% || !%inside%
  %purge% %self%
  halt
end
if !%inside.people(12625)%
  * already dead
  %purge% %self%
  halt
end
if %actor.id% != %self.var(actor_id,0)% || %actor.room% != %loc%
  * found wrong thing
  %purge% %self%
  halt
end
if %actor.is_flying% || %actor.dead% || %actor.nohassle%
  %purge% %self%
  halt
end
* ok, ready to trap:
%teleport% %self% %loc%
%send% %actor% You slip and fall into an opening in the vegetation...
%echoaround% %actor% ~%actor% slips and falls into an opening in the vegetation...
%teleport% %actor% %inside%
%teleport% %self% %inside%
%echoaround% %actor% ~%actor% falls in from above!
%load% obj 9680 %actor%
%purge% %self%
~
#12638
Strange Plant: Room commands (burn, light, chop, quit)~
2 c 0 14
L b 12625
L b 12626
L b 12627
L b 12628
L c 12625
L c 12635
L c 12644
L c 12645
L w 12626
L w 12627
L w 12629
L w 12630
L w 12631
L w 12632
burn light chop dig gather harvest pick plant quit struggle~
* Shared command trig
*
set quick_cmds chop dig gather harvest pick plant
if %quick_cmds% ~= %cmd%
  %send% %actor% It isn't safe to %cmd% right now.
  return 1
  halt
elseif %cmd% == quit
  if %actor.affect(12626)% || %actor.affect(12627)% || %actor.affect(12629)%
    %send% %actor% You can't quit right now!
    return 1
  else
    return 0
  end
  halt
elseif struggle /= %cmd%
  * struggle helper for some rooms (separate from sundew struggle)
  return 0
  if %actor.inventory(12645)%
    * real struggle
  elseif %actor.affect(12627)%
    * entwined
    %send% %actor% Struggling only makes it worse!
    %echoaround% %actor% ~%actor% struggles against the tendrils.
    %dot% #12630 %actor% 75 60 physical 10
    return 1
  elseif %actor.affect(12629)%
    * bogged
    %send% %actor% Struggling only makes it worse!
    %echoaround% %actor% ~%actor% struggles as &%actor% is pulled under.
    %dot% #12631 %actor% 75 60 physical 10
    return 1
  end
  halt
end
*
* light or burn:
return 0
set obj_targ %actor.obj_target(%arg%)%
set char_targ %actor.char_target(%arg%)%
set veh_targ %actor.veh_target(%arg%)%
if %veh_targ%
  * likely normal use
  halt
elseif %char_targ%
  switch %char_targ.vnum%
    case 12625
      %send% %actor% There's too much moisture in here to burn it.
      return 1
      halt
    break
    case 12626
      %send% %actor% The droplets of liquid covering the plant prevent it from burning.
      %load% obj 12644 %actor%
      return 1
      halt
    break
    case 12627
      * ok!
    break
    case 12628
      %send% %actor% It's submerged in the swamp; you won't have any luck burning it.
      %load% obj 12644 %actor%
      return 1
      halt
    break
    default
      * other: pass thru
      halt
    break
  done
elseif %obj_targ%
  if %obj_targ.vnum% == 12625
    * ok!
  else
    * normal use of light/burn
    halt
  end
else
  * no target: normal fail
  halt
end
*
* set return value now as we have accepted the command
return 1
*
* require burn not light
if %cmd% == light
  %send% %actor% You can't light it with this command. Try 'burn' instead.
  halt
end
*
* find lighter
set magic_light %actor.has_tech(Light-Fire)%
set use_lighter %actor.find_lighter%
if !%magic_light% && !%use_lighter%
  %send% %actor% And you don't have any way to burn it.
  halt
end
*
* check not stunned/dead
if %actor.disabled% || %actor.dead%
  %send% %actor% You can't do that right now!
  halt
end
*
* attempt it!
if %actor.wits% > %actor.dexterity%
  set trait %actor.wits%
else
  set trait %actor.dexterity%
end
if %trait% < %random.16%
  * fail!
  %send% %actor% You try to burn it, but can't get it to light!
  %echoaround% %actor% ~%actor% tries to burn the plant but can't seem to get it to light.
  nop %actor.command_lag(COMBAT-ABILITY)%
  %load% obj 12644 %actor%
else
  * success!
  %send% %actor% You move quickly and manage to burn the plant!
  %echoaround% %actor% ~%actor% manages to get close enough to burn the plant!
  * free players / slay mob
  if %obj_targ% && %obj_targ.vnum% == 12625
    %load% obj 12635
    set mob %instance.mob(12625)%
    if %mob%
      %scale% %mob% %actor.level%
      * burn players inside
      set ch %mob.room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %ch% != %mob% && !%ch.aff_flagged(!ATTACK)%
          %dot% #12632 %ch% 100 10 fire
        end
        set ch %next_ch%
      done
      %at% %mob.room% %slay% %mob%
    end
  elseif %char_targ%
    %scale% %char_targ% %actor.level%
    %slay% %char_targ%
  end
end
* mark lighter used
if %use_lighter% && !%magic_light%
  nop %use_lighter.used_lighter(%actor%)%
end
~
#12639
Strange Plant: Randomly check for victims~
2 bw 50 1
L c 12644
~
set actor %room.people%
while %actor%
  if %actor% != %self% && !%actor.nohassle% && !%actor.dead%
    %load% obj 12644 %actor%
  end
  set actor %actor.next_in_room%
done
~
#12640
Strange Plant: Only way out is up (must fly)~
2 q 100 0
~
if %direction% == up && !%actor.is_flying%
  %send% %actor% The sides of the plant are too slippery... there's no way up!
  return 0
end
~
#12641
Strange Plant: Safety catch after completion~
0 hn 100 2
L c 9680
L j 12625
~
* Teleport all players/followers out if finished
if %actor.nohassle%
  halt
end
wait 0
set room %self.room%
if %room.template% != 12625
  %purge% %self%
  halt
end
* Ensure part of the instance
nop %self.link_instance%
set to_room %instance.location%
if !%to_room%
  * No destination
  %purge% %self%
  halt
end
* Message now
%echo% You manage to escape the damaged plant!
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
  if %ch.nohassle%
    * nothing
  elseif %ch.is_pc% || !%ch.linked_to_instance%
    * Move ch
    %teleport% %ch% %to_room%
    %load% obj 9680 %ch%
  end
  set ch %next_ch%
done
~
#12642
Strange Plant: You died helper object~
1 n 100 4
L e 12625
L e 12626
L e 12627
L e 12628
~
wait 0
set actor %self.carried_by%
set room %self.room%
set outside %instance.location%
*
if !%actor%
  %purge% %self%
  halt
end
*
* determine death type
if %outside%
  set vnum %outside.building_vnum%
else
  * safety backup
  set vnum 12625
end
*
* kill npc followers
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_npc% && %ch.leader% == %actor%
    switch %vnum%
      case 12625
        %echo% ~%ch% collapses in the fluid.
      break
      case 12626
        %echo% ~%ch% gets stuck in the sundew and dies.
      break
      case 12627
        %echo% ~%ch% is strangled by the vines and dies.
      break
      case 12628
        %echo% ~%ch% drowns beneath the muck.
      break
    done
    %slay% %ch%
  end
  set ch %next_ch%
done
* and the person themselves
switch %vnum%
  case 12625
    %send% %actor% &&LThe world goes dark as you collapse in pain into the deadly fluid!&&0
    %echoaround% %actor% ~%actor% collapses in the fluid as bubbles rise from ^%actor% corpse.
    %slay% %actor% %actor.real_name.cap% has dissolved inside a strange plant at %room.coords%!
  break
  case 12626
    %send% %actor% &&LThe world goes dark as you're unable to remove the sticky dew from your face!&&0
    %echoaround% %actor% The light fades from |%actor% eyes as the sticky dew suffocates *%actor%.
    %slay% %actor% %actor.real_name.cap% has been suffocated by a strange plant at %room.coords%!
  break
  case 12627
    %send% %actor% &&LThe world fades away... You feel your neck snap just as the last light fades from your eyes.&&0
    %echoaround% %actor% There's a loud snap from |%actor% neck as the tendril chokes away the last of ^%actor% life.
    %slay% %actor% %actor.real_name.cap% has been strangled by a strange plant at %room.coords%!
  break
  case 12628
    %send% %actor% &&LDarkness fills your lungs as the swamp drowns you in silence.&&0
    %echoaround% %actor% The last few bubbles break at the muck's surface as ~%actor% vanishes below.
    %slay% %actor% %actor.real_name.cap% has been drowned by a strange plant at %room.coords%!
  break
  default
    %send% %actor% You die.
    %echoaround% %actor% ~%actor% dies.
    %slay% %actor% %actor.real_name.cap% has died at %room.coords%!
  break
done
%purge% %self%
~
#12643
Strange Plant: Reset timer on entry~
0 h 100 0
~
* if a character escapes, the prevents them from dying immediately on return
rdelete entry_time_%actor.id% %self.id%
~
#12644
Strange Plant: Trap helper~
1 n 100 13
L b 12626
L b 12627
L b 12628
L c 12637
L c 12645
L e 12625
L e 12626
L e 12627
L e 12628
L j 12626
L w 12626
L w 12627
L w 12629
~
wait 0
set actor %self.carried_by%
if !%actor%
  %purge% %self%
  halt
end
set room %actor.room%
*
* behavior depends on which trap
switch %room.building_vnum%
  case 12625
    * pitcher plant
    %load% obj 12637 %room%
    set trap %room.contents%
    if %trap.vnum% != 12637
      halt
    end
    set actor_id %actor.id%
    remote actor_id %trap.id%
    set room_id %room.id%
    remote room_id %trap.id%
    %teleport% %trap% i12626
    * %teleport% %trap% %instance.nearest_rmt(12626)%
  break
  case 12626
    * sundew plant
    set sundew %room.people(12626)%
    if !%sundew% || %actor.affect(12626)%
      %purge% %self%
      halt
    end
    %send% %actor% &&L**** You get a little too close to the sundew plant and find yourself stuck to it! ****&&0 (struggle)
    %echoaround% %actor% ~%actor% gets a little too close to the sundew plant and finds *%actor%self stuck to it!
    dg_affect #12626 @%actor% %actor% HARD-STUNNED on 300
    set ticks_%actor.id% 0
    remote ticks_%actor.id% %sundew.id%
    %load% obj 12645 %actor%
  break
  case 12627
    * lantern vines
    set vines %room.people(12627)%
    if !%vines% || %actor.affect(12627)%
      %purge% %self%
      halt
    end
    %send% %actor% &&L**** Dry vines scratch your skin as they wrap around your neck! ****&&0
    %echoaround% %actor% ~%actor% gets a little too close to the lantern vines and finds tendrils wrapped around ^%actor% neck!
    dg_affect #12627 @%actor% %actor% IMMOBILIZED on 300
    dg_affect #12627 @%actor% %actor% DISTRACTED on 300
    set ticks_%actor.id% 0
    remote ticks_%actor.id% %vines.id%
  break
  case 12628
    * bog maw
    set maw %room.people(12628)%
    if !%maw% || %actor.affect(12629)%
      %purge% %self%
      halt
    end
    %send% %actor% &&L**** The muck beneath you surges open and a massive sucking force clamps around your legs, dragging you down into the swamp... ****&&0
    %echoaround% %actor% ~%actor% makes a panic face and is yanked downward...
    dg_affect #12629 @%actor% %actor% IMMOBILIZED on 300
    dg_affect #12629 @%actor% %actor% DISTRACTED on 300
    set ticks_%actor.id% 0
    remote ticks_%actor.id% %maw.id%
  break
done
%purge% %self%
~
#12645
Strange Plant: Struggle command~
1 c 2 2
L b 12626
L w 12626
struggle~
set target 45
set times_needed 3
*
set sundew %actor.room.people(12626)%
if !%sundew%
  dg_affect #12626 %actor% off
  %send% %actor% You struggle free!
  %purge% %self%
  halt
end
*
if %actor.intelligence% > %actor.strength%
  set trait %actor.intelligence%
  set type intelligence
else
  set trait %actor.strength%
  set type strength
end
eval amount %self.var(amount,0)% + 1 + %%random.%trait%%%
if %amount% < %target%
  * still stuck
  if %type% == strength
    %send% %actor% You struggle against the sticky droplets...
    %echoaround% %actor% ~%actor% struggles to try to free *%actor%self...
  else
    %send% %actor% You look for a way to free yourself from the sticky droplets...
    %echoaround% %actor% ~%actor% looks for a way to free *%actor%self...
  end
  nop %actor.command_lag(COMBAT-ABILITY)%
  remote amount %self.id%
  halt
end
* done!
dg_affect #12626 %actor% off
if %type% == strength
  %send% %actor% You find a bit of leverage and pull yourself free!
  %echoaround% %actor% ~%actor% finds a bit of leverage and pulls *%actor%self free of the plant!
else
  %send% %actor% You manage to splash water from nearby plants onto the sundew to dilute it... you're free!
  %echoaround% %actor% ~%actor% splashes some water from nearby plants onto the sundew to dilute it... &%actor%'s free!
end
* number of completions?
eval strugglers %sundew.var(strugglers,0)% + 1
remote strugglers %sundew.id%
if %strugglers% < %times_needed%
  %purge% %self%
  halt
end
* check completion
set any 0
set ch %actor.room.people%
while %ch% && !%any%
  if %ch.affect(12626)%
    set any 1
  end
  set ch %ch.next_in_room%
done
if !%any%
  * no strugglers left
  %scale% %sundew% %actor.level%
  %slay% %sundew%
end
%purge% %self%
~
#12646
Strange Plant: Struggle safety check~
1 ab 100 2
L b 12626
L w 12626
~
* Ensures the 'struggle' handler does not stick around
set ch %self.carried_by%
if !%ch%
  * not carried
  %purge% %self%
elseif %ch.affect(12626)%
  * checks for sundew trap
  set room %ch.room%
  if !%room.people(12626)%
    * no sundew
    dg_affect #12626 %actor% off
    %purge% %self%
  end
else
  * not struggling
  %purge% %self%
end
~
#12647
Strange Plant: Stuff command to put items in the bog maw~
0 c 0 1
L c 12644
stuff give put~
* usage: stuff <object>
set requires_items 3
return 1
*
* For put/give, ensure it targets ME
if %cmd% == give || %cmd% == put
  if %actor.char_target(%arg.argument2%)% != %self%
    return 0
    halt
  end
end
*
if !%arg%
  %send% %actor% Stuff what in the bog maw?
  halt
end
*
set obj %actor.obj_target_inv(%arg.argument1%)%
if !%obj%
  %send% %actor% You don't seem to have %arg.argument1.ana% %arg.argument1%.
  halt
end
*
if %obj.is_flagged(*KEEP)%
  %send% %actor% You can't stuff @%obj% in there because it's set to 'keep'.
  halt
elseif %obj.quest%
  %send% %actor% You can't stuff @%obj% in there... you might need it.
  halt
end
*
%send% %actor% You stuff @%obj% into the bog maw!
%echoaround% %actor% ~%actor% stuffs @%obj% into the bog maw!
*
eval stuffed %self.var(stuffed,0)% + 1
if %obj.is_flagged(LARGE)%
  eval stuffed %stuffed% + 1
end
remote stuffed %self.id%
%purge% %obj%
* trap them if stuffing when untrapped
%load% obj 12644 %actor%
*
if %stuffed% >= %requires_items%
  * finished!
  %echo% ... there's an uncomfortable burbling noise...
  %scale% %self% %actor.level%
  %slay% %self%
end
~
#12648
Seedling pet rename part 1~
0 n 50 0
~
wait %random.10% s
set mode 1%random.8%
remote mode %self.id%
switch %mode%
  case 11
    %mod% %self% keywords seedling sproutkin tiny shoot
    %mod% %self% shortdesc a tiny sproutkin
    %mod% %self% longdesc There's a tiny sproutkin standing here, twitching its leaves.
    %mod% %self% lookdesc The sproutkin looks like a small green shoot with two stubby leaves and a root-ball base. It hops about energetically, waving its quivering leaves like little hands.
  break
  case 12
    %mod% %self% keywords seedling fernling lazy
    %mod% %self% shortdesc a lazy fernling
    %mod% %self% longdesc A fernling uncurls lazily on the ground.
    %mod% %self% lookdesc This plantling resembles a fresh fern frond, still coiled at the top, walking about on spindly rootlike tendrils. Its movements are slow and graceful, almost hypnotic.
  break
  case 13
    %mod% %self% keywords seedling petalbud bud trembling
    %mod% %self% shortdesc a trembling petalbud
    %mod% %self% longdesc A petalbud trembles softly as it bobs back and forth.
    %mod% %self% lookdesc The petalbud looks like a flower caught just before blooming, its petals still tightly closed. Now and then, it shivers, and the bud splits slightly to reveal a flash of brilliant color before snapping shut again.
  break
  case 14
    %mod% %self% keywords seedling acornling sccuttling
    %mod% %self% shortdesc a scuttling acornling
    %mod% %self% longdesc An acornling flaps its little leaves as it scuttles about.
    %mod% %self% lookdesc This stout creature is shaped like an acorn, with two sprouting leaves sticking out from its top. Its tiny root-feet tap quickly as it follows along, wobbling slightly with its round body.
  break
  case 15
    %mod% %self% keywords seedling mosskin forest lumpy
    %mod% %self% shortdesc a lumpy mosskin
    %mod% %self% longdesc There's a lump of forest moss here.
    %mod% %self% lookdesc The mosskin is a small, shaggy puff of damp greenery. Its surface glistens faintly, and when it hops, tiny bits of moss and spores flake off, leaving a faint trail of green fuzz behind it.
  break
  case 16
    %mod% %self% keywords seedling glowpod pod drifting
    %mod% %self% shortdesc a drifting glowpod
    %mod% %self% longdesc A glowpod casts off a soft light as it gently drifts by.
    %mod% %self% lookdesc This plantling looks like a dangling seedpod, glowing faintly from within. Thin vine tendrils suspend it in the air as it sways, glowing brighter as it moves.
  break
  case 17
    %mod% %self% keywords seedling sporeling toddling
    %mod% %self% shortdesc a toddling sporeling
    %mod% %self% longdesc A sporeling puffs out faint clouds as it toddles about.
    %mod% %self% lookdesc The sporeling is a little puffball fungus balanced on twitching root-legs. Each step causes a faint cloud of powdery spores to escape; the glow only faintly before dispersing.
  break
  case 18
    %mod% %self% keywords seedling thornling bristling spines
    %mod% %self% shortdesc a bristling thornling
    %mod% %self% longdesc A thornling bristles with tiny barbed spines, a little too close.
    %mod% %self% lookdesc This squat plantling is shaped like a spiky bush, its every surface bristling with sharp little thorns. It shifts from side to side in short, jerky motions, as though it's itching for someone to come too close.
  break
done
%echo% The little seedling grows into %self.name%!
~
#12649
Seedling pet rename part 2~
0 n 100 0
~
wait %random.10% s
set mode 2%random.7%
remote mode %self.id%
switch %mode%
  case 21
    %mod% %self% keywords seedling vinekin trailing tendrils
    %mod% %self% shortdesc a trailing vinekin
    %mod% %self% longdesc A vinekin trails its looping tendrils across the ground.
    %mod% %self% lookdesc The plantling looks like a living tangle of vines that knot and unknot themselves as it moves. Its tendrils occasionally curl upward curiously, like a plant stretching toward the sun.
  break
  case 22
    %mod% %self% keywords seedling pitcherkin squeaking
    %mod% %self% shortdesc a squeaking pitcherkin
    %mod% %self% longdesc A pitcherkin makes tiny squeaking noises as it waddles.
    %mod% %self% lookdesc The plantling is a miniature carnivorous pitcher plant with a rounded base and a wide lid that snaps shut whenever it gets startled. Its oversized eyes and squeaky chirps make it more cute than dangerous.
  break
  case 23
    %mod% %self% keywords seedling bogbloom bloom drooping petals
    %mod% %self% shortdesc a drooping bogbloom
    %mod% %self% longdesc A bogbloom droops here, dripping swamp water from its petals.
    %mod% %self% lookdesc The bogbloom looks like a heavy flower with soggy, half-rotted petals. It waddles on thick roots, trailing a faint puddle behind it, and its head lolls from side to side as though too waterlogged to hold up.
  break
  case 24
    %mod% %self% keywords seeding mandraglet rustling
    %mod% %self% shortdesc a rustling mandraglet
    %mod% %self% longdesc A mandraglet rustles faintly as it squirms.
    %mod% %self% lookdesc This tiny root-creature looks like a twisted mandrake with stubby arms and legs. Its leafy green hair fans out wildly, and now and then it lets out a soft, squeaky cry.
  break
  case 25
    %mod% %self% keywords seedling sapling wight glowing eyes
    %mod% %self% shortdesc a sapling wight
    %mod% %self% longdesc Glowing eyes peer out from the barklike skin of a sapling wight.
    %mod% %self% lookdesc This unsettling plantling looks like a small humanoid sprout, its skin made of rough bark with cracks that glow faintly from within. Its eyes glimmer like fireflies, watching every movement, waiting for the right one.
  break
  case 26
    %mod% %self% keywords seedling creeper pod shell
    %mod% %self% shortdesc a creeper pod
    %mod% %self% longdesc A creeper pod clicks its shell open and closed.
    %mod% %self% lookdesc This knobbly seedpod scuttles on root-legs, and its shell splits now and then to reveal a mess of writhing green tendrils inside. The tendrils curl and uncurl restlessly before snapping back inside.
  break
  case 27
    %mod% %self% keywords seedling mirefern fern sloshing
    %mod% %self% shortdesc a sloshing mirefern
    %mod% %self% longdesc A mirefern drips muddy water from its fronds as it sloshes about.
    %mod% %self% lookdesc The plantling looks like a shaggy bundle of swamp ferns bundled around a dripping rootball. It sloshes noisily as it waddles with its damp fronds clinging together in dripping clumps.
  break
done
%echo% The little seedling grows into %self.name%!
~
#12650
Mob block higher template id (Grove 2.0)~
0 s 100 2
L c 12667
L t 12650
~
* One quick trick to get the target room
eval room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if %actor.is_npc%
  halt
end
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
if %self.aff_flagged(STUNNED)% || %self.aff_flagged(HARD-STUNNED)% || %self.aff_flagged(!ATTACK)%
  halt
end
if %actor.aff_flagged(SNEAK)%
  halt
end
if %actor.on_quest(12650)%
  halt
end
set actor_clothes %actor.eq(clothes)%
if %actor_clothes%
  if %actor_clothes.vnum% == 12667
    halt
  end
end
%send% %actor% ~%self% bars your way.
return 0
~
#12651
Grove 2.0: Manaweaver death~
0 f 100 13
L b 12654
L b 12655
L b 12656
L b 12657
L b 12661
L b 12676
L b 12677
L c 12652
L c 12653
L j 12651
L j 12652
L t 12650
L w 12650
~
if %actor.on_quest(12650)%
  %quest% %actor% drop 12650
  %send% %actor% You fail the quest Tranquility of the Grove for killing a manaweaver.
  set fox %instance.mob(12676)%
  if %fox%
    %at% %fox.room% %load% mob 12677
    set new_mob %fox.room.people%
    %purge% %fox% $n vanishes, replaced with %new_mob.name%.
  end
end
if %self.vnum% < 12654 || %self.vnum% > 12657
  * Trash
  halt
end
* Tokens for everyone
set person %self.room.people%
while %person%
  if %person.is_pc%
    * You get a token, and you get a token, and YOU get a token!
    if %self.mob_flagged(HARD)%
      nop %person.give_currency(12650, 2)%
      %send% %person% As ~%self% dies, 2 %currency.12650(2)% fall to the ground!
      %send% %person% You take them.
    else
      nop %person.give_currency(12650, 1)%
      set curname %currency.12650(1)%
      %send% %person% As ~%self% dies, %curname.ana% %curname% falls to the ground!
      %send% %person% You take the newly created token.
    end
  end
  set person %person.next_in_room%
done
* Ensure instance is scaled
if %instance.level% < 1
  set level 1
  set person %self.room.people%
  while %person%
    if %person.is_pc%
      if %person.level% > %level%
        set level %person.level%
      end
    end
    set person %person.next_in_room%
  done
  %scale% instance %level%
end
* Check if difficulty was not selected and select it now
set oldroom %self.room%
mgoto i12651
set selector %self.room.contents(12652)%
set result %selector.result%
if %selector%
  %echo% @%selector% parts before you.
  set newroom i12652
  if !%result%
    * global var not found
    set result up
  end
  %door% %self.room% %result% room %newroom%
  %load% obj 12653
  %load% mob 12661
  %load% mob 12661
  %load% mob 12661
  %purge% %selector%
end
mgoto %oldroom%
~
#12652
Grove difficulty selector~
1 c 4 7
L b 12654
L b 12655
L b 12656
L b 12657
L b 12661
L c 12653
L j 12652
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty.
  return 1
  halt
end
if %instance.players_present% > %self.room.players_present%
  %send% %actor% You cannot set a difficulty while players are elsewhere in the adventure.
  return 1
  halt
end
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set difficulty 1
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set difficulty 2
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
set vnum 12654
while %vnum% <= 12657
  set mob %instance.mob(%vnum%)%
  if !%mob%
    * This was for debugging. We could do something about this.
    * Maybe just ignore it and keep on setting?
  else
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
  end
  eval vnum %vnum% + 1
done
set level 1
set person %self.room.people%
while %person%
  if %person.is_pc%
    if %person.level% > %level%
      set level %person.level%
    end
  end
  set person %person.next_in_room%
done
%scale% instance %level%
%send% %actor% @%self% parts before you.
%echoaround% %actor% @%self% parts before ~%actor%.
set newroom i12652
if !%result%
  * global var not found
  set result up
end
%door% %self.room% %result% room %newroom%
%load% obj 12653
%load% mob 12661
%load% mob 12661
%load% mob 12661
%purge% %self%
~
#12653
Grove delayed despawner~
1 f 0 0
~
%adventurecomplete%
~
#12654
Magiterranean Grove environment~
2 bw 10 0
~
switch %random.4%
  case 1
    %echo% You hear the howl of nearby animals.
  break
  case 2
    %echo% A treebranch brushes your back.
  break
  case 3
    %echo% You hear strange chanting through the trees.
  break
  case 4
    %echo% Your footprints seem to vanish behind you.
  break
done
~
#12655
Difficulty selector load~
1 n 100 1
L j 12652
~
set tofind 12652
set room %self.room%
set north %room.north(room)%
set east %room.east(room)%
set west %room.west(room)%
set south %room.south(room)%
set northeast %room.northeast(room)%
set northwest %room.northwest(room)%
set southeast %room.southeast(room)%
set southwest %room.southwest(room)%
if %north%
  if %north.template% >= %tofind%
    set result north
  end
end
if %east%
  if %east.template% >= %tofind%
    set result east
  end
end
if %west%
  if %west.template% >= %tofind%
    set result west
  end
end
if %northeast%
  if %northeast.template% >= %tofind%
    set result northeast
  end
end
if %northwest%
  if %northwest.template% >= %tofind%
    set result northwest
  end
end
if %southeast%
  if %southeast.template% >= %tofind%
    set result southeast
  end
end
if %southwest%
  if %southwest.template% >= %tofind%
    set result southwest
  end
end
if %south%
  if %south.template% >= %tofind%
    set result south
  end
end
if %result%
  %door% %room% %result% purge
  global result
end
~
#12656
Wildling Ambusher reveal~
0 gi 100 7
L b 12658
L b 12659
L c 12667
L o 20
L t 12650
L w 12657
L w 12658
~
if %actor%
  * Actor entered room - valid target?
  if (%actor.is_pc% && !%actor.nohassle%)
    set target %actor%
  end
else
  * entry - look for valid target in room
  wait 1
  set person %room.people%
  while %person%
    * validate
    if (%person.is_pc% && !%person.nohassle%)
      set target_clothes %person.eq(clothes)%
      if %target_clothes%
        eval clothes_valid (%target_clothes.vnum% == 12667)
      end
      if %target.on_quest(12650)% || %target.ability(Hide)% || %clothes_valid%
      else
        set target %person%
      end
    end
    set person %person.next_in_room%
  done
end
if !%target%
  halt
end
set clothes_valid 0
set target_clothes %target.eq(clothes)%
if %target_clothes%
  eval clothes_valid (%target_clothes.vnum% == 12667)
end
if %target.on_quest(12650)% || %target.ability(Hide)% || %clothes_valid%
  visible
  wait 2
  switch %self.vnum%
    case 12659
      %send% %target% You spot ~%self% swimming around nearby...
      %echoaround% %target% You spot ~%self% swimming around nearby...
    break
    case 12658
      %send% %target% You spot ~%self% clinging to the ceiling of the tunnel...
      %echoaround% %target% You spot ~%self% clinging to the ceiling of the tunnel...
    break
    default
      %send% %target% You spot ~%self% in the branches of a tree nearby...
      %echoaround% %target% You spot ~%self% in the branches of a tree nearby...
    break
  done
  * %echo% ~%self% does not react to your presence.
  halt
end
visible
wait 2
if %target.room% != %self.room% || %self.disabled% || %self.fighting%
  halt
end
switch %self.vnum%
  case 12659
    %send% %target% You spot ~%self% swimming around above you...
    %echoaround% %target% You spot ~%self% swimming around above ~%target%...
  break
  case 12658
    %send% %target% You spot ~%self% clinging to the ceiling above you...
    %echoaround% %target% You spot ~%self% clinging to the ceiling above ~%target%...
  break
  default
    %send% %target% You spot ~%self% in the branches of a tree above you...
    %echoaround% %target% You spot ~%self% in the branches of a tree above ~%target%...
  break
done
wait 3 sec
* cancel here on no-aggro islands
if %self.room.island_flagged(!AGGRO)%
  halt
end
if %target.room% != %self.room% || %self.disabled% || %self.fighting%
  halt
end
switch %self.vnum%
  case 12659
    %send% %target% ~%self% swims down and attacks you!
    %echoaround% %target% ~%self% swims down and attacks ~%target%!
  break
  case 12658
    %echoaround% %target% ~%self% drops from the ceiling and attacks ~%target%!
    %send% %target% ~%self% drops from the ceiling and attacks you!
  break
  default
    %echoaround% %target% ~%self% leaps down and attacks ~%target%!
    %send% %target% ~%self% leaps down and attacks you!
  break
done
%aggro% %target%
~
#12657
Grove underground environment~
2 bw 10 0
~
switch %random.4%
  case 1
    %echo% There's a soft scratching sound just behind the tunnel wall.
  break
  case 2
    %echo% Something skitters past your ankle.
  break
  case 3
    %echo% A strange chant echoes through the tunnel.
  break
  case 4
    %echo% A root slithers slowly across the floor.
  break
done
~
#12658
Wildling combat: Nasty Bite~
0 k 100 2
L w 12657
L w 12658
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
* Nasty bite: low damage over time
%send% %actor% ~%self% snaps ^%self% teeth and takes off a piece of your skin!
%echoaround% %actor% ~%self% snaps ^%self% teeth at ~%actor% and takes off a piece of ^%actor% skin!
%damage% %actor% 50 physical
%dot% #12658 %actor% 100 30 physical
~
#12659
Faun Shifter 2.0: Rejuvenate~
0 k 100 2
L w 12657
L w 12659
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
%echo% ~%self% glows faintly with magical vitality, and ^%self% wounds start to close.
eval amount (%level%/10) + 1
dg_affect #12659 %self% HEAL-OVER-TIME %amount% 30
~
#12660
Grove Manaweaver 2.0: Firebolt~
0 k 100 3
L w 12657
L w 12660
L w 12670
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
wait 1 sec
dg_affect #12670 %self% HARD-STUNNED on 5
if %actor.trigger_counterspell(%self%)%
  %send% %actor% ~%self% launches a bolt of fire at you, but your counterspell blocks it completely.
  %echoaround% %actor% ~%self% launches a bolt of fire at ~%actor%, but it fizzles out in the air in front of *%actor%.
  halt
else
  %send% %actor% &&r~%self% thrusts out ^%self% hand at you, and blasts you with a bolt of searing flames!
  %echoaround% %actor% ~%self% thrusts out ^%self% hand at ~%actor%, and blasts *%actor% with a bolt of searing flames!
  %damage% %actor% 100 fire
  %dot% #12660 %actor% 150 15 fire
end
~
#12661
Escaped wildling load~
0 n 100 0
~
if %instance.location%
  mgoto %instance.location%
end
mmove
mmove
mmove
~
#12662
Squirrel Shifter: Morph/Nibble~
0 k 100 3
L s 12662
L w 12657
L w 12661
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
wait 1
set old_name %self.name%
if !%self.morph%
  %morph% %self% 12662
  %send% %actor% %old_name%'s robe drops to the ground as you try to fight *%self%, and ~%self% scrambles out, attacking you!
  %echoaround% %actor% %old_name%'s robe drops to the ground as ~%actor% tries to fight *%self%, and ~%self% scrambles out, attacking *%actor%!
else
  %send% %actor% ~%self% nips at your ankles, drawing blood!
  %echoaround% %actor% ~%self% nips at |%actor% ankles, drawing blood!
  %damage% %actor% 25 physical
  %dot% #12661 %actor% 25 10 physical
end
~
#12663
Badger Shifter: Morph~
0 k 50 2
L s 12663
L w 12657
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
if !%self.morph%
  set current %self.name%
  %morph% %self% 12663
  %echo% %current% rapidly morphs into ~%self%!
end
wait 1 sec
%send% %actor% &&r~%self% sinks ^%self% teeth into your leg!
%echoaround% %actor% ~%self% sinks ^%self% teeth into |%actor% leg!
%damage% %actor% 100 physical
~
#12664
Badger Shifter: Earthen Claws~
0 k 100 3
L w 12657
L w 12664
L w 12670
~
if %self.cooldown(12657)%
  halt
end
set actor_id %actor.id%
nop %self.set_cooldown(12657, 30)%
dg_affect #12670 %self% HARD-STUNNED on 5
if %self.morph%
  set current %self.name%
  %morph% %self% normal
  %echo% %current% rapidly morphs into ~%self%!
end
wait 1 sec
if %actor.id% != %actor_id%
  * gone?
  halt
elseif %actor.trigger_counterspell(%self%)%
  %send% %actor% ~%self% gestures, and earthen claws burst from the soil, dissolving as they meet your counterspell!
  %echoaround% %actor% ~%self% gestures, and earthen claws burst from the soil, dissolving as they near ~%actor%!
  halt
else
  %send% %actor% &&r~%self% gestures, and earthen claws burst from the soil, tearing into you!
  %echoaround% %actor% &&r~%self% gestures, and earthen claws burst from the soil, tearing into ~%actor%!
  %damage% %actor% 50 physical
  eval scale %self.level%/15 + 1
  dg_affect #12664 %actor% RESIST-PHYSICAL -%scale% 15
end
~
#12665
Archweaver: Grand Fireball~
0 k 50 3
L w 12657
L w 12665
L w 12666
~
if %self.affect(12666)%
  %send% %actor% &&rThe flames wreathing |%self% staff burn you as &%self% swings at you!
  %echoaround% %actor% The flames wreathing |%self% staff burn ~%actor% as ~%self% swings at *%actor%!
  %damage% %actor% 50 fire
end
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
wait 1 sec
dg_affect #12670 %self% HARD-STUNNED on 20
%echo% ~%self% stakes ^%self% staff into the ground and begins chanting...
wait 5 sec
%echo% The bonfire flickers and dims as a glowing light builds in |%self% cupped hands...
wait 5 sec
%echo% ~%self% holds ^%self% hands skyward, and the growing radiance of the large fireball held in ^%self% hands eclipses the dimming bonfire...
wait 5 sec
set actor %self.fighting%
if !%actor%
  %echo% ~%self% discharges the fireball in a blinding flare of light!
  halt
end
%send% %actor% ~%self% hurls the grand fireball at you!
%echoaround% %actor% ~%self% hurls the grand fireball at ~%actor%!
if %actor.trigger_counterspell(%self%)%
  %send% %actor% The fireball strikes your counterspell and explodes with a fiery roar!
  %echoaround% %actor% The fireball explodes in front of ~%actor% with a fiery roar!
else
  %send% %actor% &&rThe fireball crashes into you and explodes, bowling you over!
  %echoaround% %actor% The fireball crashes into ~%actor% and explodes, bowling *%actor% over!
  dg_affect #12665 %actor% STUNNED on 5
  %damage% %actor% 300 fire
end
%echo% &&rA wave of scorching heat from the explosion washes over you!
%aoe% 75 fire
~
#12666
Archweaver: Ignite Weapon~
0 k 100 2
L w 12657
L w 12666
~
if %self.affect(12666)%
  %send% %actor% &&rThe flames wreathing |%self% staff burn you as &%self% swings at you!
  %echoaround% %actor% The flames wreathing |%self% staff burn ~%actor% as ~%self% swings at *%actor%!
  %damage% %actor% 50 fire
end
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
wait 1 sec
%echo% ~%self% thrusts ^%self% staff skyward!
dg_affect #12666 %self% SLOW on 15
~
#12667
Crow Shifter: Morph~
0 k 50 2
L w 12657
L w 12667
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
* storing ids prevents an error after the wait
set id %actor.id%
if !%self.morph%
  set current %self.name%
  %morph% %self% 12667
  %echo% %current% rapidly morphs into ~%self% and takes flight!
  wait 1 sec
end
if %actor.id% == %id%
  %send% %actor% &&r~%self% swoops down and knocks your weapon from your hand!
  %echoaround% %actor% ~%self% swoops down and knocks |%actor% weapon from ^%actor% hand!
  %damage% %actor% 5 physical
  dg_affect #12667 %actor% DISARMED on 5
end
~
#12668
Crow Shifter: Squall~
0 k 100 3
L w 12657
L w 12663
L w 12670
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
dg_affect #12670 %self% HARD-STUNNED on 5
if %self.morph%
  set current %self.name%
  %morph% %self% normal
  %echo% %current% lands and rapidly morphs into ~%self%!
  wait 1 sec
end
%echo% ~%self% gestures, and a sudden squall of wind knocks you off-balance!
set person %self.room.people%
while %person%
  if %person.is_enemy(%self%)%
    dg_affect #12663 %actor% TO-HIT -10 15
  end
  set person %person.next_in_room%
done
~
#12669
Turtle Shifter: Morph~
0 k 50 3
L s 12666
L w 12657
L w 12669
~
if %self.aff_flagged(IMMUNE-DAMAGE)% && %random.4% == 4
  %echo% ~%self% has retreated into ^%self% shell.
  %echo% &&YYou could 'stomp' on ^%self% shell to draw *%self% out.
  halt
end
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
if !%self.morph%
  set current %self.name%
  %morph% %self% 12666
  %echo% %current% rapidly morphs into ~%self%!
  wait 1 sec
end
%echo% ~%self% retreats into the safety of ^%self% hard shell.
%echo% &&YYou could 'stomp' on ^%self% shell to draw *%self% out.
dg_affect #12669 %self% IMMUNE-DAMAGE on -1
nop %self.add_mob_flag(NO-ATTACK)%
~
#12670
Turtle Shifter: Riptide~
0 k 100 3
L w 12657
L w 12669
L w 12670
~
if %self.cooldown(12657)%
  if %self.affect(12669) && %random.4% == 4
    %echo% ~%self% has retreated into ^%self% shell.
    %echo% &&YYou could 'stomp' on ^%self% shell to draw *%self% out.
    halt
  end
  halt
end
nop %self.set_cooldown(12657, 30)%
dg_affect #12670 %self% HARD-STUNNED on 5
if %self.morph%
  set current %self.name%
  %morph% %self% normal
  if %self.affect(12669)%
    %echo% ~%self% emerges from ^%self% shell.
    nop %self.remove_mob_flag(NO-ATTACK)%
    dg_affect #12669 %self% off
  end
  %echo% %current% rapidly morphs into ~%self%!
  wait 1 sec
end
%echo% ~%self% moves ^%self% arms in sweeping motions, stirring up the water into a riptide!
set cycle 1
while %cycle% <= 3
  wait 5 sec
  %echo% &&r|%self% riptide pummels you violently!
  %aoe% 40 physical
  eval cycle %cycle% + 1
done
%echo% |%self% riptide dissipates.
~
#12671
Stomp turtle shifter~
0 c 0 2
L w 12669
L w 12671
stomp~
if !%self.affect(12669)%
  %send% %actor% You don't need to do that right now.
  halt
end
%send% %actor% You stomp down on |%self% shell, and &%self% emerges, dazed.
%echoaround% %actor% ~%actor% stomps down on |%self% shell, and &%self% emerges, dazed.
dg_affect #12669 %self% off
nop %self.remove_mob_flag(NO-ATTACK)%
dg_affect #12671 %self% HARD-STUNNED on 5
~
#12672
Grove Start Progression~
2 g 100 1
L y 12650
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12650)%
end
*
* also clear breath variable if needed
if %actor.varexists(breath)%
  rdelete breath %actor.id%
end
~
#12673
Grove 2.0: Tranquility Chant~
1 c 2 23
L b 12650
L b 12651
L b 12652
L b 12653
L b 12654
L b 12655
L b 12656
L b 12657
L b 12658
L b 12659
L b 12660
L b 12661
L b 12663
L b 12686
L c 12674
L j 12650
L j 12651
L j 12652
L j 12664
L j 12665
L t 12650
L w 12650
L w 12673
chant~
if (!(tranquility /= %arg%) || %actor.position% != Standing)
  return 0
  halt
end
set room %actor.room%
set cycles_left 5
while %cycles_left% >= 0
  eval sector_valid (%room.template% >= 12652 && %room.template% <= 12699)
  set mob %room.people%
  while %mob%
    if %mob.vnum% == 12686
      set rage_spirit_here 1
    end
    set mob %mob.next_in_room%
  done
  set object %room.contents%
  while %object% && !%already_done%
    if %object.vnum% == 12674
      set already_done 1
    end
    set object %object.next_in_list%
  done
  if (%actor.room% != %room%) || !%sector_valid% || %already_done% || %rage_spirit_here% || !%actor.can_act%
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 5
      %echoaround% %actor% |%actor% chant is interrupted.
      %send% %actor% Your chant is interrupted.
    elseif %rage_spirit_here%
      %send% %actor% You can't perform the chant while the spirit of rage is here!
    elseif !%sector_valid%
      if %room.template% == 12650 || %room.template% == 12651
        %send% %actor% You must venture deeper into the Grove before performing the chant.
      else
        %send% %actor% You must perform the chant inside the Magiterranean Grove.
      end
    elseif %already_done%
      %send% %actor% The tranquility chant has already been performed here.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% ~%actor% closes ^%actor% eyes and starts a peaceful chant...
      %send% %actor% You clutch the totem of tranquility and begin to chant...
    break
    case 4
      %echoaround% %actor% ~%actor% sways as &%actor% whispers strange words...
      %send% %actor% You sway as you whisper the words of the tranquil chant...
    break
    case 3
      %echoaround% %actor% |%actor% totem of tranquility takes on a soft white glow, and the area around it seems to cool...
      %send% %actor% Your totem of tranquility takes on a soft white glow, and the area around it seems to cool...
    break
    case 2
      %echoaround% %actor% A peaceful feeling fills the area...
      %send% %actor% A peaceful feeling fills the area...
    break
    case 1
      if %room.template% == 12664 || %room.template% == 12665
        %echoaround% %actor% ~%actor% burble into the water, and the feeling of tranquility spreads...
        %send% %actor% You burble into the water, and the feeling of tranquility spreads...
      else
        %echoaround% %actor% ~%actor% whispers into the air, and the feeling of tranquility spreads...
        %send% %actor% You whisper into the air, and the feeling of tranquility spreads...
      end
    break
    case 0
      if %random.2% == 2
        %echoaround% %actor% |%actor% chant is interrupted as a spirit of rage materializes in front of *%actor%!
        %send% %actor% Your chant is interrupted as a spirit of rage materializes in front of you!
        %load% mob 12686 %instance.level%
        set spirit %self.room.people%
        set word_1 calm
        set word_2 relax
        set word_3 pacify
        set word_4 slumber
        eval correct_word %%word_%random.4%%%
        remote correct_word %spirit.id%
        %echo% A voice in your head whispers, 'Tell it to %correct_word%!'
        halt
      end
      %echoaround% %actor% ~%actor% completes ^%actor% chant, and the area is tranquilized!
      %send% %actor% You complete your chant, and the area is tranquilized!
      %load% obj 12674 %room%
      set person %room.people%
      while %person%
        set next_person %person.next_in_room%
        if %person.vnum% == 12660 || %person.vnum% == 12659 || %person.vnum% == 12658 || %person.vnum% == 12661
          %purge% %person% $n slinks away.
        elseif %person.vnum% >= 12650 && %person.vnum% <= 12657 || %person.vnum% == 12663
          dg_affect #12673 %person% !ATTACK on -1
          %echo% ~%person% looks pacified.
          if %person.vnum% >= 12654 && %person.vnum% <= 12657
            set give_token 1
          end
        end
        set person %next_person%
      done
      set done 1
      set vnum 12654
      while %vnum% <= 12657
        set mage %instance.mob(%vnum%)%
        if !%mage%
          set done 0
        else
          if !%mage.affect(12673)%
            set done 0
          end
        end
        eval vnum %vnum%+1
      done
      set person %self.room.people%
      while %person%
        if %person.is_pc% && %person.on_quest(12650)%
          if %give_token%
            %quest% %person% trigger 12650
            set curname %currency.12650(1)%.
            %send% %person% You receive %curname.ana% %curname%.
            nop %person.give_currency(12650, 1)%
            if %person.quest_finished(12650)%
              %send% %person% You have tranquilized all four of the manaweaver leaders.
            end
          end
        end
        set person %person.next_in_room%
      done
      halt
    break
  done
  * Shortcut for immortals
  if !%actor.nohassle%
    wait 5 sec
  end
  eval cycles_left %cycles_left% - 1
done
~
#12674
Underwater (Grove 2.0)~
2 bgw 100 1
L c 12675
~
if !%actor%
  * Random
  if %random.10% == 10
    switch %random.4%
      case 1
        %echo% # Something brushes against your leg.
      break
      case 2
        %echo% # You hear a wild screech burble through the water.
      break
      case 3
        %echo% # Your lungs feel like they might collapse.
      break
      case 4
        %echo% # You try to swim toward the surface, but something drags you down again.
      break
    done
  end
  set person %room.people%
  while %person%
    if %person.is_pc%
      if %person.varexists(breath)%
        if %person.breath% < 0
          if %person.is_god% || %person.is_immortal% || %person.health% < 0
            halt
          end
          %send% %person% # &&rYou are drowning!&&0
          eval amount (%person.breath%) * (-250)
          %damage% %person% %amount%
        end
      end
    end
    set person %person.next_in_room%
  done
  halt
end
if %actor.is_npc%
  halt
end
if !%actor.varexists(breath)%
  set breath 3
  remote breath %actor.id%
end
set breath %actor.breath%
eval breath %breath% - 1
if %breath% < 0
  eval amount (%breath%) * (-250)
  %damage% %actor% %amount% direct
end
remote breath %actor.id%
%load% obj 12675 %actor% inv
~
#12675
Breath Messaging (Grove 2.0 underwater)~
1 n 100 0
~
set actor %self.carried_by%
wait 1
if %actor.varexists(breath)%
  set breath %actor.breath%
  if %breath% == 0
    %send% %actor% You are about to drown!
  elseif %breath% == 1
    %send% %actor% You cannot hold your breath much longer! You should find air, and soon!
  elseif %breath% < 0
    %send% %actor% &&rYou are drowning!
  elseif %breath% <= 5
    %send% %actor% You can't hold your breath much longer... You think you could swim for another %breath% rooms.
  end
end
%purge% %self%
~
#12676
Air Supply (Grove 2.0)~
2 g 100 0
~
* change based on quest, etc
set breath 5
if %actor.varexists(breath)%
  if %actor.breath% < %breath%
    %send% %actor% # You take a deep breath of air, refreshing your air supply.
  end
end
remote breath %actor.id%
~
#12677
Apply snake oil to grove gear~
1 c 2 9
L c 12657
L c 12658
L c 12659
L c 12660
L c 12661
L c 12662
L c 12663
L c 12664
L c 12665
oil~
if !%arg%
  %send% %actor% Apply @%self% to what?
  halt
end
set target %actor.obj_target_inv(%arg%)%
if !%target%
  %send% %actor% You don't seem to have %arg.ana% '%arg%'. (You can only apply @%self% to items in your inventory.)
  halt
end
if %target.vnum% < 12657 || %target.vnum% > 12665
  %send% %actor% You can only apply @%self% to equipment from the Magiterranean: The Grove shop.
  halt
end
if %target.is_flagged(SUPERIOR)%
  %send% %actor% @%target% is already superior; applying @%self% would have no benefit.
  halt
end
if %actor.level% < 50
  %send% %actor% You must be at least level 50 to use @%self%.
  halt
end
%send% %actor% You apply @%self% to @%target%...
%echoaround% %actor% ~%actor% applies @%self% to @%target%...
%echo% As the oil soaks in, @%target% takes on a faint glow and earthy smell... and looks a LOT more powerful.
nop %target.flag(SUPERIOR)%
%scale% %target% 75
%purge% %self%
~
#12678
Seeker Stone: Grove~
1 c 2 3
L j 12650
L o 80
L w 12678
seek~
if !%arg%
  %send% %actor% Seek what?
  halt
end
set room %self.room%
if %room.rmt_flagged(!LOCATION)%
  %send% %actor% @%self% spins gently in a circle.
  halt
end
if !(grove /= %arg%)
  return 0
  halt
else
  if %actor.cooldown(12678)%
    %send% %actor% @%self% is on cooldown.
    halt
  end
  eval adv %instance.nearest_adventure(12650)%
  if !%adv%
    %send% %actor% Could not find a Grove instance.
    halt
  end
  nop %actor.set_cooldown(12678, 1800)%
  %send% %actor% You hold @%self% aloft...
  eval real_dir %%room.direction(%adv%)%%
  eval direction %%actor.dir(%real_dir%)%%
  eval distance %%room.distance(%adv%)%%
  if %distance% == 0
    %send% %actor% There is a Magiterranean Grove right here.
    halt
  elseif %distance% == 1
    set plural tile
  else
    set plural tiles
  end
  if %actor.ability(Navigation)%
    %send% %actor% There is a Magiterranean Grove at %adv.coords%, %distance% %plural% to the %direction%.
  else
    %send% %actor% There is a Magiterranean Grove %distance% %plural% to the %direction%.
  end
  %echoaround% %actor% ~%actor% holds @%self% aloft...
end
~
#12679
Manaweaver spawner~
1 n 100 2
L b 12651
L b 12652
~
if %random.2% == 2
  %load% mob 12651
else
  %load% mob 12652
end
%purge% %self%
~
#12680
Grove track~
2 c 0 2
L o 73
L o 80
track~
if (!%actor.ability(Track)% || !%actor.ability(Navigation)%)
  * Fail through to ability message
  return 0
  halt
end
if !%arg%
  * has its own message which it's funnier to not interfere with
  return 0
  halt
end
if badger /= %arg%
  set target badger
elseif manaweaver /= %arg% || weaver /= %arg% || grove /= %arg% || shifter /= %arg%
  set target manaweaver
elseif wildling /= %arg% || ambusher /= %arg%
  set target wildling
elseif archweaver /= %arg%
  set target archweaver
elseif crow /= %arg%
  set target crow
elseif turtle /= %arg%
  set target turtle
elseif gopher /= %arg%
  set target gopher
else
  %send% %actor% You can't seem to find a trail.
  halt
end
eval already_done %%track_%target%%%
if %already_done%
  set result %already_done%
else
  set direction_1 northwest
  set direction_2 north
  set direction_3 northeast
  set direction_4 east
  set direction_5 southeast
  set direction_6 south
  set direction_7 southwest
  set direction_8 west
  eval result %%direction_%random.8%%%
  set track_%target% %result%
  global track_%target%
end
%send% %actor% You find a trail to the %result%!
~
#12681
Grove 2.0 Quest Finish completes adventure~
2 v 0 0
~
%adventurecomplete%
~
#12683
Grove Manaweaver 2.0 Underwater: Water Blast~
0 k 100 2
L w 12657
L w 12670
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
dg_affect #12670 %self% HARD-STUNNED on 5
%send% %actor% ~%self% flicks ^%self% wrist and launches a blast of pressurized water at you!
%echoaround% %actor% ~%self% flicks ^%self% wrist and launches a blast of pressurized water at ~%actor%!
if %actor.trigger_counterspell(%self%)%
  %send% %actor% The water blast hits your counterspell and dissipates.
  %echoaround% %actor% The water blast dissipates in front of ~%actor%.
  halt
else
  %send% %actor% &&rThe water blast crashes into you, sending you spinning!
  %echoaround% %actor% The water blast crashes into ~%actor%, sending *%actor% spinning!
  %damage% %actor% 150 physical
end
~
#12685
Give Grove 2.0 chant item~
2 u 0 2
L c 12673
L t 12650
~
if %questvnum% == 12650
  %load% obj 12673 %actor% inv
end
~
#12686
Grove rage spirit speech~
0 d 1 13
L b 12650
L b 12654
L b 12655
L b 12656
L b 12657
L b 12658
L b 12659
L b 12660
L b 12661
L b 12663
L t 12650
L w 12650
L w 12673
*~
if %actor.is_npc% && %actor.linked_to_instance%
  halt
end
set word_1 calm
set word_2 relax
set word_3 pacify
set word_4 slumber
set iterator 1
while %iterator% <= 4
  eval current_word %%word_%iterator%%%
  if %current_word% == %correct_word%
    eval success %speech% ~= %current_word%
  elseif %speech% ~= %current_word%
    set failure 1
  end
  eval iterator %iterator% + 1
done
if %success% && !%failure%
  %echo% ~%self% begins to calm, and fades from view.
  %echo% The area is tranquilized!
  set room %self.room%
  %load% obj 12674 %room%
  set person %room.people%
  while %person%
    set next_person %person.next_in_room%
    if %person.vnum% == 12660 || %person.vnum% == 12659 || %person.vnum% == 12658 || %person.vnum% == 12661
      %purge% %person% $n slinks away.
    elseif %person.vnum% >= 12650 && %person.vnum% <= 12657 || %person.vnum% == 12663
      dg_affect #12673 %person% !ATTACK on -1
      %echo% ~%person% looks pacified.
      if %person.vnum% >= 12654 && %person.vnum% <= 12657
        set give_token 1
      end
    end
    set person %next_person%
  done
  set done 1
  set vnum 12654
  while %vnum% <= 12657
    set mage %instance.mob(%vnum%)%
    if !%mage%
      set done 0
    else
      if !%mage.affect(12673)%
        set done 0
      end
    end
    eval vnum %vnum%+1
  done
  set person %self.room.people%
  while %person%
    if %person.is_pc% && %person.on_quest(12650)%
      if %give_token%
        %quest% %person% trigger 12650
        set curname %currency.12650(1)%
        %send% %person% You receive %curname.ana% %curname%.
        nop %person.give_currency(12650, 1)%
        if %person.quest_finished(12650)%
          %send% %person% You have tranquilized all four of the manaweaver leaders.
        end
      end
    end
    set person %person.next_in_room%
  done
  %purge% %self%
else
  %send% %actor% &&r~%self% attacks you with a painful bolt of crimson light, and flies off into the trees!
  %echoaround% %actor% ~%self% attacks ~%actor% with a bolt of crimson light, and flies off into the trees!
  %damage% %actor% 200 magical
  %purge% %self%
end
~
#12687
Grove rage spirit time limit~
0 bnw 100 0
~
wait 6 sec
%echo% A voice in your head urges, 'Say it out loud!'
wait 8 sec
%echo% &&r~%self% releases a painful surge of crimson energy and flies off into the trees, looking furious!
%aoe% 150 magical
%purge% %self%
~
$
