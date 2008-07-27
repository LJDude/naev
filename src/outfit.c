/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file outfit.c
 *
 * @brief Handles all the ship outfit specifics.
 *
 * These outfits allow you to modify ships or make them more powerful and are
 *  a fundamental part of the game.
 */


#include "outfit.h"

#include <math.h>
#include <string.h>

#include "xml.h"
#include "SDL_thread.h"

#include "naev.h"
#include "log.h"
#include "pack.h"
#include "spfx.h"


#define outfit_setProp(o,p)      ((o)->properties |= p) /**< Checks outfit property. */


#define XML_OUTFIT_ID      "Outfits"   /**< XML section identifier. */
#define XML_OUTFIT_TAG     "outfit"    /**< XML section identifier. */

#define OUTFIT_DATA  "dat/outfit.xml" /**< File that contains the outfit data. */
#define OUTFIT_GFX   "gfx/outfit/" /**< Path to outfit graphics. */


/*
 * the stack
 */
static Outfit* outfit_stack = NULL; /**< Stack of outfits. */
static int outfit_nstack = 0; /**< Size of the stack. */


/*
 * Prototypes
 */
/* misc */
static DamageType outfit_strToDamageType( char *buf );
static OutfitType outfit_strToOutfitType( char *buf );
/* parsing */
static int outfit_parseDamage( DamageType *dtype, double *dmg, xmlNodePtr node );
static Outfit* outfit_parse( const xmlNodePtr parent );
static void outfit_parseSBolt( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSBeam( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSLauncher( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSAmmo( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSMod( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSAfterburner( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSMap( Outfit *temp, const xmlNodePtr parent );
static void outfit_parseSJammer( Outfit *temp, const xmlNodePtr parent );


/**
 * @fn Outfit* outfit_get( const char* name )
 *
 * @brief Gets an outfit by name.
 *
 *    @param name Name to match.
 *    @return Outfit matching name or NULL if not found.
 */
Outfit* outfit_get( const char* name )
{
   int i;
   for (i=0; i<outfit_nstack; i++)
      if (strcmp(name,outfit_stack[i].name)==0)
         return &outfit_stack[i];

   WARN("Outfit '%s' not found in stack.", name);
   return NULL;
}


/**
 * @fn char** outfit_getTech( int *n, const int *tech, const int techmax )
 *
 * @brief Gets all the outfits matching technology requirements.
 *
 * Function will already sort the outfits by type and then by price making
 *  it much easier to handle later on.
 *
 *    @param[out] n Number of outfits found.
 *    @param[in] tech Technologies to check against.  The first one represents
 *                    overall technology, the others are specific technologies.
 *    @param[in] techmax Number of technologies in tech.
 *    @return An allocated array of allocated strings with the names of outfits
 *            matching the tech requirements.
 */
char** outfit_getTech( int *n, const int *tech, const int techmax )
{
   int i,j,k, num, price;
   Outfit **outfits;
   char **outfitnames;
   OutfitType type;
   
   outfits = malloc(sizeof(Outfit*) * outfit_nstack);

   /* get the available techs */
   num = 0;
   for (i=0; i < outfit_nstack; i++)
      if (outfit_stack[i].tech <= tech[0]) { /* check vs base tech */
         outfits[num] = &outfit_stack[i];
         num++;
      }
      else {
         for(j=0; j<techmax; j++) /* check vs special techs */
            if (tech[j] ==outfit_stack[i].tech) {
               outfits[num] = &outfit_stack[i];
               num++;
            }
      }

   /* now sort by type and price */
   *n = 0;
   price = -1;
   type = OUTFIT_TYPE_NULL+1; /* first type */
   outfitnames = malloc(sizeof(char*) * num);

   /* sort by type */
   while (type < OUTFIT_TYPE_SENTINEL) {

      /* check for cheapest */
      for (j=0; j<num; j++) {

         /* must be of the current type */
         if (outfits[j]->type != type) continue;

         /* is cheapest? */
         if ((price == -1) || (outfits[price]->price > outfits[j]->price)) {

            /* check if already in stack */
            for (k=0; k<(*n); k++)
               if (strcmp(outfitnames[k],outfits[j]->name)==0)
                  break;

            /* not in stack and therefore is cheapest */
            if (k == (*n))
               price = j;
         }
      }

      if (price == -1)
         type++; 
      else {
         /* add current cheapest to stack */
         outfitnames[*n] = strdup(outfits[price]->name);
         (*n)++;
         price = -1;
      }
   }
   
   /* cleanup */
   free(outfits);

   return outfitnames;
}


/**
 * @fn void outfit_calcDamage( double *dshield, double *darmour, double *knockback,
 *       DamageType dtype, double dmg )
 *
 * @brief Gives the real shield damage, armour damage and knockback modifier.
 *
 *    @param[out] dshield Real shield damage.
 *    @param[out] darmour Real armour damage.
 *    @param[out] knockback Knocback modifier.
 *    @param[in] dtype Damage type.
 *    @param[in] dmg Amoung of damage.
 */
void outfit_calcDamage( double *dshield, double *darmour, double *knockback,
      DamageType dtype, double dmg )
{
   switch (dtype) {
      case DAMAGE_TYPE_ENERGY:
         (*dshield) = dmg*1.1;
         (*darmour) = dmg*0.7;
         (*knockback) = 0.1;
         break;
      case DAMAGE_TYPE_KINETIC:
         (*dshield) = dmg*0.8;
         (*darmour) = dmg*1.2;
         (*knockback) = 1.;
         break;
      case DAMAGE_TYPE_ION:
         (*dshield) = dmg;
         (*darmour) = dmg;
         (*knockback) = 0.4;
         break;
      case DAMAGE_TYPE_RADIATION:
         (*dshield) = 0.15; /* still take damage, just not much */
         (*darmour) = dmg;
         (*knockback) = 0.8;
         break;

      default:
         WARN("Unknown damage type: %d!", dtype);
         (*dshield) = (*darmour) = (*knockback) = 0.;
         break;
   }
}


/**
 * @fn int outfit_isWeapon( const Outfit* o )
 * @brief Checks if outfit is a fixed mounted weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a weapon (beam/bolt).
 */
int outfit_isWeapon( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_BOLT)      ||
         (o->type==OUTFIT_TYPE_BEAM) );
}
/**
 * @fn int outfit_isBolt( const Outfit* o )
 * @brief Checks if outfit is bolt type weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a bolt type weapon.
 */
int outfit_isBolt( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_BOLT)      ||
         (o->type==OUTFIT_TYPE_TURRET_BOLT) );
}
/**
 * @fn int outfit_isBeam( const Outfit* o )
 * @brief Checks if outfit is a beam type weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a beam type weapon.
 */
int outfit_isBeam( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_BEAM)      ||
         (o->type==OUTFIT_TYPE_TURRET_BEAM) );
}
/**
 * @fn int outfit_isLauncher( const Outfit* o )
 * @brief Checks if outfit is a weapon launcher.
 *    @param o Outfit to check.
 *    @return 1 if o is a weapon launcher.
 */
