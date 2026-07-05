#12900
Echo Forge: Echo of the serragon death trigger~
0 f 100 2
L b 12897
L w 12898
~
set mommy %self.room.people(12897)%
if %mommy%
  dg_affect #12898 @%self% %mommy% off
end
return 0
~
#12911
Echo Forge: Resonant peal minipet~
0 int 100 7
L j 12890
L j 12891
L j 12892
L j 12893
L j 12894
L j 12895
L s 12911
~
if %self.room.template% >= 12890 && %self.room.template% <= 12895
  * activate
  if %self.morph% != 12911
    %morph% %self% 12911
  end
else
  * deactivate
  if %self.morph% == 12911
    %morph% %self% normal
  end
end
~
#12917
Echo Forge: Entry helper~
1 n 100 9
L j 12890
L j 12891
L j 12892
L j 12893
L j 12894
L j 12895
L j 12897
L j 12898
L j 12899
~
set echo_forge 12890 12891 12892 12893 12894 12895
set echo_arena 12897 12898 12899
*
wait 2
set actor %self.carried_by%
*
if !%actor%
  * oops
elseif %echo_forge% ~= %actor.room.template%
  if !%actor.aff_flagged(FLYING)%
    switch %random.4%
      case 1
        %echoaround% %actor% Light dances along the ground as ~%actor% walks.
      break
      case 2
        %echoaround% %actor% Light sparkles in |%actor% footsteps as &%actor% walks in.
      break
      case 3
        %echoaround% %actor% ~%actor% leaves glowing footsteps as &%actor% enters.
      break
      case 4
        %echoaround% %actor% There's a flash from |%actor% feet as &%actor% stops.
      break
    done
  end
elseif %echo_arena% ~= %actor.room.template%
  %send% %actor% You hear the thudding whoosh of your own heartbeat!
end
%purge% %self%
~
#12919
Celestial Forge: Terminus movement helper obj~
1 n 100 0
~
* Causes the player to move after a brief wait
* requires this object has a 'direction' variable with a true direction
* (not the direction for the player)
wait 0
set actor %self.carried_by%
set direction %self.var(direction)%
if %actor% && %direction%
  if %actor.position% == Standing && !%actor.disabled% && !%actor.fighting% && !%actor.aff_flagged(IMMOBILIZED)%
    %force% %actor% %actor.dir(%direction%)%
  end
end
%purge% %self%
~
#12920
Celestial Forge: Terminus portal movement replacer~
2 q 100 9
L c 9680
L c 12919
L j 12920
L j 12921
L j 12922
L j 12923
L j 12924
L j 12925
L j 12926
~
* setup
if %method% != move
  * ignore
  return 1
  halt
elseif %room.template% == 12920
  * atop a mighty rock, leaping down into the crater
  set mode 1
else
  * archway portal
  set mode 2
end
* determine to-room
eval dest %%room.%direction%(room)%%
if !%dest%
  * error
  return 1
  halt
elseif %dest.template% == 12925 || %dest.template% == 12926
  * allowed to walk
  return 1
  halt
end
* out-message
if %mode% == 1
  %send% %actor% You time it just right and leap off the rock... it's a long way down!
  %echoaround% %actor% ~%actor% peers over the edge and then leaps off the rock...
elseif !%actor.aff_flagged(SNEAK)%
  %send% %actor% You step into the %actor.dir(%direction%)% archway...
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch% != %actor% && !%ch.disabled%
      if %ch.position% != Sleeping && %ch.can_see(%actor%)%
        if %actor.is_npc%
          %send% %ch% ~%actor% %actor.movetype% into the %ch.dir(%direction%)% archway and vanishes!
        else
          %send% %ch% ~%actor% steps into the %ch.dir(%direction%)% archway and vanishes!
        end
      end
    end
    set ch %next_ch%
  done
end
* teleport
%teleport% %actor% %dest%
%load% obj 9680 %actor% inv
* in-message
if %mode% == 1
  %at% %dest% %echoaround% %actor% ~%actor% comes screaming in from above but lands on ^%actor% feet!
elseif !%actor.aff_flagged(SNEAK)%
  set ch %dest.people%
  set revdir %_map.reverse(%direction%)%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch% != %actor% && !%ch.disabled%
      if %ch.position% != Sleeping && %ch.can_see(%actor%)%
        %send% %ch% ~%actor% appears from the %ch.dir(%revdir%)% archway!
      end
    end
    set ch %next_ch%
  done
