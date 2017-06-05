#450
Mirror Image no-heal~
0 pt 100
~
* Prevents 'Heal Friend' and 'Rejuvenate'
if (%ability% != 110 && %ability% != 114)
  halt
end
%send% %actor% You can't heal a mirror image!
return 0
~
$
