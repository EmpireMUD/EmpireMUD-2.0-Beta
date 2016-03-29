#33
Time Lion death 2.0~
0 f 100
~
%echo% Oh no! You've messed with the time lion and now you no longer exist!
eval room %self.room%
eval person %room.people%
while %person%
  if %person% != %self%
    %damage% %person% 10000 direct
  end
  eval person %person.next_in_room%
done
%load% mob 33
return 0
~
$
