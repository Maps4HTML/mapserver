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
    msSetError(MS_WMSERR, "Mandatory LAYER parameter missing in GetMapML request.", "msGetMapMLLayer()");
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
this request. Check wms/ows_enable_request settings.", "msGetMapMLLayer()");
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
    msSetError(MS_WMSERR, "Invalid PROJECTION parameter", "msGetMapMLLayer()");
    return MS_FAILURE;
  }
  
  /* We're good to go. Generate output for this layer */
  
  msIO_setHeader("Content-Type","text/mapml");
  msIO_sendHeaders();

  msIO_fprintf(fp, "<?xml version='1.0' encoding=\"UTF-8\" ?>\n");
  msIO_fprintf(fp, "<mapml>\n");
  msIO_fprintf(fp, "  <head>\n");

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

  // TODO: Need to map layer extent to PROJECTION coordinates
  msIO_fprintf(fp, "      <input name=\"xmin\" type=\"location\" units=\"pcrs\" position=\"top-left\" axis=\"easting\" min=\"-2.0E7\" max=\"2.0E7\" />\n");
  msIO_fprintf(fp, "      <input name=\"ymin\" type=\"location\" units=\"pcrs\" position=\"bottom-left\" axis=\"northing\" min=\"-2.0E7\" max=\"2.0E7\" />\n");
  msIO_fprintf(fp, "      <input name=\"xmax\" type=\"location\" units=\"pcrs\" position=\"top-right\" axis=\"easting\" min=\"-2.0E7\" max=\"2.0E7\" />\n");
  msIO_fprintf(fp, "      <input name=\"ymax\" type=\"location\" units=\"pcrs\" position=\"top-left\" axis=\"northing\" min=\"-2.0E7\" max=\"2.0E7\" />\n");

  /* GetMap URL */
  // TODO: Set proper output format and transparency... special metadata?
  msIO_fprintf(fp, "      <link rel=\"image\" tref=\"%sSERVICE=WMS&amp;REQUEST=GetMap&amp;FORMAT=image/png&amp;TRANSPARENT=TRUE&amp;STYLES=&amp;VERSION=1.3.0&amp;LAYERS=%s&amp;WIDTH={w}&amp;HEIGHT={h}&amp;CRS=%s&amp;BBOX={xmin},{ymin},{xmax},{ymax}&amp;m4h=t\"/>\n", script_url_encoded, pszLayer, pszCRS);


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

