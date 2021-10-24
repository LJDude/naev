--[[
<?xml version='1.0' encoding='utf8'?>
<mission name="Commodity Run">
 <avail>
  <priority>5</priority>
  <cond>var.peek("commodity_runs_active") == nil or var.peek("commodity_runs_active") &lt; 3</cond>
  <chance>90</chance>
  <location>Computer</location>
  <faction>Dvaered</faction>
  <faction>Empire</faction>
  <faction>Frontier</faction>
  <faction>Goddard</faction>
  <faction>Independent</faction>
  <faction>Proteron</faction>
  <faction>Sirius</faction>
  <faction>Soromid</faction>
  <faction>Thurion</faction>
  <faction>Traders Guild</faction>
  <faction>Za'lek</faction>
 </avail>
 <notes>
  <tier>1</tier>
 </notes>
</mission>
--]]
--[[

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

--

   Commodity delivery missions.
--]]
local pir = require "common.pirate"
local fmt = require "format"
local vntk = require "vntk"

--Mission Details
misn_title = _("%s Delivery")
misn_desc = _("%s has an insufficient supply of %s to satisfy the current demand. Go to any planet which sells this commodity and bring as much of it back as possible.")

cargo_land = {}
cargo_land[1] = _("The containers of %s are carried out of your ship and tallied. After several different men double-check the register to confirm the amount, you are paid %s and summarily dismissed.")
cargo_land[2] = _("The containers of %s are quickly and efficiently unloaded, labeled, and readied for distribution. The delivery manager thanks you with a credit chip worth %s.")
cargo_land[3] = _("The containers of %s are unloaded from your vessel by a team of dockworkers who are in no rush to finish, eventually delivering %s after the number of tonnes is determined.")
cargo_land[4] = _("The containers of %s are unloaded by robotic drones that scan and tally the contents. The human overseer hands you %s when they finish.")

osd_title = _("Commodity Delivery")
paying_faction = faction.get("Independent")


-- A script may require "missions/neutral/commodity_run" and override this
-- with a table of (raw) commodity names to choose from.
commchoices = nil


function update_active_runs( change )
   local current_runs = var.peek( "commodity_runs_active" )
   if current_runs == nil then current_runs = 0 end
   var.push( "commodity_runs_active", math.max( 0, current_runs + change ) )

   -- Note: This causes a delay (defined in create()) after accepting,
   -- completing, or aborting a commodity run mission.  This is
   -- intentional.
   var.push( "last_commodity_run", time.tonumber( time.get() ) )
end


function create ()
   -- Note: this mission does not make any system claims.

   misplanet = planet.cur()
   missys = system.cur()

   if commchoices == nil then
      local std = commodity.getStandard();
      chosen_comm = std[rnd.rnd(1, #std)]:nameRaw()
   else
      chosen_comm = commchoices[rnd.rnd(1, #commchoices)]
   end
   local comm = commodity.get(chosen_comm)
   local mult = rnd.rnd(1, 3) + math.abs(rnd.threesigma() * 2)
   price = comm:price() * mult

   local last_run = var.peek( "last_commodity_run" )
   if last_run ~= nil then
      local delay = time.create(0, 7, 0)
      if time.get() < time.fromnumber(last_run) + delay then
         misn.finish(false)
      end
   end

   for i, j in ipairs( missys:planets() ) do
      for k, v in pairs( j:commoditiesSold() ) do
         if v == comm then
            misn.finish(false)
         end
      end
   end

   -- Set Mission Details
   misn.setTitle( misn_title:format( comm:name() ) )
   misn.markerAdd( system.cur(), "computer" )
   misn.setDesc( misn_desc:format( misplanet:name(), comm:name() ) )
   misn.setReward( _("%s per tonne"):format( fmt.credits( price ) ) )
end


function accept ()
   local comm = commodity.get(chosen_comm)

   misn.accept()
   update_active_runs( 1 )

   misn.osdCreate(osd_title, {
      _("Buy as much %s as possible"):format( comm:name() ),
      _("Take the %s to %s in the %s system"):format( comm:name(), misplanet:name(), missys:name() ),
   })

   hook.enter("enter")
   hook.land("land")
end


function enter ()
   if pilot.cargoHas( player.pilot(), chosen_comm ) > 0 then
      misn.osdActive(2)
   else
      misn.osdActive(1)
   end
end


function land ()
   local amount = pilot.cargoHas( player.pilot(), chosen_comm )
   local reward = amount * price

   if planet.cur() == misplanet and amount > 0 then
      local txt = cargo_land[rnd.rnd(1, #cargo_land)]:format(
            _(chosen_comm), fmt.credits(reward) )
      vntk.msg(_("Delivery success!"), txt)
      pilot.cargoRm(player.pilot(), chosen_comm, amount)
      player.pay(reward)
      if not pir.factionIsPirate( paying_faction ) then
         pir.reputationNormalMission(rnd.rnd(2,3))
      end
      update_active_runs(-1)
      misn.finish(true)
   end
end


function abort ()
   update_active_runs(-1)
end

