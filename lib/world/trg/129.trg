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
$
