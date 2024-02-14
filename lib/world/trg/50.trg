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
    %mod% %room% icon .&0(\&?.
  break
  case 6
    %mod% %room% icon .&0}\.
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
    %mod% %room% icon &0[\&?..
  break
  case 3
    %mod% %room% icon &0|\&?..
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
#5009
Ruins icons for vehicle-buildings~
5 n 100
~
* Random ruins icons
set open_list 5003 5004 5005 5006
if %open_list% ~= %self.vnum%
  * open-style ruins
  switch %random.7%
    case 1
      %mod% %self% icon .&0_i&?.
      %mod% %self% halficon &0_i
      %mod% %self% quartericon &0i
    break
    case 2
      %mod% %self% icon .&0[.&?.
      %mod% %self% halficon &0[.
      %mod% %self% quartericon &0[
    break
    case 3
      %mod% %self% icon .&0.v&?.
      %mod% %self% halficon &0.v
      %mod% %self% quartericon &0v
    break
    case 4
      %mod% %self% icon .&0/]&?.
      %mod% %self% halficon &0/]
      %mod% %self% quartericon &0]
    break
    case 5
      %mod% %self% icon .&0(\&?.
      %mod% %self% halficon &0(\
      %mod% %self% quartericon &0\
    break
    case 6
      %mod% %self% icon .&0}\.
      %mod% %self% halficon &0}\
      %mod% %self% quartericon &0{
    break
    case 7
      %mod% %self% icon &0..}&?.
      %mod% %self% halficon &0.}
      %mod% %self% quartericon &0}
    break
  done
else
  switch %random.7%
    case 1
      %mod% %self% icon ..&0/]
      %mod% %self% halficon &0/]
      %mod% %self% quartericon &0/
    break
    case 2
      %mod% %self% icon &0[\&?..
      %mod% %self% halficon &0[\
      %mod% %self% quartericon &0\
    break
    case 3
      %mod% %self% icon &0|\&?..
      %mod% %self% halficon &0|\
      %mod% %self% quartericon &0|
    break
    case 4
      %mod% %self% icon &0[&?__&0]
      %mod% %self% halficon &0[&?_
      %mod% %self% quartericon &0[
    break
    case 5
      %mod% %self% icon ..&0/]
      %mod% %self% halficon &0/]
      %mod% %self% quartericon &0]
    break
    case 6
      %mod% %self% icon .&0-&?.&0]
      %mod% %self% halficon &0.]
      %mod% %self% quartericon &0.
    break
    case 7
      %mod% %self% icon &0[&?.&0-&?.
      %mod% %self% halficon &0[.
      %mod% %self% quartericon &0[
    break
  done
end
if %self.shortdesc% ~= #n
  %mod% %self% shortdesc some decaying ruins
end
if %self.longdesc% ~= #n
  %mod% %self% longdesc Some decaying ruins are crumbling here.
end
if %self.keywords% ~= #n
  %mod% %self% keywords ruins decaying
end
detach 5009 %self.id%
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
#5011
Ruins name verifier for flooded ruins~
5 n 100
~
if %self.shortdesc% ~= #n
  %mod% %self% shortdesc some flooded ruins
end
if %self.longdesc% ~= #n
  %mod% %self% longdesc Some flooded ruins are crumbling here.
end
if %self.keywords% ~= #n
  %mod% %self% keywords ruins flooded
end
detach 5011 %self.id%
~
$
