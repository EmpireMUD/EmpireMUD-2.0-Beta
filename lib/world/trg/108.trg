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
$