int outfit_isLauncher( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_MISSILE_DUMB) ||
         (o->type==OUTFIT_TYPE_MISSILE_SEEK)    ||
         (o->type==OUTFIT_TYPE_MISSILE_SEEK_SMART) ||
         (o->type==OUTFIT_TYPE_MISSILE_SWARM)   ||
         (o->type==OUTFIT_TYPE_MISSILE_SWARM_SMART) );
}
/**
 * @fn int outfit_isAmmo( const Outfit* o )
 * @brief Checks if outfit is ammo for a launcher.
 *    @param o Outfit to check.
 *    @return 1 if o is ammo.
 */
int outfit_isAmmo( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_MISSILE_DUMB_AMMO)  ||
         (o->type==OUTFIT_TYPE_MISSILE_SEEK_AMMO)     ||
         (o->type==OUTFIT_TYPE_MISSILE_SEEK_SMART_AMMO) ||
         (o->type==OUTFIT_TYPE_MISSILE_SWARM_AMMO)    ||
         (o->type==OUTFIT_TYPE_MISSILE_SWARM_SMART_AMMO) );
}
/**
 * @fn int outfit_isSeeker( const Outfit* o )
 * @brief Checks if outfit is a seeking weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a seeking weapon.
 */
int outfit_isSeeker( const Outfit* o )
{
   if ((o->type==OUTFIT_TYPE_MISSILE_SEEK_AMMO)     ||
         (o->type==OUTFIT_TYPE_MISSILE_SEEK_SMART_AMMO) ||
         (o->type==OUTFIT_TYPE_MISSILE_SWARM_AMMO)    ||
         (o->type==OUTFIT_TYPE_MISSILE_SWARM_SMART_AMMO))
      return 1;
   return 0;
}
/**
 * @fn int outfit_isTurret( const Outfit* o )
 * @brief Checks if outfit is a turret class weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a turret class weapon.
 */
