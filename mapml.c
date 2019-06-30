/******************************************************************************
 * $Id$
 *
 * Project:  MapServer
 * Purpose:  MapML support implementation. 
 *           See https://maps4html.github.io/HTML-Map-Element/spec/
 * Author:   Daniel Morissette
 *
 ******************************************************************************
 * Copyright (c) 1996-2019 Regents of the University of Minnesota.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies of this Software or works derived from this Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#define _GNU_SOURCE

/* TODO: remove unused headers */

#include "mapserver.h"
#include "maperror.h"
#include "mapthread.h"
#include "mapgml.h"
#include <ctype.h>
#include "maptemplate.h"
#include "mapows.h"

#include "mapogcsld.h"
#include "mapogcfilter.h"
#include "mapowscommon.h"

#include "maptime.h"
#include "mapproject.h"

#include <stdarg.h>
#include <time.h>
#include <string.h>

#ifdef WIN32
#include <process.h>
#endif


/*
** msMapMLException()
**
** Report current MapServer error in requested format.
*/

int msMapMLException(mapObj *map, const char *exception_code)
{
  msIO_setHeader("Content-Type","text/xml; charset=UTF-8");
  msIO_sendHeaders();

  msIO_printf("<?xml version='1.0' encoding=\"UTF-8\" standalone=\"no\" ?>\n");
  msIO_printf("<ServiceExceptionReport>\n");

  if (exception_code)
    msIO_printf("<ServiceException code=\"%s\">\n", exception_code);
  else
    msIO_printf("<ServiceException>\n");
  msWriteErrorXML(stdout);
  msIO_printf("</ServiceException>\n");
  msIO_printf("</ServiceExceptionReport>\n");

  return MS_FAILURE; /* so that we can call 'return msMapMLException();' anywhere */
}


