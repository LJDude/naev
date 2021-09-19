/*
 * See Licensing and Copyright notice in naev.h
 */



#ifndef MAP_FIND_H
#  define MAP_FIND_H


#include "space.h"


/**
 * @brief Represents a found target.
 */
typedef struct map_find_s {
   Planet *pnt;         /**< Planet available at. */
   StarSystem *sys;     /**< System available at. */
   char display[STRMAX_SHORT];   /**< Name to display. */
   int jumps;           /**< Jumps to system. */
   double distance;     /**< Distance to system. */
} map_find_t;


void map_inputFind( unsigned int parent, const char* str );
void map_inputFindType( unsigned int parent, const char *type );


#endif /* MAP_FIND_H */