int outfit_isTurret( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_TURRET_BOLT) ||
         (o->type==OUTFIT_TYPE_TURRET_BEAM) );
}
/**
 * @fn int outfit_isMod( const Outfit* o )
 * @brief Checks if outfit is a ship modification.
 *    @param o Outfit to check.
 *    @return 1 if o is a ship modification.
 */
int outfit_isMod( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_MODIFCATION);
}
/**
 * @fn int outfit_isAfterburner( const Outfit* o )
 * @brief Checks if outfit is an afterburner.
 *    @param o Outfit to check.
 *    @return 1 if o is an afterburner.
 */
int outfit_isAfterburner( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_AFTERBURNER);
}
/**
 * @fn int outfit_isJammer( const Outfit* o )
 * @brief Checks if outfit is a missile jammer.
 *    @param o Outfit to check.
 *    @return 1 if o is a jammer.
 */
int outfit_isJammer( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_JAMMER);
}
/**
 * @fn int outfit_isMap( const Outfit* o )
 * @brief Checks if outfit is a space map.
 *    @param o Outfit to check.
 *    @return 1 if o is a map.
 */
int outfit_isMap( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_MAP);
}


/**
 * @fn glTexture* outfit_gfx( const Outfit* o )
 * @brief Gets the outfit's graphic effect.
 *    @param o Outfit to ge information from.
 */
glTexture* outfit_gfx( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.gfx_space;
   else if (outfit_isAmmo(o)) return o->u.amm.gfx_space;
   else if (outfit_isTurret(o)) return o->u.blt.gfx_space;
   return NULL;
}
/**
 * @fn glTexture* outfit_spfx( const Outfit* o )
 * @brief Gets the outfit's sound effect.
 *    @param o Outfit to ge information from.
 */
int outfit_spfx( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.spfx;
   else if (outfit_isAmmo(o)) return o->u.amm.spfx;
   else if (outfit_isTurret(o)) return o->u.blt.spfx;
   return -1;
}
/**
 * @fn glTexture* outfit_damage( const Outfit* o )
 * @brief Gets the outfit's damage.
 *    @param o Outfit to ge information from.
 */
double outfit_damage( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.damage;
   else if (outfit_isBeam(o)) return o->u.bem.damage;
   else if (outfit_isAmmo(o)) return o->u.amm.damage;
   else if (outfit_isTurret(o)) return o->u.blt.damage;
   return -1.;
}
/**
 * @fn glTexture* outfit_damageType( const Outfit* o )
 * @brief Gets the outfit's damage type.
 *    @param o Outfit to ge information from.
 */
DamageType outfit_damageType( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.dtype;
   else if (outfit_isBeam(o)) return o->u.bem.dtype;
   else if (outfit_isAmmo(o)) return o->u.amm.dtype;
   else if (outfit_isTurret(o)) return o->u.blt.dtype;
   return DAMAGE_TYPE_NULL;
}
/**
 * @fn glTexture* outfit_delay( const Outfit* o )
 * @brief Gets the outfit's delay.
 *    @param o Outfit to ge information from.
 */
int outfit_delay( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.delay;
   else if (outfit_isBeam(o)) return o->u.bem.delay;
   else if (outfit_isLauncher(o)) return o->u.lau.delay;
   else if (outfit_isTurret(o)) return o->u.blt.delay;
   return -1;
}
/**
 * @fn glTexture* outfit_energy( const Outfit* o )
 * @brief Gets the outfit's energy usage.
 *    @param o Outfit to ge information from.
 */
double outfit_energy( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.energy;
   else if (outfit_isBeam(o)) return o->u.bem.energy;
   else if (outfit_isAmmo(o)) return o->u.amm.energy;
   else if (outfit_isTurret(o)) return o->u.blt.energy;
   return -1.;
}
/**
 * @fn glTexture* outfit_range( const Outfit* o )
 * @brief Gets the outfit's range.
 *    @param o Outfit to ge information from.
 */
double outfit_range( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.range;
   else if (outfit_isBeam(o)) return o->u.bem.range;
   else if (outfit_isAmmo(o)) return 0.8*o->u.amm.speed*o->u.amm.duration;
   else if (outfit_isTurret(o)) return o->u.blt.range;
   return -1.;
}
/**
 * @fn glTexture* outfit_speed( const Outfit* o )
 * @brief Gets the outfit's speed.
 *    @param o Outfit to ge information from.
 */
