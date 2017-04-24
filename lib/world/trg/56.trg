#5608
Study: create board~
2 o 100
~
%load% obj 1
eval board %room.contents%
%echo% %board.shortdesc% appears.
detach 5608 %self.id%
~
#5609
Study: remove board~
2 s 0
~
eval obj %room.contents%
eval next_obj %obj.next_in_list%
while %obj%
  if %obj.vnum% == 1
    %purge% %obj% $p disappears.
  end
  eval obj %next_obj%
done
~
$
