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
    elseif %progress% < 15
      %echo% A faint roar rumbles through the air as a blazing meteor gets closer to the crater!
      %at% i12925 %echo% A faint roar rumbles through the air as a blazing meteor gets closer to the crater!
      %at% i12922 %echo% The rock falls through a layer of clouds as the flames around it intensify...
    else
      %echo% A roaring fireball dominates the sky as the meteor gets closer... and closer!
      %at% i12925 %echo% A roaring fireball dominates the sky as the meteor gets closer... and closer!
      %at% i12922 %echo% A glance outward shows the ground coming up fast!
    end
  elseif %meteor% == 2
    if %progress% < 5
      %echo% A pair of burning meteors drags across through the sky.
      %at% i12925 %echo% A pair of burning meteors drags across through the sky.
      %at% i12923 %echo% The flames around both this rock and its twin seem to grow as they streak through the sky!
    elseif %progress% < 10
      %echo% A pair of burning meteors in the sky seem to be getting closer...
      %at% i12925 %echo% A pair of burning meteors in the sky seem to be getting closer...
      %at% i12923 %echo% The ground beneath you swells and fizzles as the flames grow larger and larger.
    elseif %progress% < 15
      %echo% The air itself trembles as a pair of dazzling red meteors streak toward the crater.
      %at% i12925 %echo% The air itself trembles as a pair of dazzling red meteors streak toward the crater.
      %at% i12923 %echo% Clouds evaporate in the air beyond the wall of flames as you drop through them!
    else
      %echo% The ground rumbles as the pair of meteors streak closer to the crater...
      %at% i12925 %echo% The ground rumbles as the pair of meteors streak closer to the crater...
      %at% i12923 %echo% A glance outward shows the ground coming up fast!
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
  %at% i12923 %echo% Through the flames, you see another great rock rise up through the sky, far above you, and then begin to plummet again!
  wait 1
  %at% i12922 %echo% You feel the rock lurch as it begins to drop. The flames roar up around the sides as you plummet toward the earth again!
  set meteor 2
elseif %meteor% == 2
  %echo% The twin meteors come to a stop in the sky, dangerously close to the top of your head, and then retreat back up to the heavens together!
  %at% i12925 %echo% The twin meteors come to a stop in the sky, dangerously close to the top of your head, and then retreat back up to the heavens together!
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
$