double outfit_speed( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.speed;
   else if (outfit_isAmmo(o)) return o->u.amm.speed;
   else if (outfit_isTurret(o)) return o->u.blt.speed;
   return -1.;
}



/**
 * @fn const char* outfit_getType( const Outfit* o )
 *
 * @brief Gets the outfit's specific type.
 *
 *    @param o Outfit to get specific type from.
 *    @return The specific type in human readable form.
 */
const char* outfit_getType( const Outfit* o )
{
   const char* outfit_typename[] = { 
         "NULL",
         "Bolt Cannon",
         "Beam Cannon",
         "Bolt Turret",
         "Beam Turret",
         "Dumb Missile",
         "Dumb Missile Ammunition",
         "Seeker Missile",
         "Seeker Missile Ammunition",
         "Smart Seeker Missile",
         "Smart Seeker Missile Ammunition",
         "Swarm Missile",
         "Swarm Missile Ammunition Pack",
         "Smart Swarm Missile",
         "Smart Swarm Missile Ammunition Pack",
         "Ship Modification",
         "Afterburner",
         "Jammer",
         "Map"
   };
   return outfit_typename[o->type];
}


/**
 * @fn const char* outfit_getTypeBroad( const Outfit* o )
 *
 * @brief Gets the outfit's broad type.
 *
 *    @param o Outfit to get the type of.
 *    @return The outfit's broad type in human readable form.
 */
const char* outfit_getTypeBroad( const Outfit* o )
{
   int i = 0;
   const char* outfit_typenamebroad[] = { "NULL",
         "Bolt Weapon",
         "Beam Weapon",
         "Launcher",
         "Ammo",
         "Turret",
         "Modification",
         "Afterburner",
         "Jammer",
         "Map"
   };

   if (outfit_isBolt(o)) i = 1;
   else if (outfit_isBeam(o)) i = 2;
   else if (outfit_isLauncher(o)) i = 3;
   else if (outfit_isAmmo(o)) i = 4;
   else if (outfit_isTurret(o)) i = 5;
   else if (outfit_isMod(o)) i = 6;
   else if (outfit_isAfterburner(o)) i = 7;
   else if (outfit_isJammer(o)) i = 8;
   else if (outfit_isMap(o)) i = 9;

   return outfit_typenamebroad[i];
}


/**
 * @fn static DamageType outfit_strToDamageType( char *buf )
 *
 * @brief Gets the damage type from a human readable string.
 *
 *    @param buf String to extract damage type from.
 *    @return Damage type stored in buf.
 */
static DamageType outfit_strToDamageType( char *buf )
{
   if (strcmp(buf,"energy")==0) return DAMAGE_TYPE_ENERGY;
   else if (strcmp(buf,"kinetic")==0) return DAMAGE_TYPE_KINETIC;
   else if (strcmp(buf,"ion")==0) return DAMAGE_TYPE_ION;
   else if (strcmp(buf,"radiation")==0) return DAMAGE_TYPE_RADIATION;

   WARN("Invalid damage type: '%s'", buf);
   return DAMAGE_TYPE_NULL;
}


/**
 * @fn static OutfitType outfit_strToOutfitType( char *buf )
 *
 * @brief Gets the outfit type from a human readable string.
 *
 *    @param buf String to extract outfit type from.
 *    @return Outfit type stored in buf.
 */