end
* fellows: load the delayed-move helper obj
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.leader% == %actor%
    %load% obj 12919 %ch% inv
    set obj %ch.inventory(12919)%
    if %obj%
      * store required variable
      remote direction %obj.id%
    end
  end
  set ch %next_ch%
done
return 0
~
#12921
Terminus Forge: Room commands for flavor~
2 c 0 1
L j 12920
enter jump leap~
* default to 0, return 1 if a command is intercepted
return 0
* behavior depends on room
if %room.template% == 12920
  if leap /= %cmd% || jump /= %cmd%
    %force% %actor% down
    return 1
  end
elseif %room.template% == 12921
  if enter /= %cmd%
    if archway /= %arg%
      %send% %actor% Enter which archway?
      return 1
    elseif %actor.parse_dir(%arg.argument1%)% == west
      %force% %actor% %actor.dir(west)%
      return 1
    elseif %actor.parse_dir(%arg.argument1%)% == south
      %force% %actor% %actor.dir(south)%
      return 1
    elseif %actor.parse_dir(%arg.argument1%)% == east
      %force% %actor% %actor.dir(east)%
      return 1
    end
  end
elseif %room.template% == 12922
  if enter /= %cmd%
    if archway /= %arg% || %actor.parse_dir(%arg.argument1%)% == east
      %force% %actor% %actor.dir(east)%
      return 1
    end
  end
elseif %room.template% == 12923
  if enter /= %cmd%
    if archway /= %arg% || %actor.parse_dir(%arg.argument1%)% == west
      %force% %actor% %actor.dir(west)%
      return 1
    end
  end
elseif %room.template% == 12924
  if enter /= %cmd%
    if archway /= %arg% || %actor.parse_dir(%arg.argument1%)% == south
      %force% %actor% %actor.dir(south)%
      return 1
    end
  end
end
~
#12922
Terminus Forge: Impending doom ticker~
0 b 50 0
~
* config: seconds between meteors
set interval 1200
* vars
set meteor %self.var(meteor,1)%
set timer %self.var(timer,%timestamp%)%
* skip 60 seconds of random checks
if (%timestamp% - %timer%) < %interval%
  * progress should be 0 to 20
  eval progress (%timestamp% - %timer%) / 60
  wait 60 s
  * tick message
  if %meteor% == 1
    if %progress% < 5
      %echo% A red-hot meteor fumes as it streaks through the sky.
      %at% i12925 %echo% A red-hot meteor fumes as it streaks through the sky.
      %at% i12922 %echo% The wall of flame grows brighter as the rock you're standing on plummets through the sky!
    elseif %progress% < 10
      %echo% There's a loud CRACK! as a plume of smoke blasts off of the meteor in the sky!
      %at% i12925 %echo% There's a loud CRACK! as a plume of smoke blasts off of the meteor in the sky!
      %at% i12922 %echo% There's a loud CRACK! sound from somewhere inside the rock beneath your feet!
      %at% i12920 %echo% There's a loud CRACK! as smoke bursts from another meteor, in the distance.
      %at% i12923 %echo% There's a loud CRACK! as smoke bursts from another meteor, in the distance.
    elseif %progress% < 15
      %echo% A faint roar rumbles through the air as a blazing meteor gets closer to the crater!
      %at% i12925 %echo% A faint roar rumbles through the air as a blazing meteor gets closer to the crater!
      %at% i12922 %echo% The rock falls through a layer of clouds as the flames around it intensify...
    else
      %echo% A roaring fireball dominates the sky as the meteor gets closer... and closer!
      %at% i12925 %echo% A roaring fireball dominates the sky as the meteor gets closer... and closer!
      %at% i12922 %echo% A glance outward shows the ground coming up fast!
      %at% i12920 %echo% Out below, you see another meteor about to strike the ground!
      %at% i12923 %echo% Out below, you see another meteor about to strike the ground!
    end
  elseif %meteor% == 2
    if %progress% < 5
      %echo% A pair of burning meteors drag across the sky.
      %at% i12925 %echo% A pair of burning meteors drag across the sky.
      %at% i12923 %echo% The flames around both this rock and its twin seem to grow as they streak through the sky!
    elseif %progress% < 10
      %echo% A pair of burning meteors in the sky seem to be getting closer...
      %at% i12925 %echo% A pair of burning meteors in the sky seem to be getting closer...
      %at% i12923 %echo% The ground beneath you swells and fizzles as the flames grow larger and larger.
      %at% i12920 %echo% A twin pair of burning meteors grow brighter as they streak down through the sky.
      %at% i12922 %echo% A twin pair of burning meteors grow brighter as they streak down through the sky.
    elseif %progress% < 15
      %echo% The air itself trembles as a pair of dazzling red meteors streak toward the crater.
      %at% i12925 %echo% The air itself trembles as a pair of dazzling red meteors streak toward the crater.
      %at% i12923 %echo% Beyond the wall of flames, you watch clouds evaporate as you drop through them!
    else
      %echo% The ground rumbles as the pair of meteors streak closer to the crater...
      %at% i12925 %echo% The ground rumbles as the pair of meteors streak closer to the crater...
      %at% i12923 %echo% A glance outward shows the ground coming up fast!
      %at% i12920 %echo% Out below, you see a pair of twin meteors about to strike the ground!
      %at% i12922 %echo% Out below, you see a pair of twin meteors about to strike the ground!
    end
  end
  * end this loop
  remote timer %self.id%
  halt