/*
** msWriteMapMLLayer()
**
** Return specified layer info in response to GetMapML requests.
**
** URL Parameters:
**  MAP=...
**  REQUEST=GetMapML
**  LAYER=...
**  PROJECTION= one of OSMTILE, CBMTILE, APSTILE, WGS84
**              defaults to OSMTILE if not specified (as per spec)
**
**
** Returns MS_SUCCESS/MS_FAILURE
*/
int msWriteMapMLLayer(FILE *fp, mapObj *map, cgiRequestObj *req, owsRequestObj *ows_request)
{
#ifdef USE_MAPML
  int i = 0;
  char *pszLayer = NULL;
  layerObj *lp;
  int nLayers =0;
  int iLayerIndex = -1;
  char ***nestedGroups = NULL;
  int *numNestedGroups = NULL;
  int *isUsedInNestedGroup = NULL;

  char *pszProjection = "OSMTILE", *pszCRS=NULL;
  char *script_url = NULL, *script_url_encoded = NULL;

  projectionObj proj;
  rectObj ext;

  /* We need this server's onlineresource. It will come with the trailing "?" or "&" */
  /* the returned string should be freed once we're done with it. */
  if ((script_url=msOWSGetOnlineResource(map, "MO", "onlineresource", req)) == NULL ||
      (script_url_encoded = msEncodeHTMLEntities(script_url)) == NULL)  {
    msSetError(MS_WMSERR, "Missing OnlineResource.", "msWriteMapMLLayer()");
    return MS_FAILURE;
  }

  /* Process URL Parameters */
  for(i=0; map && req && i<req->NumParams; i++) {
    
    if(strcasecmp(req->ParamNames[i], "LAYER") == 0) {
      pszLayer = req->ParamValues[i];
    }
    
    if(strcasecmp(req->ParamNames[i], "PROJECTION") == 0) {
      pszProjection = req->ParamValues[i];
    }
  }

  if (!pszLayer) {
    msSetError(MS_WMSERR, "Mandatory LAYER parameter missing in GetMapML request.", "msWriteMapMLLayer()");
    return MS_FAILURE;
  }


  /* Look for requested layer. we check for layer's and group's name */
  /* as well as wms_layer_group names */
  nestedGroups = (char***)msSmallCalloc(map->numlayers, sizeof(char**));
  numNestedGroups = (int*)msSmallCalloc(map->numlayers, sizeof(int));
  isUsedInNestedGroup = (int*)msSmallCalloc(map->numlayers, sizeof(int));
  msWMSPrepareNestedGroups(map, OWS_1_3_0, nestedGroups, numNestedGroups, isUsedInNestedGroup);

  for (i=0; i<map->numlayers; i++) {
    lp = GET_LAYER(map, i);
    if (  ((map->name && strcasecmp(map->name, pszLayer) == 0) ||
           (lp->name && strcasecmp(lp->name, pszLayer) == 0) ||
           (lp->group && strcasecmp(lp->group, pszLayer) == 0) ||
           ((numNestedGroups[i] >0) && (msStringInArray(pszLayer, nestedGroups[i], numNestedGroups[i]))) ) &&
          (msIntegerInArray(lp->index, ows_request->enabled_layers, ows_request->numlayers)) ) {
      nLayers++;
      lp->status = MS_ON;
      iLayerIndex = i;
      break; /* We only care about the first match */
    } else
      lp->status = MS_OFF;
  }

  /* free the stuff used for nested layers */
  for (i = 0; i < map->numlayers; i++) {
    if (numNestedGroups[i] > 0) {
      msFreeCharArray(nestedGroups[i], numNestedGroups[i]);
    }
  }
  msFree(nestedGroups);
  msFree(numNestedGroups);
  msFree(isUsedInNestedGroup);

  if (nLayers != 1) {
    msSetError(MS_WMSERR, "Invalid layer given in the LAYER parameter. A layer might be disabled for \
this request. Check wms/ows_enable_request settings.", "msWriteMapMLLayer()");
    return MS_FAILURE;
  }

  /* Validate PROJECTION and map it to WMS CRS */
  if (pszProjection && strcasecmp(pszProjection, "OSMTILE") == 0)
    pszCRS = "EPSG:3857";  // Web Mercator
  else if (pszProjection && strcasecmp(pszProjection, "CBMTILE") == 0)
    pszCRS = "EPSG:3978";  // Canada LCC
  else if (pszProjection && strcasecmp(pszProjection, "APSTILE") == 0)
    pszCRS = "EPSG:5936";  // Alaska Polar Stereographic
  else if (pszProjection && strcasecmp(pszProjection, "WGS84") == 0)
    pszCRS = "EPSG:4326";
  else {
    msSetError(MS_WMSERR, "Invalid PROJECTION parameter", "msWriteMapMLLayer()");
    return MS_FAILURE;
  }

  if (!msOWSIsCRSValid2(map, lp, "MO", pszCRS)) {
    msSetError(MS_WMSERR, "PROJECTION %s requires CRS %s to be valid for this layer.", "msWriteMapMLLayer()", pszProjection, pszCRS);
    return MS_FAILURE;
  }

  /* Fetch and reproject layer extent to requested CRS */
  // TODO: For now just using map extent... need to look up layer/group extent if applicable
  memcpy(&ext, &(map->extent), sizeof(rectObj));
  msInitProjection(&proj);
  if (msLoadProjectionStringEPSG(&proj, pszCRS) != 0) {
    /* Failed to load projection, msSetError shoudl already have been called */
    return MS_FAILURE;
  }
  if (msProjectionsDiffer(&(map->projection), &proj) == MS_TRUE) {
    msProjectRect(&(map->projection), &proj, &ext);
  } 
  
  /* We're good to go. Generate output for this layer */
  
  msIO_setHeader("Content-Type","text/mapml");
  msIO_sendHeaders();

  msIO_fprintf(fp, "<?xml version='1.0' encoding=\"UTF-8\" ?>\n");
  msIO_fprintf(fp, "<mapml>\n");
  msIO_fprintf(fp, "  <head>\n");

  /* If LAYER name is the top-level map then return map title, otherwise return the first matching layer's title */
  if (map->name && EQUAL(map->name, pszLayer))
    msOWSPrintEncodeMetadata(fp, &(map->web.metadata), "MO", "title", OWS_WARN, "  <title>%s</title>\n", map->name);
  else
    msOWSPrintEncodeMetadata(fp, &(lp->metadata), "MO", "title", OWS_WARN, "  <title>%s</title>\n", lp->name);
  
  //msIO_fprintf(fp, "  <base href=\"TODO-href\" />\n");
  msIO_fprintf(fp, "  <meta charset=\"utf-8\" />\n");
  msIO_fprintf(fp, "  <meta http-equiv=\"Content-Type\" content=\"text/mapml;projection=%s\" />\n", pszProjection);

  msIO_fprintf(fp, "  </head>\n");

  msIO_fprintf(fp, "  <body>\n");
  msIO_fprintf(fp, "    <extent units=\"%s\">\n", pszProjection);
  // TODO: Is Z really used for WMS case?
  msIO_fprintf(fp, "      <input name=\"z\" type=\"zoom\" value=\"10\" min=\"4\" max=\"18\" />\n");
  msIO_fprintf(fp, "      <input name=\"w\" type=\"width\" />\n");
  msIO_fprintf(fp, "      <input name=\"h\" type=\"height\" />\n");

  msIO_fprintf(fp, "      <input name=\"xmin\" type=\"location\" units=\"pcrs\" position=\"top-left\" axis=\"easting\" min=\"%g\" max=\"%g\" />\n", ext.minx, ext.maxx);
  msIO_fprintf(fp, "      <input name=\"ymin\" type=\"location\" units=\"pcrs\" position=\"bottom-left\" axis=\"northing\" min=\"%g\" max=\"%g\" />\n", ext.miny, ext.maxy);
  msIO_fprintf(fp, "      <input name=\"xmax\" type=\"location\" units=\"pcrs\" position=\"top-right\" axis=\"easting\" min=\"%g\" max=\"%g\" />\n", ext.minx, ext.maxx);
  msIO_fprintf(fp, "      <input name=\"ymax\" type=\"location\" units=\"pcrs\" position=\"top-left\" axis=\"northing\" min=\"%g\" max=\"%g\" />\n", ext.miny, ext.maxy);

  /* GetMap URL */
  // TODO: Set proper output format and transparency... special metadata?
  msIO_fprintf(fp, "      <link rel=\"image\" tref=\"%sSERVICE=WMS&amp;REQUEST=GetMap&amp;FORMAT=image/png&amp;TRANSPARENT=TRUE&amp;STYLES=&amp;VERSION=1.3.0&amp;LAYERS=%s&amp;WIDTH={w}&amp;HEIGHT={h}&amp;CRS=%s&amp;BBOX={xmin},{ymin},{xmax},{ymax}&amp;m4h=t\"/>\n", script_url_encoded, pszLayer, pszCRS);

  /* If layer is queryable then enable GetFeatureInfo */
  // TODO Check if layer is queryable (also need to check top-level map, groups, nested groups)
  // TODO: Check if wms_getfeatureinfo_formatlist includes text/mapml */
  msIO_fprintf(fp, "      <input name=\"i\" type=\"location\" axis=\"i\" units=\"map\" min=\"0.0\" max=\"0.0\" />\n");
  msIO_fprintf(fp, "      <input name=\"j\" type=\"location\" axis=\"j\" units=\"map\" min=\"0.0\" max=\"0.0\" />\n");
  msIO_fprintf(fp, "      <link rel=\"query\" tref=\"%sSERVICE=WMS&amp;REQUEST=GetFeatureInfo&amp;INFO_FORMAT=text/mapml&amp;FEATURE_COUNT=50&amp;TRANSPARENT=TRUE&amp;STYLES=&amp;VERSION=1.3.0&amp;LAYERS=%s&amp;QUERY_LAYERS=%s&amp;WIDTH={w}&amp;HEIGHT={h}&amp;CRS=%s&amp;BBOX={xmin},{ymin},{xmax},{ymax}&amp;x={i}&amp;y={j}&amp;m4h=t\"/>\n", script_url_encoded, pszLayer, pszLayer, pszCRS);

  msIO_fprintf(fp, "    </extent>\n");
  msIO_fprintf(fp, "  </body>\n");
  msIO_fprintf(fp, "</mapml>\n");

  
  /* Cleanup */
  msFree(script_url);
  msFree(script_url_encoded);
  
  return MS_SUCCESS;
#else
  msSetError(MS_WMSERR, "MapML support is not available.", "msWriteMapMLLayer()");
  return(MS_FAILURE);
#endif /* USE_MAPML */
}