#define O_CMP(s,t) \
if (strcmp(buf,(s))==0) return t; 
static OutfitType outfit_strToOutfitType( char *buf )
{
   O_CMP("bolt",OUTFIT_TYPE_BOLT);
   O_CMP("beam",OUTFIT_TYPE_BEAM);
   O_CMP("turret bolt",OUTFIT_TYPE_TURRET_BOLT);
   O_CMP("turret beam",OUTFIT_TYPE_TURRET_BEAM);
   O_CMP("missile dumb",OUTFIT_TYPE_MISSILE_DUMB);
   O_CMP("missile dumb ammo",OUTFIT_TYPE_MISSILE_DUMB_AMMO);
   O_CMP("missile seek",OUTFIT_TYPE_MISSILE_SEEK);
   O_CMP("missile seek ammo",OUTFIT_TYPE_MISSILE_SEEK_AMMO);
   O_CMP("missile smart",OUTFIT_TYPE_MISSILE_SEEK_SMART);
   O_CMP("missile smart ammo",OUTFIT_TYPE_MISSILE_SEEK_SMART_AMMO);
   O_CMP("missile swarm",OUTFIT_TYPE_MISSILE_SWARM);
   O_CMP("missile swarm ammo",OUTFIT_TYPE_MISSILE_SWARM_AMMO);
   O_CMP("missile swarm smart",OUTFIT_TYPE_MISSILE_SWARM_SMART);
   O_CMP("missile swarm smart ammo",OUTFIT_TYPE_MISSILE_SWARM_SMART_AMMO);
   O_CMP("modification",OUTFIT_TYPE_MODIFCATION);
   O_CMP("afterburner",OUTFIT_TYPE_AFTERBURNER);
   O_CMP("map",OUTFIT_TYPE_MAP);
   O_CMP("jammer",OUTFIT_TYPE_JAMMER);

   WARN("Invalid outfit type: '%s'",buf);
   return  OUTFIT_TYPE_NULL;
}
#undef O_CMP


/**
 * @fn static int outfit_parseDamage( DamageType *dtype, double *dmg, xmlNodePtr node )
 *
 * @brief Parses a damage node.
 *
 * Example damage node would be:
 * @startcode
 * <damage type="kinetic">10</damage>
 * @endcode
 *
 *    @param[out] dtype Stores the damage type here.
 *    @param[out] dmg Storse the damage here.
 *    @param[in] node Node to parse damage from.
 *    @param return 0 on success.
 */
static int outfit_parseDamage( DamageType *dtype, double *dmg, xmlNodePtr node )
{
   char *buf;

   if (xml_isNode(node,"damage")) {
      /* Get type */
      xmlr_attr(node,"type",buf);
      (*dtype) = outfit_strToDamageType(buf);
      if (buf) free(buf);
      /* Get damage */
      (*dmg) = xml_getFloat(node);
      return 0;
   }

   /* Unknown type */
   (*dtype) = DAMAGE_TYPE_NULL;
   (*dmg) = 0;
   WARN("Trying to parse non-damage node as damage node!");
   return 1;
}


/**
 * @fn static void outfit_parseSBolt( Outfit* temp, const xmlNodePtr parent )
 *
 * @brief Parses the specific area for a bolt weapon and loads it into Outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSBolt( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   char str[PATH_MAX] = "\0";

   /* Defaults */
   temp->u.blt.sound = -1;

   node = parent->xmlChildrenNode;
   do { /* load all the data */
      xmlr_float(node,"speed",temp->u.blt.speed);
      xmlr_float(node,"delay",temp->u.blt.delay);
      xmlr_float(node,"range",temp->u.blt.range);
      xmlr_float(node,"accuracy",temp->u.blt.accuracy);
      xmlr_float(node,"energy",temp->u.blt.energy);

      if (xml_isNode(node,"gfx")) {
         snprintf( str, strlen(xml_get(node))+sizeof(OUTFIT_GFX)+10,
               OUTFIT_GFX"space/%s.png", xml_get(node));
         temp->u.blt.gfx_space = gl_newSprite(str, 6, 6);
      }
      else if (xml_isNode(node,"spfx"))
         temp->u.blt.spfx = spfx_get(xml_get(node));
      else if (xml_isNode(node,"sound"))
         temp->u.blt.sound = sound_get( xml_get(node) );
      else if (xml_isNode(node,"damage"))
         outfit_parseDamage( &temp->u.blt.dtype, &temp->u.blt.damage, node );
   } while (xml_nextNode(node));

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
   MELEMENT(temp->u.blt.gfx_space==NULL,"gfx");
   MELEMENT((sound_disabled!=0) && (temp->u.blt.sound<0),"sound");
   MELEMENT(temp->u.blt.delay==0,"delay");
   MELEMENT(temp->u.blt.speed==0,"speed");
   MELEMENT(temp->u.blt.range==0,"range");
   MELEMENT(temp->u.blt.accuracy==0,"accuracy");
   MELEMENT(temp->u.blt.damage==0,"damage");
#undef MELEMENT
}


