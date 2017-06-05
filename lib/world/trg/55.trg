#5511
Sorcery Tower: Study Sorcery~
2 c 0
study~
if !%arg%
  %send% %actor% Study what? (try 'study sorcery')
  halt
end
if !(sorcery /= %arg%)
  return 0
  halt
end
eval permission %%actor.canuseroom_guest(%room%)%%
if !%permission%
  %send% %actor% You don't have permission to study here!
  return 1
  halt
end
if %actor.has_item(5511)%
  %send% %actor% You already have a High Sorcery textbook.
  return 1
  halt
end
%load% obj 5511 %actor% inv
eval item %actor.inventory(5511)%
%send% %actor% You pick up %item.shortdesc%. Reading this would surely start you on the path of High Sorcery.
%echoaround% %actor% %actor.name% takes %item.shortdesc%.
~
#5512
Study High Sorcery~
1 t 100
~
if !%self.is_flagged(ENCHANTED)%
  halt
end
if %actor.can_gain_new_skills% && %actor.skill(High Sorcery)% == 0 && !%actor.noskill(High Sorcery)%
  %send% %actor% &mYour mind begins to open to the ways of High Sorcery, and you are now an apprentice to this school.&0
  nop %actor.gain_skill(High Sorcery,1)%
end
~
$
