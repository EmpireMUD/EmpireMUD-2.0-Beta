#5608
Study: create board~
2 o 100
~
%load% obj 1
set board %room.contents%
%echo% %board.shortdesc% appears.
detach 5608 %self.id%
~
#5609
Study: remove board~
2 s 0
~
set obj %room.contents%
while %obj%
  set next_obj %obj.next_in_list%
  if %obj.vnum% == 1
    %purge% %obj% $p disappears.
  end
  set obj %next_obj%
done
~
$