/**
 * @fn static void outfit_parseSBeam( Outfit* temp, const xmlNodePtr parent )
 *
 * @brief Parses the beam weapon specifics of an outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSBeam( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;

   node = parent->xmlChildrenNode;
   do { /* load all the data */
      xmlr_float(node,"range",temp->u.bem.range);
      xmlr_float(node,"turn",temp->u.bem.turn);
      xmlr_float(node,"energy",temp->u.bem.energy);

      if (xml_isNode(node,"damage"))
         outfit_parseDamage( &temp->u.bem.dtype, &temp->u.bem.damage, node );
   } while (xml_nextNode(node));

   temp->u.bem.colour = &cWhite; /** @todo Make it loadable. */

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
   MELEMENT(temp->u.bem.range==0,"range");
   MELEMENT(temp->u.bem.turn==0,"turn");
   MELEMENT(temp->u.bem.energy==0,"energy");
   MELEMENT(temp->u.bem.damage==0,"damage");
#undef MELEMENT
}


/**
 * @fn static void outfit_parseSLauncher( Outfit* temp, const xmlNodePtr parent )
 *
 * @brief Parses the specific area for a launcher and loads it into Outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSLauncher( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node  = parent->xmlChildrenNode;


   do { /* load all the data */
      if (xml_isNode(node,"delay")) temp->u.lau.delay = xml_getInt(node);
      else if (xml_isNode(node,"ammo")) temp->u.lau.ammo = strdup(xml_get(node));
   } while (xml_nextNode(node));

#define MELEMENT(o,s)      if (o) WARN("Outfit '%s' missing '"s"' element", temp->name)
   MELEMENT(temp->u.lau.ammo==NULL,"ammo");
   MELEMENT(temp->u.lau.delay==0,"delay");
#undef MELEMENT
}


/**
 * @fn static void outfit_parseSAmmo( Outfit* temp, const xmlNodePtr parent )
 *
 * @brief Parses the specific area for a weapon and loads it into Outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSAmmo( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->xmlChildrenNode;

   char str[PATH_MAX] = "\0";

   do { /* load all the data */
      /* Basic */
      xmlr_float(node,"duration",temp->u.amm.duration);
      xmlr_float(node,"lockon",temp->u.amm.lockon);
      xmlr_float(node,"resist",temp->u.amm.resist);
      /* Movement */
      xmlr_float(node,"thrust",temp->u.amm.thrust);
      xmlr_float(node,"turn",temp->u.amm.turn);
      xmlr_float(node,"speed",temp->u.amm.speed);
      xmlr_float(node,"energy",temp->u.amm.energy);
      if (xml_isNode(node,"gfx")) {
         snprintf( str, strlen(xml_get(node))+sizeof(OUTFIT_GFX)+10,
               OUTFIT_GFX"space/%s.png", xml_get(node));
         temp->u.amm.gfx_space = gl_newSprite(str, 6, 6);
         continue;
      }
      else if (xml_isNode(node,"spfx"))
         temp->u.amm.spfx = spfx_get(xml_get(node));
      else if (xml_isNode(node,"sound"))
         temp->u.amm.sound = sound_get( xml_get(node) );
      else if (xml_isNode(node,"damage"))
         outfit_parseDamage( &temp->u.amm.dtype, &temp->u.amm.damage, node );
   } while (xml_nextNode(node));

   /* Post-processing */
   temp->u.amm.resist /= 100.; /* Set it in per one */

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
   MELEMENT(temp->u.amm.gfx_space==NULL,"gfx");
   MELEMENT((sound_disabled!=0) && (temp->u.amm.sound<0),"sound");
   MELEMENT(temp->u.amm.thrust==0,"thrust");
   /* Dumb missiles don't need everything */
   if (temp->type != OUTFIT_TYPE_MISSILE_DUMB_AMMO) {
      MELEMENT(temp->u.amm.turn==0,"turn");
      MELEMENT(temp->u.amm.lockon==0,"lockon");
   }
   MELEMENT(temp->u.amm.speed==0,"speed");
   MELEMENT(temp->u.amm.duration==0,"duration");
   MELEMENT(temp->u.amm.damage==0,"damage");
#undef MELEMENT
}


