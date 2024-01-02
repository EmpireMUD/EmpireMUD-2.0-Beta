#1100
Shimmering anvil: Lasts 1 hour~
5 ab 10
~
* requires 9681 to set load_time
if %timestamp% - %self.var(load_time)% >= 3600
  %echo% %self.shortdesc% dissipates into a cloud of dust.
  %purge% %self%
end
~
$
