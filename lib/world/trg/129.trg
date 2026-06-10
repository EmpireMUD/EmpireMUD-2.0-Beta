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
2 q 100 8
L c 9680
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
$