end
*
* otherwise time for a meteor event!
shout HEADS UP!
wait 1
%echo% ~%self% swings high and slams ^%self% eventide hammer down on the stone tree stump...
wait 1
if %meteor% == 1
  %echo% The blazing meteor comes to a halt less than a tower's height above the crater and then, miraculously, rises back into the sky to begin its descent again!
  %at% i12925 %echo% The blazing meteor comes to a halt less than a tower's height above the crater and then, miraculously, rises back into the sky to begin its descent again!
  %at% i12922 %echo% The flames around the rock die down for a moment as you feel the whole thing come to a halt in the sky...
  %at% i12922 %echo% ... and then the rock rises back into the sky!
  %at% i12920 %echo% Through the flames, you see another great rock rise up through the sky, far above you, and then begin to plummet again!
  %at% i12923 %echo% Through the flames, you see another great rock rise up through the sky, far above you, and then begin to plummet again!
  wait 1
  %at% i12922 %echo% You feel the rock lurch as it begins to drop. The flames roar up around the sides as you plummet toward the earth again!
  set meteor 2
elseif %meteor% == 2
  %echo% The twin meteors come to a stop in the sky, dangerously close to the top of your head, and then retreat back up to the heavens together!
  %at% i12925 %echo% The twin meteors come to a stop in the sky, dangerously close to the top of your head, and then retreat back up to the heavens together!
  %at% i12920 %echo% Beyond the wall of flame, you see another pair of enormous rocks spiral upward into the sky high above you, and then both rocks drop again!
  %at% i12922 %echo% Beyond the wall of flame, you see another pair of enormous rocks spiral upward into the sky high above you, and then both rocks drop again!
  %at% i12923 %echo% The roaring flames around both this rock and its twin die down as both come to a halt low in the sky...
  %at% i12923 %echo% ... the great rocks pause for just a moment and then, with a jolt, fly back up into the sky!
  wait 1
  %at% i12923 %echo% The moment's peace passes and you feel the rock begin to sink once more. In a moment, the wall of fire roars back to life as you streak through the sky!
  set meteor 1
end
* reset
set timer %timestamp%
remote timer %self.id%
remote meteor %self.id%
~
#12923
Terminus Forge: Look through archways~
2 c 0 4
L j 12921
L j 12922
L j 12923
L j 12924
look~
* return 0 in all cases because it also falls through to the people you can see
return 0
set dir %actor.parse_dir(%arg.argument1%)%
switch %room.template%
  case 12921
    if %dir% == east || %dir% == west
      %send% %actor% You approach the %actor.dir(%dir%)% archway and see a different rock... with a blazing wall of fire beyond it!
    elseif %dir% == south
      %send% %actor% You approach the %actor.dir(%dir%)% archway and see a large rock with a starry night sky beyond.
    end
  break
  case 12922
    if %dir% == east
      %send% %actor% You look through the craggy metal archway and see the crater.
    end
  break
  case 12923
    if %dir% == west
      %send% %actor% You look through the rough metal archway and see the crater.
    end
  break
  case 12924
    if %dir% == north
      %send% %actor% You look through the quiet metal archway and see the crater.
    end
  break