/*
** msWriteMapMLLayer()
**
** Dump MapML query results for WMS GetFeatureInfo
**
**
** Returns MS_SUCCESS/MS_FAILURE
*/

//TODO: Based on msGMLWriteQuery() but may be better handled using query templates??

int msWriteMapMLQuery(mapObj *map, FILE *fp, const char *namespaces)
{
#if defined(USE_MAPML)
  int status;
  int i,j,k;
  layerObj *lp=NULL;
  shapeObj shape;
  char szPath[MS_MAXPATHLEN];
  char *value;
  char *pszMapSRS = NULL;

  gmlGroupListObj *groupList=NULL;
  gmlItemListObj *itemList=NULL;
  gmlConstantListObj *constantList=NULL;
  gmlGeometryListObj *geometryList=NULL;
  gmlItemObj *item=NULL;
  gmlConstantObj *constant=NULL;

  msInitShape(&shape);


  msIO_setHeader("Content-Type","text/mapml");
  msIO_sendHeaders();

  msIO_fprintf(fp, "<?xml version='1.0' encoding=\"UTF-8\" ?>\n");
  msIO_fprintf(fp, "<mapml>\n");
  msIO_fprintf(fp, "  <head>\n");
  msIO_fprintf(fp, "  <title>GetFeatureInfo Results</title>\n");
  //msIO_fprintf(fp, "  <base href=\"TODO-href\" />\n");
  msIO_fprintf(fp, "  <meta charset=\"utf-8\" />\n");
  // TODO: Add PROJECTION param to Content-Type value
  msIO_fprintf(fp, "  <meta http-equiv=\"Content-Type\" content=\"text/mapml\" />\n");

  msIO_fprintf(fp, "  </head>\n");

  msIO_fprintf(fp, "  <body>\n");
  msIO_fprintf(fp, "    <extent />\n");  // Mandatory extent element (empty)



  /* Look up map SRS. We need an EPSG code for GML, if not then we get null and we'll fall back on the layer's SRS */
  msOWSGetEPSGProj(&(map->projection), NULL, namespaces, MS_TRUE, &pszMapSRS);

  /* step through the layers looking for query results */
  for(i=0; i<map->numlayers; i++) {
    char *pszOutputSRS = NULL;
    int nSRSDimension = 2;
    const char* geomtype;
    char *layername=NULL, *layertitle=NULL;
    
    lp = (GET_LAYER(map, map->layerorder[i]));

    if(lp->resultcache && lp->resultcache->numresults > 0) { /* found results */

#ifdef USE_PROJ
      /* Determine output SRS, if map has none, then try using layer's native SRS */
      if ((pszOutputSRS = pszMapSRS) == NULL) {
        msOWSGetEPSGProj(&(lp->projection), NULL, namespaces, MS_TRUE, &pszOutputSRS);
        if (pszOutputSRS == NULL) {
          msSetError(MS_WMSERR, "No valid EPSG code in map or layer projection for GML output", "msGMLWriteQuery()");
          continue;  /* No EPSG code, cannot output this layer */
        }
      }
#endif

      /* start this collection (layer) */
      
      // TODO: Lookup layername metadata, and chack for NULL lp->name
      /* if no layer name provided fall back on the layer name + "_layer" */
      layername = (char*) msSmallMalloc(strlen(lp->name)+7);
      sprintf(layername, "%s", lp->name);
      //msOWSPrintValidateMetadata(fp, &(lp->metadata), namespaces, "layername", OWS_NOERR, "\t<%s>\n", layername);

      layertitle = (char *) msOWSLookupMetadata(&(lp->metadata), "OM", "title");
      if (layertitle) {
        //msOWSPrintEncodeMetadata(fp, &(lp->metadata), namespaces, "title", OWS_NOERR, "\t<gml:name>%s</gml:name>\n", layertitle);
      }

      geomtype = msOWSLookupMetadata(&(lp->metadata), "OFG", "geomtype");
      if( geomtype != NULL && (strstr(geomtype, "25d") != NULL || strstr(geomtype, "25D") != NULL) )
      {
          msIO_fprintf(fp, "<!-- WARNING: 25d requested for layer '%s' but MapML only supports 2D. -->\n", lp->name);
      }

      /* populate item and group metadata structures */
      itemList = msGMLGetItems(lp, namespaces);
      constantList = msGMLGetConstants(lp, namespaces);
      groupList = msGMLGetGroups(lp, namespaces);
      geometryList = msGMLGetGeometries(lp, namespaces, MS_FALSE);
      if (itemList == NULL || constantList == NULL || groupList == NULL || geometryList == NULL) {
        msSetError(MS_MISCERR, "Unable to populate item and group metadata structures", "msGMLWriteQuery()");
        return MS_FAILURE;
      }

      for(j=0; j<lp->resultcache->numresults; j++) {
        status = msLayerGetShape(lp, &shape, &(lp->resultcache->results[j]));
        if(status != MS_SUCCESS) {
           msGMLFreeGroups(groupList);
           msGMLFreeConstants(constantList);
           msGMLFreeItems(itemList);
           msGMLFreeGeometries(geometryList);
           return(status);
        }

#ifdef USE_PROJ
        /* project the shape into the map projection (if necessary), note that this projects the bounds as well */
        if(pszOutputSRS == pszMapSRS && msProjectionsDiffer(&(lp->projection), &(map->projection))) {
          status = msProjectShape(&lp->projection, &map->projection, &shape);
          if(status != MS_SUCCESS) {
            msIO_fprintf(fp, "<!-- Warning: Failed to reproject shape: %s -->\n",msGetErrorString(","));
            continue;
          }
        }
#endif

        /* start this feature */
        msIO_fprintf(fp, "      <feature id=\"%s.%d\" class=\"%s\">\n", layername, shape.index, layername);

        // TODO: handle geomeotry output...
        //
        /* Write the feature geometry and bounding box unless 'none' was requested. */
        /* Default to bbox only if nothing specified and output full geometry only if explicitly requested */
        /*  
        if(!(geometryList && geometryList->numgeometries == 1 && strcasecmp(geometryList->geometries[0].name, "none") == 0)) {
          gmlWriteBounds(fp, OWS_GML2, &(shape.bounds), pszOutputSRS, "\t\t\t", "gml");
          if (geometryList && geometryList->numgeometries > 0 )
            gmlWriteGeometry(fp, geometryList, OWS_GML2, &(shape), pszOutputSRS, NULL, "\t\t\t", "", nSRSDimension);
        }
        */
        
        /* write properties */
        msIO_fprintf(fp, "        <properties>\n");
        msIO_fprintf(fp, "          <table>\n");
        msIO_fprintf(fp, "            <thead>\n");
        msIO_fprintf(fp, "              <tr>\n");
        msIO_fprintf(fp, "                <th role=\"columnheader\" scope=\"col\">Property Name</th>\n");
        msIO_fprintf(fp, "                <th role=\"columnheader\" scope=\"col\">Property Value</th>\n");
        msIO_fprintf(fp, "              </tr>\n");
        msIO_fprintf(fp, "            </thead>\n");
        
        for(k=0; k<itemList->numitems; k++) {
          item = &(itemList->items[k]);
          if(msItemInGroups(item->name, groupList) == MS_FALSE) {
            msIO_fprintf(fp, "            <tbody>\n");
            msIO_fprintf(fp, "              <tr>\n");
            msIO_fprintf(fp, "                <th scope=\"row\">%s</th>\n", item->name);
            msIO_fprintf(fp, "                <td itemprop=\"%s\">%s</td>\n", item->name, shape.values[k]);
            msIO_fprintf(fp, "              </tr>\n");
            msIO_fprintf(fp, "            </tbody>\n");
          }
        }

        msIO_fprintf(fp, "          </table>\n");
        msIO_fprintf(fp, "        </properties>\n");


        /* end this feature */
        msIO_fprintf(fp, "      </feature>\n");

        msFreeShape(&shape); /* init too */
      }

      /* end this collection (layer) */
      msFree(layername);
      msFree(layertitle);

      msGMLFreeGroups(groupList);
      msGMLFreeConstants(constantList);
      msGMLFreeItems(itemList);
      msGMLFreeGeometries(geometryList);

      /* msLayerClose(lp); */
    }
    if(pszOutputSRS!=pszMapSRS) {
      msFree(pszOutputSRS);
    }
  } /* next layer */


  
  /* end this document */
  msIO_fprintf(fp, "  </body>\n");
  msIO_fprintf(fp, "</mapml>\n");


  return(MS_SUCCESS);

#else
  msSetError(MS_MISCERR, "MapML support not enabled", "msWriteMapMLQuery()");
  return MS_FAILURE;
#endif
}