/**
 * @fn static void outfit_parseSMod( Outfit* temp, const xmlNodePtr parent )
 *
 * @brief Parses the modification tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSMod( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->children;

   do { /* load all the data */
      /* movement */
      xmlr_float(node,"thrust",temp->u.mod.thrust);
      xmlr_float(node,"turn",temp->u.mod.turn);
      xmlr_float(node,"speed",temp->u.mod.speed);
      /* health */
      xmlr_float(node,"armour",temp->u.mod.armour);
      xmlr_float(node,"shield",temp->u.mod.shield);
      xmlr_float(node,"energy",temp->u.mod.energy);
      xmlr_float(node,"fuel",temp->u.mod.fuel);
      if (xml_isNode(node,"armour_regen"))
         temp->u.mod.armour_regen = xml_getFloat(node)/60.0;
      else if (xml_isNode(node,"shield_regen"))
         temp->u.mod.shield_regen = xml_getFloat(node)/60.0;
      else if (xml_isNode(node,"energy_regen"))
         temp->u.mod.energy_regen = xml_getFloat(node)/60.0;
      /* misc */
      xmlr_int(node,"cargo",temp->u.mod.cargo);
   } while (xml_nextNode(node));
}


/**
 * @fn static void outfit_parseSAfterburner( Outfit* temp, const xmlNodePtr parent )
 *
 * @brief Parses the afterburner tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSAfterburner( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->children;

   /* must be >= 1. */
   temp->u.afb.thrust_perc = 1.;
   temp->u.afb.speed_perc = 1.;
   
   do { /* parse the data */
      xmlr_float(node,"rumble",temp->u.afb.rumble);
      if (xml_isNode(node,"sound"))
         temp->u.afb.sound = sound_get( xml_get(node) );

      if (xml_isNode(node,"thrust_perc"))
         temp->u.afb.thrust_perc = 1. + xml_getFloat(node)/100.;
      xmlr_float(node,"thrust_abs",temp->u.afb.thrust_abs);
      if (xml_isNode(node,"speed_perc"))
         temp->u.afb.speed_perc = 1. + xml_getFloat(node)/100.;
      xmlr_float(node,"speed_abs",temp->u.afb.speed_abs);
      xmlr_float(node,"energy",temp->u.afb.energy);
   } while (xml_nextNode(node));
}


/**
 * @fn static void outfit_parseSMap( Outfit *temp, const xmlNodePtr parent )
 *
 * @brief Parses the map tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSMap( Outfit *temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->children;

   do {
      xmlr_int(node,"radius",temp->u.map.radius);
   } while (xml_nextNode(node));

   if (temp->u.map.radius==0)
      WARN("Outfit '%s' missing/invalid 'radius' element", temp->name);
}


/**
 * @fn static void outfit_parseSJammer( Outfit *temp, const xmlNodePtr parent )
 *
 * @brief Parses the jammer tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSJammer( Outfit *temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->children;

   do {
      xmlr_float(node,"range",temp->u.jam.range);
      xmlr_float(node,"chance",temp->u.jam.chance);
      xmlr_float(node,"energy",temp->u.jam.energy);
   } while (xml_nextNode(node));

   temp->u.jam.chance /= 100.; /* Put in per one, instead of percent */
   temp->u.jam.energy /= 60.; /* It's per minute */

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
   MELEMENT(temp->u.jam.range==0.,"range");
   MELEMENT(temp->u.jam.chance==0.,"chance");
#undef MELEMENT
}


/**
 * @fn static Outfit* outfit_parse( const xmlNodePtr parent )
 *
 * @brief Parses and returns Outfit from parent node.
 *
 *    @param parent Parent node to parse outfit from.
 *    @return A newly allocated outfit set with data from parent or NULL on error.
 */