done
~
#12926
Terminus Forge: Reset splat timers on enter~
2 g 100 0
~
if %actor.is_pc%
  rdelete splat_%actor.id% %room.id%
end
~
#12927
Terminus Forge: Lion of Time intro~
0 nA 100 0
~
wait 0
set room %self.room%
* find highest visit count and raise visit counts -- and reset splat timers
set ch %room.people%
set highest 1
while %ch%
  if %ch.is_pc%
    rdelete splat_%ch.id% %room.id%
    set skithe_visits %ch.varexists(skithe_visits,0)%
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
%echo% The Lion of Time, says, '%message%'
wait 6 sec
if !%self.fighting%
  %echo% The Lion of Time, says, 'No power under the stars can stop me from devouring this bloated moment for all time!'
end
~
#12928
Terminus Forge: Splatter ticker~
2 bw 100 0
~
* config time in seconds to live
set splat_time 600
* main loop
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_pc%
    set splat %room.var(splat_%ch.id%,0)%
    if %ch.fighting% || %splat% > 0
      * falling
      if %ch.is_flying%
        * reset timer while flying
        rdelete splat_%ch.id% %room.id%
      elseif %splat% == 0
        * start
        set splat_%ch.id% %timestamp%
        remote splat_%ch.id% %room.id%
      else
        * check fall
        if (%timestamp% - %splat%) >= %splat_time%
          * dedz
          %send% %ch% &&wOops... the ground came up faster than extected...&&0
          %send% %ch% &&wThe last thing that goes through your mind as your fall ends... is your boots.&&0
          %echoaround% %ch% &&w~%ch% splatters as &%ch% hits the ground!&&0
          %slay% %ch% %ch.real_name% has run out of time... and died
        end
      end
    end
  end
  set ch %next_ch%
done
~
#12929
Lion of Time combat: Shooting stars, Consumption Time, Eternal Sunshine~
0 c 0 3
L w 12817
L w 12821
L w 12929
!stars !consume !sunshine~
set targ %arg%
set room %self.room%
set diff %self.var(diff,1)%
set cmd %cmd.substr(1)%
if %actor% != %self% || !%targ% || %targ.id% == %self.id%
  halt
elseif %cmd% == stars
  * Shooting stars (group dodge)
  scfight clear dodge
  %echo% &&wBright lights twinkle to life across the world below...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  %echo% &&w**** Stars rise up from the ground -- watch out! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval penalty 20 * %diff%
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    scfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)% && %ch.is_pc%
        if !%ch.var(did_scfdodge)%
          set hit 1
          %echo% &&wA star passes through ~%ch% as it shoots into the sky!&&0
          eval splat %room.var(splat_%ch.id%,60)% - %penalty%
          if %splat% > 0
            set splat_%ch.id% %splat%
            remote splat_%ch.id% %room.id%
          end
        else
          %send% %ch% &&wYou struggle to avoid a rising star as it shoots upward!&&0
          if %diff% == 1
            dg_affect #12821 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&w**** Here comes another one... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
    eval cycle %cycle% + 1
  done
  wait 8 s
elseif %cmd% == consume
  * Consumption Time (interrupt)
  scfight clear interrupt
  %echo% &&w**** The sunset grinds to a halt as the lion bites down on time itself! ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  scfight setup interrupt all
  eval penalty 30 * %diff%
  wait 3 s
  if %diff% > 2
    set needed %room.players_present%
  else
    set needed 1
  end
  if %self.var(count_scfinterrupt,0)% < %needed%
    %echo% &&w**** Meteors halt in the sky as the lion wolfs down time itself! ****&&0 (interrupt)
  end
  wait 3 s
  if %self.var(count_scfinterrupt,0)% >= %needed%
    %echo% &&wMeteors resume their fiery tumble as time returns to normal.&&0
    if %diff% == 1
      dg_affect #12817 %self% HARD-STUNNED on 5
    end
    wait 30 s
  else
    * %echo% wno message, just splat chance
    set ch %room.people%
    while %ch%
      if %ch.is_pc%
        eval splat %room.var(splat_%ch.id%,60)% - %penalty%
        if %splat% > 0
          set splat_%ch.id% %splat%
          remote splat_%ch.id% %room.id%
        end
      end
      set ch %ch.next_in_room%
    done
    * and heal me
    eval amount %self.maxhealth% / 15
    dg_affect #12929 %self% HEAL-OVER-TIME %amount% 15
  end
  scfight clear interrupt
