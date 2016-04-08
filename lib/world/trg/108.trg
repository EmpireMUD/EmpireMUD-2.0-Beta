#10800
Pageboy spawn~
0 n 100
~
if (!%instance.location%)
  halt
end
mgoto %instance.location%
eval room %self.room%
if %room.exit_dir%
  %room.exit_dir%
end
~
#10801
Give rejection~
0 j 100
~
%send% %actor% You can't give items to %self.name%. Try "quest finish <name>" instead.
return 0
~
#10802
Pageboy shout~
0 ab 1
~
if %random.3% == 3
  %regionecho% %self.room% 20 %self.name shouts, 'Learn the Empire skill at the (^^) Royal Planning Office!'
end
~
$
