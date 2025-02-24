--[[
AI for pers unique pilots.

Meant to be pretty flexible. You probably want to define the following:
* mem.ad -- message spammed across the system
* mem.comm_greet -- greeting message when hailed
* mem.taunt -- message to spam when engaging the enemy
--]]
require 'ai.core.core'
require 'ai.core.idle.advertiser'
local fmt = require "format"

mem.lanes_useneutral = true
mem.aggressive = true
-- Spam less often
mem.ad = nil -- Has to be set for them to spam
mem.adspamdelayalpha = 45
mem.adspamdelaybeta = 90

function create ()
   create_pre()

   -- Credits.
   local price = ai.pilot():ship():price()
   ai.setcredits( rnd.rnd(price/60, price/15) )

   -- Refuel
   mem.refuel = rnd.rnd( 1000, 3000 )
   mem.refuel_msg = fmt.f(_([["I'll supply your ship with fuel for {credits}."]]),
         {credits=fmt.credits(mem.refuel)})

   mem.loiter = rnd.rnd(15,20) -- This is the amount of waypoints the pilot will pass through before leaving the system
   create_post()
end

function taunt( target, _offense )
   if mem.taunt then
      ai.pilot():comm( target, mem.taunt )
   end
end