static Outfit* outfit_parse( const xmlNodePtr parent )
{
   Outfit* temp = CALLOC_ONE(Outfit);
   xmlNodePtr cur, node;
   char *prop;
   char str[PATH_MAX] = "\0";

   temp->name = xml_nodeProp(parent,"name"); /* already mallocs */
   if (temp->name == NULL) WARN("Outfit in "OUTFIT_DATA" has invalid or no name");

   node = parent->xmlChildrenNode;

   do { /* load all the data */
      if (xml_isNode(node,"general")) {
         cur = node->children;
         do {
            xmlr_int(cur,"max",temp->max);
            xmlr_int(cur,"tech",temp->tech);
            xmlr_int(cur,"mass",temp->mass);
            xmlr_int(cur,"price",temp->price);
            xmlr_strd(cur,"description",temp->description);
            if (xml_isNode(cur,"gfx_store")) {
               snprintf( str, strlen(xml_get(cur))+sizeof(OUTFIT_GFX)+10,
                     OUTFIT_GFX"store/%s.png", xml_get(cur));
               temp->gfx_store = gl_newImage(str);
            }

         } while (xml_nextNode(cur));
      }
      else if (xml_isNode(node,"specific")) { /* has to be processed seperately */

         /* get the type */
         prop = xml_nodeProp(node,"type");
         if (prop == NULL)
            ERR("Outfit '%s' element 'specific' missing property 'type'",temp->name);
         temp->type = outfit_strToOutfitType(prop);
         free(prop);

         /* is secondary weapon? */
         prop = xml_nodeProp(node,"secondary");
         if (prop != NULL) {
            if ((int)atoi(prop)) outfit_setProp(temp, OUTFIT_PROP_WEAP_SECONDARY);
            free(prop);
         }

         if (temp->type==OUTFIT_TYPE_NULL)
            WARN("Outfit '%s' is of type NONE", temp->name);
         else if (outfit_isBolt(temp))
            outfit_parseSBolt( temp, node );
         else if (outfit_isBeam(temp))
            outfit_parseSBeam( temp, node );
         else if (outfit_isLauncher(temp))
            outfit_parseSLauncher( temp, node );
         else if (outfit_isAmmo(temp))
            outfit_parseSAmmo( temp, node );
         else if (outfit_isMod(temp))
            outfit_parseSMod( temp, node );
         else if (outfit_isAfterburner(temp))
            outfit_parseSAfterburner( temp, node );
         else if (outfit_isMap(temp))
            outfit_parseSMap( temp, node );
         else if (outfit_isJammer(temp))
            outfit_parseSJammer( temp, node );
      }
   } while (xml_nextNode(node));

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
   MELEMENT(temp->name==NULL,"name");
   MELEMENT(temp->max==0,"max");
   MELEMENT(temp->tech==0,"tech");
   MELEMENT(temp->gfx_store==NULL,"gfx_store");
   /*MELEMENT(temp->mass==0,"mass"); Not really needed */
   MELEMENT(temp->type==0,"type");
   MELEMENT(temp->price==0,"price");
   MELEMENT(temp->description==NULL,"description");
#undef MELEMENT

   return temp;
}


/**
 * @fn int outfit_load (void)
 *
 * @brief Loads all the outfits.
 *
 *    @return 0 on success.
 */
int outfit_load (void)
{
   uint32_t bufsize;
   char *buf = pack_readfile( DATA, OUTFIT_DATA, &bufsize );

   Outfit *temp;

   xmlNodePtr node;
   xmlDocPtr doc = xmlParseMemory( buf, bufsize );

   node = doc->xmlChildrenNode;
   if (!xml_isNode(node,XML_OUTFIT_ID)) {
      ERR("Malformed '"OUTFIT_DATA"' file: missing root element '"XML_OUTFIT_ID"'");
      return -1;
   }        

   node = node->xmlChildrenNode; /* first system node */
   if (node == NULL) {
      ERR("Malformed '"OUTFIT_DATA"' file: does not contain elements");
      return -1;
   }        

   do {
      if (xml_isNode(node,XML_OUTFIT_TAG)) {

         temp = outfit_parse(node);               
         outfit_stack = realloc(outfit_stack, sizeof(Outfit)*(++outfit_nstack));
         memcpy(outfit_stack+outfit_nstack-1, temp, sizeof(Outfit));
         free(temp);
      }
   } while (xml_nextNode(node));

   xmlFreeDoc(doc);
   free(buf);
   xmlCleanupParser();

   DEBUG("Loaded %d Outfit%s", outfit_nstack, (outfit_nstack==1) ? "" : "s" );

   return 0;
}


/**
 * @fn void outfit_free (void)
 *
 * @brief Frees the outfit stack.
 */
void outfit_free (void)
{
   int i;
   for (i=0; i < outfit_nstack; i++) {
      /* free graphics */
      if (outfit_gfx(&outfit_stack[i]))
         gl_freeTexture(outfit_gfx(&outfit_stack[i]));

      /* strings */
      if (outfit_isLauncher(&outfit_stack[i]) && outfit_stack[i].u.lau.ammo)
         free(outfit_stack[i].u.lau.ammo);
      if (outfit_stack[i].description)
         free(outfit_stack[i].description);
      if (outfit_stack[i].gfx_store)
         gl_freeTexture(outfit_stack[i].gfx_store);
      free(outfit_stack[i].name);
   }
   free(outfit_stack);
}

