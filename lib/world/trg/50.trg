#5006
Open Ruins: Random Icon~
2 n 100
~
* pick random icons for ruins
switch %random.7%
  case 1
    %mod% %room% icon .&0_i&?.
  break
  case 2
    %mod% %room% icon .&0[.&?.
  break
  case 3
    %mod% %room% icon .&0.v&?.
  break
  case 4
    %mod% %room% icon .&0/]&?.
  break
  case 5
    %mod% %room% icon .&0(\\&?.
  break
  case 6
    %mod% %room% icon .&0}\\.
  break
  case 7
    %mod% %room% icon &0..}&?.
  break
done
if %room.title% ~= #n
  %mod% %room% title Some Decaying Ruins
end
detach 5006 %room.id%
~
#5007
Closed Ruins: Random Icon~
2 n 100
~
* pick random icons for ruins
switch %random.7%
  case 1
    %mod% %room% icon ..&0/]
  break
  case 2
    %mod% %room% icon &0[\\&?..
  break
  case 3
    %mod% %room% icon &0|\\&?..
  break
  case 4
    %mod% %room% icon &0[&?__&0]
  break
  case 5
    %mod% %room% icon ..&0/]
  break
  case 6
    %mod% %room% icon .&0-&?.&0]
  break
  case 7
    %mod% %room% icon &0[&?.&0-&?.
  break
done
if %room.title% ~= #n
  %mod% %room% title Some Decaying Ruins
end
detach 5007 %room.id%
~
#5010
Flooded Ruins Name Fixer~
2 n 100
~
if %room.title% ~= #n
  %mod% %room% title Some Flooded Ruins
end
detach 5010 %room.id%
~
$