elseif %cmd% == sunshine
  * Eternal Sunshine (group interrupt)
  scfight clear interrupt
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  %echo% &&w**** Meteors reverse course and rise into the air as the sun begins to peak over the western horizon... ****&&0 (interrupt)
  if %diff% > 2
    set needed %room.players_present%
  else
    set needed 1
  end
  set cycle 1
  eval pain 100 * %diff%
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    scfight setup interrupt all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)% && !%ch.dead%
        if %self.var(count_scfinterrupt,0)% < %needed%
          %send% %ch% &&WThe eternal sunshine washes over you, cleansing you from existence!&&0
          %echoaround% %ch% &&w~%ch% is washed away by the eternal sunshine!&&0
          if %diff% < 4
            %damage% %ch% %pain% direct
          else
            %slay% %ch% %ch.real_name% has been destroyed in the eternal sunshine
          end
        elseif %ch.is_pc%
          %send% %ch% &&wYou manage to interrupt the western sunrise!&&0
          if %diff% == 1
            dg_affect #12821 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&w**** The western sun is still rising -- there's little time! ****&&0 (interrupt)
        end
      end
      set ch %next_ch%
    done
    scfight clear interrupt
    eval cycle %cycle% + 1
  done
  wait 8 s
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#12945
Celestial Forge: Blazing comet minipet~
0 n 100 2
L c 12952
L w 12945
~
set ch %self.leader%
* determine whether it's a light this time or not
if %ch%
  if %ch.cooldown(12945)%
    set lit 0
  else
    set lit 1
    nop %ch.set_cooldown(12945,21600)%
  end
else
  * no ch
  set lit 0
end
* set up self
if %lit%
  %load% obj 12952 %self% about
  %mod% %self% append-lookdesc It's bright enough to light up the area!
else
  * not lit
  %mod% %self% keywords comet streaking blazing
  %mod% %self% longdesc A comet streaks overhead.
  %mod% %self% shortdesc a streaking comet
  %mod% %self% lookdesc It has a round, white nucleus and a long, diffuse gray tail. As you move to get a better look at the comet, you realize it's much closer -- and smaller -- than it first appeared.
  %mod% %self% append-lookdesc-noformat (Blazing comets can only be summoned to provide light once every 6 hours.)
end
~
#12946
Celestial Forge: Blazing comet burn-out~
1 f 0 0
~
set mob %self.worn_by%
if !%mob%
  halt
end
if !%mob.is_npc%
  halt
end
%echo% The comet dims to a pale gray.
%mod% %mob% keywords comet streaking
%mod% %mob% longdesc A comet streaks overhead.
%mod% %mob% shortdesc a streaking comet
%mod% %mob% lookdesc It has a round, white nucleus and a long, diffuse gray tail. As you move to get a better look at the comet, you realize it's much closer -- and smaller -- than it first appeared.
%mod% %mob% append-lookdesc-noformat (Blazing comets can only be summoned to provide light once every 6 hours.)
* and purge self silently
return 0
%purge% %self%
~
#12947
Impact Harness: Can't sit without mounts~
5 c 0 0
sit~
if %actor.veh_target(%arg.argument1%)% != %self%
  return 0
elseif %self.animals_harnessed% < %self.animals_required%
  if %self.animals_required% > 1
    %send% %actor% You can't sit on @%self% without %self.animals_required% animals harnessed on.
  else
    %send% %actor% You can't sit on @%self% without an animal harnessed on.
  end
  return 1
else
  return 0
end
~
#12948
Impact Harness: Impact on un-harness~
5 c 0 0
unharness~
* someone attempts to unharness anything here
return 0
wait 0
* did I get unharnessed?
if %self.animals_harnessed% < %self.animals_required%
  %echo% @%self% comes crashing down!
  set fool %self.sitting_in%
  if %fool%
    if %fool.room.is_water%
      %force% %fool% stand
    else
      %echoaround% %fool% ~%fool% splatters on the ground!
      %send% %fool% You splatter on the ground!
      %slay% %fool% %fool.real_name% has impacted the ground at %fool.room.coords%!
    end
  end
end
~
$
