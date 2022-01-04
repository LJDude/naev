--[[
   Some sort of Ionic storm-type background.
--]]
local graphics = require "love.graphics"
local bgshaders = require "bkg.lib.bgshaders"
local love_shaders = require 'love_shaders'
local lf = require 'love.filesystem'
-- We use the default background too!
require "bkg.default"

local shader, sf, sstorm

local background_default = background
function background ()
   -- Scale factor that controls computation cost. As this shader is really
   -- really expensive, we can't compute it at full resolution
   sf = naev.conf().nebu_scale * 0.5
   sf = math.max( 1.0, sf )

   -- Initialize shader
   local pixelcode = lf.read( "bkg/shaders/ionicstorm.frag" )
   shader = graphics.newShader( pixelcode, love_shaders.vertexcode )
   shader._dt = -1000 * rnd.rnd()
   shader.update = function( self, dt )
      self._dt = self._dt + dt
      self:send( "u_time", self._dt )
   end
   sstorm = bgshaders.init( shader, sf )

   -- Default nebula background
   background_default()
end

function renderfg( dt )
   -- Get camera properties
   local x, y = camera.get():get()
   local z = camera.getZoom()
   shader:send( "u_camera", x*0.5/sf, -y*0.5/sf, z )

   sstorm:render( dt, {1.0, 0.5, 1.0, 1.0} )
end
